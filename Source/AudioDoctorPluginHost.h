/*
  ==============================================================================
    AudioDoctorPluginHost.h
    GOODMETER standalone Audio Doctor - narrow AU/VST3 host helpers.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioDoctorAnalysis.h"

namespace goodmeter::audio_doctor
{

struct OfflineRenderResult
{
    Asset wet;
    juce::String error;
    juce::PluginDescription pluginDescription;
    int latencySamples = 0;
    double tailSeconds = 0.0;
};

struct PluginParameterSnapshot
{
    int index = -1;
    juce::String id;
    juce::String name;
    juce::String label;
    juce::String displayValue;
    juce::String valueText;
    float normalisedValue = 0.0f;
    bool nameUnavailable = false;
};

struct PluginParameterInfo
{
    int index = -1;
    juce::String id;
    juce::String name;
    juce::String label;
    juce::String displayValue;
    juce::String valueText;
    float normalisedValue = 0.0f;
    float defaultValue = 0.0f;
    bool nameUnavailable = false;
};

class PluginHost
{
public:
    PluginHost()
    {
        formatManager.addDefaultFormats();
    }

    const juce::PluginDescription* getCurrentPlugin() const
    {
        return hasPlugin ? &currentPlugin : nullptr;
    }

    juce::String getCurrentPluginName() const
    {
        return hasPlugin ? (currentPlugin.name + " (" + currentPlugin.pluginFormatName + ")")
                         : juce::String("No plugin loaded");
    }

    const std::vector<PluginParameterSnapshot>& getChangedParameters() const
    {
        return changedParameters;
    }

    std::vector<PluginParameterInfo> listParameters() const
    {
        std::vector<PluginParameterInfo> params;
        if (editableInstance == nullptr)
            return params;

        const auto& rawParams = editableInstance->getParameters();
        params.reserve(static_cast<size_t>(rawParams.size()));
        for (int i = 0; i < rawParams.size(); ++i)
        {
            auto* parameter = rawParams[i];
            if (parameter == nullptr)
                continue;

            PluginParameterInfo info;
            info.index = i;
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
                info.id = withId->paramID;
            if (info.id.isEmpty())
                info.id = makeParameterFallbackId(i);
            info.name = parameter->getName(80).trim();
            if (info.name.isEmpty())
            {
                info.name = makeParameterFallbackId(i);
                info.nameUnavailable = true;
            }
            info.label = parameter->getLabel().trim();
            info.normalisedValue = parameter->getValue();
            info.displayValue = parameter->getText(info.normalisedValue, 80).trim();
            if (info.displayValue.isEmpty())
                info.displayValue = parameter->getCurrentValueAsText().trim();
            if (info.displayValue.isEmpty())
                info.displayValue = juce::String(info.normalisedValue * 100.0f, 1) + "%";
            info.valueText = info.displayValue;
            info.defaultValue = parameter->getDefaultValue();
            params.push_back(info);
        }

        return params;
    }

    bool setParameterValue(const juce::String& key, float normalisedValue, juce::String& error)
    {
        if (editableInstance == nullptr)
        {
            error = "No editable plugin instance.";
            return false;
        }

        auto* parameter = findParameter(key);
        if (parameter == nullptr)
        {
            error = "Plugin parameter not found: " + key;
            return false;
        }

        const float value = juce::jlimit(0.0f, 1.0f, normalisedValue);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(value);
        parameter->endChangeGesture();
        refreshChangedParameterSnapshot();
        error.clear();
        return true;
    }

    bool loadPluginFromFile(const juce::File& file, juce::String& error)
    {
        clearPlugin();

        juce::OwnedArray<juce::PluginDescription> found;
        juce::StringArray paths;
        paths.add(file.getFullPathName());

        knownPlugins.scanAndAddDragAndDroppedFiles(formatManager, paths, found);

        if (found.isEmpty())
        {
            error = "No AU/VST3 plugin type found in selected package.";
            return false;
        }

        auto description = *found.getFirst();
        juce::String creationError;
        auto instance = createPreparedInstance(description, 48000.0, 512, creationError, nullptr, false);
        if (instance == nullptr)
        {
            error = creationError.isNotEmpty() ? creationError : "Plugin instance creation failed.";
            return false;
        }

        currentPlugin = description;
        editableInstance = std::move(instance);
        savedState.reset();
        refreshChangedParameterSnapshot();
        hasPlugin = true;
        error.clear();
        return true;
    }

    void clearPlugin()
    {
        if (editableInstance != nullptr)
        {
            editableInstance->releaseResources();
            editableInstance.reset();
        }

        currentPlugin = {};
        savedState.reset();
        changedParameters.clear();
        hasPlugin = false;
    }

    juce::AudioPluginInstance* getEditableInstance() const
    {
        return editableInstance.get();
    }

    bool ensureEditableInstance(juce::String& error)
    {
        if (!hasPlugin)
        {
            error = "No plugin loaded.";
            return false;
        }

        if (editableInstance != nullptr)
        {
            error.clear();
            return true;
        }

        auto instance = createPreparedInstance(currentPlugin,
                                               48000.0,
                                               512,
                                               error,
                                               savedState.getSize() > 0 ? &savedState : nullptr,
                                               false);

        if (instance == nullptr)
        {
            if (error.isEmpty())
                error = "Plugin instance creation failed.";
            return false;
        }

        editableInstance = std::move(instance);
        error.clear();
        return true;
    }

    bool suspendEditableInstance(juce::String& error)
    {
        if (editableInstance == nullptr)
        {
            error.clear();
            return true;
        }

        savedState.reset();
        refreshChangedParameterSnapshot();
        editableInstance->getStateInformation(savedState);
        editableInstance->releaseResources();
        editableInstance.reset();
        error.clear();
        return true;
    }

    std::unique_ptr<juce::AudioProcessorEditor> createEditorForCurrentPlugin(juce::String& error)
    {
        if (!ensureEditableInstance(error))
            return {};

        if (editableInstance == nullptr)
        {
            error = "No plugin instance available.";
            return {};
        }

        if (editableInstance->hasEditor())
            if (auto* editor = editableInstance->createEditorIfNeeded())
            {
                error.clear();
                return std::unique_ptr<juce::AudioProcessorEditor>(editor);
            }

        error.clear();
        return std::make_unique<juce::GenericAudioProcessorEditor>(*editableInstance);
    }

    bool captureCurrentState(juce::MemoryBlock& state, juce::String& error)
    {
        state.reset();

        if (!hasPlugin)
        {
            error = "No plugin loaded.";
            return false;
        }

        if (editableInstance != nullptr)
        {
            savedState.reset();
            refreshChangedParameterSnapshot();
            editableInstance->getStateInformation(savedState);
        }

        state = savedState;
        error.clear();
        return true;
    }

    void refreshChangedParameterSnapshot()
    {
        if (editableInstance == nullptr)
            return;

        changedParameters.clear();

        const auto& params = editableInstance->getParameters();
        for (int i = 0; i < params.size(); ++i)
        {
            auto* parameter = params[i];
            if (parameter == nullptr)
                continue;

            const float value = parameter->getValue();
            const float defaultValue = parameter->getDefaultValue();
            if (std::abs(value - defaultValue) < 0.0025f)
                continue;

            auto id = juce::String();
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
                id = withId->paramID;
            if (id.isEmpty())
                id = makeParameterFallbackId(i);

            auto name = parameter->getName(40).trim();
            bool nameUnavailable = false;
            if (name.isEmpty())
            {
                name = makeParameterFallbackId(i);
                nameUnavailable = true;
            }

            const auto label = parameter->getLabel().trim();
            auto valueText = parameter->getText(value, 80).trim();
            if (valueText.isEmpty())
                valueText = parameter->getCurrentValueAsText().trim();
            if (valueText.isEmpty())
                valueText = juce::String(value * 100.0f, 1) + "%";

            changedParameters.push_back({ i, id, name.toUpperCase(), label, valueText, valueText, value, nameUnavailable });
        }
    }

    OfflineRenderResult renderOffline(const Asset& dry,
                                      const juce::MemoryBlock* stateToApply = nullptr,
                                      int blockSize = 512,
                                      double fallbackTailSeconds = 1.0) const
    {
        OfflineRenderResult result;

        if (!hasPlugin)
        {
            result.error = "No plugin loaded.";
            return result;
        }

        if (dry.buffer.getNumSamples() <= 0 || dry.sampleRate <= 0.0)
        {
            result.error = "No dry audio available.";
            return result;
        }

        juce::String creationError;
        auto instance = createPreparedInstance(currentPlugin,
                                               dry.sampleRate,
                                               blockSize,
                                               creationError,
                                               stateToApply,
                                               true);

        if (instance == nullptr)
        {
            result.error = creationError.isNotEmpty() ? creationError : "Plugin instance creation failed.";
            return result;
        }

        const int hostChannels = 2;
        auto input = toStereoBuffer(dry.buffer);

        const double pluginTail = instance->getTailLengthSeconds();
        const double tailSeconds = std::isfinite(pluginTail) && pluginTail > 0.0
            ? juce::jlimit(0.0, 8.0, pluginTail)
            : fallbackTailSeconds;

        const int tailSamples = static_cast<int>(tailSeconds * dry.sampleRate);
        const int totalSamples = input.getNumSamples() + tailSamples;

        juce::AudioBuffer<float> output(hostChannels, totalSamples);
        output.clear();

        const int blockChannels = totalChannelsForRender(*instance, hostChannels);
        juce::AudioBuffer<float> block(blockChannels, blockSize);
        juce::MidiBuffer midi;

        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            const int numThisBlock = juce::jmin(blockSize, totalSamples - pos);
            block.clear();
            midi.clear();

            if (pos < input.getNumSamples())
            {
                const int inputSamples = juce::jmin(numThisBlock, input.getNumSamples() - pos);
                for (int ch = 0; ch < hostChannels; ++ch)
                    block.copyFrom(ch, 0, input, ch, pos, inputSamples);
            }

            instance->processBlock(block, midi);

            for (int ch = 0; ch < hostChannels; ++ch)
                output.copyFrom(ch, pos, block, ch, 0, numThisBlock);
        }

        result.latencySamples = juce::jmax(0, instance->getLatencySamples());
        result.tailSeconds = tailSeconds;
        result.pluginDescription = currentPlugin;

        instance->releaseResources();

        if (result.latencySamples > 0)
            compensateLatency(output, result.latencySamples);

        result.wet.name = dry.name + " -> " + currentPlugin.name;
        result.wet.sourcePath = dry.sourcePath;
        result.wet.sampleRate = dry.sampleRate;
        result.wet.buffer = std::move(output);
        refreshAnalysis(result.wet);
        return result;
    }

    OfflineRenderResult renderEditableOffline(const Asset& dry,
                                              int blockSize = 512,
                                              double fallbackTailSeconds = 1.0)
    {
        OfflineRenderResult result;

        if (!hasPlugin || editableInstance == nullptr)
        {
            result.error = "No plugin loaded.";
            return result;
        }

        if (dry.buffer.getNumSamples() <= 0 || dry.sampleRate <= 0.0)
        {
            result.error = "No dry audio available.";
            return result;
        }

        renderWithPreparedInstance(*editableInstance,
                                   currentPlugin,
                                   dry,
                                   result,
                                   blockSize,
                                   fallbackTailSeconds);
        return result;
    }

private:
    static juce::String makeParameterFallbackId(int index)
    {
        return "param_" + juce::String(index).paddedLeft('0', 2);
    }

    juce::AudioProcessorParameter* findParameter(const juce::String& key) const
    {
        if (editableInstance == nullptr)
            return nullptr;

        const auto trimmed = key.trim();
        if (trimmed.isEmpty())
            return nullptr;

        const int index = trimmed.getIntValue();
        const bool indexLike = trimmed.containsOnly("0123456789");
        const auto& params = editableInstance->getParameters();
        if (indexLike && juce::isPositiveAndBelow(index, params.size()))
            return params[index];

        const auto needle = trimmed.toLowerCase();
        for (auto* parameter : params)
        {
            if (parameter == nullptr)
                continue;

            if (parameter->getName(120).trim().toLowerCase() == needle)
                return parameter;

            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
                if (withId->paramID.trim().toLowerCase() == needle)
                    return parameter;
        }

        return nullptr;
    }

    static void configurePluginBuses(juce::AudioPluginInstance& instance, int channels)
    {
        auto desiredSet = channels <= 1 ? juce::AudioChannelSet::mono()
                                        : juce::AudioChannelSet::stereo();

        auto layout = instance.getBusesLayout();

        if (layout.inputBuses.size() > 0)
            layout.inputBuses.set(0, desiredSet);
        if (layout.outputBuses.size() > 0)
            layout.outputBuses.set(0, desiredSet);

        auto disabledLayout = layout;
        for (int i = 1; i < disabledLayout.inputBuses.size(); ++i)
            disabledLayout.inputBuses.set(i, juce::AudioChannelSet::disabled());
        for (int i = 1; i < disabledLayout.outputBuses.size(); ++i)
            disabledLayout.outputBuses.set(i, juce::AudioChannelSet::disabled());

        if (instance.checkBusesLayoutSupported(disabledLayout))
            instance.setBusesLayout(disabledLayout);
        else if (instance.checkBusesLayoutSupported(layout))
            instance.setBusesLayout(layout);
    }

    static int totalChannelsForRender(const juce::AudioPluginInstance& instance, int hostChannels)
    {
        return juce::jmax(hostChannels,
                          instance.getTotalNumInputChannels(),
                          instance.getTotalNumOutputChannels());
    }

    static void compensateLatency(juce::AudioBuffer<float>& buffer, int latencySamples)
    {
        if (latencySamples <= 0 || latencySamples >= buffer.getNumSamples())
            return;

        const int remaining = buffer.getNumSamples() - latencySamples;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            std::move(data + latencySamples, data + buffer.getNumSamples(), data);
            std::fill(data + remaining, data + buffer.getNumSamples(), 0.0f);
        }
    }

    static void renderWithPreparedInstance(juce::AudioPluginInstance& instance,
                                           const juce::PluginDescription& description,
                                           const Asset& dry,
                                           OfflineRenderResult& result,
                                           int blockSize,
                                           double fallbackTailSeconds)
    {
        configurePluginBuses(instance, 2);
        instance.setNonRealtime(true);
        instance.setRateAndBufferSizeDetails(dry.sampleRate, blockSize);
        instance.prepareToPlay(dry.sampleRate, blockSize);

        const int hostChannels = 2;
        auto input = toStereoBuffer(dry.buffer);

        const double pluginTail = instance.getTailLengthSeconds();
        const double tailSeconds = std::isfinite(pluginTail) && pluginTail > 0.0
            ? juce::jlimit(0.0, 8.0, pluginTail)
            : fallbackTailSeconds;

        const int tailSamples = static_cast<int>(tailSeconds * dry.sampleRate);
        const int totalSamples = input.getNumSamples() + tailSamples;

        juce::AudioBuffer<float> output(hostChannels, totalSamples);
        output.clear();

        const int blockChannels = totalChannelsForRender(instance, hostChannels);
        juce::AudioBuffer<float> block(blockChannels, blockSize);
        juce::MidiBuffer midi;

        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            const int numThisBlock = juce::jmin(blockSize, totalSamples - pos);
            block.clear();
            midi.clear();

            if (pos < input.getNumSamples())
            {
                const int inputSamples = juce::jmin(numThisBlock, input.getNumSamples() - pos);
                for (int ch = 0; ch < hostChannels; ++ch)
                    block.copyFrom(ch, 0, input, ch, pos, inputSamples);
            }

            instance.processBlock(block, midi);

            for (int ch = 0; ch < hostChannels; ++ch)
                output.copyFrom(ch, pos, block, ch, 0, numThisBlock);
        }

        result.latencySamples = juce::jmax(0, instance.getLatencySamples());
        result.tailSeconds = tailSeconds;
        result.pluginDescription = description;

        instance.releaseResources();

        if (result.latencySamples > 0)
            compensateLatency(output, result.latencySamples);

        result.wet.name = dry.name + " -> " + description.name;
        result.wet.sourcePath = dry.sourcePath;
        result.wet.sampleRate = dry.sampleRate;
        result.wet.buffer = std::move(output);
        refreshAnalysis(result.wet);
    }

    std::unique_ptr<juce::AudioPluginInstance> createPreparedInstance(const juce::PluginDescription& description,
                                                                      double sampleRate,
                                                                      int blockSize,
                                                                      juce::String& error,
                                                                      const juce::MemoryBlock* stateToApply,
                                                                      bool nonRealtime) const
    {
        auto instance = formatManager.createPluginInstance(description,
                                                           sampleRate,
                                                           blockSize,
                                                           error);
        if (instance == nullptr)
            return {};

        configurePluginBuses(*instance, 2);
        instance->setNonRealtime(nonRealtime);
        instance->setRateAndBufferSizeDetails(sampleRate, blockSize);

        if (stateToApply != nullptr && stateToApply->getSize() > 0)
            instance->setStateInformation(stateToApply->getData(),
                                          static_cast<int>(stateToApply->getSize()));

        instance->prepareToPlay(sampleRate, blockSize);
        return instance;
    }

    mutable juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::PluginDescription currentPlugin;
    std::unique_ptr<juce::AudioPluginInstance> editableInstance;
    juce::MemoryBlock savedState;
    std::vector<PluginParameterSnapshot> changedParameters;
    bool hasPlugin = false;
};

} // namespace goodmeter::audio_doctor
