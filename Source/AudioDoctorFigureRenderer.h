/*
  ==============================================================================
    AudioDoctorFigureRenderer.h
    GOODMETER Audio Doctor - shared static figure renderer for UI export and jobs.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <limits>
#include "AudioDoctorAnalysis.h"

namespace goodmeter::audio_doctor
{

enum class FigureView
{
    spectrum,
    envelope,
    groupDelay,
    spectrogramABC,
    reverbSpace,
    dynamics,
    layeringSpectrum,
    cstSpectrogram,
    harmonicFusion,
    groupDelayCombo,
    reverbSpaceCombo,
    dynamicsApparentDucking
};

struct FigurePluginParam
{
    juce::String name;
    juce::String valueText;
    float normalisedValue = 0.0f;
};

struct FigurePluginInfo
{
    bool valid = false;
    juce::String name;
    juce::String format;
    int latencySamples = 0;
    double tailSeconds = 0.0;
    std::vector<FigurePluginParam> changedParameters;
};

struct FigureData
{
    const Asset* dry = nullptr;
    const Asset* wetA = nullptr;
    const Asset* wetB = nullptr;
    FigurePluginInfo pluginA;
    FigurePluginInfo pluginB;
    FigurePluginInfo pluginC;
    FigureView view = FigureView::spectrum;
    juce::String viewToken = "spectrum";
    juce::String label1 = "Dry";
    juce::String label2 = "Wet A";
    juce::String label3 = "Wet B";
};

class AudioDoctorFigureRenderer
{
public:
    static juce::Image renderImage(const FigureData& data, bool dark, int width = 1800, int height = 900,
                                   bool academicLight = false)
    {
        ScopedAcademicPalette academicScope(academicLight && !dark);
        juce::Image image(juce::Image::RGB, juce::jmax(640, width), juce::jmax(420, height), true);
        juce::Graphics g(image);
        g.fillAll(backgroundColour(dark));

        auto bounds = image.getBounds().reduced(28).toFloat();
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(34.0f, juce::Font::bold));
        g.drawText("GOODMETER Audio Doctor - " + data.viewToken,
                   bounds.removeFromTop(42.0f),
                   juce::Justification::centredLeft);

        auto content = bounds.reduced(0.0f, 4.0f);
        switch (data.view)
        {
            case FigureView::envelope:
                drawTimePlot(g, content, data, "Transient Envelope", "seconds", "dBFS",
                             &Asset::envelope, -80.0f, 0.0f, dark, MetricsKind::basic);
                break;
            case FigureView::groupDelay:
                drawFrequencyPlot(g, content, data, "Group Delay", "Hz", "ms",
                                  &Asset::groupDelay, -20.0f, 80.0f, dark, MetricsKind::groupDelay);
                break;
            case FigureView::spectrogramABC:
                drawSpectrogramFigure(g, content, data, dark);
                break;
            case FigureView::reverbSpace:
                drawReverbSpaceFigure(g, content, data, dark);
                break;
            case FigureView::dynamics:
                drawTimePlot(g, content, data, "Dynamics Response", "seconds", "dBFS",
                             &Asset::dynamicsRms, -80.0f, 0.0f, dark, MetricsKind::dynamics);
                break;
            case FigureView::layeringSpectrum:
                drawLayeringSpectrumFigure(g, content, data, dark);
                break;
            case FigureView::cstSpectrogram:
                drawCstSpectrogramFigure(g, content, data, dark);
                break;
            case FigureView::harmonicFusion:
                drawFrequencyPlot(g, content, data, "Harmonic Fusion", "Hz", "dB",
                                  &Asset::spectrum, -90.0f, 0.0f, dark, MetricsKind::basic);
                break;
            case FigureView::groupDelayCombo:
                drawGroupDelayComboFigure(g, content, data, dark);
                break;
            case FigureView::reverbSpaceCombo:
                drawReverbSpaceComboFigure(g, content, data, dark);
                break;
            case FigureView::dynamicsApparentDucking:
                drawDynamicsApparentDuckingFigure(g, content, data, dark);
                break;
            case FigureView::spectrum:
            default:
                drawFrequencyPlot(g, content, data, "Spectrum Overlay", "Hz", "dB",
                                  &Asset::spectrum, -90.0f, 0.0f, dark, MetricsKind::basic);
                break;
        }

        return image;
    }

    static bool writePng(const juce::File& file, const FigureData& data, bool dark, int width = 1800, int height = 900,
                         bool academicLight = false)
    {
        auto image = renderImage(data, dark, width, height, academicLight);
        juce::PNGImageFormat png;
        file.deleteFile();
        if (auto stream = file.createOutputStream())
            return png.writeImageToStream(image, *stream);
        return false;
    }

    static juce::var writeGroupDelayMetrics(const std::vector<PlotPoint>& points,
                                            const std::vector<PlotPoint>* spectrum = nullptr)
    {
        const auto stats = computeGroupDelayStats(points, 20.0f, 20000.0f, spectrum);
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("valid", stats.valid);
        obj->setProperty("rangeMinHz", 20.0);
        obj->setProperty("rangeMaxHz", 20000.0);
        obj->setProperty("reliabilityGateDb", stats.spectrumGateDb);
        obj->setProperty("reliablePoints", stats.reliablePoints);
        obj->setProperty("reliableMinHz", stats.reliableMinHz);
        obj->setProperty("reliableMaxHz", stats.reliableMaxHz);
        obj->setProperty("meanMs", stats.meanMs);
        obj->setProperty("meanAbsMs", stats.meanAbsMs);
        obj->setProperty("minMs", stats.minMs);
        obj->setProperty("maxMs", stats.maxMs);
        obj->setProperty("peakAbsMs", stats.peakAbsMs);
        obj->setProperty("peakFreqHz", stats.peakFreq);
        obj->setProperty("lowMeanMs", stats.lowMeanMs);
        obj->setProperty("midMeanMs", stats.midMeanMs);
        obj->setProperty("highMeanMs", stats.highMeanMs);
        obj->setProperty("spanMs", stats.maxMs - stats.minMs);
        return juce::var(obj.release());
    }

private:
    using CurveMember = std::vector<PlotPoint> Asset::*;

    struct ScopedAcademicPalette
    {
        explicit ScopedAcademicPalette(bool enabled) : previous(academicLightFlag())
        {
            academicLightFlag() = enabled;
        }

        ~ScopedAcademicPalette()
        {
            academicLightFlag() = previous;
        }

        bool previous = false;
    };

    static bool& academicLightFlag()
    {
        static thread_local bool enabled = false;
        return enabled;
    }

    static bool academicLight()
    {
        return academicLightFlag();
    }

    enum class MetricsKind
    {
        basic,
        groupDelay,
        dynamics
    };

    struct GroupDelayStats
    {
        bool valid = false;
        float meanMs = 0.0f;
        float meanAbsMs = 0.0f;
        float minMs = 0.0f;
        float maxMs = 0.0f;
        float peakAbsMs = 0.0f;
        float peakFreq = 0.0f;
        float lowMeanMs = 0.0f;
        float midMeanMs = 0.0f;
        float highMeanMs = 0.0f;
        int reliablePoints = 0;
        float reliableMinHz = 0.0f;
        float reliableMaxHz = 0.0f;
        float spectrumGateDb = -120.0f;
    };

    static juce::Colour backgroundColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFFF7F8FA);
        return dark ? juce::Colour(0xFF05070B) : juce::Colour(0xFFF4F7FB);
    }

    static juce::Colour plateColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colours::white;
        return dark ? juce::Colour(0xFF0A0D13) : juce::Colours::white;
    }

    static juce::Colour plotColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFFEEF2F7);
        return dark ? juce::Colour(0xFF060A10) : juce::Colour(0xFFFBFCFE);
    }

    static juce::Colour primaryText(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFF111827);
        return dark ? juce::Colour(0xFFF6F8FB) : juce::Colour(0xFF121722);
    }

    static juce::Colour secondaryText(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFF374151);
        return dark ? juce::Colour(0xFFC9D2DE) : juce::Colour(0xFF344256);
    }

    static juce::Colour dryColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFF0077A3);
        return dark ? juce::Colour(0xFF22D3EE) : juce::Colour(0xFF006D9C);
    }

    static juce::Colour wetAColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFFE76F00);
        return dark ? juce::Colour(0xFFFFD166) : juce::Colour(0xFFC26A00);
    }

    static juce::Colour wetBColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFFC2185B);
        return dark ? juce::Colour(0xFFE6335F) : juce::Colour(0xFFC2185B);
    }

    static void drawTimePlot(juce::Graphics& g, juce::Rectangle<float> area, const FigureData& data,
                             const juce::String& title, const juce::String& xLabel, const juce::String& yLabel,
                             CurveMember curve, float minY, float maxY, bool dark, MetricsKind metricsKind)
    {
        const float maxTime = getMaxCurveX(data, curve, false);
        auto metricsArea = area.removeFromBottom(getMetricsHeight(data, metricsKind));
        drawPlotBackground(g, area, title, xLabel, yLabel, dark, false, maxTime, minY, maxY);
        const auto plot = getPlotArea(area);
        drawCurve(g, plot, data.dry, curve, dryColour(dark), false, maxTime, minY, maxY);
        drawCurve(g, plot, data.wetA, curve, wetAColour(dark), false, maxTime, minY, maxY);
        drawCurve(g, plot, data.wetB, curve, wetBColour(dark), false, maxTime, minY, maxY);
        drawFigureMetrics(g, metricsArea, data, dark, metricsKind);
    }

    static void drawFrequencyPlot(juce::Graphics& g, juce::Rectangle<float> area, const FigureData& data,
                                  const juce::String& title, const juce::String& xLabel, const juce::String& yLabel,
                                  CurveMember curve, float minY, float maxY, bool dark, MetricsKind metricsKind)
    {
        auto metricsArea = area.removeFromBottom(getMetricsHeight(data, metricsKind));
        drawPlotBackground(g, area, title, xLabel, yLabel, dark, true, 20000.0f, minY, maxY);
        const auto plot = getPlotArea(area);
        drawCurve(g, plot, data.dry, curve, dryColour(dark), true, 20000.0f, minY, maxY);
        drawCurve(g, plot, data.wetA, curve, wetAColour(dark), true, 20000.0f, minY, maxY);
        drawCurve(g, plot, data.wetB, curve, wetBColour(dark), true, 20000.0f, minY, maxY);
        if (curve == &Asset::spectrum)
            drawHarmonicPeakOverlay(g, plot, data, dark, minY, maxY);
        drawFigureMetrics(g, metricsArea, data, dark, metricsKind);
    }

    static void drawReverbSpaceFigure(juce::Graphics& g, juce::Rectangle<float> area, const FigureData& data, bool dark)
    {
        const float maxTime = getMaxCurveX(data, &Asset::energyDecay, false);
        auto metricsArea = area.removeFromBottom(getMetricsHeight(data, MetricsKind::groupDelay));
        drawPlotBackground(g, area, "Reverb / Space", "seconds", "EDC / decay (dB)", dark,
                           false, maxTime, -80.0f, 0.0f);
        const auto plot = getPlotArea(area);
        drawCurve(g, plot, data.dry, &Asset::energyDecay, dryColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, plot, data.wetA, &Asset::energyDecay, wetAColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, plot, data.wetB, &Asset::energyDecay, wetBColour(dark), false, maxTime, -80.0f, 0.0f);

        drawReverbMetrics(g, metricsArea, data, dark);
    }

    static void drawLayeringSpectrumFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                           const FigureData& data, bool dark)
    {
        auto metricsArea = area.removeFromBottom(juce::jmax(getMetricsHeight(data, MetricsKind::basic), 188.0f));
        drawPlotBackground(g, area, "Layering Spectrum", "Hz", "dB", dark, true, 20000.0f, -90.0f, 0.0f);
        const auto plot = getPlotArea(area);
        drawCurve(g, plot, data.dry, &Asset::spectrum, dryColour(dark), true, 20000.0f, -90.0f, 0.0f);
        drawCurve(g, plot, data.wetA, &Asset::spectrum, wetAColour(dark), true, 20000.0f, -90.0f, 0.0f);
        drawCurve(g, plot, data.wetB, &Asset::spectrum, wetBColour(dark), true, 20000.0f, -90.0f, 0.0f);
        drawHarmonicPeakOverlay(g, plot, data, dark, -90.0f, 0.0f);
        drawBandEnergyTable(g, metricsArea, data, dark);
    }

    static void drawGroupDelayComboFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                          const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(38.0f);
        auto metricsArea = inner.removeFromBottom(getMetricsHeight(data, MetricsKind::groupDelay));
        inner.removeFromBottom(14.0f);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("Group Delay Combo", titleArea, juce::Justification::centredLeft);

        auto envelopeArea = inner.removeFromTop(inner.getHeight() * 0.42f);
        inner.removeFromTop(18.0f);
        auto delayArea = inner;

        const float maxTime = getMaxCurveX(data, &Asset::envelope, false);
        drawPlotBackground(g, envelopeArea, "Transient Envelope", "seconds", "dBFS", dark,
                           false, maxTime, -80.0f, 0.0f);
        auto envPlot = getPlotArea(envelopeArea);
        drawCurve(g, envPlot, data.dry, &Asset::envelope, dryColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, envPlot, data.wetA, &Asset::envelope, wetAColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, envPlot, data.wetB, &Asset::envelope, wetBColour(dark), false, maxTime, -80.0f, 0.0f);

        drawPlotBackground(g, delayArea, "Frequency-dependent Delay", "Hz", "ms", dark,
                           true, 20000.0f, -20.0f, 80.0f);
        auto delayPlot = getPlotArea(delayArea);
        drawCurve(g, delayPlot, data.dry, &Asset::groupDelay, dryColour(dark), true, 20000.0f, -20.0f, 80.0f);
        drawCurve(g, delayPlot, data.wetA, &Asset::groupDelay, wetAColour(dark), true, 20000.0f, -20.0f, 80.0f);
        drawCurve(g, delayPlot, data.wetB, &Asset::groupDelay, wetBColour(dark), true, 20000.0f, -20.0f, 80.0f);

        drawFigureMetrics(g, metricsArea, data, dark, MetricsKind::groupDelay);
    }

    static void drawDynamicsApparentDuckingFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                                  const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(38.0f);
        auto metricsArea = inner.removeFromBottom(getMetricsHeight(data, MetricsKind::dynamics));
        inner.removeFromBottom(14.0f);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("Dynamics Apparent Ducking", titleArea, juce::Justification::centredLeft);

        auto rmsArea = inner.removeFromTop(inner.getHeight() * 0.50f);
        inner.removeFromTop(18.0f);
        auto deltaArea = inner;

        const float maxTime = getMaxCurveX(data, &Asset::dynamicsRms, false);
        drawPlotBackground(g, rmsArea, "RMS Dynamics", "seconds", "dBFS", dark,
                           false, maxTime, -80.0f, 0.0f);
        const auto rmsPlot = getPlotArea(rmsArea);
        drawCurve(g, rmsPlot, data.dry, &Asset::dynamicsRms, dryColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, rmsPlot, data.wetA, &Asset::dynamicsRms, wetAColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, rmsPlot, data.wetB, &Asset::dynamicsRms, wetBColour(dark), false, maxTime, -80.0f, 0.0f);

        drawPlotBackground(g, deltaArea, "Apparent Attenuation Delta", "seconds", "delta dB", dark,
                           false, maxTime, -30.0f, 30.0f);
        const auto deltaPlot = getPlotArea(deltaArea);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.32f) : juce::Colour(0xFF1F2937).withAlpha(0.28f));
        const float zeroY = yForValue(0.0f, deltaPlot, -30.0f, 30.0f);
        g.drawHorizontalLine(static_cast<int>(zeroY), deltaPlot.getX(), deltaPlot.getRight());
        drawApparentDeltaCurve(g, deltaPlot, data.dry, data.wetA, wetAColour(dark),
                               false, maxTime, -30.0f, 30.0f);
        drawApparentDeltaCurve(g, deltaPlot, data.dry, data.wetB, wetBColour(dark),
                               false, maxTime, -30.0f, 30.0f);

        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(18.0f));
        g.drawText("Negative delta = apparent attenuation, not plugin-internal gain reduction.",
                   deltaArea.reduced(86.0f, 12.0f).removeFromTop(24.0f),
                   juce::Justification::centredRight, true);

        drawFigureMetrics(g, metricsArea, data, dark, MetricsKind::dynamics);
    }

    static void drawReverbSpaceComboFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                           const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(38.0f);
        auto metricsArea = inner.removeFromBottom(getMetricsHeight(data, MetricsKind::groupDelay));
        inner.removeFromBottom(14.0f);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("Reverb Space Combo", titleArea, juce::Justification::centredLeft);

        auto edcArea = inner.removeFromTop(inner.getHeight() * 0.44f);
        inner.removeFromTop(18.0f);
        auto spectrogramArea = inner;

        const float maxTime = getMaxCurveX(data, &Asset::energyDecay, false);
        drawPlotBackground(g, edcArea, "Energy Decay Curve", "seconds", "EDC / decay (dB)", dark,
                           false, maxTime, -80.0f, 0.0f);
        const auto edcPlot = getPlotArea(edcArea);
        drawCurve(g, edcPlot, data.dry, &Asset::energyDecay, dryColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, edcPlot, data.wetA, &Asset::energyDecay, wetAColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, edcPlot, data.wetB, &Asset::energyDecay, wetBColour(dark), false, maxTime, -80.0f, 0.0f);

        drawSpectrogramTracksOnly(g, spectrogramArea, data, dark, true);
        drawReverbMetrics(g, metricsArea, data, dark);
    }

    static void drawCstSpectrogramFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                         const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(38.0f);
        auto metricsArea = inner.removeFromBottom(getMetricsHeight(data, MetricsKind::basic));
        inner.removeFromBottom(14.0f);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("CST Spectrogram", titleArea, juce::Justification::centredLeft);

        auto rmsArea = inner.removeFromTop(inner.getHeight() * 0.30f);
        inner.removeFromTop(18.0f);
        auto spectrogramArea = inner;

        const float maxTime = getMaxCurveX(data, &Asset::dynamicsRms, false);
        drawPlotBackground(g, rmsArea, "RMS Envelope with Stage Markers", "seconds", "dBFS", dark,
                           false, maxTime, -80.0f, 0.0f);
        const auto rmsPlot = getPlotArea(rmsArea);
        drawCurve(g, rmsPlot, data.dry, &Asset::dynamicsRms, dryColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, rmsPlot, data.wetA, &Asset::dynamicsRms, wetAColour(dark), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, rmsPlot, data.wetB, &Asset::dynamicsRms, wetBColour(dark), false, maxTime, -80.0f, 0.0f);
        drawStageMarkers(g, rmsPlot, data.dry, dark, maxTime, dryColour(dark), true);

        drawSpectrogramTracksOnly(g, spectrogramArea, data, dark, true);
        drawFigureMetrics(g, metricsArea, data, dark, MetricsKind::basic);
    }

    static void drawSpectrogramFigure(juce::Graphics& g, juce::Rectangle<float> area, const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(40.0f);
        auto metricsArea = inner.removeFromBottom(getMetricsHeight(data, MetricsKind::basic));
        inner.removeFromBottom(10.0f);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(34.0f, juce::Font::bold));
        g.drawText("Spectrogram A/B/C", titleArea, juce::Justification::centredLeft);

        drawSpectrogramTracksOnly(g, inner, data, dark, false);

        drawFigureMetrics(g, metricsArea, data, dark, MetricsKind::basic);
    }

    struct SpectrogramTrack
    {
        const Asset* asset = nullptr;
        const juce::Image* image = nullptr;
        double sampleRate = 48000.0;
        float durationSeconds = 0.0f;
        juce::String label;
        juce::Colour colour;
    };

    static std::vector<SpectrogramTrack> makeSpectrogramTracks(const FigureData& data, bool dark)
    {
        std::vector<SpectrogramTrack> tracks;
        if (data.dry != nullptr && !data.dry->spectrogramBlue.isNull())
            tracks.push_back({ data.dry, &data.dry->spectrogramBlue, data.dry->sampleRate,
                               static_cast<float>(data.dry->metrics.durationSeconds), data.label1, dryColour(dark) });
        if (data.wetA != nullptr && !data.wetA->spectrogramYellow.isNull())
            tracks.push_back({ data.wetA, &data.wetA->spectrogramYellow, data.wetA->sampleRate,
                               static_cast<float>(data.wetA->metrics.durationSeconds), data.label2, wetAColour(dark) });
        if (data.wetB != nullptr && !data.wetB->spectrogramPink.isNull())
            tracks.push_back({ data.wetB, &data.wetB->spectrogramPink, data.wetB->sampleRate,
                               static_cast<float>(data.wetB->metrics.durationSeconds), data.label3, wetBColour(dark) });
        return tracks;
    }

    static void drawSpectrogramTracksOnly(juce::Graphics& g, juce::Rectangle<float> area,
                                          const FigureData& data, bool dark, bool showStageMarkers)
    {
        const auto tracks = makeSpectrogramTracks(data, dark);
        if (tracks.empty())
        {
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(22.0f));
            g.drawText("No spectrogram data.", area, juce::Justification::centred);
            return;
        }

        const float gap = 18.0f;
        const float trackHeight = (area.getHeight() - gap * static_cast<float>(tracks.size() - 1))
                                / static_cast<float>(tracks.size());
        float maxDurationSeconds = 0.001f;
        for (const auto& track : tracks)
            maxDurationSeconds = juce::jmax(maxDurationSeconds, track.durationSeconds);

        auto remaining = area;
        for (const auto& track : tracks)
        {
            auto panel = remaining.removeFromTop(trackHeight);
            drawSpectrogramTrack(g, panel, *track.image, track.sampleRate,
                                 track.durationSeconds, maxDurationSeconds,
                                 track.label, track.colour, dark,
                                 showStageMarkers ? track.asset : nullptr);
            remaining.removeFromTop(gap);
        }
    }

    static void drawSpectrogramTrack(juce::Graphics& g, juce::Rectangle<float> area,
                                     const juce::Image& image, double sampleRate,
                                     float durationSeconds, float maxDurationSeconds,
                                     const juce::String& label,
                                     juce::Colour trackColour,
                                     bool dark,
                                     const Asset* markerAsset = nullptr)
    {
        g.setColour(plotColour(dark));
        g.fillRect(area);
        const float durationRatio = juce::jlimit(0.0f, 1.0f,
            durationSeconds / juce::jmax(0.001f, maxDurationSeconds));
        auto imageArea = area.withWidth(juce::jmax(1.0f, area.getWidth() * durationRatio));
        if (dark)
        {
            g.drawImage(image, imageArea, juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            auto lightImage = makeLightSpectrogramImage(image, trackColour);
            g.drawImage(lightImage, imageArea, juce::RectanglePlacement::stretchToFit);
        }

        g.setColour(dark ? juce::Colours::white.withAlpha(0.88f) : trackColour.darker(0.45f));
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText(label, area.reduced(10.0f, 6.0f), juce::Justification::topLeft);

        const float nyquist = sampleRate > 0.0 ? static_cast<float>(sampleRate * 0.5) : 24000.0f;
        const float freqs[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        g.setFont(juce::Font(13.0f));
        for (float freq : freqs)
        {
            if (freq > nyquist)
                continue;

            const float y = area.getBottom() - area.getHeight() * juce::jlimit(0.0f, 1.0f, freq / juce::jmax(1.0f, nyquist));
            g.setColour(dark ? juce::Colours::white.withAlpha(0.18f)
                             : (academicLight() ? juce::Colour(0xFFD7DEE8)
                                                : juce::Colour(0xFF334155).withAlpha(0.20f)));
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
            g.setColour(dark ? juce::Colours::white.withAlpha(0.72f)
                             : (academicLight() ? juce::Colour(0xFF374151).withAlpha(0.86f)
                                                : juce::Colour(0xFF334155).withAlpha(0.86f)));
            const auto text = freq >= 1000.0f ? juce::String(static_cast<int>(freq / 1000.0f)) + "k"
                                              : juce::String(static_cast<int>(freq));
            g.drawText(text, area.getRight() - 44.0f, y - 8.0f, 38.0f, 16.0f,
                       juce::Justification::centredRight);
        }

        if (markerAsset != nullptr)
            drawStageMarkers(g, area, markerAsset, dark, maxDurationSeconds, trackColour, false);
    }

    static juce::Image makeLightSpectrogramImage(const juce::Image& source, juce::Colour ink)
    {
        if (academicLight())
            return makeLightSpectrogramImageAcademic(source, ink);
        return makeLightSpectrogramImageOilPastel(source, ink);
    }

    static juce::Image makeLightSpectrogramImageAcademic(const juce::Image& source, juce::Colour ink)
    {
        juce::Image out(juce::Image::RGB, source.getWidth(), source.getHeight(), true);
        const auto paper = juce::Colours::white;
        const auto field = juce::Colour(0xFFEEF2F7);
        const auto deepInk = ink.darker(0.10f);

        for (int y = 0; y < source.getHeight(); ++y)
        {
            for (int x = 0; x < source.getWidth(); ++x)
            {
                const auto c = source.getPixelAt(x, y);
                const float raw = juce::jmax(c.getFloatRed(), juce::jmax(c.getFloatGreen(), c.getFloatBlue()));
                const float energy = juce::jlimit(0.0f, 1.0f, (raw - 0.075f) / 0.86f);
                if (energy < 0.016f)
                {
                    out.setPixelAt(x, y, paper);
                    continue;
                }

                const float shaped = std::pow(energy, 0.72f);
                auto outColour = field.interpolatedWith(deepInk, shaped * 0.82f);
                if (energy > 0.82f)
                    outColour = outColour.interpolatedWith(ink.darker(0.28f), (energy - 0.82f) * 0.75f);
                out.setPixelAt(x, y, outColour);
            }
        }

        return out;
    }

    static juce::Image makeLightSpectrogramImageOilPastel(const juce::Image& source, juce::Colour ink)
    {
        juce::Image out(juce::Image::RGB, source.getWidth(), source.getHeight(), true);
        const auto paper = juce::Colour(0xFFFBFCFE);
        const auto deepInk = ink.darker(0.18f);

        for (int y = 0; y < source.getHeight(); ++y)
        {
            for (int x = 0; x < source.getWidth(); ++x)
            {
                const auto c = source.getPixelAt(x, y);
                const float energy = juce::jlimit(0.0f, 1.0f,
                    (juce::jmax(c.getFloatRed(), juce::jmax(c.getFloatGreen(), c.getFloatBlue())) - 0.095f) / 0.84f);
                const float shaped = std::pow(energy, 0.78f);
                auto outColour = paper.interpolatedWith(deepInk, shaped * 0.86f);
                if (energy < 0.018f)
                    outColour = paper;
                out.setPixelAt(x, y, outColour);
            }
        }

        return out;
    }

    static void drawStageMarkers(juce::Graphics& g, juce::Rectangle<float> plot, const Asset* asset,
                                 bool dark, float maxTime, juce::Colour colour, bool showLabels)
    {
        if (asset == nullptr || asset->stageMarkers.empty())
            return;

        g.setFont(juce::Font(showLabels ? 18.0f : 14.0f, juce::Font::bold));
        for (const auto& marker : asset->stageMarkers)
        {
            const float start = static_cast<float>(marker.startSec);
            const float end = static_cast<float>(marker.endSec);
            const float x1 = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, start / juce::jmax(0.001f, maxTime));
            const float x2 = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, end / juce::jmax(0.001f, maxTime));
            const float w = juce::jmax(1.0f, x2 - x1);

            g.setColour(colour.withAlpha(dark ? 0.13f : 0.10f));
            g.fillRect(juce::Rectangle<float>(x1, plot.getY(), w, plot.getHeight()));
            g.setColour(colour.withAlpha(dark ? 0.78f : 0.62f));
            g.drawVerticalLine(static_cast<int>(x1), plot.getY(), plot.getBottom());

            if (showLabels)
            {
                const auto label = marker.label.isNotEmpty() ? marker.label : "stage";
                g.setColour(plateColour(dark).withAlpha(dark ? 0.78f : 0.86f));
                g.fillRoundedRectangle(x1 + 6.0f, plot.getY() + 8.0f, 96.0f, 24.0f, 5.0f);
                g.setColour(colour);
                g.drawText(label, x1 + 10.0f, plot.getY() + 8.0f, 88.0f, 24.0f,
                           juce::Justification::centredLeft, true);
            }
        }
    }

    static void drawBandEnergyTable(juce::Graphics& g, juce::Rectangle<float> area,
                                    const FigureData& data, bool dark)
    {
        auto left = area.removeFromLeft(area.getWidth() * 0.58f);
        area.removeFromLeft(40.0f);
        auto right = area;

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(22.0f, juce::Font::bold));
        g.drawText("Band energy (average spectrum)", left.removeFromTop(30.0f),
                   juce::Justification::centredLeft, true);

        auto drawBandRow = [&](const Asset* asset, const juce::String& label, juce::Colour colour)
        {
            if (asset == nullptr)
                return;

            auto row = left.removeFromTop(36.0f);
            g.setColour(colour);
            g.fillRect(row.removeFromLeft(14.0f).reduced(0.0f, 5.0f));
            row.removeFromLeft(10.0f);

            const auto text = label
                + " | low 20-250 " + juce::String(averageSpectrumBandDb(asset->spectrum, 20.0f, 250.0f), 1) + " dB"
                + " | mid 250-2k " + juce::String(averageSpectrumBandDb(asset->spectrum, 250.0f, 2000.0f), 1) + " dB"
                + " | high 2k-20k " + juce::String(averageSpectrumBandDb(asset->spectrum, 2000.0f, 20000.0f), 1) + " dB";
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(20.0f, juce::Font::bold));
            g.drawText(text, row, juce::Justification::centredLeft, true);
        };

        drawBandRow(data.dry, data.label1, dryColour(dark));
        drawBandRow(data.wetA, data.label2, wetAColour(dark));
        drawBandRow(data.wetB, data.label3, wetBColour(dark));

        drawPluginParameterPanel(g, right, data, dark);
    }

    static juce::Rectangle<float> getPlotArea(juce::Rectangle<float> area)
    {
        return area.withTrimmedLeft(104.0f)
                   .withTrimmedRight(28.0f)
                   .withTrimmedTop(56.0f)
                   .withTrimmedBottom(62.0f);
    }

    static float getMetricsHeight(const FigureData& data, MetricsKind metricsKind)
    {
        const bool dense = metricsKind != MetricsKind::basic;
        int assetCount = 0;
        if (data.dry != nullptr)  ++assetCount;
        if (data.wetA != nullptr) ++assetCount;
        if (data.wetB != nullptr) ++assetCount;

        int pluginCount = 0;
        if (data.pluginA.valid) ++pluginCount;
        if (data.pluginB.valid) ++pluginCount;
        if (data.pluginC.valid) ++pluginCount;

        const float assetHeight = static_cast<float>(assetCount) * (dense ? 66.0f : 38.0f);
        const float pluginHeight = static_cast<float>(pluginCount) * 108.0f;
        const float desired = juce::jmax(assetHeight, pluginHeight) + 14.0f;
        return juce::jlimit(dense ? 138.0f : 98.0f, dense ? 238.0f : 190.0f, desired);
    }

    static void drawPlotBackground(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                                   const juce::String& xLabel, const juce::String& yLabel, bool dark,
                                   bool logX, float maxX, float minY, float maxY)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto plot = getPlotArea(area);
        g.setColour(plotColour(dark));
        g.fillRect(plot);
        g.setColour(dark ? juce::Colour(0x36F6F8FB) : juce::Colour(0x30000000));
        for (int i = 0; i <= 5; ++i)
        {
            const float y = plot.getY() + plot.getHeight() * static_cast<float>(i) / 5.0f;
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        }

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(34.0f, juce::Font::bold));
        g.drawText(title, area.removeFromTop(42.0f).reduced(12.0f, 0.0f), juce::Justification::centredLeft);
        drawAxisTicks(g, plot, dark, logX, maxX, minY, maxY);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(23.0f, juce::Font::bold));
        g.drawText(xLabel, plot.withY(plot.getBottom() + 24.0f).withHeight(28.0f), juce::Justification::centred);
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.drawFittedText(yLabel,
                         juce::Rectangle<int>(static_cast<int>(plot.getX() - 176.0f),
                                              static_cast<int>(plot.getCentreY() - 12.0f),
                                              96,
                                              24),
                         juce::Justification::centredRight,
                         1,
                         0.7f);
    }

    static void drawAxisTicks(juce::Graphics& g, juce::Rectangle<float> plot, bool dark,
                              bool logX, float maxX, float minY, float maxY)
    {
        const auto tickColour = dark ? juce::Colour(0xFFEAF0F8).withAlpha(0.82f)
                                     : (academicLight() ? juce::Colour(0xFF374151).withAlpha(0.86f)
                                                        : juce::Colour(0xFF293241).withAlpha(0.82f));
        const auto gridColour = dark ? juce::Colour(0x40F6F8FB)
                                     : (academicLight() ? juce::Colour(0xFFD7DEE8)
                                                        : juce::Colour(0x35000000));

        g.setFont(juce::Font(19.0f));
        for (int i = 0; i <= 5; ++i)
        {
            const float value = maxY - (maxY - minY) * static_cast<float>(i) / 5.0f;
            const float y = plot.getY() + plot.getHeight() * static_cast<float>(i) / 5.0f;
            g.setColour(gridColour);
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            g.setColour(tickColour);
            g.drawText(juce::String(value, std::abs(value) >= 10.0f ? 0 : 1),
                       plot.getX() - 64.0f, y - 12.0f, 54.0f, 24.0f,
                       juce::Justification::centredRight);
        }

        if (logX)
        {
            const float ticks[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
            for (float freq : ticks)
            {
                const float xNorm = (std::log10(freq) - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));
                const float x = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, xNorm);
                g.setColour(gridColour);
                g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
                g.setColour(tickColour);
                g.drawText(formatFrequencyTick(freq), x - 31.0f, plot.getBottom() + 8.0f, 62.0f, 24.0f,
                           juce::Justification::centred);
            }
            return;
        }

        const int divisions = 5;
        for (int i = 0; i <= divisions; ++i)
        {
            const float t = maxX * static_cast<float>(i) / static_cast<float>(divisions);
            const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(divisions);
            g.setColour(gridColour);
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
            g.setColour(tickColour);
            g.drawText(juce::String(t, maxX >= 10.0f ? 1 : 2) + "s",
                       x - 31.0f, plot.getBottom() + 8.0f, 62.0f, 24.0f,
                       juce::Justification::centred);
        }
    }

    static juce::String formatFrequencyTick(float hz)
    {
        if (hz >= 1000.0f)
        {
            const float khz = hz / 1000.0f;
            if (std::abs(khz - std::round(khz)) < 0.001f)
                return juce::String(static_cast<int>(std::round(khz))) + "k";
            return juce::String(khz, 1).trimCharactersAtEnd("0").trimCharactersAtEnd(".") + "k";
        }
        return juce::String(static_cast<int>(hz));
    }

    static void drawCurve(juce::Graphics& g, juce::Rectangle<float> plot, const Asset* asset, CurveMember member,
                          juce::Colour colour, bool logX, float maxX, float minY, float maxY)
    {
        if (asset == nullptr)
            return;

        const auto& points = asset->*member;
        drawPointCurve(g, plot, points, colour, logX, maxX, minY, maxY, 4.0f);
    }

    static void drawPointCurve(juce::Graphics& g, juce::Rectangle<float> plot,
                               const std::vector<PlotPoint>& points, juce::Colour colour,
                               bool logX, float maxX, float minY, float maxY, float strokeWidth)
    {
        if (points.empty())
            return;

        juce::Path path;
        bool started = false;
        for (const auto& p : points)
        {
            const float xNorm = logX
                ? (std::log10(juce::jlimit(20.0f, 20000.0f, p.x)) - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f))
                : juce::jlimit(0.0f, 1.0f, p.x / juce::jmax(0.001f, maxX));
            const float yNorm = (juce::jlimit(minY, maxY, p.y) - minY) / (maxY - minY);
            const float x = plot.getX() + plot.getWidth() * xNorm;
            const float y = plot.getBottom() - plot.getHeight() * yNorm;
            if (!started)
            {
                path.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(strokeWidth));
    }

    static void drawApparentDeltaCurve(juce::Graphics& g, juce::Rectangle<float> plot,
                                       const Asset* reference, const Asset* target,
                                       juce::Colour colour, bool logX, float maxX,
                                       float minY, float maxY)
    {
        if (reference == nullptr || target == nullptr)
            return;

        const auto curve = computeApparentAttenuationCurve(reference->dynamicsRms, target->dynamicsRms);
        drawPointCurve(g, plot, curve, colour, logX, maxX, minY, maxY, 4.0f);
    }

    static float xForFrequency(float frequencyHz, juce::Rectangle<float> plot)
    {
        const float norm = (std::log10(juce::jlimit(20.0f, 20000.0f, frequencyHz)) - std::log10(20.0f))
                         / (std::log10(20000.0f) - std::log10(20.0f));
        return plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, norm);
    }

    static float yForValue(float value, juce::Rectangle<float> plot, float minY, float maxY)
    {
        const float norm = (juce::jlimit(minY, maxY, value) - minY) / (maxY - minY);
        return plot.getBottom() - plot.getHeight() * norm;
    }

    static const Asset* chooseHarmonicReference(const FigureData& data)
    {
        if (data.wetA != nullptr && !data.wetA->harmonicPeaks.empty())
            return data.wetA;
        if (data.dry != nullptr && !data.dry->harmonicPeaks.empty())
            return data.dry;
        if (data.wetB != nullptr && !data.wetB->harmonicPeaks.empty())
            return data.wetB;
        return nullptr;
    }

    static juce::Colour harmonicGlowColour(bool dark, int lane)
    {
        if (academicLight() && !dark)
        {
            if (lane == 1)
                return juce::Colour(0xFF4F46E5);
            if (lane == 2)
                return juce::Colour(0xFF008A5B);
            return juce::Colour(0xFFB91C1C);
        }

        if (lane == 1)
            return dark ? juce::Colour(0xFF6EE7FF) : juce::Colour(0xFF047C9D);
        if (lane == 2)
            return dark ? juce::Colour(0xFF9DFF6A) : juce::Colour(0xFF2F7D32);
        return dark ? juce::Colour(0xFFFFD166) : juce::Colour(0xFFB45309);
    }

    static void drawGlowPoint(juce::Graphics& g, float x, float y, juce::Colour colour, bool dark)
    {
        const float glow = 25.0f;
        g.setColour(colour.withAlpha(dark ? 0.15f : 0.11f));
        g.fillEllipse(x - glow * 0.5f, y - glow * 0.5f, glow, glow);

        const float mid = 15.0f;
        g.setColour(colour.withAlpha(dark ? 0.34f : 0.25f));
        g.fillEllipse(x - mid * 0.5f, y - mid * 0.5f, mid, mid);

        const float core = 8.0f;
        g.setColour(colour);
        g.fillEllipse(x - core * 0.5f, y - core * 0.5f, core, core);

        const float spark = 3.0f;
        g.setColour((dark ? juce::Colours::white : juce::Colour(0xFFFFFBF0)).withAlpha(0.86f));
        g.fillEllipse(x - spark * 0.5f, y - spark * 0.5f, spark, spark);
    }

    static void drawHarmonicPeakOverlay(juce::Graphics& g, juce::Rectangle<float> plot,
                                        const FigureData& data, bool dark,
                                        float minY, float maxY)
    {
        const auto gridText = dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF293241);
        if (const auto* reference = chooseHarmonicReference(data))
        {
            g.setFont(juce::Font(15.0f, juce::Font::bold));
            int drawn = 0;
            for (const auto& peak : reference->harmonicPeaks)
            {
                if (!peak.nearHarmonic || peak.expectedHz < 20.0f || peak.expectedHz > 20000.0f)
                    continue;

                const float x = xForFrequency(peak.expectedHz, plot);
                g.setColour(gridText.withAlpha(dark ? 0.16f : 0.20f));
                g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
                g.setColour(gridText.withAlpha(0.62f));
                g.drawText("H" + juce::String(peak.harmonicNumber),
                           x - 20.0f, plot.getY() + 4.0f, 40.0f, 20.0f,
                           juce::Justification::centred);

                if (++drawn >= 14)
                    break;
            }
        }

        auto drawPeakSet = [&](const Asset* asset, juce::Colour colour, int lane)
        {
            if (asset == nullptr)
                return;

            int labels = 0;
            for (const auto& peak : asset->harmonicPeaks)
            {
                if (!peak.nearHarmonic || peak.frequencyHz < 20.0f || peak.frequencyHz > 20000.0f)
                    continue;

                const float x = xForFrequency(peak.frequencyHz, plot);
                const float y = yForValue(peak.magnitudeDb, plot, minY, maxY);
                const auto glowColour = harmonicGlowColour(dark, lane);
                drawGlowPoint(g, x, y, glowColour, dark);

                if (labels < 6)
                {
                    const auto label = "H" + juce::String(peak.harmonicNumber) + " "
                                     + formatFrequencyTick(peak.frequencyHz);
                    const float labelY = y - 28.0f - static_cast<float>(lane) * 21.0f;
                    g.setColour(plateColour(dark).withAlpha(dark ? 0.74f : 0.82f));
                    g.fillRoundedRectangle(x + 8.0f, labelY, 72.0f, 20.0f, 5.0f);
                    g.setColour(glowColour);
                    g.setFont(juce::Font(15.0f, juce::Font::bold));
                    g.drawText(label, x + 12.0f, labelY, 64.0f, 20.0f,
                               juce::Justification::centredLeft, true);
                    ++labels;
                }
            }
        };

        drawPeakSet(data.dry, dryColour(dark), 0);
        drawPeakSet(data.wetA, wetAColour(dark), 1);
        drawPeakSet(data.wetB, wetBColour(dark), 2);
    }

    static float getMaxCurveX(const FigureData& data, CurveMember member, bool frequency)
    {
        if (frequency)
            return 20000.0f;

        float maxX = 0.001f;
        for (const auto* asset : { data.dry, data.wetA, data.wetB })
            if (asset != nullptr)
                for (const auto& p : asset->*member)
                    maxX = juce::jmax(maxX, p.x);
        return maxX;
    }

    static void drawFigureMetrics(juce::Graphics& g, juce::Rectangle<float> area,
                                  const FigureData& data, bool dark, MetricsKind metricsKind)
    {
        const bool dense = metricsKind != MetricsKind::basic;
        auto assetArea = area.removeFromLeft(area.getWidth() * 0.62f);
        area.removeFromLeft(42.0f);
        auto pluginArea = area;

        auto drawAsset = [&](const Asset* asset, const juce::String& label, juce::Colour colour)
        {
            if (asset == nullptr)
                return;
            auto row = assetArea.removeFromTop(dense ? 66.0f : 38.0f);
            g.setColour(colour);
            g.fillRect(row.removeFromLeft(14.0f).reduced(0.0f, dense ? 8.0f : 5.0f));

            const auto firstLine = label + " | " + juce::String(asset->metrics.sampleRate, 0) + " Hz | "
                + juce::String(asset->metrics.channels) + " ch | "
                + juce::String(asset->metrics.durationSeconds, 2) + " s | peak "
                + juce::String(asset->metrics.peakDb, 1) + " dB | rms "
                + juce::String(asset->metrics.rmsDb, 1) + " dB | crest "
                + juce::String(asset->metrics.crestDb, 1) + " dB";

            auto textRow = row.reduced(10.0f, 0.0f);
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText(firstLine, textRow.removeFromTop(dense ? 32.0f : 38.0f),
                       juce::Justification::centredLeft, true);

            if (!dense)
                return;

            const auto detail = makeAssetDetailLine(asset, label, metricsKind, data.dry);
            if (detail.isNotEmpty())
            {
                g.setColour(secondaryText(dark));
                g.setFont(juce::Font(19.0f));
                g.drawText(detail, textRow.removeFromTop(28.0f), juce::Justification::centredLeft, true);
            }
        };

        drawAsset(data.dry, data.label1, dryColour(dark));
        drawAsset(data.wetA, data.label2, wetAColour(dark));
        drawAsset(data.wetB, data.label3, wetBColour(dark));
        drawPluginParameterPanel(g, pluginArea, data, dark);
    }

    static juce::String makeAssetDetailLine(const Asset* asset, const juce::String& label,
                                            MetricsKind metricsKind, const Asset* referenceAsset)
    {
        if (asset == nullptr)
            return {};

        if (metricsKind == MetricsKind::groupDelay)
        {
            if (label.startsWithIgnoreCase("dry"))
                return "GD reference 0.00 ms";

            const auto stats = computeGroupDelayStats(asset->groupDelay, 20.0f, 20000.0f, &asset->spectrum);
            if (!stats.valid)
                return "GD metrics unavailable: selected band is below the spectrum reliability gate";

            return "GD avg " + formatDelayMs(stats.meanMs)
                + " | abs avg " + formatDelayMs(stats.meanAbsMs)
                + " | peak " + formatDelayMs(stats.peakAbsMs)
                + " at " + formatFeatureFrequency(stats.peakFreq)
                + " | reliable "
                + formatFeatureFrequency(stats.reliableMinHz) + "-"
                + formatFeatureFrequency(stats.reliableMaxHz)
                + " | L/M/H "
                + formatDelayMs(stats.lowMeanMs) + " / "
                + formatDelayMs(stats.midMeanMs) + " / "
                + formatDelayMs(stats.highMeanMs)
                + " | span " + formatDelayMs(stats.maxMs - stats.minMs);
        }

        if (metricsKind == MetricsKind::dynamics && asset->dynamicsMetrics.valid)
        {
            const auto& m = asset->dynamicsMetrics;
            auto line = "RMS P10/P50/P90 "
                + juce::String(m.rmsP10Db, 1) + " / "
                + juce::String(m.rmsP50Db, 1) + " / "
                + juce::String(m.rmsP90Db, 1) + " dB"
                + " | range " + juce::String(m.rmsRangeDb, 1) + " dB"
                + " | transient/sustain " + juce::String(m.transientToSustainDb, 1) + " dB";
            if (!label.startsWithIgnoreCase("dry"))
            {
                const auto apparentLine = formatApparentAttenuationLine(referenceAsset, asset);
                if (apparentLine.isNotEmpty())
                    line += " | " + apparentLine;
            }
            return line;
        }

        return {};
    }

    static void drawReverbMetrics(juce::Graphics& g, juce::Rectangle<float> area, const FigureData& data, bool dark)
    {
        auto assetArea = area.removeFromLeft(area.getWidth() * 0.66f);
        area.removeFromLeft(42.0f);
        auto pluginArea = area;

        auto drawAsset = [&](const Asset* asset, const juce::String& label)
        {
            if (asset == nullptr)
                return;

            const auto& m = asset->spaceMetrics;
            auto row = assetArea.removeFromTop(64.0f);
            const auto firstLine = label + " | " + juce::String(asset->metrics.sampleRate, 0) + " Hz | "
                + juce::String(asset->metrics.durationSeconds, 2) + " s | peak "
                + juce::String(asset->metrics.peakDb, 1) + " dB | rms "
                + juce::String(asset->metrics.rmsDb, 1) + " dB | crest "
                + juce::String(asset->metrics.crestDb, 1) + " dB";
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText(firstLine, row.removeFromTop(31.0f), juce::Justification::centredLeft, true);

            if (!m.valid)
                return;

            const auto rt60Source = m.rt30Seconds > 0.0f ? "RT30" : (m.rt20Seconds > 0.0f ? "RT20" : "none");
            const auto secondLine = "EDC tail " + juce::String(m.tailEndSeconds, 2) + " s"
                + " | RT20 " + formatSeconds(m.rt20Seconds)
                + " | RT30 " + formatSeconds(m.rt30Seconds)
                + " | est. RT60 " + formatSeconds(m.rt60Seconds) + " from " + rt60Source
                + " | DRR " + juce::String(m.drrDb, 1) + " dB"
                + " | Early/Late " + juce::String(m.earlyLateDb, 1) + " dB"
                + " | Corr " + juce::String(m.stereoCorrelation, 2)
                + " | S/M " + juce::String(m.sideToMidDb, 1) + " dB";
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(19.0f));
            g.drawText(secondLine, row.removeFromTop(28.0f), juce::Justification::centredLeft, true);
        };

        drawAsset(data.dry, data.label1);
        drawAsset(data.wetA, data.label2);
        drawAsset(data.wetB, data.label3);
        drawPluginParameterPanel(g, pluginArea, data, dark);
    }

    static void drawPluginParameterPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                         const FigureData& data, bool dark)
    {
        auto drawPlugin = [&](const FigurePluginInfo& plugin, const juce::String& label, juce::Colour colour)
        {
            if (!plugin.valid)
                return;

            juce::String paramsText;
            if (plugin.changedParameters.empty())
            {
                paramsText = "default/no changed parameter";
            }
            else
            {
                const int maxParams = 8;
                const int count = juce::jmin(maxParams, static_cast<int>(plugin.changedParameters.size()));
                for (int i = 0; i < count; ++i)
                {
                    if (i > 0)
                        paramsText += " | ";
                    paramsText += plugin.changedParameters[static_cast<size_t>(i)].name
                        + " " + plugin.changedParameters[static_cast<size_t>(i)].valueText;
                }

                if (static_cast<int>(plugin.changedParameters.size()) > count)
                    paramsText += " | +" + juce::String(static_cast<int>(plugin.changedParameters.size()) - count);
            }

            auto row = area.removeFromTop(34.0f);
            g.setColour(colour);
            g.fillRect(row.removeFromLeft(16.0f).withSizeKeepingCentre(16.0f, 22.0f));

            const auto title = label + " params";
            const auto pluginText = paramsText;

            row.removeFromLeft(5.0f);
            auto titleArea = row.removeFromLeft(juce::jmin(178.0f, row.getWidth() * 0.34f));
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(21.0f, juce::Font::bold));
            g.drawText(title, titleArea, juce::Justification::centredLeft, true);
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(19.0f));
            g.drawText(pluginText, row, juce::Justification::centredLeft, true);
            area.removeFromTop(6.0f);
        };

        drawPlugin(data.pluginA, "Plugin A", wetAColour(dark));
        drawPlugin(data.pluginB, "Plugin B", wetBColour(dark));
        drawPlugin(data.pluginC, "Plugin C", dryColour(dark));
    }

    static juce::String formatSeconds(float seconds)
    {
        return seconds > 0.0f ? juce::String(seconds, 2) + " s" : "n/a";
    }

    static juce::String formatDelayMs(float ms)
    {
        return juce::String(ms, std::abs(ms) >= 10.0f ? 1 : 2) + " ms";
    }

    static juce::String formatFeatureFrequency(float hz)
    {
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                             : juce::String(hz, 0) + " Hz";
    }

    static juce::String formatSignedDb(float db)
    {
        return (db > 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    }

    static juce::String formatApparentAttenuationLine(const Asset* reference, const Asset* target)
    {
        if (reference == nullptr || target == nullptr)
            return {};

        const auto stats = computeApparentAttenuationStats(reference->dynamicsRms, target->dynamicsRms);
        if (!stats.valid)
            return {};

        return "apparent attenuation max " + juce::String(stats.maxReductionDb, 1) + " dB"
            + " at " + juce::String(stats.peakReductionSeconds, 2) + " s"
            + " | mean delta " + formatSignedDb(stats.meanDeltaDb)
            + " | max lift " + juce::String(stats.maxExpansionDb, 1) + " dB";
    }

    static GroupDelayStats computeGroupDelayStats(const std::vector<PlotPoint>& points,
                                                  float minHz = 20.0f,
                                                  float maxHz = 20000.0f,
                                                  const std::vector<PlotPoint>* spectrum = nullptr)
    {
        GroupDelayStats stats;
        double sum = 0.0;
        double sumAbs = 0.0;
        int count = 0;
        float minMs = std::numeric_limits<float>::max();
        float maxMs = std::numeric_limits<float>::lowest();
        float peakAbs = 0.0f;
        float peakFreq = 0.0f;
        double bandSums[3] = { 0.0, 0.0, 0.0 };
        int bandCounts[3] = { 0, 0, 0 };
        float spectrumPeakDb = std::numeric_limits<float>::lowest();
        const bool hasSpectrum = spectrum != nullptr && !spectrum->empty();
        if (hasSpectrum)
            for (const auto& s : *spectrum)
                if (s.x >= minHz && s.x <= maxHz)
                    spectrumPeakDb = juce::jmax(spectrumPeakDb, s.y);

        const bool useSpectrumGate = hasSpectrum && spectrumPeakDb > -119.0f;
        stats.spectrumGateDb = useSpectrumGate ? spectrumPeakDb - 45.0f : -120.0f;

        for (const auto& p : points)
        {
            if (p.x < minHz || p.x > maxHz)
                continue;

            if (useSpectrumGate)
            {
                float spectrumDb = -120.0f;
                if (!interpolatePlotYAt(*spectrum, p.x, spectrumDb) || spectrumDb < stats.spectrumGateDb)
                    continue;
            }

            const float value = p.y;
            sum += static_cast<double>(value);
            sumAbs += static_cast<double>(std::abs(value));
            minMs = juce::jmin(minMs, value);
            maxMs = juce::jmax(maxMs, value);
            stats.reliableMinHz = count == 0 ? p.x : juce::jmin(stats.reliableMinHz, p.x);
            stats.reliableMaxHz = count == 0 ? p.x : juce::jmax(stats.reliableMaxHz, p.x);

            const float absValue = std::abs(value);
            if (absValue > peakAbs)
            {
                peakAbs = absValue;
                peakFreq = p.x;
            }

            const int band = p.x < 200.0f ? 0 : (p.x < 2000.0f ? 1 : 2);
            bandSums[band] += static_cast<double>(value);
            ++bandCounts[band];
            ++count;
        }

        if (count <= 0)
            return stats;

        stats.valid = true;
        stats.meanMs = static_cast<float>(sum / static_cast<double>(count));
        stats.meanAbsMs = static_cast<float>(sumAbs / static_cast<double>(count));
        stats.minMs = minMs;
        stats.maxMs = maxMs;
        stats.peakAbsMs = peakAbs;
        stats.peakFreq = peakFreq;
        stats.lowMeanMs = bandCounts[0] > 0 ? static_cast<float>(bandSums[0] / static_cast<double>(bandCounts[0])) : 0.0f;
        stats.midMeanMs = bandCounts[1] > 0 ? static_cast<float>(bandSums[1] / static_cast<double>(bandCounts[1])) : 0.0f;
        stats.highMeanMs = bandCounts[2] > 0 ? static_cast<float>(bandSums[2] / static_cast<double>(bandCounts[2])) : 0.0f;
        stats.reliablePoints = count;
        return stats;
    }
};

} // namespace goodmeter::audio_doctor
