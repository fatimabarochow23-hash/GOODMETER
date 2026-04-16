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

    std::function<void(const juce::File&)> onImportedMediaCopied;

    NonoPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
#if MARATHON_ART_STYLE
        // Create background canvas (21x24, adjusted density)
        bgCanvas = std::make_unique<DotMatrixCanvas>(21, 24);
        randomizeBackground();
#endif

        // Nono character (PNG sprite)
        holoNono = std::make_unique<HoloNonoComponent>(processor);
        holoNono->onImportFileChosen = [this](const juce::URL& url)
        {
            handleImportedUrl(url);
        };
        holoNono->setShowAnalysisResults(false);  // Disable bubble in Marathon mode
        holoNono->onClearResultsRequested = [this]()
        {
            textClearWaveActive = true;
            textClearWaveProgress = 0.0f;
        };
        addAndMakeVisible(holoNono.get());

        // Codex: 主人明确要求保留现有 PNG 精灵模式，再额外加一套 ASCII
        // 数字生命模式，所以我不替换 HoloNono，而是并排保留两套可见层。
        // HoloNono 继续承担现有导入/分析链；MarathonNono 先作为 ASCII
        // 呈现原型层，后面我们再继续深化成更完整的数字生命动效。
        asciiNono = std::make_unique<MarathonNonoComponent>(processor);
        asciiNono->setInterceptsMouseClicks(false, false);
        asciiNono->setVisible(false);
        addAndMakeVisible(asciiNono.get());
        asciiNono->setSkin(MarathonNonoComponent::SkinType::Nono);
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
        addAndMakeVisible(fileNameLabel);

        startTimerHz(30);  // 30Hz for smooth ripple animation
    }

    ~NonoPageComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
#if MARATHON_ART_STYLE
        g.fillAll(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);

        bool hasResult = (holoNono && holoNono->hasAnalysisResult());

        // Draw background symbols (skip text rows if showing results)
        juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain);
        static constexpr float sizeScales[] = { 0.56f, 0.82f, 1.04f, 1.28f };
        int gridH = bgCanvas->getHeight();
        int gridW = bgCanvas->getWidth();
        auto bounds = getLocalBounds().toFloat();
        float cellW = bounds.getWidth() / gridW;
        float cellH = bounds.getHeight() / gridH;

        for (int y = 0; y < gridH; ++y)
        {
            // Skip row -5 and -4 (text rows) if showing results (but not during clear animation)
            if (hasResult && !textClearWaveActive && (y == gridH - 5 || y == gridH - 4))
                continue;

            for (int x = 0; x < gridW; ++x)
            {
                auto cell = bgCanvas->getCell(x, y);
                float px = x * cellW;
                float py = y * cellH;

                const float scale = sizeScales[juce::jlimit<int>(0, 3, static_cast<int>(cell.sizeLevel))];
                auto symbolColour = isDarkTheme
                    ? cell.color.withMultipliedAlpha(cell.brightness)
                    : cell.color.interpolatedWith(GoodMeterLookAndFeel::textMain, 0.34f)
                        .withAlpha(0.070f + cell.brightness * 0.24f);

                if (currentRenderMode == CharacterRenderMode::ascii)
                {
                    const float nx = (static_cast<float>(x) + 0.5f) / static_cast<float>(gridW);
                    const float ny = (static_cast<float>(y) + 0.5f) / static_cast<float>(gridH);
                    const float dx = (nx - 0.5f) / 0.23f;
                    const float dy = (ny - 0.31f) / 0.16f;
                    const float focus = std::exp(-(dx * dx + dy * dy) * 1.15f);
                    const float suppression = isDarkTheme ? 0.82f : 0.90f;
                    symbolColour = symbolColour.withMultipliedAlpha(1.0f - focus * suppression);
                }

                g.setColour(symbolColour);
                g.setFont(monoFont.withHeight(monoFont.getHeight() * scale));
                juce::String str = juce::String::charToString(cell.symbol);
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

        // Codex: 主人明确指出“背景全是抽象图形，中间再围着一个盲文精灵会很杂”，
        // 所以这一轮我先停掉持续存在的 ASCII 额外信号场，只保留盲文主导的
        // 背景静息态；交互时再由 ripple 逻辑临时打散成抽象图形。

        // Draw analysis results
        if (hasResult)
        {
            drawAnalysisText(g);
        }
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
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
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

        // Text clear wave animation
        if (textClearWaveActive)
        {
            textClearWaveProgress += 0.10f;  // ~0.3 second animation at 30fps
            if (textClearWaveProgress >= 1.0f)
            {
                textClearWaveActive = false;
                textClearWaveProgress = 0.0f;
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

#if MARATHON_ART_STYLE
        if (currentRenderMode == CharacterRenderMode::ascii)
        {
            asciiFieldPhase += 0.042f;
            refreshAsciiSpriteSnapshot();
            repaint();
        }
#endif
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
        refreshAsciiSpriteSnapshot(true);
        randomizeBackground();
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
        currentRenderMode = (mode == 1) ? CharacterRenderMode::ascii
                                        : CharacterRenderMode::png;
        syncCharacterRendererVisibility();
    }

    int getCharacterRenderMode() const
    {
        return currentRenderMode == CharacterRenderMode::ascii ? 1 : 0;
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
        const bool showAscii = currentRenderMode == CharacterRenderMode::ascii;
        if (showAscii)
            refreshAsciiSpriteSnapshot(true);

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

        fileNameLabel.setText(audioEngine.getCurrentFileName(), juce::dontSendNotification);
        fileNameLabel.setVisible(true);
        resized();
        return true;
    }

    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;

    std::unique_ptr<HoloNonoComponent> holoNono;
    std::unique_ptr<MarathonNonoComponent> asciiNono;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton importButton;
    juce::Label fileNameLabel;

    bool showImportButton = false;
    bool isDarkTheme = false;
    CharacterRenderMode currentRenderMode = CharacterRenderMode::png;
    int asciiSnapshotCounter = 0;

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
                         .interpolatedWith(neutral, isDarkTheme ? 0.10f : 0.20f);
        style.brightness = juce::jlimit(0.0f, 1.0f,
                                        (isDarkTheme ? 0.36f : 0.20f)
                                        + excitation * (isDarkTheme ? 0.44f : 0.24f));
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
            return juce::Colour(0xFFFFC736);

        return juce::Colour(0xFF68E4D6);
    }

    juce::Colour getAsciiSecondaryColour() const
    {
        if (getCurrentSkinId() == 2)
            return isDarkTheme ? juce::Colour(0xFFFF8E68) : juce::Colour(0xFFE46645);

        return isDarkTheme ? juce::Colour(0xFF8ED8FF) : juce::Colour(0xFF4AA2FF);
    }

    juce::Colour getAsciiNeutralColour() const
    {
        return isDarkTheme ? juce::Colour(0xFFF2ECE2)
                           : juce::Colour(0xFF2D3440);
    }

    void drawAsciiSignalField(juce::Graphics& g, juce::Rectangle<float> bounds, bool hasResult)
    {
        static const char32_t guobaSymbols[] = { U'.', U':', U'+', U'=', U'#', U'□', U'%' };
        static const char32_t nonoSymbols[] = { U'.', U':', U'/', U'\\', U'+', U'#', U'✕' };

        const int gridH = bgCanvas->getHeight();
        const int gridW = bgCanvas->getWidth();
        const float cellW = bounds.getWidth() / gridW;
        const float cellH = bounds.getHeight() / gridH;
        const float centerX = gridW * 0.50f;
        const float centerY = showImportButton ? gridH * 0.41f : gridH * 0.40f;
        const bool guoba = getCurrentSkinId() == 2;
        const auto accent = getAsciiAccentColour();
        const auto secondary = getAsciiSecondaryColour();
        const auto neutral = getAsciiNeutralColour();
        const char32_t* ambientSymbols = guoba ? guobaSymbols : nonoSymbols;
        static constexpr float sizeScales[] = { 0.56f, 0.82f, 1.04f, 1.24f };

        auto distanceToSegment = [] (float px, float py, float ax, float ay, float bx, float by)
        {
            const float abx = bx - ax;
            const float aby = by - ay;
            const float apx = px - ax;
            const float apy = py - ay;
            const float ab2 = abx * abx + aby * aby;
            if (ab2 <= 0.0001f)
                return std::sqrt(apx * apx + apy * apy);

            const float t = juce::jlimit(0.0f, 1.0f, (apx * abx + apy * aby) / ab2);
            const float qx = ax + abx * t;
            const float qy = ay + aby * t;
            const float dx = px - qx;
            const float dy = py - qy;
            return std::sqrt(dx * dx + dy * dy);
        };

        juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain);
        g.setFont(monoFont);

        for (int y = 0; y < gridH; ++y)
        {
            if (hasResult && !textClearWaveActive && (y == gridH - 5 || y == gridH - 4))
                continue;

            for (int x = 0; x < gridW; ++x)
            {
                const float nx = (x - centerX) / (guoba ? 6.7f : 6.0f);
                const float ny = (y - centerY) / (guoba ? 5.4f : 5.0f);
                const float dist = std::sqrt(nx * nx + ny * ny);
                const float head = std::exp(-(nx * nx * 2.6f + std::pow((ny + 0.34f) / 0.86f, 2.0f) * 2.8f));
                const float torso = std::exp(-(nx * nx * 2.0f + std::pow((ny - 0.26f) / 1.05f, 2.0f) * 1.8f));
                const float leftEar = std::exp(-(std::pow((nx + 0.64f) / 0.28f, 2.0f) + std::pow((ny + 1.08f) / 0.32f, 2.0f)));
                const float rightEar = std::exp(-(std::pow((nx - 0.64f) / 0.28f, 2.0f) + std::pow((ny + 1.08f) / 0.32f, 2.0f)));
                const float shell = std::exp(-std::pow(dist - (guoba ? 1.06f : 1.00f), 2.0f) * 18.0f);
                const float wake = std::exp(-(nx * nx) * 17.0f)
                                 * (0.5f + 0.5f * std::sin(asciiFieldPhase * 2.8f - y * 0.48f));
                const float crossflow = std::exp(-(ny * ny) * 9.0f)
                                      * (0.5f + 0.5f * std::sin(asciiFieldPhase * 1.9f + x * 0.31f));
                const float topFeed = std::exp(-std::pow(distanceToSegment(nx, ny, 0.0f, -1.85f, 0.0f, -0.18f), 2.0f) * 36.0f);
                const float leftFeed = std::exp(-std::pow(distanceToSegment(nx, ny, -1.78f, guoba ? -0.10f : 0.02f, -0.34f, 0.06f), 2.0f) * 34.0f);
                const float rightFeed = std::exp(-std::pow(distanceToSegment(nx, ny, 1.78f, guoba ? -0.10f : 0.02f, 0.34f, 0.06f), 2.0f) * 34.0f);
                const float bottomFeed = std::exp(-std::pow(distanceToSegment(nx, ny, 0.0f, 1.86f, 0.0f, 0.54f), 2.0f) * 26.0f);
                const float feedPulse = 0.5f + 0.5f * std::sin(asciiFieldPhase * 3.1f - y * 0.18f + x * 0.22f);
                const float carrier = juce::jmax(juce::jmax(topFeed, leftFeed), juce::jmax(rightFeed, bottomFeed))
                                    * (0.28f + 0.52f * feedPulse);
                const float orbit = 0.5f + 0.5f * std::sin(asciiFieldPhase * 1.8f + x * 0.42f + y * 0.28f);
                const float voidCore = std::exp(-(nx * nx * 9.6f + ny * ny * 7.2f));
                const float stageMask = std::exp(-(nx * nx * 2.2f + ny * ny * 1.8f));
                const float silhouetteMask = juce::jmax(guoba ? juce::jmax(head * 0.84f, torso) : juce::jmax(head, torso),
                                                        juce::jmax(leftEar, rightEar));
                const float density = juce::jmax(carrier,
                                                 juce::jmax(shell * (0.06f + orbit * 0.08f),
                                                            juce::jmax(wake * 0.05f, crossflow * 0.04f)));

                if (density < 0.12f)
                    continue;

                if ((silhouetteMask > 0.06f || stageMask > 0.36f) && carrier < 0.58f)
                    continue;

                if (voidCore > (guoba ? 0.14f : 0.12f) && carrier < 0.52f)
                    continue;

                if (((x + y) & 1) != 0 && density < 0.28f)
                    continue;

                const float shellProximity = std::abs(dist - (guoba ? 1.06f : 1.00f));
                char32_t symbol = ambientSymbols[(x + y + (int) std::floor(asciiFieldPhase * 2.6f)) % 7];
                if (carrier > 0.34f)
                    symbol = guoba ? U'=' : (((x + y) & 1) == 0 ? U'/' : U'\\');
                else if (shellProximity < 0.06f && density > 0.22f)
                    symbol = guoba ? U'=' : U'/';

                juce::Colour symbolColour = neutral;
                if (carrier > 0.42f)
                    symbolColour = accent.interpolatedWith(secondary, 0.22f);
                else if (shellProximity < 0.08f)
                    symbolColour = secondary.interpolatedWith(neutral, 0.58f);
                else if (density > 0.26f)
                    symbolColour = accent.interpolatedWith(neutral, 0.74f);

                symbolColour = symbolColour.interpolatedWith(neutral, isDarkTheme ? 0.22f : 0.34f);

                const float alpha = isDarkTheme
                    ? juce::jlimit(0.0f, 0.26f, density * 0.42f + carrier * 0.10f)
                    : juce::jlimit(0.0f, 0.10f, density * 0.22f + carrier * 0.04f);

                uint8_t sizeLevel = 0;
                if (carrier > 0.52f || density > 0.42f)
                    sizeLevel = 2;
                else if (carrier > 0.34f || density > 0.24f)
                    sizeLevel = 1;
                if (shellProximity < 0.05f && density > 0.32f)
                    sizeLevel = 3;

                const float scale = sizeScales[sizeLevel];
                const float drawW = cellW * juce::jmin(1.32f, scale + 0.14f);
                const float drawH = cellH * juce::jmin(1.32f, scale + 0.10f);
                g.setColour(symbolColour.withAlpha(alpha));
                g.setFont(monoFont.withHeight(monoFont.getHeight() * scale));
                g.drawText(juce::String::charToString(symbol),
                           juce::Rectangle<int>((int) (x * cellW + (cellW - drawW) * 0.5f),
                                                (int) (y * cellH + (cellH - drawH) * 0.5f),
                                                (int) drawW,
                                                (int) drawH),
                           juce::Justification::centred, false);
            }
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
