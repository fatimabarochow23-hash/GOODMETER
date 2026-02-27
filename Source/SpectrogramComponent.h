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

        // Background
        g.fillAll(juce::Colours::black);

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
            // ✅ Y 轴对数映射：bottom = 20Hz, top = 20kHz
            const float freq = yToFrequency(y, height);

            // 获取该频率的幅度
            const float magnitude = getMagnitudeAtFrequency(freq);
            const float db = magnitudeToDb(magnitude);

            // 映射为颜色
            const juce::Colour colour = getColourForDb(db);

            // 在 drawX 位置画 1 像素
            g.setColour(colour);
            g.fillRect(drawX, y, 1, 1);
        }

        // 🔄 推进环形游标
        drawX = (drawX + 1) % spectrogramImage.getWidth();

        // 触发重绘
        repaint();
    }

    //==========================================================================
    /**
     * Convert Y pixel coordinate to frequency (Hz)
     * Logarithmic mapping: bottom (height-1) = 20Hz, top (0) = 20kHz
     */
    float yToFrequency(int y, int height) const
    {
        // Invert Y: top (0) = maxFreq, bottom (height-1) = minFreq
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
     * Map dB value to color gradient
     * -90dB: Transparent gray (底噪)
     * -45dB: Pink (中等能量)
     * 0dB: Bright yellow (峰值)
     */
    juce::Colour getColourForDb(float db) const
    {
        const float clamped = juce::jlimit(minDb, maxDb, db);
        const float normalized = (clamped - minDb) / (maxDb - minDb);

        // Color gradient stops
        const juce::Colour darkGray = juce::Colour(0x20202020);  // Almost transparent dark gray
        const juce::Colour pink = GoodMeterLookAndFeel::accentPink;
        const juce::Colour brightYellow = juce::Colour(0xFFFFFF00);  // Bright yellow

        // Three-stage gradient
        if (normalized < 0.5f)
        {
            // -90dB to -45dB: dark gray → pink
            const float t = normalized * 2.0f;  // 0.0 to 1.0
            return darkGray.interpolatedWith(pink, t);
        }
        else
        {
            // -45dB to 0dB: pink → bright yellow
            const float t = (normalized - 0.5f) * 2.0f;  // 0.0 to 1.0
            return pink.interpolatedWith(brightYellow, t);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
