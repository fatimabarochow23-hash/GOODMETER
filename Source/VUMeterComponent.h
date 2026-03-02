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
        auto bounds = getLocalBounds().toFloat();

        // Safety check
        if (bounds.isEmpty() || bounds.getHeight() < 20.0f)
            return;

        // Background
        g.fillAll(juce::Colours::white);

        // VU text space: shrinks when card is short, moves toward arc
        const float vuTextH = juce::jlimit(14.0f, 35.0f, bounds.getHeight() * 0.1f);
        const float bottomPad = juce::jlimit(3.0f, 12.0f, bounds.getHeight() * 0.03f);
        const float topPad = juce::jlimit(5.0f, 15.0f, bounds.getHeight() * 0.05f);
        const float sidePad = 15.0f;

        // Pivot point (arc center, near bottom)
        float cx = bounds.getCentreX();
        float cy = bounds.getBottom() - bottomPad - vuTextH;

        // Available space for the arc
        float topSpace = juce::jmax(10.0f, cy - bounds.getY() - topPad);
        float sideSpace = juce::jmax(10.0f, bounds.getWidth() / 2.0f - sidePad);

        // Radius: limited by VERTICAL space
        // Arc top is at y = cy - radius (angle=0, 12 o'clock)
        // So radius must be <= topSpace
        float radiusV = topSpace * 0.90f;

        // Half-angle: determined by horizontal space for this radius
        // Arc horizontal extent = radius * sin(halfAngle) <= sideSpace
        float sinVal = juce::jlimit(0.0f, 0.99f, sideSpace / juce::jmax(1.0f, radiusV));
        float naturalHalfAngle = std::asin(sinVal);
        float halfAngle = juce::jlimit(
            juce::MathConstants<float>::pi / 3.5f,    // min ~51°
            juce::MathConstants<float>::pi * 0.44f,    // max ~80°
            naturalHalfAngle
        );

        // If halfAngle was clamped to minimum, also cap radius by horizontal space
        float radiusH = sideSpace / std::sin(halfAngle);
        float radius = juce::jmax(15.0f, juce::jmin(radiusV, radiusH));

        float minAngle = -halfAngle;
        float maxAngle =  halfAngle;

        // 0 VU angle
        float zeroVuAngle = juce::jmap(0.0f, minVu, maxVu, minAngle, maxAngle);

        // Proportional arc thickness
        const float arcThickness = juce::jlimit(3.0f, 6.0f, radius * 0.04f);

        // Draw normal arc (-30 to 0)
        drawArc(g, cx, cy, radius, minAngle, zeroVuAngle, GoodMeterLookAndFeel::border, arcThickness);

        // Draw danger arc (0 to +3)
        drawArc(g, cx, cy, radius, zeroVuAngle, maxAngle, GoodMeterLookAndFeel::accentPink, arcThickness);

        // Draw ticks and labels
        drawTicksAndLabels(g, cx, cy, radius, minAngle, maxAngle);

        // Draw "VU" text below pivot, approaching arc as card shrinks
        const float vuFontSize = juce::jlimit(12.0f, 32.0f, vuTextH * 0.8f);
        g.setColour(GoodMeterLookAndFeel::border);
        g.setFont(juce::Font(vuFontSize, juce::Font::bold));
        g.drawText("VU",
                  static_cast<int>(bounds.getX()),
                  static_cast<int>(cy + 2.0f),
                  static_cast<int>(bounds.getWidth()),
                  static_cast<int>(vuTextH),
                  juce::Justification::centred, false);

        // Draw needle
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
        // Proportional tick dimensions
        const float majorTickLen = juce::jlimit(15.0f, 30.0f, radius * 0.2f);
        const float mediumTickLen = juce::jlimit(8.0f, 15.0f, radius * 0.1f);
        const float smallTickLen = juce::jlimit(5.0f, 10.0f, radius * 0.065f);
        const float tinyTickLen = juce::jlimit(3.0f, 7.0f, radius * 0.04f);
        const float majorTickWidth = juce::jlimit(2.5f, 4.0f, radius * 0.03f);
        const float mediumTickWidth = juce::jlimit(2.0f, 4.0f, radius * 0.025f);
        const float smallTickWidth = juce::jlimit(1.2f, 2.5f, radius * 0.015f);
        const float tinyTickWidth = juce::jlimit(0.8f, 1.5f, radius * 0.01f);
        const float labelFontSize = juce::jlimit(9.0f, 14.0f, radius * 0.1f);

        // === 1. Tiny ticks: every 1 dB from -30 to +3 (finest graduation) ===
        for (int db = -30; db <= 3; ++db)
        {
            const float angle = juce::jmap(static_cast<float>(db), minVu, maxVu, minAngle, maxAngle);
            const bool isDanger = (db > 0);
            const float innerR = radius - tinyTickLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour((isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border).withAlpha(0.25f));
            g.drawLine(x1, y1, x2, y2, tinyTickWidth);
        }

        // === 2. Small ticks: every 5 dB from -25 to -15, plus -7 ===
        const int smallTicks[] = { -25, -15, -7 };
        for (int tickVu : smallTicks)
        {
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);
            const bool isDanger = (tickVu > 0);
            const float innerR = radius - smallTickLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour((isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border).withAlpha(0.45f));
            g.drawLine(x1, y1, x2, y2, smallTickWidth);
        }

        // === 3. Medium ticks (labeled): -5, -3, -1, +1, +2, +3 ===
        const int mediumTicks[] = { -5, -3, -1, 1, 2, 3 };
        for (int tickVu : mediumTicks)
        {
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);
            const bool isDanger = (tickVu > 0);
            const float innerR = radius - mediumTickLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.drawLine(x1, y1, x2, y2, mediumTickWidth);

            // Label
            const float labelR = radius - mediumTickLen - juce::jlimit(5.0f, 10.0f, radius * 0.07f);
            const float lx = cx + std::sin(angle) * labelR;
            const float ly = cy - std::cos(angle) * labelR;
            juce::String labelText = (tickVu > 0) ? ("+" + juce::String(tickVu)) : juce::String(tickVu);

            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(labelFontSize, juce::Font::bold));
            g.drawText(labelText,
                      static_cast<int>(lx - 15), static_cast<int>(ly - 8),
                      30, 16,
                      juce::Justification::centred, false);
        }

        // === 4. Major ticks (labeled): -30, -20, -10, 0 ===
        const int majorTicks[] = { -30, -20, -10, 0 };
        for (int tickVu : majorTicks)
        {
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);
            const bool isZero = (tickVu == 0);
            const float tLen = isZero ? majorTickLen * 1.1f : majorTickLen;
            const float innerR = radius - tLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour(GoodMeterLookAndFeel::border);
            g.drawLine(x1, y1, x2, y2, majorTickWidth);

            // Label
            const float labelR = radius - tLen - juce::jlimit(5.0f, 10.0f, radius * 0.07f);
            const float lx = cx + std::sin(angle) * labelR;
            const float ly = cy - std::cos(angle) * labelR;
            juce::String labelText = juce::String(tickVu);

            g.setColour(GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(labelFontSize, juce::Font::bold));
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
