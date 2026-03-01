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

        // 🎯 动态高度映射：Peak bars 占 55%，LUFS info 占 40%（纵向拉大数据面板！）
        const int totalHeight = bounds.getHeight();
        const int barsHeight = static_cast<int>(totalHeight * 0.55f);
        const int spacing = 10;

        auto barsBounds = bounds.removeFromTop(barsHeight);
        drawPeakBars(g, barsBounds);

        // Draw LUFS info section（拿走剩余的 45% 空间）
        bounds.removeFromTop(spacing);
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

        // 🎯 平滑插值策略：每帧追赶目标值（0.3f 平滑系数）
        // 营造数字快速但连续滚动的质感，避免跳跃突变
        displayPeakL += (currentPeakL - displayPeakL) * 0.3f;
        displayPeakR += (currentPeakR - displayPeakR) * 0.3f;
        displayLUFS += (currentLUFS - displayLUFS) * 0.3f;

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

    // 📊 平滑插值显示值（每帧追赶，0.3f 平滑系数）
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
        // ✅ 单边裁剪：左右各 20px，顶部往下推 16px（不削减底部！）
        auto area = bounds.reduced(20, 0).withTrimmedTop(16);

        // Draw L channel bar (✅ 使用平滑后的显示值)
        auto barL = area.removeFromTop(barHeight);
        drawPeakBar(g, barL, displayPeakL, peakHoldL);

        // Gap
        area.removeFromTop(barGap);

        // Draw R channel bar (✅ 使用平滑后的显示值)
        auto barR = area.removeFromTop(barHeight);
        drawPeakBar(g, barR, displayPeakR, peakHoldR);

        // Draw scale ticks (Levels.tsx lines 154-161)
        g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.1f));
        g.setFont(10.0f);

        // ✅ 获取准确的上下边界（相对于 area）
        float lineTop = static_cast<float>(barL.getY());
        float lineBottom = static_cast<float>(barR.getBottom() + 4);

        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            // ✅ 使用 area 的宽度和 X 起点，而非原始 bounds
            float x = static_cast<float>(barL.getX()) + dbToX(static_cast<float>(db), static_cast<float>(barL.getWidth()));

            // Tick line (从 L 通道顶部画到 R 通道底部)
            g.drawVerticalLine(static_cast<int>(x), lineTop, lineBottom);

            // Label (贴在竖线底部)
            juce::String label = juce::String(db);
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.drawText(label,
                      static_cast<int>(x - 15), static_cast<int>(lineBottom + 2),
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
        // ✅ 响应式单位隐藏：提高阈值到 550px，确保绝对充足的物理空间
        bool showUnit = bounds.getWidth() > 550;

        // 🎯 舒适的大字体（绝对不准缩小或挤压变形！）
        const float valueFontSize = 22.0f;  // 恢复舒适的 22pt 大字体

        // Background box (Levels.tsx line 166)
        g.setColour(juce::Colour(0xFFEAEAEA));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // Border
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f, 2.0f);

        // 🎯 3-column grid layout with dynamic row heights
        auto gridBounds = bounds.reduced(16, 12);
        const int colWidth = gridBounds.getWidth() / 3;
        const int rowHeight = gridBounds.getHeight() / 2;  // 2 rows, equal height

        auto drawMetric = [&](int col, int row, const juce::String& label, float value, const juce::String& unit, bool highlight = false)
        {
            // 🎯 充分利用纵向空间！上下两行间距适中
            auto colBounds = juce::Rectangle<int>(
                gridBounds.getX() + col * colWidth,
                gridBounds.getY(),
                colWidth,
                gridBounds.getHeight()
            );

            juce::Rectangle<int> cellBounds;
            if (row == 0)
            {
                // 第一行：拿走上半截，并在底部砍掉 6px 作为间距
                cellBounds = colBounds.removeFromTop(colBounds.getHeight() / 2).reduced(0, 6);
            }
            else
            {
                // 第二行：拿走下半截，并在顶部砍掉 6px 作为间距
                cellBounds = colBounds.removeFromBottom(colBounds.getHeight() / 2).reduced(0, 6);
            }

            // 🎯 严格左右切分：40% 给标签，60% 给数值
            auto labelArea = cellBounds.removeFromLeft(static_cast<int>(cellBounds.getWidth() * 0.4f));
            auto valueArea = cellBounds;  // 剩下的 60% 全给数值

            // ✅ 左侧画标签（左对齐，稍小字体）
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText(label.toLowerCase(),
                      labelArea,
                      juce::Justification::centredLeft, false);

            // ✅ 右侧画数值和单位（右对齐，超大字体）
            juce::String valueStr = (value <= -60.0f) ? juce::String(juce::CharPointer_UTF8(u8"-∞")) : juce::String(value, 1);
            if (showUnit)
                valueStr += " " + unit;

            g.setColour(highlight ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::textMain);
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText(valueStr,
                      valueArea,
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
