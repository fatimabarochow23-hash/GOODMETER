/*
  ==============================================================================
    HoloNonoComponent.h
    GOODMETER - Holographic Companion Robot "NONO"

    2.5D vector-drawn floating sphere robot with:
    - juce::Path hand-drawn body, screen, eyes, antenna, arm
    - Figure-8 idle hover animation
    - State machine: Idle, RaiseHand, FlashEyes, CollisionHit
    - Responds to card fold/unfold events from PluginEditor
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class HoloNonoComponent : public juce::Component,
                           public juce::Timer
{
public:
    //==========================================================================
    HoloNonoComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        setSize(100, 200);
        startTimerHz(60);
    }

    ~HoloNonoComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    // External event notifications (called by PluginEditor)
    //==========================================================================

    /** Called when any card is folded (collapsed) */
    void onCardFolded(const juce::String& cardName)
    {
        // Nearby collision: Spectrogram (above) or Stereo (left)
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

        // Generic fold: raise hand wave
        triggerState(AnimState::RaiseHand);
    }

    /** Called when any card is unfolded (expanded) */
    void onCardUnfolded(const juce::String& /*cardName*/)
    {
        triggerState(AnimState::FlashEyes);
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty() || bounds.getHeight() < 40.0f)
            return;

        // Calculate body center and radius
        const float unit = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float radius = unit * 0.22f;
        const float cx = bounds.getCentreX() + idleOffsetX + collisionOffsetX;
        const float cy = bounds.getCentreY() + idleOffsetY + collisionOffsetY;

        // Draw order: back to front
        drawAntiGravityGlow(g, cx, cy, radius);
        drawShadow(g, cx, cy, radius);
        drawArm(g, cx, cy, radius);
        drawBody(g, cx, cy, radius);
        drawScreen(g, cx, cy, radius);
        drawEyes(g, cx, cy, radius);
        drawAntenna(g, cx, cy, radius);
        drawParticles(g, cx, cy, radius);
    }

    void resized() override {}

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // Animation state machine
    enum class AnimState { Idle, RaiseHand, FlashEyes, CollisionHit };
    AnimState currentState = AnimState::Idle;
    int stateTimer = 0;

    // Collision direction
    enum class CollisionDir { None, Up, Left };
    CollisionDir collisionDir = CollisionDir::None;

    // Idle figure-8 float
    float idlePhase = 0.0f;
    float idleOffsetX = 0.0f;
    float idleOffsetY = 0.0f;

    // Eye animation
    float eyeOpenness = 0.4f;      // 0 = shut, 1 = fully open
    float targetEyeOpenness = 0.4f;
    float eyeGlow = 0.7f;          // Default alpha
    float targetEyeGlow = 0.7f;

    // Arm raise (0 = folded, 1 = fully raised)
    float armRaise = 0.0f;
    float targetArmRaise = 0.0f;

    // Collision offset
    float collisionOffsetX = 0.0f;
    float collisionOffsetY = 0.0f;
    float targetCollisionX = 0.0f;
    float targetCollisionY = 0.0f;

    // Particle effect
    bool showParticles = false;
    float particleProgress = 0.0f;
    float particleOriginX = 0.0f;
    float particleOriginY = 0.0f;

    // Color constants
    static inline const juce::Colour holoCyan   = juce::Colour(0xFF00E5FF);
    static inline const juce::Colour bodyWhite  = juce::Colour(0xFFF0F0F5);
    static inline const juce::Colour bodyEdge   = juce::Colour(0xFFD0D0D8);
    static inline const juce::Colour screenDark = juce::Colour(0xFF1A1A2E);
    static inline const juce::Colour metalGray  = juce::Colour(0xFFB0B0B8);

    //==========================================================================
    void timerCallback() override
    {
        // --- Audio-reactive eye tracking ---
        float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
        float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
        float peak = juce::jmax(peakL, peakR);
        targetEyeOpenness = juce::jmap(juce::jlimit(-60.0f, 0.0f, peak),
                                        -60.0f, 0.0f, 0.3f, 1.0f);

        // --- Idle figure-8 hover (always running) ---
        idlePhase += 0.015f;
        if (idlePhase > 628.0f) idlePhase -= 628.0f;
        idleOffsetX = 1.5f * std::sin(2.0f * idlePhase);
        idleOffsetY = 2.5f * std::sin(idlePhase);

        // --- State machine animation ---
        advanceStateAnimation();

        // --- Smooth interpolation for all visual properties ---
        eyeOpenness += (targetEyeOpenness - eyeOpenness) * 0.15f;
        eyeGlow += (targetEyeGlow - eyeGlow) * 0.2f;
        armRaise += (targetArmRaise - armRaise) * 0.25f;

        // Collision offset uses faster lerp for snappy feel
        float collisionLerp = (currentState == AnimState::CollisionHit) ? 0.4f : 0.25f;
        collisionOffsetX += (targetCollisionX - collisionOffsetX) * collisionLerp;
        collisionOffsetY += (targetCollisionY - collisionOffsetY) * collisionLerp;

        // Particle animation
        if (showParticles)
        {
            particleProgress += 0.06f;
            if (particleProgress >= 1.0f)
                showParticles = false;
        }

        repaint();
    }

    //==========================================================================
    void advanceStateAnimation()
    {
        if (currentState == AnimState::Idle) return;

        stateTimer++;

        switch (currentState)
        {
            case AnimState::RaiseHand:
            {
                // Phase 1: raise (6 frames = 0.1s)
                // Phase 2: hold  (18 frames = 0.3s)
                // Phase 3: lower (12 frames = 0.2s)
                const int raiseEnd = 6;
                const int holdEnd = 24;
                const int totalEnd = 36;

                if (stateTimer <= raiseEnd)
                    targetArmRaise = static_cast<float>(stateTimer) / static_cast<float>(raiseEnd);
                else if (stateTimer <= holdEnd)
                    targetArmRaise = 1.0f;
                else if (stateTimer <= totalEnd)
                    targetArmRaise = 1.0f - static_cast<float>(stateTimer - holdEnd)
                                           / static_cast<float>(totalEnd - holdEnd);
                else
                {
                    targetArmRaise = 0.0f;
                    currentState = AnimState::Idle;
                }
                break;
            }

            case AnimState::FlashEyes:
            {
                // Phase 1: flash to max (instant, 1 frame)
                // Phase 2: hold bright (6 frames = 0.1s)
                // Phase 3: fade out (12 frames = 0.2s)
                const int holdEnd = 7;
                const int fadeEnd = 19;

                if (stateTimer <= holdEnd)
                    targetEyeGlow = 1.0f;
                else if (stateTimer <= fadeEnd)
                    targetEyeGlow = 1.0f - (static_cast<float>(stateTimer - holdEnd)
                                            / static_cast<float>(fadeEnd - holdEnd)) * 0.3f;
                else
                {
                    targetEyeGlow = 0.7f;
                    currentState = AnimState::Idle;
                }
                break;
            }

            case AnimState::CollisionHit:
            {
                // Phase 1: dash toward target (3 frames = 0.05s)
                // Phase 2: impact + particles (2 frames)
                // Phase 3: bounce back (6 frames = 0.1s)
                const int dashEnd = 3;
                const int impactEnd = 5;
                const int bounceEnd = 11;
                const float maxDash = 45.0f;

                if (stateTimer <= dashEnd)
                {
                    float t = static_cast<float>(stateTimer) / static_cast<float>(dashEnd);
                    if (collisionDir == CollisionDir::Up)
                        targetCollisionY = -maxDash * t;
                    else
                        targetCollisionX = -maxDash * t;
                }
                else if (stateTimer <= impactEnd)
                {
                    // Spawn particles at collision edge
                    if (stateTimer == dashEnd + 1)
                    {
                        showParticles = true;
                        particleProgress = 0.0f;
                        auto b = getLocalBounds().toFloat();
                        if (collisionDir == CollisionDir::Up)
                        {
                            particleOriginX = b.getCentreX();
                            particleOriginY = b.getCentreY() + collisionOffsetY;
                        }
                        else
                        {
                            particleOriginX = b.getCentreX() + collisionOffsetX;
                            particleOriginY = b.getCentreY();
                        }
                    }
                }
                else if (stateTimer <= bounceEnd)
                {
                    // Bounce back to center
                    targetCollisionX = 0.0f;
                    targetCollisionY = 0.0f;
                }
                else
                {
                    targetCollisionX = 0.0f;
                    targetCollisionY = 0.0f;
                    if (!showParticles)
                        currentState = AnimState::Idle;
                }
                break;
            }

            default: break;
        }
    }

    //==========================================================================
    void triggerState(AnimState newState)
    {
        // Don't interrupt collision (highest priority)
        if (currentState == AnimState::CollisionHit)
            return;

        currentState = newState;
        stateTimer = 0;
    }

    void triggerCollision(CollisionDir dir)
    {
        currentState = AnimState::CollisionHit;
        collisionDir = dir;
        stateTimer = 0;
        targetCollisionX = 0.0f;
        targetCollisionY = 0.0f;
    }

    //==========================================================================
    // Drawing methods (2.5D vector art)
    //==========================================================================

    /** Anti-gravity glow beneath the sphere */
    void drawAntiGravityGlow(juce::Graphics& g, float cx, float cy, float r)
    {
        const float glowW = r * 0.7f;
        const float glowH = r * 0.2f;
        const float glowY = cy + r + r * 0.08f;

        juce::ColourGradient grad(
            holoCyan.withAlpha(0.18f), cx, glowY,
            holoCyan.withAlpha(0.0f), cx, glowY + glowH * 3.0f,
            false
        );
        g.setGradientFill(grad);
        g.fillEllipse(cx - glowW, glowY, glowW * 2.0f, glowH * 3.0f);
    }

    /** Drop shadow ellipse */
    void drawShadow(juce::Graphics& g, float cx, float cy, float r)
    {
        const float shadowW = r * 0.8f;
        const float shadowH = r * 0.12f;
        const float shadowY = cy + r + r * 0.15f;

        g.setColour(juce::Colour(0x18000000));
        g.fillEllipse(cx - shadowW, shadowY, shadowW * 2.0f, shadowH * 2.0f);
    }

    /** Main sphere body with 3D shading */
    void drawBody(juce::Graphics& g, float cx, float cy, float r)
    {
        // Radial gradient: bright top-left -> darker bottom-right
        juce::ColourGradient bodyGrad(
            juce::Colour(0xFFF8F8FF), cx - r * 0.3f, cy - r * 0.3f,
            bodyEdge, cx + r * 0.5f, cy + r * 0.5f,
            true
        );
        g.setGradientFill(bodyGrad);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        // Edge stroke
        g.setColour(bodyEdge);
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);

        // Specular highlight (top-left)
        juce::ColourGradient specGrad(
            juce::Colours::white.withAlpha(0.55f), cx - r * 0.25f, cy - r * 0.35f,
            juce::Colours::white.withAlpha(0.0f), cx + r * 0.1f, cy,
            true
        );
        g.setGradientFill(specGrad);
        g.fillEllipse(cx - r * 0.6f, cy - r * 0.7f, r * 0.8f, r * 0.55f);
    }

    /** Dark screen/visor on the front of the sphere */
    void drawScreen(juce::Graphics& g, float cx, float cy, float r)
    {
        const float screenW = r * 1.3f;
        const float screenH = r * 0.85f;
        const float cornerR = r * 0.15f;

        auto screenRect = juce::Rectangle<float>(
            cx - screenW / 2.0f, cy - screenH / 2.0f, screenW, screenH
        );

        // Dark screen fill
        g.setColour(screenDark);
        g.fillRoundedRectangle(screenRect, cornerR);

        // Subtle screen border glow
        g.setColour(holoCyan.withAlpha(0.12f));
        g.drawRoundedRectangle(screenRect, cornerR, 1.0f);
    }

    /** Two digital eyes on the screen */
    void drawEyes(juce::Graphics& g, float cx, float cy, float r)
    {
        const float eyeSpacing = r * 0.32f;
        const float eyeW = r * 0.18f;
        const float eyeH = r * 0.15f * juce::jlimit(0.15f, 1.0f, eyeOpenness);

        // Ensure minimum visible height even when "half-shut"
        const float minEyeH = r * 0.03f;
        const float actualEyeH = juce::jmax(minEyeH, eyeH);

        // Draw each eye (left, right)
        for (int side = -1; side <= 1; side += 2)
        {
            const float ex = cx + eyeSpacing * static_cast<float>(side);
            const float ey = cy - r * 0.02f;

            // Bloom glow behind eye (visible when eyeGlow > 0.7)
            if (eyeGlow > 0.72f)
            {
                const float bloomR = eyeW * 2.5f * (eyeGlow - 0.5f);
                juce::ColourGradient bloomGrad(
                    holoCyan.withAlpha((eyeGlow - 0.7f) * 1.5f), ex, ey,
                    holoCyan.withAlpha(0.0f), ex + bloomR, ey,
                    true
                );
                g.setGradientFill(bloomGrad);
                g.fillEllipse(ex - bloomR, ey - bloomR, bloomR * 2.0f, bloomR * 2.0f);
            }

            // Eye core
            g.setColour(holoCyan.withAlpha(eyeGlow));
            g.fillEllipse(ex - eyeW, ey - actualEyeH, eyeW * 2.0f, actualEyeH * 2.0f);

            // Tiny specular dot (top-right of eye)
            g.setColour(juce::Colours::white.withAlpha(eyeGlow * 0.6f));
            const float dotR = eyeW * 0.2f;
            g.fillEllipse(ex + eyeW * 0.3f - dotR, ey - actualEyeH * 0.5f - dotR,
                          dotR * 2.0f, dotR * 2.0f);
        }
    }

    /** Small antenna on top of sphere */
    void drawAntenna(juce::Graphics& g, float cx, float cy, float r)
    {
        const float baseX = cx + r * 0.1f;
        const float baseY = cy - r;
        const float tipX = cx + r * 0.15f;
        const float tipY = cy - r * 1.35f;

        // Stem
        g.setColour(metalGray);
        g.drawLine(baseX, baseY, tipX, tipY, r * 0.04f);

        // Tip ball (pulsing cyan)
        float tipPulse = 0.6f + 0.4f * std::sin(idlePhase * 3.0f);
        g.setColour(holoCyan.withAlpha(tipPulse * 0.8f));
        const float tipR = r * 0.055f;
        g.fillEllipse(tipX - tipR, tipY - tipR, tipR * 2.0f, tipR * 2.0f);

        // Tip glow
        g.setColour(holoCyan.withAlpha(tipPulse * 0.2f));
        const float glowR = tipR * 2.5f;
        g.fillEllipse(tipX - glowR, tipY - glowR, glowR * 2.0f, glowR * 2.0f);
    }

    /** Mechanical arm on left side of sphere */
    void drawArm(juce::Graphics& g, float cx, float cy, float r)
    {
        // Pivot: left edge of sphere, slightly below center
        const float pivotX = cx - r * 0.85f;
        const float pivotY = cy + r * 0.1f;

        // Arm segment lengths
        const float upperLen = r * 0.5f;
        const float foreLen = r * 0.4f;

        // Angles: folded = arm tucked down, raised = arm up-left (wave)
        const float baseAngle = juce::jmap(armRaise, 0.0f, 1.0f,
                                            juce::degreesToRadians(-100.0f),   // folded: down-left
                                            juce::degreesToRadians(-210.0f));  // raised: up-left

        const float foreAngle = baseAngle + juce::jmap(armRaise, 0.0f, 1.0f,
                                                         juce::degreesToRadians(20.0f),
                                                         juce::degreesToRadians(-45.0f));

        // Joint positions
        const float elbowX = pivotX + upperLen * std::cos(baseAngle);
        const float elbowY = pivotY + upperLen * std::sin(baseAngle);
        const float handX = elbowX + foreLen * std::cos(foreAngle);
        const float handY = elbowY + foreLen * std::sin(foreAngle);

        // Draw arm path
        juce::Path armPath;
        armPath.startNewSubPath(pivotX, pivotY);
        armPath.lineTo(elbowX, elbowY);
        armPath.lineTo(handX, handY);

        g.setColour(metalGray);
        g.strokePath(armPath, juce::PathStrokeType(
            r * 0.07f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded
        ));

        // Shoulder joint
        g.setColour(bodyEdge);
        const float jointR = r * 0.05f;
        g.fillEllipse(pivotX - jointR, pivotY - jointR, jointR * 2.0f, jointR * 2.0f);

        // Elbow joint
        g.fillEllipse(elbowX - jointR, elbowY - jointR, jointR * 2.0f, jointR * 2.0f);

        // Hand (small circle)
        const float handR = r * 0.04f;
        g.setColour(metalGray.brighter(0.2f));
        g.fillEllipse(handX - handR, handY - handR, handR * 2.0f, handR * 2.0f);
    }

    /** Collision particle burst effect */
    void drawParticles(juce::Graphics& g, float cx, float cy, float r)
    {
        if (!showParticles || particleProgress >= 1.0f)
            return;

        const int numParticles = 10;
        const float alpha = (1.0f - particleProgress) * 0.85f;
        const float dist = particleProgress * r * 1.8f;
        const float size = r * 0.05f * (1.0f - particleProgress * 0.6f);

        // Particle origin is stored when collision happens
        const float ox = particleOriginX;
        const float oy = particleOriginY;

        for (int i = 0; i < numParticles; ++i)
        {
            float angle = static_cast<float>(i) * juce::MathConstants<float>::twoPi
                         / static_cast<float>(numParticles);
            float px = ox + dist * std::cos(angle);
            float py = oy + dist * std::sin(angle);

            // Alternate pink and cyan particles
            juce::Colour pColour = (i % 2 == 0)
                ? GoodMeterLookAndFeel::accentPink.withAlpha(alpha)
                : holoCyan.withAlpha(alpha);
            g.setColour(pColour);
            g.fillEllipse(px - size, py - size, size * 2.0f, size * 2.0f);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoloNonoComponent)
};
