/*
  ==============================================================================
    AmbisonicRecorder.h
    GOODMETER iOS - Built-in-mic spatial audio recorder

    Captures First-Order Ambisonics (4ch, ACN/SN3D order: W Y Z X) from the
    iPhone microphone array via Apple's spatial audio capture pipeline:

      AVCaptureDeviceInput.multichannelAudioMode = .firstOrderAmbisonics
        (iOS 18+, iPhone 16 and newer — checked at runtime)
      AVCaptureAudioDataOutput.spatialAudioChannelLayoutTag = HOA_ACN_SN3D|4
        (iOS 26+ — this is what exposes real-time FOA PCM buffers)

    On devices/OS versions without FOA support it falls back to plain
    mono/stereo capture through the same code path. Audio is written as
    32-bit float WAV at the device sample rate via a background
    ThreadedWriter, so the capture queue never touches the disk directly.

    V1 scope (2026-07-03): FOA/na fallback WAV recording only.
    Binaural (FOA -> MagLS FIR decode) is planned as a follow-up.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AmbisonicRecorder
{
public:
    struct Impl;

    // What to capture. FOA = 4ch first-order ambisonics (down-swipe / "ambeo").
    // Stereo = 2ch spatial stereo (up-swipe). On devices/OS without FOA
    // support both fall back to the plain built-in-mic capture.
    enum class CaptureMode { stereo, foa };

    AmbisonicRecorder();
    ~AmbisonicRecorder();

    /** True when this device + OS can deliver 4ch FOA PCM from the built-in
        mics (iPhone 16+/iOS 26+). Safe to call anytime; result is cached. */
    bool isFOACaptureSupported() const;

    /** Optional tap: invoked on the capture queue with each captured block so
        the host can drive live meters (e.g. feed the processor). The buffer's
        first two channels are safe to read for L/R metering (FOA: W, Y). Do
        NOT retain the buffer beyond the callback. */
    std::function<void(const juce::AudioBuffer<float>&)> onCaptureBlock;

    /** Begins capture into the given WAV file (created/overwritten).
        Returns false with errorMessage on immediate failure (e.g. mic
        permission denied). Actual session startup is asynchronous; buffers
        begin flowing shortly after. */
    bool startRecording(const juce::File& outputWavFile, CaptureMode mode, juce::String& errorMessage);

    /** Stops asynchronously. onFinished is invoked on the JUCE message
        thread once the WAV file is finalised. */
    void stopRecording(std::function<void(bool success, juce::File file)> onFinished);

    bool isRecording() const;
    double getElapsedSeconds() const;

    /** 4 while an FOA recording is active, otherwise the fallback channel
        count (1 or 2). 0 when idle before the first buffer arrives. */
    int getNumActiveChannels() const;

    /** True if the recording that is running (or just ran) used FOA. */
    bool isCurrentRecordingFOA() const;

    /** Per-channel linear peak since the last call (UI meter feed). */
    float getAndClearChannelPeak(int channelIndex);

private:
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbisonicRecorder)
};
