/*
  ==============================================================================
    PhaseCorrelationComponent.h
    GOODMETER - Phase Correlation Meter (Alchemy Condenser v2)

    Redesigned: Tight-wrap rounded glass jacket around spiral inner tube
    Features:
    - Outer jacket: rounded-rectangle hugging the sine wave amplitude + padding
    - Inner spiral tube: wavy sine path with glass outline + white fill
    - Liquid blob: cyan glow on the spiral path at correlation position
    - Glass aesthetic: thin gray stroke, white highlight, cyan outer glow
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
class PhaseCorrelationComponent : public juce::Component,
                                  public juce::Timer
{
public:
    //==========================================================================
    PhaseCorrelationComponent()
    {
        setSize(100, 180);
        startTimerHz(60);  // 60Hz visual refresh
    }

    ~PhaseCorrelationComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        if (bounds.isEmpty() || bounds.getHeight() < 30)
            return;

        const float height = static_cast<float>(bounds.getHeight());

        // Label area at bottom
        const int labelH = juce::jlimit(18, 35, static_cast<int>(height * 0.15f));
        auto labelBounds = bounds.removeFromBottom(labelH);

        // Condenser uses remaining area
        auto condenserBounds = bounds.toFloat();
        drawCondenser(g, condenserBounds);
        drawLabels(g, labelBounds);
    }

    void resized() override {}

    //==========================================================================
    void updateCorrelation(float correlation)
    {
        smoothedPhase += (correlation - smoothedPhase) * 0.1f;

        // Pre-render value text to offscreen cache
        auto bounds = getLocalBounds();
        if (!bounds.isEmpty())
        {
            const int labelH = juce::jlimit(18, 35, static_cast<int>(bounds.getHeight() * 0.15f));
            const float padX = juce::jlimit(10.0f, 40.0f, bounds.getWidth() * 0.08f);
            int lw = bounds.getWidth() - static_cast<int>(padX) * 2;
            if (lw > 10 && labelH > 5)
            {
                if (valueTextCache.isNull() || lastValueW != lw || lastValueH != labelH)
                {
                    valueTextCache = juce::Image(juce::Image::ARGB, lw, labelH, true, juce::SoftwareImageType());
                    lastValueW = lw;
                    lastValueH = labelH;
                }
                valueTextCache.clear(valueTextCache.getBounds());
                juce::Graphics tg(valueTextCache);
                const float valueFont = juce::jlimit(11.0f, 19.0f, labelH * 0.7f);
                tg.setColour(GoodMeterLookAndFeel::textMain);
                tg.setFont(juce::Font(valueFont, juce::Font::bold));
                tg.drawText(juce::String(smoothedPhase, 2), 0, 0, lw, labelH, juce::Justification::centred, false);
            }
        }

        repaint();
    }

private:
    //==========================================================================
    float smoothedPhase = 0.0f;
    float scaleFactor = 1.0f;

    static constexpr int loopsDefault = 8;   // Normal size: 8 loops
    static constexpr int loopsSmall = 6;     // Small size: 6 loops
    int currentLoops = loopsDefault;         // Adaptive per frame

    // Offscreen text caches
    juce::Image sideLabelsCache;   // STATIC: -1.0 and +1.0
    juce::Image valueTextCache;    // DYNAMIC: correlation value
    int lastSideLabelsW = 0, lastSideLabelsH = 0;
    int lastValueW = 0, lastValueH = 0;

    //==========================================================================
    void timerCallback() override {}

    //==========================================================================
    /**
     * Main condenser drawing — tight-wrap glass jacket around spiral tube
     */
    void drawCondenser(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();

        // Condenser dimensions proportional to bounds
        const float condenserWidth = bounds.getWidth() * 0.80f;
        const float condenserHeight = juce::jlimit(30.0f, 120.0f, bounds.getHeight() * 0.70f);

        scaleFactor = condenserHeight / 80.0f;

        // Adaptive loop count: 8 when big, 6 when small
        currentLoops = (condenserWidth < 200.0f) ? loopsSmall : loopsDefault;

        const float startX = cx - condenserWidth / 2.0f;
        const float endX = cx + condenserWidth / 2.0f;

        // Spiral amplitude (how tall the wave is)
        const float s = scaleFactor;
        const float amplitude = condenserHeight / 2.0f - 18.0f * s;

        // === 1. Outer glass jacket (tight-wrap rounded rectangle) ===
        // Hugs the spiral: height = amplitude * 2 + padding
        const float jacketPadY = juce::jlimit(6.0f, 14.0f, 10.0f * s);
        const float jacketPadX = juce::jlimit(4.0f, 10.0f, 6.0f * s);
        const float jacketTop = cy - amplitude - jacketPadY;
        const float jacketBottom = cy + amplitude + jacketPadY;
        const float jacketLeft = startX - jacketPadX;
        const float jacketRight = endX + jacketPadX;
        const float jacketH = jacketBottom - jacketTop;
        const float jacketW = jacketRight - jacketLeft;
        const float jacketCornerR = juce::jlimit(6.0f, 16.0f, 10.0f * s);

        // Inlet/outlet pipe dimensions
        const float pipeW = juce::jlimit(8.0f, 20.0f, 14.0f * s);
        const float pipeH = juce::jlimit(10.0f, 28.0f, 20.0f * s);
        const float pipeOffsetX = condenserWidth * 0.22f;

        // Inlet pipe position (top-left area)
        const float inletCX = startX + pipeOffsetX;
        const float inletL = inletCX - pipeW / 2.0f;
        const float inletR = inletCX + pipeW / 2.0f;

        // Outlet pipe position (bottom-right area)
        const float outletCX = endX - pipeOffsetX;
        const float outletL = outletCX - pipeW / 2.0f;
        const float outletR = outletCX + pipeW / 2.0f;

        // Draw outer jacket as a single closed rounded-rect path with pipe cutouts
        drawOuterJacket(g, jacketLeft, jacketTop, jacketW, jacketH, jacketCornerR,
                        inletL, inletR, outletL, outletR, pipeH);

        // === 2. Inner spiral tube ===
        drawInnerTube(g, startX, endX, cy, condenserWidth, amplitude);

        // === 3. Liquid blob on spiral ===
        drawLiquidBlob(g, startX, condenserWidth, cy, amplitude, bounds);

        // === 4. Center reference line ===
        drawCenterLine(g, cx, cy, condenserHeight);
    }

    //==========================================================================
    /**
     * Outer glass jacket: rounded rectangle with inlet/outlet pipe stubs
     * Elegant thin stroke + white highlight + subtle cyan glow
     */
    void drawOuterJacket(juce::Graphics& g,
                         float jx, float jy, float jw, float jh, float cornerR,
                         float inletL, float inletR, float outletL, float outletR,
                         float pipeH)
    {
        auto jacketRect = juce::Rectangle<float>(jx, jy, jw, jh);

        // Cyan outer glow (subtle, always-on for condenser identity)
        g.setColour(GoodMeterLookAndFeel::accentCyan.withAlpha(0.08f));
        g.drawRoundedRectangle(jacketRect.expanded(3.0f), cornerR + 2.0f, 4.0f);

        // Glass vessel outline
        g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.20f));
        g.drawRoundedRectangle(jacketRect, cornerR, 1.5f);

        // Glass highlight
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(jacketRect.reduced(1.0f), cornerR - 0.5f, 0.8f);

        // Inlet pipe (top) — two vertical lines going up
        const float inletTopY = jy - pipeH;
        g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.20f));
        g.drawLine(inletL, jy, inletL, inletTopY, 1.5f);
        g.drawLine(inletR, jy, inletR, inletTopY, 1.5f);

        // Outlet pipe (bottom) — two vertical lines going down
        const float outletBottomY = jy + jh + pipeH;
        g.drawLine(outletL, jy + jh, outletL, outletBottomY, 1.5f);
        g.drawLine(outletR, jy + jh, outletR, outletBottomY, 1.5f);

        // Pipe cyan glow highlights
        g.setColour(GoodMeterLookAndFeel::accentCyan.withAlpha(0.06f));
        g.drawLine(inletL + 1.0f, jy, inletL + 1.0f, inletTopY, 3.0f);
        g.drawLine(outletR - 1.0f, jy + jh, outletR - 1.0f, outletBottomY, 3.0f);
    }

    //==========================================================================
    /**
     * Create wavy inner tube path (sine wave)
     */
    juce::Path createInnerTubePath(float startX, float endX, float cy,
                                    float condenserWidth, float amplitude)
    {
        const float s = scaleFactor;
        const float ext = juce::jlimit(12.0f, 40.0f, 30.0f * s);

        juce::Path innerPath;
        innerPath.startNewSubPath(startX - ext, cy);
        innerPath.lineTo(startX, cy);

        const int steps = currentLoops * 40;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float x = startX + t * condenserWidth;
            const float y = cy + std::sin(t * juce::MathConstants<float>::pi * 2.0f
                                          * static_cast<float>(currentLoops)) * amplitude;
            innerPath.lineTo(x, y);
        }

        innerPath.lineTo(endX + ext, cy);
        return innerPath;
    }

    //==========================================================================
    /**
     * Draw inner tube: dark outline → white fill (glass tube look)
     */
    void drawInnerTube(juce::Graphics& g, float startX, float endX, float cy,
                       float condenserWidth, float amplitude)
    {
        const float s = scaleFactor;
        const float outerStroke = juce::jlimit(6.0f, 14.0f, 12.0f * s);
        const float innerStroke = juce::jlimit(3.0f, 8.0f, 7.0f * s);

        auto innerPath = createInnerTubePath(startX, endX, cy, condenserWidth, amplitude);

        // Dark outline
        g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.22f));
        g.strokePath(innerPath, juce::PathStrokeType(outerStroke, juce::PathStrokeType::curved));

        // White fill (glass interior)
        g.setColour(juce::Colours::white);
        g.strokePath(innerPath, juce::PathStrokeType(innerStroke, juce::PathStrokeType::curved));

        // Subtle highlight on tube
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.strokePath(innerPath, juce::PathStrokeType(innerStroke * 0.4f, juce::PathStrokeType::curved));
    }

    //==========================================================================
    /**
     * Liquid blob: clipped cyan/pink glow on the spiral at correlation position
     */
    void drawLiquidBlob(juce::Graphics& g, float startX, float condenserWidth,
                        float cy, float amplitude, const juce::Rectangle<float>& bounds)
    {
        const float s = scaleFactor;
        const float blobWidth = juce::jlimit(20.0f, 60.0f, 40.0f * s);
        const float innerStroke = juce::jlimit(3.0f, 8.0f, 7.0f * s);
        const float mappedX = startX + ((smoothedPhase + 1.0f) / 2.0f) * condenserWidth;

        juce::Graphics::ScopedSaveState saveState(g);

        g.reduceClipRegion(juce::Rectangle<int>(
            static_cast<int>(mappedX - blobWidth / 2.0f),
            0,
            static_cast<int>(blobWidth),
            static_cast<int>(bounds.getHeight())
        ));

        juce::Colour liquidColour = smoothedPhase > 0.0f
            ? GoodMeterLookAndFeel::accentCyan
            : GoodMeterLookAndFeel::accentPink;

        // Feathered gradient: transparent → opaque → transparent
        juce::ColourGradient gradient(
            liquidColour.withAlpha(0.0f),
            mappedX - blobWidth / 2.0f, cy,
            liquidColour.withAlpha(0.0f),
            mappedX + blobWidth / 2.0f, cy,
            false
        );
        gradient.addColour(0.5, liquidColour);

        g.setGradientFill(gradient);

        auto innerPath = createInnerTubePath(startX, startX + condenserWidth,
                                              cy, condenserWidth, amplitude);
        // Glow layer (wider)
        g.strokePath(innerPath, juce::PathStrokeType(innerStroke + 4.0f, juce::PathStrokeType::curved));

        // Core liquid
        juce::ColourGradient coreGrad(
            liquidColour.brighter(0.3f).withAlpha(0.0f),
            mappedX - blobWidth / 2.0f, cy,
            liquidColour.brighter(0.3f).withAlpha(0.0f),
            mappedX + blobWidth / 2.0f, cy,
            false
        );
        coreGrad.addColour(0.5, liquidColour.brighter(0.3f));
        g.setGradientFill(coreGrad);
        g.strokePath(innerPath, juce::PathStrokeType(innerStroke, juce::PathStrokeType::curved));
    }

    //==========================================================================
    /**
     * Center zero reference line (dashed)
     */
    void drawCenterLine(juce::Graphics& g, float cx, float cy, float condenserHeight)
    {
        const float s = scaleFactor;
        const float ext = juce::jlimit(8.0f, 20.0f, 18.0f * s);
        const float dashLen = juce::jlimit(4.0f, 8.0f, 6.0f * s);
        const float strokeW = juce::jlimit(1.5f, 3.0f, 2.5f * s);

        juce::Path centerLine;
        centerLine.startNewSubPath(cx, cy - condenserHeight / 2.0f - ext);
        centerLine.lineTo(cx, cy + condenserHeight / 2.0f + ext);

        float dashLengths[2] = { dashLen, dashLen };
        juce::PathStrokeType strokeType(strokeW);
        strokeType.createDashedStroke(centerLine, centerLine, dashLengths, 2);

        g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.25f));
        g.strokePath(centerLine, strokeType);
    }

    //==========================================================================
    /**
     * Labels: -1.0 (pink), value (center), +1.0 (cyan)
     */
    void drawLabels(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        const float padX = juce::jlimit(10.0f, 40.0f, bounds.getWidth() * 0.08f);
        auto labelBounds = bounds.reduced(static_cast<int>(padX), 0);
        int lw = labelBounds.getWidth();
        int lh = labelBounds.getHeight();
        if (lw < 10 || lh < 5) return;

        // Static side labels (rebuild on resize only)
        if (sideLabelsCache.isNull() || lastSideLabelsW != lw || lastSideLabelsH != lh)
        {
            lastSideLabelsW = lw;
            lastSideLabelsH = lh;
            sideLabelsCache = juce::Image(juce::Image::ARGB, lw, lh, true, juce::SoftwareImageType());
            juce::Graphics tg(sideLabelsCache);

            const float labelFont = juce::jlimit(9.0f, 14.0f, lh * 0.5f);
            const int sideW = juce::jlimit(30, 80, static_cast<int>(lw * 0.2f));

            tg.setFont(juce::Font(labelFont, juce::Font::bold));
            tg.setColour(GoodMeterLookAndFeel::accentPink);
            tg.drawText("-1.0", 0, 0, sideW, lh, juce::Justification::centredLeft, false);

            tg.setColour(GoodMeterLookAndFeel::accentCyan);
            tg.drawText("+1.0", lw - sideW, 0, sideW, lh, juce::Justification::centredRight, false);
        }
        g.drawImageAt(sideLabelsCache, labelBounds.getX(), labelBounds.getY());

        // Dynamic value (pre-rendered in updateCorrelation)
        if (!valueTextCache.isNull())
            g.drawImageAt(valueTextCache, labelBounds.getX(), labelBounds.getY());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseCorrelationComponent)
};
