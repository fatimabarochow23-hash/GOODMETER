/*
  ==============================================================================
    StereoImageComponent.h
    GOODMETER - Stereo Field Visualization (LRMS Cylinders + Goniometer)

    🎨 混合架构 (Hybrid Architecture):
    - Left (40%): Zero-overflow clipping for LRMS cylinders
    - Right (60%): Offscreen ghosting buffer for high-performance Goniometer
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
/**
 * Stereo Field Visualization Component
 * Left (40%): LRMS Cylinder Meters with zero-overflow clipping
 * Right (60%): Goniometer/Lissajous Plot with offscreen ghosting
 */
class StereoImageComponent : public juce::Component,
                              public juce::Timer
{
public:
    //==========================================================================
    StereoImageComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Initialize sample buffers
        sampleBufferL.fill(0.0f);
        sampleBufferR.fill(0.0f);

        // Set fixed height
        setSize(100, 350);

        // Start 60Hz timer for smooth updates
        startTimerHz(60);
    }

    ~StereoImageComponent() override
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

        // Adaptive split: tubes get minimum guaranteed width
        const int minTubeWidth = juce::jmin(120, bounds.getWidth() / 2);
        const int spacing = juce::jlimit(5, 15, static_cast<int>(bounds.getWidth() * 0.03f));
        const int leftWidth = juce::jmax(minTubeWidth, static_cast<int>(bounds.getWidth() * 0.35f));

        auto leftBounds = bounds.removeFromLeft(leftWidth);
        bounds.removeFromLeft(spacing);  // Gap
        auto rightBounds = bounds;

        // Draw left panel: LRMS Cylinders (zero-overflow clipping)
        drawLRMSCylinders(g, leftBounds);

        // Draw right panel: Goniometer (clip to rightBounds to prevent bleeding)
        {
            juce::Graphics::ScopedSaveState clipState(g);
            g.reduceClipRegion(rightBounds);
            drawGoniometer(g, rightBounds);
        }
    }

    void resized() override
    {
        // Recreate offscreen buffer on resize
        goniometerImage = juce::Image();
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // Sample buffers for Goniometer (stores recent L/R pairs)
    static constexpr int bufferSize = GOODMETERAudioProcessor::stereoSampleBufferSize;
    std::array<float, bufferSize> sampleBufferL;
    std::array<float, bufferSize> sampleBufferR;
    int sampleCount = 0;

    // Current LRMS levels (raw from processor atomics)
    float currentL = -90.0f;
    float currentR = -90.0f;
    float currentM = -90.0f;
    float currentS = -90.0f;

    // 🎯 平滑插值显示值 (Lerp smoothing for silky animation)
    float displayL = -90.0f;
    float displayR = -90.0f;
    float displayM = -90.0f;
    float displayS = -90.0f;

    // 🎯 Offscreen ghosting buffer for Goniometer
    juce::Image goniometerImage;
    float lastGoniometerWidth = 0.0f;
    float lastGoniometerHeight = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // Update LRMS levels (RMS dB values from processor)
        currentL = audioProcessor.rmsLevelL.load(std::memory_order_relaxed);
        currentR = audioProcessor.rmsLevelR.load(std::memory_order_relaxed);
        currentM = audioProcessor.rmsLevelMid.load(std::memory_order_relaxed);
        currentS = audioProcessor.rmsLevelSide.load(std::memory_order_relaxed);

        // 🎯 Lerp 平滑插值：舒适的阻尼系数 (Silky smooth animation)
        const float smoothing = 0.35f;
        displayL += (currentL - displayL) * smoothing;
        displayR += (currentR - displayR) * smoothing;
        displayM += (currentM - displayM) * smoothing;
        displayS += (currentS - displayS) * smoothing;

        // 🎯 Pull stereo samples from processor FIFO (batch pop 512 samples)
        sampleCount = 0;
        float tempL[512];
        float tempR[512];

        if (audioProcessor.stereoSampleFifoL.pop(tempL, 512) &&
            audioProcessor.stereoSampleFifoR.pop(tempR, 512))
        {
            for (int i = 0; i < 512; ++i)
            {
                sampleBufferL[i] = tempL[i];
                sampleBufferR[i] = tempR[i];
            }
            sampleCount = 512;
        }

        // 🎯 Update Goniometer offscreen buffer (ghosting effect)
        updateGoniometerBuffer();

        repaint();
    }

    //==========================================================================
    /**
     * 🎯 Update Goniometer Offscreen Buffer (High-Performance Ghosting)
     * 全象限菱形矩阵 + 曼哈顿距离越界保护
     * This method runs in timerCallback, NOT in paint()!
     */
    void updateGoniometerBuffer()
    {
        auto bounds = getLocalBounds();
        // Match paint()'s adaptive split
        const int minTubeWidth = juce::jmin(120, bounds.getWidth() / 2);
        const int spacing = juce::jlimit(5, 15, static_cast<int>(bounds.getWidth() * 0.03f));
        const int leftWidth = juce::jmax(minTubeWidth, static_cast<int>(bounds.getWidth() * 0.35f));
        bounds.removeFromLeft(leftWidth + spacing);
        auto rightBounds = bounds;

        // Match drawGoniometer's proportional padding
        const float gonPad = juce::jmin(8.0f, rightBounds.getHeight() * 0.03f);
        auto localBounds = rightBounds.toFloat().reduced(gonPad, gonPad);
        const float cx = localBounds.getCentreX() - rightBounds.getX();
        const float cy = localBounds.getCentreY() - rightBounds.getY();

        const float labelMargin = juce::jmin(20.0f, localBounds.getHeight() * 0.1f);
        const float r = juce::jmax(5.0f, juce::jmin(localBounds.getWidth(), localBounds.getHeight()) / 2.0f - labelMargin);

        // Create or resize offscreen buffer
        if (goniometerImage.isNull() ||
            rightBounds.getWidth() != static_cast<int>(lastGoniometerWidth) ||
            rightBounds.getHeight() != static_cast<int>(lastGoniometerHeight))
        {
            goniometerImage = juce::Image(juce::Image::ARGB,
                                         juce::jmax(1, rightBounds.getWidth()),
                                         juce::jmax(1, rightBounds.getHeight()),
                                         true);
            // Initialize to black
            {
                juce::Graphics initG(goniometerImage);
                initG.fillAll(juce::Colours::black);
            }
            lastGoniometerWidth = static_cast<float>(rightBounds.getWidth());
            lastGoniometerHeight = static_cast<float>(rightBounds.getHeight());
        }

        // Slow black fade - allows density accumulation
        juce::Graphics imageG(goniometerImage);
        imageG.fillAll(juce::Colours::black.withAlpha(0.04f));

        // Draw audio samples as connected path
        if (sampleCount > 1)
        {
            juce::Path audioPath;
            const float scale = r * 0.8f;

            for (int i = 0; i < sampleCount; ++i)
            {
                const float sampleL = sampleBufferL[i];
                const float sampleR = sampleBufferR[i];

                const float mid = (sampleL + sampleR);
                const float side = (sampleR - sampleL);

                float x = cx + side * scale;
                float y = cy - mid * scale;

                const float dist = std::abs(x - cx) + std::abs(y - cy);
                if (dist > r)
                {
                    const float sf = r / dist;
                    x = cx + (x - cx) * sf;
                    y = cy + (y - cy) * sf;
                }

                if (i == 0)
                    audioPath.startNewSubPath(x, y);
                else
                    audioPath.lineTo(x, y);
            }

            // Low-alpha layers: each frame adds a little, overlap accumulates to bright
            // Layer 1: Wide diffuse glow (builds up atmosphere)
            imageG.setColour(GoodMeterLookAndFeel::accentPink.withAlpha(0.12f));
            imageG.strokePath(audioPath, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved));

            // Layer 2: Medium glow (builds up body)
            imageG.setColour(GoodMeterLookAndFeel::accentPink.brighter(0.2f).withAlpha(0.18f));
            imageG.strokePath(audioPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));

            // Layer 3: Hot core (accumulates to near-white in dense areas)
            imageG.setColour(juce::Colour(0xFFFFCCDD).withAlpha(0.25f));
            imageG.strokePath(audioPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved));
        }
    }

    //==========================================================================
    /**
     * Draw LRMS as four uniform cylinders with glow aesthetic
     */
    void drawLRMSCylinders(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        const float padX = juce::jmin(10.0f, bounds.getWidth() * 0.05f);
        const float padY = juce::jmin(15.0f, bounds.getHeight() * 0.05f);
        auto area = bounds.toFloat().reduced(padX, padY);

        if (area.getHeight() < 30.0f || area.getWidth() < 40.0f)
            return;

        const float labelSpace = juce::jlimit(12.0f, 22.0f, area.getHeight() * 0.09f);
        const float tubeHeight = area.getHeight() - labelSpace;

        // Four columns with gaps
        const float gap = juce::jlimit(4.0f, 12.0f, area.getWidth() * 0.03f);
        const float colWidth = (area.getWidth() - gap * 3.0f) / 4.0f;
        const float tubeWidth = juce::jmin(colWidth * 0.50f, 18.0f);

        const float dbMin = -60.0f;
        const float dbMax = 0.0f;

        auto normLevel = [&](float db) -> float {
            float n = juce::jlimit(0.0f, 1.0f, juce::jmap(db, dbMin, dbMax, 0.0f, 1.0f));
            return n < 0.005f ? 0.0f : n;
        };

        struct TubeConfig {
            juce::String label;
            float level;
            juce::Colour color;
        };

        TubeConfig tubes[4] = {
            { "L", normLevel(displayL), GoodMeterLookAndFeel::accentPink },
            { "R", normLevel(displayR), GoodMeterLookAndFeel::accentPink },
            { "M", normLevel(displayM), GoodMeterLookAndFeel::accentYellow },
            { "S", normLevel(displayS), GoodMeterLookAndFeel::accentGreen }
        };

        for (int i = 0; i < 4; ++i)
        {
            auto& tube = tubes[i];

            // Center the tube within its column
            float colX = area.getX() + i * (colWidth + gap);
            float tx = colX + (colWidth - tubeWidth) / 2.0f;
            float ty = area.getY();
            float tw = tubeWidth;
            float th = tubeHeight;
            float cornerR = tw / 2.0f;

            // Rounded-bottom cylinder path
            juce::Path tubePath;
            tubePath.addRoundedRectangle(tx, ty, tw, th, cornerR, cornerR,
                                          false, false, true, true);

            // === Clip-based liquid fill ===
            if (tube.level > 0.0f)
            {
                juce::Graphics::ScopedSaveState state(g);
                g.reduceClipRegion(tubePath);

                float fillH = th * tube.level;
                float fillY = ty + th - fillH;

                g.setColour(tube.color.withAlpha(0.65f));
                g.fillRect(tx - 1.0f, fillY, tw + 2.0f, fillH + 1.0f);

                // Meniscus highlight
                if (fillH > 2.0f)
                {
                    g.setColour(tube.color.brighter(0.5f).withAlpha(0.5f));
                    g.fillRect(tx - 1.0f, fillY, tw + 2.0f, 1.5f);
                }
            }

            // === Outer glow (neon halo) ===
            if (tube.level > 0.05f)
            {
                float glowAlpha = juce::jlimit(0.0f, 0.12f, tube.level * 0.12f);
                g.setColour(tube.color.withAlpha(glowAlpha));
                g.strokePath(tubePath, juce::PathStrokeType(5.0f));
            }

            // === Glass outline ===
            g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.20f));
            g.strokePath(tubePath, juce::PathStrokeType(1.5f));

            // === Glass highlight ===
            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.strokePath(tubePath, juce::PathStrokeType(0.8f));

            // === Tick marks ===
            g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.10f));
            for (int tick = 1; tick <= 3; ++tick)
            {
                float tickY = ty + tick * (th / 4.0f);
                float tickLen = (tick == 2) ? tw * 0.4f : tw * 0.25f;
                g.drawLine(tx + tw - tickLen, tickY, tx + tw, tickY, 0.6f);
            }

            // === Label ===
            const float fontSize = juce::jlimit(8.0f, 12.0f, labelSpace * 0.55f);
            const float labelY = ty + th + 1.0f;
            g.setColour(GoodMeterLookAndFeel::textMain);
            g.setFont(juce::Font(fontSize, juce::Font::bold));
            g.drawText(tube.label,
                       static_cast<int>(colX),
                       static_cast<int>(labelY),
                       static_cast<int>(colWidth),
                       static_cast<int>(labelSpace),
                       juce::Justification::centred, false);
        }
    }

    //==========================================================================
    /**
     * 🎨 Draw Goniometer/Lissajous Plot (Offscreen Buffer Rendering)
     * 全屏菱形矩阵设计（Diamond Matrix）
     */
    void drawGoniometer(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        // No inner border - parent MeterCard provides the frame

        // Proportional padding
        const float gonPad = juce::jmin(8.0f, bounds.getHeight() * 0.03f);
        auto localBounds = bounds.toFloat().reduced(gonPad, gonPad);
        const float cx = localBounds.getCentreX();
        const float cy = localBounds.getCentreY();
        const float labelMargin = juce::jmin(20.0f, localBounds.getHeight() * 0.1f);
        const float r = juce::jmax(5.0f, juce::jmin(localBounds.getWidth(), localBounds.getHeight()) / 2.0f - labelMargin);

        // Fill ONLY the diamond area with black
        {
            juce::Path diamondFill;
            diamondFill.startNewSubPath(cx, cy - r);
            diamondFill.lineTo(cx + r, cy);
            diamondFill.lineTo(cx, cy + r);
            diamondFill.lineTo(cx - r, cy);
            diamondFill.closeSubPath();

            g.setColour(juce::Colours::black);
            g.fillPath(diamondFill);
        }

        // Draw offscreen buffer (trails)
        if (!goniometerImage.isNull())
        {
            // Clip to diamond so trails don't bleed outside
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clipDiamond;
            clipDiamond.startNewSubPath(cx, cy - r);
            clipDiamond.lineTo(cx + r, cy);
            clipDiamond.lineTo(cx, cy + r);
            clipDiamond.lineTo(cx - r, cy);
            clipDiamond.closeSubPath();
            g.reduceClipRegion(clipDiamond);

            g.drawImageAt(goniometerImage, bounds.getX(), bounds.getY());
        }

        // Grid on top
        drawGoniometerGrid(g, cx, cy, r);
    }

    //==========================================================================
    /**
     * 🎨 Draw Diamond Grid (菱形网格)
     * 1. 外菱形边框
     * 2. 内菱形辅助线（半径 0.5r）
     * 3. 十字交叉线（M 轴垂直，S 轴水平）
     * 4. 标签文本：M, -M, L, R
     */
    void drawGoniometerGrid(juce::Graphics& g, float cx, float cy, float r)
    {
        // White grid lines on black background
        g.setColour(juce::Colours::white.withAlpha(0.25f));

        // ========================================================================
        // 1️⃣ 外菱形边框（连接上、右、下、左四个顶点）
        // ========================================================================
        juce::Path outerDiamond;
        outerDiamond.startNewSubPath(cx, cy - r);        // 上顶点
        outerDiamond.lineTo(cx + r, cy);                 // 右顶点
        outerDiamond.lineTo(cx, cy + r);                 // 下顶点
        outerDiamond.lineTo(cx - r, cy);                 // 左顶点
        outerDiamond.closeSubPath();                     // 回到上顶点

        g.strokePath(outerDiamond, juce::PathStrokeType(1.0f));

        // ========================================================================
        // 2️⃣ 内菱形辅助线（半径为 r * 0.5f）
        // ========================================================================
        const float innerR = r * 0.5f;
        juce::Path innerDiamond;
        innerDiamond.startNewSubPath(cx, cy - innerR);
        innerDiamond.lineTo(cx + innerR, cy);
        innerDiamond.lineTo(cx, cy + innerR);
        innerDiamond.lineTo(cx - innerR, cy);
        innerDiamond.closeSubPath();

        g.strokePath(innerDiamond, juce::PathStrokeType(0.8f));

        // ========================================================================
        // 3️⃣ 十字交叉线（M 轴垂直，S 轴水平）
        // ========================================================================
        // M 轴（垂直线，从上到下）
        g.drawLine(cx, cy - r, cx, cy + r, 1.0f);

        // S 轴（水平线，从左到右）
        g.drawLine(cx - r, cy, cx + r, cy, 1.0f);

        // ========================================================================
        // 4️⃣ 标签文本（M, -M, L, R）
        // ========================================================================
        g.setColour(juce::Colour(0xff6a6a75));
        const float gridFontSize = juce::jlimit(8.0f, 11.0f, r * 0.1f);
        const float labelOff = juce::jmin(18.0f, r * 0.15f);
        g.setFont(juce::Font(gridFontSize, juce::Font::bold));

        // M: 正上方
        g.drawFittedText("M",
                        static_cast<int>(cx - 15),
                        static_cast<int>(cy - r - labelOff),
                        30, 20,
                        juce::Justification::centred, 1);

        // -M: 正下方
        g.drawFittedText("-M",
                        static_cast<int>(cx - 15),
                        static_cast<int>(cy + r + labelOff * 0.2f),
                        30, 20,
                        juce::Justification::centred, 1);

        // L: 左端点外
        g.drawFittedText("L",
                        static_cast<int>(cx - r - labelOff - 5),
                        static_cast<int>(cy - 10),
                        30, 20,
                        juce::Justification::centred, 1);

        // R: 右端点外
        g.drawFittedText("R",
                        static_cast<int>(cx + r + labelOff * 0.2f - 5),
                        static_cast<int>(cy - 10),
                        30, 20,
                        juce::Justification::centred, 1);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoImageComponent)
};
