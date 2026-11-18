#pragma once

#include "SmackSynth.h"
#include "Effects.h"
#include <algorithm>
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
    int channel = 1;
};

struct MidiNoteOff {
    int sampleOffset = 0;
    int noteNumber = 69;
    int channel = 1;
};

struct MidiCcEvent {
    int sampleOffset = 0;
    int controller = 0;
    int value = 0;
    int channel = 1;
};

struct MidiPressureEvent {
    int sampleOffset = 0;
    int channel = 1;
    int noteNumber = -1;
    int value = 0;
    bool polyphonic = false;
};

struct VoiceKeyZone {
    int lowNote = 0;
    int highNote = 127;
};

[[nodiscard]] std::optional<VoiceMode> resolveVoiceModeForNote(
    int noteNumber, const std::array<VoiceKeyZone, 4>& zones) noexcept;

struct LayerOutputChannels {
    float* bodyLeft = nullptr;
    float* bodyRight = nullptr;
    float* wireLeft = nullptr;
    float* wireRight = nullptr;
    float* transientLeft = nullptr;
    float* transientRight = nullptr;
};

struct SmackWavVoice {
    std::vector<float> left;
    std::vector<float> right;
    std::array<StereoLayerBuffer, 3> layers;
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
    float echoMix = 0.0f;          // 0..1 tempo-synced stereo slapback echo

    ThreeBandEqSettings eq{};
    float stereoWidth = 1.0f;      // 0..2
    float outputGainDb = 0.0f;     // -36..+12 dB
};

struct EngineSettings {
    SmackSynthSettings synth{};
    SmackEffectSettings effects{};
    bool noteTracking = false;     // false = Fixed (audition root), true = Note tracking
    bool lockIn = false;           // Lock in rendered WAV buffer
    bool bypass = false;
    bool keyZonesEnabled = false;
    std::array<VoiceKeyZone, 4> voiceZones{};
    bool rollEnabled = false;
    int rollSubdivision = 16;
    int rollAccentEvery = 4;
    float rollProbability = 1.0f;
    float rollVelocity = 0.35f;
    float rollAccentVelocity = 0.85f;
    int rollNote = 69;
    int midiCcForBody = 20;
    int midiCcForWire = 21;
    int midiCcForTransient = 22;
};

class SmackWavCache {
public:
    void clear() noexcept;
    void capture(const std::vector<float>& left, const std::vector<float>& right, double sampleRate);
    [[nodiscard]] bool isLocked() const noexcept { return isLocked_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] const std::vector<float>& left() const noexcept { return voices_[primaryVoiceIndex_].left; }
    [[nodiscard]] const std::vector<float>& right() const noexcept { return voices_[primaryVoiceIndex_].right; }
    [[nodiscard]] const SmackWavVoice& voice(VoiceMode mode) const noexcept;

    [[nodiscard]] std::vector<uint8_t> toWavBytes() const;
    [[nodiscard]] std::vector<uint8_t> toBankWavBytes() const;
    bool fromWavBytes(const std::vector<uint8_t>& wavBytes);

private:
    friend class SmackEngine;

    void reserveFrames(size_t frames);
    void beginCapture() noexcept;
    [[nodiscard]] SmackWavVoice& writableVoice(size_t voiceIndex) noexcept;
    [[nodiscard]] bool finishCapture(double sampleRate, VoiceMode primaryVoice = VoiceMode::DanceSnare) noexcept;
    void captureVoices(std::array<SmackWavVoice, 4>&& voices, double sampleRate,
        VoiceMode primaryVoice = VoiceMode::DanceSnare);
    void setPrimaryVoice(VoiceMode mode) noexcept {
        primaryVoiceIndex_ = static_cast<size_t>(std::clamp(static_cast<int>(mode), 0, 3));
    }

    bool isLocked_ = false;
    double sampleRate_ = 48000.0;
    size_t primaryVoiceIndex_ = 0;
    std::array<SmackWavVoice, 4> voices_;
};

class SmackEngine {
public:
    static constexpr int kLatencySamples = 0;
    static constexpr uint32_t kMaxPreparationSamplesPerCallback = 256;

    void prepare(double sampleRate);
    void reset() noexcept;

    void process(
        const EngineSettings& settings,
        const Transport& transport,
        std::span<const MidiTrigger> midiTriggers,
        float* const* channels,
        int numChannels,
        int numSamples,
        std::span<const MidiCcEvent> controlChanges = {},
        std::span<const MidiPressureEvent> pressureChanges = {},
        LayerOutputChannels layerOutputs = {},
        std::span<const MidiNoteOff> noteOffs = {}) noexcept;

    void lockCurrentSmack(const EngineSettings& settings);
    void unlockSmack() noexcept;
    bool restoreLockedWav(const std::vector<uint8_t>& wavBytes,
        VoiceMode primaryVoice = VoiceMode::DanceSnare);

    [[nodiscard]] bool isSmackLocked() const noexcept { return wavCache_.isLocked(); }
    [[nodiscard]] const SmackWavCache& wavCache() const noexcept { return wavCache_; }

    [[nodiscard]] float currentPeak() const noexcept { return currentPeak_; }
    [[nodiscard]] float currentBeatPhase() const noexcept { return currentBeatPhase_; }
    [[nodiscard]] float currentSmackProgress() const noexcept { return currentSmackProgress_; }
    [[nodiscard]] uint32_t triggerSerial() const noexcept { return triggerSerial_; }
    [[nodiscard]] bool isReversing() const noexcept { return isReversing_; }
    [[nodiscard]] uint64_t lastReverseRenderSamples() const noexcept { return lastReverseRenderSamples_; }
    [[nodiscard]] uint64_t maxReverseSamplesPerHit() const noexcept { return reverseSource_.left.capacity(); }
    [[nodiscard]] int reverseDurationSamples() const noexcept { return reverseDuration_; }
    [[nodiscard]] int reverseSourceFrames() const noexcept { return reverseSourceFrames_; }
    [[nodiscard]] uint32_t lastCallbackPreparationSamples() const noexcept { return lastCallbackPreparationSamples_; }
    [[nodiscard]] uint32_t maxCallbackPreparationSamples() const noexcept { return maxCallbackPreparationSamples_; }

private:
    void triggerSmack(
        const EngineSettings& settings,
        double samplePpq,
        double bpm,
        double beatsPerBar,
        float velocity,
        int semitoneOffset,
        bool reverseCue,
        int noteNumber,
        int channel,
        bool oneShot = false) noexcept;
    void beginLockCapture(const EngineSettings& settings);
    [[nodiscard]] uint32_t advanceLockCapture(uint32_t sampleBudget) noexcept;
    [[nodiscard]] uint32_t advanceReversePreparation(uint32_t sampleBudget) noexcept;
    void startReversePreparation(const SmackSynthSettings& triggerSettings, float velocity,
        int semitoneOffset, int duration) noexcept;

    double sampleRate_ = 48000.0;
    double fallbackPpq_ = 0.0;
    bool previousLockState_ = false;

    SmackSynth synth_;
    SmackSynth captureSynth_;
    SmackSynth reverseSynth_;
    DistortionShaper distL_;
    DistortionShaper distR_;
    ThreeBandEq eq_;
    StereoCompressor compressor_;
    BrickwallLimiter limiter_;
    std::vector<float> echoBufferL_;
    std::vector<float> echoBufferR_;
    size_t echoWriteIndex_ = 0;

    SmackWavCache wavCache_;
    double wavPlayPosition_ = -1.0;
    float wavVelocity_ = 1.0f;

    SmackWavVoice reverseSource_;
    int reversePlayHead_ = -1;
    int reverseDuration_ = 0;
    int reverseSourceFrames_ = 0;
    bool isReversing_ = false;
    bool reverseUsesCache_ = false;
    bool reversePreparationActive_ = false;

    EngineSettings captureSettings_{};
    int captureVoiceIndex_ = 0;
    bool captureVoiceStarted_ = false;
    bool lockCaptureActive_ = false;
    std::array<DistortionShaper, 4> captureDistL_{};
    std::array<DistortionShaper, 4> captureDistR_{};
    std::array<ThreeBandEq, 4> captureEq_{};
    std::array<StereoCompressor, 4> captureCompressors_{};
    std::array<BrickwallLimiter, 4> captureLimiters_{};
    SmackSynthSettings reverseSettings_{};
    int reverseRequestDuration_ = 0;

    float ccVolume_ = 1.0f;
    float ccBrightness_ = 1.0f;
    float ccBodyGain_ = 1.0f;
    float ccWireGain_ = 1.0f;
    float ccTransientGain_ = 1.0f;
    std::array<float, 16> channelPressure_{};
    float activePressure_ = 0.0f;
    int activeNote_ = -1;
    int activeChannel_ = 1;
    VoiceMode activeVoiceMode_ = VoiceMode::DanceSnare;
    int64_t lastRollStep_ = 0;
    bool hasRollStep_ = false;
    uint32_t rollRandomState_ = 0x8f31a2c7U;

    float currentPeak_ = 0.0f;
    float currentBeatPhase_ = 0.0f;
    float currentSmackProgress_ = 1.0f;
    uint32_t triggerSerial_ = 0;
    uint64_t lastReverseRenderSamples_ = 0;
    uint32_t lastCallbackPreparationSamples_ = 0;
    uint32_t maxCallbackPreparationSamples_ = 0;
};

} // namespace opensmacker::core
