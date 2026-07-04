/*
  ==============================================================================
    TrackLanesComponent.h
    GOODMETER iOS - "TRACKS" meter card: Pro Tools style channel lanes.
    Shows exactly as many lanes as the current source has channels
    (FOA take/playback = 4: W/Y/Z/X, stereo = 2: L/R, mono = 1), with the
    home-page style block-envelope waveform scrolling right-to-left.
    Data: processor.trackScope peak-envelope rings. Header-only.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"

class TrackLanesComponent : public juce::Component,
                            private juce::Timer
{
public:
    explicit TrackLanesComponent(GOODMETERAudioProcessor& proc)
        : audioProcessor(proc)
    {
        startTimerHz(30);
    }

    ~TrackLanesComponent() override { stopTimer(); }

    void setMarathonDarkStyle(bool dark) { marathonDarkStyle = dark; repaint(); }

    void paint(juce::Graphics& g) override
    {
        const bool dark = marathonDarkStyle;
        auto full = getLocalBounds().toFloat();
        g.setColour(dark ? juce::Colour(0xFF0A0D13) : GoodMeterLookAndFeel::bgMain);
        g.fillRect(full);

        const auto& scope = audioProcessor.trackScope;
        const int nCh = juce::jlimit(0, 4, scope.numChannels.load(std::memory_order_relaxed));
        const auto ink = dark ? juce::Colours::white : GoodMeterLookAndFeel::textMain;

        if (nCh <= 0 || fadeOut <= 0.01f)
        {
            g.setColour(ink.withAlpha(0.5f));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText("NO SOURCE", full, juce::Justification::centred, false);
            return;
        }

        static const char* foaNames[4] = { "W", "Y", "Z", "X" };
        static const char* stNames[2]  = { "L", "R" };

        auto area = full.reduced(6.0f, 4.0f);
        const float laneH = area.getHeight() / (float) nCh;
        const int wr = scope.writeIndex.load(std::memory_order_relaxed);
        constexpr int cap = GOODMETERAudioProcessor::TrackScope::cap;

        for (int c = 0; c < nCh; ++c)
        {
            auto lane = juce::Rectangle<float>(area.getX(), area.getY() + (float) c * laneH,
                                               area.getWidth(), laneH).reduced(0.0f, 2.0f);

            // Lane plate + separator (Pro Tools vibe)
            g.setColour(dark ? juce::Colours::white.withAlpha(0.05f)
                             : GoodMeterLookAndFeel::textMain.withAlpha(0.05f));
            g.fillRoundedRectangle(lane, 3.0f);
            g.setColour(ink.withAlpha(0.16f));
            g.drawRoundedRectangle(lane, 3.0f, 0.8f);

            // Centre line
            const float midY = lane.getCentreY();
            g.setColour(ink.withAlpha(0.18f));
            g.drawHorizontalLine((int) midY, lane.getX() + 26.0f, lane.getRight() - 2.0f);

            // Block-envelope waveform (home-page style bars), newest at right
            auto wf = lane.withTrimmedLeft(26.0f).reduced(2.0f, 1.5f);
            const int nBars = juce::jmax(16, (int) (wf.getWidth() / 3.0f));
            const float barW = wf.getWidth() / (float) nBars;
            const auto laneColour = laneAccent(c, nCh, dark);

            for (int b = 0; b < nBars; ++b)
            {
                const int idx = wr - 1 - (nBars - 1 - b);   // oldest left -> newest right
                if (idx < 0)
                    continue;
                float v = scope.lanes[(size_t) c][(size_t) (idx % cap)];
                v = juce::jlimit(0.0f, 1.0f, std::pow(v, 0.6f)) * fadeOut;
                if (v < 0.01f)
                    continue;
                const float h = juce::jmax(1.0f, v * wf.getHeight() * 0.48f);
                g.setColour(laneColour.withAlpha(0.55f + 0.45f * v));
                g.fillRect(wf.getX() + (float) b * barW, midY - h,
                           juce::jmax(1.0f, barW - 1.0f), h * 2.0f);
            }

            // Channel label chip
            const char* name = nCh == 4 ? foaNames[c]
                             : nCh == 2 ? stNames[c] : "M";
            g.setColour(laneColour.withAlpha(0.9f));
            g.fillRoundedRectangle(lane.getX() + 3.0f, lane.getY() + 3.0f, 18.0f,
                                   juce::jmin(14.0f, lane.getHeight() - 6.0f), 2.0f);
            g.setColour(dark ? juce::Colour(0xFF0A0D13) : juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(name, (int) lane.getX() + 3, (int) lane.getY() + 3, 18,
                       (int) juce::jmin(14.0f, lane.getHeight() - 6.0f),
                       juce::Justification::centred, false);
        }
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        const int w = audioProcessor.trackScope.writeIndex.load(std::memory_order_relaxed);
        if (w != lastSeenWrite)
        {
            lastSeenWrite = w;
            stalledFrames = 0;
            fadeOut = 1.0f;
        }
        else if (++stalledFrames > 15)          // source stopped: fade lanes out
            fadeOut = juce::jmax(0.0f, fadeOut - 0.06f);

        repaint();
    }

    juce::Colour laneAccent(int c, int nCh, bool dark) const
    {
        if (nCh == 4)
        {
            static const juce::uint32 foaCols[4] = { 0xFF56E1F2, 0xFF22C55E, 0xFFFFD166, 0xFFE6335F };
            return juce::Colour(foaCols[c]);
        }
        return dark ? juce::Colour(0xFF56E1F2) : juce::Colour(0xFF33415C);
    }

    GOODMETERAudioProcessor& audioProcessor;
    bool marathonDarkStyle = false;
    int lastSeenWrite = -1;
    int stalledFrames = 0;
    float fadeOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackLanesComponent)
};
