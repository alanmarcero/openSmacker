#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <vector>

namespace opensmacker::core {

inline constexpr int kNumNotes = 12;

inline constexpr std::array<std::string_view, kNumNotes> kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

enum class NoiseType {
    SnareWires = 0,
    WhiteSizzle = 1,
    VelvetAir = 2,
    MetallicRing = 3
};

enum class VoiceMode {
    DanceSnare = 0,
    ClubTom = 1,
    FmBell = 2,
    PrydaRimSmack = 3
};

enum class ClickType {
    Stick = 0,
    Mallet = 1,
    Rimshot = 2,
    Laser = 3
};

struct SmackSynthSettings {
    // Pitch & Tuning
    int note = 9;                  // 0..11 (A), default A
    int octave = 3;                // 1..8, default 3
    float fineCents = 0.0f;        // -50..+50 cents

    // Pre-Strike Flam / Drag Roll
    int preStrikeCount = 1;        // 1..5 micro-strikes before main hit
    float preStrikeSpreadMs = 11.0f;// 2..45 ms spacing between strikes
    float preStrikeDecayMs = 8.0f; // 2..35 ms decay of each strike
    float preStrikeJitter = 0.12f; // 0..1 timing and pitch humanization
    float preStrikeFlamSpread = 0.40f; // 0..1 stereo micro-panning spread
    float preStrikeLevel = 0.55f;  // 0..1 volume of flam strikes

    // Tunable Drum / Bell / Tom Body
    VoiceMode voiceMode = VoiceMode::DanceSnare;
    float toneFreqHz = 220.0f;     // 60..6000 Hz resonant shell / overtone frequency
    float toneQ = 3.5f;            // 0.5..15.0 resonance / shell ring
    float toneDecayMs = 180.0f;    // 15..1200 ms body decay
    float tonePitchEnv = 12.0f;    // -24..+36 semitones initial pitch drop/sweep
    float tonePitchDecayMs = 22.0f;// 3..150 ms pitch envelope decay
    float fmAmount = 0.30f;        // 0..1 harmonic overtone / FM metallic index
    float toneLevel = 0.85f;       // 0..1 level of tonal drum/bell core

    // Snare Wires, Sizzle & Air
    NoiseType noiseType = NoiseType::SnareWires;
    float noiseAttackMs = 0.0f;    // 0..20 ms
    float noiseHoldMs = 2.0f;      // 0..30 ms
    float noiseDecayMs = 210.0f;   // 15..1200 ms
    float noiseCurve = -0.35f;     // -1..+1 shape curve
    float noiseHPF = 280.0f;       // 20..2500 Hz
    float noiseLPF = 11000.0f;     // 800..20000 Hz
    float noiseStereoWidth = 1.15f;// 0 (mono) .. 2 (ultra-wide)
    float noiseLevel = 0.80f;      // 0..1

    // Stick / Mallet / Rim Transient
    ClickType clickType = ClickType::Stick;
    float clickLevel = 0.50f;      // 0..1
    float clickFreqHz = 3800.0f;   // 500..12000 Hz

    // Gated FDN Reverb Tail
    float reverbSize = 0.65f;      // 0.1..1.0
    float reverbDecayMs = 480.0f;  // 50..2500 ms
    float reverbDamp = 0.38f;      // 0..1 high damping
    float reverbGateMs = 180.0f;   // 0 (off) .. 600 ms
    float reverbMix = 0.32f;       // 0..1

    bool operator==(const SmackSynthSettings&) const noexcept = default;
};

// Computes base tuned frequency from note index, octave, and fine cents.
[[nodiscard]] inline float noteToHz(int noteIndex, int octave, float fineCents = 0.0f) noexcept {
    const int note = (noteIndex % kNumNotes + kNumNotes) % kNumNotes;
    const int oct = std::clamp(octave, 1, 8);
    // MIDI note: Octave 4 C is 60, Octave 4 A is 69 (440 Hz)
    const int midiNote = 12 * (oct + 1) + note;
    const float totalNotes = static_cast<float>(midiNote - 69) + (fineCents / 100.0f);
    return 440.0f * std::pow(2.0f, totalNotes / 12.0f);
}

struct BiquadState {
    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;

    void reset() noexcept {
        x1 = x2 = y1 = y2 = 0.0f;
    }
};

struct StereoFrame {
    float left = 0.0f;
    float right = 0.0f;
};

struct AllpassDiffuser {
    std::vector<float> buffer;
    size_t writeIndex = 0;
    float feedback = 0.6f;

    void prepare(size_t delaySamples, float g = 0.6f) {
        buffer.assign(std::max<size_t>(1, delaySamples), 0.0f);
        writeIndex = 0;
        feedback = g;
    }

    void reset() noexcept {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    [[nodiscard]] float process(float in) noexcept {
        if (buffer.empty()) {
            return in;
        }
        const float delayed = buffer[writeIndex];
        const float out = -feedback * in + delayed;
        buffer[writeIndex] = in + feedback * delayed;
        writeIndex = (writeIndex + 1) % buffer.size();
        return out;
    }
};

class GatedStereoReverb {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    [[nodiscard]] StereoFrame process(float inLeft, float inRight, float size, float decayMs, float damp, float gateMs, double sampleRate) noexcept;

private:
    [[nodiscard]] static float readHermite(const std::vector<float>& buffer, double delaySamples, int writePos) noexcept;

    AllpassDiffuser diffL1_;
    AllpassDiffuser diffL2_;
    AllpassDiffuser diffL3_;
    AllpassDiffuser diffR1_;
    AllpassDiffuser diffR2_;
    AllpassDiffuser diffR3_;
    std::vector<float> delayL1_;
    std::vector<float> delayL2_;
    std::vector<float> delayL3_;
    std::vector<float> delayR1_;
    std::vector<float> delayR2_;
    std::vector<float> delayR3_;
    int writeL1_ = 0, writeL2_ = 0, writeL3_ = 0;
    int writeR1_ = 0, writeR2_ = 0, writeR3_ = 0;
    float dampStateL1_ = 0.0f, dampStateL2_ = 0.0f;
    float dampStateR1_ = 0.0f, dampStateR2_ = 0.0f;
    float hpfStateL_ = 0.0f, hpfStateR_ = 0.0f;
    double lfoPhaseL_ = 0.0;
    double lfoPhaseR_ = 0.0;
    int sampleCounter_ = 0;
    float gateEnvelope_ = 1.0f;
};

// Pure C++20 Tunable Dance Snare, Bell & Tom-Tom Synthesizer
class SmackSynth {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void trigger(const SmackSynthSettings& settings, float velocity = 1.0f, int semitoneOffset = 0) noexcept;
    [[nodiscard]] StereoFrame processSample(const SmackSynthSettings& settings) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_; }
    [[nodiscard]] float getEnvelopeProgress() const noexcept;

    void renderBlock(
        std::vector<float>& leftOut,
        std::vector<float>& rightOut,
        const SmackSynthSettings& settings,
        float velocity = 1.0f,
        int semitoneOffset = 0) noexcept;

private:
    [[nodiscard]] float generateNoiseSample(NoiseType type, float tunedHz) noexcept;

    double sampleRate_ = 48000.0;
    bool isPlaying_ = false;
    float velocity_ = 1.0f;
    int semitoneOffset_ = 0;

    int totalSamplesPlayed_ = 0;
    int totalDurationSamples_ = 0;

    // Micro-strike flam / roll timing
    int preStrikeCount_ = 1;
    std::array<int, 5> burstStartSamples_{};
    std::array<float, 5> burstAmplitudes_{};
    std::array<float, 5> burstPans_{};
    int mainSmackSample_ = 0;

    // Multi-operator / modal phase accumulators
    float phaseFund_ = 0.0f;
    float phaseMode2_ = 0.0f;
    float phaseMode3_ = 0.0f;
    float phaseMod_ = 0.0f;
    float metalPhaseA_ = 0.0f;
    float metalPhaseB_ = 0.0f;

    // Filter states
    BiquadState shellFilterL_{};
    BiquadState shellFilterR_{};
    BiquadState noiseHPF_L_{};
    BiquadState noiseHPF_R_{};
    BiquadState noiseLPF_L_{};
    BiquadState noiseLPF_R_{};

    // Filter cache
    float lastHPF_ = -1.0f;
    float lastLPF_ = -1.0f;
    float b0_h_ = 0.0f, b1_h_ = 0.0f, b2_h_ = 0.0f, a1_h_ = 0.0f, a2_h_ = 0.0f;
    float b0_l_ = 0.0f, b1_l_ = 0.0f, b2_l_ = 0.0f, a1_l_ = 0.0f, a2_l_ = 0.0f;

    // PRNG seed for deterministic and jittered noise
    uint32_t prngState_ = 0x2468ACE1;
    float wireState0_ = 0.0f, wireState1_ = 0.0f;

    // Gated Reverb
    GatedStereoReverb reverb_;
};

} // namespace opensmacker::core
