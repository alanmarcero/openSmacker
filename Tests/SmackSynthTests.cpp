#include "SmackSynth.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace opensmacker::core;

TEST_CASE("noteToHz calculation is musically accurate", "[core][synth]") {
    const float a4 = noteToHz(9, 4, 0.0f);
    REQUIRE(std::abs(a4 - 440.0f) < 0.1f);

    const float aSharp4 = noteToHz(10, 4, 0.0f);
    const float a4Plus100 = noteToHz(9, 4, 100.0f);
    REQUIRE(std::abs(aSharp4 - a4Plus100) < 0.2f);
}

TEST_CASE("SmackSynth produces valid audio block without NaNs", "[core][synth]") {
    SmackSynth synth;
    synth.prepare(48000.0);

    SmackSynthSettings settings;
    std::vector<float> left;
    std::vector<float> right;

    synth.renderBlock(left, right, settings, 1.0f, 0);

    REQUIRE(!left.empty());
    REQUIRE(left.size() == right.size());

    float maxPeak = 0.0f;
    for (size_t i = 0; i < left.size(); ++i) {
        REQUIRE_FALSE(std::isnan(left[i]));
        REQUIRE_FALSE(std::isnan(right[i]));
        REQUIRE_FALSE(std::isinf(left[i]));
        REQUIRE_FALSE(std::isinf(right[i]));
        maxPeak = std::max(maxPeak, std::max(std::abs(left[i]), std::abs(right[i])));
    }

    REQUIRE(maxPeak > 0.05f);
}

TEST_CASE("SmackSynth finishes and stops playing after duration", "[core][synth]") {
    SmackSynth synth;
    synth.prepare(48000.0);

    SmackSynthSettings settings;
    settings.noiseDecayMs = 30.0f;
    settings.toneDecayMs = 30.0f;
    settings.reverbDecayMs = 50.0f;
    settings.reverbGateMs = 40.0f;

    synth.trigger(settings, 1.0f, 0);
    REQUIRE(synth.isPlaying());

    int maxSamples = 48000;
    while (synth.isPlaying() && --maxSamples > 0) {
        [[maybe_unused]] const auto s = synth.processSample(settings);
    }

    REQUIRE_FALSE(synth.isPlaying());
    REQUIRE(synth.getEnvelopeProgress() >= 1.0f);
}

TEST_CASE("GatedStereoReverb respects gate cutoff", "[core][synth]") {
    GatedStereoReverb reverb;
    reverb.prepare(48000.0);

    [[maybe_unused]] const auto initFrame = reverb.process(1.0f, 1.0f, 0.5f, 500.0f, 0.2f, 50.0f, 48000.0);

    float maxTail = 0.0f;
    for (int i = 0; i < 2000; ++i) {
        const auto f = reverb.process(0.0f, 0.0f, 0.5f, 500.0f, 0.2f, 50.0f, 48000.0);
        maxTail = std::max(maxTail, std::max(std::abs(f.left), std::abs(f.right)));
    }
    REQUIRE(maxTail > 0.01f);

    StereoFrame gatedFrame = { 0.0f, 0.0f };
    for (int i = 0; i < 3000; ++i) {
        gatedFrame = reverb.process(0.0f, 0.0f, 0.5f, 500.0f, 0.2f, 50.0f, 48000.0);
    }

    REQUIRE(gatedFrame.left == 0.0f);
    REQUIRE(gatedFrame.right == 0.0f);
}

TEST_CASE("SmackSynth renders across all VoiceModes, ClickTypes, and NoiseTypes without errors", "[core][synth]") {
    SmackSynth synth;
    synth.prepare(48000.0);

    const VoiceMode modes[] = { VoiceMode::DanceSnare, VoiceMode::ClubTom, VoiceMode::FmBell, VoiceMode::PrydaRimSmack };
    const ClickType clicks[] = { ClickType::Stick, ClickType::Mallet, ClickType::Rimshot, ClickType::Laser };
    const NoiseType noises[] = { NoiseType::SnareWires, NoiseType::WhiteSizzle, NoiseType::VelvetAir, NoiseType::MetallicRing };

    for (auto mode : modes) {
        for (auto click : clicks) {
            for (auto noise : noises) {
                SmackSynthSettings settings;
                settings.voiceMode = mode;
                settings.clickType = click;
                settings.noiseType = noise;
                settings.toneLevel = 0.85f;
                settings.clickLevel = 0.5f;
                settings.noiseLevel = 0.6f;

                std::vector<float> left, right;
                synth.renderBlock(left, right, settings, 1.0f, 0);

                REQUIRE(!left.empty());
                REQUIRE(left.size() == right.size());

                float peak = 0.0f;
                for (size_t i = 0; i < left.size(); ++i) {
                    REQUIRE_FALSE(std::isnan(left[i]));
                    REQUIRE_FALSE(std::isnan(right[i]));
                    peak = std::max(peak, std::max(std::abs(left[i]), std::abs(right[i])));
                }
                REQUIRE(peak > 0.1f);
            }
        }
    }
}

TEST_CASE("SmackSynth tone pitch responds to note and octave settings", "[core][synth]") {
    SmackSynth synth;
    synth.prepare(48000.0);

    SmackSynthSettings lowTom;
    lowTom.voiceMode = VoiceMode::ClubTom;
    lowTom.toneLevel = 1.0f;
    lowTom.noiseLevel = 0.0f;
    lowTom.clickLevel = 0.0f;
    lowTom.reverbMix = 0.0f;
    lowTom.preStrikeCount = 1;
    lowTom.note = 0; // C
    lowTom.octave = 2;

    SmackSynthSettings highBell = lowTom;
    highBell.voiceMode = VoiceMode::FmBell;
    highBell.note = 9; // A
    highBell.octave = 6;

    std::vector<float> leftLow, rightLow;
    synth.renderBlock(leftLow, rightLow, lowTom, 1.0f, 0);

    std::vector<float> leftHigh, rightHigh;
    synth.renderBlock(leftHigh, rightHigh, highBell, 1.0f, 0);

    REQUIRE(!leftLow.empty());
    REQUIRE(!leftHigh.empty());

    float diffSum = 0.0f;
    const size_t len = std::min(leftLow.size(), leftHigh.size());
    for (size_t i = 0; i < len; ++i) {
        diffSum += std::abs(leftLow[i] - leftHigh[i]);
    }
    REQUIRE(diffSum > 1.0f);
}
