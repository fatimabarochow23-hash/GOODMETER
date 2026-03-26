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

class iOSAudioEngine : private juce::ChangeListener
{
public:
    iOSAudioEngine(GOODMETERAudioProcessor& proc)
        : processor(proc)
    {
        formatManager.registerBasicFormats();

        // 0 inputs, 2 outputs (playback only, no microphone)
        auto err = deviceManager.initialise(0, 2, nullptr, true);
        if (err.isNotEmpty())
            DBG("AudioDeviceManager init error: " + err);

        // Wire: transport -> processorPlayer -> deviceManager
        processorPlayer.setProcessor(&processor);
        deviceManager.addAudioCallback(&processorPlayer);
        transportSource.addChangeListener(this);
    }

    ~iOSAudioEngine()
    {
        transportSource.setSource(nullptr);
        transportSource.removeChangeListener(this);
        deviceManager.removeAudioCallback(&processorPlayer);
        processorPlayer.setProcessor(nullptr);
    }

    //==========================================================================
    // File loading
    //==========================================================================
    bool loadFile(const juce::File& file)
    {
        stop();

        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return false;

        currentFileName = file.getFileName();

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(readerSource.get(), 0, nullptr,
                                   reader->sampleRate, 2);

        // Prepare the processor for this sample rate / block size
        processor.setPlayConfigDetails(0, 2,
                                        reader->sampleRate,
                                        deviceManager.getAudioDeviceSetup().bufferSize > 0
                                            ? deviceManager.getAudioDeviceSetup().bufferSize
                                            : 512);
        processor.prepareToPlay(reader->sampleRate,
                                deviceManager.getAudioDeviceSetup().bufferSize > 0
                                    ? deviceManager.getAudioDeviceSetup().bufferSize
                                    : 512);

        fileLoaded = true;
        return true;
    }

    //==========================================================================
    // Transport controls
    //==========================================================================
    void play()
    {
        if (fileLoaded)
            transportSource.start();
    }

    void pause()
    {
        transportSource.stop();
    }

    void stop()
    {
        transportSource.stop();
        transportSource.setPosition(0.0);
    }

    void seek(double positionSeconds)
    {
        transportSource.setPosition(positionSeconds);
    }

    //==========================================================================
    // State queries
    //==========================================================================
    bool isPlaying() const { return transportSource.isPlaying(); }
    bool isFileLoaded() const { return fileLoaded; }

    double getCurrentPosition() const { return transportSource.getCurrentPosition(); }
    double getTotalLength() const { return transportSource.getLengthInSeconds(); }

    juce::String getCurrentFileName() const { return currentFileName; }

    //==========================================================================
    // Audio source for processorPlayer
    // The processorPlayer needs audio input — we feed it from transportSource
    //==========================================================================
    juce::AudioTransportSource& getTransportSource() { return transportSource; }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        // Transport state changed (e.g. reached end of file)
        if (!transportSource.isPlaying() && transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds() - 0.01)
        {
            // Playback finished — reset to start
            transportSource.setPosition(0.0);
        }
    }

    GOODMETERAudioProcessor& processor;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    juce::String currentFileName;
    bool fileLoaded = false;
};
