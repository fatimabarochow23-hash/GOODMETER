/*
  ==============================================================================
    PhaseCorrelationComponent.h
    GOODMETER - Phase Correlation Meter (Condenser Tube)

    Translated from PhaseCorrelation.tsx
    Features: Wavy tube condenser with colored liquid blob
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
/**
 * Phase Correlation Meter Component
 * Displays correlation as a colored liquid moving through a wavy condenser tube
 * Range: -1.0 (out of phase, red) to +1.0 (in phase, green)
 */
class PhaseCorrelationComponent : public juce::Component,
                                  public juce::Timer
{
public:
    //==========================================================================
    PhaseCorrelationComponent()
    {
        // Set fixed height (from PhaseCorrelation.tsx: 180px)
        setSize(500, 180);

        // Start timer for smooth animation
        startTimer(16);  // ~60Hz
    }

    ~PhaseCorrelationComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Safety check
        if (bounds.isEmpty())
            return;

        const float width = static_cast<float>(bounds.getWidth());
        const float height = static_cast<float>(bounds.getHeight());
        const float cx = width / 2.0f;
        const float cy = height / 2.0f - 20.0f;  // PhaseCorrelation.tsx line 36

        // Draw canvas section (top 140px from 180px total)
        auto canvasBounds = bounds.removeFromTop(140);
        drawCondenser(g, canvasBounds.toFloat(), cx, cy);

        // Draw labels at bottom (PhaseCorrelation.tsx lines 150-154)
        auto labelBounds = bounds;
        drawLabels(g, labelBounds);
    }

    void resized() override
    {
        // No child components
    }

    //==========================================================================
    /**
     * Update correlation value from processor
     */
    void updateCorrelation(float correlation)
    {
        // Smooth the phase value (PhaseCorrelation.tsx line 25)
        smoothedPhase += (correlation - smoothedPhase) * 0.1f;
        repaint();
    }

private:
    //==========================================================================
    float smoothedPhase = 0.0f;

    // Constants (from PhaseCorrelation.tsx)
    static constexpr int loops = 8;  // Number of sine wave loops

    //==========================================================================
    void timerCallback() override
    {
        // Smooth animation handled in updateCorrelation()
    }

    //==========================================================================
    /**
     * Draw the condenser tube with wavy inner path
     */
    void drawCondenser(juce::Graphics& g, const juce::Rectangle<float>& bounds, float cx, float cy)
    {
        const float condenserWidth = bounds.getWidth() * 0.7f;  // PhaseCorrelation.tsx line 40
        const float condenserHeight = 120.0f;  // PhaseCorrelation.tsx line 41
        const float startX = cx - condenserWidth / 2.0f;
        const float endX = cx + condenserWidth / 2.0f;

        // Draw outer tube (PhaseCorrelation.tsx lines 45-83)
        drawOuterTube(g, startX, endX, cy, condenserHeight);

        // Draw inner wavy tube (PhaseCorrelation.tsx lines 85-110)
        drawInnerTube(g, startX, endX, cy, condenserWidth, condenserHeight);

        // Draw colored liquid blob (PhaseCorrelation.tsx lines 112-124)
        drawLiquidBlob(g, startX, condenserWidth, cy, condenserHeight, bounds);

        // Draw center zero line (PhaseCorrelation.tsx lines 126-134)
        drawCenterLine(g, cx, cy, condenserHeight);
    }

    //==========================================================================
    /**
     * Draw outer tube structure (PhaseCorrelation.tsx lines 45-83)
     */
    void drawOuterTube(juce::Graphics& g, float startX, float endX, float cy, float condenserHeight)
    {
        juce::Path outerTube;

        // Top edge
        outerTube.startNewSubPath(startX, cy - condenserHeight/2.0f);
        outerTube.lineTo(endX, cy - condenserHeight/2.0f);

        // Bottom edge
        outerTube.startNewSubPath(startX, cy + condenserHeight/2.0f);
        outerTube.lineTo(endX, cy + condenserHeight/2.0f);

        // Left end caps
        outerTube.startNewSubPath(startX, cy - condenserHeight/2.0f);
        outerTube.lineTo(startX, cy - 24.0f);
        outerTube.startNewSubPath(startX, cy + 24.0f);
        outerTube.lineTo(startX, cy + condenserHeight/2.0f);

        // Right end caps
        outerTube.startNewSubPath(endX, cy - condenserHeight/2.0f);
        outerTube.lineTo(endX, cy - 24.0f);
        outerTube.startNewSubPath(endX, cy + 24.0f);
        outerTube.lineTo(endX, cy + condenserHeight/2.0f);

        // Inlet (top left)
        outerTube.startNewSubPath(startX + 60.0f, cy - condenserHeight/2.0f);
        outerTube.lineTo(startX + 60.0f, cy - condenserHeight/2.0f - 30.0f);
        outerTube.startNewSubPath(startX + 100.0f, cy - condenserHeight/2.0f);
        outerTube.lineTo(startX + 100.0f, cy - condenserHeight/2.0f - 30.0f);

        // Outlet (bottom right)
        outerTube.startNewSubPath(endX - 100.0f, cy + condenserHeight/2.0f);
        outerTube.lineTo(endX - 100.0f, cy + condenserHeight/2.0f + 30.0f);
        outerTube.startNewSubPath(endX - 60.0f, cy + condenserHeight/2.0f);
        outerTube.lineTo(endX - 60.0f, cy + condenserHeight/2.0f + 30.0f);

        // Stroke outer tube (PhaseCorrelation.tsx lines 46-48)
        g.setColour(GoodMeterLookAndFeel::border);
        g.strokePath(outerTube, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved));
    }

    //==========================================================================
    /**
     * Create wavy inner tube path (PhaseCorrelation.tsx lines 87-98)
     */
    juce::Path createInnerTubePath(float startX, float endX, float cy, float condenserWidth, float condenserHeight)
    {
        juce::Path innerPath;

        // Start directly at tube beginning (no 80px extension)
        innerPath.startNewSubPath(startX, cy);

        // Draw sinusoidal path (PhaseCorrelation.tsx lines 91-96)
        const int steps = loops * 40;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float x = startX + t * condenserWidth;
            const float y = cy + std::sin(t * juce::MathConstants<float>::pi * 2.0f * loops) * (condenserHeight/2.0f - 28.0f);
            innerPath.lineTo(x, y);
        }

        // End directly at tube end (no 80px extension)
        innerPath.lineTo(endX, cy);

        return innerPath;
    }

    //==========================================================================
    /**
     * Draw inner tube with black outline and white fill (PhaseCorrelation.tsx lines 100-110)
     */
    void drawInnerTube(juce::Graphics& g, float startX, float endX, float cy, float condenserWidth, float condenserHeight)
    {
        auto innerPath = createInnerTubePath(startX, endX, cy, condenserWidth, condenserHeight);

        // 1. Inner tube outline (thick black) - PhaseCorrelation.tsx lines 101-104
        g.setColour(GoodMeterLookAndFeel::border);
        g.strokePath(innerPath, juce::PathStrokeType(24.0f, juce::PathStrokeType::curved));

        // 2. Inner tube inside (white) - PhaseCorrelation.tsx lines 107-110
        g.setColour(juce::Colours::white);
        g.strokePath(innerPath, juce::PathStrokeType(16.0f, juce::PathStrokeType::curved));
    }

    //==========================================================================
    /**
     * Draw colored liquid blob with clipping (PhaseCorrelation.tsx lines 112-124)
     */
    void drawLiquidBlob(juce::Graphics& g, float startX, float condenserWidth, float cy, float condenserHeight, const juce::Rectangle<float>& bounds)
    {
        // Calculate blob position based on phase (PhaseCorrelation.tsx lines 114-115)
        const float blobWidth = 160.0f;
        const float mappedX = startX + ((smoothedPhase + 1.0f) / 2.0f) * condenserWidth;

        // Create clipping rectangle (PhaseCorrelation.tsx lines 116-118)
        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(juce::Rectangle<int>(
            static_cast<int>(mappedX - blobWidth/2.0f),
            0,
            static_cast<int>(blobWidth),
            static_cast<int>(bounds.getHeight())
        ));

        // Draw colored liquid (PhaseCorrelation.tsx lines 120-123)
        auto innerPath = createInnerTubePath(startX, startX + condenserWidth, cy, condenserWidth, condenserHeight);
        juce::Colour liquidColour = smoothedPhase > 0.0f ? GoodMeterLookAndFeel::accentCyan : GoodMeterLookAndFeel::accentPink;
        g.setColour(liquidColour);
        g.strokePath(innerPath, juce::PathStrokeType(16.0f, juce::PathStrokeType::curved));
    }

    //==========================================================================
    /**
     * Draw center zero reference line (PhaseCorrelation.tsx lines 126-134)
     */
    void drawCenterLine(juce::Graphics& g, float cx, float cy, float condenserHeight)
    {
        juce::Path centerLine;
        centerLine.startNewSubPath(cx, cy - condenserHeight/2.0f - 20.0f);
        centerLine.lineTo(cx, cy + condenserHeight/2.0f + 20.0f);

        // Dashed line (PhaseCorrelation.tsx lines 130-132)
        float dashLengths[2] = { 8.0f, 8.0f };
        juce::PathStrokeType strokeType(4.0f);
        strokeType.createDashedStroke(centerLine, centerLine, dashLengths, 2);

        g.setColour(GoodMeterLookAndFeel::border);
        g.strokePath(centerLine, strokeType);
    }

    //==========================================================================
    /**
     * Draw labels at bottom (PhaseCorrelation.tsx lines 150-154)
     */
    void drawLabels(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        auto labelBounds = bounds.reduced(40, 0).withHeight(30);

        // -1.0 label (left, pink)
        g.setColour(GoodMeterLookAndFeel::accentPink);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("-1.0",
                  labelBounds.removeFromLeft(80),
                  juce::Justification::centredLeft, false);

        // +1.0 label (right, cyan)
        g.setColour(GoodMeterLookAndFeel::accentCyan);
        g.drawText("+1.0",
                  labelBounds.removeFromRight(80),
                  juce::Justification::centredRight, false);

        // Center value (middle, larger)
        juce::String valueStr = juce::String(smoothedPhase, 2);
        g.setColour(GoodMeterLookAndFeel::textMain);
        g.setFont(juce::Font(19.2f, juce::Font::bold));
        g.drawText(valueStr,
                  labelBounds,
                  juce::Justification::centred, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseCorrelationComponent)
};
