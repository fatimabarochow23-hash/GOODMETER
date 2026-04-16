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

        settingsPage->onCharacterRenderModeChanged = [this](int renderMode)
        {
            nonoPage->setCharacterRenderMode(renderMode);
        };

        settingsPage->onShowImportButtonChanged = [this](bool show)
        {
            nonoPage->setShowImportButton(show);
        };

        settingsPage->onMeterDisplayModeChanged = [this](int mode)
        {
            metersPage->setDisplayMode(mode);
        };

        settingsPage->onLoudnessStandardChanged = [this](int standardId)
        {
            metersPage->setLoudnessStandard(standardId);
        };

        settingsPage->onThemeChanged = [this](bool isDark)
        {
            applyDarkTheme(isDark);
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

        // Codex: when a video-backed session is active, page 2 transport must
        // control the same native video transport instead of only the extracted
        // audio engine. Otherwise hidden page-5 sync immediately revives
        // playback after the user pauses on page 2.
        metersPage->hasExternalTransport = [this]()
        {
            return videoPage != nullptr && videoPage->ownsSharedAudioTransport();
        };
        metersPage->isExternalTransportPlaying = [this]()
        {
            return videoPage != nullptr && videoPage->isTransportPlaying();
        };
        metersPage->getExternalTransportPosition = [this]()
        {
            return videoPage != nullptr ? videoPage->getTransportPositionSeconds() : 0.0;
        };
        metersPage->getExternalTransportLength = [this]()
        {
            return videoPage != nullptr ? videoPage->getTransportDurationSeconds() : 0.0;
        };
        metersPage->getExternalTransportName = [this]()
        {
            return videoPage != nullptr ? videoPage->getTransportDisplayName() : juce::String("No file loaded");
        };
        metersPage->playExternalTransport = [this]()
        {
            if (videoPage != nullptr)
                videoPage->playTransport();
        };
        metersPage->pauseExternalTransport = [this]()
        {
            if (videoPage != nullptr)
                videoPage->pauseTransport();
        };
        metersPage->rewindExternalTransport = [this]()
        {
            if (videoPage != nullptr)
                videoPage->rewindTransport();
        };
        metersPage->seekExternalTransport = [this](double seconds)
        {
            if (videoPage != nullptr)
                videoPage->seekTransport(seconds);
        };
        metersPage->jumpToEndExternalTransport = [this]()
        {
            if (videoPage != nullptr)
                videoPage->jumpToEndTransport();
        };

        // Sync initial state
        settingsPage->setCurrentSkin(nonoPage->getCurrentSkinId());
        settingsPage->setCharacterRenderMode(nonoPage->getCharacterRenderMode());
        settingsPage->setShowImportButton(false);  // default OFF
        settingsPage->setMeterDisplayMode(0);
        settingsPage->setLoudnessStandard(2);
        metersPage->setDisplayMode(0);
        metersPage->setLoudnessStandard(2);
        historyPage->setCurrentSkin(nonoPage->getCurrentSkinId());
        applyDarkTheme(settingsPage->isDark());

        setSize(400, 800);
    }

    ~iOSMainComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);

        // Draw Chinese character navigation bar at bottom
        auto bounds = getLocalBounds();
        float navH = 60.0f;
        auto navBar = bounds.removeFromBottom((int)navH);
        g.setColour(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);
        g.fillRect(navBar);
        g.setColour(isDarkTheme ? juce::Colours::white.withAlpha(0.08f)
                                : GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
        g.fillRect(navBar.removeFromTop(1));

        juce::String labels[] = {
            juce::CharPointer_UTF8("\xe7\x81\xb5"),  // 灵
            juce::CharPointer_UTF8("\xe9\x9f\xb3"),  // 音
            juce::CharPointer_UTF8("\xe5\xae\x9a"),  // 定
            juce::CharPointer_UTF8("\xe8\xae\xb0"),  // 记
            juce::CharPointer_UTF8("\xe8\xa7\x86")   // 视
        };
        float btnW = navBar.getWidth() / 5.0f;

        for (int i = 0; i < numPages; ++i)
        {
            auto btnArea = navBar.removeFromLeft((int)btnW);
            bool active = (i == currentPage);

            auto activeColour = isDarkTheme ? juce::Colours::white
                                            : GoodMeterLookAndFeel::textMain;
            auto inactiveColour = isDarkTheme ? juce::Colours::white.withAlpha(0.4f)
                                              : GoodMeterLookAndFeel::textMuted.withAlpha(0.82f);
            g.setColour(active ? activeColour : inactiveColour);
            g.setFont(juce::Font(32.0f, juce::Font::bold));
            g.drawText(labels[i], btnArea, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Reserve space for navigation bar at bottom
        auto contentArea = bounds.withTrimmedBottom(60);

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
        // Check if clicking navigation bar
        auto bounds = getLocalBounds();
        float navH = 60.0f;
        auto navBar = bounds.removeFromBottom((int)navH);

        if (navBar.contains(e.position.toInt()))
        {
            float btnW = navBar.getWidth() / 5.0f;
            int clickedPage = (int)(e.position.x / btnW);
            if (clickedPage >= 0 && clickedPage < numPages)
            {
                navClickConsumed = true;
                isSwiping = false;
                suppressPageSwipe = false;
                switchToPage(clickedPage);
                return;
            }
        }

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
        if (navClickConsumed)
        {
            navClickConsumed = false;
            isSwiping = false;
            suppressPageSwipe = false;
            return;
        }

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
    void applyDarkTheme(bool dark)
    {
        isDarkTheme = dark;

        // Codex: 主人要求我接手 iOS 主题，但别污染插件版和 standalone。
        // 所以这里我先把主题只往 iOS 五页和底部导航同步，不改共享 meter 本体。
        settingsPage->setDarkTheme(isDarkTheme);
        historyPage->setDarkTheme(isDarkTheme);
        metersPage->setDarkTheme(isDarkTheme);
        nonoPage->setDarkTheme(isDarkTheme);
        videoPage->setDarkTheme(isDarkTheme);
        repaint();
    }

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
    bool navClickConsumed = false;
    bool isDarkTheme = false;
};
