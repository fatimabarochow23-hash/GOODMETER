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

class iOSMainComponent : public juce::Component,
                         private juce::Timer
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

        settingsPage->onShowClipNamesChanged = [this](bool show)
        {
            nonoPage->setShowClipFileNames(show);
        };

        settingsPage->onExportFeedbackWithMidiChanged = [this](bool enabled)
        {
            if (historyPage != nullptr)
                historyPage->setExportFeedbackWithMidi(enabled);
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
        metersPage->isMarkerModeActive = [this]()
        {
            return nonoPage != nullptr && nonoPage->isMarkerModeEnabled();
        };
        metersPage->addMarkerAtCurrentPosition = [this]()
        {
            if (nonoPage != nullptr)
                nonoPage->addMarkerAtCurrentPositionFromExternal();
        };
        metersPage->getCurrentMarkerItems = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getMarkerItemsForCurrentFile() : std::vector<GoodMeterMarkerItem>{};
        };

        videoPage->isMarkerModeActive = [this]()
        {
            return nonoPage != nullptr && nonoPage->isMarkerModeEnabled();
        };
        videoPage->addMarkerAtCurrentPosition = [this]()
        {
            if (nonoPage != nullptr)
                nonoPage->addMarkerAtCurrentPositionFromExternal();
        };
        videoPage->getCurrentMarkerItems = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getMarkerItemsForCurrentFile() : std::vector<GoodMeterMarkerItem>{};
        };

        historyPage->getMarkerCurrentFileName = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getCurrentMarkerDisplayName() : juce::String();
        };
        historyPage->getMarkerCurrentFilePath = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getCurrentMarkerFilePath() : juce::String();
        };
        historyPage->getMarkerCurrentMetadataSummary = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getCurrentMarkerMetadataSummary() : juce::String();
        };
        historyPage->getMarkerCurrentDurationSeconds = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getCurrentMarkerSourceDurationSeconds() : 0.0;
        };
        historyPage->getCurrentMarkerItems = [this]()
        {
            return nonoPage != nullptr ? nonoPage->getMarkerItemsForCurrentFile() : std::vector<GoodMeterMarkerItem>{};
        };
        historyPage->updateMarkerNote = [this](const juce::String& markerId, const juce::String& note)
        {
            if (nonoPage != nullptr)
                nonoPage->updateMarkerNoteForCurrentFile(markerId, note);
        };
        historyPage->updateMarkerTags = [this](const juce::String& markerId, const juce::StringArray& tags)
        {
            if (nonoPage != nullptr)
                nonoPage->updateMarkerTagsForCurrentFile(markerId, tags);
        };
        historyPage->formatMarkerTimecode = [this](double seconds)
        {
            return nonoPage != nullptr
                ? nonoPage->formatMarkerTimecodeForDisplay(seconds)
                : juce::String();
        };
        nonoPage->onMarkerDataChanged = [this]()
        {
            if (historyPage != nullptr)
                historyPage->refreshList();
            if (metersPage != nullptr)
                metersPage->repaint();
            if (videoPage != nullptr)
                videoPage->repaint();
        };

        // Sync initial state
        settingsPage->setCurrentSkin(nonoPage->getCurrentSkinId());
        settingsPage->setCharacterRenderMode(nonoPage->getCharacterRenderMode());
        settingsPage->setShowImportButton(false);  // default OFF
        settingsPage->setShowClipNames(false);
        settingsPage->setExportFeedbackWithMidi(false);
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

        float btnW = navBar.getWidth() / 5.0f;

        for (int i = 0; i < numPages; ++i)
        {
            auto btnArea = navBar.removeFromLeft((int)btnW);
            bool active = (i == currentPage);

            auto activeColour = isDarkTheme ? juce::Colours::white
                                            : GoodMeterLookAndFeel::textMain;
            auto inactiveColour = isDarkTheme ? juce::Colours::white.withAlpha(0.4f)
                                              : GoodMeterLookAndFeel::textMuted.withAlpha(0.82f);
            const auto ink = active ? activeColour : inactiveColour;
            const auto navBackground = isDarkTheme ? juce::Colours::black
                                                   : GoodMeterLookAndFeel::bgMain;
            auto iconArea = btnArea.toFloat().withSizeKeepingCentre(34.0f, 34.0f);
            const auto centre = iconArea.getCentre();

            auto drawDot = [&](float x, float y, float size)
            {
                g.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size);
            };

            auto drawPill = [&](float cx, float cy, float w, float h)
            {
                g.fillRoundedRectangle(cx - w * 0.5f, cy - h * 0.5f, w, h, juce::jmin(w, h) * 0.48f);
            };

            g.setColour(ink);

            switch (i)
            {
                case 0: // 灵 - solid concentric circles
                {
                    auto outer = juce::Rectangle<float>(22.2f, 22.2f).withCentre(centre);
                    auto inner = juce::Rectangle<float>(14.5f, 14.5f).withCentre(centre);
                    g.drawEllipse(outer, 2.5f);
                    g.drawEllipse(inner, 2.2f);
                    break;
                }
                case 1: // 音 - three rising capsules
                {
                    drawPill(centre.x - 7.5f, centre.y + 0.2f, 4.2f, 13.0f);
                    drawPill(centre.x,        centre.y - 1.0f, 4.2f, 18.0f);
                    drawPill(centre.x + 7.5f, centre.y + 1.0f, 4.2f, 15.0f);
                    break;
                }
                case 2: // 定 - hollow D-pad cross
                {
                    const float outerArm = 24.0f;
                    const float outerThickness = 10.1f;
                    const float innerArm = 16.8f;
                    const float innerThickness = 4.3f;

                    juce::Path outerCross;
                    outerCross.addRectangle(centre.x - outerThickness * 0.5f, centre.y - outerArm * 0.5f,
                                            outerThickness, outerArm);
                    outerCross.addRectangle(centre.x - outerArm * 0.5f, centre.y - outerThickness * 0.5f,
                                            outerArm, outerThickness);

                    juce::Path innerCross;
                    innerCross.addRectangle(centre.x - innerThickness * 0.5f, centre.y - innerArm * 0.5f,
                                            innerThickness, innerArm);
                    innerCross.addRectangle(centre.x - innerArm * 0.5f, centre.y - innerThickness * 0.5f,
                                            innerArm, innerThickness);

                    juce::Graphics::ScopedSaveState save(g);
                    g.addTransform(juce::AffineTransform::rotation(settingsIconRotation, centre.x, centre.y));
                    g.fillPath(outerCross);
                    g.setColour(navBackground);
                    g.fillPath(innerCross);
                    break;
                }
                case 3: // 记 - stacked record lines
                {
                    drawPill(centre.x - 2.0f, centre.y - 7.2f, 17.0f, 3.2f);
                    drawPill(centre.x + 1.5f, centre.y,        21.0f, 3.2f);
                    drawPill(centre.x - 2.5f, centre.y + 7.2f, 15.0f, 3.2f);
                    drawDot(centre.x - 12.0f, centre.y - 7.2f, 2.7f);
                    break;
                }
                case 4: // 视 - double hollow play triangles
                {
                    juce::Path outerTriangle;
                    outerTriangle.addTriangle(centre.x - 8.0f, centre.y - 10.0f,
                                              centre.x - 8.0f, centre.y + 10.0f,
                                              centre.x + 10.5f, centre.y);

                    juce::Path innerTriangle;
                    innerTriangle.addTriangle(centre.x - 4.0f, centre.y - 6.2f,
                                              centre.x - 4.0f, centre.y + 6.2f,
                                              centre.x + 6.7f, centre.y);

                    g.strokePath(outerTriangle, juce::PathStrokeType(2.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.strokePath(innerTriangle, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }
                default:
                    break;
            }
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
    void timerCallback() override
    {
        const float delta = settingsIconTargetRotation - settingsIconRotation;
        if (std::abs(delta) < 0.0025f)
        {
            settingsIconRotation = settingsIconTargetRotation;
            stopTimer();
            repaint();
            return;
        }

        settingsIconRotation += delta * 0.22f;
        repaint();
    }

    void applyDarkTheme(bool dark)
    {
        const bool themeChanged = (isDarkTheme != dark);
        isDarkTheme = dark;
        settingsIconTargetRotation = isDarkTheme ? (juce::MathConstants<float>::pi * 0.25f) : 0.0f;

        // Codex: 主人要求我接手 iOS 主题，但别污染插件版和 standalone。
        // 所以这里我先把主题只往 iOS 五页和底部导航同步，不改共享 meter 本体。
        settingsPage->setDarkTheme(isDarkTheme);
        historyPage->setDarkTheme(isDarkTheme);
        metersPage->setDarkTheme(isDarkTheme);
        nonoPage->setDarkTheme(isDarkTheme);
        videoPage->setDarkTheme(isDarkTheme);

        if (themeChanged)
            startTimerHz(60);

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
    float settingsIconRotation = 0.0f;
    float settingsIconTargetRotation = 0.0f;
};
