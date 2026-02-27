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
        // ✅ FIX 1: 强制设定合理边界
        setSize(500, 220);  // 增加高度以容纳完整刻度盘

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
        float cy = bounds.getBottom() - 30.0f;
        float radius = bounds.getWidth() * 0.4f;

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
     */
    void updateVU(float rmsL, float rmsR)
    {
        // 1. Calculate max RMS (ClassicVUMeter.tsx line 32)
        const float rms = std::max(rmsL, rmsR);

        // 2. Strict VU math (ClassicVUMeter.tsx lines 35-36)
        // dBFS = 20 * log10(rms + epsilon)
        float dbfs = 20.0f * std::log10(rms + 0.00001f);
        float vu = dbfs;  // Direct 1:1 mapping to dBFS

        // 3. Normalize to 0.0-1.0 range (ClassicVUMeter.tsx lines 42-43)
        float targetLevel = (vu - minVu) / (maxVu - minVu);
        targetLevel = juce::jlimit(0.0f, 1.0f, targetLevel);

        // 4. Apply ballistics (smoothing) (ClassicVUMeter.tsx line 46)
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
        // Map current VU display value to angle using jmap
        const float mappedAngle = juce::jmap(currentVuDisplay, 0.0f, 1.0f, minAngle, maxAngle);

        // Needle length extends slightly past arc
        const float needleLength = radius * 0.9f;

        // Create vertical needle path pointing straight up (12 o'clock direction)
        juce::Path needle;
        needle.startNewSubPath(centerX, centerY);
        needle.lineTo(centerX, centerY - needleLength);

        // Save graphics state and apply rotation transform
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(mappedAngle, centerX, centerY));

        // Draw rotated needle
        g.setColour(GoodMeterLookAndFeel::border);
        g.strokePath(needle, juce::PathStrokeType(8.0f));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeterComponent)
};
