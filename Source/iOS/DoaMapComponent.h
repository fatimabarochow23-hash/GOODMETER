/*
  ==============================================================================
    DoaMapComponent.h
    GOODMETER iOS - "DOA MAP" meter card: real-time FOA direction-of-arrival
    heatmap (azimuth x elevation, equirectangular) + integrated virtual mic.

    Replaces ZYLIA's paid "Energy map localization" + virtual pointing:
    - Heatmap lights up wherever sound arrives from, during FOA playback AND
      FOA recording (fed by FOADoaAnalyzer intensity-vector events).
    - V-MIC mode: tap the map to aim a first-order virtual microphone there,
      audition it live (engine monitor), cycle polar patterns, export the
      current beam to a mono WAV.  Header-only.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"

class DoaMapComponent : public juce::Component,
                        private juce::Timer
{
public:
    explicit DoaMapComponent(GOODMETERAudioProcessor& proc)
        : audioProcessor(proc)
    {
        grid.fill(0.0f);
        startTimerHz(30);
    }

    ~DoaMapComponent() override { stopTimer(); }

    void setMarathonDarkStyle(bool dark) { marathonDarkStyle = dark; repaint(); }

    // Wired by the root component to the audio engine.
    std::function<void(bool, float, float, float)> onVirtualMicChanged; // enabled, az, el, pattern
    std::function<void(float, float, float)> onExportBeam;              // az, el, pattern
    std::function<void()> onExportBinaural;                             // render what you hear
    std::function<bool(bool)> onAppleSpatialToggle;                     // returns resulting state

    void notifyExportDone()
    {
        exportNote = "SAVED";
        exportNoteFrames = 60;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const bool dark = marathonDarkStyle;
        auto full = getLocalBounds().toFloat();
        g.setColour(dark ? juce::Colour(0xFF0A0D13) : GoodMeterLookAndFeel::bgMain);
        g.fillRect(full);

        auto area = full;
        controlsArea = area.removeFromBottom(30.0f).reduced(4.0f, 2.0f);
        mapArea = area.reduced(6.0f, 4.0f);
        // Equirectangular projection is naturally 2:1 (360 x 180 deg): clamp
        // the map to that aspect and centre it, so cells stay square-ish on
        // the full-page card instead of stretching into tall slivers.
        {
            const float targetH = mapArea.getWidth() * 0.5f;
            if (mapArea.getHeight() > targetH)
                mapArea = mapArea.withSizeKeepingCentre(mapArea.getWidth(), targetH);
            else
                mapArea = mapArea.withSizeKeepingCentre(mapArea.getHeight() * 2.0f,
                                                        mapArea.getHeight());
        }

        // ── Heatmap cells ──
        const float cw = mapArea.getWidth() / (float) azBins;
        const float ch = mapArea.getHeight() / (float) elBins;
        for (int ey = 0; ey < elBins; ++ey)
            for (int ax = 0; ax < azBins; ++ax)
            {
                const float v = grid[(size_t) (ey * azBins + ax)];
                if (v < 0.02f)
                    continue;
                g.setColour(heatColour(juce::jmin(1.0f, v), dark));
                g.fillRect(mapArea.getX() + (float) ax * cw,
                           mapArea.getY() + (float) ey * ch,
                           cw + 0.5f, ch + 0.5f);
            }

        // ── Grid + labels: F/L/B/R along azimuth, horizon line ──
        const auto ink = dark ? juce::Colours::white : GoodMeterLookAndFeel::textMain;
        g.setColour(ink.withAlpha(0.14f));
        for (int i = 0; i <= 4; ++i)
        {
            const float gx = mapArea.getX() + mapArea.getWidth() * (float) i / 4.0f;
            g.drawVerticalLine((int) gx, mapArea.getY(), mapArea.getBottom());
        }
        const float horizonY = mapArea.getCentreY();
        g.setColour(ink.withAlpha(0.22f));
        g.drawHorizontalLine((int) horizonY, mapArea.getX(), mapArea.getRight());

        g.setColour(ink.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        const char* azLabels[5] = { "B", "L", "F", "R", "B" };   // xN=0.5-az/2pi: +pi(back) left edge -> L -> F(centre) -> R -> back
        for (int i = 0; i <= 4; ++i)
        {
            const float gx = mapArea.getX() + mapArea.getWidth() * (float) i / 4.0f;
            g.drawText(azLabels[i], (int) gx - 8, (int) mapArea.getY() - 1, 16, 10,
                       juce::Justification::centred, false);
        }

        // ── Inactive hint ──
        if (! audioProcessor.foaDoa.isActive())
        {
            g.setColour(ink.withAlpha(0.55f));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText("AMBEO (FOA) ONLY", mapArea, juce::Justification::centred, false);
        }

        // ── Virtual mic crosshair ──
        if (vmicOn)
        {
            const auto pt = dirToPoint(vmicAz, vmicEl);
            g.setColour(GoodMeterLookAndFeel::accentYellow);
            g.drawEllipse(pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, 1.6f);
            g.drawLine(pt.x - 11.0f, pt.y, pt.x - 4.0f, pt.y, 1.2f);
            g.drawLine(pt.x + 4.0f, pt.y, pt.x + 11.0f, pt.y, 1.2f);
            g.drawLine(pt.x, pt.y - 11.0f, pt.x, pt.y - 4.0f, 1.2f);
            g.drawLine(pt.x, pt.y + 4.0f, pt.x, pt.y + 11.0f, 1.2f);
        }

        // ── Touch ripple: instant "I felt that" feedback for tap/drag ──
        if (rippleAge < rippleLife)
        {
            const float t = (float) rippleAge / (float) rippleLife;
            const float radius = 6.0f + t * 22.0f;
            g.setColour(GoodMeterLookAndFeel::accentYellow.withAlpha((1.0f - t) * 0.55f));
            g.drawEllipse(ripplePos.x - radius, ripplePos.y - radius,
                          radius * 2.0f, radius * 2.0f, 2.0f);
            g.setColour(GoodMeterLookAndFeel::accentYellow.withAlpha((1.0f - t) * 0.8f));
            g.fillEllipse(ripplePos.x - 3.0f, ripplePos.y - 3.0f, 6.0f, 6.0f);
        }

        // ── Control pills: V-MIC | pattern | EXPORT (beam) | BIN (binaural) ──
        auto row = controlsArea;
        const float bw = (row.getWidth() - 18.0f) / 4.0f;
        vmicRect = row.removeFromLeft(bw);      row.removeFromLeft(6.0f);
        patternRect = row.removeFromLeft(bw);   row.removeFromLeft(6.0f);
        exportRect = row.removeFromLeft(bw);    row.removeFromLeft(6.0f);
        binRect = row;

        auto drawPill = [&](juce::Rectangle<float> r, const juce::String& text,
                            bool filled, bool enabled)
        {
            const auto accent = GoodMeterLookAndFeel::accentYellow;
            if (filled)
            {
                g.setColour(accent.withAlpha(enabled ? 1.0f : 0.35f));
                g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
                g.setColour(juce::Colour(0xFF2A2A35));
            }
            else
            {
                g.setColour(ink.withAlpha(enabled ? 0.5f : 0.2f));
                g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.2f);
                g.setColour(ink.withAlpha(enabled ? 0.85f : 0.35f));
            }
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(text, r, juce::Justification::centred, false);
        };

        static const char* patternNames[4] = { "CARDIOID", "HYPER", "FIG-8", "OMNI" };
        drawPill(vmicRect, vmicOn ? "V-MIC ON" : "V-MIC", vmicOn, true);
        drawPill(patternRect, patternNames[patternIndex], false, vmicOn);
        drawPill(exportRect, "EXPORT", false, vmicOn);
        drawPill(binRect, "BIN WAV", false, true);

        // APPLE chip (top-right of the map): hand playback to Apple's own
        // spatializer for an A/B against our renderer.
        appleRect = { mapArea.getRight() - 58.0f, mapArea.getY() + 3.0f, 54.0f, 16.0f };
        drawPill(appleRect, "APPLE", appleSpatialOn, true);

        // Export status banner (tap feedback + completion)
        if (exportNoteFrames > 0)
        {
            g.setColour(GoodMeterLookAndFeel::accentYellow.withAlpha(0.92f));
            auto banner = mapArea.withHeight(18.0f).reduced(mapArea.getWidth() * 0.25f, 0.0f);
            g.fillRoundedRectangle(banner, 9.0f);
            g.setColour(juce::Colour(0xFF2A2A35));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(exportNote, banner, juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto p = e.position;

        if (vmicRect.contains(p))
        {
            vmicOn = ! vmicOn;
            pushVmicState();
            repaint();
            return;
        }
        if (patternRect.contains(p) && vmicOn)
        {
            patternIndex = (patternIndex + 1) % 4;
            pushVmicState();
            repaint();
            return;
        }
        if (exportRect.contains(p) && vmicOn)
        {
            // Exports render the LOADED file, not the take in progress —
            // during recording that's confusing, so refuse with a hint.
            if (audioProcessor.iosRecordingActive.load(std::memory_order_relaxed))
            {
                exportNote = "REC ACTIVE";
                exportNoteFrames = 45;
            }
            else
            {
                if (onExportBeam)
                    onExportBeam(vmicAz, vmicEl, patternValue());
                exportNote = "EXPORTING...";
                exportNoteFrames = 120;
            }
            spawnRipple(p);
            repaint();
            return;
        }
        if (binRect.contains(p))
        {
            if (audioProcessor.iosRecordingActive.load(std::memory_order_relaxed))
            {
                exportNote = "REC ACTIVE";
                exportNoteFrames = 45;
            }
            else
            {
                if (onExportBinaural)
                    onExportBinaural();
                exportNote = "EXPORTING...";
                exportNoteFrames = 120;
            }
            spawnRipple(p);
            repaint();
            return;
        }
        if (appleRect.contains(p))
        {
            if (onAppleSpatialToggle)
                appleSpatialOn = onAppleSpatialToggle(! appleSpatialOn);
            exportNote = appleSpatialOn ? "APPLE SPATIAL" : "APP RENDERER";
            exportNoteFrames = 45;
            spawnRipple(p);
            repaint();
            return;
        }
        if (mapArea.contains(p))
        {
            // Direct manipulation: tapping the map aims the virtual mic and
            // switches it on if needed (users tapped a "dead" map otherwise).
            vmicOn = true;
            pointToDir(p);
            spawnRipple(p);
            pushVmicState();
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (mapArea.contains(e.position))
        {
            vmicOn = true;
            pointToDir(e.position);
            spawnRipple(e.position);
            pushVmicState();
            repaint();
        }
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        float ev[3];
        bool got = false;
        while (audioProcessor.foaDoa.pop(ev))
        {
            got = true;
            // Adaptive energy normalisation
            const float db = 10.0f * std::log10(ev[2] + 1.0e-12f);
            peakDb = db >= peakDb ? db : juce::jmax(db, peakDb - 0.08f);
            const float v = juce::jlimit(0.0f, 1.0f, (db - (peakDb - 30.0f)) / 30.0f);
            if (v <= 0.03f)
                continue;

            // Splat a small gaussian at (az, el)
            const float axf = azToBin(ev[0]);
            const float eyf = elToBin(ev[1]);
            for (int dy = -2; dy <= 2; ++dy)
                for (int dxi = -2; dxi <= 2; ++dxi)
                {
                    const int ax = ((int) axf + dxi + azBins) % azBins;   // azimuth wraps
                    const int ey = (int) eyf + dy;
                    if (ey < 0 || ey >= elBins)
                        continue;
                    const float d2 = (float) (dxi * dxi + dy * dy);
                    grid[(size_t) (ey * azBins + ax)] =
                        juce::jmax(grid[(size_t) (ey * azBins + ax)],
                                   v * std::exp(-0.5f * d2 / 1.1f));
                }
        }

        // Decay
        for (auto& v : grid)
            v *= 0.93f;

        if (rippleAge < rippleLife)
            ++rippleAge;

        if (exportNoteFrames > 0)
            --exportNoteFrames;

        if (got || anyEnergy() || rippleAge < rippleLife || exportNoteFrames > 0)
            repaint();
    }

    void spawnRipple(juce::Point<float> p)
    {
        ripplePos = p;
        rippleAge = 0;
    }

    bool anyEnergy() const
    {
        for (auto v : grid)
            if (v > 0.02f)
                return true;
        return false;
    }

    // Mapping: map x = azimuth, front (az=0) at centre, left (+az) to the LEFT
    // of centre (mirror-true when holding the phone), elevation up = top.
    float azToBin(float az) const
    {
        const float xN = 0.5f - az / juce::MathConstants<float>::twoPi;  // az=+pi -> 0, 0 -> 0.5, -pi -> 1
        return juce::jlimit(0.0f, (float) azBins - 1.0f, xN * (float) azBins);
    }
    float elToBin(float el) const
    {
        const float yN = 0.5f - el / juce::MathConstants<float>::pi;     // +pi/2 -> top
        return juce::jlimit(0.0f, (float) elBins - 1.0f, yN * (float) elBins);
    }

    juce::Point<float> dirToPoint(float az, float el) const
    {
        return { mapArea.getX() + (0.5f - az / juce::MathConstants<float>::twoPi) * mapArea.getWidth(),
                 mapArea.getY() + (0.5f - el / juce::MathConstants<float>::pi) * mapArea.getHeight() };
    }

    void pointToDir(juce::Point<float> p)
    {
        const float xN = (p.x - mapArea.getX()) / juce::jmax(1.0f, mapArea.getWidth());
        const float yN = (p.y - mapArea.getY()) / juce::jmax(1.0f, mapArea.getHeight());
        vmicAz = (0.5f - xN) * juce::MathConstants<float>::twoPi;
        vmicEl = (0.5f - yN) * juce::MathConstants<float>::pi;
    }

    float patternValue() const
    {
        static constexpr float values[4] = { 0.5f, 0.34f, 0.0f, 1.0f }; // cardioid/hyper/fig8/omni
        return values[patternIndex];
    }

    void pushVmicState()
    {
        if (onVirtualMicChanged)
            onVirtualMicChanged(vmicOn, vmicAz, vmicEl, patternValue());
    }

    static juce::Colour heatColour(float v, bool dark)
    {
        if (dark)
        {
            // AUDIO LAB blues -> white hot
            const auto lo = juce::Colour(0xFF16244F), mid = juce::Colour(0xFF2A6BD6),
                       hi = juce::Colour(0xFF56E1F2), top = juce::Colour(0xFFEAF6FF);
            if (v < 0.5f)  return lo.interpolatedWith(mid, v * 2.0f).withAlpha(0.45f + v * 0.8f);
            if (v < 0.85f) return mid.interpolatedWith(hi, (v - 0.5f) / 0.35f);
            return hi.interpolatedWith(top, (v - 0.85f) / 0.15f);
        }
        const auto lo = juce::Colour(0xFFAAB4C4), hi = juce::Colour(0xFF33415C);
        return lo.interpolatedWith(hi, v).withAlpha(0.35f + 0.6f * v);
    }

    static constexpr int azBins = 48;
    static constexpr int elBins = 24;

    GOODMETERAudioProcessor& audioProcessor;
    bool marathonDarkStyle = false;
    std::array<float, (size_t) (azBins * elBins)> grid;
    float peakDb = -60.0f;

    bool vmicOn = false;
    float vmicAz = 0.0f, vmicEl = 0.0f;
    int patternIndex = 0;

    static constexpr int rippleLife = 16;   // frames @30Hz ~0.5s
    juce::Point<float> ripplePos;
    int rippleAge = rippleLife;
    juce::String exportNote;
    int exportNoteFrames = 0;

    juce::Rectangle<float> mapArea, controlsArea, vmicRect, patternRect, exportRect, binRect, appleRect;
    bool appleSpatialOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoaMapComponent)
};
