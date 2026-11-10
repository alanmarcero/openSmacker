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
