/*
  ==============================================================================
    Band3Component.h
    GOODMETER - 3-Band Frequency Analyzer (Alchemy Mode v3)

    Chemical Laboratory Design (Pixel-Perfect Paths):
    - LOW (20-250Hz)  = Beaker (矮胖烧杯 + 倒流口)
    - MID (250-2kHz)  = Graduated Cylinder (细长量筒 + 水平刻度线)
    - HIGH (2k-20kHz) = Erlenmeyer Flask (三角锥瓶 — 底宽顶窄)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class Band3Component : public juce::Component,
                       public juce::Timer
{
public:
    //==========================================================================
    Band3Component(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        setSize(100, 280);
        startTimerHz(60);
    }

    ~Band3Component() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        if (bounds.isEmpty())
            return;

        g.fillAll(juce::Colours::white);
        drawBand3Vessels(g, bounds);
    }

    void resized() override {}

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    float currentLow = -90.0f;
    float currentMid = -90.0f;
    float currentHigh = -90.0f;

    // Smoothed normalized display values (0.0 ~ 1.0)
    float displayLow = 0.0f;
    float displayMid = 0.0f;
    float displayHigh = 0.0f;

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        currentLow = audioProcessor.rmsLevelLow.load(std::memory_order_relaxed);
        currentMid = audioProcessor.rmsLevelMid3Band.load(std::memory_order_relaxed);
        currentHigh = audioProcessor.rmsLevelHigh.load(std::memory_order_relaxed);

        float targetLow = juce::jlimit(0.0f, 1.0f, juce::jmap(currentLow, minDb, maxDb, 0.0f, 1.0f));
        float targetMid = juce::jlimit(0.0f, 1.0f, juce::jmap(currentMid, minDb, maxDb, 0.0f, 1.0f));
        float targetHigh = juce::jlimit(0.0f, 1.0f, juce::jmap(currentHigh, minDb, maxDb, 0.0f, 1.0f));

        // Damped smoothing (silky 0.25 lerp)
        const float smoothing = 0.25f;
        displayLow += (targetLow - displayLow) * smoothing;
        displayMid += (targetMid - displayMid) * smoothing;
        displayHigh += (targetHigh - displayHigh) * smoothing;

        // Noise floor: clamp near-zero to zero
        if (displayLow < 0.005f) displayLow = 0.0f;
        if (displayMid < 0.005f) displayMid = 0.0f;
        if (displayHigh < 0.005f) displayHigh = 0.0f;

        repaint();
    }

    //==========================================================================
    void drawBand3Vessels(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        const float padX = juce::jmin(20.0f, bounds.getWidth() * 0.06f);
        const float padY = juce::jmin(25.0f, bounds.getHeight() * 0.06f);
        auto area = bounds.toFloat().reduced(padX, padY);

        if (area.getHeight() < 30.0f || area.getWidth() < 60.0f)
            return;

        // Label space at bottom
        const float labelSpace = juce::jlimit(16.0f, 28.0f, area.getHeight() * 0.12f);
        const float vesselHeight = area.getHeight() - labelSpace;

        // Three equal columns with gaps
        const float gap = juce::jlimit(8.0f, 20.0f, area.getWidth() * 0.04f);
        const float colWidth = (area.getWidth() - gap * 2.0f) / 3.0f;

        // Vessel areas — each vessel gets its own column
        auto beakerCol = juce::Rectangle<float>(area.getX(), area.getY(), colWidth, vesselHeight);
        auto cylinderCol = juce::Rectangle<float>(area.getX() + colWidth + gap, area.getY(), colWidth, vesselHeight);
        auto flaskCol = juce::Rectangle<float>(area.getX() + (colWidth + gap) * 2.0f, area.getY(), colWidth, vesselHeight);

        drawBeaker(g, beakerCol, displayLow, GoodMeterLookAndFeel::accentPink, "LOW");
        drawCylinder(g, cylinderCol, displayMid, GoodMeterLookAndFeel::accentYellow, "MID");
        drawFlask(g, flaskCol, displayHigh, GoodMeterLookAndFeel::accentGreen, "HIGH");
    }

    //==========================================================================
    // LOW = Bulbous Potion Bottle (球形魔药瓶)
    // Spherical bulb body + smooth shoulder + narrow neck + flared rim
    // All drawn as ONE continuous closed Path — no gaps, no floating geometry
    void drawBeaker(juce::Graphics& g, const juce::Rectangle<float>& col,
                    float level, const juce::Colour& color, const juce::String& label)
    {
        // === Proportional dimensions ===
        const float totalH = col.getHeight() * 0.90f;
        const float totalW = col.getWidth() * 0.80f;

        // Bulb: bottom spherical body (~60% of total height)
        const float bulbH = totalH * 0.58f;
        const float bulbW = totalW;
        const float bulbRx = bulbW / 2.0f;   // Horizontal radius
        const float bulbRy = bulbH / 2.0f;   // Vertical radius (slightly squashed)

        // Neck: narrow cylinder (~28% of total height, 30% of bulb width)
        const float neckH = totalH * 0.28f;
        const float neckW = bulbW * 0.28f;

        // Rim: flared opening at top (~14% of total height)
        const float rimH = totalH * 0.06f;
        const float rimW = neckW * 1.4f;

        // Shoulder: smooth cubic bezier transition zone
        const float shoulderH = totalH * 0.08f;

        // === Positioning (centered, sitting at bottom of column) ===
        const float cx = col.getCentreX();
        const float bottom = col.getBottom();
        const float bulbCY = bottom - bulbRy;                // Bulb center Y
        const float shoulderTopY = bottom - bulbH;           // Where shoulder starts (top of bulb)
        const float neckBottomY = shoulderTopY - shoulderH;  // Bottom of straight neck
        const float neckTopY = neckBottomY - neckH;          // Top of straight neck
        const float rimTopY = neckTopY - rimH;               // Top of flared rim

        // === Build ONE continuous closed path ===
        juce::Path vesselPath;

        // Start: top-left of rim (flared)
        vesselPath.startNewSubPath(cx - rimW / 2.0f, rimTopY);

        // Across rim top
        vesselPath.lineTo(cx + rimW / 2.0f, rimTopY);

        // Right rim → neck transition (slight inward taper)
        vesselPath.lineTo(cx + neckW / 2.0f, neckTopY);

        // Down right side of neck (straight)
        vesselPath.lineTo(cx + neckW / 2.0f, neckBottomY);

        // Right shoulder: smooth cubic from neck to bulb equator
        // Control points flare outward to create the classic flask shoulder
        vesselPath.cubicTo(
            cx + neckW / 2.0f + bulbRx * 0.1f, neckBottomY + shoulderH * 0.3f,  // CP1: slight outward
            cx + bulbRx,                         shoulderTopY + bulbRy * 0.15f,   // CP2: at bulb edge
            cx + bulbRx,                         bulbCY                           // End: bulb equator (right)
        );

        // Right half of bulb bottom arc (equator → bottom center)
        vesselPath.cubicTo(
            cx + bulbRx,       bulbCY + bulbRy * 0.55f,   // CP1
            cx + bulbRx * 0.55f, bottom,                    // CP2
            cx,                  bottom                     // End: bottom center
        );

        // Left half of bulb bottom arc (bottom center → equator)
        vesselPath.cubicTo(
            cx - bulbRx * 0.55f, bottom,                    // CP1
            cx - bulbRx,         bulbCY + bulbRy * 0.55f,   // CP2
            cx - bulbRx,         bulbCY                      // End: bulb equator (left)
        );

        // Left shoulder: smooth cubic from bulb equator to neck
        vesselPath.cubicTo(
            cx - bulbRx,                         shoulderTopY + bulbRy * 0.15f,   // CP1: at bulb edge
            cx - neckW / 2.0f - bulbRx * 0.1f,  neckBottomY + shoulderH * 0.3f,  // CP2: slight outward
            cx - neckW / 2.0f,                   neckBottomY                      // End: neck bottom left
        );

        // Up left side of neck (straight)
        vesselPath.lineTo(cx - neckW / 2.0f, neckTopY);

        // Left neck → rim transition
        vesselPath.lineTo(cx - rimW / 2.0f, rimTopY);

        vesselPath.closeSubPath();

        // Vessel bounds (full extent for liquid fill)
        auto vesselBounds = juce::Rectangle<float>(
            cx - bulbRx, rimTopY, bulbW, bottom - rimTopY
        );
        drawVesselWithLiquid(g, vesselPath, vesselBounds, level, color);
        drawVesselLabel(g, col, label);
    }

    //==========================================================================
    // MID = Graduated Cylinder (细长量筒 + 迷你水平刻度线)
    // Tall, narrow rectangle with tiny graduation marks
    void drawCylinder(juce::Graphics& g, const juce::Rectangle<float>& col,
                      float level, const juce::Colour& color, const juce::String& label)
    {
        // Cylinder: tall and narrow — 38% of column width, 90% of column height
        const float cw = col.getWidth() * 0.38f;
        const float ch = col.getHeight() * 0.90f;
        const float cx = col.getCentreX() - cw / 2.0f;
        const float cy = col.getBottom() - ch;
        const float cornerR = juce::jmin(3.0f, cw * 0.08f);

        // Slight lip at top (wider opening)
        const float lipExtra = cw * 0.08f;

        juce::Path vesselPath;
        // Top-left lip
        vesselPath.startNewSubPath(cx - lipExtra, cy);
        // Across top with lip
        vesselPath.lineTo(cx + cw + lipExtra, cy);
        // Lip step down on right
        vesselPath.lineTo(cx + cw, cy + cw * 0.15f);
        // Down right side
        vesselPath.lineTo(cx + cw, cy + ch - cornerR);
        // Bottom-right corner
        vesselPath.quadraticTo(cx + cw, cy + ch, cx + cw - cornerR, cy + ch);
        // Across bottom
        vesselPath.lineTo(cx + cornerR, cy + ch);
        // Bottom-left corner
        vesselPath.quadraticTo(cx, cy + ch, cx, cy + ch - cornerR);
        // Up left side
        vesselPath.lineTo(cx, cy + cw * 0.15f);
        // Lip step on left
        vesselPath.closeSubPath();

        auto vesselBounds = juce::Rectangle<float>(cx, cy, cw, ch);
        drawVesselWithLiquid(g, vesselPath, vesselBounds, level, color);

        // Mini graduation marks (right side of cylinder body)
        g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.12f));
        const int numTicks = 10;
        for (int t = 1; t < numTicks; ++t)
        {
            float tickY = cy + ch * (1.0f - static_cast<float>(t) / static_cast<float>(numTicks));
            float tickLen = (t % 5 == 0) ? cw * 0.45f : cw * 0.25f;
            float tickStroke = (t % 5 == 0) ? 1.0f : 0.6f;
            g.drawLine(cx + cw - tickLen, tickY, cx + cw, tickY, tickStroke);
        }

        drawVesselLabel(g, col, label);
    }

    //==========================================================================
    // HIGH = Erlenmeyer Flask (三角锥瓶 — 底宽顶窄, 平滑肩部曲线)
    void drawFlask(juce::Graphics& g, const juce::Rectangle<float>& col,
                   float level, const juce::Colour& color, const juce::String& label)
    {
        const float fw = col.getWidth();
        const float fh = col.getHeight() * 0.88f;
        const float fx = col.getX();
        const float fy = col.getBottom() - fh;

        // Neck: narrow top
        const float neckW = fw * 0.22f;
        const float neckH = fh * 0.30f;
        const float neckX = col.getCentreX() - neckW / 2.0f;

        // Body: wide bottom
        const float bodyW = fw * 0.88f;
        const float bodyX = col.getCentreX() - bodyW / 2.0f;
        const float cornerR = juce::jmin(4.0f, bodyW * 0.05f);

        // Shoulder transition Y
        const float shoulderY = fy + neckH;

        juce::Path vesselPath;
        // Start top-left of neck
        vesselPath.startNewSubPath(neckX, fy);
        // Across top of neck
        vesselPath.lineTo(neckX + neckW, fy);
        // Right shoulder: smooth curve from neck to body
        vesselPath.quadraticTo(neckX + neckW + 2.0f, shoulderY,
                               bodyX + bodyW, fy + fh - cornerR);
        // Bottom-right corner
        vesselPath.quadraticTo(bodyX + bodyW, fy + fh,
                               bodyX + bodyW - cornerR, fy + fh);
        // Across bottom
        vesselPath.lineTo(bodyX + cornerR, fy + fh);
        // Bottom-left corner
        vesselPath.quadraticTo(bodyX, fy + fh,
                               bodyX, fy + fh - cornerR);
        // Left shoulder: smooth curve from body to neck
        vesselPath.quadraticTo(neckX - 2.0f, shoulderY,
                               neckX, fy);
        vesselPath.closeSubPath();

        auto vesselBounds = juce::Rectangle<float>(bodyX, fy, bodyW, fh);
        drawVesselWithLiquid(g, vesselPath, vesselBounds, level, color);
        drawVesselLabel(g, col, label);
    }

    //==========================================================================
    // Core: Clip-based liquid fill + glass vessel outline + outer glow
    void drawVesselWithLiquid(juce::Graphics& g, const juce::Path& vesselPath,
                              const juce::Rectangle<float>& vesselArea,
                              float levelNorm, const juce::Colour& liquidColor)
    {
        // === 1. Zero-overflow clipped liquid fill ===
        if (levelNorm > 0.0f)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(vesselPath);

            float fillHeight = vesselArea.getHeight() * juce::jlimit(0.0f, 1.0f, levelNorm);
            float fillY = vesselArea.getBottom() - fillHeight;

            // Liquid body fill
            g.setColour(liquidColor.withAlpha(0.65f));
            g.fillRect(vesselArea.getX() - 2.0f, fillY,
                       vesselArea.getWidth() + 4.0f, fillHeight + 1.0f);

            // Meniscus highlight (bright line at liquid surface)
            if (fillHeight > 3.0f)
            {
                g.setColour(liquidColor.brighter(0.6f).withAlpha(0.5f));
                g.fillRect(vesselArea.getX() - 2.0f, fillY, vesselArea.getWidth() + 4.0f, 2.0f);
            }
        }

        // === 2. Subtle outer glow (neon halo) ===
        if (levelNorm > 0.05f)
        {
            float glowAlpha = juce::jlimit(0.0f, 0.12f, levelNorm * 0.12f);
            g.setColour(liquidColor.withAlpha(glowAlpha));
            g.strokePath(vesselPath, juce::PathStrokeType(5.0f));
        }

        // === 3. Glass vessel outline (elegant thin stroke) ===
        g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.20f));
        g.strokePath(vesselPath, juce::PathStrokeType(1.5f));

        // === 4. Glass highlight (subtle white reflection on left edge) ===
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.strokePath(vesselPath, juce::PathStrokeType(0.8f));
    }

    //==========================================================================
    // Label below vessel — bold, centered under the column
    void drawVesselLabel(juce::Graphics& g, const juce::Rectangle<float>& col, const juce::String& label)
    {
        const float fontSize = juce::jlimit(10.0f, 15.0f, col.getHeight() * 0.07f);
        const float labelY = col.getBottom() + 2.0f;
        const int labelH = static_cast<int>(fontSize * 1.6f);

        g.setColour(GoodMeterLookAndFeel::textMain);
        g.setFont(juce::Font(fontSize, juce::Font::bold));
        g.drawText(label,
                   static_cast<int>(col.getX()),
                   static_cast<int>(labelY),
                   static_cast<int>(col.getWidth()),
                   labelH,
                   juce::Justification::centred, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Band3Component)
};
