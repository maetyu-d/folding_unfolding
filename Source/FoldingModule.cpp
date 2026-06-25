#include "FoldingModule.h"

namespace
{
constexpr auto twoPi = juce::MathConstants<float>::twoPi;
constexpr auto halfPi = juce::MathConstants<float>::halfPi;
}

float FoldingModule::foldAt (double timeSeconds, float globalTempoBpm) const noexcept
{
    const auto wave = 0.5f + 0.5f * std::sin (static_cast<float> (timeSeconds) * angularSpeedForTempo (globalTempoBpm)
                                             + phase);
    auto shaped = smoothStep01 (wave);
    shaped = smoothStep01 (shaped);
    return shaped;
}

float FoldingModule::collisionRadiusAt (double timeSeconds, float globalTempoBpm) const noexcept
{
    const auto fold = foldAt (timeSeconds, globalTempoBpm);
    const auto horizontalFlapReach = flapDepth * std::cos (fold * halfPi);
    return radius + juce::jmax (horizontalFlapReach, flapDepth * 0.18f);
}

float FoldingModule::angularSpeedForTempo (float globalTempoBpm) const noexcept
{
    const auto beatsPerSecond = juce::jmax (1.0f, globalTempoBpm) / 60.0f;
    const auto noteDivision = juce::jlimit (minRateDivision, maxRateDivision, rateDivision);
    return twoPi * beatsPerSecond * noteDivision / 4.0f;
}

float FoldingModule::pitchForTip (int tipIndex) const noexcept
{
    const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));

    if (tipPitchRandom[index])
        return juce::jlimit (0.0f, 96.0f, tipPitchRandomLow[index]);

    const auto stored = tipPitches[index];

    if (stored == 0.0f)
        return 0.0f;

    if (stored > 0.0f)
        return juce::jlimit (36.0f, 96.0f, stored);

    constexpr float scale[] = { 0.0f, 2.0f, 4.0f, 7.0f, 9.0f, 12.0f, 14.0f, 16.0f };
    return 48.0f + static_cast<float> (juce::jlimit (3, 8, sides)) + scale[index];
}

bool FoldingModule::tipIsMuted (int tipIndex) const noexcept
{
    const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));

    if (tipPitchRandom[index])
        return juce::jmax (tipPitchRandomLow[index], tipPitchRandomHigh[index]) <= 0.0f;

    return pitchForTip (tipIndex) <= 0.0f;
}

float FoldingModule::randomPitchForTip (int tipIndex, juce::Random& random) const noexcept
{
    const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));

    if (! tipPitchRandom[index])
        return pitchForTip (tipIndex);

    const auto low = juce::jlimit (0.0f, 96.0f, juce::jmin (tipPitchRandomLow[index], tipPitchRandomHigh[index]));
    const auto high = juce::jlimit (0.0f, 96.0f, juce::jmax (tipPitchRandomLow[index], tipPitchRandomHigh[index]));

    if (high <= 0.0f)
        return 0.0f;

    const auto lowNote = juce::roundToInt (low);
    const auto highNote = juce::roundToInt (high);

    return static_cast<float> (lowNote + random.nextInt (highNote - lowNote + 1));
}

bool FoldingModule::shouldPlayTip (int tipIndex, juce::Random& random) const noexcept
{
    const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
    const auto probability = juce::jlimit (0.0f, 1.0f, tipProbabilities[index]);
    return probability >= 1.0f || random.nextFloat() <= probability;
}

void FoldingModule::initialiseTipPitches() noexcept
{
    constexpr float scale[] = { 0.0f, 2.0f, 4.0f, 7.0f, 9.0f, 12.0f, 14.0f, 16.0f };
    const auto base = 48.0f + static_cast<float> (juce::jlimit (3, 8, sides));

    for (size_t i = 0; i < tipPitches.size(); ++i)
    {
        tipPitches[i] = base + scale[i];
        tipPitchRandom[i] = false;
        tipPitchRandomLow[i] = tipPitches[i];
        tipPitchRandomHigh[i] = tipPitches[i] + 7.0f;
        tipProbabilities[i] = 1.0f;
    }
}

std::vector<Vec3> FoldingModule::floorVertices() const
{
    std::vector<Vec3> vertices;
    vertices.reserve (static_cast<size_t> (sides));

    const auto angleOffset = -juce::MathConstants<float>::halfPi + rotation;

    for (int i = 0; i < sides; ++i)
    {
        const auto angle = angleOffset + twoPi * static_cast<float> (i) / static_cast<float> (sides);
        vertices.push_back ({
            position.x + radius * std::cos (angle),
            position.y + radius * std::sin (angle),
            elevation
        });
    }

    return vertices;
}

std::vector<std::array<Vec3, 3>> FoldingModule::flapTriangles (double timeSeconds, float globalTempoBpm) const
{
    const auto floor = floorVertices();
    std::vector<std::array<Vec3, 3>> triangles;
    triangles.reserve (floor.size());

    const auto fold = foldAt (timeSeconds, globalTempoBpm);
    const auto foldAngle = fold * halfPi;
    const auto horizontalReach = flapDepth * std::cos (foldAngle);
    const auto height = flapDepth * std::sin (foldAngle);

    for (int i = 0; i < sides; ++i)
    {
        const auto next = (i + 1) % sides;
        const auto a = floor[static_cast<size_t> (i)];
        const auto b = floor[static_cast<size_t> (next)];
        const auto midpoint = Vec2 { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
        const auto outward = normalised (midpoint - position);

        const auto apex = Vec3 {
            midpoint.x + outward.x * horizontalReach,
            midpoint.y + outward.y * horizontalReach,
            elevation + height
        };

        triangles.push_back ({ a, b, apex });
    }

    return triangles;
}
