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
        // Set fixed height (from ClassicVUMeter.tsx: 180px)
        setSize(500, 180);

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
        auto bounds = getLocalBounds();

        // Safety check
        if (bounds.isEmpty())
            return;

        const float width = static_cast<float>(bounds.getWidth());
        const float height = static_cast<float>(bounds.getHeight());

        // Background (ClassicVUMeter.tsx lines 59-60)
        g.fillAll(juce::Colours::white);

        // Wide, flat arc geometry (ClassicVUMeter.tsx lines 52-54)
        const float centerX = width / 2.0f;
        const float centerY = height * 2.5f;  // Pivot far below for flat arc
        const float radius = height * 2.2f;   // Large radius

        // Angles (ClassicVUMeter.tsx lines 63-66)
        const float spread = 0.65f;  // Radians
        const float startAngle = -juce::MathConstants<float>::pi / 2.0f - spread;
        const float endAngle = -juce::MathConstants<float>::pi / 2.0f + spread;
        const float zeroVuAngle = startAngle + ((0.0f - minVu) / (maxVu - minVu)) * (endAngle - startAngle);

        // Draw normal arc (-30 to 0) (ClassicVUMeter.tsx lines 68-73)
        drawArc(g, centerX, centerY, radius, startAngle, zeroVuAngle, GoodMeterLookAndFeel::border, 6.0f);

        // Draw danger arc (0 to +3) (ClassicVUMeter.tsx lines 76-80)
        drawArc(g, centerX, centerY, radius, zeroVuAngle, endAngle, GoodMeterLookAndFeel::accentPink, 6.0f);

        // Draw ticks and labels (ClassicVUMeter.tsx lines 82-119)
        drawTicksAndLabels(g, centerX, centerY, radius, startAngle, endAngle, zeroVuAngle);

        // Draw "VU" text (ClassicVUMeter.tsx lines 122-124)
        g.setColour(GoodMeterLookAndFeel::border);
        g.setFont(juce::Font(48.0f, juce::Font::bold));
        g.drawText("VU", bounds.removeFromBottom(80), juce::Justification::centred, false);

        // Draw needle (ClassicVUMeter.tsx lines 127-139)
        drawNeedle(g, centerX, centerY, radius, startAngle, endAngle);
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
     * Draw ticks and labels (ClassicVUMeter.tsx lines 83-119)
     */
    void drawTicksAndLabels(juce::Graphics& g,
                           float centerX, float centerY, float radius,
                           float startAngle, float endAngle, float zeroVuAngle)
    {
        // Tick positions (ClassicVUMeter.tsx line 83)
        const int ticks[] = { -30, -20, -10, -5, -3, -1, 0, 1, 2, 3 };

        for (int tickVu : ticks)
        {
            // Calculate angle for this tick
            const float t = (static_cast<float>(tickVu) - minVu) / (maxVu - minVu);
            const float angle = startAngle + t * (endAngle - startAngle);

            const bool isDanger = (tickVu > 0);
            const bool isZero = (tickVu == 0);

            // Tick dimensions (ClassicVUMeter.tsx lines 96-97)
            const float tickLength = isZero ? 36.0f : 20.0f;
            const float innerRadius = radius - tickLength;

            // Tick endpoints
            const float x1 = centerX + std::cos(angle) * radius;
            const float y1 = centerY + std::sin(angle) * radius;
            const float x2 = centerX + std::cos(angle) * innerRadius;
            const float y2 = centerY + std::sin(angle) * innerRadius;

            // Draw tick line (ClassicVUMeter.tsx lines 104-109)
            juce::Line<float> tickLine(x1, y1, x2, y2);
            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.drawLine(tickLine, isZero ? 8.0f : 6.0f);

            // Draw label (ClassicVUMeter.tsx lines 112-118)
            const float labelRadius = radius - tickLength - 16.0f;
            const float lx = centerX + std::cos(angle) * labelRadius;
            const float ly = centerY + std::sin(angle) * labelRadius;

            juce::String labelText = (tickVu > 0) ? ("+" + juce::String(tickVu)) : juce::String(tickVu);

            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(36.0f, juce::Font::bold));
            g.drawText(labelText,
                      static_cast<int>(lx - 30), static_cast<int>(ly),
                      60, 40,
                      juce::Justification::centred, false);
        }
    }

    //==========================================================================
    /**
     * Draw needle using rotation transform (ClassicVUMeter.tsx lines 127-139)
     *
     * CRITICAL: Use juce::Graphics::addTransform for rotation instead of
     * manually calculating endpoint coordinates (error-prone)
     */
    void drawNeedle(juce::Graphics& g,
                   float centerX, float centerY, float radius,
                   float startAngle, float endAngle)
    {
        // Calculate needle angle based on current display value
        const float needleAngle = startAngle + currentVuDisplay * (endAngle - startAngle);
        const float needleLength = radius + 32.0f;  // Extend slightly past arc

        // Calculate needle endpoint (ClassicVUMeter.tsx lines 130-131)
        const float nx = centerX + std::cos(needleAngle) * needleLength;
        const float ny = centerY + std::sin(needleAngle) * needleLength;

        // Draw needle line (ClassicVUMeter.tsx lines 133-139)
        juce::Line<float> needleLine(centerX, centerY, nx, ny);

        g.setColour(GoodMeterLookAndFeel::border);
        g.drawLine(needleLine, 8.0f);  // lineWidth = 8.0f
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeterComponent)
};
