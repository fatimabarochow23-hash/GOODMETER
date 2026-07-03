/*
  ==============================================================================
    iOSAudioEngine.h
    GOODMETER iOS - Audio playback engine

    Wraps AudioDeviceManager + AudioTransportSource + AudioProcessorPlayer
    to enable real-time file playback through GOODMETERAudioProcessor.

    Pipeline:
      AudioFormatReader -> AudioFormatReaderSource -> AudioTransportSource
        -> AudioProcessorPlayer(GOODMETERAudioProcessor)
          -> AudioDeviceManager -> speaker
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

class iOSAudioEngine : private juce::ChangeListener,
                        private juce::AudioIODeviceCallback
{
    enum class FadeCompletionAction
    {
        none,
        pause,
        stopAtStart
    };

public:
    iOSAudioEngine(GOODMETERAudioProcessor& proc)
        : processor(proc)
    {
        formatManager.registerBasicFormats();
        transportSource.addChangeListener(this);
    }

    ~iOSAudioEngine()
    {
        // Stop outside the lock so the render callback can acknowledge it.
        transportSource.stop();

        {
            const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
            transportSource.setSource(nullptr);
            readerSource.reset();
        }

        transportSource.removeChangeListener(this);

        if (deviceInitialised)
            deviceManager.removeAudioCallback(this);
    }

    //==========================================================================
    // File loading
    //==========================================================================
    bool loadFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return false;

        // Store duration directly from reader metadata — don't rely on
        // transportSource.getLengthInSeconds() which needs prepareToPlay's
        // sampleRate to have been set (may be 0 on iOS simulator).
        fileSampleRate = reader->sampleRate;
        fileLengthSamples = reader->lengthInSamples;
        fileDurationSeconds = (reader->sampleRate > 0)
            ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate
            : 0.0;

        currentFileName = juce::URL::removeEscapeChars(file.getFileName());
        currentFilePath = file.getFullPathName();

        auto newReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

        // Stop OUTSIDE the callback lock: stop() needs the render callback
        // alive to acknowledge, and it may legitimately be playing here
        // (e.g. loading a new video while the previous one's audio runs).
        transportSource.stop();

        {
            const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

            fileLoaded = false;
            transportSource.setPosition(0.0);
            transportSource.setSource(nullptr);
            readerSource.reset();
            resetOutputEnvelope();

            readerSource = std::move(newReaderSource);
            transportSource.setSource(readerSource.get(), 0, nullptr,
                                      reader->sampleRate, 2);

            // If the audio device is already running, re-prepare the transport
            // so it picks up the new source's length with a valid sampleRate.
            auto* device = deviceManager.getCurrentAudioDevice();
            if (device != nullptr)
            {
                double sr = device->getCurrentSampleRate();
                int bs = device->getCurrentBufferSizeSamples();
                if (sr > 0)
                    transportSource.prepareToPlay(bs, sr);
            }
            else
            {
                // No audio device — prepare transport with the file's own sample rate
                // so at least getLengthInSeconds() works for UI display
                transportSource.prepareToPlay(512, reader->sampleRate);
            }

            fileLoaded = true;
        }

        return true;
    }

    void clearFile()
    {
        // Stop outside the lock — see loadFile() for why.
        transportSource.stop();

        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        fileLoaded = false;
        currentFileName.clear();
        currentFilePath.clear();
        fileDurationSeconds = 0.0;
        fileSampleRate = 0.0;
        fileLengthSamples = 0;

        transportSource.setPosition(0.0);
        transportSource.setSource(nullptr);
        readerSource.reset();
        resetOutputEnvelope();
    }

    //==========================================================================
    // Transport controls
    //==========================================================================
    void play()
    {
        ensureDeviceInitialised();

        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (fileLoaded)
        {
            ++transportCommandSerial;
            transportStopPending.store(false, std::memory_order_release);
            const double totalLength = getTotalLength();
            if (totalLength > 0.1 && transportSource.getCurrentPosition() >= totalLength - 0.01)
                transportSource.setPosition(0.0);

            forceOutputMute.store(false, std::memory_order_release);
            beginOutputFade(1.0f, FadeCompletionAction::none, -1.0);
            transportSource.start();
        }
    }

    void pause()
    {
        pauseAndSeek(-1.0);
    }

    void pauseAndSeek(double positionSeconds)
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (!fileLoaded)
            return;

        if (transportStopPending.load(std::memory_order_acquire))
            return;

        if (!transportSource.isPlaying())
        {
            transportSource.stop();
            if (positionSeconds >= 0.0)
                transportSource.setPosition(positionSeconds);
            transportStopPending.store(false, std::memory_order_release);
            silenceOutputEnvelope();
            return;
        }

        const auto serial = ++transportCommandSerial;
        beginDeferredTransportStop(serial, FadeCompletionAction::pause, positionSeconds);
    }

    void stop()
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (transportStopPending.load(std::memory_order_acquire))
            return;

        if (!fileLoaded || !transportSource.isPlaying())
        {
            transportSource.stop();
            transportSource.setPosition(0.0);
            transportStopPending.store(false, std::memory_order_release);
            silenceOutputEnvelope();
            return;
        }

        const auto serial = ++transportCommandSerial;
        beginDeferredTransportStop(serial, FadeCompletionAction::stopAtStart, 0.0);
    }

    void seek(double positionSeconds)
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
        ++transportCommandSerial;
        transportStopPending.store(false, std::memory_order_release);
        fadeCompletionAction = FadeCompletionAction::none;
        fadeCompletionSeekPosition = -1.0;
        outputEnvelopeSamplesRemaining = 0;
        transportSource.setPosition(positionSeconds);
    }

    void setVolume(float newVolume)
    {
        playbackGain.store(juce::jlimit(0.0f, 1.0f, newVolume), std::memory_order_relaxed);
    }

    float getVolume() const
    {
        return playbackGain.load(std::memory_order_relaxed);
    }

    //==========================================================================
    // State queries
    //==========================================================================
    // Report "not playing" as soon as a stop/pause has been requested, even
    // though the transport keeps running silently during the declick fade.
    // This keeps the 30Hz page-sync logic from issuing extra transport
    // commands during the fade window.
    bool isPlaying() const
    {
        return transportSource.isPlaying()
            && !transportStopPending.load(std::memory_order_acquire);
    }
    bool isFileLoaded() const { return fileLoaded; }

    double getCurrentPosition() const { return transportSource.getCurrentPosition(); }

    double getTotalLength() const
    {
        double transportLen = transportSource.getLengthInSeconds();
        // transportLen can be +infinity if transport's sampleRate is 0
        if (std::isfinite(transportLen) && transportLen > 0.01)
            return transportLen;
        return fileDurationSeconds;
    }

    juce::String getCurrentFileName() const { return currentFileName; }
    juce::String getCurrentFilePath() const { return currentFilePath; }

    //==========================================================================
    // Audio source for processorPlayer
    // The processorPlayer needs audio input — we feed it from transportSource
    //==========================================================================
    juce::AudioTransportSource& getTransportSource() { return transportSource; }

private:
    void ensureDeviceInitialised()
    {
        if (deviceInitialised)
            return;

        // Delay RemoteIO startup until the app actually needs playback.
        // Starting the device during app launch can deadlock on iOS simulator.
        auto err = deviceManager.initialise(0, 2, nullptr, true);
        juce::ignoreUnused(err);

        deviceManager.addAudioCallback(this);
        deviceInitialised = true;
    }

    //==========================================================================
    // AudioIODeviceCallback: read from transport -> process -> output
    //==========================================================================
    void audioDeviceIOCallbackWithContext(const float* const* /*inputData*/,
                                          int /*numInputChannels*/,
                                          float* const* outputData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        // Create a buffer pointing to the output channels
        juce::AudioBuffer<float> buffer(outputData, numOutputChannels, numSamples);
        buffer.clear();

        // ALWAYS pull the transport, even while force-muted.
        //
        // Why: AudioTransportSource::stop() spin-waits (up to ~1 second) for
        // getNextAudioBlock() to acknowledge the stop by setting its internal
        // 'stopped' flag. The old early-return-on-mute meant that ack never
        // happened, so every pause parked the message thread inside stop()
        // for the full timeout — and because stop() used to be called while
        // holding the device's audio callback lock, the CoreAudio render
        // thread was starved for that entire second. Dozens of consecutive
        // missed render deadlines are exactly the raspy "electric crackle"
        // burst heard on pause. Pulling unconditionally keeps the ack path
        // alive; a stopped transport just clears the region (position does
        // not advance), so this costs nothing.
        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        transportSource.getNextAudioBlock(info);

        // Meter DSP gate: while the transport is engaged (or briefly after it
        // stops, so meters decay to silence naturally) run the full metering
        // chain. Once parked, skip processBlock entirely — running K-weighting
        // + FFT pushes on pure zeros 24/7 was wasted CPU that fed iPhone
        // thermal throttling ("app gets slower the longer it runs").
        const bool transportEngaged = transportSource.isPlaying()
                                   || transportStopPending.load(std::memory_order_acquire);
        if (transportEngaged)
        {
            const double sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
            const int bs = juce::jmax(1, currentDeviceBufferSizeSamples);
            idleMeterDecayBlocksRemaining = juce::jmax(1, (int) std::ceil(1.5 * sr / bs));
        }

        if (transportEngaged || idleMeterDecayBlocksRemaining > 0)
        {
            if (!transportEngaged)
                --idleMeterDecayBlocksRemaining;

            // Feed through the processor so it can compute meters
            juce::MidiBuffer midi;
            processor.processBlock(buffer, midi);
        }

        maybeBeginEndFade();
        applyOutputEnvelope(buffer, numSamples);

        // Force-mute is now a final output gate instead of an early return,
        // so it can never break the transport's stop handshake.
        if (forceOutputMute.load(std::memory_order_acquire))
            buffer.clear();

        // Output buffer already points to outputData, so we're done
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        double sr = device->getCurrentSampleRate();
        int bs = device->getCurrentBufferSizeSamples();

        currentDeviceSampleRate = sr > 0.0 ? sr : 48000.0;
        currentDeviceBufferSizeSamples = bs > 0 ? bs : 512;
        transportSource.prepareToPlay(bs, sr);

        processor.setPlayConfigDetails(0, 2, sr, bs);
        processor.prepareToPlay(sr, bs);
    }

    void audioDeviceStopped() override
    {
        transportSource.releaseResources();
        processor.releaseResources();
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        // End-of-file rewinds are handled by page-level transport code after a
        // declick fade. Seeking here bypasses that gate and can click on iOS.
    }

    void resetOutputEnvelope()
    {
        outputEnvelopeGain = 1.0f;
        outputEnvelopeTargetGain = 1.0f;
        outputEnvelopeSamplesRemaining = 0;
        fadeCompletionAction = FadeCompletionAction::none;
        fadeCompletionSeekPosition = -1.0;
        forceOutputMute.store(false, std::memory_order_release);
    }

    int getOutputFadeSamples() const
    {
        const auto sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
        return juce::jlimit(512, 8192, (int) std::round(sr * 0.090));
    }

    void beginOutputFade(float targetGain, FadeCompletionAction completionAction, double seekAfterFade)
    {
        outputEnvelopeTargetGain = juce::jlimit(0.0f, 1.0f, targetGain);
        outputEnvelopeSamplesRemaining = getOutputFadeSamples();
        fadeCompletionAction = completionAction;
        fadeCompletionSeekPosition = seekAfterFade;
    }

    void silenceOutputEnvelope()
    {
        outputEnvelopeGain = 0.0f;
        outputEnvelopeTargetGain = 0.0f;
        outputEnvelopeSamplesRemaining = 0;
        fadeCompletionAction = FadeCompletionAction::none;
        fadeCompletionSeekPosition = -1.0;
        forceOutputMute.store(true, std::memory_order_release);
    }

    int getOutputFadeMilliseconds() const
    {
        const auto sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
        const auto bufferMs = 1000.0 * (double) juce::jmax(1, currentDeviceBufferSizeSamples) / sr;
        return juce::jlimit(20, 180,
                            (int) std::ceil(1000.0 * (double) getOutputFadeSamples() / sr
                                             + bufferMs + 8.0));
    }

    int getPostMuteDrainMilliseconds() const
    {
        const auto sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
        const auto bufferMs = 1000.0 * (double) juce::jmax(1, currentDeviceBufferSizeSamples) / sr;
        return juce::jlimit(32, 120, (int) std::ceil(bufferMs * 2.0 + 24.0));
    }

    void beginDeferredTransportStop(uint32_t serial, FadeCompletionAction action, double seekAfterFade)
    {
        transportStopPending.store(true, std::memory_order_release);
        forceOutputMute.store(false, std::memory_order_release);
        beginOutputFade(0.0f, FadeCompletionAction::none, -1.0);
        scheduleDeferredTransportStop(serial, action, seekAfterFade);
    }

    void scheduleDeferredTransportStop(uint32_t serial, FadeCompletionAction action, double seekAfterFade)
    {
        // Wait long enough for the fade to have fully reached zero AND for at
        // least one extra hardware buffer of guaranteed-silent output.
        juce::Timer::callAfterDelay(getOutputFadeMilliseconds() + getPostMuteDrainMilliseconds(),
            [this, serial, action, seekAfterFade]()
            {
                // Serial check is message-thread-safe without the lock:
                // play()/seek()/pause() all mutate the serial on this same
                // thread, and the audio callback never touches it.
                if (transportCommandSerial.load(std::memory_order_relaxed) != serial)
                    return;

                // CRITICAL: stop() must be called WITHOUT holding the audio
                // callback lock. It spin-waits for getNextAudioBlock() to
                // acknowledge the stop; holding the lock here blocks the
                // render callback, so the ack never arrives, stop() burns
                // its full ~1s timeout, and CoreAudio is starved the whole
                // time — that starvation burst was the pause crackle.
                // The envelope has been at zero for a full drain period, so
                // any transport-side discontinuity is multiplied by 0.
                transportSource.stop();

                const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

                if (transportCommandSerial.load(std::memory_order_relaxed) != serial)
                    return;

                if (action == FadeCompletionAction::stopAtStart)
                    transportSource.setPosition(0.0);
                else if (seekAfterFade >= 0.0)
                    transportSource.setPosition(seekAfterFade);

                transportStopPending.store(false, std::memory_order_release);
                silenceOutputEnvelope();
            });
    }

    void maybeBeginEndFade()
    {
        if (!fileLoaded || !transportSource.isPlaying()
            || fadeCompletionAction != FadeCompletionAction::none
            || outputEnvelopeTargetGain <= 0.0f)
        {
            return;
        }

        const double totalLength = getTotalLength();
        if (totalLength <= 0.1)
            return;

        const double remainingSeconds = totalLength - transportSource.getCurrentPosition();
        if (remainingSeconds > 0.0 && remainingSeconds <= 0.18)
            beginOutputFade(0.0f, FadeCompletionAction::none, -1.0);
    }

    void applyOutputEnvelope(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float userGain = playbackGain.load(std::memory_order_relaxed);

        if (outputEnvelopeSamplesRemaining <= 0)
        {
            outputEnvelopeGain = outputEnvelopeTargetGain;
            buffer.applyGain(userGain * outputEnvelopeGain);
            return;
        }

        int sample = 0;
        while (sample < numSamples && outputEnvelopeSamplesRemaining > 0)
        {
            const float step = (outputEnvelopeTargetGain - outputEnvelopeGain)
                / (float) outputEnvelopeSamplesRemaining;
            outputEnvelopeGain += step;
            --outputEnvelopeSamplesRemaining;

            const float gain = userGain * outputEnvelopeGain;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);

            ++sample;
        }

        if (outputEnvelopeSamplesRemaining <= 0)
        {
            outputEnvelopeGain = outputEnvelopeTargetGain;

            if (outputEnvelopeTargetGain <= 0.0f)
            {
                if (sample < numSamples)
                    buffer.clear(sample, numSamples - sample);
                return;
            }
        }

        if (sample < numSamples)
            buffer.applyGain(sample, numSamples - sample, userGain * outputEnvelopeGain);
    }

    GOODMETERAudioProcessor& processor;
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    bool deviceInitialised = false;

    juce::String currentFileName;
    juce::String currentFilePath;
    bool fileLoaded = false;
    double fileDurationSeconds = 0.0;
    double fileSampleRate = 0.0;
    int64_t fileLengthSamples = 0;
    std::atomic<float> playbackGain { 0.8f };
    double currentDeviceSampleRate = 48000.0;
    int currentDeviceBufferSizeSamples = 512;
    float outputEnvelopeGain = 1.0f;
    float outputEnvelopeTargetGain = 1.0f;
    int outputEnvelopeSamplesRemaining = 0;
    FadeCompletionAction fadeCompletionAction = FadeCompletionAction::none;
    double fadeCompletionSeekPosition = -1.0;
    std::atomic<uint32_t> transportCommandSerial { 1 };
    std::atomic<bool> transportStopPending { false };
    std::atomic<bool> forceOutputMute { false };
    int idleMeterDecayBlocksRemaining = 0; // audio-thread only
};
