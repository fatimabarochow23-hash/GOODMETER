/*
  ==============================================================================
    StandaloneNonoEditor.h
    GOODMETER - Standalone Desktop Pet Editor (v3: Color Orb Meteor Animation)

    Animation Sequence (all manual interpolation, no ComponentAnimator):
      Phase 0: Compact — Nono only (280×360)
      Phase 1: Expand canvas → color orbs shoot out from Nono's center
      Phase 2: Dwell — hold orb circular array ~0.33s (trails dissipate)
      Phase 3: Orbs spin like wheel, fly to bookshelf positions (comet trails)
      Phase 4: Card fade-in — cards appear at shelf positions (alpha 0→1)
      Phase 5: Canvas shrink — window contracts to union bounds of all children

    Hit testing:
      - Nono: circular radius check on head center
      - Cards: tight bounds on visible folded header area
      - Everything else: false → click-through to desktop
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"
#include "HoloNonoComponent.h"
#include "MeterCardComponent.h"
#include "LevelsMeterComponent.h"
#include "VUMeterComponent.h"
#include "Band3Component.h"
#include "SpectrumAnalyzerComponent.h"
#include "PhaseCorrelationComponent.h"
#include "StereoImageComponent.h"
#include "SpectrogramComponent.h"
#include "PsrMeterComponent.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

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
        setOpaque(false);

        // =================================================================
        // Create 8 REAL meter cards — ALL COLLAPSED (folded header only)
        // =================================================================

        // 0: LEVELS
        levelsCard = std::make_unique<MeterCardComponent>(
            "LEVELS", GoodMeterLookAndFeel::accentPink, false);
        levelsCard->preferredContentHeight = 200;
        levelsMeter = new LevelsMeterComponent(audioProcessor);
        levelsMeter->setupTargetMenu();
        levelsCard->setContentComponent(std::unique_ptr<juce::Component>(levelsMeter));
        levelsCard->setHeaderWidget(&levelsMeter->getTargetMenu());
        addChildComponent(levelsCard.get());

        // 1: VU METER
        vuMeterCard = std::make_unique<MeterCardComponent>(
            "VU METER", GoodMeterLookAndFeel::accentYellow, false);
        vuMeterCard->preferredContentHeight = 120;
        vuMeter = new VUMeterComponent();
        vuMeterCard->setContentComponent(std::unique_ptr<juce::Component>(vuMeter));
        addChildComponent(vuMeterCard.get());

        // 2: 3-BAND
        threeBandCard = std::make_unique<MeterCardComponent>(
            "3-BAND", GoodMeterLookAndFeel::accentPurple, false);
        threeBandCard->preferredContentHeight = 160;
        band3Meter = new Band3Component(audioProcessor);
        threeBandCard->setContentComponent(std::unique_ptr<juce::Component>(band3Meter));
        addChildComponent(threeBandCard.get());

        // 3: SPECTRUM
        spectrumCard = std::make_unique<MeterCardComponent>(
            "SPECTRUM", GoodMeterLookAndFeel::accentCyan, false);
        spectrumCard->preferredContentHeight = 220;
        spectrumAnalyzer = new SpectrumAnalyzerComponent(audioProcessor);
        spectrumCard->setContentComponent(std::unique_ptr<juce::Component>(spectrumAnalyzer));
        addChildComponent(spectrumCard.get());

        // 4: PHASE
        phaseCard = std::make_unique<MeterCardComponent>(
            "PHASE", GoodMeterLookAndFeel::accentBlue, false);
        phaseCard->preferredContentHeight = 200;
        phaseMeter = new PhaseCorrelationComponent();
        phaseCard->setContentComponent(std::unique_ptr<juce::Component>(phaseMeter));
        addChildComponent(phaseCard.get());

        // 5: STEREO
        stereoImageCard = std::make_unique<MeterCardComponent>(
            "STEREO", GoodMeterLookAndFeel::accentSoftPink, false);
        stereoImageCard->preferredContentHeight = 200;
        stereoImageMeter = new StereoImageComponent(audioProcessor);
        stereoImageCard->setContentComponent(std::unique_ptr<juce::Component>(stereoImageMeter));
        addChildComponent(stereoImageCard.get());

        // 6: SPECTROGRAM
        spectrogramCard = std::make_unique<MeterCardComponent>(
            "SPECTROGRAM", GoodMeterLookAndFeel::accentYellow, false);
        spectrogramCard->preferredContentHeight = 220;
        spectrogramMeter = new SpectrogramComponent(audioProcessor);
        spectrogramCard->setContentComponent(std::unique_ptr<juce::Component>(spectrogramMeter));
        addChildComponent(spectrogramCard.get());

        // 7: PSR
        psrCard = std::make_unique<MeterCardComponent>(
            "PSR", juce::Colour(0xFF20C997), false);
        psrCard->preferredContentHeight = 160;
        psrMeter = new PsrMeterComponent(audioProcessor);
        psrCard->setContentComponent(std::unique_ptr<juce::Component>(psrMeter));
        addChildComponent(psrCard.get());

        // =================================================================
        // Create HoloNono LAST (on top of all cards in z-order)
        // =================================================================
        holoNono = std::make_unique<HoloNonoComponent>(audioProcessor);
        addAndMakeVisible(holoNono.get());

        holoNono->onSmileOrbitTriggered = [this]()
        {
            if (phase == AnimPhase::compact)
                triggerAnimationSequence();
            else if (phase == AnimPhase::floating)
            {
                // Nono's exclusive resurrection: check for shattered cards first
                int shatteredCount = 0;
                for (int i = 0; i < numCards; ++i)
                    if (cardStowed[i]) shatteredCount++;

                if (shatteredCount > 0)
                    triggerSelectiveRecall();  // only unshatter hidden cards
                else
                    triggerRecall();           // full recall (no shattered cards)
            }
            else if (phase == AnimPhase::settled)
                triggerSettledRecall();
        };

        // Ear-pinch flip back: reset UI state when Nono returns from back/analysis
        holoNono->onEarFlipBack = [this]()
        {
            // Nothing heavy to reset in standalone mode — the flip animation
            // handles hiding the back face. Just repaint to sync.
            repaint();
        };

        // Wire hover button callbacks
        holoNono->onBodyHoverEnter = [this]()
        {
            if (hoverBtnState == HoverButtonState::hidden
                || hoverBtnState == HoverButtonState::retracting)
            {
                hoverBtnState = HoverButtonState::appearing;
                hoverBtnProgress = 0.0f;
                for (int i = 0; i < 3; ++i) hoverBtnStagger[i] = 0.0f;
            }
        };

        // When Nono is dragged in floating phase, move docked shelf with it
        holoNono->onLocalDrag = [this](int dx, int dy)
        {
            juce::ignoreUnused(dx, dy);
            if (phase == AnimPhase::floating)
            {
                nonoFloatingX = holoNono->getX();
                nonoFloatingY = holoNono->getY();
                layoutFloating();
            }
        };

        // Wire floating drag callbacks for all 8 cards
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (!card) continue;
            card->onUndockDragStarted = [this](MeterCardComponent* c, const juce::MouseEvent& e)
            {
                handleUndockDragStarted(c, e);
            };
            card->onFloatingDragging = [this](MeterCardComponent* c, const juce::MouseEvent& e)
            {
                handleFloatingDragging(c, e);
            };
            card->onFloatingDragEnded = [this](MeterCardComponent* c, const juce::MouseEvent& e)
            {
                handleFloatingDragEnded(c, e);
            };
            card->onDetachRequested = [this](MeterCardComponent* c)
            {
                int idx = findCardIndex(c);
                if (idx >= 0) detachCardFromGroup(idx);
            };
            card->onHeightChanged = [this, i]()
            {
                if (phase == AnimPhase::floating && !isSystemStowing)
                {
                    clampCardHeightToAvoidCollision(i);
                    relayoutGroupForCard(i);
                }
            };
        }

        setSize(compactW, compactH);
        setResizable(false, false);
        setInterceptsMouseClicks(false, true);

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
        g.fillAll(juce::Colours::transparentBlack);

        // Draw orbs during animation phases (fly-out, dwell, wheel-to-shelf, AND fade-in cross-dissolve)
        if (phase == AnimPhase::orbFlyOut || phase == AnimPhase::orbDwell
            || phase == AnimPhase::orbWheelToShelf || phase == AnimPhase::cardFadeIn)
        {
            drawOrbs(g);
        }

        // Draw snap guide: fluorescent edge glow on both snapping edges
        if (pendingSnap.valid && phase == AnimPhase::floating)
        {
            auto cyan = juce::Colour(0xFF06D6A0);

            // Outer haze (8px glow)
            g.setColour(cyan.withAlpha(0.15f));
            g.fillRect(pendingSnap.dragEdgeRect.expanded(4.0f));
            g.fillRect(pendingSnap.targetEdgeRect.expanded(4.0f));

            // Mid glow (4px)
            g.setColour(cyan.withAlpha(0.35f));
            g.fillRect(pendingSnap.dragEdgeRect.expanded(2.0f));
            g.fillRect(pendingSnap.targetEdgeRect.expanded(2.0f));

            // Core highlight (2px solid)
            g.setColour(cyan.withAlpha(0.85f));
            g.fillRect(pendingSnap.dragEdgeRect);
            g.fillRect(pendingSnap.targetEdgeRect);
        }

        // Draw shatter effects (Thanos snap VFX)
        drawShatterEffects(g);

        // Draw collision warning flash (red border on overlap region)
        if (collisionWarning.active && collisionWarning.alpha > 0.01f)
        {
            auto& cw = collisionWarning;
            auto warningColour = juce::Colour(0xFFE6335F);  // red

            // Outer glow
            g.setColour(warningColour.withAlpha(0.15f * cw.alpha));
            g.fillRect(cw.overlapRect.expanded(6.0f));

            // Mid glow
            g.setColour(warningColour.withAlpha(0.35f * cw.alpha));
            g.fillRect(cw.overlapRect.expanded(3.0f));

            // Core border (2px)
            g.setColour(warningColour.withAlpha(0.85f * cw.alpha));
            g.drawRect(cw.overlapRect, 2.0f);

            // Translucent fill
            g.setColour(warningColour.withAlpha(0.12f * cw.alpha));
            g.fillRect(cw.overlapRect);
        }

        // Draw hover buttons (on top of everything except snap guides)
        drawHoverButtons(g);
    }

    //==========================================================================
    // Mouse click handling for hover buttons
    //==========================================================================
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (hoverBtnState == HoverButtonState::visible)
        {
            float fx = static_cast<float>(event.x);
            float fy = static_cast<float>(event.y);
            for (int i = 0; i < 3; ++i)
            {
                if (getHoverButtonRect(i).contains(fx, fy))
                {
                    handleHoverButtonClick(i);
                    return;
                }
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        // When double-click fires on Nono body area, hide hover buttons instantly
        if (hoverBtnState != HoverButtonState::hidden)
        {
            hoverBtnState = HoverButtonState::hidden;
            hoverBtnProgress = 0.0f;
            for (int i = 0; i < 3; ++i) hoverBtnStagger[i] = 0.0f;
            hoverBtnHotIndex = -1;
            repaint();
        }
    }

    void resized() override
    {
        if (holoNono == nullptr) return;

        if (phase == AnimPhase::compact)
        {
            holoNono->setBounds(getLocalBounds());
        }
        else if (phase == AnimPhase::settled)
        {
            layoutSettled();
        }
        else if (phase == AnimPhase::floating || phase == AnimPhase::recalling)
        {
            layoutFloating();
        }
        else
        {
            // During animation: Nono at expanded position
            holoNono->setBounds(nonoExpandedX, nonoExpandedY, compactW, compactH);
        }
    }

    //==========================================================================
    // PIXEL-PERFECT HIT TEST — Zero-Trust Pure Euclidean Geometry
    //
    // NO child delegation. NO holoNono->hitTest(). NO rectangle fallback.
    // Only two shapes pass: a CIRCLE (Nono head) and a RECT (test tube).
    // Plus card visible rects (bounds minus 8px shadow).
    // Everything else → false → OS click-through to desktop.
    //==========================================================================
    bool hitTest(int x, int y) override
    {
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);

        // ── 0. Hover buttons (highest priority when visible) ──
        if (hoverBtnState == HoverButtonState::visible)
        {
            for (int i = 0; i < 3; ++i)
                if (getHoverButtonRect(i).contains(fx, fy))
                    return true;
        }

        // ── 1. Nono body — Euclidean circle, nothing else ──
        if (holoNono != nullptr && holoNono->isVisible())
        {
            auto nonoPos = holoNono->getPosition();
            float nonoW = static_cast<float>(holoNono->getWidth());
            float nonoH = static_cast<float>(holoNono->getHeight());
            float unit = juce::jmin(nonoW, nonoH);
            float bodyR = unit * 0.18f;

            // Head center in editor coords (exact mirror of HoloNono::paint math)
            float nonoCX = static_cast<float>(nonoPos.x) + nonoW * 0.5f - bodyR * 0.6f;
            float nonoCY = static_cast<float>(nonoPos.y) + nonoH * 0.5f - bodyR * 0.3f;

            // Euclidean distance² — hit radius = 1.5× body radius
            float hitR = bodyR * 1.5f;
            float dx = fx - nonoCX;
            float dy = fy - nonoCY;
            if (dx * dx + dy * dy <= hitR * hitR)
                return true;

            // ── 2. Test tube — tight rectangle only ──
            // Tube center: (cx + r*1.7, cy - r*0.1), visual size r*0.28 × r*1.1
            float tubeX = nonoCX + bodyR * 1.7f;
            float tubeY = nonoCY - bodyR * 0.1f;
            float tubeW = bodyR * 0.5f;    // slightly wider than 0.28r for grab comfort
            float tubeH = bodyR * 1.3f;    // covers rim to bulb bottom
            if (fx >= tubeX - tubeW * 0.5f && fx <= tubeX + tubeW * 0.5f
                && fy >= tubeY - tubeH * 0.5f && fy <= tubeY + tubeH * 0.5f)
                return true;
        }

        // ── 3. Visible cards — visible rect only (exclude 8px shadow offset) ──
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (card == nullptr || !card->isVisible()) continue;

            auto cb = card->getBounds();
            const int shadowOff = 8;
            auto visibleRect = juce::Rectangle<int>(
                cb.getX() + shadowOff,
                cb.getY() + shadowOff,
                cb.getWidth() - shadowOff,
                cb.getHeight() - shadowOff);

            if (visibleRect.contains(x, y))
                return true;
        }

        // ── 4. EVERYTHING else: unconditional click-through ──
        return false;
    }

    //==========================================================================
    void timerCallback() override
    {
        // 60Hz meter data feed
        float peakL     = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
        float peakR     = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
        float rmsL      = audioProcessor.rmsLevelL.load(std::memory_order_relaxed);
        float rmsR      = audioProcessor.rmsLevelR.load(std::memory_order_relaxed);
        float momentary = audioProcessor.lufsLevel.load(std::memory_order_relaxed);
        float shortTerm = audioProcessor.lufsShortTerm.load(std::memory_order_relaxed);
        float integrated= audioProcessor.lufsIntegrated.load(std::memory_order_relaxed);
        float phaseVal  = audioProcessor.phaseCorrelation.load(std::memory_order_relaxed);

        if (++lraFrameCounter >= 6)
        {
            audioProcessor.pushShortTermLUFSForLRA(shortTerm);
            audioProcessor.calculateLRARealtime();
            lraFrameCounter = 0;
        }

        float luRangeVal = audioProcessor.luRange.load(std::memory_order_relaxed);

        if (phase != AnimPhase::compact)
        {
            if (levelsMeter)  levelsMeter->updateMetrics(peakL, peakR, momentary, shortTerm, integrated, luRangeVal);
            if (vuMeter)      vuMeter->updateVU(rmsL, rmsR);
            if (phaseMeter)   phaseMeter->updateCorrelation(phaseVal);
        }

        // =================================================================
        // Animation state machine
        // =================================================================
        switch (phase)
        {
            case AnimPhase::orbFlyOut:       tickOrbFlyOut();       break;
            case AnimPhase::orbDwell:        tickOrbDwell();        break;
            case AnimPhase::orbWheelToShelf: tickOrbWheelToShelf(); break;
            case AnimPhase::cardFadeIn:      tickCardFadeIn();      break;
            case AnimPhase::canvasShrink:    tickCanvasShrink();    break;
            case AnimPhase::recalling:       tickRecall();          break;
            default: break;
        }

        // Hover button animation (runs in ALL phases)
        tickHoverButtons();

        // Thanos snap shatter physics (runs in ALL phases)
        tickShatterEffects();

        // Collision warning fade-out (runs in ALL phases)
        if (collisionWarning.active)
        {
            collisionWarning.alpha -= 1.0f / 30.0f;  // ~0.5s fade at 60Hz
            if (collisionWarning.alpha <= 0.0f)
            {
                collisionWarning.active = false;
                collisionWarning.alpha = 0.0f;
            }
            repaint();
        }
    }

private:
    GOODMETERAudioProcessor& audioProcessor;
    GoodMeterLookAndFeel customLookAndFeel;
    std::unique_ptr<HoloNonoComponent> holoNono;
    int lraFrameCounter = 0;

    // Meter components (raw pointers — owned by MeterCardComponents)
    LevelsMeterComponent*       levelsMeter       = nullptr;
    VUMeterComponent*           vuMeter           = nullptr;
    Band3Component*             band3Meter        = nullptr;
    SpectrumAnalyzerComponent*  spectrumAnalyzer  = nullptr;
    PhaseCorrelationComponent*  phaseMeter        = nullptr;
    StereoImageComponent*       stereoImageMeter  = nullptr;
    SpectrogramComponent*       spectrogramMeter  = nullptr;
    PsrMeterComponent*          psrMeter          = nullptr;

    // Card wrappers
    std::unique_ptr<MeterCardComponent> levelsCard, vuMeterCard, threeBandCard,
                                        spectrumCard, phaseCard, stereoImageCard,
                                        spectrogramCard, psrCard;

    //==========================================================================
    // Layout constants
    //==========================================================================
    static constexpr int compactW  = 280;
    static constexpr int compactH  = 360;
    static constexpr int expandedW = 1100;
    static constexpr int expandedH = 1100;
    static constexpr int numCards  = 8;

    // Folded card dimensions (wider to fit "SPECTROGRAM" text comfortably)
    static constexpr int foldedCardW = 220;
    static constexpr int foldedCardH = 56;

    // Sword array orbit radius (sized to avoid overlap with 220px-wide cards)
    static constexpr float arcRadius = 320.0f;

    // Nono position in expanded layout (visual center at ~500,500)
    static constexpr int nonoExpandedX = 390;
    static constexpr int nonoExpandedY = 335;
    static constexpr float nonoLocalCX = 110.0f;
    static constexpr float nonoLocalCY = 165.0f;

    // Settled layout
    static constexpr int settledPadding = 10;
    static constexpr int shatterOverflow = 80;  // extra margin when particles are flying
    static constexpr int settledGap     = 20;   // gap between shelf and Nono
    static constexpr int shelfGap       = 4;    // gap between stacked cards

    // Fly-out speed: each card takes ~18 frames (0.3s) at 60Hz — 25% faster
    static constexpr float flySpeed = 1.0f / 18.0f;

    // Wheel-to-shelf: ~1.08s total at 60Hz — 28% faster
    static constexpr float wheelSpeed = 1.0f / 65.0f;

    // Wheel spin amount: 1.5 full rotations during transition
    static constexpr float wheelSpinTotal = 3.0f * juce::MathConstants<float>::pi;

    //==========================================================================
    // Animation state machine
    //==========================================================================
    enum class AnimPhase
    {
        compact,         // Phase 0: Nono only
        orbFlyOut,       // Phase 1: color orbs shoot out from Nono center
        orbDwell,        // Phase 2: orbs hold in circular array ~0.33s
        orbWheelToShelf, // Phase 3: orbs spin like wheel → fly to shelf positions
        cardFadeIn,      // Phase 4: cards fade in at shelf positions
        canvasShrink,    // Phase 5: shrink window to fit
        settled,         // Final: Nono + bookshelf, done
        floating,        // Cards undocked, canvas covers full screen
        recalling        // Cards flying back to shelf
    };

    AnimPhase phase = AnimPhase::compact;
    int animFrameCounter = 0;
    int nextCardIndex = 0;

    // Per-card fly-out state
    float cardFlyProgress[numCards] = {};
    bool  cardLaunched[numCards] = {};

    // Wheel-to-shelf state
    float wheelProgress = 0.0f;

    // Shelf target positions in expanded canvas (computed when entering orbWheelToShelf)
    float shelfTargetX = 0.0f;
    float shelfTargetStartY = 0.0f;

    //==========================================================================
    // Orb animation data (color orbs + comet trails)
    //==========================================================================
    struct OrbState
    {
        float cx = 0.0f, cy = 0.0f;  // current position (editor coords)
        juce::Colour colour;
    };
    OrbState orbStates[numCards];

    // Comet trail: ring buffer storing recent positions per orb
    static constexpr int trailLen = 16;
    juce::Point<float> orbTrail[numCards][trailLen];
    int trailHead = 0;

    // Orb visual size
    static constexpr float orbRadius = 16.0f;

    // Card fade-in alpha (after orbs land at shelf)
    float cardFadeInAlpha = 0.0f;

    //==========================================================================
    // Floating phase state
    //==========================================================================
    struct CardFloatState
    {
        bool isFloating = false;
        int snapGroupID = -1;
    };
    CardFloatState cardFloatState[numCards] = {};

    // Nono position in the full-screen floating canvas
    int nonoFloatingX = 0;
    int nonoFloatingY = 0;

    // Floating drag state
    int floatingDragCardIndex = -1;
    juce::Point<int> floatingDragOffset;

    // Recall animation state
    float recallProgress[numCards] = {};
    juce::Point<float> recallStartPos[numCards] = {};
    bool recallingCard[numCards] = {};  // true = this card participates in recall animation
    float recallTargetX = 0.0f;
    float recallTargetStartY = 0.0f;

    //==========================================================================
    // Snap group management
    //==========================================================================
    struct EdgeRelation
    {
        int cardA;       // upper or left card
        int cardB;       // lower or right card
        bool isVertical; // true = top-bottom stacking, false = left-right
    };

    struct SnapGroup
    {
        int groupID;
        std::vector<int> members;
        std::map<int, juce::Point<int>> offsets;
        std::vector<EdgeRelation> edgeRelations;
    };
    std::vector<SnapGroup> snapGroups;
    int nextGroupID = 0;

    // Pending snap (visual guide during drag)
    struct PendingSnap
    {
        bool valid = false;
        int dragCardIndex = -1;
        int targetCardIndex = -1;
        juce::Point<int> snapDelta;
        bool isVertical = false;                     // snap direction
        juce::Rectangle<float> dragEdgeRect;         // drag card's snapping edge
        juce::Rectangle<float> targetEdgeRect;       // target card's snapping edge
    };
    PendingSnap pendingSnap;

    // Collision warning (red flash when snap is rejected due to expansion forecast)
    struct CollisionWarning
    {
        bool active = false;
        float alpha = 0.0f;
        juce::Rectangle<float> overlapRect;
    };
    CollisionWarning collisionWarning;

    // Snap constants
    static constexpr int snapThreshold = 14;         // pixels for snap detection
    static constexpr int snapOverlapMin = 10;         // perpendicular overlap minimum

    //==========================================================================
    // Hover button system (3 icon buttons below Nono)
    //==========================================================================
    enum class HoverButtonState
    {
        hidden,      // Buttons not visible
        appearing,   // Fly-in animation (0→1 over 12 frames)
        visible,     // Buttons fully visible, interactive
        retracting   // Fly-back animation (1→0 over 10 frames)
    };

    HoverButtonState hoverBtnState = HoverButtonState::hidden;
    float hoverBtnProgress = 0.0f;   // 0=hidden, 1=visible
    int hoverBtnHotIndex = -1;       // -1=none, 0=gear, 1=tape, 2=shard
    float hoverBtnStagger[3] = {};   // per-button stagger progress for appear animation

    // Button geometry constants
    static constexpr float hoverBtnSize = 52.0f;
    static constexpr float hoverBtnGap = 10.0f;
    static constexpr float hoverBtnStripW = 3 * 52 + 2 * 10;  // 176px

    // Button icon images (loaded once from BinaryData)
    juce::Image btnSettingsImg;
    juce::Image btnRecordImg;
    juce::Image btnStowImg;
    bool hoverBtnIconsLoaded = false;

    // Recording state (for tape button pulse)
    bool isRecording = false;

    //==========================================================================
    // Thanos Snap Stow system (Phase 3)
    //==========================================================================
    bool cardStowed[numCards] = {};   // true = card is stowed (hidden via snap)

    // Per-card shatter VFX state
    struct ShatterShard
    {
        float x = 0, y = 0;           // offset from card center
        float vx = 0, vy = 0;         // velocity
        float angle = 0, spin = 0;    // rotation
        float sz = 8.0f;              // triangle size
        juce::Colour colour;
    };

    struct ShatterSparkle
    {
        float x = 0, y = 0;
        float vx = 0, vy = 0;
        float sz = 3.0f;
        float alpha = 1.0f;
    };

    struct CardShatterState
    {
        bool active = false;
        float progress = 0.0f;        // 0→1 over ~1.2s
        float originX = 0, originY = 0;  // card center when shatter started
        ShatterShard shards[12];
        ShatterSparkle sparkles[8];
    };
    CardShatterState cardShatterStates[numCards] = {};
    bool anyShatterActive = false;

    // System animation flag: when true, onHeightChanged skips collision/relayout
    bool isSystemStowing = false;

    //==========================================================================
    // Easing functions
    //==========================================================================
    static float easeOutCubic(float t)
    {
        float u = 1.0f - t;
        return 1.0f - u * u * u;
    }

    static float easeInCubic(float t)
    {
        return t * t * t;
    }

    static float easeInOutCubic(float t)
    {
        return t < 0.5f
            ? 4.0f * t * t * t
            : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    //==========================================================================
    // Hover button helpers
    //==========================================================================

    void loadHoverButtonIcons()
    {
        if (hoverBtnIconsLoaded) return;
        btnSettingsImg = juce::ImageCache::getFromMemory(
            BinaryData::btn_settings_png, BinaryData::btn_settings_pngSize);
        btnRecordImg = juce::ImageCache::getFromMemory(
            BinaryData::btn_record_png, BinaryData::btn_record_pngSize);
        btnStowImg = juce::ImageCache::getFromMemory(
            BinaryData::btn_stow_png, BinaryData::btn_stow_pngSize);
        hoverBtnIconsLoaded = true;
    }

    /** Get Nono's center and body radius in editor coordinates */
    struct NonoGeometry { float cx, cy, bodyR; };
    NonoGeometry getNonoCenterAndRadius() const
    {
        if (holoNono == nullptr) return { 0, 0, 0 };
        auto nonoPos = holoNono->getPosition();
        float nonoW = static_cast<float>(holoNono->getWidth());
        float nonoH = static_cast<float>(holoNono->getHeight());
        float unit = juce::jmin(nonoW, nonoH);
        float bodyR = unit * 0.18f;
        float cx = static_cast<float>(nonoPos.x) + nonoW * 0.5f - bodyR * 0.6f;
        float cy = static_cast<float>(nonoPos.y) + nonoH * 0.5f - bodyR * 0.3f;
        return { cx, cy, bodyR };
    }

    /** Get rect for hover button i (0=gear, 1=tape, 2=shard) at FULL visibility */
    juce::Rectangle<float> getHoverButtonRect(int i) const
    {
        auto [cx, cy, bodyR] = getNonoCenterAndRadius();
        float stripX = cx - hoverBtnStripW / 2.0f;
        float stripY = cy + bodyR * 1.5f + 8.0f;
        float bx = stripX + static_cast<float>(i) * (hoverBtnSize + hoverBtnGap);
        return { bx, stripY, hoverBtnSize, hoverBtnSize };
    }

    /** Safe zone: union of Nono body bounding box + button strip */
    juce::Rectangle<float> getHoverSafeZone() const
    {
        auto [cx, cy, bodyR] = getNonoCenterAndRadius();
        float hitR = bodyR * 1.5f;
        auto nonoRect = juce::Rectangle<float>(cx - hitR, cy - hitR, hitR * 2, hitR * 2);
        float stripX = cx - hoverBtnStripW / 2.0f;
        float stripY = cy + bodyR * 1.5f + 8.0f;
        auto btnRect = juce::Rectangle<float>(stripX, stripY, hoverBtnStripW, hoverBtnSize);
        return nonoRect.getUnion(btnRect);
    }

    /** Draw hover buttons in paint() */
    void drawHoverButtons(juce::Graphics& g)
    {
        if (hoverBtnState == HoverButtonState::hidden) return;
        loadHoverButtonIcons();

        auto [cx, cy, bodyR] = getNonoCenterAndRadius();

        const juce::Image* icons[3] = { &btnSettingsImg, &btnRecordImg, &btnStowImg };

        // Accent colours per button for glow effects
        const juce::Colour glowColours[3] = {
            GoodMeterLookAndFeel::accentCyan,     // gear — cyan
            GoodMeterLookAndFeel::accentPink,     // tape — pink
            GoodMeterLookAndFeel::accentPurple    // shard — purple
        };

        for (int i = 0; i < 3; ++i)
        {
            float t = juce::jlimit(0.0f, 1.0f, hoverBtnStagger[i]);
            if (t <= 0.001f) continue;

            float easedT;
            if (hoverBtnState == HoverButtonState::appearing || hoverBtnState == HoverButtonState::visible)
                easedT = easeOutCubic(t);
            else
                easedT = 1.0f - easeInCubic(1.0f - t);

            // Fly from Nono center to final position
            auto finalRect = getHoverButtonRect(i);
            float startX = cx - hoverBtnSize / 2.0f;
            float startY = cy - hoverBtnSize / 2.0f;

            float bx = startX + (finalRect.getX() - startX) * easedT;
            float by = startY + (finalRect.getY() - startY) * easedT;
            float scale = 0.3f + 0.7f * easedT;
            float alpha = easedT;

            float sz = hoverBtnSize * scale;
            float drawX = bx + (hoverBtnSize - sz) / 2.0f;
            float drawY = by + (hoverBtnSize - sz) / 2.0f;

            bool isHot = (i == hoverBtnHotIndex && hoverBtnState == HoverButtonState::visible);

            // Recording pulse for tape button
            float pulseAlpha = 1.0f;
            if (i == 1 && isRecording)
            {
                float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
                pulseAlpha = 0.6f + 0.4f * std::sin(ms * 0.012f); // ~2Hz pulse
            }

            // ── Glow backing layer (replaces opaque circle background) ──
            // Radial gradient glow behind the icon — visible on hover or recording pulse
            if (isHot || (i == 1 && isRecording))
            {
                float glowR = sz * 0.75f;
                float gcx = drawX + sz * 0.5f;
                float gcy = drawY + sz * 0.5f;

                juce::Colour glowCol = (i == 1 && isRecording && !isHot)
                    ? juce::Colour(0xFFE6335F)   // red glow for recording
                    : glowColours[i];

                float glowAlpha = isHot ? 0.45f * alpha : 0.30f * alpha * pulseAlpha;

                juce::ColourGradient glow(
                    glowCol.withAlpha(glowAlpha), gcx, gcy,
                    glowCol.withAlpha(0.0f), gcx + glowR, gcy, true);
                g.setGradientFill(glow);
                g.fillEllipse(gcx - glowR, gcy - glowR, glowR * 2.0f, glowR * 2.0f);
            }

            // ── Draw PNG icon at full button size (no padding, no circle) ──
            if (icons[i] != nullptr && icons[i]->isValid())
            {
                auto destRect = juce::Rectangle<float>(drawX, drawY, sz, sz);

                float iconAlpha = alpha * (isHot ? 1.0f : 0.85f);
                if (i == 1 && isRecording) iconAlpha *= pulseAlpha;

                g.setOpacity(iconAlpha);
                g.drawImage(*icons[i], destRect,
                            juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
                g.setOpacity(1.0f);
            }
        }
    }

    /** Tick hover button animation in timerCallback */
    void tickHoverButtons()
    {
        switch (hoverBtnState)
        {
            case HoverButtonState::hidden:
                break;

            case HoverButtonState::appearing:
            {
                hoverBtnProgress = juce::jmin(1.0f, hoverBtnProgress + 1.0f / 12.0f);
                // Staggered appear: button 0 starts immediately, 1 after 2 frames, 2 after 4 frames
                for (int i = 0; i < 3; ++i)
                {
                    float staggerDelay = static_cast<float>(i) * (2.0f / 12.0f);
                    float localT = juce::jlimit(0.0f, 1.0f, (hoverBtnProgress - staggerDelay) / (1.0f - staggerDelay));
                    hoverBtnStagger[i] = localT;
                }
                if (hoverBtnProgress >= 1.0f)
                {
                    hoverBtnState = HoverButtonState::visible;
                    for (int i = 0; i < 3; ++i) hoverBtnStagger[i] = 1.0f;
                }
                repaint();
                break;
            }

            case HoverButtonState::visible:
            {
                // Poll global mouse position against safe zone
                auto safeZone = getHoverSafeZone();
                auto screenPos = juce::Desktop::getMousePosition();
                auto localPos = getLocalPoint(nullptr, screenPos);
                float fx = static_cast<float>(localPos.x);
                float fy = static_cast<float>(localPos.y);

                if (!safeZone.contains(fx, fy)
                    && !juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
                {
                    hoverBtnState = HoverButtonState::retracting;
                    hoverBtnProgress = 1.0f;
                    hoverBtnHotIndex = -1;
                }
                else
                {
                    // Update hot index
                    hoverBtnHotIndex = -1;
                    for (int i = 0; i < 3; ++i)
                    {
                        if (getHoverButtonRect(i).contains(fx, fy))
                        {
                            hoverBtnHotIndex = i;
                            break;
                        }
                    }
                }
                break;
            }

            case HoverButtonState::retracting:
            {
                hoverBtnProgress = juce::jmax(0.0f, hoverBtnProgress - 1.0f / 10.0f);
                for (int i = 0; i < 3; ++i)
                    hoverBtnStagger[i] = hoverBtnProgress;
                if (hoverBtnProgress <= 0.0f)
                {
                    hoverBtnState = HoverButtonState::hidden;
                    for (int i = 0; i < 3; ++i) hoverBtnStagger[i] = 0.0f;
                }
                repaint();
                break;
            }
        }
    }

    /** Handle click on hover button */
    void handleHoverButtonClick(int buttonIndex)
    {
        switch (buttonIndex)
        {
            case 0: // Gear → Settings menu
                showSettingsMenu();
                break;
            case 1: // Tape → Toggle recording
                handleTapeButtonClick();
                break;
            case 2: // Shard → Thanos snap stow
                handleShardButtonClick();
                break;
        }
    }

    // Forward declarations for button click handlers (implemented in later phases)
    void handleTapeButtonClick()
    {
        if (audioProcessor.audioRecorder.getIsRecording())
        {
            // Stop recording
            audioProcessor.audioRecorder.stop();
            isRecording = false;

            // Reveal the just-finished file in Finder immediately
            auto lastFile = audioProcessor.audioRecorder.getLastRecordedFile();
            if (lastFile.existsAsFile())
                lastFile.revealToUser();
        }
        else
        {
            // Generate unique filename in the user's chosen recording directory
            auto now = juce::Time::getCurrentTime();
            auto filename = "GOODMETER_" + now.formatted("%Y%m%d_%H%M%S") + ".wav";
            auto recDir = getRecordingDirectory();
            auto file = recDir.getChildFile(filename);

            double sr = 48000.0;
            if (auto* dm = getDeviceManager())
            {
                if (auto* device = dm->getCurrentAudioDevice())
                    sr = device->getCurrentSampleRate();
            }

            if (audioProcessor.audioRecorder.start(file, sr, 2))
                isRecording = true;
        }

        // Dismiss hover buttons
        hoverBtnState = HoverButtonState::retracting;
        hoverBtnProgress = 1.0f;
        hoverBtnHotIndex = -1;
    }

    //==========================================================================
    // Phase 3: Thanos Snap Stow (shard button)
    //==========================================================================

    /** Strict three-stage unidirectional shatter (NO recall/resurrection).
     *
     *  Stage 1: If harbor cards OR floating-folded cards exist → shatter them.
     *           Expanded floating cards survive. EXIT.
     *  Stage 2: If Stage 1 didn't hit, but floating-expanded cards exist →
     *           stow them back to harbor (force collapse + full state reset). EXIT.
     *  Stage 3: If only harbor cards remain → shatter them all.
     *
     *  This function NEVER calls triggerSelectiveRecall or triggerRecall.
     *  Resurrection is Nono's exclusive power. */
    void handleShardButtonClick()
    {
        // In compact phase, shard button does nothing (no cards visible)
        if (phase == AnimPhase::compact)
            return;

        // =====================================================================
        // Census: classify every non-stowed visible card
        // =====================================================================
        int harborCount = 0;        // docked, visible, not floating
        int floatingFoldedCount = 0; // floating, visible, NOT expanded
        int floatingExpandedCount = 0; // floating, visible, expanded

        for (int i = 0; i < numCards; ++i)
        {
            if (cardStowed[i]) continue;
            auto* card = getCard(i);
            if (!card || !card->isVisible()) continue;

            if (cardFloatState[i].isFloating)
            {
                if (card->getExpanded())
                    floatingExpandedCount++;
                else
                    floatingFoldedCount++;
            }
            else
            {
                harborCount++;
            }
        }

        // =====================================================================
        // STAGE 1: Shatter harbor cards + floating-folded cards
        //          Expanded floaters survive untouched.
        // =====================================================================
        if (harborCount > 0 || floatingFoldedCount > 0)
        {
            isSystemStowing = true;

            // Dissolve snap groups for non-expanded cards only
            for (auto& group : snapGroups)
            {
                for (int idx : group.members)
                {
                    auto* mc = getCard(idx);
                    if (mc && !mc->getExpanded())
                    {
                        cardFloatState[idx].snapGroupID = -1;
                        mc->showDetachButton = false;
                    }
                }
            }
            // Clean up groups: remove shattered members, delete empty/singleton groups
            for (int g = static_cast<int>(snapGroups.size()) - 1; g >= 0; --g)
            {
                auto& members = snapGroups[static_cast<size_t>(g)].members;
                members.erase(
                    std::remove_if(members.begin(), members.end(), [this](int idx)
                    {
                        auto* mc = getCard(idx);
                        return mc == nullptr || !mc->getExpanded();
                    }),
                    members.end());
                if (members.size() <= 1)
                {
                    for (int idx : members)
                    {
                        cardFloatState[idx].snapGroupID = -1;
                        auto* mc = getCard(idx);
                        if (mc) mc->showDetachButton = false;
                    }
                    snapGroups.erase(snapGroups.begin() + g);
                }
            }

            isSystemStowing = false;

            // Shatter eligible cards (harbor + floating-folded)
            for (int i = 0; i < numCards; ++i)
            {
                if (cardStowed[i]) continue;
                auto* card = getCard(i);
                if (!card || !card->isVisible()) continue;

                // Expanded floating cards survive Stage 1
                if (cardFloatState[i].isFloating && card->getExpanded())
                    continue;

                // Force collapse if somehow expanded (safety)
                if (card->getExpanded())
                    card->setExpanded(false, false);

                cardStowed[i] = true;
                triggerShatterVFX(i);
            }

            expandWindowForShatter();

            // Dismiss hover buttons and EXIT
            hoverBtnState = HoverButtonState::retracting;
            hoverBtnProgress = 1.0f;
            hoverBtnHotIndex = -1;
            return;
        }

        // =====================================================================
        // STAGE 2: No harbor or folded-floating cards. Stow expanded floaters
        //          back to harbor (NOT shatter — just dock them).
        //          Full dirty-data cleanup on each card.
        // =====================================================================
        if (floatingExpandedCount > 0)
        {
            isSystemStowing = true;

            // Dissolve ALL snap groups
            for (auto& group : snapGroups)
            {
                for (int idx : group.members)
                {
                    cardFloatState[idx].snapGroupID = -1;
                    auto* mc = getCard(idx);
                    if (mc) mc->showDetachButton = false;
                }
            }
            snapGroups.clear();

            // Stow each expanded floater back to harbor with full state reset
            for (int i = 0; i < numCards; ++i)
            {
                if (cardStowed[i]) continue;
                auto* card = getCard(i);
                if (!card || !card->isVisible()) continue;
                if (!cardFloatState[i].isFloating) continue;

                // ⚠️ Dirty data cleanup
                card->setExpanded(false, false);       // force collapse
                card->isDocked = true;                 // re-dock
                card->showDetachButton = false;
                card->customContentHeight = -1;        // reset custom sizing
                card->customWidth = -1;
                cardFloatState[i].isFloating = false;   // force un-float
                cardFloatState[i].snapGroupID = -1;
            }

            isSystemStowing = false;

            // Transition to settled layout (shrink window to fit shelf)
            phase = AnimPhase::canvasShrink;
            animFrameCounter = 0;

            // Dismiss hover buttons and EXIT
            hoverBtnState = HoverButtonState::retracting;
            hoverBtnProgress = 1.0f;
            hoverBtnHotIndex = -1;
            return;
        }

        // =====================================================================
        // STAGE 3: Only harbor cards remain (no floaters at all). Shatter all.
        // =====================================================================
        {
            isSystemStowing = true;

            // Dissolve all snap groups (safety)
            for (auto& group : snapGroups)
            {
                for (int idx : group.members)
                {
                    cardFloatState[idx].snapGroupID = -1;
                    auto* mc = getCard(idx);
                    if (mc) mc->showDetachButton = false;
                }
            }
            snapGroups.clear();

            isSystemStowing = false;

            bool anyShattered = false;
            for (int i = 0; i < numCards; ++i)
            {
                if (cardStowed[i]) continue;
                auto* card = getCard(i);
                if (!card || !card->isVisible()) continue;

                if (card->getExpanded())
                    card->setExpanded(false, false);

                cardStowed[i] = true;
                triggerShatterVFX(i);
                anyShattered = true;
            }

            if (anyShattered)
                expandWindowForShatter();
        }

        // Dismiss hover buttons
        hoverBtnState = HoverButtonState::retracting;
        hoverBtnProgress = 1.0f;
        hoverBtnHotIndex = -1;
    }

    /** Initialize shatter particles for a card being stowed */
    void triggerShatterVFX(int cardIndex)
    {
        auto* card = getCard(cardIndex);
        if (!card) return;

        auto cb = card->getBounds();
        float cx = static_cast<float>(cb.getCentreX());
        float cy = static_cast<float>(cb.getCentreY());

        auto& state = cardShatterStates[cardIndex];
        state.active = true;
        state.progress = 0.0f;
        state.originX = cx;
        state.originY = cy;

        auto colour = getCardColour(cardIndex);
        auto& rng = juce::Random::getSystemRandom();

        // 12 triangular shards — radial burst
        for (int i = 0; i < 12; ++i)
        {
            auto& s = state.shards[i];
            float angle = static_cast<float>(i) * (juce::MathConstants<float>::twoPi / 12.0f)
                        + (rng.nextFloat() - 0.5f) * 0.5f;
            float speed = 3.0f + rng.nextFloat() * 5.0f;
            s.x = 0; s.y = 0;
            s.vx = std::cos(angle) * speed;
            s.vy = std::sin(angle) * speed;
            s.angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            s.spin = (rng.nextFloat() - 0.5f) * 0.3f;
            s.sz = 5.0f + rng.nextFloat() * 8.0f;
            s.colour = colour.withMultipliedBrightness(0.7f + rng.nextFloat() * 0.6f);
        }

        // 8 sparkles — fast outward burst
        for (int i = 0; i < 8; ++i)
        {
            auto& sp = state.sparkles[i];
            float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            float speed = 5.0f + rng.nextFloat() * 7.0f;
            sp.x = 0; sp.y = 0;
            sp.vx = std::cos(angle) * speed;
            sp.vy = std::sin(angle) * speed;
            sp.sz = 2.0f + rng.nextFloat() * 4.0f;
            sp.alpha = 1.0f;
        }

        // Hide the actual card immediately
        card->setVisible(false);
        anyShatterActive = true;
    }

    /** Expand the window by shatterOverflow px in all directions so particles
     *  don't get clipped. The extra area is transparent → invisible.
     *  All child positions shift by +overflow to compensate. */
    bool shatterExpanded = false;

    void expandWindowForShatter()
    {
        if (shatterExpanded) return;

        auto* topLevel = getTopLevelComponent();
        if (!topLevel) return;

        shatterExpanded = true;

        // Remember pre-expansion window position
        auto pos = topLevel->getScreenPosition();
        int oldW = topLevel->getWidth();
        int oldH = topLevel->getHeight();

        // Expand window: add overflow on all 4 sides
        topLevel->setTopLeftPosition(pos.x - shatterOverflow, pos.y - shatterOverflow);
        topLevel->setSize(oldW + shatterOverflow * 2, oldH + shatterOverflow * 2);

        // Shift all children by +overflow so they stay in the same screen position
        for (int i = getNumChildComponents() - 1; i >= 0; --i)
        {
            auto* child = getChildComponent(i);
            auto cb = child->getBounds();
            child->setBounds(cb.translated(shatterOverflow, shatterOverflow));
        }

        // Also shift shatter origins that are already set
        for (int i = 0; i < numCards; ++i)
        {
            if (cardShatterStates[i].active)
            {
                cardShatterStates[i].originX += static_cast<float>(shatterOverflow);
                cardShatterStates[i].originY += static_cast<float>(shatterOverflow);
            }
        }
    }

    void shrinkWindowAfterShatter()
    {
        if (!shatterExpanded) return;

        auto* topLevel = getTopLevelComponent();
        if (!topLevel) return;

        shatterExpanded = false;

        auto pos = topLevel->getScreenPosition();
        int oldW = topLevel->getWidth();
        int oldH = topLevel->getHeight();

        // Shift children back
        for (int i = getNumChildComponents() - 1; i >= 0; --i)
        {
            auto* child = getChildComponent(i);
            auto cb = child->getBounds();
            child->setBounds(cb.translated(-shatterOverflow, -shatterOverflow));
        }

        // Shrink window
        topLevel->setTopLeftPosition(pos.x + shatterOverflow, pos.y + shatterOverflow);
        topLevel->setSize(oldW - shatterOverflow * 2, oldH - shatterOverflow * 2);
    }

    /** Tick shatter physics — called from timerCallback */
    void tickShatterEffects()
    {
        if (!anyShatterActive) return;

        bool stillActive = false;
        for (int i = 0; i < numCards; ++i)
        {
            auto& state = cardShatterStates[i];
            if (!state.active) continue;

            state.progress += 0.014f;  // ~1.2s at 60Hz
            if (state.progress >= 1.0f)
            {
                state.active = false;
                continue;
            }
            stillActive = true;

            // Shards: gravity + air drag
            for (auto& s : state.shards)
            {
                s.x += s.vx;
                s.y += s.vy;
                s.vy += 0.15f;    // gravity
                s.vx *= 0.985f;   // air drag
                s.angle += s.spin;
            }

            // Sparkles: fast dissipation
            for (auto& sp : state.sparkles)
            {
                sp.x += sp.vx;
                sp.y += sp.vy;
                sp.vx *= 0.95f;
                sp.vy *= 0.95f;
                sp.alpha = juce::jmax(0.0f, sp.alpha - 0.025f);
            }
        }

        anyShatterActive = stillActive;

        // When all particles are done, shrink window back to normal
        if (!stillActive)
            shrinkWindowAfterShatter();

        repaint();
    }

    /** Draw shatter effects — called from paint() */
    void drawShatterEffects(juce::Graphics& g)
    {
        if (!anyShatterActive) return;

        for (int i = 0; i < numCards; ++i)
        {
            auto& state = cardShatterStates[i];
            if (!state.active) continue;

            float globalFade = juce::jlimit(0.0f, 1.0f, 1.0f - state.progress * 0.8f);

            // Draw shards
            for (const auto& s : state.shards)
            {
                float alpha = globalFade;
                if (alpha < 0.01f) continue;

                float sx = state.originX + s.x;
                float sy = state.originY + s.y;

                juce::Path shard;
                float half = s.sz * 0.5f;
                shard.addTriangle(-half, -half * 0.6f,
                                   half, -half * 0.3f,
                                   0.0f, half);
                shard.applyTransform(juce::AffineTransform::rotation(s.angle)
                    .translated(sx, sy));

                g.setColour(s.colour.withAlpha(0.8f * alpha));
                g.fillPath(shard);

                // Edge glow
                g.setColour(s.colour.brighter(0.3f).withAlpha(0.4f * alpha));
                g.strokePath(shard, juce::PathStrokeType(1.5f));
            }

            // Draw sparkles
            for (const auto& sp : state.sparkles)
            {
                float alpha = sp.alpha * globalFade;
                if (alpha < 0.01f) continue;

                float sx = state.originX + sp.x;
                float sy = state.originY + sp.y;

                // Bright core
                g.setColour(juce::Colours::white.withAlpha(0.9f * alpha));
                g.fillEllipse(sx - sp.sz * 0.5f, sy - sp.sz * 0.5f, sp.sz, sp.sz);

                // Outer glow
                float glowSz = sp.sz * 2.5f;
                g.setColour(getCardColour(i).withAlpha(0.3f * alpha));
                g.fillEllipse(sx - glowSz * 0.5f, sy - glowSz * 0.5f, glowSz, glowSz);
            }

            // Initial flash (first 20% of animation)
            if (state.progress < 0.2f)
            {
                float flashAlpha = (0.2f - state.progress) / 0.2f * 0.5f;
                float flashR = 60.0f * (1.0f + state.progress * 3.0f);
                juce::ColourGradient flash(
                    getCardColour(i).withAlpha(flashAlpha), state.originX, state.originY,
                    getCardColour(i).withAlpha(0.0f), state.originX + flashR, state.originY, true);
                g.setGradientFill(flash);
                g.fillEllipse(state.originX - flashR, state.originY - flashR,
                              flashR * 2.0f, flashR * 2.0f);
            }
        }
    }

    /** Trigger selective recall: only stowed/hidden cards fly back to harbor.
     *  Cards currently floating on screen (expanded or folded) are untouched. */
    void triggerSelectiveRecall()
    {
        // Count stowed cards
        int stowedCount = 0;
        for (int i = 0; i < numCards; ++i)
            if (cardStowed[i]) stowedCount++;

        if (stowedCount == 0) return;

        // If we're in floating or settled, recall only stowed cards
        if (phase == AnimPhase::floating || phase == AnimPhase::settled)
        {
            // If settled, promote to floating first (expands canvas to full screen)
            if (phase == AnimPhase::settled)
                promoteSettledToFloating();

            // Unstow ONLY stowed cards — leave floating cards completely untouched
            for (int i = 0; i < numCards; ++i)
            {
                if (!cardStowed[i]) continue;  // skip non-stowed (active floating) cards
                cardStowed[i] = false;

                auto* card = getCard(i);
                if (!card) continue;

                card->setVisible(true);
                card->setExpanded(false, false);
                card->isDocked = true;
                cardFloatState[i].isFloating = false;
                cardFloatState[i].snapGroupID = -1;
            }

            // Trigger recall animation that only moves recovered cards
            triggerRecall();
        }
        else if (phase == AnimPhase::compact)
        {
            // All stowed from compact phase — just unstow, they'll appear on next orbit
            for (int i = 0; i < numCards; ++i)
                cardStowed[i] = false;
        }
    }

    //==========================================================================
    // Phase 2: Settings Menu (gear button)
    //==========================================================================

    /** Access the StandalonePluginHolder's AudioDeviceManager via singleton */
    juce::AudioDeviceManager* getDeviceManager() const
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            return &holder->deviceManager;
        return nullptr;
    }

    /** Get the user's chosen recording directory (reads from shared PropertiesFile).
     *  Falls back to ~/Desktop if no custom path is set or saved path is invalid. */
    juce::File getRecordingDirectory() const
    {
        if (auto* app = dynamic_cast<juce::JUCEApplication*>(juce::JUCEApplication::getInstance()))
        {
            // Access the ApplicationProperties via the app's PropertiesFile
            // The PropertiesFile is stored at ~/Library/Application Support/GOODMETER.settings
            juce::PropertiesFile::Options opts;
            opts.applicationName     = juce::CharPointer_UTF8(JucePlugin_Name);
            opts.filenameSuffix      = ".settings";
            opts.osxLibrarySubFolder = "Application Support";

            auto propsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile(opts.osxLibrarySubFolder)
                .getChildFile(opts.applicationName + opts.filenameSuffix);

            if (propsFile.existsAsFile())
            {
                juce::XmlDocument doc(propsFile);
                auto xml = doc.getDocumentElement();
                if (xml != nullptr)
                {
                    auto saved = xml->getStringAttribute("recordingDirectory", "");
                    if (saved.isEmpty())
                    {
                        // PropertiesFile stores as child elements: <VALUE name="key" val="value"/>
                        for (auto* child : xml->getChildIterator())
                        {
                            if (child->getStringAttribute("name") == "recordingDirectory")
                            {
                                saved = child->getStringAttribute("val");
                                break;
                            }
                        }
                    }
                    if (saved.isNotEmpty())
                    {
                        juce::File dir(saved);
                        if (dir.isDirectory())
                            return dir;
                    }
                }
            }
        }
        return juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    }

    void showSettingsMenu()
    {
        juce::PopupMenu menu;
        int itemID = 1;

        // ── Section 1: Audio Input Device ──
        menu.addSectionHeader("Audio Input");

        auto* dm = getDeviceManager();
        juce::String currentDeviceName;

        if (dm != nullptr)
        {
            if (auto* currentDevice = dm->getCurrentAudioDevice())
                currentDeviceName = currentDevice->getName();

            auto& deviceTypes = dm->getAvailableDeviceTypes();
            for (auto* deviceType : deviceTypes)
            {
                auto deviceNames = deviceType->getDeviceNames(true); // input devices
                for (int i = 0; i < deviceNames.size(); ++i)
                {
                    bool isCurrent = (deviceNames[i] == currentDeviceName);
                    menu.addItem(itemID, deviceNames[i], true, isCurrent);
                    itemID++;
                }
            }
        }
        else
        {
            menu.addItem(itemID++, "(No audio device)", false);
        }

        menu.addSeparator();

        // ── Section 2: Audio Settings Dialog ──
        const int audioSettingsID = 900;
        menu.addItem(audioSettingsID, "Audio Settings...");

        menu.addSeparator();

        // ── Section 3: Quit ──
        const int quitID = 999;
        menu.addItem(quitID, "Quit GOODMETER");

        // Show menu at the gear button position
        auto btnRect = getHoverButtonRect(0);
        auto screenBtnPos = localPointToGlobal(juce::Point<int>(
            static_cast<int>(btnRect.getCentreX()),
            static_cast<int>(btnRect.getBottom())));

        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetScreenArea(juce::Rectangle<int>(screenBtnPos.x - 1, screenBtnPos.y, 2, 2)),
            [this](int result)
            {
                if (result == 0) return; // dismissed

                if (result == 999)
                {
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    return;
                }

                if (result == 900)
                {
                    // Show Audio Settings — INPUT ONLY (output channels = 0,0)
                    if (auto* devMgr = getDeviceManager())
                    {
                        auto* dialogContent = new juce::AudioDeviceSelectorComponent(
                            *devMgr,
                            0, 256,   // minInputChannels, maxInputChannels
                            0, 0,     // minOutputChannels, maxOutputChannels = 0 → hides Output
                            true,     // showMidiInputOptions
                            false,    // showMidiOutputSelector
                            true,     // showChannelsAsStereoPairs
                            false);   // hideAdvancedOptions
                        dialogContent->setSize(450, 300);
                        dialogContent->setLookAndFeel(&customLookAndFeel);

                        juce::DialogWindow::LaunchOptions opts;
                        opts.content.setOwned(dialogContent);
                        opts.dialogTitle = "AUDIO SETTINGS";
                        opts.dialogBackgroundColour = GoodMeterLookAndFeel::bgPanel;
                        opts.escapeKeyTriggersCloseButton = true;
                        opts.useNativeTitleBar = false;  // use JUCE title bar so LookAndFeel applies
                        opts.resizable = false;
                        opts.componentToCentreAround = this;

                        auto* dialog = opts.launchAsync();
                        if (dialog != nullptr)
                            dialog->setLookAndFeel(&customLookAndFeel);
                    }
                    return;
                }

                // Audio input device selection
                if (auto* devMgr = getDeviceManager())
                {
                    int idx = result - 1;
                    auto& devTypes = devMgr->getAvailableDeviceTypes();
                    for (auto* deviceType : devTypes)
                    {
                        auto deviceNames = deviceType->getDeviceNames(true);
                        if (idx < deviceNames.size())
                        {
                            auto setup = devMgr->getAudioDeviceSetup();
                            setup.inputDeviceName = deviceNames[idx];
                            devMgr->setAudioDeviceSetup(setup, true);
                            return;
                        }
                        idx -= deviceNames.size();
                    }
                }
            });

        // Dismiss hover buttons after showing menu
        hoverBtnState = HoverButtonState::retracting;
        hoverBtnProgress = 1.0f;
        hoverBtnHotIndex = -1;
    }

    //==========================================================================
    MeterCardComponent* getCard(int index) const
    {
        switch (index)
        {
            case 0: return levelsCard.get();
            case 1: return vuMeterCard.get();
            case 2: return threeBandCard.get();
            case 3: return spectrumCard.get();
            case 4: return phaseCard.get();
            case 5: return stereoImageCard.get();
            case 6: return spectrogramCard.get();
            case 7: return psrCard.get();
            default: return nullptr;
        }
    }

    // Radial angle for card i (uniform 360°, starting from bottom)
    float cardAngle(int index) const
    {
        return static_cast<float>(index)
             * (juce::MathConstants<float>::twoPi / static_cast<float>(numCards))
             + juce::MathConstants<float>::halfPi;
    }

    // Get the status colour for card i (matches card construction order)
    juce::Colour getCardColour(int index) const
    {
        switch (index)
        {
            case 0: return GoodMeterLookAndFeel::accentPink;      // LEVELS
            case 1: return GoodMeterLookAndFeel::accentYellow;    // VU METER
            case 2: return GoodMeterLookAndFeel::accentPurple;    // 3-BAND
            case 3: return GoodMeterLookAndFeel::accentCyan;      // SPECTRUM
            case 4: return GoodMeterLookAndFeel::accentBlue;      // PHASE
            case 5: return GoodMeterLookAndFeel::accentSoftPink;  // STEREO
            case 6: return GoodMeterLookAndFeel::accentYellow;    // SPECTROGRAM
            case 7: return juce::Colour(0xFF20C997);              // PSR
            default: return juce::Colours::white;
        }
    }

    //==========================================================================
    // Settled layout — called from resized() when phase == settled
    //==========================================================================
    void layoutSettled()
    {
        // Count visible (non-stowed) cards for proper shelf sizing
        int visibleCount = 0;
        for (int i = 0; i < numCards; ++i)
            if (!cardStowed[i]) visibleCount++;

        int totalShelfH = visibleCount * foldedCardH + juce::jmax(0, visibleCount - 1) * shelfGap;
        int h = getHeight();
        int w = getWidth();

        int shelfStartY = (h - totalShelfH) / 2;

        // If no visible cards, center Nono without shelf offset
        int nonoX, nonoY;
        if (visibleCount > 0)
        {
            nonoX = settledPadding + foldedCardW + settledGap;
            nonoY = (h - compactH) / 2;
        }
        else
        {
            nonoX = (w - compactW) / 2;
            nonoY = (h - compactH) / 2;
        }

        holoNono->setBounds(nonoX, nonoY, compactW, compactH);

        int slot = 0;
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (!card) continue;

            if (cardStowed[i])
            {
                card->setVisible(false);
                continue;
            }

            card->setTransform(juce::AffineTransform());
            card->setBounds(settledPadding,
                            shelfStartY + slot * (foldedCardH + shelfGap),
                            foldedCardW, foldedCardH);
            slot++;
        }
    }

    //==========================================================================
    // TRIGGER: Start the full animation sequence
    //==========================================================================
    void triggerAnimationSequence()
    {
        if (phase != AnimPhase::compact) return;

        auto* topLevel = getTopLevelComponent();
        if (!topLevel)
        {
            // Safety: if window not available, force Nono's orbit to cancel
            // so the user can click again and retry
            holoNono->cancelOrbit();
            return;
        }

        // Record Nono's screen position BEFORE expansion
        auto nonoScreenPos = holoNono->getScreenPosition();
        auto windowPos = topLevel->getScreenPosition();

        // Expand canvas
        phase = AnimPhase::orbFlyOut;
        topLevel->setSize(expandedW, expandedH);

        // Compensate window position to lock Nono in place
        auto newNonoScreenPos = holoNono->getScreenPosition();
        int dx = nonoScreenPos.x - newNonoScreenPos.x;
        int dy = nonoScreenPos.y - newNonoScreenPos.y;
        int newX = windowPos.x + dx;
        int newY = windowPos.y + dy;

        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            auto screen = display->userArea;
            newX = juce::jlimit(screen.getX(), screen.getRight()  - expandedW, newX);
            newY = juce::jlimit(screen.getY(), screen.getBottom() - expandedH, newY);
        }
        topLevel->setTopLeftPosition(newX, newY);

        // Reset all card animation state
        float centerX = static_cast<float>(nonoExpandedX) + nonoLocalCX;
        float centerY = static_cast<float>(nonoExpandedY) + nonoLocalCY;

        for (int i = 0; i < numCards; ++i)
        {
            cardFlyProgress[i] = 0.0f;
            cardLaunched[i] = false;

            auto* card = getCard(i);
            if (!card) continue;
            card->setExpanded(false, false);
            card->setTransform(juce::AffineTransform());
            card->setVisible(false);   // Cards hidden — only orbs visible during animation
            card->isDocked = true;     // Lock all interaction in shelf mode

            // Initialize orb trail to Nono center
            orbStates[i] = { centerX, centerY, getCardColour(i) };
            for (int j = 0; j < trailLen; ++j)
                orbTrail[i][j] = { centerX, centerY };
        }

        trailHead = 0;
        cardFadeInAlpha = 0.0f;
        animFrameCounter = 0;
        nextCardIndex = 0;
        wheelProgress = 0.0f;
    }

    //==========================================================================
    // PHASE 1: Orb fly-out — color orbs shoot out from Nono center
    //==========================================================================
    void tickOrbFlyOut()
    {
        animFrameCounter++;

        // Launch orbs sequentially every 2 frames (~33ms apart)
        if (animFrameCounter % 2 == 0 && nextCardIndex < numCards)
        {
            cardLaunched[nextCardIndex] = true;
            nextCardIndex++;
        }

        // Advance all launched orbs
        bool allDone = (nextCardIndex >= numCards);
        for (int i = 0; i < numCards; ++i)
        {
            if (!cardLaunched[i]) { allDone = false; continue; }
            if (cardFlyProgress[i] < 1.0f)
            {
                cardFlyProgress[i] = juce::jmin(1.0f, cardFlyProgress[i] + flySpeed);
                if (cardFlyProgress[i] < 1.0f) allDone = false;
            }
        }

        updateOrbPositions();
        repaint();

        if (allDone)
        {
            phase = AnimPhase::orbDwell;
            animFrameCounter = 0;
        }
    }

    //==========================================================================
    // PHASE 2: Orb dwell — hold circular array for ~0.33s (20 frames)
    //==========================================================================
    void tickOrbDwell()
    {
        animFrameCounter++;
        updateOrbPositions();  // keep writing trail (positions static → trail converges)
        repaint();

        if (animFrameCounter >= 20)
        {
            // ============================================================
            // Compute shelf target by "pre-rehearsing" layoutSettled delta.
            // ============================================================
            int totalShelfH = numCards * foldedCardH + (numCards - 1) * shelfGap;
            int settledFinalH = juce::jmax(totalShelfH + settledPadding * 2,
                                           compactH + settledPadding * 2);

            int settledNonoX = settledPadding + foldedCardW + settledGap;
            int settledNonoY = (settledFinalH - compactH) / 2;
            int settledCardX = settledPadding;
            int settledCardStartY = (settledFinalH - totalShelfH) / 2;

            int deltaX = settledCardX - settledNonoX;
            int deltaY = settledCardStartY - settledNonoY;

            shelfTargetX = static_cast<float>(nonoExpandedX + deltaX);
            shelfTargetStartY = static_cast<float>(nonoExpandedY + deltaY);

            phase = AnimPhase::orbWheelToShelf;
            animFrameCounter = 0;
            wheelProgress = 0.0f;
        }
    }

    //==========================================================================
    // PHASE 3: Orb wheel-to-shelf — spinning dissolution to bookshelf positions
    //==========================================================================
    void tickOrbWheelToShelf()
    {
        wheelProgress = juce::jmin(1.0f, wheelProgress + wheelSpeed);

        updateOrbPositions();
        repaint();

        if (wheelProgress >= 1.0f)
        {
            // Snap cards to exact shelf positions (still hidden)
            for (int i = 0; i < numCards; ++i)
            {
                auto* card = getCard(i);
                if (!card) continue;

                card->setTransform(juce::AffineTransform());
                int by = static_cast<int>(shelfTargetStartY) + i * (foldedCardH + shelfGap);
                card->setBounds(static_cast<int>(shelfTargetX), by, foldedCardW, foldedCardH);
            }

            phase = AnimPhase::cardFadeIn;
            cardFadeInAlpha = 0.0f;
            animFrameCounter = 0;
        }
    }

    //==========================================================================
    // Update orb positions based on current animation state
    // (stores coordinates in orbStates + writes trail ring buffer)
    //==========================================================================
    void updateOrbPositions()
    {
        float centerX = static_cast<float>(nonoExpandedX) + nonoLocalCX;
        float centerY = static_cast<float>(nonoExpandedY) + nonoLocalCY;

        for (int i = 0; i < numCards; ++i)
        {
            if (!cardLaunched[i]) continue;

            float orbitAngle = cardAngle(i);
            float cx, cy;

            if (phase == AnimPhase::orbFlyOut || phase == AnimPhase::orbDwell)
            {
                float t = easeOutCubic(cardFlyProgress[i]);
                float orbitCX = centerX + arcRadius * std::cos(orbitAngle);
                float orbitCY = centerY + arcRadius * std::sin(orbitAngle);
                cx = centerX + (orbitCX - centerX) * t;
                cy = centerY + (orbitCY - centerY) * t;
            }
            else if (phase == AnimPhase::orbWheelToShelf)
            {
                float t = easeInOutCubic(wheelProgress);
                float globalSpin = wheelProgress * wheelSpinTotal;
                float currOrbitAngle = orbitAngle + globalSpin;
                float orbitCX = centerX + arcRadius * std::cos(currOrbitAngle);
                float orbitCY = centerY + arcRadius * std::sin(currOrbitAngle);

                // Target: status dot center inside the card header
                // Card body offset from bounds = maxShadow(8) - restHover(4) = 4
                // Dot center X = bodyOffset + cardPadding(16) + dotDiameter(14)/2 = 27
                // Dot center Y = bodyOffset + headerHeight(48)/2 = 28
                static constexpr float dotOffsetX = 4.0f + 16.0f + 7.0f;  // 27
                static constexpr float dotOffsetY = 4.0f + 24.0f;         // 28

                float shelfCX = shelfTargetX + dotOffsetX;
                float shelfCY = shelfTargetStartY
                              + static_cast<float>(i) * (foldedCardH + shelfGap)
                              + dotOffsetY;

                cx = orbitCX + (shelfCX - orbitCX) * t;
                cy = orbitCY + (shelfCY - orbitCY) * t;
            }
            else continue;

            orbStates[i] = { cx, cy, getCardColour(i) };
            orbTrail[i][trailHead] = { cx, cy };
        }
        trailHead = (trailHead + 1) % trailLen;
    }

    //==========================================================================
    // Draw color orbs + comet trails (called from paint during orb + cardFadeIn phases)
    //==========================================================================
    void drawOrbs(juce::Graphics& g)
    {
        // Cross-dissolve: during cardFadeIn, orbs fade out as cards fade in
        const bool isCrossDissolve = (phase == AnimPhase::cardFadeIn);
        const float dissolveFactor = isCrossDissolve ? (1.0f - cardFadeInAlpha) : 1.0f;

        if (dissolveFactor <= 0.01f) return;  // fully dissolved, skip all drawing

        for (int i = 0; i < numCards; ++i)
        {
            if (!cardLaunched[i]) continue;

            auto colour = orbStates[i].colour;
            float ocx = orbStates[i].cx;
            float ocy = orbStates[i].cy;

            // 1. Comet trail — SKIP during cross-dissolve (orbs are static, trails are visual noise)
            if (!isCrossDissolve)
            {
                for (int j = 0; j < trailLen; ++j)
                {
                    int idx = (trailHead + j) % trailLen;  // oldest → newest
                    auto pos = orbTrail[i][idx];

                    float age = static_cast<float>(j) / static_cast<float>(trailLen);
                    float alpha = age * 0.4f;
                    float radius = orbRadius * (0.3f + 0.7f * age);

                    g.setColour(colour.withAlpha(alpha));
                    g.fillEllipse(pos.x - radius * 0.5f, pos.y - radius * 0.5f,
                                  radius, radius);
                }
            }

            // 2. Radial gradient glow (alpha modulated by dissolveFactor)
            {
                float glowR = orbRadius * 2.5f;
                juce::ColourGradient glow(
                    colour.withAlpha(0.20f * dissolveFactor), ocx, ocy,
                    juce::Colours::transparentBlack, ocx + glowR, ocy, true);
                g.setGradientFill(glow);
                g.fillEllipse(ocx - glowR, ocy - glowR, glowR * 2.0f, glowR * 2.0f);
            }

            // 3. Orb main body (alpha modulated by dissolveFactor)
            float orbAlpha = (phase == AnimPhase::orbFlyOut)
                ? juce::jmin(1.0f, easeOutCubic(cardFlyProgress[i]) * 2.5f)
                : 1.0f;
            orbAlpha *= dissolveFactor;

            GoodMeterLookAndFeel::drawStatusDot(
                g, ocx - orbRadius * 0.5f, ocy - orbRadius * 0.5f,
                orbRadius, colour.withAlpha(orbAlpha));
        }
    }

    //==========================================================================
    // PHASE 4: Card fade-in — cards appear at shelf positions after orbs land
    //==========================================================================
    void tickCardFadeIn()
    {
        // First frame: make cards visible at alpha 0 (orbs still at full opacity)
        // Subsequent frames: ramp alpha up while orbs dissolve out
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (!card) continue;

            if (!card->isVisible())
            {
                card->setVisible(true);
                card->setAlpha(0.0f);
            }
            card->setAlpha(cardFadeInAlpha);
        }

        repaint();

        // Increment AFTER applying — ensures first frame is exactly 0.0 / 1.0
        cardFadeInAlpha = juce::jmin(1.0f, cardFadeInAlpha + 1.0f / 20.0f);  // ~0.33s

        if (cardFadeInAlpha >= 1.0f)
        {
            phase = AnimPhase::canvasShrink;
            animFrameCounter = 0;
        }
    }

    //==========================================================================
    // PHASE 5: Canvas shrink — contract window to union bounds of all children
    //==========================================================================
    void tickCanvasShrink()
    {
        auto* topLevel = getTopLevelComponent();
        if (!topLevel) { phase = AnimPhase::settled; return; }

        // Record Nono screen position before shrink
        auto nonoScreenPos = holoNono->getScreenPosition();
        auto windowPos = topLevel->getScreenPosition();

        // Compute final window size from settled layout (visible cards only)
        int visibleCount = 0;
        for (int i = 0; i < numCards; ++i)
            if (!cardStowed[i]) visibleCount++;

        int totalShelfH = visibleCount * foldedCardH + juce::jmax(0, visibleCount - 1) * shelfGap;
        int finalH = juce::jmax(totalShelfH + settledPadding * 2,
                                compactH + settledPadding * 2);
        int finalW = (visibleCount > 0)
            ? settledPadding + foldedCardW + settledGap + compactW + settledPadding
            : settledPadding + compactW + settledPadding;  // no shelf → Nono only

        // Set phase BEFORE resize — resized() will call layoutSettled()
        phase = AnimPhase::settled;
        holoNono->useLocalDrag = false;
        topLevel->setSize(finalW, finalH);

        // Compensate position to keep Nono on screen
        auto newNonoScreenPos = holoNono->getScreenPosition();
        int ddx = nonoScreenPos.x - newNonoScreenPos.x;
        int ddy = nonoScreenPos.y - newNonoScreenPos.y;
        int nx = windowPos.x + ddx;
        int ny = windowPos.y + ddy;

        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            auto screen = display->userArea;
            nx = juce::jlimit(screen.getX(), screen.getRight()  - finalW, nx);
            ny = juce::jlimit(screen.getY(), screen.getBottom() - finalH, ny);
        }
        topLevel->setTopLeftPosition(nx, ny);
    }

    //==========================================================================
    // FLOATING PHASE: undocking, free-drag, layout
    //==========================================================================

    int findCardIndex(MeterCardComponent* card) const
    {
        for (int i = 0; i < numCards; ++i)
            if (getCard(i) == card) return i;
        return -1;
    }

    //--------------------------------------------------------------------------
    // Enter floating phase: expand window to full primary display
    //--------------------------------------------------------------------------
    void enterFloatingPhase(int undockingCardIndex, const juce::MouseEvent& event)
    {
        auto* topLevel = getTopLevelComponent();
        if (!topLevel) return;

        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (!display) return;
        auto screenArea = display->userArea;

        // Record screen positions of Nono and all cards before expansion
        auto nonoScreenPos = holoNono->getScreenPosition();
        juce::Point<int> cardScreenPositions[numCards];
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (card && card->isVisible())
                cardScreenPositions[i] = card->getScreenPosition();
        }

        // Compute Nono's position in the new full-screen canvas
        nonoFloatingX = nonoScreenPos.x - screenArea.getX();
        nonoFloatingY = nonoScreenPos.y - screenArea.getY();

        // Switch phase BEFORE resize so resized() calls layoutFloating()
        phase = AnimPhase::floating;
        holoNono->useLocalDrag = true;

        // Expand to full screen
        topLevel->setTopLeftPosition(screenArea.getX(), screenArea.getY());
        topLevel->setSize(screenArea.getWidth(), screenArea.getHeight());

        // Undock the specific card
        undockCard(undockingCardIndex);

        // Position the undocked card at its pre-expansion screen location
        auto* card = getCard(undockingCardIndex);
        if (card)
        {
            int cardX = cardScreenPositions[undockingCardIndex].x - screenArea.getX();
            int cardY = cardScreenPositions[undockingCardIndex].y - screenArea.getY();
            card->setBounds(cardX, cardY, foldedCardW, foldedCardH);
        }

        // Record drag offset (mouse position relative to card top-left)
        auto screenPos = event.getScreenPosition();
        floatingDragOffset = screenPos - cardScreenPositions[undockingCardIndex];
        floatingDragCardIndex = undockingCardIndex;
    }

    //--------------------------------------------------------------------------
    // Undock a single card (set flags, no layout change)
    //--------------------------------------------------------------------------
    void undockCard(int cardIndex)
    {
        auto* card = getCard(cardIndex);
        if (!card) return;

        card->isDocked = false;
        cardFloatState[cardIndex].isFloating = true;
    }

    //--------------------------------------------------------------------------
    // Layout for floating phase: Nono stays put, docked cards stack as shelf
    //--------------------------------------------------------------------------
    void layoutFloating()
    {
        // Nono at stored floating position
        holoNono->setBounds(nonoFloatingX, nonoFloatingY, compactW, compactH);

        // Count remaining docked cards (exclude stowed)
        int dockedCount = 0;
        for (int i = 0; i < numCards; ++i)
            if (!cardFloatState[i].isFloating && !cardStowed[i]) dockedCount++;

        // Stack docked cards to the left of Nono (same relative layout as settled)
        int shelfX = nonoFloatingX - settledGap - foldedCardW;
        int totalShelfH = dockedCount > 0
            ? dockedCount * foldedCardH + (dockedCount - 1) * shelfGap
            : 0;
        int shelfStartY = nonoFloatingY + (compactH - totalShelfH) / 2;

        int slot = 0;
        for (int i = 0; i < numCards; ++i)
        {
            if (cardFloatState[i].isFloating || cardStowed[i]) continue;
            auto* card = getCard(i);
            if (!card) continue;
            card->setTransform(juce::AffineTransform());
            card->setBounds(shelfX, shelfStartY + slot * (foldedCardH + shelfGap),
                            foldedCardW, foldedCardH);
            slot++;
        }

        // Floating cards: don't reposition (positions managed by drag logic)
    }

    //--------------------------------------------------------------------------
    // Drag handlers (called from MeterCardComponent callbacks)
    //--------------------------------------------------------------------------
    void handleUndockDragStarted(MeterCardComponent* card, const juce::MouseEvent& event)
    {
        int cardIndex = findCardIndex(card);
        if (cardIndex < 0) return;

        if (phase == AnimPhase::settled)
        {
            // First undock: expand to full screen
            enterFloatingPhase(cardIndex, event);
        }
        else if (phase == AnimPhase::floating)
        {
            // Set up drag tracking (works for both newly undocked AND re-dragged cards)
            auto screenPos = event.getScreenPosition();
            auto cardScreenPos = card->getScreenPosition();
            floatingDragOffset = screenPos - cardScreenPos;
            floatingDragCardIndex = cardIndex;

            // If this card is still docked, undock it and relayout
            if (!cardFloatState[cardIndex].isFloating)
            {
                undockCard(cardIndex);
                layoutFloating();
            }
        }
    }

    void handleFloatingDragging(MeterCardComponent* card, const juce::MouseEvent& event)
    {
        juce::ignoreUnused(card);
        if (floatingDragCardIndex < 0) return;

        auto* dragCard = getCard(floatingDragCardIndex);
        if (!dragCard) return;

        // Convert screen mouse position to local editor coords
        auto screenPos = event.getScreenPosition();
        auto localPos = getLocalPoint(nullptr, screenPos);

        int newX = localPos.x - floatingDragOffset.x;
        int newY = localPos.y - floatingDragOffset.y;

        // If card is in a group, use stored offsets for rigid body positioning.
        // This is more robust than per-frame delta propagation, which can drift
        // if any other code (hover animation, height relayout) modifies positions
        // between frames.
        int groupID = cardFloatState[floatingDragCardIndex].snapGroupID;
        if (groupID >= 0)
        {
            for (auto& group : snapGroups)
            {
                if (group.groupID != groupID) continue;

                // Compute anchor position from drag card's desired position
                auto dragOffsetIt = group.offsets.find(floatingDragCardIndex);
                if (dragOffsetIt == group.offsets.end()) break;

                int anchorX = newX - dragOffsetIt->second.x;
                int anchorY = newY - dragOffsetIt->second.y;

                // Position ALL members at anchor + their stored offset
                for (int memberIdx : group.members)
                {
                    auto* mc = getCard(memberIdx);
                    if (!mc) continue;
                    auto it = group.offsets.find(memberIdx);
                    if (it == group.offsets.end()) continue;
                    mc->setTopLeftPosition(anchorX + it->second.x, anchorY + it->second.y);
                }
                break;
            }
        }
        else
        {
            dragCard->setTopLeftPosition(newX, newY);
        }

        // Snap detection
        pendingSnap = findBestSnap(floatingDragCardIndex);

        // Real-time snap preview: if snap detected, move card (and group) to
        // snap position NOW. This eliminates the jarring teleport on mouseUp.
        // When snap loses range next frame, card returns to mouse position naturally.
        if (pendingSnap.valid)
        {
            auto* dc = getCard(floatingDragCardIndex);
            if (dc)
            {
                int gid = cardFloatState[floatingDragCardIndex].snapGroupID;
                if (gid >= 0)
                {
                    for (auto& group : snapGroups)
                    {
                        if (group.groupID != gid) continue;
                        for (int memberIdx : group.members)
                        {
                            auto* mc = getCard(memberIdx);
                            if (mc) mc->setTopLeftPosition(
                                mc->getX() + pendingSnap.snapDelta.x,
                                mc->getY() + pendingSnap.snapDelta.y);
                        }
                        break;
                    }
                }
                else
                {
                    dc->setTopLeftPosition(
                        dc->getX() + pendingSnap.snapDelta.x,
                        dc->getY() + pendingSnap.snapDelta.y);
                }
            }
        }

        repaint();
    }

    void handleFloatingDragEnded(MeterCardComponent* card, const juce::MouseEvent& event)
    {
        juce::ignoreUnused(card, event);

        // Commit snap if pending (with expansion collision check)
        if (pendingSnap.valid && floatingDragCardIndex >= 0)
        {
            // Check if snap would cause collision when cards expand
            auto overlapRect = checkExpansionCollision(pendingSnap);
            if (!overlapRect.isEmpty())
            {
                // Reject snap — trigger red flash warning
                collisionWarning.active = true;
                collisionWarning.alpha = 1.0f;
                collisionWarning.overlapRect = overlapRect.toFloat();
                pendingSnap.valid = false;
            }
            else
            {
                int dragIdx = floatingDragCardIndex;
                // skipMove=true: position already applied by real-time snap preview
                commitSnap(pendingSnap, true);

                // ============================================================
                // Two-step L-shape snap: after first snap commits, the card
                // is now in a group. Check if a SECOND snap edge exists
                // (e.g. snapped right edge, now also close to a bottom edge).
                // This allows forming 2×2 rectangle groups in one drag.
                // allowSameGroup=true so we can find edges within the group.
                // ============================================================
                auto secondSnap = findBestSnap(dragIdx, true);
                if (secondSnap.valid)
                {
                    auto secondOverlap = checkExpansionCollision(secondSnap);
                    if (secondOverlap.isEmpty())
                        commitSnap(secondSnap, false);  // 2nd snap was NOT previewed
                }
            }
        }

        // Recalculate group offsets after drag (ensures consistency even without snap)
        if (floatingDragCardIndex >= 0)
        {
            int gid = cardFloatState[floatingDragCardIndex].snapGroupID;
            if (gid >= 0)
                recalcGroupOffsets(gid);
        }

        // Alignment Divorce: detach any members that lost alignment during drag
        validateGroupAlignments();

        pendingSnap.valid = false;
        floatingDragCardIndex = -1;
        repaint();
    }

    //==========================================================================
    // SNAP ENGINE: detection, commit, group management
    //==========================================================================

    /** Get the visual card rectangle (excluding 8px shadow) */
    juce::Rectangle<int> getCardVisualRect(int cardIndex) const
    {
        auto* card = getCard(cardIndex);
        if (!card) return {};
        auto b = card->getBounds();
        return juce::Rectangle<int>(b.getX() + 8, b.getY() + 8,
                                     b.getWidth() - 8, b.getHeight() - 8);
    }

    /** Full-volume overlap check: given a drag card index and its would-be
     *  visual rect after snap delta, check if it intersects ANY other visible
     *  floating card on screen (regardless of group ID).
     *  Handles group drag: all group members shift by snapDelta.
     *  targetIdx is excluded from collision checks (it's the snap partner).
     *  Returns true if there's an overlap → snap should be rejected. */
    bool wouldSnapCauseOverlap(int dragIdx, int targetIdx, juce::Point<int> snapDelta) const
    {
        // Collect all cards that are MOVING (drag card + its group members)
        int movingCards[numCards];
        int numMoving = 0;
        int dragGroup = cardFloatState[dragIdx].snapGroupID;

        if (dragGroup >= 0)
        {
            for (const auto& g : snapGroups)
            {
                if (g.groupID != dragGroup) continue;
                for (int idx : g.members)
                    movingCards[numMoving++] = idx;
                break;
            }
        }
        else
        {
            movingCards[numMoving++] = dragIdx;
        }

        // For each moving card, check post-snap rect against all non-moving visible cards
        for (int m = 0; m < numMoving; ++m)
        {
            int movIdx = movingCards[m];
            auto movRect = getCardVisualRect(movIdx);
            if (movRect.isEmpty()) continue;
            auto postSnapRect = movRect.translated(snapDelta.x, snapDelta.y);

            for (int i = 0; i < numCards; ++i)
            {
                // Skip self
                if (i == movIdx) continue;
                // Skip the snap target (edge-to-edge is expected)
                if (i == targetIdx) continue;
                // Skip other moving cards (they move together)
                bool isMoving = false;
                for (int j = 0; j < numMoving; ++j)
                    if (movingCards[j] == i) { isMoving = true; break; }
                if (isMoving) continue;

                if (cardStowed[i]) continue;
                auto* card = getCard(i);
                if (!card || !card->isVisible()) continue;
                if (!cardFloatState[i].isFloating && phase == AnimPhase::floating) continue;

                auto otherRect = getCardVisualRect(i);
                if (otherRect.isEmpty()) continue;

                if (postSnapRect.intersects(otherRect))
                    return true;  // overlap detected → reject snap
            }
        }

        return false;  // no overlap
    }

    /** Compute expanded visual rect for a card (simulating full expansion).
     *  Uses preferredContentHeight if set, else customContentHeight, else 150 fallback. */
    juce::Rectangle<int> getExpandedVisualRect(int cardIndex) const
    {
        auto* card = getCard(cardIndex);
        if (!card) return {};
        auto b = card->getBounds();

        // Expanded height: priority chain matches MeterCardComponent::getDesiredHeight()
        int headerH = card->isMiniMode ? 24 : 48;
        int contentH;
        if (card->customContentHeight > 0)
            contentH = card->customContentHeight;
        else if (card->preferredContentHeight > 0)
            contentH = card->preferredContentHeight;
        else
            contentH = 150;  // fallback
        int pad = card->isMiniMode ? 2 : 16;
        int expandedH = headerH + contentH + pad * 2 + 8;  // +8 for shadow

        // Width stays the same (or use customWidth if set)
        int expandedW = b.getWidth();

        // Visual rect excludes shadow: offset by +8, size minus 8
        return juce::Rectangle<int>(b.getX() + 8, b.getY() + 8,
                                     expandedW - 8, expandedH - 8);
    }

    /** Check if committing a snap would cause collision when cards expand.
     *  Simulates all cards in the would-be group at expanded size,
     *  propagating vertical relayout, then checks:
     *  1. Intra-group: any pair of expanded group members still overlap after relayout
     *  2. Extra-group: expanded group union vs all non-group floating cards
     *  Returns the overlap rect if collision found, or empty rect if safe. */
    juce::Rectangle<int> checkExpansionCollision(const PendingSnap& snap) const
    {
        // Determine the would-be group members after this snap
        int dragGroup = cardFloatState[snap.dragCardIndex].snapGroupID;
        int targetGroup = cardFloatState[snap.targetCardIndex].snapGroupID;

        // Collect all members of the would-be merged group
        int groupMembers[numCards];
        int numMembers = 0;

        // Add drag card's group (or just drag card)
        if (dragGroup >= 0)
        {
            for (const auto& g : snapGroups)
            {
                if (g.groupID != dragGroup) continue;
                for (int idx : g.members)
                    groupMembers[numMembers++] = idx;
                break;
            }
        }
        else
        {
            groupMembers[numMembers++] = snap.dragCardIndex;
        }

        // Add target card's group (or just target card)
        if (targetGroup >= 0 && targetGroup != dragGroup)
        {
            for (const auto& g : snapGroups)
            {
                if (g.groupID != targetGroup) continue;
                for (int idx : g.members)
                    groupMembers[numMembers++] = idx;
                break;
            }
        }
        else if (targetGroup < 0)
        {
            // Only add if not already in the list
            bool already = false;
            for (int j = 0; j < numMembers; ++j)
                if (groupMembers[j] == snap.targetCardIndex) { already = true; break; }
            if (!already)
                groupMembers[numMembers++] = snap.targetCardIndex;
        }

        // Compute expanded visual rects for all group members
        // (after snap delta is applied to drag card's group)
        juce::Rectangle<int> expandedRects[numCards];
        for (int j = 0; j < numMembers; ++j)
        {
            int idx = groupMembers[j];
            expandedRects[j] = getExpandedVisualRect(idx);

            // Apply snap delta to drag card (and its group members)
            bool inDragGroup = (dragGroup >= 0 && cardFloatState[idx].snapGroupID == dragGroup)
                               || idx == snap.dragCardIndex;
            if (inDragGroup)
                expandedRects[j] = expandedRects[j].translated(snap.snapDelta.x, snap.snapDelta.y);
        }

        // Simulate vertical relayout: for stacked cards, push lower ones down
        for (int pass = 0; pass < numMembers; ++pass)
        {
            for (int a = 0; a < numMembers; ++a)
            {
                for (int b = a + 1; b < numMembers; ++b)
                {
                    auto& ra = expandedRects[a];
                    auto& rb = expandedRects[b];
                    auto intersection = ra.getIntersection(rb);
                    if (!intersection.isEmpty())
                    {
                        // Push the lower one down
                        if (ra.getY() <= rb.getY())
                            rb = rb.translated(0, intersection.getHeight());
                        else
                            ra = ra.translated(0, intersection.getHeight());
                    }
                }
            }
        }

        // ── Intra-group check: after relayout, verify no members still overlap ──
        for (int a = 0; a < numMembers; ++a)
        {
            for (int b = a + 1; b < numMembers; ++b)
            {
                auto overlap = expandedRects[a].getIntersection(expandedRects[b]);
                if (!overlap.isEmpty())
                    return overlap;  // intra-group collision after expansion
            }
        }

        // Compute union of all expanded group rects
        juce::Rectangle<int> groupUnion;
        for (int j = 0; j < numMembers; ++j)
        {
            if (j == 0)
                groupUnion = expandedRects[j];
            else
                groupUnion = groupUnion.getUnion(expandedRects[j]);
        }

        // ── Extra-group check: against all non-group floating cards ──
        for (int i = 0; i < numCards; ++i)
        {
            // Skip group members
            bool isMember = false;
            for (int j = 0; j < numMembers; ++j)
                if (groupMembers[j] == i) { isMember = true; break; }
            if (isMember) continue;

            if (!cardFloatState[i].isFloating) continue;
            if (cardStowed[i]) continue;
            auto* card = getCard(i);
            if (!card || !card->isVisible()) continue;

            auto cardRect = getCardVisualRect(i);
            if (cardRect.isEmpty()) continue;

            auto overlap = groupUnion.getIntersection(cardRect);
            if (!overlap.isEmpty())
                return overlap;  // collision found
        }

        return {};  // no collision
    }

    /** Find best snap candidate for dragged card.
     *  allowSameGroup: when true, also checks edges against same-group members
     *  (used for L-shape second-pass snap after first commitSnap). */
    PendingSnap findBestSnap(int dragIdx, bool allowSameGroup = false)
    {
        PendingSnap best;
        float bestDist = static_cast<float>(snapThreshold + 1);
        auto dragRect = getCardVisualRect(dragIdx);
        if (dragRect.isEmpty()) return best;

        for (int i = 0; i < numCards; ++i)
        {
            if (i == dragIdx) continue;
            if (!cardFloatState[i].isFloating) continue;
            auto* target = getCard(i);
            if (!target || !target->isVisible()) continue;

            int dragGroup = cardFloatState[dragIdx].snapGroupID;
            int targetGroup = cardFloatState[i].snapGroupID;
            if (!allowSameGroup && dragGroup >= 0 && dragGroup == targetGroup) continue;

            // For same-group second-pass: skip cards that already have a
            // direct edge relation with the drag card (already snapped)
            if (allowSameGroup && dragGroup >= 0 && dragGroup == targetGroup)
            {
                bool alreadyRelated = false;
                for (auto& group : snapGroups)
                {
                    if (group.groupID != dragGroup) continue;
                    for (const auto& rel : group.edgeRelations)
                    {
                        if ((rel.cardA == dragIdx && rel.cardB == i) ||
                            (rel.cardA == i && rel.cardB == dragIdx))
                        {
                            alreadyRelated = true;
                            break;
                        }
                    }
                    break;
                }
                if (alreadyRelated) continue;
            }

            auto tRect = getCardVisualRect(i);
            if (tRect.isEmpty()) continue;

            struct EdgeCandidate
            {
                float dist;
                juce::Point<int> delta;
                bool vertical;
                juce::Rectangle<float> dragEdge;
                juce::Rectangle<float> targetEdge;
            };

            std::vector<EdgeCandidate> candidates;

            // Strict alignment: top/left edges must always be flush.
            // Horizontal snap (side-by-side) → top edges flush.
            // Vertical snap (stacking) → left edges flush.
            auto nearEdgeAlignY = [&]() -> int
            {
                return tRect.getY() - dragRect.getY();  // force top-edge flush
            };

            auto nearEdgeAlignX = [&]() -> int
            {
                return tRect.getX() - dragRect.getX();  // force left-edge flush
            };

            // Helper: build edge rects covering union of both cards (after snap)
            auto buildHorizEdgeRects = [](const juce::Rectangle<int>& dR, const juce::Rectangle<int>& tR,
                                           int dx, int dy, bool dragIsRight)
            {
                // After snap, drag card shifts by (dx, dy)
                auto adjDrag = dR.translated(dx, dy);
                float unionTop = static_cast<float>(juce::jmin(adjDrag.getY(), tR.getY()));
                float unionBot = static_cast<float>(juce::jmax(adjDrag.getBottom(), tR.getBottom()));
                float h = unionBot - unionTop;

                juce::Rectangle<float> de, te;
                if (dragIsRight)
                {
                    // drag.right meets target.left
                    float snapX = static_cast<float>(tR.getX());
                    de = { snapX - 1.0f, unionTop, 2.0f, h };
                    te = { snapX - 1.0f, unionTop, 2.0f, h };
                }
                else
                {
                    // drag.left meets target.right
                    float snapX = static_cast<float>(tR.getRight());
                    de = { snapX - 1.0f, unionTop, 2.0f, h };
                    te = { snapX - 1.0f, unionTop, 2.0f, h };
                }
                return std::make_pair(de, te);
            };

            auto buildVertEdgeRects = [](const juce::Rectangle<int>& dR, const juce::Rectangle<int>& tR,
                                          int dx, int dy, bool dragIsBelow)
            {
                auto adjDrag = dR.translated(dx, dy);
                float unionLeft = static_cast<float>(juce::jmin(adjDrag.getX(), tR.getX()));
                float unionRight = static_cast<float>(juce::jmax(adjDrag.getRight(), tR.getRight()));
                float w = unionRight - unionLeft;

                juce::Rectangle<float> de, te;
                if (dragIsBelow)
                {
                    float snapY = static_cast<float>(tR.getBottom());
                    de = { unionLeft, snapY - 1.0f, w, 2.0f };
                    te = { unionLeft, snapY - 1.0f, w, 2.0f };
                }
                else
                {
                    float snapY = static_cast<float>(tR.getY());
                    de = { unionLeft, snapY - 1.0f, w, 2.0f };
                    te = { unionLeft, snapY - 1.0f, w, 2.0f };
                }
                return std::make_pair(de, te);
            };

            // A.right ↔ B.left (drag's right edge meets target's left edge)
            {
                int gap = tRect.getX() - dragRect.getRight();
                int overlapY = juce::jmin(dragRect.getBottom(), tRect.getBottom())
                             - juce::jmax(dragRect.getY(), tRect.getY());
                if (std::abs(gap) <= snapThreshold && overlapY >= snapOverlapMin)
                {
                    int dy = nearEdgeAlignY();
                    // Full-volume collision check: would post-snap position overlap anyone?
                    if (!wouldSnapCauseOverlap(dragIdx, i, { gap, dy }))
                    {
                        auto [de, te] = buildHorizEdgeRects(dragRect, tRect, gap, dy, true);
                        candidates.push_back({
                            static_cast<float>(std::abs(gap)), { gap, dy }, false, de, te
                        });
                    }
                }
            }

            // A.left ↔ B.right (drag's left edge meets target's right edge)
            {
                int gap = dragRect.getX() - tRect.getRight();
                int overlapY = juce::jmin(dragRect.getBottom(), tRect.getBottom())
                             - juce::jmax(dragRect.getY(), tRect.getY());
                if (std::abs(gap) <= snapThreshold && overlapY >= snapOverlapMin)
                {
                    int dy = nearEdgeAlignY();
                    // Full-volume collision check: would post-snap position overlap anyone?
                    if (!wouldSnapCauseOverlap(dragIdx, i, { -gap, dy }))
                    {
                        auto [de, te] = buildHorizEdgeRects(dragRect, tRect, -gap, dy, false);
                        candidates.push_back({
                            static_cast<float>(std::abs(gap)), { -gap, dy }, false, de, te
                        });
                    }
                }
            }

            // A.bottom ↔ B.top (drag's bottom edge meets target's top edge)
            {
                int gap = tRect.getY() - dragRect.getBottom();
                int overlapX = juce::jmin(dragRect.getRight(), tRect.getRight())
                             - juce::jmax(dragRect.getX(), tRect.getX());
                if (std::abs(gap) <= snapThreshold && overlapX >= snapOverlapMin)
                {
                    int dx = nearEdgeAlignX();
                    // Full-volume collision check: would post-snap position overlap anyone?
                    if (!wouldSnapCauseOverlap(dragIdx, i, { dx, gap }))
                    {
                        auto [de, te] = buildVertEdgeRects(dragRect, tRect, dx, gap, true);
                        candidates.push_back({
                            static_cast<float>(std::abs(gap)), { dx, gap }, true, de, te
                        });
                    }
                }
            }

            // A.top ↔ B.bottom (drag's top edge meets target's bottom edge)
            {
                int gap = dragRect.getY() - tRect.getBottom();
                int overlapX = juce::jmin(dragRect.getRight(), tRect.getRight())
                             - juce::jmax(dragRect.getX(), tRect.getX());
                if (std::abs(gap) <= snapThreshold && overlapX >= snapOverlapMin)
                {
                    int dx = nearEdgeAlignX();
                    // Full-volume collision check: would post-snap position overlap anyone?
                    if (!wouldSnapCauseOverlap(dragIdx, i, { dx, -gap }))
                    {
                        auto [de, te] = buildVertEdgeRects(dragRect, tRect, dx, -gap, false);
                        candidates.push_back({
                            static_cast<float>(std::abs(gap)), { dx, -gap }, true, de, te
                        });
                    }
                }
            }

            for (const auto& c : candidates)
            {
                if (c.dist < bestDist)
                {
                    bestDist = c.dist;
                    best.valid = true;
                    best.dragCardIndex = dragIdx;
                    best.targetCardIndex = i;
                    best.snapDelta = c.delta;
                    best.isVertical = c.vertical;
                    best.dragEdgeRect = c.dragEdge;
                    best.targetEdgeRect = c.targetEdge;
                }
            }
        }

        return best;
    }

    /** Commit a snap: move card into position and form/merge groups.
     *  skipMove: if true, skip the position adjustment (already done by real-time preview). */
    void commitSnap(const PendingSnap& snap, bool skipMove = false)
    {
        auto* dragCard = getCard(snap.dragCardIndex);
        if (!dragCard) return;

        // Move the drag card (and its group) by snapDelta
        if (!skipMove)
        {
            int groupID = cardFloatState[snap.dragCardIndex].snapGroupID;
            if (groupID >= 0)
            {
                for (auto& group : snapGroups)
                {
                    if (group.groupID != groupID) continue;
                    for (int memberIdx : group.members)
                    {
                        auto* mc = getCard(memberIdx);
                        if (mc) mc->setTopLeftPosition(mc->getX() + snap.snapDelta.x,
                                                        mc->getY() + snap.snapDelta.y);
                    }
                    break;
                }
            }
            else
            {
                dragCard->setTopLeftPosition(dragCard->getX() + snap.snapDelta.x,
                                              dragCard->getY() + snap.snapDelta.y);
            }
        }

        // Form or merge groups
        int dragGroupID = cardFloatState[snap.dragCardIndex].snapGroupID;
        int targetGroupID = cardFloatState[snap.targetCardIndex].snapGroupID;

        if (dragGroupID < 0 && targetGroupID < 0)
        {
            // Neither has a group → create new
            SnapGroup newGroup;
            newGroup.groupID = nextGroupID++;
            newGroup.members.push_back(snap.targetCardIndex);
            newGroup.members.push_back(snap.dragCardIndex);
            snapGroups.push_back(newGroup);
            cardFloatState[snap.targetCardIndex].snapGroupID = newGroup.groupID;
            cardFloatState[snap.dragCardIndex].snapGroupID = newGroup.groupID;

            updateShowDetachButtons(newGroup.groupID);
        }
        else if (dragGroupID < 0)
        {
            // Target has a group, add drag card to it
            for (auto& group : snapGroups)
            {
                if (group.groupID != targetGroupID) continue;
                group.members.push_back(snap.dragCardIndex);
                cardFloatState[snap.dragCardIndex].snapGroupID = targetGroupID;
                updateShowDetachButtons(targetGroupID);
                break;
            }
        }
        else if (targetGroupID < 0)
        {
            // Drag has a group, add target to it
            for (auto& group : snapGroups)
            {
                if (group.groupID != dragGroupID) continue;
                group.members.push_back(snap.targetCardIndex);
                cardFloatState[snap.targetCardIndex].snapGroupID = dragGroupID;
                updateShowDetachButtons(dragGroupID);
                break;
            }
        }
        else if (dragGroupID != targetGroupID)
        {
            // Both have groups → merge target's group into drag's group
            SnapGroup* dragGroup = nullptr;
            SnapGroup* targetGroup = nullptr;
            for (auto& group : snapGroups)
            {
                if (group.groupID == dragGroupID) dragGroup = &group;
                if (group.groupID == targetGroupID) targetGroup = &group;
            }
            if (dragGroup && targetGroup)
            {
                for (int idx : targetGroup->members)
                {
                    dragGroup->members.push_back(idx);
                    cardFloatState[idx].snapGroupID = dragGroupID;
                }
                // Preserve edge relations from target group (prevents broken chains)
                for (const auto& rel : targetGroup->edgeRelations)
                    dragGroup->edgeRelations.push_back(rel);
                // Remove the target group
                snapGroups.erase(std::remove_if(snapGroups.begin(), snapGroups.end(),
                    [targetGroupID](const SnapGroup& g) { return g.groupID == targetGroupID; }),
                    snapGroups.end());
                updateShowDetachButtons(dragGroupID);
            }
        }

        // Record edge relation for this snap
        {
            int finalGroupID = cardFloatState[snap.dragCardIndex].snapGroupID;
            for (auto& group : snapGroups)
            {
                if (group.groupID != finalGroupID) continue;
                EdgeRelation rel;
                if (snap.isVertical)
                {
                    // Vertical snap: determine which is on top
                    auto dragPos = getCard(snap.dragCardIndex)->getPosition();
                    auto targetPos = getCard(snap.targetCardIndex)->getPosition();
                    if (dragPos.y < targetPos.y)
                    {
                        rel.cardA = snap.dragCardIndex;   // drag is above
                        rel.cardB = snap.targetCardIndex;
                    }
                    else
                    {
                        rel.cardA = snap.targetCardIndex;  // target is above
                        rel.cardB = snap.dragCardIndex;
                    }
                }
                else
                {
                    // Horizontal snap: determine which is on left
                    auto dragPos = getCard(snap.dragCardIndex)->getPosition();
                    auto targetPos = getCard(snap.targetCardIndex)->getPosition();
                    if (dragPos.x < targetPos.x)
                    {
                        rel.cardA = snap.dragCardIndex;
                        rel.cardB = snap.targetCardIndex;
                    }
                    else
                    {
                        rel.cardA = snap.targetCardIndex;
                        rel.cardB = snap.dragCardIndex;
                    }
                }
                rel.isVertical = snap.isVertical;
                group.edgeRelations.push_back(rel);
                break;
            }
        }

        // Recalculate offsets for the group
        int finalGroupID = cardFloatState[snap.dragCardIndex].snapGroupID;

        // ============================================================
        // Contagion auto-match: on vertical snap, widen the narrower
        // card to match the wider one's width. Only applies when both
        // cards are expanded (floating) and the snap is vertical.
        // ============================================================
        if (snap.isVertical)
        {
            auto* cardA = getCard(snap.dragCardIndex);
            auto* cardB = getCard(snap.targetCardIndex);
            if (cardA && cardB && cardA->getExpanded() && cardB->getExpanded())
            {
                int wA = cardA->getWidth();
                int wB = cardB->getWidth();
                if (wA != wB)
                {
                    int wider = juce::jmax(wA, wB);
                    auto* narrowCard = (wA < wB) ? cardA : cardB;
                    narrowCard->setSize(wider, narrowCard->getHeight());
                    narrowCard->customWidth = wider - 8;  // store without shadow
                    narrowCard->resized();
                    narrowCard->repaint();
                }
            }
        }

        recalcGroupOffsets(finalGroupID);
    }

    /** Recalculate offsets in a group relative to anchor (first member) */
    void recalcGroupOffsets(int groupID)
    {
        for (auto& group : snapGroups)
        {
            if (group.groupID != groupID) continue;
            if (group.members.empty()) return;

            auto* anchor = getCard(group.members[0]);
            if (!anchor) return;
            auto anchorPos = anchor->getPosition();

            group.offsets.clear();
            for (int idx : group.members)
            {
                auto* mc = getCard(idx);
                if (mc) group.offsets[idx] = mc->getPosition() - anchorPos;
            }
            break;
        }
    }

    /** Update showDetachButton for all cards in a group */
    void updateShowDetachButtons(int groupID)
    {
        for (auto& group : snapGroups)
        {
            if (group.groupID != groupID) continue;
            bool show = group.members.size() > 1;
            for (int idx : group.members)
            {
                auto* mc = getCard(idx);
                if (mc) mc->showDetachButton = show;
            }
            break;
        }
    }

    /** Detach a card from its group — with BFS Connected Components split.
     *
     *  After removing the card and its edge relations, the remaining members
     *  may form multiple disconnected subgraphs (e.g. A-B-C vertical stack,
     *  detach B → A and C are no longer connected).
     *
     *  BFS finds all connected components. The first component keeps the
     *  original group; additional components become new groups (or solo cards).
     *  recalcGroupOffsets is called for every resulting group.
     */
    void detachCardFromGroup(int cardIndex)
    {
        int groupID = cardFloatState[cardIndex].snapGroupID;
        if (groupID < 0) return;

        cardFloatState[cardIndex].snapGroupID = -1;
        auto* card = getCard(cardIndex);
        if (card) card->showDetachButton = false;

        for (auto it = snapGroups.begin(); it != snapGroups.end(); ++it)
        {
            if (it->groupID != groupID) continue;

            it->members.erase(
                std::remove(it->members.begin(), it->members.end(), cardIndex),
                it->members.end());

            // Remove edge relations involving this card
            it->edgeRelations.erase(
                std::remove_if(it->edgeRelations.begin(), it->edgeRelations.end(),
                    [cardIndex](const EdgeRelation& r) {
                        return r.cardA == cardIndex || r.cardB == cardIndex;
                    }),
                it->edgeRelations.end());

            if (it->members.empty())
            {
                snapGroups.erase(it);
            }
            else
            {
                splitGroupByConnectivity(it);
            }
            break;
        }

        // Nudge detached card 20px to the right for visual clarity
        if (card)
            card->setTopLeftPosition(card->getX() + 20, card->getY());
    }

    //==========================================================================
    // BFS Connected Components: shared by detachCardFromGroup and
    // validateGroupAlignments. Takes an iterator to a SnapGroup, runs BFS
    // on its members using edgeRelations as adjacency. Splits disconnected
    // subgraphs into separate groups (or releases solo cards).
    // May erase/modify the group pointed to by `it`.
    //==========================================================================
    void splitGroupByConnectivity(std::vector<SnapGroup>::iterator it)
    {
        int groupID = it->groupID;

        if (it->members.empty())
        {
            snapGroups.erase(it);
            return;
        }

        // BFS: componentOf[cardIdx] = component index, -1 = unvisited
        int componentOf[numCards];
        for (int i = 0; i < numCards; ++i) componentOf[i] = -1;

        std::vector<std::vector<int>> components;
        for (int seed : it->members)
        {
            if (componentOf[seed] >= 0) continue;

            int compIdx = static_cast<int>(components.size());
            components.push_back({});

            int queue[numCards];
            int qHead = 0, qTail = 0;
            queue[qTail++] = seed;
            componentOf[seed] = compIdx;

            while (qHead < qTail)
            {
                int cur = queue[qHead++];
                components[compIdx].push_back(cur);

                for (const auto& rel : it->edgeRelations)
                {
                    int neighbor = -1;
                    if (rel.cardA == cur) neighbor = rel.cardB;
                    else if (rel.cardB == cur) neighbor = rel.cardA;

                    if (neighbor >= 0 && componentOf[neighbor] < 0)
                    {
                        componentOf[neighbor] = compIdx;
                        queue[qTail++] = neighbor;
                    }
                }
            }
        }

        // Apply split results
        if (components.size() <= 1)
        {
            if (it->members.size() <= 1)
            {
                for (int idx : it->members)
                {
                    cardFloatState[idx].snapGroupID = -1;
                    auto* mc = getCard(idx);
                    if (mc) mc->showDetachButton = false;
                }
                snapGroups.erase(it);
            }
            else
            {
                updateShowDetachButtons(groupID);
                recalcGroupOffsets(groupID);
            }
        }
        else
        {
            // Multiple components — split.
            auto allRelations = it->edgeRelations;

            auto relationsForComp = [&](const std::vector<int>& comp)
            {
                std::vector<EdgeRelation> result;
                for (const auto& rel : allRelations)
                {
                    bool aIn = std::find(comp.begin(), comp.end(), rel.cardA) != comp.end();
                    bool bIn = std::find(comp.begin(), comp.end(), rel.cardB) != comp.end();
                    if (aIn && bIn)
                        result.push_back(rel);
                }
                return result;
            };

            // Component 0 keeps original group
            auto& comp0 = components[0];
            it->members = comp0;
            it->edgeRelations = relationsForComp(comp0);

            if (comp0.size() <= 1)
            {
                for (int idx : comp0)
                {
                    cardFloatState[idx].snapGroupID = -1;
                    auto* mc = getCard(idx);
                    if (mc) mc->showDetachButton = false;
                }
                snapGroups.erase(it);
            }
            else
            {
                updateShowDetachButtons(groupID);
                recalcGroupOffsets(groupID);
            }

            // Remaining components become new groups
            for (size_t ci = 1; ci < components.size(); ++ci)
            {
                auto& comp = components[ci];

                if (comp.size() <= 1)
                {
                    for (int idx : comp)
                    {
                        cardFloatState[idx].snapGroupID = -1;
                        auto* mc = getCard(idx);
                        if (mc) mc->showDetachButton = false;
                    }
                }
                else
                {
                    SnapGroup newGroup;
                    newGroup.groupID = nextGroupID++;
                    newGroup.members = comp;
                    newGroup.edgeRelations = relationsForComp(comp);

                    snapGroups.push_back(newGroup);

                    for (int idx : comp)
                        cardFloatState[idx].snapGroupID = newGroup.groupID;

                    updateShowDetachButtons(newGroup.groupID);
                    recalcGroupOffsets(newGroup.groupID);
                }
            }
        }
    }

    //==========================================================================
    // Alignment Divorce: after relayout or collision resolution, check every
    // EdgeRelation in every group. If the alignment axis is off by > 2px,
    // remove that relation and split the group via BFS connectivity check.
    //==========================================================================
    void validateGroupAlignments()
    {
        if (phase != AnimPhase::floating) return;

        bool anyChanged = false;

        for (auto groupIt = snapGroups.begin(); groupIt != snapGroups.end(); )
        {
            bool relationsRemoved = false;

            groupIt->edgeRelations.erase(
                std::remove_if(groupIt->edgeRelations.begin(), groupIt->edgeRelations.end(),
                    [this, &relationsRemoved](const EdgeRelation& rel) -> bool
                    {
                        auto rectA = getCardVisualRect(rel.cardA);
                        auto rectB = getCardVisualRect(rel.cardB);
                        if (rectA.isEmpty() || rectB.isEmpty()) return false;

                        bool aligned;
                        if (rel.isVertical)
                            aligned = std::abs(rectA.getX() - rectB.getX()) <= 2;
                        else
                            aligned = std::abs(rectA.getY() - rectB.getY()) <= 2;

                        if (!aligned)
                        {
                            relationsRemoved = true;
                            return true;
                        }
                        return false;
                    }),
                groupIt->edgeRelations.end());

            if (!relationsRemoved)
            {
                ++groupIt;
                continue;
            }

            anyChanged = true;
            splitGroupByConnectivity(groupIt);
            break;  // snapGroups modified, restart iteration
        }

        // Recurse if any changes (handles cascading splits, max numCards depth)
        if (anyChanged)
            validateGroupAlignments();
    }

    //==========================================================================
    // Global collision resolution: push overlapping floating cards apart.
    //
    // Treats ALL card pairs as potential collisions EXCEPT pairs with a
    // direct edge relation (those are managed by relayoutGroupForCard).
    // Same-group cards without edge relations are treated as solid obstacles.
    //==========================================================================
    void resolveGlobalCollisions()
    {
        if (phase != AnimPhase::floating) return;

        // Up to 3 passes to resolve cascading pushes
        for (int pass = 0; pass < 3; ++pass)
        {
            bool anyPushed = false;

            for (int i = 0; i < numCards; ++i)
            {
                if (!cardFloatState[i].isFloating) continue;
                if (cardStowed[i]) continue;
                auto* cardA = getCard(i);
                if (!cardA || !cardA->isVisible()) continue;

                for (int j = i + 1; j < numCards; ++j)
                {
                    if (!cardFloatState[j].isFloating) continue;
                    if (cardStowed[j]) continue;
                    auto* cardB = getCard(j);
                    if (!cardB || !cardB->isVisible()) continue;

                    // Skip pairs with a direct edge relation (managed by relayout)
                    if (hasDirectEdgeRelation(i, j)) continue;

                    auto rectA = getCardVisualRect(i);
                    auto rectB = getCardVisualRect(j);
                    if (rectA.isEmpty() || rectB.isEmpty()) continue;

                    auto overlap = rectA.getIntersection(rectB);
                    if (overlap.isEmpty()) continue;

                    int groupA = cardFloatState[i].snapGroupID;
                    int groupB = cardFloatState[j].snapGroupID;

                    // Compute push direction: push the smaller/solo card away
                    // If same group, push the one that is NOT the source of the resize
                    // For simplicity: always push B (the higher-index card)
                    int pushX = 0, pushY = 0;
                    int overlapW = overlap.getWidth();
                    int overlapH = overlap.getHeight();

                    if (overlapW <= overlapH)
                    {
                        int centerA = rectA.getCentreX();
                        int centerB = rectB.getCentreX();
                        pushX = (centerB >= centerA) ? overlapW + 4 : -(overlapW + 4);
                    }
                    else
                    {
                        int centerA = rectA.getCentreY();
                        int centerB = rectB.getCentreY();
                        pushY = (centerB >= centerA) ? overlapH + 4 : -(overlapH + 4);
                    }

                    // Apply push: for same-group cards, only move the individual card;
                    // for different-group cards, move the entire group
                    if (groupA >= 0 && groupA == groupB)
                    {
                        // Intra-group non-related: push just card B individually
                        cardB->setTopLeftPosition(cardB->getX() + pushX, cardB->getY() + pushY);
                    }
                    else
                    {
                        int pushGroupID = cardFloatState[j].snapGroupID;
                        if (pushGroupID >= 0)
                        {
                            // Push entire group
                            for (auto& group : snapGroups)
                            {
                                if (group.groupID != pushGroupID) continue;
                                for (int memberIdx : group.members)
                                {
                                    auto* mc = getCard(memberIdx);
                                    if (mc) mc->setTopLeftPosition(mc->getX() + pushX, mc->getY() + pushY);
                                }
                                recalcGroupOffsets(pushGroupID);
                                break;
                            }
                        }
                        else
                        {
                            cardB->setTopLeftPosition(cardB->getX() + pushX, cardB->getY() + pushY);
                        }
                    }

                    anyPushed = true;
                }
            }

            if (!anyPushed) break;  // no more collisions
        }
    }

    /** Check if two cards have a direct edge relation in any group */
    bool hasDirectEdgeRelation(int cardA, int cardB) const
    {
        int groupA = cardFloatState[cardA].snapGroupID;
        int groupB = cardFloatState[cardB].snapGroupID;
        if (groupA < 0 || groupA != groupB) return false;

        for (const auto& group : snapGroups)
        {
            if (group.groupID != groupA) continue;
            for (const auto& rel : group.edgeRelations)
            {
                if ((rel.cardA == cardA && rel.cardB == cardB) ||
                    (rel.cardA == cardB && rel.cardB == cardA))
                    return true;
            }
            break;
        }
        return false;
    }

    //==========================================================================
    // Steel-plate height clamping: when a card expands, check if its new
    // visual rect overlaps any card below it (that isn't a direct edge
    // relation neighbor). If so, clamp the card's height to avoid overlap.
    // This makes cards "self-adapt" to tight spaces instead of pushing.
    //==========================================================================
    void clampCardHeightToAvoidCollision(int cardIndex)
    {
        auto* card = getCard(cardIndex);
        if (!card || !card->isVisible()) return;
        if (!card->getExpanded()) return;  // only clamp when expanding

        auto myRect = getCardVisualRect(cardIndex);
        if (myRect.isEmpty()) return;

        int minBottom = myRect.getBottom();  // current bottom edge (after expansion)

        for (int i = 0; i < numCards; ++i)
        {
            if (i == cardIndex) continue;
            if (cardStowed[i]) continue;
            if (!cardFloatState[i].isFloating) continue;
            auto* other = getCard(i);
            if (!other || !other->isVisible()) continue;

            // Skip cards with direct edge relation (they're managed by relayout)
            if (hasDirectEdgeRelation(cardIndex, i)) continue;

            auto otherRect = getCardVisualRect(i);
            if (otherRect.isEmpty()) continue;

            // Only check cards that are BELOW and laterally overlapping
            if (otherRect.getY() <= myRect.getY()) continue;  // not below
            int lateralOverlap = juce::jmin(myRect.getRight(), otherRect.getRight())
                               - juce::jmax(myRect.getX(), otherRect.getX());
            if (lateralOverlap <= 0) continue;  // no lateral overlap

            // If our expanded bottom extends past this card's top, clamp
            if (myRect.getBottom() > otherRect.getY())
            {
                minBottom = juce::jmin(minBottom, otherRect.getY());
            }
        }

        // If we need to clamp, reduce the card's height
        int clampedVisualBottom = minBottom;
        int clampedBoundsBottom = clampedVisualBottom;  // visual rect starts at bounds+8
        int clampedHeight = clampedBoundsBottom - card->getY();

        if (clampedHeight < card->getHeight() && clampedHeight > (card->isMiniMode ? 24 : 48))
        {
            // Clamp: set customContentHeight to limit expansion
            int headerH = card->isMiniMode ? 24 : 48;
            int pad = card->isMiniMode ? 2 : 16;
            int maxContentH = clampedHeight - headerH - pad * 2 - 8;  // -8 for shadow
            if (maxContentH > 0)
            {
                card->customContentHeight = maxContentH;
                int newH = headerH + maxContentH + pad * 2 + 8;
                card->setSize(card->getWidth(), newH);
                card->resized();
            }
        }
    }

    //==========================================================================
    // Group relayout: when a card's height changes, push neighbors accordingly
    //
    // Propagation: for every vertical relation where this card is cardA (above),
    // move cardB so its visual top meets cardA's visual bottom. Then recurse
    // on cardB to propagate down the chain.
    //
    // Uses a visited set to prevent infinite recursion in cyclic graphs.
    //==========================================================================
    void relayoutGroupForCard(int cardIndex)
    {
        bool visited[numCards] = {};
        relayoutGroupForCardImpl(cardIndex, visited);

        // After group relayout completes, resolve global collisions
        resolveGlobalCollisions();

        // Alignment Divorce: detach any members that lost alignment
        validateGroupAlignments();
    }

    void relayoutGroupForCardImpl(int cardIndex, bool* visited)
    {
        if (cardIndex < 0 || cardIndex >= numCards) return;
        if (visited[cardIndex]) return;
        visited[cardIndex] = true;

        int groupID = cardFloatState[cardIndex].snapGroupID;
        if (groupID < 0) return;

        for (auto& group : snapGroups)
        {
            if (group.groupID != groupID) continue;

            for (const auto& rel : group.edgeRelations)
            {
                // Single-direction only: cardA → cardB (top→bottom, left→right).
                // Card's top-left is anchored; only right/bottom edges move on
                // expand/collapse/resize, so only push cards on those sides.
                if (rel.cardA != cardIndex) continue;

                auto* cardB = getCard(rel.cardB);
                if (!cardB || visited[rel.cardB]) continue;

                auto rectA = getCardVisualRect(rel.cardA);
                auto rectB = getCardVisualRect(rel.cardB);
                if (rectA.isEmpty() || rectB.isEmpty()) continue;

                if (rel.isVertical)
                {
                    // cardB's visual top should align with cardA's visual bottom
                    int delta = rectA.getBottom() - rectB.getY();
                    if (delta != 0)
                        cardB->setTopLeftPosition(cardB->getX(), cardB->getY() + delta);
                }
                else
                {
                    // cardB's visual left should align with cardA's visual right
                    int delta = rectA.getRight() - rectB.getX();
                    if (delta != 0)
                        cardB->setTopLeftPosition(cardB->getX() + delta, cardB->getY());
                }

                // Recursively propagate to cardB's downstream neighbors
                relayoutGroupForCardImpl(rel.cardB, visited);
            }

            recalcGroupOffsets(groupID);
            break;
        }
    }

    //==========================================================================
    // SETTLED → FLOATING PROMOTION
    // Expands window to full screen and sets phase to floating, preserving
    // all card positions by converting from settled-relative to screen-relative.
    //==========================================================================
    void promoteSettledToFloating()
    {
        auto* topLevel = getTopLevelComponent();
        if (!topLevel) return;

        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (!display) return;
        auto screenArea = display->userArea;

        // Record screen positions before expansion
        auto nonoScreenPos = holoNono->getScreenPosition();
        juce::Point<int> cardScreenPositions[numCards];
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (card && card->isVisible())
                cardScreenPositions[i] = card->getScreenPosition();
        }

        // Compute Nono's floating position
        nonoFloatingX = nonoScreenPos.x - screenArea.getX();
        nonoFloatingY = nonoScreenPos.y - screenArea.getY();

        // Switch to floating phase BEFORE resize
        phase = AnimPhase::floating;
        holoNono->useLocalDrag = true;

        // Expand to full screen
        topLevel->setTopLeftPosition(screenArea.getX(), screenArea.getY());
        topLevel->setSize(screenArea.getWidth(), screenArea.getHeight());

        // Restore card positions in new coordinate space
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (!card || !card->isVisible()) continue;
            int cx = cardScreenPositions[i].x - screenArea.getX();
            int cy = cardScreenPositions[i].y - screenArea.getY();
            card->setTopLeftPosition(cx, cy);
        }
    }

    //==========================================================================
    // SETTLED RECALL: Nono click in settled phase → promote + recall
    // If stowed cards exist, recall them. Otherwise, collapse back to compact.
    //==========================================================================
    void triggerSettledRecall()
    {
        // Check if any cards are stowed
        int stowedCount = 0;
        for (int i = 0; i < numCards; ++i)
            if (cardStowed[i]) stowedCount++;

        if (stowedCount > 0)
        {
            // Recall stowed cards: promote to floating → unstow → recall anim
            triggerSelectiveRecall();
        }
        else
        {
            // No stowed cards — do a full recall (collapse all back to compact)
            promoteSettledToFloating();
            triggerRecall();
        }
    }

    //==========================================================================
    // RECALL ANIMATION: cards fly back → shrink
    // Respects per-card participation: floating (undocked) cards are left in place.
    //==========================================================================
    void triggerRecall()
    {
        if (phase != AnimPhase::floating) return;

        phase = AnimPhase::recalling;

        // Determine which cards participate in the recall animation.
        // Cards that are currently floating (isFloating == true) and NOT docked
        // are left untouched — they stay where they are.
        // Shattered/stowed cards are NEVER recalled by triggerRecall —
        // only triggerSelectiveRecall can unshatter them.
        bool anyParticipating = false;
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);

            // Shattered cards: absolutely do NOT touch
            if (cardStowed[i])
            {
                recallingCard[i] = false;
                continue;
            }

            // Active floaters: skip (they stay floating)
            bool isActiveFloater = cardFloatState[i].isFloating && card != nullptr && !card->isDocked;

            recallingCard[i] = !isActiveFloater;

            if (recallingCard[i] && card != nullptr)
            {
                card->setExpanded(false, false);  // collapse for shelf
                anyParticipating = true;
            }
        }

        // Dissolve snap groups only for recalling cards
        for (int g = static_cast<int>(snapGroups.size()) - 1; g >= 0; --g)
        {
            auto& members = snapGroups[static_cast<size_t>(g)].members;
            bool anyMemberRecalling = false;
            for (int idx : members)
            {
                if (recallingCard[idx])
                    anyMemberRecalling = true;
            }
            if (anyMemberRecalling)
            {
                for (int idx : members)
                {
                    cardFloatState[idx].snapGroupID = -1;
                    auto* mc = getCard(idx);
                    if (mc) mc->showDetachButton = false;
                }
                snapGroups.erase(snapGroups.begin() + g);
            }
        }

        // Record current positions as start positions (only for participating cards)
        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (!card) continue;
            if (recallingCard[i])
            {
                recallStartPos[i] = card->getPosition().toFloat();
                recallProgress[i] = 0.0f;
            }
            else
            {
                recallProgress[i] = 1.0f;  // mark as "already done" so it doesn't block completion
            }
        }

        // Compute recall target (shelf position in current canvas — visible cards only)
        int recallVisibleCount = 0;
        for (int i = 0; i < numCards; ++i)
            if (!cardStowed[i]) recallVisibleCount++;

        int totalShelfH = recallVisibleCount * foldedCardH
                        + juce::jmax(0, recallVisibleCount - 1) * shelfGap;
        int settledFinalH = juce::jmax(totalShelfH + settledPadding * 2,
                                       compactH + settledPadding * 2);

        int settledNonoX = settledPadding + foldedCardW + settledGap;
        int settledNonoY = (settledFinalH - compactH) / 2;
        int settledCardX = settledPadding;
        int settledCardStartY = (settledFinalH - totalShelfH) / 2;

        int deltaX = settledCardX - settledNonoX;
        int deltaY = settledCardStartY - settledNonoY;

        recallTargetX = static_cast<float>(nonoFloatingX + deltaX);
        recallTargetStartY = static_cast<float>(nonoFloatingY + deltaY);
    }

    void tickRecall()
    {
        bool allDone = true;
        static constexpr float recallSpeed = 1.0f / 30.0f;  // ~0.5s at 60Hz

        for (int i = 0; i < numCards; ++i)
        {
            auto* card = getCard(i);
            if (!card) continue;

            // Skip cards not participating in recall (active floaters)
            if (!recallingCard[i]) continue;

            recallProgress[i] = juce::jmin(1.0f, recallProgress[i] + recallSpeed);
            if (recallProgress[i] < 1.0f) allDone = false;

            float t = easeInOutCubic(recallProgress[i]);

            float targetX = recallTargetX;
            float targetY = recallTargetStartY + static_cast<float>(i) * (foldedCardH + shelfGap);

            float cx = recallStartPos[i].x + (targetX - recallStartPos[i].x) * t;
            float cy = recallStartPos[i].y + (targetY - recallStartPos[i].y) * t;

            card->setTransform(juce::AffineTransform());
            card->setBounds(static_cast<int>(cx), static_cast<int>(cy), foldedCardW, foldedCardH);
        }

        if (allDone)
        {
            // Re-dock only recalled cards; leave active floaters untouched
            for (int i = 0; i < numCards; ++i)
            {
                if (!recallingCard[i]) continue;
                auto* card = getCard(i);
                if (!card) continue;
                card->isDocked = true;
                card->showDetachButton = false;
                card->customContentHeight = -1;  // reset custom sizing
                card->customWidth = -1;
                cardFloatState[i].isFloating = false;
                cardFloatState[i].snapGroupID = -1;
            }

            // Check if any active floaters or stowed cards remain on screen.
            // If so, stay in floating phase (don't shrink to settled).
            bool hasActiveFloater = false;
            bool hasStowed = false;
            for (int i = 0; i < numCards; ++i)
            {
                if (cardStowed[i]) { hasStowed = true; continue; }
                if (cardFloatState[i].isFloating) hasActiveFloater = true;
            }

            if (hasActiveFloater || hasStowed)
            {
                // Stay in floating phase — layout the shelf with recalled cards
                phase = AnimPhase::floating;
                layoutFloating();
            }
            else
            {
                // All cards are docked, none stowed → shrink to settled
                phase = AnimPhase::canvasShrink;
                animFrameCounter = 0;
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneNonoEditor)
};
