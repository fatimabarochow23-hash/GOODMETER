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

        if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
        {
            juce::Image newImage(juce::Image::ARGB,
                                 bounds.getWidth(),
                                 bounds.getHeight(),
                                 true);
            newImage.clear(newImage.getBounds(), juce::Colours::white);

            // Preserve existing waterfall data by rescaling old image
            if (!spectrogramImage.isNull() &&
                spectrogramImage.getWidth() > 0 &&
                spectrogramImage.getHeight() > 0)
            {
                juce::Graphics gNew(newImage);

                const int oldW = spectrogramImage.getWidth();
                const int newW = newImage.getWidth();
                const int newH = newImage.getHeight();

                // Reconstruct the logical order: old data left, new data right
                // Part 1: pixels from drawX..oldW (older data) -> left side
                int oldPartWidth = oldW - drawX;
                if (oldPartWidth > 0)
                {
                    float destW = static_cast<float>(oldPartWidth * newW) / static_cast<float>(oldW);
                    gNew.drawImage(spectrogramImage,
                                   0, 0, static_cast<int>(destW), newH,
                                   drawX, 0, oldPartWidth, spectrogramImage.getHeight());
                }

                // Part 2: pixels from 0..drawX (newer data) -> right side
                if (drawX > 0)
                {
                    float destStart = static_cast<float>((oldW - drawX) * newW) / static_cast<float>(oldW);
                    float destW = static_cast<float>(drawX * newW) / static_cast<float>(oldW);
                    gNew.drawImage(spectrogramImage,
                                   static_cast<int>(destStart), 0, static_cast<int>(destW), newH,
                                   0, 0, drawX, spectrogramImage.getHeight());
                }

                // After rescale, the logical cursor is at the right edge
                drawX = newW > 0 ? (newW - 1) : 0;
            }
            else
            {
                drawX = 0;
            }

            spectrogramImage = newImage;
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
    static constexpr float minFreq = 30.0f;    // 30 Hz (bottom) - 压缩底部无用空白
    static constexpr float maxFreq = 20000.0f; // 20 kHz (top)

    // dB range for color mapping (压榨动态范围！)
    static constexpr float minDb = -80.0f;  // 底噪
    static constexpr float maxDb = -10.0f;  // 天花板降低，普通音乐也能触发峰值色

    //==========================================================================
    void timerCallback() override
    {
        if (spectrogramImage.isNull())
            return;

        const int height = spectrogramImage.getHeight();
        if (height <= 0)
            return;

        // Drain ALL available FFT frames, draw one column per frame
        bool drewAny = false;
        while (audioProcessor.fftFifoSpectrogramL.pop(fftData.data(), numBins))
        {
            // Time-domain smoothing (cloud texture)
            if (isFirstFrame)
            {
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

            // Render one pixel column using BitmapData (zero-overhead direct memory write)
            drawOneColumn(height);
            drawX = (drawX + 1) % spectrogramImage.getWidth();
            drewAny = true;
        }

        if (drewAny)
            repaint();
    }

    //==========================================================================
    /**
     * Render a single pixel column at drawX using BitmapData for maximum speed
     */
    void drawOneColumn(int height)
    {
        const float sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        const float frequencyRatio = maxFreq / minFreq;

        juce::Image::BitmapData bitmapData(spectrogramImage, drawX, 0, 1, height,
                                            juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            const float normalizedY = 1.0f - (static_cast<float>(y) / static_cast<float>(height));
            const float currentFreq = minFreq * std::pow(frequencyRatio, normalizedY);

            const float binFloat = (currentFreq * static_cast<float>(GOODMETERAudioProcessor::fftSize)) / sampleRate;
            const int binIndex = static_cast<int>(binFloat);
            const float fraction = binFloat - static_cast<float>(binIndex);

            float rawMagnitude = 0.0f;
            if (binIndex >= 0 && binIndex < numBins - 1)
            {
                const float mag1 = smoothedFftData[binIndex];
                const float mag2 = smoothedFftData[binIndex + 1];
                rawMagnitude = mag1 + fraction * (mag2 - mag1);
            }
            else
            {
                rawMagnitude = smoothedFftData[juce::jlimit(0, numBins - 1, binIndex)];
            }

            const float scaledAmplitude = rawMagnitude / static_cast<float>(GOODMETERAudioProcessor::fftSize);
            const float db = juce::Decibels::gainToDecibels(scaledAmplitude, -100.0f);
            const juce::Colour colour = getColourForDb(db);

            bitmapData.setPixelColour(0, y, colour);
        }
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
     * 🌸 粉色云雾调色板（Web 版高动态范围复刻）
     * 彻底废弃 Alpha 通道，使用纯色 RGB 插值！
     *
     * 三级调色板：
     * - 0.0 (静音): 纯白色（与卡片背景融合）
     * - 0.5 (中等): RGB(230, 51, 95) 标志性主粉色
     * - 1.0 (峰值): RGB(110, 15, 40) 极深邃暗绯红色（深色线条）
     *
     * dB 映射：-80dB (底噪) → -10dB (天花板)
     */
    juce::Colour getColourForDb(float db) const
    {
        // 压榨动态范围：-80dB ~ -10dB 映射到 0.0 ~ 1.0
        float normalized = juce::jmap(db, minDb, maxDb, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);  // 严格限制

        // 三种纯色（无任何透明度！）
        const juce::Colour bg = juce::Colours::white;   // 静音：纯白底色（与卡片融合）
        const juce::Colour mid(230, 51, 95);            // 中等能量：标志性主粉色
        const juce::Colour peak(110, 15, 40);           // 峰值：极深邃暗绯红（深色线条）

        // 分段插值
        if (normalized < 0.5f)
        {
            // 0.0 ~ 0.5: 灰白 → 纯粉色
            return bg.interpolatedWith(mid, normalized * 2.0f);
        }
        else
        {
            // 0.5 ~ 1.0: 纯粉色 → 深暗绯红（爆音感）
            return mid.interpolatedWith(peak, (normalized - 0.5f) * 2.0f);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
