#pragma once

#include "Geometry.h"

enum class PowerSwitchActivationMode
{
    tipToggle,
    timedOff
};

enum class PowerSwitchRetriggerPolicy
{
    ignoreWhileOff,
    turnOnWhileOff
};

struct PowerSwitch
{
    int id = 0;
    Vec2 position;
    float elevation = 0.0f;
    float triggerRadius = 34.0f;
    float areaRadius = 230.0f;
    bool powered = true;
    float flash = 0.0f;
    float pulse = 0.0f;
    double restoreAtSeconds = -1.0;
    float offDurationSeconds = 8.0f;
    PowerSwitchActivationMode activationMode = PowerSwitchActivationMode::tipToggle;
    PowerSwitchRetriggerPolicy retriggerPolicy = PowerSwitchRetriggerPolicy::ignoreWhileOff;

    bool controls (Vec2 point) const noexcept
    {
        return distance (position, point) <= areaRadius;
    }
};

struct PowerSource
{
    int id = 0;
    Vec2 position;
    float elevation = 0.0f;
    float radius = 46.0f;
    bool powered = true;
    float flash = 0.0f;
    double restoreAtSeconds = -1.0;
};
