/*
  ==============================================================================
    NonoPageComponent.h
    GOODMETER iOS - Page 1: Nono/Guoba character + file import + playback

    Main interface page featuring:
    - HoloNonoComponent (full character with all expressions/skins)
    - Import Audio button -> UIDocumentPicker via FileChooser
    - Play/Pause button + progress bar for real-time playback
    - Skin selector (Nono/Guoba)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../HoloNonoComponent.h"
#include "../GoodMeterLookAndFeel.h"
#include "iOSAudioEngine.h"

class NonoPageComponent : public juce::Component,
                           public juce::Timer
{
public:
    NonoPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
        // Nono character
        holoNono = std::make_unique<HoloNonoComponent>(processor);
        holoNono->initSkinMenu();
        addAndMakeVisible(holoNono.get());

        // Wire back-face "+" click to open file chooser
        // HoloNonoComponent calls openFileChooser() internally for back-face clicks,
        // but we also provide an Import button below

        // Skin selector
        skinMenu = std::make_unique<juce::ComboBox>();
        skinMenu->addItem("Nono", 1);
        skinMenu->addItem("Guoba", 2);
        skinMenu->setSelectedId(holoNono->isGuoba() ? 2 : 1, juce::dontSendNotification);
        skinMenu->onChange = [this]()
        {
            switch (skinMenu->getSelectedId())
            {
                case 1: holoNono->setSkin(HoloNonoComponent::SkinType::Nono); break;
                case 2: holoNono->setSkin(HoloNonoComponent::SkinType::Guoba); break;
                default: break;
            }
        };
        addAndMakeVisible(skinMenu.get());

        // Import button
        importButton.setButtonText("IMPORT AUDIO");
        importButton.onClick = [this]() { openImportDialog(); };
        addAndMakeVisible(importButton);

        // Play/Pause button (hidden until file loaded)
        playButton.setButtonText("PLAY");
        playButton.onClick = [this]()
        {
            if (audioEngine.isPlaying())
            {
                audioEngine.pause();
                playButton.setButtonText("PLAY");
            }
            else
            {
                audioEngine.play();
                playButton.setButtonText("PAUSE");
            }
        };
        playButton.setVisible(false);
        addAndMakeVisible(playButton);

        // Progress slider (hidden until file loaded)
        progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        progressSlider.setRange(0.0, 1.0, 0.001);
        progressSlider.onValueChange = [this]()
        {
            if (progressSlider.isMouseButtonDown())
            {
                double total = audioEngine.getTotalLength();
                audioEngine.seek(progressSlider.getValue() * total);
            }
        };
        progressSlider.setVisible(false);
        addAndMakeVisible(progressSlider);

        // File name label
        fileNameLabel.setJustificationType(juce::Justification::centred);
        fileNameLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        fileNameLabel.setVisible(false);
        addAndMakeVisible(fileNameLabel);

        startTimerHz(15);
    }

    ~NonoPageComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Safe area insets on iOS
        auto safeArea = bounds.reduced(8, 0);

        // Bottom controls area: ~25% of height
        float controlH = juce::jmin(safeArea.getHeight() * 0.25f, 180.0f);
        auto controlArea = safeArea.removeFromBottom(static_cast<int>(controlH));

        // Nono occupies the remaining top area
        holoNono->setBounds(safeArea);

        // Layout controls from top to bottom in control area
        auto controlPadded = controlArea.reduced(20, 4);

        // Row 1: Import + Skin selector
        auto row1 = controlPadded.removeFromTop(44);
        skinMenu->setBounds(row1.removeFromRight(100));
        row1.removeFromRight(8);
        importButton.setBounds(row1);

        controlPadded.removeFromTop(8);

        // Row 2: File name (if loaded)
        if (audioEngine.isFileLoaded())
        {
            auto row2 = controlPadded.removeFromTop(24);
            fileNameLabel.setBounds(row2);
            controlPadded.removeFromTop(4);
        }

        // Row 3: Play button + progress
        if (audioEngine.isFileLoaded())
        {
            auto row3 = controlPadded.removeFromTop(44);
            playButton.setBounds(row3.removeFromLeft(80));
            row3.removeFromLeft(8);
            progressSlider.setBounds(row3);
        }
    }

    void timerCallback() override
    {
        // Update transport UI
        if (audioEngine.isFileLoaded())
        {
            if (!playButton.isVisible())
            {
                playButton.setVisible(true);
                progressSlider.setVisible(true);
                fileNameLabel.setVisible(true);
                fileNameLabel.setText(audioEngine.getCurrentFileName(),
                                      juce::dontSendNotification);
                resized();
            }

            // Update progress slider (only if not being dragged)
            if (!progressSlider.isMouseButtonDown())
            {
                double total = audioEngine.getTotalLength();
                if (total > 0.0)
                    progressSlider.setValue(audioEngine.getCurrentPosition() / total,
                                            juce::dontSendNotification);
            }

            // Update play button text
            playButton.setButtonText(audioEngine.isPlaying() ? "PAUSE" : "PLAY");
        }
    }

    /** Get reference to HoloNono for external wiring */
    HoloNonoComponent* getHoloNono() { return holoNono.get(); }

private:
    void openImportDialog()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Audio File", juce::File{},
            "*.wav;*.mp3;*.aiff;*.aif;*.flac;*.ogg;*.m4a;*.caf");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.existsAsFile())
                {
                    // Load into audio engine for playback
                    audioEngine.loadFile(result);

                    // Also trigger Nono's offline analysis
                    if (holoNono != nullptr)
                        holoNono->analyzeFile(result);
                }
            });
    }

    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;

    std::unique_ptr<HoloNonoComponent> holoNono;
    std::unique_ptr<juce::ComboBox> skinMenu;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton importButton;
    juce::TextButton playButton;
    juce::Slider progressSlider;
    juce::Label fileNameLabel;
};
