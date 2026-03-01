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

        // 🎨 40/60 Split Layout
        const int leftWidth = static_cast<int>(bounds.getWidth() * 0.4f);
        const int spacing = 15;

        auto leftBounds = bounds.removeFromLeft(leftWidth);
        bounds.removeFromLeft(spacing);  // Gap
        auto rightBounds = bounds;

        // Draw left panel: LRMS Cylinders (zero-overflow clipping)
        drawLRMSCylinders(g, leftBounds);

        // Draw right panel: Goniometer (offscreen buffer)
        drawGoniometer(g, rightBounds);
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
        const int leftWidth = static_cast<int>(bounds.getWidth() * 0.4f);
        bounds.removeFromLeft(leftWidth + 15);  // 切掉左侧量筒区域
        auto rightBounds = bounds;

        // 🎯 菱形中心 = 右侧面板的绝对中心点（与 drawGoniometer 完全一致）
        auto localBounds = rightBounds.toFloat().reduced(15, 15);
        const float cx = localBounds.getCentreX() - rightBounds.getX();
        const float cy = localBounds.getCentreY() - rightBounds.getY();

        // 🎯 半径 = 宽高的一半（取最小值，留 10px 安全边距）
        const float r = juce::jmin(localBounds.getWidth(), localBounds.getHeight()) / 2.0f - 10.0f;

        // Create or resize offscreen buffer
        if (goniometerImage.isNull() ||
            rightBounds.getWidth() != static_cast<int>(lastGoniometerWidth) ||
            rightBounds.getHeight() != static_cast<int>(lastGoniometerHeight))
        {
            goniometerImage = juce::Image(juce::Image::ARGB,
                                         juce::jmax(1, rightBounds.getWidth()),
                                         juce::jmax(1, rightBounds.getHeight()),
                                         true);
            lastGoniometerWidth = static_cast<float>(rightBounds.getWidth());
            lastGoniometerHeight = static_cast<float>(rightBounds.getHeight());
        }

        // 🎨 纯白快速褪色 (Fast White Fade - 极简主义美学)
        // 使用白色制造褪色感，提高 Alpha 值加快褪色速度，防止毛线球堆积
        juce::Graphics imageG(goniometerImage);
        imageG.fillAll(juce::Colours::white.withAlpha(0.2f));  // ✅ 快速褪色 (0.2f = 迅速淡化旧线条)

        // 🎯 Draw new audio samples as connected line path
        if (sampleCount > 1)
        {
            juce::Path audioPath;

            // 🎯 调整合适的放大比例（确保信号撑满菱形）
            const float scale = r * 0.8f;

            // Build path by connecting all points
            for (int i = 0; i < sampleCount; ++i)
            {
                const float sampleL = sampleBufferL[i];
                const float sampleR = sampleBufferR[i];

                // 🎯 M/S transformation - 全象限菱形矩阵（删除 abs！）
                const float mid = (sampleL + sampleR);   // ✅ 允许负数进入下半菱形
                const float side = (sampleR - sampleL);  // X 轴（立体声宽度）

                // 🔬 Math: x = cx + side * scale, y = cy - mid * scale
                const float x = cx + side * scale;
                const float y = cy - mid * scale;

                // 🎯 菱形越界保护（曼哈顿距离算法 - 极其关键！）
                // 计算点到中心的曼哈顿距离（|Δx| + |Δy|）
                const float dist = std::abs(x - cx) + std::abs(y - cy);

                float finalX = x;
                float finalY = y;

                if (dist > r)
                {
                    // 超出菱形边界，按比例缩放回边缘
                    const float scaleFactor = r / dist;
                    finalX = cx + (x - cx) * scaleFactor;
                    finalY = cy + (y - cy) * scaleFactor;
                }

                // Add to path
                if (i == 0)
                    audioPath.startNewSubPath(finalX, finalY);
                else
                    audioPath.lineTo(finalX, finalY);
            }

            // 🌟 白底镭射核心 (Neon Core on White - 极简主义锐利线条)
            // 双层渲染：轻柔光晕 + 极致锐利核心

            // 1️⃣ 轻柔光晕 (Outer Glow)：轻度扩散，防止白底被染红
            imageG.setColour(GoodMeterLookAndFeel::accentPink.withAlpha(0.25f));  // ✅ 0.35f → 0.25f (更轻柔)
            imageG.strokePath(audioPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));  // ✅ 6.0f → 4.0f (适中光晕)

            // 2️⃣ 极致锐利能量核心 (Solid Core)：绝对不透明的实心线
            imageG.setColour(GoodMeterLookAndFeel::accentPink.withAlpha(1.0f));  // ✅ 彻底不透明
            imageG.strokePath(audioPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved));  // ✅ 1.0f → 1.2f (更锐利)
        }
    }

    //==========================================================================
    /**
     * 🎨 Draw LRMS Cylinder Meters (Industrial Test Tube Design)
     * 严格对标 Web 版化学试管设计
     */
    void drawLRMSCylinders(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        // 🎨 Draw rounded border
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(2.0f), 6.0f, 2.0f);

        // 1️⃣ 严谨的网格阵列计算 (The Grid)
        auto area = bounds.toFloat().reduced(10.0f, 20.0f);  // 留出四周空白

        const float tubeWidth = juce::jmin(16.0f, area.getWidth() / 8.0f);  // 试管宽度
        const float spacing = (area.getWidth() - tubeWidth * 4.0f) / 3.0f;  // 试管间距
        const float tubeHeight = area.getHeight() - 25.0f;  // 留出底部画字母的空间

        // dB range for level mapping
        const float minDb = -60.0f;
        const float maxDb = 0.0f;

        // 2️⃣ 定义四根试管配置 (The 4 Tubes)
        struct TubeConfig {
            juce::String label;
            float valueDb;
            juce::Colour color;
        };

        // ✅ 使用平滑后的 display 变量进行绘制
        TubeConfig tubes[4] = {
            { "L", displayL, GoodMeterLookAndFeel::accentPink },
            { "R", displayR, GoodMeterLookAndFeel::accentPink },
            { "M", displayM, GoodMeterLookAndFeel::accentYellow },
            { "S", displayS, GoodMeterLookAndFeel::accentGreen }
        };

        // 3️⃣ 循环绘制四根试管
        for (int i = 0; i < 4; ++i)
        {
            auto& tube = tubes[i];

            // 计算当前试管的绘图区域
            juce::Rectangle<float> tubeBounds(
                area.getX() + i * (tubeWidth + spacing),
                area.getY(),
                tubeWidth,
                tubeHeight
            );

            // 4️⃣ 创建平头 U 型试管路径 (The U-Tube Path)
            // 使用底部圆角矩形，确保顶部平直、底部圆润
            juce::Path tubePath;
            tubePath.addRoundedRectangle(
                tubeBounds.getX(),
                tubeBounds.getY(),
                tubeBounds.getWidth(),
                tubeBounds.getHeight(),
                tubeWidth / 2.0f  // 圆角半径 = 试管宽度的一半
            );

            // 5️⃣ 零溢出裁剪法填充液体 (Zero-Overflow Fill - 核心！)
            // 计算液体填充比例 (0.0 ~ 1.0)
            const float levelNorm = juce::jmap(tube.valueDb, minDb, maxDb, 0.0f, 1.0f);
            const float clampedLevel = juce::jlimit(0.0f, 1.0f, levelNorm);

            if (clampedLevel > 0.0f)
            {
                // 使用裁剪区域确保液体完美贴合试管底部圆角
                juce::Graphics::ScopedSaveState state(g);
                g.reduceClipRegion(tubePath);  // 🔒 裁剪生效！

                // 计算液体矩形（从底部向上填充）
                const float liquidY = tubeBounds.getBottom() - clampedLevel * tubeBounds.getHeight();
                juce::Rectangle<float> liquidBounds(
                    tubeBounds.getX(),
                    liquidY,
                    tubeBounds.getWidth(),
                    tubeBounds.getHeight()
                );

                // 填充液体颜色
                g.setColour(tube.color.withAlpha(0.8f));
                g.fillRect(liquidBounds);

            } // state 结束，裁剪区恢复

            // 6️⃣ 绘制深色工业外框 (Industrial Border)
            g.setColour(juce::Colour(0xff2a2a35).withAlpha(0.9f));
            g.strokePath(tubePath, juce::PathStrokeType(1.5f));

            // 7️⃣ 绘制右侧刻度线 (Ticks on Right Side)
            g.setColour(juce::Colours::grey.withAlpha(0.5f));
            for (int tick = 1; tick <= 3; ++tick)
            {
                const float tickY = tubeBounds.getY() + tick * (tubeBounds.getHeight() / 4.0f);
                g.drawLine(
                    tubeBounds.getRight(), tickY,
                    tubeBounds.getRight() + 4.0f, tickY,
                    1.0f
                );
            }

            // 8️⃣ 底部字母精准居中 (Bottom Label Centered)
            const float labelY = tubeBounds.getBottom() + 5.0f;
            g.setColour(GoodMeterLookAndFeel::textMain);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText(
                tube.label,
                static_cast<int>(tubeBounds.getX()),
                static_cast<int>(labelY),
                static_cast<int>(tubeBounds.getWidth()),
                20,
                juce::Justification::centred,
                false
            );
        }
    }

    //==========================================================================
    /**
     * 🎨 Draw Goniometer/Lissajous Plot (Offscreen Buffer Rendering)
     * 全屏菱形矩阵设计（Diamond Matrix）
     */
    void drawGoniometer(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        // 🎨 Draw rounded border
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(2.0f), 6.0f, 2.0f);

        // 🎯 菱形中心 = 右侧面板的绝对中心点
        auto localBounds = bounds.toFloat().reduced(15, 15);
        const float cx = localBounds.getCentreX();
        const float cy = localBounds.getCentreY();

        // 🎯 半径 = 宽高的一半（取最小值，留 10px 安全边距）
        const float r = juce::jmin(localBounds.getWidth(), localBounds.getHeight()) / 2.0f - 10.0f;

        // 🎯 CORRECT LAYER ORDER: Draw offscreen buffer FIRST (trails in background)
        if (!goniometerImage.isNull())
        {
            g.drawImageAt(goniometerImage, bounds.getX(), bounds.getY());
        }

        // 🎯 Then draw grid ON TOP (grid remains crisp and visible)
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
        // 🎨 极其微弱的细线
        g.setColour(juce::Colours::grey.withAlpha(0.2f));

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
        g.setFont(juce::Font(11.0f, juce::Font::bold));

        // M: 正上方（外扩 10px）
        g.drawFittedText("M",
                        static_cast<int>(cx - 15),
                        static_cast<int>(cy - r - 20),
                        30, 20,
                        juce::Justification::centred, 1);

        // -M: 正下方（外扩 10px）
        g.drawFittedText("-M",
                        static_cast<int>(cx - 15),
                        static_cast<int>(cy + r + 5),
                        30, 20,
                        juce::Justification::centred, 1);

        // L: 左端点外（外扩 10px）
        g.drawFittedText("L",
                        static_cast<int>(cx - r - 25),
                        static_cast<int>(cy - 10),
                        30, 20,
                        juce::Justification::centred, 1);

        // R: 右端点外（外扩 10px）
        g.drawFittedText("R",
                        static_cast<int>(cx + r - 5),
                        static_cast<int>(cy - 10),
                        30, 20,
                        juce::Justification::centred, 1);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoImageComponent)
};
