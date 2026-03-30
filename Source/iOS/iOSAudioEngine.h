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
public:
    iOSAudioEngine(GOODMETERAudioProcessor& proc)
        : processor(proc)
    {
        formatManager.registerBasicFormats();

        // 0 inputs, 2 outputs (playback only, no microphone)
        auto err = deviceManager.initialise(0, 2, nullptr, true);
        juce::ignoreUnused(err);

        // Use ourselves as audio callback — we read from transport,
        // feed through processor, then output to speaker
        deviceManager.addAudioCallback(this);
        transportSource.addChangeListener(this);
    }

    ~iOSAudioEngine()
    {
        {
            const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
            transportSource.stop();
            transportSource.setSource(nullptr);
            readerSource.reset();
        }

        transportSource.removeChangeListener(this);
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

        {
            const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

            fileLoaded = false;
            transportSource.stop();
            transportSource.setPosition(0.0);
            transportSource.setSource(nullptr);
            readerSource.reset();

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
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        fileLoaded = false;
        currentFileName.clear();
        currentFilePath.clear();
        fileDurationSeconds = 0.0;
        fileSampleRate = 0.0;
        fileLengthSamples = 0;

        transportSource.stop();
        transportSource.setPosition(0.0);
        transportSource.setSource(nullptr);
        readerSource.reset();
    }

    //==========================================================================
    // Transport controls
    //==========================================================================
    void play()
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (fileLoaded)
        {
            const double totalLength = getTotalLength();
            if (totalLength > 0.1 && transportSource.getCurrentPosition() >= totalLength - 0.01)
                transportSource.setPosition(0.0);

            transportSource.start();
        }
    }

    void pause()
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
        transportSource.stop();
    }

    void stop()
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
        transportSource.stop();
        transportSource.setPosition(0.0);
    }

    void seek(double positionSeconds)
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
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
    bool isPlaying() const { return transportSource.isPlaying(); }
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

        // Fill buffer from transport source (file playback)
        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        transportSource.getNextAudioBlock(info);

        // Feed through the processor so it can compute meters
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        buffer.applyGain(playbackGain.load(std::memory_order_relaxed));

        // Output buffer already points to outputData, so we're done
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        double sr = device->getCurrentSampleRate();
        int bs = device->getCurrentBufferSizeSamples();

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
        // Transport state changed (e.g. reached end of file)
        if (!transportSource.isPlaying() && transportSource.getLengthInSeconds() > 0.1
            && transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds() - 0.01)
        {
            // Playback finished — reset to start
            transportSource.setPosition(0.0);
        }
    }

    GOODMETERAudioProcessor& processor;
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    juce::String currentFileName;
    juce::String currentFilePath;
    bool fileLoaded = false;
    double fileDurationSeconds = 0.0;
    double fileSampleRate = 0.0;
    int64_t fileLengthSamples = 0;
    std::atomic<float> playbackGain { 0.8f };
};
