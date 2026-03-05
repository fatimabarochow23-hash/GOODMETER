/*
  ==============================================================================
    StandaloneNonoEditor.h
    GOODMETER - Standalone Desktop Pet Editor

    Nono-only editor for standalone mode:
    - Transparent background (no card frame)
    - HoloNonoComponent fills entire editor
    - Minimal 60Hz timer for LRA calculation
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"
#include "HoloNonoComponent.h"

//==============================================================================
class StandaloneNonoEditor : public juce::AudioProcessorEditor,
                             public juce::Timer
{
public:
    //==========================================================================
    StandaloneNonoEditor(GOODMETERAudioProcessor& p)
        : AudioProcessorEditor(&p), audioProcessor(p)
    {
        setLookAndFeel(&customLookAndFeel);

        // Transparent editor: no solid background
        setOpaque(false);

        // Create HoloNono directly (no MeterCard wrapper)
        holoNono = std::make_unique<HoloNonoComponent>(audioProcessor);
        addAndMakeVisible(holoNono.get());

        // Non-resizable compact size for desktop pet
        setSize(280, 360);
        setResizable(false, false);

        // Start 60Hz timer for LRA calculation
        startTimerHz(60);
    }

    ~StandaloneNonoEditor() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        // Fully transparent — only Nono's own paint() draws anything
        g.fillAll(juce::Colours::transparentBlack);
    }

    void resized() override
    {
        if (holoNono != nullptr)
            holoNono->setBounds(getLocalBounds());
    }

    //==========================================================================
    void timerCallback() override
    {
        // Minimal timer: feed LRA calculation (same as original editor)
        float shortTerm = audioProcessor.lufsShortTerm.load(std::memory_order_relaxed);

        if (++lraFrameCounter >= 6)
        {
            audioProcessor.pushShortTermLUFSForLRA(shortTerm);
            audioProcessor.calculateLRARealtime();
            lraFrameCounter = 0;
        }
    }

private:
    GOODMETERAudioProcessor& audioProcessor;
    GoodMeterLookAndFeel customLookAndFeel;
    std::unique_ptr<HoloNonoComponent> holoNono;
    int lraFrameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneNonoEditor)
};
