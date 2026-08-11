#pragma once

#include "phasewraith/PhaseWraithDspPrimitives.h"

#include <array>

namespace phasewraith
{

/** Realtime-safe parameter set for the PhaseWraith stereo effect. */
struct PhaseWraithParameters
{
    float rate = 0.22f;
    float depth = 0.62f;
    float center = 0.48f;
    float spread = 0.34f;
    float feedback = 0.18f;
    float direction = 1.0f;
    float mix = 0.84f;
};

/** Stereo phaser with two wrapped six-stage allpass banks per channel. */
class PhaseWraithEngine
{
public:
    PhaseWraithEngine();

    /** Sets the sample rate and rebuilds coefficients; invalid rates fall back to 44.1 kHz. */
    void prepare (double sampleRate) noexcept;

    /** Clears hold, filter, and deterministic state. */
    void reset() noexcept;

    /** Clamps and applies all public parameters. */
    void setParameters (const PhaseWraithParameters& parameters) noexcept;

    /** Processes one stereo input frame and returns finite output bounded to +/-0.98. */
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;

    /** Processes stereo buffers in-place. Null buffers and non-positive sizes are ignored. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    class AllpassStage
    {
    public:
        void reset() noexcept;
        [[nodiscard]] float process (float input, float coefficient) noexcept;

    private:
        float z = 0.0f;
    };

    using Bank = std::array<AllpassStage, 6>;

    struct ClampedParameters
    {
        float rate = 0.22f;
        float depth = 0.62f;
        float center = 0.48f;
        float spread = 0.34f;
        float feedback = 0.18f;
        float direction = 1.0f;
        float mix = 0.84f;
    };

    [[nodiscard]] float processChannel (float input, Bank& bankA, Bank& bankB, float& feedbackState, float phaseOffset) noexcept;
    [[nodiscard]] float processBank (float input, Bank& bank, float sweep) const noexcept;
    [[nodiscard]] float stageCoefficient (float sweep, int stage) const noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;
    float leftFeedback = 0.0f;
    float rightFeedback = 0.0f;
    Bank leftA;
    Bank leftB;
    Bank rightA;
    Bank rightB;
};

} // namespace phasewraith
