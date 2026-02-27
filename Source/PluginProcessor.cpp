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

//==============================================================================
GOODMETERAudioProcessor::GOODMETERAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize LUFS buffer to zero
    lufsBufferL.fill(0.0f);
    lufsBufferR.fill(0.0f);

    // Initialize FFT buffer to zero
    fftBufferL.fill(0.0f);
    fftBufferR.fill(0.0f);
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

    // Reset all DSP state
    kWeightingL.reset();
    kWeightingR.reset();

    lufsBufferL.fill(0.0f);
    lufsBufferR.fill(0.0f);
    lufsBufferIndex = 0;

    fftBufferL.fill(0.0f);
    fftBufferR.fill(0.0f);
    fftBufferIndex = 0;
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
    // 🎵 TEST SIGNAL GENERATOR (Pulsing Noise with LFO Modulation)
    // 方便在 Standalone 模式下测试所有表盘的动画和物理阻尼
    //==========================================================================
    #define ENABLE_TEST_SIGNAL 0  // ✅ DISABLED - 已撤掉测试信号
    #if ENABLE_TEST_SIGNAL
    {
        static float lfoPhase = 0.0f;
        static juce::Random random;

        // 产生一个大概 1Hz - 2Hz 的缓慢脉冲包络 (0.0 到 1.0)
        const float lfoStep = juce::MathConstants<float>::twoPi * 1.5f / static_cast<float>(currentSampleRate);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            lfoPhase += lfoStep;
            if (lfoPhase >= juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;

            // 呼吸包络：让声音有节奏地变大变小
            const float envelope = (std::sin(lfoPhase) + 1.0f) * 0.5f;

            // 生成随机噪音，并套用包络,音量控制在大概 -12dB 到 -6dB 左右
            const float noiseL = (random.nextFloat() * 2.0f - 1.0f) * 0.3f * envelope;
            const float noiseR = (random.nextFloat() * 2.0f - 1.0f) * 0.3f * envelope;

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
        // 5. FFT Buffer Accumulation
        //======================================================================
        fftBufferL[fftBufferIndex] = sampleL;
        fftBufferR[fftBufferIndex] = sampleR;
        fftBufferIndex++;

        if (fftBufferIndex >= fftSize)
        {
            // Apply Hann window
            window.multiplyWithWindowingTable(fftBufferL.data(), fftSize);
            window.multiplyWithWindowingTable(fftBufferR.data(), fftSize);

            // Perform FFT (in-place)
            fft.performFrequencyOnlyForwardTransform(fftBufferL.data());
            fft.performFrequencyOnlyForwardTransform(fftBufferR.data());

            // Push to FIFO for GUI thread
            fftFifoL.push(fftBufferL.data(), fftSize / 2);
            fftFifoR.push(fftBufferR.data(), fftSize / 2);

            // Reset FFT buffer
            fftBufferIndex = 0;
            fftBufferL.fill(0.0f);
            fftBufferR.fill(0.0f);
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
}

//==============================================================================
juce::AudioProcessorEditor* GOODMETERAudioProcessor::createEditor()
{
    return new GOODMETERAudioProcessorEditor(*this);
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
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GOODMETERAudioProcessor();
}
