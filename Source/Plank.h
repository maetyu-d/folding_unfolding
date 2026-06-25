#pragma once

#include "Geometry.h"
#include "JuceIncludes.h"

struct Plank
{
    int id = 0;
    Vec2 position;
    float elevation = 0.0f;
    float length = 288.0f;
    float angle = 0.0f;
    float rotation = 0.0f;
    float flash = 0.0f;
    bool powered = true;
    int attachedPlatterId = -1;
    int attachedStandIndex = -1;

    Vec2 socketPosition() const noexcept
    {
        const auto direction = rotation + angle;
        return {
            position.x + std::cos (direction) * length,
            position.y + std::sin (direction) * length
        };
    }

    float socketRadius() const noexcept
    {
        return juce::jlimit (18.0f, 52.0f, length * 0.085f);
    }

    float hitRadius() const noexcept
    {
        return length + socketRadius();
    }
};
