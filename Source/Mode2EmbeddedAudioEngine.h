#pragma once

#include "CollisionSynth.h"
#include "EmbeddedScAudioEngine.h"
#include "JuceIncludes.h"
#include "SonicEvent.h"

#include <map>
#include <set>
#include <vector>

#if UNFOLDING_HAS_WELD_CHUCK
#include "EmbeddedChucKEngine.h"
#endif

class Mode2EmbeddedAudioEngine
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int outputChannels);
    void release() noexcept;
    void reset();
    void setTransport (float bpm, double timeSeconds, bool playing);
    void triggerSound (SonicEventType type,
                       int sidesA,
                       int sidesB,
                       float foldA,
                       float foldB,
                       float pitchOverride = -1.0f,
                       TipSoundLanguage language = TipSoundLanguage::superCollider,
                       const juce::String& program = {},
                       int tipIndex = -1);
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    bool isReady() const noexcept;
    juce::String getStatusText() const;
    juce::String validateProgram (TipSoundLanguage language, const juce::String& program);

private:
    struct RenderedVoice
    {
        juce::AudioBuffer<float> buffer;
        int position = 0;
    };

    gridcollider::EmbeddedScAudioEngine embeddedSc;
    CollisionSynth fallbackSynth;
    juce::AudioBuffer<float> renderScratch;
    juce::CriticalSection renderedVoiceLock;
    std::vector<RenderedVoice> renderedVoices;
    std::map<juce::String, juce::String> loadedSuperColliderPrograms;
    std::set<juce::String> failedSuperColliderPrograms;

    double currentSampleRate = 44100.0;
    int currentMaximumBlockSize = 0;
    int currentOutputChannels = 2;
    bool embeddedScReady = false;
    int nextProgramId = 1;
    float currentBpm = 120.0f;
    double currentTimeSeconds = 0.0;
    bool currentTransportPlaying = true;

    juce::String synthForProgram (const juce::String& program);
    juce::String compileSuperColliderProgram (const juce::String& program);
    static juce::String wrappedSuperColliderSource (const juce::String& synthName, const juce::String& program);
    bool renderChucKProgram (const juce::String& program,
                             int sidesA,
                             int sidesB,
                             float foldA,
                             float foldB,
                             float pitchOverride,
                             int tipIndex);
    void renderBufferedVoices (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
};
