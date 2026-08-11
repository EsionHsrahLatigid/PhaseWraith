#include "phasewraith/PhaseWraithEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace phasewraith
{
namespace
{
constexpr float ceiling = 0.98f;

[[nodiscard]] float sanitizeAudio (float value) noexcept
{
    return clampFinite (value, -8.0f, 8.0f, 0.0f);
}

[[nodiscard]] float wrapUnit (float value) noexcept
{
    value -= std::floor (value);
    return value < 0.0f ? value + 1.0f : value;
}
} // namespace

void PhaseWraithEngine::AllpassStage::reset() noexcept
{
    z = 0.0f;
}

float PhaseWraithEngine::AllpassStage::process (float input, float coefficient) noexcept
{
    const auto safeInput = sanitizeAudio (input);
    const auto a = clampFinite (coefficient, -0.94f, 0.94f, 0.0f);
    const auto output = -a * safeInput + z;
    z = safeInput + a * output;
    return sanitizeAudio (output);
}

PhaseWraithEngine::PhaseWraithEngine()
{
    prepare (44100.0);
    reset();
}

void PhaseWraithEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    reset();
}

void PhaseWraithEngine::reset() noexcept
{
    lfoPhase = 0.0f;
    leftFeedback = 0.0f;
    rightFeedback = 0.0f;
    for (auto* bank : { &leftA, &leftB, &rightA, &rightB })
        for (auto& stage : *bank)
            stage.reset();
}

void PhaseWraithEngine::setParameters (const PhaseWraithParameters& parameters) noexcept
{
    params.rate = clampFinite (parameters.rate, 0.0f, 1.0f, PhaseWraithParameters {}.rate);
    params.depth = clampFinite (parameters.depth, 0.0f, 1.0f, PhaseWraithParameters {}.depth);
    params.center = clampFinite (parameters.center, 0.0f, 1.0f, PhaseWraithParameters {}.center);
    params.spread = clampFinite (parameters.spread, 0.0f, 1.0f, PhaseWraithParameters {}.spread);
    params.feedback = clampFinite (parameters.feedback, -0.95f, 0.95f, PhaseWraithParameters {}.feedback);
    params.direction = clampFinite (parameters.direction, -1.0f, 1.0f, PhaseWraithParameters {}.direction);
    params.mix = clampFinite (parameters.mix, 0.0f, 1.0f, PhaseWraithParameters {}.mix);
}

StereoFrame PhaseWraithEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto dryLeft = sanitizeAudio (inputLeft);
    const auto dryRight = sanitizeAudio (inputRight);
    const auto direction = params.direction < 0.0f ? -1.0f : 1.0f;
    const auto rateHz = 0.015f * std::pow (400.0f, params.rate);
    lfoPhase = wrapUnit (lfoPhase + direction * rateHz / static_cast<float> (sampleRate));

    const auto spread = (params.spread - 0.5f) * 0.45f;
    const auto wetLeft = processChannel (dryLeft, leftA, leftB, leftFeedback, spread);
    const auto wetRight = processChannel (dryRight, rightA, rightB, rightFeedback, -spread + 0.17f * params.spread);

    const auto dry = 1.0f - params.mix;
    return sanitizeFrame (dryLeft * dry + wetLeft * params.mix,
                          dryRight * dry + wetRight * params.mix);
}

void PhaseWraithEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample (left[i], right[i]);
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

float PhaseWraithEngine::processChannel (float input, Bank& bankA, Bank& bankB, float& feedbackState, float phaseOffset) noexcept
{
    const auto fed = sanitizeAudio (input + feedbackState * params.feedback);
    const auto sweepA = wrapUnit (lfoPhase + phaseOffset);
    const auto sweepB = wrapUnit (sweepA + 0.5f);
    const auto bankOutA = processBank (fed, bankA, sweepA);
    const auto bankOutB = processBank (fed, bankB, sweepB);

    const auto crossfadePhase = sweepA * 2.0f * std::numbers::pi_v<float>;
    const auto gainA = std::cos (crossfadePhase) * 0.5f + 0.5f;
    const auto gainB = std::sin (crossfadePhase) * 0.5f + 0.5f;
    const auto norm = 1.0f / std::sqrt (gainA * gainA + gainB * gainB + 1.0e-6f);
    const auto wet = (bankOutA * gainA + bankOutB * gainB) * norm;
    feedbackState = boundedDrive (wet, 0.9f);
    return sanitizeAudio (wet);
}

float PhaseWraithEngine::processBank (float input, Bank& bank, float sweep) const noexcept
{
    auto sample = input;
    for (int stage = 0; stage < static_cast<int> (bank.size()); ++stage)
        sample = bank[static_cast<std::size_t> (stage)].process (sample, stageCoefficient (sweep, stage));
    return sample;
}

float PhaseWraithEngine::stageCoefficient (float sweep, int stage) const noexcept
{
    const auto stageOffset = static_cast<float> (stage) / 6.0f;
    const auto phase = wrapUnit (sweep + stageOffset * (0.24f + params.spread * 0.32f));
    const auto sinus = 0.5f - 0.5f * std::cos (phase * 2.0f * std::numbers::pi_v<float>);
    const auto center = 0.16f + params.center * 0.62f;
    const auto depth = params.depth * 0.42f;
    const auto position = std::clamp (center + (sinus - 0.5f) * depth, 0.04f, 0.96f);
    return 0.08f + position * 0.84f;
}

StereoFrame PhaseWraithEngine::sanitizeFrame (float left, float right) const noexcept
{
    auto safeLeft = boundedDrive (left, 1.05f + std::fabs (params.feedback) * 0.45f);
    auto safeRight = boundedDrive (right, 1.05f + std::fabs (params.feedback) * 0.45f);
    if (std::fabs (safeLeft) < 1.0e-20f)
        safeLeft = 0.0f;
    if (std::fabs (safeRight) < 1.0e-20f)
        safeRight = 0.0f;
    return { std::clamp (safeLeft, -ceiling, ceiling),
             std::clamp (safeRight, -ceiling, ceiling) };
}

} // namespace phasewraith
