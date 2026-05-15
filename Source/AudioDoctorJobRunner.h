/*
  ==============================================================================
    AudioDoctorJobRunner.h
    GOODMETER Audio Doctor - local JSON job runner for reproducible thesis tasks.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "AudioDoctorPluginHost.h"
#include "AudioDoctorFigureRenderer.h"

namespace goodmeter::audio_doctor
{

class JobRunner
{
public:
    bool runJobFile(const juce::File& jobFile, juce::String& responseText)
    {
        const auto parsed = juce::JSON::parse(jobFile.loadFileAsString());
        if (!parsed.isObject())
            return fail("Invalid JSON job file.", responseText);

        schemaVersion = static_cast<int>(getDouble(parsed, "schemaVersion", 1.0));
        response = std::make_unique<juce::DynamicObject>();
        response->setProperty("schemaVersion", schemaVersion);
        response->setProperty("status", "error");
        response->setProperty("jobPath", jobFile.getFullPathName());

        job = parsed;
        sessionId = getString(job, "sessionId",
                    getString(job, "sessionName", "audio_doctor_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")));
        outDir = juce::File(getString(getObject(job, "export"), "outDir", jobFile.getSiblingFile("AudioDoctor_Job_Exports").getFullPathName()))
                     .getChildFile(sanitizeFileToken(sessionId));

        if (!outDir.exists() && !outDir.createDirectory())
            return fail("Could not create output directory: " + outDir.getFullPathName(), responseText);

        response->setProperty("sessionId", sessionId);
        response->setProperty("outDir", outDir.getFullPathName());

        if (!loadDry())
            return finish(responseText);

        if (!loadPluginsAndRender())
            return finish(responseText);

        refreshTransfer();
        writeOutputs();

        response->setProperty("status", "ok");
        responseText = juce::JSON::toString(juce::var(response.release()), true);
        const auto responseFile = outDir.getChildFile("response.json");
        responseFile.replaceWithText(responseText);
        return true;
    }

private:
    using AssetPtr = std::unique_ptr<Asset>;

    struct RenderInfo
    {
        bool valid = false;
        juce::PluginDescription plugin;
        int latencySamples = 0;
        double tailSeconds = 0.0;
        bool stateLoaded = false;
        juce::String stateSource;
        juce::String statePath;
        juce::String stateHash;
        juce::int64 stateBytes = 0;
        juce::String stateLabel;
    };

    struct PluginStateLoadResult
    {
        bool requested = false;
        bool loaded = false;
        juce::String source;
        juce::String path;
        juce::String hash;
        juce::int64 bytes = 0;
        juce::String label;
        juce::MemoryBlock state;
    };

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
    };

    static juce::var getObject(const juce::var& source, const juce::String& name)
    {
        if (auto* obj = source.getDynamicObject())
            return obj->getProperty(juce::Identifier(name));
        return {};
    }

    static juce::String getString(const juce::var& source, const juce::String& name, const juce::String& fallback = {})
    {
        const auto value = getObject(source, name);
        return value.isVoid() || value.isUndefined() ? fallback : value.toString();
    }

    static double getDouble(const juce::var& source, const juce::String& name, double fallback)
    {
        const auto value = getObject(source, name);
        return value.isVoid() || value.isUndefined() ? fallback : static_cast<double>(value);
    }

    static bool getBool(const juce::var& source, const juce::String& name, bool fallback)
    {
        const auto value = getObject(source, name);
        return value.isVoid() || value.isUndefined() ? fallback : static_cast<bool>(value);
    }

    static juce::String sanitizeFileToken(juce::String text)
    {
        text = text.trim();
        if (text.isEmpty())
            text = "Untitled";

        const juce::String illegal = "\\/:*?\"<>|";
        for (int i = 0; i < illegal.length(); ++i)
            text = text.replaceCharacter(illegal[i], '_');

        text = text.replace(" ", "_").replace("\t", "_").replace("\n", "_").replace("\r", "_");
        while (text.contains("__"))
            text = text.replace("__", "_");
        return text.substring(0, 160).trimCharactersAtEnd("_");
    }

    bool fail(const juce::String& message, juce::String& responseText)
    {
        if (response != nullptr)
        {
            response->setProperty("status", "error");
            response->setProperty("error", message);
            responseText = juce::JSON::toString(juce::var(response.release()), true);
        }
        else
        {
            responseText = "{\"status\":\"error\",\"error\":\"" + message + "\"}";
        }

        return false;
    }

    bool finish(juce::String& responseText)
    {
        responseText = juce::JSON::toString(juce::var(response.release()), true);
        outDir.getChildFile("response.json").replaceWithText(responseText);
        return false;
    }

    bool loadDry()
    {
        juce::String error;
        const auto sources = getObject(job, "sources");
        if (sources.isObject())
        {
            if (!loadSource("dryA", getObject(sources, "dryA"), dryAsset, error))
                return false;
            if (!loadSource("dryB", getObject(sources, "dryB"), dryBAsset, error))
                return false;
            if (!loadSource("dryC", getObject(sources, "dryC"), dryCAsset, error))
                return false;

            if (dryAsset == nullptr)
            {
                response->setProperty("error", "sources.dryA is required for schemaVersion 2 jobs.");
                return false;
            }

            loadDisplaySlots();
            return true;
        }

        const auto drySpec = getObject(job, "dry");
        const auto generated = getObject(job, "generateSignal");
        if (drySpec.isObject() && getString(drySpec, "path").isNotEmpty())
        {
            dryAsset = std::make_unique<Asset>();
            if (!readAudioFile(juce::File(getString(drySpec, "path")), *dryAsset, error))
            {
                response->setProperty("error", "Dry import failed: " + error);
                return false;
            }
            applySelection(*dryAsset, getObject(drySpec, "selection"));
            if (!applySourceEdits("dryA", *dryAsset, drySpec, error))
            {
                response->setProperty("error", "Dry edit failed: " + error);
                return false;
            }
            loadDisplaySlots();
            return true;
        }

        if (generated.isObject())
        {
            dryAsset = std::make_unique<Asset>(generateSignal(generated));
            loadDisplaySlots();
            return dryAsset->buffer.getNumSamples() > 0;
        }

        const auto signal = getObject(job, "signal");
        if (signal.isObject())
        {
            dryAsset = std::make_unique<Asset>(generateSignal(signal));
            loadDisplaySlots();
            return dryAsset->buffer.getNumSamples() > 0;
        }

        response->setProperty("error", "Job needs dry.path or generateSignal.");
        return false;
    }

    bool loadSource(const juce::String& key, const juce::var& spec, AssetPtr& asset, juce::String& error)
    {
        if (!spec.isObject())
            return true;

        const auto path = getString(spec, "path");
        if (path.isNotEmpty())
        {
            asset = std::make_unique<Asset>();
            if (!readAudioFile(juce::File(path), *asset, error))
            {
                response->setProperty("error", key + " import failed: " + error);
                return false;
            }
        }
        else
        {
            const auto signal = getObject(spec, "generateSignal").isObject() ? getObject(spec, "generateSignal") : getObject(spec, "signal");
            if (signal.isObject())
                asset = std::make_unique<Asset>(generateSignal(signal));
        }

        if (asset == nullptr)
            return true;

        asset->name = key.toUpperCase() + " " + asset->name;
        applySelection(*asset, getObject(spec, "selection"));
        return applySourceEdits(key, *asset, spec, error);
    }

    bool applySourceEdits(const juce::String& key, Asset& asset, const juce::var& spec, juce::String& error)
    {
        const auto edits = getObject(spec, "edits").isObject() ? getObject(spec, "edits") : getObject(spec, "edit");
        if (!edits.isObject())
            return true;

        auto derivedDir = outDir.getChildFile("derived_audio");
        derivedDir.createDirectory();
        return applyEditToAsset(asset, edits, derivedDir, key.toUpperCase().replace("DRY", "DRY "), error);
    }

    void loadDisplaySlots()
    {
        displaySlotSources = { "dryA", "wetA", "wetB" };
        const auto slots = getObject(job, "displaySlots");
        if (auto* arr = slots.getArray())
        {
            int fallbackIndex = 0;
            for (const auto& item : *arr)
            {
                int slotIndex = fallbackIndex;
                juce::String source;
                if (item.isObject())
                {
                    slotIndex = juce::jlimit(0, 2, static_cast<int>(getDouble(item, "slot", static_cast<double>(fallbackIndex + 1))) - 1);
                    source = getString(item, "source").trim().toLowerCase();
                }
                else
                {
                    source = item.toString().trim().toLowerCase();
                }

                if (source.isNotEmpty() && isKnownSourceId(source))
                    displaySlotSources[static_cast<size_t>(slotIndex)] = normaliseSourceId(source);

                ++fallbackIndex;
                if (fallbackIndex >= 3)
                    break;
            }
        }
    }

    static bool isKnownSourceId(const juce::String& source)
    {
        const auto id = normaliseSourceId(source);
        return id == "dryA" || id == "dryB" || id == "dryC"
            || id == "wetA" || id == "wetB" || id == "wetC";
    }

    static juce::String normaliseSourceId(juce::String source)
    {
        source = source.trim().replace(" ", "").replace("_", "").replace("-", "").toLowerCase();
        if (source == "dry" || source == "drya")
            return "dryA";
        if (source == "dryb")
            return "dryB";
        if (source == "dryc")
            return "dryC";
        if (source == "weta" || source == "plugina")
            return "wetA";
        if (source == "wetb" || source == "pluginb")
            return "wetB";
        if (source == "wetc" || source == "pluginc")
            return "wetC";
        return source;
    }

    static TerrainCamera terrainCameraForToken(juce::String token)
    {
        token = token.trim().toLowerCase().replace("-", "_").replace(" ", "_");
        if (token == "front_high" || token == "fronthigh")
            return TerrainCamera::frontHigh;
        if (token == "front_low" || token == "frontlow")
            return TerrainCamera::frontLow;
        if (token == "side_low" || token == "sidelow")
            return TerrainCamera::sideLow;
        if (token == "side_high" || token == "sidehigh")
            return TerrainCamera::sideHigh;
        return TerrainCamera::diagonal;
    }

    static juce::String terrainCameraToken(TerrainCamera camera)
    {
        switch (camera)
        {
            case TerrainCamera::frontHigh: return "front_high";
            case TerrainCamera::frontLow:  return "front_low";
            case TerrainCamera::sideLow:   return "side_low";
            case TerrainCamera::sideHigh:  return "side_high";
            case TerrainCamera::diagonal:
            default:                       return "diagonal";
        }
    }

    static SpatialWindow spatialWindowForToken(juce::String token)
    {
        token = token.trim().toLowerCase().replace("-", "_").replace(" ", "_");
        if (token == "attack" || token == "attack_window" || token == "onset")
            return SpatialWindow::attack;
        if (token == "body" || token == "body_window" || token == "middle")
            return SpatialWindow::body;
        if (token == "tail" || token == "tail_window" || token == "decay")
            return SpatialWindow::tail;
        return SpatialWindow::full;
    }

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

    TerrainCamera getJobTerrainCamera() const
    {
        const auto exportSpec = getObject(job, "export");
        return terrainCameraForToken(getString(exportSpec, "terrainCamera",
                                     getString(exportSpec, "terrainView",
                                     getString(job, "terrainCamera",
                                     getString(job, "terrainView", "diagonal")))));
    }

    bool getJobTerrainTimeReversed() const
    {
        const auto exportSpec = getObject(job, "export");
        return getBool(exportSpec, "terrainTimeReversed",
               getBool(exportSpec, "timeReversed",
               getBool(exportSpec, "reverseTime",
               getBool(job, "terrainTimeReversed",
               getBool(job, "timeReversed",
               getBool(job, "reverseTime", false))))));
    }

    SpatialWindow getJobSpatialWindow() const
    {
        const auto exportSpec = getObject(job, "export");
        return spatialWindowForToken(getString(exportSpec, "spatialWindow",
                                     getString(exportSpec, "timeWindow",
                                     getString(exportSpec, "window",
                                     getString(job, "spatialWindow",
                                     getString(job, "timeWindow",
                                     getString(job, "window", "full")))))));
    }

    float getJobSpatialTimePositionSeconds() const
    {
        const auto exportSpec = getObject(job, "export");
        return static_cast<float>(getDouble(exportSpec, "spatialTimePositionSeconds",
                                  getDouble(exportSpec, "spatialTimePosition",
                                  getDouble(exportSpec, "timePositionSeconds",
                                  getDouble(exportSpec, "timePosition",
                                  getDouble(job, "spatialTimePositionSeconds",
                                  getDouble(job, "spatialTimePosition",
                                  getDouble(job, "timePositionSeconds",
                                  getDouble(job, "timePosition", -1.0)))))))));
    }

    float getJobCrystalYawRadians() const
    {
        const auto exportSpec = getObject(job, "export");
        return static_cast<float>(getDouble(exportSpec, "crystalYawRadians",
                                  getDouble(exportSpec, "crystalYaw",
                                  getDouble(exportSpec, "cameraYaw",
                                  getDouble(exportSpec, "yaw",
                                  getDouble(job, "crystalYawRadians",
                                  getDouble(job, "crystalYaw",
                                  getDouble(job, "cameraYaw",
                                  getDouble(job, "yaw", -0.68)))))))));
    }

    float getJobCrystalPitchRadians() const
    {
        const auto exportSpec = getObject(job, "export");
        return static_cast<float>(getDouble(exportSpec, "crystalPitchRadians",
                                  getDouble(exportSpec, "crystalPitch",
                                  getDouble(exportSpec, "cameraPitch",
                                  getDouble(exportSpec, "pitch",
                                  getDouble(job, "crystalPitchRadians",
                                  getDouble(job, "crystalPitch",
                                  getDouble(job, "cameraPitch",
                                  getDouble(job, "pitch", 0.54)))))))));
    }

    bool getJobSharedScale() const
    {
        const auto exportSpec = getObject(job, "export");
        return getBool(exportSpec, "sharedScale",
               getBool(job, "sharedScale", true));
    }

    static juce::Colour parseHexColour(juce::String text, juce::Colour fallback)
    {
        text = text.trim();
        if (text.startsWithChar('#'))
            text = text.substring(1);
        if (text.length() == 6)
            text = "ff" + text;
        if (text.length() != 8)
            return fallback;

        uint32_t value = 0;
        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            const int digit = c >= '0' && c <= '9' ? static_cast<int>(c - '0')
                            : c >= 'a' && c <= 'f' ? static_cast<int>(c - 'a' + 10)
                            : c >= 'A' && c <= 'F' ? static_cast<int>(c - 'A' + 10)
                            : -1;
            if (digit < 0)
                return fallback;
            value = (value << 4) | static_cast<uint32_t>(digit);
        }
        return juce::Colour(value);
    }

    BandHighlightConfig getBandHighlightConfig(const juce::var& spec = {}) const
    {
        auto config = AudioDoctorFigureRenderer::makeDefaultBandHighlightConfig();
        auto bandSpec = getObject(spec, "bandHighlight");
        if (bandSpec.isVoid() || bandSpec.isUndefined())
        {
            const auto exportSpec = getObject(job, "export");
            bandSpec = getObject(exportSpec, "bandHighlight");
        }
        if (bandSpec.isVoid() || bandSpec.isUndefined())
            bandSpec = getObject(job, "bandHighlight");

        if (bandSpec.isVoid() || bandSpec.isUndefined())
            return config;

        config.enabled = getBool(bandSpec, "enabled", true);
        config.dimInactiveBands = getBool(bandSpec, "dimInactiveBands", config.dimInactiveBands);
        config.inactiveAlpha = static_cast<float>(getDouble(bandSpec, "inactiveAlpha", config.inactiveAlpha));
        config.overlayAlpha = static_cast<float>(getDouble(bandSpec, "overlayAlpha", config.overlayAlpha));

        if (auto* bands = getObject(bandSpec, "bands").getArray())
        {
            config.bands.clear();
            for (const auto& item : *bands)
            {
                BandHighlightBand band;
                band.id = getString(item, "id", "band").trim().toLowerCase();
                band.label = getString(item, "label", band.id);
                band.minHz = static_cast<float>(getDouble(item, "minHz", 20.0));
                band.maxHz = static_cast<float>(getDouble(item, "maxHz", 20000.0));
                band.colour = parseHexColour(getString(item, "color", getString(item, "colour")), juce::Colour(0xFFF3C64E));
                band.active = true;
                config.bands.push_back(band);
            }
        }

        juce::StringArray activeBands;
        if (auto* active = getObject(bandSpec, "activeBands").getArray())
            for (const auto& item : *active)
                activeBands.add(item.toString().trim().toLowerCase());
        else
        {
            const auto single = getString(bandSpec, "activeBand", getString(bandSpec, "band"));
            if (single.isNotEmpty())
                activeBands.add(single.trim().toLowerCase());
        }

        if (!activeBands.isEmpty())
        {
            for (auto& band : config.bands)
                band.active = activeBands.contains("all") || activeBands.contains(band.id.toLowerCase());
        }

        return config;
    }

    MaskingFusionSettings getMaskingFusionSettings(const juce::var& spec = {}) const
    {
        MaskingFusionSettings settings;
        auto applyFigureTokenDefaults = [&] (juce::String token)
        {
            token = token.trim().toLowerCase().replace("-", "_").replace(" ", "_");
            if (token.contains("dodecahedron"))
            {
                settings.figureType = "dodecahedron_crystal";
                settings.preferredBandCount = 24;
                settings.bandScale = "bark_24";
                settings.criticalBandMode = "bark_24";
            }
            else if (token.contains("critical_band_crystal"))
            {
                settings.figureType = "critical_band_crystal";
                settings.preferredBandCount = 24;
                settings.bandScale = "bark_24";
                settings.criticalBandMode = "bark_24";
            }
        };

        auto cfg = getObject(spec, "layerFitFusion");
        if (cfg.isVoid() || cfg.isUndefined())
            cfg = getObject(spec, "maskingFusion");
        if (cfg.isVoid() || cfg.isUndefined())
            cfg = getObject(job, "layerFitFusion");
        if (cfg.isVoid() || cfg.isUndefined())
            cfg = getObject(job, "maskingFusion");
        if (cfg.isVoid() || cfg.isUndefined())
        {
            for (const auto& view : getViews())
                applyFigureTokenDefaults(view);
            return settings;
        }

        settings.figureType = getString(cfg, "figureType", settings.figureType);
        settings.preferredBandCount = static_cast<int>(getDouble(cfg, "preferredBandCount",
                                                     getDouble(cfg, "bandCount",
                                                     getDouble(cfg, "criticalBandCount", settings.preferredBandCount))));
        settings.bandScale = getString(cfg, "frequencyScale",
                             getString(cfg, "bandScale",
                             getString(cfg, "criticalBandScale", settings.bandScale)));
        settings.criticalBandMode = getString(cfg, "criticalBandMode", settings.criticalBandMode);
        settings.maskingModel = getString(cfg, "maskingModel", settings.maskingModel);
        settings.integrationTimeMs = static_cast<float>(getDouble(cfg, "integrationTimeMs", settings.integrationTimeMs));
        settings.dominanceThresholdDb = static_cast<float>(getDouble(cfg, "dominanceThresholdDb", settings.dominanceThresholdDb));
        settings.gateDbBelowPeak = static_cast<float>(getDouble(cfg, "gateDbBelowPeak", settings.gateDbBelowPeak));
        settings.upwardSpread = getBool(cfg, "upwardSpread", settings.upwardSpread);
        settings.upwardSpreadWeight = static_cast<float>(getDouble(cfg, "upwardSpreadWeight", settings.upwardSpreadWeight));
        settings.upwardSpreadSlopeDbPerBark = static_cast<float>(getDouble(cfg, "upwardSpreadSlopeDbPerBark",
                                                               getDouble(cfg, "upwardSlopeDbPerBark", settings.upwardSpreadSlopeDbPerBark)));
        settings.downwardSpreadSlopeDbPerBark = static_cast<float>(getDouble(cfg, "downwardSpreadSlopeDbPerBark",
                                                                 getDouble(cfg, "downwardSlopeDbPerBark", settings.downwardSpreadSlopeDbPerBark)));
        settings.maskingRiskRangeDb = static_cast<float>(getDouble(cfg, "maskingRiskRangeDb", settings.maskingRiskRangeDb));
        settings.interauralCoherenceWeight = static_cast<float>(getDouble(cfg, "interauralCoherenceWeight", settings.interauralCoherenceWeight));
        settings.estimatedMaskedThreshold = getBool(cfg, "estimatedMaskedThreshold", false);
        settings.showPairwiseRisk = getBool(cfg, "showPairwiseRisk", settings.showPairwiseRisk);
        settings.showDominantSource = getBool(cfg, "showDominantSource", settings.showDominantSource);
        settings.showFusionTendency = getBool(cfg, "showFusionTendency", settings.showFusionTendency);
        applyFigureTokenDefaults(settings.figureType);
        return settings;
    }

    juce::var getLayerFitFusionConfig(const juce::var& spec = {}) const
    {
        auto cfg = getObject(spec, "layerFitFusion");
        if (cfg.isVoid() || cfg.isUndefined())
            cfg = getObject(spec, "maskingFusion");
        if (cfg.isVoid() || cfg.isUndefined())
            cfg = getObject(job, "layerFitFusion");
        if (cfg.isVoid() || cfg.isUndefined())
            cfg = getObject(job, "maskingFusion");
        return cfg;
    }

    std::array<juce::String, 3> getMaskingFusionSources(const juce::var& spec = {}) const
    {
        std::array<juce::String, 3> sources = displaySlotSources;
        auto cfg = getLayerFitFusionConfig(spec);

        if (cfg.isObject())
        {
            const bool explicitlySetsLayerSources = !getObject(cfg, "sources").isVoid()
                || getString(cfg, "source1").isNotEmpty()
                || getString(cfg, "source2").isNotEmpty()
                || getString(cfg, "source3").isNotEmpty()
                || getString(cfg, "stem1").isNotEmpty()
                || getString(cfg, "stem2").isNotEmpty()
                || getString(cfg, "stem3").isNotEmpty();
            if (explicitlySetsLayerSources)
                sources = { juce::String(), juce::String(), juce::String() };

            if (auto* arr = getObject(cfg, "sources").getArray())
            {
                for (int i = 0; i < juce::jmin(3, arr->size()); ++i)
                {
                    const auto id = normaliseSourceId((*arr)[i].isObject()
                        ? getString((*arr)[i], "source", getString((*arr)[i], "id", getString((*arr)[i], "slot")))
                        : (*arr)[i].toString());
                    if (isKnownSourceId(id))
                        sources[static_cast<size_t>(i)] = id;
                }
            }

            const char* keys[3] = { "source1", "source2", "source3" };
            const char* altKeys[3] = { "stem1", "stem2", "stem3" };
            for (int i = 0; i < 3; ++i)
            {
                const auto id = normaliseSourceId(getString(cfg, keys[i], getString(cfg, altKeys[i])));
                if (isKnownSourceId(id))
                    sources[static_cast<size_t>(i)] = id;
            }

            const char* legacyKeys[3] = { "sourceA", "sourceB", "mixSource" };
            const char* legacyAltKeys[3] = { "a", "b", "mix" };
            for (int i = 0; i < 3; ++i)
            {
                const auto id = normaliseSourceId(getString(cfg, legacyKeys[i], getString(cfg, legacyAltKeys[i])));
                if (isKnownSourceId(id))
                    sources[static_cast<size_t>(i)] = id;
            }
        }

        return sources;
    }

    juce::String getLayerFitFusionBounceSource(const juce::var& spec = {}) const
    {
        auto cfg = getLayerFitFusionConfig(spec);
        if (!cfg.isObject())
            return {};

        auto id = getString(cfg, "bounceSource",
                  getString(cfg, "bounce",
                  getString(cfg, "mixSource")));
        const auto lower = id.trim().toLowerCase().replace(" ", "").replace("_", "").replace("-", "");
        if (lower.isEmpty() || lower == "auto" || lower == "autobounce" || lower == "autobounceselectedstems"
            || lower == "linear" || lower == "linearsum")
            return {};

        id = normaliseSourceId(id);
        return isKnownSourceId(id) ? id : juce::String();
    }

    bool getThesisTimeReversed(const juce::var& spec) const
    {
        const auto value = !getObject(spec, "terrainTimeReversed").isVoid() ? getObject(spec, "terrainTimeReversed")
                         : (!getObject(spec, "timeReversed").isVoid() ? getObject(spec, "timeReversed")
                         : getObject(spec, "reverseTime"));
        if (value.isVoid() || value.isUndefined())
            return getJobTerrainTimeReversed();
        const auto text = value.toString().trim().toLowerCase();
        if (text == "auto")
            return getJobTerrainTimeReversed();
        if (text == "flip" || text == "flipped" || text == "flip_time" || text == "reversed" || text == "reverse")
            return true;
        if (text == "normal" || text == "forward")
            return false;
        return static_cast<bool>(value);
    }

    Asset* getAssetById(const juce::String& source) const
    {
        const auto id = normaliseSourceId(source);
        if (id == "dryA") return dryAsset.get();
        if (id == "dryB") return dryBAsset.get();
        if (id == "dryC") return dryCAsset.get();
        if (id == "wetA") return wetA.get();
        if (id == "wetB") return wetB.get();
        if (id == "wetC") return wetC.get();
        return nullptr;
    }

    juce::String getLayerFitBounceWarning(const std::array<juce::String, 3>& sources,
                                          const juce::String& bounceSource) const
    {
        if (bounceSource.isNotEmpty())
            return {};

        double sampleRate = 0.0;
        for (const auto& source : sources)
        {
            const auto* asset = getAssetById(source);
            if (asset == nullptr || asset->buffer.getNumSamples() <= 0)
                continue;

            if (sampleRate <= 0.0)
                sampleRate = asset->sampleRate;
            else if (std::abs(asset->sampleRate - sampleRate) > 0.5)
                return "Auto Bounce note: selected stems have different sample rates; analysis resamples to the first valid stem sample rate before summing.";
        }

        return {};
    }

    static juce::String labelForSourceId(const juce::String& source)
    {
        const auto id = normaliseSourceId(source);
        if (id == "dryA") return "DRY A";
        if (id == "dryB") return "DRY B";
        if (id == "dryC") return "DRY C";
        if (id == "wetA") return "WET A";
        if (id == "wetB") return "WET B";
        if (id == "wetC") return "WET C";
        return source;
    }

    Asset generateSignal(const juce::var& spec)
    {
        const auto params = getObject(spec, "params");
        auto readString = [&](const juce::String& name, const juce::String& fallback)
        {
            return getString(params, name, getString(spec, name, fallback));
        };
        auto readDouble = [&](const juce::String& name, double fallback)
        {
            return getDouble(params, name, getDouble(spec, name, fallback));
        };
        auto readInt = [&](const juce::String& name, int fallback)
        {
            return static_cast<int>(std::round(readDouble(name, static_cast<double>(fallback))));
        };
        auto readBool = [&](const juce::String& name, bool fallback)
        {
            return getBool(params, name, getBool(spec, name, fallback));
        };

        GeneratedSignalSpec signal;
        signal.type = readString("type", "impulse");
        signal.preset = readString("preset", "thesis_default");
        signal.sampleRate = readDouble("sampleRate", 48000.0);
        signal.channels = readInt("channels", 2);
        signal.seconds = readDouble("seconds", signal.type.equalsIgnoreCase("sweep") ? 6.0 : 2.0);
        signal.levelDb = readDouble("levelDb", readDouble("level", -6.0));
        signal.frequencyHz = readDouble("frequencyHz", readDouble("frequency", 1000.0));
        signal.startHz = readDouble("startHz", readDouble("startFrequencyHz", 20.0));
        signal.endHz = readDouble("endHz", readDouble("endFrequencyHz", 20000.0));
        signal.phaseDegrees = readDouble("phaseDegrees", readDouble("phase", 0.0));
        signal.harmonicCount = readInt("harmonicCount", readInt("harmonics", 6));
        signal.harmonicRolloffDb = readDouble("harmonicRolloffDb", readDouble("rolloffDb", 6.0));
        signal.seed = readInt("seed", 0xAD10);
        signal.invert = readBool("invert", false);
        signal.noiseAmount = readDouble("noiseAmount", signal.noiseAmount);
        signal.modRateHz = readDouble("modRateHz", signal.modRateHz);
        signal.modDepth = readDouble("modDepth", signal.modDepth);
        signal.curve = readDouble("curve", signal.curve);
        signal.bodyHz = readDouble("bodyHz", signal.bodyHz);
        signal.bodyDecayMs = readDouble("bodyDecayMs", signal.bodyDecayMs);
        signal.crackHz = readDouble("crackHz", signal.crackHz);
        signal.crackAmount = readDouble("crackAmount", signal.crackAmount);
        signal.noiseBandLowHz = readDouble("noiseBandLowHz", signal.noiseBandLowHz);
        signal.noiseBandHighHz = readDouble("noiseBandHighHz", signal.noiseBandHighHz);
        signal.transientMs = readDouble("transientMs", signal.transientMs);
        signal.fundamentalHz = readDouble("fundamentalHz", signal.fundamentalHz);
        signal.decayMs = readDouble("decayMs", signal.decayMs);
        signal.tailBandLowHz = readDouble("tailBandLowHz", signal.tailBandLowHz);
        signal.tailBandHighHz = readDouble("tailBandHighHz", signal.tailBandHighHz);
        signal.damping = readDouble("damping", signal.damping);
        signal.earlyReflectionAmount = readDouble("earlyReflectionAmount", signal.earlyReflectionAmount);
        signal.stereoWidth = readDouble("stereoWidth", signal.stereoWidth);
        signal.chargeStartSec = readDouble("chargeStartSec", signal.chargeStartSec);
        signal.shotTimeSec = readDouble("shotTimeSec", signal.shotTimeSec);
        signal.tailStartSec = readDouble("tailStartSec", signal.tailStartSec);
        signal.stageBalance = readDouble("stageBalance", signal.stageBalance);
        signal.fundamentalAHz = readDouble("fundamentalAHz", signal.fundamentalAHz);
        signal.fundamentalBHz = readDouble("fundamentalBHz", signal.fundamentalBHz);
        signal.detuneCents = readDouble("detuneCents", signal.detuneCents);
        signal.overlapAmount = readDouble("overlapAmount", signal.overlapAmount);
        return makeGeneratedSignalAsset(signal);
    }

    void applySelection(Asset& asset, const juce::var& selection)
    {
        if (!selection.isObject())
            return;

        const double start = juce::jmax(0.0, getDouble(selection, "start", 0.0));
        const double end = getDouble(selection, "end", asset.metrics.durationSeconds);
        if (end <= start)
            return;

        const int startSample = juce::jlimit(0, asset.buffer.getNumSamples(), static_cast<int>(start * asset.sampleRate));
        const int endSample = juce::jlimit(startSample, asset.buffer.getNumSamples(), static_cast<int>(end * asset.sampleRate));
        const int length = endSample - startSample;
        if (length <= 0 || length == asset.buffer.getNumSamples())
            return;

        juce::AudioBuffer<float> clipped(asset.buffer.getNumChannels(), length);
        for (int ch = 0; ch < clipped.getNumChannels(); ++ch)
            clipped.copyFrom(ch, 0, asset.buffer, ch, startSample, length);

        const auto originalSourcePath = asset.sourcePath;
        asset.buffer = std::move(clipped);
        asset.name += " selection";
        asset.editMetadata.valid = true;
        asset.editMetadata.originalSourcePath = originalSourcePath;
        asset.editMetadata.derivedSourcePath = originalSourcePath;
        asset.editMetadata.trimStartSeconds = static_cast<double>(startSample) / juce::jmax(1.0, asset.sampleRate);
        asset.editMetadata.trimEndSeconds = static_cast<double>(endSample) / juce::jmax(1.0, asset.sampleRate);
        asset.editMetadata.createdAt = juce::Time::getCurrentTime().toISO8601(true);
        refreshAnalysis(asset);
    }

    bool loadPluginsAndRender()
    {
        bool ok = true;
        ok = loadPluginAndMaybeRender("pluginA", hostA, wetA, renderInfoA) && ok;
        ok = loadPluginAndMaybeRender("pluginB", hostB, wetB, renderInfoB) && ok;
        ok = loadPluginAndMaybeRender("pluginC", hostC, wetC, renderInfoC) && ok;
        return ok;
    }

    bool loadPluginAndMaybeRender(const juce::String& key, PluginHost& host, AssetPtr& wet, RenderInfo& renderInfo)
    {
        const auto pluginSpec = getObject(job, key);
        if (!pluginSpec.isObject())
            return true;

        juce::String error;
        const auto path = getString(pluginSpec, "path", getString(pluginSpec, "plugin_id_or_path"));
        if (path.isEmpty())
            return true;

        if (!host.loadPluginFromFile(juce::File(path), error))
        {
            response->setProperty("error", "Load " + key + " failed: " + error);
            return false;
        }

        PluginStateLoadResult stateLoad;
        if (!loadPluginState(pluginSpec, stateLoad, error))
        {
            response->setProperty("error", "Load " + key + " state failed: " + error);
            return false;
        }

        if (stateLoad.loaded)
        {
            if (!host.applyState(stateLoad.state, error))
            {
                response->setProperty("error", "Apply " + key + " state failed: " + error);
                return false;
            }
        }

        if (auto* params = getObject(pluginSpec, "params").getArray())
        {
            for (const auto& p : *params)
            {
                const auto paramKey = getString(p, "id", getString(p, "name", getString(p, "index")));
                const float value = static_cast<float>(getDouble(p, "value", 0.0));
                if (!host.setParameterValue(paramKey, value, error))
                {
                    response->setProperty("error", "Set " + key + " parameter failed: " + error);
                    return false;
                }
            }
        }

        juce::MemoryBlock state;
        if (!host.captureCurrentState(state, error))
        {
            response->setProperty("error", "Capture " + key + " state failed: " + error);
            return false;
        }
        if (const auto* desc = host.getCurrentPlugin())
            renderInfo.plugin = *desc;
        renderInfo.stateLoaded = stateLoad.loaded;
        renderInfo.stateSource = stateLoad.source;
        renderInfo.statePath = stateLoad.path;
        renderInfo.stateHash = stateLoad.hash;
        renderInfo.stateBytes = stateLoad.bytes;
        renderInfo.stateLabel = stateLoad.label;

        const auto renderSpec = getObject(job, "render");
        const auto wetId = wetIdForPluginKey(key);
        const auto requestedSlot = getString(renderSpec, "slot");
        const bool shouldRender = !renderSpec.isObject()
                               || requestedSlot.isEmpty()
                               || requestedSlot.equalsIgnoreCase(key.fromLastOccurrenceOf("plugin", false, false))
                               || normaliseSourceId(requestedSlot) == wetId;
        if (!shouldRender)
            return true;

        AssetPtr renderInput;
        if (!makeRenderInputAsset(wetId, renderInput, error))
        {
            response->setProperty("error", "Render " + key + " input failed: " + error);
            return false;
        }

        const double tailSeconds = getTailSeconds(renderSpec);
        auto result = host.renderOffline(*renderInput, &state, 512, tailSeconds);
        if (result.error.isNotEmpty())
        {
            response->setProperty("error", "Render " + key + " failed: " + result.error);
            return false;
        }

        wet = std::make_unique<Asset>(std::move(result.wet));
        wet->name = labelForSourceId(wetId) + " " + wet->name;
        wet->groupDelay = computeTransferGroupDelay(renderInput->buffer, wet->buffer, renderInput->sampleRate);
        if (wetId == "wetA")
            renderReferenceA = std::move(renderInput);
        else if (wetId == "wetB")
            renderReferenceB = std::move(renderInput);
        else if (wetId == "wetC")
            renderReferenceC = std::move(renderInput);

        renderInfo.plugin = result.pluginDescription;
        renderInfo.latencySamples = result.latencySamples;
        renderInfo.tailSeconds = result.tailSeconds;
        renderInfo.stateLoaded = stateLoad.loaded;
        renderInfo.stateSource = stateLoad.source;
        renderInfo.statePath = stateLoad.path;
        renderInfo.stateHash = stateLoad.hash;
        renderInfo.stateBytes = stateLoad.bytes;
        renderInfo.stateLabel = stateLoad.label;
        renderInfo.valid = true;
        return true;
    }

    bool loadPluginState(const juce::var& pluginSpec, PluginStateLoadResult& result, juce::String& error) const
    {
        result = {};

        auto path = getString(pluginSpec, "statePath",
                    getString(pluginSpec, "presetPath"));
        const auto presetValue = getString(pluginSpec, "preset");
        if (path.isEmpty() && presetValue.isNotEmpty())
        {
            const juce::File presetFile(presetValue);
            if (presetFile.existsAsFile())
                path = presetValue;
            else
                result.label = presetValue;
        }

        if (path.isNotEmpty())
        {
            result.requested = true;
            const juce::File stateFile(path);
            if (!stateFile.existsAsFile())
            {
                error = "State/preset file not found: " + path;
                return false;
            }

            if (isBeamPrivatePresetFile(pluginSpec, stateFile))
            {
                error = "BEAM .preset/.nodepreset files are not native plugin state. "
                        "Load the preset in BEAM, capture/export pluginStateBase64 or a GOODMETER state file, then use that state for Job Runner.";
                return false;
            }

            if (!stateFile.loadFileAsData(result.state))
            {
                error = "Could not read state/preset file: " + path;
                return false;
            }

            result.loaded = true;
            result.source = getString(pluginSpec, "statePath").isNotEmpty() ? "statePath" : "presetPath";
            if (result.source == "presetPath" && getString(pluginSpec, "statePath").isEmpty()
                && getString(pluginSpec, "presetPath").isEmpty())
                result.source = "preset";
            result.path = stateFile.getFullPathName();
            result.bytes = static_cast<juce::int64>(result.state.getSize());
            result.hash = hashMemoryBlockFnv1a64(result.state);
            if (result.label.isEmpty())
                result.label = stateFile.getFileNameWithoutExtension();
            error.clear();
            return true;
        }

        const auto encoded = getString(pluginSpec, "pluginStateBase64");
        if (encoded.isNotEmpty())
        {
            result.requested = true;
            if (!result.state.fromBase64Encoding(encoded))
            {
                error = "pluginStateBase64 is not valid base64.";
                return false;
            }

            result.loaded = true;
            result.source = "pluginStateBase64";
            result.bytes = static_cast<juce::int64>(result.state.getSize());
            result.hash = hashMemoryBlockFnv1a64(result.state);
            result.label = getString(pluginSpec, "preset", getString(pluginSpec, "stateName", "base64_state"));
            error.clear();
            return true;
        }

        error.clear();
        return true;
    }

    static bool isBeamPrivatePresetFile(const juce::var& pluginSpec, const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        if (ext != ".preset" && ext != ".nodepreset")
            return false;

        const auto pluginText = (getString(pluginSpec, "path") + " "
                              + getString(pluginSpec, "plugin") + " "
                              + getString(pluginSpec, "name")).toLowerCase();
        const auto presetText = (file.getFullPathName() + " " + file.getFileName()).toLowerCase();
        return pluginText.contains("beam") || pluginText.contains("lunacy audio")
            || presetText.contains("lunacy audio/beam");
    }

    static juce::String wetIdForPluginKey(const juce::String& key)
    {
        if (key.equalsIgnoreCase("pluginB"))
            return "wetB";
        if (key.equalsIgnoreCase("pluginC"))
            return "wetC";
        return "wetA";
    }

    juce::StringArray getRenderInputIds(const juce::String& wetId) const
    {
        juce::StringArray ids;
        const auto routing = getObject(getObject(job, "renderRouting"), wetId);
        if (routing.isObject())
        {
            if (auto* arr = getObject(routing, "inputs").getArray())
            {
                for (const auto& item : *arr)
                {
                    const auto id = normaliseSourceId(item.toString());
                    if (id.startsWith("dry") && isKnownSourceId(id))
                        ids.addIfNotAlreadyThere(id);
                }
            }

            const auto single = normaliseSourceId(getString(routing, "input"));
            if (single.startsWith("dry") && isKnownSourceId(single))
                ids.addIfNotAlreadyThere(single);
        }

        if (ids.isEmpty())
            ids.add("dryA");

        if (!allowsMixedRenderInputs() && ids.size() > 1)
        {
            const auto first = ids[0];
            ids.clear();
            ids.add(first);
        }

        return ids;
    }

    bool allowsMixedRenderInputs() const
    {
        const auto routing = getObject(job, "renderRouting");
        const auto mode = getString(routing, "mode",
                         getString(routing, "routingMode",
                         getString(job, "routingMode", "mix"))).trim().toLowerCase();
        return !(mode == "controlled" || mode == "one_to_one" || mode == "one-to-one");
    }

    bool makeRenderInputAsset(const juce::String& wetId, AssetPtr& result, juce::String& error) const
    {
        const auto inputIds = getRenderInputIds(wetId);
        std::vector<const Asset*> inputs;
        juce::StringArray usedIds;
        for (const auto& id : inputIds)
        {
            if (const auto* asset = getAssetById(id))
            {
                inputs.push_back(asset);
                usedIds.add(id);
            }
        }

        if (inputs.empty())
        {
            error = "No available dry input for " + labelForSourceId(wetId);
            return false;
        }

        if (inputs.size() == 1)
        {
            result = std::make_unique<Asset>(*inputs.front());
            return true;
        }

        const double sampleRate = inputs.front()->sampleRate;
        int maxSamples = 0;
        for (const auto* asset : inputs)
        {
            if (std::abs(asset->sampleRate - sampleRate) > 1.0)
            {
                error = "Multi-input render currently requires matching sample rates.";
                return false;
            }
            maxSamples = juce::jmax(maxSamples, asset->buffer.getNumSamples());
        }

        Asset mixed;
        mixed.name = labelForSourceId(wetId) + " mixed input " + usedIds.joinIntoString("+");
        mixed.sourcePath = "mixed:" + usedIds.joinIntoString("+");
        mixed.sampleRate = sampleRate;
        mixed.buffer.setSize(2, maxSamples);
        mixed.buffer.clear();

        const float gain = 1.0f / static_cast<float>(inputs.size());
        for (const auto* asset : inputs)
        {
            auto stereo = toStereoBuffer(asset->buffer);
            const int samples = juce::jmin(maxSamples, stereo.getNumSamples());
            for (int ch = 0; ch < mixed.buffer.getNumChannels(); ++ch)
                mixed.buffer.addFrom(ch, 0, stereo, ch, 0, samples, gain);
        }

        refreshAnalysis(mixed);
        result = std::make_unique<Asset>(std::move(mixed));
        return true;
    }

    static double getTailSeconds(const juce::var& renderSpec)
    {
        const auto tail = getObject(renderSpec, "tailSeconds");
        if (tail.isString() && tail.toString().equalsIgnoreCase("auto"))
            return 1.0;
        if (!tail.isVoid() && !tail.isUndefined())
            return static_cast<double>(tail);
        return 1.0;
    }

    void refreshTransfer()
    {
        auto refreshFor = [this](AssetPtr& wet, const AssetPtr& reference)
        {
            const Asset* ref = reference != nullptr ? reference.get() : dryAsset.get();
            if (wet == nullptr || ref == nullptr)
                return;

            wet->groupDelay = computeTransferGroupDelay(ref->buffer, wet->buffer, ref->sampleRate);
        };

        refreshFor(wetA, renderReferenceA);
        refreshFor(wetB, renderReferenceB);
        refreshFor(wetC, renderReferenceC);
    }

    void writeOutputs()
    {
        auto dataDir = outDir.getChildFile("data");
        auto figureDir = outDir.getChildFile("figures");
        dataDir.createDirectory();
        figureDir.createDirectory();

        juce::Array<juce::var> figures;
        juce::Array<juce::var> dataFiles;
        juce::Array<juce::var> thesisFigures;

        const auto views = getViews();
        const auto preset = getExportPreset();
        for (const auto& view : views)
        {
            const auto png = figureDir.getChildFile(sanitizeFileToken(sessionId + "_" + view + "_" + preset)).withFileExtension(".png");
            writeFigure(png, view, preset);
            figures.add(png.getFullPathName());
        }

        writeThesisFigures(figureDir, preset, figures, thesisFigures);

        writeAssetCurves(dataDir, "dry", dryAsset.get(), dataFiles);
        writeAssetCurves(dataDir, "dryA", dryAsset.get(), dataFiles);
        writeAssetCurves(dataDir, "dryB", dryBAsset.get(), dataFiles);
        writeAssetCurves(dataDir, "dryC", dryCAsset.get(), dataFiles);
        writeAssetCurves(dataDir, "wetA", wetA.get(), dataFiles);
        writeAssetCurves(dataDir, "wetB", wetB.get(), dataFiles);
        writeAssetCurves(dataDir, "wetC", wetC.get(), dataFiles);
        writeVisibleSlotCurves(dataDir, dataFiles);
        writeApparentAttenuationCsv(dataDir, "display2_vs_display1",
                                    getAssetById(displaySlotSources[0]), getAssetById(displaySlotSources[1]), dataFiles);
        writeApparentAttenuationCsv(dataDir, "display3_vs_display1",
                                    getAssetById(displaySlotSources[0]), getAssetById(displaySlotSources[2]), dataFiles);
        writeApparentAttenuationCsv(dataDir, "wetA_vs_render_reference",
                                    renderReferenceA != nullptr ? renderReferenceA.get() : dryAsset.get(), wetA.get(), dataFiles);
        writeApparentAttenuationCsv(dataDir, "wetB_vs_render_reference",
                                    renderReferenceB != nullptr ? renderReferenceB.get() : dryAsset.get(), wetB.get(), dataFiles);
        writeApparentAttenuationCsv(dataDir, "wetC_vs_render_reference",
                                    renderReferenceC != nullptr ? renderReferenceC.get() : dryAsset.get(), wetC.get(), dataFiles);
        const auto maskingSources = getMaskingFusionSources();
        const auto bounceSource = getLayerFitFusionBounceSource();
        const std::array<const Asset*, 3> layerFitSources
        {
            getAssetById(maskingSources[0]),
            getAssetById(maskingSources[1]),
            getAssetById(maskingSources[2])
        };
        const auto* layerFitBounce = getAssetById(bounceSource);
        juce::StringArray writtenLayerFitRoles;
        auto writeLayerFitData = [&] (const juce::String& role, MaskingFusionSettings settings)
        {
            const auto safeRole = sanitizeFileToken(role);
            if (writtenLayerFitRoles.contains(safeRole))
                return;

            writtenLayerFitRoles.add(safeRole);
            writeLayerFitFusionCsv(dataDir, safeRole, layerFitSources, layerFitBounce, settings, dataFiles);
        };

        const auto baseLayerFitSettings = getMaskingFusionSettings();
        writeLayerFitData("layer_fit_fusion", baseLayerFitSettings);
        for (const auto& view : views)
        {
            auto token = view.trim().toLowerCase().replace("-", "_").replace(" ", "_");
            const bool isLayerFitView = token.contains("dodecahedron") || token.contains("critical_band_crystal")
                                     || token.contains("critical_band_terrain") || token.contains("time_frequency_terrain")
                                     || token.contains("spatial_image") || token.contains("masking") || token.contains("layer_fit")
                                     || (token.contains("fusion") && token.contains("fit"));
            if (!isLayerFitView)
                continue;

            auto settings = baseLayerFitSettings;
            if (token.contains("dodecahedron"))
            {
                settings.figureType = "dodecahedron_crystal";
                settings.preferredBandCount = 24;
                settings.bandScale = "bark_24";
                settings.criticalBandMode = "bark_24";
            }
            else if (token.contains("critical_band_crystal"))
            {
                settings.figureType = "critical_band_crystal";
                settings.preferredBandCount = 24;
                settings.bandScale = "bark_24";
                settings.criticalBandMode = "bark_24";
            }
            else if (token.contains("spatial_image"))
            {
                settings.figureType = "spatial_image";
            }
            else if (token.contains("time_frequency_terrain") || token.contains("terrain"))
            {
                settings.figureType = token.contains("critical_band_terrain") ? "critical_band_terrain" : "terrain";
            }
            else if (settings.figureType.isEmpty())
            {
                settings.figureType = "critical_band_terrain";
            }

            writeLayerFitData("layer_fit_fusion_" + settings.figureType, settings);
        }

        const auto manifestFile = outDir.getChildFile("manifest.json");
        const auto appendixFile = outDir.getChildFile("appendix_table.csv");
        writeAppendixTable(appendixFile, thesisFigures, dataFiles);
        writeManifest(manifestFile, figures, dataFiles, thesisFigures, appendixFile);
        const auto summaryFile = outDir.getChildFile("job_summary.md");
        writeJobSummary(summaryFile, figures, dataFiles, thesisFigures, appendixFile);
        response->setProperty("figures", juce::var(figures));
        response->setProperty("thesisFigures", juce::var(thesisFigures));
        response->setProperty("data", juce::var(dataFiles));
        response->setProperty("manifest", manifestFile.getFullPathName());
        response->setProperty("appendixTable", appendixFile.getFullPathName());
        response->setProperty("summary", summaryFile.getFullPathName());
    }

    void writeVisibleSlotCurves(const juce::File& dataDir, juce::Array<juce::var>& dataFiles) const
    {
        for (size_t i = 0; i < displaySlotSources.size(); ++i)
        {
            const auto* asset = getAssetById(displaySlotSources[i]);
            if (asset == nullptr)
                continue;

            writeAssetCurves(dataDir, "display" + juce::String(static_cast<int>(i + 1)), asset, dataFiles);
        }
    }

    juce::String getExportPreset() const
    {
        const auto exportSpec = getObject(job, "export");
        const auto preset = getString(exportSpec, "preset", getString(exportSpec, "theme", "ui_dark"))
                                .toLowerCase()
                                .replace("-", "_")
                                .replace(" ", "_");
        return preset.isNotEmpty() ? preset : "ui_dark";
    }

    static bool isAcademicLightPreset(const juce::String& preset)
    {
        return preset.contains("academic");
    }

    static bool isDarkPreset(const juce::String& preset)
    {
        return !preset.contains("light") && !isAcademicLightPreset(preset);
    }

    int getExportWidth() const
    {
        return juce::jlimit(640, 6000, static_cast<int>(std::round(getDouble(getObject(job, "export"), "width", 1800.0))));
    }

    int getExportHeight() const
    {
        return juce::jlimit(420, 6000, static_cast<int>(std::round(getDouble(getObject(job, "export"), "height", 900.0))));
    }

    juce::StringArray getViews() const
    {
        juce::StringArray views;
        if (auto* arr = getObject(job, "views").getArray())
            for (const auto& v : *arr)
                views.add(v.toString().toLowerCase().replace("-", "_"));

        if (views.isEmpty())
            views.add("reverb_space");
        return views;
    }

    void writeFigure(const juce::File& file, const juce::String& view, const juce::String& preset)
    {
        AudioDoctorFigureRenderer::writePng(file, makeFigureData(view), isDarkPreset(preset),
                                            getExportWidth(), getExportHeight(),
                                            isAcademicLightPreset(preset));
    }

    void writeThesisFigures(const juce::File& figureDir, const juce::String& preset,
                            juce::Array<juce::var>& figures,
                            juce::Array<juce::var>& thesisFigures)
    {
        const auto specs = getThesisFigureSpecs();
        const bool dark = isDarkPreset(preset);
        const bool academicLight = isAcademicLightPreset(preset);
        juce::StringArray usedFileTokens;
        int figureIndex = 1;
        for (const auto& spec : specs)
        {
            const auto token = normaliseThesisTemplateToken(getString(spec, "template", spec.toString()));
            if (token.isEmpty())
                continue;

            const auto cameraVariants = getCameraVariants(spec);
            const auto timeVariants = getTimeReverseVariants(spec);
            const auto windowVariants = getSpatialWindowVariants(spec);
            for (const auto& cameraToken : cameraVariants)
            {
                for (const auto& timeVariant : timeVariants)
                {
                    for (const auto& windowVariant : windowVariants)
                    {
                        auto figureData = makeThesisFigureData(spec, token);
                        if (cameraToken.isNotEmpty())
                            figureData.terrainCamera = terrainCameraForToken(cameraToken);
                        if (timeVariant.hasOverride)
                            figureData.terrainTimeReversed = timeVariant.value;
                        applySpatialWindowVariant(figureData, windowVariant);

                        const auto width = juce::jmax(1800, getExportWidth());
                        const auto height = juce::jmax(1050, getExportHeight());
                        auto fileToken = makeThesisFigureFileToken(figureIndex, spec, token, figureData, preset);
                        while (usedFileTokens.contains(fileToken))
                            fileToken = makeThesisFigureFileToken(++figureIndex, spec, token, figureData, preset);
                        usedFileTokens.add(fileToken);

                        const auto png = figureDir.getChildFile(fileToken).withFileExtension(".png");
                        AudioDoctorFigureRenderer::writePng(png, figureData, dark, width, height, academicLight);
                        figures.add(png.getFullPathName());

                        auto obj = std::make_unique<juce::DynamicObject>();
                        obj->setProperty("index", figureIndex);
                        obj->setProperty("template", token);
                        obj->setProperty("viewToken", figureData.viewToken);
                        obj->setProperty("path", png.getFullPathName());
                        obj->setProperty("preset", preset);
                        obj->setProperty("palette", academicLight ? "academic_light" : (dark ? "ui_dark" : "ui_light"));
                        obj->setProperty("width", width);
                        obj->setProperty("height", height);
                        obj->setProperty("sources", writeThesisFigureSources(figureData));
                        obj->setProperty("sharedScale", figureData.sharedScale);
                        writeSpatialScaleProperties(*obj, figureData);
                        writeBandHighlightProperties(*obj, figureData.bandHighlight);
                        if (figureData.view == FigureView::maskingFusion)
                        {
                            const auto sources = getMaskingFusionSources(spec);
                            const auto bounceSource = getLayerFitFusionBounceSource(spec);
                            writeMaskingFusionProperties(*obj, figureData.maskingFusionSettings,
                                                         sources, bounceSource,
                                                         getLayerFitBounceWarning(sources, bounceSource),
                                                         sampleRateForFigureData(figureData),
                                                         figureData.spatialTimePositionSeconds,
                                                         figureData.terrainCamera,
                                                         figureData.crystalYawRadians,
                                                         figureData.crystalPitchRadians);
                        }
                        if (figureData.view == FigureView::spatialHeatmap)
                        {
                            obj->setProperty("terrainCamera", terrainCameraToken(figureData.terrainCamera));
                            obj->setProperty("terrainTimeReversed", figureData.terrainTimeReversed);
                        }
                        if (figureData.view == FigureView::spatialImpression)
                        {
                            obj->setProperty("terrainCamera", terrainCameraToken(figureData.terrainCamera));
                            if (figureData.spatialWindowStartSeconds >= 0.0f && figureData.spatialWindowEndSeconds > figureData.spatialWindowStartSeconds)
                            {
                                obj->setProperty("spatialWindow", windowVariant.token.isNotEmpty() ? windowVariant.token : "custom");
                                obj->setProperty("spatialWindowStartSeconds", figureData.spatialWindowStartSeconds);
                                obj->setProperty("spatialWindowEndSeconds", figureData.spatialWindowEndSeconds);
                            }
                            else if (figureData.spatialTimePositionSeconds >= 0.0f)
                            {
                                obj->setProperty("spatialTimePositionSeconds", figureData.spatialTimePositionSeconds);
                            }
                            else
                            {
                                obj->setProperty("spatialWindow", spatialWindowToken(figureData.spatialWindow));
                            }
                        }
                        if (figureData.processingNote.isNotEmpty())
                            obj->setProperty("processingNote", figureData.processingNote);
                        obj->setProperty("notes", thesisTemplateBoundaryNote(token));
                        thesisFigures.add(juce::var(obj.release()));
                        ++figureIndex;
                    }
                }
            }
        }
    }

    struct TimeReverseVariant
    {
        bool hasOverride = false;
        bool value = false;
    };

    struct SpatialWindowVariant
    {
        bool hasOverride = false;
        juce::String token;
        SpatialWindow window = SpatialWindow::full;
        float timePositionSeconds = -1.0f;
        float startSeconds = -1.0f;
        float endSeconds = -1.0f;
    };

    juce::StringArray getCameraVariants(const juce::var& spec) const
    {
        juce::StringArray variants;
        auto append = [&](const juce::var& value)
        {
            const auto token = value.toString().trim();
            if (token.isNotEmpty())
                variants.addIfNotAlreadyThere(token);
        };

        const auto cameras = getObject(spec, "cameras").isVoid() ? getObject(spec, "terrainCameras")
                                                                 : getObject(spec, "cameras");
        if (auto* arr = cameras.getArray())
            for (const auto& item : *arr)
                append(item);

        if (variants.isEmpty())
            variants.add({});
        return variants;
    }

    std::vector<TimeReverseVariant> getTimeReverseVariants(const juce::var& spec) const
    {
        std::vector<TimeReverseVariant> variants;
        auto append = [&](const juce::var& value)
        {
            const auto text = value.toString().trim().toLowerCase();
            if (text == "auto")
            {
                variants.push_back({ true, false });
                variants.push_back({ true, true });
            }
            else if (text == "flip" || text == "flipped" || text == "flip_time" || text == "reversed" || text == "reverse")
            {
                variants.push_back({ true, true });
            }
            else if (text == "normal" || text == "forward")
            {
                variants.push_back({ true, false });
            }
            else
            {
                variants.push_back({ true, static_cast<bool>(value) });
            }
        };

        const auto arrayValue = getObject(spec, "timeReversedVariants").isVoid() ? getObject(spec, "reverseTimeVariants")
                                                                                 : getObject(spec, "timeReversedVariants");
        if (auto* arr = arrayValue.getArray())
            for (const auto& item : *arr)
                append(item);

        if (variants.empty())
        {
            const auto single = getObject(spec, "terrainTimeReversed").isVoid() ? getObject(spec, "timeReversed")
                                                                               : getObject(spec, "terrainTimeReversed");
            if (!single.isVoid() && !single.isUndefined() && single.toString().trim().equalsIgnoreCase("auto"))
                append(single);
            else
                variants.push_back({});
        }

        return variants;
    }

    std::vector<SpatialWindowVariant> getSpatialWindowVariants(const juce::var& spec) const
    {
        std::vector<SpatialWindowVariant> variants;
        const auto windows = getObject(spec, "windows").isVoid() ? getObject(spec, "spatialWindows")
                                                                 : getObject(spec, "windows");
        if (auto* arr = windows.getArray())
        {
            for (const auto& item : *arr)
            {
                SpatialWindowVariant variant;
                variant.hasOverride = true;
                if (item.isObject())
                {
                    variant.token = getString(item, "name",
                                    getString(item, "token",
                                    getString(item, "window", "custom"))).trim().toLowerCase().replace(" ", "_");
                    variant.window = spatialWindowForToken(getString(item, "window", variant.token));
                    variant.startSeconds = static_cast<float>(getDouble(item, "start",
                                                          getDouble(item, "startSeconds",
                                                          getDouble(item, "spatialWindowStartSeconds", -1.0))));
                    variant.endSeconds = static_cast<float>(getDouble(item, "end",
                                                        getDouble(item, "endSeconds",
                                                        getDouble(item, "spatialWindowEndSeconds", -1.0))));
                    variant.timePositionSeconds = static_cast<float>(getDouble(item, "timePosition",
                                                                    getDouble(item, "timePositionSeconds",
                                                                    getDouble(item, "spatialTimePositionSeconds", -1.0))));
                }
                else
                {
                    variant.token = item.toString().trim().toLowerCase().replace(" ", "_");
                    variant.window = spatialWindowForToken(variant.token);
                }
                variants.push_back(variant);
            }
        }

        if (variants.empty())
            variants.push_back({});
        return variants;
    }

    static void applySpatialWindowVariant(FigureData& data, const SpatialWindowVariant& variant)
    {
        if (!variant.hasOverride)
            return;

        data.spatialWindow = variant.window;
        data.spatialTimePositionSeconds = variant.timePositionSeconds;
        data.spatialWindowStartSeconds = variant.startSeconds;
        data.spatialWindowEndSeconds = variant.endSeconds;
    }

    juce::String makeThesisFigureFileToken(int index, const juce::var& spec, const juce::String& templateToken,
                                           const FigureData& data, const juce::String& preset) const
    {
        juce::StringArray parts;
        parts.add(sessionId);
        parts.add("thesis");
        parts.add(juce::String(index).paddedLeft('0', 2));
        parts.add(templateToken);

        if (data.view == FigureView::spatialHeatmap || data.view == FigureView::spatialImpression)
            parts.add(terrainCameraToken(data.terrainCamera));

        if (data.view == FigureView::spatialHeatmap)
            parts.add(data.terrainTimeReversed ? "flip_time" : "normal");

        if (data.view == FigureView::spatialImpression)
        {
            if (data.spatialWindowStartSeconds >= 0.0f && data.spatialWindowEndSeconds > data.spatialWindowStartSeconds)
                parts.add("window_" + formatSecondsToken(data.spatialWindowStartSeconds) + "-" + formatSecondsToken(data.spatialWindowEndSeconds));
            else if (data.spatialTimePositionSeconds >= 0.0f)
                parts.add("time_" + formatSecondsToken(data.spatialTimePositionSeconds));
            else
                parts.add(spatialWindowToken(data.spatialWindow));
        }

        parts.add(preset);
        const auto title = getString(spec, "title");
        if (title.isNotEmpty())
            parts.add(sanitizeFileToken(title).substring(0, 48));
        return sanitizeFileToken(parts.joinIntoString("_")).substring(0, 180);
    }

    static juce::String formatSecondsToken(float seconds)
    {
        return juce::String(seconds, 2).replaceCharacter('.', 'p');
    }

    static void writeSpatialScaleProperties(juce::DynamicObject& obj, const FigureData& data)
    {
        auto update = [&](const Asset* asset)
        {
            if (asset == nullptr || !asset->spatialHeatmap.metrics.valid)
                return;
            const auto& m = asset->spatialHeatmap.metrics;
            if (data.sharedScale)
            {
                obj.setProperty("scaleMinDb", obj.hasProperty("scaleMinDb")
                                              ? juce::jmin(static_cast<double>(obj.getProperty("scaleMinDb")), static_cast<double>(m.floorDb))
                                              : static_cast<double>(m.floorDb));
                obj.setProperty("scaleMaxDb", obj.hasProperty("scaleMaxDb")
                                              ? juce::jmax(static_cast<double>(obj.getProperty("scaleMaxDb")), static_cast<double>(m.ceilingDb))
                                              : static_cast<double>(m.ceilingDb));
            }
        };

        update(data.dry);
        update(data.wetA);
        update(data.wetB);
    }

    static void writeBandHighlightProperties(juce::DynamicObject& obj, const BandHighlightConfig& config)
    {
        if (!config.enabled)
            return;

        auto bandObj = std::make_unique<juce::DynamicObject>();
        bandObj->setProperty("enabled", config.enabled);
        bandObj->setProperty("dimInactiveBands", config.dimInactiveBands);
        bandObj->setProperty("inactiveAlpha", config.inactiveAlpha);
        bandObj->setProperty("overlayAlpha", config.overlayAlpha);

        juce::Array<juce::var> bands;
        juce::Array<juce::var> activeBands;
        for (const auto& band : config.bands)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("id", band.id);
            item->setProperty("label", band.label);
            item->setProperty("minHz", band.minHz);
            item->setProperty("maxHz", band.maxHz);
            item->setProperty("color", band.colour.toDisplayString(false));
            item->setProperty("active", band.active);
            if (band.active)
                activeBands.add(band.id);
            bands.add(juce::var(item.release()));
        }
        bandObj->setProperty("bands", juce::var(bands));
        bandObj->setProperty("activeBands", juce::var(activeBands));
        obj.setProperty("bandHighlight", juce::var(bandObj.release()));
    }

    static double sampleRateForFigureData(const FigureData& data)
    {
        for (auto* asset : data.fitSources)
            if (asset != nullptr && asset->sampleRate > 0.0)
                return asset->sampleRate;
        for (auto* asset : { data.dry, data.wetA, data.wetB, data.fitBounceSource })
            if (asset != nullptr && asset->sampleRate > 0.0)
                return asset->sampleRate;
        return 48000.0;
    }

    static void writeMaskingFusionProperties(juce::DynamicObject& obj, const MaskingFusionSettings& settings,
                                             const std::array<juce::String, 3>& sources,
                                             const juce::String& bounceSource = {},
                                             const juce::String& warning = {},
                                             double sampleRate = 48000.0,
                                             float timeSeconds = -1.0f,
                                             TerrainCamera camera = TerrainCamera::diagonal,
                                             float crystalYawRadians = -0.68f,
                                             float crystalPitchRadians = 0.54f)
    {
        auto cfg = std::make_unique<juce::DynamicObject>();
        cfg->setProperty("mode", "layer_fit_fusion");
        cfg->setProperty("algorithmName", "critical_band_layer_fit_fusion");
        cfg->setProperty("algorithmVersion", 3);
        cfg->setProperty("psychoacousticMode", "bark_24_proxy");
        juce::Array<juce::var> sourceArray;
        juce::Array<juce::var> stemArray;
        int sourceCount = 0;
        int stemIndex = 0;
        for (const auto& source : sources)
        {
            ++stemIndex;
            if (isKnownSourceId(source))
            {
                const auto normalised = normaliseSourceId(source);
                sourceArray.add(normalised);
                auto stem = std::make_unique<juce::DynamicObject>();
                stem->setProperty("slot", "Stem " + juce::String(stemIndex));
                stem->setProperty("source", normalised);
                stem->setProperty("label", labelForSourceId(normalised));
                stemArray.add(juce::var(stem.release()));
                ++sourceCount;
            }
        }
        cfg->setProperty("sourceCount", sourceCount);
        cfg->setProperty("sources", juce::var(sourceArray));
        cfg->setProperty("stemSources", juce::var(stemArray));
        cfg->setProperty("source1", sources[0]);
        cfg->setProperty("source2", sources[1]);
        cfg->setProperty("source3", sources[2]);
        cfg->setProperty("bounceSource", bounceSource.isNotEmpty() ? bounceSource : "auto_bounce_selected_stems");
        cfg->setProperty("bounceSourceMode", bounceSource.isNotEmpty() ? "external_bounce_source" : "auto_bounce_linear_sum");
        cfg->setProperty("bounceLengthPolicy", bounceSource.isNotEmpty() ? "external_bounce_source_duration"
                                                                         : "max_selected_stem_length_zero_padded");
        cfg->setProperty("sampleRatePolicy", "analysis_sample_rate_first_valid_stem_resample_linear_if_needed");
        cfg->setProperty("analysisSampleRate", sampleRate);
        if (warning.isNotEmpty())
            cfg->setProperty("warning", warning);
        cfg->setProperty("figureType", settings.figureType);
        cfg->setProperty("bandScale", effectiveCriticalBandScale(settings.bandScale, settings.figureType));
        cfg->setProperty("criticalBandScale", effectiveCriticalBandScale(settings.bandScale, settings.figureType));
        cfg->setProperty("criticalBandMode", settings.criticalBandMode);
        cfg->setProperty("maskingModel", settings.maskingModel);
        cfg->setProperty("preferredBandCount", preferredCriticalBandCountForSettings(settings));
        cfg->setProperty("integrationTimeMs", settings.integrationTimeMs);
        cfg->setProperty("dominanceThresholdDb", settings.dominanceThresholdDb);
        cfg->setProperty("gateDbBelowPeak", settings.gateDbBelowPeak);
        cfg->setProperty("upwardSpread", settings.upwardSpread);
        cfg->setProperty("upwardSpreadWeight", settings.upwardSpreadWeight);
        cfg->setProperty("upwardSpreadSlopeDbPerBark", settings.upwardSpreadSlopeDbPerBark);
        cfg->setProperty("downwardSpreadSlopeDbPerBark", settings.downwardSpreadSlopeDbPerBark);
        cfg->setProperty("maskingRiskRangeDb", settings.maskingRiskRangeDb);
        cfg->setProperty("interauralCoherenceWeight", settings.interauralCoherenceWeight);
        cfg->setProperty("estimatedMaskedThreshold", settings.estimatedMaskedThreshold);
        auto overlay = std::make_unique<juce::DynamicObject>();
        overlay->setProperty("selectedStemDisplay", "same_coordinate_overlay");
        overlay->setProperty("timeFrequencyTerrain", "shared_2_5d_time_frequency_energy_renderer");
        overlay->setProperty("spatialImage", "same_lcr_frequency_space_overlay");
        overlay->setProperty("criticalBandRiskOverlay", "contour_ribbons_no_point_markers");
        overlay->setProperty("bandSoloGhostMode", "LayerFitBandSoloGhostMode");
        cfg->setProperty("overlay", juce::var(overlay.release()));
        const auto figureType = settings.figureType.trim().toLowerCase();
        if (figureType.contains("crystal"))
        {
            auto crystal = makeCriticalBandManifest(sampleRate, settings);
            if (auto* crystalObj = crystal.getDynamicObject())
            {
                crystalObj->setProperty("timeSeconds", timeSeconds);
                const float windowSeconds = juce::jmax(0.08f, settings.integrationTimeMs * 0.001f * 1.6f);
                if (timeSeconds >= 0.0f)
                {
                    crystalObj->setProperty("windowStartSeconds", juce::jmax(0.0f, timeSeconds - windowSeconds * 0.5f));
                        crystalObj->setProperty("windowEndSeconds", timeSeconds + windowSeconds * 0.5f);
                }
                if (figureType.contains("dodecahedron"))
                {
                    auto cameraObj = std::make_unique<juce::DynamicObject>();
                    cameraObj->setProperty("preset", terrainCameraToken(camera));
                    cameraObj->setProperty("yaw", crystalYawRadians);
                    cameraObj->setProperty("pitch", crystalPitchRadians);
                    cameraObj->setProperty("roll", 0.0);
                    crystalObj->setProperty("camera", juce::var(cameraObj.release()));
                    crystalObj->setProperty("selectedStems", juce::var(stemArray));
                    crystalObj->setProperty("bounce", bounceSource.isNotEmpty() ? bounceSource : "auto_bounce_selected_stems");
                }
                else
                {
                    crystalObj->setProperty("camera", terrainCameraToken(camera));
                }
            }
            cfg->setProperty(figureType.contains("dodecahedron") ? "dodecahedronCrystal" : "criticalBandCrystal",
                             crystal);
        }
        cfg->setProperty("boundaryNote",
                         "Approximate critical-band layer fit/fusion risk proxy; not a measured psychoacoustic threshold.");
        auto cfgVar = juce::var(cfg.release());
        obj.setProperty("layerFitFusion", cfgVar);
        obj.setProperty("maskingFusion", cfgVar);
    }

    juce::Array<juce::var> getThesisFigureSpecs() const
    {
        juce::Array<juce::var> specs;
        auto thesis = getObject(job, "thesisFigures").isVoid() ? getObject(job, "thesisFigure")
                                                               : getObject(job, "thesisFigures");
        if (thesis.isVoid())
        {
            const auto exportSpec = getObject(job, "export");
            thesis = getObject(exportSpec, "thesisFigures").isVoid() ? getObject(exportSpec, "thesisFigure")
                                                                     : getObject(exportSpec, "thesisFigures");
        }
        if (auto* arr = thesis.getArray())
        {
            for (const auto& item : *arr)
                specs.add(item);
            return specs;
        }

        if (thesis.isObject() || thesis.isString())
            specs.add(thesis);

        return specs;
    }

    FigureData makeFigureData(const juce::String& view) const
    {
        FigureData data;
        data.dry = getAssetById(displaySlotSources[0]);
        data.wetA = getAssetById(displaySlotSources[1]);
        data.wetB = getAssetById(displaySlotSources[2]);
        data.label1 = labelForSourceId(displaySlotSources[0]);
        data.label2 = labelForSourceId(displaySlotSources[1]);
        data.label3 = labelForSourceId(displaySlotSources[2]);
        data.pluginA = makeFigurePluginInfo(hostA, renderInfoA);
        data.pluginB = makeFigurePluginInfo(hostB, renderInfoB);
        data.pluginC = makeFigurePluginInfo(hostC, renderInfoC);
        data.view = figureViewForString(view);
        data.viewToken = view;
        data.processingNote = getString(job, "processingNote", getString(getObject(job, "export"), "processingNote"));
        data.terrainCamera = getJobTerrainCamera();
        data.terrainTimeReversed = getJobTerrainTimeReversed();
        data.crystalYawRadians = getJobCrystalYawRadians();
        data.crystalPitchRadians = getJobCrystalPitchRadians();
        data.spatialWindow = getJobSpatialWindow();
        data.spatialTimePositionSeconds = getJobSpatialTimePositionSeconds();
        data.sharedScale = getJobSharedScale();
        data.bandHighlight = getBandHighlightConfig();
        data.maskingFusionSettings = getMaskingFusionSettings();
        const auto viewToken = view.trim().toLowerCase().replace("-", "_").replace(" ", "_");
        if (viewToken.contains("dodecahedron_crystal") || viewToken.contains("dodecahedron"))
        {
            data.maskingFusionSettings.figureType = "dodecahedron_crystal";
            data.maskingFusionSettings.preferredBandCount = 24;
        }
        else if (viewToken.contains("critical_band_crystal"))
        {
            data.maskingFusionSettings.figureType = "critical_band_crystal";
            data.maskingFusionSettings.preferredBandCount = 24;
        }
        else if (viewToken.contains("critical_band_terrain"))
        {
            data.maskingFusionSettings.figureType = "critical_band_terrain";
        }
        else if (viewToken.contains("time_frequency_terrain") || viewToken == "terrain")
        {
            data.maskingFusionSettings.figureType = "terrain";
        }
        else if (viewToken.contains("layer_fit_spatial_image"))
        {
            data.maskingFusionSettings.figureType = "spatial_image";
        }
        const auto fitSources = getMaskingFusionSources();
        data.fitSources = { getAssetById(fitSources[0]), getAssetById(fitSources[1]), getAssetById(fitSources[2]) };
        data.fitLabels = { labelForSourceId(fitSources[0]), labelForSourceId(fitSources[1]), labelForSourceId(fitSources[2]) };
        const auto bounceSource = getLayerFitFusionBounceSource();
        data.fitBounceSource = getAssetById(bounceSource);
        data.fitBounceLabel = bounceSource.isNotEmpty() ? labelForSourceId(bounceSource) : "Auto Bounce Selected Stems";
        data.fitBounceAuto = bounceSource.isEmpty();
        data.fitFigureType = data.maskingFusionSettings.figureType;
        return data;
    }

    FigureData makeThesisFigureData(const juce::var& spec, const juce::String& templateToken) const
    {
        FigureData data = makeFigureData(templateToken);
        const auto sources = getThesisSources(spec);
        data.dry = getAssetById(sources[0]);
        data.wetA = getAssetById(sources[1]);
        data.wetB = getAssetById(sources[2]);
        data.label1 = labelForSourceId(sources[0]);
        data.label2 = labelForSourceId(sources[1]);
        data.label3 = labelForSourceId(sources[2]);
        data.view = thesisFigureViewForToken(templateToken);
        data.viewToken = getString(spec, "title", thesisFigureTitle(templateToken));
        data.processingNote = getString(spec, "processingNote", getString(job, "processingNote"));
        data.terrainCamera = terrainCameraForToken(getString(spec, "terrainCamera",
                                               getString(spec, "terrainView",
                                               terrainCameraToken(getJobTerrainCamera()))));
        data.terrainTimeReversed = getThesisTimeReversed(spec);
        data.crystalYawRadians = static_cast<float>(getDouble(spec, "crystalYawRadians",
                                                    getDouble(spec, "crystalYaw",
                                                    getDouble(spec, "cameraYaw",
                                                    getDouble(spec, "yaw", getJobCrystalYawRadians())))));
        data.crystalPitchRadians = static_cast<float>(getDouble(spec, "crystalPitchRadians",
                                                      getDouble(spec, "crystalPitch",
                                                      getDouble(spec, "cameraPitch",
                                                      getDouble(spec, "pitch", getJobCrystalPitchRadians())))));
        data.spatialWindow = spatialWindowForToken(getString(spec, "spatialWindow",
                                            getString(spec, "timeWindow",
                                            getString(spec, "window", spatialWindowToken(getJobSpatialWindow())))));
        data.spatialTimePositionSeconds = static_cast<float>(getDouble(spec, "spatialTimePositionSeconds",
                                                             getDouble(spec, "spatialTimePosition",
                                                             getDouble(spec, "timePositionSeconds",
                                                             getDouble(spec, "timePosition",
                                                             getJobSpatialTimePositionSeconds())))));
        data.spatialWindowStartSeconds = static_cast<float>(getDouble(spec, "spatialWindowStartSeconds",
                                                            getDouble(spec, "windowStartSeconds",
                                                            getDouble(spec, "startSeconds", -1.0))));
        data.spatialWindowEndSeconds = static_cast<float>(getDouble(spec, "spatialWindowEndSeconds",
                                                          getDouble(spec, "windowEndSeconds",
                                                          getDouble(spec, "endSeconds", -1.0))));
        data.sharedScale = getBool(spec, "sharedScale", getJobSharedScale());
        data.bandHighlight = getBandHighlightConfig(spec);
        data.maskingFusionSettings = getMaskingFusionSettings(spec);
        const auto templateFigureToken = templateToken.trim().toLowerCase().replace("-", "_").replace(" ", "_");
        if (templateFigureToken.contains("dodecahedron_crystal") || templateFigureToken.contains("dodecahedron"))
        {
            data.maskingFusionSettings.figureType = "dodecahedron_crystal";
            data.maskingFusionSettings.preferredBandCount = 24;
        }
        else if (templateFigureToken.contains("critical_band_crystal"))
        {
            data.maskingFusionSettings.figureType = "critical_band_crystal";
            data.maskingFusionSettings.preferredBandCount = 24;
        }
        const auto fitSources = getMaskingFusionSources(spec);
        data.fitSources = { getAssetById(fitSources[0]), getAssetById(fitSources[1]), getAssetById(fitSources[2]) };
        data.fitLabels = { labelForSourceId(fitSources[0]), labelForSourceId(fitSources[1]), labelForSourceId(fitSources[2]) };
        const auto bounceSource = getLayerFitFusionBounceSource(spec);
        data.fitBounceSource = getAssetById(bounceSource);
        data.fitBounceLabel = bounceSource.isNotEmpty() ? labelForSourceId(bounceSource) : "Auto Bounce Selected Stems";
        data.fitBounceAuto = bounceSource.isEmpty();
        data.fitFigureType = data.maskingFusionSettings.figureType;
        return data;
    }

    std::array<juce::String, 3> getThesisSources(const juce::var& spec) const
    {
        std::array<juce::String, 3> sources = displaySlotSources;
        auto sourceSpec = getObject(spec, "sources");
        if (sourceSpec.isVoid() || sourceSpec.isUndefined())
            sourceSpec = getObject(spec, "slots");
        if (auto* arr = sourceSpec.getArray())
        {
            for (int i = 0; i < juce::jmin(3, arr->size()); ++i)
            {
                const auto id = normaliseSourceId((*arr)[i].toString());
                if (isKnownSourceId(id))
                    sources[static_cast<size_t>(i)] = id;
            }
            return sources;
        }

        if (sourceSpec.isObject())
        {
            const char* keys[3] = { "source1", "source2", "source3" };
            const char* altKeys[3] = { "a", "b", "c" };
            for (int i = 0; i < 3; ++i)
            {
                auto id = normaliseSourceId(getString(sourceSpec, keys[i], getString(sourceSpec, altKeys[i])));
                if (isKnownSourceId(id))
                    sources[static_cast<size_t>(i)] = id;
            }
        }

        const char* directKeys[3] = { "source1", "source2", "source3" };
        for (int i = 0; i < 3; ++i)
        {
            auto id = normaliseSourceId(getString(spec, directKeys[i]));
            if (isKnownSourceId(id))
                sources[static_cast<size_t>(i)] = id;
        }

        return sources;
    }

    juce::var writeThesisFigureSources(const FigureData& data) const
    {
        juce::Array<juce::var> sources;
        auto append = [&](int slot, const juce::String& sourceId, const Asset* asset)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty("slot", slot);
            obj->setProperty("source", sourceId);
            obj->setProperty("label", labelForSourceId(sourceId));
            obj->setProperty("hasAsset", asset != nullptr);
            if (asset != nullptr)
            {
                obj->setProperty("assetName", asset->name);
                obj->setProperty("sourceType", asset->generatedSignal ? "generated" : "file");
                obj->setProperty("sourcePath", asset->sourcePath);
                obj->setProperty("sourceHash", asset->generatedSignal ? hashGeneratedSignalSpec(asset->generatedSignalSpec)
                                                                      : hashSourceFnv1a64(asset->sourcePath));
            }
            sources.add(juce::var(obj.release()));
        };

        append(1, labelToSourceId(data.label1, data.dry), data.dry);
        append(2, labelToSourceId(data.label2, data.wetA), data.wetA);
        append(3, labelToSourceId(data.label3, data.wetB), data.wetB);
        return juce::var(sources);
    }

    static juce::String labelToSourceId(const juce::String& label, const Asset*)
    {
        return normaliseSourceId(label);
    }

    static juce::String normaliseThesisTemplateToken(juce::String token)
    {
        token = token.trim().toLowerCase().replace("-", "_").replace(" ", "_");
        if (token.contains("dodecahedron"))
            return "dodecahedron_crystal";
        if (token.contains("masking") || token.contains("overlap_fusion"))
            return "masking_fusion";
        if (token.contains("material") || token.contains("fire") || token.contains("flame"))
        {
            if (token.contains("electric") || token.contains("electricity") || token.contains("spark"))
                return "material_electric";
            if (token.contains("water") || token.contains("liquid") || token.contains("splash"))
                return "material_water";
            if (token.contains("fire") || token.contains("flame"))
                return "material_fire";
            return "material_signature";
        }
        if (token.contains("electric") || token.contains("electricity") || token.contains("spark"))
            return "material_electric";
        if (token.contains("water") || token.contains("liquid") || token.contains("splash"))
            return "material_water";
        const bool spatialImage = (token.contains("spatial") && (token.contains("image") || token.contains("impression")))
                               || token.contains("lcr");
        if (spatialImage)
            return "spatial_impression";
        const bool terrainVariant = token.contains("2_5d") || token.contains("2.5d") || token.contains("terrain");
        if (terrainVariant && (token.contains("spectro") || token.contains("reverb") || token.contains("space")))
            return "spatial_heatmap";
        if (token.contains("cst") || token.contains("charge") || token.contains("shot") || token.contains("tail"))
            return "cst_spectrogram";
        if (token.contains("group") || token.contains("delay"))
            return "group_delay_combo";
        if (token.contains("dynamic") || token.contains("duck") || token.contains("attenuation"))
            return "dynamics_apparent_ducking";
        if (token.contains("critical_band_terrain")
            || token.contains("time_frequency_terrain")
            || token.contains("layer_fit_spatial_image"))
            return "layer_fit_fusion";
        if (token.contains("spatial") || token.contains("heatmap") || token.contains("terrain") || token.contains("width"))
            return "spatial_heatmap";
        if (token.contains("reverb") || token.contains("space"))
            return "reverb_space_combo";
        if (token.contains("critical_band_crystal")
            || token.contains("layer_fit") || (token.contains("fusion") && token.contains("fit"))
            || token.contains("masking") || token.contains("overlap_fusion"))
            return "layer_fit_fusion";
        if (token.contains("harmonic") || token.contains("fusion"))
            return "harmonic_fusion";
        if (token.contains("layer"))
            return "layering_spectrum";
        return token.isNotEmpty() ? token : "layering_spectrum";
    }

    static FigureView thesisFigureViewForToken(const juce::String& token)
    {
        if (token == "cst_spectrogram")
            return FigureView::cstSpectrogram;
        if (token == "group_delay_combo")
            return FigureView::groupDelayCombo;
        if (token == "dynamics_apparent_ducking")
            return FigureView::dynamicsApparentDucking;
        if (token == "spatial_heatmap")
            return FigureView::spatialHeatmap;
        if (token == "spatial_impression")
            return FigureView::spatialImpression;
        if (token == "material_fire" || token == "material_electric"
            || token == "material_water" || token == "material_signature")
            return FigureView::spatialHeatmap;
        if (token == "masking_fusion" || token == "overlap_fusion" || token == "layer_fit_fusion"
            || token == "critical_band_terrain" || token == "time_frequency_terrain" || token == "layer_fit_spatial_image"
            || token == "critical_band_crystal" || token == "dodecahedron_crystal")
            return FigureView::maskingFusion;
        if (token == "reverb_space_combo")
            return FigureView::reverbSpaceCombo;
        if (token == "harmonic_fusion")
            return FigureView::harmonicFusion;
        if (token == "layering_spectrum")
            return FigureView::layeringSpectrum;
        return FigureView::spectrum;
    }

    static juce::String thesisFigureTitle(const juce::String& token)
    {
        if (token == "cst_spectrogram")
            return "Thesis Figure - CST Spectrogram";
        if (token == "group_delay_combo")
            return "Thesis Figure - Group Delay Combo";
        if (token == "dynamics_apparent_ducking")
            return "Thesis Figure - Dynamics Apparent Ducking";
        if (token == "spatial_heatmap")
            return "Thesis Figure - Spatial Energy Terrain";
        if (token == "spatial_impression")
            return "Thesis Figure - Spatial Image";
        if (token == "material_fire")
            return "Material Signature - Fire";
        if (token == "material_electric")
            return "Material Signature - Electric";
        if (token == "material_water")
            return "Material Signature - Water";
        if (token == "material_signature")
            return "Material Signature - 2.5D";
        if (token == "dodecahedron_crystal")
            return "Thesis Figure - Dodecahedron Crystal";
        if (token == "masking_fusion" || token == "overlap_fusion" || token == "layer_fit_fusion"
            || token == "critical_band_crystal")
            return "Thesis Figure - Layer Fit / Fusion";
        if (token == "reverb_space_combo")
            return "Thesis Figure - Reverb Space Combo";
        if (token == "harmonic_fusion")
            return "Thesis Figure - Harmonic Fusion";
        if (token == "layering_spectrum")
            return "Thesis Figure - Layering Spectrum";
        return "Thesis Figure - " + token;
    }

    static juce::String thesisTemplateBoundaryNote(const juce::String& token)
    {
        if (token == "dynamics_apparent_ducking")
            return "Apparent attenuation is target RMS minus reference RMS; it is not plugin-internal gain reduction.";
        if (token == "group_delay_combo")
            return "Group-delay summary ignores bins more than 45 dB below the selected spectrum peak.";
        if (token == "reverb_space_combo")
            return "Direct and early windows are onset-aware; estimated RT60 is derived from RT30 or RT20.";
        if (token == "spatial_heatmap")
            return "Spatial width is derived from local Side/Mid ratio, L/R correlation, and L/R balance; the figure is a 2.5D terrain projection, not a plugin-internal meter.";
        if (token == "spatial_impression")
            return "Spatial image integrates the selected time window into L-C-R by frequency energy using L/R balance, Side/Mid ratio, and correlation; it is a stereo-derived impression, not a multichannel bus meter.";
        if (token == "material_fire" || token == "material_electric"
            || token == "material_water" || token == "material_signature")
            return "Material signature uses the 2.5D spectrogram terrain projection to show time-frequency energy texture; it is an offline visual summary, not a material classifier.";
        if (token == "dodecahedron_crystal")
            return "Dodecahedron crystal groups 24 critical bands into 12 pentagonal faces. Each face is split into five triangular wedges: the stronger band receives three wedges, the weaker band receives two, and wedge height is driven directly by that band's signal energy. It is a signal-energy proxy, not a measured psychoacoustic threshold.";
        if (token == "masking_fusion" || token == "overlap_fusion" || token == "layer_fit_fusion"
            || token == "critical_band_crystal")
            return "Layer Fit/Fusion is an approximate critical-band overlap and bounce-delta proxy. It visualizes stem overlap, dominance, and fitted bounce behavior; it is not a measured psychoacoustic threshold.";
        if (token == "cst_spectrogram")
            return "Stage markers come from the generated signal spec or imported metadata when available.";
        return "Template is rendered from Audio Doctor analysis curves and linked CSV data.";
    }

    static FigureView figureViewForString(const juce::String& view)
    {
        const auto lower = view.toLowerCase().replace("-", "_").replace(" ", "_");
        if (lower == "cst_spectrogram" || lower == "group_delay_combo"
            || lower == "dynamics_apparent_ducking" || lower == "reverb_space_combo"
            || lower == "harmonic_fusion" || lower == "layering_spectrum"
            || lower == "spatial_heatmap" || lower == "spatial_impression"
            || lower == "material_fire" || lower == "material_electric"
            || lower == "material_water" || lower == "material_signature"
            || lower == "masking_fusion" || lower == "overlap_fusion" || lower == "layer_fit_fusion"
            || lower == "critical_band_terrain" || lower == "time_frequency_terrain" || lower == "layer_fit_spatial_image"
            || lower == "critical_band_crystal" || lower == "dodecahedron_crystal")
            return thesisFigureViewForToken(lower);
        if (lower.contains("dodecahedron")
            || lower.contains("critical_band_crystal")
            || lower.contains("masking") || lower.contains("layer_fit")
            || (lower.contains("fusion") && (lower.contains("overlap") || lower.contains("fit"))))
            return FigureView::maskingFusion;
        if ((lower.contains("spatial") && (lower.contains("image") || lower.contains("impression")))
            || lower.contains("lcr"))
            return FigureView::spatialImpression;
        if (lower.contains("spatial") || lower.contains("heatmap") || lower.contains("terrain") || lower.contains("width"))
            return FigureView::spatialHeatmap;
        if (lower.contains("spectro") || lower.contains("waterfall"))
            return FigureView::spectrogramABC;
        if (lower.contains("reverb") || lower.contains("space"))
            return FigureView::reverbSpace;
        if (lower.contains("dynamic") || lower.contains("sidechain"))
            return FigureView::dynamics;
        if (lower.contains("group"))
            return FigureView::groupDelay;
        if (lower.contains("envelope"))
            return FigureView::envelope;
        return FigureView::spectrum;
    }

    static FigurePluginInfo makeFigurePluginInfo(const PluginHost& host, const RenderInfo& renderInfo)
    {
        FigurePluginInfo info;
        if (host.getCurrentPlugin() == nullptr)
            return info;

        const auto* desc = host.getCurrentPlugin();
        info.valid = true;
        info.name = desc->name;
        info.format = desc->pluginFormatName;
        info.latencySamples = renderInfo.latencySamples;
        info.tailSeconds = renderInfo.tailSeconds;

        for (const auto& p : host.getChangedParameters())
            info.changedParameters.push_back({ p.name, p.valueText, p.normalisedValue });

        return info;
    }

    using CurveMember = std::vector<PlotPoint> Asset::*;

    static juce::Colour dryColour()  { return juce::Colour(0xFF22D3EE); }
    static juce::Colour wetAColour() { return juce::Colour(0xFFFFD166); }
    static juce::Colour wetBColour() { return juce::Colour(0xFFE6335F); }

    void drawTimePlot(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                      const juce::String& xLabel, const juce::String& yLabel,
                      CurveMember curve, float minY, float maxY, bool dark, MetricsKind metricsKind)
    {
        auto metricsArea = area.removeFromBottom(metricsKind == MetricsKind::basic ? 250.0f : 310.0f);
        drawPlotBackground(g, area, title, xLabel, yLabel, dark);
        const float maxTime = getMaxCurveX(curve, false);
        const auto plot = area.reduced(80.0f, 80.0f);
        drawCurve(g, plot, dryAsset.get(), curve, dryColour(), false, maxTime, minY, maxY);
        drawCurve(g, plot, wetA.get(), curve, wetAColour(), false, maxTime, minY, maxY);
        drawCurve(g, plot, wetB.get(), curve, wetBColour(), false, maxTime, minY, maxY);
        drawFigureMetrics(g, metricsArea, dark, metricsKind);
    }

    void drawFrequencyPlot(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                           const juce::String& xLabel, const juce::String& yLabel,
                           CurveMember curve, float minY, float maxY, bool dark, MetricsKind metricsKind)
    {
        auto metricsArea = area.removeFromBottom(metricsKind == MetricsKind::basic ? 250.0f : 310.0f);
        drawPlotBackground(g, area, title, xLabel, yLabel, dark);
        const auto plot = area.reduced(80.0f, 80.0f);
        drawCurve(g, plot, dryAsset.get(), curve, dryColour(), true, 20000.0f, minY, maxY);
        drawCurve(g, plot, wetA.get(), curve, wetAColour(), true, 20000.0f, minY, maxY);
        drawCurve(g, plot, wetB.get(), curve, wetBColour(), true, 20000.0f, minY, maxY);
        drawFigureMetrics(g, metricsArea, dark, metricsKind);
    }

    void drawReverbSpaceFigure(juce::Graphics& g, juce::Rectangle<float> area, bool dark)
    {
        auto metricsArea = area.removeFromBottom(320.0f);
        drawPlotBackground(g, area, "Reverb / Space", "seconds", "EDC / decay (dB)", dark);

        const float maxTime = getMaxCurveX(&Asset::energyDecay, false);
        const auto plot = area.reduced(80.0f, 80.0f);
        drawCurve(g, plot, dryAsset.get(), &Asset::energyDecay, dryColour(), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, plot, wetA.get(), &Asset::energyDecay, wetAColour(), false, maxTime, -80.0f, 0.0f);
        drawCurve(g, plot, wetB.get(), &Asset::energyDecay, wetBColour(), false, maxTime, -80.0f, 0.0f);

        drawReverbMetrics(g, metricsArea, dark);
    }

    void drawSpectrogramFigure(juce::Graphics& g, juce::Rectangle<float> area, bool dark)
    {
        g.setColour(dark ? juce::Colour(0xFF0A0D13) : juce::Colour(0xFFFFFFFF));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB) : juce::Colour(0x22000000));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto inner = area.reduced(80.0f, 60.0f);
        auto titleArea = inner.removeFromTop(46.0f);
        auto metricsArea = inner.removeFromBottom(230.0f);
        inner.removeFromBottom(20.0f);

        g.setColour(dark ? juce::Colour(0xFFF6F8FB) : juce::Colour(0xFF121722));
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        g.drawText("Spectrogram A/B/C", titleArea, juce::Justification::centredLeft);

        struct Track
        {
            const juce::Image* image = nullptr;
            double sampleRate = 48000.0;
            float durationSeconds = 0.0f;
            juce::String label;
        };

        std::vector<Track> tracks;
        if (dryAsset != nullptr && !dryAsset->spectrogramBlue.isNull())
            tracks.push_back({ &dryAsset->spectrogramBlue, dryAsset->sampleRate,
                               static_cast<float>(dryAsset->metrics.durationSeconds), "Dry" });
        if (wetA != nullptr && !wetA->spectrogramYellow.isNull())
            tracks.push_back({ &wetA->spectrogramYellow, wetA->sampleRate,
                               static_cast<float>(wetA->metrics.durationSeconds), "Wet A" });
        if (wetB != nullptr && !wetB->spectrogramPink.isNull())
            tracks.push_back({ &wetB->spectrogramPink, wetB->sampleRate,
                               static_cast<float>(wetB->metrics.durationSeconds), "Wet B" });

        if (tracks.empty())
        {
            g.setColour(dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF1A202C));
            g.setFont(juce::Font(22.0f));
            g.drawText("No spectrogram data.", inner, juce::Justification::centred);
            drawFigureMetrics(g, metricsArea, dark, MetricsKind::basic);
            return;
        }

        const float gap = 18.0f;
        const float trackHeight = (inner.getHeight() - gap * static_cast<float>(tracks.size() - 1))
                                / static_cast<float>(tracks.size());
        float maxDurationSeconds = 0.001f;
        for (const auto& track : tracks)
            maxDurationSeconds = juce::jmax(maxDurationSeconds, track.durationSeconds);

        auto remaining = inner;
        for (const auto& track : tracks)
        {
            auto panel = remaining.removeFromTop(trackHeight);
            drawSpectrogramTrack(g, panel, *track.image, track.sampleRate,
                                 track.durationSeconds, maxDurationSeconds,
                                 track.label, dark);
            remaining.removeFromTop(gap);
        }

        drawFigureMetrics(g, metricsArea, dark, MetricsKind::basic);
    }

    static void drawSpectrogramTrack(juce::Graphics& g, juce::Rectangle<float> area,
                                     const juce::Image& image, double sampleRate,
                                     float durationSeconds, float maxDurationSeconds,
                                     const juce::String& label, bool dark)
    {
        g.setColour(dark ? juce::Colour(0xFF060A10) : juce::Colour(0xFF10141D));
        g.fillRect(area);
        const float durationRatio = juce::jlimit(0.0f, 1.0f,
            durationSeconds / juce::jmax(0.001f, maxDurationSeconds));
        auto imageArea = area.withWidth(juce::jmax(1.0f, area.getWidth() * durationRatio));
        g.drawImage(image, imageArea, juce::RectanglePlacement::stretchToFit);

        g.setColour(juce::Colours::white.withAlpha(0.88f));
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
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
            g.setColour(juce::Colours::white.withAlpha(0.72f));
            const auto text = freq >= 1000.0f ? juce::String(static_cast<int>(freq / 1000.0f)) + "k"
                                              : juce::String(static_cast<int>(freq));
            g.drawText(text, area.getRight() - 44.0f, y - 8.0f, 38.0f, 16.0f,
                       juce::Justification::centredRight);
        }
    }

    static void drawPlotBackground(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                                   const juce::String& xLabel, const juce::String& yLabel, bool dark)
    {
        g.setColour(dark ? juce::Colour(0xFF0A0D13) : juce::Colour(0xFFFFFFFF));
        g.fillRoundedRectangle(area, 12.0f);
        g.setColour(dark ? juce::Colour(0x33F6F8FB) : juce::Colour(0x22000000));
        g.drawRoundedRectangle(area, 12.0f, 1.2f);

        auto plot = area.reduced(80.0f, 80.0f);
        g.setColour(dark ? juce::Colour(0xFF060A10) : juce::Colour(0xFF10141D));
        g.fillRect(plot);
        g.setColour(dark ? juce::Colour(0x28F6F8FB) : juce::Colour(0x25FFFFFF));
        for (int i = 0; i <= 5; ++i)
        {
            const float y = plot.getY() + plot.getHeight() * static_cast<float>(i) / 5.0f;
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        }

        g.setColour(dark ? juce::Colour(0xFFF6F8FB) : juce::Colour(0xFF121722));
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        g.drawText(title, area.removeFromTop(44.0f).reduced(12.0f, 0.0f), juce::Justification::centredLeft);
        g.setFont(juce::Font(18.0f));
        g.drawText(xLabel, plot.withY(plot.getBottom() + 20.0f).withHeight(24.0f), juce::Justification::centred);
        g.setFont(juce::Font(14.0f));
        g.drawFittedText(yLabel,
                         juce::Rectangle<int>(static_cast<int>(plot.getX() - 152.0f),
                                              static_cast<int>(plot.getCentreY() - 12.0f),
                                              142,
                                              24),
                         juce::Justification::centredRight,
                         1,
                         0.7f);
    }

    void drawCurve(juce::Graphics& g, juce::Rectangle<float> plot, const Asset* asset, CurveMember member,
                   juce::Colour colour, bool logX, float maxX, float minY, float maxY)
    {
        if (asset == nullptr)
            return;

        const auto& points = asset->*member;
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
        g.strokePath(path, juce::PathStrokeType(4.0f));
    }

    float getMaxCurveX(CurveMember member, bool frequency) const
    {
        if (frequency)
            return 20000.0f;

        float maxX = 0.001f;
        for (const auto* asset : { dryAsset.get(), wetA.get(), wetB.get() })
            if (asset != nullptr)
                for (const auto& p : asset->*member)
                    maxX = juce::jmax(maxX, p.x);
        return maxX;
    }

    void drawFigureMetrics(juce::Graphics& g, juce::Rectangle<float> area, bool dark, MetricsKind metricsKind)
    {
        const bool dense = metricsKind != MetricsKind::basic;
        auto assetArea = area.removeFromLeft(area.getWidth() * 0.62f);
        area.removeFromLeft(42.0f);
        auto pluginArea = area;

        g.setColour(dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF1A202C));
        g.setFont(juce::Font(18.0f));
        auto drawAsset = [&](const Asset* asset, const juce::String& label, juce::Colour colour)
        {
            if (asset == nullptr)
                return;
            auto row = assetArea.removeFromTop(dense ? 58.0f : 30.0f);
            g.setColour(colour);
            g.fillRect(row.removeFromLeft(14.0f).reduced(0.0f, dense ? 8.0f : 5.0f));

            const auto firstLine = label + " | " + juce::String(asset->metrics.sampleRate, 0) + " Hz | "
                + juce::String(asset->metrics.channels) + " ch | "
                + juce::String(asset->metrics.durationSeconds, 2) + " s | peak "
                + juce::String(asset->metrics.peakDb, 1) + " dB | rms "
                + juce::String(asset->metrics.rmsDb, 1) + " dB | crest "
                + juce::String(asset->metrics.crestDb, 1) + " dB";

            auto textRow = row.reduced(10.0f, 0.0f);
            g.setColour(dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF1A202C));
            g.setFont(juce::Font(18.0f));
            g.drawText(firstLine, textRow.removeFromTop(dense ? 28.0f : 30.0f),
                       juce::Justification::centredLeft, true);

            if (!dense)
                return;

            const auto detail = makeAssetDetailLine(asset, label, metricsKind);
            if (detail.isNotEmpty())
            {
                g.setColour(dark ? juce::Colour(0xFFC9D2DE) : juce::Colour(0xFF425064));
                g.setFont(juce::Font(16.0f));
                g.drawText(detail, textRow.removeFromTop(24.0f), juce::Justification::centredLeft, true);
            }
        };

        drawAsset(dryAsset.get(), "Dry", dryColour());
        drawAsset(wetA.get(), "Wet A", wetAColour());
        drawAsset(wetB.get(), "Wet B", wetBColour());
        drawPluginParameterPanel(g, pluginArea, dark);
    }

    juce::String makeAssetDetailLine(const Asset* asset, const juce::String& label, MetricsKind metricsKind)
    {
        if (asset == nullptr)
            return {};

        if (metricsKind == MetricsKind::groupDelay)
        {
            if (label == "Dry")
                return "GD reference 0.00 ms";

            const auto stats = computeGroupDelayStats(asset->groupDelay);
            if (!stats.valid)
                return {};

            return "GD avg " + formatDelayMs(stats.meanMs)
                + " | abs avg " + formatDelayMs(stats.meanAbsMs)
                + " | peak " + formatDelayMs(stats.peakAbsMs)
                + " at " + formatFeatureFrequency(stats.peakFreq)
                + " | L/M/H "
                + formatDelayMs(stats.lowMeanMs) + " / "
                + formatDelayMs(stats.midMeanMs) + " / "
                + formatDelayMs(stats.highMeanMs)
                + " | span " + formatDelayMs(stats.maxMs - stats.minMs);
        }

        if (metricsKind == MetricsKind::dynamics && asset->dynamicsMetrics.valid)
        {
            const auto& m = asset->dynamicsMetrics;
            return "RMS P10/P50/P90 "
                + juce::String(m.rmsP10Db, 1) + " / "
                + juce::String(m.rmsP50Db, 1) + " / "
                + juce::String(m.rmsP90Db, 1) + " dB"
                + " | range " + juce::String(m.rmsRangeDb, 1) + " dB"
                + " | transient/sustain " + juce::String(m.transientToSustainDb, 1) + " dB";
        }

        return {};
    }

    void drawReverbMetrics(juce::Graphics& g, juce::Rectangle<float> area, bool dark)
    {
        auto assetArea = area.removeFromLeft(area.getWidth() * 0.66f);
        area.removeFromLeft(42.0f);
        auto pluginArea = area;

        g.setColour(dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF1A202C));
        g.setFont(juce::Font(18.0f));

        auto drawAsset = [&](const Asset* asset, const juce::String& label)
        {
            if (asset == nullptr)
                return;

            const auto& m = asset->spaceMetrics;
            auto row = assetArea.removeFromTop(54.0f);
            const auto firstLine = label + " | " + juce::String(asset->metrics.sampleRate, 0) + " Hz | "
                + juce::String(asset->metrics.durationSeconds, 2) + " s | peak "
                + juce::String(asset->metrics.peakDb, 1) + " dB | rms "
                + juce::String(asset->metrics.rmsDb, 1) + " dB | crest "
                + juce::String(asset->metrics.crestDb, 1) + " dB";
            g.drawText(firstLine, row.removeFromTop(24.0f), juce::Justification::centredLeft, true);

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
            g.setColour(dark ? juce::Colour(0xFFC9D2DE) : juce::Colour(0xFF425064));
            g.drawText(secondLine, row.removeFromTop(24.0f), juce::Justification::centredLeft, true);
            g.setColour(dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF1A202C));
        };

        drawAsset(dryAsset.get(), "Dry");
        drawAsset(wetA.get(), "Wet A");
        drawAsset(wetB.get(), "Wet B");
        drawPluginParameterPanel(g, pluginArea, dark);
    }

    void drawPluginParameterPanel(juce::Graphics& g, juce::Rectangle<float> area, bool dark)
    {
        auto drawPlugin = [&](PluginHost& host, const RenderInfo& renderInfo,
                              const juce::String& label, juce::Colour colour)
        {
            if (host.getCurrentPlugin() == nullptr)
                return;

            auto block = area.removeFromTop(98.0f);
            g.setColour(colour);
            g.fillRect(block.removeFromLeft(14.0f).reduced(0.0f, 6.0f));

            const auto* desc = host.getCurrentPlugin();
            const auto title = label + " params | " + desc->name + " (" + desc->pluginFormatName + ")";
            const auto renderLine = "latency " + juce::String(renderInfo.latencySamples)
                + " samples | tail " + juce::String(renderInfo.tailSeconds, 2) + " s";

            g.setColour(dark ? juce::Colour(0xFFEAF0F8) : juce::Colour(0xFF1A202C));
            g.setFont(juce::Font(17.0f, juce::Font::bold));
            g.drawText(title, block.removeFromTop(26.0f), juce::Justification::centredLeft, true);
            g.setColour(dark ? juce::Colour(0xFFC9D2DE) : juce::Colour(0xFF425064));
            g.setFont(juce::Font(15.0f));
            g.drawText(renderLine, block.removeFromTop(22.0f), juce::Justification::centredLeft, true);

            juce::String paramsText;
            const auto& params = host.getChangedParameters();
            if (params.empty())
            {
                paramsText = "default/no changed parameter";
            }
            else
            {
                const int maxParams = 8;
                const int count = juce::jmin(maxParams, static_cast<int>(params.size()));
                for (int i = 0; i < count; ++i)
                {
                    if (i > 0)
                        paramsText += " | ";
                    paramsText += params[static_cast<size_t>(i)].name + " " + params[static_cast<size_t>(i)].valueText;
                }

                if (static_cast<int>(params.size()) > count)
                    paramsText += " | +" + juce::String(static_cast<int>(params.size()) - count);
            }

            g.setColour(dark ? juce::Colour(0xFFC9D2DE) : juce::Colour(0xFF425064));
            g.setFont(juce::Font(16.0f));
            g.drawText(paramsText, block.removeFromTop(42.0f), juce::Justification::centredLeft, true);
            area.removeFromTop(14.0f);
        };

        drawPlugin(hostA, renderInfoA, "Plugin A", wetAColour());
        drawPlugin(hostB, renderInfoB, "Plugin B", wetBColour());
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

    static GroupDelayStats computeGroupDelayStats(const std::vector<PlotPoint>& points,
                                                  float minHz = 20.0f,
                                                  float maxHz = 20000.0f)
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

        for (const auto& p : points)
        {
            if (p.x < minHz || p.x > maxHz)
                continue;

            const float value = p.y;
            sum += static_cast<double>(value);
            sumAbs += static_cast<double>(std::abs(value));
            minMs = juce::jmin(minMs, value);
            maxMs = juce::jmax(maxMs, value);

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
        return stats;
    }

    static juce::String csvEscape(juce::String text)
    {
        text = text.replace("\"", "\"\"");
        return "\"" + text + "\"";
    }

    juce::String pluginChainSummary() const
    {
        juce::StringArray parts;
        auto append = [&](const juce::String& label, const PluginHost& host)
        {
            if (host.getCurrentPlugin() != nullptr)
                parts.add(label + ": " + host.getCurrentPlugin()->name + " (" + host.getCurrentPlugin()->pluginFormatName + ")");
        };
        append("A", hostA);
        append("B", hostB);
        append("C", hostC);
        return parts.isEmpty() ? "none" : parts.joinIntoString(" | ");
    }

    static juce::String joinThesisSourceField(const juce::var& figure, const juce::String& field)
    {
        juce::StringArray parts;
        if (auto* arr = getObject(figure, "sources").getArray())
        {
            for (const auto& source : *arr)
            {
                const auto value = getString(source, field);
                if (value.isNotEmpty())
                    parts.add(value);
            }
        }
        return parts.joinIntoString(" | ");
    }

    static juce::String chapterHintForThesisTemplate(const juce::String& token)
    {
        if (token == "material_fire" || token == "material_electric"
            || token == "material_water" || token == "material_signature"
            || token == "masking_fusion" || token == "overlap_fusion")
            return "Chapter 2";
        if (token == "layering_spectrum" || token == "cst_spectrogram" || token == "harmonic_fusion")
            return "Chapter 3";
        if (token == "group_delay_combo" || token == "reverb_space_combo" || token == "dynamics_apparent_ducking")
            return "Chapter 4";
        return "";
    }

    void writeAppendixTable(const juce::File& appendixFile,
                            const juce::Array<juce::var>& thesisFigures,
                            const juce::Array<juce::var>& dataFiles) const
    {
        juce::String text = "figurePath,template,sourceLabels,assetNames,sourceHashes,pluginChain,dataDirectory,dataFileCount,chapterHint,boundaryNote\n";
        const auto dataDirectory = dataFiles.size() > 0
            ? juce::File(dataFiles[0].toString()).getParentDirectory().getFullPathName()
            : juce::String();

        for (const auto& figure : thesisFigures)
        {
            const auto token = getString(figure, "template");
            text += csvEscape(getString(figure, "path")) + ","
                 + csvEscape(token) + ","
                 + csvEscape(joinThesisSourceField(figure, "label")) + ","
                 + csvEscape(joinThesisSourceField(figure, "assetName")) + ","
                 + csvEscape(joinThesisSourceField(figure, "sourceHash")) + ","
                 + csvEscape(pluginChainSummary()) + ","
                 + csvEscape(dataDirectory) + ","
                 + csvEscape(juce::String(dataFiles.size())) + ","
                 + csvEscape(chapterHintForThesisTemplate(token)) + ","
                 + csvEscape(getString(figure, "notes")) + "\n";
        }

        appendixFile.replaceWithText(text);
    }

    void writeManifest(const juce::File& manifestFile, const juce::Array<juce::var>& figures,
                       const juce::Array<juce::var>& dataFiles,
                       const juce::Array<juce::var>& thesisFigures,
                       const juce::File& appendixFile)
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("schemaVersion", schemaVersion);
        root->setProperty("sessionId", sessionId);
        root->setProperty("tool", "GOODMETER Audio Doctor Job Runner");
        root->setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true));
        root->setProperty("analysisSettings", writeAnalysisSettings());
        root->setProperty("exportSettings", writeExportSettings());
        writeBandHighlightProperties(*root, getBandHighlightConfig());
        const auto maskingSources = getMaskingFusionSources();
        const auto maskingBounceSource = getLayerFitFusionBounceSource();
        double layerFitSampleRate = 0.0;
        auto rememberLayerFitSampleRate = [&](const juce::String& sourceId)
        {
            const auto* asset = getAssetById(sourceId);
            if (layerFitSampleRate <= 0.0 && asset != nullptr && asset->sampleRate > 0.0)
                layerFitSampleRate = asset->sampleRate;
        };
        for (const auto& sourceId : maskingSources)
            rememberLayerFitSampleRate(sourceId);
        rememberLayerFitSampleRate(maskingBounceSource);
        if (layerFitSampleRate <= 0.0)
            layerFitSampleRate = 48000.0;

        writeMaskingFusionProperties(*root, getMaskingFusionSettings(), maskingSources, maskingBounceSource,
                                     getLayerFitBounceWarning(maskingSources, maskingBounceSource),
                                     layerFitSampleRate, getJobSpatialTimePositionSeconds(), getJobTerrainCamera(),
                                     getJobCrystalYawRadians(), getJobCrystalPitchRadians());
        root->setProperty("figures", juce::var(figures));
        root->setProperty("thesisFigures", juce::var(thesisFigures));
        root->setProperty("appendixTable", appendixFile.getFullPathName());
        root->setProperty("dataFiles", juce::var(dataFiles));
        root->setProperty("displaySlots", writeDisplaySlotsManifest());
        root->setProperty("renderRouting", writeRenderRoutingManifest());
        root->setProperty("dry", writeAssetManifest(dryAsset.get()));
        root->setProperty("dryA", writeAssetManifest(dryAsset.get()));
        root->setProperty("dryB", writeAssetManifest(dryBAsset.get()));
        root->setProperty("dryC", writeAssetManifest(dryCAsset.get()));
        root->setProperty("wetA", writeAssetManifest(wetA.get()));
        root->setProperty("wetB", writeAssetManifest(wetB.get()));
        root->setProperty("wetC", writeAssetManifest(wetC.get()));
        root->setProperty("pluginA", writePluginManifest(hostA, renderInfoA));
        root->setProperty("pluginB", writePluginManifest(hostB, renderInfoB));
        root->setProperty("pluginC", writePluginManifest(hostC, renderInfoC));
        manifestFile.replaceWithText(juce::JSON::toString(juce::var(root.release()), true));
    }

    void writeJobSummary(const juce::File& summaryFile, const juce::Array<juce::var>& figures,
                         const juce::Array<juce::var>& dataFiles,
                         const juce::Array<juce::var>& thesisFigures,
                         const juce::File& appendixFile) const
    {
        juce::String text;
        text << "# GOODMETER Audio Doctor Job Summary\n\n";
        text << "- sessionId: `" << sessionId << "`\n";
        text << "- schemaVersion: `" << schemaVersion << "`\n";
        text << "- exportPreset: `" << getExportPreset() << "`\n";
        text << "- figureSize: `" << getExportWidth() << "x" << getExportHeight() << "`\n";
        text << "- figures: `" << figures.size() << "`\n";
        text << "- thesisFigures: `" << thesisFigures.size() << "`\n";
        text << "- dataFiles: `" << dataFiles.size() << "`\n\n";
        if (thesisFigures.size() > 0)
            text << "- appendixTable: `" << appendixFile.getFullPathName() << "`\n\n";

        auto appendAsset = [&](const juce::String& label, const Asset* asset)
        {
            if (asset == nullptr)
                return;
            text << "## " << label << "\n";
            text << "- name: `" << asset->name << "`\n";
            text << "- sourceType: `" << (asset->generatedSignal ? "generated" : "file") << "`\n";
            text << "- sourceHash: `" << (asset->generatedSignal ? hashGeneratedSignalSpec(asset->generatedSignalSpec)
                                                               : hashSourceFnv1a64(asset->sourcePath)) << "`\n";
            text << "- duration: `" << juce::String(asset->metrics.durationSeconds, 3) << " s`\n";
            text << "- peak/rms/crest: `" << juce::String(asset->metrics.peakDb, 1) << " / "
                 << juce::String(asset->metrics.rmsDb, 1) << " / "
                 << juce::String(asset->metrics.crestDb, 1) << " dB`\n";
            if (asset->generatedSignal)
                text << "- generatedType: `" << normaliseGeneratedSignalSpec(asset->generatedSignalSpec).type << "`\n";
            if (! asset->stageMarkers.empty())
            {
                text << "- stageMarkers:";
                for (const auto& marker : asset->stageMarkers)
                    text << " `" << marker.label << " " << juce::String(marker.startSec, 2)
                         << "-" << juce::String(marker.endSec, 2) << "s`";
                text << "\n";
            }
            text << "\n";
        };

        appendAsset("DRY A", dryAsset.get());
        appendAsset("DRY B", dryBAsset.get());
        appendAsset("DRY C", dryCAsset.get());
        appendAsset("WET A", wetA.get());
        appendAsset("WET B", wetB.get());
        appendAsset("WET C", wetC.get());

        if (thesisFigures.size() > 0)
        {
            text << "## Thesis Figures\n";
            for (const auto& figure : thesisFigures)
                text << "- `" << getString(figure, "template") << "`: "
                     << getString(figure, "path") << "\n";
            text << "\n";
        }
        summaryFile.replaceWithText(text);
    }

    juce::var writeDisplaySlotsManifest() const
    {
        juce::Array<juce::var> slots;
        for (size_t i = 0; i < displaySlotSources.size(); ++i)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            const auto source = normaliseSourceId(displaySlotSources[i]);
            const auto* asset = getAssetById(source);
            obj->setProperty("slot", static_cast<int>(i + 1));
            obj->setProperty("source", source);
            obj->setProperty("label", labelForSourceId(source));
            obj->setProperty("hasAsset", asset != nullptr);
            if (asset != nullptr)
            {
                obj->setProperty("assetName", asset->name);
                obj->setProperty("sourcePath", asset->sourcePath);
            }
            slots.add(juce::var(obj.release()));
        }
        return juce::var(slots);
    }

    juce::var writeRenderRoutingManifest() const
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("mode", allowsMixedRenderInputs() ? "mix" : "controlled");
        writeOneRenderRoute(*root, "wetA", wetA.get(), renderInfoA);
        writeOneRenderRoute(*root, "wetB", wetB.get(), renderInfoB);
        writeOneRenderRoute(*root, "wetC", wetC.get(), renderInfoC);
        return juce::var(root.release());
    }

    void writeOneRenderRoute(juce::DynamicObject& root,
                             const juce::String& wetId,
                             const Asset* wetAsset,
                             const RenderInfo& renderInfo) const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        const auto routing = getObject(getObject(job, "renderRouting"), wetId);
        const auto inputs = getRenderInputIds(wetId);
        juce::Array<juce::var> inputArray;
        for (const auto& id : inputs)
            inputArray.add(id);

        obj->setProperty("inputs", juce::var(inputArray));
        const auto fallbackChain = wetId == "wetB" ? "pluginChainB" : (wetId == "wetC" ? "pluginChainC" : "pluginChainA");
        obj->setProperty("chain", getString(routing, "chain", fallbackChain));
        obj->setProperty("mixMode", inputs.size() > 1 ? "mixed_then_rendered" : "single_input_rendered");
        obj->setProperty("rendered", wetAsset != nullptr);
        obj->setProperty("latencySamples", renderInfo.latencySamples);
        obj->setProperty("tailSeconds", renderInfo.tailSeconds);
        const AssetPtr& reference = wetId == "wetB" ? renderReferenceB
                                 : wetId == "wetC" ? renderReferenceC
                                                    : renderReferenceA;
        const Asset* referenceAsset = reference != nullptr ? reference.get() : dryAsset.get();
        if (wetAsset != nullptr && referenceAsset != nullptr)
            obj->setProperty("apparentAttenuation",
                             writeApparentAttenuationStatsJson(
                                 computeApparentAttenuationStats(referenceAsset->dynamicsRms,
                                                                 wetAsset->dynamicsRms)));
        root.setProperty(wetId, juce::var(obj.release()));
    }

    static juce::var writeAssetManifest(const Asset* asset)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        if (asset == nullptr)
            return juce::var(obj.release());

        obj->setProperty("name", asset->name);
        obj->setProperty("sourcePath", asset->sourcePath);
        const auto sourceFileName = asset->generatedSignal ? asset->name
                                  : juce::File(asset->sourcePath).getFileName();
        obj->setProperty("sourceFileName", sourceFileName.isNotEmpty() ? sourceFileName : asset->name);
        obj->setProperty("sourceType", asset->generatedSignal ? "generated" : "file");
        obj->setProperty("sourceHash", asset->generatedSignal ? hashGeneratedSignalSpec(asset->generatedSignalSpec)
                                                              : hashSourceFnv1a64(asset->sourcePath));
        obj->setProperty("hashInputVersion", asset->generatedSignal ? generatedSignalSpecHashInputVersion() : "sourceFileBytes.fnv1a64");
        obj->setProperty("sourceBytes", asset->generatedSignal ? static_cast<juce::int64>(0) : sourceBytesOnDisk(asset->sourcePath));
        if (asset->generatedSignal)
            obj->setProperty("generatedSignalSpec", writeGeneratedSignalSpecJson(asset->generatedSignalSpec));
        obj->setProperty("sampleRate", asset->sampleRate);
        obj->setProperty("channels", asset->metrics.channels);
        obj->setProperty("durationSeconds", asset->metrics.durationSeconds);
        const double selectionStart = asset->editMetadata.valid ? asset->editMetadata.trimStartSeconds : 0.0;
        const double selectionEnd = asset->editMetadata.valid && asset->editMetadata.trimEndSeconds > selectionStart
                                  ? asset->editMetadata.trimEndSeconds
                                  : asset->metrics.durationSeconds;
        auto selection = std::make_unique<juce::DynamicObject>();
        selection->setProperty("startSeconds", selectionStart);
        selection->setProperty("endSeconds", selectionEnd);
        selection->setProperty("analysisDurationSeconds", asset->metrics.durationSeconds);
        selection->setProperty("sourceDurationKnown", asset->editMetadata.valid || asset->metrics.durationSeconds > 0.0);
        obj->setProperty("selection", juce::var(selection.release()));
        obj->setProperty("selectionStartSeconds", selectionStart);
        obj->setProperty("selectionEndSeconds", selectionEnd);
        obj->setProperty("analysisDurationSeconds", asset->metrics.durationSeconds);
        obj->setProperty("peakDb", asset->metrics.peakDb);
        obj->setProperty("rmsDb", asset->metrics.rmsDb);
        obj->setProperty("crestDb", asset->metrics.crestDb);
        obj->setProperty("reverbSpace", writeSpaceMetrics(asset->spaceMetrics));
        obj->setProperty("dynamics", writeDynamicsMetrics(asset->dynamicsMetrics));
        obj->setProperty("groupDelayMetrics", AudioDoctorFigureRenderer::writeGroupDelayMetrics(asset->groupDelay,
                                                                                                &asset->spectrum));
        obj->setProperty("spectrumPeaks", writeSpectrumPeaksJson(asset->spectrumPeaks));
        obj->setProperty("harmonicPeaks", writeSpectrumPeaksJson(asset->harmonicPeaks));
        obj->setProperty("spatialHeatmap", writeSpatialHeatmapMetricsJson(asset->spatialHeatmap.metrics));
        obj->setProperty("stageMarkers", writeStageMarkersJson(asset->stageMarkers));
        obj->setProperty("analysisSummary", writeAnalysisSummaryJson(asset->metrics));
        obj->setProperty("editMetadata", writeEditMetadata(asset->editMetadata));
        return juce::var(obj.release());
    }

    static juce::var writeEditMetadata(const EditMetadata& edit)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("valid", edit.valid);
        if (!edit.valid)
            return juce::var(obj.release());

        obj->setProperty("channelName", edit.channelName);
        obj->setProperty("originalSourcePath", edit.originalSourcePath);
        obj->setProperty("derivedSourcePath", edit.derivedSourcePath);
        obj->setProperty("trimStartSeconds", edit.trimStartSeconds);
        obj->setProperty("trimEndSeconds", edit.trimEndSeconds);
        obj->setProperty("fadeInMs", edit.fadeInMs);
        obj->setProperty("fadeOutMs", edit.fadeOutMs);
        obj->setProperty("snapToZeroCrossing", edit.snapToZeroCrossing);
        obj->setProperty("createdAt", edit.createdAt);
        return juce::var(obj.release());
    }

    static juce::var writeSpaceMetrics(const ReverbSpaceMetrics& m)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("valid", m.valid);
        obj->setProperty("tailEndSeconds", m.tailEndSeconds);
        obj->setProperty("rt20Seconds", m.rt20Seconds);
        obj->setProperty("rt30Seconds", m.rt30Seconds);
        obj->setProperty("rt60Seconds", m.rt60Seconds);
        obj->setProperty("rt60Estimated", m.rt60Seconds > 0.0f);
        obj->setProperty("rt60DerivedFrom", m.rt30Seconds > 0.0f ? "RT30" : (m.rt20Seconds > 0.0f ? "RT20" : ""));
        obj->setProperty("onsetSeconds", m.onsetSeconds);
        obj->setProperty("directWindowMs", 20);
        obj->setProperty("earlyWindowMs", 80);
        obj->setProperty("drrDb", m.drrDb);
        obj->setProperty("earlyLateDb", m.earlyLateDb);
        obj->setProperty("stereoCorrelation", m.stereoCorrelation);
        obj->setProperty("sideToMidDb", m.sideToMidDb);
        return juce::var(obj.release());
    }

    static juce::var writeAnalysisSettings()
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("spectrum", "average FFT, fftSize=16384, window=hann, overlap=50%, units=Hz/dB");
        obj->setProperty("spectrumPeaks", "local maxima over average spectrum, annotated against estimated harmonic series, units=Hz/dB/cents");
        obj->setProperty("envelope", "peak envelope, units=seconds/dBFS");
        obj->setProperty("groupDelay", "cross-spectrum Wet*conj(Dry), unwrapped phase derivative, units=Hz/ms; summary metrics ignore bins more than 45 dB below the selected spectrum peak");
        obj->setProperty("energyDecay", "Schroeder-style reverse cumulative energy from onset, units=seconds/dB");
        obj->setProperty("dynamicsRms", "50ms RMS envelope, units=seconds/dBFS");
        obj->setProperty("apparentAttenuation", "target RMS envelope minus render-reference RMS envelope; negative delta means apparent attenuation, not plugin-internal gain reduction");
        obj->setProperty("spectrogram", "STFT, fftSize=1024, hopSize=256, window=hann, linear FFT-bin image, maxWidth=2048, units=time/frequency/magnitude");
        const char* spatialTerrainDescription = "STFT, fftSize=1024, hopSize=256, window=hann, linear FFT-bin image projected as 2.5D energy terrain; widthIndex is derived from Side/Mid ratio plus L/R correlation and balance, units=seconds/Hz/dB/width";
        obj->setProperty("spatialHeatmap", spatialTerrainDescription);
        obj->setProperty("spatialTerrain", spatialTerrainDescription);
        obj->setProperty("spatialImpression", "Selected time window integrated into L-C-R by frequency energy; x=L/C/R, y=Hz, intensity=energy density, colour follows L-C-R position (blue/green=left, yellow=center, red/pink=right), while width/spread derives from L/R balance, Side/Mid ratio, and correlation.");
        return juce::var(obj.release());
    }

    juce::var writeExportSettings() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        const auto exportSpec = getObject(job, "export");
        obj->setProperty("preset", getExportPreset());
        obj->setProperty("theme", getString(exportSpec, "theme", getExportPreset()));
        obj->setProperty("palette", isAcademicLightPreset(getExportPreset()) ? "academic_light"
                                      : (isDarkPreset(getExportPreset()) ? "ui_dark" : "ui_light"));
        obj->setProperty("colormap", getString(exportSpec, "colormap", ""));
        obj->setProperty("fixedDbScale", getBool(exportSpec, "fixedDbScale", false));
        obj->setProperty("floorDb", getDouble(exportSpec, "floorDb", -80.0));
        obj->setProperty("ceilingDb", getDouble(exportSpec, "ceilingDb", 0.0));
        obj->setProperty("terrainCamera", terrainCameraToken(getJobTerrainCamera()));
        obj->setProperty("terrainTimeReversed", getJobTerrainTimeReversed());
        obj->setProperty("spatialWindow", spatialWindowToken(getJobSpatialWindow()));
        if (getJobSpatialTimePositionSeconds() >= 0.0f)
        {
            const float spatialTime = getJobSpatialTimePositionSeconds();
            obj->setProperty("spatialTimePositionSeconds", getJobSpatialTimePositionSeconds());
            float spatialDuration = 0.001f;
            double spatialSampleRate = 48000.0;
            bool spatialSampleRateSet = false;
            for (auto* asset : { dryAsset.get(), dryBAsset.get(), dryCAsset.get(), wetA.get(), wetB.get(), wetC.get() })
            {
                if (asset == nullptr)
                    continue;
                spatialDuration = juce::jmax(spatialDuration, static_cast<float>(asset->metrics.durationSeconds));
                if (!spatialSampleRateSet && asset->sampleRate > 0.0)
                {
                    spatialSampleRate = asset->sampleRate;
                    spatialSampleRateSet = true;
                }
            }
            const float width = juce::jlimit(0.080f, 0.420f, spatialDuration * 0.085f);
            const float displayStart = juce::jlimit(0.0f, spatialDuration, spatialTime - width * 0.5f);
            const float displayEnd = juce::jlimit(displayStart + 0.001f, spatialDuration, spatialTime + width * 0.5f);
            obj->setProperty("displayTimeStartSeconds", displayStart);
            obj->setProperty("displayTimeEndSeconds", displayEnd);
            obj->setProperty("analysisWindowStartSeconds", displayStart);
            obj->setProperty("analysisWindowEndSeconds", juce::jmin(spatialDuration, displayEnd + static_cast<float>(1024.0 / spatialSampleRate)));
        }
        obj->setProperty("sharedScale", getJobSharedScale());
        obj->setProperty("width", getExportWidth());
        obj->setProperty("height", getExportHeight());
        return juce::var(obj.release());
    }

    static juce::var writeDynamicsMetrics(const DynamicsMetrics& m)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("valid", m.valid);
        obj->setProperty("rmsRangeDb", m.rmsRangeDb);
        obj->setProperty("rmsP10Db", m.rmsP10Db);
        obj->setProperty("rmsP50Db", m.rmsP50Db);
        obj->setProperty("rmsP90Db", m.rmsP90Db);
        obj->setProperty("transientToSustainDb", m.transientToSustainDb);
        obj->setProperty("onsetSeconds", m.onsetSeconds);
        obj->setProperty("transientWindowStartSeconds", m.transientWindowStartSeconds);
        obj->setProperty("transientWindowEndSeconds", m.transientWindowEndSeconds);
        obj->setProperty("sustainWindowStartSeconds", m.sustainWindowStartSeconds);
        obj->setProperty("sustainWindowEndSeconds", m.sustainWindowEndSeconds);
        obj->setProperty("actualTransientWindowStartSeconds", m.actualTransientWindowStartSeconds);
        obj->setProperty("actualTransientWindowEndSeconds", m.actualTransientWindowEndSeconds);
        obj->setProperty("actualSustainWindowStartSeconds", m.actualSustainWindowStartSeconds);
        obj->setProperty("actualSustainWindowEndSeconds", m.actualSustainWindowEndSeconds);
        if (m.warning.isNotEmpty())
            obj->setProperty("warning", m.warning);
        return juce::var(obj.release());
    }

    static juce::var writeGroupDelayMetrics(const std::vector<PlotPoint>& points)
    {
        const auto stats = computeGroupDelayStats(points);
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("valid", stats.valid);
        obj->setProperty("rangeMinHz", 20.0);
        obj->setProperty("rangeMaxHz", 20000.0);
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

    static juce::var writePluginManifest(PluginHost& host, const RenderInfo& renderInfo)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        if (host.getCurrentPlugin() == nullptr)
            return juce::var(obj.release());

        const auto* desc = host.getCurrentPlugin();
        obj->setProperty("name", desc->name);
        obj->setProperty("manufacturer", desc->manufacturerName);
        obj->setProperty("format", desc->pluginFormatName);
        obj->setProperty("identifier", desc->createIdentifierString());
        obj->setProperty("latencySamples", renderInfo.latencySamples);
        obj->setProperty("tailSeconds", renderInfo.tailSeconds);
        obj->setProperty("stateLoaded", renderInfo.stateLoaded);
        obj->setProperty("stateSource", renderInfo.stateSource);
        obj->setProperty("statePath", renderInfo.statePath);
        obj->setProperty("stateHash", renderInfo.stateHash);
        obj->setProperty("stateBytes", renderInfo.stateBytes);
        obj->setProperty("stateLabel", renderInfo.stateLabel);

        juce::MemoryBlock currentState;
        juce::String stateError;
        if (host.captureCurrentState(currentState, stateError) && currentState.getSize() > 0)
        {
            obj->setProperty("stateCaptured", true);
            obj->setProperty("currentStateHash", hashMemoryBlockFnv1a64(currentState));
            obj->setProperty("currentStateBytes", static_cast<juce::int64>(currentState.getSize()));
            obj->setProperty("pluginStateBase64", currentState.toBase64Encoding());
        }
        else
        {
            obj->setProperty("stateCaptured", false);
            obj->setProperty("stateCaptureError", stateError.isNotEmpty() ? stateError : "Plugin returned an empty state.");
        }

        juce::Array<juce::var> params;
        for (const auto& p : host.listParameters())
        {
            auto param = std::make_unique<juce::DynamicObject>();
            param->setProperty("index", p.index);
            param->setProperty("id", p.id);
            param->setProperty("name", p.name);
            param->setProperty("label", p.label);
            param->setProperty("normalizedValue", p.normalisedValue);
            param->setProperty("displayValue", p.displayValue);
            param->setProperty("nameUnavailable", p.nameUnavailable);
            param->setProperty("valueText", p.valueText);
            param->setProperty("normalisedValue", p.normalisedValue);
            param->setProperty("defaultValue", p.defaultValue);
            params.add(juce::var(param.release()));
        }
        obj->setProperty("parameters", juce::var(params));

        juce::Array<juce::var> changedParams;
        for (const auto& p : host.getChangedParameters())
        {
            auto param = std::make_unique<juce::DynamicObject>();
            param->setProperty("index", p.index);
            param->setProperty("id", p.id);
            param->setProperty("name", p.name);
            param->setProperty("label", p.label);
            param->setProperty("normalizedValue", p.normalisedValue);
            param->setProperty("displayValue", p.displayValue);
            param->setProperty("nameUnavailable", p.nameUnavailable);
            param->setProperty("valueText", p.valueText);
            param->setProperty("normalisedValue", p.normalisedValue);
            changedParams.add(juce::var(param.release()));
        }
        obj->setProperty("changedParameters", juce::var(changedParams));
        return juce::var(obj.release());
    }

    juce::var job;
    int schemaVersion = 1;
    juce::String sessionId;
    juce::File outDir;
    std::unique_ptr<juce::DynamicObject> response;
    std::array<juce::String, 3> displaySlotSources { "dryA", "wetA", "wetB" };
    AssetPtr dryAsset;
    AssetPtr dryBAsset;
    AssetPtr dryCAsset;
    AssetPtr wetA;
    AssetPtr wetB;
    AssetPtr wetC;
    AssetPtr renderReferenceA;
    AssetPtr renderReferenceB;
    AssetPtr renderReferenceC;
    PluginHost hostA;
    PluginHost hostB;
    PluginHost hostC;
    RenderInfo renderInfoA;
    RenderInfo renderInfoB;
    RenderInfo renderInfoC;
};

inline bool runAudioDoctorJobFile(const juce::File& jobFile, juce::String& responseText)
{
    JobRunner runner;
    return runner.runJobFile(jobFile, responseText);
}

} // namespace goodmeter::audio_doctor
