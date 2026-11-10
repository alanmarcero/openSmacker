#pragma once

#include "SmackSynth.h"
#include "Effects.h"
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace opensmacker::core {

struct Transport {
    double bpm = 138.0;
    std::optional<double> ppqPosition;
    double beatsPerBar = 4.0;
    bool isPlaying = true;
};

struct MidiTrigger {
    int sampleOffset = 0;
    int noteNumber = 69; // A4
    float velocity = 1.0f;
    bool audition = false;
};

struct MidiCcEvent {
    int sampleOffset = 0;
    int controller = 0;
    int value = 0;
};

struct SmackEffectSettings {
    DistortionType distType = DistortionType::Tape;
    float distDrive = 0.14f;       // 0..1
    float distTone = 0.0f;         // -1..+1
    float distMix = 0.22f;         // 0..1

    CompressorSettings compressor{};
    LimiterSettings limiter{};

    ReverseSyncMode reverseMode = ReverseSyncMode::Off;
    float reverseMix = 0.5f;       // 0..1

    float stereoWidth = 1.0f;      // 0..2
    float outputGainDb = 0.0f;     // -36..+12 dB
};

struct EngineSettings {
    SmackSynthSettings synth{};
    SmackEffectSettings effects{};
    bool noteTracking = false;     // false = Fixed (audition root), true = Note tracking
    bool lockIn = false;           // Lock in rendered WAV buffer
    bool bypass = false;
};

class SmackWavCache {
public:
    void clear() noexcept;
    void capture(const std::vector<float>& left, const std::vector<float>& right, double sampleRate);
    [[nodiscard]] bool isLocked() const noexcept { return isLocked_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] const std::vector<float>& left() const noexcept { return left_; }
    [[nodiscard]] const std::vector<float>& right() const noexcept { return right_; }

    [[nodiscard]] std::vector<uint8_t> toWavBytes() const;
    bool fromWavBytes(const std::vector<uint8_t>& wavBytes);

private:
    bool isLocked_ = false;
    double sampleRate_ = 48000.0;
    std::vector<float> left_;
    std::vector<float> right_;
};

class SmackEngine {
public:
    static constexpr int kLatencySamples = 0;

    void prepare(double sampleRate);
    void reset() noexcept;

    void process(
        const EngineSettings& settings,
        const Transport& transport,
        std::span<const MidiTrigger> midiTriggers,
        float* const* channels,
        int numChannels,
        int numSamples,
        std::span<const MidiCcEvent> controlChanges = {}) noexcept;

    void lockCurrentSmack(const EngineSettings& settings);
    void unlockSmack() noexcept;
    bool restoreLockedWav(const std::vector<uint8_t>& wavBytes);

    [[nodiscard]] bool isSmackLocked() const noexcept { return wavCache_.isLocked(); }
    [[nodiscard]] const SmackWavCache& wavCache() const noexcept { return wavCache_; }

    [[nodiscard]] float currentPeak() const noexcept { return currentPeak_; }
    [[nodiscard]] float currentBeatPhase() const noexcept { return currentBeatPhase_; }
    [[nodiscard]] float currentSmackProgress() const noexcept { return currentSmackProgress_; }
    [[nodiscard]] uint32_t triggerSerial() const noexcept { return triggerSerial_; }
    [[nodiscard]] bool isReversing() const noexcept { return isReversing_; }

private:
    void triggerSmack(
        const EngineSettings& settings,
        double samplePpq,
        double bpm,
        double beatsPerBar,
        float velocity,
        int semitoneOffset,
        bool reverseCue) noexcept;

    double sampleRate_ = 48000.0;
    double fallbackPpq_ = 0.0;
    bool previousLockState_ = false;

    SmackSynth synth_;
    DistortionShaper distL_;
    DistortionShaper distR_;
    StereoCompressor compressor_;
    BrickwallLimiter limiter_;

    SmackWavCache wavCache_;
    int wavPlayHead_ = -1;
    float wavVelocity_ = 1.0f;

    std::vector<float> reverseBufferL_;
    std::vector<float> reverseBufferR_;
    int reversePlayHead_ = -1;
    int reverseDuration_ = 0;
    bool isReversing_ = false;

    float ccVolume_ = 1.0f;
    float ccBrightness_ = 1.0f;

    float currentPeak_ = 0.0f;
    float currentBeatPhase_ = 0.0f;
    float currentSmackProgress_ = 1.0f;
    uint32_t triggerSerial_ = 0;
};

} // namespace opensmacker::core
