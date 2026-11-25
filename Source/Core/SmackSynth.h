#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <ranges>
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
    bool articulatedModes = true;
    // Shaped one-shots use short drum partials, smooth decay, and 4x oversampled FM.
    // Saved sessions from before this engine keep this flag false.
    bool shapedEngine = false;

    int note = 9;              // A. The plugin keeps this fixed; key tracking moves the played note.
    int octave = 4;            // A4, the same root the other drum synths track from.
    float fineCents = 0.0f;

    int preStrikeCount = 1;
    float preStrikeSpreadMs = 11.0f;
    float preStrikeDecayMs = 8.0f;
    float preStrikeJitter = 0.12f;
    float preStrikeFlamSpread = 0.40f;
    float preStrikeLevel = 0.55f;

    VoiceMode voiceMode = VoiceMode::DanceSnare;
    float toneFreqHz = 220.0f;
    float toneQ = 3.5f;
    float toneDecayMs = 180.0f;
    float tonePitchEnv = 12.0f;
    float tonePitchDecayMs = 22.0f;
    float fmAmount = 0.0f;
    float shellSpread = 0.0f;
    float subThump = 0.0f;
    float toneLevel = 0.85f;
    float bodyHPF = 20.0f;
    float bodyLPF = 20000.0f;
    float bodyPan = 0.0f;
    float bodyVelocityDepth = 1.0f;
    float wireVelocityDepth = 1.0f;
    float transientVelocityDepth = 1.0f;
    float bodyAftertouchDepth = 0.0f;
    float wireAftertouchDepth = 0.0f;
    float transientAftertouchDepth = 0.0f;
    float aftertouchValue = 0.0f;
    float bodyPerformanceGain = 1.0f;
    float wirePerformanceGain = 1.0f;
    float transientPerformanceGain = 1.0f;

    // Full ADSR gates on MIDI note-off; false preserves one-shot playback.
    bool fullAdsrEnabled = false;
    float ampAttackMs = 0.35f;
    float ampDecayMs = 72.0f;
    float ampSustain = 0.70f;
    float ampReleaseMs = 180.0f;

    NoiseType noiseType = NoiseType::SnareWires;
    float noiseAttackMs = 0.0f;
    float noiseHoldMs = 2.0f;
    float noiseDecayMs = 210.0f;
    float noiseCurve = -0.35f;
    float noiseHPF = 280.0f;
    float noiseLPF = 11000.0f;
    float noiseStereoWidth = 1.15f;
    float noiseLevel = 0.80f;
    float wireTension = 0.55f;
    float wirePan = 0.0f;

    ClickType clickType = ClickType::Stick;
    float clickLevel = 0.50f;
    float clickFreqHz = 3800.0f;
    float transientHPF = 20.0f;
    float transientLPF = 20000.0f;
    float transientPan = 0.0f;

    float reverbSize = 0.65f;
    float reverbDecayMs = 480.0f;
    float reverbDamp = 0.38f;
    float reverbGateMs = 180.0f;
    float reverbMix = 0.32f;

    bool operator==(const SmackSynthSettings&) const noexcept = default;
};

[[nodiscard]] inline float noteToHz(int noteIndex, int octave, float fineCents = 0.0f) noexcept {
    const int note = (noteIndex % kNumNotes + kNumNotes) % kNumNotes;
    const int oct = std::clamp(octave, 1, 8);
    const int midiNote = 12 * (oct + 1) + note;
    const float totalNotes = static_cast<float>(midiNote - 69) + (fineCents / 100.0f);
    return 440.0f * std::pow(2.0f, totalNotes / 12.0f);
}

// ZDF TPT state-variable filter prevents Direct-Form II modulation clicks at high resonance.
struct ZdfSvf {
    double ic1eq = 0.0;
    double ic2eq = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double a3 = 0.0;
    double dampingK = 1.4142135623730951;
    float smoothedHz = 1000.0f;
    float cachedHz = -1.0f;
    float cachedQ = -1.0f;
    bool initialized = false;

    void reset() noexcept {
        ic1eq = 0.0;
        ic2eq = 0.0;
        cachedHz = -1.0f;
        cachedQ = -1.0f;
        initialized = false;
    }

    void updateCoefficients(float targetHz, float resonanceQ, double sampleRate, float smoothAlpha) noexcept {
        const float nyquistGuard = static_cast<float>(std::max(8000.0, sampleRate) * 0.45);
        const float clampedHz = std::clamp(targetHz, 12.0f, nyquistGuard);
        const float clampedQ = std::clamp(resonanceQ, 0.25f, 20.0f);
        if (!initialized) {
            smoothedHz = clampedHz;
            initialized = true;
        }
        smoothedHz += smoothAlpha * (clampedHz - smoothedHz);
        if (std::abs(smoothedHz - cachedHz) <= 0.05f && std::abs(clampedQ - cachedQ) <= 0.002f)
            return;
        cachedHz = smoothedHz;
        cachedQ = clampedQ;
        const double g = std::tan(std::numbers::pi * std::clamp(static_cast<double>(cachedHz) / std::max(8000.0, sampleRate), 1.0e-4, 0.46));
        dampingK = 1.0 / static_cast<double>(cachedQ);
        a1 = 1.0 / (1.0 + g * (g + dampingK));
        a2 = g * a1;
        a3 = g * a2;
    }

    [[nodiscard]] float processLowpass(float input) noexcept {
        const double v0 = static_cast<double>(input);
        const double v3 = v0 - ic2eq;
        const double v1 = a1 * ic1eq + a2 * v3;
        const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;
        const bool isFinite = std::isfinite(v2);
        return isFinite ? static_cast<float>(v2) : 0.0f;
    }

    [[nodiscard]] float processHighpass(float input) noexcept {
        const double v0 = static_cast<double>(input);
        const double v3 = v0 - ic2eq;
        const double v1 = a1 * ic1eq + a2 * v3;
        const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;
        const double hp = v0 - dampingK * v1 - v2;
        const bool isFinite = std::isfinite(hp);
        return isFinite ? static_cast<float>(hp) : 0.0f;
    }

    [[nodiscard]] float processBandpassNormalized(float input) noexcept {
        const double v0 = static_cast<double>(input);
        const double v3 = v0 - ic2eq;
        const double v1 = a1 * ic1eq + a2 * v3;
        const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;
        const double bp = dampingK * v1;
        const bool isFinite = std::isfinite(bp);
        return isFinite ? static_cast<float>(bp) : 0.0f;
    }
};

// First-order antiderivative anti-aliasing suppresses tanh foldback harmonics.
struct AdaaTanh {
    double prevInput = 0.0;
    double prevAntideriv = 0.0;
    bool primed = false;

    void reset() noexcept {
        prevInput = 0.0;
        prevAntideriv = 0.0;
        primed = false;
    }

    [[nodiscard]] float process(float input) noexcept {
        const double curr = std::clamp(static_cast<double>(input), -32.0, 32.0);
        const double absCurr = std::abs(curr);
        const double currAntideriv = absCurr + std::log1p(std::exp(-2.0 * absCurr)) - std::numbers::ln2;
        if (!primed) {
            prevInput = curr;
            prevAntideriv = currAntideriv;
            primed = true;
            return static_cast<float>(std::tanh(curr));
        }
        const double delta = curr - prevInput;
        double out = 0.0;
        if (std::abs(delta) > 1.0e-5)
            out = (currAntideriv - prevAntideriv) / delta;
        if (std::abs(delta) <= 1.0e-5)
            out = std::tanh(0.5 * (curr + prevInput));
        prevInput = curr;
        prevAntideriv = currAntideriv;
        const bool isFinite = std::isfinite(out);
        return isFinite ? static_cast<float>(out) : 0.0f;
    }
};

// Two 17th-order polyphase allpass halfband stages reject aliases by more than 90 dB.
struct HalfbandDown4x {
    static constexpr std::array<float, 4> kBranch0{
        0.03668150216364802f, 0.27463175937945444f, 0.56109869787919531f, 0.76974183386322703f
    };
    static constexpr std::array<float, 4> kBranch1{
        0.13654762463195794f, 0.42313861743656711f, 0.67754004997416184f, 0.88920281218984905f
    };

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

    [[nodiscard]] static float decimate2x(float olderSample, float newerSample,
                                          std::array<float, 4>& state0, std::array<float, 4>& state1) noexcept {
        float branchNew = newerSample;
        float branchOld = olderSample;
        std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t idx) {
            const float outNew = kBranch0[idx] * branchNew + state0[idx];
            state0[idx] = branchNew - kBranch0[idx] * outNew;
            branchNew = outNew;

            const float outOld = kBranch1[idx] * branchOld + state1[idx];
            state1[idx] = branchOld - kBranch1[idx] * outOld;
            branchOld = outOld;
        });
        return 0.5f * (branchNew + branchOld);
    }

    [[nodiscard]] float process(const std::array<float, 4>& subSamples) noexcept {
        const float mid0 = decimate2x(subSamples[0], subSamples[1], stage1State0, stage1State1);
        const float mid1 = decimate2x(subSamples[2], subSamples[3], stage1State0, stage1State1);
        return decimate2x(mid0, mid1, stage2State0, stage2State1);
    }
};

struct ModalResonator2Pole {
    float stateU = 0.0f;
    float stateV = 0.0f;

    void reset() noexcept {
        stateU = 0.0f;
        stateV = 0.0f;
    }

    [[nodiscard]] float step(float excitation, float radius, float cosOmega, float sinOmega) noexcept {
        const float nextU = radius * (stateU * cosOmega - stateV * sinOmega) + excitation;
        const float nextV = radius * (stateU * sinOmega + stateV * cosOmega);
        const bool finiteU = std::isfinite(nextU);
        const bool finiteV = std::isfinite(nextV);
        stateU = finiteU ? nextU : 0.0f;
        stateV = finiteV ? nextV : 0.0f;
        return stateV;
    }
};

struct StereoFrame {
    float left = 0.0f;
    float right = 0.0f;
    float bodyLeft = 0.0f;
    float bodyRight = 0.0f;
    float wireLeft = 0.0f;
    float wireRight = 0.0f;
    float transientLeft = 0.0f;
    float transientRight = 0.0f;
};

struct StereoLayerBuffer {
    std::vector<float> left;
    std::vector<float> right;
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
        if (buffer.empty())
            return in;
        const float delayed = buffer[writeIndex];
        const float out = -feedback * in + delayed;
        buffer[writeIndex] = in + feedback * delayed;
        writeIndex = (writeIndex + 1) % buffer.size();
        return out;
    }
};

class GatedStereoReverb {
public:
    static constexpr size_t kNumLines = 8;

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
    std::array<std::vector<float>, kNumLines> fdnDelay_{};
    std::array<std::vector<float>, kNumLines> lineAllpass_{};
    std::array<int, kNumLines> fdnWrite_{};
    std::array<size_t, kNumLines> lineApWrite_{};
    std::array<float, kNumLines> xoverLowState_{};
    std::array<float, kNumLines> xoverHighState_{};
    std::array<double, kNumLines> lfoPhase_{};
    float hpfStateL_ = 0.0f;
    float hpfStateR_ = 0.0f;
    int sampleCounter_ = 0;
    float gateEnvelope_ = 1.0f;
};

class SmackSynth {
public:
    static constexpr size_t kNumWireCombs = 8;

    void prepare(double sampleRate);
    void reset() noexcept;
    void trigger(const SmackSynthSettings& settings, float velocity = 1.0f, int semitoneOffset = 0,
        int noteNumber = -1, int channel = 1) noexcept;
    void noteOff(int noteNumber, int channel, const SmackSynthSettings& settings) noexcept;
    [[nodiscard]] StereoFrame processSample(const SmackSynthSettings& settings) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_; }
    [[nodiscard]] float getEnvelopeProgress() const noexcept;

    void renderBlock(
        std::vector<float>& leftOut,
        std::vector<float>& rightOut,
        const SmackSynthSettings& settings,
        float velocity = 1.0f,
        int semitoneOffset = 0,
        std::array<StereoLayerBuffer, 3>* layerOutputs = nullptr) noexcept;
    [[nodiscard]] int renderPrefix(
        std::vector<float>& leftOut,
        std::vector<float>& rightOut,
        const SmackSynthSettings& settings,
        int maxSamples,
        float velocity = 1.0f,
        int semitoneOffset = 0,
        std::array<StereoLayerBuffer, 3>* layerOutputs = nullptr) noexcept;

private:
    [[nodiscard]] StereoFrame generateStereoNoise(
        NoiseType type, float tunedHz, float overtoneEnv, float wireTension, float bottomHeadModal) noexcept;

    double sampleRate_ = 48000.0;
    float filterSmoothAlpha_ = 0.01f;
    bool isPlaying_ = false;
    float bodyVelocity_ = 1.0f;
    float strikeBrightness_ = 1.0f;
    std::array<float, 3> baseModeRatios_{};
    float wireVelocity_ = 1.0f;
    float transientVelocity_ = 1.0f;
    int semitoneOffset_ = 0;

    int totalSamplesPlayed_ = 0;
    int totalDurationSamples_ = 0;
    int dryDurationSamples_ = 0;
    bool fullAdsrEnabled_ = false;
    bool noteOffReceived_ = false;
    int activeNoteNumber_ = -1;
    int activeChannel_ = 1;
    int releaseStartSample_ = -1;
    int releaseDurationSamples_ = 0;
    int reverbTailSamples_ = 0;
    float releaseStartLevel_ = 0.0f;

    int preStrikeCount_ = 1;
    std::array<int, 5> burstStartSamples_{};
    std::array<float, 5> burstAmplitudes_{};
    std::array<float, 5> burstPans_{};
    int mainSmackSample_ = 0;

    float phaseFund_ = 0.0f;
    float phaseSub_ = 0.0f;
    float phaseOctave_ = 0.0f;
    float phaseMod1_ = 0.0f;
    float phaseMod2_ = 0.0f;
    float phaseFundR_ = 0.0f;
    std::array<ModalResonator2Pole, 3> shellModesL_{};
    std::array<ModalResonator2Pole, 3> shellModesR_{};
    std::array<ModalResonator2Pole, 3> rimModes_{};
    std::array<float, 4> metalPhasesL_{};
    std::array<float, 4> metalPhasesR_{};
    float clickPhase_ = 0.0f;
    float clickPhase2_ = 0.0f;

    HalfbandDown4x bodyDownL_{};
    HalfbandDown4x bodyDownR_{};
    HalfbandDown4x rawModalDownL_{};
    HalfbandDown4x rawModalDownR_{};
    HalfbandDown4x transientDown_{};

    ZdfSvf shellFilterL_{};
    ZdfSvf shellFilterR_{};
    AdaaTanh shellSatL_{};
    AdaaTanh shellSatR_{};
    AdaaTanh rimSat_{};

    ZdfSvf noiseHPF_L_{};
    ZdfSvf noiseHPF_R_{};
    ZdfSvf noiseLPF_L_{};
    ZdfSvf noiseLPF_R_{};
    ZdfSvf bodyHPF_L_{};
    ZdfSvf bodyHPF_R_{};
    ZdfSvf bodyLPF_L_{};
    ZdfSvf bodyLPF_R_{};
    ZdfSvf transientHPF_L_{};
    ZdfSvf transientHPF_R_{};
    ZdfSvf transientLPF_L_{};
    ZdfSvf transientLPF_R_{};
    ZdfSvf tomVelvetFilterL_{};
    ZdfSvf tomVelvetFilterR_{};

    std::array<std::vector<float>, kNumWireCombs> wireCombL_{};
    std::array<std::vector<float>, kNumWireCombs> wireCombR_{};
    std::array<int, kNumWireCombs> wireWriteL_{};
    std::array<int, kNumWireCombs> wireWriteR_{};
    std::array<float, kNumWireCombs> wireDispStateL_{};
    std::array<float, kNumWireCombs> wireDispStateR_{};
    std::array<float, kNumWireCombs> wireDampStateL_{};
    std::array<float, kNumWireCombs> wireDampStateR_{};

    uint32_t prngStateL_ = 0x2468ACE1U;
    uint32_t prngStateR_ = 0x1357BDF0U;
    uint32_t tomVelvetPrng_ = 0x9E3779B9U;
    int tomVelvetCounter_ = 0;
    int tomVelvetNextPulse_ = 4;
    float tomVelvetPulseSign_ = 1.0f;
    std::array<float, 2> tomVelvetHistoryL_{};
    std::array<float, 2> tomVelvetHistoryR_{};
    float wireStateL_ = 0.0f;
    float wireStateR_ = 0.0f;
    float velvetStateL_ = 0.0f;
    float velvetStateR_ = 0.0f;

    GatedStereoReverb reverb_;
};

} // namespace opensmacker::core
