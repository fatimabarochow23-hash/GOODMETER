/*
  ==============================================================================
    SpectrogramComponent.h
    GOODMETER - Waterfall Spectrogram (Phase 3.5)

    High-performance ring buffer rendering (NO image copy!)
    Features: 60Hz update, logarithmic Y-axis, smooth color gradient
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
/**
 * Waterfall Spectrogram Component
 * Ring buffer rendering: draws new column, wraps around without image copy
 * Y-axis: 20Hz (bottom) to 20kHz (top) with logarithmic mapping
 * Color: -90dB (transparent gray) → -45dB (pink) → 0dB (bright yellow)
 */
class SpectrogramComponent : public juce::Component,
                               public juce::Timer
{
public:
    //==========================================================================
    SpectrogramComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Set fixed height
        setSize(100, 300);

        // Start 60Hz timer for smooth waterfall animation
        startTimerHz(60);
    }

    ~SpectrogramComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // ✅ 背景必须是干净的白色（通透感）
        g.fillAll(juce::Colours::white);

        // Safety check
        if (spectrogramImage.isNull() || bounds.isEmpty())
            return;

        const int width = bounds.getWidth();
        const int height = bounds.getHeight();

        // 🎨 零开销环形渲染：分两段拼接，产生"向左流动"错觉
        // 1. 将原图从 drawX 到末尾的"老数据"，画在屏幕左侧
        if (width - drawX > 0)
        {
            g.drawImage(spectrogramImage,
                       0, 0, width - drawX, height,                // 目标区域 (Dest)
                       drawX, 0, width - drawX, height);           // 源区域 (Source)
        }

        // 2. 将原图从 0 到 drawX 的"新数据"，画在屏幕右侧
        if (drawX > 0)
        {
            g.drawImage(spectrogramImage,
                       width - drawX, 0, drawX, height,            // 目标区域 (Dest)
                       0, 0, drawX, height);                       // 源区域 (Source)
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // 重新分配离屏缓冲区（宽高变化时）
        if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
        {
            spectrogramImage = juce::Image(juce::Image::ARGB,
                                          bounds.getWidth(),
                                          bounds.getHeight(),
                                          true);  // Clear to transparent
            drawX = 0;  // 重置游标
        }
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // 离屏缓冲区与环形游标
    juce::Image spectrogramImage;
    int drawX = 0;

    // FFT data storage
    static constexpr int numBins = GOODMETERAudioProcessor::fftSize / 2;
    std::array<float, numBins> fftData;

    // Frequency range (logarithmic)
    static constexpr float minFreq = 20.0f;    // 20 Hz (bottom)
    static constexpr float maxFreq = 20000.0f; // 20 kHz (top)

    // dB range for color mapping
    static constexpr float minDb = -90.0f;
    static constexpr float maxDb = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // 🎯 从 processor 拉取最新 FFT 数据
        if (!audioProcessor.fftFifoL.pop(fftData.data(), numBins))
            return;  // 没有新数据

        if (spectrogramImage.isNull())
            return;

        const int height = spectrogramImage.getHeight();
        if (height <= 0)
            return;

        // 🎨 创建离屏 Graphics 上下文
        juce::Graphics g(spectrogramImage);

        // 绘制单列像素（从上到下）
        for (int y = 0; y < height; ++y)
        {
            // ✅ Y 轴反转：top (y=0) = 20kHz, bottom (y=height-1) = 20Hz
            const float freq = yToFrequency(y, height);

            // 获取该频率的幅度
            const float magnitude = getMagnitudeAtFrequency(freq);
            const float db = magnitudeToDb(magnitude);

            // 映射为粉色能量流颜色
            const juce::Colour colour = getColourForDb(db);

            // 🚀 极速写入：使用 setPixelAt 直接写入像素
            spectrogramImage.setPixelAt(drawX, y, colour);
        }

        // 🔄 推进环形游标
        drawX = (drawX + 1) % spectrogramImage.getWidth();

        // 触发重绘
        repaint();
    }

    //==========================================================================
    /**
     * Convert Y pixel coordinate to frequency (Hz)
     * ✅ 反转映射：top (y=0) = 20kHz, bottom (y=height-1) = 20Hz
     */
    float yToFrequency(int y, int height) const
    {
        // 归一化：top (0) = 1.0, bottom (height-1) = 0.0
        const float normalized = 1.0f - (static_cast<float>(y) / static_cast<float>(height - 1));

        // Logarithmic interpolation
        const float logMin = std::log10(minFreq);
        const float logMax = std::log10(maxFreq);
        const float logFreq = logMin + normalized * (logMax - logMin);

        return std::pow(10.0f, logFreq);
    }

    /**
     * Convert frequency (Hz) to FFT bin index
     */
    int frequencyToBin(float freq) const
    {
        const float sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        const int bin = static_cast<int>((freq * GOODMETERAudioProcessor::fftSize) / sampleRate);
        return juce::jlimit(0, numBins - 1, bin);
    }

    /**
     * Get magnitude at specific frequency (with interpolation)
     */
    float getMagnitudeAtFrequency(float freq) const
    {
        const int bin = frequencyToBin(freq);
        return fftData[bin];
    }

    /**
     * Convert magnitude to dB
     */
    float magnitudeToDb(float magnitude) const
    {
        return 20.0f * std::log10(magnitude + 1e-8f);
    }

    /**
     * 🌸 粉色能量流调色板（全新审美）
     * -90dB: 完全透明白色（底噪消失）
     * -45dB: 半透明柔和粉色（能量体主体）
     * 0dB: 炽热发光粉色（峰值冲击）
     */
    juce::Colour getColourForDb(float db) const
    {
        // 归一化到 0.0-1.0 范围
        const float normalized = juce::jmap(db, minDb, maxDb, 0.0f, 1.0f);

        // 三段式渐变
        if (normalized < 0.5f)
        {
            // -90dB to -45dB: 透明白色 → 半透明粉色
            const float t = normalized * 2.0f;  // 0.0 to 1.0
            const juce::Colour transparentWhite = juce::Colours::white.withAlpha(0.0f);
            const juce::Colour softPink = GoodMeterLookAndFeel::accentPink.withAlpha(0.5f);
            return transparentWhite.interpolatedWith(softPink, t);
        }
        else
        {
            // -45dB to 0dB: 半透明粉色 → 炽热发光粉色
            const float t = (normalized - 0.5f) * 2.0f;  // 0.0 to 1.0
            const juce::Colour softPink = GoodMeterLookAndFeel::accentPink.withAlpha(0.5f);
            const juce::Colour hotPink = GoodMeterLookAndFeel::accentPink.brighter(0.8f).withAlpha(1.0f);
            return softPink.interpolatedWith(hotPink, t);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
