/*
  ==============================================================================
    iOSMainComponent.h
    GOODMETER iOS - Root component with five-page horizontal swipe navigation

    Page 0 (NonoPageComponent): Nono/Guoba character, file import, analysis
    Page 1 (MetersPageComponent): 8 scrollable meter cards + transport bar
    Page 2 (SettingsPageComponent): Skin selector, import button toggle
    Page 3 (HistoryPageComponent): Imported audio/video history
    Page 4 (VideoPageComponent): Video player + hidden drawer transport

    Navigation: horizontal swipe between pages, page indicator dots at bottom
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "iOSPluginDefines.h"
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"
#include "iOSAudioEngine.h"
#include "NonoPageComponent.h"
#include "MetersPageComponent.h"
#include "SettingsPageComponent.h"
#include "HistoryPageComponent.h"
#include "VideoPageComponent.h"

class iOSMainComponent : public juce::Component
{
public:
    iOSMainComponent()
    {
        setLookAndFeel(&lookAndFeel);

        // Create processor and audio engine
        processor = std::make_unique<GOODMETERAudioProcessor>();
        audioEngine = std::make_unique<iOSAudioEngine>(*processor);

        // Create pages
        nonoPage = std::make_unique<NonoPageComponent>(*processor, *audioEngine);
        metersPage = std::make_unique<MetersPageComponent>(*processor, *audioEngine);
        settingsPage = std::make_unique<SettingsPageComponent>();
        historyPage = std::make_unique<HistoryPageComponent>();
        videoPage = std::make_unique<VideoPageComponent>(*processor, *audioEngine);

        addAndMakeVisible(nonoPage.get());
        addChildComponent(metersPage.get());    // hidden initially
        addChildComponent(settingsPage.get());  // hidden initially
        addChildComponent(historyPage.get());   // hidden initially
        addChildComponent(videoPage.get());     // hidden initially

        nonoPage->onImportedMediaCopied = [this](const juce::File& file)
        {
            historyPage->refreshList();

            if (isVideoFile(file))
                videoPage->loadVideo(file);
        };

        // ── Wire Settings callbacks ──
        settingsPage->onSkinChanged = [this](int skinId)
        {
            nonoPage->setSkin(skinId);
            historyPage->setCurrentSkin(skinId);
        };

        settingsPage->onShowImportButtonChanged = [this](bool show)
        {
            nonoPage->setShowImportButton(show);
        };

        settingsPage->onMeterDisplayModeChanged = [this](int mode)
        {
            metersPage->setDisplayMode(mode);
        };

        historyPage->onFileRequested = [this](const juce::File& file)
        {
            if (isVideoFile(file))
            {
                // Kick the same video->audio extraction / playback pipeline that
                // page 1 import uses, so page 5 meters read the video's audio
                // instead of showing a silent shell when a video is loaded
                // directly from History.
                nonoPage->loadLibraryFile(file);

                if (videoPage->loadVideo(file))
                    switchToPage(4);
            }
            else if (nonoPage->loadLibraryFile(file))
                switchToPage(0);
        };

        historyPage->onDeleteFileRequested = [this](const juce::File& file)
        {
            if (audioEngine->isFileLoaded()
                && audioEngine->getCurrentFilePath() == file.getFullPathName())
            {
                audioEngine->clearFile();
            }

            if (videoPage->getCurrentVideoPath() == file.getFullPathName())
                videoPage->clearVideo();

            if (file.existsAsFile())
                file.deleteFile();
        };

        // Sync initial state
        settingsPage->setCurrentSkin(nonoPage->getCurrentSkinId());
        settingsPage->setShowImportButton(false);  // default OFF
        settingsPage->setMeterDisplayMode(0);
        metersPage->setDisplayMode(0);
        historyPage->setCurrentSkin(nonoPage->getCurrentSkinId());

        setSize(400, 800);
    }

    ~iOSMainComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);

        // Draw page indicator dots
        auto bounds = getLocalBounds();
        float dotY = static_cast<float>(bounds.getHeight()) - 30.0f;
        float dotSpacing = 16.0f;
        float dotR = 4.0f;
        float totalW = dotSpacing * static_cast<float>(numPages - 1);
        float startX = bounds.getCentreX() - totalW / 2.0f;

        for (int i = 0; i < numPages; ++i)
        {
            float x = startX + static_cast<float>(i) * dotSpacing;
            bool active = (i == currentPage);

            g.setColour(active ? GoodMeterLookAndFeel::textMain
                               : GoodMeterLookAndFeel::textMuted.withAlpha(0.3f));
            g.fillEllipse(x - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Reserve space for page dots at bottom
        auto contentArea = bounds.withTrimmedBottom(40);

        nonoPage->setBounds(contentArea);
        metersPage->setBounds(contentArea);
        settingsPage->setBounds(contentArea);
        historyPage->setBounds(contentArea);
        videoPage->setBounds(contentArea);
    }

    //==========================================================================
    // Horizontal swipe navigation
    //==========================================================================
    void mouseDown(const juce::MouseEvent& e) override
    {
        swipeStartX = e.position.x;
        isSwiping = false;
        suppressPageSwipe = (currentPage == 4
                             && videoPage != nullptr
                             && videoPage->shouldConsumeHorizontalSwipe(
                                    e.getEventRelativeTo(videoPage.get()).position));
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (suppressPageSwipe)
            return;

        float dx = e.position.x - swipeStartX;
        if (std::abs(dx) > 20.0f)
            isSwiping = true;
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (suppressPageSwipe)
        {
            suppressPageSwipe = false;
            return;
        }

        if (!isSwiping) return;

        float dx = e.position.x - swipeStartX;

        if (dx < -50.0f && currentPage < numPages - 1)
        {
            // Swipe left -> next page
            switchToPage(currentPage + 1);
        }
        else if (dx > 50.0f && currentPage > 0)
        {
            // Swipe right -> previous page
            switchToPage(currentPage - 1);
        }

        isSwiping = false;
        suppressPageSwipe = false;
    }

private:
    void switchToPage(int newPage)
    {
        if (newPage == currentPage) return;

        nonoPage->setVisible(newPage == 0);
        metersPage->setVisible(newPage == 1);
        settingsPage->setVisible(newPage == 2);
        historyPage->setVisible(newPage == 3);
        videoPage->setVisible(newPage == 4);

        // Sync settings when entering settings page
        if (newPage == 2)
            settingsPage->setCurrentSkin(nonoPage->getCurrentSkinId());
        else if (newPage == 3)
        {
            historyPage->setCurrentSkin(nonoPage->getCurrentSkinId());
            historyPage->refreshList();
        }

        currentPage = newPage;
        repaint();
    }

    GoodMeterLookAndFeel lookAndFeel;

    static bool isVideoFile(const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".mp4" || ext == ".mov" || ext == ".m4v"
            || ext == ".avi" || ext == ".mkv" || ext == ".mpg"
            || ext == ".mpeg" || ext == ".webm";
    }

    std::unique_ptr<GOODMETERAudioProcessor> processor;
    std::unique_ptr<iOSAudioEngine> audioEngine;

    std::unique_ptr<NonoPageComponent> nonoPage;
    std::unique_ptr<MetersPageComponent> metersPage;
    std::unique_ptr<SettingsPageComponent> settingsPage;
    std::unique_ptr<HistoryPageComponent> historyPage;
    std::unique_ptr<VideoPageComponent> videoPage;

    static constexpr int numPages = 5;  // Nono, Meters, Settings, History, Video
    int currentPage = 0;
    float swipeStartX = 0.0f;
    bool isSwiping = false;
    bool suppressPageSwipe = false;
};
