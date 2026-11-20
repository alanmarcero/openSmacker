#include "SmackSynth.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

using namespace opensmacker::core;

namespace {

double bandPowerGoertzel(const std::vector<float>& signal, double sampleRate, double lowHz, double highHz, int stepBins = 1) {
    if (signal.empty())
        return 0.0;
    const int count = static_cast<int>(signal.size());
    const int minBin = std::max(1, static_cast<int>(std::ceil(lowHz * count / sampleRate)));
    const int maxBin = std::min(count / 2 - 1, static_cast<int>(std::floor(highHz * count / sampleRate)));
    if (maxBin < minBin)
        return 0.0;
    double totalPower = 0.0;
    int bins = 0;
    const int stride = std::max(1, stepBins);
    const int numSteps = (maxBin - minBin) / stride + 1;
    std::ranges::for_each(std::views::iota(0, numSteps), [&](int idx) {
        const int bin = minBin + idx * stride;
        const double omega = 2.0 * std::numbers::pi * static_cast<double>(bin) / static_cast<double>(count);
        const double coeff = 2.0 * std::cos(omega);
        double s1 = 0.0;
        double s2 = 0.0;
        std::ranges::for_each(std::views::iota(0, count), [&](int n) {
            const double window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) / static_cast<double>(count - 1));
            const double s0 = static_cast<double>(signal[static_cast<size_t>(n)]) * window + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        });
        const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        totalPower += std::max(0.0, power);
        ++bins;
    });
    if (bins == 0)
        return 0.0;
    return totalPower / (static_cast<double>(bins) * static_cast<double>(count) * static_cast<double>(count));
}

} // namespace

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

TEST_CASE("SmackSynth body and transient filters affect only their own layers", "[core][synth][filters]") {
    const auto renderEnergy = [](bool transient, float highpass, float lowpass) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.voiceMode = VoiceMode::ClubTom;
        settings.toneFreqHz = 180.0f;
        settings.toneLevel = transient ? 0.0f : 1.0f;
        settings.noiseLevel = 0.0f;
        settings.clickLevel = transient ? 1.0f : 0.0f;
        settings.clickFreqHz = 8500.0f;
        settings.reverbMix = 0.0f;
        if (transient) {
            settings.transientHPF = highpass;
            settings.transientLPF = lowpass;
        } else {
            settings.bodyHPF = highpass;
            settings.bodyLPF = lowpass;
        }
        std::vector<float> left, right;
        std::array<StereoLayerBuffer, 3> layers;
        synth.renderBlock(left, right, settings, 1.0f, 0, &layers);
        const auto& layer = layers[transient ? 2U : 0U];
        float energy = 0.0f;
        for (size_t i = 0; i < layer.left.size(); ++i)
            energy += layer.left[i] * layer.left[i] + layer.right[i] * layer.right[i];
        return energy;
    };

    const float bodyOpen = renderEnergy(false, 20.0f, 20000.0f);
    CHECK(renderEnergy(false, 1800.0f, 20000.0f) < bodyOpen * 0.65f);
    CHECK(renderEnergy(false, 20.0f, 120.0f) < bodyOpen * 0.65f);

    const float transientOpen = renderEnergy(true, 20.0f, 20000.0f);
    CHECK(renderEnergy(true, 7500.0f, 20000.0f) < transientOpen * 0.75f);
    CHECK(renderEnergy(true, 20.0f, 1400.0f) < transientOpen * 0.75f);
}

TEST_CASE("SmackSynth body, wire, and transient pan controls place each layer", "[core][synth][pan]") {
    const auto renderPan = [](int layerIndex, float pan) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.voiceMode = VoiceMode::ClubTom;
        settings.toneLevel = layerIndex == 0 ? 1.0f : 0.0f;
        settings.noiseLevel = layerIndex == 1 ? 1.0f : 0.0f;
        settings.clickLevel = layerIndex == 2 ? 1.0f : 0.0f;
        settings.reverbMix = 0.0f;
        settings.bodyPan = layerIndex == 0 ? pan : 0.0f;
        settings.wirePan = layerIndex == 1 ? pan : 0.0f;
        settings.transientPan = layerIndex == 2 ? pan : 0.0f;
        std::vector<float> left, right;
        std::array<StereoLayerBuffer, 3> layers;
        synth.renderBlock(left, right, settings, 1.0f, 0, &layers);
        float leftEnergy = 0.0f, rightEnergy = 0.0f;
        for (size_t i = 0; i < layers[static_cast<size_t>(layerIndex)].left.size(); ++i) {
            const auto& layer = layers[static_cast<size_t>(layerIndex)];
            leftEnergy += layer.left[i] * layer.left[i];
            rightEnergy += layer.right[i] * layer.right[i];
        }
        return std::pair{leftEnergy, rightEnergy};
    };

    for (int layer = 0; layer < 3; ++layer) {
        const auto left = renderPan(layer, -1.0f);
        const auto right = renderPan(layer, 1.0f);
        CHECK(left.first > left.second * 100.0f);
        CHECK(right.second > right.first * 100.0f);
    }
}

TEST_CASE("SmackSynth wire tension changes snare-wire texture", "[core][synth][wires]") {
    const auto render = [](float tension) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.toneLevel = 0.0f;
        settings.clickLevel = 0.0f;
        settings.noiseType = NoiseType::SnareWires;
        settings.noiseLevel = 1.0f;
        settings.noiseDecayMs = 120.0f;
        settings.noiseHPF = 20.0f;
        settings.noiseLPF = 20000.0f;
        settings.reverbMix = 0.0f;
        settings.wireTension = tension;
        std::vector<float> left, right;
        std::array<StereoLayerBuffer, 3> layers;
        synth.renderBlock(left, right, settings, 1.0f, 0, &layers);
        return layers[1].left;
    };

    const auto loose = render(0.0f);
    const auto tight = render(1.0f);
    float difference = 0.0f;
    for (size_t i = 0; i < std::min(loose.size(), tight.size()); ++i)
        difference += std::abs(loose[i] - tight[i]);
    REQUIRE(difference > 1.0f);
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

TEST_CASE("SmackSynth exposes body wire and transient output layers", "[core][synth][layers]") {
    SmackSynth synth;
    synth.prepare(48000.0);

    SmackSynthSettings settings;
    settings.reverbMix = 0.0f;
    synth.trigger(settings, 1.0f, 0);

    float bodyEnergy = 0.0f;
    float wireEnergy = 0.0f;
    float transientEnergy = 0.0f;
    for (int sample = 0; sample < 512; ++sample) {
        const auto frame = synth.processSample(settings);
        bodyEnergy += std::abs(frame.bodyLeft) + std::abs(frame.bodyRight);
        wireEnergy += std::abs(frame.wireLeft) + std::abs(frame.wireRight);
        transientEnergy += std::abs(frame.transientLeft) + std::abs(frame.transientRight);
    }

    REQUIRE(bodyEnergy > 0.1f);
    REQUIRE(wireEnergy > 0.1f);
    REQUIRE(transientEnergy > 0.1f);
}

TEST_CASE("SmackSynth applies velocity response independently to each layer", "[core][synth][velocity]") {
    const auto renderPeak = [](SmackSynthSettings settings, float velocity) {
        SmackSynth synth;
        synth.prepare(48000.0);
        settings.reverbMix = 0.0f;
        synth.trigger(settings, velocity, 0);
        float peak = 0.0f;
        for (int sample = 0; sample < 512; ++sample) {
            const auto frame = synth.processSample(settings);
            peak = std::max(peak, std::max({ std::abs(frame.bodyLeft), std::abs(frame.wireLeft), std::abs(frame.transientLeft) }));
        }
        return peak;
    };

    SmackSynthSettings body;
    body.noiseLevel = 0.0f;
    body.clickLevel = 0.0f;
    body.preStrikeCount = 1;
    body.bodyVelocityDepth = 1.0f;
    body.wireVelocityDepth = 0.0f;
    body.transientVelocityDepth = 0.0f;
    const float bodyAtFullVelocity = renderPeak(body, 1.0f);
    const float bodyAtHalfVelocity = renderPeak(body, 0.5f);
    REQUIRE(bodyAtHalfVelocity < bodyAtFullVelocity * 0.65f);
    REQUIRE(bodyAtHalfVelocity > bodyAtFullVelocity * 0.35f);

    SmackSynthSettings wire = body;
    wire.toneLevel = 0.0f;
    wire.noiseLevel = 1.0f;
    wire.wireVelocityDepth = 1.0f;
    const float wireAtFullVelocity = renderPeak(wire, 1.0f);
    const float wireAtHalfVelocity = renderPeak(wire, 0.5f);
    REQUIRE(wireAtHalfVelocity < wireAtFullVelocity * 0.65f);
    REQUIRE(wireAtHalfVelocity > wireAtFullVelocity * 0.35f);

    SmackSynthSettings transient = body;
    transient.toneLevel = 0.0f;
    transient.noiseLevel = 0.0f;
    transient.clickLevel = 1.0f;
    transient.transientVelocityDepth = 1.0f;
    const float transientAtFullVelocity = renderPeak(transient, 1.0f);
    const float transientAtHalfVelocity = renderPeak(transient, 0.5f);
    REQUIRE(transientAtHalfVelocity < transientAtFullVelocity * 0.65f);
    REQUIRE(transientAtHalfVelocity > transientAtFullVelocity * 0.35f);
}

TEST_CASE("SmackSynth aftertouch increases only the selected layer response", "[core][synth][aftertouch]") {
    SmackSynth synth;
    synth.prepare(48000.0);

    SmackSynthSettings settings;
    settings.noiseLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.preStrikeCount = 1;
    settings.reverbMix = 0.0f;
    settings.bodyAftertouchDepth = 1.0f;
    settings.wireAftertouchDepth = 0.0f;
    settings.transientAftertouchDepth = 0.0f;
    synth.trigger(settings, 1.0f, 0);

    settings.aftertouchValue = 1.0f;
    float touchedBodyPeak = 0.0f;
    for (int sample = 0; sample < 512; ++sample)
        touchedBodyPeak = std::max(touchedBodyPeak, std::abs(synth.processSample(settings).bodyLeft));

    SmackSynth untouched;
    untouched.prepare(48000.0);
    settings.aftertouchValue = 0.0f;
    untouched.trigger(settings, 1.0f, 0);
    float bodyPeak = 0.0f;
    for (int sample = 0; sample < 512; ++sample)
        bodyPeak = std::max(bodyPeak, std::abs(untouched.processSample(settings).bodyLeft));

    REQUIRE(touchedBodyPeak > bodyPeak * 1.5f);
}

TEST_CASE("SmackSynth Full ADSR sustains until note-off and supports a 20-second release", "[core][synth][adsr]") {
    SmackSynth synth;
    synth.prepare(8000.0);
    SmackSynthSettings settings;
    settings.fullAdsrEnabled = true;
    settings.ampAttackMs = 0.0f;
    settings.ampDecayMs = 1.0f;
    settings.ampSustain = 0.65f;
    settings.ampReleaseMs = 20000.0f;
    settings.toneDecayMs = 15.0f;
    settings.noiseLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.preStrikeCount = 1;
    settings.reverbMix = 0.0f;
    synth.trigger(settings, 1.0f, 0, 60, 2);

    float lateSustain = 0.0f;
    for (int i = 0; i < 8000; ++i) {
        const auto frame = synth.processSample(settings);
        if (i >= 7600)
            lateSustain += std::abs(frame.bodyLeft);
    }
    REQUIRE(lateSustain > 0.1f);
    REQUIRE(synth.isPlaying());

    synth.noteOff(60, 2, settings);
    float firstRelease = 0.0f;
    for (int i = 0; i < 8000; ++i) {
        const auto frame = synth.processSample(settings);
        firstRelease += std::abs(frame.bodyLeft);
    }
    REQUIRE(firstRelease > 0.1f);
    REQUIRE(synth.isPlaying());
    for (int i = 0; i < 160000; ++i)
        (void) synth.processSample(settings);
    REQUIRE_FALSE(synth.isPlaying());
}

TEST_CASE("SmackSynth one-shot mode ignores MIDI note-off and retains identical output", "[core][synth][adsr][compatibility]") {
    SmackSynth baseline;
    SmackSynth withNoteOff;
    baseline.prepare(48000.0);
    withNoteOff.prepare(48000.0);
    SmackSynthSettings settings;
    settings.fullAdsrEnabled = false;
    baseline.trigger(settings, 1.0f, 0, 69, 1);
    withNoteOff.trigger(settings, 1.0f, 0, 69, 1);
    for (int i = 0; i < 256; ++i) {
        if (i == 64)
            withNoteOff.noteOff(69, 1, settings);
        const auto expected = baseline.processSample(settings);
        const auto actual = withNoteOff.processSample(settings);
        REQUIRE(actual.left == expected.left);
        REQUIRE(actual.right == expected.right);
    }
}

TEST_CASE("SmackSynth tracks an octave shift in its tuned metallic noise", "[core][synth][pitch_tracking]") {
    const auto render = [](int semitoneOffset) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.fullAdsrEnabled = true;
        settings.voiceMode = VoiceMode::ClubTom;
        settings.noiseType = NoiseType::MetallicRing;
        settings.toneLevel = 0.0f;
        settings.noiseLevel = 1.0f;
        settings.clickLevel = 0.0f;
        settings.noiseDecayMs = 1200.0f;
        settings.ampSustain = 1.0f;
        settings.reverbMix = 0.0f;
        synth.trigger(settings, 1.0f, semitoneOffset, 69, 1);
        std::vector<float> samples(24000);
        for (auto& sample : samples)
            sample = synth.processSample(settings).wireLeft;
        return samples;
    };
    const auto low = render(0);
    const auto high = render(12);
    float difference = 0.0f;
    for (size_t i = 0; i < low.size(); ++i)
        difference += std::abs(low[i] - high[i]);
    REQUIRE(difference > 100.0f);
}

TEST_CASE("Velocity shapes the struck overtones and a held body settles to its root", "[core][synth][modes]") {
    for (const auto rate : { 44100, 96000 }) {
        for (const auto octave : { 3, 5 }) {
            for (const bool shaped : { false, true }) {
            for (auto mode : { VoiceMode::DanceSnare, VoiceMode::ClubTom, VoiceMode::FmBell, VoiceMode::PrydaRimSmack }) {
                SmackSynthSettings settings;
                settings.shapedEngine = shaped;
                settings.voiceMode = mode;
                settings.octave = octave;
                settings.fullAdsrEnabled = true;
                settings.tonePitchEnv = 0;
                settings.fmAmount = 0.8f;
                settings.toneDecayMs = 75.0f;
                settings.noiseLevel = settings.clickLevel = settings.reverbMix = 0;
                SmackSynth soft, hard;
                soft.prepare(rate);
                hard.prepare(rate);
                soft.trigger(settings, 0.4f, 0, 60, 1);
                hard.trigger(settings, 1.0f, 0, 60, 1);
                double attackDiff = 0, heldDiff = 0, heldEnergy = 0;
                for (int i = 0; i < rate / 3; ++i) {
                    const auto quiet = soft.processSample(settings).bodyLeft / 0.4f;
                    const auto loud = hard.processSample(settings).bodyLeft;
                    const double diff = std::abs(quiet - loud);
                    if (i < rate / 40) attackDiff += diff;
                    if (i > rate / 4) {
                        heldDiff += diff;
                        heldEnergy += std::abs(loud);
                    }
                }
                CAPTURE(rate, octave, static_cast<int>(mode), shaped);
                CHECK(attackDiff / (rate / 40) > 0.01);
                CHECK(heldEnergy / (rate / 12) > 0.03);
                CHECK(heldDiff < heldEnergy * 0.01);
                hard.noteOff(60, 1, settings);
                for (int i = 0; i < rate / 4; ++i) (void) hard.processSample(settings);
                CHECK_FALSE(hard.isPlaying());
            }
            }
        }
    }
}

TEST_CASE("Sub thump is an octave sine without a DC offset", "[core][synth][sub]") {
    SmackSynth synth;
    synth.prepare(48000.0);
    SmackSynthSettings settings;
    settings.voiceMode = VoiceMode::ClubTom;
    settings.subThump = 1.0f;
    settings.noiseLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.reverbMix = 0.0f;
    settings.bodyHPF = 20.0f;
    settings.toneDecayMs = 180.0f;
    settings.preStrikeCount = 1;
    settings.shapedEngine = true;
    std::vector<float> left, right;
    synth.renderBlock(left, right, settings, 1.0f, 0);
    double mean = 0.0;
    int negative = 0;
    for (float sample : left) {
        mean += sample;
        if (sample < -0.02f) ++negative;
    }
    mean /= static_cast<double>(left.size());
    REQUIRE(std::abs(mean) < 0.02);
    REQUIRE(negative > 20);
}

TEST_CASE("Shaped snares do not sustain a loud body sine under the wires", "[core][synth][shaped]") {
    const auto lateBody = [](bool shaped) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.shapedEngine = shaped;
        settings.voiceMode = VoiceMode::DanceSnare;
        settings.noiseLevel = 0.0f;
        settings.clickLevel = 0.0f;
        settings.reverbMix = 0.0f;
        settings.toneLevel = 1.0f;
        settings.toneDecayMs = 180.0f;
        settings.fmAmount = 0.2f;
        settings.preStrikeCount = 1;
        synth.trigger(settings, 1.0f, 0);
        double energy = 0.0;
        for (int i = 0; i < 9600; ++i) {
            const auto frame = synth.processSample(settings);
            if (i > 2880)
                energy += std::abs(frame.bodyLeft);
        }
        return energy;
    };
    REQUIRE(lateBody(true) < lateBody(false) * 0.35f);
}

TEST_CASE("Oversampled FM bell preserves high-octave FM depth without sub-fundamental aliasing", "[core][synth][oversampling]") {
    const auto renderBell = [](float fmAmount, bool shaped) {
        SmackSynth synth;
        synth.prepare(44100.0);
        SmackSynthSettings settings;
        settings.shapedEngine = shaped;
        settings.fullAdsrEnabled = !shaped;
        settings.voiceMode = VoiceMode::FmBell;
        settings.note = 9;
        settings.octave = 7;
        settings.tonePitchEnv = 0.0f;
        settings.fmAmount = fmAmount;
        settings.toneDecayMs = 240.0f;
        settings.noiseLevel = 0.0f;
        settings.clickLevel = 0.0f;
        settings.reverbMix = 0.0f;
        settings.preStrikeCount = 1;
        std::vector<float> out(2048, 0.0f);
        synth.trigger(settings, 1.0f, 0, 105, 1);
        std::ranges::for_each(out, [&](float& sample) { sample = synth.processSample(settings).bodyLeft; });
        return out;
    };

    const auto shapedHighFm = renderBell(1.0f, true);
    const double carrierPower = bandPowerGoertzel(shapedHighFm, 44100.0, 3300.0, 3750.0);
    const double upperSidebandPower = bandPowerGoertzel(shapedHighFm, 44100.0, 13800.0, 15500.0, 2);
    REQUIRE(upperSidebandPower > carrierPower * 0.05);

    const auto sustainedHighFm = renderBell(1.0f, false);
    const double sustainedCarrier = bandPowerGoertzel(sustainedHighFm, 44100.0, 3300.0, 3750.0);
    const double foldbackLowBand = bandPowerGoertzel(sustainedHighFm, 44100.0, 120.0, 1600.0, 2);
    REQUIRE(foldbackLowBand < sustainedCarrier * 1.0e-3);
}

TEST_CASE("Physical snare-wire comb network couples nonlinearly to bottom-head modal amplitude", "[core][synth][wires][waveguide]") {
    const auto renderWires = [](float toneLevel) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.shapedEngine = true;
        settings.voiceMode = VoiceMode::DanceSnare;
        settings.toneLevel = toneLevel;
        settings.noiseType = NoiseType::SnareWires;
        settings.noiseLevel = 0.9f;
        settings.wireTension = 0.7f;
        settings.clickLevel = 0.0f;
        settings.reverbMix = 0.0f;
        settings.preStrikeCount = 1;
        synth.trigger(settings, 1.0f, 0);
        std::vector<float> wires(2048, 0.0f);
        std::ranges::for_each(wires, [&](float& sample) { sample = synth.processSample(settings).wireLeft; });
        return wires;
    };

    const auto uncoupled = renderWires(0.0f);
    const auto coupled = renderWires(1.0f);
    double diff = 0.0;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, coupled.size()), [&](size_t idx) {
        diff += std::abs(static_cast<double>(coupled[idx]) - static_cast<double>(uncoupled[idx]));
    });
    REQUIRE(diff > 1.0);
}

TEST_CASE("Struck 2-pole shell modal bank follows circular-membrane stiffness dispersion", "[core][synth][modes][dispersion]") {
    const auto renderModal = [](float spread) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.shapedEngine = true;
        settings.voiceMode = VoiceMode::ClubTom;
        settings.note = 9;
        settings.octave = 2;
        settings.tonePitchEnv = 0.0f;
        settings.fmAmount = 0.0f;
        settings.shellSpread = spread;
        settings.toneDecayMs = 350.0f;
        settings.toneQ = 0.6f;
        settings.noiseLevel = 0.0f;
        settings.clickLevel = 0.0f;
        settings.reverbMix = 0.0f;
        settings.preStrikeCount = 1;
        synth.trigger(settings, 1.0f, 0);
        std::vector<float> body(4096, 0.0f);
        std::ranges::for_each(body, [&](float& sample) { sample = synth.processSample(settings).bodyLeft; });
        return body;
    };

    const auto neutral = renderModal(0.0f);
    const auto stiff = renderModal(0.5f);
    const double neutralUpper = bandPowerGoertzel(neutral, 48000.0, 355.0, 440.0);
    const double stiffUpper = bandPowerGoertzel(stiff, 48000.0, 355.0, 440.0);
    REQUIRE(stiffUpper > neutralUpper * 4.0);
}

TEST_CASE("ZDF TPT SVF shell bandpass and ADAA shell saturation suppress high-drive aliasing", "[core][synth][svf][adaa]") {
    SmackSynth synth;
    synth.prepare(44100.0);
    SmackSynthSettings settings;
    settings.shapedEngine = false;
    settings.fullAdsrEnabled = true;
    settings.voiceMode = VoiceMode::ClubTom;
    settings.note = 5;
    settings.octave = 8;
    settings.toneFreqHz = 5588.0f;
    settings.toneQ = 15.0f;
    settings.tonePitchEnv = 0.0f;
    settings.fmAmount = 0.0f;
    settings.noiseLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.reverbMix = 0.0f;
    settings.preStrikeCount = 1;
    synth.trigger(settings, 1.0f, 0, 113, 1);

    std::ranges::for_each(std::views::iota(0, 4096), [&](int) { (void) synth.processSample(settings); });
    std::vector<float> sustained(2048, 0.0f);
    std::ranges::for_each(sustained, [&](float& sample) { sample = synth.processSample(settings).bodyLeft; });

    const double fundBand = bandPowerGoertzel(sustained, 44100.0, 5480.0, 5700.0);
    const double seventhHarmonicAlias = bandPowerGoertzel(sustained, 44100.0, 4900.0, 5080.0);
    REQUIRE(fundBand > 1.0e-4);
    REQUIRE(seventhHarmonicAlias < fundBand * 2.0e-5);
}

TEST_CASE("12 dB ZDF SVF layer filters reject out-of-band snare sub-bleed steeply", "[core][synth][filters][svf]") {
    SmackSynth synth;
    synth.prepare(48000.0);
    SmackSynthSettings settings;
    settings.toneLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.reverbMix = 0.0f;
    settings.preStrikeCount = 1;
    settings.noiseType = NoiseType::WhiteSizzle;
    settings.noiseLevel = 1.0f;
    settings.noiseDecayMs = 400.0f;
    settings.noiseHPF = 1400.0f;
    settings.noiseLPF = 18000.0f;
    synth.trigger(settings, 1.0f, 0);

    std::vector<float> wires(4096, 0.0f);
    std::ranges::for_each(wires, [&](float& sample) { sample = synth.processSample(settings).wireLeft; });

    const double passband = bandPowerGoertzel(wires, 48000.0, 2800.0, 7000.0, 4);
    const double subBand = bandPowerGoertzel(wires, 48000.0, 50.0, 200.0, 1);
    REQUIRE(passband > 1.0e-6);
    REQUIRE(subBand < passband * 4.0e-4);
}

TEST_CASE("Two-stage pitch trajectory combines fast stick-head snap with slower body settle", "[core][synth][pitch_env]") {
    constexpr double sampleRate = 48000.0;
    SmackSynth synth;
    synth.prepare(sampleRate);
    SmackSynthSettings settings;
    settings.shapedEngine = false;
    settings.fullAdsrEnabled = true;
    settings.ampAttackMs = 0.05f;
    settings.ampSustain = 1.0f;
    settings.voiceMode = VoiceMode::ClubTom;
    settings.note = 9;
    settings.octave = 3;
    settings.tonePitchEnv = 24.0f;
    settings.tonePitchDecayMs = 45.0f;
    settings.fmAmount = 0.0f;
    settings.toneQ = 0.5f;
    settings.noiseLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.reverbMix = 0.0f;
    settings.preStrikeCount = 1;
    synth.trigger(settings, 1.0f, 0, 57, 1);

    std::vector<float> signal(24000, 0.0f);
    std::ranges::for_each(signal, [&](float& sample) { sample = synth.processSample(settings).bodyLeft; });

    const auto windowFreqSemitones = [&](int startSample, int lengthSamples) {
        std::vector<double> crossings;
        std::ranges::for_each(std::views::iota(startSample + 1, startSample + lengthSamples), [&](int idx) {
            const float prev = signal[static_cast<size_t>(idx - 1)];
            const float curr = signal[static_cast<size_t>(idx)];
            if (prev < 0.0f && curr >= 0.0f) {
                const double frac = static_cast<double>(-prev) / std::max(1.0e-9, static_cast<double>(curr - prev));
                crossings.push_back(static_cast<double>(idx - 1) + frac);
            }
        });
        REQUIRE(crossings.size() >= 2);
        const double avgPeriod = (crossings.back() - crossings.front()) / static_cast<double>(crossings.size() - 1);
        const double hz = sampleRate / avgPeriod;
        return 12.0 * std::log2(hz / 220.0);
    };

    const double initialSt = windowFreqSemitones(0, 180);
    const double afterSnapSt = windowFreqSemitones(336, 336);
    const double midBodySt = windowFreqSemitones(2400, 960);
    const double settledSt = windowFreqSemitones(19200, 2400);

    REQUIRE(initialSt - afterSnapSt > 6.0);
    REQUIRE(midBodySt - settledSt > 2.5);
    REQUIRE(std::abs(settledSt) < 0.5);
}

TEST_CASE("ClubTom velvet membrane coupling avoids high-frequency hash buzz while scaling with velocity", "[core][synth][tom][velvet]") {
    const auto renderTom = [](float velocity) {
        SmackSynth synth;
        synth.prepare(48000.0);
        SmackSynthSettings settings;
        settings.shapedEngine = true;
        settings.voiceMode = VoiceMode::ClubTom;
        settings.note = 9;
        settings.octave = 2;
        settings.tonePitchEnv = 0.0f;
        settings.fmAmount = 0.0f;
        settings.toneDecayMs = 200.0f;
        settings.bodyLPF = 20000.0f;
        settings.noiseLevel = 0.0f;
        settings.clickLevel = 0.0f;
        settings.reverbMix = 0.0f;
        settings.preStrikeCount = 1;
        synth.trigger(settings, velocity, 0);
        std::vector<float> body(2048, 0.0f);
        std::ranges::for_each(body, [&](float& sample) { sample = synth.processSample(settings).bodyLeft; });
        return body;
    };

    const auto hard = renderTom(1.0f);
    const auto soft = renderTom(0.25f);
    const double fundamental = bandPowerGoertzel(hard, 48000.0, 90.0, 250.0);
    const double staticUltrasonicBuzz = bandPowerGoertzel(hard, 48000.0, 9500.0, 19000.0, 6);
    REQUIRE(staticUltrasonicBuzz < fundamental * 1.0e-5);

    const double hardAirRatio = bandPowerGoertzel(hard, 48000.0, 1400.0, 4200.0, 3) / std::max(1.0e-12, fundamental);
    const double softFund = bandPowerGoertzel(soft, 48000.0, 90.0, 250.0);
    const double softAirRatio = bandPowerGoertzel(soft, 48000.0, 1400.0, 4200.0, 3) / std::max(1.0e-12, softFund);
    REQUIRE(hardAirRatio > softAirRatio * 1.4);
}

TEST_CASE("8-line Householder FDN reverb shapes LF and HF RT60 independently with dense tail", "[core][synth][reverb][fdn]") {
    const auto renderTail = [](float damp) {
        GatedStereoReverb reverb;
        reverb.prepare(48000.0);
        std::vector<float> tail(36000, 0.0f);
        std::ranges::for_each(std::views::iota(0, static_cast<int>(tail.size())), [&](int idx) {
        float exc = 0.0f;
        if (idx < 64)
            exc = std::sin(0.35f * static_cast<float>(idx)) + std::sin(0.02f * static_cast<float>(idx));
        const auto wet = reverb.process(exc, exc * 0.8f, 0.45f, 2200.0f, damp, 0.0f, 48000.0);
            tail[static_cast<size_t>(idx)] = 0.5f * (wet.left + wet.right);
        });
        return tail;
    };

    const auto bright = renderTail(0.05f);
    const auto damped = renderTail(0.92f);
    const std::vector<float> earlyBright(bright.begin() + 2400, bright.begin() + 6496);
    const std::vector<float> lateBright(bright.begin() + 26000, bright.begin() + 30096);
    const std::vector<float> lateDamped(damped.begin() + 26000, damped.begin() + 30096);

    const double brightHfRatio = bandPowerGoertzel(lateBright, 48000.0, 4200.0, 9000.0, 4)
        / std::max(1.0e-14, bandPowerGoertzel(lateBright, 48000.0, 500.0, 1600.0, 2));
    const double dampedHfRatio = bandPowerGoertzel(lateDamped, 48000.0, 4200.0, 9000.0, 4)
        / std::max(1.0e-14, bandPowerGoertzel(lateDamped, 48000.0, 500.0, 1600.0, 2));
    REQUIRE(brightHfRatio > dampedHfRatio * 8.0);

    const double earlyLfMid = bandPowerGoertzel(earlyBright, 48000.0, 90.0, 190.0, 1)
        / std::max(1.0e-14, bandPowerGoertzel(earlyBright, 48000.0, 600.0, 1400.0, 2));
    const double lateLfMid = bandPowerGoertzel(lateBright, 48000.0, 90.0, 190.0, 1)
        / std::max(1.0e-14, bandPowerGoertzel(lateBright, 48000.0, 600.0, 1400.0, 2));
    REQUIRE(lateLfMid < earlyLfMid * 0.45);
}

TEST_CASE("Oversampled rimshot transient excites three wood and hoop modes around 1.8, 4.2, and 8.7 kHz", "[core][synth][transient][rimshot]") {
    SmackSynth synth;
    synth.prepare(48000.0);
    SmackSynthSettings settings;
    settings.shapedEngine = true;
    settings.toneLevel = 0.0f;
    settings.noiseLevel = 0.0f;
    settings.reverbMix = 0.0f;
    settings.preStrikeCount = 1;
    settings.clickType = ClickType::Rimshot;
    settings.clickLevel = 1.0f;
    settings.clickFreqHz = 3800.0f;
    synth.trigger(settings, 1.0f, 0);

    std::vector<float> clickBuf(1024, 0.0f);
    std::ranges::for_each(clickBuf, [&](float& sample) { sample = synth.processSample(settings).transientLeft; });

    const double woodMode = bandPowerGoertzel(clickBuf, 48000.0, 1450.0, 2200.0);
    const double valley1 = bandPowerGoertzel(clickBuf, 48000.0, 2600.0, 3100.0);
    const double hoopMode = bandPowerGoertzel(clickBuf, 48000.0, 3700.0, 4700.0);
    const double crackMode = bandPowerGoertzel(clickBuf, 48000.0, 7800.0, 9600.0);
    REQUIRE(woodMode > valley1 * 2.0);
    REQUIRE(hoopMode > valley1 * 1.8);
    REQUIRE(crackMode > 1.0e-7);
}
