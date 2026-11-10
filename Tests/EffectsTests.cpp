#include "Effects.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

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
