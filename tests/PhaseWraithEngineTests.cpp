#include "phasewraith/PhaseWraithEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using phasewraith::PhaseWraithEngine;
using phasewraith::PhaseWraithParameters;

namespace
{

std::vector<float> renderTone (PhaseWraithParameters params, int samples)
{
    PhaseWraithEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset();

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
    {
        const auto sample = std::sin (static_cast<float> (i) * 0.043f) * 0.35f
                          + std::sin (static_cast<float> (i) * 0.179f) * 0.18f;
        output.push_back (engine.processSample (sample, sample * 0.81f).left);
    }
    return output;
}

float averageDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    assert (a.size() == b.size());
    float total = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i)
        total += std::fabs (a[i] - b[i]);
    return total / static_cast<float> (a.size());
}

float peak (const std::vector<float>& samples)
{
    float result = 0.0f;
    for (const auto sample : samples)
        result = std::max (result, std::fabs (sample));
    return result;
}

void testSilenceStaysSilent()
{
    PhaseWraithEngine engine;
    engine.prepare (48000.0);
    engine.reset();

    for (int i = 0; i < 8192; ++i)
    {
        const auto frame = engine.processSample (0.0f, 0.0f);
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}

void testDepthMovesAllpassSweep()
{
    PhaseWraithParameters shallow;
    shallow.rate = 0.48f;
    shallow.depth = 0.0f;
    shallow.mix = 1.0f;

    auto deep = shallow;
    deep.depth = 1.0f;

    const auto shallowOutput = renderTone (shallow, 8192);
    const auto deepOutput = renderTone (deep, 8192);
    assert (averageDifference (shallowOutput, deepOutput) > 0.015f);
}

void testDirectionReversesDeterministicSweep()
{
    PhaseWraithParameters forward;
    forward.rate = 0.62f;
    forward.depth = 0.82f;
    forward.direction = 1.0f;

    auto reverse = forward;
    reverse.direction = -1.0f;

    const auto forwardOutput = renderTone (forward, 8192);
    const auto reverseOutput = renderTone (reverse, 8192);
    assert (averageDifference (forwardOutput, reverseOutput) > 0.01f);
}

void testSignedFeedbackChangesResonance()
{
    PhaseWraithParameters negative;
    negative.depth = 0.8f;
    negative.feedback = -0.7f;
    negative.mix = 1.0f;

    auto positive = negative;
    positive.feedback = 0.7f;

    const auto negativeOutput = renderTone (negative, 8192);
    const auto positiveOutput = renderTone (positive, 8192);
    assert (averageDifference (negativeOutput, positiveOutput) > 0.01f);
    assert (peak (positiveOutput) <= 0.9801f);
}

void testDeterministic()
{
    PhaseWraithParameters params;
    params.rate = 0.37f;
    params.depth = 0.74f;
    params.center = 0.41f;
    params.spread = 0.66f;
    params.feedback = -0.33f;

    const auto a = renderTone (params, 4096);
    const auto b = renderTone (params, 4096);
    assert (a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        assert (std::fabs (a[i] - b[i]) <= 1.0e-6f);
}

void testFiniteBoundedExtremeParameters()
{
    PhaseWraithParameters params;
    params.rate = 1000.0f;
    params.depth = 1000.0f;
    params.center = 1000.0f;
    params.spread = 1000.0f;
    params.feedback = 1000.0f;
    params.direction = std::numeric_limits<float>::infinity();
    params.mix = 1000.0f;

    PhaseWraithEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.reset();

    for (int i = 0; i < 8192; ++i)
    {
        const auto frame = engine.processSample (1000.0f, -1000.0f);
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
    }
}

void testDenormalInputDoesNotLeak()
{
    PhaseWraithEngine engine;
    engine.prepare (48000.0);
    engine.reset();

    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample (1.0e-30f, -1.0e-30f);
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}

} // namespace

int main()
{
    testSilenceStaysSilent();
    testDepthMovesAllpassSweep();
    testDirectionReversesDeterministicSweep();
    testSignedFeedbackChangesResonance();
    testDeterministic();
    testFiniteBoundedExtremeParameters();
    testDenormalInputDoesNotLeak();

    std::cout << "PhaseWraithEngineTests passed\n";
    return 0;
}
