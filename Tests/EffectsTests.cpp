#include "Effects.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <ranges>
#include <vector>

using namespace opensmacker::core;

TEST_CASE("DistortionShaper processes all distortion curves cleanly", "[core][effects]") {
    DistortionShaper shaper;
    const DistortionType types[] = {
        DistortionType::Tape,
        DistortionType::Tube,
        DistortionType::HardClip,
        DistortionType::Wavefold
    };

    for (auto type : types) {
        shaper.reset();
        const float out = shaper.processSample(0.6f, type, 0.45f, 0.2f, 48000.0);
        REQUIRE_FALSE(std::isnan(out));
        REQUIRE_FALSE(std::isinf(out));
        REQUIRE(std::abs(out) > 0.01f);
        REQUIRE(std::abs(out) <= 2.0f);
    }
}

TEST_CASE("BrickwallLimiter enforces ceiling without overshoot", "[core][effects]") {
    BrickwallLimiter limiter;
    limiter.prepare(48000.0);

    LimiterSettings settings;
    settings.ceilingDb = -1.0f;
    settings.releaseMs = 20.0f;
    settings.enabled = true;

    const float ceilingLin = std::pow(10.0f, settings.ceilingDb / 20.0f);

    for (int i = 0; i < 200; ++i) {
        float l = 2.5f * std::sin(static_cast<float>(i) * 0.1f);
        float r = -2.5f * std::cos(static_cast<float>(i) * 0.1f);
        limiter.process(l, r, settings);
        REQUIRE(std::abs(l) <= ceilingLin + 1e-4f);
        REQUIRE(std::abs(r) <= ceilingLin + 1e-4f);
    }
}

TEST_CASE("Synced reverse helper computes positive sample duration", "[core][effects]") {
    const int samples = computeSyncedReverseSamples(ReverseSyncMode::BeatEnd, 1.25, 138.0, 4.0, 48000.0);
    REQUIRE(samples > 1000);
    REQUIRE(shouldTriggerReverse(ReverseSyncMode::Beat4Auto, false, 3.2, 4.0));
    REQUIRE_FALSE(shouldTriggerReverse(ReverseSyncMode::Beat4Auto, false, 1.2, 4.0));
}

TEST_CASE("DistortionShaper 4x oversampled ADAA wavefolder suppresses foldback aliasing", "[core][effects][oversampling]") {
    constexpr double sampleRate = 48000.0;
    constexpr int warmup = 256;
    constexpr int count = 2048;
    DistortionShaper shaper;
    shaper.reset();
    std::vector<float> output(count, 0.0f);
    std::ranges::for_each(std::views::iota(0, warmup + count), [&](int idx) {
        const float input = 0.85f * std::sin(2.0f * std::numbers::pi_v<float> * 5500.0f * static_cast<float>(idx) / static_cast<float>(sampleRate));
        const float shaped = shaper.processSample(input, DistortionType::Wavefold, 0.78f, 0.0f, sampleRate);
        if (idx >= warmup)
            output[static_cast<size_t>(idx - warmup)] = shaped;
    });

    double fundPower = 0.0;
    double aliasPower = 0.0;
    const int aliasBins = static_cast<int>(std::floor(4200.0 * count / sampleRate));
    std::ranges::for_each(std::views::iota(8, aliasBins), [&](int bin) {
        const double omega = 2.0 * std::numbers::pi * static_cast<double>(bin) / static_cast<double>(count);
        const double coeff = 2.0 * std::cos(omega);
        double s1 = 0.0;
        double s2 = 0.0;
        std::ranges::for_each(std::views::iota(0, count), [&](int n) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) / static_cast<double>(count - 1));
            const double s0 = static_cast<double>(output[static_cast<size_t>(n)]) * w + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        });
        aliasPower += std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2);
    });
    const int fundBin = static_cast<int>(std::round(5500.0 * count / sampleRate));
    const double fundOmega = 2.0 * std::numbers::pi * static_cast<double>(fundBin) / static_cast<double>(count);
    const double fundCoeff = 2.0 * std::cos(fundOmega);
    double f1 = 0.0;
    double f2 = 0.0;
    std::ranges::for_each(std::views::iota(0, count), [&](int n) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) / static_cast<double>(count - 1));
        const double f0 = static_cast<double>(output[static_cast<size_t>(n)]) * w + fundCoeff * f1 - f2;
        f2 = f1;
        f1 = f0;
    });
    fundPower = std::max(1.0e-12, f1 * f1 + f2 * f2 - fundCoeff * f1 * f2);
    const double meanAliasPower = aliasPower / static_cast<double>(aliasBins - 8);
    REQUIRE(meanAliasPower < fundPower * 1.0e-4);
}
