#include "PhaseWraithPlugin.h"

#include "ProductState.h"

#if ! PHASEWRAITH_HEADLESS_TEST
#include "ParameterGridEditor.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace phasewraith::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'P', 'W', 'R', '1' }};
constexpr int stateVersion = 1;
constexpr std::size_t presetParameterCount = 7;

constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ 0.22f, 0.62f, 0.48f, 0.34f, 0.18f, 1.00f, 0.84f }},
    {{ 0.38f, 0.78f, 0.42f, 0.62f, -0.36f, -1.00f, 0.90f }},
    {{ 0.14f, 0.88f, 0.58f, 0.92f, 0.24f, 1.00f, 0.78f }},
    {{ 0.56f, 0.72f, 0.36f, 0.48f, 0.66f, 1.00f, 0.72f }}
}};

yup::AudioParameter::Ptr makeParameter (const char* id,
                                        const char* name,
                                        int hostID,
                                        float minValue,
                                        float maxValue,
                                        float defaultValue,
                                        yup::AudioParameter::ParameterUnit unit,
                                        float smoothingMs)
{
    return yup::AudioParameterBuilder()
        .withID (id)
        .withName (name)
        .withHostID (static_cast<yup::uint32> (hostID))
        .withRange (minValue, maxValue)
        .withDefault (defaultValue)
        .withSmoothing (smoothingMs)
        .withModulatable (true)
        .withUnit (unit)
        .build();
}
} // namespace

PhaseWraithPlugin::PhaseWraithPlugin()
    : yup::AudioProcessor ("PhaseWraith",
                           yup::AudioBusLayout ({
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2),
                                                },
                                                {
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2),
                                                }))
{
    parameters[rate] = makeParameter ("rate", "Rate", rate, 0.0f, 1.0f, presetValues[0][rate], yup::AudioParameter::ParameterUnit::Percent, 60.0f);
    parameters[depth] = makeParameter ("depth", "Depth", depth, 0.0f, 1.0f, presetValues[0][depth], yup::AudioParameter::ParameterUnit::Percent, 28.0f);
    parameters[center] = makeParameter ("center", "Center", center, 0.0f, 1.0f, presetValues[0][center], yup::AudioParameter::ParameterUnit::Percent, 28.0f);
    parameters[spread] = makeParameter ("spread", "Spread", spread, 0.0f, 1.0f, presetValues[0][spread], yup::AudioParameter::ParameterUnit::Percent, 32.0f);
    parameters[feedback] = makeParameter ("feedback", "Feedback", feedback, -0.95f, 0.95f, presetValues[0][feedback], yup::AudioParameter::ParameterUnit::Percent, 34.0f);
    parameters[direction] = makeParameter ("direction", "Direction", direction, -1.0f, 1.0f, presetValues[0][direction], yup::AudioParameter::ParameterUnit::Percent, 80.0f);
    parameters[mix] = makeParameter ("mix", "Mix", mix, 0.0f, 1.0f, presetValues[0][mix], yup::AudioParameter::ParameterUnit::Percent, 20.0f);

    for (const auto& parameter : parameters)
        addParameter (parameter);

    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void PhaseWraithPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    engine.reset();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);

    syncParameterValuesFromParameters();
    updateEngineParameters();
    controlUpdateCountdown = 0;
    inputPeakMilli.store (0, std::memory_order_relaxed);
    outputPeakMilli.store (0, std::memory_order_relaxed);

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionSampleRate = std::isfinite (spec.sampleRate) && spec.sampleRate > 1.0 ? spec.sampleRate : 44100.0;
    auditionPhase = 0.0f;
    auditionNoise = 0x6d2b79f5u;
#endif
}

void PhaseWraithPlugin::releaseResources()
{
}

void PhaseWraithPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;
    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        advanceParameterHandles (sample);
        if (controlUpdateCountdown <= 0)
        {
            updateEngineParameters();
            controlUpdateCountdown = parameterUpdateCadenceSamples;
        }
        --controlUpdateCountdown;

        auto inputLeft = left != nullptr ? left[sample] : 0.0f;
        auto inputRight = right != nullptr ? right[sample] : inputLeft;

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
        const auto audition = renderAuditionFrame();
        inputLeft += audition.left;
        inputRight += audition.right;
#endif

        blockInputPeak = std::max (blockInputPeak, std::max (std::fabs (inputLeft), std::fabs (inputRight)));

        const auto frame = engine.processSample (inputLeft, inputRight);
        if (left != nullptr)
            left[sample] = frame.left;
        if (right != nullptr)
            right[sample] = frame.right;
        blockOutputPeak = std::max (blockOutputPeak, std::max (std::fabs (frame.left), std::fabs (frame.right)));

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;
    }

    inputPeakMilli.store (static_cast<int> (std::clamp (blockInputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f),
                          std::memory_order_relaxed);
    outputPeakMilli.store (static_cast<int> (std::clamp (blockOutputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f),
                           std::memory_order_relaxed);
    context.midi.clear();
}

void PhaseWraithPlugin::flush()
{
    engine.reset();
    controlUpdateCountdown = 0;
    inputPeakMilli.store (0, std::memory_order_relaxed);
    outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionPhase = 0.0f;
    auditionNoise = 0x6d2b79f5u;
#endif
}

bool PhaseWraithPlugin::acceptsMidi() const noexcept
{
    return false;
}

bool PhaseWraithPlugin::producesMidi() const noexcept
{
    return false;
}

int PhaseWraithPlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void PhaseWraithPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}

int PhaseWraithPlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String PhaseWraithPlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void PhaseWraithPlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result PhaseWraithPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0;
    const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
    if (result.failed())
        return result;

    currentPreset.store (loadedPreset, std::memory_order_relaxed);
    return yup::Result::ok();
}

yup::Result PhaseWraithPlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed));
}

bool PhaseWraithPlugin::hasEditor() const
{
#if PHASEWRAITH_HEADLESS_TEST
    return false;
#else
    return true;
#endif
}

yup::AudioProcessorEditor* PhaseWraithPlugin::createEditor()
{
#if PHASEWRAITH_HEADLESS_TEST
    return nullptr;
#else
    return new ParameterGridEditor (*this,
                                    "PhaseWraith",
                                    "Dual six-stage barberpole phaser with standalone-only audition.",
                                    0xfff2f2f0u);
#endif
}

float PhaseWraithPlugin::getInputPeakLevel() const noexcept
{
    return static_cast<float> (inputPeakMilli.load (std::memory_order_relaxed)) * 0.001f;
}

float PhaseWraithPlugin::getOutputPeakLevel() const noexcept
{
    return static_cast<float> (outputPeakMilli.load (std::memory_order_relaxed)) * 0.001f;
}

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
void PhaseWraithPlugin::setAuditionEnabled (bool shouldBeEnabled) noexcept
{
    auditionEnabled.store (shouldBeEnabled ? 1 : 0, std::memory_order_relaxed);
}

bool PhaseWraithPlugin::isAuditionEnabled() const noexcept
{
    return auditionEnabled.load (std::memory_order_relaxed) != 0;
}

void PhaseWraithPlugin::setAuditionType (int type) noexcept
{
    auditionType.store (std::clamp (type, 0, 1), std::memory_order_relaxed);
}

int PhaseWraithPlugin::getAuditionType() const noexcept
{
    return auditionType.load (std::memory_order_relaxed);
}
#endif

void PhaseWraithPlugin::advanceParameterHandles (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
    {
        parameterHandles[i].advanceToSample (samplePosition);
        currentParameterValues[i] = parameterHandles[i].getNextValue();
    }
}

void PhaseWraithPlugin::syncParameterValuesFromParameters() noexcept
{
    for (std::size_t i = 0; i < parameters.size(); ++i)
        currentParameterValues[i] = parameters[i]->getValue();
}

void PhaseWraithPlugin::updateEngineParameters() noexcept
{
    phasewraith::PhaseWraithParameters engineParameters;
    engineParameters.rate = currentParameterValues[rate];
    engineParameters.depth = currentParameterValues[depth];
    engineParameters.center = currentParameterValues[center];
    engineParameters.spread = currentParameterValues[spread];
    engineParameters.feedback = currentParameterValues[feedback];
    engineParameters.direction = currentParameterValues[direction];
    engineParameters.mix = currentParameterValues[mix];
    engine.setParameters (engineParameters);
}

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
StereoFrame PhaseWraithPlugin::renderAuditionFrame() noexcept
{
    if (auditionEnabled.load (std::memory_order_relaxed) == 0)
        return {};

    auditionPhase += 96.0f / static_cast<float> (auditionSampleRate);
    if (auditionPhase >= 1.0f)
        auditionPhase -= 1.0f;

    auditionNoise ^= auditionNoise << 13u;
    auditionNoise ^= auditionNoise >> 17u;
    auditionNoise ^= auditionNoise << 5u;
    if (auditionNoise == 0u)
        auditionNoise = 0x6d2b79f5u;

    const auto type = auditionType.load (std::memory_order_relaxed);
    const auto noise = static_cast<float> (static_cast<double> (auditionNoise) / 2147483648.0 - 1.0);
    const auto pulse = auditionPhase < 0.18f ? 1.0f : -0.55f;
    const auto saw = auditionPhase * 2.0f - 1.0f;
    const auto source = type == 0 ? saw * 0.22f + noise * 0.035f : pulse * 0.18f + noise * 0.055f;
    return { source, source * 0.93f };
}
#endif

} // namespace phasewraith::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new phasewraith::plugin::PhaseWraithPlugin();
}
