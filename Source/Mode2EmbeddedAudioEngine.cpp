#include "Mode2EmbeddedAudioEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int melodicDegrees[] = { 0, 2, 4, 7, 9, 12 };

double midiForEvent (int sides, float fold, float pitchOverride) noexcept
{
    if (pitchOverride > 0.0f)
        return juce::jlimit (24.0f, 108.0f, pitchOverride);

    const auto sideIndex = juce::jlimit (3, 8, sides) - 3;
    const auto degree = melodicDegrees[static_cast<size_t> (sideIndex)];
    const auto lift = fold > 0.72f ? 12 : 0;
    return 60.0 + static_cast<double> (degree + lift);
}

juce::String instrumentForEvent (SonicEventType type, int sides, float fold)
{
    if (type == SonicEventType::sectionChange)
        return "pad";

    if (type == SonicEventType::switchOn)
        return "choir";

    if (type == SonicEventType::switchOff)
        return "sub";

    if (type == SonicEventType::cablePulse)
        return "grain";

    if (type == SonicEventType::collision)
        return fold > 0.7f ? "metal" : "perc";

    switch (juce::jlimit (3, 8, sides))
    {
        case 3:  return "pluck";
        case 4:  return "bell";
        case 5:  return "fm";
        case 6:  return "string";
        case 7:  return "metal";
        case 8:  return "choir";
        default: break;
    }

    return "tone";
}

float velocityForEvent (SonicEventType type, float foldA, float foldB) noexcept
{
    const auto fold = juce::jlimit (0.0f, 1.0f, 0.5f * (foldA + foldB));

    if (type == SonicEventType::tipTrigger)
        return juce::jlimit (0.18f, 0.92f, 0.46f + fold * 0.32f);

    if (type == SonicEventType::collision)
        return juce::jlimit (0.12f, 0.62f, 0.24f + fold * 0.18f);

    if (type == SonicEventType::sectionChange)
        return 0.52f;

    return juce::jlimit (0.10f, 0.70f, 0.28f + fold * 0.20f);
}

std::uint64_t durationTicksForEvent (SonicEventType type) noexcept
{
    switch (type)
    {
        case SonicEventType::tipTrigger:    return 1;
        case SonicEventType::collision:     return 1;
        case SonicEventType::cablePulse:    return 1;
        case SonicEventType::switchOn:      return 2;
        case SonicEventType::switchOff:     return 2;
        case SonicEventType::sectionChange: return 4;
        default: break;
    }

    return 1;
}

int panColumnForEvent (int sidesA, int sidesB, float foldA, float foldB) noexcept
{
    const auto shape = static_cast<float> (juce::jlimit (-5, 5, sidesA - sidesB));
    const auto motion = juce::jlimit (-1.0f, 1.0f, (foldA - foldB) + shape * 0.12f);
    return juce::jlimit (0, 63, juce::roundToInt (31.5f + motion * 28.0f));
}
}

void Mode2EmbeddedAudioEngine::prepare (double sampleRate, int maximumBlockSize, int outputChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentMaximumBlockSize = juce::jmax (64, maximumBlockSize);
    currentOutputChannels = juce::jmax (1, outputChannels);

    fallbackSynth.prepare (currentSampleRate);
    renderScratch.setSize (currentOutputChannels, currentMaximumBlockSize, false, true, false);

    embeddedScReady = embeddedSc.prepare (currentSampleRate, currentMaximumBlockSize, currentOutputChannels);
    if (embeddedScReady)
        embeddedSc.setMasterLevel (0.82f);
}

void Mode2EmbeddedAudioEngine::release() noexcept
{
    embeddedSc.release();
    embeddedScReady = false;
    fallbackSynth.reset();
    renderScratch.setSize (0, 0);

    const juce::ScopedLock lock (renderedVoiceLock);
    renderedVoices.clear();
}

void Mode2EmbeddedAudioEngine::reset()
{
    fallbackSynth.reset();
    loadedSuperColliderPrograms.clear();
    failedSuperColliderPrograms.clear();
    nextProgramId = 1;

    {
        const juce::ScopedLock lock (renderedVoiceLock);
        renderedVoices.clear();
    }

    if (embeddedScReady)
        embeddedSc.setTransport (currentBpm, 0, currentTransportPlaying);
}

void Mode2EmbeddedAudioEngine::setTransport (float bpm, double timeSeconds, bool playing)
{
    currentBpm = juce::jlimit (40.0f, 240.0f, bpm);
    currentTimeSeconds = juce::jmax (0.0, timeSeconds);
    currentTransportPlaying = playing;

    if (embeddedScReady)
    {
        const auto beat = currentTimeSeconds * static_cast<double> (currentBpm) / 60.0;
        embeddedSc.setTransport (currentBpm, static_cast<std::uint64_t> (juce::jmax (0.0, beat)), playing);
    }
}

void Mode2EmbeddedAudioEngine::triggerSound (SonicEventType type,
                                             int sidesA,
                                             int sidesB,
                                             float foldA,
                                             float foldB,
                                             float pitchOverride,
                                             TipSoundLanguage language,
                                             const juce::String& program,
                                             int tipIndex)
{
    if (! embeddedScReady)
    {
        fallbackSynth.triggerSound (type, sidesA, sidesB, foldA, foldB, pitchOverride);
        return;
    }

    if (type == SonicEventType::tipTrigger
        && language == TipSoundLanguage::chuck
        && renderChucKProgram (program, sidesA, sidesB, foldA, foldB, pitchOverride, tipIndex))
    {
        return;
    }

    gridcollider::EventFields fields;
    fields.timestampSeconds = currentTimeSeconds;
    fields.tick = static_cast<std::uint64_t> (juce::jmax (0.0, currentTimeSeconds * static_cast<double> (currentBpm) / 60.0));
    fields.sourceCell = { panColumnForEvent (sidesA, sidesB, foldA, foldB), juce::jlimit (0, 63, sidesA * 7) };
    fields.instrumentName = instrumentForEvent (type, sidesA, foldA);

    if (type == SonicEventType::tipTrigger && language == TipSoundLanguage::superCollider)
    {
        if (const auto synth = synthForProgram (program); synth.isNotEmpty())
            fields.instrumentName = synth;
    }

    fields.pitch = juce::roundToInt (midiForEvent (sidesA, foldA, pitchOverride));
    fields.velocity = velocityForEvent (type, foldA, foldB);
    fields.durationTicks = durationTicksForEvent (type);
    fields.parameters["fold"] = juce::String (juce::jlimit (0.0f, 1.0f, foldA), 3);
    fields.parameters["otherFold"] = juce::String (juce::jlimit (0.0f, 1.0f, foldB), 3);
    fields.parameters["sides"] = juce::String (juce::jlimit (3, 8, sidesA));
    fields.parameters["otherSides"] = juce::String (juce::jlimit (3, 8, sidesB));
    fields.parameters["tip"] = juce::String (juce::jlimit (0, 7, tipIndex));
    fields.parameters["velocity"] = juce::String (fields.velocity, 3);
    fields.parameters["tempo"] = juce::String (currentBpm, 3);

    embeddedSc.setTransport (currentBpm, fields.tick, currentTransportPlaying);
    embeddedSc.enqueue ({ gridcollider::InternalEvent { gridcollider::NoteEvent { fields } } });
}

void Mode2EmbeddedAudioEngine::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    if (! embeddedScReady)
    {
        fallbackSynth.render (buffer, startSample, numSamples);
        renderBufferedVoices (buffer, startSample, numSamples);
        return;
    }

    if (numSamples > currentMaximumBlockSize)
        return;

    renderScratch.setSize (currentOutputChannels, numSamples, false, false, true);
    embeddedSc.render (renderScratch);

    const auto channelsToCopy = juce::jmin (buffer.getNumChannels(), renderScratch.getNumChannels());
    for (int channel = 0; channel < channelsToCopy; ++channel)
        buffer.addFrom (channel, startSample, renderScratch, channel, 0, numSamples);

    renderBufferedVoices (buffer, startSample, numSamples);
}

bool Mode2EmbeddedAudioEngine::isReady() const noexcept
{
    return embeddedScReady;
}

juce::String Mode2EmbeddedAudioEngine::getStatusText() const
{
    return embeddedScReady ? embeddedSc.getStatusText()
                           : "Embedded SC fallback: " + embeddedSc.getLastError();
}

juce::String Mode2EmbeddedAudioEngine::validateProgram (TipSoundLanguage language, const juce::String& program)
{
    if (program.trim().isEmpty())
        return "empty program";

    if (language == TipSoundLanguage::chuck)
    {
       #if UNFOLDING_HAS_WELD_CHUCK
        return "ChucK ready";
       #else
        return "ChucK unavailable in this build";
       #endif
    }

    if (! embeddedScReady)
        return "SC not ready";

    if (synthForProgram (program).isNotEmpty())
        return "compiled";

    const auto error = embeddedSc.getLastError().trim();
    return error.isNotEmpty() ? "error: " + error : "compile error";
}

juce::String Mode2EmbeddedAudioEngine::synthForProgram (const juce::String& program)
{
    const auto trimmed = program.trim();

    if (trimmed.isEmpty())
        return {};

    if (const auto existing = loadedSuperColliderPrograms.find (trimmed); existing != loadedSuperColliderPrograms.end())
        return existing->second;

    if (failedSuperColliderPrograms.count (trimmed) > 0)
        return {};

    return compileSuperColliderProgram (trimmed);
}

juce::String Mode2EmbeddedAudioEngine::compileSuperColliderProgram (const juce::String& program)
{
    const auto synthName = "unfold_tip_" + juce::String (nextProgramId++);
    const auto source = wrappedSuperColliderSource (synthName, program);

    if (! embeddedSc.loadSynthDef (synthName, source))
    {
        failedSuperColliderPrograms.insert (program);
        return {};
    }

    loadedSuperColliderPrograms[program] = synthName;
    return synthName;
}

juce::String Mode2EmbeddedAudioEngine::wrappedSuperColliderSource (const juce::String& synthName, const juce::String& program)
{
    const auto trimmed = program.trim();

    if (trimmed.containsIgnoreCase ("SynthDef("))
        return trimmed.replace ("__name__", synthName, true);

    return "SynthDef(\\"
        + synthName
        + ", {\n"
        + trimmed
        + "\n})";
}

bool Mode2EmbeddedAudioEngine::renderChucKProgram (const juce::String& program,
                                                   int sidesA,
                                                   int sidesB,
                                                   float foldA,
                                                   float foldB,
                                                   float pitchOverride,
                                                   int tipIndex)
{
#if UNFOLDING_HAS_WELD_CHUCK
    const auto trimmed = program.trim();

    if (trimmed.isEmpty())
        return false;

    EmbeddedChucKEngine engine;
    constexpr auto chunkSize = 256;

    if (! engine.prepare (currentSampleRate, chunkSize, 0, currentOutputChannels))
        return false;

    std::vector<EmbeddedChucKEngine::ParameterBinding> bindings {
        { "hostPitch", static_cast<float> (midiForEvent (sidesA, foldA, pitchOverride)), 0.0f, 127.0f },
        { "hostAmp", velocityForEvent (SonicEventType::tipTrigger, foldA, foldB) * 0.46f, 0.0f, 1.0f },
        { "hostSustain", 0.42f + juce::jlimit (0.0f, 1.0f, foldA) * 0.68f, 0.01f, 4.0f },
        { "hostFold", juce::jlimit (0.0f, 1.0f, foldA), 0.0f, 1.0f },
        { "hostOtherFold", juce::jlimit (0.0f, 1.0f, foldB), 0.0f, 1.0f },
        { "hostVelocity", velocityForEvent (SonicEventType::tipTrigger, foldA, foldB), 0.0f, 1.0f },
        { "hostTempo", currentBpm, 1.0f, 320.0f },
        { "hostTip", static_cast<float> (juce::jlimit (0, 7, tipIndex)), 0.0f, 7.0f },
        { "hostSides", static_cast<float> (juce::jlimit (3, 8, sidesA)), 3.0f, 8.0f },
        { "hostOtherSides", static_cast<float> (juce::jlimit (3, 8, sidesB)), 3.0f, 8.0f }
    };

    if (! engine.loadProgram (trimmed, bindings))
        return false;

    for (size_t i = 0; i < bindings.size(); ++i)
        engine.setParameterValue (static_cast<int> (i), bindings[i].defaultValue);

    const auto framesToRender = juce::roundToInt (currentSampleRate * 2.4);
    RenderedVoice voice;
    voice.buffer.setSize (currentOutputChannels, framesToRender, false, true, false);

    juce::AudioBuffer<float> emptyInput;
    juce::AudioBuffer<float> chunkOutput (currentOutputChannels, chunkSize);

    for (int offset = 0; offset < framesToRender; offset += chunkSize)
    {
        const auto frames = juce::jmin (chunkSize, framesToRender - offset);
        chunkOutput.clear();
        chunkOutput.setSize (currentOutputChannels, frames, false, false, true);
        engine.process (emptyInput, chunkOutput);

        for (int channel = 0; channel < currentOutputChannels; ++channel)
            voice.buffer.copyFrom (channel, offset, chunkOutput, channel, 0, frames);
    }

    engine.release();

    const juce::ScopedLock lock (renderedVoiceLock);
    renderedVoices.push_back (std::move (voice));

    if (renderedVoices.size() > 12)
        renderedVoices.erase (renderedVoices.begin(),
                              renderedVoices.begin() + static_cast<std::ptrdiff_t> (renderedVoices.size() - 12));

    return true;
#else
    juce::ignoreUnused (program, sidesA, sidesB, foldA, foldB, pitchOverride, tipIndex);
    return false;
#endif
}

void Mode2EmbeddedAudioEngine::renderBufferedVoices (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! renderedVoiceLock.tryEnter())
        return;

    const auto channelCount = buffer.getNumChannels();

    for (auto& voice : renderedVoices)
    {
        const auto remaining = voice.buffer.getNumSamples() - voice.position;
        const auto frames = juce::jmin (numSamples, remaining);

        if (frames <= 0)
            continue;

        const auto sourceChannels = voice.buffer.getNumChannels();
        for (int channel = 0; channel < channelCount; ++channel)
            buffer.addFrom (channel,
                            startSample,
                            voice.buffer,
                            juce::jmin (channel, sourceChannels - 1),
                            voice.position,
                            frames);

        voice.position += frames;
    }

    renderedVoices.erase (std::remove_if (renderedVoices.begin(),
                                          renderedVoices.end(),
                                          [] (const RenderedVoice& voice)
                                          {
                                              return voice.position >= voice.buffer.getNumSamples();
                                          }),
                          renderedVoices.end());

    renderedVoiceLock.exit();
}
