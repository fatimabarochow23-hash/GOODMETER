/*
  ==============================================================================
    NonoPageComponent.h
    GOODMETER iOS - Page 1: Nono/Guoba character + file import + analysis

    Main interface page featuring:
    - HoloNonoComponent (full character with all expressions/skins)
    - Optional Import Audio button (controlled by Settings page)
    - When import button hidden: double-tap character to import
    - After import: Nono shows analysis results
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../HoloNonoComponent.h"
#include "../VideoAudioExtractor.h"
#include "../GoodMeterLookAndFeel.h"
#include "iOSAudioEngine.h"

class NonoPageComponent : public juce::Component,
                           public juce::Timer
{
public:
    std::function<void(const juce::File&)> onImportedMediaCopied;

    NonoPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
        // Nono character
        holoNono = std::make_unique<HoloNonoComponent>(processor);
        holoNono->onImportFileChosen = [this](const juce::URL& url)
        {
            handleImportedUrl(url);
        };
        addAndMakeVisible(holoNono.get());

        // Import button (hidden by default, controlled by Settings)
        importButton.setButtonText("IMPORT AUDIO");
        importButton.onClick = [this]() { openImportDialog(); };
        importButton.setVisible(showImportButton);
        addAndMakeVisible(importButton);

        // File name label (shown after import)
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
        auto safeArea = bounds.reduced(8, 0);

        if (showImportButton)
        {
            // Import button visible: Nono takes ~70% top, controls at bottom
            float controlH = juce::jmin(safeArea.getHeight() * 0.20f, 120.0f);
            auto controlArea = safeArea.removeFromBottom(static_cast<int>(controlH));
            holoNono->setBounds(safeArea);

            auto controlPadded = controlArea.reduced(20, 4);

            // Row 1: Import button
            auto row1 = controlPadded.removeFromTop(44);
            importButton.setBounds(row1);

            controlPadded.removeFromTop(4);

            // Row 2: File name (if loaded)
            if (audioEngine.isFileLoaded())
            {
                auto row2 = controlPadded.removeFromTop(24);
                fileNameLabel.setBounds(row2);
            }
        }
        else
        {
            // Import button hidden: Nono is more centered, takes full space
            // Reserve a small area for file name at bottom if loaded
            if (audioEngine.isFileLoaded())
            {
                auto fileRow = safeArea.removeFromBottom(30);
                fileNameLabel.setBounds(fileRow.reduced(20, 2));
            }

            holoNono->setBounds(safeArea);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        // Double-tap to import (always works, primary method when button is hidden)
        openImportDialog();
    }

    void timerCallback() override
    {
        // Update file name label
        if (audioEngine.isFileLoaded())
        {
            auto currentName = audioEngine.getCurrentFileName();
            bool needsLayout = !fileNameLabel.isVisible();

            if (!fileNameLabel.isVisible())
                fileNameLabel.setVisible(true);

            if (fileNameLabel.getText() != currentName)
            {
                fileNameLabel.setText(currentName, juce::dontSendNotification);
                needsLayout = true;
            }

            if (needsLayout)
                resized();
        }
        else if (fileNameLabel.isVisible())
        {
            fileNameLabel.setVisible(false);
            fileNameLabel.setText({}, juce::dontSendNotification);
            resized();
        }
    }

    //==========================================================================
    // Public API — called by iOSMainComponent
    //==========================================================================

    /** Get reference to HoloNono for external wiring */
    HoloNonoComponent* getHoloNono() { return holoNono.get(); }

    /** Show or hide the IMPORT AUDIO button */
    void setShowImportButton(bool show)
    {
        showImportButton = show;
        importButton.setVisible(show);
        resized();
        repaint();
    }

    /** Change character skin */
    void setSkin(int skinId)
    {
        if (holoNono == nullptr) return;
        switch (skinId)
        {
            case 1: holoNono->setSkin(HoloNonoComponent::SkinType::Nono); break;
            case 2: holoNono->setSkin(HoloNonoComponent::SkinType::Guoba); break;
            default: break;
        }
    }

    int getCurrentSkinId() const
    {
        if (holoNono && holoNono->isGuoba()) return 2;
        return 1;
    }

    bool loadLibraryFile(const juce::File& file)
    {
        if (isVideoFile(file))
            return startVideoLoad(file, false);

        return loadAnalyzedFile(file);
    }

private:
    void openImportDialog()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Media File", juce::File{},
            "*.wav;*.mp3;*.aiff;*.aif;*.flac;*.ogg;*.m4a;*.caf;*.mp4;*.mov;*.m4v;*.avi;*.mkv;*.mpg;*.mpeg;*.webm");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto resultUrl = fc.getURLResult();
                if (!resultUrl.isEmpty())
                    handleImportedUrl(resultUrl);
            });
    }

    juce::File copyImportedUrlToDocuments(const juce::URL& pickedUrl)
    {
        if (pickedUrl.isEmpty())
            return {};

        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto fileName = juce::URL::removeEscapeChars(pickedUrl.getFileName());

        if (fileName.isEmpty() && pickedUrl.isLocalFile())
            fileName = pickedUrl.getLocalFile().getFileName();

        if (fileName.isEmpty())
            fileName = "ImportedAudio.wav";

        auto localCopy = docsDir.getChildFile(fileName);

        if (pickedUrl.isLocalFile())
        {
            auto sourceFile = pickedUrl.getLocalFile();

            if (sourceFile.existsAsFile()
                && sourceFile.getFullPathName() == localCopy.getFullPathName())
            {
                return sourceFile;
            }

            if (localCopy.existsAsFile())
                localCopy.deleteFile();

            if (sourceFile.existsAsFile()
                && sourceFile.copyFileTo(localCopy)
                && localCopy.existsAsFile())
            {
                return localCopy;
            }
        }

        if (localCopy.existsAsFile())
            localCopy.deleteFile();

        auto input = pickedUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress));

        if (input == nullptr)
            return {};

        auto output = localCopy.createOutputStream();
        if (output == nullptr)
            return {};

        output->writeFromInputStream(*input, -1);
        output->flush();

        return localCopy.existsAsFile() ? localCopy : juce::File{};
    }

    void handleImportedUrl(const juce::URL& pickedUrl)
    {
        auto localCopy = copyImportedUrlToDocuments(pickedUrl);
        if (!localCopy.existsAsFile())
            return;

        if (onImportedMediaCopied != nullptr)
            onImportedMediaCopied(localCopy);

        if (isVideoFile(localCopy))
        {
            startVideoLoad(localCopy, true);
            return;
        }

        loadAnalyzedFile(localCopy);
    }

    static bool isVideoFile(const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".mp4" || ext == ".mov" || ext == ".m4v"
            || ext == ".avi" || ext == ".mkv" || ext == ".mpg"
            || ext == ".mpeg" || ext == ".webm";
    }

    static juce::File getExtractedAudioFileForVideo(const juce::File& videoFile)
    {
        auto baseName = juce::URL::removeEscapeChars(videoFile.getFileNameWithoutExtension());
        if (baseName.isEmpty())
            baseName = "ImportedVideo";

        return videoFile.getParentDirectory().getChildFile("Extract_" + baseName + ".wav");
    }

    bool startVideoLoad(const juce::File& videoFile, bool forceReextract)
    {
        if (!videoFile.existsAsFile())
            return false;

        auto outputFile = getExtractedAudioFileForVideo(videoFile);
        if (!forceReextract && outputFile.existsAsFile())
            return loadAnalyzedFile(outputFile);

        if (holoNono != nullptr)
            holoNono->triggerExtractExpression();

        auto safeThis = juce::Component::SafePointer<NonoPageComponent>(this);
        VideoAudioExtractor::extractAudio(videoFile, outputFile,
            [safeThis, outputFile](bool success)
            {
                if (auto* self = safeThis.getComponent())
                {
                    if (self->holoNono != nullptr)
                        self->holoNono->stopExtractExpression();

                    if (success && outputFile.existsAsFile())
                        self->loadAnalyzedFile(outputFile);
                }
            });

        return true;
    }

    bool loadAnalyzedFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        // iOS import must behave like a single transaction:
        // page 1 analysis and page 2 playback always point at the same file.
        // If playback fails to swap to the new asset, don't let analysis race ahead
        // and leave the UI in a split "new on page 1 / old on page 2" state.
        const bool playbackLoaded = audioEngine.loadFile(file);
        if (!playbackLoaded)
            return false;

        if (holoNono != nullptr)
            holoNono->analyzeFile(file);

        fileNameLabel.setText(audioEngine.getCurrentFileName(), juce::dontSendNotification);
        fileNameLabel.setVisible(true);
        resized();
        return true;
    }

    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;

    std::unique_ptr<HoloNonoComponent> holoNono;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton importButton;
    juce::Label fileNameLabel;

    bool showImportButton = false;  // default OFF (user double-taps to import)
};
