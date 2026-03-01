/*
  ==============================================================================
    Band3Component.h
    GOODMETER - 3-Band Frequency Analyzer (Alchemy Mode)

    🧪 Chemical Laboratory Design:
    - LOW (20-250Hz)  = Beaker (矮胖烧杯)
    - MID (250-2kHz)  = Cylinder (细长量筒)
    - HIGH (2k-20kHz) = Erlenmeyer Flask (尖顶三角瓶)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
/**
 * 3-Band Frequency Analyzer Component
 * Displays LOW/MID/HIGH band energy as chemical vessels with liquid fill
 */
class Band3Component : public juce::Component,
                       public juce::Timer
{
public:
    //==========================================================================
    Band3Component(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Set initial size (width will be controlled by parent MeterCard)
        setSize(100, 280);

        // Start 60Hz timer for smooth liquid animation
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

        // Safety check
        if (bounds.isEmpty())
            return;

        // Background
        g.fillAll(juce::Colours::white);

        // Border
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRect(bounds.toFloat(), 2.0f);

        // Draw the three alchemical vessels
        drawBand3Vessels(g, bounds);
    }

    void resized() override
    {
        // No child components
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // Current RMS levels (raw from processor atomics)
    float currentLow = -90.0f;
    float currentMid = -90.0f;
    float currentHigh = -90.0f;

    // 🎯 平滑插值显示值 (Lerp smoothing for silky liquid animation)
    float displayLow = 0.0f;   // Normalized 0.0 ~ 1.2+ (allows overflow)
    float displayMid = 0.0f;
    float displayHigh = 0.0f;

    // dB range for level mapping
    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // Update band RMS levels from processor
        currentLow = audioProcessor.rmsLevelLow.load(std::memory_order_relaxed);
        currentMid = audioProcessor.rmsLevelMid3Band.load(std::memory_order_relaxed);
        currentHigh = audioProcessor.rmsLevelHigh.load(std::memory_order_relaxed);

        // Convert dB to normalized 0.0 ~ 1.2+ (allows overflow > 1.0)
        float targetLow = juce::jmap(currentLow, minDb, maxDb, 0.0f, 1.0f);
        float targetMid = juce::jmap(currentMid, minDb, maxDb, 0.0f, 1.0f);
        float targetHigh = juce::jmap(currentHigh, minDb, maxDb, 0.0f, 1.0f);

        // 🎯 Lerp 平滑插值 (silky smooth liquid animation, 0.3f damping)
        const float smoothing = 0.3f;
        displayLow += (targetLow - displayLow) * smoothing;
        displayMid += (targetMid - displayMid) * smoothing;
        displayHigh += (targetHigh - displayHigh) * smoothing;

        repaint();
    }

    //==========================================================================
    /**
     * 🧪 Draw the three alchemical vessels (Beaker, Cylinder, Flask)
     */
    void drawBand3Vessels(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        // 1️⃣ 严密的 3 列 Grid 领地切分 (equal width, leave space for labels)
        auto area = bounds.toFloat().reduced(20.0f, 30.0f);

        const float vesselWidth = area.getWidth() / 3.0f;
        const float vesselHeight = area.getHeight() - 30.0f;  // Reserve 30px for bottom labels

        // Define three vessel areas
        auto beakerArea = juce::Rectangle<float>(
            area.getX(),
            area.getY(),
            vesselWidth,
            vesselHeight
        ).reduced(10.0f, 0.0f);

        auto cylinderArea = juce::Rectangle<float>(
            area.getX() + vesselWidth,
            area.getY(),
            vesselWidth,
            vesselHeight
        ).reduced(10.0f, 0.0f);

        auto flaskArea = juce::Rectangle<float>(
            area.getX() + vesselWidth * 2.0f,
            area.getY(),
            vesselWidth,
            vesselHeight
        ).reduced(10.0f, 0.0f);

        // 2️⃣ Draw each vessel with its unique shape
        drawBeaker(g, beakerArea, displayLow, GoodMeterLookAndFeel::accentPink, "LOW");
        drawCylinder(g, cylinderArea, displayMid, GoodMeterLookAndFeel::accentYellow, "MID");
        drawFlask(g, flaskArea, displayHigh, GoodMeterLookAndFeel::accentGreen, "HIGH");
    }

    //==========================================================================
    /**
     * 🧪 Draw LOW frequency vessel - Beaker (矮胖烧杯)
     * Wide and short, stable base
     */
    void drawBeaker(juce::Graphics& g, const juce::Rectangle<float>& area,
                    float level, const juce::Colour& color, const juce::String& label)
    {
        // Beaker shape: Wide rounded rectangle with slight taper at top
        juce::Path beakerPath;

        const float w = area.getWidth();
        const float h = area.getHeight();
        const float x = area.getX();
        const float y = area.getY();

        // Create beaker outline (slightly tapered)
        const float topWidth = w * 0.85f;
        const float bottomWidth = w;
        const float offset = (bottomWidth - topWidth) / 2.0f;

        beakerPath.startNewSubPath(x + offset, y);
        beakerPath.lineTo(x + offset + topWidth, y);
        beakerPath.lineTo(x + bottomWidth, y + h);
        beakerPath.lineTo(x, y + h);
        beakerPath.closeSubPath();

        // Draw glass vessel and liquid
        drawVesselWithLiquid(g, beakerPath, area, level, color);

        // Draw bottom label
        drawLabel(g, area, label);
    }

    //==========================================================================
    /**
     * 🧪 Draw MID frequency vessel - Cylinder (细长量筒)
     * Tall and narrow, uniform width
     */
    void drawCylinder(juce::Graphics& g, const juce::Rectangle<float>& area,
                      float level, const juce::Colour& color, const juce::String& label)
    {
        // Cylinder shape: Tall rounded rectangle with uniform width
        juce::Path cylinderPath;

        const float w = area.getWidth() * 0.6f;  // Narrow cylinder
        const float h = area.getHeight();
        const float x = area.getCentreX() - w / 2.0f;
        const float y = area.getY();

        cylinderPath.addRoundedRectangle(x, y, w, h, w / 2.0f);

        // Draw glass vessel and liquid
        auto cylinderArea = juce::Rectangle<float>(x, y, w, h);
        drawVesselWithLiquid(g, cylinderPath, cylinderArea, level, color);

        // Draw bottom label
        drawLabel(g, area, label);
    }

    //==========================================================================
    /**
     * 🧪 Draw HIGH frequency vessel - Erlenmeyer Flask (尖顶三角瓶)
     * Narrow neck at top, wide base at bottom
     */
    void drawFlask(juce::Graphics& g, const juce::Rectangle<float>& area,
                   float level, const juce::Colour& color, const juce::String& label)
    {
        // Flask shape: Narrow top transitioning to wide bottom
        juce::Path flaskPath;

        const float w = area.getWidth();
        const float h = area.getHeight();
        const float x = area.getX();
        const float y = area.getY();

        // Neck width (narrow top)
        const float neckWidth = w * 0.3f;
        const float neckHeight = h * 0.25f;
        const float neckX = x + (w - neckWidth) / 2.0f;

        // Flask body (wide bottom)
        const float bodyWidth = w * 0.9f;
        const float bodyHeight = h - neckHeight;
        const float bodyX = x + (w - bodyWidth) / 2.0f;

        // Create flask outline
        flaskPath.startNewSubPath(neckX, y);  // Top left of neck
        flaskPath.lineTo(neckX + neckWidth, y);  // Top right of neck
        flaskPath.lineTo(neckX + neckWidth, y + neckHeight);  // Bottom right of neck
        flaskPath.lineTo(bodyX + bodyWidth, y + h);  // Bottom right of body
        flaskPath.lineTo(bodyX, y + h);  // Bottom left of body
        flaskPath.lineTo(neckX, y + neckHeight);  // Bottom left of neck
        flaskPath.closeSubPath();

        // Draw glass vessel and liquid
        drawVesselWithLiquid(g, flaskPath, area, level, color);

        // Draw bottom label
        drawLabel(g, area, label);
    }

    //==========================================================================
    /**
     * 🎨 Core drawing logic: Glass vessel + Liquid fill + Overflow detection
     */
    void drawVesselWithLiquid(juce::Graphics& g, const juce::Path& vesselPath,
                              const juce::Rectangle<float>& vesselArea,
                              float levelNorm, const juce::Colour& color)
    {
        // 3️⃣ Liquid fill with clip-based rendering (ZERO OVERFLOW!)
        {
            juce::Graphics::ScopedSaveState state(g);

            // Calculate liquid fill height (allows > 1.0 for overflow)
            float fillHeight = vesselArea.getHeight() * juce::jlimit(0.0f, 1.0f, levelNorm);

            // 🔒 Zero-overflow clipping: Clip to vessel path
            g.reduceClipRegion(vesselPath);

            // Fill liquid from bottom up
            g.setColour(color.withAlpha(0.7f));
            g.fillRect(
                vesselArea.getX(),
                vesselArea.getBottom() - fillHeight,
                vesselArea.getWidth(),
                fillHeight
            );

        }  // Restore clip region

        // 4️⃣ Overflow detection: Draw spilling liquid if level > 1.0
        if (levelNorm > 1.0f)
        {
            drawOverflow(g, vesselArea, color);
        }

        // Draw glass vessel outline (微弱浅灰描边)
        g.setColour(juce::Colours::grey.withAlpha(0.2f));
        g.strokePath(vesselPath, juce::PathStrokeType(2.0f));
    }

    //==========================================================================
    /**
     * 💥 Draw overflow effect when liquid exceeds vessel capacity
     */
    void drawOverflow(juce::Graphics& g, const juce::Rectangle<float>& vesselArea,
                      const juce::Colour& color)
    {
        // Draw spilling liquid from top of vessel
        const float spillY = vesselArea.getY();
        const float spillHeight = 15.0f;

        juce::Path spillPath;
        spillPath.startNewSubPath(vesselArea.getCentreX() - 10.0f, spillY);
        spillPath.lineTo(vesselArea.getCentreX() + 10.0f, spillY);
        spillPath.lineTo(vesselArea.getCentreX() + 15.0f, spillY - spillHeight);
        spillPath.lineTo(vesselArea.getCentreX() - 15.0f, spillY - spillHeight);
        spillPath.closeSubPath();

        g.setColour(color.withAlpha(0.6f));
        g.fillPath(spillPath);

        // Draw vapor/steam effect (small circles)
        g.setColour(color.withAlpha(0.3f));
        for (int i = 0; i < 3; ++i)
        {
            float vaporX = vesselArea.getCentreX() + (i - 1) * 10.0f;
            float vaporY = spillY - spillHeight - 5.0f - i * 5.0f;
            g.fillEllipse(vaporX - 3.0f, vaporY - 3.0f, 6.0f, 6.0f);
        }
    }

    //==========================================================================
    /**
     * 📝 Draw bottom label (LOW, MID, HIGH)
     */
    void drawLabel(juce::Graphics& g, const juce::Rectangle<float>& area, const juce::String& label)
    {
        const float labelY = area.getBottom() + 5.0f;

        g.setColour(GoodMeterLookAndFeel::textMain);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(
            label,
            static_cast<int>(area.getX()),
            static_cast<int>(labelY),
            static_cast<int>(area.getWidth()),
            20,
            juce::Justification::centred,
            false
        );
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Band3Component)
};
