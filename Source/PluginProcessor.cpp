/*
  ==============================================================================
    PluginProcessor.cpp
    GOODMETER - Professional Audio Metering Plugin

    Company: Solaris
    Version: 1.0.0

    DSP Implementation: Real-time safe metering algorithms
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
#include "StandaloneNonoEditor.h"
#endif

//==============================================================================
GOODMETERAudioProcessor::GOODMETERAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize LUFS buffer to zero
    lufsBufferL.fill(0.0f);
    lufsBufferR.fill(0.0f);

    // Initialize Short-Term LUFS buffer
    stLufsBufferL.fill(0.0f);
    stLufsBufferR.fill(0.0f);

    // Initialize FFT ring buffer to zero
    fftRingL.fill(0.0f);
    fftRingR.fill(0.0f);
    fftWorkBuffer.fill(0.0f);

    // Reserve LRA history
    lraHistory.reserve(lraMaxSamples);
    integratedBlockLufs.reserve(4096);
}

GOODMETERAudioProcessor::~GOODMETERAudioProcessor()
{
}

//==============================================================================
const juce::String GOODMETERAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GOODMETERAudioProcessor::acceptsMidi() const
{
    return false;
}

bool GOODMETERAudioProcessor::producesMidi() const
{
    return false;
}

bool GOODMETERAudioProcessor::isMidiEffect() const
{
    return false;
}

double GOODMETERAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GOODMETERAudioProcessor::getNumPrograms()
{
    return 1;
}

int GOODMETERAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GOODMETERAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String GOODMETERAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void GOODMETERAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void GOODMETERAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    // Prepare K-Weighting filters for LUFS
    kWeightingL.prepare(sampleRate);
    kWeightingR.prepare(sampleRate);

    // Prepare 3-Band frequency filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    // LOW band: Butterworth Low-pass @ 250Hz (4th order)
    *lowPassL_250Hz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 250.0f, 0.707f);
    *lowPassR_250Hz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 250.0f, 0.707f);

    // MID band: HP @ 250Hz cascaded with LP @ 2kHz (flat summing crossover)
    *midHpL_250Hz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 250.0f, 0.707f);
    *midHpR_250Hz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 250.0f, 0.707f);
    *midLpL_2kHz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 2000.0f, 0.707f);
    *midLpR_2kHz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 2000.0f, 0.707f);

    // HIGH band: Butterworth High-pass @ 2kHz (4th order)
    *highPassL_2kHz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 2000.0f, 0.707f);
    *highPassR_2kHz.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 2000.0f, 0.707f);

    lowPassL_250Hz.prepare(spec);
    lowPassR_250Hz.prepare(spec);
    midHpL_250Hz.prepare(spec);
    midHpR_250Hz.prepare(spec);
    midLpL_2kHz.prepare(spec);
    midLpR_2kHz.prepare(spec);
    highPassL_2kHz.prepare(spec);
    highPassR_2kHz.prepare(spec);

    // Reset all DSP state
    kWeightingL.reset();
    kWeightingR.reset();

    lowPassL_250Hz.reset();
    lowPassR_250Hz.reset();
    midHpL_250Hz.reset();
    midHpR_250Hz.reset();
    midLpL_2kHz.reset();
    midLpR_2kHz.reset();
    highPassL_2kHz.reset();
    highPassR_2kHz.reset();

    lufsBufferL.fill(0.0f);
    lufsBufferR.fill(0.0f);
    lufsBufferIndex = 0;

    stLufsBufferL.fill(0.0f);
    stLufsBufferR.fill(0.0f);
    stLufsBufferIndex = 0;

    // Reset integrated LUFS state
    {
        std::lock_guard<std::mutex> lock(integratedMutex);
        integratedBlockLufs.clear();
    }
    integratedBlockSampleCount = 0;

    // Reset LRA history
    {
        std::lock_guard<std::mutex> lock(lraMutex);
        lraHistory.clear();
    }

    fftRingL.fill(0.0f);
    fftRingR.fill(0.0f);
    fftRingIndex = 0;
    fftSamplesSinceLastPush = 0;
}

void GOODMETERAudioProcessor::releaseResources()
{
    // Reset when playback stops
}

bool GOODMETERAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void GOODMETERAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't have input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    //==========================================================================
    // 🎵 TEST SIGNAL GENERATOR (Broadband Breathing Noise)
    // 宽带噪音 + 低频 LFO 呼吸包络（营造瀑布图的粉色云雾质感）
    //==========================================================================
    #define ENABLE_TEST_SIGNAL 0  // ✅ DISABLED - Pure audio passthrough for production
    #if ENABLE_TEST_SIGNAL
    {
        static float lfoPhase = 0.0f;
        static juce::Random random;

        const float sampleRate = static_cast<float>(currentSampleRate);

        // LFO 控制整体呼吸包络（1.5 Hz 呼吸节奏）
        const float lfoStep = juce::MathConstants<float>::twoPi * 1.5f / sampleRate;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            // 更新 LFO 相位
            lfoPhase += lfoStep;
            if (lfoPhase >= juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;

            // 呼吸包络（0.0 ~ 1.0）
            const float envelope = (std::sin(lfoPhase) + 1.0f) * 0.5f;

            // 🌫️ 宽带白噪（-1.0 ~ +1.0）
            const float noiseL = (random.nextFloat() * 2.0f - 1.0f) * 0.3f * envelope;

            // 🔄 右声道添加去相关噪音（营造立体声宽度）
            const float correlation = std::cos(lfoPhase);
            const float decorrelation = std::sin(lfoPhase);
            const float noiseR_decorrelated = (random.nextFloat() * 2.0f - 1.0f) * 0.3f;
            const float noiseR = noiseL * correlation + noiseR_decorrelated * decorrelation * envelope;

            // 强制覆盖输入缓冲区
            buffer.setSample(0, i, noiseL);
            if (buffer.getNumChannels() > 1)
            {
                buffer.setSample(1, i, noiseR);
            }
        }
    }
    #endif

    // Handle mono input (duplicate to both channels)
    const int numChannels = juce::jmin(2, totalNumInputChannels);
    const int numSamples = buffer.getNumSamples();

    const float* channelDataL = buffer.getReadPointer(0);
    const float* channelDataR = numChannels > 1 ? buffer.getReadPointer(1) : channelDataL;

    //==========================================================================
    // Local accumulators (stack-allocated, real-time safe)
    //==========================================================================
    float localPeakL = 0.0f;
    float localPeakR = 0.0f;
    float localSumSquareL = 0.0f;
    float localSumSquareR = 0.0f;
    float localSumXY = 0.0f;
    float localSumX2 = 0.0f;
    float localSumY2 = 0.0f;

    //==========================================================================
    // Sample-by-sample processing
    //==========================================================================
    for (int i = 0; i < numSamples; ++i)
    {
        const float sampleL = channelDataL[i];
        const float sampleR = channelDataR[i];

        //======================================================================
        // 1. Peak Detection
        //======================================================================
        const float absL = std::abs(sampleL);
        const float absR = std::abs(sampleR);
        if (absL > localPeakL) localPeakL = absL;
        if (absR > localPeakR) localPeakR = absR;

        //======================================================================
        // 2. RMS Accumulation
        //======================================================================
        localSumSquareL += sampleL * sampleL;
        localSumSquareR += sampleR * sampleR;

        //======================================================================
        // 3. Phase Correlation Accumulation
        //======================================================================
        localSumXY += sampleL * sampleR;
        localSumX2 += sampleL * sampleL;
        localSumY2 += sampleR * sampleR;

        //======================================================================
        // 4. K-Weighted LUFS Processing
        //======================================================================
        const float kWeightedL = kWeightingL.processSample(sampleL);
        const float kWeightedR = kWeightingR.processSample(sampleR);

        // Store in circular buffer
        lufsBufferL[lufsBufferIndex] = kWeightedL;
        lufsBufferR[lufsBufferIndex] = kWeightedR;
        lufsBufferIndex = (lufsBufferIndex + 1) % lufsBufferSize;

        //======================================================================
        // 5. FFT Ring Buffer with 50% Overlap (doubles FFT frame rate)
        //======================================================================
        fftRingL[fftRingIndex] = sampleL;
        fftRingR[fftRingIndex] = sampleR;
        fftRingIndex = (fftRingIndex + 1) % fftSize;
        fftSamplesSinceLastPush++;

        if (fftSamplesSinceLastPush >= fftHopSize)
        {
            // Copy ring buffer into working buffer (unwrap circular to linear)
            for (int j = 0; j < fftSize; ++j)
            {
                fftWorkBuffer[j] = fftRingL[(fftRingIndex + j) % fftSize];
            }
            // Zero the second half (working memory for FFT)
            std::fill(fftWorkBuffer.begin() + fftSize, fftWorkBuffer.end(), 0.0f);

            // Apply window + FFT for L channel
            window.multiplyWithWindowingTable(fftWorkBuffer.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftWorkBuffer.data());
            fftFifoL.push(fftWorkBuffer.data(), fftSize / 2);
            fftFifoSpectrogramL.push(fftWorkBuffer.data(), fftSize / 2);

            // Same for R channel
            for (int j = 0; j < fftSize; ++j)
            {
                fftWorkBuffer[j] = fftRingR[(fftRingIndex + j) % fftSize];
            }
            std::fill(fftWorkBuffer.begin() + fftSize, fftWorkBuffer.end(), 0.0f);
            window.multiplyWithWindowingTable(fftWorkBuffer.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftWorkBuffer.data());
            fftFifoR.push(fftWorkBuffer.data(), fftSize / 2);

            fftSamplesSinceLastPush = 0;
        }
    }

    //==========================================================================
    // Calculate and update atomic metrics
    //==========================================================================

    // Peak (convert to dB)
    const float peakL_dB = localPeakL > 1e-8f ? 20.0f * std::log10(localPeakL) : -90.0f;
    const float peakR_dB = localPeakR > 1e-8f ? 20.0f * std::log10(localPeakR) : -90.0f;
    peakLevelL.store(peakL_dB, std::memory_order_relaxed);
    peakLevelR.store(peakR_dB, std::memory_order_relaxed);

    // RMS (convert to dB)
    const float rmsL = std::sqrt(localSumSquareL / numSamples);
    const float rmsR = std::sqrt(localSumSquareR / numSamples);
    const float rmsL_dB = rmsL > 1e-8f ? 20.0f * std::log10(rmsL) : -90.0f;
    const float rmsR_dB = rmsR > 1e-8f ? 20.0f * std::log10(rmsR) : -90.0f;
    rmsLevelL.store(rmsL_dB, std::memory_order_relaxed);
    rmsLevelR.store(rmsR_dB, std::memory_order_relaxed);

    // Phase Correlation (-1.0 to +1.0)
    const float denominator = std::sqrt(localSumX2 * localSumY2);
    const float correlation = (denominator > 1e-8f) ? (localSumXY / denominator) : 0.0f;
    phaseCorrelation.store(correlation, std::memory_order_relaxed);

    //==========================================================================
    // LUFS Calculation (Momentary - 400ms window)
    //==========================================================================
    const int windowSamples = static_cast<int>(currentSampleRate * 0.4);
    const int startIdx = juce::jmax(0, lufsBufferIndex - windowSamples);
    const int endIdx = lufsBufferIndex;

    float lufsSumL = 0.0f;
    float lufsSumR = 0.0f;
    int lufsCount = 0;

    // Handle circular buffer wrap-around
    if (endIdx >= startIdx)
    {
        for (int i = startIdx; i < endIdx; ++i)
        {
            lufsSumL += lufsBufferL[i] * lufsBufferL[i];
            lufsSumR += lufsBufferR[i] * lufsBufferR[i];
            lufsCount++;
        }
    }
    else
    {
        // Wrap around
        for (int i = startIdx; i < lufsBufferSize; ++i)
        {
            lufsSumL += lufsBufferL[i] * lufsBufferL[i];
            lufsSumR += lufsBufferR[i] * lufsBufferR[i];
            lufsCount++;
        }
        for (int i = 0; i < endIdx; ++i)
        {
            lufsSumL += lufsBufferL[i] * lufsBufferL[i];
            lufsSumR += lufsBufferR[i] * lufsBufferR[i];
            lufsCount++;
        }
    }

    if (lufsCount > 0)
    {
        const float meanSquareL = lufsSumL / lufsCount;
        const float meanSquareR = lufsSumR / lufsCount;
        const float sumMeanSquare = meanSquareL + meanSquareR;

        float lufs_dB = -70.0f;
        if (sumMeanSquare > 1e-10f)
        {
            lufs_dB = -0.691f + 10.0f * std::log10(sumMeanSquare);
        }
        lufsLevel.store(lufs_dB, std::memory_order_relaxed);
    }

    //==========================================================================
    // Short-Term LUFS (3s window) — same K-weighted data, longer window
    //==========================================================================
    // Store K-weighted samples in short-term buffer too (reuse from lufs buffer)
    // We already wrote K-weighted samples to lufsBuffer above. Copy them to stLufs buffer.
    {
        // Walk backwards from current lufsBufferIndex by numSamples
        for (int i = 0; i < numSamples; ++i)
        {
            int srcIdx = (lufsBufferIndex - numSamples + i + lufsBufferSize) % lufsBufferSize;
            stLufsBufferL[stLufsBufferIndex] = lufsBufferL[srcIdx];
            stLufsBufferR[stLufsBufferIndex] = lufsBufferR[srcIdx];
            stLufsBufferIndex = (stLufsBufferIndex + 1) % stLufsBufferSize;
        }
    }

    // Calculate Short-Term LUFS (3s window)
    {
        const int stWindowSamples = juce::jmin(static_cast<int>(currentSampleRate * 3.0), stLufsBufferSize);
        float stSumL = 0.0f, stSumR = 0.0f;
        int stCount = 0;

        for (int i = 0; i < stWindowSamples; ++i)
        {
            int idx = (stLufsBufferIndex - stWindowSamples + i + stLufsBufferSize) % stLufsBufferSize;
            stSumL += stLufsBufferL[idx] * stLufsBufferL[idx];
            stSumR += stLufsBufferR[idx] * stLufsBufferR[idx];
            stCount++;
        }

        if (stCount > 0)
        {
            float stMeanSq = stSumL / stCount + stSumR / stCount;
            float st_dB = (stMeanSq > 1e-10f) ? (-0.691f + 10.0f * std::log10(stMeanSq)) : -70.0f;
            lufsShortTerm.store(st_dB, std::memory_order_relaxed);
        }
    }

    //==========================================================================
    // Integrated LUFS — accumulate 400ms blocks for gating (BS.1770-4)
    //==========================================================================
    integratedBlockSampleCount += numSamples;
    if (integratedBlockSampleCount >= static_cast<int>(currentSampleRate * 0.4))
    {
        // We have one 400ms block worth — compute its loudness
        float blockLufs = lufsLevel.load(std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(integratedMutex);
            integratedBlockLufs.push_back(blockLufs);
        }

        integratedBlockSampleCount = 0;

        // Compute Integrated LUFS with absolute + relative gating
        std::vector<float> localBlocks;
        {
            std::lock_guard<std::mutex> lock(integratedMutex);
            localBlocks = integratedBlockLufs;
        }

        if (!localBlocks.empty())
        {
            // Step 1: Absolute gate — remove blocks <= -70 LUFS
            std::vector<float> gated1;
            for (float v : localBlocks)
                if (v > -70.0f) gated1.push_back(v);

            if (!gated1.empty())
            {
                // Step 2: Calculate average of gated blocks (in linear power)
                double linSum = 0.0;
                for (float v : gated1)
                    linSum += std::pow(10.0, v / 10.0);
                double avgLin = linSum / gated1.size();
                float relativeGate = static_cast<float>(10.0 * std::log10(avgLin)) - 10.0f;

                // Step 3: Relative gate — remove blocks below relativeGate
                double finalSum = 0.0;
                int finalCount = 0;
                for (float v : gated1)
                {
                    if (v > relativeGate)
                    {
                        finalSum += std::pow(10.0, v / 10.0);
                        finalCount++;
                    }
                }

                if (finalCount > 0)
                {
                    float intLufs = static_cast<float>(10.0 * std::log10(finalSum / finalCount));
                    lufsIntegrated.store(intLufs, std::memory_order_relaxed);
                }
            }
        }
    }

    //==========================================================================
    // Mid/Side (M/S) Calculation
    //==========================================================================
    float localSumSquareMid = 0.0f;
    float localSumSquareSide = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sampleL = channelDataL[i];
        const float sampleR = channelDataR[i];

        const float mid = (sampleL + sampleR) * 0.5f;
        const float side = (sampleL - sampleR) * 0.5f;

        localSumSquareMid += mid * mid;
        localSumSquareSide += side * side;
    }

    const float rmsMid = std::sqrt(localSumSquareMid / numSamples);
    const float rmsSide = std::sqrt(localSumSquareSide / numSamples);
    const float rmsMid_dB = rmsMid > 1e-8f ? 20.0f * std::log10(rmsMid) : -90.0f;
    const float rmsSide_dB = rmsSide > 1e-8f ? 20.0f * std::log10(rmsSide) : -90.0f;
    rmsLevelMid.store(rmsMid_dB, std::memory_order_relaxed);
    rmsLevelSide.store(rmsSide_dB, std::memory_order_relaxed);

    //==========================================================================
    // 3-Band Frequency Analysis (LOW/MID/HIGH)
    //==========================================================================
    float localSumSquareLow = 0.0f;
    float localSumSquareMid3Band = 0.0f;
    float localSumSquareHigh = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sampleL = channelDataL[i];
        const float sampleR = channelDataR[i];

        // Apply band filters
        const float lowL = lowPassL_250Hz.processSample(sampleL);
        const float lowR = lowPassR_250Hz.processSample(sampleR);
        const float midL = midLpL_2kHz.processSample(midHpL_250Hz.processSample(sampleL));
        const float midR = midLpR_2kHz.processSample(midHpR_250Hz.processSample(sampleR));
        const float highL = highPassL_2kHz.processSample(sampleL);
        const float highR = highPassR_2kHz.processSample(sampleR);

        // Accumulate RMS for each band (stereo sum)
        localSumSquareLow += (lowL * lowL + lowR * lowR);
        localSumSquareMid3Band += (midL * midL + midR * midR);
        localSumSquareHigh += (highL * highL + highR * highR);
    }

    // Calculate RMS and convert to dB
    const float rmsLow = std::sqrt(localSumSquareLow / (numSamples * 2));
    const float rmsMid3Band = std::sqrt(localSumSquareMid3Band / (numSamples * 2));
    const float rmsHigh = std::sqrt(localSumSquareHigh / (numSamples * 2));

    const float rmsLow_dB = rmsLow > 1e-8f ? 20.0f * std::log10(rmsLow) : -90.0f;
    const float rmsMid3Band_dB = rmsMid3Band > 1e-8f ? 20.0f * std::log10(rmsMid3Band) : -90.0f;
    const float rmsHigh_dB = rmsHigh > 1e-8f ? 20.0f * std::log10(rmsHigh) : -90.0f;

    rmsLevelLow.store(rmsLow_dB, std::memory_order_relaxed);
    rmsLevelMid3Band.store(rmsMid3Band_dB, std::memory_order_relaxed);
    rmsLevelHigh.store(rmsHigh_dB, std::memory_order_relaxed);

    //==========================================================================
    // Stereo Image Sample Buffer (for Goniometer/Lissajous)
    // 🎯 批量打包推送 512 个点到 FIFO（解决容量瓶颈 Bug）
    //==========================================================================
    // Downsample: push every 2nd sample (更密集的线条)
    for (int i = 0; i < numSamples; i += 2)
    {
        const float sampleL = channelDataL[i];
        const float sampleR = channelDataR[i];

        // 积攒到本地缓冲区
        tempStereoBufL[tempStereoIndex] = sampleL;
        tempStereoBufR[tempStereoIndex] = sampleR;
        tempStereoIndex++;

        // 🎯 攒满 512 个点后，一次性打包推入 FIFO！
        if (tempStereoIndex >= 512)
        {
            stereoSampleFifoL.push(tempStereoBufL.data(), 512);
            stereoSampleFifoR.push(tempStereoBufR.data(), 512);
            tempStereoIndex = 0;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* GOODMETERAudioProcessor::createEditor()
{
#if JucePlugin_Build_Standalone
    return new StandaloneNonoEditor(*this);
#else
    return new GOODMETERAudioProcessorEditor(*this);
#endif
}

bool GOODMETERAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
void GOODMETERAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
    // TODO: Save plugin state if needed
}

void GOODMETERAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
    // TODO: Restore plugin state if needed
}

//==============================================================================
void GOODMETERAudioProcessor::pushShortTermLUFSForLRA(float stLufs)
{
    std::lock_guard<std::mutex> lock(lraMutex);
    if (static_cast<int>(lraHistory.size()) >= lraMaxSamples)
        lraHistory.erase(lraHistory.begin()); // FIFO: drop oldest
    lraHistory.push_back(stLufs);
}

//==============================================================================
void GOODMETERAudioProcessor::calculateLRARealtime()
{
    // Copy history under lock
    std::vector<float> data;
    {
        std::lock_guard<std::mutex> lock(lraMutex);
        data = lraHistory;
    }

    if (data.size() < 2)
    {
        luRange.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Step 1: Absolute gate — remove <= -70.0 LUFS
    std::vector<float> gated1;
    gated1.reserve(data.size());
    for (float v : data)
        if (v > -70.0f) gated1.push_back(v);

    if (gated1.size() < 2)
    {
        luRange.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Step 2: Calculate average (in linear power domain)
    double linSum = 0.0;
    for (float v : gated1)
        linSum += std::pow(10.0, v / 10.0);
    double avgLin = linSum / gated1.size();
    float relativeGate = static_cast<float>(10.0 * std::log10(avgLin)) - 20.0f;

    // Step 3: Relative gate — remove below relativeGate
    std::vector<float> gated2;
    gated2.reserve(gated1.size());
    for (float v : gated1)
        if (v > relativeGate) gated2.push_back(v);

    if (gated2.size() < 2)
    {
        luRange.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Step 4: Sort and get percentiles
    std::sort(gated2.begin(), gated2.end());

    size_t idx10 = static_cast<size_t>(gated2.size() * 0.10);
    size_t idx95 = static_cast<size_t>(gated2.size() * 0.95);

    // Clamp indices
    if (idx10 >= gated2.size()) idx10 = 0;
    if (idx95 >= gated2.size()) idx95 = gated2.size() - 1;

    float lra = gated2[idx95] - gated2[idx10];
    luRange.store(juce::jmax(0.0f, lra), std::memory_order_relaxed);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GOODMETERAudioProcessor();
}
