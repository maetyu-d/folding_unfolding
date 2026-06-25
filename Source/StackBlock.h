#pragma once

#include "Geometry.h"

struct StackBlock
{
    int id = 0;
    Vec2 position;
    float size = 96.0f;
    int levels = 1;
    float levelHeight = 42.0f;
    float flash = 0.0f;
    bool powered = true;

    float topElevation() const noexcept
    {
        return static_cast<float> (juce::jlimit (1, 16, levels)) * levelHeight;
    }

    float halfSize() const noexcept
    {
        return size * 0.5f;
    }

    bool contains (Vec2 point) const noexcept
    {
        const auto half = halfSize();
        return std::abs (point.x - position.x) <= half
            && std::abs (point.y - position.y) <= half;
    }

    float hitRadius() const noexcept
    {
        return halfSize() * 1.42f;
    }
};
