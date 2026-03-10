/*
  ==============================================================================
    SystemAudioCapture.h
    GOODMETER - System Audio Capture via macOS CoreAudio Process Tap

    PIMPL pattern: pure C++ interface, Obj-C/CoreAudio types hidden in .mm.
    Any .cpp file can safely #include this header.

    Requires macOS 14.2+ (Sonoma) for AudioHardwareCreateProcessTap.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#if JUCE_MAC && JucePlugin_Build_Standalone

#include <memory>

class SystemAudioCapture
{
public:
    SystemAudioCapture();
    ~SystemAudioCapture();

    // Start capturing system audio asynchronously (never blocks the message thread).
    // expectedSampleRate: the JUCE audio device's sample rate (for ring buffer sizing).
    // Poll isActive() to check when stream is ready.
    void startAsync(double expectedSampleRate);

    // Stop capturing and release CoreAudio resources.
    void stop();

    // Returns true if the tap is actively capturing.
    bool isActive() const;

    // Pull captured samples into destination buffers (called from processBlock).
    // destR may be nullptr for mono output.
    // Returns actual number of samples read (may be < maxSamples).
    int readSamples(float* destL, float* destR, int maxSamples);

    // Get the stream's native sample rate (may differ from JUCE's).
    double getStreamSampleRate() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // JUCE_MAC && JucePlugin_Build_Standalone
