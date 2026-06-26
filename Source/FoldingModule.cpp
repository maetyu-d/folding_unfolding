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

TipSoundLanguage FoldingModule::soundLanguageForTip (int tipIndex) const noexcept
{
    return tipSoundLanguages[static_cast<size_t> (juce::jlimit (0, 7, tipIndex))];
}

const juce::String& FoldingModule::soundProgramForTip (int tipIndex) const noexcept
{
    return tipSoundPrograms[static_cast<size_t> (juce::jlimit (0, 7, tipIndex))];
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
        tipSoundLanguages[i] = TipSoundLanguage::superCollider;
        tipSoundPrograms[i] = defaultTipSoundProgram (TipSoundLanguage::superCollider,
                                                      static_cast<int> (i),
                                                      sides);
    }
}

juce::String FoldingModule::defaultTipSoundProgram (TipSoundLanguage language, int tipIndex, int sides)
{
    if (language == TipSoundLanguage::chuck)
    {
        return "// Tip " + juce::String (juce::jlimit (0, 7, tipIndex) + 1) + " ChucK one-shot sketch\n"
            "SinOsc osc => ADSR env => dac;\n"
            "Std.mtof(hostPitch + (hostFold * 12.0)) => osc.freq;\n"
            "env.set(4::ms, (80 + hostSustain * 420)::ms, 0.0, 120::ms);\n"
            "hostAmp * hostVelocity => osc.gain;\n"
            "env.keyOn();\n"
            "(120 + hostSustain * 520)::ms => now;\n"
            "env.keyOff();\n"
            "160::ms => now;\n";
    }

    return "|out = 0, pitch = 60, amp = 0.32, sustain = 0.45, pan = 0, fold = 1, sides = "
        + juce::String (juce::jlimit (3, 8, sides))
        + ", tip = "
        + juce::String (juce::jlimit (0, 7, tipIndex))
        + ", velocity = 1|\n"
          "var freq = (48 + (sides * 1.5) + (tip * 2) + (fold * 12)).midicps;\n"
          "var env = EnvGen.kr(Env.perc(0.004, sustain.max(0.05), curve: -5), doneAction: 2);\n"
          "var tone = SinOsc.ar(freq * [1, 2.01], 0, [0.82, 0.12]).sum;\n"
          "tone = tone + (BPF.ar(WhiteNoise.ar(0.18), freq * (4 + fold), 0.18) * fold);\n"
          "Out.ar(out, Pan2.ar(tone * env * amp * velocity, pan));";
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
