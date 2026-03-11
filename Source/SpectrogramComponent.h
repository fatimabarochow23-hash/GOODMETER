/*
  ==============================================================================
    SpectrogramComponent.h
    GOODMETER - Waterfall Spectrogram (Data Ring Buffer Architecture)

    Industrial-grade: raw FFT history + lossless rebuild on resize
    Features: 60Hz single-column scroll, zero-stretch paint, instant resize
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class SpectrogramComponent : public juce::Component,
                               public juce::Thread,
                               public juce::AsyncUpdater
{
public:
    //==========================================================================
    SpectrogramComponent(GOODMETERAudioProcessor& processor)
        : juce::Thread("SpectroDataThread"), audioProcessor(processor)
    {
        smoothedFftData.fill(0.0f);

        fftHistory.resize(historySize);
        for (auto& frame : fftHistory)
            frame.fill(0.0f);

        setSize(100, 300);
        startThread(juce::Thread::Priority::high);
    }

    ~SpectrogramComponent() override
    {
        cancelPendingUpdate();  // 防止析构后回调
        stopThread(1000);
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        if (bounds.isEmpty()) return;
        auto plotBounds = getPlotBounds();

        const juce::ScopedLock sl(imageLock);

        if (spectrogramImage.isNull())
        {
            drawFreqScaleOverlay(g, plotBounds);
            return;
        }

        int width = plotBounds.getWidth();
        int height = plotBounds.getHeight();
        int imgW = spectrogramImage.getWidth();   // 固定 2048
        int imgH = spectrogramImage.getHeight();
        int px = plotBounds.getX();
        int py = plotBounds.getY();

        int drawWidth = juce::jmin(width, imgW);
        int readPos = (drawX - drawWidth + imgW) % imgW;

        if (readPos + drawWidth <= imgW)
        {
            // 连续段：1:1 直取
            g.drawImage(spectrogramImage,
                        px + width - drawWidth, py, drawWidth, height,
                        readPos, 0, drawWidth, imgH);
        }
        else
        {
            // 跨越环形边界，分两段拼接
            int part1W = imgW - readPos;
            int part2W = drawWidth - part1W;

            g.drawImage(spectrogramImage,
                        px + width - drawWidth, py, part1W, height,
                        readPos, 0, part1W, imgH);

            g.drawImage(spectrogramImage,
                        px + width - drawWidth + part1W, py, part2W, height,
                        0, 0, part2W, imgH);
        }

        drawFreqScaleOverlay(g, plotBounds);
    }

    void resized() override
    {
    }

    //==========================================================================
    void handleAsyncUpdate() override
    {
        repaint();  // 主线程苏醒时只优雅重绘一次
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // 多线程保护锁 (OpenGL 渲染线程 vs 主线程)
    juce::CriticalSection imageLock;

    // 离屏缓冲区与环形游标 (固定 historySize 宽)
    juce::Image spectrogramImage;
    int drawX = 0;

    // 原始数据环形缓冲 (2048 帧，无损存储)
    static constexpr int historySize = 2048;
    static constexpr int internalHeight = 512;  // 固定内部渲染高度，永不改变
    std::vector<std::array<float, GOODMETERAudioProcessor::fftSize / 2>> fftHistory;
    int historyHead = 0;

    // FFT data storage
    static constexpr int numBins = GOODMETERAudioProcessor::fftSize / 2;
    std::array<float, numBins> fftData;

    // 时间平滑缓冲
    std::array<float, numBins> smoothedFftData;
    bool isFirstFrame = true;

    // Frequency range (logarithmic)
    static constexpr float minFreq = 30.0f;
    static constexpr float maxFreq = 20000.0f;

    // dB range for color mapping
    static constexpr float minDb = -80.0f;
    static constexpr float maxDb = -10.0f;

    // Offscreen freq scale text cache (STATIC — only rebuild on resize)
    juce::Image freqTextCache;
    int lastFreqCacheW = 0;
    int lastFreqCacheH = 0;

    //==========================================================================
    juce::Rectangle<int> getPlotBounds() const
    {
        return getLocalBounds().withTrimmedLeft(5).withTrimmedRight(5)
                               .withTrimmedTop(5).withTrimmedBottom(0);
    }

    //==========================================================================
    void run() override
    {
        while (!threadShouldExit())
        {
            bool drewAny = false;

            {
                const juce::ScopedLock sl(imageLock);

                if (spectrogramImage.isNull())
                {
                    spectrogramImage = juce::Image(juce::Image::ARGB, historySize, internalHeight, true);
                    spectrogramImage.clear(spectrogramImage.getBounds(), juce::Colours::transparentBlack);
                    drawX = 0;
                }

                while (audioProcessor.fftFifoSpectrogramL.pop(fftData.data(), numBins))
                {
                    if (threadShouldExit()) return;

                    if (isFirstFrame)
                    {
                        smoothedFftData = fftData;
                        isFirstFrame = false;
                    }
                    else
                    {
                        for (int i = 0; i < numBins; ++i)
                            smoothedFftData[i] = smoothedFftData[i] * 0.3f + fftData[i] * 0.7f;
                    }

                    fftHistory[static_cast<size_t>(historyHead)] = smoothedFftData;
                    historyHead = (historyHead + 1) % historySize;

                    drawOneColumn(internalHeight);
                    drawX = (drawX + 1) % historySize;
                    drewAny = true;
                }
            } // ScopedLock released — GPU paint() can proceed

            if (drewAny)
            {
                triggerAsyncUpdate();  // 自带合并：冻结期间只记1次标记，不积压消息
            }

            wait(16); // ~60fps throttle
        }
    }

    //==========================================================================
    void drawOneColumn(int height)
    {
        const float sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        const float frequencyRatio = maxFreq / minFreq;
        const float fftSizeF = static_cast<float>(GOODMETERAudioProcessor::fftSize);

        juce::Image::BitmapData bitmapData(spectrogramImage, drawX, 0, 1, height,
                                            juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            const float normalizedY = 1.0f - (static_cast<float>(y) / static_cast<float>(height));
            const float currentFreq = minFreq * std::pow(frequencyRatio, normalizedY);

            const float binFloat = (currentFreq * fftSizeF) / sampleRate;
            const int binIndex = static_cast<int>(binFloat);
            const float fraction = binFloat - static_cast<float>(binIndex);

            float rawMagnitude = 0.0f;
            if (binIndex >= 0 && binIndex < numBins - 1)
            {
                rawMagnitude = smoothedFftData[binIndex] + fraction * (smoothedFftData[binIndex + 1] - smoothedFftData[binIndex]);
            }
            else
            {
                rawMagnitude = smoothedFftData[juce::jlimit(0, numBins - 1, binIndex)];
            }

            const float scaledAmplitude = rawMagnitude / fftSizeF;
            const float db = juce::Decibels::gainToDecibels(scaledAmplitude, -100.0f);
            bitmapData.setPixelColour(0, y, getColourForDb(db));
        }
    }

    //==========================================================================
    float yToFrequency(int y, int height) const
    {
        const float normalized = 1.0f - (static_cast<float>(y) / static_cast<float>(height - 1));
        const float logMin = std::log10(minFreq);
        const float logMax = std::log10(maxFreq);
        return std::pow(10.0f, logMin + normalized * (logMax - logMin));
    }

    int frequencyToBin(float freq) const
    {
        const float sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        const int bin = static_cast<int>((freq * GOODMETERAudioProcessor::fftSize) / sampleRate);
        return juce::jlimit(0, numBins - 1, bin);
    }

    float getMagnitudeAtFrequency(float freq) const
    {
        return smoothedFftData[frequencyToBin(freq)];
    }

    float magnitudeToDb(float magnitude) const
    {
        return 20.0f * std::log10(magnitude + 1e-8f);
    }

    //==========================================================================
    juce::Colour getColourForDb(float db) const
    {
        float normalized = juce::jmap(db, minDb, maxDb, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);

        const juce::Colour bg = juce::Colours::white;
        const juce::Colour mid(230, 51, 95);
        const juce::Colour peak(110, 15, 40);

        if (normalized < 0.5f)
            return bg.interpolatedWith(mid, normalized * 2.0f);
        else
            return mid.interpolatedWith(peak, (normalized - 0.5f) * 2.0f);
    }

    //==========================================================================
    void drawFreqScaleOverlay(juce::Graphics& g, const juce::Rectangle<int>& plotBounds)
    {
        if (plotBounds.getHeight() < 20)
            return;

        int pw = plotBounds.getWidth();
        int ph = plotBounds.getHeight();

        // Only rebuild text cache on resize
        if (freqTextCache.isNull() || lastFreqCacheW != pw || lastFreqCacheH != ph)
        {
            lastFreqCacheW = pw;
            lastFreqCacheH = ph;
            freqTextCache = juce::Image(juce::Image::ARGB, pw, ph, true, juce::SoftwareImageType());
            juce::Graphics tg(freqTextCache);

            const float logMin = std::log10(minFreq);
            const float logMax = std::log10(maxFreq);
            const float logRange = logMax - logMin;
            const float plotH = static_cast<float>(ph);
            const float rightX = static_cast<float>(pw);

            const float tickFreqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

            tg.setColour(juce::Colours::black.withAlpha(0.85f));
            tg.setFont(juce::Font(11.0f, juce::Font::bold));

            // Reserve top/bottom margins so 20k and 50 labels don't clip
            const float labelH = 12.0f;
            const float topMargin = labelH * 0.5f;   // half-label above top tick
            const float botMargin = 2.0f;             // minimal bottom safety
            const float usableH = plotH - topMargin - botMargin;

            for (float freq : tickFreqs)
            {
                float normY = (std::log10(freq) - logMin) / logRange;
                float y = topMargin + usableH * (1.0f - normY);

                tg.drawLine(rightX - 4.0f, y, rightX, y, 1.5f);

                juce::String text = (freq >= 1000.0f)
                    ? juce::String(static_cast<int>(freq / 1000.0f)) + "k"
                    : juce::String(static_cast<int>(freq));

                // Center label vertically on tick, clamped to image bounds
                float textY = juce::jlimit(0.0f, plotH - labelH, y - labelH * 0.5f);
                tg.drawText(text,
                           static_cast<int>(rightX - 42.0f),
                           static_cast<int>(textY),
                           36, static_cast<int>(labelH),
                           juce::Justification::right, false);
            }
        }

        // Blit cached text
        g.drawImageAt(freqTextCache, plotBounds.getX(), plotBounds.getY());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
