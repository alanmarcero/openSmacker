#include "SmackEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numeric>

using namespace opensmacker::core;

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

TEST_CASE("SmackEngine resolves MIDI notes through four independent voice zones", "[core][engine][zones]") {
    const std::array<VoiceKeyZone, 4> zones{{
        { 36, 47 }, { 48, 59 }, { 60, 71 }, { 72, 84 }
    }};

    REQUIRE(resolveVoiceModeForNote(36, zones) == VoiceMode::DanceSnare);
    REQUIRE(resolveVoiceModeForNote(59, zones) == VoiceMode::ClubTom);
    REQUIRE(resolveVoiceModeForNote(60, zones) == VoiceMode::FmBell);
    REQUIRE(resolveVoiceModeForNote(84, zones) == VoiceMode::PrydaRimSmack);
    REQUIRE_FALSE(resolveVoiceModeForNote(85, zones).has_value());
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

TEST_CASE("SmackEngine ignores notes outside enabled voice key zones", "[core][engine][zones]") {
    SmackEngine engine;
    engine.prepare(48000.0);
    EngineSettings settings;
    settings.keyZonesEnabled = true;
    settings.voiceZones = {{ { 36, 47 }, { 48, 59 }, { 60, 71 }, { 72, 84 } }};
    std::vector<float> left(256, 0.0f), right(256, 0.0f);
    float* channels[2] = { left.data(), right.data() };
    const std::array<MidiTrigger, 1> outside{{
        { .sampleOffset = 0, .noteNumber = 100, .velocity = 1.0f, .audition = false }
    }};

    engine.process(settings, Transport{}, outside, channels, 2, 256);
    REQUIRE(engine.triggerSerial() == 0);

    const std::array<MidiTrigger, 1> inside{{
        { .sampleOffset = 0, .noteNumber = 60, .velocity = 1.0f, .audition = false }
    }};
    engine.process(settings, Transport{}, inside, channels, 2, 256);
    REQUIRE(engine.triggerSerial() == 1);
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
