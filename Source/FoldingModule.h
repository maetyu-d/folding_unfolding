#pragma once

#include "Geometry.h"
#include "SonicEvent.h"
#include <array>
#include <vector>

struct FoldingModule
{
    static constexpr float defaultGlobalTempoBpm = 120.0f;
    static constexpr float minRateDivision = 0.25f;
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
    float soundingPitch = -1.0f;
    float soundingPitchFlash = 0.0f;
    bool powered = true;
    int attachedPlatterId = -1;
    int attachedStandIndex = -1;
    int attachedPlankId = -1;
    std::array<float, 8> tipPitches {};
    std::array<bool, 8> tipPitchRandom {};
    std::array<float, 8> tipPitchRandomLow {};
    std::array<float, 8> tipPitchRandomHigh {};
    std::array<float, 8> tipProbabilities { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<TipSoundLanguage, 8> tipSoundLanguages {};
    std::array<juce::String, 8> tipSoundPrograms {};

    float foldAt (double timeSeconds, float globalTempoBpm = defaultGlobalTempoBpm) const noexcept;
    float collisionRadiusAt (double timeSeconds, float globalTempoBpm = defaultGlobalTempoBpm) const noexcept;
    float angularSpeedForTempo (float globalTempoBpm) const noexcept;
    float pitchForTip (int tipIndex) const noexcept;
    bool tipIsMuted (int tipIndex) const noexcept;
    TipSoundLanguage soundLanguageForTip (int tipIndex) const noexcept;
    const juce::String& soundProgramForTip (int tipIndex) const noexcept;
    float randomPitchForTip (int tipIndex, juce::Random& random) const noexcept;
    bool shouldPlayTip (int tipIndex, juce::Random& random) const noexcept;
    void initialiseTipPitches() noexcept;
    static juce::String defaultTipSoundProgram (TipSoundLanguage language, int tipIndex = 0, int sides = 6);
    int timeSignatureNumerator() const noexcept { return sides; }
    std::vector<Vec3> floorVertices() const;
    std::vector<std::array<Vec3, 3>> flapTriangles (double timeSeconds,
                                                    float globalTempoBpm = defaultGlobalTempoBpm) const;
};
