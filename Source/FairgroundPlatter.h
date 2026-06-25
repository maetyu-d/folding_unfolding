#pragma once

#include "Geometry.h"
#include "JuceIncludes.h"

#include <array>

struct FairgroundPlatter
{
    static constexpr float minRotationsPerBar = 0.125f;
    static constexpr float maxRotationsPerBar = 4.0f;

    int id = 0;
    Vec2 position;
    int stands = 4;
    float diameter = 260.0f;
    float elevation = 0.0f;
    float rateDivision = 1.0f;
    float phase = 0.0f;
    float rotation = 0.0f;
    float flash = 0.0f;
    bool powered = true;
    int attachedPlatterId = -1;
    int attachedStandIndex = -1;
    int attachedPlankId = -1;
    std::array<float, 8> standFlashes {};

    float angularSpeedForTempo (float globalTempoBpm) const noexcept
    {
        const auto beatsPerSecond = juce::jmax (1.0f, globalTempoBpm) / 60.0f;
        const auto rotationsPerBar = juce::jlimit (minRotationsPerBar, maxRotationsPerBar, rateDivision);
        return juce::MathConstants<float>::twoPi * beatsPerSecond * rotationsPerBar / 4.0f;
    }

    float rotationAt (double timeSeconds, float globalTempoBpm) const noexcept
    {
        return static_cast<float> (timeSeconds) * angularSpeedForTempo (globalTempoBpm) + phase;
    }

    Vec2 standPosition (int standIndex, double timeSeconds, float globalTempoBpm) const noexcept
    {
        const auto clampedStands = juce::jlimit (1, 8, stands);
        const auto angle = rotation + rotationAt (timeSeconds, globalTempoBpm)
                         + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                             / static_cast<float> (clampedStands);
        const auto radius = diameter * 0.5f;

        return {
            position.x + std::cos (angle) * radius,
            position.y + std::sin (angle) * radius
        };
    }

    float mountRadius() const noexcept
    {
        return juce::jlimit (10.0f, 44.0f, diameter * 0.055f);
    }

    float hitRadius() const noexcept
    {
        return diameter * 0.5f + mountRadius();
    }
};
