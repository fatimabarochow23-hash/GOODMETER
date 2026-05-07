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
        renderInfo.valid = true;
        return true;
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
        for (const auto& spec : specs)
        {
            const auto token = normaliseThesisTemplateToken(getString(spec, "template", spec.toString()));
            if (token.isEmpty())
                continue;

            auto figureData = makeThesisFigureData(spec, token);
            const auto png = figureDir.getChildFile(sanitizeFileToken(sessionId + "_thesis_" + token + "_" + preset))
                                      .withFileExtension(".png");
            AudioDoctorFigureRenderer::writePng(png, figureData, dark,
                                                juce::jmax(1800, getExportWidth()),
                                                juce::jmax(1050, getExportHeight()),
                                                academicLight);
            figures.add(png.getFullPathName());

            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty("template", token);
            obj->setProperty("viewToken", figureData.viewToken);
            obj->setProperty("path", png.getFullPathName());
            obj->setProperty("preset", preset);
            obj->setProperty("palette", academicLight ? "academic_light" : (dark ? "ui_dark" : "ui_light"));
            obj->setProperty("width", juce::jmax(1800, getExportWidth()));
            obj->setProperty("height", juce::jmax(1050, getExportHeight()));
            obj->setProperty("sources", writeThesisFigureSources(figureData));
            if (figureData.processingNote.isNotEmpty())
                obj->setProperty("processingNote", figureData.processingNote);
            obj->setProperty("notes", thesisTemplateBoundaryNote(token));
            thesisFigures.add(juce::var(obj.release()));
        }
    }

    juce::Array<juce::var> getThesisFigureSpecs() const
    {
        juce::Array<juce::var> specs;
        const auto thesis = getObject(job, "thesisFigures").isVoid() ? getObject(job, "thesisFigure")
                                                                     : getObject(job, "thesisFigures");
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
        if (token.contains("cst") || token.contains("charge") || token.contains("shot") || token.contains("tail"))
            return "cst_spectrogram";
        if (token.contains("group") || token.contains("delay"))
            return "group_delay_combo";
        if (token.contains("dynamic") || token.contains("duck") || token.contains("attenuation"))
            return "dynamics_apparent_ducking";
        if (token.contains("spatial") || token.contains("heatmap") || token.contains("width"))
            return "spatial_heatmap";
        if (token.contains("reverb") || token.contains("space"))
            return "reverb_space_combo";
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
            return "Thesis Figure - Spatial Energy Heatmap";
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
            return "Spatial width is derived from local Side/Mid ratio, L/R correlation, and L/R balance; it is an offline analysis map, not a plugin-internal meter.";
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
            || lower == "spatial_heatmap")
            return thesisFigureViewForToken(lower);
        if (lower.contains("spatial") || lower.contains("heatmap") || lower.contains("width"))
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
        obj->setProperty("spatialHeatmap", "STFT, fftSize=1024, hopSize=256, window=hann, linear FFT-bin image, hue/widthIndex from Side/Mid ratio plus L/R correlation and balance, units=seconds/Hz/dB/width");
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
