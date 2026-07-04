/*
  ==============================================================================
    FOABinauralDecoder.h
    GOODMETER iOS - First-order-Ambisonics -> binaural (headphone) decoder

    Signal path (all real DSP, no placeholder):

      4ch FOA (ACN order W,Y,Z,X / SN3D)
        -> max-rE sampling decode to 6 virtual loudspeakers on an octahedron
           (front/back, HARD LEFT / HARD RIGHT, up/down):
           feed_s = (W + sqrt(3) * <dir_s, (X,Y,Z)>) / 6.
           The hard-left/right speakers are the point: a fully lateral source
           puts its energy on a speaker at 0 deg to one ear and 180 deg to
           the other, so ITD/ILD reach their true physical maxima. (The
           previous cube layout had no speaker beyond +/-45 deg azimuth,
           which capped the rendered image swing at roughly +/-40 deg —
           measured by the user as "movement never exceeded ~90 deg total".)
        -> per speaker, per ear: spherical-head HRTF
             * ITD  : Woodworth ray-path delay around a rigid sphere
             * ILD  : Brown-Duda head-shadow filter (one-pole/one-zero),
                      H(s) = (1 + a*s/(2*w0)) / (1 + s/(2*w0)), w0 = c/r,
                      alpha(theta) = 1.05 + 0.95*cos(theta * 180/150 deg)
               (C. P. Brown & R. O. Duda, "A structural model for binaural
                sound synthesis", IEEE Trans. Speech Audio Proc., 1998)
        -> sum of 16 filtered/delayed paths = L, R.

    Notes / honest limitations:
      * This is a physics-model HRTF (rigid sphere), not a measured-pinna
        HRTF: azimuth cues (left/right ITD+ILD) are strong and real;
        elevation & front/back cues are weak — which is also an inherent
        limit of first-order material from a phone mic array.
      * Everything is generated analytically at prepare() time: no data
        tables, no network, no Apple private API. Fully deterministic.
      * Real-time safe: no allocation, no locks in process().

    Used for: playback of FOA_*.wav recordings (iOSAudioEngine) and
    reusable for offline FOA -> binaural WAV export later.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class FOABinauralDecoder
{
public:
    FOABinauralDecoder() = default;

    /** Must be called (message thread / device-stopped context) before
        process(). Recomputes filters and delays for the sample rate. */
    void prepare(double sampleRate)
    {
        fs = (sampleRate > 8000.0 ? sampleRate : 48000.0);

        const double a = headRadiusMetres;
        const double c = speedOfSoundMs;
        const double w0 = c / a;              // head-shadow corner (rad/s)
        const double fs2 = 2.0 * fs;          // bilinear-transform constant

        // Longest Woodworth delay: theta = pi -> (a/c)*(1 + pi/2)
        maxDelaySamples = (int) std::ceil((a / c) * (1.0 + juce::MathConstants<double>::halfPi) * fs) + 2;

        for (int s = 0; s < numSpeakers; ++s)
        {
            // Component of this speaker's unit direction along the ear axis
            // (ambiX: X fwd, Y left, Z up; left ear = +Y, right ear = -Y).
            const double sy = speakerDir[s][1];

            for (int ear = 0; ear < 2; ++ear)
            {
                // Angle between source direction and this ear's axis.
                // Left ear axis = +Y, right ear axis = -Y.
                const double cosTheta = (ear == 0 ? sy : -sy);
                const double theta = std::acos(juce::jlimit(-1.0, 1.0, cosTheta));

                // --- Woodworth ITD (kept causal: 0 at the nearest angle) ---
                const double delaySec = (theta <= juce::MathConstants<double>::halfPi)
                    ? (a / c) * (1.0 - std::cos(theta))
                    : (a / c) * (1.0 + (theta - juce::MathConstants<double>::halfPi));

                auto& p = paths[(size_t) s][(size_t) ear];
                p.delaySamples = (float) juce::jlimit(0.0, (double) (maxDelaySamples - 2), delaySec * fs);

                // --- Brown-Duda head shadow ---
                // alpha: ~2.0 facing the ear (slight HF boost), ~0.1 in full
                // shadow at 150 deg, partial recovery behind (bright spot).
                const double alpha = 1.05 + 0.95 * std::cos(theta * (180.0 / 150.0));

                const double a0 = w0 + fs2;
                p.b0 = (float) ((w0 + alpha * fs2) / a0);
                p.b1 = (float) ((w0 - alpha * fs2) / a0);
                p.a1 = (float) ((w0 - fs2) / a0);
                p.x1 = 0.0f;
                p.y1 = 0.0f;
            }
        }

        speakerScratch.assign((size_t) maxBlockSize, 0.0f);
        for (int ear = 0; ear < 2; ++ear)
        {
            accum[ear].assign((size_t) (maxBlockSize + maxDelaySamples + 2), 0.0f);
            overflow[ear].assign((size_t) (maxDelaySamples + 2), 0.0f);
        }

        prepared = true;
    }

    bool isPrepared() const noexcept { return prepared; }

    /** Lateral (Y / left-right) compensation gain, applied at decode.
        Measured on-device (calibrated clap-walk test, iPhone 17 Pro, portrait):
        hard-side sources read only ~±20-40 deg of azimuth instead of ±90 —
        Apple's portrait FOA encodes the left-right dipole several dB low
        (the width-axis mic pair has the smallest spacing). Boosting Y widens
        the rendered image back out. Tune by ear: 1.0 = off, 2.0 = +6 dB. */
    float lateralBoost = 1.7f;

    /** foaIn: >= 4 channels, ACN/SN3D (W,Y,Z,X). stereoOut: >= 2 channels.
        Real-time safe. If numSamples exceeds the preallocated block size the
        call safely degrades to a plain W-to-both-ears copy (never happens
        with iOS device buffer sizes). */
    void process(const juce::AudioBuffer<float>& foaIn,
                 juce::AudioBuffer<float>& stereoOut,
                 int numSamples) noexcept
    {
        if (!prepared || foaIn.getNumChannels() < 4 || stereoOut.getNumChannels() < 2)
            return;

        const float* W = foaIn.getReadPointer(0); // ACN 0
        const float* Y = foaIn.getReadPointer(1); // ACN 1
        const float* Z = foaIn.getReadPointer(2); // ACN 2
        const float* X = foaIn.getReadPointer(3); // ACN 3

        float* outL = stereoOut.getWritePointer(0);
        float* outR = stereoOut.getWritePointer(1);

        if (numSamples > maxBlockSize)
        {
            // Degenerate fallback: mono W to both ears (still correct-ish).
            for (int i = 0; i < numSamples; ++i)
                outL[i] = outR[i] = W[i];
            return;
        }

        juce::ScopedNoDenormals noDenormals;

        const int tailLen = maxDelaySamples + 2;
        const int total = numSamples + tailLen;

        // Seed accumulators with last block's overflow, clear the rest.
        for (int ear = 0; ear < 2; ++ear)
        {
            float* acc = accum[ear].data();
            std::copy(overflow[ear].begin(), overflow[ear].end(), acc);
            juce::FloatVectorOperations::clear(acc + tailLen, total - tailLen);
        }

        for (int s = 0; s < numSpeakers; ++s)
        {
            // Octahedron max-rE sampling decode (ACN/SN3D input):
            //   feed = (1/6) * ( W + 3*g1 * <dir, (X,Y,Z)> ),  g1 = 1/sqrt(3)
            //        = (1/6) * W + (sqrt(3)/6) * <dir, (X,Y,Z)>
            const float gw = 1.0f / 6.0f;
            const float gd = 0.28867513f; // sqrt(3)/6
            const float gx = gd * (float) speakerDir[s][0];
            const float gy = gd * (float) speakerDir[s][1] * lateralBoost; // see lateralBoost
            const float gz = gd * (float) speakerDir[s][2];

            float* spk = speakerScratch.data();
            for (int i = 0; i < numSamples; ++i)
                spk[i] = gw * W[i] + gx * X[i] + gy * Y[i] + gz * Z[i];

            for (int ear = 0; ear < 2; ++ear)
            {
                auto& p = paths[(size_t) s][(size_t) ear];
                float* acc = accum[ear].data();

                const int   di = (int) p.delaySamples;
                const float df = p.delaySamples - (float) di;
                const float g0 = 1.0f - df;

                float x1 = p.x1, y1 = p.y1;
                const float b0 = p.b0, b1 = p.b1, a1 = p.a1;

                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = spk[i];
                    const float v = b0 * x + b1 * x1 - a1 * y1;
                    x1 = x;
                    y1 = v;

                    acc[i + di]     += v * g0;   // linear-interp fractional delay
                    acc[i + di + 1] += v * df;
                }

                p.x1 = x1;
                p.y1 = y1;
            }
        }

        // Emit this block, keep the tail for the next one.
        std::copy(accum[0].begin(), accum[0].begin() + numSamples, outL);
        std::copy(accum[1].begin(), accum[1].begin() + numSamples, outR);
        std::copy(accum[0].begin() + numSamples, accum[0].begin() + total, overflow[0].begin());
        std::copy(accum[1].begin() + numSamples, accum[1].begin() + total, overflow[1].begin());
    }

    /** Clears filter/delay state (call when seeking/restarting playback). */
    void reset() noexcept
    {
        for (auto& perSpeaker : paths)
            for (auto& p : perSpeaker)
            {
                p.x1 = 0.0f;
                p.y1 = 0.0f;
            }

        for (int ear = 0; ear < 2; ++ear)
            std::fill(overflow[ear].begin(), overflow[ear].end(), 0.0f);
    }

    static constexpr int maxBlockSize = 8192;

private:
    struct EarPath
    {
        float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f; // head-shadow one-pole/one-zero
        float x1 = 0.0f, y1 = 0.0f;            // filter state
        float delaySamples = 0.0f;             // Woodworth ITD
    };

    static constexpr int numSpeakers = 6;
    // Octahedron: front, back, HARD LEFT, HARD RIGHT, up, down
    // (ambiX axes: x = front, y = left, z = up).
    static constexpr double speakerDir[numSpeakers][3] = {
        { +1.0,  0.0,  0.0 },   // front
        { -1.0,  0.0,  0.0 },   // back
        {  0.0, +1.0,  0.0 },   // hard left  (0 deg to left ear)
        {  0.0, -1.0,  0.0 },   // hard right (0 deg to right ear)
        {  0.0,  0.0, +1.0 },   // above
        {  0.0,  0.0, -1.0 },   // below
    };

    static constexpr double headRadiusMetres = 0.0875;
    static constexpr double speedOfSoundMs   = 343.0;

    double fs = 48000.0;
    int maxDelaySamples = 64;
    bool prepared = false;

    std::array<std::array<EarPath, 2>, numSpeakers> paths;
    std::vector<float> speakerScratch;
    std::array<std::vector<float>, 2> accum;
    std::array<std::vector<float>, 2> overflow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FOABinauralDecoder)
};
