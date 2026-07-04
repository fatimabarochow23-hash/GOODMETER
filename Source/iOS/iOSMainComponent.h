/*
  ==============================================================================
    iOSMainComponent.h
    GOODMETER iOS - Root component with four-page horizontal swipe navigation

    Page 0 (NonoPageComponent): Nono/Guoba character, file import, analysis
    Page 1 (Media): merged audio/video page — shows MetersPageComponent for
                    audio files or VideoPageComponent for video files, chosen
                    automatically by the type of the last imported/loaded media
    Page 2 (SettingsPageComponent): Skin selector, import button toggle
    Page 3 (HistoryPageComponent): Imported audio/video history

    Navigation: horizontal swipe between pages, nav icons at bottom
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

// Self-painting filled pill button. The shared LookAndFeel deliberately draws
// TextButtons with a transparent normal state (so buttonColourId is ignored),
// which made the recording STOP button blend into the background. This paints
// its own solid fill so it reads clearly in any skin.
class FilledPillButton : public juce::Button
{
public:
    FilledPillButton() : juce::Button({}) {}
    juce::Colour fill { juce::Colour(0xFFFF3B30) };
    juce::Colour textCol { juce::Colours::white };

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        auto c = down ? fill.darker(0.18f) : (over ? fill.brighter(0.10f) : fill);
        g.setColour(c);
        g.fillRoundedRectangle(b, b.getHeight() * 0.5f);
        g.setColour(textCol);
        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }
};

class iOSMainComponent : public juce::Component,
                         private juce::Timer
{
public:
    iOSMainComponent()
    {
        setLookAndFeel(&lookAndFeel);

        // Persistent app settings (first persistence infra on iOS — currently
        // stores the recording file-name scheme; other settings can join later).
        {
            juce::PropertiesFile::Options opts;
            opts.applicationName = "GOODMETER";
            opts.filenameSuffix = ".settings";
            opts.folderName = "GOODMETER";
            appSettings = std::make_unique<juce::PropertiesFile>(opts);
        }

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
            {
                videoPage->loadVideo(file);
                setMediaMode(true);
            }
            else
            {
                if (videoPage->hasLoadedVideo())
                    videoPage->clearVideo();
                setMediaMode(false);
            }
        };

        // After a spatial recording finishes, land on the media page so the
        // fresh take is one tap from playback (import stays on its own page).
        nonoPage->onRecordingRequestsMediaPage = [this]()
        {
            switchToPage(1);
        };

        // While a spatial recording is running: switch to the meters page so the
        // user watches input levels, and pulse a red border around the screen.
        nonoPage->onRecordingStateChanged = [this](bool active)
        {
            recordingActive = active;
            recordStopButton.setVisible(active);
            if (active)
            {
                // Colour the STOP button by skin so it reads clearly:
                // Guoba = yellow (dark text), Nono = blue (white text).
                const bool guoba = (nonoPage != nullptr && nonoPage->isGuobaSkin());
                recordStopButton.fill = guoba ? GoodMeterLookAndFeel::accentYellow
                                              : GoodMeterLookAndFeel::accentBlue;
                recordStopButton.textCol = guoba ? GoodMeterLookAndFeel::textMain
                                                 : juce::Colours::white;
                recordStopButton.setEnabled(true);
                recordStopButton.setButtonText("STOP  0:00");
                setMediaMode(false);   // meters, not video
                switchToPage(1);
                recordStopButton.toFront(false);
                recordingBorderPhase = 0.0f;
                startTimerHz(30);      // drive the pulsing border + clock
            }
            resized();
            repaint();
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

        // Recording file-name scheme: persist + push to the recorder page.
        settingsPage->onRecNamingModeChanged = [this](int mode)
        {
            appSettings->setValue("recNamingMode", mode);
            appSettings->saveIfNeeded();
            nonoPage->setRecordingNaming(mode, appSettings->getValue("recPrefix", "REC"));
        };
        settingsPage->onRecPrefixChanged = [this](const juce::String& prefix)
        {
            appSettings->setValue("recPrefix", prefix);
            appSettings->saveIfNeeded();
            nonoPage->setRecordingNaming(appSettings->getIntValue("recNamingMode", 0), prefix);
        };

        historyPage->onFileRequested = [this](const juce::File& file)
        {
            if (isVideoFile(file))
            {
                // Kick the same video->audio extraction / playback pipeline that
                // page 1 import uses, so the media page meters read the video's
                // audio instead of showing a silent shell when a video is loaded
                // directly from History.
                nonoPage->loadLibraryFile(file);

                if (videoPage->loadVideo(file))
                {
                    setMediaMode(true);
                    switchToPage(1);
                }
            }
            else if (nonoPage->loadLibraryFile(file))
            {
                if (videoPage->hasLoadedVideo())
                    videoPage->clearVideo();
                setMediaMode(false);

                switchToPage(0);
            }
        };

        historyPage->onDeleteFileRequested = [this](const juce::File& file)
        {
            if (audioEngine->isFileLoaded()
                && audioEngine->getCurrentFilePath() == file.getFullPathName())
            {
                audioEngine->clearFile();
            }

            if (videoPage->getCurrentVideoPath() == file.getFullPathName())
            {
                videoPage->clearVideo();
                setMediaMode(false);
            }

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

        // Restore persisted recording naming scheme
        {
            const int recMode = appSettings->getIntValue("recNamingMode", 0);
            const auto recPrefix = appSettings->getValue("recPrefix", "REC");
            settingsPage->setRecNaming(recMode, recPrefix);
            nonoPage->setRecordingNaming(recMode, recPrefix);
        }

        // Global STOP button — lives on the root so it stays visible while
        // recording even after we auto-switch to the meters page.
        recordStopButton.setButtonText("STOP");
        recordStopButton.onClick = [this]()
        {
            recordStopButton.setEnabled(false); // debounce during finalize
            if (nonoPage != nullptr)
                nonoPage->stopSpatialRecordingFromUI();
        };
        recordStopButton.setVisible(false);
        addChildComponent(recordStopButton); // hidden until recording (addAndMakeVisible would force-show it)

        setSize(400, 800);
    }

    ~iOSMainComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);

        // Draw graphic-icon navigation bar at bottom
        auto bounds = getLocalBounds();
        float navH = 60.0f;
        auto navBar = bounds.removeFromBottom((int)navH);
        g.setColour(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);
        g.fillRect(navBar);
        g.setColour(isDarkTheme ? juce::Colours::white.withAlpha(0.08f)
                                : GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
        g.fillRect(navBar.removeFromTop(1));

        float btnW = navBar.getWidth() / (float) numPages;

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
                case 0: // Nono page - solid concentric circles
                {
                    auto outer = juce::Rectangle<float>(22.2f, 22.2f).withCentre(centre);
                    auto inner = juce::Rectangle<float>(14.5f, 14.5f).withCentre(centre);
                    g.drawEllipse(outer, 2.5f);
                    g.drawEllipse(inner, 2.2f);
                    break;
                }
                case 1: // Media page (audio/video) - three rising capsules
                {
                    drawPill(centre.x - 7.5f, centre.y + 0.2f, 4.2f, 13.0f);
                    drawPill(centre.x,        centre.y - 1.0f, 4.2f, 18.0f);
                    drawPill(centre.x + 7.5f, centre.y + 1.0f, 4.2f, 15.0f);
                    break;
                }
                case 2: // Settings page - hollow D-pad cross
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
                case 3: // History page - stacked record lines
                {
                    drawPill(centre.x - 2.0f, centre.y - 7.2f, 17.0f, 3.2f);
                    drawPill(centre.x + 1.5f, centre.y,        21.0f, 3.2f);
                    drawPill(centre.x - 2.5f, centre.y + 7.2f, 15.0f, 3.2f);
                    drawDot(centre.x - 12.0f, centre.y - 7.2f, 2.7f);
                    break;
                }
                default:
                    break;
            }
        }
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (!recordingActive)
            return;

        // "REC" anime VFX: staggered light waves spawn at the screen edge and
        // sweep inward — white-hot leading edge, red body, fading tail —
        // riding over a tight breathing base glow hugging the bezel.
        const auto red = juce::Colour(0xFFFF3B30);
        auto area = getLocalBounds().toFloat();
        const float norm = recordingBorderPhase / juce::MathConstants<float>::twoPi;

        // Layer 1: base glow, anchored at the edge, gentle breathing
        const float breathe = 0.30f + 0.20f * (0.5f + 0.5f * std::sin(recordingBorderPhase * 2.0f));
        for (int i = 0; i < 3; ++i)
        {
            const float inset = 1.0f + (float) i * 2.2f;
            g.setColour(red.withAlpha(breathe * (1.0f - (float) i * 0.30f)));
            g.drawRoundedRectangle(area.reduced(inset), 14.0f + inset * 0.4f,
                                   2.2f - (float) i * 0.5f);
        }

        // Layer 2: three inward waves at mutually incommensurate speeds, each
        // with a sin-shaped envelope (born and dying at exactly zero alpha) —
        // the combined pattern never visibly repeats, no loop seam.
        const float travel = juce::jmin(34.0f, area.getWidth() * 0.06f);
        static constexpr float waveSpeed[3]  = { 0.62f, 0.47f, 0.383f };
        static constexpr float waveOffset[3] = { 0.00f, 0.37f, 0.71f };
        for (int k = 0; k < 3; ++k)
        {
            const float t = std::fmod(norm * waveSpeed[k] + waveOffset[k], 1.0f);
            const float d = 1.0f - std::pow(1.0f - t, 1.6f);   // ease-out travel
            const float inset = 2.0f + d * travel;
            const float a = std::pow(std::sin(t * juce::MathConstants<float>::pi), 1.35f);
            if (a < 0.02f)
                continue;

            const float r = 14.0f + inset * 0.55f;

            // Fading tail between the wavefront and the edge
            for (int j = 1; j <= 2; ++j)
            {
                const float tailIn = inset - (float) j * 3.5f;
                if (tailIn < 1.5f)
                    break;
                g.setColour(red.withAlpha(a * 0.16f / (float) j));
                g.drawRoundedRectangle(area.reduced(tailIn),
                                       14.0f + tailIn * 0.55f, 3.0f);
            }

            // Red body of the wavefront
            g.setColour(red.withAlpha(a * 0.55f));
            g.drawRoundedRectangle(area.reduced(inset), r, 2.6f - d * 1.2f);

            // White-hot core on the leading (inner) edge — the anime accent
            g.setColour(juce::Colours::white.withAlpha(a * 0.35f));
            g.drawRoundedRectangle(area.reduced(inset + 1.2f), r + 0.6f, 1.0f);
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

        // STOP button floats just above the nav bar, centered.
        auto stopRow = bounds.withTrimmedBottom(60).removeFromBottom(64);
        recordStopButton.setBounds(stopRow.withSizeKeepingCentre(
            juce::jmin(220, stopRow.getWidth() - 48), 44));
        recordStopButton.toFront(false);
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
            float btnW = navBar.getWidth() / (float) numPages;
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
        suppressPageSwipe = (currentPage == 1
                             && mediaModeVideo
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
        bool keepRunning = false;

        // Pulsing recording border
        if (recordingActive)
        {
            recordingBorderPhase += 0.09f;
            // Wrap far out (not at 2*pi): wrapping every revolution made all
            // waves jump in unison once per cycle — the visible "loop seam".
            if (recordingBorderPhase > juce::MathConstants<float>::twoPi * 10000.0f)
                recordingBorderPhase = 0.0f;

            if (nonoPage != nullptr)
                recordStopButton.setButtonText("STOP  " + nonoPage->getRecordingElapsedText());

            keepRunning = true;
            repaint();
        }

        // Settings gear rotation easing
        const float delta = settingsIconTargetRotation - settingsIconRotation;
        if (std::abs(delta) >= 0.0025f)
        {
            settingsIconRotation += delta * 0.22f;
            keepRunning = true;
            repaint();
        }
        else
        {
            settingsIconRotation = settingsIconTargetRotation;
        }

        if (!keepRunning)
            stopTimer();
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
        metersPage->setVisible(newPage == 1 && !mediaModeVideo);
        videoPage->setVisible(newPage == 1 && mediaModeVideo);
        settingsPage->setVisible(newPage == 2);
        historyPage->setVisible(newPage == 3);

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

    // Merged media page: page 1 renders in audio layout (meter cards) or
    // video layout depending on the type of the last imported/loaded file.
    void setMediaMode(bool videoMode)
    {
        if (mediaModeVideo == videoMode)
            return;

        mediaModeVideo = videoMode;

        if (currentPage == 1)
        {
            metersPage->setVisible(!mediaModeVideo);
            videoPage->setVisible(mediaModeVideo);
        }

        repaint();
    }

    GoodMeterLookAndFeel lookAndFeel;
    std::unique_ptr<juce::PropertiesFile> appSettings;

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

    static constexpr int numPages = 4;  // Nono, Media (audio/video), Settings, History
    int currentPage = 0;
    bool mediaModeVideo = false;  // media page layout: false = audio meters, true = video
    float swipeStartX = 0.0f;
    bool isSwiping = false;
    bool suppressPageSwipe = false;
    bool navClickConsumed = false;
    bool isDarkTheme = false;
    float settingsIconRotation = 0.0f;
    float settingsIconTargetRotation = 0.0f;
    bool recordingActive = false;
    float recordingBorderPhase = 0.0f;
    FilledPillButton recordStopButton;
};
