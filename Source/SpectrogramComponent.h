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
        // Initialize smoothed FFT buffer to zero
        smoothedFftData.fill(0.0f);

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

    // ✅ 时间平滑缓冲（核心云雾魔法）
    std::array<float, numBins> smoothedFftData;
    bool isFirstFrame = true;

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

        // 🌫️ 时间平滑处理（核心云雾魔法）
        // Web 版 smoothingTimeConstant = 0.85 → 0.85 旧数据 + 0.15 新数据
        if (isFirstFrame)
        {
            // 首帧直接复制，避免从 0 开始的长尾巴
            smoothedFftData = fftData;
            isFirstFrame = false;
        }
        else
        {
            for (int i = 0; i < numBins; ++i)
            {
                smoothedFftData[i] = smoothedFftData[i] * 0.85f + fftData[i] * 0.15f;
            }
        }

        if (spectrogramImage.isNull())
            return;

        const int height = spectrogramImage.getHeight();
        if (height <= 0)
            return;

        // 绘制单列像素（从上到下）
        for (int y = 0; y < height; ++y)
        {
            // ✅ Y 轴反转：top (y=0) = 20kHz, bottom (y=height-1) = 20Hz
            const float freq = yToFrequency(y, height);

            // ✅ 使用平滑后的数据（而非原始 fftData）
            const float magnitude = getMagnitudeAtFrequency(freq);
            const float db = magnitudeToDb(magnitude);

            // 映射为粉色云雾颜色
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
     * Get magnitude at specific frequency (使用平滑后的数据)
     */
    float getMagnitudeAtFrequency(float freq) const
    {
        const int bin = frequencyToBin(freq);
        return smoothedFftData[bin];  // ✅ 使用平滑缓冲
    }

    /**
     * Convert magnitude to dB
     */
    float magnitudeToDb(float magnitude) const
    {
        return 20.0f * std::log10(magnitude + 1e-8f);
    }

    /**
     * 🌸 粉色云雾调色板（Goodhertz 风格）
     * -90dB: 完全透明白色（静音，露出底层白色）
     * -45dB: 半透明柔和粉（云雾主体，alpha=0.35）
     * 0dB: 纯实心粉色（峰值冲击，alpha=1.0，绝不发白！）
     */
    juce::Colour getColourForDb(float db) const
    {
        // 钳制并归一化到 0.0-1.0 范围
        const float clamped = juce::jlimit(minDb, maxDb, db);
        const float normalized = juce::jmap(clamped, minDb, maxDb, 0.0f, 1.0f);

        // 三种核心色
        const juce::Colour bg = juce::Colours::white.withAlpha(0.0f);        // 静音：全透明
        const juce::Colour mid = juce::Colour(230, 51, 95).withAlpha(0.35f); // 中等：半透明粉（云雾主体）
        const juce::Colour peak = juce::Colour(230, 51, 95).withAlpha(1.0f); // 峰值：纯实心粉（不发白）

        // 分段插值
        if (normalized < 0.5f)
        {
            // 0.0 ~ 0.5: bg → mid
            const float t = normalized * 2.0f;
            return bg.interpolatedWith(mid, t);
        }
        else
        {
            // 0.5 ~ 1.0: mid → peak
            const float t = (normalized - 0.5f) * 2.0f;
            return mid.interpolatedWith(peak, t);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
