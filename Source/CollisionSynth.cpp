#include "CollisionSynth.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto twoPi = juce::MathConstants<double>::twoPi;
constexpr int melodicDegrees[] = { 0, 2, 4, 7, 9, 12 };

float smoothStep (float value) noexcept
{
    const auto t = juce::jlimit (0.0f, 1.0f, value);
    return t * t * (3.0f - 2.0f * t);
}

double noteForSides (int sides, float fold) noexcept
{
    const auto degree = melodicDegrees[static_cast<size_t> (juce::jlimit (3, 8, sides) - 3)];
    const auto foldLift = fold > 0.72f ? 12 : 0;

    return 60.0 + static_cast<double> (degree + foldLift);
}
}

void CollisionSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    resizeDelay();
    reset();
}

void CollisionSynth::reset()
{
    const juce::ScopedLock lock (voiceLock);
    voices.clear();
    voices.reserve (18);
    std::fill (delayLeft.begin(), delayLeft.end(), 0.0f);
    std::fill (delayRight.begin(), delayRight.end(), 0.0f);
    delayWriteIndex = 0;
}

void CollisionSynth::addVoice (double midiNote, float gain, double duration, float pan, float brightness)
{
    Voice voice;
    voice.fundamental = midiToFrequency (midiNote);
    voice.fifth = midiToFrequency (midiNote + 7.0);
    voice.octave = midiToFrequency (midiNote + 12.0);
    voice.colour = midiToFrequency (midiNote + 24.0);
    voice.duration = duration;
    voice.gain = gain;
    voice.pan = juce::jlimit (-0.86f, 0.86f, pan);
    voice.brightness = juce::jlimit (0.0f, 1.0f, brightness);
    voices.push_back (voice);
}

void CollisionSynth::triggerSound (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride)
{
    const auto averageFold = 0.5f * (foldA + foldB);
    const auto noteA = noteForSides (sidesA, foldA);
    const auto noteB = noteForSides (sidesB, foldB);
    const auto melodicNote = pitchOverride > 0.0f ? static_cast<double> (juce::jlimit (36.0f, 96.0f, pitchOverride))
                                                  : foldA >= foldB ? noteA : noteB;
    const auto openness = juce::jlimit (0.0f, 1.0f, std::abs (static_cast<float> (sidesA - sidesB)) / 5.0f);
    const auto pan = juce::jlimit (-0.72f, 0.72f, static_cast<float> (sidesA - sidesB) * 0.18f);

    const juce::ScopedLock lock (voiceLock);

    if (type == SonicEventType::collision)
    {
        addVoice (melodicNote,
                  0.044f + averageFold * 0.016f,
                  0.46 + averageFold * 0.24,
                  pan,
                  0.10f + averageFold * 0.10f + openness * 0.04f);
    }
    else if (type == SonicEventType::switchOn)
    {
        addVoice (melodicNote, 0.052f, 0.82, pan, 0.18f);
        addVoice (melodicNote + 7.0, 0.032f, 0.96, pan * 0.55f + 0.16f, 0.16f);
    }
    else if (type == SonicEventType::switchOff)
    {
        addVoice (melodicNote - 12.0, 0.048f, 0.74, pan, 0.08f);
        addVoice (melodicNote - 5.0, 0.024f, 0.62, pan * 0.5f - 0.12f, 0.05f);
    }
    else if (type == SonicEventType::cablePulse)
    {
        addVoice (melodicNote + 12.0, 0.030f, 0.50, pan * 0.65f, 0.12f);
        addVoice (melodicNote + 19.0, 0.018f, 0.62, pan * 0.35f + 0.22f, 0.10f);
    }
    else if (type == SonicEventType::tipTrigger)
    {
        const auto tipNote = pitchOverride > 0.0f ? melodicNote : melodicNote + 24.0;
        addVoice (tipNote, 0.040f, 0.30, pan * 0.4f, 0.26f);
        addVoice (tipNote + 12.0, 0.014f, 0.36, pan * 0.2f + 0.18f, 0.18f);
    }
    else if (type == SonicEventType::sectionChange)
    {
        addVoice (melodicNote, 0.060f, 0.95, -0.24f, 0.18f);
        addVoice (melodicNote + 7.0, 0.040f, 1.08, 0.18f, 0.16f);
        addVoice (melodicNote + 12.0, 0.028f, 1.20, 0.42f, 0.12f);
    }

    if (voices.size() > 18)
        voices.erase (voices.begin(), voices.begin() + static_cast<std::ptrdiff_t> (voices.size() - 18));
}

void CollisionSynth::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;

    const auto channelCount = buffer.getNumChannels();

    if (channelCount == 0)
        return;

    if (! voiceLock.tryEnter())
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto left = 0.0f;
        auto right = 0.0f;

        for (auto& voice : voices)
        {
            const auto env = envelopeFor (voice);
            const auto drift = 0.003 * std::sin (voice.lfoPhase);
            const auto fundamental = std::sin (voice.phaseFundamental);
            const auto fifth = std::sin (voice.phaseFifth + drift);
            const auto octave = std::sin (voice.phaseOctave);
            const auto colour = std::sin (voice.phaseColour - drift);
            const auto shimmer = 0.74 * fundamental
                               + 0.14 * fifth
                               + 0.10 * octave
                               + static_cast<double> (voice.brightness) * 0.025 * colour;
            const auto value = static_cast<float> (shimmer) * env * voice.gain;
            const auto panMotion = juce::jlimit (-0.86f, 0.86f, voice.pan + static_cast<float> (0.08 * std::sin (voice.lfoPhase * 0.37)));
            const auto leftGain = std::sqrt (0.5f * (1.0f - panMotion));
            const auto rightGain = std::sqrt (0.5f * (1.0f + panMotion));

            left += value * leftGain;
            right += value * rightGain;

            voice.phaseFundamental += twoPi * voice.fundamental / sampleRate;
            voice.phaseFifth += twoPi * voice.fifth / sampleRate;
            voice.phaseOctave += twoPi * voice.octave / sampleRate;
            voice.phaseColour += twoPi * voice.colour / sampleRate;
            voice.lfoPhase += twoPi * 0.08 / sampleRate;
            voice.age += 1.0 / sampleRate;

            if (voice.phaseFundamental > twoPi) voice.phaseFundamental -= twoPi;
            if (voice.phaseFifth > twoPi) voice.phaseFifth -= twoPi;
            if (voice.phaseOctave > twoPi) voice.phaseOctave -= twoPi;
            if (voice.phaseColour > twoPi) voice.phaseColour -= twoPi;
            if (voice.lfoPhase > twoPi) voice.lfoPhase -= twoPi;
        }

        if (! delayLeft.empty())
        {
            const auto readIndexA = (delayWriteIndex + delayLeft.size() - delayReadOffsetA) % delayLeft.size();
            const auto readIndexB = (delayWriteIndex + delayRight.size() - delayReadOffsetB) % delayRight.size();
            const auto delayedLeft = delayLeft[readIndexA];
            const auto delayedRight = delayRight[readIndexB];
            const auto inputLeft = left;
            const auto inputRight = right;

            left += delayedRight * 0.08f;
            right += delayedLeft * 0.08f;

            delayLeft[delayWriteIndex] = inputLeft + delayedRight * 0.12f;
            delayRight[delayWriteIndex] = inputRight + delayedLeft * 0.12f;
            delayWriteIndex = (delayWriteIndex + 1) % delayLeft.size();
        }

        left = std::tanh (left * 0.92f);
        right = std::tanh (right * 0.92f);

        buffer.addSample (0, startSample + sample, left);

        if (channelCount > 1)
            buffer.addSample (1, startSample + sample, right);
    }

    voices.erase (std::remove_if (voices.begin(),
                                  voices.end(),
                                  [] (const Voice& voice) { return voice.age >= voice.duration; }),
                  voices.end());
    voiceLock.exit();
}

float CollisionSynth::envelopeFor (const Voice& voice) noexcept
{
    constexpr auto attack = 0.014;

    if (voice.age < attack)
        return smoothStep (static_cast<float> (voice.age / attack));

    const auto progress = juce::jlimit (0.0, 1.0, voice.age / voice.duration);
    const auto release = 1.0 - smoothStep (static_cast<float> ((progress - 0.38) / 0.62));
    const auto decay = std::exp (-3.3 * progress);

    return static_cast<float> (decay * release);
}

double CollisionSynth::midiToFrequency (double midiNote) noexcept
{
    return 440.0 * std::pow (2.0, (midiNote - 69.0) / 12.0);
}

void CollisionSynth::resizeDelay()
{
    const auto size = static_cast<size_t> (juce::jmax (1.0, sampleRate * 0.72));
    delayLeft.assign (size, 0.0f);
    delayRight.assign (size, 0.0f);
    const auto maxOffset = size > 1 ? size - 1 : size;
    delayReadOffsetA = juce::jlimit<size_t> (1, maxOffset, static_cast<size_t> (sampleRate * 0.31));
    delayReadOffsetB = juce::jlimit<size_t> (1, maxOffset, static_cast<size_t> (sampleRate * 0.43));
    delayWriteIndex = 0;
}
