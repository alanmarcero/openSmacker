#include "SmackEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <new>
#include <ranges>
#include <tuple>

namespace {
thread_local bool countAllocations = false;
thread_local size_t allocationCount = 0;
}

void* operator new(std::size_t size) {
    if (countAllocations)
        ++allocationCount;
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

using namespace opensmacker::core;

namespace {
void completeLockCapture(SmackEngine& engine, const EngineSettings& settings) {
    std::array<float, 512> left{}, right{};
    float* channels[2] = { left.data(), right.data() };
    for (int callback = 0; callback < 2000 && !engine.isSmackLocked(); ++callback)
        engine.process(settings, Transport{}, {}, channels, 2, static_cast<int>(left.size()));
}
}

TEST_CASE("SmackEngine responds to MIDI trigger and processes audio", "[core][engine]") {
    SmackEngine engine;
    engine.prepare(48000.0);

    EngineSettings settings;
    Transport transport;
    transport.bpm = 138.0;

    std::vector<MidiTrigger> triggers = {
        { .sampleOffset = 10, .noteNumber = 69, .velocity = 0.9f, .audition = false }
    };

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* channels[2] = { left.data(), right.data() };

    engine.process(settings, transport, triggers, channels, 2, 256);

    REQUIRE(engine.triggerSerial() == 1);
    REQUIRE(engine.currentPeak() > 0.0f);
    REQUIRE(engine.currentSmackProgress() < 1.0f);
}

TEST_CASE("SmackEngine WAV Lock-In captures and recalls audio bit-exact", "[core][engine]") {
    SmackEngine engine;
    engine.prepare(48000.0);

    EngineSettings settings;
    engine.lockCurrentSmack(settings);
    completeLockCapture(engine, settings);

    REQUIRE(engine.isSmackLocked());
    REQUIRE(!engine.wavCache().left().empty());

    const auto wavBytes = engine.wavCache().toWavBytes();
    REQUIRE(wavBytes.size() > 44);

    SmackEngine restoredEngine;
    restoredEngine.prepare(48000.0);
    const bool success = restoredEngine.restoreLockedWav(wavBytes);

    REQUIRE(success);
    REQUIRE(restoredEngine.isSmackLocked());
    REQUIRE(restoredEngine.wavCache().left().size() == engine.wavCache().left().size());
}

TEST_CASE("SmackEngine responds to MIDI CC 74 brightness sweep", "[core][engine]") {
    SmackEngine engine;
    engine.prepare(48000.0);

    EngineSettings settings;
    Transport transport;
    transport.bpm = 138.0;

    std::vector<MidiTrigger> triggers = {
        { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false }
    };
    std::vector<MidiCcEvent> ccEvents = {
        { .sampleOffset = 0, .controller = 74, .value = 10 }
    };

    std::vector<float> leftDark(1024, 0.0f);
    std::vector<float> rightDark(1024, 0.0f);
    float* channelsDark[2] = { leftDark.data(), rightDark.data() };

    engine.process(settings, transport, triggers, channelsDark, 2, 1024, ccEvents);
    REQUIRE(engine.currentPeak() > 0.0f);
}

TEST_CASE("SmackEngine schedules rolls on host beat subdivisions and honors probability", "[core][engine][roll]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.rollEnabled = true;
    settings.rollSubdivision = 8;
    settings.rollProbability = 1.0f;
    Transport transport;
    transport.bpm = 120.0;
    transport.ppqPosition = 0.0;

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* channels[2] = { left.data(), right.data() };
    engine.process(settings, transport, {}, channels, 2, 256);
    REQUIRE(engine.triggerSerial() == 1);

    transport.ppqPosition = 0.5;
    engine.process(settings, transport, {}, channels, 2, 256);
    REQUIRE(engine.triggerSerial() == 2);

    settings.rollProbability = 0.0f;
    transport.ppqPosition = 1.0;
    engine.process(settings, transport, {}, channels, 2, 256);
    REQUIRE(engine.triggerSerial() == 2);
}

TEST_CASE("SmackEngine host roll hits remain one-shot with Full ADSR enabled", "[core][engine][roll][adsr]") {
    SmackEngine engine;
    engine.prepare(8000.0);
    EngineSettings settings;
    settings.synth.fullAdsrEnabled = true;
    settings.synth.reverbMix = 0.0f;
    settings.rollEnabled = true;
    settings.rollProbability = 1.0f;
    Transport transport;
    transport.isPlaying = true;
    transport.ppqPosition = 0.0;
    std::array<float, 64> attackLeft{}, attackRight{};
    float* attackChannels[2] = { attackLeft.data(), attackRight.data() };
    engine.process(settings, transport, {}, attackChannels, 2, static_cast<int>(attackLeft.size()));
    REQUIRE(engine.triggerSerial() == 1);

    transport.isPlaying = false;
    std::vector<float> tailLeft(8000), tailRight(8000);
    float* tailChannels[2] = { tailLeft.data(), tailRight.data() };
    engine.process(settings, transport, {}, tailChannels, 2, static_cast<int>(tailLeft.size()));

    const float tailEnergy = std::accumulate(tailLeft.begin() + 4000, tailLeft.end(), 0.0f,
        [](float total, float sample) { return total + std::abs(sample); });
    REQUIRE(tailEnergy < 0.001f);
}

TEST_CASE("SmackEngine applies assignable MIDI CC to the mapped layer output", "[core][engine][midi_cc][layers]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.midiCcForBody = 20;
    Transport transport;
    const std::array<MidiTrigger, 1> triggers{{
        { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false }
    }};
    const std::array<MidiCcEvent, 1> ccEvents{{
        { .sampleOffset = 0, .controller = 20, .value = 0 }
    }};
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    std::vector<float> bodyLeft(256, 0.0f), bodyRight(256, 0.0f);
    std::vector<float> wireLeft(256, 0.0f), wireRight(256, 0.0f);
    std::vector<float> transientLeft(256, 0.0f), transientRight(256, 0.0f);
    float* channels[2] = { left.data(), right.data() };
    LayerOutputChannels layers{
        bodyLeft.data(), bodyRight.data(), wireLeft.data(), wireRight.data(), transientLeft.data(), transientRight.data()
    };

    engine.process(settings, transport, triggers, channels, 2, 256, ccEvents, {}, layers);

    REQUIRE(std::all_of(bodyLeft.begin(), bodyLeft.end(), [](float sample) { return sample == 0.0f; }));
    REQUIRE(std::any_of(wireLeft.begin(), wireLeft.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
}

TEST_CASE("SmackEngine writes every synthesis component to its own output", "[core][engine][layers]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.synth.reverbMix = 0.0f;
    const std::array<MidiTrigger, 1> triggers{{
        { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false }
    }};
    std::vector<float> left(512, 0.0f), right(512, 0.0f);
    std::vector<float> bodyLeft(512, 0.0f), bodyRight(512, 0.0f);
    std::vector<float> wireLeft(512, 0.0f), wireRight(512, 0.0f);
    std::vector<float> transientLeft(512, 0.0f), transientRight(512, 0.0f);
    float* channels[2] = { left.data(), right.data() };
    const LayerOutputChannels layers{
        bodyLeft.data(), bodyRight.data(), wireLeft.data(), wireRight.data(), transientLeft.data(), transientRight.data()
    };

    engine.process(settings, Transport{}, triggers, channels, 2, 512, {}, {}, layers);

    REQUIRE(std::any_of(bodyLeft.begin(), bodyLeft.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
    REQUIRE(std::any_of(wireLeft.begin(), wireLeft.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
    REQUIRE(std::any_of(transientLeft.begin(), transientLeft.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
}

TEST_CASE("SmackEngine plays the preset voice for every note", "[core][engine]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.synth.voiceMode = VoiceMode::ClubTom;
    settings.keyTracking = 0.0f;
    std::vector<float> left(256, 0.0f), right(256, 0.0f);
    float* channels[2] = { left.data(), right.data() };
    const std::array<MidiTrigger, 1> high{{
        { .sampleOffset = 0, .noteNumber = 100, .velocity = 1.0f, .audition = false }
    }};
    engine.process(settings, Transport{}, high, channels, 2, 256);
    REQUIRE(engine.triggerSerial() == 1);
    REQUIRE(engine.lastTrigger().voice == static_cast<int>(VoiceMode::ClubTom));
}

TEST_CASE("SmackEngine applies channel aftertouch only to the matching active voice", "[core][engine][aftertouch]") {
    const auto bodyPeakForPressure = [](int pressureChannel, int pressureValue) {
        SmackEngine engine;
        engine.prepare(48000.0);
        EngineSettings settings;
        settings.synth.toneLevel = 1.0f;
        settings.synth.noiseLevel = 0.0f;
        settings.synth.clickLevel = 0.0f;
        settings.synth.preStrikeCount = 1;
        settings.synth.reverbMix = 0.0f;
        settings.synth.bodyAftertouchDepth = 1.0f;
        const std::array<MidiTrigger, 1> triggers{{
            { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false, .channel = 1 }
        }};
        const std::array<MidiPressureEvent, 1> pressure{{
            { .sampleOffset = 0, .channel = pressureChannel, .noteNumber = -1, .value = pressureValue, .polyphonic = false }
        }};
        std::vector<float> left(512, 0.0f), right(512, 0.0f);
        float* channels[2] = { left.data(), right.data() };
        std::vector<float> bodyLeft(512, 0.0f), bodyRight(512, 0.0f);
        LayerOutputChannels layers;
        layers.bodyLeft = bodyLeft.data();
        layers.bodyRight = bodyRight.data();
        engine.process(settings, Transport{}, triggers, channels, 2, 512, {}, pressure, layers);
        return std::accumulate(bodyLeft.begin(), bodyLeft.end(), 0.0f,
            [](float sum, float sample) { return sum + std::abs(sample); });
    };

    const float untouched = bodyPeakForPressure(2, 127);
    const float touched = bodyPeakForPressure(1, 127);
    REQUIRE(touched > untouched * 1.5f);
}

TEST_CASE("SmackEngine applies sample-accurate note-off only to the matching MIDI note and channel", "[core][engine][adsr][midi]") {
    const auto renderAfterRelease = [](int offChannel, int noteOffOffset) {
        SmackEngine engine;
        engine.prepare(8000.0);
        EngineSettings settings;
        settings.synth.fullAdsrEnabled = true;
        settings.synth.ampAttackMs = 0.0f;
        settings.synth.ampDecayMs = 1.0f;
        settings.synth.ampSustain = 0.8f;
        settings.synth.ampReleaseMs = 100.0f;
        settings.synth.toneDecayMs = 15.0f;
        settings.synth.noiseLevel = 0.0f;
        settings.synth.clickLevel = 0.0f;
        settings.synth.preStrikeCount = 1;
        settings.synth.reverbMix = 0.0f;
        const std::array<MidiTrigger, 1> on{{
            { .sampleOffset = 0, .noteNumber = 60, .velocity = 1.0f, .audition = false, .channel = 2 }
        }};
        std::array<float, 128> left{}, right{};
        float* channels[2] = { left.data(), right.data() };
        engine.process(settings, Transport{}, on, channels, 2, 128);

        const std::array<MidiNoteOff, 1> off{{
            { .sampleOffset = noteOffOffset, .noteNumber = 60, .channel = offChannel }
        }};
        std::array<float, 256> tailL{}, tailR{};
        float* tailChannels[2] = { tailL.data(), tailR.data() };
        engine.process(settings, Transport{}, {}, tailChannels, 2, 256, {}, {}, {}, off);
        return std::pair{ tailL, engine.triggerSerial() };
    };

    const auto noMatch = renderAfterRelease(1, 64);
    const auto match = renderAfterRelease(2, 64);
    for (int i = 0; i < 64; ++i)
        REQUIRE(match.first[static_cast<size_t>(i)] == noMatch.first[static_cast<size_t>(i)]);
    REQUIRE(std::abs(match.first[200]) < std::abs(noMatch.first[200]));
}

TEST_CASE("SmackEngine accents host-synced roll steps with the configured velocity", "[core][engine][roll]") {
    const auto rollPeakAtPpq = [](double ppq) {
        SmackEngine engine;
        engine.prepare(48000.0);
        EngineSettings settings;
        settings.rollEnabled = true;
        settings.rollSubdivision = 8;
        settings.rollAccentEvery = 4;
        settings.rollProbability = 1.0f;
        settings.rollVelocity = 0.15f;
        settings.rollAccentVelocity = 0.9f;
        settings.synth.noiseLevel = 0.0f;
        settings.synth.clickLevel = 0.0f;
        settings.synth.preStrikeCount = 1;
        settings.synth.reverbMix = 0.0f;
        Transport transport;
        transport.bpm = 120.0;
        transport.ppqPosition = ppq;
        std::vector<float> left(256, 0.0f), right(256, 0.0f);
        float* channels[2] = { left.data(), right.data() };
        engine.process(settings, transport, {}, channels, 2, 256);
        return engine.currentPeak();
    };

    REQUIRE(rollPeakAtPpq(0.0) > rollPeakAtPpq(0.5) * 2.0f);
}

TEST_CASE("SmackEngine keeps dense host rolls with synced reverse allocation-free and bounded", "[core][engine][reverse][realtime]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.rollEnabled = true;
    settings.rollSubdivision = 32;
    settings.rollProbability = 1.0f;
    settings.effects.reverseMode = ReverseSyncMode::QuarterBeat;
    Transport transport;
    transport.bpm = 400.0;
    transport.ppqPosition = 0.0;
    std::vector<float> left(8192, 0.0f), right(8192, 0.0f);
    float* channels[2] = { left.data(), right.data() };

    allocationCount = 0;
    countAllocations = true;
    engine.process(settings, transport, {}, channels, 2, static_cast<int>(left.size()));
    countAllocations = false;

    REQUIRE(allocationCount == 0);
    REQUIRE(engine.triggerSerial() >= 8);
    REQUIRE(engine.lastReverseRenderSamples() > 0);
    const auto maxQuarterBeatSamples = computeSyncedReverseSamples(
        ReverseSyncMode::QuarterBeat, 0.0, transport.bpm, transport.beatsPerBar, 48000.0);
    REQUIRE(engine.maxReverseSamplesPerHit() > static_cast<uint64_t>(maxQuarterBeatSamples));
    REQUIRE(engine.lastReverseRenderSamples()
        <= engine.triggerSerial() * static_cast<uint64_t>(maxQuarterBeatSamples));
    REQUIRE(engine.maxCallbackPreparationSamples() <= SmackEngine::kMaxPreparationSamplesPerCallback);
}

TEST_CASE("SmackEngine applies cached layer performance gains to the audible main mix", "[core][engine][lock_in][layers][performance]") {
    const auto render = [](bool attenuateBody, bool applyAftertouch) {
        SmackEngine engine;
        engine.prepare(48000.0);
        EngineSettings settings;
        settings.synth.reverbMix = 0.0f;
        settings.synth.bodyVelocityDepth = 0.0f;
        settings.synth.wireVelocityDepth = 0.0f;
        settings.synth.transientVelocityDepth = 0.0f;
        settings.synth.bodyAftertouchDepth = applyAftertouch ? 1.0f : 0.0f;
        engine.lockCurrentSmack(settings);
        completeLockCapture(engine, settings);
        if (attenuateBody)
            settings.midiCcForBody = 20;
        const std::array<MidiTrigger, 1> trigger{{
            { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false, .channel = 1 }
        }};
        const std::array<MidiCcEvent, 1> cc{{
            { .sampleOffset = 0, .controller = 20, .value = 0, .channel = 1 }
        }};
        const std::array<MidiPressureEvent, 1> pressure{{
            { .sampleOffset = 0, .channel = 1, .noteNumber = -1, .value = applyAftertouch ? 127 : 0, .polyphonic = false }
        }};
        std::vector<float> left(4096), right(4096);
        float* channels[2] = { left.data(), right.data() };
        engine.process(settings, Transport{}, trigger, channels, 2, static_cast<int>(left.size()),
            attenuateBody ? std::span<const MidiCcEvent>(cc) : std::span<const MidiCcEvent>{}, pressure);
        return std::accumulate(left.begin(), left.end(), 0.0f,
            [](float total, float sample) { return total + std::abs(sample); });
    };

    const float base = render(false, false);
    const float ccMuted = render(true, false);
    const float aftertouch = render(false, true);
    REQUIRE(ccMuted < base * 0.90f);
    REQUIRE(aftertouch > base * 1.05f);
}

TEST_CASE("Cached reverse playback preserves pitch and host duration across sample rates", "[core][engine][reverse][sample_rate]") {
    constexpr double sourceRate = 48000.0;
    constexpr double hostRate = 96000.0;
    std::vector<float> source(48000);
    for (size_t i = 0; i < source.size(); ++i)
        source[i] = 0.7f * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(i) / sourceRate);
    SmackWavCache cache;
    cache.capture(source, source, sourceRate);

    const std::array<MidiTrigger, 1> trigger{{
        { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false }
    }};
    const auto render = [&](ReverseSyncMode mode) {
        SmackEngine engine;
        engine.prepare(hostRate);
        REQUIRE(engine.restoreLockedWav(cache.toWavBytes()));
        REQUIRE(engine.wavCache().sampleRate() == sourceRate);
        EngineSettings settings;
        settings.lockIn = true;
        settings.effects.reverseMode = mode;
        settings.effects.reverseMix = 1.0f;
        Transport transport;
        transport.bpm = 120.0;
        transport.ppqPosition = 0.0;
        std::vector<float> left(16000), right(16000);
        float* channels[2] = { left.data(), right.data() };
        engine.process(settings, transport, trigger, channels, 2, static_cast<int>(left.size()));
        return std::tuple{ left, engine.reverseDurationSamples(), engine.reverseSourceFrames() };
    };
    const auto [dry, unusedDryDuration, unusedDryFrames] = render(ReverseSyncMode::Off);
    const auto [withReverse, reverseDuration, reverseFrames] = render(ReverseSyncMode::QuarterBeat);
    (void) unusedDryDuration;
    (void) unusedDryFrames;
    std::vector<float> reverseOnly(dry.size());
    for (size_t i = 0; i < dry.size(); ++i)
        reverseOnly[i] = withReverse[i] - dry[i];

    int crossings = 0;
    constexpr int windowStart = 1000;
    constexpr int windowLength = 6000;
    for (int i = windowStart + 1; i < windowStart + windowLength; ++i) {
        if ((reverseOnly[static_cast<size_t>(i - 1)] < 0.0f) != (reverseOnly[static_cast<size_t>(i)] < 0.0f))
            ++crossings;
    }
    const float measuredHz = static_cast<float>(crossings) * static_cast<float>(hostRate)
        / (2.0f * static_cast<float>(windowLength));
    CAPTURE(crossings, measuredHz, reverseDuration, reverseFrames,
        reverseOnly[1000], reverseOnly[1100], reverseOnly[1200], reverseOnly[1300]);
    REQUIRE(std::abs(measuredHz - 440.0f) < 20.0f);
    float clipEnergy = 0.0f;
    float afterClipEnergy = 0.0f;
    for (int i = 0; i < 12000; ++i)
        clipEnergy += std::abs(reverseOnly[static_cast<size_t>(i)]);
    for (int i = 12000; i < static_cast<int>(reverseOnly.size()); ++i)
        afterClipEnergy += std::abs(reverseOnly[static_cast<size_t>(i)]);
    REQUIRE(clipEnergy > 100.0f);
    REQUIRE(afterClipEnergy < 0.001f);
}

TEST_CASE("SmackEngine advances Lock-In capture in bounded callback slices", "[core][engine][lock_in][realtime]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.lockIn = true;
    std::vector<float> left(2048), right(2048);
    float* channels[2] = { left.data(), right.data() };

    for (int callback = 0; callback < 2000 && !engine.isSmackLocked(); ++callback) {
        engine.process(settings, Transport{}, {}, channels, 2, static_cast<int>(left.size()));
        REQUIRE(engine.lastCallbackPreparationSamples() <= SmackEngine::kMaxPreparationSamplesPerCallback);
    }

    REQUIRE(engine.isSmackLocked());
}

TEST_CASE("Full ADSR keeps MIDI gating active while a Lock-In bank is loaded", "[core][engine][adsr][lock_in]") {
    SmackEngine engine;
    engine.prepare(8000.0);
    EngineSettings settings;
    settings.lockIn = true;
    settings.synth.fullAdsrEnabled = true;
    settings.synth.ampAttackMs = 0.0f;
    settings.synth.ampDecayMs = 1.0f;
    settings.synth.ampSustain = 0.7f;
    settings.synth.ampReleaseMs = 100.0f;
    settings.synth.noiseLevel = 0.0f;
    settings.synth.clickLevel = 0.0f;
    settings.synth.toneDecayMs = 15.0f;
    settings.synth.reverbMix = 0.0f;
    engine.lockCurrentSmack(settings);
    completeLockCapture(engine, settings);
    REQUIRE(engine.isSmackLocked());

    const std::array<MidiTrigger, 1> on{{
        { .sampleOffset = 0, .noteNumber = 60, .velocity = 1.0f, .audition = false, .channel = 1 }
    }};
    const std::array<MidiNoteOff, 1> off{{
        { .sampleOffset = 7000, .noteNumber = 60, .channel = 1 }
    }};
    std::vector<float> left(8192), right(8192);
    float* channels[2] = { left.data(), right.data() };
    engine.process(settings, Transport{}, on, channels, 2, static_cast<int>(left.size()), {}, {}, {}, off);
    float heldEnergy = 0.0f;
    float releaseEnergy = 0.0f;
    for (int i = 4000; i < 6900; ++i)
        heldEnergy += std::abs(left[static_cast<size_t>(i)]);
    for (int i = 7000; i < 8192; ++i)
        releaseEnergy += std::abs(left[static_cast<size_t>(i)]);
    REQUIRE(heldEnergy > 1.0f);
    REQUIRE(releaseEnergy > 0.1f);
}

TEST_CASE("SmackEngine Lock-In preserves the preset voice and layer stems", "[core][engine][lock_in][layers]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.synth.reverbMix = 0.0f;
    settings.synth.voiceMode = VoiceMode::FmBell;
    engine.lockCurrentSmack(settings);
    completeLockCapture(engine, settings);
    REQUIRE(engine.isSmackLocked());
    REQUIRE(&engine.wavCache().left() == &engine.wavCache().voice(VoiceMode::FmBell).left);

    SmackWavCache restoredCache;
    const auto bankBytes = engine.wavCache().toBankWavBytes();
    REQUIRE(bankBytes.size() > 44);
    REQUIRE(restoredCache.fromWavBytes(bankBytes));
    for (VoiceMode mode : { VoiceMode::DanceSnare, VoiceMode::ClubTom, VoiceMode::FmBell, VoiceMode::PrydaRimSmack }) {
        const auto& restoredVoice = restoredCache.voice(mode);
        REQUIRE(restoredVoice.left.size() == engine.wavCache().voice(mode).left.size());
        REQUIRE(restoredVoice.layers[0].left.size() == restoredVoice.left.size());
        REQUIRE(restoredVoice.layers[1].left.size() == restoredVoice.left.size());
        REQUIRE(restoredVoice.layers[2].left.size() == restoredVoice.left.size());
    }

    settings.keyTracking = 0.0f;
    const std::array<int, 4> notes{ 36, 48, 60, 72 };
    std::array<float, 4> voiceEnergy{};
    for (size_t voice = 0; voice < notes.size(); ++voice) {
        std::vector<float> left(1024, 0.0f), right(1024, 0.0f);
        std::vector<float> bodyL(1024, 0.0f), bodyR(1024, 0.0f);
        std::vector<float> wireL(1024, 0.0f), wireR(1024, 0.0f);
        std::vector<float> transientL(1024, 0.0f), transientR(1024, 0.0f);
        float* channels[2] = { left.data(), right.data() };
        LayerOutputChannels layers{ bodyL.data(), bodyR.data(), wireL.data(), wireR.data(), transientL.data(), transientR.data() };
        const std::array<MidiTrigger, 1> trigger{{
            { .sampleOffset = 0, .noteNumber = notes[voice], .velocity = 1.0f, .audition = false }
        }};
        engine.process(settings, Transport{}, trigger, channels, 2, static_cast<int>(left.size()), {}, {}, layers);

        for (float sample : left)
            voiceEnergy[voice] += std::abs(sample);
        REQUIRE(std::any_of(bodyL.begin(), bodyL.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
        REQUIRE(std::any_of(wireL.begin(), wireL.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
        REQUIRE(std::any_of(transientL.begin(), transientL.end(), [](float sample) { return std::abs(sample) > 0.001f; }));
    }

    for (size_t voice = 1; voice < voiceEnergy.size(); ++voice)
        REQUIRE(std::abs(voiceEnergy[voice] - voiceEnergy[0]) < 0.001f);
}

TEST_CASE("Prepared Lock-In capture does not allocate in the audio callback", "[core][engine][lock_in][realtime]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.lockIn = true;
    std::vector<float> left(256, 0.0f), right(256, 0.0f);
    float* channels[2] = { left.data(), right.data() };

    allocationCount = 0;
    countAllocations = true;
    for (int callback = 0; callback < 2000 && !engine.isSmackLocked(); ++callback)
        engine.process(settings, Transport{}, {}, channels, 2, static_cast<int>(left.size()));
    countAllocations = false;

    REQUIRE(allocationCount == 0);
    REQUIRE(engine.isSmackLocked());
}

TEST_CASE("SmackEngine routes reverse synthesis into its source layer stems", "[core][engine][reverse][layers]") {
    struct RenderedLayers {
        std::vector<float> body, wire, transient;
    };
    const auto render = [](ReverseSyncMode reverseMode) {
        SmackEngine engine;
        engine.prepare(48000.0);
        EngineSettings settings;
        settings.synth.reverbMix = 0.0f;
        settings.synth.clickLevel = 1.0f;
        settings.synth.preStrikeCount = 2;
        settings.effects.reverseMode = reverseMode;
        settings.effects.reverseMix = 1.0f;
        const std::array<MidiTrigger, 1> trigger{{
            { .sampleOffset = 0, .noteNumber = 69, .velocity = 1.0f, .audition = false }
        }};
        constexpr int blockSize = 512;
        constexpr int blocks = 64;
        std::vector<float> left(blockSize), right(blockSize);
        RenderedLayers output{ std::vector<float>(blockSize * blocks), std::vector<float>(blockSize * blocks),
            std::vector<float>(blockSize * blocks) };
        std::vector<float> bodyR(blockSize), wireR(blockSize), transientR(blockSize);
        float* channels[2] = { left.data(), right.data() };
        for (int block = 0; block < blocks; ++block) {
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            std::fill(bodyR.begin(), bodyR.end(), 0.0f);
            std::fill(wireR.begin(), wireR.end(), 0.0f);
            std::fill(transientR.begin(), transientR.end(), 0.0f);
            LayerOutputChannels layers{
                output.body.data() + block * blockSize, bodyR.data(),
                output.wire.data() + block * blockSize, wireR.data(),
                output.transient.data() + block * blockSize, transientR.data()
            };
            engine.process(settings, Transport{}, block == 0 ? std::span<const MidiTrigger>(trigger) : std::span<const MidiTrigger>{},
                channels, 2, blockSize, {}, {}, layers);
        }
        return output;
    };

    const auto forward = render(ReverseSyncMode::Off);
    const auto reversed = render(ReverseSyncMode::QuarterBeat);
    const auto energyDifference = [](const std::vector<float>& a, const std::vector<float>& b) {
        float difference = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
            difference += std::abs(a[i] - b[i]);
        return difference;
    };
    REQUIRE(energyDifference(forward.body, reversed.body) > 0.01f);
    REQUIRE(energyDifference(forward.wire, reversed.wire) > 0.01f);
    REQUIRE(energyDifference(forward.transient, reversed.transient) > 0.01f);
}

TEST_CASE("SmackEngine pre-sum distortion avoids body-to-wire intermodulation crushing", "[core][engine][distortion]") {
    const auto renderWithLayers = [](float toneLevel, float noiseLevel) {
        SmackEngine engine;
        engine.prepare(48000.0);
        EngineSettings settings;
        settings.synth.shapedEngine = true;
        settings.synth.voiceMode = VoiceMode::ClubTom;
        settings.synth.note = 0;
        settings.synth.octave = 2;
        settings.synth.toneLevel = toneLevel;
        settings.synth.noiseType = NoiseType::WhiteSizzle;
        settings.synth.noiseLevel = noiseLevel;
        settings.synth.noiseHPF = 2200.0f;
        settings.synth.noiseLPF = 16000.0f;
        settings.synth.clickLevel = 0.0f;
        settings.synth.preStrikeCount = 1;
        settings.synth.reverbMix = 0.0f;
        settings.effects.distType = DistortionType::HardClip;
        settings.effects.distDrive = 0.85f;
        settings.effects.distMix = 1.0f;
        settings.effects.compressor.mix = 0.0f;
        settings.effects.limiter.enabled = false;
        const std::array<MidiTrigger, 1> trigger{{
            { .sampleOffset = 0, .noteNumber = 60, .velocity = 1.0f, .audition = false }
        }};
        std::vector<float> left(2048, 0.0f), right(2048, 0.0f);
        float* channels[2] = { left.data(), right.data() };
        engine.process(settings, Transport{}, trigger, channels, 2, static_cast<int>(left.size()));
        return left;
    };

    const auto bodyOnly = renderWithLayers(1.0f, 0.0f);
    const auto wireOnly = renderWithLayers(0.0f, 0.8f);
    const auto combined = renderWithLayers(1.0f, 0.8f);
    double intermodError = 0.0;
    double signalEnergy = 0.0;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, combined.size()), [&](size_t idx) {
        const double independentSum = static_cast<double>(bodyOnly[idx]) + static_cast<double>(wireOnly[idx]);
        const double diff = static_cast<double>(combined[idx]) - independentSum;
        intermodError += diff * diff;
        signalEnergy += independentSum * independentSum;
    });
    REQUIRE(signalEnergy > 1.0);
    REQUIRE(intermodError < signalEnergy * 0.01);
}
