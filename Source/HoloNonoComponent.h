/*
  ==============================================================================
    HoloNonoComponent.h
    GOODMETER - Cyber-Alchemist Companion "NONO" - ULTIMATE INTERACTIVE FORM

    Interactive robot with offline audio analysis:
    - Double-click to flip (front <-> back)
    - Back face: "+" icon, click or drag audio file to analyze
    - Analysis: blue ripple animation, async LUFS/peak thread
    - Results: peak dBFS, momentary/short-term max, integrated LUFS
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
struct NonoAnalysisResult
{
    float peakDBFS          = -100.0f;
    float momentaryMaxLUFS  = -100.0f;
    float shortTermMaxLUFS  = -100.0f;
    float integratedLUFS    = -100.0f;
    float centerLUFS        = -100.0f;   // Center channel (C) integrated LUFS (>= 6ch only)
    int   numChannels       = 0;
};

//==============================================================================
class HoloNonoComponent : public juce::Component,
                           public juce::Timer
#if ! JUCE_IOS
                         , public juce::FileDragAndDropTarget
#endif
{
public:
    //==========================================================================
    enum class SkinType { Guoba, Nono };

    HoloNonoComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        setSize(100, 200);
        startTimerHz(60);

        // Load GUOBA sprite from BinaryData
        guobaSprite = juce::ImageCache::getFromMemory(
            BinaryData::guoba_png, BinaryData::guoba_pngSize);

        // Load GUOBA nose sprite
        guobaNose = juce::ImageCache::getFromMemory(
            BinaryData::guoba_nose_png, BinaryData::guoba_nose_pngSize);

        lastScreenPos = getScreenPosition();
        lastW = getWidth();
        lastH = getHeight();

        auto& rng = juce::Random::getSystemRandom();
        for (auto& b : tubeBubbles)
        {
            b.xOff = rng.nextFloat() * 0.6f - 0.3f;
            b.phase = rng.nextFloat();
            b.sz = 1.5f + rng.nextFloat() * 2.5f;
            b.spd = 0.008f + rng.nextFloat() * 0.012f;
        }
    }

    ~HoloNonoComponent() override
    {
        stopTimer();
        if (analysisThread)
        {
            analysisThread->signalThreadShouldExit();
            analysisThread->notify();  // wake thread if sleeping/waiting
            analysisThread->stopThread(5000);  // generous timeout for large files
            analysisThread.reset();
        }
    }

    //==========================================================================
    // Card fold/unfold callbacks (only active in Front state)
    //==========================================================================

    // Callback: double-click test tube → enter jiggle/edit mode
    std::function<void()> onTestTubeDoubleClicked;

    // Callback: double-click NONO body while in edit mode → exit jiggle mode
    std::function<void()> onExitJiggleMode;

    // Callback: right-double-click → toggle Mini Mode
    std::function<void()> onRightDoubleClick;

    // Callback: smile orbit triggered → editor launches card fly-out
    std::function<void()> onSmileOrbitTriggered;

    // Callback: mouse hover enters/exits Nono body region
    std::function<void()> onBodyHoverEnter;
    std::function<void()> onBodyHoverExit;

    // Callback: ear-pinch flip back from analysis/back state
    std::function<void()> onEarFlipBack;

    // Skin system
    SkinType currentSkin = SkinType::Guoba;
    void setSkin(SkinType s)
    {
        if (s == currentSkin) return;
        currentSkin = s;
        // Reset holo visor when switching skins
        isHoloVisor = false;
        holoTransition = 0.0f;
        repaint();
    }
    SkinType getSkin() const { return currentSkin; }
    bool isGuoba() const { return currentSkin == SkinType::Guoba; }

    // Skin selector ComboBox (for embedding in MeterCard header)
    juce::ComboBox& getSkinMenu() { return skinMenu; }

    void initSkinMenu()
    {
        skinMenu.addItem("Nono", 1);
        skinMenu.addItem("Guoba", 2);
        skinMenu.setSelectedId(currentSkin == SkinType::Nono ? 1 : 2, juce::dontSendNotification);
        skinMenu.setJustificationType(juce::Justification::centredRight);
        skinMenu.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        skinMenu.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        skinMenu.setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
        skinMenu.onChange = [this]() {
            switch (skinMenu.getSelectedId())
            {
                case 1: setSkin(SkinType::Nono);  break;
                case 2: setSkin(SkinType::Guoba); break;
                default: break;
            }
        };
        addAndMakeVisible(skinMenu);
    }

    /** Force-cancel an orbit in progress (called when animation launch fails) */
    void cancelOrbit()
    {
        isOrbiting = false;
        isOrbitLocked = false;
        isSmiling = false;
        orbitProgress = 0.0f;
        smileFramesLeft = 0;
        orbitLockFramesLeft = 0;
    }

    /** Programmatic file analysis — callable from iOS page or external code.
        Flips to back, runs offline LUFS analysis, shows results. */
    void analyzeFile(const juce::File& file)
    {
        if (!file.existsAsFile()) return;

        // If on front, flip to back first then analyze
        if (nonoState == NonoState::Front || nonoState == NonoState::ShowingResults)
        {
            pendingAnalysisFile = file;
            flipTarget = 1.0f;
            nonoState = NonoState::Flipping;
            flipDestination = NonoState::Analyzing;
        }
        else
        {
            startAnalysis(file);
        }
    }

    /** Trigger shy expression (>< eyes) for 1.5 seconds — callable from editor */
    void triggerShyExpression()
    {
        isShy = true;
        shyFramesLeft = 90;  // 1.5 seconds at 60Hz
    }

    /** Trigger amber extraction face animation (runs until stopExtractExpression is called) */
    void triggerExtractExpression()
    {
        isExtractingVideo = true;
        extractFrameCounter = 0;

        // Generate synthetic waveform data for the amber grid
        if (extractSyntheticHistory.empty())
        {
            juce::Random rng;
            for (int i = 0; i < 200; ++i)
            {
                GridColumn col;
                col.level = 0.05f + rng.nextFloat() * 0.55f;
                col.randomSeed = static_cast<uint32_t>(rng.nextInt());
                extractSyntheticHistory.push_back(col);
            }
        }
    }

    /** Stop amber extraction face animation */
    void stopExtractExpression()
    {
        isExtractingVideo = false;
        extractFrameCounter = 0;
        extractSyntheticHistory.clear();
    }

    /** Trigger time-rewind holographic face animation for 1.5 seconds */
    void triggerRewindExpression()
    {
        isRewinding = true;
        rewindFramesLeft = 90;  // 1.5 seconds at 60Hz
        rewindStartMs = juce::Time::getMillisecondCounterHiRes();

        // Snapshot the current gridWaveformHistory for reverse playback
        rewindSnapshot = gridWaveformHistory;

        // If snapshot is empty, generate synthetic data so the effect still looks cool
        if (rewindSnapshot.empty())
        {
            juce::Random rng;
            for (int i = 0; i < 150; ++i)
            {
                GridColumn col;
                col.level = 0.1f + rng.nextFloat() * 0.6f;
                col.randomSeed = static_cast<uint32_t>(rng.nextInt());
                rewindSnapshot.push_back(col);
            }
        }
    }

    // Callback: local drag in floating phase (instead of moving the window)
    // dx, dy are screen-space deltas from drag start
    std::function<void(int dx, int dy)> onLocalDrag;

    // Flag: when true, dragging Nono moves its local bounds (not the window)
    bool useLocalDrag = false;

    // Edit mode flag (set by PluginEditor)
    bool isEditMode = false;

    // Wink state (set by PluginEditor when swap is ready)
    bool isWinking = false;
    void setWinking(bool w) { isWinking = w; repaint(); }

    // Dizzy state (nausea accumulator driven)
    bool isDizzy = false;

    void onCardFolded(const juce::String& cardName)
    {
        if (nonoState != NonoState::Front) return;
        if (cardName == "SPECTROGRAM")
        {
            triggerCollision(CollisionDir::Up);
            return;
        }
        if (cardName == "STEREO")
        {
            triggerCollision(CollisionDir::Left);
            return;
        }
        triggerFrontAnim(FrontAnim::FlashEyes);
    }

    void onCardUnfolded(const juce::String& /*cardName*/)
    {
        if (nonoState != NonoState::Front) return;
        triggerFrontAnim(FrontAnim::FlashEyes);
    }

    //==========================================================================
    // Mouse: double-click body = flip/clear, double-click tube = edit mode
    //==========================================================================
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        // Cancel any pending smile click (this is a double-click, not a single-click)
        pendingSmileClick = false;

        // Block all interactions during orbit
        if (isOrbitLocked) return;

        // Right-double-click → toggle Mini Mode
        if (e.mods.isRightButtonDown())
        {
            if (onRightDoubleClick != nullptr)
                onRightDoubleClick();
            return;
        }

        auto pos = e.position;

        // Test tube region → enter edit mode (only when NOT in edit mode)
        if (tubeHitRect.contains(pos))
        {
            if (!isEditMode && onTestTubeDoubleClicked)
                onTestTubeDoubleClicked();
            return;
        }

        // Lightning badge double-click → toggle holo visor mode
        if ((nonoState == NonoState::Front || nonoState == NonoState::ShowingResults)
            && !badgeHitRect.isEmpty() && badgeHitRect.contains(pos))
        {
            triggerLightningToggle();
            return;
        }

        // Body region
        if (!bodyHitRect.contains(pos))
            return;

        // If in edit mode, body double-click = EXIT edit mode
        if (isEditMode)
        {
            if (onExitJiggleMode)
                onExitJiggleMode();
            return;
        }

        // Normal mode: flip/clear logic
        switch (nonoState)
        {
            case NonoState::Front:
                flipTarget = 1.0f;
                nonoState = NonoState::Flipping;
                flipDestination = NonoState::Back;
                break;

            case NonoState::Back:
                flipTarget = 0.0f;
                nonoState = NonoState::Flipping;
                flipDestination = NonoState::Front;
                break;

            case NonoState::ShowingResults:
                nonoState = NonoState::ClearingData;
                targetPourAngle = juce::degreesToRadians(120.0f);
                bubbleFadeAlpha = 1.0f;
                break;

            default: break;
        }
    }

    //==========================================================================
    // Drag & Drop (desktop only — iOS uses UIDocumentPicker via FileChooser)
    //==========================================================================
#if ! JUCE_IOS
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& f : files)
        {
            auto ext = juce::File(f).getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" ||
                ext == ".aif" || ext == ".flac" || ext == ".ogg")
                return true;
        }
        return false;
    }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        isDragHovering = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override
    {
        isDragHovering = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        isDragHovering = false;
        if (isOrbitLocked) return;

        for (const auto& f : files)
        {
            juce::File file(f);
            auto ext = file.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" ||
                ext == ".aif" || ext == ".flac" || ext == ".ogg")
            {
                // If on front, flip to back first then analyze
                if (nonoState == NonoState::Front || nonoState == NonoState::ShowingResults)
                {
                    pendingAnalysisFile = file;
                    flipTarget = 1.0f;
                    nonoState = NonoState::Flipping;
                    flipDestination = NonoState::Analyzing;
                }
                else
                {
                    startAnalysis(file);
                }
                return;
            }
        }
    }
#endif // ! JUCE_IOS

    //==========================================================================
    // Paint
    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty() || bounds.getHeight() < 40.0f)
            return;

        // ===== 动态空间绝对切割 =====
        bool showBubble = ((nonoState == NonoState::ShowingResults
                            || nonoState == NonoState::ClearingData) && hasResults);
        juce::Rectangle<float> bubbleArea;
        auto nonoDrawArea = bounds;

        if (showBubble)
            bubbleArea = nonoDrawArea.removeFromBottom(nonoDrawArea.getHeight() * 0.38f);

        const float unit = juce::jmin(nonoDrawArea.getWidth(), nonoDrawArea.getHeight());
        const float radius = unit * 0.18f;

        // Orbit offset: smooth elliptical loop around panel inner border
        float orbitOX = 0.0f, orbitOY = 0.0f;
        if (isOrbiting && orbitProgress > 0.0f)
        {
            float angle = orbitProgress * juce::MathConstants<float>::twoPi;
            // Smooth envelope: sin(π·t) → departs from center, returns to center
            float envelope = std::sin(orbitProgress * juce::MathConstants<float>::pi);
            // Maximum safe radii: panel half-size minus body+tube footprint, ×0.88 safety margin
            float orbitRx = juce::jmax(10.0f, (nonoDrawArea.getWidth() * 0.5f - radius * 2.0f) * 0.88f);
            float orbitRy = juce::jmax(10.0f, (nonoDrawArea.getHeight() * 0.5f - radius * 1.2f) * 0.85f);
            orbitOX = envelope * orbitRx * std::sin(angle);
            orbitOY = envelope * orbitRy * -std::cos(angle);
        }

        const float cx = nonoDrawArea.getCentreX() - radius * 0.6f + idleOffsetX + collisionOffsetX + orbitOX;
        const float cy = nonoDrawArea.getCentreY() - radius * 0.3f + idleOffsetY + collisionOffsetY + orbitOY;

        // Update hit test regions
        if (isGuoba())
        {
            // GUOBA body hit rect: based on sprite bounds (trimmed bottom 20%)
            float spriteH = radius * 4.0f;
            float spriteW = spriteH;
            float anchorY = cy - radius * 0.1f;
            float spriteY = anchorY - spriteH * 0.38f;
            bodyHitRect = { cx - spriteW * 0.5f, spriteY, spriteW, spriteH * 0.80f };
        }
        else
        {
            // NONO body hit rect: elliptical body
            bodyHitRect = { cx - radius, cy - radius, radius * 2.0f, radius * 2.0f };
        }
        float htX = cx + radius * 1.7f;
        float htY = cy - radius * 0.1f;
        tubeHitRect = { htX - radius * 0.3f, htY - radius * 0.6f, radius * 0.6f, radius * 1.2f };

        // Compute flip visual scale
        float hScale = 1.0f;
        bool showFront = true;

        if (nonoState == NonoState::Flipping)
        {
            if (flipProgress < 0.5f)
            {
                hScale = 1.0f - flipProgress * 2.0f;
                showFront = (flipDestination == NonoState::Back || flipDestination == NonoState::Analyzing);
            }
            else
            {
                hScale = (flipProgress - 0.5f) * 2.0f;
                showFront = !(flipDestination == NonoState::Back || flipDestination == NonoState::Analyzing);
            }
        }
        else
        {
            showFront = (nonoState == NonoState::Front || nonoState == NonoState::ShowingResults
                         || nonoState == NonoState::ClearingData);
            hScale = 1.0f;
        }

        hScale = juce::jmax(0.02f, hScale);

        // Ear hit rects — skin-dependent
        if (isGuoba())
        {
            // GUOBA ears at ~(24%, 5%) and (76%, 5%) in the PNG — top of head
            float spriteH = radius * 4.0f;
            float spriteW = spriteH;
            float anchorY = cy - radius * 0.1f;
            float spriteY = anchorY - spriteH * 0.38f;
            float earLX = cx - spriteW * 0.5f * hScale + spriteW * 0.24f * hScale;
            float earRX = cx - spriteW * 0.5f * hScale + spriteW * 0.76f * hScale;
            float earY = spriteY + spriteH * 0.05f;
            float earW = radius * 0.9f * hScale;
            float earH = radius * 0.9f;
            earHitRect[0] = { earLX - earW * 0.5f, earY - earH * 0.3f, earW, earH };
            earHitRect[1] = { earRX - earW * 0.5f, earY - earH * 0.3f, earW, earH };
        }
        else
        {
            // NONO ears: positioned at holographic blade anchors
            const float sphereTop = cy - radius;
            float earSpecs[2][2] = { {-0.72f, -0.18f}, {0.72f, -0.18f} };
            for (int i = 0; i < 2; ++i)
            {
                float eax = cx + radius * earSpecs[i][0] * hScale;
                float eay = sphereTop + radius * earSpecs[i][1] + earBobOffset;
                float earW = radius * 0.35f * hScale;
                float earH = radius * 0.75f;
                earHitRect[i] = { eax - earW * 0.5f, eay - earH * 0.5f, earW, earH };
            }
        }

        // Lightning badge hit rect: belt buckle at ~(50%, 78%) of sprite
        {
            float spriteH = radius * 4.0f;
            float spriteW = spriteH;
            float anchorY = cy - radius * 0.1f;
            float spriteY = anchorY - spriteH * 0.38f;
            float badgeCX = cx;                            // center of sprite
            float badgeCY = spriteY + spriteH * 0.78f;    // 78% down from top
            float badgeR  = radius * 0.35f * hScale;      // generous hit area
            badgeHitRect = { badgeCX - badgeR, badgeCY - badgeR * 0.7f,
                             badgeR * 2.0f, badgeR * 1.4f };
        }

        // Store visor hit path for back-face click detection (uses same coords as drawVisor/drawBackFace)
        visorHitPath = buildVisorPath(cx, cy - radius * 0.06f, radius, hScale);

        // Draw layers
        drawAntiGravityGlow(g, cx, cy, radius);
        drawShadow(g, cx, cy, radius);
        drawHolographicEars(g, cx, cy, radius, hScale);
        drawBody(g, cx, cy, radius, hScale);

        if (showFront)
        {
            drawVisor(g, cx, cy, radius, hScale);

            if (audioProcessor.audioRecorder.getIsRecording())
                drawRecordingGrid(g, cx, cy, radius, hScale);
            else
                drawEyes(g, cx, cy, radius, hScale);
        }
        else
        {
            drawBackFace(g, cx, cy, radius, hScale);
        }

        // Drag hover highlight
        if (isDragHovering)
        {
            g.setColour(accentCol().withAlpha(0.25f));
            g.drawEllipse(cx - radius * hScale - 4.0f, cy - radius - 4.0f,
                          radius * hScale * 2.0f + 8.0f, radius * 2.0f + 8.0f, 3.0f);
        }

        // Analysis ripples
        if (nonoState == NonoState::Analyzing)
            drawAnalysisRipples(g, cx, cy, radius);

        // Floating test tube (always visible)
        drawFloatingTestTube(g, cx, cy, radius);

        // Results bubble in dedicated bottom area
        if (showBubble)
            drawResultsBubble(g, bubbleArea, cx);

        drawParticles(g, cx, cy, radius);

        // Lightning VFX (drawn on top of everything)
        if (showLightningVFX)
            drawLightningVFX(g, cx, cy, radius);
    }

    void resized() override {}

    //==========================================================================
    // Hit test: only return true for Nono's visible body/tube regions.
    // Transparent areas return false → macOS passes clicks through to desktop.
    //==========================================================================
    bool hitTest(int x, int y) override
    {
        auto pt = juce::Point<float>(static_cast<float>(x), static_cast<float>(y));

        // Body hit test — skin-dependent
        if (!bodyHitRect.isEmpty())
        {
            if (isGuoba())
            {
                // GUOBA: rectangle check (sprite is not elliptical)
                if (bodyHitRect.contains(pt))
                    return true;
            }
            else
            {
                // NONO: elliptical check (more accurate)
                float bcx = bodyHitRect.getCentreX();
                float bcy = bodyHitRect.getCentreY();
                float rx = bodyHitRect.getWidth() * 0.5f;
                float ry = bodyHitRect.getHeight() * 0.5f;
                if (rx > 0.0f && ry > 0.0f)
                {
                    float dx = (pt.x - bcx) / rx;
                    float dy = (pt.y - bcy) / ry;
                    if (dx * dx + dy * dy <= 1.0f)
                        return true;
                }
            }
        }

        // Lightning badge on belt buckle
        if (!badgeHitRect.isEmpty() && badgeHitRect.contains(pt))
            return true;

        // Test tube region (rectangle check is fine for this shape)
        if (!tubeHitRect.isEmpty() && tubeHitRect.contains(pt))
            return true;

        // Ear regions (clickable when in Back/Analyzing/ShowingResults for flip-back)
        for (int i = 0; i < 2; ++i)
            if (!earHitRect[i].isEmpty() && earHitRect[i].contains(pt))
                return true;

        // Results bubble area (approximate: bottom 38% when showing results)
        if (nonoState == NonoState::ShowingResults || nonoState == NonoState::ClearingData)
        {
            auto bounds = getLocalBounds().toFloat();
            auto bubbleArea = bounds.removeFromBottom(bounds.getHeight() * 0.38f);
            if (bubbleArea.contains(pt))
                return true;
        }

        return false;
    }

    //==========================================================================
    // Window drag: in standalone mode, drag Nono's body to move the window.
    // Click vs drag is distinguished by a 4px distance threshold.
    //==========================================================================
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (isOrbitLocked) return;

        // Record drag start position for all clicks
        dragStartPos = e.getScreenPosition();
        isDraggingWindow = false;

        // ===== Ear-pinch flip back =====
        // When Nono is showing back face or results, clicking an ear flips back to front
        if (nonoState == NonoState::Back || nonoState == NonoState::ShowingResults
            || nonoState == NonoState::Analyzing)
        {
            for (int i = 0; i < 2; ++i)
            {
                if (!earHitRect[i].isEmpty() && earHitRect[i].contains(e.position))
                {
                    triggerEarFlipBack();
                    return;
                }
            }
        }

        // ===== Lightning badge: toggle holo visor mode =====
        // (moved to mouseDoubleClick to avoid accidental triggers)

        // Front-face body single-click → pending smile (300ms delay to distinguish from double-click)
        // Exclude badge area to avoid triggering smile when toggling holo
        if ((nonoState == NonoState::Front || nonoState == NonoState::ShowingResults)
            && bodyHitRect.contains(e.position)
            && !badgeHitRect.contains(e.position)
            && !isEditMode && !e.mods.isRightButtonDown())
        {
            pendingSmileClick = true;
            pendingSmileClickTime = juce::Time::getMillisecondCounter();
        }

        if (nonoState == NonoState::Back && !visorHitPath.isEmpty()
            && visorHitPath.contains(e.position.x, e.position.y))
            openFileChooser();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isOrbitLocked) return;

        auto currentScreenPos = e.getScreenPosition();
        auto delta = currentScreenPos - dragStartPos;

        // 4px threshold to distinguish click from drag
        if (!isDraggingWindow && delta.getDistanceFromOrigin() > 4)
        {
            isDraggingWindow = true;
            // Cancel pending smile since this is a drag, not a click
            pendingSmileClick = false;

            if (useLocalDrag)
            {
                // Record Nono's local position at drag start
                nonoPosAtDragStart = getPosition();
            }
            else
            {
                // Record the window's position at drag start
                if (auto* topLevel = getTopLevelComponent())
                    windowPosAtDragStart = topLevel->getScreenPosition();
            }
        }

        if (isDraggingWindow)
        {
            if (useLocalDrag)
            {
                // Floating phase: move Nono within the editor canvas
                setTopLeftPosition(nonoPosAtDragStart.x + delta.x,
                                   nonoPosAtDragStart.y + delta.y);
                if (onLocalDrag)
                    onLocalDrag(delta.x, delta.y);
            }
            else
            {
                // Normal: move the entire window
                if (auto* topLevel = getTopLevelComponent())
                {
                    topLevel->setTopLeftPosition(windowPosAtDragStart.x + delta.x,
                                                 windowPosAtDragStart.y + delta.y);
                }
            }
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingWindow = false;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        // Track body hover for hover-button system
        bool nowOverBody = !bodyHitRect.isEmpty() && bodyHitRect.contains(e.position);
        if (nowOverBody && !isBodyHovered)
        {
            isBodyHovered = true;
            if (onBodyHoverEnter) onBodyHoverEnter();
        }
        else if (!nowOverBody && isBodyHovered)
        {
            isBodyHovered = false;
            if (onBodyHoverExit) onBodyHoverExit();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (isBodyHovered)
        {
            isBodyHovered = false;
            if (onBodyHoverExit) onBodyHoverExit();
        }
    }

private:
    // Window drag state
    juce::Point<int> dragStartPos;
    juce::Point<int> windowPosAtDragStart;
    juce::Point<int> nonoPosAtDragStart;  // local position at drag start (for useLocalDrag mode)
    bool isDraggingWindow = false;
    bool isBodyHovered = false;           // body hover tracking for hover buttons
    //==========================================================================
    // Dizzy: nausea accumulator state
    //==========================================================================
    juce::Point<int> lastScreenPos;
    int lastW = 0;
    int lastH = 0;
    float nauseaLevel = 0.0f;
    int dizzyRecoveryFrames = 0;
    int osQueryCounter = 0;

    //==========================================================================
    // Inner: Biquad filter for K-weighting
    //==========================================================================
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        double process(double x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() { z1 = z2 = 0; }
    };

    static void computeKWeighting(double sampleRate, Biquad& stage1, Biquad& stage2)
    {
        // Stage 1: High shelf pre-filter (ITU-R BS.1770-4)
        {
            const double Vh = 1.58489319246111; // 10^(4/20)
            const double K = std::tan(juce::MathConstants<double>::pi * 1681.974450955533 / sampleRate);
            const double Ksq = K * K;
            const double denom = 1.0 + std::sqrt(2.0) * K + Ksq;
            stage1.b0 = (Vh + std::sqrt(2.0 * Vh) * K + Ksq) / denom;
            stage1.b1 = 2.0 * (Ksq - Vh) / denom;
            stage1.b2 = (Vh - std::sqrt(2.0 * Vh) * K + Ksq) / denom;
            stage1.a1 = 2.0 * (Ksq - 1.0) / denom;
            stage1.a2 = (1.0 - std::sqrt(2.0) * K + Ksq) / denom;
        }
        // Stage 2: High-pass RLB weighting
        {
            const double Q = 0.5003270373238773;
            const double K = std::tan(juce::MathConstants<double>::pi * 38.13547087602444 / sampleRate);
            const double Ksq = K * K;
            const double denom = 1.0 + K / Q + Ksq;
            stage2.b0 = 1.0 / denom;
            stage2.b1 = -2.0 / denom;
            stage2.b2 = 1.0 / denom;
            stage2.a1 = 2.0 * (Ksq - 1.0) / denom;
            stage2.a2 = (1.0 - K / Q + Ksq) / denom;
        }
    }

    //==========================================================================
    // Inner: Analysis Thread (async, EBU R128 compliant)
    //==========================================================================
    class AnalysisThread : public juce::Thread
    {
    public:
        AnalysisThread(const juce::File& file,
                       juce::Component::SafePointer<HoloNonoComponent> owner)
            : Thread("NONO-Analysis"), audioFile(file), safeOwner(owner) {}

        void run() override
        {
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats();

            std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(audioFile));
            if (!reader)
            {
                callbackResult(NonoAnalysisResult{});
                return;
            }

            const int numCh = juce::jmin((int)reader->numChannels, 64);
            const double sr = reader->sampleRate;
            const juce::int64 maxSamples = (juce::int64)(sr * 600.0); // 10 min cap
            const juce::int64 totalSamples = juce::jmin(reader->lengthInSamples, maxSamples);

            if (totalSamples <= 0 || sr <= 0)
            {
                callbackResult(NonoAnalysisResult{});
                return;
            }

            // ---- ITU-R BS.1770-4 channel weighting ----
            // Standard layout: L R C LFE Ls Rs ...
            // LFE (ch index 3 for >= 6ch) is excluded (weight = 0)
            // Surround/height channels (index >= 4 for >= 6ch) get +1.5 dB (G_i = ~1.41)
            // For stereo (2ch) or unknown layouts: equal weight 1.0
            std::vector<double> channelWeight(numCh, 1.0);
            if (numCh >= 6)
            {
                // ch0=L, ch1=R, ch2=C: weight 1.0
                channelWeight[3] = 0.0;  // LFE — excluded per BS.1770
                for (int ch = 4; ch < numCh; ++ch)
                    channelWeight[ch] = 1.41253754;  // +1.5 dB = 10^(1.5/10)
            }

            // ---- K-weighting filters (one pair per channel) ----
            std::vector<Biquad> s1(numCh), s2(numCh);
            for (int ch = 0; ch < numCh; ++ch)
                computeKWeighting(sr, s1[ch], s2[ch]);

            // ---- 100ms sub-block accumulators ----
            const int subBlockSize = (int)(sr * 0.1);
            if (subBlockSize <= 0) { callbackResult(NonoAnalysisResult{}); return; }

            std::vector<double> subMS;              // weighted mean-square per 100ms sub-block
            std::vector<double> subBlockPower(numCh, 0.0); // running sum per channel
            int subBlockPos = 0;                     // sample position within current sub-block

            // Center channel (ch[2]) separate accumulator for dialogue loudness
            const bool hasCenter = (numCh >= 6);
            std::vector<double> centerSubMS;         // center-only mean-square per 100ms
            double centerBlockPower = 0.0;

            NonoAnalysisResult result;
            float globalMaxMag = 0.0f;

            // ==== CHUNKED READ LOOP (never loads entire file) ====
            // For large channel counts, reduce block size to limit memory
            const int blockSize = (numCh <= 8) ? 65536 : juce::jmax(4096, 65536 * 2 / numCh);
            juce::AudioBuffer<float> buffer(numCh, blockSize);
            juce::int64 samplesRead = 0;

            while (samplesRead < totalSamples)
            {
                if (threadShouldExit()) return;

                const int toRead = (int)juce::jmin((juce::int64)blockSize, totalSamples - samplesRead);
                buffer.clear();

                // Read all channels: JUCE AudioFormatReader::read with
                // destBuffer having numCh channels reads channels 0..numCh-1
                reader->read(&buffer, 0, toRead, samplesRead, true, true);
                samplesRead += toRead;

                // ---- True Peak (before K-weighting, all channels) ----
                for (int ch = 0; ch < numCh; ++ch)
                {
                    if (channelWeight[ch] > 0.0)  // skip LFE for peak too
                        globalMaxMag = juce::jmax(globalMaxMag,
                                                  buffer.getMagnitude(ch, 0, toRead));
                }

                // ---- K-weight in-place + accumulate 100ms sub-blocks ----
                for (int i = 0; i < toRead; ++i)
                {
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        if (channelWeight[ch] <= 0.0) continue;  // skip LFE
                        double x = (double)buffer.getSample(ch, i);
                        x = s1[ch].process(x);
                        x = s2[ch].process(x);
                        subBlockPower[ch] += x * x;

                        // Accumulate center channel separately
                        if (hasCenter && ch == 2)
                            centerBlockPower += x * x;
                    }

                    ++subBlockPos;
                    if (subBlockPos >= subBlockSize)
                    {
                        double ms = 0.0;
                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            ms += channelWeight[ch] * subBlockPower[ch] / subBlockSize;
                            subBlockPower[ch] = 0.0;
                        }
                        subMS.push_back(ms);

                        // Center channel sub-block (weight 1.0, single channel)
                        if (hasCenter)
                        {
                            centerSubMS.push_back(centerBlockPower / subBlockSize);
                            centerBlockPower = 0.0;
                        }

                        subBlockPos = 0;
                    }
                }
            }

            reader.reset(); // release file handle immediately

            // ---- Peak dBFS ----
            result.peakDBFS = juce::Decibels::gainToDecibels(globalMaxMag, -100.0f);
            result.numChannels = numCh;

            if (threadShouldExit()) return;

            // ---- LUFS calculations from sub-block data ----
            const int numSubBlocks = (int)subMS.size();

            auto toLUFS = [](double power) -> float {
                return (power > 0.0) ? (float)(-0.691 + 10.0 * std::log10(power)) : -100.0f;
            };

            // Momentary max (400ms = 4 sub-blocks)
            {
                const int win = 4;
                float maxVal = -100.0f;
                for (int i = 0; i <= numSubBlocks - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += subMS[i + j];
                    maxVal = juce::jmax(maxVal, toLUFS(sum / win));
                }
                result.momentaryMaxLUFS = maxVal;
            }

            // Short-term max (3000ms = 30 sub-blocks)
            {
                const int win = 30;
                float maxVal = -100.0f;
                for (int i = 0; i <= numSubBlocks - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += subMS[i + j];
                    maxVal = juce::jmax(maxVal, toLUFS(sum / win));
                }
                result.shortTermMaxLUFS = maxVal;
            }

            // Integrated LUFS (EBU R128 dual gating)
            {
                const int win = 4;
                std::vector<double> winPower;
                std::vector<float> winLUFS;

                for (int i = 0; i <= numSubBlocks - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += subMS[i + j];
                    double pwr = sum / win;
                    winPower.push_back(pwr);
                    winLUFS.push_back(toLUFS(pwr));
                }

                // Absolute gate (-70 LUFS)
                double absSum = 0;
                int absCnt = 0;
                for (size_t i = 0; i < winPower.size(); ++i)
                {
                    if (winLUFS[i] > -70.0f)
                    {
                        absSum += winPower[i];
                        absCnt++;
                    }
                }

                if (absCnt > 0)
                {
                    float relThresh = toLUFS(absSum / absCnt) - 10.0f;

                    double relSum = 0;
                    int relCnt = 0;
                    for (size_t i = 0; i < winPower.size(); ++i)
                    {
                        if (winLUFS[i] > -70.0f && winLUFS[i] > relThresh)
                        {
                            relSum += winPower[i];
                            relCnt++;
                        }
                    }
                    result.integratedLUFS = (relCnt > 0) ? toLUFS(relSum / relCnt) : -100.0f;
                }
                else
                {
                    result.integratedLUFS = -100.0f;
                }
            }

            // Center channel integrated LUFS (EBU R128 dual gating, ch[2] only)
            if (hasCenter && !centerSubMS.empty())
            {
                const int win = 4;
                const int numCenterSubs = (int)centerSubMS.size();
                std::vector<double> cWinPower;
                std::vector<float> cWinLUFS;

                for (int i = 0; i <= numCenterSubs - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += centerSubMS[i + j];
                    double pwr = sum / win;
                    cWinPower.push_back(pwr);
                    cWinLUFS.push_back(toLUFS(pwr));
                }

                // Absolute gate (-70 LUFS)
                double absSum = 0;
                int absCnt = 0;
                for (size_t i = 0; i < cWinPower.size(); ++i)
                {
                    if (cWinLUFS[i] > -70.0f)
                    {
                        absSum += cWinPower[i];
                        absCnt++;
                    }
                }

                if (absCnt > 0)
                {
                    float relThresh = toLUFS(absSum / absCnt) - 10.0f;
                    double relSum = 0;
                    int relCnt = 0;
                    for (size_t i = 0; i < cWinPower.size(); ++i)
                    {
                        if (cWinLUFS[i] > -70.0f && cWinLUFS[i] > relThresh)
                        {
                            relSum += cWinPower[i];
                            relCnt++;
                        }
                    }
                    result.centerLUFS = (relCnt > 0) ? toLUFS(relSum / relCnt) : -100.0f;
                }
            }

            callbackResult(result);
        }

    private:
        juce::File audioFile;
        juce::Component::SafePointer<HoloNonoComponent> safeOwner;

        void callbackResult(const NonoAnalysisResult& r)
        {
            auto safe = safeOwner;
            auto result = r;
            juce::MessageManager::callAsync([safe, result]()
            {
                if (auto* comp = safe.getComponent())
                    comp->onAnalysisComplete(result);
            });
        }
    };

    //==========================================================================
    // Enums & State
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    enum class NonoState { Front, Flipping, Back, Analyzing, ShowingResults, ClearingData };
    NonoState nonoState = NonoState::Front;
    NonoState flipDestination = NonoState::Back;

    enum class FrontAnim { Idle, FlashEyes, CollisionHit };
    FrontAnim frontAnim = FrontAnim::Idle;
    int frontAnimTimer = 0;

    enum class CollisionDir { None, Up, Left };
    CollisionDir collisionDir = CollisionDir::None;

    // Flip animation
    float flipProgress = 0.0f;  // 0 = front, 1 = back
    float flipTarget = 0.0f;

    // Idle figure-8
    float idlePhase = 0.0f;
    float idleOffsetX = 0.0f;
    float idleOffsetY = 0.0f;

    // Eye animation
    float eyeOpenness = 0.4f;
    float targetEyeOpenness = 0.4f;
    float eyeGlow = 0.7f;
    float targetEyeGlow = 0.7f;

    // Pupil mouse-tracking state
    float pupilOffsetLX = 0.0f, pupilOffsetLY = 0.0f;  // current smoothed offset (left)
    float pupilOffsetRX = 0.0f, pupilOffsetRY = 0.0f;  // current smoothed offset (right)
    juce::Point<int> lastMousePos { -1, -1 };

    // Recording glitch grid waveform state
    struct GridColumn { float level; uint32_t randomSeed; };
    std::vector<GridColumn> gridWaveformHistory;
    bool wasRecording = false;   // edge detection for start/stop transitions
    float prevGridLevel = 0.0f;  // previous frame level for first-order difference (HF bias)

    // Test tube
    float tubeAngle = 0.0f;
    float tubeLiquidWave = 0.0f;
    float audioLevel = 0.0f;
    float liquidHeight = 1.0f;       // 0 = empty, 1 = full
    float clearPourAngle = 0.0f;     // current pour rotation (radians)
    float targetPourAngle = 0.0f;    // target pour rotation
    float bubbleFadeAlpha = 1.0f;    // results bubble fade
    float neonBreathPhase = 0.0f;    // back-face neon breathing
    float earBobOffset = 0.0f;       // ear floating Y offset

    // Auto-refill state machine (3s cooldown → smooth fill)
    juce::uint32 emptyTimestamp = 0;
    bool isWaitingToRefill = false;
    bool isRefilling = false;

    // Overload explosion state (peak >= 0 dBFS)
    bool isExploded = false;
    float explosionProgress = 0.0f;   // 0→1 animation progress
    struct ExpShard {
        float x, y;          // offset from tube center
        float angle;         // rotation
        float vx, vy;        // velocity
        float spin;          // angular velocity
        float sz;            // size
    };
    ExpShard shards[6];
    struct ExpDrop {
        float x, y;
        float vx, vy;
        float sz;
        float gravity;
    };
    ExpDrop drops[10];

    // Collision
    float collisionOffsetX = 0.0f;
    float collisionOffsetY = 0.0f;
    float targetCollisionX = 0.0f;
    float targetCollisionY = 0.0f;

    // Particles
    bool showParticles = false;
    float particleProgress = 0.0f;
    float particleOriginX = 0.0f;
    float particleOriginY = 0.0f;

    // Analysis state
    std::unique_ptr<AnalysisThread> analysisThread;
    NonoAnalysisResult analysisResult;
    bool hasResults = false;
    float ripplePhase = 0.0f;
    bool isDragHovering = false;
    juce::File pendingAnalysisFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Smile + orbit animation state
    bool isSmiling = false;
    bool isShy = false;              // shy expression: >< eyes after ear-pinch
    int shyFramesLeft = 0;           // shy duration countdown (1.5s = 90 frames at 60Hz)
    bool isRewinding = false;        // time-rewind holographic face animation
    int rewindFramesLeft = 0;        // rewind duration countdown (1.5s = 90 frames at 60Hz)
    double rewindStartMs = 0.0;      // timestamp when rewind started
    std::vector<GridColumn> rewindSnapshot;  // snapshot of gridWaveformHistory for reverse playback
    bool isExtractingVideo = false;  // amber extraction face animation
    int extractFrameCounter = 0;     // frame counter for extraction animation scrolling
    std::vector<GridColumn> extractSyntheticHistory;  // synthetic waveform for amber grid
    bool isOrbiting = false;
    bool isOrbitLocked = false;       // all mouse interactions disabled during orbit
    float orbitProgress = 0.0f;       // 0→1 during orbit (1.2s)
    int smileFramesLeft = 0;          // smile duration countdown (2s = 120 frames)
    int orbitLockFramesLeft = 0;      // interaction lock countdown (1.5s = 90 frames)
    bool pendingSmileClick = false;   // delayed single-click detection
    juce::uint32 pendingSmileClickTime = 0;

    // Hit test regions (computed in paint, used in mouse handlers)
    juce::Rectangle<float> bodyHitRect;
    juce::Rectangle<float> tubeHitRect;
    juce::Rectangle<float> earHitRect[2];  // [0] = left ear, [1] = right ear
    juce::Rectangle<float> badgeHitRect;   // lightning badge on belt buckle
    juce::Path visorHitPath;               // visor shape for back-face click detection

    // Holo visor mode (toggled by lightning badge click)
    bool isHoloVisor = false;              // true = yellow visor + gray-white fill + blue bean eyes
    float holoTransition = 0.0f;           // smooth 0→1 transition

    // Lightning VFX state
    bool showLightningVFX = false;
    float lightningProgress = 0.0f;        // 0→1 over ~0.5s
    struct LightningBolt {
        float x1, y1, x2, y2;             // start/end points
        float midOffsetX;                   // zigzag offset
        float brightness;
    };
    LightningBolt lightningBolts[5];

    // Edit mode transition (0=normal, 1=editMode → color/bubble change)
    float editModeTransition = 0.0f;

    // Test tube bubbles
    struct TubeBubble
    {
        float xOff = 0.0f;   // horizontal offset within tube (-0.3..0.3)
        float phase = 0.0f;  // vertical phase (0..1, wraps)
        float sz = 2.0f;     // bubble radius
        float spd = 0.01f;   // rise speed per frame
    };
    TubeBubble tubeBubbles[8];

    // Colors — skin-dependent accent via accentCol()
    static inline const juce::Colour guobaGold    = juce::Colour(0xFFF9E353);  // Guoba: yellow/gold frame
    static inline const juce::Colour nonoBlue     = juce::Colour(0xFF00AAFF);  // Nono: electric blue
    static inline const juce::Colour sunsetOrange = juce::Colour(0xFFFF7B3A);  // Guoba holo-mode eye color
    static inline const juce::Colour bodyEdge     = juce::Colour(0xFFD0D0D8);
    static inline const juce::Colour screenDark   = juce::Colour(0xFF0E0E1E);
    static inline const juce::Colour magicPink    = juce::Colour(0xFFFF2A7F);
    static inline const juce::Colour neonGreen    = juce::Colour(0xFF39FF14);

    /** Skin-aware accent color: gold for Guoba, blue for Nono */
    juce::Colour accentCol() const { return isGuoba() ? guobaGold : nonoBlue; }

    // GUOBA sprite image
    juce::Image guobaSprite;
    juce::Image guobaNose;

    // Skin selector dropdown
    juce::ComboBox skinMenu;

    // Visor mask fade animation (0=hidden, 1=fully visible)
    float visorAlpha = 1.0f;
    float visorAlphaTarget = 1.0f;

    //==========================================================================
    // Timer
    //==========================================================================
    void timerCallback() override
    {
        // 60Hz → 30Hz smart throttle during mouse drag
        if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        {
            static int dragThrottleCounter = 0;
            if (++dragThrottleCounter % 2 != 0) return;
        }

        // ==========================================
        // 慢速轨道 (10Hz)：每6帧查一次岗，避开OS跨进程IPC锁竞争
        // (Desktop only — no window movement tracking on iOS)
        // ==========================================
#if ! JUCE_IOS
        osQueryCounter++;
        if (osQueryCounter >= 6)
        {
            osQueryCounter = 0;
            auto currentPos = getScreenPosition();
            int currentW = getWidth();
            int currentH = getHeight();

            float deltaX = static_cast<float>(currentPos.x - lastScreenPos.x);
            float deltaY = static_cast<float>(currentPos.y - lastScreenPos.y);
            float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            float deltaW = static_cast<float>(std::abs(currentW - lastW));
            float deltaH = static_cast<float>(std::abs(currentH - lastH));

            float violence = distance + deltaW + deltaH;
            if (violence > 30.0f)
                nauseaLevel += 18.0f;

            lastScreenPos = currentPos;
            lastW = currentW;
            lastH = currentH;
        }
#endif // ! JUCE_IOS

        // ==========================================
        // 高速轨道 (60Hz)：自然衰减 + 状态机
        // ==========================================
        nauseaLevel -= 0.5f;
        nauseaLevel = juce::jlimit(0.0f, 100.0f, nauseaLevel);

        if (nauseaLevel >= 90.0f)
        {
            isDizzy = true;
            dizzyRecoveryFrames = 30;
        }

        if (isDizzy)
        {
            dizzyRecoveryFrames--;
            if (dizzyRecoveryFrames <= 0 && nauseaLevel < 10.0f)
                isDizzy = false;
        }

        // Audio level
        float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
        float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
        float peak = juce::jmax(peakL, peakR);
        float rawLevel = juce::jmap(juce::jlimit(-60.0f, 0.0f, peak), -60.0f, 0.0f, 0.0f, 1.0f);
        audioLevel += (rawLevel - audioLevel) * 0.2f;

        // Recording grid waveform: push HF-biased level into scrolling history
        // First-order difference acts as a high-pass filter: responds to transients,
        // ignores sustained low-frequency content.
        {
            bool nowRecording = audioProcessor.audioRecorder.getIsRecording();
            if (nowRecording)
            {
                float hfEnergy = std::abs(audioLevel - prevGridLevel);
                prevGridLevel = audioLevel;
                // Display gain: diff signal is tiny, amplify aggressively
                float displayGain = 22.0f;
                float visualLevel = hfEnergy * displayGain;
                // Soft clipping: sqrt compresses dynamic range —
                // quiet transients still produce visible bars, loud ones don't saturate
                float scaledHF = std::sqrt(juce::jlimit(0.0f, 1.0f, visualLevel));
                GridColumn col;
                col.level = scaledHF;
                col.randomSeed = static_cast<uint32_t>(juce::Random::getSystemRandom().nextInt());
                gridWaveformHistory.push_back(col);
                // Cap at 200 entries (oldest at front, newest at back)
                if (gridWaveformHistory.size() > 200)
                    gridWaveformHistory.erase(gridWaveformHistory.begin());
            }
            else if (wasRecording)
            {
                gridWaveformHistory.clear();
                prevGridLevel = 0.0f;
            }
            wasRecording = nowRecording;
        }

        targetEyeOpenness = 0.3f + audioLevel * 0.7f;

        // Idle figure-8: Nono floats; Guoba stands still (sprite doesn't suit floating)
        if (isGuoba())
        {
            idleOffsetX = 0.0f;
            idleOffsetY = 0.0f;
        }
        else
        {
            idlePhase += 0.015f;
            if (idlePhase > 628.0f) idlePhase -= 628.0f;
            idleOffsetX = 1.5f * std::sin(2.0f * idlePhase);
            idleOffsetY = 2.5f * std::sin(idlePhase);
        }

        // Visor fade animation (~0.3s transition)
        visorAlpha += (visorAlphaTarget - visorAlpha) * 0.12f;
        if (std::abs(visorAlpha - visorAlphaTarget) < 0.01f)
            visorAlpha = visorAlphaTarget;

        // Holo visor transition (smooth 0→1 / 1→0, ~0.25s)
        {
            float holoTarget = isHoloVisor ? 1.0f : 0.0f;
            holoTransition += (holoTarget - holoTransition) * 0.14f;
            if (std::abs(holoTransition - holoTarget) < 0.005f)
                holoTransition = holoTarget;
        }

        // Lightning VFX progress (~0.5s = 30 frames)
        if (showLightningVFX)
        {
            lightningProgress += 1.0f / 30.0f;
            if (lightningProgress >= 1.0f)
            {
                showLightningVFX = false;
                lightningProgress = 0.0f;
            }
        }

        // Front face animations
        advanceFrontAnim();

        // Smooth lerps
        eyeOpenness += (targetEyeOpenness - eyeOpenness) * 0.15f;
        eyeGlow += (targetEyeGlow - eyeGlow) * 0.2f;

        // Pupil mouse-tracking: compute target offsets per eye, then smooth
        {
            auto mousePos = getMouseXYRelative();
            bool mouseChanged = (mousePos != lastMousePos);
            lastMousePos = mousePos;

            // Recompute eye centers (mirrors paint() coordinate math)
            auto bounds = getLocalBounds().toFloat();
            float unit = juce::jmin(bounds.getWidth(), bounds.getHeight());
            float r = unit * 0.18f;
            float bodyCX = bounds.getCentreX() - r * 0.6f + idleOffsetX + collisionOffsetX;
            float bodyCY = bounds.getCentreY() - r * 0.3f + idleOffsetY + collisionOffsetY;
            float eyeVCY = bodyCY - r * 0.1f;
            float eyeSpacing = r * 0.30f;

            float mx = static_cast<float>(mousePos.x);
            float my = static_cast<float>(mousePos.y);

            // Pupil geometry: max offset = eye half-width, sensitivity maps distance
            float eyeW = r * 0.13f;
            float maxOff = eyeW * 0.38f;  // stay well inside the eye shape
            float sensitivity = 0.012f;

            for (int idx = 0; idx < 2; ++idx)
            {
                float ecx = (idx == 0) ? (bodyCX - eyeSpacing) : (bodyCX + eyeSpacing);
                float ecy = eyeVCY;
                float dx = mx - ecx;
                float dy = my - ecy;
                float dist = std::hypot(dx, dy);
                float angle = std::atan2(dy, dx);
                float offset = juce::jmin(maxOff, dist * sensitivity);
                float targetX = std::cos(angle) * offset;
                float targetY = std::sin(angle) * offset;

                // Smooth lerp (0.18 = snappy but not jarring)
                if (idx == 0)
                {
                    pupilOffsetLX += (targetX - pupilOffsetLX) * 0.18f;
                    pupilOffsetLY += (targetY - pupilOffsetLY) * 0.18f;
                }
                else
                {
                    pupilOffsetRX += (targetX - pupilOffsetRX) * 0.18f;
                    pupilOffsetRY += (targetY - pupilOffsetRY) * 0.18f;
                }
            }

            if (mouseChanged)
                repaint();
        }

        float collLerp = (frontAnim == FrontAnim::CollisionHit) ? 0.4f : 0.25f;
        collisionOffsetX += (targetCollisionX - collisionOffsetX) * collLerp;
        collisionOffsetY += (targetCollisionY - collisionOffsetY) * collLerp;

        // Flip animation
        if (std::abs(flipTarget - flipProgress) > 0.01f)
        {
            flipProgress += (flipTarget - flipProgress) * 0.18f;
        }
        else if (nonoState == NonoState::Flipping)
        {
            flipProgress = flipTarget;

            if (flipDestination == NonoState::Analyzing)
            {
                nonoState = NonoState::Analyzing;
                if (pendingAnalysisFile.existsAsFile())
                    startAnalysis(pendingAnalysisFile);
            }
            else
            {
                nonoState = flipDestination;
            }
        }

        // Tube shake (dual-sine)
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        if (audioLevel > 0.03f)
        {
            float intensity = juce::jlimit(0.0f, 1.0f, audioLevel * 1.8f);
            float maxAngle = juce::degreesToRadians(18.0f);
            float shake = (std::sin(ms * 0.015f) * 0.7f + std::sin(ms * 0.004f) * 0.3f);
            tubeAngle += (intensity * maxAngle * shake - tubeAngle) * 0.25f;
            tubeLiquidWave = intensity * 3.0f *
                             (std::sin(ms * 0.012f) * 0.6f + std::sin(ms * 0.005f) * 0.4f);
        }
        else
        {
            tubeAngle *= 0.92f;
            tubeLiquidWave *= 0.92f;
        }

        // Ripple phase for analysis animation
        if (nonoState == NonoState::Analyzing)
            ripplePhase += 0.02f;

        // Neon breathing phase (always ticks for back face)
        neonBreathPhase += 0.03f;
        if (neonBreathPhase > juce::MathConstants<float>::twoPi * 100.0f)
            neonBreathPhase -= juce::MathConstants<float>::twoPi * 100.0f;

        // Ear independent bobbing (low-freq sine)
        earBobOffset = std::sin(ms * 0.003f) * 3.0f;

        // Edit mode transition (smooth 0→1 / 1→0)
        {
            float editTarget = isEditMode ? 1.0f : 0.0f;
            editModeTransition += (editTarget - editModeTransition) * 0.08f;
            if (std::abs(editModeTransition - editTarget) < 0.005f)
                editModeTransition = editTarget;
        }

        // Bubble phase cycling (always tick, visible when editModeTransition > 0)
        if (editModeTransition > 0.01f)
        {
            for (auto& b : tubeBubbles)
            {
                b.phase += b.spd * (0.5f + editModeTransition * 0.5f);
                if (b.phase > 1.0f)
                    b.phase -= 1.0f;
            }
        }

        // ClearingData animation: fade bubble → pour tube → drain liquid → restore → Front
        if (nonoState == NonoState::ClearingData)
        {
            // Phase 1: fade out bubble
            bubbleFadeAlpha = juce::jmax(0.0f, bubbleFadeAlpha - 0.08f);

            // Phase 2: tilt tube to pour position
            clearPourAngle += (targetPourAngle - clearPourAngle) * 0.15f;

            // Phase 3: drain liquid once tilted past 60°
            if (clearPourAngle > juce::degreesToRadians(60.0f))
                liquidHeight = juce::jmax(0.0f, liquidHeight - 0.04f);

            // Phase 4: once empty, restore tube upright → go to Front
            if (liquidHeight <= 0.01f)
            {
                targetPourAngle = 0.0f;
                clearPourAngle += (0.0f - clearPourAngle) * 0.12f;

                if (std::abs(clearPourAngle) < 0.02f)
                {
                    clearPourAngle = 0.0f;
                    liquidHeight = 0.0f;
                    hasResults = false;
                    nonoState = NonoState::Front;
                    flipProgress = 0.0f;
                    flipTarget = 0.0f;

                    // Trigger 3-second refill cooldown
                    emptyTimestamp = juce::Time::getMillisecondCounter();
                    isWaitingToRefill = true;
                    isRefilling = false;
                }
            }
        }

        // Auto-refill state machine: 3s cooldown → smooth fill
        if (isWaitingToRefill)
        {
            if (juce::Time::getMillisecondCounter() - emptyTimestamp >= 3000)
            {
                isWaitingToRefill = false;
                isExploded = false;     // 碎玻璃瞬间复原！
                isRefilling = true;
                liquidHeight = 0.0f;    // 确保从空开始注水
            }
        }

        if (isRefilling)
        {
            liquidHeight += 0.04f;
            if (liquidHeight >= 1.0f)
            {
                liquidHeight = 1.0f;
                isRefilling = false;
            }
        }

        // Explosion physics: animate shards + drops
        if (isExploded && explosionProgress < 1.0f)
        {
            explosionProgress += 0.012f; // ~1.2s to full fade at 60Hz
            for (auto& s : shards)
            {
                s.x += s.vx;
                s.y += s.vy;
                s.vy += 0.18f; // gravity
                s.vx *= 0.985f; // air drag
                s.angle += s.spin;
            }
            for (auto& d : drops)
            {
                d.x += d.vx;
                d.y += d.vy;
                d.vy += d.gravity; // per-drop gravity
                d.vx *= 0.98f;
            }
        }

        // Particles
        if (showParticles)
        {
            particleProgress += 0.06f;
            if (particleProgress >= 1.0f)
                showParticles = false;
        }

        // Smile + orbit: delayed single-click detection (300ms to distinguish from double-click)
        if (pendingSmileClick && juce::Time::getMillisecondCounter() - pendingSmileClickTime > 300)
        {
            pendingSmileClick = false;
            triggerSmileOrbit();
        }

        // Orbit animation progress (1.2s = 72 frames at 60Hz)
        if (isOrbiting)
        {
            orbitProgress += 1.0f / 72.0f;
            if (orbitProgress >= 1.0f)
            {
                orbitProgress = 0.0f;
                isOrbiting = false;
            }
        }

        // Smile countdown (2s = 120 frames)
        if (smileFramesLeft > 0)
        {
            smileFramesLeft--;
            if (smileFramesLeft <= 0)
                isSmiling = false;
        }

        // Shy countdown (1.5s = 90 frames)
        if (shyFramesLeft > 0)
        {
            shyFramesLeft--;
            if (shyFramesLeft <= 0)
                isShy = false;
        }

        // Rewind countdown (1.5s = 90 frames)
        if (rewindFramesLeft > 0)
        {
            rewindFramesLeft--;
            if (rewindFramesLeft <= 0)
            {
                isRewinding = false;
                rewindSnapshot.clear();
            }
        }

        // Extraction animation: advance frame counter (continuous scrolling)
        if (isExtractingVideo)
        {
            extractFrameCounter++;
            // Continuously inject new synthetic columns to keep the grid alive
            if (extractFrameCounter % 2 == 0 && !extractSyntheticHistory.empty())
            {
                juce::Random rng;
                GridColumn col;
                col.level = 0.05f + rng.nextFloat() * 0.55f;
                col.randomSeed = static_cast<uint32_t>(rng.nextInt());
                extractSyntheticHistory.push_back(col);
                if (extractSyntheticHistory.size() > 300)
                    extractSyntheticHistory.erase(extractSyntheticHistory.begin());
            }
        }

        // Orbit lock countdown (1.5s = 90 frames from orbit start)
        if (orbitLockFramesLeft > 0)
        {
            orbitLockFramesLeft--;
            if (orbitLockFramesLeft <= 0)
                isOrbitLocked = false;
        }

        repaint();
    }

    //==========================================================================
    // Front-face animation state machine
    //==========================================================================
    void advanceFrontAnim()
    {
        if (frontAnim == FrontAnim::Idle) return;
        frontAnimTimer++;

        switch (frontAnim)
        {
            case FrontAnim::FlashEyes:
            {
                if (frontAnimTimer <= 7)
                    targetEyeGlow = 1.0f;
                else if (frontAnimTimer <= 19)
                    targetEyeGlow = 1.0f - (static_cast<float>(frontAnimTimer - 7) / 12.0f) * 0.3f;
                else
                {
                    targetEyeGlow = 0.7f;
                    frontAnim = FrontAnim::Idle;
                }
                break;
            }
            case FrontAnim::CollisionHit:
            {
                const float maxDash = 45.0f;
                if (frontAnimTimer <= 3)
                {
                    float t = static_cast<float>(frontAnimTimer) / 3.0f;
                    if (collisionDir == CollisionDir::Up) targetCollisionY = -maxDash * t;
                    else targetCollisionX = -maxDash * t;
                }
                else if (frontAnimTimer <= 5)
                {
                    if (frontAnimTimer == 4)
                    {
                        showParticles = true;
                        particleProgress = 0.0f;
                        auto b = getLocalBounds().toFloat();
                        if (collisionDir == CollisionDir::Up)
                            { particleOriginX = b.getCentreX(); particleOriginY = b.getCentreY() + collisionOffsetY; }
                        else
                            { particleOriginX = b.getCentreX() + collisionOffsetX; particleOriginY = b.getCentreY(); }
                    }
                }
                else if (frontAnimTimer <= 11)
                {
                    targetCollisionX = targetCollisionY = 0.0f;
                }
                else
                {
                    targetCollisionX = targetCollisionY = 0.0f;
                    if (!showParticles) frontAnim = FrontAnim::Idle;
                }
                break;
            }
            default: break;
        }
    }

    void triggerFrontAnim(FrontAnim a)
    {
        if (frontAnim == FrontAnim::CollisionHit) return;
        frontAnim = a;
        frontAnimTimer = 0;
    }

    void triggerCollision(CollisionDir dir)
    {
        frontAnim = FrontAnim::CollisionHit;
        collisionDir = dir;
        frontAnimTimer = 0;
        targetCollisionX = targetCollisionY = 0.0f;
    }

    void triggerSmileOrbit()
    {
        if (isOrbiting || isOrbitLocked) return;
        isSmiling = true;
        isOrbiting = true;
        isOrbitLocked = true;
        orbitProgress = 0.0f;
        smileFramesLeft = 120;       // 2 seconds at 60Hz
        orbitLockFramesLeft = 90;    // 1.5 seconds at 60Hz

        if (onSmileOrbitTriggered)
            onSmileOrbitTriggered();
    }

    void triggerEarFlipBack()
    {
        // Stop any running analysis
        if (analysisThread)
        {
            analysisThread->signalThreadShouldExit();
            analysisThread->notify();
            analysisThread->stopThread(2000);
            analysisThread.reset();
        }

        // Activate shy expression
        isShy = true;
        shyFramesLeft = 90;  // 1.5 seconds at 60Hz

        // Flip back to front
        flipTarget = 0.0f;
        nonoState = NonoState::Flipping;
        flipDestination = NonoState::Front;

        // Reset analysis visuals
        hasResults = false;
        ripplePhase = 0.0f;
        bubbleFadeAlpha = 1.0f;

        // Notify editor
        if (onEarFlipBack)
            onEarFlipBack();
    }

    void triggerLightningToggle()
    {
        isHoloVisor = !isHoloVisor;

        // Fire lightning VFX
        showLightningVFX = true;
        lightningProgress = 0.0f;

        // Generate random bolt paths radiating from badge center
        auto& rng = juce::Random::getSystemRandom();
        float bcx = badgeHitRect.getCentreX();
        float bcy = badgeHitRect.getCentreY();
        for (auto& bolt : lightningBolts)
        {
            float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            float len = 30.0f + rng.nextFloat() * 50.0f;
            bolt.x1 = bcx;
            bolt.y1 = bcy;
            bolt.x2 = bcx + std::cos(angle) * len;
            bolt.y2 = bcy + std::sin(angle) * len;
            bolt.midOffsetX = (rng.nextFloat() - 0.5f) * 20.0f;
            bolt.brightness = 0.6f + rng.nextFloat() * 0.4f;
        }
    }

    //==========================================================================
    // Analysis control
    //==========================================================================
    void openFileChooser()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Audio File", juce::File{},
            "*.wav;*.mp3;*.aiff;*.aif;*.flac;*.ogg");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.existsAsFile())
                    startAnalysis(result);
            });
    }

    void startAnalysis(const juce::File& file)
    {
        if (analysisThread)
        {
            analysisThread->signalThreadShouldExit();
            analysisThread->notify();
            analysisThread->stopThread(3000);
            analysisThread.reset();
        }

        nonoState = NonoState::Analyzing;
        flipTarget = 1.0f;
        flipProgress = 1.0f;
        hasResults = false;
        ripplePhase = 0.0f;
        liquidHeight = 1.0f;       // refill tube
        bubbleFadeAlpha = 1.0f;    // reset bubble opacity

        auto safeThis = juce::Component::SafePointer<HoloNonoComponent>(this);
        analysisThread = std::make_unique<AnalysisThread>(file, safeThis);
        analysisThread->startThread();
    }

    void onAnalysisComplete(const NonoAnalysisResult& result)
    {
        analysisResult = result;
        hasResults = true;

        // Overload detection: peak >= 0 dBFS → EXPLODE!
        if (result.peakDBFS >= 0.0f)
        {
            isExploded = true;
            explosionProgress = 0.0f;
            liquidHeight = 0.0f;

            // Initialize shards with random velocities
            auto& rng = juce::Random::getSystemRandom();
            for (auto& s : shards)
            {
                s.x = 0.0f;
                s.y = 0.0f;
                s.angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
                s.vx = (rng.nextFloat() - 0.5f) * 8.0f;
                s.vy = -rng.nextFloat() * 6.0f - 2.0f;
                s.spin = (rng.nextFloat() - 0.5f) * 0.4f;
                s.sz = 4.0f + rng.nextFloat() * 8.0f;
            }
            for (auto& d : drops)
            {
                d.x = 0.0f;
                d.y = 0.0f;
                d.vx = (rng.nextFloat() - 0.5f) * 10.0f;
                d.vy = -rng.nextFloat() * 8.0f - 3.0f;
                d.sz = 2.0f + rng.nextFloat() * 6.0f;
                d.gravity = 0.25f + rng.nextFloat() * 0.15f;
            }

            // 3-second cooldown → auto-restore
            emptyTimestamp = juce::Time::getMillisecondCounter();
            isWaitingToRefill = true;
            isRefilling = false;
        }

        // Flip back to front, show results
        flipTarget = 0.0f;
        nonoState = NonoState::Flipping;
        flipDestination = NonoState::ShowingResults;
    }

    //==========================================================================
    // Drawing: Common
    //==========================================================================
    void drawAntiGravityGlow(juce::Graphics& g, float cx, float cy, float r)
    {
        const float glowY = cy + r + r * 0.08f;
        juce::ColourGradient grad(
            accentCol().withAlpha(0.18f), cx, glowY,
            accentCol().withAlpha(0.0f), cx, glowY + r * 0.7f, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - r * 0.8f, glowY, r * 1.6f, r * 0.65f);
    }

    void drawShadow(juce::Graphics& g, float cx, float cy, float r)
    {
        g.setColour(juce::Colour(0x18000000));
        g.fillEllipse(cx - r * 0.8f, cy + r + r * 0.15f, r * 1.6f, r * 0.24f);
    }

    void drawBody(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (isGuoba())
        {
            // === GUOBA: sprite image ===
            if (hScale < 0.1f || guobaSprite.isNull()) return;

            float spriteH = r * 4.0f;
            float spriteW = spriteH;
            float anchorY = cy - r * 0.1f;
            float spriteY = anchorY - spriteH * 0.38f;
            float sw = spriteW * hScale;
            g.setOpacity(1.0f);
            g.drawImage(guobaSprite,
                cx - sw * 0.5f, spriteY, sw, spriteH,
                0, 0, guobaSprite.getWidth(), guobaSprite.getHeight());
        }
        else
        {
            // === NONO: elliptical gradient body ===
            float rx = r * hScale;

            juce::ColourGradient bodyGrad(
                juce::Colour(0xFFFFFFFF), cx - rx * 0.35f, cy - r * 0.4f,
                juce::Colour(0xFFC0C4CE), cx + rx * 0.55f, cy + r * 0.6f, true);
            bodyGrad.addColour(0.45, juce::Colour(0xFFF0F1F5));
            bodyGrad.addColour(0.8,  juce::Colour(0xFFD5D8E0));
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx - rx, cy - r, rx * 2.0f, r * 2.0f);

            g.setColour(juce::Colour(0xFFB8BCC8));
            g.drawEllipse(cx - rx, cy - r, rx * 2.0f, r * 2.0f, 1.5f);

            if (hScale > 0.5f)
            {
                juce::ColourGradient specGrad(
                    juce::Colours::white.withAlpha(0.65f), cx - rx * 0.3f, cy - r * 0.4f,
                    juce::Colours::white.withAlpha(0.0f),  cx + rx * 0.05f, cy - r * 0.05f, true);
                g.setGradientFill(specGrad);
                g.fillEllipse(cx - rx * 0.6f, cy - r * 0.7f, rx * 0.7f, r * 0.5f);
            }

            if (hScale > 0.5f)
            {
                juce::ColourGradient rimGrad(
                    juce::Colours::white.withAlpha(0.0f), cx, cy,
                    juce::Colour(0xFFDDE0EA).withAlpha(0.35f), cx + rx * 0.7f, cy + r * 0.6f, true);
                g.setGradientFill(rimGrad);
                g.fillEllipse(cx + rx * 0.15f, cy + r * 0.2f, rx * 0.7f, r * 0.55f);
            }
        }
    }

    //==========================================================================
    // Drawing: Holographic Ears (Seer-style solid base + energy blade)
    //==========================================================================
    void drawHolographicEars(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (isGuoba())
            return;  // GUOBA: ears are part of the sprite image

        // === NONO: Seer-style solid base capsule + energy blade ===
        if (hScale < 0.3f) return;

        const juce::Colour baseCol(0xFFE0E5EC);
        const juce::Colour bladeCol(0xFF00E5FF);
        const float tiltAngle = juce::degreesToRadians(15.0f);
        const float sphereTop = cy - r;

        struct EarSpec { float xOff, yOff, scale, bob; };
        EarSpec specs[2] = {
            { -0.72f, -0.18f, 0.95f, earBobOffset },
            {  0.72f, -0.18f, 0.95f, earBobOffset }
        };

        for (int i = 0; i < 2; ++i)
        {
            int side = (i == 0) ? -1 : 1;
            float sc = specs[i].scale;

            float baseW = r * 0.22f * hScale * sc;
            float baseH = r * 0.32f * sc;
            float bladeW = r * 0.13f * hScale * sc;
            float bladeH = r * 0.6f * sc;

            float anchorX = cx + r * specs[i].xOff * hScale;
            float anchorY = sphereTop + r * specs[i].yOff + specs[i].bob;
            float angle = tiltAngle * static_cast<float>(side);

            auto xform = juce::AffineTransform::rotation(angle, anchorX, anchorY);

            // Solid base (capsule)
            juce::Path basePath;
            basePath.addRoundedRectangle(
                anchorX - baseW / 2.0f, anchorY - baseH / 2.0f,
                baseW, baseH, baseW * 0.45f);
            basePath.applyTransform(xform);

            g.setColour(baseCol);
            g.fillPath(basePath);
            g.setColour(bodyEdge);
            g.strokePath(basePath, juce::PathStrokeType(1.5f));

            // Holographic blade
            float bladeBot = anchorY - baseH * 0.3f;
            float bladeTop = bladeBot - bladeH;
            float bw = bladeW;

            juce::Path bladePath;
            bladePath.startNewSubPath(anchorX - bw / 2.0f, bladeBot);
            bladePath.cubicTo(
                anchorX - bw * 0.55f, bladeBot - bladeH * 0.4f,
                anchorX - bw * 0.2f,  bladeBot - bladeH * 0.75f,
                anchorX,              bladeTop);
            bladePath.cubicTo(
                anchorX + bw * 0.2f,  bladeBot - bladeH * 0.75f,
                anchorX + bw * 0.55f, bladeBot - bladeH * 0.4f,
                anchorX + bw / 2.0f,  bladeBot);
            bladePath.closeSubPath();
            bladePath.applyTransform(xform);

            g.setColour(bladeCol.withAlpha(0.1f));
            g.strokePath(bladePath, juce::PathStrokeType(8.0f * sc));
            g.setColour(bladeCol.withAlpha(0.25f));
            g.strokePath(bladePath, juce::PathStrokeType(4.0f * sc));

            auto gradBot = juce::Point<float>(anchorX, bladeBot).transformedBy(xform);
            auto gradTop = juce::Point<float>(anchorX, bladeTop).transformedBy(xform);
            juce::ColourGradient bladeGrad(
                bladeCol.withAlpha(0.85f), gradBot.x, gradBot.y,
                bladeCol.withAlpha(0.3f),  gradTop.x, gradTop.y, false);
            g.setGradientFill(bladeGrad);
            g.fillPath(bladePath);

            g.setColour(bladeCol.withAlpha(0.7f));
            g.strokePath(bladePath, juce::PathStrokeType(1.2f));
        }
    }

    //==========================================================================
    // Drawing: Time-Rewind face — reuses recording grid visual style
    // Plays rewindSnapshot in REVERSE at 3× speed (data scrolls rightward)
    //==========================================================================
    void drawRewindFace(juce::Graphics& g, float cx, float vcy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        // Visor bounding box (folded asymmetric egg)
        float a   = r * 0.8094f * hScale;
        float fYT = 0.8713f * r * 0.6448f;
        float fYB = 0.8713f * r * 0.48f;
        float vw  = a * 2.0f;
        float vh  = fYT + fYB;

        float left   = cx - a;
        float top    = vcy - fYT;
        float right  = cx + a;
        float bottom = vcy + fYB;

        // Clip to visor path
        g.saveState();
        auto visorClip = buildVisorPath(cx, vcy, r, hScale);
        g.reduceClipRegion(visorClip);

        auto deepBlue     = juce::Colour(0xFF1A3AE8);   // deep blue — main
        auto accentPurple = juce::Colour(0xFF9944FF);    // purple — glitch accent
        const float cell = 4.0f;

        // ── 1. Faint grid lines (deep blue, subtle) ──
        g.setColour(deepBlue.withAlpha(0.08f));
        for (float gx = left; gx <= right; gx += cell)
            g.fillRect(gx, top, 0.5f, vh);
        for (float gy = top; gy <= bottom; gy += cell)
            g.fillRect(left, gy, vw, 0.5f);

        // ── 2. Bottom baseline (raised for visor-edge clearance) ──
        float baselineY = bottom - vh * 0.08f - 12.0f;
        g.setColour(deepBlue.withAlpha(0.30f));
        g.fillRect(left, baselineY - 0.5f, vw, 1.0f);

        // ── 3. Reverse-scrolling histogram from rewindSnapshot ──
        int numCols = static_cast<int>(vw / cell);
        float maxDrawH = vh * 0.70f;
        int maxCells = static_cast<int>(maxDrawH / cell);
        int snapSize = static_cast<int>(rewindSnapshot.size());

        if (snapSize > 0)
        {
            // Animation progress: 0 → 1 over 1.5s (90 frames)
            float t = 1.0f - static_cast<float>(rewindFramesLeft) / 90.0f;
            t = juce::jlimit(0.0f, 1.0f, t);

            // 3× speed: scroll through 3× the snapshot length during the animation
            int scrollOffset = static_cast<int>(t * static_cast<float>(snapSize) * 3.0f);

            for (int col = 0; col < numCols; ++col)
            {
                // Same right-to-left draw position as recording grid
                float colX = right - (static_cast<float>(col) + 1.0f) * cell;

                // Reverse: shift read index backwards by scrollOffset → data scrolls right
                int rawIdx = snapSize - 1 - col - scrollOffset;
                int histIdx = ((rawIdx % snapSize) + snapSize) % snapSize;

                const auto& colData = rewindSnapshot[static_cast<size_t>(histIdx)];
                float amplitude = colData.level;
                uint32_t colSeed = colData.randomSeed;

                int activeCells = juce::jmax(0, static_cast<int>(amplitude * static_cast<float>(maxCells)));
                if (activeCells == 0) continue;

                for (int row = 0; row < activeCells; ++row)
                {
                    // Stack upward from baseline (same as recording grid)
                    float cellY = baselineY - static_cast<float>(row + 1) * cell;
                    float dist = static_cast<float>(row)
                               / static_cast<float>(juce::jmax(1, activeCells));

                    // Per-cell deterministic RNG (same seed scheme as recording grid)
                    juce::Random cellRng(colSeed + static_cast<uint32_t>(row) * 7919u);
                    bool isPurpleGlitch = cellRng.nextFloat() < 0.12f;
                    bool isHollow = cellRng.nextFloat() < 0.35f;

                    if (dist > 0.75f)
                    {
                        // Top edge — sparse: lower alpha
                        float edgeAlpha = cellRng.nextFloat() > 0.5f ? 0.5f : 0.3f;

                        if (isPurpleGlitch)
                            g.setColour(accentPurple.withAlpha(edgeAlpha * 0.8f));
                        else
                            g.setColour(deepBlue.withAlpha(edgeAlpha));

                        if (isHollow)
                            g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                        else
                            g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                    }
                    else
                    {
                        // Core pixels
                        if (isPurpleGlitch)
                        {
                            g.setColour(accentPurple.withAlpha(0.75f));
                            if (isHollow)
                                g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                            else
                                g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                        }
                        else
                        {
                            g.setColour(deepBlue.withAlpha(0.85f));
                            g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                        }
                    }
                }

                // Scattered glitch pixels above envelope (same as recording grid)
                juce::Random scatterRng(colSeed + 99991u);
                if (scatterRng.nextFloat() < 0.12f)
                {
                    int extraRow = activeCells + 1 + scatterRng.nextInt(4);
                    float cellY = baselineY - static_cast<float>(extraRow + 1) * cell;
                    bool purpleScatter = scatterRng.nextFloat() < 0.25f;
                    g.setColour(purpleScatter ? accentPurple.withAlpha(0.2f)
                                              : deepBlue.withAlpha(0.15f));
                    g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                }
            }
        }

        // ── 4. Scanline interference (reversed sweep: bottom → top) ──
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        float scanY = bottom - std::fmod(ms * 0.08f, vh + 8.0f) + 4.0f;
        g.setColour(deepBlue.withAlpha(0.10f));
        g.fillRect(left, scanY, vw, 2.0f);
        g.setColour(accentPurple.withAlpha(0.04f));
        g.fillRect(left, scanY + 2.0f, vw, 1.0f);

        // ── 5. Rewind indicator — pulsing deep blue ◀◀ (replaces REC dot) ──
        float blinkAlpha = 0.5f + 0.5f * std::sin(ms * 0.012f);
        float indX = left + 18.0f;
        float indY = top + 12.0f;
        float triW = 4.5f, triH = 5.5f;

        g.setColour(deepBlue.withAlpha(blinkAlpha * 0.9f));
        juce::Path tri1;
        tri1.addTriangle(indX + triW, indY, indX + triW, indY + triH,
                          indX, indY + triH * 0.5f);
        g.fillPath(tri1);
        juce::Path tri2;
        float off2 = triW * 1.1f;
        tri2.addTriangle(indX + off2 + triW, indY, indX + off2 + triW, indY + triH,
                          indX + off2, indY + triH * 0.5f);
        g.fillPath(tri2);

        g.restoreState();
    }

    //==========================================================================
    // Drawing: Time-Rewind face (NONO original) — cyan + red glitch, ellipse clip
    //==========================================================================
    void drawRewindFaceNono(juce::Graphics& g, float cx, float vcy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        // Visor geometry (elliptical — original Nono visor)
        float vw = r * 1.7f * hScale;
        float vh = r * 1.4f;

        float left   = cx - vw / 2.0f;
        float top    = vcy - vh / 2.0f;
        float right  = cx + vw / 2.0f;
        float bottom = vcy + vh / 2.0f;

        // Clip to visor ellipse
        g.saveState();
        juce::Path visorClip;
        visorClip.addEllipse(left, top, vw, vh);
        g.reduceClipRegion(visorClip);

        auto holoCyan  = juce::Colour(0xFF00FFFF);
        auto glitchRed = juce::Colour(0xFFFF2A3A);
        const float cell = 4.0f;

        // ── 1. Faint grid lines ──
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        for (float gx = left; gx <= right; gx += cell)
            g.fillRect(gx, top, 0.5f, vh);
        for (float gy = top; gy <= bottom; gy += cell)
            g.fillRect(left, gy, vw, 0.5f);

        // ── 2. Bottom baseline ──
        float baselineY = bottom - vh * 0.08f - 12.0f;
        g.setColour(holoCyan.withAlpha(0.25f));
        g.fillRect(left, baselineY - 0.5f, vw, 1.0f);

        // ── 3. Reverse-scrolling histogram from rewindSnapshot ──
        int numCols = static_cast<int>(vw / cell);
        float maxDrawH = vh * 0.70f;
        int maxCells = static_cast<int>(maxDrawH / cell);
        int snapSize = static_cast<int>(rewindSnapshot.size());

        if (snapSize > 0)
        {
            float t = 1.0f - static_cast<float>(rewindFramesLeft) / 90.0f;
            t = juce::jlimit(0.0f, 1.0f, t);
            int scrollOffset = static_cast<int>(t * static_cast<float>(snapSize) * 3.0f);

            for (int col = 0; col < numCols; ++col)
            {
                float colX = right - (static_cast<float>(col) + 1.0f) * cell;
                int rawIdx = snapSize - 1 - col - scrollOffset;
                int histIdx = ((rawIdx % snapSize) + snapSize) % snapSize;

                const auto& colData = rewindSnapshot[static_cast<size_t>(histIdx)];
                float amplitude = colData.level;
                uint32_t colSeed = colData.randomSeed;

                int activeCells = juce::jmax(0, static_cast<int>(amplitude * static_cast<float>(maxCells)));
                if (activeCells == 0) continue;

                for (int row = 0; row < activeCells; ++row)
                {
                    float cellY = baselineY - static_cast<float>(row + 1) * cell;
                    float dist = static_cast<float>(row)
                               / static_cast<float>(juce::jmax(1, activeCells));

                    juce::Random cellRng(colSeed + static_cast<uint32_t>(row) * 7919u);
                    bool isRedGlitch = cellRng.nextFloat() < 0.12f;
                    bool isHollow = cellRng.nextFloat() < 0.35f;

                    if (dist > 0.75f)
                    {
                        float edgeAlpha = cellRng.nextFloat() > 0.5f ? 0.5f : 0.3f;
                        if (isRedGlitch)
                            g.setColour(glitchRed.withAlpha(edgeAlpha * 0.8f));
                        else
                            g.setColour(holoCyan.withAlpha(edgeAlpha));

                        if (isHollow)
                            g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                        else
                            g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                    }
                    else
                    {
                        if (isRedGlitch)
                        {
                            g.setColour(glitchRed.withAlpha(0.75f));
                            if (isHollow)
                                g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                            else
                                g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                        }
                        else
                        {
                            g.setColour(holoCyan.withAlpha(0.8f));
                            g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                        }
                    }
                }

                juce::Random scatterRng(colSeed + 99991u);
                if (scatterRng.nextFloat() < 0.12f)
                {
                    int extraRow = activeCells + 1 + scatterRng.nextInt(4);
                    float cellY = baselineY - static_cast<float>(extraRow + 1) * cell;
                    bool redScatter = scatterRng.nextFloat() < 0.25f;
                    g.setColour(redScatter ? glitchRed.withAlpha(0.2f)
                                           : holoCyan.withAlpha(0.15f));
                    g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                }
            }
        }

        // ── 4. Scanline interference (reversed sweep) ──
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        float scanY = bottom - std::fmod(ms * 0.08f, vh + 8.0f) + 4.0f;
        g.setColour(holoCyan.withAlpha(0.08f));
        g.fillRect(left, scanY, vw, 2.0f);
        g.setColour(glitchRed.withAlpha(0.03f));
        g.fillRect(left, scanY + 2.0f, vw, 1.0f);

        // ── 5. Rewind indicator — pulsing cyan ◀◀ ──
        float blinkAlpha = 0.5f + 0.5f * std::sin(ms * 0.012f);
        float indX = left + 18.0f;
        float indY = top + 12.0f;
        float triW = 4.5f, triH = 5.5f;

        g.setColour(holoCyan.withAlpha(blinkAlpha * 0.9f));
        juce::Path tri1;
        tri1.addTriangle(indX + triW, indY, indX + triW, indY + triH,
                          indX, indY + triH * 0.5f);
        g.fillPath(tri1);
        juce::Path tri2;
        float off2 = triW * 1.1f;
        tri2.addTriangle(indX + off2 + triW, indY, indX + off2 + triW, indY + triH,
                          indX + off2, indY + triH * 0.5f);
        g.fillPath(tri2);

        g.restoreState();
    }

    //==========================================================================
    // Drawing: Video Extraction face — reuses recording grid visual style
    // Same forward scrolling as recording, but amber colored
    //==========================================================================
    void drawExtractFace(juce::Graphics& g, float cx, float vcy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        // Visor bounding box (folded asymmetric egg)
        float a   = r * 0.8094f * hScale;
        float fYT = 0.8713f * r * 0.6448f;
        float fYB = 0.8713f * r * 0.48f;
        float vw  = a * 2.0f;
        float vh  = fYT + fYB;

        float left   = cx - a;
        float top    = vcy - fYT;
        float right  = cx + a;
        float bottom = vcy + fYB;

        // Clip to visor path
        g.saveState();
        auto visorClip = buildVisorPath(cx, vcy, r, hScale);
        g.reduceClipRegion(visorClip);

        auto amberColor = juce::Colour(0xFFD2911E);   // warm amber
        auto glitchRed  = juce::Colour(0xFFFF2A3A);
        const float cell = 4.0f;

        // ── 1. Faint grid lines (same as recording) ──
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        for (float gx = left; gx <= right; gx += cell)
            g.fillRect(gx, top, 0.5f, vh);
        for (float gy = top; gy <= bottom; gy += cell)
            g.fillRect(left, gy, vw, 0.5f);

        // ── 2. Bottom baseline (raised for visor-edge clearance) ──
        float baselineY = bottom - vh * 0.08f - 12.0f;
        g.setColour(amberColor.withAlpha(0.25f));
        g.fillRect(left, baselineY - 0.5f, vw, 1.0f);

        // ── 3. Forward-scrolling histogram from extractSyntheticHistory ──
        int numCols = static_cast<int>(vw / cell);
        float maxDrawH = vh * 0.70f;
        int maxCells = static_cast<int>(maxDrawH / cell);
        int histSize = static_cast<int>(extractSyntheticHistory.size());

        for (int col = 0; col < numCols; ++col)
        {
            // Same right-to-left draw as recording: newest data on the right
            int histIdx = histSize - 1 - col;
            if (histIdx < 0) break;

            const auto& colData = extractSyntheticHistory[static_cast<size_t>(histIdx)];
            float amplitude = colData.level;
            uint32_t colSeed = colData.randomSeed;
            float colX = right - (static_cast<float>(col) + 1.0f) * cell;

            int activeCells = juce::jmax(0, static_cast<int>(amplitude * static_cast<float>(maxCells)));
            if (activeCells == 0) continue;

            for (int row = 0; row < activeCells; ++row)
            {
                // Stack upward from baseline
                float cellY = baselineY - static_cast<float>(row + 1) * cell;
                float dist = static_cast<float>(row)
                           / static_cast<float>(juce::jmax(1, activeCells));

                // Per-cell deterministic RNG (same scheme as recording grid)
                juce::Random cellRng(colSeed + static_cast<uint32_t>(row) * 7919u);
                bool isRedGlitch = cellRng.nextFloat() < 0.12f;
                bool isHollow = cellRng.nextFloat() < 0.35f;

                if (dist > 0.75f)
                {
                    float edgeAlpha = cellRng.nextFloat() > 0.5f ? 0.5f : 0.3f;

                    if (isRedGlitch)
                        g.setColour(glitchRed.withAlpha(edgeAlpha * 0.8f));
                    else
                        g.setColour(amberColor.withAlpha(edgeAlpha));

                    if (isHollow)
                        g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                    else
                        g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                }
                else
                {
                    if (isRedGlitch)
                    {
                        g.setColour(glitchRed.withAlpha(0.75f));
                        if (isHollow)
                            g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                        else
                            g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                    }
                    else
                    {
                        g.setColour(amberColor.withAlpha(0.8f));
                        g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                    }
                }
            }

            // Scattered glitch pixels above envelope
            juce::Random scatterRng(colSeed + 99991u);
            if (scatterRng.nextFloat() < 0.12f)
            {
                int extraRow = activeCells + 1 + scatterRng.nextInt(4);
                float cellY = baselineY - static_cast<float>(extraRow + 1) * cell;
                bool redScatter = scatterRng.nextFloat() < 0.25f;
                g.setColour(redScatter ? glitchRed.withAlpha(0.2f)
                                       : amberColor.withAlpha(0.15f));
                g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
            }
        }

        // ── 4. Scanline interference (forward sweep: top → bottom) ──
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        float scanY = top + std::fmod(ms * 0.06f, vh + 8.0f) - 4.0f;
        g.setColour(amberColor.withAlpha(0.08f));
        g.fillRect(left, scanY, vw, 2.0f);
        g.setColour(glitchRed.withAlpha(0.03f));
        g.fillRect(left, scanY + 2.0f, vw, 1.0f);

        // ── 5. EXTRACT indicator — pulsing amber filmstrip icon ──
        float blinkAlpha = 0.5f + 0.5f * std::sin(ms * 0.010f);
        float indX = left + 18.0f;
        float indY = top + 12.0f;

        // Small filmstrip: two vertical bars with notches
        g.setColour(amberColor.withAlpha(blinkAlpha * 0.9f));
        g.fillRect(indX, indY, 2.0f, 6.0f);
        g.fillRect(indX + 4.0f, indY, 2.0f, 6.0f);
        g.fillRect(indX + 8.0f, indY, 2.0f, 6.0f);
        // Connecting strip
        g.fillRect(indX, indY + 2.0f, 10.0f, 2.0f);

        g.restoreState();
    }

    //==========================================================================
    // Drawing: Folded asymmetric egg visor path builder
    // Width 81.5, top-half 32.5, bottom-half 24 (shortened), fold at chord-width=40
    // Bottom edge: gentle chin-wave ("M" bump) inspired by 喵喵 face shape
    //==========================================================================
    juce::Path buildVisorPath(float cx, float vcy, float r, float hScale) const
    {
        const float a    = r * 0.8094f * hScale;  // semi-width
        const float bTop = r * 0.6448f;           // upper semi-height (unchanged)
        const float bBot = r * 0.48f;             // lower semi-height (shortened — was 0.5753)
        const float fYT  = 0.8713f * bTop;        // fold Y top
        const float fYB  = 0.8713f * bBot;        // fold Y bottom

        juce::Path p;
        const int N = 200;

        for (int i = 0; i <= N; ++i)
        {
            float t = juce::MathConstants<float>::twoPi
                    * static_cast<float>(i) / static_cast<float>(N);
            float xE   = a * std::cos(t);
            float sinT = std::sin(t);
            float yE   = (sinT >= 0.0f) ? (bTop * sinT) : (bBot * sinT);

            float yF = yE;
            if (yE > fYT)        yF = 2.0f * fYT - yE;   // reflect above fold
            else if (yE < -fYB)  yF = -2.0f * fYB - yE;  // reflect below fold

            // ── Chin-wave: gentle "M" bump on bottom half ──
            // Only affects the bottom portion (sinT < 0 → screen-Y below center)
            if (sinT < 0.0f)
            {
                // cosT goes from 1→-1→1 in bottom half; use cos(2t) for double bump
                float cosT = std::cos(t);
                // Smooth bump: peaks at left-center and right-center of chin
                float wave = r * 0.045f * (0.5f + 0.5f * std::cos(cosT * 3.14159f * 2.0f))
                           * (-sinT);  // scale by depth (strongest at bottom)
                yF -= wave;  // push inward (upward in math-Y → screen-Y moves down less)
            }

            float px = cx + xE;
            float py = vcy - yF;  // screen Y inverted

            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }

        p.closeSubPath();
        return p;
    }

    //==========================================================================
    // Drawing: Front face
    //==========================================================================
    void drawVisor(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (!isGuoba())
        {
            // === NONO: dark elliptical visor with blue neon frame ===
            float vw = r * 1.7f * hScale;
            float vh = r * 1.4f;
            float vcy = cy - r * 0.1f;

            g.setColour(screenDark);
            g.fillEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh);

            g.setColour(accentCol().withAlpha(0.08f));
            g.drawEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh, 8.0f);
            g.setColour(accentCol().withAlpha(0.30f));
            g.drawEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh, 3.5f);
            g.setColour(accentCol().withAlpha(0.85f));
            g.drawEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh, 1.5f);
            return;
        }

        // === GUOBA: fur pattern / holo visor with yellow frame ===
        if (visorAlpha < 0.01f) return;

        float vcy = cy - r * 0.06f;  // shifted up slightly
        auto visorPath = buildVisorPath(cx, vcy, r, hScale);

        float maskL = cx - r * 0.8094f * hScale;
        float maskR = cx + r * 0.8094f * hScale;
        float vTop = vcy - r * 1.2f;
        float vBot = vcy + r * 1.2f;

        g.saveState();
        g.reduceClipRegion(visorPath);

        if (holoTransition > 0.99f)
        {
            // ===== FULL HOLO: gray-white gradient fill (translucent, airy) =====
            juce::ColourGradient holoGrad(
                juce::Colour(0xFFF6F6F8).withAlpha(visorAlpha), cx, vTop,
                juce::Colour(0xFFD8D8DE).withAlpha(visorAlpha), cx, vBot, false);
            holoGrad.addColour(0.35, juce::Colour(0xFFECECF0).withAlpha(visorAlpha));
            g.setGradientFill(holoGrad);
            g.fillRect(maskL - r, vTop, (maskR - maskL) + r * 2.0f, vBot - vTop);
        }
        else if (holoTransition < 0.01f)
        {
            // ===== FULL NORMAL: three-arch fur pattern =====
            float leftEyeX  = cx - r * 0.30f * hScale;
            float rightEyeX = cx + r * 0.30f * hScale;
            float baseY       = vcy + r * 0.15f;
            float cheekCtrlY  = vcy - r * 0.10f;
            float noseCtrlY   = vcy - r * 0.22f;
            float leftEyeInnerX  = leftEyeX  - r * 0.02f * hScale;
            float rightEyeInnerX = rightEyeX + r * 0.02f * hScale;
            float transY = vcy + r * 0.05f;

            // White base
            g.setColour(juce::Colour(0xFFFFFEFA).withAlpha(visorAlpha));
            g.fillRect(maskL - r, vTop, (maskR - maskL) + r * 2.0f, vBot - vTop);

            // Gray top with three upward arches
            juce::Path grayTopPath;
            grayTopPath.startNewSubPath(maskL, vTop);
            grayTopPath.lineTo(maskR, vTop);
            grayTopPath.lineTo(maskR, baseY);
            float rcCtrlX = (maskR + rightEyeInnerX) * 0.5f;
            grayTopPath.quadraticTo(rcCtrlX, cheekCtrlY, rightEyeInnerX, transY);
            grayTopPath.quadraticTo(cx, noseCtrlY, leftEyeInnerX, transY);
            float lcCtrlX = (maskL + leftEyeInnerX) * 0.5f;
            grayTopPath.quadraticTo(lcCtrlX, cheekCtrlY, maskL, baseY);
            grayTopPath.closeSubPath();

            g.setColour(juce::Colour(0xFFCCCCCC).withAlpha(visorAlpha * 0.65f));
            g.fillPath(grayTopPath);
        }
        else
        {
            // ===== TRANSITION: cross-fade between fur and solid =====
            float na = 1.0f - holoTransition;

            // Fur layer
            {
                float leftEyeX  = cx - r * 0.30f * hScale;
                float rightEyeX = cx + r * 0.30f * hScale;
                float baseY       = vcy + r * 0.15f;
                float cheekCtrlY  = vcy - r * 0.10f;
                float noseCtrlY   = vcy - r * 0.22f;
                float leftEyeInnerX  = leftEyeX  + r * 0.15f * hScale;
                float rightEyeInnerX = rightEyeX - r * 0.15f * hScale;
                float transY = vcy + r * 0.05f;

                g.setColour(juce::Colour(0xFFFFFEFA).withAlpha(visorAlpha * na));
                g.fillRect(maskL - r, vTop, (maskR - maskL) + r * 2.0f, vBot - vTop);

                juce::Path grayTopPath;
                grayTopPath.startNewSubPath(maskL, vTop);
                grayTopPath.lineTo(maskR, vTop);
                grayTopPath.lineTo(maskR, baseY);
                float rcCtrlX = (maskR + rightEyeInnerX) * 0.5f;
                grayTopPath.quadraticTo(rcCtrlX, cheekCtrlY, rightEyeInnerX, transY);
                grayTopPath.quadraticTo(cx, noseCtrlY, leftEyeInnerX, transY);
                float lcCtrlX = (maskL + leftEyeInnerX) * 0.5f;
                grayTopPath.quadraticTo(lcCtrlX, cheekCtrlY, maskL, baseY);
                grayTopPath.closeSubPath();

                g.setColour(juce::Colour(0xFFCCCCCC).withAlpha(visorAlpha * na * 0.65f));
                g.fillPath(grayTopPath);
            }

            // Holo gradient layer on top
            juce::ColourGradient holoGrad(
                juce::Colour(0xFFF6F6F8).withAlpha(visorAlpha * holoTransition), cx, vTop,
                juce::Colour(0xFFD8D8DE).withAlpha(visorAlpha * holoTransition), cx, vBot, false);
            holoGrad.addColour(0.35, juce::Colour(0xFFECECF0).withAlpha(visorAlpha * holoTransition));
            g.setGradientFill(holoGrad);
            g.fillRect(maskL - r, vTop, (maskR - maskL) + r * 2.0f, vBot - vTop);
        }

        g.restoreState();

        // Yellow visor frame glow rings (always visible)
        g.setColour(accentCol().withAlpha(0.08f * visorAlpha));
        g.strokePath(visorPath, juce::PathStrokeType(8.0f));
        g.setColour(accentCol().withAlpha(0.30f * visorAlpha));
        g.strokePath(visorPath, juce::PathStrokeType(3.5f));
        g.setColour(accentCol().withAlpha(0.85f * visorAlpha));
        g.strokePath(visorPath, juce::PathStrokeType(1.5f));
    }

    void drawEyes(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        float vcy = isGuoba() ? (cy - r * 0.06f) : (cy - r * 0.1f);
        float spacing = r * 0.30f * hScale;
        float ew = r * 0.13f * hScale;

        float maxH = r * 0.5f, minH = r * 0.08f;
        float eh = juce::jmap(juce::jlimit(0.2f, 1.0f, eyeOpenness), 0.2f, 1.0f, minH, maxH);

        // ===== NONO SKIN: original blue neon eyes + all expressions always active =====
        if (!isGuoba())
        {
            auto eyeCol = accentCol();  // blue for Nono

            // TIME-REWIND
            if (isRewinding)
            {
                drawRewindFaceNono(g, cx, vcy, r, hScale);
                return;
            }

            // VIDEO EXTRACTION
            if (isExtractingVideo)
            {
                drawExtractFace(g, cx, vcy, r, hScale);
                return;
            }

            // Dizzy: spinning spiral
            if (isDizzy)
            {
                float angle = static_cast<float>(juce::Time::getMillisecondCounterHiRes()) * 0.015f;
                for (int idx = 0; idx < 2; ++idx)
                {
                    float ex = (idx == 0) ? (cx - spacing) : (cx + spacing);
                    float spiralR = r * 0.22f;
                    float eyeAngle = (idx == 0) ? angle : -angle;

                    juce::ColourGradient bloom(
                        eyeCol.withAlpha(0.3f), ex, vcy,
                        eyeCol.withAlpha(0.0f), ex + spiralR * 1.5f, vcy, true);
                    g.setGradientFill(bloom);
                    g.fillEllipse(ex - spiralR * 1.5f, vcy - spiralR * 1.5f,
                                  spiralR * 3.0f, spiralR * 3.0f);

                    juce::Path spiral;
                    const float turns = 3.0f;
                    const float mxAngle = turns * juce::MathConstants<float>::twoPi;
                    bool first = true;
                    for (float a = 0.0f; a <= mxAngle; a += 0.15f)
                    {
                        float t = a / mxAngle;
                        float sr = spiralR * t;
                        float px = ex + sr * std::cos(a);
                        float py = vcy + sr * std::sin(a);
                        if (first) { spiral.startNewSubPath(px, py); first = false; }
                        else spiral.lineTo(px, py);
                    }

                    g.saveState();
                    g.addTransform(juce::AffineTransform::rotation(eyeAngle, ex, vcy));
                    g.setColour(eyeCol.withAlpha(0.1f));
                    g.strokePath(spiral, juce::PathStrokeType(6.0f));
                    g.setColour(eyeCol.withAlpha(0.35f));
                    g.strokePath(spiral, juce::PathStrokeType(3.0f));
                    g.setColour(eyeCol.withAlpha(0.9f));
                    g.strokePath(spiral, juce::PathStrokeType(1.5f));
                    g.restoreState();
                }
                return;
            }

            struct EyeParams { float x, w, corner; };
            EyeParams nonoEyes[2] = {
                { cx - spacing, ew, ew * 0.4f },
                { cx + spacing, ew, ew * 0.4f }
            };

            // Shy: >< squinting
            if (isShy)
            {
                for (int idx = 0; idx < 2; ++idx)
                {
                    float ex = nonoEyes[idx].x;
                    float chevW = spacing * 0.5f;
                    float chevH = eh * 1.4f;

                    float bloomR = chevH * 0.5f;
                    juce::ColourGradient bloom(
                        magicPink.withAlpha(0.25f), ex, vcy,
                        magicPink.withAlpha(0.0f), ex + bloomR, vcy, true);
                    g.setGradientFill(bloom);
                    g.fillEllipse(ex - bloomR, vcy - bloomR, bloomR * 2.0f, bloomR * 2.0f);

                    juce::Path shyPath;
                    if (idx == 0)
                    {
                        shyPath.startNewSubPath(ex - chevW * 0.4f, vcy - chevH * 0.35f);
                        shyPath.lineTo(ex + chevW * 0.4f, vcy);
                        shyPath.lineTo(ex - chevW * 0.4f, vcy + chevH * 0.35f);
                    }
                    else
                    {
                        shyPath.startNewSubPath(ex + chevW * 0.4f, vcy - chevH * 0.35f);
                        shyPath.lineTo(ex - chevW * 0.4f, vcy);
                        shyPath.lineTo(ex + chevW * 0.4f, vcy + chevH * 0.35f);
                    }

                    g.setColour(magicPink.withAlpha(0.08f));
                    g.strokePath(shyPath, juce::PathStrokeType(5.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                    g.setColour(magicPink.withAlpha(0.25f));
                    g.strokePath(shyPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                    g.setColour(magicPink.withAlpha(0.9f));
                    g.strokePath(shyPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                }
                return;
            }

            // Smile: upward chevron
            if (isSmiling)
            {
                for (int idx = 0; idx < 2; ++idx)
                {
                    float ex = nonoEyes[idx].x;
                    float chevW = spacing * 0.75f;
                    float chevH = eh * 2.0f;
                    float tipY = vcy - chevH * 0.35f;
                    float baseY = vcy + chevH * 0.35f;

                    float bloomR = chevH * 0.6f;
                    juce::ColourGradient bloom(
                        eyeCol.withAlpha(0.3f), ex, vcy,
                        eyeCol.withAlpha(0.0f), ex + bloomR, vcy, true);
                    g.setGradientFill(bloom);
                    g.fillEllipse(ex - bloomR, vcy - bloomR, bloomR * 2.0f, bloomR * 2.0f);

                    juce::Path smilePath;
                    smilePath.startNewSubPath(ex - chevW * 0.5f, baseY);
                    smilePath.lineTo(ex, tipY);
                    smilePath.lineTo(ex + chevW * 0.5f, baseY);

                    g.setColour(eyeCol.withAlpha(0.10f));
                    g.strokePath(smilePath, juce::PathStrokeType(7.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                    g.setColour(eyeCol.withAlpha(0.30f));
                    g.strokePath(smilePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                    g.setColour(eyeCol.withAlpha(eyeGlow));
                    g.strokePath(smilePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                }
                return;
            }

            // Wink: left egg + right "<" chevron
            if (isWinking)
            {
                {
                    float eggCX = nonoEyes[0].x + pupilOffsetLX;
                    float eggCY = vcy + pupilOffsetLY;
                    float eggW = ew * 1.3f;
                    float eggH = eh * 1.8f;

                    float bloomR = eggH * 0.9f;
                    juce::ColourGradient bloom(
                        eyeCol.withAlpha(0.35f), eggCX, eggCY,
                        eyeCol.withAlpha(0.0f), eggCX + bloomR, eggCY, true);
                    g.setGradientFill(bloom);
                    g.fillEllipse(eggCX - bloomR, eggCY - bloomR, bloomR * 2.0f, bloomR * 2.0f);

                    g.setColour(eyeCol.withAlpha(eyeGlow));
                    g.fillEllipse(eggCX - eggW * 0.5f, eggCY - eggH * 0.5f, eggW, eggH);

                    g.setColour(juce::Colours::white.withAlpha(0.55f));
                    float specW = eggW * 0.4f, specH = eggH * 0.12f;
                    g.fillEllipse(eggCX - specW * 0.5f, eggCY - eggH * 0.5f + eggH * 0.1f, specW, specH);
                }
                {
                    float chevW = spacing * 0.85f;
                    float chevH = eh * 2.2f;
                    float tipX = cx + spacing * 0.35f;
                    float tipY2 = vcy;
                    float openX = tipX + chevW;

                    juce::Path winkPath;
                    winkPath.startNewSubPath(openX, tipY2 - chevH * 0.5f);
                    winkPath.lineTo(tipX, tipY2);
                    winkPath.lineTo(openX, tipY2 + chevH * 0.5f);

                    g.setColour(eyeCol.withAlpha(0.12f));
                    g.strokePath(winkPath, juce::PathStrokeType(14.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                    g.setColour(eyeCol.withAlpha(0.35f));
                    g.strokePath(winkPath, juce::PathStrokeType(9.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                    g.setColour(eyeCol.withAlpha(eyeGlow));
                    g.strokePath(winkPath, juce::PathStrokeType(7.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                }
                return;
            }

            // Default: blue neon rounded-rect eyes (audio-reactive)
            for (int idx = 0; idx < 2; ++idx)
            {
                auto& eye = nonoEyes[idx];
                float pOffX = (idx == 0) ? pupilOffsetLX : pupilOffsetRX;
                float pOffY = (idx == 0) ? pupilOffsetLY : pupilOffsetRY;
                float ecx = eye.x + pOffX;
                float ecy = vcy + pOffY;

                if (eyeGlow > 0.72f)
                {
                    float br = juce::jmax(eye.w, eh) * 2.0f * (eyeGlow - 0.3f);
                    juce::ColourGradient bg(
                        eyeCol.withAlpha((eyeGlow - 0.5f) * 1.5f), ecx, ecy,
                        eyeCol.withAlpha(0.0f), ecx + br, ecy, true);
                    g.setGradientFill(bg);
                    g.fillEllipse(ecx - br, ecy - br, br * 2.0f, br * 2.0f);
                }

                g.setColour(eyeCol.withAlpha(eyeGlow));
                g.fillRoundedRectangle(ecx - eye.w / 2.0f, ecy - eh / 2.0f, eye.w, eh, eye.corner);

                g.setColour(juce::Colours::white.withAlpha(eyeGlow * 0.5f));
                float sw = eye.w * 0.45f, sh = eh * 0.1f;
                g.fillRoundedRectangle(ecx - sw / 2.0f, ecy - eh / 2.0f + eh * 0.08f, sw, sh, sw * 0.3f);
            }
            return;
        }

        // ===== GUOBA SKIN: plush eyes (normal) / sunset orange eyes + expressions (holo) =====
        bool inHolo = (holoTransition > 0.5f);

        // ===== TIME-REWIND: Reverse holographic bar waveform (highest priority) =====
        if (isRewinding && inHolo)
        {
            drawRewindFace(g, cx, vcy, r, hScale);
            return;
        }

        // ===== VIDEO EXTRACTION: Amber grid waveform =====
        if (isExtractingVideo && inHolo)
        {
            drawExtractFace(g, cx, vcy, r, hScale);
            return;
        }

        // ===== Dizzy: spinning spiral / mosquito-coil eyes (holo only) =====
        if (isDizzy && inHolo)
        {
            float angle = static_cast<float>(juce::Time::getMillisecondCounterHiRes()) * 0.015f;

            for (int idx = 0; idx < 2; ++idx)
            {
                float ex = (idx == 0) ? (cx - spacing) : (cx + spacing);
                float spiralR = r * 0.22f;
                float eyeAngle = (idx == 0) ? angle : -angle;

                juce::ColourGradient bloom(
                    sunsetOrange.withAlpha(0.3f), ex, vcy,
                    sunsetOrange.withAlpha(0.0f), ex + spiralR * 1.5f, vcy, true);
                g.setGradientFill(bloom);
                g.fillEllipse(ex - spiralR * 1.5f, vcy - spiralR * 1.5f,
                              spiralR * 3.0f, spiralR * 3.0f);

                juce::Path spiral;
                const float turns = 3.0f;
                const float mxAngle = turns * juce::MathConstants<float>::twoPi;
                bool first = true;
                for (float a = 0.0f; a <= mxAngle; a += 0.15f)
                {
                    float t = a / mxAngle;
                    float sr = spiralR * t;
                    float px = ex + sr * std::cos(a);
                    float py = vcy + sr * std::sin(a);
                    if (first) { spiral.startNewSubPath(px, py); first = false; }
                    else spiral.lineTo(px, py);
                }

                g.saveState();
                g.addTransform(juce::AffineTransform::rotation(eyeAngle, ex, vcy));
                g.setColour(sunsetOrange.withAlpha(0.1f));
                g.strokePath(spiral, juce::PathStrokeType(6.0f));
                g.setColour(sunsetOrange.withAlpha(0.35f));
                g.strokePath(spiral, juce::PathStrokeType(3.0f));
                g.setColour(sunsetOrange.withAlpha(0.9f));
                g.strokePath(spiral, juce::PathStrokeType(1.5f));
                g.restoreState();
            }
            return;
        }

        struct EyeParams { float x, w, corner; };
        EyeParams eyes[2] = {
            { cx - spacing, ew, ew * 0.4f },
            { cx + spacing, ew, ew * 0.4f }
        };

        // ===== Shy: >< squinting eyes (holo only) =====
        if (isShy && inHolo)
        {
            for (int idx = 0; idx < 2; ++idx)
            {
                float ex = eyes[idx].x;
                float chevW = spacing * 0.5f;
                float chevH = eh * 1.4f;

                float bloomR = chevH * 0.5f;
                juce::ColourGradient bloom(
                    magicPink.withAlpha(0.25f), ex, vcy,
                    magicPink.withAlpha(0.0f), ex + bloomR, vcy, true);
                g.setGradientFill(bloom);
                g.fillEllipse(ex - bloomR, vcy - bloomR, bloomR * 2.0f, bloomR * 2.0f);

                juce::Path shyPath;
                if (idx == 0)
                {
                    shyPath.startNewSubPath(ex - chevW * 0.4f, vcy - chevH * 0.35f);
                    shyPath.lineTo(ex + chevW * 0.4f, vcy);
                    shyPath.lineTo(ex - chevW * 0.4f, vcy + chevH * 0.35f);
                }
                else
                {
                    shyPath.startNewSubPath(ex + chevW * 0.4f, vcy - chevH * 0.35f);
                    shyPath.lineTo(ex - chevW * 0.4f, vcy);
                    shyPath.lineTo(ex + chevW * 0.4f, vcy + chevH * 0.35f);
                }

                g.setColour(magicPink.withAlpha(0.08f));
                g.strokePath(shyPath, juce::PathStrokeType(5.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(magicPink.withAlpha(0.25f));
                g.strokePath(shyPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(magicPink.withAlpha(0.9f));
                g.strokePath(shyPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            }
            return;
        }

        // ===== Smile: upward "∧" chevron eyes (holo only) =====
        if (isSmiling && inHolo)
        {
            for (int idx = 0; idx < 2; ++idx)
            {
                float ex = eyes[idx].x;
                float chevW = spacing * 0.75f;
                float chevH = eh * 2.0f;
                float tipY = vcy - chevH * 0.35f;
                float baseY = vcy + chevH * 0.35f;

                float bloomR = chevH * 0.6f;
                juce::ColourGradient bloom(
                    sunsetOrange.withAlpha(0.3f), ex, vcy,
                    sunsetOrange.withAlpha(0.0f), ex + bloomR, vcy, true);
                g.setGradientFill(bloom);
                g.fillEllipse(ex - bloomR, vcy - bloomR, bloomR * 2.0f, bloomR * 2.0f);

                juce::Path smilePath;
                smilePath.startNewSubPath(ex - chevW * 0.5f, baseY);
                smilePath.lineTo(ex, tipY);
                smilePath.lineTo(ex + chevW * 0.5f, baseY);

                g.setColour(sunsetOrange.withAlpha(0.10f));
                g.strokePath(smilePath, juce::PathStrokeType(7.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(sunsetOrange.withAlpha(0.30f));
                g.strokePath(smilePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(sunsetOrange.withAlpha(eyeGlow));
                g.strokePath(smilePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            }
            return;
        }

        // ===== Wink: left=nono egg, right="<" chevron (holo only) =====
        if (isWinking && inHolo)
        {
            // Left eye: Nono-style rounded rect (sunset orange)
            {
                float eggCX = eyes[0].x + pupilOffsetLX;
                float eggCY = vcy + pupilOffsetLY;

                float bloomR = juce::jmax(ew, eh) * 2.0f * (eyeGlow - 0.3f);
                if (eyeGlow > 0.72f)
                {
                    juce::ColourGradient bg(
                        sunsetOrange.withAlpha((eyeGlow - 0.5f) * 1.5f), eggCX, eggCY,
                        sunsetOrange.withAlpha(0.0f), eggCX + bloomR, eggCY, true);
                    g.setGradientFill(bg);
                    g.fillEllipse(eggCX - bloomR, eggCY - bloomR, bloomR * 2.0f, bloomR * 2.0f);
                }
                g.setColour(sunsetOrange.withAlpha(eyeGlow));
                g.fillRoundedRectangle(eggCX - ew / 2.0f, eggCY - eh / 2.0f, ew, eh, ew * 0.4f);

                g.setColour(juce::Colours::white.withAlpha(eyeGlow * 0.5f));
                float sw = ew * 0.45f, sh = eh * 0.1f;
                g.fillRoundedRectangle(eggCX - sw / 2.0f, eggCY - eh / 2.0f + eh * 0.08f, sw, sh, sw * 0.3f);
            }

            // Right eye: sharp "<" wink chevron
            {
                float chevW = spacing * 0.85f;
                float chevH = eh * 2.2f;
                float tipX = cx + spacing * 0.35f;
                float tipY = vcy;
                float openX = tipX + chevW;

                juce::Path winkPath;
                winkPath.startNewSubPath(openX, tipY - chevH * 0.5f);
                winkPath.lineTo(tipX, tipY);
                winkPath.lineTo(openX, tipY + chevH * 0.5f);

                g.setColour(sunsetOrange.withAlpha(0.12f));
                g.strokePath(winkPath, juce::PathStrokeType(14.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(sunsetOrange.withAlpha(0.35f));
                g.strokePath(winkPath, juce::PathStrokeType(9.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(sunsetOrange.withAlpha(eyeGlow));
                g.strokePath(winkPath, juce::PathStrokeType(7.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            }
            return;
        }

        // ===== DEFAULT EYES =====
        for (int idx = 0; idx < 2; ++idx)
        {
            auto& eye = eyes[idx];
            float pOffX = (idx == 0) ? pupilOffsetLX : pupilOffsetRX;
            float pOffY = (idx == 0) ? pupilOffsetLY : pupilOffsetRY;
            float ecx = eye.x + pOffX;
            float ecy = vcy + pOffY;

            if (inHolo)
            {
                // === HOLO: Sunset orange neon rounded-rect eyes (audio-reactive) ===
                if (eyeGlow > 0.72f)
                {
                    float br = juce::jmax(eye.w, eh) * 2.0f * (eyeGlow - 0.3f);
                    juce::ColourGradient bg(
                        sunsetOrange.withAlpha((eyeGlow - 0.5f) * 1.5f), ecx, ecy,
                        sunsetOrange.withAlpha(0.0f), ecx + br, ecy, true);
                    g.setGradientFill(bg);
                    g.fillEllipse(ecx - br, ecy - br, br * 2.0f, br * 2.0f);
                }

                g.setColour(sunsetOrange.withAlpha(eyeGlow));
                g.fillRoundedRectangle(ecx - eye.w / 2.0f, ecy - eh / 2.0f, eye.w, eh, eye.corner);

                g.setColour(juce::Colours::white.withAlpha(eyeGlow * 0.5f));
                float sw = eye.w * 0.45f, sh = eh * 0.1f;
                g.fillRoundedRectangle(ecx - sw / 2.0f, ecy - eh / 2.0f + eh * 0.08f, sw, sh, sw * 0.3f);
            }
            else
            {
                // === NORMAL: Plush-toy black pupil + white highlight ===
                float eyeRadius = r * 0.10f * hScale;

                g.setColour(juce::Colour(0xFF111111));
                g.fillEllipse(ecx - eyeRadius, ecy - eyeRadius,
                              eyeRadius * 2.0f, eyeRadius * 2.0f);

                float hlRadius = eyeRadius * 0.32f;
                float hlX = ecx - eyeRadius * 0.28f;
                float hlY = ecy - eyeRadius * 0.33f;
                g.setColour(juce::Colours::white.withAlpha(0.95f));
                g.fillEllipse(hlX - hlRadius, hlY - hlRadius,
                              hlRadius * 2.0f, hlRadius * 2.0f);
            }
        }

        // ===== GUOBA NOSE (normal mode only) =====
        if (!inHolo && !guobaNose.isNull())
        {
            // Position: centered between eyes, slightly below eye line
            // Nose width ≈ 40% of eye spacing, aspect ratio preserved from source image
            float noseW = spacing * 0.715f;
            float noseAspect = static_cast<float>(guobaNose.getHeight())
                             / static_cast<float>(guobaNose.getWidth());
            float noseH = noseW * noseAspect;
            float noseX = cx - noseW * 0.5f;
            float noseY = vcy - r * 0.02f;  // between eyes, slightly above center

            g.setOpacity(1.0f);
            g.drawImage(guobaNose,
                noseX, noseY, noseW, noseH,
                0, 0, guobaNose.getWidth(), guobaNose.getHeight());
        }
    }

    //==========================================================================
    // Drawing: Recording Glitch Grid Waveform (replaces eyes during recording)
    //
    //   - Driven by first-order difference energy (high-frequency bias)
    //   - Sparse texture: mixed fillRect/drawRect for data-loss aesthetic
    //   - Clipped to visor ellipse so nothing bleeds outside the face
    //==========================================================================
    void drawRecordingGrid(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        // Visor bounding box (folded asymmetric egg)
        float vcy = cy - r * 0.06f;  // shifted up (matching visor)
        float a   = r * 0.8094f * hScale;
        float fYT = 0.8713f * r * 0.6448f;
        float fYB = 0.8713f * r * 0.48f;
        float vw  = a * 2.0f;
        float vh  = fYT + fYB;

        float left   = cx - a;
        float top    = vcy - fYT;
        float right  = cx + a;
        float bottom = vcy + fYB;

        // Clip to visor path
        g.saveState();
        auto visorClip = buildVisorPath(cx, vcy, r, hScale);
        g.reduceClipRegion(visorClip);

        // Gray-white gradient backdrop (matching holo visor feel, so red pixels pop)
        juce::ColourGradient gridBg(
            juce::Colour(0xFFF2F2F5).withAlpha(0.95f), cx, top,
            juce::Colour(0xFFD5D5DC).withAlpha(0.95f), cx, bottom, false);
        gridBg.addColour(0.35, juce::Colour(0xFFE8E8EC).withAlpha(0.95f));
        g.setGradientFill(gridBg);
        g.fillRect(left, top, vw, vh);

        const float cell = 4.0f;
        auto recordRed   = juce::Colour(0xFFE82030);   // vivid red — main
        auto accentBlack = juce::Colour(0xFF0A0A14);    // near-black — glitch

        // ── 1. Faint grid lines (dark red, subtle) ──
        g.setColour(recordRed.withAlpha(0.10f));
        for (float gx = left; gx <= right; gx += cell)
            g.fillRect(gx, top, 0.5f, vh);
        for (float gy = top; gy <= bottom; gy += cell)
            g.fillRect(left, gy, vw, 0.5f);

        // ── 2. Bottom baseline (red) ──
        float baselineY = bottom - vh * 0.08f - 12.0f;  // raised 12px for visor-edge clearance
        g.setColour(recordRed.withAlpha(0.35f));
        g.fillRect(left, baselineY - 0.5f, vw, 1.0f);

        // ── 3. Unipolar bottom-up histogram ──
        int numCols = static_cast<int>(vw / cell);
        float maxDrawH = vh * 0.70f;  // 70% height cap
        int maxCells = static_cast<int>(maxDrawH / cell);
        int histSize = static_cast<int>(gridWaveformHistory.size());

        for (int col = 0; col < numCols; ++col)
        {
            int histIdx = histSize - 1 - col;
            if (histIdx < 0) break;

            const auto& colData = gridWaveformHistory[static_cast<size_t>(histIdx)];
            float amplitude = colData.level;
            uint32_t colSeed = colData.randomSeed;
            float colX = right - (static_cast<float>(col) + 1.0f) * cell;

            int activeCells = juce::jmax(0, static_cast<int>(amplitude * static_cast<float>(maxCells)));
            if (activeCells == 0) continue;

            for (int row = 0; row < activeCells; ++row)
            {
                // Stack upward from baseline
                float cellY = baselineY - static_cast<float>(row + 1) * cell;

                float dist = static_cast<float>(row)
                           / static_cast<float>(juce::jmax(1, activeCells));

                // Per-cell deterministic random seeded from column's birth entropy + row
                // This scrolls WITH the data — glitch pixels ride the waveform left
                juce::Random cellRng(colSeed + static_cast<uint32_t>(row) * 7919u);
                bool isBlackGlitch = cellRng.nextFloat() < 0.12f;  // 12% black accent
                bool isHollow = cellRng.nextFloat() < 0.35f;        // 35% hollow outline

                if (dist > 0.75f)
                {
                    // Top edge — sparse: lower alpha
                    float edgeAlpha = cellRng.nextFloat() > 0.5f ? 0.5f : 0.3f;

                    if (isBlackGlitch)
                        g.setColour(accentBlack.withAlpha(edgeAlpha * 0.9f));
                    else
                        g.setColour(recordRed.withAlpha(edgeAlpha));

                    if (isHollow)
                        g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                    else
                        g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                }
                else
                {
                    // Core pixels
                    if (isBlackGlitch)
                    {
                        g.setColour(accentBlack.withAlpha(0.85f));
                        if (isHollow)
                            g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
                        else
                            g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                    }
                    else
                    {
                        g.setColour(recordRed.withAlpha(0.85f));
                        g.fillRect(colX, cellY, cell - 1.0f, cell - 1.0f);
                    }
                }
            }

            // Scattered glitch pixels above envelope (seeded from column data)
            juce::Random scatterRng(colSeed + 99991u);
            if (scatterRng.nextFloat() < 0.12f)
            {
                int extraRow = activeCells + 1 + scatterRng.nextInt(4);
                float cellY = baselineY - static_cast<float>(extraRow + 1) * cell;
                bool blackScatter = scatterRng.nextFloat() < 0.25f;
                g.setColour(blackScatter ? accentBlack.withAlpha(0.3f)
                                         : recordRed.withAlpha(0.20f));
                g.drawRect(colX, cellY, cell - 1.0f, cell - 1.0f, 0.8f);
            }
        }

        // ── 4. Scanline interference (slow vertical sweep) ──
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        float scanY = top + std::fmod(ms * 0.06f, vh + 8.0f) - 4.0f;
        g.setColour(recordRed.withAlpha(0.08f));
        g.fillRect(left, scanY, vw, 2.0f);
        g.setColour(accentBlack.withAlpha(0.04f));
        g.fillRect(left, scanY + 2.0f, vw, 1.0f);

        // ── 5. REC indicator — pulsing red dot, upper-left corner ──
        float blinkAlpha = 0.5f + 0.5f * std::sin(ms * 0.008f);
        float dotX = left + 8.0f;
        float dotY = top + 8.0f;
        g.setColour(juce::Colour(0xFFFF0030).withAlpha(blinkAlpha));
        g.fillEllipse(dotX, dotY, 4.0f, 4.0f);

        g.restoreState();
    }

    //==========================================================================
    // Drawing: Back face — Neon Holographic Drop Zone
    //==========================================================================
    void drawBackFace(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (hScale < 0.2f) return;

        if (!isGuoba())
        {
            // === NONO: dark elliptical back + circular ring + cross ===
            float rx = r * hScale * 0.85f;
            g.setColour(juce::Colour(0xFF1A1A28));
            g.fillEllipse(cx - rx, cy - r * 0.85f, rx * 2.0f, r * 1.7f);

            if (nonoState != NonoState::Back && nonoState != NonoState::Analyzing)
                return;

            float breath = 0.65f + 0.35f * std::sin(neonBreathPhase);
            float ringR = r * 0.55f * hScale;

            juce::Path circlePath;
            circlePath.addEllipse(cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);

            g.setColour(accentCol().withAlpha(0.12f * breath));
            g.strokePath(circlePath, juce::PathStrokeType(8.0f * hScale));
            g.setColour(accentCol().withAlpha(0.35f * breath));
            g.strokePath(circlePath, juce::PathStrokeType(4.0f * hScale));
            g.setColour(accentCol().withAlpha(0.9f * breath));
            g.strokePath(circlePath, juce::PathStrokeType(1.5f * hScale));

            float crossLen = ringR * 0.55f;
            float crossThickBase = r * 0.04f * hScale;

            juce::Path hBar;
            hBar.addRoundedRectangle(cx - crossLen, cy - crossThickBase / 2.0f,
                                      crossLen * 2.0f, crossThickBase, crossThickBase * 0.3f);
            juce::Path vBar;
            vBar.addRoundedRectangle(cx - crossThickBase / 2.0f, cy - crossLen,
                                      crossThickBase, crossLen * 2.0f, crossThickBase * 0.3f);

            g.setColour(accentCol().withAlpha(0.15f * breath));
            g.strokePath(hBar, juce::PathStrokeType(6.0f * hScale));
            g.strokePath(vBar, juce::PathStrokeType(6.0f * hScale));
            g.setColour(accentCol().withAlpha(0.4f * breath));
            g.strokePath(hBar, juce::PathStrokeType(3.0f * hScale));
            g.strokePath(vBar, juce::PathStrokeType(3.0f * hScale));
            g.setColour(accentCol().withAlpha(0.95f * breath));
            g.fillPath(hBar);
            g.fillPath(vBar);
            return;
        }

        // === GUOBA: fur-patterned visor back face ===
        float vcy = cy - r * 0.06f;  // shifted up (matching visor)
        auto visorPath = buildVisorPath(cx, vcy, r, hScale);

        // Three-segment all-upward-arching boundary (same as drawVisor)
        float maskL = cx - r * 0.8094f * hScale;
        float maskR = cx + r * 0.8094f * hScale;
        float leftEyeX  = cx - r * 0.30f * hScale;
        float rightEyeX = cx + r * 0.30f * hScale;

        float baseY       = vcy + r * 0.15f;
        float cheekCtrlY  = vcy - r * 0.10f;
        float noseCtrlY   = vcy - r * 0.22f;
        float leftEyeInnerX  = leftEyeX  - r * 0.02f * hScale;
        float rightEyeInnerX = rightEyeX + r * 0.02f * hScale;
        float transY = vcy + r * 0.05f;

        auto grayFur  = juce::Colour(0xFFCCCCCC).withAlpha(0.65f);
        auto whiteFur = juce::Colour(0xFFFFFEFA);
        float vTop = vcy - r * 1.2f;
        float vBot = vcy + r * 1.2f;

        g.saveState();
        g.reduceClipRegion(visorPath);

        // 1. Fill entire visor white
        g.setColour(whiteFur);
        g.fillRect(maskL - r, vTop, (maskR - maskL) + r * 2.0f, vBot - vTop);

        // 2. Three-segment upward-arching gray top path
        juce::Path grayTopPath;
        grayTopPath.startNewSubPath(maskL, vTop);
        grayTopPath.lineTo(maskR, vTop);
        grayTopPath.lineTo(maskR, baseY);

        // Seg 3 (right cheek): arch UP
        float rcCtrlX = (maskR + rightEyeInnerX) * 0.5f;
        grayTopPath.quadraticTo(rcCtrlX, cheekCtrlY,
                                rightEyeInnerX, transY);
        // Seg 2 (nose bridge): BIG arch UP
        grayTopPath.quadraticTo(cx, noseCtrlY,
                                leftEyeInnerX, transY);
        // Seg 1 (left cheek): arch UP
        float lcCtrlX = (maskL + leftEyeInnerX) * 0.5f;
        grayTopPath.quadraticTo(lcCtrlX, cheekCtrlY,
                                maskL, baseY);

        grayTopPath.closeSubPath();

        g.setColour(grayFur);
        g.fillPath(grayTopPath);

        g.restoreState();

        if (nonoState != NonoState::Back && nonoState != NonoState::Analyzing)
            return;

        // Breathing alpha
        float breath = 0.65f + 0.35f * std::sin(neonBreathPhase);

        // --- Holographic ring: 3-layer glow (traces visor shape) ---
        g.setColour(accentCol().withAlpha(0.12f * breath));
        g.strokePath(visorPath, juce::PathStrokeType(8.0f * hScale));
        g.setColour(accentCol().withAlpha(0.35f * breath));
        g.strokePath(visorPath, juce::PathStrokeType(4.0f * hScale));
        g.setColour(accentCol().withAlpha(0.9f * breath));
        g.strokePath(visorPath, juce::PathStrokeType(1.5f * hScale));

        // --- Neon cross "+" centered in visor ---
        float crossLen = r * 0.35f * hScale;
        float crossThickBase = r * 0.04f * hScale;

        // Horizontal bar
        juce::Path hBar;
        hBar.addRoundedRectangle(cx - crossLen, vcy - crossThickBase / 2.0f,
                                  crossLen * 2.0f, crossThickBase, crossThickBase * 0.3f);
        // Vertical bar
        juce::Path vBar;
        vBar.addRoundedRectangle(cx - crossThickBase / 2.0f, vcy - crossLen,
                                  crossThickBase, crossLen * 2.0f, crossThickBase * 0.3f);

        // Cross layer 1: wide soft glow
        g.setColour(accentCol().withAlpha(0.15f * breath));
        g.strokePath(hBar, juce::PathStrokeType(6.0f * hScale));
        g.strokePath(vBar, juce::PathStrokeType(6.0f * hScale));
        // Cross layer 2: medium glow
        g.setColour(accentCol().withAlpha(0.4f * breath));
        g.strokePath(hBar, juce::PathStrokeType(3.0f * hScale));
        g.strokePath(vBar, juce::PathStrokeType(3.0f * hScale));
        // Cross layer 3: bright core fill
        g.setColour(accentCol().withAlpha(0.95f * breath));
        g.fillPath(hBar);
        g.fillPath(vBar);
    }

    //==========================================================================
    // Drawing: Analysis ripples
    //==========================================================================
    void drawAnalysisRipples(juce::Graphics& g, float cx, float cy, float r)
    {
        const int numRipples = 4;
        for (int i = 0; i < numRipples; ++i)
        {
            float phase = std::fmod(ripplePhase + static_cast<float>(i) / static_cast<float>(numRipples), 1.0f);
            float rippleR = r * 0.4f + phase * r * 2.5f;
            float alpha = (1.0f - phase) * 0.35f;
            g.setColour(accentCol().withAlpha(alpha));
            g.drawEllipse(cx - rippleR, cy - rippleR, rippleR * 2.0f, rippleR * 2.0f, 2.0f);
        }
    }

    //==========================================================================
    // Drawing: Results panel
    //==========================================================================
    void drawResultsBubble(juce::Graphics& g, juce::Rectangle<float> area, float nonoX)
    {
        const float fade = bubbleFadeAlpha;
        if (fade <= 0.01f) return;

        const float tailH = 14.0f;

        // Tail pointing up toward NONO
        juce::Path tail;
        float tailBaseY = area.getY() + tailH;
        tail.addTriangle(nonoX - 12.0f, tailBaseY,
                         nonoX + 12.0f, tailBaseY,
                         nonoX, area.getY());

        g.setColour(juce::Colour(0xFFF5F7FA).withMultipliedAlpha(fade));
        g.fillPath(tail);

        // Bubble body fills remaining area
        auto body = area.withTrimmedTop(tailH).reduced(4.0f, 0.0f);
        g.setColour(juce::Colour(0xFFF5F7FA).withMultipliedAlpha(fade));
        g.fillRoundedRectangle(body, 8.0f);
        g.setColour(accentCol().withAlpha(0.5f * fade));
        g.drawRoundedRectangle(body, 8.0f, 1.5f);
        g.strokePath(tail, juce::PathStrokeType(1.5f));

        // ===== Adaptive Grid: 2x2 (stereo) or 3x2 (multichannel with center) =====
        auto textArea = body.reduced(10.0f, 6.0f);

        // Channel count badge (top-right corner of body)
        if (analysisResult.numChannels > 2)
        {
            juce::String chStr = juce::String(analysisResult.numChannels) + "ch";
            float badgeFontSize = juce::jlimit(9.0f, 13.0f, textArea.getHeight() * 0.1f);
            g.setFont(juce::Font(badgeFontSize, juce::Font::bold));
            float badgeW = g.getCurrentFont().getStringWidthFloat(chStr) + 10.0f;
            float badgeH = badgeFontSize + 4.0f;
            auto badgeRect = juce::Rectangle<float>(body.getRight() - badgeW - 6.0f,
                                                     body.getY() + 4.0f, badgeW, badgeH);
            g.setColour(accentCol().withAlpha(0.2f * fade));
            g.fillRoundedRectangle(badgeRect, 4.0f);
            g.setColour(accentCol().withAlpha(0.9f * fade));
            g.drawText(chStr, badgeRect.toNearestInt(), juce::Justification::centred, false);
        }

        // Determine if we have center channel data
        const bool showCenter = (analysisResult.numChannels >= 6 && analysisResult.centerLUFS > -99.0f);
        const int numRows = showCenter ? 3 : 2;
        const int numCols = 2;

        float fontSize = juce::jlimit(12.0f, 24.0f, textArea.getHeight() / static_cast<float>(numRows) * 0.38f);
        float cellW = textArea.getWidth() / static_cast<float>(numCols);
        float cellH = textArea.getHeight() / static_cast<float>(numRows);

        struct MetricInfo { const char* label; float value; const char* unit; };

        // Build metrics array: 4 base + optional center
        MetricInfo baseMetrics[5] = {
            { u8"\u5cf0\u503c",             analysisResult.peakDBFS,         "dBFS" },
            { u8"\u77ac\u65f6\u6700\u5927", analysisResult.momentaryMaxLUFS, "LUFS" },
            { u8"\u77ed\u671f\u6700\u5927", analysisResult.shortTermMaxLUFS, "LUFS" },
            { u8"\u5e73\u5747\u54cd\u5ea6", analysisResult.integratedLUFS,   "LUFS" },
            { u8"\u4e2d\u58f0\u9053",       analysisResult.centerLUFS,       "LUFS" }
        };

        // Grid positions: row-major, 2 columns
        // Stereo:      [peak, momentary] [short-term, integrated]
        // Multichannel: [peak, momentary] [short-term, integrated] [center, —]
        int gridPos[][2] = { {0,0}, {1,0}, {0,1}, {1,1}, {0,2} };
        int numMetrics = showCenter ? 5 : 4;

        for (int i = 0; i < numMetrics; ++i)
        {
            float cx = textArea.getX() + gridPos[i][0] * cellW;
            float cy = textArea.getY() + gridPos[i][1] * cellH;
            auto cell = juce::Rectangle<float>(cx, cy, cellW, cellH).reduced(3.0f);

            // For the center channel row (single item spanning wider label)
            float labelRatio = (i == 4) ? 0.35f : 0.42f;
            auto labelRect = cell.removeFromLeft(cell.getWidth() * labelRatio);
            auto valueRect = cell;

            // Label (muted grey)
            g.setFont(juce::Font(fontSize * 0.8f, juce::Font::bold));
            g.setColour(GoodMeterLookAndFeel::textMuted.withMultipliedAlpha(fade));
            g.drawText(juce::String(juce::CharPointer_UTF8(baseMetrics[i].label)),
                       labelRect.toNearestInt(), juce::Justification::centredLeft, false);

            // Value (electric blue, center channel uses magicPink accent)
            g.setFont(juce::Font(fontSize, juce::Font::bold));
            g.setColour((i == 4 ? magicPink : accentCol()).withMultipliedAlpha(fade));
            juce::String valStr = (baseMetrics[i].value <= -99.0f)
                                   ? juce::String(juce::CharPointer_UTF8(u8"\u2013\u221e"))
                                   : juce::String(baseMetrics[i].value, 1);
            g.drawText(valStr + " " + baseMetrics[i].unit,
                       valueRect.toNearestInt(), juce::Justification::centredLeft, false);
        }
    }

    //==========================================================================
    // Drawing: Floating test tube (always visible)
    //==========================================================================
    void drawFloatingTestTube(juce::Graphics& g, float cx, float cy, float r)
    {
        const float tubeX = cx + r * 1.7f;
        const float tubeY = cy - r * 0.1f;
        const float tubeW = r * 0.28f;
        const float tubeH = r * 1.1f;
        const float bulbR = tubeW * 0.65f;
        const float rimH = r * 0.07f;

        float tL = tubeX - tubeW / 2.0f, tR = tubeX + tubeW / 2.0f;
        float tTop = tubeY - tubeH / 2.0f + rimH, tBot = tubeY + tubeH / 2.0f;

        // ============================================================
        // EXPLOSION MODE: broken tube stump + flying shards + splashes
        // ============================================================
        if (isExploded)
        {
            float fade = juce::jlimit(0.0f, 1.0f, 1.0f - explosionProgress * 0.3f);

            // --- 1. Jagged broken tube base (bottom 40%) ---
            float breakY = tTop + (tBot - tTop) * 0.55f;
            juce::Path brokenTube;
            brokenTube.startNewSubPath(tL, tBot - bulbR);
            brokenTube.cubicTo(tL, tBot, tR, tBot, tR, tBot - bulbR);
            brokenTube.lineTo(tR, breakY + tubeH * 0.05f);
            brokenTube.lineTo(tubeX + tubeW * 0.15f, breakY + tubeH * 0.15f);
            brokenTube.lineTo(tubeX + tubeW * 0.05f, breakY - tubeH * 0.02f);
            brokenTube.lineTo(tubeX - tubeW * 0.1f, breakY + tubeH * 0.12f);
            brokenTube.lineTo(tL, breakY + tubeH * 0.08f);
            brokenTube.closeSubPath();

            // Glass fill
            g.setColour(juce::Colours::white.withAlpha(0.3f * fade));
            g.fillPath(brokenTube);
            g.setColour(bodyEdge.withAlpha(0.7f * fade));
            g.strokePath(brokenTube, juce::PathStrokeType(1.5f));

            // Jagged crack glow (danger red)
            g.setColour(juce::Colour(0xFFFF0055).withAlpha(0.4f * fade));
            g.strokePath(brokenTube, juce::PathStrokeType(3.0f));

            // Residual rim (cracked)
            juce::Path rimStub;
            rimStub.addRoundedRectangle(tubeX - tubeW * 0.65f, tubeY - tubeH / 2.0f,
                                         tubeW * 1.3f, rimH, 2.0f);
            g.setColour(bodyEdge.withAlpha(0.5f * fade));
            g.fillPath(rimStub);

            // --- 2. Flying glass shards ---
            for (const auto& s : shards)
            {
                float alpha = juce::jlimit(0.0f, 1.0f, 1.0f - explosionProgress * 0.8f);
                if (alpha < 0.01f) continue;

                float sx = tubeX + s.x;
                float sy = tubeY + s.y;

                juce::Path shard;
                float half = s.sz * 0.5f;
                shard.addTriangle(-half, -half * 0.6f,
                                   half, -half * 0.3f,
                                   0.0f, half);
                shard.applyTransform(juce::AffineTransform::rotation(s.angle)
                    .translated(sx, sy));

                // Glass shard: white core + cyan edge glow
                g.setColour(juce::Colours::white.withAlpha(0.7f * alpha));
                g.fillPath(shard);
                g.setColour(accentCol().withAlpha(0.5f * alpha));
                g.strokePath(shard, juce::PathStrokeType(1.5f));
                // Outer glow
                g.setColour(accentCol().withAlpha(0.15f * alpha));
                g.strokePath(shard, juce::PathStrokeType(4.0f));
            }

            // --- 3. Liquid splatter drops ---
            const juce::Colour dangerPink(0xFFFF0055);
            for (const auto& d : drops)
            {
                float alpha = juce::jlimit(0.0f, 1.0f, 1.0f - explosionProgress * 0.7f);
                if (alpha < 0.01f) continue;

                float dx = tubeX + d.x;
                float dy = tubeY + d.y;
                float dsz = d.sz * (1.0f - explosionProgress * 0.3f);

                // Outer glow
                g.setColour(dangerPink.withAlpha(0.2f * alpha));
                g.fillEllipse(dx - dsz * 1.5f, dy - dsz * 1.5f, dsz * 3.0f, dsz * 3.0f);
                // Core drop
                g.setColour(dangerPink.withAlpha(0.85f * alpha));
                g.fillEllipse(dx - dsz * 0.5f, dy - dsz * 0.5f, dsz, dsz);
                // Specular
                g.setColour(juce::Colours::white.withAlpha(0.4f * alpha));
                g.fillEllipse(dx - dsz * 0.15f, dy - dsz * 0.25f, dsz * 0.3f, dsz * 0.25f);
            }

            // --- 4. Central explosion flash (first 30% of animation) ---
            if (explosionProgress < 0.3f)
            {
                float flashAlpha = (0.3f - explosionProgress) / 0.3f;
                float flashR = r * 0.6f * (1.0f + explosionProgress * 2.0f);
                juce::ColourGradient flash(
                    dangerPink.withAlpha(0.6f * flashAlpha), tubeX, tubeY,
                    dangerPink.withAlpha(0.0f), tubeX + flashR, tubeY, true);
                g.setGradientFill(flash);
                g.fillEllipse(tubeX - flashR, tubeY - flashR, flashR * 2.0f, flashR * 2.0f);
            }

            return; // Skip normal tube drawing
        }

        // ============================================================
        // NORMAL MODE: intact test tube
        // ============================================================

        // Combine normal shake + pouring angle; pivot at tube bottom
        float totalAngle = tubeAngle + clearPourAngle;
        float pivotX = tubeX;
        float pivotY = tubeY + tubeH / 2.0f;
        auto rotation = juce::AffineTransform::rotation(totalAngle, pivotX, pivotY);

        // Glass shell
        juce::Path tubePath;
        tubePath.startNewSubPath(tL, tTop);
        tubePath.lineTo(tL, tBot - bulbR);
        tubePath.cubicTo(tL, tBot, tR, tBot, tR, tBot - bulbR);
        tubePath.lineTo(tR, tTop);
        tubePath.closeSubPath();
        tubePath.applyTransform(rotation);

        // Edit mode glow around tube
        if (editModeTransition > 0.05f)
        {
            g.setColour(neonGreen.withAlpha(0.15f * editModeTransition));
            g.strokePath(tubePath, juce::PathStrokeType(6.0f));
            g.setColour(neonGreen.withAlpha(0.08f * editModeTransition));
            g.strokePath(tubePath, juce::PathStrokeType(12.0f));
        }

        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.fillPath(tubePath);
        g.setColour(bodyEdge.withAlpha(0.65f));
        g.strokePath(tubePath, juce::PathStrokeType(1.3f));

        // Liquid (controlled by liquidHeight: 0=empty, 1=full)
        if (liquidHeight > 0.01f)
        {
            float fullLiqH = tubeH * 0.6f;
            float liqH = fullLiqH * liquidHeight;
            float liqTop = tBot - liqH;
            float wave = tubeLiquidWave * liquidHeight;
            float lL = tL + 1.5f, lR = tR - 1.5f, lB = tBot - bulbR;

            juce::Path liqPath;
            liqPath.startNewSubPath(lL, liqTop + wave * 0.4f);
            liqPath.cubicTo(lL + tubeW * 0.3f, liqTop - wave,
                            lR - tubeW * 0.3f, liqTop + wave,
                            lR, liqTop - wave * 0.4f);
            liqPath.lineTo(lR, lB);
            liqPath.cubicTo(lR, tBot - 1.5f, lL, tBot - 1.5f, lL, lB);
            liqPath.closeSubPath();
            liqPath.applyTransform(rotation);

            // Color transition: magicPink → neonGreen based on editModeTransition
            juce::Colour liqColTop = magicPink.interpolatedWith(neonGreen, editModeTransition);
            juce::Colour liqColBot = magicPink.darker(0.4f).interpolatedWith(
                neonGreen.darker(0.3f), editModeTransition);

            juce::ColourGradient liqGrad(
                liqColTop.withAlpha(0.9f), tubeX, liqTop,
                liqColBot.withAlpha(0.95f), tubeX, tBot, false);
            g.setGradientFill(liqGrad);
            g.fillPath(liqPath);

            // Boiling bubbles (visible when editModeTransition > 0)
            if (editModeTransition > 0.02f)
            {
                float bubbleAlpha = editModeTransition * 0.7f;
                float tubeInnerW = (lR - lL);
                float tubeCX = (lL + lR) * 0.5f;

                for (const auto& b : tubeBubbles)
                {
                    // Bubble Y: rises from bottom to top of liquid
                    float bubbleY = tBot - bulbR * 0.5f - b.phase * liqH * 0.9f;
                    float bubbleX = tubeCX + b.xOff * tubeInnerW;

                    // Transform bubble position
                    auto bp = juce::Point<float>(bubbleX, bubbleY).transformedBy(rotation);

                    float bsz = b.sz * editModeTransition;
                    float bAlpha = bubbleAlpha * (1.0f - b.phase * 0.6f);  // fade near top

                    // Outer glow
                    g.setColour(neonGreen.withAlpha(bAlpha * 0.3f));
                    g.fillEllipse(bp.x - bsz * 1.5f, bp.y - bsz * 1.5f, bsz * 3.0f, bsz * 3.0f);
                    // Core
                    g.setColour(juce::Colours::white.withAlpha(bAlpha * 0.85f));
                    g.fillEllipse(bp.x - bsz * 0.5f, bp.y - bsz * 0.5f, bsz, bsz);
                }
            }
        }

        // Rim
        juce::Path rimPath;
        rimPath.addRoundedRectangle(tubeX - tubeW * 0.65f, tubeY - tubeH / 2.0f,
                                     tubeW * 1.3f, rimH, 2.0f);
        rimPath.applyTransform(rotation);
        g.setColour(bodyEdge);
        g.fillPath(rimPath);
    }

    //==========================================================================
    // Drawing: Particles
    //==========================================================================
    void drawParticles(juce::Graphics& g, float /*cx*/, float /*cy*/, float r)
    {
        if (!showParticles || particleProgress >= 1.0f) return;

        const int numP = 10;
        float alpha = (1.0f - particleProgress) * 0.85f;
        float dist = particleProgress * r * 1.8f;
        float sz = r * 0.05f * (1.0f - particleProgress * 0.6f);

        for (int i = 0; i < numP; ++i)
        {
            float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(numP);
            float px = particleOriginX + dist * std::cos(a);
            float py = particleOriginY + dist * std::sin(a);
            g.setColour(((i % 2 == 0) ? GoodMeterLookAndFeel::accentPink : accentCol()).withAlpha(alpha));
            g.fillEllipse(px - sz, py - sz, sz * 2.0f, sz * 2.0f);
        }
    }

    //==========================================================================
    // Drawing: Lightning VFX (radiates from belt badge on holo toggle)
    //==========================================================================
    void drawLightningVFX(juce::Graphics& g, float /*cx*/, float /*cy*/, float /*r*/)
    {
        float fade = 1.0f - lightningProgress;
        float flash = fade * fade;  // quadratic falloff for snappy feel

        // Central flash at badge
        float bcx = badgeHitRect.getCentreX();
        float bcy = badgeHitRect.getCentreY();
        float flashR = 15.0f + lightningProgress * 40.0f;

        juce::ColourGradient flashGrad(
            accentCol().withAlpha(0.7f * flash), bcx, bcy,
            accentCol().withAlpha(0.0f), bcx + flashR, bcy, true);
        g.setGradientFill(flashGrad);
        g.fillEllipse(bcx - flashR, bcy - flashR, flashR * 2.0f, flashR * 2.0f);

        // Lightning bolts: 3-segment zigzag paths
        for (const auto& bolt : lightningBolts)
        {
            float boltAlpha = bolt.brightness * flash;
            if (boltAlpha < 0.01f) continue;

            // Expand bolt length over time
            float t = juce::jmin(1.0f, lightningProgress * 3.0f);
            float ex = bolt.x1 + (bolt.x2 - bolt.x1) * t;
            float ey = bolt.y1 + (bolt.y2 - bolt.y1) * t;
            float mx = (bolt.x1 + ex) * 0.5f + bolt.midOffsetX;
            float my = (bolt.y1 + ey) * 0.5f;

            // Mid sub-segment for extra zigzag
            float mx2 = (mx + ex) * 0.5f - bolt.midOffsetX * 0.6f;
            float my2 = (my + ey) * 0.5f;

            juce::Path boltPath;
            boltPath.startNewSubPath(bolt.x1, bolt.y1);
            boltPath.lineTo(mx, my);
            boltPath.lineTo(mx2, my2);
            boltPath.lineTo(ex, ey);

            // 3-layer glow
            g.setColour(accentCol().withAlpha(0.08f * boltAlpha));
            g.strokePath(boltPath, juce::PathStrokeType(6.0f));
            g.setColour(accentCol().withAlpha(0.3f * boltAlpha));
            g.strokePath(boltPath, juce::PathStrokeType(3.0f));
            g.setColour(juce::Colours::white.withAlpha(0.9f * boltAlpha));
            g.strokePath(boltPath, juce::PathStrokeType(1.2f));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoloNonoComponent)
};
