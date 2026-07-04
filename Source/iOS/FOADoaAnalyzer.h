/*
  ==============================================================================
    FOADoaAnalyzer.h
    GOODMETER iOS - real-time FOA direction-of-arrival analyzer.

    The same intensity-vector math used (offline, in Python) for the clap
    calibration sessions: for ACN/SN3D first-order ambisonics the acoustic
    intensity direction is proportional to (<W*X>, <W*Y>, <W*Z>). Fed with
    short blocks from either the playback path or the capture tap, it emits
    (azimuth, elevation, energy) events through a lock-free FIFO for the
    DOA MAP meter card. Header-only, allocation-free on the audio thread.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class FOADoaAnalyzer
{
public:
    FOADoaAnalyzer() = default;

    /** Audio/capture thread. Channels in ACN order: w, y, z, x (SN3D). */
    void process(const float* w, const float* y, const float* z, const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            sw2 += w[i] * w[i];
            swx += w[i] * x[i];
            swy += w[i] * y[i];
            swz += w[i] * z[i];
            if (++count >= blockLen)
                flush();
        }
        lastFedMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
    }

    /** UI thread: pops one (azimuthRad, elevationRad, energy) event. */
    bool pop(float* azElEnergy3)            { return fifo.pop(azElEnergy3, 3); }

    /** UI thread: true while an FOA source has fed us recently. */
    bool isActive() const
    {
        return juce::Time::getMillisecondCounter()
                 - lastFedMs.load(std::memory_order_relaxed) < 600;
    }

private:
    void flush()
    {
        const float e = sw2 / (float) blockLen;
        const float len = std::sqrt(swx * swx + swy * swy + swz * swz);
        if (e > 1.0e-9f && len > 1.0e-12f)
        {
            float ev[3] = { std::atan2(swy, swx),                              // azimuth: 0 = front(+X), +pi/2 = left(+Y)
                            std::atan2(swz, std::sqrt(swx * swx + swy * swy)), // elevation
                            e };
            fifo.push(ev, 3);
        }
        sw2 = swx = swy = swz = 0.0f;
        count = 0;
    }

    static constexpr int blockLen = 512;   // ~10.7 ms @ 48k -> ~90 events/s
    float sw2 = 0.0f, swx = 0.0f, swy = 0.0f, swz = 0.0f;
    int count = 0;
    std::atomic<juce::uint32> lastFedMs { 0 };

    // Tiny self-contained SPSC ring of (az, el, energy) events.
    struct EventRing
    {
        static constexpr int cap = 256;
        std::array<float, (size_t) cap * 3> data {};
        std::atomic<int> head { 0 }, tail { 0 };

        bool push(const float* e, int)
        {
            const int t = tail.load(std::memory_order_relaxed);
            const int nt = (t + 1) % cap;
            if (nt == head.load(std::memory_order_acquire))
                return false;
            std::copy(e, e + 3, data.begin() + (size_t) t * 3);
            tail.store(nt, std::memory_order_release);
            return true;
        }

        bool pop(float* e, int)
        {
            const int h = head.load(std::memory_order_relaxed);
            if (h == tail.load(std::memory_order_acquire))
                return false;
            std::copy(data.begin() + (size_t) h * 3, data.begin() + (size_t) h * 3 + 3, e);
            head.store((h + 1) % cap, std::memory_order_release);
            return true;
        }
    };

    EventRing fifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FOADoaAnalyzer)
};
