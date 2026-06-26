#pragma once

#include "JuceIncludes.h"
#include "SonicEvent.h"
#include <vector>

class CollisionSynth
{
public:
    void prepare (double newSampleRate);
    void reset();
    void triggerSound (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride = -1.0f);
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

private:
    struct Voice
    {
        double fundamental = 440.0;
        double fifth = 660.0;
        double octave = 880.0;
        double colour = 1320.0;
        double phaseFundamental = 0.0;
        double phaseFifth = 0.0;
        double phaseOctave = 0.0;
        double phaseColour = 0.0;
        double lfoPhase = 0.0;
        double age = 0.0;
        double duration = 1.8;
        float gain = 0.2f;
        float pan = 0.0f;
        float brightness = 0.5f;
    };

    void addVoice (double midiNote, float gain, double duration, float pan, float brightness);
    static float envelopeFor (const Voice& voice) noexcept;
    static double midiToFrequency (double midiNote) noexcept;
    void resizeDelay();

    juce::CriticalSection voiceLock;
    std::vector<Voice> voices;
    std::vector<float> delayLeft;
    std::vector<float> delayRight;
    size_t delayWriteIndex = 0;
    size_t delayReadOffsetA = 1;
    size_t delayReadOffsetB = 1;
    double sampleRate = 44100.0;
};
