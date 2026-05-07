/*
  ==============================================================================
    AudioDoctorAnalysis.h
    GOODMETER standalone Audio Doctor - offline analysis helpers.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

namespace goodmeter::audio_doctor
{

struct Metrics
{
    double sampleRate = 0.0;
    int channels = 0;
    juce::int64 samples = 0;
    double durationSeconds = 0.0;
    float peakDb = -120.0f;
    float rmsDb = -120.0f;
    float crestDb = 0.0f;
};

struct ReverbSpaceMetrics
{
    bool valid = false;
    float onsetSeconds = 0.0f;
    float tailEndSeconds = 0.0f;
    float directEnergyDb = -120.0f;
    float earlyEnergyDb = -120.0f;
    float lateEnergyDb = -120.0f;
    float drrDb = 0.0f;
    float earlyLateDb = 0.0f;
    float rt20Seconds = 0.0f;
    float rt30Seconds = 0.0f;
    float rt60Seconds = 0.0f;
    float stereoCorrelation = 0.0f;
    float sideToMidDb = 0.0f;
};

struct DynamicsMetrics
{
    bool valid = false;
    float rmsRangeDb = 0.0f;
    float rmsP10Db = -120.0f;
    float rmsP50Db = -120.0f;
    float rmsP90Db = -120.0f;
    float transientToSustainDb = 0.0f;
    float onsetSeconds = 0.0f;
};

struct ApparentAttenuationStats
{
    bool valid = false;
    int pointCount = 0;
    float meanDeltaDb = 0.0f;
    float meanAbsDeltaDb = 0.0f;
    float maxReductionDb = 0.0f;
    float maxExpansionDb = 0.0f;
    float peakReductionSeconds = 0.0f;
    float peakExpansionSeconds = 0.0f;
};

struct EditMetadata
{
    bool valid = false;
    juce::String channelName;
    juce::String originalSourcePath;
    juce::String derivedSourcePath;
    double trimStartSeconds = 0.0;
    double trimEndSeconds = 0.0;
    double fadeInMs = 0.0;
    double fadeOutMs = 0.0;
    bool snapToZeroCrossing = false;
    juce::String createdAt;
};

struct PlotPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct GeneratedSignalSpec
{
    juce::String type = "impulse";
    juce::String preset = "thesis_default";
    double sampleRate = 48000.0;
    int channels = 2;
    double seconds = 2.0;
    double levelDb = -6.0;
    double frequencyHz = 1000.0;
    double startHz = 20.0;
    double endHz = 20000.0;
    double phaseDegrees = 0.0;
    int harmonicCount = 6;
    double harmonicRolloffDb = 6.0;
    int seed = 0xAD10;
    bool invert = false;
    double noiseAmount = 0.35;
    double modRateHz = 4.0;
    double modDepth = 0.18;
    double curve = 1.6;
    double bodyHz = 90.0;
    double bodyDecayMs = 240.0;
    double crackHz = 4200.0;
    double crackAmount = 0.55;
    double noiseBandLowHz = 1200.0;
    double noiseBandHighHz = 9000.0;
    double transientMs = 55.0;
    double fundamentalHz = 180.0;
    double decayMs = 900.0;
    double tailBandLowHz = 120.0;
    double tailBandHighHz = 5200.0;
    double damping = 0.45;
    double earlyReflectionAmount = 0.22;
    double stereoWidth = 0.35;
    double chargeStartSec = 0.0;
    double shotTimeSec = 0.82;
    double tailStartSec = 0.92;
    double stageBalance = 1.0;
    double fundamentalAHz = 220.0;
    double fundamentalBHz = 330.0;
    double detuneCents = 0.0;
    double overlapAmount = 0.55;
};

struct StageMarker
{
    juce::String label;
    double startSec = 0.0;
    double endSec = 0.0;
};

struct SpectrumPeak
{
    float frequencyHz = 0.0f;
    float magnitudeDb = -120.0f;
    int harmonicNumber = 0;
    float expectedHz = 0.0f;
    float deltaCents = 0.0f;
    bool nearHarmonic = false;
};

struct SpatialHeatmapMetrics
{
    bool valid = false;
    double durationSeconds = 0.0;
    double tailStartSeconds = 0.0;
    double tailEndSeconds = 0.0;
    int timeBins = 0;
    int frequencyBins = 0;
    int fftSize = 0;
    int hopSize = 0;
    float minFrequencyHz = 20.0f;
    float maxFrequencyHz = 20000.0f;
    float floorDb = -90.0f;
    float ceilingDb = 0.0f;
    float stereoCorrelationMean = 0.0f;
    float stereoCorrelationMin = 0.0f;
    float sideToMidDbMean = 0.0f;
    float sideToMidDbTail = 0.0f;
    float leftRmsDb = -120.0f;
    float rightRmsDb = -120.0f;
    float lrRmsDiffDb = 0.0f;
};

struct SpatialHeatmapCell
{
    float timeStartSeconds = 0.0f;
    float timeEndSeconds = 0.0f;
    float frequencyLowHz = 0.0f;
    float frequencyHighHz = 0.0f;
    float energyDb = -120.0f;
    float widthIndex = 0.0f;
    float sideToMidDb = -120.0f;
    float lrBalanceDb = 0.0f;
    float correlation = 0.0f;
};

struct SpatialHeatmapAnalysis
{
    SpatialHeatmapMetrics metrics;
    juce::Image image;
    std::vector<SpatialHeatmapCell> sampledCells;
};

struct Asset
{
    juce::String name;
    juce::String sourcePath;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 48000.0;
    Metrics metrics;
    ReverbSpaceMetrics spaceMetrics;
    DynamicsMetrics dynamicsMetrics;
    EditMetadata editMetadata;
    bool generatedSignal = false;
    GeneratedSignalSpec generatedSignalSpec;
    std::vector<StageMarker> stageMarkers;
    std::vector<PlotPoint> spectrum;
    std::vector<SpectrumPeak> spectrumPeaks;
    std::vector<SpectrumPeak> harmonicPeaks;
    std::vector<PlotPoint> envelope;
    std::vector<PlotPoint> energyDecay;
    std::vector<PlotPoint> dynamicsRms;
    std::vector<PlotPoint> groupDelay;
    juce::Image spectrogramBlue;
    juce::Image spectrogramYellow;
    juce::Image spectrogramPink;
    SpatialHeatmapAnalysis spatialHeatmap;
};

inline float safeGainToDb(float gain)
{
    return juce::Decibels::gainToDecibels(juce::jmax(0.0f, gain), -120.0f);
}

inline float safeDbToGain(double db)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(juce::Decibels::decibelsToGain(db)));
}

inline juce::String normaliseGeneratedType(juce::String type)
{
    type = type.trim().toLowerCase().replaceCharacter('-', '_').replaceCharacter(' ', '_');
    if (type == "harmonic" || type == "harmonics")
        return "harmonic_series";
    if (type == "noise")
        return "white_noise";
    return type.isNotEmpty() ? type : "impulse";
}

inline GeneratedSignalSpec normaliseGeneratedSignalSpec(GeneratedSignalSpec spec)
{
    spec.type = normaliseGeneratedType(spec.type);
    spec.preset = spec.preset.trim().isNotEmpty() ? spec.preset.trim() : "thesis_default";
    spec.sampleRate = juce::jlimit(8000.0, 384000.0, spec.sampleRate);
    spec.channels = juce::jlimit(1, 2, spec.channels);
    spec.seconds = juce::jlimit(0.02, 120.0, spec.seconds);
    spec.levelDb = juce::jlimit(-90.0, 0.0, spec.levelDb);
    spec.frequencyHz = juce::jlimit(10.0, spec.sampleRate * 0.45, spec.frequencyHz);
    spec.startHz = juce::jlimit(1.0, spec.sampleRate * 0.45, spec.startHz);
    spec.endHz = juce::jlimit(1.0, spec.sampleRate * 0.49, spec.endHz);
    if (spec.endHz <= spec.startHz)
        spec.endHz = juce::jmin(spec.sampleRate * 0.49, spec.startHz + 1.0);
    spec.harmonicCount = juce::jlimit(1, 64, spec.harmonicCount);
    spec.harmonicRolloffDb = juce::jlimit(0.0, 48.0, spec.harmonicRolloffDb);
    spec.noiseAmount = juce::jlimit(0.0, 1.0, spec.noiseAmount);
    spec.modDepth = juce::jlimit(0.0, 1.0, spec.modDepth);
    spec.curve = juce::jlimit(0.2, 6.0, spec.curve);
    spec.crackAmount = juce::jlimit(0.0, 1.0, spec.crackAmount);
    spec.stereoWidth = juce::jlimit(0.0, 1.0, spec.stereoWidth);
    spec.overlapAmount = juce::jlimit(0.0, 1.0, spec.overlapAmount);
    return spec;
}

inline juce::String generatedSignalSpecHashInputVersion()
{
    return "generatedSignalSpec.v1.normalizedJson.fnv1a64";
}

inline juce::String stableJsonNumber(double value)
{
    if (std::abs(value) < 0.0000005)
        value = 0.0;
    return juce::String(value, 6);
}

inline juce::var writeGeneratedSignalSpecJson(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("type", spec.type);
    root->setProperty("preset", spec.preset);
    root->setProperty("sampleRate", spec.sampleRate);
    root->setProperty("channels", spec.channels);
    root->setProperty("seconds", spec.seconds);
    root->setProperty("levelDb", spec.levelDb);
    root->setProperty("seed", spec.seed);

    auto params = std::make_unique<juce::DynamicObject>();
    params->setProperty("frequencyHz", spec.frequencyHz);
    params->setProperty("startHz", spec.startHz);
    params->setProperty("endHz", spec.endHz);
    params->setProperty("phaseDegrees", spec.phaseDegrees);
    params->setProperty("harmonicCount", spec.harmonicCount);
    params->setProperty("harmonicRolloffDb", spec.harmonicRolloffDb);
    params->setProperty("invert", spec.invert);
    params->setProperty("noiseAmount", spec.noiseAmount);
    params->setProperty("modRateHz", spec.modRateHz);
    params->setProperty("modDepth", spec.modDepth);
    params->setProperty("curve", spec.curve);
    params->setProperty("bodyHz", spec.bodyHz);
    params->setProperty("bodyDecayMs", spec.bodyDecayMs);
    params->setProperty("crackHz", spec.crackHz);
    params->setProperty("crackAmount", spec.crackAmount);
    params->setProperty("noiseBandLowHz", spec.noiseBandLowHz);
    params->setProperty("noiseBandHighHz", spec.noiseBandHighHz);
    params->setProperty("transientMs", spec.transientMs);
    params->setProperty("fundamentalHz", spec.fundamentalHz);
    params->setProperty("decayMs", spec.decayMs);
    params->setProperty("tailBandLowHz", spec.tailBandLowHz);
    params->setProperty("tailBandHighHz", spec.tailBandHighHz);
    params->setProperty("damping", spec.damping);
    params->setProperty("earlyReflectionAmount", spec.earlyReflectionAmount);
    params->setProperty("stereoWidth", spec.stereoWidth);
    params->setProperty("chargeStartSec", spec.chargeStartSec);
    params->setProperty("shotTimeSec", spec.shotTimeSec);
    params->setProperty("tailStartSec", spec.tailStartSec);
    params->setProperty("stageBalance", spec.stageBalance);
    params->setProperty("fundamentalAHz", spec.fundamentalAHz);
    params->setProperty("fundamentalBHz", spec.fundamentalBHz);
    params->setProperty("detuneCents", spec.detuneCents);
    params->setProperty("overlapAmount", spec.overlapAmount);
    root->setProperty("params", juce::var(params.release()));
    return juce::var(root.release());
}

inline juce::String generatedSignalSpecStableJson(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    juce::String json;
    auto addString = [&](const juce::String& name, const juce::String& value)
    {
        if (json.isNotEmpty())
            json += ",";
        json += "\"" + name + "\":\"" + value.replace("\"", "\\\"") + "\"";
    };
    auto addNumber = [&](const juce::String& name, double value)
    {
        if (json.isNotEmpty())
            json += ",";
        json += "\"" + name + "\":" + stableJsonNumber(value);
    };
    auto addInt = [&](const juce::String& name, int value)
    {
        if (json.isNotEmpty())
            json += ",";
        json += "\"" + name + "\":" + juce::String(value);
    };
    auto addBool = [&](const juce::String& name, bool value)
    {
        if (json.isNotEmpty())
            json += ",";
        json += "\"" + name + "\":" + juce::String(value ? "true" : "false");
    };

    addString("type", spec.type);
    addString("preset", spec.preset);
    addNumber("sampleRate", spec.sampleRate);
    addInt("channels", spec.channels);
    addNumber("seconds", spec.seconds);
    addNumber("levelDb", spec.levelDb);
    addInt("seed", spec.seed);
    addNumber("frequencyHz", spec.frequencyHz);
    addNumber("startHz", spec.startHz);
    addNumber("endHz", spec.endHz);
    addNumber("phaseDegrees", spec.phaseDegrees);
    addInt("harmonicCount", spec.harmonicCount);
    addNumber("harmonicRolloffDb", spec.harmonicRolloffDb);
    addBool("invert", spec.invert);
    addNumber("noiseAmount", spec.noiseAmount);
    addNumber("modRateHz", spec.modRateHz);
    addNumber("modDepth", spec.modDepth);
    addNumber("curve", spec.curve);
    addNumber("bodyHz", spec.bodyHz);
    addNumber("bodyDecayMs", spec.bodyDecayMs);
    addNumber("crackHz", spec.crackHz);
    addNumber("crackAmount", spec.crackAmount);
    addNumber("noiseBandLowHz", spec.noiseBandLowHz);
    addNumber("noiseBandHighHz", spec.noiseBandHighHz);
    addNumber("transientMs", spec.transientMs);
    addNumber("fundamentalHz", spec.fundamentalHz);
    addNumber("decayMs", spec.decayMs);
    addNumber("tailBandLowHz", spec.tailBandLowHz);
    addNumber("tailBandHighHz", spec.tailBandHighHz);
    addNumber("damping", spec.damping);
    addNumber("earlyReflectionAmount", spec.earlyReflectionAmount);
    addNumber("stereoWidth", spec.stereoWidth);
    addNumber("chargeStartSec", spec.chargeStartSec);
    addNumber("shotTimeSec", spec.shotTimeSec);
    addNumber("tailStartSec", spec.tailStartSec);
    addNumber("stageBalance", spec.stageBalance);
    addNumber("fundamentalAHz", spec.fundamentalAHz);
    addNumber("fundamentalBHz", spec.fundamentalBHz);
    addNumber("detuneCents", spec.detuneCents);
    addNumber("overlapAmount", spec.overlapAmount);
    return "{" + json + "}";
}

inline uint64_t fnv1a64Bytes(const void* data, size_t size, uint64_t seed = 1469598103934665603ULL)
{
    auto hash = seed;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline juce::String hashGeneratedSignalSpec(const GeneratedSignalSpec& spec)
{
    const auto stable = generatedSignalSpecStableJson(spec);
    const auto hash = fnv1a64Bytes(stable.toRawUTF8(), static_cast<size_t>(stable.getNumBytesAsUTF8()));
    return "generated-fnv1a64:" + juce::String::toHexString(static_cast<juce::int64>(hash));
}

inline juce::var writeStageMarkersJson(const std::vector<StageMarker>& markers)
{
    juce::Array<juce::var> array;
    for (const auto& marker : markers)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("label", marker.label);
        obj->setProperty("startSec", marker.startSec);
        obj->setProperty("endSec", marker.endSec);
        array.add(juce::var(obj.release()));
    }
    return juce::var(array);
}

inline juce::var writeAnalysisSummaryJson(const Metrics& metrics)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("sampleRate", metrics.sampleRate);
    obj->setProperty("channels", metrics.channels);
    obj->setProperty("durationSeconds", metrics.durationSeconds);
    obj->setProperty("peakDb", metrics.peakDb);
    obj->setProperty("rmsDb", metrics.rmsDb);
    obj->setProperty("crestDb", metrics.crestDb);
    return juce::var(obj.release());
}

inline juce::AudioBuffer<float> toStereoBuffer(const juce::AudioBuffer<float>& input)
{
    const int samples = input.getNumSamples();
    juce::AudioBuffer<float> result(2, samples);
    result.clear();

    if (samples <= 0 || input.getNumChannels() <= 0)
        return result;

    if (input.getNumChannels() == 1)
    {
        result.copyFrom(0, 0, input, 0, 0, samples);
        result.copyFrom(1, 0, input, 0, 0, samples);
    }
    else
    {
        result.copyFrom(0, 0, input, 0, 0, samples);
        result.copyFrom(1, 0, input, 1, 0, samples);
    }

    return result;
}

inline juce::AudioBuffer<float> mixToMono(const juce::AudioBuffer<float>& input)
{
    juce::AudioBuffer<float> mono(1, input.getNumSamples());
    mono.clear();

    const int channels = input.getNumChannels();
    const int samples = input.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return mono;

    for (int ch = 0; ch < channels; ++ch)
        mono.addFrom(0, 0, input, ch, 0, samples, 1.0f / static_cast<float>(channels));

    return mono;
}

inline Metrics computeMetrics(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    Metrics m;
    m.sampleRate = sampleRate;
    m.channels = buffer.getNumChannels();
    m.samples = buffer.getNumSamples();
    m.durationSeconds = sampleRate > 0.0 ? static_cast<double>(m.samples) / sampleRate : 0.0;

    double sumSquares = 0.0;
    juce::int64 count = 0;
    float peak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float v = data[i];
            peak = juce::jmax(peak, std::abs(v));
            sumSquares += static_cast<double>(v) * static_cast<double>(v);
            ++count;
        }
    }

    const float rms = count > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count))) : 0.0f;
    m.peakDb = safeGainToDb(peak);
    m.rmsDb = safeGainToDb(rms);
    m.crestDb = m.peakDb - m.rmsDb;
    return m;
}

inline std::vector<PlotPoint> computeEnvelope(const juce::AudioBuffer<float>& buffer,
                                              double sampleRate,
                                              int maxPoints = 1200)
{
    std::vector<PlotPoint> points;
    const int samples = buffer.getNumSamples();
    if (samples <= 0 || sampleRate <= 0.0)
        return points;

    auto mono = mixToMono(buffer);
    const auto* data = mono.getReadPointer(0);
    const int pointCount = juce::jmin(maxPoints, samples);
    points.reserve(static_cast<size_t>(pointCount));

    for (int p = 0; p < pointCount; ++p)
    {
        const int start = p * samples / pointCount;
        const int end = (p + 1) * samples / pointCount;
        float peak = 0.0f;

        for (int i = start; i < end; ++i)
            peak = juce::jmax(peak, std::abs(data[i]));

        points.push_back({ static_cast<float>(start / sampleRate), safeGainToDb(peak) });
    }

    return points;
}

inline int findOnsetSample(const juce::AudioBuffer<float>& buffer, float thresholdDbBelowPeak = 36.0f)
{
    const int samples = buffer.getNumSamples();
    if (samples <= 0 || buffer.getNumChannels() <= 0)
        return 0;

    auto mono = mixToMono(buffer);
    const auto* data = mono.getReadPointer(0);
    float peak = 0.0f;
    for (int i = 0; i < samples; ++i)
        peak = juce::jmax(peak, std::abs(data[i]));

    const float threshold = peak * juce::Decibels::decibelsToGain(-thresholdDbBelowPeak);
    if (threshold <= 0.0f)
        return 0;

    for (int i = 0; i < samples; ++i)
        if (std::abs(data[i]) >= threshold)
            return i;

    return 0;
}

inline std::vector<PlotPoint> computeEnergyDecayCurve(const juce::AudioBuffer<float>& buffer,
                                                      double sampleRate,
                                                      int maxPoints = 1200)
{
    std::vector<PlotPoint> points;
    const int samples = buffer.getNumSamples();
    if (samples <= 0 || sampleRate <= 0.0)
        return points;

    auto mono = mixToMono(buffer);
    const auto* data = mono.getReadPointer(0);
    const int onset = findOnsetSample(buffer);
    if (onset >= samples)
        return points;

    std::vector<double> cumulative(static_cast<size_t>(samples - onset), 0.0);
    double running = 0.0;
    for (int i = samples - 1; i >= onset; --i)
    {
        running += static_cast<double>(data[i]) * static_cast<double>(data[i]);
        cumulative[static_cast<size_t>(i - onset)] = running;
    }

    const double reference = juce::jmax(1.0e-18, cumulative.front());
    const int pointCount = juce::jmin(maxPoints, samples - onset);
    points.reserve(static_cast<size_t>(pointCount));

    for (int p = 0; p < pointCount; ++p)
    {
        const int index = p * (samples - onset - 1) / juce::jmax(1, pointCount - 1);
        const double norm = juce::jmax(1.0e-18, cumulative[static_cast<size_t>(index)] / reference);
        points.push_back({ static_cast<float>(index / sampleRate),
                           juce::jlimit(-100.0f, 0.0f, static_cast<float>(10.0 * std::log10(norm))) });
    }

    return points;
}

inline std::vector<PlotPoint> computeRmsEnvelope(const juce::AudioBuffer<float>& buffer,
                                                double sampleRate,
                                                double windowMs = 50.0,
                                                int maxPoints = 1200)
{
    std::vector<PlotPoint> points;
    const int samples = buffer.getNumSamples();
    if (samples <= 0 || sampleRate <= 0.0)
        return points;

    auto mono = mixToMono(buffer);
    const auto* data = mono.getReadPointer(0);
    const int window = juce::jmax(16, static_cast<int>(sampleRate * windowMs / 1000.0));
    const int pointCount = juce::jmin(maxPoints, samples);
    points.reserve(static_cast<size_t>(pointCount));

    for (int p = 0; p < pointCount; ++p)
    {
        const int centre = p * samples / pointCount;
        const int start = juce::jlimit(0, samples, centre - window / 2);
        const int end = juce::jlimit(start + 1, samples, centre + window / 2);
        double sumSquares = 0.0;
        for (int i = start; i < end; ++i)
            sumSquares += static_cast<double>(data[i]) * static_cast<double>(data[i]);

        const float rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(end - start)));
        points.push_back({ static_cast<float>(centre / sampleRate), safeGainToDb(rms) });
    }

    return points;
}

inline double windowEnergy(const juce::AudioBuffer<float>& mono, int start, int end)
{
    start = juce::jlimit(0, mono.getNumSamples(), start);
    end = juce::jlimit(start, mono.getNumSamples(), end);
    const auto* data = mono.getReadPointer(0);
    double energy = 0.0;
    for (int i = start; i < end; ++i)
        energy += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    return energy;
}

inline ReverbSpaceMetrics computeReverbSpaceMetrics(const juce::AudioBuffer<float>& buffer,
                                                    double sampleRate,
                                                    const std::vector<PlotPoint>& edc)
{
    ReverbSpaceMetrics metrics;
    const int samples = buffer.getNumSamples();
    if (samples <= 0 || sampleRate <= 0.0)
        return metrics;

    auto mono = mixToMono(buffer);
    const int onset = findOnsetSample(buffer);
    const int directEnd = onset + static_cast<int>(sampleRate * 0.020);
    const int earlyEnd = onset + static_cast<int>(sampleRate * 0.080);

    const double directEnergy = windowEnergy(mono, onset, directEnd);
    const double earlyEnergy = windowEnergy(mono, directEnd, earlyEnd);
    const double lateEnergy = windowEnergy(mono, earlyEnd, samples);
    const double eps = 1.0e-18;

    metrics.valid = true;
    metrics.onsetSeconds = static_cast<float>(static_cast<double>(onset) / sampleRate);
    metrics.directEnergyDb = safeGainToDb(static_cast<float>(std::sqrt(directEnergy / juce::jmax(1.0, static_cast<double>(directEnd - onset)))));
    metrics.earlyEnergyDb = safeGainToDb(static_cast<float>(std::sqrt(earlyEnergy / juce::jmax(1.0, static_cast<double>(earlyEnd - directEnd)))));
    metrics.lateEnergyDb = safeGainToDb(static_cast<float>(std::sqrt(lateEnergy / juce::jmax(1.0, static_cast<double>(samples - earlyEnd)))));
    metrics.drrDb = static_cast<float>(10.0 * std::log10((directEnergy + eps) / (earlyEnergy + lateEnergy + eps)));
    metrics.earlyLateDb = static_cast<float>(10.0 * std::log10((directEnergy + earlyEnergy + eps) / (lateEnergy + eps)));

    metrics.tailEndSeconds = static_cast<float>(static_cast<double>(samples - onset) / sampleRate);
    for (const auto& p : edc)
    {
        if (p.y <= -60.0f)
        {
            metrics.tailEndSeconds = p.x;
            break;
        }
    }

    auto estimateDecay = [&](float topDb, float bottomDb, float extrapolateDb)
    {
        double sumX = 0.0;
        double sumY = 0.0;
        double sumXX = 0.0;
        double sumXY = 0.0;
        int count = 0;
        for (const auto& p : edc)
        {
            if (p.y <= topDb && p.y >= bottomDb)
            {
                sumX += p.x;
                sumY += p.y;
                sumXX += static_cast<double>(p.x) * static_cast<double>(p.x);
                sumXY += static_cast<double>(p.x) * static_cast<double>(p.y);
                ++count;
            }
        }

        const double denom = static_cast<double>(count) * sumXX - sumX * sumX;
        if (count < 4 || std::abs(denom) <= 1.0e-9)
            return 0.0f;

        const double slopeDbPerSecond = (static_cast<double>(count) * sumXY - sumX * sumY) / denom;
        return slopeDbPerSecond < -0.001 ? static_cast<float>(-static_cast<double>(extrapolateDb) / slopeDbPerSecond)
                                         : 0.0f;
    };

    metrics.rt20Seconds = estimateDecay(-5.0f, -25.0f, 20.0f);
    metrics.rt30Seconds = estimateDecay(-5.0f, -35.0f, 30.0f);
    metrics.rt60Seconds = metrics.rt30Seconds > 0.0f ? metrics.rt30Seconds * 2.0f
                        : metrics.rt20Seconds > 0.0f ? metrics.rt20Seconds * 3.0f
                                                      : 0.0f;

    if (buffer.getNumChannels() >= 2)
    {
        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(1);
        double lr = 0.0;
        double ll = 0.0;
        double rr = 0.0;
        double mid = 0.0;
        double side = 0.0;
        for (int i = 0; i < samples; ++i)
        {
            const double l = left[i];
            const double r = right[i];
            lr += l * r;
            ll += l * l;
            rr += r * r;
            const double m = (l + r) * 0.5;
            const double s = (l - r) * 0.5;
            mid += m * m;
            side += s * s;
        }

        metrics.stereoCorrelation = static_cast<float>(lr / std::sqrt((ll + eps) * (rr + eps)));
        metrics.sideToMidDb = static_cast<float>(10.0 * std::log10((side + eps) / (mid + eps)));
    }

    return metrics;
}

inline DynamicsMetrics computeDynamicsMetrics(const juce::AudioBuffer<float>& buffer,
                                              double sampleRate,
                                              const std::vector<PlotPoint>& rmsEnvelope)
{
    DynamicsMetrics metrics;
    if (buffer.getNumSamples() <= 0 || sampleRate <= 0.0 || rmsEnvelope.empty())
        return metrics;

    std::vector<float> values;
    values.reserve(rmsEnvelope.size());
    for (const auto& p : rmsEnvelope)
        if (p.y > -119.0f)
            values.push_back(p.y);

    if (values.empty())
        return metrics;

    std::sort(values.begin(), values.end());
    auto percentile = [&](float p)
    {
        const int index = juce::jlimit(0, static_cast<int>(values.size()) - 1,
                                       static_cast<int>(std::round(p * static_cast<float>(values.size() - 1))));
        return values[static_cast<size_t>(index)];
    };

    auto mono = mixToMono(buffer);
    const int onset = findOnsetSample(buffer);
    const int transientEnd = onset + static_cast<int>(sampleRate * 0.050);
    const int sustainEnd = onset + static_cast<int>(sampleRate * 0.500);
    const double transientEnergy = windowEnergy(mono, onset, transientEnd);
    const double sustainEnergy = windowEnergy(mono, transientEnd, sustainEnd);
    const double transientRms = std::sqrt(transientEnergy / juce::jmax(1.0, static_cast<double>(transientEnd - onset)));
    const double sustainRms = std::sqrt(sustainEnergy / juce::jmax(1.0, static_cast<double>(sustainEnd - transientEnd)));

    metrics.valid = true;
    metrics.onsetSeconds = static_cast<float>(static_cast<double>(onset) / sampleRate);
    metrics.rmsP10Db = percentile(0.10f);
    metrics.rmsP50Db = percentile(0.50f);
    metrics.rmsP90Db = percentile(0.90f);
    metrics.rmsRangeDb = metrics.rmsP90Db - metrics.rmsP10Db;
    metrics.transientToSustainDb = static_cast<float>(20.0 * std::log10((transientRms + 1.0e-12) / (sustainRms + 1.0e-12)));
    return metrics;
}

inline bool interpolatePlotYAt(const std::vector<PlotPoint>& points, float x, float& y)
{
    if (points.empty())
        return false;

    if (x < points.front().x || x > points.back().x)
        return false;

    auto it = std::lower_bound(points.begin(), points.end(), x,
                               [](const PlotPoint& p, float value) { return p.x < value; });

    if (it == points.begin())
    {
        y = it->y;
        return true;
    }

    if (it == points.end())
    {
        y = points.back().y;
        return true;
    }

    const auto& right = *it;
    const auto& left = *(it - 1);
    const float width = right.x - left.x;
    if (std::abs(width) <= std::numeric_limits<float>::epsilon())
    {
        y = right.y;
        return true;
    }

    const float t = juce::jlimit(0.0f, 1.0f, (x - left.x) / width);
    y = left.y + (right.y - left.y) * t;
    return true;
}

inline std::vector<PlotPoint> computeApparentAttenuationCurve(const std::vector<PlotPoint>& referenceRms,
                                                             const std::vector<PlotPoint>& targetRms,
                                                             float ignoreBothBelowDb = -78.0f)
{
    std::vector<PlotPoint> curve;
    if (referenceRms.empty() || targetRms.empty())
        return curve;

    curve.reserve(targetRms.size());
    for (const auto& p : targetRms)
    {
        float referenceDb = 0.0f;
        if (!interpolatePlotYAt(referenceRms, p.x, referenceDb))
            continue;

        if (referenceDb <= ignoreBothBelowDb && p.y <= ignoreBothBelowDb)
            continue;

        curve.push_back({ p.x, p.y - referenceDb });
    }

    return curve;
}

inline ApparentAttenuationStats computeApparentAttenuationStats(const std::vector<PlotPoint>& curve)
{
    ApparentAttenuationStats stats;
    if (curve.empty())
        return stats;

    double sum = 0.0;
    double sumAbs = 0.0;
    for (const auto& p : curve)
    {
        if (!std::isfinite(p.y))
            continue;

        sum += p.y;
        sumAbs += std::abs(p.y);

        const float reduction = -p.y;
        if (reduction > stats.maxReductionDb)
        {
            stats.maxReductionDb = reduction;
            stats.peakReductionSeconds = p.x;
        }

        if (p.y > stats.maxExpansionDb)
        {
            stats.maxExpansionDb = p.y;
            stats.peakExpansionSeconds = p.x;
        }

        ++stats.pointCount;
    }

    if (stats.pointCount <= 0)
        return stats;

    stats.valid = true;
    stats.meanDeltaDb = static_cast<float>(sum / static_cast<double>(stats.pointCount));
    stats.meanAbsDeltaDb = static_cast<float>(sumAbs / static_cast<double>(stats.pointCount));
    return stats;
}

inline ApparentAttenuationStats computeApparentAttenuationStats(const std::vector<PlotPoint>& referenceRms,
                                                               const std::vector<PlotPoint>& targetRms)
{
    return computeApparentAttenuationStats(computeApparentAttenuationCurve(referenceRms, targetRms));
}

inline juce::var writeApparentAttenuationStatsJson(const ApparentAttenuationStats& stats)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("valid", stats.valid);
    obj->setProperty("method", "target RMS envelope minus render-reference RMS envelope; negative delta means apparent attenuation, not plugin-internal gain reduction");
    obj->setProperty("pointCount", stats.pointCount);
    obj->setProperty("meanDeltaDb", stats.meanDeltaDb);
    obj->setProperty("meanAbsDeltaDb", stats.meanAbsDeltaDb);
    obj->setProperty("maxReductionDb", stats.maxReductionDb);
    obj->setProperty("maxExpansionDb", stats.maxExpansionDb);
    obj->setProperty("peakReductionSeconds", stats.peakReductionSeconds);
    obj->setProperty("peakExpansionSeconds", stats.peakExpansionSeconds);
    return juce::var(obj.release());
}

inline std::vector<PlotPoint> computeAverageSpectrum(const juce::AudioBuffer<float>& buffer,
                                                     double sampleRate,
                                                     int fftOrder = 14,
                                                     int maxFrames = 512)
{
    std::vector<PlotPoint> points;
    const int samples = buffer.getNumSamples();

    while (fftOrder > 11 && samples < (1 << fftOrder))
        --fftOrder;

    const int fftSize = 1 << fftOrder;
    const int halfSize = fftSize / 2;

    if (sampleRate <= 0.0 || samples < fftSize)
        return points;

    auto mono = mixToMono(buffer);
    const auto* data = mono.getReadPointer(0);

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);

    const int hop = fftSize / 2;
    const int availableFrames = juce::jmax(1, (samples - fftSize) / hop + 1);
    const int frames = juce::jmin(maxFrames, availableFrames);
    std::vector<double> magnitudes(static_cast<size_t>(halfSize), 0.0);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);

    for (int frame = 0; frame < frames; ++frame)
    {
        const int sourceFrame = frame * availableFrames / frames;
        const int pos = sourceFrame * hop;
        std::fill(fftData.begin(), fftData.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
            fftData[static_cast<size_t>(i)] = data[pos + i];

        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        for (int bin = 1; bin < halfSize; ++bin)
            magnitudes[static_cast<size_t>(bin)] += static_cast<double>(fftData[static_cast<size_t>(bin)]);
    }

    std::vector<float> binDb(static_cast<size_t>(halfSize), -120.0f);
    for (int bin = 1; bin < halfSize; ++bin)
    {
        const float avgMag = static_cast<float>(magnitudes[static_cast<size_t>(bin)] / static_cast<double>(frames));
        binDb[static_cast<size_t>(bin)] = safeGainToDb(avgMag / static_cast<float>(fftSize)) + 54.0f;
    }

    auto interpolateBinDb = [&](double exactBin)
    {
        exactBin = juce::jlimit(1.0, static_cast<double>(halfSize - 1), exactBin);
        const int bin0 = juce::jlimit(1, halfSize - 1, static_cast<int>(std::floor(exactBin)));
        const int bin1 = juce::jlimit(1, halfSize - 1, bin0 + 1);
        const float frac = static_cast<float>(exactBin - static_cast<double>(bin0));
        return juce::jmap(frac, binDb[static_cast<size_t>(bin0)], binDb[static_cast<size_t>(bin1)]);
    };

    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;
    constexpr int pointCount = 2200;
    const double logMin = std::log10(minFreq);
    const double logMax = std::log10(maxFreq);
    const double logStep = (logMax - logMin) / static_cast<double>(pointCount - 1);
    points.reserve(static_cast<size_t>(pointCount));

    for (int i = 0; i < pointCount; ++i)
    {
        const double centreLog = logMin + static_cast<double>(i) * logStep;
        const float freq = static_cast<float>(std::pow(10.0, centreLog));
        const double exactBin = static_cast<double>(freq) * static_cast<double>(fftSize) / sampleRate;

        const double smoothingBins = juce::jlimit(0.0, 18.0, exactBin * 0.006);
        if (smoothingBins < 1.0)
        {
            points.push_back({ freq, interpolateBinDb(exactBin) });
            continue;
        }

        const int taps = juce::jlimit(1, 9, static_cast<int>(std::ceil(smoothingBins)));
        double weightedSum = 0.0;
        double weightTotal = 0.0;
        for (int offset = -taps; offset <= taps; ++offset)
        {
            const double distance = std::abs(static_cast<double>(offset)) / (smoothingBins + 0.0001);
            const double weight = juce::jmax(0.0, 1.0 - distance);
            weightedSum += static_cast<double>(interpolateBinDb(exactBin + static_cast<double>(offset))) * weight;
            weightTotal += weight;
        }

        points.push_back({ freq, static_cast<float>(weightedSum / juce::jmax(0.0001, weightTotal)) });
    }

    return points;
}

inline float centsBetween(float frequencyHz, float expectedHz)
{
    if (frequencyHz <= 0.0f || expectedHz <= 0.0f)
        return 0.0f;

    return static_cast<float>(1200.0 * std::log2(static_cast<double>(frequencyHz) / static_cast<double>(expectedHz)));
}

inline float estimateFundamentalHz(const std::vector<SpectrumPeak>& peaks)
{
    if (peaks.empty())
        return 0.0f;

    float strongestDb = -120.0f;
    for (const auto& peak : peaks)
        strongestDb = juce::jmax(strongestDb, peak.magnitudeDb);

    float bestFundamental = 0.0f;
    float bestScore = 0.0f;
    for (const auto& candidate : peaks)
    {
        const float f0 = candidate.frequencyHz;
        if (f0 < 40.0f || f0 > 1200.0f || candidate.magnitudeDb < strongestDb - 42.0f)
            continue;

        float score = 0.0f;
        int matches = 0;
        for (int harmonic = 1; harmonic <= 18; ++harmonic)
        {
            const float expected = f0 * static_cast<float>(harmonic);
            if (expected > 20000.0f)
                break;

            float bestDelta = 9999.0f;
            float bestDb = -120.0f;
            for (const auto& peak : peaks)
            {
                const float delta = std::abs(centsBetween(peak.frequencyHz, expected));
                if (delta < bestDelta)
                {
                    bestDelta = delta;
                    bestDb = peak.magnitudeDb;
                }
            }

            if (bestDelta <= 38.0f)
            {
                ++matches;
                const float amplitudeScore = juce::jlimit(0.0f, 1.0f, (bestDb - (strongestDb - 54.0f)) / 54.0f);
                score += (1.0f + amplitudeScore) / std::sqrt(static_cast<float>(harmonic));
            }
        }

        if (matches >= 2)
            score += 0.18f / juce::jmax(1.0f, f0 / 220.0f);

        if (score > bestScore)
        {
            bestScore = score;
            bestFundamental = f0;
        }
    }

    if (bestFundamental > 0.0f)
        return bestFundamental;

    for (const auto& peak : peaks)
        if (peak.frequencyHz >= 40.0f && peak.frequencyHz <= 1200.0f && peak.magnitudeDb >= strongestDb - 36.0f)
            return peak.frequencyHz;

    return peaks.front().frequencyHz;
}

inline std::vector<SpectrumPeak> computeSpectrumPeaks(const std::vector<PlotPoint>& spectrum,
                                                      int maxPeaks = 48)
{
    std::vector<SpectrumPeak> peaks;
    if (spectrum.size() < 5)
        return peaks;

    float maxDb = -120.0f;
    for (const auto& p : spectrum)
        maxDb = juce::jmax(maxDb, p.y);

    std::vector<SpectrumPeak> candidates;
    for (size_t i = 2; i + 2 < spectrum.size(); ++i)
    {
        const auto& p = spectrum[i];
        if (p.x < 30.0f || p.x > 20000.0f || p.y < maxDb - 54.0f || p.y < -96.0f)
            continue;

        if (p.y <= spectrum[i - 1].y || p.y <= spectrum[i + 1].y)
            continue;

        const size_t left = i > 8 ? i - 8 : 0;
        const size_t right = juce::jmin(spectrum.size() - 1, i + 8);
        float shoulder = -120.0f;
        shoulder = juce::jmax(shoulder, spectrum[left].y);
        shoulder = juce::jmax(shoulder, spectrum[right].y);
        if (p.y - shoulder < 0.9f)
            continue;

        candidates.push_back({ p.x, p.y, 0, 0.0f, 0.0f, false });
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b)
    {
        return a.magnitudeDb > b.magnitudeDb;
    });

    for (const auto& candidate : candidates)
    {
        bool tooClose = false;
        for (const auto& accepted : peaks)
        {
            if (std::abs(centsBetween(candidate.frequencyHz, accepted.frequencyHz)) < 32.0f)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            peaks.push_back(candidate);

        if (static_cast<int>(peaks.size()) >= maxPeaks)
            break;
    }

    std::sort(peaks.begin(), peaks.end(), [](const auto& a, const auto& b)
    {
        return a.frequencyHz < b.frequencyHz;
    });

    const float fundamental = estimateFundamentalHz(peaks);
    if (fundamental <= 0.0f)
        return peaks;

    for (auto& peak : peaks)
    {
        const int harmonic = juce::jlimit(1, 128, static_cast<int>(std::round(peak.frequencyHz / fundamental)));
        const float expected = fundamental * static_cast<float>(harmonic);
        const float delta = centsBetween(peak.frequencyHz, expected);
        peak.harmonicNumber = harmonic;
        peak.expectedHz = expected;
        peak.deltaCents = delta;
        peak.nearHarmonic = std::abs(delta) <= 38.0f;
    }

    return peaks;
}

inline std::vector<SpectrumPeak> selectHarmonicPeaks(const std::vector<SpectrumPeak>& peaks,
                                                     int maxHarmonics = 18)
{
    std::vector<SpectrumPeak> harmonics;
    for (const auto& peak : peaks)
    {
        if (!peak.nearHarmonic || peak.harmonicNumber <= 0)
            continue;

        auto existing = std::find_if(harmonics.begin(), harmonics.end(), [&](const auto& p)
        {
            return p.harmonicNumber == peak.harmonicNumber;
        });

        if (existing == harmonics.end())
            harmonics.push_back(peak);
        else if (peak.magnitudeDb > existing->magnitudeDb)
            *existing = peak;
    }

    std::sort(harmonics.begin(), harmonics.end(), [](const auto& a, const auto& b)
    {
        return a.harmonicNumber < b.harmonicNumber;
    });

    if (static_cast<int>(harmonics.size()) > maxHarmonics)
        harmonics.resize(static_cast<size_t>(maxHarmonics));

    return harmonics;
}

inline std::vector<PlotPoint> computeTransferGroupDelay(const juce::AudioBuffer<float>& dryBuffer,
                                                        const juce::AudioBuffer<float>& wetBuffer,
                                                        double sampleRate,
                                                        int fftOrder = 14,
                                                        int maxFrames = 96)
{
    std::vector<PlotPoint> points;

    const int fftSize = 1 << fftOrder;
    const int halfSize = fftSize / 2;
    const int samples = juce::jmin(dryBuffer.getNumSamples(), wetBuffer.getNumSamples());
    if (sampleRate <= 0.0 || samples < fftSize)
        return points;

    auto dryMono = mixToMono(dryBuffer);
    auto wetMono = mixToMono(wetBuffer);
    const auto* dry = dryMono.getReadPointer(0);
    const auto* wet = wetMono.getReadPointer(0);

    juce::dsp::FFT fft(fftOrder);
    std::vector<juce::dsp::Complex<float>> dryIn(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> wetIn(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> dryOut(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> wetOut(static_cast<size_t>(fftSize));
    std::vector<std::complex<double>> cross(static_cast<size_t>(halfSize));

    const int hop = fftSize / 2;
    const int availableFrames = juce::jmax(1, (samples - fftSize) / hop + 1);
    const int frames = juce::jmin(maxFrames, availableFrames);

    for (int frame = 0; frame < frames; ++frame)
    {
        const int sourceFrame = frame * availableFrames / frames;
        const int pos = sourceFrame * hop;

        for (int i = 0; i < fftSize; ++i)
        {
            const double phase = juce::MathConstants<double>::twoPi * static_cast<double>(i)
                               / static_cast<double>(fftSize - 1);
            const float win = static_cast<float>(0.5 - 0.5 * std::cos(phase));
            dryIn[static_cast<size_t>(i)] = { dry[pos + i] * win, 0.0f };
            wetIn[static_cast<size_t>(i)] = { wet[pos + i] * win, 0.0f };
        }

        fft.perform(dryIn.data(), dryOut.data(), false);
        fft.perform(wetIn.data(), wetOut.data(), false);

        for (int bin = 1; bin < halfSize; ++bin)
        {
            const auto x = std::complex<double>(dryOut[static_cast<size_t>(bin)].real(),
                                                dryOut[static_cast<size_t>(bin)].imag());
            const auto y = std::complex<double>(wetOut[static_cast<size_t>(bin)].real(),
                                                wetOut[static_cast<size_t>(bin)].imag());
            cross[static_cast<size_t>(bin)] += y * std::conj(x);
        }
    }

    std::vector<double> phases(static_cast<size_t>(halfSize), 0.0);
    double unwrapOffset = 0.0;
    double previousRaw = 0.0;
    bool hasPrevious = false;

    for (int bin = 1; bin < halfSize; ++bin)
    {
        const auto c = cross[static_cast<size_t>(bin)];
        const double raw = std::atan2(c.imag(), c.real());

        if (hasPrevious)
        {
            const double diff = raw - previousRaw;
            if (diff > juce::MathConstants<double>::pi)
                unwrapOffset -= juce::MathConstants<double>::twoPi;
            else if (diff < -juce::MathConstants<double>::pi)
                unwrapOffset += juce::MathConstants<double>::twoPi;
        }

        phases[static_cast<size_t>(bin)] = raw + unwrapOffset;
        previousRaw = raw;
        hasPrevious = true;
    }

    const int stride = juce::jmax(1, halfSize / 900);
    for (int bin = 2; bin < halfSize; bin += stride)
    {
        const double f0 = (static_cast<double>(bin - 1) * sampleRate) / static_cast<double>(fftSize);
        const double f1 = (static_cast<double>(bin) * sampleRate) / static_cast<double>(fftSize);
        const double freq = (f0 + f1) * 0.5;
        if (freq < 20.0 || freq > 20000.0)
            continue;

        const auto c = cross[static_cast<size_t>(bin)];
        if (std::abs(c) < 1.0e-9)
            continue;

        const double dPhase = phases[static_cast<size_t>(bin)] - phases[static_cast<size_t>(bin - 1)];
        const double groupDelayMs = -dPhase / (juce::MathConstants<double>::twoPi * (f1 - f0)) * 1000.0;

        if (std::isfinite(groupDelayMs))
            points.push_back({ static_cast<float>(freq),
                               juce::jlimit(-20.0f, 80.0f, static_cast<float>(groupDelayMs)) });
    }

    return points;
}

enum class SpectrogramPalette
{
    Blue,
    Yellow,
    Pink
};

inline juce::Image computeSpectrogramImage(const juce::AudioBuffer<float>& buffer,
                                           double sampleRate,
                                           SpectrogramPalette palette,
                                           int imageWidth = 2048,
                                           int fftOrder = 10)
{
    const int fftSize = 1 << fftOrder;
    const int halfFFT = fftSize / 2;
    const int samples = buffer.getNumSamples();

    if (sampleRate <= 0.0 || samples < fftSize || imageWidth <= 0)
        return {};

    auto mono = mixToMono(buffer);
    const auto* data = mono.getReadPointer(0);

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    const int hopSize = fftSize / 4;
    const int numFrames = (samples - fftSize) / hopSize + 1;
    const int renderWidth = juce::jmin(imageWidth, numFrames);
    std::vector<std::vector<float>> magnitudes(static_cast<size_t>(renderWidth),
                                               std::vector<float>(static_cast<size_t>(halfFFT), 0.0f));

    float globalMax = 1.0e-10f;
    for (int col = 0; col < renderWidth; ++col)
    {
        const int frameIndex = col * numFrames / renderWidth;
        const int pos = frameIndex * hopSize;
        std::fill(fftData.begin(), fftData.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
            fftData[static_cast<size_t>(i)] = data[pos + i];

        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        for (int bin = 0; bin < halfFFT; ++bin)
        {
            const float mag = fftData[static_cast<size_t>(bin)];
            magnitudes[static_cast<size_t>(col)][static_cast<size_t>(bin)] = mag;
            globalMax = juce::jmax(globalMax, mag);
        }
    }

    const float logMax = std::log10(globalMax + 1.0e-10f);
    juce::Image image(juce::Image::ARGB, renderWidth, halfFFT, true);

    for (int col = 0; col < renderWidth; ++col)
    {
        for (int bin = 0; bin < halfFFT; ++bin)
        {
            const float mag = magnitudes[static_cast<size_t>(col)][static_cast<size_t>(bin)];
            const float logMag = std::log10(mag + 1.0e-10f);
            const float norm = juce::jlimit(0.0f, 1.0f, (logMag - (logMax - 4.0f)) / 4.0f);

            juce::Colour c;
            if (palette == SpectrogramPalette::Yellow)
            {
                if (norm < 0.3f)
                    c = juce::Colour(0xFF0A0A04).interpolatedWith(juce::Colour(0xFF8B6600), norm / 0.3f);
                else if (norm < 0.6f)
                    c = juce::Colour(0xFF8B6600).interpolatedWith(juce::Colour(0xFFFFD700), (norm - 0.3f) / 0.3f);
                else
                    c = juce::Colour(0xFFFFD700).interpolatedWith(juce::Colour(0xFFFFF8E8), (norm - 0.6f) / 0.4f);
            }
            else if (palette == SpectrogramPalette::Pink)
            {
                if (norm < 0.3f)
                    c = juce::Colour(0xFF140812).interpolatedWith(juce::Colour(0xFF9A1E65), norm / 0.3f);
                else if (norm < 0.6f)
                    c = juce::Colour(0xFF9A1E65).interpolatedWith(juce::Colour(0xFFFF3E91), (norm - 0.3f) / 0.3f);
                else
                    c = juce::Colour(0xFFFF3E91).interpolatedWith(juce::Colour(0xFFFFE8F4), (norm - 0.6f) / 0.4f);
            }
            else
            {
                if (norm < 0.3f)
                    c = juce::Colour(0xFF0A0A18).interpolatedWith(juce::Colour(0xFF2244AA), norm / 0.3f);
                else if (norm < 0.6f)
                    c = juce::Colour(0xFF2244AA).interpolatedWith(juce::Colour(0xFF40C8E0), (norm - 0.3f) / 0.3f);
                else
                    c = juce::Colour(0xFF40C8E0).interpolatedWith(juce::Colour(0xFFE8E4F0), (norm - 0.6f) / 0.4f);
            }

            const int y = halfFFT - 1 - bin;
            image.setPixelAt(col, y, c);
        }
    }

    return image;
}

inline juce::Image computeSpectrogramImage(const juce::AudioBuffer<float>& buffer,
                                           double sampleRate,
                                           bool useYellow,
                                           int imageWidth = 2048,
                                           int fftOrder = 10)
{
    return computeSpectrogramImage(buffer,
                                   sampleRate,
                                   useYellow ? SpectrogramPalette::Yellow : SpectrogramPalette::Blue,
                                   imageWidth,
                                   fftOrder);
}

inline float computeSpatialWidthIndex(float sideToMidDb, float correlation, float lrBalanceDb)
{
    const float sideWidth = juce::jlimit(0.0f, 1.0f, (sideToMidDb + 30.0f) / 36.0f);
    const float corrWidth = juce::jlimit(0.0f, 1.0f, (1.0f - correlation) * 0.5f);
    const float balanceWidth = juce::jlimit(0.0f, 1.0f, std::abs(lrBalanceDb) / 18.0f);
    return juce::jlimit(0.0f, 1.0f, sideWidth * 0.72f + corrWidth * 0.20f + balanceWidth * 0.08f);
}

inline juce::Colour spatialHeatmapColour(float widthIndex, float alpha)
{
    const auto narrow = juce::Colour(0xFF22D3EE);
    const auto transition = juce::Colour(0xFFFFD166);
    const auto wide = juce::Colour(0xFFE6335F);
    auto colour = widthIndex < 0.5f
        ? narrow.interpolatedWith(transition, widthIndex * 2.0f)
        : transition.interpolatedWith(wide, (widthIndex - 0.5f) * 2.0f);
    return colour.withAlpha(juce::jlimit(0.0f, 1.0f, alpha));
}

inline SpatialHeatmapAnalysis computeSpatialHeatmapAnalysis(const juce::AudioBuffer<float>& buffer,
                                                            double sampleRate,
                                                            int imageWidth = 1536,
                                                            int fftOrder = 10)
{
    SpatialHeatmapAnalysis result;

    const int fftSize = 1 << fftOrder;
    const int halfFFT = fftSize / 2;
    const int samples = buffer.getNumSamples();
    if (sampleRate <= 0.0 || samples < fftSize || imageWidth <= 0 || buffer.getNumChannels() <= 0)
        return result;

    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left;
    const double eps = 1.0e-18;

    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        const double l = left[i];
        const double r = right[i];
        leftEnergy += l * l;
        rightEnergy += r * r;
    }

    juce::dsp::FFT fft(fftOrder);
    std::vector<juce::dsp::Complex<float>> leftIn(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> rightIn(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> leftOut(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> rightOut(static_cast<size_t>(fftSize));

    const int hopSize = fftSize / 4;
    const int numFrames = (samples - fftSize) / hopSize + 1;
    const int renderWidth = juce::jmin(imageWidth, numFrames);
    if (renderWidth <= 0)
        return result;

    auto prepareFrame = [&](int col)
    {
        const int frameIndex = col * numFrames / renderWidth;
        const int pos = frameIndex * hopSize;
        for (int i = 0; i < fftSize; ++i)
        {
            const double phase = juce::MathConstants<double>::twoPi * static_cast<double>(i)
                               / static_cast<double>(fftSize - 1);
            const float win = static_cast<float>(0.5 - 0.5 * std::cos(phase));
            leftIn[static_cast<size_t>(i)] = { left[pos + i] * win, 0.0f };
            rightIn[static_cast<size_t>(i)] = { right[pos + i] * win, 0.0f };
        }
        fft.perform(leftIn.data(), leftOut.data(), false);
        fft.perform(rightIn.data(), rightOut.data(), false);
        return pos;
    };

    double maxEnergy = eps;
    for (int col = 0; col < renderWidth; ++col)
    {
        prepareFrame(col);
        for (int bin = 1; bin < halfFFT; ++bin)
        {
            const auto l = leftOut[static_cast<size_t>(bin)];
            const auto r = rightOut[static_cast<size_t>(bin)];
            const double lPower = static_cast<double>(l.real()) * static_cast<double>(l.real())
                                + static_cast<double>(l.imag()) * static_cast<double>(l.imag());
            const double rPower = static_cast<double>(r.real()) * static_cast<double>(r.real())
                                + static_cast<double>(r.imag()) * static_cast<double>(r.imag());
            maxEnergy = juce::jmax(maxEnergy, (lPower + rPower) * 0.5);
        }
    }

    const float ceilingDb = static_cast<float>(10.0 * std::log10(maxEnergy + eps));
    const float floorDb = ceilingDb - 72.0f;
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    const float maxFrequency = juce::jmin(20000.0f, nyquist);
    const int onset = findOnsetSample(buffer);
    const double duration = static_cast<double>(samples) / sampleRate;
    double tailStart = static_cast<double>(onset) / sampleRate + 0.080;
    if (tailStart >= duration)
        tailStart = duration * 0.5;

    result.image = juce::Image(juce::Image::ARGB, renderWidth, halfFFT, true);
    result.sampledCells.reserve(12000);

    double corrWeighted = 0.0;
    double corrWeight = 0.0;
    std::vector<float> gatedCorrelations;
    double midSum = 0.0;
    double sideSum = 0.0;
    double tailMidSum = 0.0;
    double tailSideSum = 0.0;
    int gatedPoints = 0;
    const int xStride = juce::jmax(1, renderWidth / 150);
    const int yStride = juce::jmax(1, halfFFT / 80);

    for (int col = 0; col < renderWidth; ++col)
    {
        const int pos = prepareFrame(col);
        const float timeStart = static_cast<float>(static_cast<double>(pos) / sampleRate);
        const float timeEnd = static_cast<float>(static_cast<double>(pos + hopSize) / sampleRate);

        for (int bin = 1; bin < halfFFT; ++bin)
        {
            const auto l = leftOut[static_cast<size_t>(bin)];
            const auto r = rightOut[static_cast<size_t>(bin)];
            const double lr = static_cast<double>(l.real()) * static_cast<double>(r.real())
                            + static_cast<double>(l.imag()) * static_cast<double>(r.imag());
            const double lPower = static_cast<double>(l.real()) * static_cast<double>(l.real())
                                + static_cast<double>(l.imag()) * static_cast<double>(l.imag());
            const double rPower = static_cast<double>(r.real()) * static_cast<double>(r.real())
                                + static_cast<double>(r.imag()) * static_cast<double>(r.imag());
            const double energy = (lPower + rPower) * 0.5;
            const float energyDb = static_cast<float>(10.0 * std::log10(energy + eps));
            const float norm = juce::jlimit(0.0f, 1.0f, (energyDb - floorDb) / juce::jmax(1.0f, ceilingDb - floorDb));

            const std::complex<double> lc(l.real(), l.imag());
            const std::complex<double> rc(r.real(), r.imag());
            const auto mid = (lc + rc) * 0.5;
            const auto side = (lc - rc) * 0.5;
            const double midPower = std::norm(mid);
            const double sidePower = std::norm(side);
            const float correlation = juce::jlimit(-1.0f, 1.0f,
                static_cast<float>(lr / std::sqrt((lPower + eps) * (rPower + eps))));
            const float sideToMidDb = static_cast<float>(10.0 * std::log10((sidePower + eps) / (midPower + eps)));
            const float lrBalanceDb = static_cast<float>(10.0 * std::log10((rPower + eps) / (lPower + eps)));
            const float widthIndex = computeSpatialWidthIndex(sideToMidDb, correlation, lrBalanceDb);

            if (norm > 0.010f)
            {
                const float alpha = std::pow(norm, 0.64f) * 0.96f;
                result.image.setPixelAt(col, halfFFT - 1 - bin, spatialHeatmapColour(widthIndex, alpha));
            }

            const float freq = static_cast<float>(static_cast<double>(bin) * sampleRate / static_cast<double>(fftSize));
            const bool inBand = freq >= 20.0f && freq <= maxFrequency;
            const bool aboveGate = energyDb >= ceilingDb - 42.0f;
            if (inBand && aboveGate)
            {
                corrWeighted += static_cast<double>(correlation) * energy;
                corrWeight += energy;
                gatedCorrelations.push_back(correlation);
                midSum += midPower;
                sideSum += sidePower;
                if (static_cast<double>(timeStart) >= tailStart)
                {
                    tailMidSum += midPower;
                    tailSideSum += sidePower;
                }
                ++gatedPoints;
            }

            if ((col % xStride) == 0 && (bin % yStride) == 0 && norm > 0.025f)
            {
                SpatialHeatmapCell cell;
                cell.timeStartSeconds = timeStart;
                cell.timeEndSeconds = timeEnd;
                const float binWidth = static_cast<float>(sampleRate / static_cast<double>(fftSize));
                cell.frequencyLowHz = juce::jmax(0.0f, freq - binWidth * 0.5f);
                cell.frequencyHighHz = freq + binWidth * 0.5f;
                cell.energyDb = energyDb;
                cell.widthIndex = widthIndex;
                cell.sideToMidDb = sideToMidDb;
                cell.lrBalanceDb = lrBalanceDb;
                cell.correlation = correlation;
                result.sampledCells.push_back(cell);
            }
        }
    }

    auto& m = result.metrics;
    m.valid = true;
    m.durationSeconds = duration;
    m.tailStartSeconds = tailStart;
    m.tailEndSeconds = duration;
    m.timeBins = renderWidth;
    m.frequencyBins = halfFFT;
    m.fftSize = fftSize;
    m.hopSize = hopSize;
    m.maxFrequencyHz = maxFrequency;
    m.floorDb = floorDb;
    m.ceilingDb = ceilingDb;
    m.leftRmsDb = safeGainToDb(static_cast<float>(std::sqrt(leftEnergy / juce::jmax(1.0, static_cast<double>(samples)))));
    m.rightRmsDb = safeGainToDb(static_cast<float>(std::sqrt(rightEnergy / juce::jmax(1.0, static_cast<double>(samples)))));
    m.lrRmsDiffDb = m.rightRmsDb - m.leftRmsDb;
    m.stereoCorrelationMean = corrWeight > eps ? static_cast<float>(corrWeighted / corrWeight) : 0.0f;
    if (!gatedCorrelations.empty())
    {
        std::sort(gatedCorrelations.begin(), gatedCorrelations.end());
        const int p05 = juce::jlimit(0, static_cast<int>(gatedCorrelations.size()) - 1,
                                    static_cast<int>(std::round(static_cast<float>(gatedCorrelations.size() - 1) * 0.05f)));
        m.stereoCorrelationMin = gatedCorrelations[static_cast<size_t>(p05)];
    }
    else
    {
        m.stereoCorrelationMin = 0.0f;
    }
    m.sideToMidDbMean = static_cast<float>(10.0 * std::log10((sideSum + eps) / (midSum + eps)));
    m.sideToMidDbTail = static_cast<float>(10.0 * std::log10((tailSideSum + eps) / (tailMidSum + eps)));
    return result;
}

inline void refreshAnalysis(Asset& asset)
{
    asset.metrics = computeMetrics(asset.buffer, asset.sampleRate);
    asset.envelope = computeEnvelope(asset.buffer, asset.sampleRate);
    asset.energyDecay = computeEnergyDecayCurve(asset.buffer, asset.sampleRate);
    asset.dynamicsRms = computeRmsEnvelope(asset.buffer, asset.sampleRate);
    asset.spaceMetrics = computeReverbSpaceMetrics(asset.buffer, asset.sampleRate, asset.energyDecay);
    asset.dynamicsMetrics = computeDynamicsMetrics(asset.buffer, asset.sampleRate, asset.dynamicsRms);
    asset.spectrum = computeAverageSpectrum(asset.buffer, asset.sampleRate);
    asset.spectrumPeaks = computeSpectrumPeaks(asset.spectrum);
    asset.harmonicPeaks = selectHarmonicPeaks(asset.spectrumPeaks);
    asset.spectrogramBlue = computeSpectrogramImage(asset.buffer, asset.sampleRate, SpectrogramPalette::Blue);
    asset.spectrogramYellow = computeSpectrogramImage(asset.buffer, asset.sampleRate, SpectrogramPalette::Yellow);
    asset.spectrogramPink = computeSpectrogramImage(asset.buffer, asset.sampleRate, SpectrogramPalette::Pink);
    asset.spatialHeatmap = computeSpatialHeatmapAnalysis(asset.buffer, asset.sampleRate);
}

inline bool readAudioFile(const juce::File& file, Asset& out, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file.";
        return false;
    }

    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max())
    {
        error = "Audio file is too long for this first Audio Doctor pass.";
        return false;
    }

    juce::AudioBuffer<float> loaded(static_cast<int>(reader->numChannels),
                                    static_cast<int>(reader->lengthInSamples));
    reader->read(&loaded, 0, loaded.getNumSamples(), 0, true, true);

    out.name = file.getFileName();
    out.sourcePath = file.getFullPathName();
    out.sampleRate = reader->sampleRate;
    out.buffer = toStereoBuffer(loaded);
    refreshAnalysis(out);
    return true;
}

inline int findNearestZeroCrossing(const juce::AudioBuffer<float>& buffer,
                                   int sample,
                                   int searchRadiusSamples)
{
    if (buffer.getNumSamples() <= 1)
        return sample;

    sample = juce::jlimit(0, buffer.getNumSamples() - 1, sample);
    searchRadiusSamples = juce::jmax(0, searchRadiusSamples);

    int best = sample;
    float bestAbs = std::numeric_limits<float>::max();
    const int start = juce::jlimit(0, buffer.getNumSamples() - 1, sample - searchRadiusSamples);
    const int end = juce::jlimit(0, buffer.getNumSamples() - 1, sample + searchRadiusSamples);

    for (int i = start; i <= end; ++i)
    {
        float v = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            v += buffer.getSample(ch, i);
        v /= static_cast<float>(juce::jmax(1, buffer.getNumChannels()));

        const float a = std::abs(v);
        if (a < bestAbs)
        {
            bestAbs = a;
            best = i;
        }

        if (i > start)
        {
            float prev = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                prev += buffer.getSample(ch, i - 1);
            prev /= static_cast<float>(juce::jmax(1, buffer.getNumChannels()));
            if ((prev <= 0.0f && v >= 0.0f) || (prev >= 0.0f && v <= 0.0f))
                return i;
        }
    }

    return best;
}

inline bool writeAudioFile(const juce::File& file,
                           const juce::AudioBuffer<float>& buffer,
                           double sampleRate,
                           juce::String& error)
{
    file.deleteFile();
    if (!file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();

    juce::WavAudioFormat wav;
    auto stream = file.createOutputStream();
    if (stream == nullptr)
    {
        error = "Could not create derived audio file: " + file.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), sampleRate, static_cast<unsigned int>(buffer.getNumChannels()),
                            24, {}, 0));
    if (writer == nullptr)
    {
        error = "Could not create WAV writer: " + file.getFullPathName();
        return false;
    }

    stream.release();
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        error = "Could not write derived WAV: " + file.getFullPathName();
        return false;
    }

    return true;
}

inline bool applyEditToAsset(Asset& asset,
                             const juce::var& editSpec,
                             const juce::File& derivedDir,
                             const juce::String& channelName,
                             juce::String& error)
{
    if (!editSpec.isObject())
        return true;

    auto getEditObject = [](const juce::var& source, const juce::String& name)
    {
        if (auto* obj = source.getDynamicObject())
            return obj->getProperty(juce::Identifier(name));
        return juce::var();
    };

    auto getEditDouble = [&](const juce::String& name, double fallback)
    {
        const auto value = getEditObject(editSpec, name);
        return value.isVoid() || value.isUndefined() ? fallback : static_cast<double>(value);
    };

    auto getEditBool = [&](const juce::String& name, bool fallback)
    {
        const auto value = getEditObject(editSpec, name);
        return value.isVoid() || value.isUndefined() ? fallback : static_cast<bool>(value);
    };

    const double sourceDuration = asset.metrics.durationSeconds > 0.0
                                ? asset.metrics.durationSeconds
                                : static_cast<double>(asset.buffer.getNumSamples()) / juce::jmax(1.0, asset.sampleRate);
    const double trimStart = juce::jlimit(0.0, sourceDuration, getEditDouble("trimStartS", getEditDouble("start", 0.0)));
    const double trimEnd = juce::jlimit(trimStart, sourceDuration, getEditDouble("trimEndS", getEditDouble("end", sourceDuration)));
    const double fadeInMs = juce::jmax(0.0, getEditDouble("fadeInMs", 0.0));
    const double fadeOutMs = juce::jmax(0.0, getEditDouble("fadeOutMs", 0.0));
    const bool snapToZero = getEditBool("snapToZeroCrossing", false);

    int startSample = juce::jlimit(0, asset.buffer.getNumSamples(), static_cast<int>(std::round(trimStart * asset.sampleRate)));
    int endSample = juce::jlimit(startSample, asset.buffer.getNumSamples(), static_cast<int>(std::round(trimEnd * asset.sampleRate)));

    if (snapToZero)
    {
        const int radius = static_cast<int>(std::round(asset.sampleRate * 0.004));
        startSample = findNearestZeroCrossing(asset.buffer, startSample, radius);
        endSample = findNearestZeroCrossing(asset.buffer, endSample, radius);
        if (endSample <= startSample)
            endSample = juce::jmin(asset.buffer.getNumSamples(), startSample + 1);
    }

    const int length = endSample - startSample;
    if (length <= 0)
    {
        error = "Edit produced empty asset for " + channelName;
        return false;
    }

    juce::AudioBuffer<float> edited(asset.buffer.getNumChannels(), length);
    for (int ch = 0; ch < edited.getNumChannels(); ++ch)
        edited.copyFrom(ch, 0, asset.buffer, ch, startSample, length);

    const int fadeInSamples = juce::jlimit(0, edited.getNumSamples(), static_cast<int>(std::round(fadeInMs * asset.sampleRate / 1000.0)));
    const int fadeOutSamples = juce::jlimit(0, edited.getNumSamples(), static_cast<int>(std::round(fadeOutMs * asset.sampleRate / 1000.0)));
    if (fadeInSamples > 1)
        for (int ch = 0; ch < edited.getNumChannels(); ++ch)
            edited.applyGainRamp(ch, 0, fadeInSamples, 0.0f, 1.0f);
    if (fadeOutSamples > 1)
        for (int ch = 0; ch < edited.getNumChannels(); ++ch)
            edited.applyGainRamp(ch, edited.getNumSamples() - fadeOutSamples, fadeOutSamples, 1.0f, 0.0f);

    const auto originalPath = asset.sourcePath;
    asset.buffer = std::move(edited);
    asset.name += " edit";
    refreshAnalysis(asset);

    asset.editMetadata.valid = true;
    asset.editMetadata.channelName = channelName;
    asset.editMetadata.originalSourcePath = originalPath;
    asset.editMetadata.trimStartSeconds = static_cast<double>(startSample) / juce::jmax(1.0, asset.sampleRate);
    asset.editMetadata.trimEndSeconds = static_cast<double>(endSample) / juce::jmax(1.0, asset.sampleRate);
    asset.editMetadata.fadeInMs = fadeInMs;
    asset.editMetadata.fadeOutMs = fadeOutMs;
    asset.editMetadata.snapToZeroCrossing = snapToZero;
    asset.editMetadata.createdAt = juce::Time::getCurrentTime().toISO8601(true);

    if (derivedDir != juce::File{})
    {
        const auto safeChannel = channelName.replace(" ", "_").replace("/", "_").toLowerCase();
        const auto derivedFile = derivedDir.getChildFile(safeChannel + "_derived_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")).withFileExtension(".wav");
        if (!writeAudioFile(derivedFile, asset.buffer, asset.sampleRate, error))
            return false;
        asset.editMetadata.derivedSourcePath = derivedFile.getFullPathName();
        asset.sourcePath = derivedFile.getFullPathName();
    }

    return true;
}

inline Asset makeSineAsset(double frequency, double sampleRate = 48000.0, double seconds = 4.0,
                           double levelDb = -3.0, double phaseDegrees = 0.0, bool invert = false)
{
    Asset asset;
    asset.name = juce::String(frequency, 0) + " Hz sine " + juce::String(levelDb, 1) + " dBFS";
    asset.sourcePath = "generated:" + asset.name;
    asset.sampleRate = sampleRate;

    const int samples = static_cast<int>(seconds * sampleRate);
    asset.buffer.setSize(2, samples);
    const float gain = safeDbToGain(levelDb) * (invert ? -1.0f : 1.0f);
    const double phaseOffset = juce::MathConstants<double>::twoPi * phaseDegrees / 360.0;
    for (int i = 0; i < samples; ++i)
    {
        const float v = gain * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi
                                  * frequency * static_cast<double>(i) / sampleRate + phaseOffset));
        asset.buffer.setSample(0, i, v);
        asset.buffer.setSample(1, i, v);
    }

    refreshAnalysis(asset);
    return asset;
}

inline Asset makeHarmonicSeriesAsset(double fundamentalHz, int harmonicCount = 6, double rolloffDb = 6.0,
                                     double sampleRate = 48000.0, double seconds = 4.0,
                                     double levelDb = -6.0, double phaseDegrees = 0.0, bool invert = false)
{
    Asset asset;
    harmonicCount = juce::jlimit(1, 32, harmonicCount);
    fundamentalHz = juce::jlimit(10.0, sampleRate * 0.45, fundamentalHz);
    asset.name = juce::String(fundamentalHz, 0) + " Hz harmonic series H" + juce::String(harmonicCount);
    asset.sourcePath = "generated:" + asset.name + " level " + juce::String(levelDb, 1) + " dBFS";
    asset.sampleRate = sampleRate;

    const int samples = static_cast<int>(seconds * sampleRate);
    asset.buffer.setSize(2, samples);
    const float baseGain = safeDbToGain(levelDb) * (invert ? -1.0f : 1.0f);
    const double phaseOffset = juce::MathConstants<double>::twoPi * phaseDegrees / 360.0;

    double normalise = 0.0;
    for (int h = 1; h <= harmonicCount; ++h)
        if (fundamentalHz * static_cast<double>(h) < sampleRate * 0.49)
            normalise += juce::Decibels::decibelsToGain(-rolloffDb * static_cast<double>(h - 1));
    normalise = juce::jmax(1.0, normalise);

    for (int i = 0; i < samples; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        double sample = 0.0;
        for (int h = 1; h <= harmonicCount; ++h)
        {
            const double frequency = fundamentalHz * static_cast<double>(h);
            if (frequency >= sampleRate * 0.49)
                break;

            const double harmonicGain = juce::Decibels::decibelsToGain(-rolloffDb * static_cast<double>(h - 1));
            sample += harmonicGain * std::sin(juce::MathConstants<double>::twoPi * frequency * t + phaseOffset);
        }

        const float v = juce::jlimit(-1.0f, 1.0f, static_cast<float>(sample / normalise) * baseGain);
        asset.buffer.setSample(0, i, v);
        asset.buffer.setSample(1, i, v);
    }

    refreshAnalysis(asset);
    return asset;
}

inline Asset makeNoiseAsset(bool pink, double sampleRate = 48000.0, double seconds = 4.0,
                            double levelDb = -12.0, int seed = 0xAD10, bool invert = false)
{
    Asset asset;
    asset.name = juce::String(pink ? "pink noise" : "white noise") + " " + juce::String(levelDb, 1) + " dBFS";
    asset.sourcePath = "generated:" + asset.name;
    asset.sampleRate = sampleRate;

    juce::Random random(seed);
    const int samples = static_cast<int>(seconds * sampleRate);
    asset.buffer.setSize(2, samples);

    float b0 = 0.0f;
    const float gain = safeDbToGain(levelDb) * (invert ? -1.0f : 1.0f);
    for (int i = 0; i < samples; ++i)
    {
        float white = random.nextFloat() * 2.0f - 1.0f;
        float v = white;
        if (pink)
        {
            b0 = 0.99765f * b0 + white * 0.099046f;
            v = b0;
        }

        v = juce::jlimit(-1.0f, 1.0f, v * gain);
        asset.buffer.setSample(0, i, v);
        asset.buffer.setSample(1, i, v);
    }

    refreshAnalysis(asset);
    return asset;
}

inline Asset makeImpulseAsset(double sampleRate = 48000.0, double seconds = 2.0,
                              double levelDb = -1.0, bool singleSample = false, bool invert = false)
{
    Asset asset;
    asset.name = singleSample ? "single-sample impulse" : "click impulse";
    asset.name += " " + juce::String(levelDb, 1) + " dBFS";
    asset.sourcePath = "generated:" + asset.name;
    asset.sampleRate = sampleRate;

    const int samples = static_cast<int>(seconds * sampleRate);
    asset.buffer.setSize(2, samples);
    asset.buffer.clear();

    const int centre = static_cast<int>(0.15 * sampleRate);
    const int clickSamples = singleSample ? 1 : 16;
    const float gain = safeDbToGain(levelDb) * (invert ? -1.0f : 1.0f);
    for (int i = 0; i < clickSamples && centre + i < samples; ++i)
    {
        const float v = gain * (singleSample ? 1.0f : std::exp(-static_cast<float>(i) * 0.45f));
        asset.buffer.setSample(0, centre + i, v);
        asset.buffer.setSample(1, centre + i, v);
    }

    refreshAnalysis(asset);
    return asset;
}

inline Asset makeTransientBurstAsset(double sampleRate = 48000.0, double seconds = 2.0,
                                     double levelDb = -6.0, double baseHz = 130.0,
                                     double brightHz = 870.0, int seed = 0xAD10, bool invert = false)
{
    Asset asset;
    asset.name = "transient burst " + juce::String(levelDb, 1) + " dBFS";
    asset.sourcePath = "generated:" + asset.name;
    asset.sampleRate = sampleRate;

    const int samples = static_cast<int>(seconds * sampleRate);
    asset.buffer.setSize(2, samples);
    asset.buffer.clear();

    juce::Random random(seed);
    const int start = static_cast<int>(0.15 * sampleRate);
    const int length = static_cast<int>(0.12 * sampleRate);
    const float gain = safeDbToGain(levelDb) * (invert ? -1.0f : 1.0f);
    for (int i = 0; i < length && start + i < samples; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(juce::jmax(1, length - 1));
        const float attack = juce::jlimit(0.0f, 1.0f, t / 0.035f);
        const float decay = std::exp(-t * 9.0f);
        const float tone = std::sin(juce::MathConstants<float>::twoPi * static_cast<float>(baseHz) * static_cast<float>(i) / static_cast<float>(sampleRate))
                         + 0.45f * std::sin(juce::MathConstants<float>::twoPi * static_cast<float>(brightHz) * static_cast<float>(i) / static_cast<float>(sampleRate));
        const float noise = random.nextFloat() * 2.0f - 1.0f;
        const float v = juce::jlimit(-1.0f, 1.0f, (tone * 0.36f + noise * 0.18f) * attack * decay * gain);
        asset.buffer.setSample(0, start + i, v);
        asset.buffer.setSample(1, start + i, v);
    }

    refreshAnalysis(asset);
    return asset;
}

inline Asset makeSweepAsset(double sampleRate = 48000.0, double seconds = 6.0,
                            double startFreq = 20.0, double endFreq = 20000.0,
                            double levelDb = -6.0, double phaseDegrees = 0.0, bool invert = false)
{
    Asset asset;
    startFreq = juce::jlimit(1.0, sampleRate * 0.45, startFreq);
    endFreq = juce::jlimit(startFreq + 1.0, sampleRate * 0.49, endFreq);
    asset.name = juce::String(startFreq, 0) + " Hz - " + juce::String(endFreq, 0) + " Hz sweep";
    asset.sourcePath = "generated:" + asset.name;
    asset.sampleRate = sampleRate;

    const int samples = static_cast<int>(seconds * sampleRate);
    asset.buffer.setSize(2, samples);

    const double logRatio = std::log(endFreq / startFreq);
    double phase = juce::MathConstants<double>::twoPi * phaseDegrees / 360.0;
    const float gain = safeDbToGain(levelDb) * (invert ? -1.0f : 1.0f);

    for (int i = 0; i < samples; ++i)
    {
        const double t = static_cast<double>(i) / static_cast<double>(samples);
        const double freq = startFreq * std::exp(logRatio * t);
        phase += juce::MathConstants<double>::twoPi * freq / sampleRate;

        const float fade = static_cast<float>(juce::jmin(1.0, juce::jmin(t * 20.0, (1.0 - t) * 20.0)));
        const float v = gain * fade * std::sin(static_cast<float>(phase));
        asset.buffer.setSample(0, i, v);
        asset.buffer.setSample(1, i, v);
    }

    refreshAnalysis(asset);
    return asset;
}

inline void addClippedSample(juce::AudioBuffer<float>& buffer, int channel, int sample, float value)
{
    if (channel < 0 || channel >= buffer.getNumChannels() || sample < 0 || sample >= buffer.getNumSamples())
        return;

    buffer.setSample(channel, sample, juce::jlimit(-1.0f, 1.0f, buffer.getSample(channel, sample) + value));
}

inline void mixAssetAt(juce::AudioBuffer<float>& destination, const juce::AudioBuffer<float>& source,
                       int startSample, float gain = 1.0f)
{
    const int channels = juce::jmin(destination.getNumChannels(), source.getNumChannels());
    const int maxSamples = juce::jmin(source.getNumSamples(), destination.getNumSamples() - juce::jmax(0, startSample));
    if (channels <= 0 || maxSamples <= 0)
        return;

    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < maxSamples; ++i)
            addClippedSample(destination, ch, startSample + i, source.getSample(ch, i) * gain);
}

inline Asset finaliseGeneratedAsset(Asset asset, GeneratedSignalSpec spec,
                                    const juce::String& name,
                                    std::vector<StageMarker> stageMarkers = {})
{
    spec = normaliseGeneratedSignalSpec(spec);
    asset.generatedSignal = true;
    asset.generatedSignalSpec = spec;
    asset.stageMarkers = std::move(stageMarkers);
    asset.sourcePath = "generated:" + spec.type + ":" + spec.preset;
    if (name.isNotEmpty())
        asset.name = name;
    return asset;
}

inline Asset makeChargeRiserAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    Asset asset;
    asset.name = "charge riser " + juce::String(spec.startHz, 0) + "-" + juce::String(spec.endHz, 0) + " Hz";
    asset.sampleRate = spec.sampleRate;
    const int samples = juce::jmax(1, static_cast<int>(std::round(spec.seconds * spec.sampleRate)));
    asset.buffer.setSize(spec.channels, samples);
    asset.buffer.clear();

    juce::Random random(spec.seed);
    const double ratio = spec.endHz / juce::jmax(1.0, spec.startHz);
    const float gain = safeDbToGain(spec.levelDb) * (spec.invert ? -1.0f : 1.0f);
    double phase = juce::MathConstants<double>::twoPi * spec.phaseDegrees / 360.0;
    for (int i = 0; i < samples; ++i)
    {
        const double n = samples > 1 ? static_cast<double>(i) / static_cast<double>(samples - 1) : 0.0;
        const double envelope = std::pow(n, spec.curve);
        const double freq = spec.startHz * std::pow(ratio, n);
        phase += juce::MathConstants<double>::twoPi * freq / spec.sampleRate;

        double tone = std::sin(phase);
        for (int h = 2; h <= spec.harmonicCount; ++h)
        {
            const double harmonicFreq = freq * static_cast<double>(h);
            if (harmonicFreq >= spec.sampleRate * 0.49)
                break;
            tone += juce::Decibels::decibelsToGain(-spec.harmonicRolloffDb * static_cast<double>(h - 1))
                  * std::sin(phase * static_cast<double>(h));
        }
        tone /= juce::jmax(1.0, static_cast<double>(spec.harmonicCount) * 0.42);

        const double pulse = (1.0 - spec.modDepth)
                           + spec.modDepth * (0.5 + 0.5 * std::sin(juce::MathConstants<double>::twoPi * spec.modRateHz * n * spec.seconds));
        const double noise = (random.nextDouble() * 2.0 - 1.0) * spec.noiseAmount;
        const float left = juce::jlimit(-1.0f, 1.0f, static_cast<float>((tone + noise * 0.35) * envelope * pulse) * gain);
        const float right = juce::jlimit(-1.0f, 1.0f, static_cast<float>((tone * (1.0 - spec.stereoWidth * 0.18) - noise * 0.28 * spec.stereoWidth) * envelope * pulse) * gain);
        asset.buffer.setSample(0, i, left);
        if (asset.buffer.getNumChannels() > 1)
            asset.buffer.setSample(1, i, right);
    }

    refreshAnalysis(asset);
    return finaliseGeneratedAsset(std::move(asset), spec, "charge_riser " + spec.preset);
}

inline Asset makeShotImpactAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    Asset asset;
    asset.name = "shot impact " + juce::String(spec.bodyHz, 0) + " Hz body";
    asset.sampleRate = spec.sampleRate;
    const int samples = juce::jmax(1, static_cast<int>(std::round(spec.seconds * spec.sampleRate)));
    asset.buffer.setSize(spec.channels, samples);
    asset.buffer.clear();

    juce::Random random(spec.seed);
    const float gain = safeDbToGain(spec.levelDb) * (spec.invert ? -1.0f : 1.0f);
    const int start = juce::jlimit(0, samples - 1, static_cast<int>(std::round(spec.sampleRate * 0.04)));
    const double bodyTau = juce::jmax(0.005, spec.bodyDecayMs / 1000.0);
    const double crackTau = juce::jmax(0.001, spec.transientMs / 1000.0);
    double bodyPhase = 0.0;
    for (int i = start; i < samples; ++i)
    {
        const double t = static_cast<double>(i - start) / spec.sampleRate;
        const double bodyFreq = spec.bodyHz * (1.0 + 0.85 * std::exp(-t / 0.035));
        bodyPhase += juce::MathConstants<double>::twoPi * bodyFreq / spec.sampleRate;
        const double body = std::sin(bodyPhase) * std::exp(-t / bodyTau);
        const double mid = 0.36 * std::sin(bodyPhase * 2.7) * std::exp(-t / (bodyTau * 0.55));
        const double crackEnv = std::exp(-t / crackTau);
        const double crack = (random.nextDouble() * 2.0 - 1.0) * spec.crackAmount * crackEnv;
        const double click = t < 0.002 ? (1.0 - t / 0.002) * 0.65 : 0.0;
        const float value = juce::jlimit(-1.0f, 1.0f, static_cast<float>((body * 0.78 + mid + crack * 0.42 + click) * gain));
        asset.buffer.setSample(0, i, value);
        if (asset.buffer.getNumChannels() > 1)
        {
            const float right = juce::jlimit(-1.0f, 1.0f, value * (1.0f - static_cast<float>(spec.stereoWidth) * 0.12f)
                                                       + static_cast<float>(crack * spec.stereoWidth * gain * 0.18));
            asset.buffer.setSample(1, i, right);
        }
    }

    refreshAnalysis(asset);
    return finaliseGeneratedAsset(std::move(asset), spec, "shot_impact " + spec.preset,
                                  { { "Shot", static_cast<double>(start) / spec.sampleRate,
                                             juce::jmin(spec.seconds, static_cast<double>(start) / spec.sampleRate + spec.transientMs / 1000.0) } });
}

inline Asset makeTailDecayAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    Asset asset;
    asset.name = "tail decay " + juce::String(spec.fundamentalHz, 0) + " Hz";
    asset.sampleRate = spec.sampleRate;
    const int samples = juce::jmax(1, static_cast<int>(std::round(spec.seconds * spec.sampleRate)));
    asset.buffer.setSize(spec.channels, samples);
    asset.buffer.clear();

    juce::Random random(spec.seed);
    const float gain = safeDbToGain(spec.levelDb) * (spec.invert ? -1.0f : 1.0f);
    const double decay = juce::jmax(0.02, spec.decayMs / 1000.0);
    std::array<double, 5> phase {};
    for (int i = 0; i < samples; ++i)
    {
        const double t = static_cast<double>(i) / spec.sampleRate;
        const double env = std::exp(-t / decay);
        double tone = 0.0;
        for (int r = 0; r < 5; ++r)
        {
            const double freq = spec.fundamentalHz * (1.0 + 0.41 * static_cast<double>(r))
                              * (1.0 - spec.damping * 0.18 * (t / juce::jmax(0.01, spec.seconds)));
            phase[static_cast<size_t>(r)] += juce::MathConstants<double>::twoPi * juce::jmax(10.0, freq) / spec.sampleRate;
            tone += std::sin(phase[static_cast<size_t>(r)]) * juce::Decibels::decibelsToGain(-5.5 * static_cast<double>(r));
        }
        const double noiseEnv = std::exp(-t / (decay * 0.72));
        const double noise = (random.nextDouble() * 2.0 - 1.0) * spec.noiseAmount * noiseEnv;
        const float left = juce::jlimit(-1.0f, 1.0f, static_cast<float>((tone * 0.42 + noise * 0.25) * env) * gain);
        const float right = juce::jlimit(-1.0f, 1.0f, static_cast<float>((tone * 0.40 - noise * 0.24 * spec.stereoWidth) * env) * gain);
        asset.buffer.setSample(0, i, left);
        if (asset.buffer.getNumChannels() > 1)
            asset.buffer.setSample(1, i, right);
    }

    const int reflectionDelay = static_cast<int>(0.042 * spec.sampleRate);
    const float reflectionGain = static_cast<float>(spec.earlyReflectionAmount);
    for (int ch = 0; ch < asset.buffer.getNumChannels(); ++ch)
        for (int i = samples - 1; i >= reflectionDelay; --i)
            addClippedSample(asset.buffer, ch, i, asset.buffer.getSample(ch, i - reflectionDelay) * reflectionGain);

    refreshAnalysis(asset);
    return finaliseGeneratedAsset(std::move(asset), spec, "tail_decay " + spec.preset,
                                  { { "Tail", 0.0, spec.seconds } });
}

inline Asset makeBandLimitedNoiseAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    Asset asset;
    asset.name = "band limited noise " + juce::String(spec.noiseBandLowHz, 0) + "-" + juce::String(spec.noiseBandHighHz, 0) + " Hz";
    asset.sampleRate = spec.sampleRate;
    const int samples = juce::jmax(1, static_cast<int>(std::round(spec.seconds * spec.sampleRate)));
    asset.buffer.setSize(spec.channels, samples);
    asset.buffer.clear();

    juce::Random random(spec.seed);
    const int partials = 36;
    std::array<double, 36> freqs {};
    std::array<double, 36> phases {};
    const double low = juce::jlimit(10.0, spec.sampleRate * 0.45, spec.noiseBandLowHz);
    const double high = juce::jlimit(low + 1.0, spec.sampleRate * 0.49, spec.noiseBandHighHz);
    for (int p = 0; p < partials; ++p)
    {
        const double n = static_cast<double>(p) / static_cast<double>(partials - 1);
        freqs[static_cast<size_t>(p)] = low * std::pow(high / low, n);
        phases[static_cast<size_t>(p)] = random.nextDouble() * juce::MathConstants<double>::twoPi;
    }

    const float gain = safeDbToGain(spec.levelDb) * (spec.invert ? -1.0f : 1.0f);
    for (int i = 0; i < samples; ++i)
    {
        const double n = samples > 1 ? static_cast<double>(i) / static_cast<double>(samples - 1) : 0.0;
        const double env = juce::jmin(1.0, juce::jmin(n * 20.0, (1.0 - n) * 20.0));
        double sample = 0.0;
        for (int p = 0; p < partials; ++p)
        {
            phases[static_cast<size_t>(p)] += juce::MathConstants<double>::twoPi * freqs[static_cast<size_t>(p)] / spec.sampleRate;
            sample += std::sin(phases[static_cast<size_t>(p)]);
        }
        sample /= static_cast<double>(partials);
        const float value = juce::jlimit(-1.0f, 1.0f, static_cast<float>(sample * env) * gain * 3.0f);
        asset.buffer.setSample(0, i, value);
        if (asset.buffer.getNumChannels() > 1)
            asset.buffer.setSample(1, i, value * (1.0f - static_cast<float>(spec.stereoWidth) * 0.20f));
    }

    refreshAnalysis(asset);
    return finaliseGeneratedAsset(std::move(asset), spec, "band_limited_noise " + spec.preset);
}

inline Asset makeHarmonicFusionTestAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    Asset asset;
    asset.name = "harmonic fusion " + juce::String(spec.fundamentalAHz, 0) + "/" + juce::String(spec.fundamentalBHz, 0) + " Hz";
    asset.sampleRate = spec.sampleRate;
    const int samples = juce::jmax(1, static_cast<int>(std::round(spec.seconds * spec.sampleRate)));
    asset.buffer.setSize(spec.channels, samples);
    asset.buffer.clear();

    const float gain = safeDbToGain(spec.levelDb) * (spec.invert ? -1.0f : 1.0f);
    const double detuneRatio = std::pow(2.0, spec.detuneCents / 1200.0);
    for (int i = 0; i < samples; ++i)
    {
        const double t = static_cast<double>(i) / spec.sampleRate;
        double a = 0.0;
        double b = 0.0;
        for (int h = 1; h <= spec.harmonicCount; ++h)
        {
            const double roll = juce::Decibels::decibelsToGain(-spec.harmonicRolloffDb * static_cast<double>(h - 1));
            const double fa = spec.fundamentalAHz * static_cast<double>(h);
            const double fb = spec.fundamentalBHz * detuneRatio * static_cast<double>(h);
            if (fa < spec.sampleRate * 0.49)
                a += roll * std::sin(juce::MathConstants<double>::twoPi * fa * t);
            if (fb < spec.sampleRate * 0.49)
                b += roll * std::sin(juce::MathConstants<double>::twoPi * fb * t + 0.37);
        }
        const double mix = a * (1.0 - spec.overlapAmount * 0.35) + b * spec.overlapAmount;
        const float value = juce::jlimit(-1.0f, 1.0f, static_cast<float>(mix * 0.35) * gain);
        asset.buffer.setSample(0, i, value);
        if (asset.buffer.getNumChannels() > 1)
            asset.buffer.setSample(1, i, juce::jlimit(-1.0f, 1.0f, static_cast<float>((a * 0.78 + b * 1.05) * 0.28) * gain));
    }

    refreshAnalysis(asset);
    return finaliseGeneratedAsset(std::move(asset), spec, "harmonic_fusion_test " + spec.preset);
}

inline Asset makeCstEventAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);
    spec.seconds = juce::jmax(spec.seconds, spec.tailStartSec + 0.35);
    const double shotTime = juce::jlimit(0.04, spec.seconds - 0.08, spec.shotTimeSec);
    const double tailStart = juce::jlimit(shotTime + 0.015, spec.seconds - 0.02, spec.tailStartSec);
    const double chargeStart = juce::jlimit(0.0, shotTime - 0.02, spec.chargeStartSec);

    Asset asset;
    asset.name = "cst event " + spec.preset;
    asset.sampleRate = spec.sampleRate;
    const int samples = juce::jmax(1, static_cast<int>(std::round(spec.seconds * spec.sampleRate)));
    asset.buffer.setSize(spec.channels, samples);
    asset.buffer.clear();

    auto chargeSpec = spec;
    chargeSpec.type = "charge_riser";
    chargeSpec.seconds = juce::jmax(0.08, shotTime - chargeStart);
    chargeSpec.levelDb = spec.levelDb - 4.0;
    chargeSpec.seed = spec.seed + 11;

    auto shotSpec = spec;
    shotSpec.type = "shot_impact";
    shotSpec.seconds = juce::jmax(0.35, spec.seconds - shotTime);
    shotSpec.levelDb = spec.levelDb;
    shotSpec.seed = spec.seed + 23;

    auto tailSpec = spec;
    tailSpec.type = "tail_decay";
    tailSpec.seconds = juce::jmax(0.25, spec.seconds - tailStart);
    tailSpec.levelDb = spec.levelDb - 7.0;
    tailSpec.seed = spec.seed + 37;

    auto charge = makeChargeRiserAsset(chargeSpec);
    auto shot = makeShotImpactAsset(shotSpec);
    auto tail = makeTailDecayAsset(tailSpec);
    mixAssetAt(asset.buffer, charge.buffer, static_cast<int>(std::round(chargeStart * spec.sampleRate)), static_cast<float>(spec.stageBalance));
    mixAssetAt(asset.buffer, shot.buffer, static_cast<int>(std::round(shotTime * spec.sampleRate)), 1.0f);
    mixAssetAt(asset.buffer, tail.buffer, static_cast<int>(std::round(tailStart * spec.sampleRate)), 1.0f);

    refreshAnalysis(asset);
    return finaliseGeneratedAsset(std::move(asset), spec, "cst_event " + spec.preset,
                                  { { "Charge", chargeStart, shotTime },
                                    { "Shot", shotTime, tailStart },
                                    { "Tail", tailStart, spec.seconds } });
}

inline Asset makeGeneratedSignalAsset(GeneratedSignalSpec spec)
{
    spec = normaliseGeneratedSignalSpec(spec);

    if (spec.type == "charge_riser")
        return makeChargeRiserAsset(spec);

    if (spec.type == "shot_impact")
        return makeShotImpactAsset(spec);

    if (spec.type == "tail_decay")
        return makeTailDecayAsset(spec);

    if (spec.type == "cst_event")
        return makeCstEventAsset(spec);

    if (spec.type == "harmonic_fusion_test")
        return makeHarmonicFusionTestAsset(spec);

    if (spec.type == "band_limited_noise")
        return makeBandLimitedNoiseAsset(spec);

    if (spec.type == "sine")
        return finaliseGeneratedAsset(makeSineAsset(spec.frequencyHz, spec.sampleRate, spec.seconds, spec.levelDb, spec.phaseDegrees, spec.invert),
                                      spec, "sine " + juce::String(spec.frequencyHz, 0) + " Hz");

    if (spec.type == "harmonic_series")
        return finaliseGeneratedAsset(makeHarmonicSeriesAsset(spec.frequencyHz, spec.harmonicCount, spec.harmonicRolloffDb,
                                                              spec.sampleRate, spec.seconds, spec.levelDb, spec.phaseDegrees, spec.invert),
                                      spec, "harmonic_series " + juce::String(spec.frequencyHz, 0) + " Hz");

    if (spec.type == "noise" || spec.type == "white_noise")
        return finaliseGeneratedAsset(makeNoiseAsset(false, spec.sampleRate, spec.seconds, spec.levelDb, spec.seed, spec.invert),
                                      spec, "white_noise " + spec.preset);

    if (spec.type == "pink_noise")
        return finaliseGeneratedAsset(makeNoiseAsset(true, spec.sampleRate, spec.seconds, spec.levelDb, spec.seed, spec.invert),
                                      spec, "pink_noise " + spec.preset);

    if (spec.type == "sweep")
        return finaliseGeneratedAsset(makeSweepAsset(spec.sampleRate, spec.seconds, spec.startHz, spec.endHz,
                                                     spec.levelDb, spec.phaseDegrees, spec.invert),
                                      spec, "sweep " + juce::String(spec.startHz, 0) + "-" + juce::String(spec.endHz, 0) + " Hz");

    if (spec.type == "transient_burst")
        return finaliseGeneratedAsset(makeTransientBurstAsset(spec.sampleRate, spec.seconds, spec.levelDb,
                                                              spec.frequencyHz, spec.endHz, spec.seed, spec.invert),
                                      spec, "transient_burst " + spec.preset);

    if (spec.type == "click")
        return finaliseGeneratedAsset(makeImpulseAsset(spec.sampleRate, spec.seconds, spec.levelDb, false, spec.invert),
                                      spec, "click " + spec.preset);

    return finaliseGeneratedAsset(makeImpulseAsset(spec.sampleRate, spec.seconds, spec.levelDb, true, spec.invert),
                                  spec, "impulse " + spec.preset);
}

inline juce::String hashSourceFnv1a64(const juce::String& sourcePath)
{
    constexpr uint64_t offsetBasis = 1469598103934665603ULL;
    const juce::File file(sourcePath);
    if (!file.existsAsFile())
    {
        const auto hash = fnv1a64Bytes(sourcePath.toRawUTF8(), static_cast<size_t>(sourcePath.getNumBytesAsUTF8()), offsetBasis);
        return sourcePath.startsWith("generated:") ? "generated-fnv1a64:" + juce::String::toHexString(static_cast<juce::int64>(hash))
                                                   : juce::String();
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return {};

    uint64_t hash = offsetBasis;
    std::array<uint8_t, 8192> buffer {};
    while (!stream.isExhausted())
    {
        const auto bytesRead = stream.read(buffer.data(), static_cast<int>(buffer.size()));
        if (bytesRead <= 0)
            break;
        hash = fnv1a64Bytes(buffer.data(), static_cast<size_t>(bytesRead), hash);
    }

    return "fnv1a64:" + juce::String::toHexString(static_cast<juce::int64>(hash));
}

inline juce::int64 sourceBytesOnDisk(const juce::String& sourcePath)
{
    const juce::File file(sourcePath);
    return file.existsAsFile() ? file.getSize() : 0;
}

inline void writeCurveCsv(const juce::File& dataDir, const juce::String& role, const juce::String& name,
                          const juce::String& header, const std::vector<PlotPoint>& points,
                          juce::Array<juce::var>& dataFiles)
{
    if (points.empty())
        return;

    const auto file = dataDir.getChildFile(role + "_" + name).withFileExtension(".csv");
    juce::String text = header + "\n";
    for (const auto& p : points)
        text += juce::String(p.x, 6) + "," + juce::String(p.y, 6) + "\n";

    file.replaceWithText(text);
    dataFiles.add(file.getFullPathName());
}

inline void writeApparentAttenuationCsv(const juce::File& dataDir, const juce::String& role,
                                        const Asset* reference, const Asset* target,
                                        juce::Array<juce::var>& dataFiles)
{
    if (reference == nullptr || target == nullptr)
        return;

    writeCurveCsv(dataDir, role, "apparent_attenuation_seconds_delta_db", "seconds,delta_dB",
                  computeApparentAttenuationCurve(reference->dynamicsRms, target->dynamicsRms),
                  dataFiles);
}

inline void writeSpectrumPeakCsv(const juce::File& dataDir, const juce::String& role,
                                 const std::vector<SpectrumPeak>& peaks,
                                 juce::Array<juce::var>& dataFiles)
{
    if (peaks.empty())
        return;

    const auto file = dataDir.getChildFile(role + "_spectrum_peaks").withFileExtension(".csv");
    juce::String text = "frequencyHz,dB,harmonicNumber,expectedHz,deltaCents,nearHarmonic\n";
    for (const auto& peak : peaks)
    {
        text += juce::String(peak.frequencyHz, 6) + ","
             + juce::String(peak.magnitudeDb, 6) + ","
             + juce::String(peak.harmonicNumber) + ","
             + juce::String(peak.expectedHz, 6) + ","
             + juce::String(peak.deltaCents, 6) + ","
             + juce::String(peak.nearHarmonic ? 1 : 0) + "\n";
    }

    file.replaceWithText(text);
    dataFiles.add(file.getFullPathName());
}

inline juce::var writeSpectrumPeaksJson(const std::vector<SpectrumPeak>& peaks)
{
    juce::Array<juce::var> array;
    for (const auto& peak : peaks)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("frequencyHz", peak.frequencyHz);
        obj->setProperty("dB", peak.magnitudeDb);
        obj->setProperty("harmonicNumber", peak.harmonicNumber);
        obj->setProperty("expectedHz", peak.expectedHz);
        obj->setProperty("deltaCents", peak.deltaCents);
        obj->setProperty("nearHarmonic", peak.nearHarmonic);
        array.add(juce::var(obj.release()));
    }
    return juce::var(array);
}

inline juce::var writeSpatialHeatmapMetricsJson(const SpatialHeatmapMetrics& m)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("valid", m.valid);
    obj->setProperty("durationSeconds", m.durationSeconds);
    obj->setProperty("tailStartSeconds", m.tailStartSeconds);
    obj->setProperty("tailEndSeconds", m.tailEndSeconds);
    obj->setProperty("timeBins", m.timeBins);
    obj->setProperty("frequencyBins", m.frequencyBins);
    obj->setProperty("fftSize", m.fftSize);
    obj->setProperty("hopSize", m.hopSize);
    obj->setProperty("window", "hann");
    obj->setProperty("frequencyScale", "linear FFT bin");
    obj->setProperty("minFrequencyHz", m.minFrequencyHz);
    obj->setProperty("maxFrequencyHz", m.maxFrequencyHz);
    obj->setProperty("floorDb", m.floorDb);
    obj->setProperty("ceilingDb", m.ceilingDb);
    obj->setProperty("widthMapping", "widthIndex combines Side/Mid ratio, local L/R correlation, and L/R balance; 0=narrow/correlated, 1=wide/decorrelated or strongly split");
    obj->setProperty("stereoCorrelationMean", m.stereoCorrelationMean);
    obj->setProperty("stereoCorrelationMin", m.stereoCorrelationMin);
    obj->setProperty("stereoCorrelationMinDefinition", "5th percentile of reliable STFT cells, not raw single-cell minimum");
    obj->setProperty("sideToMidDbMean", m.sideToMidDbMean);
    obj->setProperty("sideToMidDbTail", m.sideToMidDbTail);
    obj->setProperty("leftRmsDb", m.leftRmsDb);
    obj->setProperty("rightRmsDb", m.rightRmsDb);
    obj->setProperty("lrRmsDiffDb", m.lrRmsDiffDb);
    return juce::var(obj.release());
}

inline float averageSpectrumBandDb(const std::vector<PlotPoint>& spectrum, float minHz, float maxHz)
{
    double energy = 0.0;
    int count = 0;
    for (const auto& p : spectrum)
    {
        if (p.x < minHz || p.x >= maxHz)
            continue;
        const double gain = juce::Decibels::decibelsToGain(static_cast<double>(p.y));
        energy += gain * gain;
        ++count;
    }

    if (count <= 0)
        return -120.0f;
    return safeGainToDb(static_cast<float>(std::sqrt(energy / static_cast<double>(count))));
}

inline juce::AudioBuffer<float> sliceBuffer(const juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    startSample = juce::jlimit(0, buffer.getNumSamples(), startSample);
    endSample = juce::jlimit(startSample, buffer.getNumSamples(), endSample);
    juce::AudioBuffer<float> slice(buffer.getNumChannels(), juce::jmax(1, endSample - startSample));
    slice.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        slice.copyFrom(ch, 0, buffer, ch, startSample, endSample - startSample);
    return slice;
}

inline void writeSpectrogramSummaryCsv(const juce::File& dataDir, const juce::String& role,
                                       const Asset* asset, juce::Array<juce::var>& dataFiles)
{
    if (asset == nullptr || ! asset->spectrogramBlue.isValid())
        return;

    const auto file = dataDir.getChildFile(role + "_spectrogram_summary").withFileExtension(".csv");
    juce::String text = "role,sampleRate,durationSeconds,blueWidth,blueHeight,yellowWidth,yellowHeight,pinkWidth,pinkHeight\n";
    text += role + ","
          + juce::String(asset->sampleRate, 6) + ","
          + juce::String(asset->metrics.durationSeconds, 6) + ","
          + juce::String(asset->spectrogramBlue.getWidth()) + ","
          + juce::String(asset->spectrogramBlue.getHeight()) + ","
          + juce::String(asset->spectrogramYellow.getWidth()) + ","
          + juce::String(asset->spectrogramYellow.getHeight()) + ","
          + juce::String(asset->spectrogramPink.getWidth()) + ","
          + juce::String(asset->spectrogramPink.getHeight()) + "\n";
    file.replaceWithText(text);
    dataFiles.add(file.getFullPathName());
}

inline void writeSpatialHeatmapCsv(const juce::File& dataDir, const juce::String& role,
                                   const Asset* asset, juce::Array<juce::var>& dataFiles)
{
    if (asset == nullptr || ! asset->spatialHeatmap.metrics.valid)
        return;

    const auto summaryFile = dataDir.getChildFile(role + "_spatial_heatmap_summary").withFileExtension(".csv");
    const auto& m = asset->spatialHeatmap.metrics;
    juce::String summary = "role,durationSeconds,tailStartSeconds,tailEndSeconds,timeBins,frequencyBins,fftSize,hopSize,minFrequencyHz,maxFrequencyHz,floorDb,ceilingDb,stereoCorrelationMean,stereoCorrelationMin,sideToMidDbMean,sideToMidDbTail,leftRmsDb,rightRmsDb,lrRmsDiffDb\n";
    summary += role + ","
             + juce::String(m.durationSeconds, 6) + ","
             + juce::String(m.tailStartSeconds, 6) + ","
             + juce::String(m.tailEndSeconds, 6) + ","
             + juce::String(m.timeBins) + ","
             + juce::String(m.frequencyBins) + ","
             + juce::String(m.fftSize) + ","
             + juce::String(m.hopSize) + ","
             + juce::String(m.minFrequencyHz, 6) + ","
             + juce::String(m.maxFrequencyHz, 6) + ","
             + juce::String(m.floorDb, 6) + ","
             + juce::String(m.ceilingDb, 6) + ","
             + juce::String(m.stereoCorrelationMean, 6) + ","
             + juce::String(m.stereoCorrelationMin, 6) + ","
             + juce::String(m.sideToMidDbMean, 6) + ","
             + juce::String(m.sideToMidDbTail, 6) + ","
             + juce::String(m.leftRmsDb, 6) + ","
             + juce::String(m.rightRmsDb, 6) + ","
             + juce::String(m.lrRmsDiffDb, 6) + "\n";
    summaryFile.replaceWithText(summary);
    dataFiles.add(summaryFile.getFullPathName());

    if (asset->spatialHeatmap.sampledCells.empty())
        return;

    const auto cellsFile = dataDir.getChildFile(role + "_spatial_heatmap_cells").withFileExtension(".csv");
    juce::String cells = "timeStartSeconds,timeEndSeconds,frequencyLowHz,frequencyHighHz,energyDb,widthIndex,sideToMidDb,lrBalanceDb,correlation\n";
    for (const auto& cell : asset->spatialHeatmap.sampledCells)
    {
        cells += juce::String(cell.timeStartSeconds, 6) + ","
              + juce::String(cell.timeEndSeconds, 6) + ","
              + juce::String(cell.frequencyLowHz, 6) + ","
              + juce::String(cell.frequencyHighHz, 6) + ","
              + juce::String(cell.energyDb, 6) + ","
              + juce::String(cell.widthIndex, 6) + ","
              + juce::String(cell.sideToMidDb, 6) + ","
              + juce::String(cell.lrBalanceDb, 6) + ","
              + juce::String(cell.correlation, 6) + "\n";
    }

    cellsFile.replaceWithText(cells);
    dataFiles.add(cellsFile.getFullPathName());
}

inline void writeStageMarkersCsv(const juce::File& dataDir, const juce::String& role,
                                 const Asset* asset, juce::Array<juce::var>& dataFiles)
{
    if (asset == nullptr || asset->stageMarkers.empty())
        return;

    const auto file = dataDir.getChildFile(role + "_stage_markers").withFileExtension(".csv");
    juce::String text = "role,label,startSec,endSec,peakDb,rmsDb,crestDb,lowEnergyDb,midEnergyDb,highEnergyDb\n";
    for (const auto& marker : asset->stageMarkers)
    {
        const int startSample = static_cast<int>(std::round(marker.startSec * asset->sampleRate));
        const int endSample = static_cast<int>(std::round(marker.endSec * asset->sampleRate));
        auto segment = sliceBuffer(asset->buffer, startSample, endSample);
        const auto metrics = computeMetrics(segment, asset->sampleRate);
        const auto spectrum = computeAverageSpectrum(segment, asset->sampleRate, 10);
        const auto label = marker.label.replace("\"", "\"\"");
        text += role + ","
              + "\"" + label + "\","
              + juce::String(marker.startSec, 6) + ","
              + juce::String(marker.endSec, 6) + ","
              + juce::String(metrics.peakDb, 6) + ","
              + juce::String(metrics.rmsDb, 6) + ","
              + juce::String(metrics.crestDb, 6) + ","
              + juce::String(averageSpectrumBandDb(spectrum, 20.0f, 250.0f), 6) + ","
              + juce::String(averageSpectrumBandDb(spectrum, 250.0f, 2000.0f), 6) + ","
              + juce::String(averageSpectrumBandDb(spectrum, 2000.0f, 20000.0f), 6) + "\n";
    }

    file.replaceWithText(text);
    dataFiles.add(file.getFullPathName());
}

inline void writeAssetCurves(const juce::File& dataDir, const juce::String& role, const Asset* asset,
                             juce::Array<juce::var>& dataFiles)
{
    if (asset == nullptr)
        return;

    writeCurveCsv(dataDir, role, "spectrum_hz_db",            "hz,dB",        asset->spectrum,     dataFiles);
    writeSpectrumPeakCsv(dataDir, role, asset->spectrumPeaks, dataFiles);
    writeCurveCsv(dataDir, role, "envelope_seconds_dbfs",     "seconds,dBFS", asset->envelope,     dataFiles);
    writeCurveCsv(dataDir, role, "group_delay_hz_ms",         "hz,ms",        asset->groupDelay,   dataFiles);
    writeCurveCsv(dataDir, role, "energy_decay_seconds_db",   "seconds,dB",   asset->energyDecay,  dataFiles);
    writeCurveCsv(dataDir, role, "dynamics_rms_seconds_dbfs", "seconds,dBFS", asset->dynamicsRms,  dataFiles);
    writeSpectrogramSummaryCsv(dataDir, role, asset, dataFiles);
    writeSpatialHeatmapCsv(dataDir, role, asset, dataFiles);
    writeStageMarkersCsv(dataDir, role, asset, dataFiles);
}

} // namespace goodmeter::audio_doctor
