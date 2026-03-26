/*
  ==============================================================================
    DeepFilterProcessor.h
    GOODMETER - DeepFilterNet3 Offline Noise Reduction via ONNX Runtime

    Pipeline (offline, entire file):
      1. Mix to mono, resample to 48 kHz if needed
      2. STFT (960-pt Vorbis window, hop 480, via Accelerate DFT)
      3. Feature extraction: ERB bands (32) + complex spec (96 bins)
      4. ONNX inference: enc → erb_dec (ERB mask) + df_dec (DF coefficients)
      5. Apply ERB mask to full spectrum (481 bins)
      6. Apply deep filtering (complex 5-tap FIR on first 96 bins)
      7. ISTFT → time domain

    Chunked processing with GRU warmup overlap for memory efficiency.
    960 = 15 × 2^6 → Accelerate vDSP_DFT_zop handles it natively.

    Bug fixes v2 (matching Rust reference libDF):
      - ERB features: mean_power with subtractive normalization /40
      - Spec features: divide by sqrt(running_mean_of_magnitude)
      - DF tap order: f + lookahead - tap (not f + tap - lookahead)
      - ISTFT: no winSum normalization (Vorbis + 50% overlap = Princen-Bradley)
      - EMA states initialized to 0 (not first frame value)
      - Zero-padding before STFT to avoid edge artifacts
      - ERB mask applied to local cache, only usable frames written to output
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <onnxruntime_cxx_api.h>
#include <Accelerate/Accelerate.h>
#include <complex>
#include <vector>
#include <array>
#include <atomic>
#include <numeric>
#include <cmath>

class DeepFilterProcessor
{
public:
    DeepFilterProcessor() = default;

    ~DeepFilterProcessor()
    {
        if (fwdDFTSetup) vDSP_DFT_DestroySetup(fwdDFTSetup);
        if (invDFTSetup) vDSP_DFT_DestroySetup(invDFTSetup);
    }

    /** Load 3 ONNX models + build precomputed tables.
        modelDir must contain enc.onnx, erb_dec.onnx, df_dec.onnx */
    bool initialize(const juce::File& modelDir)
    {
        try
        {
            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "DeepFilter");

            Ort::SessionOptions opts;
            opts.SetIntraOpNumThreads(4);
            opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            auto load = [&](const juce::String& name) {
                auto path = modelDir.getChildFile(name).getFullPathName().toStdString();
                return std::make_unique<Ort::Session>(*env, path.c_str(), opts);
            };

            encSession     = load("enc.onnx");
            erbDecSession  = load("erb_dec.onnx");
            dfDecSession   = load("df_dec.onnx");

            buildVorbisWindow();
            buildERBBands();

            fwdDFTSetup = vDSP_DFT_zop_CreateSetup(nullptr, kFFTSize, vDSP_DFT_FORWARD);
            invDFTSetup = vDSP_DFT_zop_CreateSetup(nullptr, kFFTSize, vDSP_DFT_INVERSE);
            if (!fwdDFTSetup || !invDFTSetup) return false;

            initialized = true;
            return true;
        }
        catch (const Ort::Exception& e)
        {
            DBG("DeepFilter init error: " << e.what());
            return false;
        }
    }

    bool isInitialized() const { return initialized; }

    //==========================================================================
    /** Offline denoise: processes EACH channel independently through DFN3.
        Returns denoised buffer with same channel count and sample rate.
        wetDry: 1.0 = fully denoised, 0.0 = original (dry). Default 0.85.
        progress is updated from 0.0 → 1.0 during processing. */
    //==========================================================================
    juce::AudioBuffer<float> process(
        const juce::AudioBuffer<float>& input,
        double inputSampleRate,
        std::atomic<float>& progress,
        float wetDry = 0.85f)
    {
        if (!initialized || input.getNumSamples() == 0) return {};

        int numCh = input.getNumChannels();
        int inputLen = input.getNumSamples();
        juce::AudioBuffer<float> result(numCh, inputLen);
        result.clear();

        progress.store(0.01f);

        // Process each channel independently through the full DFN3 pipeline
        for (int ch = 0; ch < numCh; ++ch)
        {
          try
          {
            float chBase = static_cast<float>(ch) / static_cast<float>(numCh);
            float chSpan = 1.0f / static_cast<float>(numCh);

            // Extract single channel
            juce::AudioBuffer<float> chanBuf(1, inputLen);
            chanBuf.copyFrom(0, 0, input, ch, 0, inputLen);

            // Resample to 48 kHz if needed
            bool needsResample = (std::abs(inputSampleRate - kSR) > 1.0);
            if (needsResample)
                chanBuf = resampleLinear(chanBuf, inputSampleRate, kSR);

            int numSamples = chanBuf.getNumSamples();

            // Zero-pad: add kHopSize silence on each side to avoid edge artifacts
            int pad = kHopSize;
            int paddedLen = numSamples + 2 * pad;
            juce::AudioBuffer<float> padded(1, paddedLen);
            padded.clear();
            padded.copyFrom(0, pad, chanBuf, 0, 0, numSamples);

            const float* paddedData = padded.getReadPointer(0);

            // STFT
            int numFrames = (paddedLen - kFFTSize) / kHopSize + 1;
            if (numFrames < 3) continue;

            std::vector<float> specRe(numFrames * kNbBins);
            std::vector<float> specIm(numFrames * kNbBins);
            computeSTFT(paddedData, paddedLen, numFrames, specRe, specIm);
            progress.store(chBase + chSpan * 0.06f);

            // Feature extraction (EMA states are local → fresh per channel)
            std::vector<float> featERB(numFrames * kNbERB);
            std::vector<float> featSpec(2 * numFrames * kNbDF);
            computeFeatures(specRe, specIm, numFrames, featERB, featSpec);
            progress.store(chBase + chSpan * 0.10f);

            // Enhanced spectra (start as copy of original)
            std::vector<float> enhRe = specRe;
            std::vector<float> enhIm = specIm;

            // Chunked ONNX inference + post-processing
            // Use a sub-progress that maps to this channel's portion
            std::atomic<float> subProgress { 0.0f };
            processChunked(featERB, featSpec, specRe, specIm,
                           enhRe, enhIm, numFrames, subProgress);
            progress.store(chBase + chSpan * 0.90f);

            // ISTFT
            int outputLen = (numFrames - 1) * kHopSize + kFFTSize;
            juce::AudioBuffer<float> paddedOut(1, outputLen);
            paddedOut.clear();
            computeISTFT(enhRe, enhIm, numFrames,
                         paddedOut.getWritePointer(0), outputLen);

            // Remove padding — extract the original-length segment
            juce::AudioBuffer<float> chanOut(1, numSamples);
            chanOut.clear();
            int copyLen = juce::jmin(numSamples, outputLen - pad);
            if (copyLen > 0)
                chanOut.copyFrom(0, 0, paddedOut, 0, pad, copyLen);

            // Resample back if needed
            if (needsResample)
                chanOut = resampleLinear(chanOut, kSR, inputSampleRate);

            // Wet/dry blend and write to result
            int finalLen = juce::jmin(chanOut.getNumSamples(), inputLen);
            const float* wet = chanOut.getReadPointer(0);
            const float* dry = input.getReadPointer(ch);
            float* dst = result.getWritePointer(ch);

            float w = juce::jlimit(0.0f, 1.0f, wetDry);
            float d = 1.0f - w;
            for (int s = 0; s < finalLen; ++s)
                dst[s] = wet[s] * w + dry[s] * d;

            progress.store(chBase + chSpan);
          }
          catch (const std::bad_alloc&)
          {
              DBG("DeepFilter: out of memory on channel " << ch << " — passing through original");
              result.copyFrom(ch, 0, input, ch, 0, inputLen);
              progress.store(static_cast<float>(ch + 1) / static_cast<float>(numCh));
          }
          catch (const Ort::Exception& e)
          {
              DBG("DeepFilter: ONNX error on channel " << ch << ": " << e.what());
              result.copyFrom(ch, 0, input, ch, 0, inputLen);
              progress.store(static_cast<float>(ch + 1) / static_cast<float>(numCh));
          }
        }

        progress.store(1.0f);
        return result;
    }

private:
    //==========================================================================
    // Constants (from DeepFilterNet3 config.ini)
    //==========================================================================
    static constexpr int    kSR          = 48000;
    static constexpr int    kFFTSize     = 960;
    static constexpr int    kHopSize     = 480;
    static constexpr int    kNbBins      = kFFTSize / 2 + 1;  // 481
    static constexpr int    kNbERB       = 32;
    static constexpr int    kNbDF        = 96;
    static constexpr int    kDFOrder     = 5;
    static constexpr int    kDFLookahead = 2;
    static constexpr int    kMinERBFreqs = 2;
    static constexpr float  kNormTau     = 1.0f;
    // alpha = exp(-hop / (tau * sr)) ≈ 0.99005
    static constexpr float  kNormAlpha   = 0.99005f;

    // Chunk processing
    static constexpr int kChunkStride = 2000;  // usable output frames per chunk
    static constexpr int kWarmUp      = 200;   // GRU warmup frames (prepended)

    //==========================================================================
    // State
    //==========================================================================
    bool initialized = false;

    std::unique_ptr<Ort::Env>     env;
    std::unique_ptr<Ort::Session> encSession, erbDecSession, dfDecSession;
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    std::array<float, kFFTSize> vorbisWindow {};
    std::vector<int> erbBandWidths;    // [32] — bins per band
    std::vector<int> erbBandOffsets;   // [33] — cumulative bin offsets

    vDSP_DFT_Setup fwdDFTSetup = nullptr;
    vDSP_DFT_Setup invDFTSetup = nullptr;

    //==========================================================================
    // Precomputed tables
    //==========================================================================
    void buildVorbisWindow()
    {
        for (int n = 0; n < kFFTSize; ++n)
        {
            float x = juce::MathConstants<float>::pi
                      * (n + 0.5f) / static_cast<float>(kFFTSize);
            float s = std::sin(x);
            vorbisWindow[n] = std::sin(juce::MathConstants<float>::halfPi * s * s);
        }
    }

    /** Build ERB band partition: 481 FFT bins → 32 ERB bands (contiguous). */
    void buildERBBands()
    {
        auto fToErb = [](float f) { return 21.4f * std::log10(1.0f + 0.00437f * f); };
        auto erbToF = [](float e) { return (std::pow(10.0f, e / 21.4f) - 1.0f) / 0.00437f; };

        float nyquist  = kSR / 2.0f;
        float minErb   = fToErb(0.0f);
        float maxErb   = fToErb(nyquist);
        float erbStep  = (maxErb - minErb) / static_cast<float>(kNbERB);
        float freqRes  = static_cast<float>(kSR) / static_cast<float>(kFFTSize); // 50 Hz

        erbBandWidths.resize(kNbERB);
        float curFreq = 0.0f;

        for (int b = 0; b < kNbERB; ++b)
        {
            float curErb   = fToErb(curFreq);
            float tgtFreq  = erbToF(curErb + erbStep);
            int nbFreqs    = juce::jmax(kMinERBFreqs,
                static_cast<int>(std::round((tgtFreq - curFreq) / freqRes)));
            erbBandWidths[b] = nbFreqs;
            curFreq += nbFreqs * freqRes;
        }

        // Adjust last band so total = kNbBins
        int total = 0;
        for (int w : erbBandWidths) total += w;
        erbBandWidths.back() += kNbBins - total;

        // Cumulative offsets (size 33)
        erbBandOffsets.resize(kNbERB + 1);
        erbBandOffsets[0] = 0;
        for (int b = 0; b < kNbERB; ++b)
            erbBandOffsets[b + 1] = erbBandOffsets[b] + erbBandWidths[b];
    }

    //==========================================================================
    // Audio helpers
    //==========================================================================
    static juce::AudioBuffer<float> mixToMono(const juce::AudioBuffer<float>& a)
    {
        int nCh = a.getNumChannels(), nS = a.getNumSamples();
        juce::AudioBuffer<float> m(1, nS);
        if (nCh == 1)
            m.copyFrom(0, 0, a, 0, 0, nS);
        else
        {
            m.clear();
            float g = 1.0f / static_cast<float>(nCh);
            for (int c = 0; c < nCh; ++c)
                m.addFrom(0, 0, a, c, 0, nS, g);
        }
        return m;
    }

    static juce::AudioBuffer<float> resampleLinear(
        const juce::AudioBuffer<float>& src, double srcRate, double dstRate)
    {
        double ratio = dstRate / srcRate;
        int newLen = static_cast<int>(src.getNumSamples() * ratio);
        juce::AudioBuffer<float> dst(src.getNumChannels(), newLen);

        for (int ch = 0; ch < src.getNumChannels(); ++ch)
        {
            const float* s = src.getReadPointer(ch);
            float* d = dst.getWritePointer(ch);
            int srcLen = src.getNumSamples();
            for (int i = 0; i < newLen; ++i)
            {
                double pos = i / ratio;
                int i0 = static_cast<int>(pos);
                int i1 = juce::jmin(i0 + 1, srcLen - 1);
                float f = static_cast<float>(pos - i0);
                d[i] = s[i0] * (1.0f - f) + s[i1] * f;
            }
        }
        return dst;
    }

    //==========================================================================
    // STFT (forward) — 960-pt complex DFT via Accelerate
    //==========================================================================
    void computeSTFT(const float* input, int numSamples, int numFrames,
                     std::vector<float>& outRe, std::vector<float>& outIm)
    {
        std::vector<float> dftIr(kFFTSize), dftIi(kFFTSize, 0.0f);
        std::vector<float> dftOr(kFFTSize), dftOi(kFFTSize);

        for (int f = 0; f < numFrames; ++f)
        {
            int pos = f * kHopSize;
            for (int n = 0; n < kFFTSize; ++n)
            {
                int idx = pos + n;
                dftIr[n] = (idx < numSamples) ? input[idx] * vorbisWindow[n] : 0.0f;
                dftIi[n] = 0.0f;
            }

            vDSP_DFT_Execute(fwdDFTSetup,
                dftIr.data(), dftIi.data(), dftOr.data(), dftOi.data());

            int off = f * kNbBins;
            for (int k = 0; k < kNbBins; ++k)
            {
                outRe[off + k] = dftOr[k];
                outIm[off + k] = dftOi[k];
            }
        }
    }

    //==========================================================================
    // ISTFT (inverse) — overlap-add with Vorbis window
    // FIX: No winSum normalization. Vorbis window with 50% overlap satisfies
    //      the Princen-Bradley condition (w²(n) + w²(n+N/2) = 1), so
    //      analysis_window × synthesis_window overlap-add = unity gain.
    //==========================================================================
    void computeISTFT(const std::vector<float>& inRe,
                      const std::vector<float>& inIm,
                      int numFrames, float* output, int numSamples)
    {
        std::vector<float> dftIr(kFFTSize), dftIi(kFFTSize);
        std::vector<float> dftOr(kFFTSize), dftOi(kFFTSize);

        float invN = 1.0f / static_cast<float>(kFFTSize);

        for (int f = 0; f < numFrames; ++f)
        {
            int off = f * kNbBins;

            // Reconstruct full spectrum (Hermitian symmetry)
            for (int k = 0; k < kNbBins; ++k)
            {
                dftIr[k] = inRe[off + k];
                dftIi[k] = inIm[off + k];
            }
            for (int k = kNbBins; k < kFFTSize; ++k)
            {
                dftIr[k] =  inRe[off + (kFFTSize - k)];
                dftIi[k] = -inIm[off + (kFFTSize - k)];
            }

            vDSP_DFT_Execute(invDFTSetup,
                dftIr.data(), dftIi.data(), dftOr.data(), dftOi.data());

            int pos = f * kHopSize;
            for (int n = 0; n < kFFTSize; ++n)
            {
                int idx = pos + n;
                if (idx < numSamples)
                {
                    float w = vorbisWindow[n];
                    output[idx] += dftOr[n] * invN * w;
                    // No winSum accumulation or division — Princen-Bradley
                }
            }
        }
    }

    //==========================================================================
    // Feature extraction (ERB + complex spec with running-mean normalization)
    //
    // FIXED to match Rust reference (libDF):
    //   ERB: band_mean_norm_erb → feature = (mean_power - running_mean) / 40
    //   Spec: band_unit_norm    → X_norm = X / sqrt(running_mean_of_mag)
    //   EMA state starts at 0 (not first-frame value)
    //==========================================================================
    void computeFeatures(const std::vector<float>& specRe,
                         const std::vector<float>& specIm,
                         int numFrames,
                         std::vector<float>& featERB,
                         std::vector<float>& featSpec)
    {
        // Running-mean normalization state — initialized to 0 (matching Rust reference)
        std::vector<float> erbState(kNbERB, 0.0f);
        std::vector<float> specState(kNbDF, 0.0f);

        for (int f = 0; f < numFrames; ++f)
        {
            int sOff = f * kNbBins;

            // ── feat_erb: mean power per band, subtractive normalization / 40 ──
            // Reference: compute_band_erb → band_mean_norm_erb
            //   mean_power = sum(|X|²) / bandwidth
            //   state = state * alpha + mean_power * (1 - alpha)
            //   feature = (mean_power - state) / 40.0
            for (int b = 0; b < kNbERB; ++b)
            {
                float power = 0.0f;
                int bs = erbBandOffsets[b], be = erbBandOffsets[b + 1];
                for (int k = bs; k < be; ++k)
                {
                    float re = specRe[sOff + k], im = specIm[sOff + k];
                    power += re * re + im * im;
                }
                float meanPower = power / static_cast<float>(be - bs);

                // EMA update: state starts at 0, NOT initialized to first frame
                erbState[b] = erbState[b] * kNormAlpha
                              + meanPower * (1.0f - kNormAlpha);

                // Subtractive normalization with constant scaling
                featERB[f * kNbERB + b] = (meanPower - erbState[b]) / 40.0f;
            }

            // ── feat_spec: complex bins 0..95, unit-norm by sqrt(running_mean_of_mag) ──
            // Reference: band_unit_norm
            //   state = state * alpha + |X| * (1 - alpha)
            //   X_norm = X / sqrt(state)
            for (int k = 0; k < kNbDF; ++k)
            {
                float re  = specRe[sOff + k];
                float im  = specIm[sOff + k];
                float mag = std::sqrt(re * re + im * im);

                // EMA update: state starts at 0
                specState[k] = specState[k] * kNormAlpha
                               + mag * (1.0f - kNormAlpha);

                // Divide by sqrt of running mean (the "sqrt quirk" from Issue #514)
                float inv = 1.0f / (std::sqrt(specState[k]) + 1e-10f);

                // Layout: [channel0 = real][channel1 = imag], each [S, 96]
                featSpec[f * kNbDF + k]                      = re * inv;
                featSpec[numFrames * kNbDF + f * kNbDF + k]  = im * inv;
            }
        }
    }

    //==========================================================================
    // Chunked ONNX inference + post-processing
    //
    // FIXED:
    //   - ERB mask applied to local cache for all chunk frames (needed by DF taps)
    //   - Only usable (non-warmup) frames written to output enhRe/enhIm
    //   - DF tap order: f + kDFLookahead - tap (matching Rust reference)
    //==========================================================================
    void processChunked(const std::vector<float>& featERB,
                        const std::vector<float>& featSpec,
                        const std::vector<float>& specRe,
                        const std::vector<float>& specIm,
                        std::vector<float>& enhRe,
                        std::vector<float>& enhIm,
                        int totalFrames,
                        std::atomic<float>& progress)
    {
        int numChunks = (totalFrames + kChunkStride - 1) / kChunkStride;

        for (int ch = 0; ch < numChunks; ++ch)
        {
            int outStart = ch * kChunkStride;
            int outEnd   = juce::jmin(outStart + kChunkStride, totalFrames);
            int inStart  = juce::jmax(0, outStart - kWarmUp);
            int S        = outEnd - inStart;   // frames in this chunk
            int warmOff  = outStart - inStart; // warmup frames to skip

            if (S <= 0) continue;

            // ── Slice features for this chunk ──
            std::vector<float> cERB(S * kNbERB);
            std::vector<float> cSpec(2 * S * kNbDF);

            for (int f = 0; f < S; ++f)
            {
                int gf = inStart + f;
                std::copy_n(&featERB[gf * kNbERB],  kNbERB,
                            &cERB[f * kNbERB]);
                std::copy_n(&featSpec[gf * kNbDF],   kNbDF,
                            &cSpec[f * kNbDF]);
                std::copy_n(&featSpec[totalFrames * kNbDF + gf * kNbDF], kNbDF,
                            &cSpec[S * kNbDF + f * kNbDF]);
            }

            try
            {
                // ── Encoder ──
                auto enc = runEncoder(cERB, cSpec, S);

                // ── ERB decoder → mask [1, 1, S, 32] ──
                auto mask = runERBDecoder(enc.emb, enc.e3, enc.e2, enc.e1, enc.e0,
                                          enc.embShape, enc.e3Shape, enc.e2Shape,
                                          enc.e1Shape, enc.e0Shape);

                // ── DF decoder → coefs [1, S, 96, 10], alpha [1, S, 1] ──
                auto df = runDFDecoder(enc.emb, enc.c0,
                                       enc.embShape, enc.c0Shape);

                // ── Apply ERB mask to LOCAL cache for ALL chunk frames ──
                // We need all frames (including warmup) available for DF taps
                // Local cache: all 481 bins per frame, stored as [S][kNbBins]
                std::vector<float> localRe(S * kNbBins);
                std::vector<float> localIm(S * kNbBins);

                for (int f = 0; f < S; ++f)
                {
                    int gf   = inStart + f;
                    int gOff = gf * kNbBins;
                    int lOff = f * kNbBins;

                    for (int b = 0; b < kNbERB; ++b)
                    {
                        float gain = mask[f * kNbERB + b];
                        int bs = erbBandOffsets[b], be = erbBandOffsets[b + 1];
                        for (int k = bs; k < be; ++k)
                        {
                            localRe[lOff + k] = specRe[gOff + k] * gain;
                            localIm[lOff + k] = specIm[gOff + k] * gain;
                        }
                    }
                }

                // ── Deep filtering (only usable frames written to output) ──
                for (int f = warmOff; f < S; ++f)
                {
                    int gf   = inStart + f;
                    int gOff = gf * kNbBins;
                    int lOff = f * kNbBins;

                    // Write ERB-masked result for bins >= kNbDF (DF doesn't touch these)
                    for (int k = kNbDF; k < kNbBins; ++k)
                    {
                        enhRe[gOff + k] = localRe[lOff + k];
                        enhIm[gOff + k] = localIm[lOff + k];
                    }

                    // Alpha blending factor from df_dec (sigmoid output)
                    float alpha = (f < static_cast<int>(df.alpha.size()))
                                  ? df.alpha[f] : 0.5f;

                    for (int k = 0; k < kNbDF; ++k)
                    {
                        float dfRe = 0.0f, dfIm = 0.0f;

                        for (int tap = 0; tap < kDFOrder; ++tap)
                        {
                            // FIX: Correct tap order matching Rust reference
                            // tap 0 → future frame (f + lookahead)
                            // tap 4 → past frame (f + lookahead - 4 = f - 2)
                            int srcLocalF = f + kDFLookahead - tap;
                            if (srcLocalF < 0 || srcLocalF >= S) continue;

                            float sRe = localRe[srcLocalF * kNbBins + k];
                            float sIm = localIm[srcLocalF * kNbBins + k];

                            // coefs layout: [S, 96, 10] → 10 = 5 taps × (re,im)
                            int ci = f * kNbDF * (kDFOrder * 2)
                                   + k * (kDFOrder * 2) + tap * 2;
                            float cRe = df.coefs[ci];
                            float cIm = df.coefs[ci + 1];

                            // Complex multiply: coef × source
                            dfRe += cRe * sRe - cIm * sIm;
                            dfIm += cRe * sIm + cIm * sRe;
                        }

                        // Blend DF result with ERB-only result
                        float eRe = localRe[lOff + k];
                        float eIm = localIm[lOff + k];
                        enhRe[gOff + k] = alpha * dfRe + (1.0f - alpha) * eRe;
                        enhIm[gOff + k] = alpha * dfIm + (1.0f - alpha) * eIm;
                    }
                }
            }
            catch (const Ort::Exception& e)
            {
                DBG("DeepFilter ONNX chunk error: " << e.what());
            }

            progress.store(0.10f + 0.85f
                * static_cast<float>(ch + 1) / static_cast<float>(numChunks));
        }
    }

    //==========================================================================
    // ONNX inference helpers
    //==========================================================================

    /** Container for encoder outputs + their shapes (for decoder inputs). */
    struct EncOut
    {
        std::vector<float> e0, e1, e2, e3, emb, c0, lsnr;
        std::vector<int64_t> e0Shape, e1Shape, e2Shape, e3Shape;
        std::vector<int64_t> embShape, c0Shape;
    };

    EncOut runEncoder(const std::vector<float>& erbFeat,
                      const std::vector<float>& specFeat,
                      int S)
    {
        int64_t erbDims[]  = {1, 1, static_cast<int64_t>(S), kNbERB};
        int64_t specDims[] = {1, 2, static_cast<int64_t>(S), kNbDF};

        auto erbT  = Ort::Value::CreateTensor<float>(
            memInfo, const_cast<float*>(erbFeat.data()),
            erbFeat.size(), erbDims, 4);
        auto specT = Ort::Value::CreateTensor<float>(
            memInfo, const_cast<float*>(specFeat.data()),
            specFeat.size(), specDims, 4);

        const char* inNames[]  = {"feat_erb", "feat_spec"};
        const char* outNames[] = {"e0","e1","e2","e3","emb","c0","lsnr"};

        std::vector<Ort::Value> inputs;
        inputs.push_back(std::move(erbT));
        inputs.push_back(std::move(specT));

        auto outs = encSession->Run(Ort::RunOptions{nullptr},
            inNames, inputs.data(), 2, outNames, 7);

        auto grab = [](Ort::Value& v) -> std::pair<std::vector<float>, std::vector<int64_t>> {
            auto info = v.GetTensorTypeAndShapeInfo();
            auto shape = info.GetShape();
            size_t n = info.GetElementCount();
            const float* p = v.GetTensorData<float>();
            return { std::vector<float>(p, p + n), shape };
        };

        EncOut r;
        auto [d0, s0] = grab(outs[0]); r.e0 = std::move(d0); r.e0Shape = std::move(s0);
        auto [d1, s1] = grab(outs[1]); r.e1 = std::move(d1); r.e1Shape = std::move(s1);
        auto [d2, s2] = grab(outs[2]); r.e2 = std::move(d2); r.e2Shape = std::move(s2);
        auto [d3, s3] = grab(outs[3]); r.e3 = std::move(d3); r.e3Shape = std::move(s3);
        auto [de, se] = grab(outs[4]); r.emb = std::move(de); r.embShape = std::move(se);
        auto [dc, sc] = grab(outs[5]); r.c0 = std::move(dc); r.c0Shape = std::move(sc);
        { auto info = outs[6].GetTensorTypeAndShapeInfo();
          size_t n = info.GetElementCount();
          const float* p = outs[6].GetTensorData<float>();
          r.lsnr.assign(p, p + n); }

        return r;
    }

    /** ERB decoder → gains mask [1, 1, S, 32], values in [0,1]. */
    std::vector<float> runERBDecoder(
        const std::vector<float>& emb,
        const std::vector<float>& e3,
        const std::vector<float>& e2,
        const std::vector<float>& e1,
        const std::vector<float>& e0,
        const std::vector<int64_t>& embShape,
        const std::vector<int64_t>& e3Shape,
        const std::vector<int64_t>& e2Shape,
        const std::vector<int64_t>& e1Shape,
        const std::vector<int64_t>& e0Shape)
    {
        auto mkT = [&](const std::vector<float>& d, const std::vector<int64_t>& sh) {
            return Ort::Value::CreateTensor<float>(
                memInfo, const_cast<float*>(d.data()), d.size(),
                sh.data(), sh.size());
        };

        std::vector<Ort::Value> ins;
        ins.push_back(mkT(emb, embShape));
        ins.push_back(mkT(e3, e3Shape));
        ins.push_back(mkT(e2, e2Shape));
        ins.push_back(mkT(e1, e1Shape));
        ins.push_back(mkT(e0, e0Shape));

        const char* inN[] = {"emb","e3","e2","e1","e0"};
        const char* outN[] = {"m"};

        auto outs = erbDecSession->Run(Ort::RunOptions{nullptr},
            inN, ins.data(), 5, outN, 1);

        auto info = outs[0].GetTensorTypeAndShapeInfo();
        size_t n = info.GetElementCount();
        const float* p = outs[0].GetTensorData<float>();
        return std::vector<float>(p, p + n);
    }

    /** DF decoder output container. */
    struct DFOut
    {
        std::vector<float> coefs;  // [1, S, 96, 10]
        std::vector<float> alpha;  // [1, S, 1] → flattened to [S]
    };

    DFOut runDFDecoder(
        const std::vector<float>& emb,
        const std::vector<float>& c0,
        const std::vector<int64_t>& embShape,
        const std::vector<int64_t>& c0Shape)
    {
        auto mkT = [&](const std::vector<float>& d, const std::vector<int64_t>& sh) {
            return Ort::Value::CreateTensor<float>(
                memInfo, const_cast<float*>(d.data()), d.size(),
                sh.data(), sh.size());
        };

        std::vector<Ort::Value> ins;
        ins.push_back(mkT(emb, embShape));
        ins.push_back(mkT(c0, c0Shape));

        const char* inN[] = {"emb","c0"};
        const char* outN[] = {"coefs","235"};

        auto outs = dfDecSession->Run(Ort::RunOptions{nullptr},
            inN, ins.data(), 2, outN, 2);

        DFOut r;
        { auto info = outs[0].GetTensorTypeAndShapeInfo();
          size_t n = info.GetElementCount();
          const float* p = outs[0].GetTensorData<float>();
          r.coefs.assign(p, p + n); }
        { auto info = outs[1].GetTensorTypeAndShapeInfo();
          size_t n = info.GetElementCount();
          const float* p = outs[1].GetTensorData<float>();
          r.alpha.assign(p, p + n); }

        return r;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepFilterProcessor)
};
