#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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

class DistortionShaper {
public:
    void reset() noexcept;
    [[nodiscard]] float processSample(float input, DistortionType type, float drive, float tone, double sampleRate) noexcept;

private:
    float toneFilterState_ = 0.0f;
};

struct CompressorSettings {
    float thresholdDb = -14.0f; // -40..0 dB
    float ratio = 4.0f;         // 1..20
    float attackMs = 5.0f;      // 0.1..50 ms
    float releaseMs = 80.0f;    // 10..500 ms
    float makeupDb = 3.0f;      // 0..24 dB
    float mix = 1.0f;           // 0..1

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
    float ceilingDb = -0.3f;    // -12..0 dB
    float releaseMs = 20.0f;    // 5..100 ms
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

/** Stereo 3-band EQ with smooth log-frequency glide and +/-12 dB cut/boost per band. */
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
