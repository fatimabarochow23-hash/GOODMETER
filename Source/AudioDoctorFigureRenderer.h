/*
  ==============================================================================
    AudioDoctorFigureRenderer.h
    GOODMETER Audio Doctor - shared static figure renderer for UI export and jobs.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
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
    dynamicsApparentDucking,
    spatialHeatmap,
    spatialImpression,
    maskingFusion
};

enum class TerrainCamera
{
    frontHigh,
    frontLow,
    diagonal,
    sideLow,
    sideHigh
};

enum class SpatialWindow
{
    full,
    attack,
    body,
    tail
};

struct BandHighlightBand
{
    juce::String id;
    juce::String label;
    float minHz = 20.0f;
    float maxHz = 20000.0f;
    juce::Colour colour;
    bool active = true;
};

struct BandHighlightConfig
{
    bool enabled = false;
    bool dimInactiveBands = true;
    float inactiveAlpha = 0.22f;
    float overlayAlpha = 0.55f;
    std::vector<BandHighlightBand> bands;
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
    juce::String processingNote;
    TerrainCamera terrainCamera = TerrainCamera::diagonal;
    bool terrainTimeReversed = false;
    float crystalYawRadians = -0.68f;
    float crystalPitchRadians = 0.54f;
    SpatialWindow spatialWindow = SpatialWindow::full;
    float spatialTimePositionSeconds = -1.0f;
    float spatialWindowStartSeconds = -1.0f;
    float spatialWindowEndSeconds = -1.0f;
    bool sharedScale = true;
    float sharedScaleMinDb = -120.0f;
    float sharedScaleMaxDb = 0.0f;
    BandHighlightConfig bandHighlight;
    MaskingFusionSettings maskingFusionSettings;
    std::array<const Asset*, 3> fitSources { nullptr, nullptr, nullptr };
    std::array<juce::String, 3> fitLabels { "Stem 1", "Stem 2", "Stem 3" };
    const Asset* fitBounceSource = nullptr;
    juce::String fitBounceLabel = "Auto Bounce Selected Stems";
    bool fitBounceAuto = true;
    juce::String fitFigureType = "critical_band_terrain";
};

class AudioDoctorFigureRenderer
{
public:
    static juce::Image renderImage(const FigureData& data, bool dark, int width = 1800, int height = 900,
                                   bool academicLight = false, bool previewBoost = false)
    {
        ScopedAcademicPalette academicScope(academicLight && !dark);
        ScopedPreviewBoost previewScope(previewBoost && dark);
        juce::Image image(juce::Image::RGB, juce::jmax(640, width), juce::jmax(420, height), true);
        juce::Graphics g(image);
        g.fillAll(backgroundColour(dark));

        auto bounds = image.getBounds().reduced(28).toFloat();
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(34.0f, juce::Font::bold));
        auto displayToken = data.viewToken.replace("SpatialTerrain", "Spatial Terrain")
                                          .replace("SpatialImage", "Spatial Image")
                                          .replace("SpectrogramABC2_5D", "Spectrogram A/B/C 2.5D")
                                          .replace("DodecahedronCrystal", "Dodecahedron Crystal");
        if (displayToken == "dodecahedron_crystal")
            displayToken = "Layer Fit / Fusion - Dodecahedron Crystal";
        else if (displayToken == "critical_band_crystal")
            displayToken = "Layer Fit / Fusion - Critical Band Crystal";
        else if (displayToken == "layer_fit_fusion")
            displayToken = "Layer Fit / Fusion";
        g.drawText("GOODMETER Audio Doctor - " + displayToken,
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
            case FigureView::spatialHeatmap:
                drawSpatialHeatmapFigure(g, content, data, dark);
                break;
            case FigureView::spatialImpression:
                drawSpatialImpressionFigure(g, content, data, dark);
                break;
            case FigureView::maskingFusion:
                drawMaskingFusionFigure(g, content, data, dark);
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

    static BandHighlightConfig makeDefaultBandHighlightConfig()
    {
        BandHighlightConfig config;
        config.enabled = false;
        config.dimInactiveBands = true;
        config.inactiveAlpha = 0.22f;
        config.overlayAlpha = 0.55f;
        config.bands = {
            { "low",  "Low",  20.0f,   200.0f,   juce::Colour(0xFF4EA3FF), true },
            { "mid",  "Mid",  200.0f,  2000.0f,  juce::Colour(0xFFF3C64E), true },
            { "high", "High", 2000.0f, 20000.0f, juce::Colour(0xFFF05A7E), true }
        };
        return config;
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
        auto gate = std::make_unique<juce::DynamicObject>();
        gate->setProperty("method", "cross_spectrum_magnitude_gate");
        gate->setProperty("dryWetMagnitudeGateDbBelowPeak", -60.0);
        gate->setProperty("summarySpectrumGateDbBelowPeak", stats.spectrumGateDb);
        gate->setProperty("note", "Transfer group-delay points are produced only from dry/wet bins above the cross-spectrum magnitude gate; summary stats also ignore unreliable low-spectrum bins.");
        obj->setProperty("groupDelayGate", juce::var(gate.release()));
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

    struct ScopedPreviewBoost
    {
        explicit ScopedPreviewBoost(bool enabled) : previous(previewBoostFlag())
        {
            previewBoostFlag() = enabled;
        }

        ~ScopedPreviewBoost()
        {
            previewBoostFlag() = previous;
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

    static bool& previewBoostFlag()
    {
        static thread_local bool enabled = false;
        return enabled;
    }

    static bool previewBoost()
    {
        return previewBoostFlag();
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
        if (dark && previewBoost()) return juce::Colour(0xFF0C1118);
        return dark ? juce::Colour(0xFF05070B) : juce::Colour(0xFFF4F7FB);
    }

    static juce::Colour plateColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colours::white;
        if (dark && previewBoost()) return juce::Colour(0xFF101720);
        return dark ? juce::Colour(0xFF0A0D13) : juce::Colours::white;
    }

    static juce::Colour plotColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFFEEF2F7);
        if (dark && previewBoost()) return juce::Colour(0xFF111923);
        return dark ? juce::Colour(0xFF060A10) : juce::Colour(0xFFFBFCFE);
    }

    static juce::Colour primaryText(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFF111827);
        if (dark && previewBoost()) return juce::Colour(0xFFFFFFFF);
        return dark ? juce::Colour(0xFFF6F8FB) : juce::Colour(0xFF121722);
    }

    static juce::Colour secondaryText(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFF374151);
        if (dark && previewBoost()) return juce::Colour(0xFFE6EDF6);
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

    static juce::Colour layerFitBounceColour(bool dark)
    {
        if (academicLight() && !dark) return juce::Colour(0xFF4F7D36);
        return dark ? juce::Colour(0xFFBEEA8A) : juce::Colour(0xFF5F8E3D);
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

    static float yForMaskingFrequency(float frequencyHz, juce::Rectangle<float> plot)
    {
        const float norm = (std::log10(juce::jlimit(20.0f, 20000.0f, frequencyHz)) - std::log10(20.0f))
                         / (std::log10(20000.0f) - std::log10(20.0f));
        return plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, norm);
    }

    static juce::Colour maskingSourceColour(const juce::String& source, bool dark)
    {
        if (source.containsIgnoreCase("3") || source.containsIgnoreCase("C"))
            return wetBColour(dark);
        if (source.containsIgnoreCase("2") || source.containsIgnoreCase("B"))
            return wetAColour(dark);
        return dryColour(dark);
    }

    static juce::String makeLayerFitSourceList(const std::array<juce::String, 3>& labels,
                                               const std::array<const Asset*, 3>& sources)
    {
        juce::StringArray names;
        for (int i = 0; i < 3; ++i)
            if (sources[static_cast<size_t>(i)] != nullptr)
                names.add(labels[static_cast<size_t>(i)]);
        return names.isEmpty() ? "none" : names.joinIntoString(" + ");
    }

    struct LayerFitTrack
    {
        const Asset* asset = nullptr;
        juce::String label;
        juce::Colour colour;
        int stemIndex = 0;
    };

    static std::vector<LayerFitTrack> makeLayerFitTracks(const std::array<const Asset*, 3>& sources,
                                                         const std::array<juce::String, 3>& labels,
                                                         bool dark)
    {
        const juce::Colour colours[3] = { dryColour(dark), wetAColour(dark), wetBColour(dark) };
        std::vector<LayerFitTrack> tracks;
        for (int i = 0; i < 3; ++i)
        {
            const auto* asset = sources[static_cast<size_t>(i)];
            if (asset == nullptr || asset->buffer.getNumSamples() <= 0)
                continue;
            tracks.push_back({ asset, labels[static_cast<size_t>(i)], colours[i], i });
        }
        return tracks;
    }

    static float layerFitMaxTime(const MaskingFusionAnalysis& analysis)
    {
        float maxTime = 0.001f;
        for (const auto& cell : analysis.cells)
            maxTime = juce::jmax(maxTime, cell.timeEndSeconds);
        return maxTime;
    }

    static int layerFitBandCount(const MaskingFusionAnalysis& analysis)
    {
        int maxBand = 0;
        for (const auto& cell : analysis.cells)
            maxBand = juce::jmax(maxBand, cell.bandIndex);
        return maxBand + 1;
    }

    struct LayerFitFocusedSummary
    {
        bool strongestValid = false;
        bool scoped = false;
        float riskAreaPercent = 0.0f;
        float strongestRiskFrequencyHz = 0.0f;
        float strongestRiskTimeSeconds = 0.0f;
        int strongestRiskBandIndex = -1;
    };

    struct LayerFitFocusScope
    {
        bool useBandScope = false;
        bool useTimeScope = false;
        bool scoped = false;
        float minTime = -std::numeric_limits<float>::max();
        float maxTime = std::numeric_limits<float>::max();
    };

    static juce::String layerFitBandLabel(int bandIndex)
    {
        if (bandIndex < 0)
            return "band --";

        const int oneBased = bandIndex + 1;
        return juce::String("B") + (oneBased < 10 ? "0" : "") + juce::String(oneBased);
    }

    static LayerFitFocusScope makeLayerFitFocusScope(const MaskingFusionAnalysis& analysis,
                                                     const FigureData& data)
    {
        LayerFitFocusScope scope;
        scope.useBandScope = layerFitBandSoloActive(data.bandHighlight);
        scope.useTimeScope = data.spatialTimePositionSeconds >= 0.0f;
        scope.scoped = scope.useBandScope || scope.useTimeScope;

        if (scope.useTimeScope)
        {
            const float analysisMaxTime = layerFitMaxTime(analysis);
            const float focusTime = juce::jlimit(0.0f, analysisMaxTime, data.spatialTimePositionSeconds);
            const float windowSeconds = juce::jmax(0.08f, analysis.settings.integrationTimeMs * 0.001f * 1.6f);
            scope.minTime = juce::jmax(0.0f, focusTime - windowSeconds * 0.5f);
            scope.maxTime = juce::jmin(analysisMaxTime, focusTime + windowSeconds * 0.5f);
        }

        return scope;
    }

    static bool layerFitCellMatchesScope(const MaskingFusionCell& cell,
                                         const LayerFitFocusScope& scope,
                                         const FigureData& data)
    {
        const float cellTime = (cell.timeStartSeconds + cell.timeEndSeconds) * 0.5f;
        if (scope.useTimeScope && (cellTime < scope.minTime || cellTime > scope.maxTime))
            return false;

        const float cellFrequency = (cell.frequencyLowHz + cell.frequencyHighHz) * 0.5f;
        if (scope.useBandScope && !layerFitFrequencyInActiveBand(data.bandHighlight, cellFrequency))
            return false;

        return true;
    }

    static LayerFitFocusedSummary makeLayerFitFocusedSummary(const MaskingFusionAnalysis& analysis,
                                                             const FigureData& data)
    {
        LayerFitFocusedSummary out;
        out.riskAreaPercent = analysis.summary.riskAreaPercent;

        const auto scope = makeLayerFitFocusScope(analysis, data);
        out.scoped = scope.scoped;

        int considered = 0;
        int risky = 0;
        float strongestRisk = 0.0f;
        for (const auto& cell : analysis.cells)
        {
            if (!layerFitCellMatchesScope(cell, scope, data))
                continue;

            const float cellTime = (cell.timeStartSeconds + cell.timeEndSeconds) * 0.5f;
            const float cellFrequency = (cell.frequencyLowHz + cell.frequencyHighHz) * 0.5f;
            ++considered;
            if (cell.maskingRiskIndex > 0.15f)
                ++risky;

            if (cell.maskingRiskIndex > strongestRisk)
            {
                strongestRisk = cell.maskingRiskIndex;
                out.strongestRiskFrequencyHz = cellFrequency;
                out.strongestRiskTimeSeconds = cellTime;
                out.strongestRiskBandIndex = cell.bandIndex;
                out.strongestValid = true;
            }
        }

        if (considered > 0)
            out.riskAreaPercent = static_cast<float>(100.0 * static_cast<double>(risky) / static_cast<double>(considered));
        else if (!out.scoped)
        {
            out.strongestRiskFrequencyHz = analysis.summary.strongestRiskFrequencyHz;
            out.strongestRiskTimeSeconds = analysis.summary.strongestRiskTimeSeconds;
            out.strongestRiskBandIndex = analysis.summary.strongestRiskBandIndex;
            out.strongestValid = analysis.summary.valid;
        }

        return out;
    }

    static float layerFitCellDb(const MaskingFusionCell& cell, int stemIndex, bool bouncePanel)
    {
        if (bouncePanel)
            return cell.mixDb;
        if (stemIndex == 1)
            return cell.energyBDb;
        if (stemIndex == 2)
            return cell.energyCDb;
        return cell.energyADb;
    }

    static float layerFitFrequencyNorm(float frequencyHz)
    {
        const float minHz = 20.0f;
        const float maxHz = 20000.0f;
        const float safe = juce::jlimit(minHz, maxHz, frequencyHz);
        return juce::jlimit(0.0f, 1.0f, std::log(safe / minHz) / std::log(maxHz / minHz));
    }

    static float layerFitBandLowNorm(const MaskingFusionCell& cell, int bandCount, bool criticalBands)
    {
        if (criticalBands)
            return static_cast<float>(cell.bandIndex) / static_cast<float>(juce::jmax(1, bandCount));
        return layerFitFrequencyNorm(cell.frequencyLowHz);
    }

    static float layerFitBandHighNorm(const MaskingFusionCell& cell, int bandCount, bool criticalBands)
    {
        if (criticalBands)
            return static_cast<float>(cell.bandIndex + 1) / static_cast<float>(juce::jmax(1, bandCount));
        return layerFitFrequencyNorm(cell.frequencyHighHz);
    }

    struct LayerFitDbRange
    {
        float floorDb = -90.0f;
        float ceilingDb = 0.0f;
    };

    static LayerFitDbRange makeLayerFitDbRange(const MaskingFusionAnalysis& analysis,
                                               const std::vector<LayerFitTrack>& tracks,
                                               bool includeMix)
    {
        float peak = -120.0f;
        for (const auto& cell : analysis.cells)
        {
            if (includeMix)
                peak = juce::jmax(peak, cell.mixDb);
            for (const auto& track : tracks)
                peak = juce::jmax(peak, layerFitCellDb(cell, track.stemIndex, false));
        }

        if (peak <= -119.0f)
            peak = 0.0f;
        return { peak - 72.0f, peak + 3.0f };
    }

    static float layerFitEnergyNorm(float db, LayerFitDbRange range)
    {
        const float norm = juce::jlimit(0.0f, 1.0f,
            (db - range.floorDb) / juce::jmax(1.0f, range.ceilingDb - range.floorDb));
        return std::pow(norm, 0.86f);
    }

    static void drawLayerFitTerrainGrid(juce::Graphics& g, juce::Rectangle<float> plot,
                                        const MaskingFusionAnalysis& analysis,
                                        bool dark, TerrainCamera camera, bool timeReversed,
                                        bool criticalBands)
    {
        const auto axis = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.58f : 0.40f)
                               : juce::Colour(0xFF64748B).withAlpha(0.46f);
        const auto grid = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.26f : 0.18f)
                               : juce::Colour(0xFF94A3B8).withAlpha(0.34f);
        const auto text = secondaryText(dark).withAlpha(dark ? 0.92f : 0.88f);
        const float maxTime = layerFitMaxTime(analysis);
        const int bandCount = layerFitBandCount(analysis);

        auto p00 = projectSpatialTerrainPoint(plot, 0.0f, 0.0f, 0.0f, camera);
        auto p10 = projectSpatialTerrainPoint(plot, 1.0f, 0.0f, 0.0f, camera);
        auto p01 = projectSpatialTerrainPoint(plot, 0.0f, 1.0f, 0.0f, camera);
        auto p11 = projectSpatialTerrainPoint(plot, 1.0f, 1.0f, 0.0f, camera);

        g.setColour(axis);
        g.drawLine({ p00, p10 }, 1.35f);
        g.drawLine({ p00, p01 }, 1.15f);
        g.drawLine({ p01, p11 }, 1.0f);
        g.drawLine({ p10, p11 }, 1.0f);

        g.setFont(juce::Font(18.0f, juce::Font::bold));
        for (int i = 0; i <= 4; ++i)
        {
            const float t = static_cast<float>(i) / 4.0f;
            auto a = projectSpatialTerrainPoint(plot, t, 0.0f, 0.0f, camera);
            auto b = projectSpatialTerrainPoint(plot, t, 1.0f, 0.0f, camera);
            g.setColour(grid);
            g.drawLine({ a, b }, i == 0 || i == 4 ? 1.0f : 0.68f);
            g.setColour(text);
            const float seconds = maxTime * (timeReversed ? 1.0f - t : t);
            g.drawText(juce::String(seconds, maxTime >= 10.0f ? 1 : 2) + "s",
                       a.x - 44.0f, a.y + 10.0f, 88.0f, 26.0f,
                       juce::Justification::centred);
        }

        if (criticalBands)
        {
            const int step = juce::jmax(1, bandCount / 5);
            for (int band = 0; band <= bandCount; band += step)
            {
                const float f = static_cast<float>(band) / static_cast<float>(juce::jmax(1, bandCount));
                auto a = projectSpatialTerrainPoint(plot, 0.0f, f, 0.0f, camera);
                auto b = projectSpatialTerrainPoint(plot, 1.0f, f, 0.0f, camera);
                g.setColour(grid);
                g.drawLine({ a, b }, band == 0 || band >= bandCount ? 1.0f : 0.68f);
                g.setColour(text);
                g.drawText("B" + juce::String(juce::jlimit(1, bandCount, band + 1)),
                           a.x - 62.0f, a.y - 12.0f, 54.0f, 24.0f,
                           juce::Justification::centredRight);
            }
        }
        else
        {
            for (float hz : { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
            {
                const float f = layerFitFrequencyNorm(hz);
                auto a = projectSpatialTerrainPoint(plot, 0.0f, f, 0.0f, camera);
                auto b = projectSpatialTerrainPoint(plot, 1.0f, f, 0.0f, camera);
                g.setColour(grid);
                g.drawLine({ a, b }, hz == 1000.0f || hz == 10000.0f ? 0.98f : 0.58f);
                g.setColour(text);
                g.drawText(formatFrequencyTick(hz), a.x - 70.0f, a.y - 12.0f, 62.0f, 24.0f,
                           juce::Justification::centredRight);
            }
        }
    }

    static void drawLayerFitSurface(juce::Graphics& g, juce::Rectangle<float> plot,
                                    const MaskingFusionAnalysis& analysis,
                                    const LayerFitTrack& track, bool bouncePanel,
                                    bool dark, bool criticalBands, TerrainCamera camera,
                                    bool timeReversed, LayerFitDbRange range,
                                    const BandHighlightConfig& bandHighlight)
    {
        const float maxTime = layerFitMaxTime(analysis);
        const int bandCount = layerFitBandCount(analysis);
        const auto ridge = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.44f : 0.30f)
                                : juce::Colour(0xFF111827).withAlpha(0.26f);

        for (int band = bandCount - 1; band >= 0; --band)
        {
            for (const auto& cell : analysis.cells)
            {
                if (cell.bandIndex != band)
                    continue;

                const float frequencyHz = std::sqrt(juce::jmax(1.0f, cell.frequencyLowHz * cell.frequencyHighHz));
                const bool activeBand = layerFitFrequencyInActiveBand(bandHighlight, frequencyHz);
                const float baseEnergy = layerFitEnergyNorm(layerFitCellDb(cell, track.stemIndex, bouncePanel), range);
                const float energy = baseEnergy * (activeBand ? 1.0f : (layerFitBandSoloActive(bandHighlight) ? 0.62f : 1.0f));
                if (energy < (criticalBands ? 0.014f : 0.026f))
                    continue;

                float t0 = cell.timeStartSeconds / maxTime;
                float t1 = cell.timeEndSeconds / maxTime;
                if (timeReversed)
                {
                    t0 = 1.0f - cell.timeEndSeconds / maxTime;
                    t1 = 1.0f - cell.timeStartSeconds / maxTime;
                }
                t0 = juce::jlimit(0.0f, 1.0f, t0);
                t1 = juce::jlimit(t0 + 0.001f, 1.0f, t1);

                const float f0 = layerFitBandLowNorm(cell, bandCount, criticalBands);
                const float f1 = layerFitBandHighNorm(cell, bandCount, criticalBands);
                auto p00 = projectSpatialTerrainPoint(plot, t0, f0, energy, camera);
                auto p10 = projectSpatialTerrainPoint(plot, t1, f0, energy, camera);
                auto p11 = projectSpatialTerrainPoint(plot, t1, f1, energy, camera);
                auto p01 = projectSpatialTerrainPoint(plot, t0, f1, energy, camera);

                juce::Path quad;
                quad.startNewSubPath(p00);
                quad.lineTo(p10);
                quad.lineTo(p11);
                quad.lineTo(p01);
                quad.closeSubPath();

                const float fMid = (f0 + f1) * 0.5f;
                const float shade = criticalBands ? 0.10f + fMid * 0.18f : 0.08f + fMid * 0.22f;
                auto fill = activeBand
                    ? track.colour.interpolatedWith(juce::Colours::white, dark ? energy * 0.32f : energy * 0.18f)
                                  .interpolatedWith(juce::Colours::black, dark ? shade : shade * 0.18f)
                    : layerFitBandSoloGhostColour(dark, energy, fMid);
                if (activeBand)
                    fill = applyBandHighlightTint(fill, bandHighlight, frequencyHz, dark, energy);
                if (bouncePanel && activeBand)
                    fill = fill.interpolatedWith(juce::Colour(0xFF7DDC8A), juce::jlimit(0.0f, 0.32f, std::max(0.0f, cell.mixGainDb) / 18.0f));
                const float alpha = activeBand ? (dark ? 0.54f + energy * 0.28f : 0.50f + energy * 0.24f)
                                               : (dark ? 0.12f + energy * 0.14f : 0.16f + energy * 0.12f);
                g.setColour(fill.withAlpha(alpha));
                g.fillPath(quad);

                if (energy > 0.20f)
                {
                    g.setColour(ridge.withAlpha(ridge.getFloatAlpha() * (activeBand ? (0.45f + energy * 0.65f)
                                                                                     : (0.10f + energy * 0.18f))));
                    juce::Path top;
                    top.startNewSubPath(p00);
                    top.lineTo(p10);
                    g.strokePath(top, juce::PathStrokeType(activeBand ? (0.55f + energy * 0.85f)
                                                                       : (0.35f + energy * 0.35f)));
                }
            }
        }
    }

    static void drawLayerFitRiskOverlay(juce::Graphics& g, juce::Rectangle<float> plot,
                                        const MaskingFusionAnalysis& analysis, bool criticalBands,
                                        bool dark, TerrainCamera camera, bool timeReversed, LayerFitDbRange range,
                                        const BandHighlightConfig& bandHighlight)
    {
        if (analysis.summary.sourceCount < 2)
            return;

        const float maxTime = layerFitMaxTime(analysis);
        const int bandCount = layerFitBandCount(analysis);
        for (const auto& cell : analysis.cells)
        {
            if (cell.maskingRiskIndex < 0.22f && cell.fusionTendency < 0.28f)
                continue;

            const float frequencyHz = std::sqrt(juce::jmax(1.0f, cell.frequencyLowHz * cell.frequencyHighHz));
            if (!layerFitFrequencyInActiveBand(bandHighlight, frequencyHz))
                continue;

            float t0 = cell.timeStartSeconds / maxTime;
            float t1 = cell.timeEndSeconds / maxTime;
            if (timeReversed)
            {
                t0 = 1.0f - cell.timeEndSeconds / maxTime;
                t1 = 1.0f - cell.timeStartSeconds / maxTime;
            }
            t0 = juce::jlimit(0.0f, 1.0f, t0);
            t1 = juce::jlimit(t0 + 0.001f, 1.0f, t1);

            const float f0 = layerFitBandLowNorm(cell, bandCount, criticalBands);
            const float f1 = layerFitBandHighNorm(cell, bandCount, criticalBands);
            const float fMid = (f0 + f1) * 0.5f;
            const float topDb = juce::jmax(cell.energyADb, juce::jmax(cell.energyBDb, cell.energyCDb));
            const float energy = juce::jlimit(0.06f, 1.0f, layerFitEnergyNorm(topDb, range) + 0.04f)
                               * bandHighlightEnergyScale(bandHighlight, frequencyHz);
            const float risk = juce::jlimit(0.0f, 1.0f, cell.maskingRiskIndex);
            const float fusion = juce::jlimit(0.0f, 1.0f, cell.fusionTendency);
            auto colour = juce::Colour(0xFFFF365F).interpolatedWith(juce::Colour(0xFFC77DFF), fusion * 0.55f);
            colour = applyBandHighlightTint(colour, bandHighlight, frequencyHz, dark, juce::jmax(risk, fusion));

            auto p0 = projectSpatialTerrainPoint(plot, t0, fMid, energy, camera);
            auto p1 = projectSpatialTerrainPoint(plot, t1, fMid, energy, camera);
            auto b1 = projectSpatialTerrainPoint(plot, t1, fMid, energy * 0.16f, camera);
            auto b0 = projectSpatialTerrainPoint(plot, t0, fMid, energy * 0.16f, camera);
            juce::Path ribbon;
            ribbon.startNewSubPath(p0);
            ribbon.lineTo(p1);
            ribbon.lineTo(b1);
            ribbon.lineTo(b0);
            ribbon.closeSubPath();

            const float strength = juce::jmax(risk, fusion);
            g.setColour(colour.withAlpha(0.10f + strength * 0.22f));
            g.fillPath(ribbon);
            g.setColour(colour.withAlpha(0.38f + strength * 0.40f));
            juce::Path ridge;
            ridge.startNewSubPath(p0);
            ridge.lineTo(p1);
            g.strokePath(ridge, juce::PathStrokeType(0.8f + strength * 2.1f));
        }
    }

    static void drawLayerFitTerrainPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                         const MaskingFusionAnalysis& analysis,
                                         const std::vector<LayerFitTrack>& tracks,
                                         bool bouncePanel, const juce::String& titleText,
                                         bool dark, bool criticalBands,
                                         TerrainCamera camera, bool timeReversed,
                                         const BandHighlightConfig& bandHighlight)
    {
        const auto bg = dark ? juce::Colour(0xFF070C12) : juce::Colour(0xFFF8FAFC);
        g.setColour(bg);
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.18f) : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        auto title = area.removeFromTop(42.0f).reduced(16.0f, 0.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText(titleText, title, juce::Justification::centredLeft, true);

        auto plot = area.reduced(24.0f, 16.0f);
        if (tracks.empty())
        {
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText(bouncePanel ? "Auto Bounce needs at least one selected stem." : "No selected stem is available.",
                       plot, juce::Justification::centred, true);
            return;
        }

        const auto range = makeLayerFitDbRange(analysis, tracks, bouncePanel);
        drawLayerFitTerrainGrid(g, plot, analysis, dark, camera, timeReversed, criticalBands);
        if (bouncePanel)
        {
            const LayerFitTrack bounceTrack { nullptr, "Auto Bounce", layerFitBounceColour(dark), 0 };
            drawLayerFitSurface(g, plot, analysis, bounceTrack, true, dark, criticalBands,
                                camera, timeReversed, range, bandHighlight);
        }
        else
        {
            for (const auto& track : tracks)
                drawLayerFitSurface(g, plot, analysis, track, false, dark, criticalBands,
                                    camera, timeReversed, range, bandHighlight);
        }
        if (!bouncePanel)
            drawLayerFitRiskOverlay(g, plot, analysis, criticalBands, dark,
                                    camera, timeReversed, range, bandHighlight);
    }

    struct CrystalBandSnapshot
    {
        float frequencyLowHz = 20.0f;
        float frequencyHighHz = 20000.0f;
        std::array<float, 3> stemDb { -120.0f, -120.0f, -120.0f };
        float bounceDb = -120.0f;
        float maskingRisk = 0.0f;
        float fusionTendency = 0.0f;
        float mixGainDb = 0.0f;
        float mixLossDb = 0.0f;
        float dominanceDb = 0.0f;
        juce::String dominantSource = "None";
        juce::String weakerSource = "None";
    };

    struct CrystalSnapshot
    {
        float timeSeconds = 0.0f;
        float windowStartSeconds = 0.0f;
        float windowEndSeconds = 0.0f;
        std::vector<CrystalBandSnapshot> bands;
    };

    static CrystalSnapshot makeCrystalSnapshot(const MaskingFusionAnalysis& analysis,
                                               const FigureData& data)
    {
        CrystalSnapshot snapshot;
        const int bandCount = layerFitBandCount(analysis);
        snapshot.bands.resize(static_cast<size_t>(bandCount));

        const float maxTime = layerFitMaxTime(analysis);
        snapshot.timeSeconds = data.spatialTimePositionSeconds >= 0.0f
            ? juce::jlimit(0.0f, maxTime, data.spatialTimePositionSeconds)
            : juce::jlimit(0.0f, maxTime, analysis.summary.strongestRiskTimeSeconds);

        const float windowSeconds = juce::jmax(0.08f, analysis.settings.integrationTimeMs * 0.001f * 1.6f);
        snapshot.windowStartSeconds = juce::jlimit(0.0f, maxTime, snapshot.timeSeconds - windowSeconds * 0.5f);
        snapshot.windowEndSeconds = juce::jlimit(snapshot.windowStartSeconds, maxTime,
                                                 snapshot.timeSeconds + windowSeconds * 0.5f);

        for (int band = 0; band < bandCount; ++band)
        {
            auto& out = snapshot.bands[static_cast<size_t>(band)];
            float bestDistance = std::numeric_limits<float>::max();
            const MaskingFusionCell* fallback = nullptr;
            int count = 0;

            auto absorb = [&] (const MaskingFusionCell& cell)
            {
                out.frequencyLowHz = cell.frequencyLowHz;
                out.frequencyHighHz = cell.frequencyHighHz;
                out.stemDb[0] = juce::jmax(out.stemDb[0], cell.energyADb);
                out.stemDb[1] = juce::jmax(out.stemDb[1], cell.energyBDb);
                out.stemDb[2] = juce::jmax(out.stemDb[2], cell.energyCDb);
                out.bounceDb = juce::jmax(out.bounceDb, cell.mixDb);
                out.maskingRisk = juce::jmax(out.maskingRisk, cell.maskingRiskIndex);
                out.fusionTendency = juce::jmax(out.fusionTendency, cell.fusionTendency);
                out.mixGainDb = std::abs(cell.mixGainDb) > std::abs(out.mixGainDb) ? cell.mixGainDb : out.mixGainDb;
                out.mixLossDb = std::abs(cell.mixLossDb) > std::abs(out.mixLossDb) ? cell.mixLossDb : out.mixLossDb;
                out.dominanceDb = juce::jmax(out.dominanceDb, cell.dominanceDb);
                if (out.dominantSource == "None" || cell.maskingRiskIndex >= out.maskingRisk)
                    out.dominantSource = cell.dominantSource;
                if (out.weakerSource == "None" || cell.maskingRiskIndex >= out.maskingRisk)
                    out.weakerSource = cell.weakerSource;
                ++count;
            };

            for (const auto& cell : analysis.cells)
            {
                if (cell.bandIndex != band)
                    continue;

                const float center = (cell.timeStartSeconds + cell.timeEndSeconds) * 0.5f;
                const float distance = std::abs(center - snapshot.timeSeconds);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    fallback = &cell;
                }

                if (center >= snapshot.windowStartSeconds && center <= snapshot.windowEndSeconds)
                    absorb(cell);
            }

            if (count == 0 && fallback != nullptr)
                absorb(*fallback);
        }

        return snapshot;
    }

    struct CrystalCameraProjection
    {
        float yaw = -0.62f;
        float pitch = 0.52f;
        float zoom = 1.0f;
    };

    static CrystalCameraProjection crystalProjectionForCamera(TerrainCamera camera)
    {
        switch (camera)
        {
            case TerrainCamera::frontHigh: return { 0.0f, 0.58f, 1.04f };
            case TerrainCamera::frontLow:  return { 0.0f, 0.32f, 1.08f };
            case TerrainCamera::sideLow:   return { juce::MathConstants<float>::halfPi, 0.34f, 1.08f };
            case TerrainCamera::sideHigh:  return { juce::MathConstants<float>::halfPi, 0.62f, 1.04f };
            case TerrainCamera::diagonal:
            default:                       return { -0.68f, 0.54f, 1.0f };
        }
    }

    static juce::Point<float> projectCrystalPoint(juce::Rectangle<float> plot,
                                                  float x, float y, float z,
                                                  TerrainCamera camera)
    {
        const auto p = crystalProjectionForCamera(camera);
        const float c = std::cos(p.yaw);
        const float s = std::sin(p.yaw);
        const float xr = x * c - y * s;
        const float yr = x * s + y * c;
        const float scale = juce::jmin(plot.getWidth(), plot.getHeight()) * 0.44f * p.zoom;
        return {
            plot.getCentreX() + xr * scale,
            plot.getCentreY() + yr * scale * p.pitch - z * scale * (0.62f + p.pitch * 0.36f)
        };
    }

    static juce::Colour crystalInteractionColour(const CrystalBandSnapshot& band, bool dark)
    {
        const float risk = juce::jlimit(0.0f, 1.0f, band.maskingRisk);
        const float fusion = juce::jlimit(0.0f, 1.0f, band.fusionTendency);
        const float gain = juce::jlimit(0.0f, 1.0f, band.mixGainDb / 12.0f);
        const float loss = juce::jlimit(0.0f, 1.0f, -band.mixLossDb / 18.0f);

        if (risk > 0.38f)
            return juce::Colour(0xFFFF2D78).interpolatedWith(juce::Colour(0xFFFF6B6B), risk * 0.45f);
        if (fusion > 0.30f)
            return juce::Colour(0xFF7C5CFF).interpolatedWith(juce::Colour(0xFF22D3EE), fusion * 0.34f);
        if (gain > 0.18f)
            return juce::Colour(0xFFFFD166).interpolatedWith(juce::Colours::white, gain * 0.44f);
        if (loss > 0.18f)
            return juce::Colour(0xFF8A2648).interpolatedWith(juce::Colour(0xFFFF2D78), loss * 0.25f);

        return dark ? juce::Colour(0xFF6B7280) : juce::Colour(0xFF94A3B8);
    }

    static float crystalBandStrength(const CrystalBandSnapshot& band, int mode, int stemIndex,
                                     LayerFitDbRange range)
    {
        if (mode == 0)
            return layerFitEnergyNorm(band.stemDb[static_cast<size_t>(stemIndex)], range);
        if (mode == 2)
            return layerFitEnergyNorm(band.bounceDb, range);

        const float risk = juce::jlimit(0.0f, 1.0f, band.maskingRisk);
        const float fusion = juce::jlimit(0.0f, 1.0f, band.fusionTendency);
        const float gain = juce::jlimit(0.0f, 1.0f, band.mixGainDb / 12.0f);
        const float loss = juce::jlimit(0.0f, 1.0f, -band.mixLossDb / 18.0f);
        return juce::jlimit(0.0f, 1.0f, juce::jmax(risk, juce::jmax(fusion, juce::jmax(gain, loss))));
    }

    static void drawCrystalFacets(juce::Graphics& g, juce::Rectangle<float> plot,
                                  const CrystalSnapshot& snapshot,
                                  const std::vector<LayerFitTrack>& tracks,
                                  bool dark, TerrainCamera camera,
                                  LayerFitDbRange range, int mode,
                                  bool showBandLabels = false)
    {
        struct Face
        {
            juce::Path path;
            juce::Colour fill;
            juce::Colour edge;
            float alpha = 0.4f;
            float edgeAlpha = 0.4f;
            float depth = 0.0f;
        };

        std::vector<Face> faces;
        const int bandCount = static_cast<int>(snapshot.bands.size());
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float baseInner = 0.18f;
        const float baseOuter = 0.44f;

        auto pushFace = [&] (int bandIndex, float strength, juce::Colour colour,
                             float alpha, float stemOffset)
        {
            const float a0 = twoPi * static_cast<float>(bandIndex) / static_cast<float>(bandCount) - juce::MathConstants<float>::halfPi;
            const float a1 = twoPi * static_cast<float>(bandIndex + 1) / static_cast<float>(bandCount) - juce::MathConstants<float>::halfPi;
            const float energy = juce::jlimit(0.0f, 1.0f, strength);
            const float inner = baseInner + stemOffset;
            const float outer = baseOuter + stemOffset + 0.54f * std::sqrt(energy);
            const float z0 = 0.04f + energy * 0.08f;
            const float z1 = 0.14f + energy * 0.68f;

            const auto p0 = projectCrystalPoint(plot, std::cos(a0) * inner, std::sin(a0) * inner, z0, camera);
            const auto p1 = projectCrystalPoint(plot, std::cos(a1) * inner, std::sin(a1) * inner, z0, camera);
            const auto p2 = projectCrystalPoint(plot, std::cos(a1) * outer, std::sin(a1) * outer, z1, camera);
            const auto p3 = projectCrystalPoint(plot, std::cos(a0) * outer, std::sin(a0) * outer, z1, camera);

            juce::Path path;
            path.startNewSubPath(p0);
            path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(p3);
            path.closeSubPath();

            const float mid = (a0 + a1) * 0.5f;
            const float light = 0.22f + 0.48f * juce::jlimit(0.0f, 1.0f, (std::sin(mid - 0.72f) + 1.0f) * 0.5f);
            auto fill = colour.interpolatedWith(juce::Colours::white, dark ? light * 0.20f : light * 0.10f)
                              .interpolatedWith(juce::Colours::black, dark ? (1.0f - light) * 0.18f : (1.0f - light) * 0.06f);

            Face face;
            face.path = path;
            face.fill = fill;
            face.edge = colour.interpolatedWith(juce::Colours::white, dark ? 0.28f : 0.12f);
            face.alpha = alpha * (0.42f + energy * 0.52f);
            face.edgeAlpha = juce::jlimit(0.20f, 0.92f, alpha + energy * 0.38f);
            face.depth = std::sin(mid + crystalProjectionForCamera(camera).yaw) * (outer + stemOffset);
            faces.push_back(std::move(face));
        };

        if (mode == 0)
        {
            const float offsets[3] = { -0.018f, 0.0f, 0.018f };
            for (int stem = 0; stem < 3; ++stem)
            {
                bool active = false;
                juce::Colour colour = dryColour(dark);
                for (const auto& track : tracks)
                {
                    if (track.stemIndex == stem)
                    {
                        active = true;
                        colour = track.colour;
                        break;
                    }
                }
                if (!active)
                    continue;

                for (int band = 0; band < bandCount; ++band)
                    pushFace(band, crystalBandStrength(snapshot.bands[static_cast<size_t>(band)], mode, stem, range),
                             colour, dark ? 0.42f : 0.34f, offsets[stem]);
            }
        }
        else
        {
            const auto colour = mode == 2 ? layerFitBounceColour(dark) : juce::Colour(0xFF8B5CF6);
            for (int band = 0; band < bandCount; ++band)
            {
                const auto& b = snapshot.bands[static_cast<size_t>(band)];
                auto fill = mode == 1 ? crystalInteractionColour(b, dark) : colour;
                pushFace(band, crystalBandStrength(b, mode, 0, range), fill, dark ? 0.62f : 0.52f, 0.0f);
            }
        }

        std::sort(faces.begin(), faces.end(), [] (const Face& a, const Face& b) { return a.depth < b.depth; });
        for (const auto& face : faces)
        {
            g.setColour(face.fill.withAlpha(face.alpha));
            g.fillPath(face.path);
            g.setColour(face.edge.withAlpha(face.edgeAlpha));
            g.strokePath(face.path, juce::PathStrokeType(0.85f));
        }

        const auto ringColour = dark ? juce::Colours::white.withAlpha(0.22f)
                                     : juce::Colour(0xFF334155).withAlpha(0.26f);
        g.setColour(ringColour);
        for (int i = 0; i < 3; ++i)
        {
            const float radius = baseOuter + static_cast<float>(i) * 0.18f;
            juce::Path ring;
            for (int band = 0; band <= bandCount; ++band)
            {
                const float a = twoPi * static_cast<float>(band) / static_cast<float>(bandCount) - juce::MathConstants<float>::halfPi;
                const auto p = projectCrystalPoint(plot, std::cos(a) * radius, std::sin(a) * radius, 0.0f, camera);
                if (band == 0)
                    ring.startNewSubPath(p);
                else
                    ring.lineTo(p);
            }
            g.strokePath(ring, juce::PathStrokeType(i == 0 ? 1.1f : 0.55f));
        }

        if (showBandLabels && bandCount > 0)
        {
            const auto labelFill = dark ? juce::Colour(0xFF02060A).withAlpha(0.62f)
                                        : juce::Colours::white.withAlpha(0.84f);
            const auto labelBorder = dark ? juce::Colours::white.withAlpha(0.18f)
                                          : juce::Colour(0xFF334155).withAlpha(0.18f);
            const auto labelColour = primaryText(dark).withAlpha(dark ? 0.96f : 0.90f);
            g.setFont(juce::Font(15.5f, juce::Font::bold));

            for (int band = 0; band < bandCount; ++band)
            {
                const float angle = twoPi * (static_cast<float>(band) + 0.5f) / static_cast<float>(bandCount)
                                  - juce::MathConstants<float>::halfPi;
                const float radius = baseOuter + 0.42f + (band % 2 == 0 ? 0.0f : 0.045f);
                const auto point = projectCrystalPoint(plot,
                                                       std::cos(angle) * radius,
                                                       std::sin(angle) * radius,
                                                       0.12f,
                                                       camera);
                const auto label = juce::String::formatted("B%02d", band + 1);
                auto bounds = juce::Rectangle<float>(50.0f, 23.0f).withCentre(point);
                bounds = bounds.translated(std::cos(angle) * 5.0f, std::sin(angle) * 5.0f);
                g.setColour(labelFill);
                g.fillRoundedRectangle(bounds, 5.0f);
                g.setColour(labelBorder);
                g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
                g.setColour(labelColour);
                g.drawText(label, bounds, juce::Justification::centred, true);
            }
        }
    }

    static void drawCriticalBandCrystalPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                             const CrystalSnapshot& snapshot,
                                             const std::vector<LayerFitTrack>& tracks,
                                             bool dark, TerrainCamera camera,
                                             LayerFitDbRange range, int mode,
                                             const juce::String& titleText,
                                             bool showBandLabels = false)
    {
        g.setColour(dark ? juce::Colour(0xFF070C12) : juce::Colour(0xFFF8FAFC));
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.18f) : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        auto title = area.removeFromTop(48.0f).reduced(16.0f, 0.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(23.0f, juce::Font::bold));
        g.drawText(titleText, title, juce::Justification::centredLeft, true);

        auto plot = area.reduced(16.0f, 10.0f);
        drawCrystalFacets(g, plot, snapshot, tracks, dark, camera, range, mode, showBandLabels);
    }

    static void drawCriticalBandCrystalFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                              const MaskingFusionAnalysis& analysis,
                                              const std::vector<LayerFitTrack>& tracks,
                                              const FigureData& data,
                                              bool dark)
    {
        auto snapshot = makeCrystalSnapshot(analysis, data);
        const auto range = makeLayerFitDbRange(analysis, tracks, true);
        auto header = area.removeFromTop(38.0f);
        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(19.0f, juce::Font::bold));
        g.drawText("Time " + juce::String(snapshot.timeSeconds, 2) + "s | window "
                    + juce::String(snapshot.windowStartSeconds, 2) + "-"
                    + juce::String(snapshot.windowEndSeconds, 2) + "s | Bands: "
                    + juce::String(static_cast<int>(snapshot.bands.size())),
                   header, juce::Justification::centredRight, true);

        constexpr float gap = 18.0f;
        auto left = area.removeFromLeft((area.getWidth() - gap * 2.0f) / 3.0f);
        area.removeFromLeft(gap);
        auto middle = area.removeFromLeft((area.getWidth() - gap) * 0.5f);
        area.removeFromLeft(gap);
        auto right = area;

        drawCriticalBandCrystalPanel(g, left, snapshot, tracks, dark, data.terrainCamera, range, 0,
                                     juce::String::fromUTF8("所选声层 / Selected Stems"));
        drawCriticalBandCrystalPanel(g, middle, snapshot, tracks, dark, data.terrainCamera, range, 1,
                                     juce::String::fromUTF8("析出关系 / Derived Interaction"), false);
        drawCriticalBandCrystalPanel(g, right, snapshot, tracks, dark, data.terrainCamera, range, 2,
                                     juce::String::fromUTF8("合成结果 / Auto Bounce"));
    }

    struct CrystalVec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    static CrystalVec3 crystalAdd(CrystalVec3 a, CrystalVec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    static CrystalVec3 crystalSub(CrystalVec3 a, CrystalVec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    static CrystalVec3 crystalMul(CrystalVec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    static float crystalDot(CrystalVec3 a, CrystalVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static CrystalVec3 crystalCross(CrystalVec3 a, CrystalVec3 b)
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }

    static CrystalVec3 crystalNormalise(CrystalVec3 v)
    {
        const float length = std::sqrt(juce::jmax(0.000001f, crystalDot(v, v)));
        return crystalMul(v, 1.0f / length);
    }

    struct DodecahedronFace
    {
        int faceIndex = 0;
        CrystalVec3 normal;
        std::array<CrystalVec3, 5> vertices;
    };

    static std::vector<DodecahedronFace> makeDodecahedronFaces()
    {
        const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
        const std::array<CrystalVec3, 12> icoVertices {{
            { -1.0f,  phi, 0.0f }, {  1.0f,  phi, 0.0f }, { -1.0f, -phi, 0.0f }, {  1.0f, -phi, 0.0f },
            {  0.0f, -1.0f, phi }, {  0.0f,  1.0f, phi }, {  0.0f, -1.0f, -phi }, {  0.0f,  1.0f, -phi },
            {  phi, 0.0f, -1.0f }, {  phi, 0.0f,  1.0f }, { -phi, 0.0f, -1.0f }, { -phi, 0.0f,  1.0f }
        }};
        const std::array<std::array<int, 3>, 20> icoFaces {{
            {{ 0, 11, 5 }}, {{ 0, 5, 1 }}, {{ 0, 1, 7 }}, {{ 0, 7, 10 }}, {{ 0, 10, 11 }},
            {{ 1, 5, 9 }}, {{ 5, 11, 4 }}, {{ 11, 10, 2 }}, {{ 10, 7, 6 }}, {{ 7, 1, 8 }},
            {{ 3, 9, 4 }}, {{ 3, 4, 2 }}, {{ 3, 2, 6 }}, {{ 3, 6, 8 }}, {{ 3, 8, 9 }},
            {{ 4, 9, 5 }}, {{ 2, 4, 11 }}, {{ 6, 2, 10 }}, {{ 8, 6, 7 }}, {{ 9, 8, 1 }}
        }};

        std::array<CrystalVec3, 20> dualVertices {};
        for (int i = 0; i < static_cast<int>(icoFaces.size()); ++i)
        {
            const auto& f = icoFaces[static_cast<size_t>(i)];
            dualVertices[static_cast<size_t>(i)] = crystalNormalise(crystalMul(crystalAdd(crystalAdd(icoVertices[static_cast<size_t>(f[0])],
                                                                                                    icoVertices[static_cast<size_t>(f[1])]),
                                                                                       icoVertices[static_cast<size_t>(f[2])]), 1.0f / 3.0f));
        }

        std::vector<DodecahedronFace> faces;
        faces.reserve(12);
        for (int vertexIndex = 0; vertexIndex < static_cast<int>(icoVertices.size()); ++vertexIndex)
        {
            const auto normal = crystalNormalise(icoVertices[static_cast<size_t>(vertexIndex)]);
            const auto up = std::abs(normal.z) < 0.85f ? CrystalVec3 { 0.0f, 0.0f, 1.0f }
                                                       : CrystalVec3 { 0.0f, 1.0f, 0.0f };
            const auto u = crystalNormalise(crystalCross(up, normal));
            const auto v = crystalCross(normal, u);

            std::vector<int> incident;
            for (int faceIndex = 0; faceIndex < static_cast<int>(icoFaces.size()); ++faceIndex)
            {
                const auto& f = icoFaces[static_cast<size_t>(faceIndex)];
                if (f[0] == vertexIndex || f[1] == vertexIndex || f[2] == vertexIndex)
                    incident.push_back(faceIndex);
            }

            std::sort(incident.begin(), incident.end(), [&] (int a, int b)
            {
                const auto va = dualVertices[static_cast<size_t>(a)];
                const auto vb = dualVertices[static_cast<size_t>(b)];
                const float aa = std::atan2(crystalDot(va, v), crystalDot(va, u));
                const float ab = std::atan2(crystalDot(vb, v), crystalDot(vb, u));
                return aa < ab;
            });

            if (incident.size() == 5)
            {
                DodecahedronFace face;
                face.faceIndex = vertexIndex;
                face.normal = normal;
                for (int i = 0; i < 5; ++i)
                    face.vertices[static_cast<size_t>(i)] = dualVertices[static_cast<size_t>(incident[static_cast<size_t>(i)])];
                faces.push_back(face);
            }
        }

        return faces;
    }

    struct DodecahedronProjectedPoint
    {
        juce::Point<float> point;
        float depth = 0.0f;
        float light = 0.0f;
    };

    static DodecahedronProjectedPoint projectDodecahedronPoint(juce::Rectangle<float> plot,
                                                               CrystalVec3 p,
                                                               float yaw,
                                                               float pitch,
                                                               float zoom)
    {
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float x1 = p.x * cy - p.y * sy;
        const float y1 = p.x * sy + p.y * cy;
        const float z1 = p.z;
        const float y2 = y1 * cp - z1 * sp;
        const float z2 = y1 * sp + z1 * cp;
        const float scale = juce::jmin(plot.getWidth(), plot.getHeight()) * 0.34f * zoom;
        return {
            { plot.getCentreX() + x1 * scale,
              plot.getCentreY() + y2 * scale * 0.72f - z2 * scale * 0.82f },
            y2 + z2 * 0.26f,
            juce::jlimit(0.0f, 1.0f, 0.40f + 0.60f * z2)
        };
    }

    static float dodecahedronSumDb(float a, float b)
    {
        if (a <= -119.0f)
            return b;
        if (b <= -119.0f)
            return a;
        const float hi = juce::jmax(a, b);
        return hi + 10.0f * std::log10(std::pow(10.0f, (a - hi) / 10.0f)
                                     + std::pow(10.0f, (b - hi) / 10.0f));
    }

    static CrystalBandSnapshot mergeDodecahedronFaceBands(const CrystalSnapshot& snapshot, int faceIndex)
    {
        const int a = faceIndex * 2;
        const int b = juce::jmin(a + 1, static_cast<int>(snapshot.bands.size()) - 1);
        if (a < 0 || a >= static_cast<int>(snapshot.bands.size()))
            return {};

        auto out = snapshot.bands[static_cast<size_t>(a)];
        const auto& second = snapshot.bands[static_cast<size_t>(b)];
        out.frequencyHighHz = second.frequencyHighHz;
        for (int i = 0; i < 3; ++i)
            out.stemDb[static_cast<size_t>(i)] = dodecahedronSumDb(out.stemDb[static_cast<size_t>(i)],
                                                                   second.stemDb[static_cast<size_t>(i)]);
        out.bounceDb = dodecahedronSumDb(out.bounceDb, second.bounceDb);
        if (second.maskingRisk > out.maskingRisk)
        {
            out.dominantSource = second.dominantSource;
            out.weakerSource = second.weakerSource;
        }
        out.maskingRisk = juce::jmax(out.maskingRisk, second.maskingRisk);
        out.fusionTendency = juce::jmax(out.fusionTendency, second.fusionTendency);
        out.mixGainDb = std::abs(second.mixGainDb) > std::abs(out.mixGainDb) ? second.mixGainDb : out.mixGainDb;
        out.mixLossDb = std::abs(second.mixLossDb) > std::abs(out.mixLossDb) ? second.mixLossDb : out.mixLossDb;
        out.dominanceDb = juce::jmax(out.dominanceDb, second.dominanceDb);
        return out;
    }

    static float dodecahedronFaceBandScale(const CrystalBandSnapshot& face, const BandHighlightConfig& config)
    {
        if (!layerFitBandSoloActive(config))
            return 1.0f;

        return dodecahedronFaceOverlapsActiveBand(face, config) ? 1.0f : juce::jlimit(0.10f, 1.0f, config.inactiveAlpha);
    }

    static bool dodecahedronFaceOverlapsActiveBand(const CrystalBandSnapshot& face, const BandHighlightConfig& config)
    {
        if (!layerFitBandSoloActive(config))
            return true;

        for (const auto& band : config.bands)
        {
            if (!band.active)
                continue;

            const bool overlaps = face.frequencyHighHz > band.minHz && face.frequencyLowHz < band.maxHz;
            if (overlaps)
                return true;
        }

        return false;
    }

    static juce::String dodecahedronFaceBandLabel(int faceIndex)
    {
        const int firstBand = faceIndex * 2 + 1;
        return juce::String::formatted("B%02d+B%02d", firstBand, firstBand + 1);
    }

    static const CrystalBandSnapshot* dodecahedronFaceBandAt(const CrystalSnapshot& snapshot,
                                                             int faceIndex,
                                                             int pairOffset)
    {
        const int bandIndex = faceIndex * 2 + pairOffset;
        if (bandIndex < 0 || bandIndex >= static_cast<int>(snapshot.bands.size()))
            return nullptr;

        return &snapshot.bands[static_cast<size_t>(bandIndex)];
    }

    static void drawDodecahedronCrystalSolid(juce::Graphics& g, juce::Rectangle<float> plot,
                                             const CrystalSnapshot& snapshot,
                                             const std::vector<LayerFitTrack>& tracks,
                                             bool dark,
                                             float yaw,
                                             float pitch,
                                             LayerFitDbRange range,
                                             int mode,
                                             const BandHighlightConfig& bandHighlight)
    {
        struct DrawFace
        {
            juce::Path topPath;
            std::array<juce::Path, 5> sidePaths;
            juce::Point<float> center;
            std::array<juce::Point<float>, 5> basePoints;
            std::array<juce::Point<float>, 5> topPoints;
            juce::Colour fill;
            juce::Colour sideFill;
            juce::Colour edge;
            juce::Colour ridgeA;
            juce::Colour ridgeB;
            juce::String label;
            float alpha = 0.4f;
            float edgeAlpha = 0.4f;
            float ridgeAlphaA = 0.4f;
            float ridgeAlphaB = 0.4f;
            float labelAlpha = 0.0f;
            float ridgeWidthA = 1.0f;
            float ridgeWidthB = 1.0f;
            float depth = 0.0f;
            int pointCount = 5;
        };

        const auto faces = makeDodecahedronFaces();
        std::vector<DrawFace> drawFaces;
        drawFaces.reserve(faces.size() * 3);

        auto appendWedge = [&] (const DodecahedronFace& face,
                                const CrystalBandSnapshot& band,
                                int faceBandIndex,
                                int wedgeIndex,
                                bool labelThisFace,
                                float strength,
                                juce::Colour baseColour,
                                float alpha,
                                float objectScale,
                                float objectLift)
        {
            const float soloScale = dodecahedronFaceBandScale(band, bandHighlight);
            const float energy = juce::jlimit(0.0f, 1.0f, strength);
            const float baseLift = objectLift;
            const float growth = 0.035f + 0.52f * std::sqrt(energy);
            const float scale = objectScale * 0.80f;
            const auto baseNormal = crystalMul(face.normal, baseLift);
            const auto topNormal = crystalMul(face.normal, baseLift + growth);

            DrawFace out;
            out.pointCount = 3;
            out.alpha = alpha * (0.35f + energy * 0.58f) * soloScale;
            out.edgeAlpha = juce::jlimit(0.16f, 0.88f, (alpha + energy * 0.34f) * soloScale);
            out.ridgeAlphaA = juce::jlimit(0.12f, 0.94f, (0.28f + energy * 0.58f) * soloScale);
            out.ridgeAlphaB = out.ridgeAlphaA;
            out.label = labelThisFace ? dodecahedronFaceBandLabel(faceBandIndex) : juce::String();
            const bool labelInFocus = dodecahedronFaceOverlapsActiveBand(band, bandHighlight);
            out.labelAlpha = (mode == 0 || !labelInFocus) ? 0.0f
                                                           : juce::jlimit(0.18f, 0.90f, (0.32f + energy * 0.44f) * soloScale);
            out.ridgeWidthA = 0.9f + 2.0f * energy;
            out.ridgeWidthB = 0.8f + 1.8f * energy;
            out.fill = baseColour.interpolatedWith(juce::Colours::white, dark ? 0.10f : 0.05f);
            out.sideFill = baseColour.interpolatedWith(juce::Colours::black, dark ? 0.20f : 0.08f);
            out.edge = baseColour.interpolatedWith(juce::Colours::white, dark ? 0.55f : 0.20f);
            out.ridgeA = baseColour.interpolatedWith(juce::Colours::white, dark ? 0.80f : 0.35f);
            out.ridgeB = baseColour.interpolatedWith(juce::Colours::black, dark ? 0.08f : 0.16f);

            float depth = 0.0f;
            const int nextWedge = (wedgeIndex + 1) % 5;
            const std::array<CrystalVec3, 3> wedgeVertices {{
                face.normal,
                face.vertices[static_cast<size_t>(wedgeIndex)],
                face.vertices[static_cast<size_t>(nextWedge)]
            }};

            auto projectedCenter = projectDodecahedronPoint(plot, crystalAdd(crystalMul(face.normal, scale), topNormal), yaw, pitch, 1.0f);
            out.center = projectedCenter.point;
            juce::Point<float> centroid;
            for (int i = 0; i < out.pointCount; ++i)
            {
                const auto base3 = crystalAdd(crystalMul(wedgeVertices[static_cast<size_t>(i)], scale), baseNormal);
                const auto top3 = crystalAdd(crystalMul(wedgeVertices[static_cast<size_t>(i)], scale), topNormal);
                const auto projectedBase = projectDodecahedronPoint(plot, base3, yaw, pitch, 1.0f);
                const auto projectedTop = projectDodecahedronPoint(plot, top3, yaw, pitch, 1.0f);
                out.basePoints[static_cast<size_t>(i)] = projectedBase.point;
                out.topPoints[static_cast<size_t>(i)] = projectedTop.point;
                centroid += projectedTop.point;
                depth += (projectedBase.depth + projectedTop.depth) * 0.5f;
            }
            out.center = centroid / static_cast<float>(out.pointCount);
            out.depth = depth / static_cast<float>(out.pointCount);

            const float shade = juce::jlimit(0.0f, 1.0f, projectedCenter.light);
            out.fill = out.fill.interpolatedWith(dark ? juce::Colours::white : juce::Colour(0xFF111827),
                                                 dark ? shade * 0.18f : (1.0f - shade) * 0.06f);
            out.sideFill = out.sideFill.interpolatedWith(dark ? juce::Colours::white : juce::Colour(0xFF111827),
                                                         dark ? shade * 0.10f : (1.0f - shade) * 0.04f);
            out.topPath.startNewSubPath(out.topPoints[0]);
            for (int i = 1; i < out.pointCount; ++i)
                out.topPath.lineTo(out.topPoints[static_cast<size_t>(i)]);
            out.topPath.closeSubPath();
            for (int i = 0; i < out.pointCount; ++i)
            {
                const int next = (i + 1) % out.pointCount;
                auto& side = out.sidePaths[static_cast<size_t>(i)];
                side.startNewSubPath(out.basePoints[static_cast<size_t>(i)]);
                side.lineTo(out.basePoints[static_cast<size_t>(next)]);
                side.lineTo(out.topPoints[static_cast<size_t>(next)]);
                side.lineTo(out.topPoints[static_cast<size_t>(i)]);
                side.closeSubPath();
            }
            drawFaces.push_back(std::move(out));
        };

        auto appendFaceBandPair = [&] (const DodecahedronFace& face,
                                       int faceIndex,
                                       const CrystalBandSnapshot& bandA,
                                       const CrystalBandSnapshot& bandB,
                                       float strengthA,
                                       float strengthB,
                                       juce::Colour colourA,
                                       juce::Colour colourB,
                                       float alpha,
                                       float objectScale,
                                       float objectLift)
        {
            const bool firstBandIsStronger = strengthA >= strengthB;
            const int strongOffset = faceIndex % 5;
            bool placedLabel = false;

            for (int wedge = 0; wedge < 5; ++wedge)
            {
                const int rotated = (wedge + 5 - strongOffset) % 5;
                const bool useStrongBand = rotated < 3;
                const bool useFirstBand = useStrongBand ? firstBandIsStronger : !firstBandIsStronger;
                const bool shouldLabel = !placedLabel && mode != 0 && useStrongBand;
                if (shouldLabel)
                    placedLabel = true;

                appendWedge(face,
                            useFirstBand ? bandA : bandB,
                            faceIndex,
                            wedge,
                            shouldLabel,
                            useFirstBand ? strengthA : strengthB,
                            useFirstBand ? colourA : colourB,
                            alpha,
                            objectScale,
                            objectLift);
            }
        };

        const int faceCount = juce::jmin(12, static_cast<int>((snapshot.bands.size() + 1) / 2));
        if (mode == 0)
        {
            for (const auto& track : tracks)
            {
                if (track.asset == nullptr)
                    continue;

                const float offset = (static_cast<float>(track.stemIndex) - 1.0f) * 0.035f;
                for (int face = 0; face < faceCount; ++face)
                {
                    const auto* bandA = dodecahedronFaceBandAt(snapshot, face, 0);
                    const auto* bandB = dodecahedronFaceBandAt(snapshot, face, 1);
                    if (bandA == nullptr)
                        continue;
                    if (bandB == nullptr)
                        bandB = bandA;

                    const float strengthA = crystalBandStrength(*bandA, mode, track.stemIndex, range);
                    const float strengthB = crystalBandStrength(*bandB, mode, track.stemIndex, range);
                    appendFaceBandPair(faces[static_cast<size_t>(face)], face, *bandA, *bandB,
                                       strengthA, strengthB,
                                       track.colour, track.colour,
                                       dark ? 0.46f : 0.36f,
                                       0.96f + offset, offset);
                }
            }
        }
        else
        {
            for (int face = 0; face < faceCount; ++face)
            {
                const auto* bandA = dodecahedronFaceBandAt(snapshot, face, 0);
                const auto* bandB = dodecahedronFaceBandAt(snapshot, face, 1);
                if (bandA == nullptr)
                    continue;
                if (bandB == nullptr)
                    bandB = bandA;

                const float strengthA = crystalBandStrength(*bandA, mode, 0, range);
                const float strengthB = crystalBandStrength(*bandB, mode, 0, range);
                const auto colourA = mode == 2 ? layerFitBounceColour(dark) : crystalInteractionColour(*bandA, dark);
                const auto colourB = mode == 2 ? layerFitBounceColour(dark) : crystalInteractionColour(*bandB, dark);
                appendFaceBandPair(faces[static_cast<size_t>(face)], face, *bandA, *bandB,
                                   strengthA, strengthB, colourA, colourB,
                                   dark ? 0.64f : 0.52f, 1.0f, 0.0f);
            }
        }

        std::sort(drawFaces.begin(), drawFaces.end(), [] (const DrawFace& a, const DrawFace& b)
        {
            return a.depth > b.depth;
        });

        for (const auto& face : drawFaces)
        {
            for (int i = 0; i < face.pointCount; ++i)
            {
                const float sideShade = 0.72f + static_cast<float>(i) * 0.06f;
                g.setColour(face.sideFill.withAlpha(face.alpha * 0.72f).withMultipliedBrightness(sideShade));
                g.fillPath(face.sidePaths[static_cast<size_t>(i)]);
            }

            g.setColour(face.fill.withAlpha(face.alpha));
            g.fillPath(face.topPath);
            g.setColour(face.edge.withAlpha(face.edgeAlpha));
            for (const auto& side : face.sidePaths)
                g.strokePath(side, juce::PathStrokeType(0.72f));
            g.strokePath(face.topPath, juce::PathStrokeType(1.05f));

            for (int i = 0; i < face.pointCount; ++i)
            {
                juce::Path ridge;
                ridge.startNewSubPath(face.center);
                ridge.lineTo(face.topPoints[static_cast<size_t>(i)]);
                g.setColour((i % 2 == 0 ? face.ridgeA : face.ridgeB)
                                .withAlpha(i % 2 == 0 ? face.ridgeAlphaA : face.ridgeAlphaB));
                g.strokePath(ridge, juce::PathStrokeType(i % 2 == 0 ? face.ridgeWidthA : face.ridgeWidthB));
            }
        }

        if (mode != 0)
        {
            const auto labelFill = dark ? juce::Colour(0xFF02060A).withAlpha(0.58f)
                                        : juce::Colours::white.withAlpha(0.78f);
            const auto labelText = dark ? juce::Colours::white
                                        : juce::Colour(0xFF111827);
            g.setFont(juce::Font(14.5f, juce::Font::bold));
            for (const auto& face : drawFaces)
            {
                if (face.label.isEmpty() || face.labelAlpha <= 0.05f)
                    continue;

                auto labelArea = juce::Rectangle<float>(face.center.x - 36.0f, face.center.y - 10.0f, 72.0f, 20.0f);
                g.setColour(labelFill.withMultipliedAlpha(face.labelAlpha));
                g.fillRoundedRectangle(labelArea, 4.0f);
                g.setColour(labelText.withAlpha(face.labelAlpha));
                g.drawText(face.label, labelArea, juce::Justification::centred, true);
            }
        }
    }

    static void drawDodecahedronCrystalPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                             const CrystalSnapshot& snapshot,
                                             const std::vector<LayerFitTrack>& tracks,
                                             bool dark,
                                             float yaw,
                                             float pitch,
                                             LayerFitDbRange range,
                                             int mode,
                                             const BandHighlightConfig& bandHighlight,
                                             const juce::String& titleText)
    {
        g.setColour(dark ? juce::Colour(0xFF070C12) : juce::Colour(0xFFF8FAFC));
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.18f) : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        auto title = area.removeFromTop(52.0f).reduced(16.0f, 0.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(23.0f, juce::Font::bold));
        g.drawText(titleText, title, juce::Justification::centredLeft, true);

        auto plot = area.reduced(16.0f, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.10f) : juce::Colour(0xFF334155).withAlpha(0.13f));
        g.drawEllipse(plot.withSizeKeepingCentre(juce::jmin(plot.getWidth(), plot.getHeight()) * 0.78f,
                                                 juce::jmin(plot.getWidth(), plot.getHeight()) * 0.78f),
                      1.0f);
        drawDodecahedronCrystalSolid(g, plot, snapshot, tracks, dark, yaw, pitch, range, mode, bandHighlight);
    }

    static void drawDodecahedronCrystalFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                              const MaskingFusionAnalysis& analysis,
                                              const std::vector<LayerFitTrack>& tracks,
                                              const FigureData& data,
                                              bool dark)
    {
        auto snapshot = makeCrystalSnapshot(analysis, data);
        const auto range = makeLayerFitDbRange(analysis, tracks, true);
        auto header = area.removeFromTop(38.0f);
        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(19.0f, juce::Font::bold));
        g.drawText("Time " + juce::String(snapshot.timeSeconds, 2) + "s | window "
                    + juce::String(snapshot.windowStartSeconds, 2) + "-"
                    + juce::String(snapshot.windowEndSeconds, 2) + "s | 12 pentagonal faces / 24 bands",
                   header, juce::Justification::centredRight, true);

        constexpr float gap = 18.0f;
        auto left = area.removeFromLeft((area.getWidth() - gap * 2.0f) / 3.0f);
        area.removeFromLeft(gap);
        auto middle = area.removeFromLeft((area.getWidth() - gap) * 0.5f);
        area.removeFromLeft(gap);
        auto right = area;

        drawDodecahedronCrystalPanel(g, left, snapshot, tracks, dark, data.crystalYawRadians,
                                     data.crystalPitchRadians, range, 0, data.bandHighlight,
                                     juce::String::fromUTF8("所选声层 / Selected Stems"));
        drawDodecahedronCrystalPanel(g, middle, snapshot, tracks, dark, data.crystalYawRadians,
                                     data.crystalPitchRadians, range, 1, data.bandHighlight,
                                     juce::String::fromUTF8("析出关系 / Derived Interaction"));
        drawDodecahedronCrystalPanel(g, right, snapshot, tracks, dark, data.crystalYawRadians,
                                     data.crystalPitchRadians, range, 2, data.bandHighlight,
                                     juce::String::fromUTF8("合成结果 / Auto Bounce"));
    }

    static juce::String layerFitMetricBandText(const MaskingFusionAnalysis& analysis,
                                               const FigureData& data,
                                               int metricIndex)
    {
        const auto scope = makeLayerFitFocusScope(analysis, data);
        struct BandScore
        {
            int bandIndex = -1;
            float score = 0.0f;
        };

        std::vector<BandScore> scores;
        const float threshold = metricIndex == 1 ? 0.05f : 0.015f;

        auto addScore = [&] (int bandIndex, float score)
        {
            if (bandIndex < 0 || score <= threshold)
                return;

            for (auto& item : scores)
            {
                if (item.bandIndex == bandIndex)
                {
                    item.score = juce::jmax(item.score, score);
                    return;
                }
            }

            scores.push_back({ bandIndex, score });
        };

        for (const auto& cell : analysis.cells)
        {
            if (!layerFitCellMatchesScope(cell, scope, data))
                continue;

            float score = 0.0f;
            if (metricIndex == 0)
                score = cell.maskingRiskIndex;
            else if (metricIndex == 1)
                score = cell.mixGainDb;
            else
                score = cell.fusionTendency;

            addScore(cell.bandIndex, score);
        }

        if (scores.empty())
            return "B--";

        std::sort(scores.begin(), scores.end(), [] (const BandScore& a, const BandScore& b)
        {
            if (a.score == b.score)
                return a.bandIndex < b.bandIndex;
            return a.score > b.score;
        });

        const int maxBands = 3;
        juce::String text;
        for (int i = 0; i < juce::jmin(maxBands, static_cast<int>(scores.size())); ++i)
        {
            if (i > 0)
                text += " / ";
            text += layerFitBandLabel(scores[static_cast<size_t>(i)].bandIndex);
        }

        if (static_cast<int>(scores.size()) > maxBands)
            text += " +" + juce::String(static_cast<int>(scores.size()) - maxBands);

        return text;
    }

    static void drawLayerFitRelationLegend(juce::Graphics& g, juce::Rectangle<float> area,
                                           const MaskingFusionAnalysis& analysis,
                                           const FigureData& data,
                                           bool dark)
    {
        if (area.getWidth() < 420.0f || area.getHeight() < 108.0f)
            return;

        const auto red = juce::Colour(0xFFFF2D78);
        const auto yellow = juce::Colour(0xFFFFD166);
        const auto purple = juce::Colour(0xFF7C5CFF);
        const auto border = dark ? juce::Colours::white.withAlpha(0.12f)
                                 : juce::Colour(0xFFCBD5E1).withAlpha(0.70f);

        g.setColour(dark ? juce::Colour(0xFF070C12).withAlpha(0.52f)
                         : juce::Colour(0xFFF8FAFC).withAlpha(0.92f));
        g.fillRoundedRectangle(area, 9.0f);
        g.setColour(border);
        g.drawRoundedRectangle(area.reduced(0.5f), 9.0f, 1.0f);

        area.reduce(12.0f, 7.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(25.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8("析出关系颜色 / Derived colour key"),
                   area.removeFromTop(28.0f), juce::Justification::centredLeft, true);
        area.removeFromTop(2.0f);

        auto drawItem = [&] (juce::Rectangle<float> item, juce::Colour colour,
                             const juce::String& titleCn, const juce::String& titleEn,
                             const juce::String& bandText)
        {
            auto swatch = item.removeFromLeft(28.0f).reduced(0.0f, 3.0f);
            g.setColour(colour);
            g.fillRoundedRectangle(swatch, 3.0f);
            item.removeFromLeft(9.0f);

            const float bandHeight = juce::jlimit(38.0f, 52.0f, item.getHeight() * 0.42f);
            auto labelArea = item.removeFromTop(item.getHeight() - bandHeight - 3.0f);

            g.setColour(primaryText(dark));
            g.setFont(juce::Font(item.getWidth() < 170.0f ? 22.0f : 26.0f, juce::Font::bold));
            g.drawText(titleCn, labelArea.removeFromTop(labelArea.getHeight() * 0.50f),
                       juce::Justification::centredLeft, true);
            g.setColour(secondaryText(dark).withAlpha(dark ? 0.98f : 0.92f));
            g.setFont(juce::Font(item.getWidth() < 170.0f ? 20.0f : 23.0f, juce::Font::bold));
            g.drawText(titleEn, labelArea, juce::Justification::centredLeft, true);

            item.removeFromTop(3.0f);
            auto bandArea = item.removeFromTop(bandHeight);

            g.setColour(colour.withAlpha(dark ? 0.18f : 0.14f));
            g.fillRoundedRectangle(bandArea, 6.0f);
            g.setColour(colour);
            g.setFont(juce::Font(item.getWidth() < 170.0f ? 23.0f : 27.0f, juce::Font::bold));
            g.drawText(bandText, bandArea, juce::Justification::centred, true);
        };

        const auto redBand = layerFitMetricBandText(analysis, data, 0);
        const auto yellowBand = layerFitMetricBandText(analysis, data, 1);
        const auto purpleBand = layerFitMetricBandText(analysis, data, 2);

        auto row = area;
        const float itemGap = 14.0f;
        const float itemWidth = (row.getWidth() - itemGap * 2.0f) / 3.0f;
        auto redItem = row.removeFromLeft(itemWidth);
        row.removeFromLeft(itemGap);
        auto purpleItem = row.removeFromLeft(itemWidth);
        row.removeFromLeft(itemGap);
        auto yellowItem = row;

        drawItem(redItem,
                 red,
                 juce::String::fromUTF8("遮蔽风险"),
                 "Masking risk",
                 redBand);
        drawItem(purpleItem,
                 purple,
                 juce::String::fromUTF8("融合倾向"),
                 "Fusion",
                 purpleBand);
        drawItem(yellowItem,
                 yellow,
                 juce::String::fromUTF8("合成增益"),
                 "Mix gain",
                 yellowBand);
    }

    static void drawLayerFitFusionMetrics(juce::Graphics& g, juce::Rectangle<float> area,
                                          const MaskingFusionAnalysis& analysis,
                                          const FigureData& data,
                                          const std::vector<LayerFitTrack>& tracks,
                                          bool dark)
    {
        const bool showRelationLegend = analysis.settings.figureType.containsIgnoreCase("crystal")
                                     || analysis.settings.figureType.containsIgnoreCase("dodecahedron");
        if (showRelationLegend && area.getWidth() > 960.0f)
        {
            auto legendArea = area.removeFromRight(juce::jlimit(520.0f, 720.0f, area.getWidth() * 0.40f));
            area.removeFromRight(22.0f);
            drawLayerFitRelationLegend(g, legendArea.reduced(0.0f, 4.0f), analysis, data, dark);
        }

        const auto focused = makeLayerFitFocusedSummary(analysis, data);
        const auto strongestText = focused.strongestValid
            ? (formatFrequencyTick(focused.strongestRiskFrequencyHz) + " Hz - "
               + juce::String(focused.strongestRiskTimeSeconds, 2) + "s | "
               + layerFitBandLabel(focused.strongestRiskBandIndex))
            : juce::String("-- | band --");

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(25.0f, juce::Font::bold));
        auto topLine = area.removeFromTop(40.0f);
        g.drawText("Risk area " + juce::String(focused.riskAreaPercent, 1) + "% | strongest "
                    + strongestText,
                   topLine, juce::Justification::centredLeft, true);

        auto detail = area.removeFromTop(34.0f);
        const bool enoughSources = analysis.summary.sourceCount >= 2;
        g.setColour(enoughSources ? secondaryText(dark)
                                  : (dark ? juce::Colour(0xFFFFD166) : juce::Colour(0xFFB45309)));
        g.setFont(juce::Font(21.0f, enoughSources ? juce::Font::plain : juce::Font::bold));
        g.drawText("Sources " + juce::String(analysis.summary.sourceCount)
                    + (enoughSources ? "" : " | masking/fusion needs at least two selected stems")
                    + " | figure " + analysis.settings.figureType
                    + " | mean overlap " + juce::String(analysis.summary.meanOverlapDb, 1) + " dB | mix gain "
                    + juce::String(analysis.summary.meanMixGainDb, 1) + " dB | mix loss "
                    + juce::String(analysis.summary.meanMixLossDb, 1) + " dB | "
                    + analysis.settings.bandScale + " / " + analysis.settings.criticalBandMode,
                   detail, juce::Justification::centredLeft, true);

        area.removeFromTop(4.0f);
        const float rowHeight = juce::jlimit(34.0f, 44.0f,
            tracks.empty() ? 40.0f : area.getHeight() / static_cast<float>(juce::jmax(1, static_cast<int>(tracks.size()))));
        for (const auto& track : tracks)
        {
            if (track.asset == nullptr)
                continue;

            auto row = area.removeFromTop(rowHeight);
            auto swatch = row.removeFromLeft(22.0f).reduced(2.0f, 5.0f);
            g.setColour(track.colour);
            g.fillRoundedRectangle(swatch, 1.5f);
            row.removeFromLeft(10.0f);

            const auto* asset = track.asset;
            const auto text = track.label + " | " + juce::String(asset->metrics.sampleRate, 0) + " Hz | "
                + juce::String(asset->metrics.channels) + " ch | "
                + juce::String(asset->metrics.durationSeconds, 2) + " s | peak "
                + juce::String(asset->metrics.peakDb, 1) + " dB | rms "
                + juce::String(asset->metrics.rmsDb, 1) + " dB | crest "
                + juce::String(asset->metrics.crestDb, 1) + " dB";

            g.setColour(primaryText(dark));
            g.setFont(juce::Font(21.0f, juce::Font::bold));
            g.drawText(text, row, juce::Justification::centredLeft, true);
        }
    }

    static void drawMaskingHeatPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                     const MaskingFusionAnalysis& analysis, bool dark,
                                     const juce::String& titleText, bool deltaPanel)
    {
        const auto bg = dark ? juce::Colour(0xFF070C12) : juce::Colour(0xFFF8FAFC);
        g.setColour(bg);
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.18f) : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        auto title = area.removeFromTop(36.0f).reduced(16.0f, 0.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText(titleText, title, juce::Justification::centredLeft, true);

        auto plot = area.reduced(46.0f, 18.0f);
        plot.removeFromBottom(22.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.10f) : juce::Colour(0xFF94A3B8).withAlpha(0.26f));
        const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        for (float hz : freqs)
        {
            const float y = yForMaskingFrequency(hz, plot);
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        }
        for (int i = 0; i <= 4; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / 4.0f;
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
        }

        float maxTime = 0.001f;
        for (const auto& cell : analysis.cells)
            maxTime = juce::jmax(maxTime, cell.timeEndSeconds);

        for (const auto& cell : analysis.cells)
        {
            const float x0 = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, cell.timeStartSeconds / maxTime);
            const float x1 = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, cell.timeEndSeconds / maxTime);
            const float y0 = yForMaskingFrequency(cell.frequencyHighHz, plot);
            const float y1 = yForMaskingFrequency(cell.frequencyLowHz, plot);
            auto rect = juce::Rectangle<float>::leftTopRightBottom(x0, y0, juce::jmax(x0 + 1.0f, x1), juce::jmax(y0 + 1.0f, y1));

            if (deltaPanel)
            {
                const float gain = juce::jlimit(0.0f, 1.0f, cell.mixGainDb / 12.0f);
                const float loss = juce::jlimit(0.0f, 1.0f, -cell.mixLossDb / 18.0f);
                if (gain <= 0.02f && loss <= 0.02f)
                    continue;
                auto colour = gain >= loss ? juce::Colour(0xFFE9D45C).interpolatedWith(juce::Colour(0xFF7DDC8A), gain)
                                           : juce::Colour(0xFF6D5DF5).interpolatedWith(juce::Colour(0xFFFF4F83), loss);
                g.setColour(colour.withAlpha(juce::jlimit(0.12f, 0.82f, 0.16f + juce::jmax(gain, loss) * 0.68f)));
                g.fillRect(rect);
            }
            else
            {
                const float overlap = juce::jlimit(0.0f, 1.0f, (cell.overlapDb + 72.0f) / 72.0f);
                const float risk = juce::jlimit(0.0f, 1.0f, cell.maskingRiskIndex);
                if (overlap <= 0.03f && risk <= 0.03f)
                    continue;
                auto colour = maskingSourceColour(cell.dominantSource, dark)
                                .interpolatedWith(juce::Colour(0xFFC77DFF), overlap * 0.42f)
                                .interpolatedWith(juce::Colour(0xFFFF335F), risk * 0.72f);
                g.setColour(colour.withAlpha(juce::jlimit(0.10f, 0.88f, overlap * 0.30f + risk * 0.62f)));
                g.fillRect(rect);
            }
        }

        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        for (float hz : { 100.0f, 1000.0f, 10000.0f, 20000.0f })
        {
            const float y = yForMaskingFrequency(hz, plot);
            g.drawText(formatFrequencyTick(hz), plot.getRight() + 8.0f, y - 12.0f, 56.0f, 24.0f, juce::Justification::centredLeft);
        }
        g.drawText("0s", plot.getX() - 10.0f, plot.getBottom() + 4.0f, 60.0f, 24.0f, juce::Justification::centredLeft);
        g.drawText(juce::String(maxTime, 2) + "s", plot.getRight() - 70.0f, plot.getBottom() + 4.0f, 70.0f, 24.0f, juce::Justification::centredRight);
    }

    static void drawMaskingFusionFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                        const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(56.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(34.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8("声层贴合 / 融合 / Layer Fit & Fusion"),
                   titleArea.removeFromTop(36.0f), juce::Justification::centredLeft, true);
        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(19.0f));
        g.drawText("Selected stems are compared against an auto/manual bounce. Values are critical-band overlap and fusion proxies, not measured hearing thresholds.",
                   titleArea, juce::Justification::centredLeft, true);

        auto metricsArea = inner.removeFromBottom(188.0f);
        inner.removeFromBottom(12.0f);
        std::array<const Asset*, 3> sources = data.fitSources;
        std::array<juce::String, 3> labels = data.fitLabels;
        bool hasFitSource = false;
        for (auto* source : sources)
            hasFitSource = hasFitSource || source != nullptr;
        if (!hasFitSource)
        {
            sources = { data.dry, data.wetA, data.wetB };
            labels = { data.label1, data.label2, data.label3 };
        }

        auto settings = data.maskingFusionSettings;
        if (data.fitFigureType.isNotEmpty())
            settings.figureType = data.fitFigureType;

        auto analysis = computeLayerFitFusionAnalysis(sources,
                                                      data.fitBounceAuto ? nullptr : data.fitBounceSource,
                                                      settings);
        if (!analysis.summary.valid)
        {
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(24.0f, juce::Font::bold));
            g.drawText("Layer Fit / Fusion needs at least one selected stem.", inner, juce::Justification::centred);
            return;
        }

        const auto tracks = makeLayerFitTracks(sources, labels, dark);
        const auto figureType = settings.figureType.trim().toLowerCase();
        const bool dodecahedronCrystal = figureType.contains("dodecahedron");
        const bool criticalBandCrystal = !dodecahedronCrystal && figureType.contains("crystal");
        const bool spatialImage = figureType.contains("spatial");
        const bool criticalBands = !criticalBandCrystal && !dodecahedronCrystal && !spatialImage
                                && (figureType.contains("critical") || figureType.contains("band"));
        const bool timeFrequencyTerrain = !spatialImage && !criticalBands;
        if (dodecahedronCrystal)
        {
            drawDodecahedronCrystalFigure(g, inner, analysis, tracks, data, dark);
            drawLayerFitFusionMetrics(g, metricsArea, analysis, data, tracks, dark);
            return;
        }
        if (criticalBandCrystal)
        {
            drawCriticalBandCrystalFigure(g, inner, analysis, tracks, data, dark);
            drawLayerFitFusionMetrics(g, metricsArea, analysis, data, tracks, dark);
            return;
        }

        constexpr float gap = 26.0f;
        auto left = inner.removeFromLeft((inner.getWidth() - gap) * 0.5f);
        inner.removeFromLeft(gap);
        if (spatialImage)
        {
            drawLayerFitSpatialImagePanels(g, left, inner, data, sources, labels, dark);
        }
        else if (timeFrequencyTerrain)
        {
            std::vector<SpatialHeatmapTrack> stemTerrainTracks;
            for (const auto& track : tracks)
                if (track.asset != nullptr && track.asset->spatialHeatmap.metrics.valid && track.asset->spatialHeatmap.image.isValid())
                    stemTerrainTracks.push_back({ track.asset, track.label, track.colour });

            std::unique_ptr<Asset> autoBounce;
            const Asset* bounce = data.fitBounceSource;
            juce::String bounceLabel = data.fitBounceLabel;
            if (data.fitBounceAuto)
            {
                autoBounce = makeLayerFitAutoBounceAsset(tracks);
                bounce = autoBounce.get();
                bounceLabel = "Auto Bounce";
            }

            std::vector<SpatialHeatmapTrack> bounceTerrainTracks;
            if (bounce != nullptr && bounce->spatialHeatmap.metrics.valid && bounce->spatialHeatmap.image.isValid())
                bounceTerrainTracks.push_back({ bounce, bounceLabel, layerFitBounceColour(dark) });

            auto scaleTracks = stemTerrainTracks;
            scaleTracks.insert(scaleTracks.end(), bounceTerrainTracks.begin(), bounceTerrainTracks.end());
            const auto sharedScale = makeSpatialDbScale(scaleTracks, data);

            float maxDurationSeconds = 0.001f;
            for (const auto& track : scaleTracks)
                maxDurationSeconds = juce::jmax(maxDurationSeconds,
                    static_cast<float>(track.asset->spatialHeatmap.metrics.durationSeconds));

            const auto rightTitle = data.fitBounceAuto ? "Auto Bounce / 2.5D Fitted Result"
                                                       : ("Bounce / " + data.fitBounceLabel);
            drawLayerFitTimeFrequencyStack(g, left, stemTerrainTracks, data,
                                           "Selected Stems / 2.5D Time-Frequency-Energy", dark,
                                           sharedScale, maxDurationSeconds);
            drawLayerFitTimeFrequencyStack(g, inner, bounceTerrainTracks, data, rightTitle, dark,
                                           sharedScale, maxDurationSeconds);
        }
        else
        {
            const auto leftTitle = criticalBands ? "Selected Stems / Critical Band Terrain"
                                                 : "Selected Stems / Time-Frequency Terrain";
            const auto rightTitle = data.fitBounceAuto ? "Auto Bounce / Fitted Result"
                                                       : ("Bounce / " + data.fitBounceLabel);
            drawLayerFitTerrainPanel(g, left, analysis, tracks, false, leftTitle, dark,
                                     criticalBands, data.terrainCamera, data.terrainTimeReversed, data.bandHighlight);
            drawLayerFitTerrainPanel(g, inner, analysis, tracks, true, rightTitle, dark,
                                     criticalBands, data.terrainCamera, data.terrainTimeReversed, data.bandHighlight);
        }

        drawLayerFitFusionMetrics(g, metricsArea, analysis, data, tracks, dark);
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
        auto metricsArea = inner.removeFromBottom(juce::jlimit(150.0f, 196.0f, getMetricsHeight(data, MetricsKind::groupDelay)));
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

        auto edcArea = inner.removeFromTop(inner.getHeight() * 0.40f);
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
        auto metricsArea = inner.removeFromBottom(juce::jlimit(112.0f, 150.0f, getMetricsHeight(data, MetricsKind::basic)));
        inner.removeFromBottom(8.0f);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(34.0f, juce::Font::bold));
        g.drawText("Spectrogram A/B/C", titleArea, juce::Justification::centredLeft);

        drawSpectrogramTracksOnly(g, inner, data, dark, false);

        drawFigureMetrics(g, metricsArea, data, dark, MetricsKind::basic);
    }

    struct SpatialHeatmapTrack
    {
        const Asset* asset = nullptr;
        juce::String label;
        juce::Colour colour;
    };

    struct SpatialDbScale
    {
        bool enabled = false;
        float floorDb = -120.0f;
        float ceilingDb = 0.0f;
    };

    static std::vector<SpatialHeatmapTrack> makeSpatialHeatmapTracks(const FigureData& data, bool dark)
    {
        std::vector<SpatialHeatmapTrack> tracks;
        auto push = [&](const Asset* asset, const juce::String& label, juce::Colour colour)
        {
            if (asset != nullptr && asset->spatialHeatmap.metrics.valid && asset->spatialHeatmap.image.isValid()
                && static_cast<int>(tracks.size()) < 2)
                tracks.push_back({ asset, label, colour });
        };

        push(data.dry, data.label1, dryColour(dark));
        push(data.wetA, data.label2, wetAColour(dark));
        push(data.wetB, data.label3, wetBColour(dark));
        return tracks;
    }

    static SpatialDbScale makeSpatialDbScale(const std::vector<SpatialHeatmapTrack>& tracks,
                                             const FigureData& data)
    {
        SpatialDbScale scale;
        if (!data.sharedScale || tracks.size() < 2)
            return scale;

        scale.floorDb = std::numeric_limits<float>::max();
        scale.ceilingDb = -std::numeric_limits<float>::max();
        for (const auto& track : tracks)
        {
            const auto& m = track.asset->spatialHeatmap.metrics;
            if (!m.valid)
                continue;
            scale.floorDb = juce::jmin(scale.floorDb, m.floorDb);
            scale.ceilingDb = juce::jmax(scale.ceilingDb, m.ceilingDb);
        }

        if (scale.floorDb < scale.ceilingDb - 1.0f)
            scale.enabled = true;
        else
            scale = {};

        return scale;
    }

    static void drawSpatialHeatmapFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                         const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(42.0f);
        if (data.bandHighlight.enabled)
            drawBandHighlightLegend(g, titleArea.removeFromRight(520.0f), data.bandHighlight, dark);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("Spatial Energy Terrain", titleArea, juce::Justification::centredLeft);

        if (data.processingNote.isNotEmpty())
        {
            auto noteArea = inner.removeFromTop(30.0f);
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(18.0f));
            g.drawText(data.processingNote, noteArea, juce::Justification::centredLeft, true);
        }

        auto metricsArea = inner.removeFromBottom(174.0f);
        inner.removeFromBottom(12.0f);

        const auto tracks = makeSpatialHeatmapTracks(data, dark);
        if (tracks.empty())
        {
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(22.0f));
            g.drawText("No spatial terrain data.", inner, juce::Justification::centred);
            return;
        }

        float maxDurationSeconds = 0.001f;
        for (const auto& track : tracks)
            maxDurationSeconds = juce::jmax(maxDurationSeconds, static_cast<float>(track.asset->spatialHeatmap.metrics.durationSeconds));
        const auto sharedScale = makeSpatialDbScale(tracks, data);

        if (tracks.size() == 1)
        {
            drawSpatialTerrainPanel(g, inner, tracks.front(), maxDurationSeconds, dark,
                                    data.terrainCamera, data.terrainTimeReversed, sharedScale, data.bandHighlight);
        }
        else
        {
            constexpr float gap = 28.0f;
            auto left = inner.removeFromLeft((inner.getWidth() - gap) * 0.5f);
            inner.removeFromLeft(gap);
            drawSpatialTerrainPanel(g, left, tracks[0], maxDurationSeconds, dark,
                                    data.terrainCamera, data.terrainTimeReversed, sharedScale, data.bandHighlight);
            drawSpatialTerrainPanel(g, inner, tracks[1], maxDurationSeconds, dark,
                                    data.terrainCamera, data.terrainTimeReversed, sharedScale, data.bandHighlight);
        }

        drawSpatialHeatmapMetrics(g, metricsArea, tracks, dark);
    }

    struct SpatialWindowRange
    {
        float startSeconds = 0.0f;
        float endSeconds = 0.0f;
        juce::String label;
    };

    static juce::String spatialWindowToken(SpatialWindow window)
    {
        switch (window)
        {
            case SpatialWindow::attack: return "attack";
            case SpatialWindow::body:   return "body";
            case SpatialWindow::tail:   return "tail";
            case SpatialWindow::full:
            default:                    return "full";
        }
    }

    static SpatialWindowRange getSpatialWindowRange(const SpatialHeatmapMetrics& metrics, SpatialWindow window)
    {
        const float duration = static_cast<float>(juce::jmax(0.001, metrics.durationSeconds));
        const float tailStart = juce::jlimit(0.0f, duration, static_cast<float>(metrics.tailStartSeconds));
        const float bodyEnd = juce::jlimit(tailStart, duration, tailStart + (duration - tailStart) * 0.42f);

        switch (window)
        {
            case SpatialWindow::attack:
                return { 0.0f, juce::jlimit(0.001f, duration, tailStart), "Attack 0-" + juce::String(tailStart, 2) + "s" };
            case SpatialWindow::body:
                return { tailStart, bodyEnd, "Body " + juce::String(tailStart, 2) + "-" + juce::String(bodyEnd, 2) + "s" };
            case SpatialWindow::tail:
                return { tailStart, duration, "Tail " + juce::String(tailStart, 2) + "-" + juce::String(duration, 2) + "s" };
            case SpatialWindow::full:
            default:
                return { 0.0f, duration, "Full 0-" + juce::String(duration, 2) + "s" };
        }
    }

    static SpatialWindowRange getSpatialTimeSliceRange(const SpatialHeatmapMetrics& metrics, float timeSeconds)
    {
        const float duration = static_cast<float>(juce::jmax(0.001, metrics.durationSeconds));
        if (timeSeconds < 0.0f)
            return getSpatialWindowRange(metrics, SpatialWindow::full);

        const float centre = juce::jlimit(0.0f, duration, timeSeconds);
        const float width = juce::jlimit(0.080f, 0.420f, duration * 0.085f);
        const float start = juce::jlimit(0.0f, duration, centre - width * 0.5f);
        const float end = juce::jlimit(start + 0.001f, duration, centre + width * 0.5f);
        return { start, end, "Time " + juce::String(centre, 2) + "s | window "
                           + juce::String(start, 2) + "-" + juce::String(end, 2) + "s" };
    }

    static SpatialWindowRange getExplicitSpatialWindowRange(const SpatialHeatmapMetrics& metrics,
                                                            float startSeconds,
                                                            float endSeconds,
                                                            juce::String label = {})
    {
        const float duration = static_cast<float>(juce::jmax(0.001, metrics.durationSeconds));
        const float start = juce::jlimit(0.0f, duration, startSeconds);
        const float end = juce::jlimit(start + 0.001f, duration, endSeconds);
        if (label.isEmpty())
            label = "Window " + juce::String(start, 2) + "-" + juce::String(end, 2) + "s";
        return { start, end, label };
    }

    static float spatialImpressionFrequencyNorm(float frequencyHz, float maxHz)
    {
        const float minHz = 100.0f;
        const float hi = juce::jmax(minHz * 1.25f, maxHz);
        const float safe = juce::jlimit(minHz, hi, frequencyHz);
        return juce::jlimit(0.0f, 1.0f, std::log(safe / minHz) / std::log(hi / minHz));
    }

    static float frequencyFromLogNorm(float norm, float maxHz)
    {
        const float minHz = 100.0f;
        const float hi = juce::jmax(minHz * 1.25f, maxHz);
        return minHz * std::pow(hi / minHz, juce::jlimit(0.0f, 1.0f, norm));
    }

    static const BandHighlightBand* bandForFrequency(const BandHighlightConfig& config, float frequencyHz)
    {
        if (!config.enabled)
            return nullptr;

        for (const auto& band : config.bands)
            if (frequencyHz >= band.minHz && frequencyHz < band.maxHz)
                return &band;

        return nullptr;
    }

    static bool hasActiveBand(const BandHighlightConfig& config)
    {
        if (!config.enabled)
            return false;

        for (const auto& band : config.bands)
            if (band.active)
                return true;

        return false;
    }

    static float bandHighlightEnergyScale(const BandHighlightConfig& config, float frequencyHz)
    {
        if (!config.enabled || !hasActiveBand(config))
            return 1.0f;

        const auto* band = bandForFrequency(config, frequencyHz);
        if (band != nullptr && band->active)
            return 1.0f;

        return config.dimInactiveBands ? juce::jlimit(0.04f, 1.0f, config.inactiveAlpha) : 1.0f;
    }

    static juce::Colour applyBandHighlightTint(juce::Colour base, const BandHighlightConfig& config,
                                               float frequencyHz, bool dark, float energy = 1.0f)
    {
        if (!config.enabled || !hasActiveBand(config))
            return base;

        const auto* band = bandForFrequency(config, frequencyHz);
        if (band != nullptr && band->active)
        {
            const float amount = juce::jlimit(0.0f, 0.86f, config.overlayAlpha * (0.45f + energy * 0.55f));
            return base.interpolatedWith(band->colour, amount);
        }

        if (!config.dimInactiveBands)
            return base;

        const float alpha = juce::jlimit(0.08f, 1.0f, config.inactiveAlpha);
        const auto quiet = dark ? juce::Colour(0xFF101820) : juce::Colour(0xFFE7ECF2);
        return base.interpolatedWith(quiet, 1.0f - alpha).withMultipliedAlpha(juce::jlimit(0.18f, 1.0f, alpha * 1.35f));
    }

    static bool layerFitBandSoloActive(const BandHighlightConfig& config)
    {
        return config.enabled && hasActiveBand(config);
    }

    static bool layerFitFrequencyInActiveBand(const BandHighlightConfig& config, float frequencyHz)
    {
        if (!layerFitBandSoloActive(config))
            return true;

        const auto* band = bandForFrequency(config, frequencyHz);
        return band != nullptr && band->active;
    }

    static juce::Colour layerFitBandSoloGhostColour(bool dark, float energy, float depth = 0.0f)
    {
        const auto base = dark ? juce::Colour(0xFF080D13) : juce::Colour(0xFFE8EDF3);
        const auto lift = dark ? juce::Colour(0xFF6F7B87) : juce::Colour(0xFF6B7280);
        const auto shade = base.interpolatedWith(lift, juce::jlimit(0.0f, 0.34f, 0.08f + energy * 0.22f));
        return shade.interpolatedWith(juce::Colours::black, dark ? depth * 0.10f : depth * 0.03f);
    }

    static float spatialImpressionPanFromBalance(float lrBalanceDb)
    {
        constexpr float hardPanDb = 30.0f;
        return juce::jlimit(0.0f, 1.0f, 0.5f + lrBalanceDb / (hardPanDb * 2.0f));
    }

    static float spatialImpressionPanAuthority(float lrBalanceDb, float correlation)
    {
        constexpr float hardPanDb = 30.0f;
        const float balance = juce::jlimit(0.0f, 1.0f, std::abs(lrBalanceDb) / hardPanDb);
        const float antiPhase = juce::jlimit(0.0f, 1.0f, -correlation);
        return juce::jlimit(0.12f, 0.92f, 0.18f + std::pow(balance, 0.72f) * 0.72f - antiPhase * 0.12f);
    }

    static float spatialImpressionSpread(float widthIndex, float correlation, float lrBalanceDb)
    {
        constexpr float hardPanDb = 30.0f;
        const float width = juce::jlimit(0.0f, 1.0f, widthIndex);
        const float decorrelation = juce::jlimit(0.0f, 1.0f, (1.0f - correlation) * 0.5f);
        const float balance = juce::jlimit(0.0f, 1.0f, std::abs(lrBalanceDb) / hardPanDb);
        return juce::jlimit(0.018f, 0.235f,
            0.022f + std::pow(width, 1.32f) * 0.150f + decorrelation * 0.050f - balance * 0.022f);
    }

    static std::vector<float> buildSpatialImpressionGrid(const SpatialHeatmapAnalysis& analysis,
                                                         SpatialWindow window,
                                                         float timePositionSeconds,
                                                         float windowStartSeconds,
                                                         float windowEndSeconds,
                                                         SpatialDbScale sharedScale,
                                                         const BandHighlightConfig& bandHighlight,
                                                         int xSteps, int fSteps,
                                                         SpatialWindowRange& rangeOut,
                                                         bool layerFitBandSoloGhost = false)
    {
        std::vector<float> grid(static_cast<size_t>(xSteps * fSteps), 0.0f);
        if (!analysis.metrics.valid || analysis.sampledCells.empty())
            return grid;

        rangeOut = (windowStartSeconds >= 0.0f && windowEndSeconds > windowStartSeconds)
            ? getExplicitSpatialWindowRange(analysis.metrics, windowStartSeconds, windowEndSeconds)
            : (timePositionSeconds >= 0.0f ? getSpatialTimeSliceRange(analysis.metrics, timePositionSeconds)
                                           : getSpatialWindowRange(analysis.metrics, window));
        const float minDb = sharedScale.enabled ? sharedScale.floorDb : analysis.metrics.floorDb;
        const float maxDb = sharedScale.enabled ? sharedScale.ceilingDb : analysis.metrics.ceilingDb;
        const float dbSpan = juce::jmax(1.0f, maxDb - minDb);
        const float maxHz = analysis.metrics.maxFrequencyHz > 0.0f ? analysis.metrics.maxFrequencyHz : 20000.0f;

        auto at = [xSteps] (int x, int f) -> size_t
        {
            return static_cast<size_t>(f * xSteps + x);
        };

        for (const auto& cell : analysis.sampledCells)
        {
            const float overlap = juce::jmax(0.0f, juce::jmin(cell.timeEndSeconds, rangeOut.endSeconds)
                                                   - juce::jmax(cell.timeStartSeconds, rangeOut.startSeconds));
            const float cellDuration = juce::jmax(0.001f, cell.timeEndSeconds - cell.timeStartSeconds);
            if (overlap <= 0.0f)
                continue;

            const float frequency = juce::jmax(20.0f, (cell.frequencyLowHz + cell.frequencyHighHz) * 0.5f);
            const int fIndex = juce::jlimit(0, fSteps - 1,
                juce::roundToInt(spatialImpressionFrequencyNorm(frequency, maxHz) * static_cast<float>(fSteps - 1)));

            const float rawPan = spatialImpressionPanFromBalance(cell.lrBalanceDb);
            const float panAuthority = spatialImpressionPanAuthority(cell.lrBalanceDb, cell.correlation);
            const float pan = 0.5f + (rawPan - 0.5f) * panAuthority;
            const float spread = spatialImpressionSpread(cell.widthIndex, cell.correlation, cell.lrBalanceDb);
            const float energyNorm = std::pow(juce::jlimit(0.0f, 1.0f, (cell.energyDb - minDb) / dbSpan), 1.18f)
                                   * juce::jlimit(0.0f, 1.0f, overlap / cellDuration)
                                   * (layerFitBandSoloGhost ? 1.0f : bandHighlightEnergyScale(bandHighlight, frequency));
            if (energyNorm <= 0.002f)
                continue;

            for (int x = 0; x < xSteps; ++x)
            {
                const float xNorm = static_cast<float>(x) / static_cast<float>(juce::jmax(1, xSteps - 1));
                const float d = (xNorm - pan) / spread;
                const float weight = std::exp(-0.5f * d * d);
                grid[at(x, fIndex)] += energyNorm * weight;
            }
        }

        float maxValue = 0.0f;
        for (float value : grid)
            maxValue = juce::jmax(maxValue, value);
        if (maxValue > 0.0f)
        {
            constexpr float visibilityFloor = 0.026f;
            for (auto& value : grid)
            {
                const float normalised = juce::jlimit(0.0f, 1.0f, value / maxValue);
                value = normalised < visibilityFloor ? 0.0f : std::pow(normalised, 0.86f);
            }
        }

        return grid;
    }

    static void drawSpatialImpressionPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                           const SpatialHeatmapTrack& track, bool dark,
                                           SpatialWindow window, float timePositionSeconds,
                                           float windowStartSeconds, float windowEndSeconds,
                                           TerrainCamera camera, SpatialDbScale sharedScale,
                                           const BandHighlightConfig& bandHighlight)
    {
        auto labelArea = area.removeFromTop(44.0f);
        g.setColour(track.colour);
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText(track.label, labelArea, juce::Justification::centredLeft, true);

        auto plot = area.withTrimmedBottom(42.0f);
        const auto bg = dark ? (previewBoost() ? juce::Colour(0xFF0D141C) : juce::Colour(0xFF0C1118))
                             : juce::Colour(0xFFF5F7FA);
        g.setColour(bg);
        g.fillRoundedRectangle(plot, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.30f : 0.18f)
                         : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(plot, 8.0f, 1.0f);

        auto inner = plot.reduced(48.0f, 24.0f);
        inner.removeFromTop(4.0f);
        inner.removeFromBottom(18.0f);
        const int xSteps = 80;
        const int fSteps = 128;
        SpatialWindowRange range;
        auto grid = buildSpatialImpressionGrid(track.asset->spatialHeatmap, window, timePositionSeconds,
                                               windowStartSeconds, windowEndSeconds, sharedScale,
                                               bandHighlight,
                                               xSteps, fSteps, range);
        auto at = [xSteps] (int x, int f) -> size_t { return static_cast<size_t>(f * xSteps + x); };

        const auto gridColour = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.24f : 0.16f)
                                     : juce::Colour(0xFF64748B).withAlpha(0.20f);
        const auto text = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.96f : 0.82f)
                               : juce::Colour(0xFF334155).withAlpha(0.90f);

        const bool sideCamera = isSideTerrainCamera(camera);
        const bool highCamera = camera == TerrainCamera::frontHigh || camera == TerrainCamera::sideHigh
                             || camera == TerrainCamera::diagonal;
        const float baseWidth = inner.getWidth() * (sideCamera ? 0.82f : 0.72f);
        const float depthX = inner.getWidth() * (sideCamera ? 0.105f : 0.155f);
        const float depthY = inner.getHeight() * (highCamera ? 0.54f : 0.48f);
        const float heightScale = inner.getHeight() * (highCamera ? 0.34f : 0.31f);
        const float originX = inner.getX() + inner.getWidth() * (sideCamera ? 0.040f : 0.065f);
        const float originY = inner.getBottom() - inner.getHeight() * 0.035f;
        auto project = [&] (float xNorm, float fNorm, float value) -> juce::Point<float>
        {
            const float primary = sideCamera ? fNorm : xNorm;
            const float depth = sideCamera ? xNorm : fNorm;
            const float perspective = 1.0f - depth * (highCamera ? 0.18f : 0.12f);
            const float x = originX + primary * baseWidth * perspective + depth * depthX
                          + (1.0f - perspective) * baseWidth * 0.5f;
            const float y = originY - depth * depthY - juce::jlimit(0.0f, 1.0f, value) * heightScale;
            return { x, y };
        };

        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(plot.toNearestInt());

        juce::Path floor;
        auto p00 = project(0.0f, 0.0f, 0.0f);
        auto p10 = project(1.0f, 0.0f, 0.0f);
        auto p11 = project(1.0f, 1.0f, 0.0f);
        auto p01 = project(0.0f, 1.0f, 0.0f);
        floor.startNewSubPath(p00.x, p00.y);
        floor.lineTo(p10.x, p10.y);
        floor.lineTo(p11.x, p11.y);
        floor.lineTo(p01.x, p01.y);
        floor.closeSubPath();
        g.setColour(dark ? juce::Colour(0xFF111820).withAlpha(previewBoost() ? 0.78f : 0.66f)
                         : juce::Colour(0xFFE8EDF3).withAlpha(0.88f));
        g.fillPath(floor);
        g.setColour(gridColour);
        g.strokePath(floor, juce::PathStrokeType(1.0f));

        for (int i = 0; i <= 8; ++i)
        {
            const float fNorm = static_cast<float>(i) / 8.0f;
            auto a = project(0.0f, fNorm, 0.0f);
            auto b = project(1.0f, fNorm, 0.0f);
            g.drawLine(a.x, a.y, b.x, b.y, i == 0 ? 1.2f : 0.7f);
        }
        for (float xNorm : { 0.0f, 0.5f, 1.0f })
        {
            auto a = project(xNorm, 0.0f, 0.0f);
            auto b = project(xNorm, 1.0f, 0.0f);
            g.drawLine(a.x, a.y, b.x, b.y, 0.8f);
        }

        for (int f = fSteps - 2; f >= 0; --f)
        {
            const float fNorm0 = static_cast<float>(f) / static_cast<float>(fSteps - 1);
            const float fNorm1 = static_cast<float>(f + 1) / static_cast<float>(fSteps - 1);
            for (int x = 0; x < xSteps - 1; ++x)
            {
                const float xNorm0 = static_cast<float>(x) / static_cast<float>(xSteps - 1);
                const float xNorm1 = static_cast<float>(x + 1) / static_cast<float>(xSteps - 1);
                const float v00 = grid[at(x, f)];
                const float v10 = grid[at(x + 1, f)];
                const float v11 = grid[at(x + 1, f + 1)];
                const float v01 = grid[at(x, f + 1)];
                const float avg = (v00 + v10 + v11 + v01) * 0.25f;
                if (avg <= 0.006f)
                    continue;

                juce::Path quad;
                auto q00 = project(xNorm0, fNorm0, v00);
                auto q10 = project(xNorm1, fNorm0, v10);
                auto q11 = project(xNorm1, fNorm1, v11);
                auto q01 = project(xNorm0, fNorm1, v01);
                quad.startNewSubPath(q00.x, q00.y);
                quad.lineTo(q10.x, q10.y);
                quad.lineTo(q11.x, q11.y);
                quad.lineTo(q01.x, q01.y);
                quad.closeSubPath();

                const float xMid = (xNorm0 + xNorm1) * 0.5f;
                const float maxHz = track.asset->spatialHeatmap.metrics.maxFrequencyHz > 0.0f
                    ? track.asset->spatialHeatmap.metrics.maxFrequencyHz : 20000.0f;
                const float frequencyHz = frequencyFromLogNorm((fNorm0 + fNorm1) * 0.5f, maxHz);
                auto colour = spatialHeatmapColour(xMid, juce::jlimit(0.0f, 0.94f, 0.28f + avg * 0.62f));
                colour = applyBandHighlightTint(colour, bandHighlight, frequencyHz, dark, avg);
                colour = colour.interpolatedWith(juce::Colours::white, dark ? avg * 0.18f : avg * 0.08f)
                               .interpolatedWith(juce::Colours::black, dark ? fNorm0 * 0.20f : fNorm0 * 0.05f);
                g.setColour(colour);
                g.fillPath(quad);
            }
        }

        const auto frontWallShade = dark ? juce::Colours::black.withAlpha(0.18f)
                                         : juce::Colour(0xFF94A3B8).withAlpha(0.16f);
        for (int x = 0; x < xSteps - 1; ++x)
        {
            const float xNorm0 = static_cast<float>(x) / static_cast<float>(xSteps - 1);
            const float xNorm1 = static_cast<float>(x + 1) / static_cast<float>(xSteps - 1);
            const float v0 = grid[at(x, 0)];
            const float v1 = grid[at(x + 1, 0)];
            if ((v0 + v1) <= 0.012f)
                continue;
            juce::Path wall;
            auto b0 = project(xNorm0, 0.0f, 0.0f);
            auto b1 = project(xNorm1, 0.0f, 0.0f);
            auto t1 = project(xNorm1, 0.0f, v1);
            auto t0 = project(xNorm0, 0.0f, v0);
            wall.startNewSubPath(b0.x, b0.y);
            wall.lineTo(b1.x, b1.y);
            wall.lineTo(t1.x, t1.y);
            wall.lineTo(t0.x, t0.y);
            wall.closeSubPath();
            g.setColour(spatialHeatmapColour((xNorm0 + xNorm1) * 0.5f, 0.42f).interpolatedWith(frontWallShade, 0.32f));
            g.fillPath(wall);
        }

        const auto ridge = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.58f : 0.42f)
                                : juce::Colour(0xFF111827).withAlpha(0.34f);
        const int frequencyRidgeStep = juce::jmax(2, fSteps / 28);
        for (int f = 0; f < fSteps; f += frequencyRidgeStep)
        {
            juce::Path path;
            bool started = false;
            float peak = 0.0f;
            const float fNorm = static_cast<float>(f) / static_cast<float>(fSteps - 1);
            for (int x = 0; x < xSteps; ++x)
            {
                const float xNorm = static_cast<float>(x) / static_cast<float>(xSteps - 1);
                const float value = grid[at(x, f)];
                peak = juce::jmax(peak, value);
                auto p = project(xNorm, fNorm, value);
                if (!started)
                {
                    path.startNewSubPath(p.x, p.y);
                    started = true;
                }
                else
                {
                    path.lineTo(p.x, p.y);
                }
            }
            if (peak > 0.018f)
            {
                g.setColour(ridge.withAlpha(ridge.getFloatAlpha() * (0.32f + peak * 0.82f)));
                g.strokePath(path, juce::PathStrokeType(0.55f + peak * 1.05f));
            }
        }

        const int lcrRidgeStep = juce::jmax(5, xSteps / 11);
        for (int x = 0; x < xSteps; x += lcrRidgeStep)
        {
            juce::Path path;
            bool started = false;
            float peak = 0.0f;
            const float xNorm = static_cast<float>(x) / static_cast<float>(xSteps - 1);
            for (int f = 0; f < fSteps; ++f)
            {
                const float fNorm = static_cast<float>(f) / static_cast<float>(fSteps - 1);
                const float value = grid[at(x, f)];
                peak = juce::jmax(peak, value);
                auto p = project(xNorm, fNorm, value);
                if (!started)
                {
                    path.startNewSubPath(p.x, p.y);
                    started = true;
                }
                else
                {
                    path.lineTo(p.x, p.y);
                }
            }
            if (peak > 0.018f)
            {
                g.setColour(ridge.withAlpha(ridge.getFloatAlpha() * 0.52f));
                g.strokePath(path, juce::PathStrokeType(0.52f));
            }
        }

        g.setColour(text);
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        auto l = project(0.0f, 0.0f, 0.0f);
        auto c = project(0.5f, 0.0f, 0.0f);
        auto r = project(1.0f, 0.0f, 0.0f);
        g.drawText("L", l.x - 18.0f, l.y + (sideCamera ? -36.0f : 10.0f), 40.0f, 32.0f, juce::Justification::centred);
        g.drawText("C", c.x - 20.0f, c.y + (sideCamera ? -36.0f : 10.0f), 42.0f, 32.0f, juce::Justification::centred);
        g.drawText("R", r.x - 18.0f, r.y + (sideCamera ? -36.0f : 10.0f), 40.0f, 32.0f, juce::Justification::centred);

        const auto& metrics = track.asset->spatialHeatmap.metrics;
        const float maxHz = metrics.maxFrequencyHz > 0.0f ? metrics.maxFrequencyHz : 20000.0f;
        const float ticks[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        for (float hz : ticks)
        {
            if (hz > maxHz)
                continue;
            const float fNorm = spatialImpressionFrequencyNorm(hz, maxHz);
            auto p = project(sideCamera ? 0.0f : 1.0f, fNorm, 0.0f);
            g.drawText(formatFrequencyTick(hz), p.x + 10.0f, p.y + (sideCamera ? 12.0f : -18.0f), 96.0f, 36.0f,
                       juce::Justification::centredLeft);
        }

        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText(range.label, plot.reduced(22.0f, 16.0f).removeFromTop(46.0f), juce::Justification::centredRight, true);
    }

    static void drawSpatialImpressionFigure(juce::Graphics& g, juce::Rectangle<float> area,
                                            const FigureData& data, bool dark)
    {
        g.setColour(plateColour(dark));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB)
                         : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0x22000000)));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(44.0f, 34.0f);
        auto titleArea = inner.removeFromTop(68.0f);
        auto legendArea = titleArea.removeFromRight(600.0f);
        if (data.bandHighlight.enabled)
        {
            drawBandHighlightLegend(g, legendArea.removeFromTop(32.0f), data.bandHighlight, dark);
            legendArea.removeFromTop(4.0f);
        }
        drawSpatialLcrLegend(g, legendArea, dark);

        g.setColour(primaryText(dark));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("Spatial Impression L-C-R", titleArea, juce::Justification::centredLeft);

        if (data.processingNote.isNotEmpty())
        {
            auto noteArea = inner.removeFromTop(30.0f);
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(18.0f));
            g.drawText(data.processingNote, noteArea, juce::Justification::centredLeft, true);
        }

        auto metricsArea = inner.removeFromBottom(174.0f);
        inner.removeFromBottom(12.0f);

        const auto tracks = makeSpatialHeatmapTracks(data, dark);
        if (tracks.empty())
        {
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(22.0f));
            g.drawText("No spatial impression data.", inner, juce::Justification::centred);
            return;
        }

        const auto sharedScale = makeSpatialDbScale(tracks, data);
        if (tracks.size() == 1)
        {
            drawSpatialImpressionPanel(g, inner, tracks.front(), dark, data.spatialWindow,
                                       data.spatialTimePositionSeconds,
                                       data.spatialWindowStartSeconds, data.spatialWindowEndSeconds,
                                       data.terrainCamera, sharedScale, data.bandHighlight);
        }
        else
        {
            constexpr float gap = 28.0f;
            auto left = inner.removeFromLeft((inner.getWidth() - gap) * 0.5f);
            inner.removeFromLeft(gap);
            drawSpatialImpressionPanel(g, left, tracks[0], dark, data.spatialWindow,
                                       data.spatialTimePositionSeconds,
                                       data.spatialWindowStartSeconds, data.spatialWindowEndSeconds,
                                       data.terrainCamera, sharedScale, data.bandHighlight);
            drawSpatialImpressionPanel(g, inner, tracks[1], dark, data.spatialWindow,
                                       data.spatialTimePositionSeconds,
                                       data.spatialWindowStartSeconds, data.spatialWindowEndSeconds,
                                       data.terrainCamera, sharedScale, data.bandHighlight);
        }

        drawSpatialHeatmapMetrics(g, metricsArea, tracks, dark);
    }

    static std::unique_ptr<Asset> makeLayerFitAutoBounceAsset(const std::vector<LayerFitTrack>& tracks)
    {
        if (tracks.empty())
            return {};

        double sampleRate = 0.0;
        int samples = 0;
        int channels = 0;
        for (const auto& track : tracks)
        {
            if (track.asset == nullptr || track.asset->buffer.getNumSamples() <= 0)
                continue;
            if (sampleRate <= 0.0)
                sampleRate = track.asset->sampleRate;
            samples = juce::jmax(samples, track.asset->buffer.getNumSamples());
            channels = juce::jmax(channels, track.asset->buffer.getNumChannels());
        }

        if (sampleRate <= 0.0 || samples <= 0)
            return {};

        auto asset = std::make_unique<Asset>();
        asset->name = "Auto Bounce Selected Stems";
        asset->sourcePath = "auto_bounce_selected_stems";
        asset->sampleRate = sampleRate;
        asset->buffer.setSize(juce::jmax(2, channels), samples);
        asset->buffer.clear();

        for (int i = 0; i < samples; ++i)
        {
            for (int ch = 0; ch < asset->buffer.getNumChannels(); ++ch)
            {
                float sum = 0.0f;
                for (const auto& track : tracks)
                {
                    const auto* source = track.asset;
                    if (source == nullptr || source->buffer.getNumSamples() <= i || source->buffer.getNumChannels() <= 0)
                        continue;
                    const int srcCh = juce::jlimit(0, source->buffer.getNumChannels() - 1, ch);
                    sum += source->buffer.getSample(srcCh, i);
                }
                asset->buffer.setSample(ch, i, sum);
            }
        }

        asset->metrics = computeMetrics(asset->buffer, asset->sampleRate);
        asset->spatialHeatmap = computeSpatialHeatmapAnalysis(asset->buffer, asset->sampleRate);
        return asset;
    }

    static void drawLayerFitSpatialStack(juce::Graphics& g, juce::Rectangle<float> area,
                                         const std::vector<SpatialHeatmapTrack>& tracks,
                                         const FigureData& data, bool dark,
                                         SpatialDbScale sharedScale,
                                         bool layerFitBandSoloGhost = true)
    {
        if (tracks.empty())
        {
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText("No spatial data for the selected stem.", area, juce::Justification::centred, true);
            return;
        }

        const auto bg = dark ? (previewBoost() ? juce::Colour(0xFF0D141C)
                                               : juce::Colour(0xFF0C1118))
                             : juce::Colour(0xFFF5F7FA);
        g.setColour(dark ? juce::Colour(0xFF070C12) : juce::Colour(0xFFF8FAFC));
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.28f : 0.18f)
                         : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        auto title = area.removeFromTop(42.0f).reduced(16.0f, 0.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText(tracks.size() > 1 ? "Selected Stems / Same L-C-R Space" : tracks.front().label,
                   title, juce::Justification::centredLeft, true);

        auto plot = area.reduced(24.0f, 16.0f);
        juce::ColourGradient gradient(bg.brighter(dark ? 0.10f : 0.01f), plot.getTopLeft(),
                                      bg.darker(dark ? 0.10f : 0.03f), plot.getBottomRight(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(plot, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.26f : 0.16f)
                         : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(plot, 8.0f, 1.0f);

        auto inner = plot.reduced(48.0f, 28.0f);
        inner.removeFromBottom(30.0f);
        const int xSteps = 80;
        const int fSteps = 128;
        auto at = [xSteps] (int x, int f) -> size_t { return static_cast<size_t>(f * xSteps + x); };

        struct GridTrack
        {
            const SpatialHeatmapTrack* track = nullptr;
            std::vector<float> grid;
            SpatialWindowRange range;
        };

        std::vector<GridTrack> grids;
        grids.reserve(tracks.size());
        for (const auto& track : tracks)
        {
            if (track.asset == nullptr || !track.asset->spatialHeatmap.metrics.valid)
                continue;

            GridTrack item;
            item.track = &track;
            item.grid = buildSpatialImpressionGrid(track.asset->spatialHeatmap, data.spatialWindow,
                                                   data.spatialTimePositionSeconds,
                                                   data.spatialWindowStartSeconds,
                                                   data.spatialWindowEndSeconds,
                                                   sharedScale, data.bandHighlight,
                                                   xSteps, fSteps, item.range,
                                                   layerFitBandSoloGhost);
            grids.push_back(std::move(item));
        }

        if (grids.empty())
        {
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText("No spatial data for the selected stem.", plot, juce::Justification::centred, true);
            return;
        }

        const auto gridColour = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.23f : 0.15f)
                                     : juce::Colour(0xFF64748B).withAlpha(0.22f);
        const auto text = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.96f : 0.84f)
                               : juce::Colour(0xFF334155).withAlpha(0.92f);
        const bool sideCamera = isSideTerrainCamera(data.terrainCamera);
        const bool highCamera = data.terrainCamera == TerrainCamera::frontHigh
                             || data.terrainCamera == TerrainCamera::sideHigh
                             || data.terrainCamera == TerrainCamera::diagonal;
        const float baseWidth = inner.getWidth() * (sideCamera ? 0.82f : 0.72f);
        const float depthX = inner.getWidth() * (sideCamera ? 0.105f : 0.155f);
        const float depthY = inner.getHeight() * (highCamera ? 0.54f : 0.48f);
        const float heightScale = inner.getHeight() * (highCamera ? 0.34f : 0.31f);
        const float originX = inner.getX() + inner.getWidth() * (sideCamera ? 0.040f : 0.065f);
        const float originY = inner.getBottom() - inner.getHeight() * 0.035f;
        auto project = [&] (float xNorm, float fNorm, float value) -> juce::Point<float>
        {
            const float primary = sideCamera ? fNorm : xNorm;
            const float depth = sideCamera ? xNorm : fNorm;
            const float perspective = 1.0f - depth * (highCamera ? 0.18f : 0.12f);
            const float x = originX + primary * baseWidth * perspective + depth * depthX
                          + (1.0f - perspective) * baseWidth * 0.5f;
            const float y = originY - depth * depthY - juce::jlimit(0.0f, 1.0f, value) * heightScale;
            return { x, y };
        };

        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(plot.toNearestInt());

        juce::Path floor;
        auto p00 = project(0.0f, 0.0f, 0.0f);
        auto p10 = project(1.0f, 0.0f, 0.0f);
        auto p11 = project(1.0f, 1.0f, 0.0f);
        auto p01 = project(0.0f, 1.0f, 0.0f);
        floor.startNewSubPath(p00);
        floor.lineTo(p10);
        floor.lineTo(p11);
        floor.lineTo(p01);
        floor.closeSubPath();
        g.setColour(dark ? juce::Colour(0xFF111820).withAlpha(previewBoost() ? 0.72f : 0.60f)
                         : juce::Colour(0xFFE8EDF3).withAlpha(0.84f));
        g.fillPath(floor);
        g.setColour(gridColour);
        g.strokePath(floor, juce::PathStrokeType(1.0f));

        for (int i = 0; i <= 8; ++i)
        {
            const float fNorm = static_cast<float>(i) / 8.0f;
            auto a = project(0.0f, fNorm, 0.0f);
            auto b = project(1.0f, fNorm, 0.0f);
            g.drawLine(a.x, a.y, b.x, b.y, i == 0 ? 1.2f : 0.7f);
        }
        for (float xNorm : { 0.0f, 0.5f, 1.0f })
        {
            auto a = project(xNorm, 0.0f, 0.0f);
            auto b = project(xNorm, 1.0f, 0.0f);
            g.drawLine(a.x, a.y, b.x, b.y, 0.8f);
        }

        const float baseAlpha = grids.size() > 1 ? 0.38f : 0.72f;
        for (const auto& item : grids)
        {
            const auto& grid = item.grid;
            const auto& track = *item.track;
            for (int f = fSteps - 2; f >= 0; --f)
            {
                const float fNorm0 = static_cast<float>(f) / static_cast<float>(fSteps - 1);
                const float fNorm1 = static_cast<float>(f + 1) / static_cast<float>(fSteps - 1);
                for (int x = 0; x < xSteps - 1; ++x)
                {
                    const float xNorm0 = static_cast<float>(x) / static_cast<float>(xSteps - 1);
                    const float xNorm1 = static_cast<float>(x + 1) / static_cast<float>(xSteps - 1);
                    const float v00 = grid[at(x, f)];
                    const float v10 = grid[at(x + 1, f)];
                    const float v11 = grid[at(x + 1, f + 1)];
                    const float v01 = grid[at(x, f + 1)];
                    const float avg = (v00 + v10 + v11 + v01) * 0.25f;
                    if (avg <= 0.006f)
                        continue;

                    juce::Path quad;
                    auto q00 = project(xNorm0, fNorm0, v00);
                    auto q10 = project(xNorm1, fNorm0, v10);
                    auto q11 = project(xNorm1, fNorm1, v11);
                    auto q01 = project(xNorm0, fNorm1, v01);
                    quad.startNewSubPath(q00);
                    quad.lineTo(q10);
                    quad.lineTo(q11);
                    quad.lineTo(q01);
                    quad.closeSubPath();

                    const float xMid = (xNorm0 + xNorm1) * 0.5f;
                    const float frequencyHz = frequencyFromLogNorm((fNorm0 + fNorm1) * 0.5f,
                        item.track->asset != nullptr && item.track->asset->spatialHeatmap.metrics.maxFrequencyHz > 0.0f
                            ? item.track->asset->spatialHeatmap.metrics.maxFrequencyHz : 20000.0f);
                    const bool activeBand = !layerFitBandSoloGhost || layerFitFrequencyInActiveBand(data.bandHighlight, frequencyHz);
                    auto colour = activeBand
                        ? track.colour
                            .interpolatedWith(spatialHeatmapColour(xMid, 0.48f + avg * 0.44f), grids.size() > 1 ? 0.20f : 0.46f)
                            .interpolatedWith(juce::Colours::white, dark ? avg * 0.16f : avg * 0.08f)
                            .interpolatedWith(juce::Colours::black, dark ? fNorm0 * 0.14f : fNorm0 * 0.04f)
                        : layerFitBandSoloGhostColour(dark, avg, fNorm0);
                    colour = colour.withAlpha(activeBand ? juce::jlimit(0.08f, 0.86f, baseAlpha + avg * 0.20f)
                                                         : juce::jlimit(0.08f, 0.24f, 0.10f + avg * 0.12f));
                    g.setColour(colour);
                    g.fillPath(quad);
                }
            }

            const auto ridge = track.colour.interpolatedWith(juce::Colours::white, dark ? 0.44f : 0.20f)
                                             .withAlpha(grids.size() > 1 ? 0.62f : 0.72f);
            const int ridgeStep = juce::jmax(3, fSteps / 24);
            for (int f = 0; f < fSteps; f += ridgeStep)
            {
                juce::Path path;
                bool started = false;
                float peak = 0.0f;
                const float fNorm = static_cast<float>(f) / static_cast<float>(fSteps - 1);
                for (int x = 0; x < xSteps; ++x)
                {
                    const float xNorm = static_cast<float>(x) / static_cast<float>(xSteps - 1);
                    const float value = grid[at(x, f)];
                    peak = juce::jmax(peak, value);
                    auto p = project(xNorm, fNorm, value);
                    if (!started)
                    {
                        path.startNewSubPath(p);
                        started = true;
                    }
                    else
                    {
                        path.lineTo(p);
                    }
                }
                if (peak > 0.018f)
                {
                    g.setColour(ridge.withAlpha(ridge.getFloatAlpha() * (0.40f + peak * 0.62f)));
                    g.strokePath(path, juce::PathStrokeType(0.55f + peak * 0.9f));
                }
            }
        }

        g.setColour(text);
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        auto l = project(0.0f, 0.0f, 0.0f);
        auto c = project(0.5f, 0.0f, 0.0f);
        auto r = project(1.0f, 0.0f, 0.0f);
        g.drawText("L", l.x - 16.0f, l.y + (sideCamera ? -34.0f : 8.0f), 36.0f, 30.0f, juce::Justification::centred);
        g.drawText("C", c.x - 18.0f, c.y + (sideCamera ? -34.0f : 8.0f), 38.0f, 30.0f, juce::Justification::centred);
        g.drawText("R", r.x - 16.0f, r.y + (sideCamera ? -34.0f : 8.0f), 36.0f, 30.0f, juce::Justification::centred);

        const auto& metrics = tracks.front().asset->spatialHeatmap.metrics;
        const float maxHz = metrics.maxFrequencyHz > 0.0f ? metrics.maxFrequencyHz : 20000.0f;
        const float ticks[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        for (float hz : ticks)
        {
            if (hz > maxHz)
                continue;
            const float fNorm = spatialImpressionFrequencyNorm(hz, maxHz);
            auto p = project(sideCamera ? 0.0f : 1.0f, fNorm, 0.0f);
            g.drawText(formatFrequencyTick(hz), p.x + 8.0f, p.y + (sideCamera ? 10.0f : -16.0f), 80.0f, 30.0f,
                       juce::Justification::centredLeft);
        }

        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText(grids.front().range.label, plot.reduced(18.0f, 12.0f).removeFromTop(38.0f),
                   juce::Justification::centredRight, true);

        auto legend = plot.reduced(18.0f, 12.0f).removeFromBottom(32.0f);
        for (const auto& item : grids)
        {
            auto slot = legend.removeFromLeft(juce::jmin(220.0f, legend.getWidth()));
            g.setColour(item.track->colour);
            g.fillRect(slot.removeFromLeft(18.0f).reduced(0.0f, 6.0f));
            slot.removeFromLeft(8.0f);
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(18.0f, juce::Font::bold));
            g.drawText(item.track->label, slot, juce::Justification::centredLeft, true);
        }
    }

    static void drawLayerFitTimeFrequencyStack(juce::Graphics& g, juce::Rectangle<float> area,
                                               const std::vector<SpatialHeatmapTrack>& tracks,
                                               const FigureData& data,
                                               const juce::String& titleText,
                                               bool dark, SpatialDbScale sharedScale,
                                               float maxDurationSeconds)
    {
        const auto bg = dark ? (previewBoost() ? juce::Colour(0xFF0D141C)
                                               : juce::Colour(0xFF0C1118))
                             : juce::Colour(0xFFF5F7FA);
        g.setColour(dark ? juce::Colour(0xFF070C12) : juce::Colour(0xFFF8FAFC));
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.18f) : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        auto title = area.removeFromTop(42.0f).reduced(16.0f, 0.0f);
        g.setColour(primaryText(dark));
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText(titleText, title, juce::Justification::centredLeft, true);

        auto plot = area.reduced(24.0f, 16.0f);
        if (tracks.empty())
        {
            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText("No 2.5D terrain data for the selected source.", plot, juce::Justification::centred, true);
            return;
        }

        juce::ColourGradient gradient(bg.brighter(dark ? 0.12f : 0.01f), plot.getTopLeft(),
                                      bg.darker(dark ? 0.12f : 0.03f), plot.getBottomRight(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(plot, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.28f : 0.18f)
                         : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(plot, 8.0f, 1.0f);

        auto surfaceArea = plot.reduced(12.0f, 10.0f);
        const auto& gridMetrics = tracks.front().asset->spatialHeatmap.metrics;
        drawSpatialTerrainGrid(g, surfaceArea, gridMetrics, maxDurationSeconds, dark,
                               data.terrainCamera, data.terrainTimeReversed, true, false);

        const float alpha = tracks.size() > 1 ? 0.58f : 0.92f;
        const float tint = tracks.size() > 1 ? 0.42f : 0.26f;
        for (const auto& track : tracks)
        {
            drawSpatialTerrainSurface(g, surfaceArea, track.asset->spatialHeatmap, maxDurationSeconds,
                                      dark, data.terrainCamera, data.terrainTimeReversed,
                                      sharedScale, data.bandHighlight, track.colour, tint, alpha, false, true);
        }

        drawSpatialTerrainGrid(g, surfaceArea, gridMetrics, maxDurationSeconds, dark,
                               data.terrainCamera, data.terrainTimeReversed, false, true);

        auto legend = plot.reduced(18.0f, 14.0f).removeFromBottom(34.0f);
        for (const auto& track : tracks)
        {
            auto item = legend.removeFromLeft(juce::jmin(260.0f, legend.getWidth()));
            g.setColour(track.colour);
            g.fillRect(item.removeFromLeft(18.0f).reduced(0.0f, 6.0f));
            item.removeFromLeft(8.0f);
            g.setColour(primaryText(dark));
            g.setFont(juce::Font(18.0f, juce::Font::bold));
            g.drawText(track.label, item, juce::Justification::centredLeft, true);
        }
    }

    static void drawLayerFitSpatialImagePanels(juce::Graphics& g,
                                               juce::Rectangle<float> left,
                                               juce::Rectangle<float> right,
                                               const FigureData& data,
                                               const std::array<const Asset*, 3>& sources,
                                               const std::array<juce::String, 3>& labels,
                                               bool dark)
    {
        const auto tracks = makeLayerFitTracks(sources, labels, dark);
        std::vector<SpatialHeatmapTrack> stemTracks;
        for (const auto& track : tracks)
            if (track.asset != nullptr && track.asset->spatialHeatmap.metrics.valid && track.asset->spatialHeatmap.image.isValid())
                stemTracks.push_back({ track.asset, track.label, track.colour });

        std::unique_ptr<Asset> autoBounce;
        const Asset* bounce = data.fitBounceSource;
        juce::String bounceLabel = data.fitBounceLabel;
        if (data.fitBounceAuto)
        {
            autoBounce = makeLayerFitAutoBounceAsset(tracks);
            bounce = autoBounce.get();
            bounceLabel = "Auto Bounce";
        }

        std::vector<SpatialHeatmapTrack> bounceTracks;
        if (bounce != nullptr && bounce->spatialHeatmap.metrics.valid && bounce->spatialHeatmap.image.isValid())
            bounceTracks.push_back({ bounce, bounceLabel, layerFitBounceColour(dark) });

        auto scaleTracks = stemTracks;
        scaleTracks.insert(scaleTracks.end(), bounceTracks.begin(), bounceTracks.end());
        const auto sharedScale = makeSpatialDbScale(scaleTracks, data);

        drawLayerFitSpatialStack(g, left, stemTracks, data, dark, sharedScale);
        drawLayerFitSpatialStack(g, right, bounceTracks, data, dark, sharedScale);
    }

    static void drawSpatialWidthLegend(juce::Graphics& g, juce::Rectangle<float> area, bool dark)
    {
        if (area.getWidth() < 280.0f || area.getHeight() < 42.0f)
            return;

        auto bar = area.withTrimmedTop(8.0f).withTrimmedBottom(30.0f).removeFromTop(18.0f);
        const int steps = 96;
        for (int i = 0; i < steps; ++i)
        {
            const float t0 = static_cast<float>(i) / static_cast<float>(steps);
            const float x = bar.getX() + bar.getWidth() * t0;
            const float w = bar.getWidth() / static_cast<float>(steps) + 1.0f;
            g.setColour(spatialHeatmapColour(t0, 0.95f));
            g.fillRect(x, bar.getY(), w, bar.getHeight());
        }

        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        g.drawText("narrow / correlated", area.withTrimmedTop(bar.getBottom() - area.getY() + 4.0f).removeFromLeft(area.getWidth() * 0.5f),
                   juce::Justification::centredLeft, true);
        g.drawText("wide / side / split", area.withTrimmedTop(bar.getBottom() - area.getY() + 4.0f),
                   juce::Justification::centredRight, true);
    }

    static void drawBandHighlightLegend(juce::Graphics& g, juce::Rectangle<float> area,
                                        const BandHighlightConfig& config, bool dark)
    {
        if (!config.enabled || config.bands.empty() || area.getWidth() < 220.0f || area.getHeight() < 24.0f)
            return;

        auto row = area.reduced(4.0f, 2.0f);
        const float gap = 10.0f;
        const float itemW = (row.getWidth() - gap * static_cast<float>(config.bands.size() - 1))
                          / static_cast<float>(juce::jmax(1, static_cast<int>(config.bands.size())));
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        for (const auto& band : config.bands)
        {
            auto item = row.removeFromLeft(itemW);
            row.removeFromLeft(gap);
            const bool active = band.active || !hasActiveBand(config);
            g.setColour(band.colour.withAlpha(active ? 0.95f : 0.22f));
            g.fillRoundedRectangle(item.removeFromLeft(30.0f).reduced(0.0f, 6.0f), 2.0f);
            item.removeFromLeft(6.0f);
            g.setColour((active ? primaryText(dark) : secondaryText(dark)).withMultipliedAlpha(active ? 1.0f : 0.48f));
            g.drawText(band.label + " " + juce::String(static_cast<int>(band.minHz)) + "-"
                         + (band.maxHz >= 1000.0f ? juce::String(band.maxHz / 1000.0f, 0) + "k"
                                                   : juce::String(static_cast<int>(band.maxHz))),
                       item, juce::Justification::centredLeft, true);
        }
    }

    static void drawSpatialLcrLegend(juce::Graphics& g, juce::Rectangle<float> area, bool dark)
    {
        if (area.getWidth() < 280.0f || area.getHeight() < 42.0f)
            return;

        auto bar = area.withTrimmedTop(8.0f).withTrimmedBottom(30.0f).removeFromTop(18.0f);
        const int steps = 96;
        for (int i = 0; i < steps; ++i)
        {
            const float t0 = static_cast<float>(i) / static_cast<float>(steps);
            const float x = bar.getX() + bar.getWidth() * t0;
            const float w = bar.getWidth() / static_cast<float>(steps) + 1.0f;
            g.setColour(spatialHeatmapColour(t0, 0.95f));
            g.fillRect(x, bar.getY(), w, bar.getHeight());
        }

        g.setColour(secondaryText(dark));
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        auto labelArea = area.withTrimmedTop(bar.getBottom() - area.getY() + 4.0f);
        g.drawText("left / L", labelArea.removeFromLeft(area.getWidth() * 0.32f),
                   juce::Justification::centredLeft, true);
        g.drawText("center / C", labelArea.removeFromLeft(area.getWidth() * 0.36f),
                   juce::Justification::centred, true);
        g.drawText("right / R", labelArea,
                   juce::Justification::centredRight, true);
    }

    static float sampleSpatialTerrainEnergy(const juce::Image& image, float timeNorm, float freqNorm)
    {
        if (!image.isValid())
            return 0.0f;

        const int x = juce::jlimit(0, image.getWidth() - 1,
                                   juce::roundToInt(timeNorm * static_cast<float>(image.getWidth() - 1)));
        const int y = juce::jlimit(0, image.getHeight() - 1,
                                   juce::roundToInt((1.0f - freqNorm) * static_cast<float>(image.getHeight() - 1)));

        float energy = 0.0f;
        for (int yy = juce::jmax(0, y - 1); yy <= juce::jmin(image.getHeight() - 1, y + 1); ++yy)
            for (int xx = juce::jmax(0, x - 1); xx <= juce::jmin(image.getWidth() - 1, x + 1); ++xx)
                energy = juce::jmax(energy, image.getPixelAt(xx, yy).getFloatAlpha());

        return juce::jlimit(0.0f, 1.0f, std::pow(energy, 0.78f));
    }

    static float remapSpatialAlphaToScale(float alpha, const SpatialHeatmapMetrics& metrics, SpatialDbScale sharedScale)
    {
        alpha = juce::jlimit(0.0f, 1.0f, alpha);
        if (!sharedScale.enabled)
            return alpha;

        const float localSpan = juce::jmax(1.0f, metrics.ceilingDb - metrics.floorDb);
        const float localNorm = std::pow(juce::jlimit(0.0f, 1.0f, alpha / 0.96f), 1.0f / 0.64f);
        const float energyDb = metrics.floorDb + localNorm * localSpan;
        const float sharedNorm = juce::jlimit(0.0f, 1.0f,
            (energyDb - sharedScale.floorDb) / juce::jmax(1.0f, sharedScale.ceilingDb - sharedScale.floorDb));
        return std::pow(sharedNorm, 0.64f) * 0.96f;
    }

    static float sampleSpatialTerrainEnergySoft(const SpatialHeatmapAnalysis& analysis, float timeNorm, float freqNorm,
                                                SpatialDbScale sharedScale)
    {
        const auto& image = analysis.image;
        if (!image.isValid())
            return 0.0f;

        const float x = juce::jlimit(0.0f, static_cast<float>(image.getWidth() - 1),
                                     timeNorm * static_cast<float>(image.getWidth() - 1));
        const float y = juce::jlimit(0.0f, static_cast<float>(image.getHeight() - 1),
                                     (1.0f - freqNorm) * static_cast<float>(image.getHeight() - 1));
        const int ix = juce::roundToInt(x);
        const int iy = juce::roundToInt(y);
        float weighted = 0.0f;
        float weights = 0.0f;
        float peak = 0.0f;

        for (int yy = juce::jmax(0, iy - 2); yy <= juce::jmin(image.getHeight() - 1, iy + 2); ++yy)
        {
            for (int xx = juce::jmax(0, ix - 2); xx <= juce::jmin(image.getWidth() - 1, ix + 2); ++xx)
            {
                const float dx = (static_cast<float>(xx) - x) / 2.0f;
                const float dy = (static_cast<float>(yy) - y) / 2.0f;
                const float weight = juce::jmax(0.0f, 1.0f - (dx * dx + dy * dy) * 0.36f);
                const float alpha = image.getPixelAt(xx, yy).getFloatAlpha();
                weighted += alpha * weight;
                weights += weight;
                peak = juce::jmax(peak, alpha);
            }
        }

        const float smooth = weights > 0.0f ? weighted / weights : 0.0f;
        return juce::jlimit(0.0f, 1.0f,
            std::pow(remapSpatialAlphaToScale(smooth * 0.78f + peak * 0.22f, analysis.metrics, sharedScale), 0.74f));
    }

    static float sampleSpatialTerrainWidth(const juce::Image& image, float timeNorm, float freqNorm)
    {
        if (!image.isValid())
            return 0.5f;

        const int x = juce::jlimit(0, image.getWidth() - 1,
                                   juce::roundToInt(timeNorm * static_cast<float>(image.getWidth() - 1)));
        const int y = juce::jlimit(0, image.getHeight() - 1,
                                   juce::roundToInt((1.0f - freqNorm) * static_cast<float>(image.getHeight() - 1)));
        const auto c = image.getPixelAt(x, y);
        if (c.getFloatAlpha() <= 0.01f)
            return 0.5f;

        const float warm = c.getFloatRed() + 0.5f * c.getFloatGreen();
        const float cool = c.getFloatBlue() + 0.5f * c.getFloatGreen();
        return juce::jlimit(0.0f, 1.0f, 0.5f + (warm - cool) * 0.45f);
    }

    struct TerrainProjection
    {
        float originX = 0.055f;
        float originY = 0.070f;
        float timeSpan = 0.770f;
        float depthX = 0.175f;
        float depthY = 0.310f;
        float heightScale = 0.590f;
        float perspective = 0.100f;
    };

    static TerrainProjection terrainProjectionForCamera(TerrainCamera camera)
    {
        switch (camera)
        {
            case TerrainCamera::frontHigh:
                return { 0.072f, 0.060f, 0.840f, 0.095f, 0.700f, 0.330f, 0.220f };
            case TerrainCamera::frontLow:
                return { 0.074f, 0.060f, 0.840f, 0.070f, 0.510f, 0.430f, 0.120f };
            case TerrainCamera::sideLow:
                return { 0.062f, 0.060f, 0.835f, 0.060f, 0.500f, 0.420f, 0.115f };
            case TerrainCamera::sideHigh:
                return { 0.060f, 0.060f, 0.830f, 0.120f, 0.700f, 0.330f, 0.230f };
            case TerrainCamera::diagonal:
            default:
                return { 0.060f, 0.060f, 0.805f, 0.190f, 0.610f, 0.380f, 0.205f };
        }
    }

    static bool isSideTerrainCamera(TerrainCamera camera)
    {
        return camera == TerrainCamera::sideLow || camera == TerrainCamera::sideHigh;
    }

    static bool isFrontTerrainCamera(TerrainCamera camera)
    {
        return camera == TerrainCamera::frontLow || camera == TerrainCamera::frontHigh;
    }

    static float terrainDisplayNormForFrequency(float frequencyHz, float nyquistHz)
    {
        const float minHz = 100.0f;
        const float maxHz = juce::jmax(minHz * 1.25f, nyquistHz);
        const float safeFrequency = juce::jlimit(minHz, maxHz, frequencyHz);
        return juce::jlimit(0.0f, 1.0f, std::log(safeFrequency / minHz) / std::log(maxHz / minHz));
    }

    static float terrainDataNormForDisplay(float displayNorm, float nyquistHz)
    {
        const float minHz = 100.0f;
        const float maxHz = juce::jmax(minHz * 1.25f, nyquistHz);
        const float frequencyHz = minHz * std::pow(maxHz / minHz, juce::jlimit(0.0f, 1.0f, displayNorm));
        return juce::jlimit(0.0f, 1.0f, frequencyHz / juce::jmax(1.0f, nyquistHz));
    }

    static juce::Point<float> projectSpatialTerrainPoint(juce::Rectangle<float> plot,
                                                         float timeNorm, float freqNorm, float energyNorm,
                                                         TerrainCamera camera)
    {
        const auto p = terrainProjectionForCamera(camera);
        const float originX = plot.getX() + plot.getWidth() * p.originX;
        const float originY = plot.getBottom() - plot.getHeight() * p.originY;
        const float mainSpan = plot.getWidth() * p.timeSpan;
        const float depthX = plot.getWidth() * p.depthX;
        const float depthY = plot.getHeight() * p.depthY;
        const float heightScale = plot.getHeight() * p.heightScale;

        if (isFrontTerrainCamera(camera))
        {
            const float perspective = 1.0f - freqNorm * p.perspective;
            return {
                originX + timeNorm * mainSpan * perspective + freqNorm * depthX,
                originY - freqNorm * depthY - energyNorm * heightScale
            };
        }

        const float perspective = 1.0f - timeNorm * p.perspective;
        return {
            originX + freqNorm * mainSpan * perspective + timeNorm * depthX,
            originY - timeNorm * depthY - energyNorm * heightScale
        };
    }

    static void drawSpatialTerrainGrid(juce::Graphics& g, juce::Rectangle<float> plot,
                                       const SpatialHeatmapMetrics& metrics, float maxDurationSeconds,
                                       bool dark, TerrainCamera camera,
                                       bool timeReversed,
                                       bool drawInterior = true, bool drawLabels = true)
    {
        const auto axis = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.62f : 0.42f)
                               : juce::Colour(0xFF64748B).withAlpha(0.36f);
        const auto grid = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.34f : 0.24f)
                               : juce::Colour(0xFF94A3B8).withAlpha(0.30f);
        const auto text = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.98f : 0.88f)
                               : juce::Colour(0xFF334155).withAlpha(0.88f);
        const bool frontCamera = isFrontTerrainCamera(camera);

        auto p00 = projectSpatialTerrainPoint(plot, 0.0f, 0.0f, 0.0f, camera);
        auto p10 = projectSpatialTerrainPoint(plot, 1.0f, 0.0f, 0.0f, camera);
        auto p01 = projectSpatialTerrainPoint(plot, 0.0f, 1.0f, 0.0f, camera);
        auto p11 = projectSpatialTerrainPoint(plot, 1.0f, 1.0f, 0.0f, camera);

        g.setColour(axis);
        g.drawLine({ p00, p10 }, 1.35f);
        g.drawLine({ p00, p01 }, 1.20f);
        g.drawLine({ p01, p11 }, 1.05f);
        g.drawLine({ p10, p11 }, 1.05f);

        g.setFont(juce::Font(30.0f, juce::Font::bold));
        for (int i = 0; i <= 4; ++i)
        {
            const float t = static_cast<float>(i) / 4.0f;
            auto a = projectSpatialTerrainPoint(plot, t, 0.0f, 0.0f, camera);
            auto b = projectSpatialTerrainPoint(plot, t, 1.0f, 0.0f, camera);
            if (drawInterior || i == 0 || i == 4)
            {
                g.setColour(grid);
                g.drawLine({ a, b }, i == 0 || i == 4 ? 1.0f : 0.75f);
            }
            if (drawLabels)
            {
                g.setColour(text);
                const float seconds = maxDurationSeconds * (timeReversed ? (1.0f - t) : t);
                const auto label = juce::String(seconds, maxDurationSeconds >= 10.0f ? 1 : 2) + "s";
                if (frontCamera)
                {
                    g.drawText(label, a.x - 58.0f, a.y + 14.0f, 116.0f, 36.0f,
                               juce::Justification::centred);
                }
                else
                {
                    g.drawText(label, a.x - 140.0f, a.y - 19.0f, 122.0f, 38.0f,
                               juce::Justification::centredRight);
                }
            }
        }

        const float nyquist = metrics.maxFrequencyHz > 0.0f ? juce::jmax(metrics.maxFrequencyHz, 1000.0f) : 24000.0f;
        const float freqs[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        float lastFreqLabelX = -10000.0f;
        float lastFreqLabelY = -10000.0f;
        for (float freq : freqs)
        {
            if (freq > nyquist)
                continue;
            const float f = terrainDisplayNormForFrequency(freq, nyquist);
            auto a = projectSpatialTerrainPoint(plot, 0.0f, f, 0.0f, camera);
            auto b = projectSpatialTerrainPoint(plot, 1.0f, f, 0.0f, camera);
            if (drawInterior || freq == 100.0f || freq == 20000.0f)
            {
                g.setColour(grid);
                g.drawLine({ a, b }, freq == 1000.0f || freq == 10000.0f ? 1.05f : 0.72f);
            }
            if (! drawLabels)
                continue;

            g.setColour(text);
            const auto label = formatFrequencyTick(freq);
            if (std::abs(a.x - lastFreqLabelX) < 86.0f && std::abs(a.y - lastFreqLabelY) < 42.0f)
                continue;
            lastFreqLabelX = a.x;
            lastFreqLabelY = a.y;
            if (frontCamera)
            {
                g.drawText(label, a.x - 116.0f, a.y - 19.0f, 100.0f, 38.0f,
                           juce::Justification::centredRight);
            }
            else
            {
                g.drawText(label, a.x - 52.0f, a.y + 16.0f, 104.0f, 38.0f,
                           juce::Justification::centred);
            }
        }
    }

    static void drawSpatialTerrainMeshSurface(juce::Graphics& g, juce::Rectangle<float> plot,
                                              const SpatialHeatmapAnalysis& analysis, float maxDurationSeconds,
                                              bool dark, TerrainCamera camera,
                                              bool timeReversed, SpatialDbScale sharedScale,
                                              const BandHighlightConfig& bandHighlight,
                                              juce::Colour surfaceTint = juce::Colour(),
                                              float tintAmount = 0.0f,
                                              float alphaScale = 1.0f,
                                              bool drawGrid = true,
                                              bool layerFitBandSoloGhost = false)
    {
        const auto low = dark ? juce::Colour(0xFF0C1117) : juce::Colour(0xFF6F7A86);
        const auto mid = dark ? (previewBoost() ? juce::Colour(0xFF6B7A86)
                                                : juce::Colour(0xFF4D5A65))
                              : juce::Colour(0xFF99A4AF);
        const auto high = dark ? (previewBoost() ? juce::Colour(0xFFF3F7FB)
                                                 : juce::Colour(0xFFDDE6EE))
                               : juce::Colour(0xFFE7EBF0);
        const auto ridge = dark ? juce::Colour(0xFFF6FAFF)
                                : juce::Colour(0xFFFFFFFF);
        const auto contour = dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.12f : 0.09f)
                                  : juce::Colour(0xFF475569).withAlpha(0.10f);

        const int timeSteps = juce::jlimit(48, 96, analysis.image.getWidth() / 7);
        const int frequencySteps = juce::jlimit(56, 118, analysis.image.getHeight() / 5);
        const float duration = static_cast<float>(analysis.metrics.durationSeconds);
        const float durationRatio = juce::jlimit(0.0f, 1.0f, duration / juce::jmax(0.001f, maxDurationSeconds));
        const float nyquist = analysis.metrics.maxFrequencyHz > 0.0f
            ? juce::jmax(analysis.metrics.maxFrequencyHz, 1000.0f) : 24000.0f;

        const auto index = [frequencySteps] (int t, int f)
        {
            return static_cast<size_t>(t * (frequencySteps + 1) + f);
        };

        std::vector<juce::Point<float>> topPoints(static_cast<size_t>((timeSteps + 1) * (frequencySteps + 1)));
        std::vector<juce::Point<float>> groundPoints(topPoints.size());
        std::vector<float> energies(topPoints.size(), 0.0f);
        std::vector<float> widths(topPoints.size(), 0.5f);

        if (drawGrid)
            drawSpatialTerrainGrid(g, plot, analysis.metrics, maxDurationSeconds, dark, camera, timeReversed, true, false);

        for (int t = 0; t <= timeSteps; ++t)
        {
            const float timeNorm = static_cast<float>(t) / static_cast<float>(juce::jmax(1, timeSteps));
            const bool activeTime = timeReversed
                ? timeNorm >= 1.0f - durationRatio - 0.001f
                : timeNorm <= durationRatio + 0.001f;
            const float localTime = timeReversed ? (1.0f - timeNorm) : timeNorm;
            const float sourceTime = durationRatio > 0.0f ? juce::jlimit(0.0f, 1.0f, localTime / durationRatio) : 0.0f;
            for (int f = 0; f <= frequencySteps; ++f)
            {
                const float displayFreqNorm = static_cast<float>(f) / static_cast<float>(juce::jmax(1, frequencySteps));
                const float dataFreqNorm = terrainDataNormForDisplay(displayFreqNorm, nyquist);
                const float frequencyHz = frequencyFromLogNorm(displayFreqNorm, nyquist);
                const bool active = activeTime;
                const float rawEnergy = active ? sampleSpatialTerrainEnergySoft(analysis, sourceTime, dataFreqNorm, sharedScale) : 0.0f;
                const float energyScale = layerFitBandSoloGhost ? 1.0f : bandHighlightEnergyScale(bandHighlight, frequencyHz);
                const float energy = std::pow(juce::jlimit(0.0f, 1.0f, (rawEnergy * energyScale - 0.095f) / 0.905f), 1.32f);
                const float width = active ? sampleSpatialTerrainWidth(analysis.image, sourceTime, dataFreqNorm) : 0.5f;
                const auto i = index(t, f);
                energies[i] = energy;
                widths[i] = width;
                topPoints[i] = projectSpatialTerrainPoint(plot, timeNorm, displayFreqNorm, energy, camera);
                groundPoints[i] = projectSpatialTerrainPoint(plot, timeNorm, displayFreqNorm, 0.0f, camera);
            }
        }

        const auto drawCell = [&] (int t, int f)
        {
            const float depth = static_cast<float>(t) / static_cast<float>(juce::jmax(1, timeSteps));
            const auto i00 = index(t, f);
            const auto i01 = index(t, f + 1);
            const auto i10 = index(t + 1, f);
            const auto i11 = index(t + 1, f + 1);
            const float e00 = energies[i00];
            const float e01 = energies[i01];
            const float e10 = energies[i10];
            const float e11 = energies[i11];
            const float e = (e00 + e01 + e10 + e11) * 0.25f;
            const float peak = juce::jmax(juce::jmax(e00, e01), juce::jmax(e10, e11));
            if (peak < 0.030f && e < 0.020f)
                return;

            const float slopeFrequency = std::abs(e01 - e00) + std::abs(e11 - e10);
            const float slopeTime = std::abs(e10 - e00) + std::abs(e11 - e01);
            const float slope = juce::jlimit(0.0f, 1.0f, (slopeFrequency + slopeTime) * 0.60f);
            const float width = (widths[i00] + widths[i01] + widths[i10] + widths[i11]) * 0.25f;
            const float fNorm = (static_cast<float>(f) + 0.5f) / static_cast<float>(juce::jmax(1, frequencySteps));
            const float frequencyHz = frequencyFromLogNorm(fNorm, nyquist);
            const bool activeBand = !layerFitBandSoloGhost || layerFitFrequencyInActiveBand(bandHighlight, frequencyHz);
            const float depthShade = isFrontTerrainCamera(camera)
                ? 1.0f - (static_cast<float>(f) / static_cast<float>(juce::jmax(1, frequencySteps))) * (camera == TerrainCamera::frontHigh ? 0.54f : 0.36f)
                : 1.0f - depth * (camera == TerrainCamera::sideHigh ? 0.55f : 0.36f);
            const float energyContrast = std::pow(juce::jlimit(0.0f, 1.0f, e), 0.72f);
            const float lightness = juce::jlimit(0.0f, 1.0f,
                0.04f + energyContrast * 0.80f + slope * 0.42f + depthShade * 0.13f);
            auto fill = activeBand
                ? low.interpolatedWith(mid, lightness)
                     .interpolatedWith(high, juce::jlimit(0.0f, 1.0f, energyContrast * 0.58f + slope * 0.28f))
                     .interpolatedWith(spatialHeatmapColour(width, 0.82f), dark ? 0.018f : 0.08f)
                : layerFitBandSoloGhostColour(dark, energyContrast, depth);
            if (activeBand)
                fill = applyBandHighlightTint(fill, bandHighlight, frequencyHz, dark, energyContrast);
            if (tintAmount > 0.0f && activeBand)
                fill = fill.interpolatedWith(surfaceTint, juce::jlimit(0.0f, 1.0f, tintAmount));
            const float alpha = (activeBand ? (dark ? (previewBoost() ? 0.86f + energyContrast * 0.12f : 0.82f + energyContrast * 0.14f)
                                                    : (0.84f + energyContrast * 0.12f))
                                            : (dark ? 0.15f + energyContrast * 0.12f
                                                    : 0.18f + energyContrast * 0.10f))
                              * juce::jlimit(0.0f, 1.0f, alphaScale);

            juce::Path cell;
            cell.startNewSubPath(topPoints[i00]);
            cell.lineTo(topPoints[i01]);
            cell.lineTo(topPoints[i11]);
            cell.lineTo(topPoints[i10]);
            cell.closeSubPath();
            g.setColour(fill.withAlpha(alpha));
            g.fillPath(cell);
        };

        if (isFrontTerrainCamera(camera))
        {
            for (int f = frequencySteps - 1; f >= 0; --f)
                for (int t = 0; t < timeSteps; ++t)
                    drawCell(t, f);
        }
        else
        {
            for (int t = timeSteps - 1; t >= 0; --t)
                for (int f = 0; f < frequencySteps; ++f)
                    drawCell(t, f);
        }

        if (drawGrid)
            drawSpatialTerrainGrid(g, plot, analysis.metrics, maxDurationSeconds, dark, camera, timeReversed, false, true);

        const int timeRidgeStep = juce::jmax(8, timeSteps / 6);
        for (int t = timeSteps; t >= 0; t -= timeRidgeStep)
        {
            juce::Path line;
            bool started = false;
            float peak = 0.0f;
            for (int f = 0; f <= frequencySteps; ++f)
            {
                const auto i = index(t, f);
                peak = juce::jmax(peak, energies[i]);
                if (!started)
                {
                    line.startNewSubPath(topPoints[i]);
                    started = true;
                }
                else
                {
                    line.lineTo(topPoints[i]);
                }
            }
            g.setColour(ridge.withAlpha((previewBoost() ? 0.16f : 0.12f) + peak * 0.30f));
            g.strokePath(line, juce::PathStrokeType(0.72f + peak * 0.95f));
        }

        const int frequencyContourStep = juce::jmax(18, frequencySteps / 4);
        for (int f = 0; f <= frequencySteps; f += frequencyContourStep)
        {
            juce::Path line;
            bool started = false;
            for (int t = 0; t <= timeSteps; ++t)
            {
                const auto i = index(t, f);
                if (!started)
                {
                    line.startNewSubPath(topPoints[i]);
                    started = true;
                }
                else
                {
                    line.lineTo(topPoints[i]);
                }
            }
            g.setColour(contour);
            g.strokePath(line, juce::PathStrokeType(0.46f));
        }
    }

    static void drawSpatialTerrainSurface(juce::Graphics& g, juce::Rectangle<float> plot,
                                          const SpatialHeatmapAnalysis& analysis, float maxDurationSeconds,
                                          bool dark, TerrainCamera camera,
                                          bool timeReversed, SpatialDbScale sharedScale,
                                          const BandHighlightConfig& bandHighlight,
                                          juce::Colour surfaceTint = juce::Colour(),
                                          float tintAmount = 0.0f,
                                          float alphaScale = 1.0f,
                                          bool drawGrid = true,
                                          bool layerFitBandSoloGhost = false)
    {
        drawSpatialTerrainMeshSurface(g, plot, analysis, maxDurationSeconds, dark, camera, timeReversed,
                                      sharedScale, bandHighlight, surfaceTint, tintAmount, alphaScale, drawGrid,
                                      layerFitBandSoloGhost);
    }

    static void drawSpatialTerrainPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                        const SpatialHeatmapTrack& track, float maxDurationSeconds,
                                        bool dark, TerrainCamera camera,
                                        bool timeReversed, SpatialDbScale sharedScale,
                                        const BandHighlightConfig& bandHighlight)
    {
        auto labelArea = area.removeFromTop(44.0f);
        g.setColour(track.colour);
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText(track.label, labelArea, juce::Justification::centredLeft, true);

        auto plot = area.withTrimmedBottom(46.0f);
        const auto bg = dark ? (previewBoost() ? juce::Colour(0xFF0D141C)
                                               : juce::Colour(0xFF0C1118))
                             : juce::Colour(0xFFF5F7FA);
        juce::ColourGradient gradient(bg.brighter(dark ? 0.12f : 0.01f), plot.getTopLeft(),
                                      bg.darker(dark ? 0.12f : 0.03f), plot.getBottomRight(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(plot, 8.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(previewBoost() ? 0.28f : 0.18f)
                         : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(plot, 8.0f, 1.0f);

        const auto& analysis = track.asset->spatialHeatmap;
        drawSpatialTerrainSurface(g, plot.reduced(12.0f, 10.0f), analysis, maxDurationSeconds, dark,
                                  camera, timeReversed, sharedScale, bandHighlight);
    }

    static void drawSpatialHeatmapTicks(juce::Graphics& g, juce::Rectangle<float> plot,
                                        const SpatialHeatmapMetrics& metrics, float maxDurationSeconds, bool dark)
    {
        const auto grid = dark ? juce::Colours::white.withAlpha(0.18f)
                               : (academicLight() ? juce::Colour(0xFFD7DEE8) : juce::Colour(0xFF334155).withAlpha(0.22f));
        const auto text = dark ? juce::Colours::white.withAlpha(0.76f)
                               : (academicLight() ? juce::Colour(0xFF374151).withAlpha(0.88f) : juce::Colour(0xFF334155).withAlpha(0.88f));
        const float nyquist = metrics.maxFrequencyHz > 0.0f ? juce::jmax(metrics.maxFrequencyHz, 1000.0f) : 24000.0f;
        const float freqs[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        for (float freq : freqs)
        {
            if (freq > nyquist)
                continue;
            const float y = plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, freq / nyquist);
            g.setColour(grid);
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            g.setColour(text);
            g.drawText(formatFrequencyTick(freq), plot.getRight() - 82.0f, y - 15.0f, 76.0f, 30.0f,
                       juce::Justification::centredRight);
        }

        for (int i = 0; i <= 4; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / 4.0f;
            const float seconds = maxDurationSeconds * static_cast<float>(i) / 4.0f;
            g.setColour(grid.withAlpha(grid.getFloatAlpha() * 0.75f));
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
            g.setColour(text);
            g.drawText(juce::String(seconds, maxDurationSeconds >= 10.0f ? 1 : 2) + "s",
                       x - 46.0f, plot.getBottom() + 10.0f, 92.0f, 30.0f,
                       juce::Justification::centred);
        }
    }

    static void drawSpatialHeatmapMetrics(juce::Graphics& g, juce::Rectangle<float> area,
                                          const std::vector<SpatialHeatmapTrack>& tracks, bool dark)
    {
        if (tracks.empty())
            return;

        constexpr float gap = 28.0f;
        const float columnWidth = tracks.size() == 1 ? area.getWidth()
                                                     : (area.getWidth() - gap) * 0.5f;
        auto remaining = area;
        for (const auto& track : tracks)
        {
            auto block = remaining.removeFromLeft(columnWidth);
            remaining.removeFromLeft(gap);

            const auto& m = track.asset->spatialHeatmap.metrics;
            g.setColour(track.colour);
            g.fillRect(block.removeFromLeft(24.0f).reduced(0.0f, 6.0f));
            block.removeFromLeft(18.0f);

            g.setColour(primaryText(dark));
            g.setFont(juce::Font(36.0f, juce::Font::bold));
            g.drawText(track.label + " | Corr mean " + juce::String(m.stereoCorrelationMean, 2)
                         + " / low5 " + juce::String(m.stereoCorrelationMin, 2),
                       block.removeFromTop(58.0f), juce::Justification::centredLeft, true);

            g.setColour(secondaryText(dark));
            g.setFont(juce::Font(30.0f));
            g.drawText("S/M mean " + juce::String(m.sideToMidDbMean, 1) + " dB"
                         + " | tail " + juce::String(m.sideToMidDbTail, 1) + " dB"
                         + " | L/R diff " + juce::String(m.lrRmsDiffDb, 1) + " dB",
                       block.removeFromTop(50.0f), juce::Justification::centredLeft, true);
            g.drawText("Tail " + juce::String(m.tailStartSeconds, 2) + "-" + juce::String(m.tailEndSeconds, 2) + " s"
                         + " | bins " + juce::String(m.timeBins) + " x " + juce::String(m.frequencyBins)
                         + " | scale " + juce::String(m.floorDb, 1) + " to " + juce::String(m.ceilingDb, 1) + " dB",
                       block.removeFromTop(50.0f), juce::Justification::centredLeft, true);
        }
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

        const float gap = tracks.size() > 1 ? 12.0f : 0.0f;
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
