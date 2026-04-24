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
#include "../VideoAudioExtractor.h"
#include "../GoodMeterLookAndFeel.h"
#include "iOSAudioEngine.h"
#include "MarkerModel.h"
#include "DigitalTimecodeRenderer.h"

// Marathon art style: iOS uses dot matrix rendering for BACKGROUND only
#define MARATHON_ART_STYLE 1

#include "../HoloNonoComponent.h"

#if MARATHON_ART_STYLE
    #include "MarathonRenderer.h"
    #include "MarathonNonoComponent.h"
#endif

class NonoPageComponent : public juce::Component,
                           public juce::Timer
{
public:
    enum class CharacterRenderMode { png = 0, ascii = 1 };
    struct FileMarkerEntry
    {
        juce::String filePath;
        std::vector<GoodMeterMarkerItem> items;
    };

    std::function<void(const juce::File&)> onImportedMediaCopied;
    std::function<void()> onMarkerDataChanged;

    NonoPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
#if MARATHON_ART_STYLE
        // Create background canvas (21x24, adjusted density)
        bgCanvas = std::make_unique<DotMatrixCanvas>(21, 24);
        clipFormatManager.registerBasicFormats();
        randomizeBackground();
        refreshClipLayerAssets();
#endif

        // Nono character (PNG sprite)
        holoNono = std::make_unique<HoloNonoComponent>(processor);
        holoNono->onImportFileChosen = [this](const juce::URL& url)
        {
            handleImportedUrl(url);
        };
        holoNono->setShowAnalysisResults(false);  // Disable bubble in Marathon mode
        holoNono->onTestTubeDoubleClicked = [this]()
        {
            if (!audioEngine.isFileLoaded())
                return;

            markerModeActive = true;
            markerModeJustEntered = true;
            repaint();
        };
        holoNono->onTestTubeLongPressed = [this]()
        {
            if (!markerModeActive)
                return;

            markerModeActive = false;
            markerModeJustEntered = false;
            repaint();
        };
        holoNono->isMarkerModeActive = [this]()
        {
            return markerModeActive && audioEngine.isFileLoaded();
        };
        holoNono->onMarkerModeDoubleClicked = [this]()
        {
            addMarkerAtCurrentPosition();
        };
        holoNono->onClearResultsRequested = [this]()
        {
            if (markerModeActive)
            {
                addMarkerAtCurrentPosition();
                return;
            }

            fileNameDotMatrixDismissed = false;
            textClearWaveActive = true;
            textClearWaveProgress = 0.0f;
        };
        addAndMakeVisible(holoNono.get());

        // Keep the ASCII implementation in code for now, but the visible iOS
        // flow is back to the PNG character path only.
        asciiNono = std::make_unique<MarathonNonoComponent>(processor);
        asciiNono->setInterceptsMouseClicks(false, false);
        asciiNono->setVisible(false);
        addAndMakeVisible(asciiNono.get());
        asciiNono->setSkin(holoNono->isGuoba()
            ? MarathonNonoComponent::SkinType::Guoba
            : MarathonNonoComponent::SkinType::Nono);
        asciiNono->setDarkTheme(isDarkTheme);
        syncCharacterRendererVisibility();

        // Import button
        importButton.setButtonText("IMPORT AUDIO");
        GoodMeterLookAndFeel::markAsIOSEnglishMono(importButton);
        importButton.onClick = [this]() { openImportDialog(); };
        importButton.setVisible(showImportButton);
        addAndMakeVisible(importButton);

        // File name label
        fileNameLabel.setJustificationType(juce::Justification::centred);
        GoodMeterLookAndFeel::markAsIOSEnglishMono(fileNameLabel);
        fileNameLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        fileNameLabel.setVisible(false);
        addChildComponent(fileNameLabel);

        startTimerHz(30);  // 30Hz for smooth ripple animation
    }

    ~NonoPageComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
#if MARATHON_ART_STYLE
        const auto baseColour = isDarkTheme ? juce::Colours::black
                                            : GoodMeterLookAndFeel::bgPanel;
        const auto bounds = getLocalBounds().toFloat();
        const bool hasResult = (holoNono && holoNono->hasAnalysisResult());

        g.fillAll(baseColour);
        drawAbstractBackgroundLayer(g, bounds, hasResult);
        g.setColour(baseColour.withAlpha(isDarkTheme ? 0.08f : 0.12f));
        g.fillRect(bounds);
        drawClipBlockLayer(g, bounds);

        // Draw analysis results
        if (hasResult)
        {
            drawAnalysisText(g);
        }

        if (audioEngine.isFileLoaded() && !fileNameLabel.getText().isEmpty()
            && (!fileNameDotMatrixDismissed || textClearWaveActive))
            drawDotMatrixFileName(g, fileNameLabel.getBounds().toFloat());

#else
        g.fillAll(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);
#endif
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto safeArea = bounds;

        if (showImportButton)
        {
            // Import button visible: Nono takes ~70% top, controls at bottom
            float controlH = juce::jmin(safeArea.getHeight() * 0.20f, 120.0f);
            auto controlArea = safeArea.removeFromBottom(static_cast<int>(controlH));
            holoNono->setBounds(safeArea);
            asciiNono->setBounds(safeArea);
            refreshAsciiSpriteSnapshot(true);

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
            asciiNono->setBounds(safeArea);
            refreshAsciiSpriteSnapshot(true);
        }

        focusCurrentClipInViewport();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (markerModeActive && audioEngine.isFileLoaded())
        {
            addMarkerAtCurrentPosition();
            return;
        }

        if (!showImportButton
            && currentRenderMode == CharacterRenderMode::ascii
            && asciiNono != nullptr
            && asciiNono->getBounds().contains(e.getPosition()))
        {
            openImportDialog();
            return;
        }

        juce::Component::mouseDoubleClick(e);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
#if MARATHON_ART_STYLE
        auto bounds = getLocalBounds().toFloat();
        float cellW = bounds.getWidth() / bgCanvas->getWidth();
        float cellH = bounds.getHeight() / bgCanvas->getHeight();

        longPressCenterX = (int)(e.position.x / cellW);
        longPressCenterY = (int)(e.position.y / cellH);
        dragStartX = longPressCenterX;
        dragStartY = longPressCenterY;
        pressStartTime = juce::Time::getMillisecondCounterHiRes();
        wasDragged = false;
        longPressActive = false;
        longPressRadius = 0.0f;
#endif
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
#if MARATHON_ART_STYLE
        auto bounds = getLocalBounds().toFloat();
        float cellW = bounds.getWidth() / bgCanvas->getWidth();
        float cellH = bounds.getHeight() / bgCanvas->getHeight();

        int newX = (int)(e.position.x / cellW);
        int newY = (int)(e.position.y / cellH);

        // Update long press center to follow finger
        longPressCenterX = newX;
        longPressCenterY = newY;

        int dx = newX - dragStartX;
        int dy = newY - dragStartY;
        int dragDist = (int)std::sqrt(dx*dx + dy*dy);

        if (dragDist > 1)
            wasDragged = true;
#endif
    }

    void mouseUp(const juce::MouseEvent&) override
    {
#if MARATHON_ART_STYLE
        double pressDuration = juce::Time::getMillisecondCounterHiRes() - pressStartTime;

        // Calculate drag distance and direction
        int dx = longPressCenterX - dragStartX;
        int dy = longPressCenterY - dragStartY;
        int dragDist = (int)std::sqrt(dx*dx + dy*dy);

        // Swipe gesture: trigger fan ripple
        if (wasDragged && dragDist > 3)
        {
            triggerFanRipple(dragStartX, dragStartY, dx, dy, dragDist);
        }
        // Quick tap: trigger full-screen ripple
        else if (!wasDragged && pressDuration < 200.0 && !rippleActive)
        {
            rippleCenterX = longPressCenterX;
            rippleCenterY = longPressCenterY;
            rippleRadius = 0.0f;
            rippleVelocity = 0.5f;
            rippleActive = true;
        }

        longPressActive = false;
        longPressRadius = 0.0f;
        longPressWaveCount = 0;
        pressStartTime = 0.0;
#endif
    }

    void timerCallback() override
    {
#if MARATHON_ART_STYLE
        if (rippleActive)
        {
            rippleUpdate();
        }

        if (fanRippleActive)
        {
            updateFanRipple();
        }

        // Long press continuous small ripple
        if (longPressActive)
        {
            // Calculate speed based on wave count (26 waves to reach max speed)
            float speedFactor = juce::jmin(1.0f, longPressWaveCount / 26.0f);
            float currentSpeed = 0.15f + speedFactor * 0.45f;  // 0.15 -> 0.6

            longPressRadius += currentSpeed;

            // Calculate max radius (increases after wave 20)
            float maxRadius = longPressMaxRadius;
            if (longPressWaveCount >= 20)
            {
                float extraRadius = (longPressWaveCount - 20) * 0.08f;
                maxRadius += extraRadius;
            }

            if (longPressRadius > maxRadius)
            {
                longPressRadius = 0.0f;
                longPressWaveCount++;
            }

            updateLongPressRipple();
        }
        else if (!longPressActive && pressStartTime > 0.0)
        {
            // Check if user is holding down (long press without drag)
            double pressDuration = juce::Time::getMillisecondCounterHiRes() - pressStartTime;
            if (pressDuration > 200.0)
            {
                longPressActive = true;
                longPressRadius = 0.0f;
                longPressWaveCount = 0;
            }
        }

        // Auto ripple every 15 seconds
        autoRippleTimer += 0.033f;  // 30Hz timer
        if (autoRippleTimer >= 15.0f && !rippleActive)
        {
            autoRippleTimer = 0.0f;
            triggerAutoRipple();
        }

        // Text clear dissolve animation
        if (textClearWaveActive)
        {
            textClearWaveProgress += 0.028f;  // ~1.2 second dissolve at 30fps
            if (textClearWaveProgress >= 1.0f)
            {
                textClearWaveActive = false;
                textClearWaveProgress = 0.0f;
                fileNameDotMatrixDismissed = true;
                // Clear the analysis data after animation completes
                if (holoNono)
                    holoNono->clearAnalysisResults();
            }
            repaint();
        }

        // Check if analysis result changed
        static bool hadResult = false;
        bool hasResult = (holoNono && holoNono->hasAnalysisResult());
        if (hasResult != hadResult)
        {
            hadResult = hasResult;
            repaint();
        }
#endif

        if (audioEngine.isFileLoaded())
        {
            auto currentName = audioEngine.getCurrentFileName();
            auto currentPath = audioEngine.getCurrentFilePath();
            bool needsLayout = fileNameLabel.getText().isEmpty();
            const auto displayText = markerModeActive
                ? getCurrentMarkerTimecodeText()
                : currentName;

            if (lastDotMatrixFilePath != currentPath)
            {
                lastDotMatrixFilePath = currentPath;
                fileNameDotMatrixDismissed = false;
                needsLayout = true;
                markerModeJustEntered = false;
            }

            if (fileNameLabel.getText() != displayText)
            {
                fileNameLabel.setText(displayText, juce::dontSendNotification);
                if (!markerModeActive)
                    fileNameDotMatrixDismissed = false;
                needsLayout = needsLayout || markerModeJustEntered;
            }

            if (needsLayout)
                resized();

            markerModeJustEntered = false;
        }
        else if (!fileNameLabel.getText().isEmpty())
        {
            fileNameLabel.setText({}, juce::dontSendNotification);
            fileNameDotMatrixDismissed = false;
            lastDotMatrixFilePath.clear();
            markerModeActive = false;
            markerModeJustEntered = false;
            resized();
        }

#if MARATHON_ART_STYLE
        auto mapBandDbToDisplay = [] (float db) -> float
        {
            float target = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f));
            target = std::pow(target, 0.5f);
            return target < 0.005f ? 0.0f : target;
        };

        if (audioEngine.isPlaying())
        {
            const float targetLow = mapBandDbToDisplay(processor.rmsLevelLow.load(std::memory_order_relaxed));
            const float targetMid = mapBandDbToDisplay(processor.rmsLevelMid3Band.load(std::memory_order_relaxed));
            const float targetHigh = mapBandDbToDisplay(processor.rmsLevelHigh.load(std::memory_order_relaxed));
            const float smoothing = 0.26f;
            dotWaveBandLow += (targetLow - dotWaveBandLow) * smoothing;
            dotWaveBandMid += (targetMid - dotWaveBandMid) * smoothing;
            dotWaveBandHigh += (targetHigh - dotWaveBandHigh) * smoothing;
        }
        else
        {
            dotWaveBandLow += (0.0f - dotWaveBandLow) * 0.18f;
            dotWaveBandMid += (0.0f - dotWaveBandMid) * 0.18f;
            dotWaveBandHigh += (0.0f - dotWaveBandHigh) * 0.18f;
        }

        if (currentRenderMode == CharacterRenderMode::ascii)
        {
            asciiFieldPhase += 0.042f;
            refreshAsciiSpriteSnapshot();
            repaint();
        }

        if (audioEngine.isPlaying() || markerModeActive)
            repaint();
#endif
    }

    //==========================================================================
    // Public API — called by iOSMainComponent
    //==========================================================================

    /** Get reference to HoloNono for external wiring */
    HoloNonoComponent* getHoloNono() { return holoNono.get(); }

    bool isMarkerModeEnabled() const { return markerModeActive; }

    void addMarkerAtCurrentPositionFromExternal()
    {
        addMarkerAtCurrentPosition();
    }

    std::vector<double> getMarkersForCurrentFile() const
    {
        std::vector<double> times;
        for (const auto& item : getMarkerItemsForCurrentFile())
            times.push_back(item.seconds);
        return times;
    }

    std::vector<GoodMeterMarkerItem> getMarkerItemsForCurrentFile() const
    {
        if (const auto* markers = getMarkerItemsForPath(getCurrentMarkerFileKey()))
            return markers->items;
        return {};
    }

    void updateMarkerNoteForCurrentFile(const juce::String& markerId, const juce::String& note)
    {
        if (auto* markers = getMarkerItemsForPath(getCurrentMarkerFileKey()))
        {
            for (auto& item : markers->items)
            {
                if (item.id == markerId)
                {
                    item.note = note;
                    break;
                }
            }
        }
    }

    void updateMarkerTagsForCurrentFile(const juce::String& markerId, const juce::StringArray& tags)
    {
        if (auto* markers = getMarkerItemsForPath(getCurrentMarkerFileKey()))
        {
            for (auto& item : markers->items)
            {
                if (item.id == markerId)
                {
                    item.tags = tags;
                    break;
                }
            }
        }
    }

    juce::String getCurrentMarkerFilePath() const
    {
        return getCurrentMarkerFileKey();
    }

    juce::String getCurrentMarkerDisplayName() const
    {
        if (const auto* item = getCurrentClipLayerItem())
            return juce::URL::removeEscapeChars(item->mediaFile.getFileName());

        return juce::URL::removeEscapeChars(audioEngine.getCurrentFileName());
    }

    juce::String getCurrentMarkerMetadataSummary() const
    {
        if (const auto* item = getCurrentClipLayerItem())
            return item->technicalSummary;

        return {};
    }

    double getCurrentMarkerSourceDurationSeconds() const
    {
        double duration = juce::jmax(0.0, audioEngine.getTotalLength());

        if (const auto* item = getCurrentClipLayerItem())
            duration = juce::jmax(duration, item->durationSeconds);

        return duration;
    }

    juce::String getCurrentMarkerTimecodeTextPublic() const
    {
        return getCurrentMarkerTimecodeText();
    }

    juce::String formatMarkerTimecodeForDisplay(double seconds) const
    {
        return formatMarkerTimecodeForCurrentFile(seconds);
    }

    /** Show or hide the IMPORT AUDIO button */
    void setShowImportButton(bool show)
    {
        showImportButton = show;
        importButton.setVisible(show);
        resized();
        repaint();
    }

    void setShowClipFileNames(bool show)
    {
        showClipFileNames = show;
        repaint();
    }

    void setDarkTheme(bool dark)
    {
        isDarkTheme = dark;

        // Codex: 主人要把 iOS 五页统一进同一套主题系统。
        // 我这里先只改 iOS 第 1 页自己的页面层，不去污染插件版和 standalone 共用组件。
        fileNameLabel.setColour(juce::Label::textColourId,
                                isDarkTheme ? juce::Colours::white.withAlpha(0.78f)
                                            : GoodMeterLookAndFeel::textMuted);
        importButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        importButton.setColour(juce::TextButton::textColourOffId,
                               isDarkTheme ? juce::Colours::white.withAlpha(0.88f)
                                           : GoodMeterLookAndFeel::textMain);
        importButton.setColour(juce::TextButton::textColourOnId,
                               isDarkTheme ? juce::Colours::white.withAlpha(0.96f)
                                           : GoodMeterLookAndFeel::textMain);
        if (asciiNono != nullptr)
            asciiNono->setDarkTheme(dark);
        if (currentRenderMode == CharacterRenderMode::ascii)
            refreshAsciiSpriteSnapshot(true);
        randomizeBackground();
        refreshClipLayerThemeState();
        repaint();
    }

    bool isDark() const { return isDarkTheme; }

    /** Change character skin */
    void setSkin(int skinId)
    {
        if (holoNono == nullptr) return;
        switch (skinId)
        {
            case 1:
                holoNono->setSkin(HoloNonoComponent::SkinType::Nono);
                if (asciiNono != nullptr)
                    asciiNono->setSkin(MarathonNonoComponent::SkinType::Nono);
                refreshAsciiSpriteSnapshot(true);
                randomizeBackground();
                repaint();
                break;
            case 2:
                holoNono->setSkin(HoloNonoComponent::SkinType::Guoba);
                if (asciiNono != nullptr)
                    asciiNono->setSkin(MarathonNonoComponent::SkinType::Guoba);
                refreshAsciiSpriteSnapshot(true);
                randomizeBackground();
                repaint();
                break;
            default: break;
        }
    }

    int getCurrentSkinId() const
    {
        if (holoNono && holoNono->isGuoba()) return 2;
        return 1;
    }

    void setCharacterRenderMode(int mode)
    {
        juce::ignoreUnused(mode);
        currentRenderMode = CharacterRenderMode::png;
        syncCharacterRendererVisibility();
    }

    int getCharacterRenderMode() const
    {
        return 0;
    }

    bool loadLibraryFile(const juce::File& file)
    {
        if (isVideoFile(file))
            return startVideoLoad(file, false);

        return loadAnalyzedFile(file);
    }

private:
    void syncCharacterRendererVisibility()
    {
        const bool showAscii = false;

        if (holoNono != nullptr)
            holoNono->setVisible(!showAscii);
        if (asciiNono != nullptr)
            asciiNono->setVisible(showAscii);

        repaint();
    }

    void refreshAsciiSpriteSnapshot(bool force = false)
    {
        if (asciiNono == nullptr || holoNono == nullptr)
            return;

        if (!force)
        {
            ++asciiSnapshotCounter;
            if ((asciiSnapshotCounter % 2) != 0)
                return;
        }

        auto fullBounds = holoNono->getLocalBounds();
        auto captureBounds = holoNono->getAsciiCaptureBounds().getSmallestIntegerContainer()
                             .getIntersection(fullBounds);
        if (fullBounds.isEmpty() || captureBounds.isEmpty())
            return;

        juce::Image full(juce::Image::ARGB, fullBounds.getWidth(), fullBounds.getHeight(), true);
        {
            juce::Graphics gg(full);
            gg.fillAll(juce::Colours::transparentBlack);
            const bool wasVisible = holoNono->isVisible();
            holoNono->setVisible(true);
            holoNono->paintEntireComponent(gg, false);
            holoNono->setVisible(wasVisible);
        }

        juce::Image cropped(juce::Image::ARGB, captureBounds.getWidth(), captureBounds.getHeight(), true);
        {
            juce::Graphics cg(cropped);
            cg.fillAll(juce::Colours::transparentBlack);
            cg.drawImageAt(full, -captureBounds.getX(), -captureBounds.getY());
        }

        asciiNono->setSourceSpriteImage(cropped);
    }

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

        refreshClipLayerAssets();

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
        if (asciiNono != nullptr)
            asciiNono->triggerExtractExpression();

        auto safeThis = juce::Component::SafePointer<NonoPageComponent>(this);
        VideoAudioExtractor::extractAudio(videoFile, outputFile,
            [safeThis, outputFile](bool success)
            {
                if (auto* self = safeThis.getComponent())
                {
                    if (self->holoNono != nullptr)
                        self->holoNono->stopExtractExpression();
                    if (self->asciiNono != nullptr)
                        self->asciiNono->stopExtractExpression();

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
        if (asciiNono != nullptr)
            asciiNono->analyzeFile(file);

        refreshClipLayerAssets();
        focusCurrentClipInViewport();
        fileNameLabel.setText(audioEngine.getCurrentFileName(), juce::dontSendNotification);
        fileNameLabel.setVisible(false);
        fileNameDotMatrixDismissed = false;
        lastDotMatrixFilePath = audioEngine.getCurrentFilePath();
        resized();
        return true;
    }

    struct ClipLayerItem
    {
        juce::File mediaFile;
        juce::File waveformFile;
        juce::String displayName;
        std::vector<float> waveformPeaks;
        double durationSeconds = 0.0;
        double markerTimecodeFps = 30.0;
        double markerTimecodeStartSeconds = 0.0;
        uint32_t layoutHash = 0;
        juce::Colour waveformColour = juce::Colours::white;
        juce::String technicalSummary;
        bool isVideo = false;
        bool isCurrent = false;
        bool hasTimecodeMetadata = false;
        bool hasReadableStartTimecode = false;
        bool usesDropFrameTimecode = false;
        int timecodeFrameQuanta = 30;
        int64_t timecodeStartFrameNumber = 0;
        juce::String startTimecodeText;
    };

    struct PositionedClip
    {
        const ClipLayerItem* item = nullptr;
        int track = 0;
        double startSeconds = 0.0;
        double visualDuration = 0.0;
    };

    static bool isAudioFile(const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".aif"
            || ext == ".flac" || ext == ".ogg" || ext == ".m4a" || ext == ".caf";
    }

    static bool isExtractedVideoAudioProxy(const juce::File& file)
    {
        return file.getFileName().startsWith("Extract_") && isAudioFile(file);
    }

    juce::Colour getClipWaveformColour(int index, bool darkTheme) const
    {
        static const juce::Colour darkPalette[] =
        {
            juce::Colour(0xFF79E6C4), // mint
            juce::Colour(0xFF65C7FF), // cyan blue
            juce::Colour(0xFF9DDB68), // lime
            juce::Colour(0xFFB98BFF), // violet
            juce::Colour(0xFFFF8E73), // coral
            juce::Colour(0xFFFFD36E), // amber
            juce::Colour(0xFF7FD8E6), // aqua
            juce::Colour(0xFFD8A5FF)  // lilac
        };

        static const juce::Colour lightPalette[] =
        {
            juce::Colour(0xFF36B48F),
            juce::Colour(0xFF2496D2),
            juce::Colour(0xFF73A63E),
            juce::Colour(0xFF8A65D1),
            juce::Colour(0xFFD36C58),
            juce::Colour(0xFFD49A1E),
            juce::Colour(0xFF2F9FB3),
            juce::Colour(0xFFB35CB3)
        };

        const auto& palette = darkTheme ? darkPalette : lightPalette;
        return palette[index % static_cast<int>(std::size(darkTheme ? darkPalette : lightPalette))];
    }

    std::vector<float> buildClipWaveformPeaks(const juce::File& file,
                                              int desiredPeakCount,
                                              double& outDurationSeconds)
    {
        outDurationSeconds = 0.0;

        if (!file.existsAsFile())
            return {};

        std::unique_ptr<juce::AudioFormatReader> reader(clipFormatManager.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            return {};

        outDurationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;

        const int sourceLimitedCount = (int) juce::jmin<juce::int64>((juce::int64) desiredPeakCount,
                                                                     reader->lengthInSamples);
        const int peakCount = juce::jlimit(48, 220, sourceLimitedCount);
        std::vector<float> peaks(static_cast<size_t>(peakCount), 0.0f);

        juce::AudioBuffer<float> scratch((int) juce::jmax<uint32_t>(1u, reader->numChannels), 2048);
        const juce::int64 totalSamples = reader->lengthInSamples;

        for (int peakIndex = 0; peakIndex < peakCount; ++peakIndex)
        {
            const juce::int64 startSample = (totalSamples * peakIndex) / peakCount;
            const juce::int64 endSample = juce::jmax<juce::int64>(startSample + 1,
                                                                  (totalSamples * (peakIndex + 1)) / peakCount);

            float peak = 0.0f;
            for (juce::int64 pos = startSample; pos < endSample; pos += scratch.getNumSamples())
            {
                const int chunk = (int) juce::jmin<juce::int64>(scratch.getNumSamples(), endSample - pos);
                scratch.clear();
                reader->read(&scratch, 0, chunk, pos, true, true);

                for (int ch = 0; ch < scratch.getNumChannels(); ++ch)
                {
                    const float* data = scratch.getReadPointer(ch);
                    for (int s = 0; s < chunk; ++s)
                        peak = juce::jmax(peak, std::abs(data[s]));
                }
            }

            peaks[(size_t) peakIndex] = juce::jlimit(0.0f, 1.0f, peak);
        }

        return peaks;
    }

    void refreshClipLayerAssets()
    {
        juce::Array<juce::File> files;
        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        docsDir.findChildFiles(files, juce::File::findFiles, false);

        std::vector<juce::File> mediaFiles;
        mediaFiles.reserve((size_t) files.size());

        for (const auto& file : files)
        {
            if (isVideoFile(file))
            {
                mediaFiles.push_back(file);
                continue;
            }

            if (isAudioFile(file) && !isExtractedVideoAudioProxy(file))
                mediaFiles.push_back(file);
        }

        std::sort(mediaFiles.begin(), mediaFiles.end(),
                  [] (const juce::File& a, const juce::File& b)
                  {
                      return a.getLastModificationTime() < b.getLastModificationTime();
                  });

        std::vector<ClipLayerItem> newItems;
        newItems.reserve(mediaFiles.size());

        const auto currentPath = audioEngine.getCurrentFilePath();

        for (size_t index = 0; index < mediaFiles.size(); ++index)
        {
            const auto& media = mediaFiles[index];
            ClipLayerItem item;
            item.mediaFile = media;
            item.isVideo = isVideoFile(media);
            item.waveformFile = item.isVideo ? getExtractedAudioFileForVideo(media) : media;
            item.displayName = juce::URL::removeEscapeChars(media.getFileNameWithoutExtension());
            if (item.displayName.length() > 18)
                item.displayName = item.displayName.substring(0, 16) + "..";

            item.layoutHash = (uint32_t) item.displayName.hashCode();
            item.waveformColour = getClipWaveformColour((int) index, isDarkTheme);
            item.isCurrent = currentPath.isNotEmpty()
                          && (currentPath == media.getFullPathName()
                              || currentPath == item.waveformFile.getFullPathName());

            if (item.isVideo)
            {
                const auto timingInfo = VideoAudioExtractor::getVideoTimingInfo(media);
                item.markerTimecodeFps = normaliseMarkerDisplayFps(timingInfo.nominalFrameRate);
                item.markerTimecodeStartSeconds = timingInfo.timecodeStartSeconds;
                item.hasTimecodeMetadata = timingInfo.hasTimecodeMetadata;
                item.hasReadableStartTimecode = timingInfo.hasReadableStartTimecode;
                item.usesDropFrameTimecode = timingInfo.usesDropFrameTimecode;
                item.timecodeFrameQuanta = juce::jmax(1, timingInfo.timecodeFrameQuanta);
                item.timecodeStartFrameNumber = timingInfo.startFrameNumber;
                item.startTimecodeText = timingInfo.startTimecodeText;
                item.technicalSummary = timingInfo.technicalSummary;
            }
            else
            {
                const auto timingInfo = VideoAudioExtractor::getAudioTimingInfo(media);
                item.markerTimecodeFps = normaliseMarkerDisplayFps(timingInfo.markerTimecodeFps);
                item.markerTimecodeStartSeconds = 0.0;
                item.hasTimecodeMetadata = timingInfo.hasTimecodeMetadata;
                item.hasReadableStartTimecode = timingInfo.hasReadableStartTimecode;
                item.usesDropFrameTimecode = timingInfo.usesDropFrameTimecode;
                item.timecodeFrameQuanta = juce::jmax(1, timingInfo.timecodeFrameQuanta);
                item.timecodeStartFrameNumber = timingInfo.startFrameNumber;
                item.startTimecodeText = timingInfo.startTimecodeText;
                item.technicalSummary = timingInfo.technicalSummary;
            }

            if (item.isVideo && !item.waveformFile.existsAsFile())
                ensureClipWaveformProxy(item.mediaFile, item.waveformFile);

            double durationSeconds = 0.0;
            if (item.waveformFile.existsAsFile())
                item.waveformPeaks = buildClipWaveformPeaks(item.waveformFile, 180, durationSeconds);

            if (durationSeconds <= 0.0)
                durationSeconds = 4.5 + (double) ((item.layoutHash % 70u) / 20.0);

            item.durationSeconds = durationSeconds;
            newItems.push_back(std::move(item));
        }

        clipLayerItems = std::move(newItems);
        focusCurrentClipInViewport();
        repaint();
    }

    void refreshClipLayerThemeState()
    {
        const auto currentPath = audioEngine.getCurrentFilePath();
        for (size_t index = 0; index < clipLayerItems.size(); ++index)
        {
            auto& item = clipLayerItems[index];
            item.waveformColour = getClipWaveformColour((int) index, isDarkTheme);
            item.isCurrent = currentPath.isNotEmpty()
                          && (currentPath == item.mediaFile.getFullPathName()
                              || currentPath == item.waveformFile.getFullPathName());
        }
    }

    std::vector<PositionedClip> buildClipLayout(int visibleTracks, double secondsPerScreen) const
    {
        std::vector<PositionedClip> positioned;
        if (visibleTracks <= 0 || clipLayerItems.empty())
            return positioned;

        double minDurationSeconds = std::numeric_limits<double>::max();
        double maxDurationSeconds = 0.0;
        for (const auto& item : clipLayerItems)
        {
            minDurationSeconds = juce::jmin(minDurationSeconds, item.durationSeconds);
            maxDurationSeconds = juce::jmax(maxDurationSeconds, item.durationSeconds);
        }

        if (!std::isfinite(minDurationSeconds))
            minDurationSeconds = 0.0;

        const double minVisualDuration = secondsPerScreen * 0.16;
        const double maxVisualDuration = secondsPerScreen * 0.92;
        const double spread = juce::jmax(0.001, maxDurationSeconds - minDurationSeconds);

        std::vector<double> trackCursors((size_t) visibleTracks, 0.0);

        positioned.reserve(clipLayerItems.size());

        for (size_t itemIndex = 0; itemIndex < clipLayerItems.size(); ++itemIndex)
        {
            const auto& item = clipLayerItems[itemIndex];
            const int track = (int) (itemIndex % (size_t) visibleTracks);
            double normalised = 0.0;
            if (maxDurationSeconds <= 0.001)
            {
                normalised = 0.0;
            }
            else if (spread < 0.25)
            {
                normalised = juce::jlimit(0.0, 1.0, item.durationSeconds / maxDurationSeconds);
            }
            else
            {
                normalised = juce::jlimit(0.0, 1.0,
                                          (item.durationSeconds - minDurationSeconds) / spread);
            }

            const double visualNorm = std::pow(normalised, 0.82);
            const double duration = juce::jmap(visualNorm, 0.0, 1.0,
                                               minVisualDuration, maxVisualDuration);
            const double start = trackCursors[(size_t) track];
            const double gap = 0.14 + (double) ((item.layoutHash >> 11) % 100u) / 100.0 * 0.22;

            positioned.push_back(PositionedClip { &item, track, start, duration });
            trackCursors[(size_t) track] = start + duration + gap;
        }

        const double targetTrackSpan = secondsPerScreen * 1.01;
        for (int track = 0; track < visibleTracks; ++track)
        {
            const double trackSpan = trackCursors[(size_t) track];
            if (trackSpan <= 0.001 || trackSpan >= targetTrackSpan)
                continue;

            const double scale = targetTrackSpan / trackSpan;
            for (auto& placed : positioned)
            {
                if (placed.track != track)
                    continue;

                placed.startSeconds *= scale;
                placed.visualDuration *= scale;
            }

            trackCursors[(size_t) track] = targetTrackSpan;
        }

        return positioned;
    }

    void focusCurrentClipInViewport()
    {
        const auto currentPath = audioEngine.getCurrentFilePath();
        if (currentPath.isEmpty())
        {
            clipViewportStartSeconds = 0.0;
            return;
        }

        const bool landscape = getWidth() > getHeight();
        const int visibleTracks = landscape ? 4 : 10;
        const double secondsPerScreen = landscape ? 12.0 : 18.0;
        const auto layout = buildClipLayout(visibleTracks, secondsPerScreen);
        double timelineEnd = 0.0;
        for (const auto& placed : layout)
            timelineEnd = juce::jmax(timelineEnd, placed.startSeconds + placed.visualDuration);

        for (const auto& placed : layout)
        {
            if (placed.item == nullptr)
                continue;

            if (currentPath == placed.item->mediaFile.getFullPathName()
                || currentPath == placed.item->waveformFile.getFullPathName())
            {
                const double clipEnd = placed.startSeconds + placed.visualDuration;
                double targetStart = std::floor(placed.startSeconds / secondsPerScreen) * secondsPerScreen;
                if (clipEnd > targetStart + secondsPerScreen)
                    targetStart = clipEnd - secondsPerScreen;

                const double maxViewportStart = juce::jmax(0.0, timelineEnd - secondsPerScreen);
                targetStart = juce::jlimit(0.0, maxViewportStart, targetStart);

                clipViewportStartSeconds = targetStart;
                return;
            }
        }

        clipViewportStartSeconds = 0.0;
    }

    void ensureClipWaveformProxy(const juce::File& videoFile, const juce::File& outputFile)
    {
        const auto key = videoFile.getFullPathName();
        if (pendingClipExtractions.contains(key))
            return;

        pendingClipExtractions.add(key);
        auto safeThis = juce::Component::SafePointer<NonoPageComponent>(this);
        VideoAudioExtractor::extractAudio(videoFile, outputFile,
            [safeThis, key](bool)
            {
                if (auto* self = safeThis.getComponent())
                {
                    self->pendingClipExtractions.removeString(key);
                    self->refreshClipLayerAssets();
                }
            });
    }

    void drawClipWaveform(juce::Graphics& g,
                          juce::Rectangle<float> area,
                          const std::vector<float>& peaks,
                          juce::Colour colour,
                          bool isCurrentClip) const
    {
        if (area.isEmpty())
            return;

        const auto waveformColour = colour.withMultipliedAlpha(isDarkTheme
                                                               ? (isCurrentClip ? 0.98f : 0.90f)
                                                               : (isCurrentClip ? 0.94f : 0.84f));

        const float centreY = area.getCentreY();
        const float halfHeight = area.getHeight() * 0.46f;

        g.setColour(waveformColour.withAlpha(isDarkTheme ? 0.18f : 0.14f));
        g.fillRect(area.withY(centreY - 0.5f).withHeight(1.0f));

        if (peaks.empty())
        {
            g.setColour(waveformColour.withAlpha(isDarkTheme ? 0.42f : 0.35f));
            g.drawRect(area, 1.0f);
            return;
        }

        const int columns = juce::jmax(16, (int) std::floor(area.getWidth()));
        juce::Path fillPath;
        juce::Path strokePath;

        for (int i = 0; i < columns; ++i)
        {
            const int sampleIndex = juce::jlimit(0, (int) peaks.size() - 1,
                                                 (int) std::floor(((float) i / juce::jmax(1, columns - 1))
                                                                  * (float) (peaks.size() - 1)));
            const float amplitude = std::pow(peaks[(size_t) sampleIndex], 0.70f);
            const float extent = juce::jmax(1.0f, amplitude * halfHeight);
            const float x = area.getX() + (float) i;
            const float yTop = centreY - extent;
            const float yBottom = centreY + extent;

            if (i == 0)
            {
                fillPath.startNewSubPath(x, yTop);
                strokePath.startNewSubPath(x, yTop);
            }
            else
            {
                fillPath.lineTo(x, yTop);
                strokePath.lineTo(x, yTop);
            }

            if ((i % 9) == 0 && amplitude > 0.35f)
                g.drawLine(x, yTop, x, yBottom, isCurrentClip ? 1.25f : 1.0f);
        }

        for (int i = columns - 1; i >= 0; --i)
        {
            const int sampleIndex = juce::jlimit(0, (int) peaks.size() - 1,
                                                 (int) std::floor(((float) i / juce::jmax(1, columns - 1))
                                                                  * (float) (peaks.size() - 1)));
            const float amplitude = std::pow(peaks[(size_t) sampleIndex], 0.70f);
            const float extent = juce::jmax(1.0f, amplitude * halfHeight);
            const float x = area.getX() + (float) i;
            fillPath.lineTo(x, centreY + extent);
        }

        fillPath.closeSubPath();

        g.setColour(waveformColour.withAlpha(isDarkTheme ? 0.34f : 0.28f));
        g.fillPath(fillPath);

        g.setColour(waveformColour.withAlpha(isCurrentClip ? 1.0f : 0.92f));
        g.strokePath(strokePath, juce::PathStrokeType(isCurrentClip ? 1.55f : 1.25f));
    }

    void drawAbstractBackgroundLayer(juce::Graphics& g,
                                     juce::Rectangle<float> bounds,
                                     bool hasResult)
    {
        juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain);
        static constexpr float sizeScales[] = { 0.56f, 0.82f, 1.04f, 1.28f };
        const int gridH = bgCanvas->getHeight();
        const int gridW = bgCanvas->getWidth();
        const float cellW = bounds.getWidth() / (float) gridW;
        const float cellH = bounds.getHeight() / (float) gridH;

        for (int y = 0; y < gridH; ++y)
        {
            if (hasResult && !textClearWaveActive && (y == gridH - 5 || y == gridH - 4))
                continue;

            for (int x = 0; x < gridW; ++x)
            {
                const auto cell = bgCanvas->getCell(x, y);
                const float px = (float) x * cellW;
                const float py = (float) y * cellH;

                const float scale = sizeScales[juce::jlimit<int>(0, 3, static_cast<int>(cell.sizeLevel))];
                float symbolScale = scale;
                auto symbolColour = isDarkTheme
                    ? cell.color.withMultipliedAlpha(cell.brightness)
                    : cell.color.interpolatedWith(GoodMeterLookAndFeel::textMain, 0.34f)
                        .withAlpha(0.070f + cell.brightness * 0.24f);

                if (currentRenderMode == CharacterRenderMode::ascii)
                {
                    const float nx = ((float) x + 0.5f) / (float) gridW;
                    const float ny = ((float) y + 0.5f) / (float) gridH;
                    const float dx = (nx - 0.5f) / 0.23f;
                    const float dy = (ny - 0.31f) / 0.16f;
                    const float focus = std::exp(-(dx * dx + dy * dy) * 1.15f);
                    const float suppression = isDarkTheme ? 0.82f : 0.90f;
                    symbolColour = symbolColour.withMultipliedAlpha(1.0f - focus * suppression);
                }

                juce::String str;

                if (cell.symbol == U'⠿')
                {
                    static const char32_t darkAbstractSymbols[] = { U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯', U'=' };
                    static const char32_t lightAbstractSymbols[] = { U'/', U'\\', U'✕', U'+', U'=', U'·' };
                    const uint32_t hash = backgroundHash(x + 19, y + 43);
                    const uint32_t densityGate = isDarkTheme ? 36u : 28u;
                    if ((hash % 100u) > densityGate)
                        continue;

                    const auto& symbolSet = isDarkTheme ? darkAbstractSymbols : lightAbstractSymbols;
                    const auto symbolCount = isDarkTheme
                        ? static_cast<uint32_t>(std::size(darkAbstractSymbols))
                        : static_cast<uint32_t>(std::size(lightAbstractSymbols));
                    str = juce::String::charToString(symbolSet[hash % symbolCount]);
                    symbolColour = isDarkTheme
                        ? juce::Colour(0xFFF2ECE2).withAlpha(0.040f + cell.brightness * 0.085f)
                        : [&]()
                        {
                            const bool guoba = getCurrentSkinId() == 2;
                            const auto accent = getAsciiAccentColour();
                            const auto secondary = getAsciiSecondaryColour();
                            const uint32_t tintBucket = (hash / 7u) % 10u;
                            juce::Colour base = guoba ? accent : secondary;
                            float alpha = 0.18f;
                            if (tintBucket < 5u)
                                base = guoba ? accent : secondary;
                            else if (tintBucket < 8u)
                            {
                                base = guoba ? secondary : accent;
                                alpha = 0.14f;
                            }
                            else
                                base = accent.interpolatedWith(secondary, 0.5f);
                            return base.withAlpha(alpha + cell.brightness * 0.06f);
                        }();
                }
                else
                {
                    str = juce::String::charToString(cell.symbol);
                    if (!isDarkTheme && cell.symbol != U' ')
                        symbolColour = cell.color.withAlpha(0.40f + cell.brightness * 0.35f);
                }

                g.setColour(symbolColour);
                g.setFont(monoFont.withHeight(monoFont.getHeight() * symbolScale));
                const float drawW = cellW * juce::jmin(1.34f, scale + 0.16f);
                const float drawH = cellH * juce::jmin(1.34f, scale + 0.12f);
                g.drawText(str,
                           juce::Rectangle<int>((int) (px + (cellW - drawW) * 0.5f),
                                                (int) (py + (cellH - drawH) * 0.5f),
                                                (int) drawW,
                                                (int) drawH),
                           juce::Justification::centred, false);
            }
        }
    }

    void drawClipBlockLayer(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const bool landscape = bounds.getWidth() > bounds.getHeight();
        const int visibleTracks = landscape ? 4 : 10;
        const float topInset = landscape ? 14.0f : 18.0f;
        const float bottomInset = landscape ? 22.0f : 28.0f;
        const float sideInset = 0.0f;

        auto clipArea = bounds.reduced(sideInset, 0.0f)
                              .withTrimmedTop(topInset)
                              .withTrimmedBottom(bottomInset);

        if (clipArea.isEmpty() || visibleTracks <= 0)
            return;

        const float trackGap = 0.0f;
        const float trackHeight = clipArea.getHeight() / (float) visibleTracks;
        const float secondsPerScreen = landscape ? 12.0f : 18.0f;
        const double playheadSeconds = audioEngine.getCurrentPosition();
        const double playheadDuration = juce::jmax(0.001, audioEngine.getTotalLength());
        const bool spotlightModeActive = audioEngine.isFileLoaded() && audioEngine.isPlaying();

        const auto laneLine = isDarkTheme ? juce::Colours::white.withAlpha(0.07f)
                                          : GoodMeterLookAndFeel::textMain.withAlpha(0.06f);
        const auto clipFill = isDarkTheme ? juce::Colours::transparentBlack
                                          : GoodMeterLookAndFeel::textMain.withAlpha(0.035f);
        const auto clipOutline = isDarkTheme ? juce::Colours::transparentBlack
                                             : GoodMeterLookAndFeel::textMain.withAlpha(0.14f);
        const auto labelColour = isDarkTheme ? juce::Colours::white.withAlpha(0.62f)
                                             : GoodMeterLookAndFeel::textMain.withAlpha(0.52f);
        const auto playheadOrange = juce::Colour(0xFFFF8C00);

        g.setColour(laneLine);
        for (int track = 0; track <= visibleTracks; ++track)
        {
            const float y = clipArea.getY() + trackHeight * (float) track;
            g.drawHorizontalLine((int) std::round(y), clipArea.getX(), clipArea.getRight());
        }

        if (clipLayerItems.empty())
            return;
        const auto positioned = buildClipLayout(visibleTracks, secondsPerScreen);
        const double viewportStart = clipViewportStartSeconds;
        const double viewportEnd = viewportStart + secondsPerScreen;
        double renderStart = viewportStart;
        double renderEnd = viewportEnd;
        bool hasVisibleEnvelope = false;

        for (const auto& placed : positioned)
        {
            const double clipStart = placed.startSeconds;
            const double clipEnd = placed.startSeconds + placed.visualDuration;
            if (clipEnd <= viewportStart || clipStart >= viewportEnd)
                continue;

            if (!hasVisibleEnvelope)
            {
                renderStart = clipStart;
                renderEnd = clipEnd;
                hasVisibleEnvelope = true;
            }
            else
            {
                renderStart = juce::jmin(renderStart, clipStart);
                renderEnd = juce::jmax(renderEnd, clipEnd);
            }
        }

        const double renderSpan = juce::jmax(0.001, renderEnd - renderStart);
        const float pixelsPerSecond = clipArea.getWidth() / (float) renderSpan;
        std::vector<juce::Rectangle<float>> clipRects(positioned.size());
        std::vector<std::vector<int>> visibleIndicesByTrack((size_t) visibleTracks);

        auto heroAvoidRect = [&]() -> juce::Rectangle<float>
        {
            if (holoNono == nullptr || !holoNono->isVisible())
                return {};

            auto localRect = holoNono->getAsciiCaptureBounds();
            if (localRect.isEmpty())
                return {};

            auto rect = localRect.translated((float) holoNono->getX(), (float) holoNono->getY());
            rect = rect.expanded(landscape ? 10.0f : 8.0f,
                                 landscape ? 8.0f : 12.0f);

            // Avoid the main body, but don't carve a large empty pocket on the
            // sprite's right side — it's fine if clips tuck behind the tube.
            const float keepWidth = rect.getWidth() * (holoNono->isGuoba() ? 0.70f : 0.76f);
            rect.setWidth(keepWidth);
            return rect;
        }();

        for (size_t index = 0; index < positioned.size(); ++index)
        {
            const auto& placed = positioned[index];
            const float laneY = clipArea.getY() + (trackHeight + trackGap) * (float) placed.track;
            const float clipHeight = trackHeight;
            const float clipWidth = juce::jmax(54.0f, (float) (placed.visualDuration * pixelsPerSecond));
            const float x = clipArea.getX()
                          + (float) ((placed.startSeconds - renderStart) * pixelsPerSecond);

            clipRects[index] = juce::Rectangle<float>(x, laneY, clipWidth, clipHeight);
            if (clipRects[index].getRight() > clipArea.getX()
                && clipRects[index].getX() < clipArea.getRight()
                && placed.track >= 0
                && placed.track < visibleTracks)
            {
                visibleIndicesByTrack[(size_t) placed.track].push_back((int) index);
            }
        }

        for (int track = 0; track < visibleTracks; ++track)
        {
            auto& indices = visibleIndicesByTrack[(size_t) track];
            if (indices.empty())
                continue;

            std::sort(indices.begin(), indices.end(),
                      [&clipRects](int a, int b)
                      {
                          return clipRects[(size_t) a].getX() < clipRects[(size_t) b].getX();
                      });

            const float baseGap = 6.0f;
            const float availableWidth = clipArea.getWidth();

            std::vector<float> widths;
            widths.reserve(indices.size());
            float totalWidth = 0.0f;
            for (int index : indices)
            {
                const float width = clipRects[(size_t) index].getWidth();
                widths.push_back(width);
                totalWidth += width;
            }

            const float availableForClips = juce::jmax(20.0f, availableWidth - baseGap * (float) juce::jmax(0, (int) indices.size() - 1));
            if (totalWidth > 0.0f)
            {
                const float scale = juce::jlimit(0.72f, 1.42f, availableForClips / totalWidth);
                for (auto& width : widths)
                    width *= scale;
            }

            float cursor = clipArea.getX();

            for (size_t i = 0; i < indices.size(); ++i)
            {
                auto& rect = clipRects[(size_t) indices[i]];
                const float width = widths[i];
                rect.setX(cursor);
                rect.setWidth(width);
                cursor += width + baseGap;
            }

            if ((int) indices.size() > 3)
            {
                const float rightGap = clipArea.getRight()
                    - clipRects[(size_t) indices.back()].getRight();
                if (rightGap > 10.0f)
                {
                    const int tailCount = juce::jmax(1, (int) indices.size() / 2);
                    const int tailStart = (int) indices.size() - tailCount;
                    for (int i = tailStart; i < (int) indices.size(); ++i)
                    {
                        const float factor = tailCount == 1 ? 1.0f
                            : (float) (i - tailStart + 1) / (float) tailCount;
                        clipRects[(size_t) indices[(size_t) i]]
                            .setX(clipRects[(size_t) indices[(size_t) i]].getX() + rightGap * factor);
                    }
                }
            }

            if (!heroAvoidRect.isEmpty())
            {
                for (int pos = 0; pos < (int) indices.size(); ++pos)
                {
                    auto& rect = clipRects[(size_t) indices[(size_t) pos]];
                    const auto overlap = rect.getIntersection(heroAvoidRect);
                    const float overlapArea = overlap.getWidth() * overlap.getHeight();
                    const float rectArea = rect.getWidth() * rect.getHeight();
                    if (rectArea <= 0.0f || overlapArea < rectArea * 0.90f)
                        continue;

                    const float minX = pos == 0
                        ? clipArea.getX()
                        : clipRects[(size_t) indices[(size_t) pos - 1]].getRight() + baseGap;
                    const float maxX = pos == (int) indices.size() - 1
                        ? clipArea.getRight() - rect.getWidth()
                        : clipRects[(size_t) indices[(size_t) pos + 1]].getX() - rect.getWidth() - baseGap;
                    if (maxX < minX)
                        continue;

                    const float leftCandidate = juce::jlimit(minX, maxX,
                        heroAvoidRect.getX() - rect.getWidth() * 0.22f);
                    const float rightCandidate = juce::jlimit(minX, maxX,
                        heroAvoidRect.getRight() - rect.getWidth() * 0.78f);

                    const float leftOverlap = rect.withX(leftCandidate).getIntersection(heroAvoidRect).getWidth()
                                            * rect.withX(leftCandidate).getIntersection(heroAvoidRect).getHeight();
                    const float rightOverlap = rect.withX(rightCandidate).getIntersection(heroAvoidRect).getWidth()
                                             * rect.withX(rightCandidate).getIntersection(heroAvoidRect).getHeight();

                    if (leftOverlap <= rightOverlap)
                        rect.setX(leftCandidate);
                    else
                        rect.setX(rightCandidate);
                }
            }
        }

        for (size_t index = 0; index < positioned.size(); ++index)
        {
            const auto& placed = positioned[index];
            const float laneY = clipArea.getY() + (trackHeight + trackGap) * (float) placed.track;
            const float clipHeight = trackHeight;
            const float clipY = laneY;
            const bool isHighlighted = spotlightModeActive && placed.item->isCurrent;

            auto waveformColour = placed.item->waveformColour;
            auto perClipFill = clipFill;
            auto perClipOutline = clipOutline;
            auto perClipLabel = labelColour;

            if (!spotlightModeActive)
            {
                if (isDarkTheme)
                {
                    perClipOutline = juce::Colours::transparentBlack;
                    perClipLabel = juce::Colours::white.withAlpha(0.62f);
                }
            }
            else if (isDarkTheme)
            {
                if (isHighlighted)
                {
                    perClipOutline = juce::Colours::transparentBlack;
                    perClipLabel = juce::Colour(0xFFF3EEE4).withAlpha(0.72f);
                }
                else
                {
                    waveformColour = waveformColour.interpolatedWith(juce::Colour(0xFF0B0F16), 0.60f)
                                                   .withAlpha(0.58f);
                    perClipOutline = juce::Colours::transparentBlack;
                    perClipLabel = juce::Colour(0xFFF3EEE4).withAlpha(0.18f);
                }
            }
            else
            {
                if (isHighlighted)
                {
                    perClipFill = waveformColour.withAlpha(0.045f);
                    perClipOutline = waveformColour.withAlpha(0.78f);
                    perClipLabel = GoodMeterLookAndFeel::textMain.withAlpha(0.56f);
                }
                else
                {
                    const float grey = juce::jlimit(0.0f, 1.0f,
                                                    waveformColour.getPerceivedBrightness() * 0.92f);
                    waveformColour = juce::Colour::fromFloatRGBA(grey, grey, grey, 0.50f);
                    perClipFill = GoodMeterLookAndFeel::textMain.withAlpha(0.018f);
                    perClipOutline = GoodMeterLookAndFeel::textMain.withAlpha(0.05f);
                    perClipLabel = GoodMeterLookAndFeel::textMain.withAlpha(0.14f);
                }
            }

            auto fullClipRect = clipRects[index];
            fullClipRect.setY(clipY);
            fullClipRect.setHeight(clipHeight);
            auto visibleRect = fullClipRect.getIntersection(clipArea);
            if (visibleRect.getWidth() < 8.0f)
                continue;

            if (isHighlighted)
            {
                if (const auto* markers = getMarkerItemsForPath(getCurrentMarkerFileKey()))
                {
                    const double duration = juce::jmax(0.001, placed.item->durationSeconds);
                    for (const auto& marker : markers->items)
                    {
                        const float markerProgress = (float) juce::jlimit(0.0, 1.0, marker.seconds / duration);
                        const float markerX = fullClipRect.getX() + fullClipRect.getWidth() * markerProgress;
                        if (markerX < clipArea.getX() || markerX > clipArea.getRight())
                            continue;

                        const auto markerColour = marker.colour.withAlpha(isDarkTheme ? 0.82f : 0.54f);
                        const auto markerGlow = marker.colour.withAlpha(isDarkTheme ? 0.22f : 0.14f);

                        g.setColour(markerGlow);
                        g.fillRect(markerX - 3.0f, bounds.getY(), 6.0f, bounds.getHeight());
                        g.setColour(markerColour);
                        g.fillRect(markerX - 0.75f, bounds.getY(), 1.5f, bounds.getHeight());
                    }
                }
            }

            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(visibleRect.toNearestInt());

            g.setColour(perClipFill);
            g.fillRect(fullClipRect);

            g.setColour(perClipOutline);
            g.drawRect(fullClipRect, isHighlighted ? 1.7f : 1.0f);

            if (showClipFileNames && fullClipRect.getWidth() >= 64.0f && fullClipRect.getHeight() >= 18.0f)
            {
                auto clipNameArea = fullClipRect.reduced(6.0f, 4.0f);
                clipNameArea.setHeight(11.0f);
                g.setColour(isDarkTheme
                    ? juce::Colour(0xFFF3EEE4).withAlpha(isHighlighted ? 0.82f : 0.58f)
                    : GoodMeterLookAndFeel::textMain.withAlpha(isHighlighted ? 0.74f : 0.52f));
                g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(8.6f, juce::Font::bold));
                g.drawFittedText(placed.item->displayName.toUpperCase(),
                                 clipNameArea.toNearestInt(),
                                 juce::Justification::topLeft, 1, 0.9f);
            }

            auto waveformArea = fullClipRect.reduced(4.0f, 3.0f);
            drawClipWaveform(g, waveformArea, placed.item->waveformPeaks,
                             waveformColour, isHighlighted);

            if (isHighlighted)
            {
                const float progress = (float) juce::jlimit(0.0, 1.0, playheadSeconds / playheadDuration);
                const float playheadX = fullClipRect.getX() + fullClipRect.getWidth() * progress;
                if (playheadX >= visibleRect.getX() && playheadX <= visibleRect.getRight())
                {
                    const float glowW = 10.0f;
                    juce::ColourGradient glow(
                        playheadOrange.withAlpha(isDarkTheme ? 0.32f : 0.26f),
                        playheadX, fullClipRect.getCentreY(),
                        juce::Colours::transparentBlack,
                        playheadX + glowW, fullClipRect.getCentreY(),
                        false);
                    glow.addColour(0.0, playheadOrange.withAlpha(isDarkTheme ? 0.32f : 0.26f));
                    g.setGradientFill(glow);
                    g.fillRect(playheadX - glowW * 0.5f, fullClipRect.getY(), glowW, fullClipRect.getHeight());

                    g.setColour(playheadOrange.withAlpha(0.92f));
                    g.fillRect(playheadX - 1.0f, fullClipRect.getY(), 2.0f, fullClipRect.getHeight());
                }
            }
        }
    }

    static bool isDotMatrixCjk(juce::juce_wchar c) noexcept
    {
        return (c >= 0x3400 && c <= 0x4DBF)
            || (c >= 0x4E00 && c <= 0x9FFF)
            || (c >= 0xF900 && c <= 0xFAFF);
    }

    static bool isFooterCjkLike(juce::juce_wchar c) noexcept
    {
        if (isDotMatrixCjk(c))
            return true;

        return (c >= 0x3000 && c <= 0x303F)   // CJK symbols / punctuation, includes 《》
            || (c >= 0xFF00 && c <= 0xFFEF)   // full-width forms
            || c == 0x2014 || c == 0x2015     // em dash / horizontal bar
            || c == 0x2026;                   // ellipsis
    }

    static juce::String dotMatrixPinyinFor(juce::juce_wchar c)
    {
        struct Entry { const char* hanzi; const char* latin; };
        static const Entry entries[] = {
            { "一", "YI" }, { "阵", "ZHEN" }, { "风", "FENG" }, { "不", "BU" },
            { "安", "AN" }, { "的", "DE" }, { "公", "GONG" }, { "寓", "YU" },
            { "祥", "XIANG" }, { "室", "SHI" }, { "内", "NEI" }, { "广", "GUANG" },
            { "阔", "KUO" }, { "缓", "HUAN" }, { "慢", "MAN" }, { "嚎", "HAO" },
            { "叫", "JIAO" }, { "草", "CAO" }, { "地", "DI" }, { "走", "ZOU" },
            { "潜", "QIAN" }, { "伏", "FU" }, { "运", "YUN" }, { "动", "DONG" },
            { "鞋", "XIE" }, { "原", "YUAN" }, { "奔", "BEN" }, { "跑", "PAO" },
            { "自", "ZI" }, { "然", "RAN" }, { "树", "SHU" }, { "吹", "CHUI" },
            { "拂", "FU" }, { "叶", "YE" }, { "失", "SHI" }, { "落", "LUO" },
            { "星", "XING" }, { "船", "CHUAN" }, { "马", "MA" }, { "拉", "LA" },
            { "松", "SONG" }, { "效", "XIAO" }, { "设", "SHE" }, { "计", "JI" },
            { "分", "FEN" }, { "析", "XI" }, { "高", "GAO" }, { "码", "MA" },
            { "率", "LV" }, { "海", "HAI" }, { "浪", "LANG" }, { "雨", "YU" },
            { "雷", "LEI" }, { "电", "DIAN" }, { "鸟", "NIAO" }, { "虫", "CHONG" },
            { "水", "SHUI" }, { "滴", "DI" }, { "门", "MEN" }, { "窗", "CHUANG" },
            { "开", "KAI" }, { "关", "GUAN" }, { "车", "CHE" }, { "路", "LU" },
            { "人", "REN" }, { "声", "SHENG" }, { "脚", "JIAO" }, { "步", "BU" },
            { "回", "HUI" }, { "音", "YIN" }, { "低", "DI" }, { "频", "PIN" },
            { "空", "KONG" }, { "旷", "KUANG" }, { "远", "YUAN" }, { "近", "JIN" },
            { "呼", "HU" }, { "啸", "XIAO" }, { "穿", "CHUAN" }, { "过", "GUO" },
            { "夜", "YE" }, { "白", "BAI" }, { "天", "TIAN" }, { "猫", "MAO" },
            { "狗", "GOU" }, { "笑", "XIAO" }, { "哭", "KU" }
        };

        const auto glyph = juce::String::charToString(c);
        for (const auto& entry : entries)
            if (glyph == juce::String::fromUTF8(entry.hanzi))
                return juce::String(entry.latin);

        return {};
    }

    static juce::String translateDotMatrixChineseRun(const juce::String& run)
    {
        struct PhraseEntry { const char* source; const char* target; };
        static const PhraseEntry phrases[] = {
            { "动效设计分析", "MOTION ANALYSIS" },
            { "失落星船", "LOST STARSHIP" },
            { "高码率", "HIGH BITRATE" },
            { "自然风声", "NATURE WIND" },
            { "树叶", "LEAVES" },
            { "吹拂", "RUSTLE" },
            { "一阵", "GUST" },
            { "不安的", "UNEASY" },
            { "不祥的", "OMINOUS" },
            { "公寓", "APT" },
            { "室内", "INDOOR" },
            { "室外", "OUTDOOR" },
            { "广阔", "WIDE" },
            { "缓慢", "SLOW" },
            { "嚎叫", "HOWL" },
            { "草地", "GRASS" },
            { "慢走", "WALK" },
            { "潜伏", "STEALTH" },
            { "运动鞋", "SNEAKER" },
            { "原地", "INPLACE" },
            { "奔跑", "RUN" },
            { "脚步", "FOOTSTEP" },
            { "回声", "ECHO" },
            { "低频", "LOW END" },
            { "高频", "HIGH END" },
            { "噪音", "NOISE" },
            { "环境", "AMBIENT" },
            { "风声", "WIND" },
            { "风", "WIND" },
            { "水滴", "DRIP" },
            { "水", "WATER" },
            { "海浪", "SURF" },
            { "雨声", "RAIN" },
            { "雷声", "THUNDER" },
            { "树", "TREE" },
            { "门", "DOOR" },
            { "窗", "WINDOW" },
            { "车", "CAR" },
            { "路", "ROAD" },
            { "人声", "VOX" },
            { "鸟", "BIRD" },
            { "虫", "BUG" },
            { "马拉松", "MARATHON" }
        };

        juce::String out;
        int index = 0;
        while (index < run.length())
        {
            int bestLength = 0;
            juce::String bestTarget;

            for (const auto& entry : phrases)
            {
                const auto source = juce::String::fromUTF8(entry.source);
                if (source.length() <= bestLength)
                    continue;

                if (run.substring(index).startsWith(source))
                {
                    bestLength = source.length();
                    bestTarget = juce::String::fromUTF8(entry.target);
                }
            }

            if (bestLength > 0)
            {
                if (out.isNotEmpty() && !out.endsWithChar(' '))
                    out << ' ';
                out << bestTarget;
                index += bestLength;
                continue;
            }

            auto pinyin = dotMatrixPinyinFor(run[index]);
            if (pinyin.isEmpty())
                pinyin = "HAN";

            if (out.isNotEmpty() && !out.endsWithChar(' '))
                out << ' ';
            out << pinyin;
            ++index;
        }

        return out;
    }

    static juce::String buildDotMatrixDisplayText(const juce::String& raw)
    {
        auto appendWordToken = [](juce::String& out, const juce::String& token)
        {
            if (token.isEmpty())
                return;

            if (out.isNotEmpty()
                && !out.endsWithChar(' ')
                && !out.endsWithChar('-')
                && !out.endsWithChar('_')
                && !out.endsWithChar('.'))
                out << ' ';

            out << token;
        };

        juce::String out;
        int index = 0;

        while (index < raw.length())
        {
            const auto c = raw[index];

            if (isDotMatrixCjk(c))
            {
                int end = index + 1;
                while (end < raw.length() && isDotMatrixCjk(raw[end]))
                    ++end;

                appendWordToken(out, translateDotMatrixChineseRun(raw.substring(index, end)));
                index = end;
                continue;
            }

            if (c < 0x80 && juce::CharacterFunctions::isLetterOrDigit(c))
            {
                int end = index + 1;
                while (end < raw.length()
                       && raw[end] < 0x80
                       && juce::CharacterFunctions::isLetterOrDigit(raw[end]))
                    ++end;

                appendWordToken(out, raw.substring(index, end).toUpperCase());
                index = end;
                continue;
            }

            if (c == '.' || c == '_' || c == '-')
            {
                while (out.endsWithChar(' '))
                    out = out.dropLastCharacters(1);

                out << juce::CharacterFunctions::toUpperCase(c);
                ++index;
                continue;
            }

            if (juce::CharacterFunctions::isWhitespace(c)
                || c == ':'
                || c == 0xFF1A
                || c == 0x3010
                || c == 0x3011
                || c == '('
                || c == ')'
                || c == 0xFF08
                || c == 0xFF09
                || c == '['
                || c == ']'
                || c == 0xFF0C
                || c == 0x3001
                || c == '/'
                || c == '\\')
            {
                if (out.isNotEmpty()
                    && !out.endsWithChar(' ')
                    && !out.endsWithChar('-')
                    && !out.endsWithChar('_')
                    && !out.endsWithChar('.'))
                    out << ' ';

                ++index;
                continue;
            }

            ++index;
        }

        while (out.contains("  "))
            out = out.replace("  ", " ");

        return out.trim().toUpperCase();
    }

    static int dotMatrixColumnCount(const juce::String& text) noexcept
    {
        if (text.isEmpty())
            return 0;

        return juce::jmax(1, text.length() * 6 - 1);
    }

    static bool dotMatrixTextFits(const juce::String& text, juce::Rectangle<float> area) noexcept
    {
        if (text.isEmpty())
            return false;

        const int totalColumns = dotMatrixColumnCount(text);
        const float cell = juce::jmin(area.getHeight() / 7.0f,
                                      area.getWidth() / (float) totalColumns) * 0.70f;
        return cell >= 1.5f;
    }

    static juce::String compactDotMatrixToken(juce::String token)
    {
        token = token.trim().toUpperCase();
        if (token.length() <= 7)
            return token;

        if (token.containsOnly("0123456789-_."))
            return token.substring(0, 10);

        return token.substring(0, 6);
    }

    static juce::String fitDotMatrixDisplayText(const juce::String& raw,
                                                juce::Rectangle<float> area)
    {
        auto text = buildDotMatrixDisplayText(raw);

        if (text.isEmpty())
        {
            text = raw.toUpperCase()
                      .retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. ");
            while (text.contains("  "))
                text = text.replace("  ", " ");
            text = text.trim();
        }

        if (text.isEmpty())
            return {};

        if (dotMatrixTextFits(text, area))
            return text;

        const int lastDot = text.lastIndexOfChar('.');
        if (lastDot > 0)
        {
            auto withoutExtension = text.substring(0, lastDot).trim();
            if (!withoutExtension.isEmpty())
            {
                text = withoutExtension;
                if (dotMatrixTextFits(text, area))
                    return text;
            }
        }

        juce::StringArray tokens;
        tokens.addTokens(text, " ", "");
        tokens.removeEmptyStrings();

        if (tokens.size() > 1)
        {
            juce::String compact;
            for (auto token : tokens)
            {
                const auto shortened = compactDotMatrixToken(token);
                if (shortened.isEmpty())
                    continue;
                if (compact.isNotEmpty())
                    compact << ' ';
                compact << shortened;
            }

            if (!compact.isEmpty())
            {
                text = compact;
                if (dotMatrixTextFits(text, area))
                    return text;
            }

            while (tokens.size() > 1)
            {
                tokens.remove(tokens.size() - 1);
                juce::String fewer;
                for (auto token : tokens)
                {
                    const auto shortened = compactDotMatrixToken(token);
                    if (shortened.isEmpty())
                        continue;
                    if (fewer.isNotEmpty())
                        fewer << ' ';
                    fewer << shortened;
                }

                if (dotMatrixTextFits(fewer, area))
                    return fewer;
            }
        }

        const int maxColumnsByWidth = (int) std::floor((area.getWidth() / 1.5f) * 0.70f);
        const int maxChars = juce::jmax(4, (maxColumnsByWidth + 1) / 6);
        if (text.length() > maxChars)
            text = text.substring(0, maxChars);

        return text.trim();
    }

    static bool dotMatrixTextContainsCjk(const juce::String& text) noexcept
    {
        for (int i = 0; i < text.length(); ++i)
            if (isDotMatrixCjk(text[i]))
                return true;

        return false;
    }

    const ClipLayerItem* getCurrentClipLayerItem() const
    {
        const auto currentPath = audioEngine.getCurrentFilePath();
        if (currentPath.isEmpty())
            return nullptr;

        for (const auto& item : clipLayerItems)
        {
            if (item.isCurrent
                || currentPath == item.mediaFile.getFullPathName()
                || currentPath == item.waveformFile.getFullPathName())
                return &item;
        }

        return nullptr;
    }

    juce::String getCurrentMarkerFileKey() const
    {
        if (const auto* item = getCurrentClipLayerItem())
            return item->mediaFile.getFullPathName();

        return audioEngine.getCurrentFilePath();
    }

    static juce::Colour getMarkerPaletteColour(int index) noexcept
    {
        static const juce::Colour palette[] =
        {
            GoodMeterLookAndFeel::accentPink,
            GoodMeterLookAndFeel::accentBlue,
            GoodMeterLookAndFeel::accentYellow,
            GoodMeterLookAndFeel::accentCyan,
            GoodMeterLookAndFeel::accentGreen,
            GoodMeterLookAndFeel::accentPurple,
            juce::Colour(0xFFFF8E73),
            juce::Colour(0xFF7FD8E6)
        };

        return palette[juce::jmax(0, index) % (int) std::size(palette)];
    }

    juce::File getMarkerFrameCacheFile(const juce::String& filePath,
                                       const juce::String& markerId) const
    {
        auto root = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                        .getChildFile(".marker_frames");
        root.createDirectory();
        auto base = juce::File(filePath).getFileNameWithoutExtension()
                        .retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
        if (base.isEmpty())
            base = "marker";
        auto safeId = markerId.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
        if (safeId.isEmpty())
            safeId = "id";
        return root.getChildFile(base + "_" + safeId + ".png");
    }

    struct FileMarkerEntry* getMarkerItemsForPath(const juce::String& filePath)
    {
        if (filePath.isEmpty())
            return nullptr;

        for (auto& entry : markerEntries)
            if (entry.filePath == filePath)
                return &entry;

        markerEntries.push_back({ filePath, {} });
        return &markerEntries.back();
    }

    const FileMarkerEntry* getMarkerItemsForPath(const juce::String& filePath) const
    {
        if (filePath.isEmpty())
            return nullptr;

        for (const auto& entry : markerEntries)
            if (entry.filePath == filePath)
                return &entry;

        return nullptr;
    }

    GoodMeterMarkerItem* findMarkerItemById(const juce::String& filePath, const juce::String& markerId)
    {
        if (auto* entry = getMarkerItemsForPath(filePath))
        {
            for (auto& item : entry->items)
                if (item.id == markerId)
                    return &item;
        }
        return nullptr;
    }

    static juce::String formatMarkerTimecode(double seconds, double fps = 30.0)
    {
        seconds = juce::jmax(0.0, seconds);
        const int totalWholeSeconds = (int) std::floor(seconds);
        const int hours = totalWholeSeconds / 3600;
        const int minutes = (totalWholeSeconds / 60) % 60;
        const int secs = totalWholeSeconds % 60;
        const int frames = juce::jlimit(0, (int) fps - 1,
            (int) std::floor((seconds - std::floor(seconds)) * fps + 0.0001));

        return juce::String::formatted("%02d:%02d:%02d:%02d",
                                       hours, minutes, secs, frames);
    }

    static juce::String formatDropFrameMarkerTimecode(int64_t frameNumber, int frameQuanta)
    {
        frameQuanta = juce::jmax(1, frameQuanta);
        frameNumber = juce::jmax<int64_t>(0, frameNumber);

        const int dropFrames = juce::jmax(2, (int) std::round((double) frameQuanta * 0.0666666667));
        const int64_t framesPerHour = (int64_t) frameQuanta * 60 * 60;
        const int64_t framesPer24Hours = framesPerHour * 24;
        const int64_t framesPer10Minutes = (int64_t) frameQuanta * 60 * 10 - (int64_t) dropFrames * 9;
        const int64_t framesPerMinute = (int64_t) frameQuanta * 60 - dropFrames;

        frameNumber %= framesPer24Hours;
        const int64_t tenMinuteBlocks = frameNumber / framesPer10Minutes;
        const int64_t leftover = frameNumber % framesPer10Minutes;

        int64_t adjustedFrameNumber = frameNumber + tenMinuteBlocks * dropFrames * 9;
        if (leftover >= dropFrames)
            adjustedFrameNumber += dropFrames * ((leftover - dropFrames) / framesPerMinute + 1);

        const int hours = (int) (adjustedFrameNumber / framesPerHour);
        const int minutes = (int) ((adjustedFrameNumber / ((int64_t) frameQuanta * 60)) % 60);
        const int seconds = (int) ((adjustedFrameNumber / frameQuanta) % 60);
        const int frames = (int) (adjustedFrameNumber % frameQuanta);

        return juce::String::formatted("%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    }

    static double normaliseMarkerDisplayFps(double fps)
    {
        if (!std::isfinite(fps) || fps <= 1.0)
            return 30.0;

        if (std::abs(fps - 23.976) < 0.08 || std::abs(fps - 24.0) < 0.08)
            return 24.0;
        if (std::abs(fps - 25.0) < 0.08)
            return 25.0;
        if (std::abs(fps - 29.97) < 0.08 || std::abs(fps - 30.0) < 0.08)
            return 30.0;
        if (std::abs(fps - 50.0) < 0.08)
            return 50.0;
        if (std::abs(fps - 59.94) < 0.08 || std::abs(fps - 60.0) < 0.08)
            return 60.0;

        return std::round(fps);
    }

    double getCurrentMarkerDisplayFps() const
    {
        if (const auto* item = getCurrentClipLayerItem())
        {
            if (item->isVideo && item->markerTimecodeFps > 1.0)
                return item->markerTimecodeFps;
        }

        return markerDisplayFps;
    }

    juce::String formatMarkerTimecodeForCurrentFile(double seconds) const
    {
        if (const auto* item = getCurrentClipLayerItem())
        {
            if (item->isVideo)
            {
                if (item->hasReadableStartTimecode)
                {
                    const double secondsFromTimecodeStart = juce::jmax(0.0, seconds - item->markerTimecodeStartSeconds);
                    const int64_t frameOffset = (int64_t) std::llround(secondsFromTimecodeStart * item->markerTimecodeFps);
                    const int64_t frameNumber = item->timecodeStartFrameNumber + frameOffset;

                    if (item->usesDropFrameTimecode)
                        return formatDropFrameMarkerTimecode(frameNumber, item->timecodeFrameQuanta);

                    return formatMarkerTimecode((double) frameNumber / juce::jmax(1, item->timecodeFrameQuanta),
                                                item->timecodeFrameQuanta);
                }

                return formatMarkerTimecode(seconds, item->markerTimecodeFps);
            }
        }

        return formatMarkerTimecode(seconds, markerDisplayFps);
    }

    juce::String getCurrentMarkerTimecodeText() const
    {
        if (!audioEngine.isFileLoaded())
            return {};

        return formatMarkerTimecodeForCurrentFile(audioEngine.getCurrentPosition());
    }

    void addMarkerAtCurrentPosition()
    {
        const auto markerPath = getCurrentMarkerFileKey();
        if (markerPath.isEmpty())
            return;

        auto* markers = getMarkerItemsForPath(markerPath);
        if (markers == nullptr)
            return;

        const double position = juce::jlimit(0.0, juce::jmax(0.0, audioEngine.getTotalLength()),
                                             audioEngine.getCurrentPosition());

        for (const auto& existing : markers->items)
            if (std::abs(existing.seconds - position) < 0.08)
                return;

        GoodMeterMarkerItem item;
        item.id = juce::Uuid().toString().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
        item.seconds = position;
        item.colour = getMarkerPaletteColour((int) markers->items.size());
        item.isVideo = false;

        if (const auto* currentItem = getCurrentClipLayerItem())
        {
            item.isVideo = currentItem->isVideo;

            if (item.isVideo)
            {
                const auto cacheFile = getMarkerFrameCacheFile(markerPath, item.id);
                item.frameImagePath = cacheFile.getFullPathName();
                item.framePending = true;
            }
        }

        markers->items.push_back(item);
        std::sort(markers->items.begin(), markers->items.end(),
                  [] (const auto& a, const auto& b) { return a.seconds < b.seconds; });

        if (item.isVideo)
        {
            const auto cacheFile = juce::File(item.frameImagePath);
            VideoAudioExtractor::extractFrameImage(juce::File(markerPath), item.seconds, cacheFile,
                [safeThis = juce::Component::SafePointer<NonoPageComponent>(this),
                 markerPath, markerId = item.id, cacheFile](bool success)
                {
                    if (auto* self = safeThis.getComponent())
                    {
                        if (auto* marker = self->findMarkerItemById(markerPath, markerId))
                        {
                            marker->framePending = false;
                            if (success && cacheFile.existsAsFile())
                                marker->frameImagePath = cacheFile.getFullPathName();
                        }

                        if (self->onMarkerDataChanged != nullptr)
                            self->onMarkerDataChanged();
                        self->repaint();
                    }
                });
        }

        if (onMarkerDataChanged != nullptr)
            onMarkerDataChanged();
        repaint();
    }

    void rebuildDotMatrixFileNameCache(const juce::String& rawText,
                                       juce::Rectangle<float> area) const
    {
        const auto areaKey = area.toNearestInt();
        const auto currentPath = audioEngine.getCurrentFilePath();
        const auto cacheKey = currentPath + "::" + rawText.trim();

        if (cacheKey == cachedDotMatrixSourceText
            && areaKey == cachedDotMatrixArea
            && (!cachedDotMatrixImage.isNull() || !cachedDotMatrixPoints.empty()))
            return;

        cachedDotMatrixSourceText = cacheKey;
        cachedDotMatrixArea = areaKey;
        cachedDotMatrixRenderedText.clear();
        cachedDotMatrixPoints.clear();
        cachedDotMatrixDotSize = 0.0f;
        cachedDotMatrixCentre = area.getCentre();
        cachedDotMatrixImage = {};

        auto displayText = juce::URL::removeEscapeChars(rawText.trim());
        if (currentPath.isEmpty() || displayText.isEmpty()
            || area.getWidth() <= 2.0f || area.getHeight() <= 2.0f)
            return;

        auto compressMiddle = [] (const juce::String& text)
        {
            if (text.length() <= 18)
                return text;

            const int keepFront = juce::jmax(6, text.length() / 2 - 4);
            const int keepBack = juce::jmax(5, text.length() - keepFront - 1);
            return text.substring(0, keepFront) + juce::String::fromUTF8("…")
                 + text.substring(text.length() - keepBack);
        };

        auto makeFooterFont = [] (float size)
        {
            return juce::Font(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold);
        };

        auto compressMiddleToWidth = [&] (const juce::String& fullText,
                                          const juce::Font& font,
                                          float widthLimit)
        {
            auto candidate = fullText;
            if (font.getStringWidthFloat(candidate) <= widthLimit)
                return candidate;

            int keepFront = juce::jmax(4, fullText.length() / 2 - 2);
            int keepBack = juce::jmax(4, fullText.length() - keepFront - 1);

            while (keepFront > 1 || keepBack > 1)
            {
                candidate = fullText.substring(0, keepFront)
                          + juce::String::fromUTF8("…")
                          + fullText.substring(fullText.length() - keepBack);
                if (font.getStringWidthFloat(candidate) <= widthLimit)
                    return candidate;

                if (keepFront >= keepBack && keepFront > 1)
                    --keepFront;
                else if (keepBack > 1)
                    --keepBack;
                else
                    break;
            }

            return compressMiddle(fullText);
        };

        const juce::String rawDisplayText = displayText;
        const float minFontSize = 13.2f;
        float fontSize = juce::jlimit(minFontSize, 20.0f, area.getHeight() * 0.62f);
        const float maxWidth = juce::jmax(10.0f, area.getWidth() - 12.0f);
        float totalWidth = 0.0f;

        for (int attempt = 0; attempt < 12; ++attempt)
        {
            const auto font = makeFooterFont(fontSize);
            totalWidth = font.getStringWidthFloat(displayText);

            if (totalWidth <= maxWidth || fontSize <= minFontSize + 0.05f)
                break;

            fontSize *= 0.95f;
        }

        fontSize = juce::jmax(minFontSize, fontSize);
        auto footerFont = makeFooterFont(fontSize);
        totalWidth = footerFont.getStringWidthFloat(displayText);
        if (totalWidth > maxWidth)
        {
            displayText = compressMiddleToWidth(rawDisplayText, footerFont, maxWidth);
            totalWidth = footerFont.getStringWidthFloat(displayText);
        }

        if (totalWidth <= 1.0f)
            return;

        const int rasterW = juce::jmax(1, (int) std::ceil(area.getWidth()));
        const int rasterH = juce::jmax(1, (int) std::ceil(area.getHeight()));
        juce::Image raster(juce::Image::ARGB, rasterW, rasterH, true);
        juce::Graphics rg(raster);
        rg.fillAll(juce::Colours::transparentBlack);

        const auto rasterColour = juce::Colours::white;
        const float contentHeight = footerFont.getHeight();
        const float baselineY = (area.getHeight() - contentHeight) * 0.5f - 0.5f;
        rg.setColour(rasterColour);
        rg.setFont(footerFont);
        rg.drawText(displayText,
                    juce::Rectangle<float>(0.0f, baselineY, area.getWidth(), contentHeight + 4.0f),
                    juce::Justification::centred, false);

        std::vector<juce::Point<float>> points;
        const float sampleStep = juce::jlimit(1.8f, 3.0f, area.getHeight() / 10.5f);
        const float dot = juce::jmax(1.1f, sampleStep * 0.78f);
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (float y = 0.0f; y < (float) rasterH; y += sampleStep)
        {
            for (float x = 0.0f; x < (float) rasterW; x += sampleStep)
            {
                const auto pixel = raster.getPixelAt((int) std::floor(x), (int) std::floor(y));
                if (pixel.getAlpha() < 64)
                    continue;

                const float px = area.getX() + x;
                const float py = area.getY() + y;
                points.emplace_back(px, py);
                minX = juce::jmin(minX, px);
                minY = juce::jmin(minY, py);
                maxX = juce::jmax(maxX, px + dot);
                maxY = juce::jmax(maxY, py + dot);
            }
        }

        if (points.empty())
            return;

        cachedDotMatrixRenderedText = displayText;
        cachedDotMatrixImage = raster;
        cachedDotMatrixPoints = std::move(points);
        cachedDotMatrixDotSize = dot;
        cachedDotMatrixCentre = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f };
    }

    static const char* dotGlyphRows(juce::juce_wchar c) noexcept
    {
        switch (juce::CharacterFunctions::toUpperCase(c))
        {
            case 0x4E00: return "....."
                                "....."
                                "....."
                                "#####"
                                "....."
                                "....."
                                "....."; // 一
            case 'A': return ".###."
                             "#...#"
                             "#...#"
                             "#####"
                             "#...#"
                             "#...#"
                             "#...#";
            case 'B': return "####."
                             "#...#"
                             "#...#"
                             "####."
                             "#...#"
                             "#...#"
                             "####.";
            case 'C': return ".###."
                             "#...#"
                             "#...."
                             "#...."
                             "#...."
                             "#...#"
                             ".###.";
            case 'D': return "####."
                             "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             "####.";
            case 'E': return "#####"
                             "#...."
                             "#...."
                             "####."
                             "#...."
                             "#...."
                             "#####";
            case 'F': return "#####"
                             "#...."
                             "#...."
                             "####."
                             "#...."
                             "#...."
                             "#....";
            case 'G': return ".###."
                             "#...#"
                             "#...."
                             "#.###"
                             "#...#"
                             "#...#"
                             ".###.";
            case 'H': return "#...#"
                             "#...#"
                             "#...#"
                             "#####"
                             "#...#"
                             "#...#"
                             "#...#";
            case 'I': return "#####"
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#.."
                             "#####";
            case 'J': return "..###"
                             "...#."
                             "...#."
                             "...#."
                             "#..#."
                             "#..#."
                             ".##..";
            case 'K': return "#...#"
                             "#..#."
                             "#.#.."
                             "##..."
                             "#.#.."
                             "#..#."
                             "#...#";
            case 'L': return "#...."
                             "#...."
                             "#...."
                             "#...."
                             "#...."
                             "#...."
                             "#####";
            case 'M': return "#...#"
                             "##.##"
                             "#.#.#"
                             "#.#.#"
                             "#...#"
                             "#...#"
                             "#...#";
            case 'N': return "#...#"
                             "##..#"
                             "##..#"
                             "#.#.#"
                             "#..##"
                             "#..##"
                             "#...#";
            case 'O': return ".###."
                             "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             ".###.";
            case 'P': return "####."
                             "#...#"
                             "#...#"
                             "####."
                             "#...."
                             "#...."
                             "#....";
            case 'Q': return ".###."
                             "#...#"
                             "#...#"
                             "#...#"
                             "#.#.#"
                             "#..#."
                             ".##.#";
            case 'R': return "####."
                             "#...#"
                             "#...#"
                             "####."
                             "#.#.."
                             "#..#."
                             "#...#";
            case 'S': return ".####"
                             "#...."
                             "#...."
                             ".###."
                             "....#"
                             "....#"
                             "####.";
            case 'T': return "#####"
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#..";
            case 'U': return "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             ".###.";
            case 'V': return "#...#"
                             "#...#"
                             "#...#"
                             "#...#"
                             ".#.#."
                             ".#.#."
                             "..#..";
            case 'W': return "#...#"
                             "#...#"
                             "#...#"
                             "#.#.#"
                             "#.#.#"
                             "##.##"
                             "#...#";
            case 'X': return "#...#"
                             "#...#"
                             ".#.#."
                             "..#.."
                             ".#.#."
                             "#...#"
                             "#...#";
            case 'Y': return "#...#"
                             "#...#"
                             ".#.#."
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#..";
            case 'Z': return "#####"
                             "....#"
                             "...#."
                             "..#.."
                             ".#..."
                             "#...."
                             "#####";
            case '0': return ".###."
                             "#...#"
                             "#..##"
                             "#.#.#"
                             "##..#"
                             "#...#"
                             ".###.";
            case '1': return "..#.."
                             ".##.."
                             "..#.."
                             "..#.."
                             "..#.."
                             "..#.."
                             ".###.";
            case '2': return ".###."
                             "#...#"
                             "....#"
                             "...#."
                             "..#.."
                             ".#..."
                             "#####";
            case '3': return "#####"
                             "....#"
                             "...#."
                             "..##."
                             "....#"
                             "#...#"
                             ".###.";
            case '4': return "...#."
                             "..##."
                             ".#.#."
                             "#..#."
                             "#####"
                             "...#."
                             "...#.";
            case '5': return "#####"
                             "#...."
                             "####."
                             "....#"
                             "....#"
                             "#...#"
                             ".###.";
            case '6': return ".###."
                             "#...#"
                             "#...."
                             "####."
                             "#...#"
                             "#...#"
                             ".###.";
            case '7': return "#####"
                             "....#"
                             "...#."
                             "..#.."
                             ".#..."
                             ".#..."
                             ".#...";
            case '8': return ".###."
                             "#...#"
                             "#...#"
                             ".###."
                             "#...#"
                             "#...#"
                             ".###.";
            case '9': return ".###."
                             "#...#"
                             "#...#"
                             ".####"
                             "....#"
                             "#...#"
                             ".###.";
            case '-': return "....."
                             "....."
                             "....."
                             ".###."
                             "....."
                             "....."
                             ".....";
            case '_': return "....."
                             "....."
                             "....."
                             "....."
                             "....."
                             "....."
                             "#####";
            case '.': return "....."
                             "....."
                             "....."
                             "....."
                             "....."
                             "..#.."
                             "..#..";
            case ' ': return "....."
                             "....."
                             "....."
                             "....."
                             "....."
                             "....."
                             ".....";
            default:
                if (c > 0x7f)
                {
                    return ".###."
                           "#...#"
                           "#.#.#"
                           "#.#.#"
                           "#.#.#"
                           "#...#"
                           ".###.";
                }
                return "#####"
                       "#...#"
                       "...#."
                       "..#.."
                       "..#.."
                       "....."
                       "..#..";
        }
    }

    void drawDotMatrixFileName(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (area.isEmpty())
            return;

        if (markerModeActive && !textClearWaveActive)
        {
            const auto timecode = getCurrentMarkerTimecodeText();
            if (timecode.isNotEmpty())
            {
                auto timeArea = area.reduced(4.0f, 1.0f);
                const auto ink = isDarkTheme ? juce::Colour(0xFFF6F0E6).withAlpha(0.98f)
                                             : GoodMeterLookAndFeel::ink.withAlpha(0.98f);
                const auto glow = isDarkTheme ? juce::Colour(0xFFFFF8EE).withAlpha(0.10f)
                                              : GoodMeterLookAndFeel::ink.withAlpha(0.10f);
                GoodMeterDigitalTimecode::draw(g, timeArea, timecode, ink, glow);
                return;
            }
        }

        rebuildDotMatrixFileNameCache(fileNameLabel.getText(), area);
        if ((cachedDotMatrixImage.isNull() && cachedDotMatrixPoints.empty())
            || cachedDotMatrixDotSize <= 0.0f)
            return;

        const float dot = cachedDotMatrixDotSize;
        const bool scatterActive = textClearWaveActive;

        if (!scatterActive)
        {
            auto compressMiddle = [] (const juce::String& text)
            {
                if (text.length() <= 18)
                    return text;

                const int keepFront = juce::jmax(6, text.length() / 2 - 4);
                const int keepBack = juce::jmax(5, text.length() - keepFront - 1);
                return text.substring(0, keepFront) + juce::String::fromUTF8("…")
                     + text.substring(text.length() - keepBack);
            };

            auto makeFooterFont = [] (float size)
            {
                return juce::Font(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold);
            };

            auto compressMiddleToWidth = [&] (const juce::String& fullText,
                                              const juce::Font& font,
                                              float widthLimit)
            {
                auto candidate = fullText;
                if (font.getStringWidthFloat(candidate) <= widthLimit)
                    return candidate;

                int keepFront = juce::jmax(4, fullText.length() / 2 - 2);
                int keepBack = juce::jmax(4, fullText.length() - keepFront - 1);

                while (keepFront > 1 || keepBack > 1)
                {
                    candidate = fullText.substring(0, keepFront)
                              + juce::String::fromUTF8("…")
                              + fullText.substring(fullText.length() - keepBack);
                    if (font.getStringWidthFloat(candidate) <= widthLimit)
                        return candidate;

                    if (keepFront >= keepBack && keepFront > 1)
                        --keepFront;
                    else if (keepBack > 1)
                        --keepBack;
                    else
                        break;
                }

                return compressMiddle(fullText);
            };

            auto displayText = juce::URL::removeEscapeChars(fileNameLabel.getText().trim());
            if (displayText.isEmpty())
                return;

            const juce::String rawDisplayText = displayText;
            const float minFontSize = 13.2f;
            float fontSize = juce::jlimit(minFontSize, 20.0f, area.getHeight() * 0.62f);
            const float maxWidth = juce::jmax(10.0f, area.getWidth() - 12.0f);

            for (int attempt = 0; attempt < 12; ++attempt)
            {
                const auto font = makeFooterFont(fontSize);
                const float totalWidth = font.getStringWidthFloat(displayText);
                if (totalWidth <= maxWidth || fontSize <= minFontSize + 0.05f)
                    break;

                fontSize *= 0.95f;
            }

            fontSize = juce::jmax(minFontSize, fontSize);
            const auto footerFont = makeFooterFont(fontSize);
            float totalWidth = footerFont.getStringWidthFloat(displayText);
            if (totalWidth > maxWidth)
            {
                displayText = compressMiddleToWidth(rawDisplayText, footerFont, maxWidth);
                totalWidth = footerFont.getStringWidthFloat(displayText);
            }

            const float contentHeight = footerFont.getHeight();
            const float baselineY = area.getY() + (area.getHeight() - contentHeight) * 0.5f - 0.5f;
            const auto textColour = isDarkTheme
                ? juce::Colour(0xFFF6F0E6).withAlpha(0.98f)
                : GoodMeterLookAndFeel::ink.withAlpha(0.98f);
            const auto inkBoostColour = isDarkTheme
                ? juce::Colour(0xFFFFF8EE).withAlpha(0.18f)
                : GoodMeterLookAndFeel::ink.withAlpha(0.14f);
            g.setFont(footerFont);
            g.setColour(inkBoostColour);
            g.drawText(displayText,
                       juce::Rectangle<float>(area.getX() + 0.22f, baselineY, area.getWidth(), contentHeight + 4.0f),
                       juce::Justification::centred, false);
            g.setColour(textColour);
            g.drawText(displayText,
                       juce::Rectangle<float>(area.getX(), baselineY, area.getWidth(), contentHeight + 4.0f),
                       juce::Justification::centred, false);
            return;
        }

        const auto dotColour = isDarkTheme
            ? juce::Colour(0xFFF3EEE4).withAlpha(0.96f)
            : GoodMeterLookAndFeel::textMain.withAlpha(0.90f);
        const auto glowColour = isDarkTheme
            ? juce::Colour(0xFFF3EEE4).withAlpha(0.12f)
            : GoodMeterLookAndFeel::textMain.withAlpha(0.07f);
        const auto burstDotColour = isDarkTheme
            ? juce::Colour(0xFFFFF8EE).withAlpha(0.94f)
            : GoodMeterLookAndFeel::textMain.withAlpha(0.88f);
        const auto burstGlowColour = isDarkTheme
            ? juce::Colour(0xFFFFF8EE).withAlpha(0.20f)
            : GoodMeterLookAndFeel::textMain.withAlpha(0.10f);
        const float scatterProgress = juce::jlimit(0.0f, 1.0f, textClearWaveProgress);
        const float textCenterX = cachedDotMatrixCentre.x;
        const float textCenterY = cachedDotMatrixCentre.y;

        auto hashed01 = [this](int a, int b) -> float
        {
            return (float) (backgroundHash(a, b) & 0xffffu) / 65535.0f;
        };

        for (size_t pointIndex = 0; pointIndex < cachedDotMatrixPoints.size(); ++pointIndex)
        {
            const auto& point = cachedDotMatrixPoints[pointIndex];
            const float px = point.x;
            const float py = point.y;
            const int seed = (int) pointIndex;
            const float dh0 = hashed01(2003 + seed * 37, 3001 + seed * 29);
            const float dh1 = hashed01(4007 + seed * 17, 5003 + seed * 31);
            const float dh2 = hashed01(6001 + seed * 19, 7001 + seed * 23);

            float drawX = px;
            float drawY = py;
            float alphaScale = 1.0f;
            float sizeScale = 1.0f;
            auto activeDot = dotColour;
            auto activeGlow = glowColour;

            if (scatterActive)
            {
                const float delay = dh0 * 0.12f;
                const float localP = juce::jlimit(0.0f, 1.0f,
                                                  (scatterProgress - delay)
                                                  / juce::jmax(0.001f, 1.0f - delay));

                if (localP <= 0.0f)
                {
                    juce::Rectangle<float> r(px, py, dot, dot);
                    g.setColour(glowColour);
                    g.fillRect(r.expanded(dot * 0.38f));
                    g.setColour(dotColour);
                    g.fillRect(r);
                    continue;
                }

                const float dx = px - textCenterX;
                const float dy = py - textCenterY;
                float angle = std::atan2(dy, dx);
                if (std::abs(dx) < 0.01f && std::abs(dy) < 0.01f)
                    angle = (dh1 - 0.5f) * juce::MathConstants<float>::twoPi;

                angle += (dh1 - 0.5f) * 1.08f;
                const float dirX = std::cos(angle);
                const float dirY = std::sin(angle);
                const float eased = 1.0f - std::pow(1.0f - localP, 2.2f);
                const float burstDistance = dot * (5.5f + dh2 * 8.0f)
                                          + std::abs(dx) * 0.24f;
                const float flutterX = std::sin(localP * 9.0f + dh2 * 7.0f) * dot * 0.55f * (1.0f - localP * 0.45f);
                const float flutterY = std::cos(localP * 7.0f + dh1 * 8.0f) * dot * 0.42f * (1.0f - localP * 0.35f);
                const float gravity = std::pow(localP, 1.55f) * dot * (1.8f + dh0 * 2.8f);

                drawX = px + dirX * burstDistance * eased + flutterX;
                drawY = py + dirY * burstDistance * eased + flutterY + gravity;
                alphaScale = juce::jlimit(0.0f, 1.0f, 1.0f - std::pow(localP, 1.28f));
                sizeScale = 1.0f + std::sin(localP * juce::MathConstants<float>::pi) * 0.24f
                                  - localP * 0.30f;
                activeDot = dotColour.interpolatedWith(burstDotColour, 0.45f + localP * 0.35f);
                activeGlow = glowColour.interpolatedWith(burstGlowColour, 0.55f + localP * 0.30f);

                if (localP > 0.18f && (seed % 3 == 0))
                {
                    const float shardP = juce::jlimit(0.0f, 1.0f, (localP - 0.18f) / 0.82f);
                    const float shardAngle = angle + (dh2 - 0.5f) * 0.9f;
                    const float shardDistance = burstDistance * (0.45f + dh1 * 0.50f) * shardP;
                    const float shardSize = juce::jmax(1.0f, dot * (0.36f - shardP * 0.14f));
                    juce::Rectangle<float> sr(px + std::cos(shardAngle) * shardDistance,
                                              py + std::sin(shardAngle) * shardDistance + gravity * 0.62f,
                                              shardSize, shardSize);
                    g.setColour(activeGlow.withMultipliedAlpha(alphaScale * 0.40f));
                    g.fillRect(sr.expanded(shardSize * 0.35f));
                    g.setColour(activeDot.withMultipliedAlpha(alphaScale * 0.55f));
                    g.fillRect(sr);
                }
            }

            if (alphaScale <= 0.02f)
                continue;

            const float sz = juce::jmax(1.0f, dot * sizeScale);
            drawY = juce::jmax(area.getY() + 0.75f, drawY);
            juce::Rectangle<float> r(drawX, drawY, sz, sz);
            g.setColour(activeGlow.withMultipliedAlpha(alphaScale));
            g.fillRect(r.expanded(sz * 0.42f));
            g.setColour(activeDot.withMultipliedAlpha(alphaScale));
            g.fillRect(r);
        }
    }

    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;

    std::unique_ptr<HoloNonoComponent> holoNono;
    std::unique_ptr<MarathonNonoComponent> asciiNono;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton importButton;
    juce::Label fileNameLabel;

    juce::AudioFormatManager clipFormatManager;
    std::vector<ClipLayerItem> clipLayerItems;
    juce::StringArray pendingClipExtractions;
    double clipViewportStartSeconds = 0.0;

    bool showImportButton = false;
    bool showClipFileNames = false;
    bool isDarkTheme = false;
    bool markerModeActive = false;
    bool markerModeJustEntered = false;
    double markerDisplayFps = 30.0;
    CharacterRenderMode currentRenderMode = CharacterRenderMode::png;
    int asciiSnapshotCounter = 0;
    bool fileNameDotMatrixDismissed = false;
    juce::String lastDotMatrixFilePath;
    std::vector<FileMarkerEntry> markerEntries;
    mutable juce::String cachedDotMatrixSourceText;
    mutable juce::String cachedDotMatrixRenderedText;
    mutable juce::Rectangle<int> cachedDotMatrixArea;
    mutable juce::Image cachedDotMatrixImage;
    mutable std::vector<juce::Point<float>> cachedDotMatrixPoints;
    mutable juce::Point<float> cachedDotMatrixCentre;
    mutable float cachedDotMatrixDotSize = 0.0f;
    float dotWaveBandLow = 0.0f;
    float dotWaveBandMid = 0.0f;
    float dotWaveBandHigh = 0.0f;

#if MARATHON_ART_STYLE
    std::unique_ptr<DotMatrixCanvas> bgCanvas;
    bool rippleActive = false;
    int rippleCenterX = 0;
    int rippleCenterY = 0;
    float rippleRadius = 0.0f;
    float rippleVelocity = 0.5f;  // Initial velocity
    const float rippleAcceleration = 0.15f;  // Acceleration per frame

    // Text clear wave animation
    bool textClearWaveActive = false;
    float textClearWaveProgress = 0.0f;  // 0.0 = start, 1.0 = complete

    // Auto ripple system
    float autoRippleTimer = 0.0f;
    int autoRipplePhase = 0;  // 0-9 cycle

    // Interactive ripple system
    bool longPressActive = false;
    int longPressCenterX = 0;
    int longPressCenterY = 0;
    float longPressRadius = 0.0f;
    int dragStartX = 0;
    int dragStartY = 0;
    double pressStartTime = 0.0;
    bool wasDragged = false;
    const float longPressMaxRadius = 2.0f;  // Reduced to 1/3
    int longPressWaveCount = 0;  // Track wave cycles
    float asciiFieldPhase = 0.0f;
    float signalBandPhase = 0.0f;

    // Fan ripple system
    bool fanRippleActive = false;
    int fanOriginX = 0;
    int fanOriginY = 0;
    float fanDirectionX = 0.0f;
    float fanDirectionY = 0.0f;
    float fanMaxRadius = 0.0f;
    float fanRadius = 0.0f;
    float fanVelocity = 0.5f;
    const float fanAngle = 0.785398f;  // 45 degrees in radians

    void triggerAutoRipple()
    {
        int w = bgCanvas->getWidth();
        int h = bgCanvas->getHeight();

        switch (autoRipplePhase)
        {
            case 0: rippleCenterX = 0; rippleCenterY = 0; break;  // Top-left
            case 1: rippleCenterX = 0; rippleCenterY = h - 1; break;  // Bottom-left
            case 2: rippleCenterX = w - 1; rippleCenterY = h - 1; break;  // Bottom-right
            case 3: rippleCenterX = w - 1; rippleCenterY = 0; break;  // Top-right
            case 4: rippleCenterX = w / 2; rippleCenterY = 0; break;  // Top-center (sweep down)
            case 5: rippleCenterX = w - 1; rippleCenterY = h - 1; break;  // Bottom-right
            case 6: rippleCenterX = w - 1; rippleCenterY = 0; break;  // Top-right
            case 7: rippleCenterX = 0; rippleCenterY = 0; break;  // Top-left
            case 8: rippleCenterX = 0; rippleCenterY = h - 1; break;  // Bottom-left
            case 9: rippleCenterX = w / 2; rippleCenterY = h - 1; break;  // Bottom-center (sweep up)
        }

        rippleRadius = 0.0f;
        rippleVelocity = 0.5f;
        rippleActive = true;

        autoRipplePhase = (autoRipplePhase + 1) % 10;
    }

    void triggerFanRipple(int originX, int originY, int dx, int dy, int dragDist)
    {
        fanOriginX = originX;
        fanOriginY = originY;

        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.01f)
        {
            fanDirectionX = dx / len;
            fanDirectionY = dy / len;
        }
        else
        {
            fanDirectionX = 1.0f;
            fanDirectionY = 0.0f;
        }

        fanMaxRadius = dragDist * 1.5f;  // Proportional to drag distance
        fanRadius = 0.0f;
        fanVelocity = 0.5f;
        fanRippleActive = true;
    }

    struct BackgroundCellStyle
    {
        char32_t symbol = U' ';
        juce::Colour colour = juce::Colours::white;
        float brightness = 0.0f;
        uint8_t sizeLevel = 0;
    };

    uint32_t backgroundHash(int x, int y) const
    {
        uint32_t seed = static_cast<uint32_t>((x + 37) * 73856093u)
                      ^ static_cast<uint32_t>((y + 91) * 19349663u)
                      ^ static_cast<uint32_t>((getCurrentSkinId() == 2 ? 0xA57Bu : 0x4D2Fu))
                      ^ static_cast<uint32_t>(isDarkTheme ? 0x91E10DA5u : 0x27D4EB2Fu);
        seed ^= seed >> 13;
        seed *= 1274126177u;
        seed ^= seed >> 16;
        return seed;
    }

    BackgroundCellStyle makeBaseBackgroundCell(int x, int y) const
    {
        BackgroundCellStyle style;
        const uint32_t hash = backgroundHash(x, y);
        style.symbol = U'⠿'; // Full 2x3 Braille cell is the stable resting state.

        // Keep the background base uniform in glyph shape and colour. Only the
        // size tier varies, so the canvas stays dense and calm instead of
        // turning into stripes or decorative guide rails.
        const uint32_t sizeBucket = hash % 100u;
        style.sizeLevel = sizeBucket < 16u ? 0
                         : sizeBucket < 46u ? 1
                         : sizeBucket < 78u ? 2
                                            : 3;

        style.colour = isDarkTheme ? juce::Colour(0xFFE7E0D6)
                                   : juce::Colour(0xFF353A42);

        const float baseBrightness = isDarkTheme ? 0.24f : 0.72f;
        const float tierLift = isDarkTheme ? 0.028f : 0.050f;
        style.brightness = juce::jlimit(0.0f, 1.0f,
                                        baseBrightness + tierLift * static_cast<float>(style.sizeLevel));
        return style;
    }

    BackgroundCellStyle makeDisturbedBackgroundCell(int x, int y, float excitation) const
    {
        static const char32_t abstractSymbols[] = { U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯', U'=' };

        auto style = makeBaseBackgroundCell(x, y);
        if (style.symbol == U' ')
            return style;

        const uint32_t hash = backgroundHash(x + 17, y + 31);
        const auto accent = getAsciiAccentColour();
        const auto secondary = getAsciiSecondaryColour();
        const auto neutral = getAsciiNeutralColour();

        style.symbol = abstractSymbols[hash % static_cast<uint32_t>(std::size(abstractSymbols))];
        style.colour = accent.interpolatedWith(secondary, 0.18f + excitation * 0.32f)
                         .interpolatedWith(neutral, isDarkTheme ? 0.10f : 0.06f);
        style.brightness = juce::jlimit(0.0f, 1.0f,
                                        (isDarkTheme ? 0.36f : 0.38f)
                                        + excitation * (isDarkTheme ? 0.44f : 0.42f));
        style.sizeLevel = juce::jlimit<uint8_t>(1, 3,
                                                static_cast<uint8_t>(style.sizeLevel
                                                                     + (excitation > 0.72f ? 2
                                                                        : (excitation > 0.42f ? 1 : 0))));
        return style;
    }

    void applyBackgroundCell(int x, int y, const BackgroundCellStyle& style)
    {
        bgCanvas->setCell(x, y, style.symbol, style.colour, 0, style.brightness, style.sizeLevel);
    }

    void randomizeBackground()
    {
        for (int y = 0; y < bgCanvas->getHeight(); ++y)
        {
            for (int x = 0; x < bgCanvas->getWidth(); ++x)
            {
                applyBackgroundCell(x, y, makeBaseBackgroundCell(x, y));
            }
        }
    }

    juce::Colour getAsciiAccentColour() const
    {
        if (getCurrentSkinId() == 2)
            return isDarkTheme ? juce::Colour(0xFFFFC736)
                               : juce::Colour(0xFFFFB830);  // warm amber (clean, not muddy)

        return isDarkTheme ? juce::Colour(0xFF68E4D6)
                           : juce::Colour(0xFF00AAFF);
    }

    juce::Colour getAsciiSecondaryColour() const
    {
        if (getCurrentSkinId() == 2)
            return isDarkTheme ? juce::Colour(0xFFFF8E68) : juce::Colour(0xFFFFD166);  // soft apricot

        return isDarkTheme ? juce::Colour(0xFF8ED8FF) : juce::Colour(0xFF62D9FF);
    }

    juce::Colour getAsciiNeutralColour() const
    {
        return isDarkTheme ? juce::Colour(0xFFF2ECE2)
                           : juce::Colour(0xFF2D3440);
    }

    void drawSignalBandOverlay(juce::Graphics& g, juce::Rectangle<float> bounds,
                               juce::Rectangle<float> heroRect, bool hasResult)
    {
        if (heroRect.isEmpty())
            return;

        const bool guoba = getCurrentSkinId() == 2;
        const float moduleH = juce::jlimit(3.0f, 7.0f, heroRect.getHeight() * 0.030f);
        const float miniH = juce::jmax(2.0f, moduleH * 0.58f);
        const float unitW = juce::jlimit(5.0f, 14.0f, heroRect.getWidth() * 0.062f);
        const float gap = unitW * 0.42f;
        const float railW = juce::jmax(1.0f, unitW * 0.13f);

        const auto primary = isDarkTheme ? juce::Colour(0xFFF0ECE4)
                                         : juce::Colour(0xFF454C56);
        const auto secondary = isDarkTheme ? juce::Colour(0xFFD9D3CA)
                                           : juce::Colour(0xFF7E8793);
        const auto tertiary = isDarkTheme ? juce::Colour(0xFFB8B1A7)
                                          : juce::Colour(0xFFA6AFBA);
        const auto hairline = isDarkTheme ? juce::Colour(0x55F0ECE4)
                                          : juce::Colour(0x30454C56);

        auto drawModule = [&] (juce::Rectangle<float> rect, juce::Colour colour, float alpha = 1.0f)
        {
            rect = rect.getIntersection(bounds);
            if (rect.isEmpty())
                return;

            g.setColour(colour.withMultipliedAlpha(alpha));
            g.fillRect(rect);
        };

        juce::ignoreUnused(hasResult, guoba, miniH, railW, tertiary, hairline, gap);

        const auto barColour = isDarkTheme ? juce::Colour(0xFFF1EEE7)
                                           : juce::Colour(0xFF101317);
        const auto secondaryBar = isDarkTheme ? juce::Colour(0xFFD9D3C8)
                                              : juce::Colour(0xFF1D232A);
        const auto lineColour = isDarkTheme ? juce::Colour(0xD8FBF6EE)
                                            : juce::Colour(0xCC11151A);

        auto promoteLongSegments = [] (std::vector<float> widths, std::initializer_list<int> longIndices)
        {
            for (int idx : longIndices)
            {
                if (!juce::isPositiveAndBelow(idx, (int) widths.size()))
                    continue;

                const float longWidth = (idx % 3 == 0) ? 5.2f
                                       : (idx % 3 == 1) ? 4.5f
                                                        : 3.9f;
                widths[(size_t) idx] = juce::jmax(widths[(size_t) idx], longWidth);
            }

            return widths;
        };

        const std::vector<float> topBandWidths{
            0.44f, 0.56f, 0.42f, 0.90f, 0.48f, 0.64f, 0.40f, 1.25f,
            0.46f, 0.58f, 0.41f, 0.82f, 0.48f, 1.90f, 0.44f, 0.56f,
            0.42f, 0.98f, 0.46f, 0.61f, 0.40f, 1.10f, 0.47f, 0.60f,
            0.43f, 2.35f, 0.48f, 0.62f, 0.42f, 0.96f, 0.46f, 0.58f,
            0.41f, 1.18f, 0.47f, 0.60f, 0.43f, 1.55f, 0.46f, 0.58f
        };
        const auto middleBandWidths = promoteLongSegments(std::vector<float>{
            0.46f, 0.60f, 0.42f, 1.05f, 0.47f, 0.63f, 0.41f, 0.88f,
            0.48f, 2.10f, 0.44f, 0.58f, 0.42f, 1.22f, 0.46f, 0.60f,
            0.41f, 0.96f, 0.48f, 0.62f, 0.43f, 1.45f, 0.46f, 0.59f,
            0.41f, 0.86f, 0.48f, 0.61f, 0.42f, 2.55f, 0.47f, 0.60f,
            0.42f, 1.18f, 0.46f, 0.58f, 0.41f, 0.96f, 0.48f, 1.35f,
            0.43f, 0.58f
        }, { 9, 21, 29, 39 });
        const std::vector<float> bottomBandWidths{
            0.45f, 0.58f, 0.42f, 0.98f, 0.46f, 0.61f, 0.41f, 1.35f,
            0.48f, 0.60f, 0.42f, 0.92f, 0.46f, 2.20f, 0.43f, 0.57f,
            0.41f, 1.08f, 0.47f, 0.62f, 0.42f, 0.96f, 0.46f, 0.58f,
            0.43f, 1.62f, 0.47f, 0.60f, 0.42f, 0.88f, 0.46f, 2.65f,
            0.43f, 0.58f, 0.41f, 1.14f, 0.47f, 0.60f, 0.42f, 1.42f
        };

        const float sideInset = 4.0f;
        const float gapRatio = 0.26f;
        const float rowScale = 1.6f;
        const float gapScale = 2.0f;

        auto bandUnitFootprint = [&] (const std::vector<float>& widths)
        {
            float total = 0.0f;
            for (float width : widths)
                total += width;

            return total + gapRatio * static_cast<float>(juce::jmax(0, (int) widths.size() - 1));
        };

        const float bandSpan = juce::jmax(120.0f, bounds.getWidth() - sideInset * 2.0f);
        const float bandH = juce::jlimit(18.0f, 34.0f, heroRect.getHeight() * 0.16f);
        const float baseRowGap = juce::jlimit(36.0f, 62.0f, heroRect.getHeight() * 0.28f);
        const float rowGap = baseRowGap * 1.5f;
        const float bottomBandY = heroRect.getY() - heroRect.getHeight() * 0.06f;
        const float middleBandY = bottomBandY - rowGap;
        const float topBandY = middleBandY - rowGap;

        auto drawBarcodeBand = [&] (float startX, float y, const std::vector<float>& widthUnits,
                                    float rowUnitW, float rowGapW,
                                    float alphaBase, juce::Colour baseColour, juce::Colour altColour,
                                    float zoneBias,
                                    std::vector<juce::Rectangle<float>>& out)
        {
            float cursor = startX;
            for (size_t idx = 0; idx < widthUnits.size(); ++idx)
            {
                const float units = widthUnits[idx];
                const float width = rowUnitW * units;
                const float zoneOffset = (idx < widthUnits.size() / 3) ? -0.04f
                                          : (idx < (widthUnits.size() * 2) / 3 ? 0.0f : 0.03f);
                const float localOffset = (idx % 5 == 0) ? 0.03f
                                          : (idx % 5 == 1) ? -0.02f
                                          : (idx % 5 == 2) ? 0.01f
                                          : (idx % 5 == 3) ? -0.035f
                                                           : 0.015f;
                const float yOffset = zoneBias + zoneOffset + localOffset;
                const float heightScale = units > 1.9f ? 0.98f
                                         : units > 1.2f ? 0.90f
                                         : (idx % 4 == 0) ? 0.78f
                                         : (idx % 4 == 1) ? 0.92f
                                         : (idx % 4 == 2) ? 0.84f
                                                          : 0.88f;
                juce::Rectangle<float> rect(cursor,
                                            y + bandH * yOffset,
                                            width,
                                            bandH * heightScale);
                out.push_back(rect);
                drawModule(rect,
                           (idx % 5 == 1 || idx % 7 == 4) ? altColour : baseColour,
                           alphaBase - 0.025f * static_cast<float>(idx % 2));
                cursor += width + rowGapW;
            }
        };

        std::vector<juce::Rectangle<float>> topBand;
        std::vector<juce::Rectangle<float>> middleBand;
        std::vector<juce::Rectangle<float>> bottomBand;

        const float baseTopUnitW = bandSpan / bandUnitFootprint(topBandWidths);
        const float baseMiddleUnitW = bandSpan / bandUnitFootprint(middleBandWidths);
        const float baseBottomUnitW = bandSpan / bandUnitFootprint(bottomBandWidths);
        const float topUnitW = baseTopUnitW * rowScale;
        const float middleUnitW = baseMiddleUnitW * rowScale;
        const float bottomUnitW = baseBottomUnitW * rowScale;
        const float topGapW = juce::jmax(1.2f, baseTopUnitW * gapRatio * gapScale);
        const float middleGapW = juce::jmax(1.2f, baseMiddleUnitW * gapRatio * gapScale);
        const float bottomGapW = juce::jmax(1.2f, baseBottomUnitW * gapRatio * gapScale);

        const float topBandX = bounds.getX() + sideInset;
        const float middleBandX = bounds.getX() + sideInset;
        const float bottomBandX = bounds.getX() + sideInset;

        drawBarcodeBand(topBandX, topBandY, topBandWidths, topUnitW, topGapW,
                        isDarkTheme ? 0.995f : 0.98f, barColour, secondaryBar, -0.02f, topBand);
        drawBarcodeBand(middleBandX, middleBandY, middleBandWidths, middleUnitW, middleGapW,
                        isDarkTheme ? 0.985f : 0.97f, barColour, secondaryBar, 0.00f, middleBand);
        drawBarcodeBand(bottomBandX, bottomBandY, bottomBandWidths, bottomUnitW, bottomGapW,
                        isDarkTheme ? 0.975f : 0.955f, barColour, secondaryBar, 0.02f, bottomBand);

        auto animatedIndex = [&] (int base, int drift, float phaseOffset, int count)
        {
            if (count <= 0)
                return 0;

            return juce::jlimit(0, count - 1,
                                base + (int) std::round(std::sin(signalBandPhase + phaseOffset)
                                                        * static_cast<float>(drift)));
        };

        auto connectAnimatedFan = [&] (const std::vector<juce::Rectangle<float>>& sources,
                                       int sourceBase, int sourceCount, int sourceDrift,
                                       const std::vector<juce::Rectangle<float>>& targets,
                                       int targetBase, int targetDrift,
                                       float targetDriftX, float targetDriftY,
                                       float phaseOffset, juce::Colour colour, float lineWidth)
        {
            if (sources.empty() || targets.empty())
                return;

            const int sourceCenter = animatedIndex(sourceBase, sourceDrift, phaseOffset,
                                                   (int) sources.size());
            const int targetIndex = animatedIndex(targetBase, targetDrift, phaseOffset + 1.3f,
                                                  (int) targets.size());
            const auto& targetRect = targets[(size_t) targetIndex];
            const juce::Point<float> target(targetRect.getCentreX()
                                                + std::sin(signalBandPhase * 1.07f + phaseOffset)
                                                    * targetDriftX,
                                            targetRect.getY()
                                                + std::cos(signalBandPhase * 0.83f + phaseOffset)
                                                    * targetDriftY);

            g.setColour(colour);
            const int half = sourceCount / 2;
            for (int i = 0; i < sourceCount; ++i)
            {
                const int idx = juce::jlimit(0, (int) sources.size() - 1,
                                             sourceCenter - half + i);
                const auto& source = sources[(size_t) idx];
                const juce::Point<float> start(source.getCentreX()
                                                   + std::sin(signalBandPhase * 0.91f
                                                              + phaseOffset
                                                              + (float) i * 0.31f) * 1.4f,
                                               source.getBottom());
                g.drawLine(start.x, start.y, target.x, target.y, lineWidth);
            }
        };

        if (!middleBand.empty())
        {
            connectAnimatedFan(topBand, 5, 6, 1,
                               middleBand, 16, 2,
                               5.5f, 1.8f, 0.15f, lineColour, 1.25f);
            connectAnimatedFan(topBand, 26, 6, 1,
                               middleBand, 23, 2,
                               6.5f, 1.4f, 1.35f, lineColour, 1.25f);
        }

        if (!bottomBand.empty())
        {
            connectAnimatedFan(middleBand, 6, 5, 2,
                               bottomBand, 10, 3,
                               4.0f, 1.2f, 0.85f, lineColour.withMultipliedAlpha(0.92f), 1.15f);
            connectAnimatedFan(middleBand, 31, 6, 2,
                               bottomBand, 30, 2,
                               7.0f, 1.6f, 2.10f, lineColour.withMultipliedAlpha(0.90f), 1.10f);
        }

        const std::vector<float> lowerBand1Widths{
            0.46f, 0.60f, 0.42f, 1.05f, 0.47f, 0.63f, 0.41f, 0.88f,
            0.48f, 2.10f, 0.44f, 0.58f, 0.42f, 1.22f, 0.46f, 0.60f,
            0.41f, 0.96f, 0.48f, 0.62f, 0.43f, 1.45f, 0.46f, 0.59f,
            0.41f, 0.86f, 0.48f, 0.61f, 0.42f, 2.55f, 0.47f, 0.60f,
            0.42f, 1.18f, 0.46f, 0.58f, 0.41f, 0.96f, 0.48f, 1.35f,
            0.43f, 0.58f
        };
        const auto lowerBand2Widths = promoteLongSegments(std::vector<float>{
            0.45f, 0.58f, 0.42f, 0.98f, 0.46f, 0.61f, 0.41f, 1.35f,
            0.48f, 0.60f, 0.42f, 0.92f, 0.46f, 2.20f, 0.43f, 0.57f,
            0.41f, 1.08f, 0.47f, 0.62f, 0.42f, 0.96f, 0.46f, 0.58f,
            0.43f, 1.62f, 0.47f, 0.60f, 0.42f, 0.88f, 0.46f, 2.65f,
            0.43f, 0.58f, 0.41f, 1.14f, 0.47f, 0.60f, 0.42f, 1.42f
        }, { 24 });
        const std::vector<float> lowerBand3Widths{
            0.44f, 0.56f, 0.42f, 0.90f, 0.48f, 0.64f, 0.40f, 1.25f,
            0.46f, 0.58f, 0.41f, 0.82f, 0.48f, 1.90f, 0.44f, 0.56f,
            0.42f, 0.98f, 0.46f, 0.61f, 0.40f, 1.10f, 0.47f, 0.60f,
            0.43f, 2.35f, 0.48f, 0.62f, 0.42f, 0.96f, 0.46f, 0.58f,
            0.41f, 1.18f, 0.47f, 0.60f, 0.43f, 1.55f, 0.46f, 0.58f
        };
        const auto lowerBand4Widths = promoteLongSegments(std::vector<float>{
            0.47f, 0.61f, 0.42f, 0.94f, 0.46f, 0.59f, 0.41f, 1.18f,
            0.47f, 0.60f, 0.42f, 0.90f, 0.45f, 1.72f, 0.43f, 0.58f,
            0.41f, 1.02f, 0.47f, 0.60f, 0.42f, 0.92f, 0.46f, 0.58f,
            0.43f, 1.28f, 0.46f, 0.59f, 0.42f, 2.18f, 0.47f, 0.60f,
            0.43f, 0.58f, 0.41f, 1.06f, 0.46f, 0.60f, 0.42f, 1.24f
        }, { 14, 29, 38 });

        auto lowerBandBaseUnitW = [&] (const std::vector<float>& widths)
        {
            return bandSpan / bandUnitFootprint(widths);
        };

        const float gridRows = (bgCanvas != nullptr) ? static_cast<float>(juce::jmax(1, bgCanvas->getHeight()))
                                                     : 36.0f;
        const float gridCellH = bounds.getHeight() / gridRows;
        const float visualBottomInset = guoba ? 8.0f : 6.0f;
        const float lowerAnchorY = heroRect.getBottom() - visualBottomInset;
        const float snappedLowerRowY = bounds.getY()
                                     + std::ceil((lowerAnchorY - bounds.getY()) / gridCellH) * gridCellH;
        const float lowerTopY = juce::jmax(bounds.getY(), snappedLowerRowY - gridCellH);
        std::vector<juce::Rectangle<float>> lowerBand1;
        std::vector<juce::Rectangle<float>> lowerBand2;
        std::vector<juce::Rectangle<float>> lowerBand3;
        std::vector<juce::Rectangle<float>> lowerBand4;

        const float lowerMaxYOffset = 0.075f;
        const float lowerMaxHeightScale = 0.98f;
        const float lowerBandBottomFactor = lowerMaxYOffset + lowerMaxHeightScale;
        const float lowerBottomTarget = bounds.getBottom();
        const float lowerBand4Y = lowerBottomTarget - bandH * lowerBandBottomFactor;
        const float lowerRowGap = juce::jmax(18.0f, (lowerBand4Y - lowerTopY) / 3.0f);
        const float lowerBand1Y = lowerTopY;
        const float lowerBand2Y = lowerBand1Y + lowerRowGap;
        const float lowerBand3Y = lowerBand2Y + lowerRowGap;

        const float lowerBand1BaseUnitW = lowerBandBaseUnitW(lowerBand1Widths);
        const float lowerBand2BaseUnitW = lowerBandBaseUnitW(lowerBand2Widths);
        const float lowerBand3BaseUnitW = lowerBandBaseUnitW(lowerBand3Widths);
        const float lowerBand4BaseUnitW = lowerBandBaseUnitW(lowerBand4Widths);
        const float lowerBand1UnitW = lowerBand1BaseUnitW * rowScale;
        const float lowerBand2UnitW = lowerBand2BaseUnitW * rowScale;
        const float lowerBand3UnitW = lowerBand3BaseUnitW * rowScale;
        const float lowerBand4UnitW = lowerBand4BaseUnitW * rowScale;
        const float lowerBand1GapW = juce::jmax(1.0f, lowerBand1BaseUnitW * gapRatio * gapScale);
        const float lowerBand2GapW = juce::jmax(1.0f, lowerBand2BaseUnitW * gapRatio * gapScale);
        const float lowerBand3GapW = juce::jmax(1.0f, lowerBand3BaseUnitW * gapRatio * gapScale);
        const float lowerBand4GapW = juce::jmax(1.0f, lowerBand4BaseUnitW * gapRatio * gapScale);

        const float lowerBandX = bounds.getX() + sideInset;

        drawBarcodeBand(lowerBandX, lowerBand1Y, lowerBand1Widths, lowerBand1UnitW, lowerBand1GapW,
                        isDarkTheme ? 0.95f : 0.93f, barColour, secondaryBar, -0.01f, lowerBand1);
        drawBarcodeBand(lowerBandX, lowerBand2Y, lowerBand2Widths, lowerBand2UnitW, lowerBand2GapW,
                        isDarkTheme ? 0.945f : 0.925f, barColour, secondaryBar, 0.01f, lowerBand2);
        drawBarcodeBand(lowerBandX, lowerBand3Y, lowerBand3Widths, lowerBand3UnitW, lowerBand3GapW,
                        isDarkTheme ? 0.94f : 0.92f, barColour, secondaryBar, -0.015f, lowerBand3);
        drawBarcodeBand(lowerBandX, lowerBand4Y, lowerBand4Widths, lowerBand4UnitW, lowerBand4GapW,
                        isDarkTheme ? 0.935f : 0.915f, barColour, secondaryBar, 0.015f, lowerBand4);

        const auto lowerLineColour = lineColour.withMultipliedAlpha(isDarkTheme ? 0.78f : 0.62f);

        if (!lowerBand2.empty())
        {
            connectAnimatedFan(lowerBand1, 6, 4, 1,
                               lowerBand2, 9, 2,
                               3.5f, 1.0f, 1.65f, lowerLineColour, 0.95f);
            connectAnimatedFan(lowerBand1, 33, 5, 1,
                               lowerBand2, 30, 3,
                               6.0f, 1.5f, 2.75f, lowerLineColour, 0.95f);
        }

        if (!lowerBand3.empty())
        {
            connectAnimatedFan(lowerBand2, 12, 5, 2,
                               lowerBand3, 16, 2,
                               5.0f, 1.1f, 0.55f, lowerLineColour.withMultipliedAlpha(0.92f), 0.90f);
            connectAnimatedFan(lowerBand2, 29, 4, 2,
                               lowerBand3, 33, 2,
                               4.2f, 1.4f, 3.05f, lowerLineColour.withMultipliedAlpha(0.88f), 0.90f);
        }

        if (!lowerBand4.empty())
        {
            connectAnimatedFan(lowerBand3, 8, 4, 1,
                               lowerBand4, 12, 2,
                               4.0f, 1.2f, 2.25f, lowerLineColour.withMultipliedAlpha(0.86f), 0.90f);
            connectAnimatedFan(lowerBand3, 27, 6, 2,
                               lowerBand4, 27, 3,
                               6.2f, 1.8f, 4.05f, lowerLineColour.withMultipliedAlpha(0.84f), 0.90f);
        }
    }

    void rippleUpdate()
    {
        int w = bgCanvas->getWidth();
        int h = bgCanvas->getHeight();

        int currentRadius = (int)rippleRadius;
        int minX = juce::jmax(0, rippleCenterX - currentRadius - 1);
        int maxX = juce::jmin(w - 1, rippleCenterX + currentRadius + 1);
        int minY = juce::jmax(0, rippleCenterY - currentRadius - 1);
        int maxY = juce::jmin(h - 1, rippleCenterY + currentRadius + 1);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                int dx = x - rippleCenterX;
                int dy = y - rippleCenterY;
                int dist = (int)std::sqrt(dx*dx + dy*dy);

                if (dist == currentRadius)
                {
                    applyBackgroundCell(x, y, makeDisturbedBackgroundCell(x, y, 0.72f));
                }
                else if (dist < currentRadius)
                {
                    auto restored = makeBaseBackgroundCell(x, y);
                    const float wake = juce::jmax(0.0f, 1.0f - (currentRadius - dist) * 0.12f);
                    restored.brightness = juce::jmin(1.0f, restored.brightness + wake * (isDarkTheme ? 0.14f : 0.06f));
                    applyBackgroundCell(x, y, restored);
                }
            }
        }

        // Accelerate
        rippleVelocity += rippleAcceleration;
        rippleRadius += rippleVelocity;

        int maxDist = (int)std::sqrt(w*w + h*h);
        if (rippleRadius > maxDist)
        {
            rippleActive = false;
        }

        repaint();
    }

    void updateFanRipple()
    {
        int w = bgCanvas->getWidth();
        int h = bgCanvas->getHeight();
        int currentRadius = (int)fanRadius;

        int minX = juce::jmax(0, fanOriginX - currentRadius - 1);
        int maxX = juce::jmin(w - 1, fanOriginX + currentRadius + 1);
        int minY = juce::jmax(0, fanOriginY - currentRadius - 1);
        int maxY = juce::jmin(h - 1, fanOriginY + currentRadius + 1);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                int dx = x - fanOriginX;
                int dy = y - fanOriginY;
                float dist = std::sqrt(dx*dx + dy*dy);

                if (std::abs(dist - fanRadius) < 1.0f)
                {
                    // Check if point is within 45-degree fan
                    float dotProduct = (dx * fanDirectionX + dy * fanDirectionY) / juce::jmax(0.01f, dist);
                    float angle = std::acos(juce::jlimit(-1.0f, 1.0f, dotProduct));

                    if (angle <= fanAngle / 2.0f)
                    {
                        applyBackgroundCell(x, y, makeDisturbedBackgroundCell(x, y, 0.66f));
                    }
                    else if (dist < fanRadius)
                    {
                        applyBackgroundCell(x, y, makeBaseBackgroundCell(x, y));
                    }
                }
            }
        }

        fanVelocity += 0.15f;
        fanRadius += fanVelocity;

        if (fanRadius > fanMaxRadius)
        {
            fanRippleActive = false;
        }

        repaint();
    }

    void updateLongPressRipple()
    {
        int w = bgCanvas->getWidth();
        int h = bgCanvas->getHeight();
        int currentRadius = (int)longPressRadius;

        int minX = juce::jmax(0, longPressCenterX - currentRadius - 1);
        int maxX = juce::jmin(w - 1, longPressCenterX + currentRadius + 1);
        int minY = juce::jmax(0, longPressCenterY - currentRadius - 1);
        int maxY = juce::jmin(h - 1, longPressCenterY + currentRadius + 1);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                int dx = x - longPressCenterX;
                int dy = y - longPressCenterY;
                int dist = (int)std::sqrt(dx*dx + dy*dy);

                if (dist == currentRadius)
                {
                    applyBackgroundCell(x, y, makeDisturbedBackgroundCell(x, y, 0.58f));
                }
                else if (dist < currentRadius)
                {
                    auto restored = makeBaseBackgroundCell(x, y);
                    restored.brightness = juce::jmin(1.0f, restored.brightness + (isDarkTheme ? 0.10f : 0.04f));
                    applyBackgroundCell(x, y, restored);
                }
            }
        }

        repaint();
    }

    void drawAnalysisText(juce::Graphics& g)
    {
        const auto& result = holoNono->getAnalysisResult();
        auto bounds = getLocalBounds();
        int gridH = bgCanvas->getHeight();
        int gridW = bgCanvas->getWidth();
        float cellW = bounds.getWidth() / gridW;
        float cellH = bounds.getHeight() / (float)gridH;

        // Calculate wave position (0.0 = full width, 1.0 = left edge)
        float waveX = textClearWaveActive ? (bounds.getWidth() * (1.0f - textClearWaveProgress)) : bounds.getWidth();

        g.setColour(isDarkTheme ? juce::Colours::white.withAlpha(0.95f)
                                : GoodMeterLookAndFeel::textMain.withAlpha(0.96f));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold));

        // Row -5 (bottom 5th row): Peak (left) and Momentary (right)
        float row5Y = bounds.getBottom() - cellH * 5;

        // Clip text to only show left of wave
        g.saveState();
        g.reduceClipRegion(juce::Rectangle<int>(0, (int)row5Y, (int)waveX, (int)cellH));
        g.drawText("Peak " + juce::String(result.peakDBFS, 1) + " dBFS",
                   20, row5Y, 180, cellH, juce::Justification::centredLeft, false);
        g.drawText("Momentary " + juce::String(result.momentaryMaxLUFS, 1) + " LUFS",
                   bounds.getRight() - 200, row5Y, 180, cellH, juce::Justification::centredLeft, false);
        g.restoreState();

        // Row -4 (bottom 4th row): Short-term (left) and Integrated (right)
        float row4Y = bounds.getBottom() - cellH * 4;

        g.saveState();
        g.reduceClipRegion(juce::Rectangle<int>(0, (int)row4Y, (int)waveX, (int)cellH));
        g.drawText("Short-term " + juce::String(result.shortTermMaxLUFS, 1) + " LUFS",
                   20, row4Y, 180, cellH, juce::Justification::centredLeft, false);
        g.drawText("Integrated " + juce::String(result.integratedLUFS, 1) + " LUFS",
                   bounds.getRight() - 200, row4Y, 180, cellH, juce::Justification::centredLeft, false);
        g.restoreState();
    }
#endif
};
