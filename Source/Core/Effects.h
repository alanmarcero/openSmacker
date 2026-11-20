#pragma once

#include "SmackSynth.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <vector>

namespace opensmacker::core {

enum class DistortionType {
    Tape = 0,
    Tube = 1,
    HardClip = 2,
    Wavefold = 3
};

enum class ReverseSyncMode {
    Off = 0,
    BeatEnd = 1,
    Beat4Auto = 2,
    HalfBeat = 3,
    QuarterBeat = 4
};

struct HalfbandUp4x {
    std::array<float, 4> stage1State0{};
    std::array<float, 4> stage1State1{};
    std::array<float, 4> stage2State0{};
    std::array<float, 4> stage2State1{};

    void reset() noexcept {
        stage1State0.fill(0.0f);
        stage1State1.fill(0.0f);
        stage2State0.fill(0.0f);
        stage2State1.fill(0.0f);
    }

    void primeDc(float dcValue) noexcept {
        std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t idx) {
            const float z0 = (1.0f - HalfbandDown4x::kBranch0[idx]) * dcValue;
            const float z1 = (1.0f - HalfbandDown4x::kBranch1[idx]) * dcValue;
            stage1State0[idx] = z0;
            stage1State1[idx] = z1;
            stage2State0[idx] = z0;
            stage2State1[idx] = z1;
        });
    }

    static std::pair<float, float> upsample2x(float input,
                                              std::array<float, 4>& state0,
                                              std::array<float, 4>& state1) noexcept {
        float branch0 = input;
        float branch1 = input;
        std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t idx) {
            const float out0 = HalfbandDown4x::kBranch0[idx] * branch0 + state0[idx];
            state0[idx] = branch0 - HalfbandDown4x::kBranch0[idx] * out0;
            branch0 = out0;

            const float out1 = HalfbandDown4x::kBranch1[idx] * branch1 + state1[idx];
            state1[idx] = branch1 - HalfbandDown4x::kBranch1[idx] * out1;
            branch1 = out1;
        });
        return { branch0, branch1 };
    }

    [[nodiscard]] std::array<float, 4> process(float input) noexcept {
        const auto [mid0, mid1] = upsample2x(input, stage1State0, stage1State1);
        const auto [out0, out1] = upsample2x(mid0, stage2State0, stage2State1);
        const auto [out2, out3] = upsample2x(mid1, stage2State0, stage2State1);
        return { out0, out1, out2, out3 };
    }
};

class DistortionShaper {
public:
    void reset() noexcept;
    [[nodiscard]] float processSample(float input, DistortionType type, float drive, float tone, double sampleRate) noexcept;

private:
    [[nodiscard]] float shapeSubSample(float scaledInput, DistortionType type) noexcept;

    HalfbandUp4x upsampler_{};
    HalfbandDown4x downsampler_{};
    double prevInput_ = 0.0;
    double prevAntideriv_ = 0.0;
    bool primed_ = false;
    float toneFilterState_ = 0.0f;
};

struct CompressorSettings {
    float thresholdDb = -14.0f;
    float ratio = 4.0f;
    float attackMs = 5.0f;
    float releaseMs = 80.0f;
    float makeupDb = 3.0f;
    float mix = 1.0f;

    bool operator==(const CompressorSettings&) const noexcept = default;
};

class StereoCompressor {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float& left, float& right, const CompressorSettings& settings) noexcept;

private:
    double sampleRate_ = 48000.0;
    float envelopeDb_ = -100.0f;
};

struct LimiterSettings {
    float ceilingDb = -0.3f;
    float releaseMs = 20.0f;
    bool enabled = true;

    bool operator==(const LimiterSettings&) const noexcept = default;
};

struct ThreeBandEqSettings {
    float lowFreqHz = 80.0f;
    float lowGainDb = 0.0f;
    float midFreqHz = 1000.0f;
    float midGainDb = 0.0f;
    float highFreqHz = 7500.0f;
    float highGainDb = 0.0f;

    bool operator==(const ThreeBandEqSettings&) const noexcept = default;
};

// Stereo 3-band EQ glides cutoff frequencies in log space to prevent zipper artifacts.
class ThreeBandEq {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float& left, float& right, const ThreeBandEqSettings& settings) noexcept;

private:
    enum class BandType { lowShelf, peak, highShelf };
    struct Band {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1L = 0.0, z2L = 0.0, z1R = 0.0, z2R = 0.0;
        float smoothedLogFreq = 0.0f;
        float smoothedDb = 0.0f;
        bool initialized = false;
    };

    void updateBand(Band& band, BandType type, float minHz, float maxHz, float targetHz, float targetDb) noexcept;
    static void filterBand(Band& band, float& left, float& right) noexcept;

    double sampleRate_ = 48000.0;
    float glideCoeff_ = 0.001f;
    std::array<Band, 3> bands_{};
};

class BrickwallLimiter {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float& left, float& right, const LimiterSettings& settings) noexcept;

private:
    double sampleRate_ = 48000.0;
    float gain_ = 1.0f;
};

[[nodiscard]] bool shouldTriggerReverse(
    ReverseSyncMode mode,
    bool manualCue,
    double ppqPosition,
    double beatsPerBar) noexcept;

[[nodiscard]] double reverseTargetPpq(
    ReverseSyncMode mode,
    double ppqPosition,
    double beatsPerBar) noexcept;

[[nodiscard]] int computeSyncedReverseSamples(
    ReverseSyncMode mode,
    double ppqPosition,
    double bpm,
    double beatsPerBar,
    double sampleRate) noexcept;

} // namespace opensmacker::core
