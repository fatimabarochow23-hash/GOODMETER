/*
  ==============================================================================
    VUMeterComponent.h
    GOODMETER - Classic VU Meter with ballistics

    Translated from ClassicVUMeter.tsx
    Features: Flat arc, needle animation with damping, dual color zones
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
/**
 * Classic VU Meter Component with ballistic damping
 * Range: -30 VU to +3 VU
 * Dual zones: normal (-30 to 0) and danger (0 to +3)
 */
class VUMeterComponent : public juce::Component,
                         public juce::Timer
{
public:
    //==========================================================================
    VUMeterComponent()
    {
        // ✅ 只设置高度，宽度由父容器（MeterCard）控制
        setSize(100, 220);  // 初始宽度会被父容器覆盖

        // Start 60Hz timer for smooth needle animation
        startTimerHz(60);
    }

    ~VUMeterComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        // ✅ 1. 动态中心与半径（绝不写死）
        auto bounds = getLocalBounds().toFloat();

        // Safety check
        if (bounds.isEmpty())
            return;

        // Background
        g.fillAll(juce::Colours::white);

        // 底部留边距，圆心在下方，画半圆
        float cx = bounds.getCentreX();
        float cy = bounds.getBottom() - 20.0f;  // 底部留点边距

        // 🔒 安全半径上限保护（防止削顶）
        // 1. 按宽度计算的理想半径
        float radiusByWidth = bounds.getWidth() * 0.4f;

        // 2. 按高度计算的极限半径（顶部留出 10px 安全区防止削顶）
        float radiusByHeight = cy - bounds.getY() - 10.0f;

        // 3. 取两者的最小值！这样无论怎么拉伸，都不会冲出盒子！
        float radius = juce::jmin(radiusByWidth, radiusByHeight);

        // ✅ 2. 强制使用正确的弧度范围
        // JUCE: 0° = 12点钟方向（正上方）
        // VU 表从 -60° 到 +60° 摆动
        float minAngle = -juce::MathConstants<float>::pi / 3.0f;  // -60°
        float maxAngle = juce::MathConstants<float>::pi / 3.0f;   // +60°

        // 计算 0 VU 的角度位置
        float zeroVuAngle = juce::jmap(0.0f, minVu, maxVu, minAngle, maxAngle);

        // Draw normal arc (-30 to 0)
        drawArc(g, cx, cy, radius, minAngle, zeroVuAngle, GoodMeterLookAndFeel::border, 6.0f);

        // Draw danger arc (0 to +3)
        drawArc(g, cx, cy, radius, zeroVuAngle, maxAngle, GoodMeterLookAndFeel::accentPink, 6.0f);

        // Draw ticks and labels
        drawTicksAndLabels(g, cx, cy, radius, minAngle, maxAngle);

        // Draw "VU" text
        g.setColour(GoodMeterLookAndFeel::border);
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        auto textBounds = bounds.removeFromBottom(50);
        g.drawText("VU", textBounds, juce::Justification::centred, false);

        // ✅ 3. 完美的指针旋转法 (AffineTransform)
        drawNeedle(g, cx, cy, radius, minAngle, maxAngle);
    }

    void resized() override
    {
        // No child components
    }

    //==========================================================================
    /**
     * Update VU value from processor (called from PluginEditor::timerCallback)
     *
     * CRITICAL: Processor 传入的已经是 dB 值（rmsL_dB, rmsR_dB）
     * 不要再做 log10 转换！
     */
    void updateVU(float rmsL_dB, float rmsR_dB)
    {
        // 1. Calculate max RMS in dB (ClassicVUMeter.tsx line 32)
        const float vu_dB = std::max(rmsL_dB, rmsR_dB);

        // 2. Normalize to 0.0-1.0 range (ClassicVUMeter.tsx lines 42-43)
        // VU range: -30 dB to +3 dB
        float targetLevel = (vu_dB - minVu) / (maxVu - minVu);
        targetLevel = juce::jlimit(0.0f, 1.0f, targetLevel);

        // 3. Apply ballistics (smoothing) (ClassicVUMeter.tsx line 46)
        currentVuDisplay += (targetLevel - currentVuDisplay) * vuSmoothing;

        repaint();
    }

private:
    //==========================================================================
    // VU range constants (ClassicVUMeter.tsx lines 39-40)
    static constexpr float minVu = -30.0f;
    static constexpr float maxVu = 3.0f;

    // Ballistics (ClassicVUMeter.tsx line 15)
    static constexpr float vuSmoothing = 0.08f;

    // Current display value (0.0 to 1.0)
    float currentVuDisplay = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // Smooth animation handled in updateVU()
    }

    //==========================================================================
    /**
     * Draw circular arc using juce::Path
     */
    void drawArc(juce::Graphics& g,
                 float centerX, float centerY, float radius,
                 float startAngle, float endAngle,
                 const juce::Colour& colour, float lineWidth)
    {
        juce::Path arcPath;
        arcPath.addCentredArc(centerX, centerY, radius, radius,
                             0.0f,  // rotation
                             startAngle, endAngle,
                             true);  // startAsNewSubPath

        g.setColour(colour);
        g.strokePath(arcPath, juce::PathStrokeType(lineWidth));
    }

    //==========================================================================
    /**
     * Draw ticks and labels
     */
    void drawTicksAndLabels(juce::Graphics& g,
                           float cx, float cy, float radius,
                           float minAngle, float maxAngle)
    {
        // Tick positions
        const int ticks[] = { -30, -20, -10, -5, -3, -1, 0, 1, 2, 3 };

        for (int tickVu : ticks)
        {
            // Map VU value to angle using juce::jmap
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);

            const bool isDanger = (tickVu > 0);
            const bool isZero = (tickVu == 0);

            // Tick dimensions
            const float tickLength = isZero ? 30.0f : 15.0f;
            const float innerRadius = radius - tickLength;

            // Tick endpoints (从圆心向外辐射)
            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerRadius;
            const float y2 = cy - std::cos(angle) * innerRadius;

            // Draw tick line
            juce::Line<float> tickLine(x1, y1, x2, y2);
            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.drawLine(tickLine, isZero ? 4.0f : 3.0f);

            // Draw label
            const float labelRadius = radius - tickLength - 10.0f;
            const float lx = cx + std::sin(angle) * labelRadius;
            const float ly = cy - std::cos(angle) * labelRadius;

            juce::String labelText = (tickVu > 0) ? ("+" + juce::String(tickVu)) : juce::String(tickVu);

            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText(labelText,
                      static_cast<int>(lx - 15), static_cast<int>(ly - 8),
                      30, 16,
                      juce::Justification::centred, false);
        }
    }

    //==========================================================================
    /**
     * Draw needle using AffineTransform rotation (CORRECT METHOD)
     *
     * Pattern from user's template:
     * 1. Map currentVuDisplay (0.0-1.0) to angle range using jmap
     * 2. Create vertical needle path pointing straight up (12 o'clock)
     * 3. Save graphics state with ScopedSaveState
     * 4. Apply rotation transform around pivot point (cx, cy)
     * 5. Stroke the transformed path
     */
    void drawNeedle(juce::Graphics& g,
                   float centerX, float centerY, float radius,
                   float minAngle, float maxAngle)
    {
        // 🔒 CRITICAL: 数值安全锁 - 防止 NaN/Infinity 炸毁 AffineTransform
        float safeVuDisplay = currentVuDisplay;

        // 检查并修复 NaN/Infinity
        if (std::isnan(safeVuDisplay) || std::isinf(safeVuDisplay))
            safeVuDisplay = 0.0f;  // 重置到最小位置

        // 严格限幅到 0.0-1.0 范围
        safeVuDisplay = juce::jlimit(0.0f, 1.0f, safeVuDisplay);

        // Map current VU display value to angle using jmap
        const float mappedAngle = juce::jmap(safeVuDisplay, 0.0f, 1.0f, minAngle, maxAngle);

        // Needle length extends slightly past arc
        const float needleLength = radius * 0.9f;

        // Create vertical needle path pointing straight up (12 o'clock direction)
        juce::Path needle;
        needle.startNewSubPath(centerX, centerY);
        needle.lineTo(centerX, centerY - needleLength);

        // 🎨 Z-Index 正确顺序：先画所有背景，最后画指针
        // Save graphics state and apply rotation transform
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(mappedAngle, centerX, centerY));

        // 🔴 Draw rotated needle in RED (highly visible)
        g.setColour(juce::Colours::red);
        g.strokePath(needle, juce::PathStrokeType(3.0f));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeterComponent)
};
