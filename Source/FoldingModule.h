#pragma once

#include "Geometry.h"
#include <array>
#include <vector>

struct FoldingModule
{
    static constexpr float defaultGlobalTempoBpm = 120.0f;
    static constexpr float maxRateDivision = 32.0f;

    int id = 0;
    Vec2 position;
    int sides = 6;
    float radius = 54.0f;
    float flapDepth = 32.0f;
    float elevation = 0.0f;
    float rateDivision = 1.0f;
    float phase = 0.0f;
    float rotation = 0.0f;
    float flash = 0.0f;
    bool powered = true;
    int attachedPlatterId = -1;
    int attachedStandIndex = -1;
    int attachedPlankId = -1;
    std::array<float, 8> tipPitches {};

    float foldAt (double timeSeconds, float globalTempoBpm = defaultGlobalTempoBpm) const noexcept;
    float collisionRadiusAt (double timeSeconds, float globalTempoBpm = defaultGlobalTempoBpm) const noexcept;
    float angularSpeedForTempo (float globalTempoBpm) const noexcept;
    float pitchForTip (int tipIndex) const noexcept;
    void initialiseTipPitches() noexcept;
    int timeSignatureNumerator() const noexcept { return sides; }
    std::vector<Vec3> floorVertices() const;
    std::vector<std::array<Vec3, 3>> flapTriangles (double timeSeconds,
                                                    float globalTempoBpm = defaultGlobalTempoBpm) const;
};
