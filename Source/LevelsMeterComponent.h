/*
  ==============================================================================
    LevelsMeterComponent.h
    GOODMETER - Peak and LUFS Level Meters

    Translated from Levels.tsx
    Features: Peak bars with gradient, peak hold indicators, LUFS readout
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
/**
 * Peak and LUFS Level Meter Component
 * Displays L/R peak bars with gradient coloring and peak hold indicators
 * Shows momentary/short-term/integrated LUFS values
 */
class LevelsMeterComponent : public juce::Component,
                             public juce::Timer
{
public:
    //==========================================================================
    LevelsMeterComponent()
    {
        // ✅ 只设置高度，宽度由父容器（MeterCard）控制
        setSize(100, 200);  // 初始宽度会被父容器覆盖

        // Start timer for peak hold decay (matches Levels.tsx: 1000ms hold)
        startTimer(16);  // ~60Hz for smooth decay
    }

    ~LevelsMeterComponent() override
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

        // Draw peak bars section (top 68px)
        auto barsBounds = bounds.removeFromTop(68);
        drawPeakBars(g, barsBounds);

        // Draw LUFS info section (bottom, with 36px gap from Levels.tsx line 166)
        bounds.removeFromTop(36);
        auto infoBounds = bounds;
        drawLUFSInfo(g, infoBounds);
    }

    void resized() override
    {
        // No child components to layout
    }

    //==========================================================================
    /**
     * Update meter values from processor (called from PluginEditor::timerCallback)
     */
    void updateMetrics(float peakL_dB, float peakR_dB, float lufs_dB)
    {
        currentPeakL = peakL_dB;
        currentPeakR = peakR_dB;
        currentLUFS = lufs_dB;

        // 🎯 平滑插值策略：每帧追赶目标值（0.15f 平滑系数）
        // 营造数字快速但连续滚动的质感，避免跳跃突变
        displayPeakL += (currentPeakL - displayPeakL) * 0.15f;
        displayPeakR += (currentPeakR - displayPeakR) * 0.15f;
        displayLUFS += (currentLUFS - displayLUFS) * 0.15f;

        // Update peak holds (logic from Levels.tsx lines 41-56)
        auto now = juce::Time::getMillisecondCounterHiRes();

        if (currentPeakL > peakHoldL || (now - peakHoldTimeL) > 1000.0)
        {
            if (currentPeakL > peakHoldL)
            {
                peakHoldL = currentPeakL;
                peakHoldTimeL = now;
            }
            else
            {
                peakHoldL -= 0.5f;  // Decay
            }
        }

        if (currentPeakR > peakHoldR || (now - peakHoldTimeR) > 1000.0)
        {
            if (currentPeakR > peakHoldR)
            {
                peakHoldR = currentPeakR;
                peakHoldTimeR = now;
            }
            else
            {
                peakHoldR -= 0.5f;  // Decay
            }
        }

        repaint();
    }

    /**
     * Set loudness standard for target reference line
     */
    void setStandard(const juce::String& standardName)
    {
        standard = standardName;
        repaint();
    }

private:
    //==========================================================================
    // Current values (updated every frame from processor)
    float currentPeakL = -90.0f;
    float currentPeakR = -90.0f;
    float currentLUFS = -70.0f;

    // 📊 平滑插值显示值（每帧追赶，0.15f 平滑系数）
    // 营造数字快速但连续滚动的质感
    float displayPeakL = -90.0f;
    float displayPeakR = -90.0f;
    float displayLUFS = -70.0f;

    // Peak hold state (Levels.tsx lines 25-28)
    float peakHoldL = -60.0f;
    float peakHoldR = -60.0f;
    double peakHoldTimeL = 0.0;
    double peakHoldTimeR = 0.0;

    // Loudness standard
    juce::String standard = "EBU R128";

    // Constants (from Levels.tsx)
    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;
    static constexpr int barHeight = 28;
    static constexpr int barGap = 12;

    //==========================================================================
    void timerCallback() override
    {
        // Peak hold decay is handled in updateMetrics()
        // This timer just ensures smooth repaints
    }

    //==========================================================================
    /**
     * Convert dB value to pixel X position (Levels.tsx lines 91-94)
     */
    float dbToX(float db, float width) const
    {
        const float clamped = juce::jlimit(minDb, maxDb, db);
        return ((clamped - minDb) / (maxDb - minDb)) * width;
    }

    //==========================================================================
    /**
     * Draw a single peak bar (Levels.tsx lines 76-129)
     */
    void drawPeakBar(juce::Graphics& g,
                     const juce::Rectangle<int>& bounds,
                     float currentPeak,
                     float holdPeak)
    {
        auto b = bounds.toFloat();
        const float width = b.getWidth();

        // Background (Levels.tsx line 80-81)
        g.setColour(juce::Colour(0xFFEAEAEA));
        g.fillRect(b);

        // Border (Levels.tsx line 83-86)
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRect(b, 2.0f);

        // Calculate X positions
        const float currentX = dbToX(currentPeak, width);
        const float holdX = dbToX(holdPeak, width);

        // Gradient fill (Levels.tsx lines 98-108)
        juce::ColourGradient gradient(
            GoodMeterLookAndFeel::accentGreen,  // #00D084
            0.0f, b.getCentreY(),
            GoodMeterLookAndFeel::accentPink,   // #E6335F
            width, b.getCentreY(),
            false
        );

        // Add color stops (green → yellow → red)
        gradient.addColour(dbToX(-18.0f, width) / width, GoodMeterLookAndFeel::accentGreen);
        gradient.addColour(dbToX(-18.0f, width) / width, GoodMeterLookAndFeel::accentYellow);
        gradient.addColour(dbToX(-6.0f, width) / width, GoodMeterLookAndFeel::accentYellow);
        gradient.addColour(dbToX(-6.0f, width) / width, GoodMeterLookAndFeel::accentPink);

        g.setGradientFill(gradient);
        g.fillRect(b.withWidth(currentX));

        // Peak hold line (Levels.tsx lines 111-113)
        g.setColour(GoodMeterLookAndFeel::border);
        g.fillRect(b.withX(holdX).withWidth(4.0f));

        // Target loudness reference line (Levels.tsx lines 116-128)
        float targetLoudness = -23.0f;  // EBU R128
        if (standard == "ATSC A/85")
            targetLoudness = -24.0f;
        else if (standard == "AES Streaming")
            targetLoudness = -16.0f;

        const float targetX = dbToX(targetLoudness, width);
        g.setColour(GoodMeterLookAndFeel::accentCyan);  // #06D6A0

        // Dashed line
        juce::Path dashPath;
        dashPath.startNewSubPath(targetX, b.getY());
        dashPath.lineTo(targetX, b.getBottom());

        float dashLengths[2] = { 8.0f, 8.0f };
        juce::PathStrokeType strokeType(4.0f);
        strokeType.createDashedStroke(dashPath, dashPath, dashLengths, 2);

        g.strokePath(dashPath, strokeType);
    }

    //==========================================================================
    /**
     * Draw both peak bars and scale ticks (Levels.tsx lines 131-162)
     */
    void drawPeakBars(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        // Create mutable copy
        auto area = bounds;

        // Draw L channel bar
        auto barL = area.removeFromTop(barHeight);
        drawPeakBar(g, barL, currentPeakL, peakHoldL);

        // Gap
        area.removeFromTop(barGap);

        // Draw R channel bar
        auto barR = area.removeFromTop(barHeight);
        drawPeakBar(g, barR, currentPeakR, peakHoldR);

        // Draw scale ticks (Levels.tsx lines 154-161)
        g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.1f));
        g.setFont(10.0f);

        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            float x = dbToX(static_cast<float>(db), static_cast<float>(bounds.getWidth()));

            // Tick line
            g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(barHeight * 2 + barGap));

            // Label
            juce::String label = juce::String(db);
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.drawText(label,
                      static_cast<int>(x - 15), barHeight * 2 + barGap + 2,
                      30, 12,
                      juce::Justification::centred, false);
        }
    }

    //==========================================================================
    /**
     * Draw LUFS info grid (Levels.tsx lines 166-209)
     */
    void drawLUFSInfo(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        // Background box (Levels.tsx line 166)
        g.setColour(juce::Colour(0xFFEAEAEA));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // Border
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f, 2.0f);

        // 3-column grid layout (Levels.tsx: grid-cols-3)
        auto gridBounds = bounds.reduced(16, 12);
        const int colWidth = gridBounds.getWidth() / 3;

        auto drawMetric = [&](int col, int row, const juce::String& label, float value, const juce::String& unit, bool highlight = false)
        {
            auto cellBounds = gridBounds.withX(gridBounds.getX() + col * colWidth)
                                       .withY(gridBounds.getY() + row * 32)
                                       .withWidth(colWidth)
                                       .withHeight(28);

            // Label (Levels.tsx: text-[0.8rem] font-[600] lowercase)
            // Use separate bounds for label to avoid modifying cellBounds
            auto labelBounds = cellBounds.removeFromLeft(colWidth / 2);
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.setFont(juce::Font(12.8f, juce::Font::bold));
            g.drawText(label.toLowerCase(),
                      labelBounds,
                      juce::Justification::centredLeft, false);

            // Value (Levels.tsx: text-[1.2rem] font-[800])
            // cellBounds now contains the right half for the value
            juce::String valueStr = (value <= -60.0f) ? juce::String(juce::CharPointer_UTF8(u8"-∞")) : juce::String(value, 1);
            valueStr += " " + unit;

            g.setColour(highlight ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::textMain);
            g.setFont(juce::Font(19.2f, juce::Font::bold));
            g.drawText(valueStr,
                      cellBounds,  // This now correctly uses the remaining right half
                      juce::Justification::centredRight, false);
        };

        // 📊 使用降帧后的显示值（每 10 帧更新，约 6Hz）
        // Column 1
        drawMetric(0, 0, "momentary", displayLUFS, "LUFS", displayLUFS > -10.0f);
        drawMetric(0, 1, "true peak l", displayPeakL, "dBTP", displayPeakL > -1.0f);

        // Column 2
        drawMetric(1, 0, "short-term", displayLUFS, "LUFS");  // Simplified for now
        drawMetric(1, 1, "true peak r", displayPeakR, "dBTP", displayPeakR > -1.0f);

        // Column 3
        drawMetric(2, 0, "integrated", displayLUFS, "LUFS");  // Simplified for now
        drawMetric(2, 1, "lu range", 5.2f, "LU");  // Mock value
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelsMeterComponent)
};
