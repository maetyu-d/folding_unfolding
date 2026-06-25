#pragma once

#include "JuceIncludes.h"
#include <cmath>

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Vec2 operator+ (Vec2 a, Vec2 b) noexcept { return { a.x + b.x, a.y + b.y }; }
inline Vec2 operator- (Vec2 a, Vec2 b) noexcept { return { a.x - b.x, a.y - b.y }; }
inline Vec2 operator* (Vec2 a, float scale) noexcept { return { a.x * scale, a.y * scale }; }
inline Vec2 operator/ (Vec2 a, float scale) noexcept { return { a.x / scale, a.y / scale }; }

inline Vec2& operator+= (Vec2& a, Vec2 b) noexcept
{
    a.x += b.x;
    a.y += b.y;
    return a;
}

inline float length (Vec2 value) noexcept
{
    return std::sqrt (value.x * value.x + value.y * value.y);
}

inline float distance (Vec2 a, Vec2 b) noexcept
{
    return length (a - b);
}

inline Vec2 normalised (Vec2 value) noexcept
{
    const auto len = length (value);
    return len > 0.0001f ? value / len : Vec2 {};
}

inline float smoothStep01 (float value) noexcept
{
    const auto t = juce::jlimit (0.0f, 1.0f, value);
    return t * t * (3.0f - 2.0f * t);
}

enum class CityViewMode
{
    isometric,
    topDown
};

struct IsoProjector
{
    float zoom = 1.0f;
    juce::Point<float> pan;
    juce::Point<float> centre;
    CityViewMode viewMode = CityViewMode::isometric;

    juce::Point<float> project (Vec3 point) const noexcept
    {
        if (viewMode == CityViewMode::topDown)
        {
            return {
                centre.x + pan.x + point.x * zoom,
                centre.y + pan.y + point.y * zoom
            };
        }

        constexpr auto cos30 = 0.8660254038f;
        constexpr auto sin30 = 0.5f;

        const auto sx = (point.x - point.y) * cos30;
        const auto sy = (point.x + point.y) * sin30 - point.z;

        return {
            centre.x + pan.x + sx * zoom,
            centre.y + pan.y + sy * zoom
        };
    }

    Vec2 unprojectToGround (juce::Point<float> screenPoint) const noexcept
    {
        return unprojectToElevation (screenPoint, 0.0f);
    }

    Vec2 unprojectToElevation (juce::Point<float> screenPoint, float elevation) const noexcept
    {
        if (viewMode == CityViewMode::topDown)
        {
            juce::ignoreUnused (elevation);
            return {
                (screenPoint.x - centre.x - pan.x) / zoom,
                (screenPoint.y - centre.y - pan.y) / zoom
            };
        }

        constexpr auto cos30 = 0.8660254038f;
        constexpr auto sin30 = 0.5f;

        const auto sx = (screenPoint.x - centre.x - pan.x) / zoom;
        const auto sy = (screenPoint.y - centre.y - pan.y) / zoom;

        const auto xMinusY = sx / cos30;
        const auto xPlusY = (sy + elevation) / sin30;

        return {
            0.5f * (xMinusY + xPlusY),
            0.5f * (xPlusY - xMinusY)
        };
    }
};
