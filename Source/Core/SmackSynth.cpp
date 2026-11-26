#include "SmackSynth.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <ranges>

namespace opensmacker::core {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kButterworthQ = 0.70710678f;
constexpr float kBesselQ = 0.57735027f;

float nextRandom(uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) * (2.0f / 4294967296.0f) - 1.0f;
}

float wrapPhase(float phase) noexcept {
    return phase >= kTwoPi ? std::fmod(phase, kTwoPi) : phase;
}

float amplitudeEnvelope(int sample, int duration, const SmackSynthSettings& settings, float sampleRate) noexcept {
    if (sample < 0 || duration <= 0 || sample >= duration)
        return 0.0f;

    const float total = static_cast<float>(duration);
    const float releaseSamples = std::clamp(settings.ampReleaseMs, 5.0f, 800.0f) * 0.001f * sampleRate;
    const float releaseStart = std::max(0.0f, total - std::min(total, releaseSamples));
    const float stageBudget = releaseStart;
    float attack = std::clamp(settings.ampAttackMs, 0.05f, 20.0f) * 0.001f * sampleRate;
    float decay = std::clamp(settings.ampDecayMs, 1.0f, 600.0f) * 0.001f * sampleRate;
    const float stageLength = attack + decay;
    if (stageLength > stageBudget && stageLength > 0.0f) {
        const float scale = stageBudget / stageLength;
        attack *= scale;
        decay *= scale;
    }

    const float time = static_cast<float>(sample);
    const float sustain = std::clamp(settings.ampSustain, 0.0f, 1.0f);
    if (attack > 0.0f && time < attack)
        return std::sin(0.5f * kPi * time / attack);
    if (decay > 0.0f && time < attack + decay) {
        const float phase = (time - attack) / decay;
        const float smooth = phase * phase * (3.0f - 2.0f * phase);
        return 1.0f + (sustain - 1.0f) * smooth;
    }
    if (time < releaseStart)
        return sustain;
    const float releaseLength = std::max(1.0f, total - releaseStart);
    const float phase = std::clamp((time - releaseStart) / releaseLength, 0.0f, 1.0f);
    return sustain * 0.5f * (1.0f + std::cos(kPi * phase));
}

// One-shot contour falls continuously to silence with no sustain plateau.
float shapedOneShot(int sample, int duration, const SmackSynthSettings& settings, float sampleRate) noexcept {
    if (sample < 0 || duration <= 1 || sample >= duration)
        return 0.0f;
    const float dur = static_cast<float>(duration);
    const float attack = std::clamp(settings.ampAttackMs, 0.05f, 20.0f) * 0.001f * sampleRate;
    const float time = static_cast<float>(sample);
    if (attack > 1.0f && time < attack)
        return std::sin(0.5f * kPi * time / attack);

    const float sustain = std::clamp(settings.ampSustain, 0.0f, 1.0f);
    const float tau = std::max(0.012f * sampleRate,
        std::clamp(settings.ampDecayMs, 8.0f, 600.0f) * 0.001f * sampleRate * std::lerp(0.62f, 1.25f, sustain));
    float env = std::exp(-(time - std::min(time, attack)) / tau);
    const float release = std::clamp(settings.ampReleaseMs, 8.0f, 500.0f) * 0.001f * sampleRate;
    const float land = std::clamp(release, 0.008f * sampleRate, dur * 0.5f);
    if (time > dur - land) {
        const float phase = std::clamp((time - (dur - land)) / land, 0.0f, 1.0f);
        env *= 0.5f * (1.0f + std::cos(kPi * phase));
    }
    return env;
}

struct ShapedProfile {
    float fundScale;
    std::array<float, 3> modeScales;
    std::array<float, 3> modeGains;
    float fundGain;
    float fmScale;
    bool bell;
    bool octave;
};

ShapedProfile shapedProfile(VoiceMode mode) noexcept {
    switch (mode) {
        case VoiceMode::DanceSnare:
            return { 0.48f, { 0.16f, 0.10f, 0.06f }, { 0.22f, 0.10f, 0.045f }, 0.96f, 0.5f, false, false };
        case VoiceMode::ClubTom:
            return { 0.58f, { 0.26f, 0.18f, 0.12f }, { 0.21f, 0.12f, 0.095f }, 0.90f, 0.40f, false, true };
        case VoiceMode::FmBell:
            return { 0.95f, { 0.56f, 0.42f, 0.28f }, { 0.42f, 0.26f, 0.12f }, 0.74f, 1.0f, true, false };
        case VoiceMode::PrydaRimSmack:
            return { 0.42f, { 0.12f, 0.07f, 0.045f }, { 0.24f, 0.12f, 0.06f }, 0.74f, 0.35f, false, false };
    }
    return { 0.30f, { 0.08f, 0.05f, 0.03f }, { 0.12f, 0.06f, 0.03f }, 0.7f, 0.5f, false, false };
}

float gatedEnvelope(int sample, const SmackSynthSettings& settings, float sampleRate) noexcept {
    const float time = static_cast<float>(std::max(0, sample));
    const float attack = std::clamp(settings.ampAttackMs, 0.0f, 10000.0f) * 0.001f * sampleRate;
    const float decay = std::clamp(settings.ampDecayMs, 0.0f, 10000.0f) * 0.001f * sampleRate;
    const float sustain = std::clamp(settings.ampSustain, 0.0f, 1.0f);
    if (attack > 0.0f && time < attack)
        return std::sin(0.5f * kPi * time / attack);
    if (decay > 0.0f && time < attack + decay) {
        const float phase = (time - attack) / decay;
        const float smooth = phase * phase * (3.0f - 2.0f * phase);
        return 1.0f + (sustain - 1.0f) * smooth;
    }
    return sustain;
}

constexpr std::array<int, 8> kFdnBaseDelays{ 907, 1153, 1399, 1697, 1987, 2293, 2609, 2953 };
constexpr std::array<int, 8> kFdnAllpassDelays{ 23, 29, 37, 43, 53, 61, 71, 83 };
constexpr std::array<double, 8> kFdnLfoRatesHz{ 0.17, 0.23, 0.31, 0.41, 0.53, 0.67, 0.79, 0.97 };
constexpr std::array<int, 8> kWireBaseDelaysL{ 29, 37, 53, 71, 97, 127, 163, 211 };
constexpr std::array<int, 8> kWireBaseDelaysR{ 31, 41, 59, 73, 101, 131, 167, 223 };

} // namespace

float GatedStereoReverb::readHermite(const std::vector<float>& buffer, double delaySamples, int writePos) noexcept {
    if (buffer.size() < 4)
        return 0.0f;
    const double size = static_cast<double>(buffer.size());
    double position = static_cast<double>(writePos) - delaySamples;
    position = std::fmod(position, size);
    if (position < 0.0)
        position += size;
    const int index = static_cast<int>(position);
    const float frac = static_cast<float>(position - static_cast<double>(index));
    const int bufLen = static_cast<int>(buffer.size());
    const float y0 = buffer[static_cast<size_t>((index - 1 + bufLen) % bufLen)];
    const float y1 = buffer[static_cast<size_t>(index)];
    const float y2 = buffer[static_cast<size_t>((index + 1) % bufLen)];
    const float y3 = buffer[static_cast<size_t>((index + 2) % bufLen)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

void GatedStereoReverb::prepare(double sampleRate) {
    const double rate = std::clamp(sampleRate, 8000.0, 384000.0) / 48000.0;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumLines), [&](size_t idx) {
        const auto delayLen = static_cast<size_t>(std::max(32, static_cast<int>(std::ceil(kFdnBaseDelays[idx] * rate)) + 48));
        const auto apLen = static_cast<size_t>(std::max(8, static_cast<int>(std::ceil(kFdnAllpassDelays[idx] * rate))));
        fdnDelay_[idx].assign(delayLen, 0.0f);
        lineAllpass_[idx].assign(apLen, 0.0f);
    });
    diffL1_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(149 * rate))), 0.62f);
    diffL2_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(379 * rate))), 0.56f);
    diffL3_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(541 * rate))), 0.51f);
    diffR1_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(113 * rate))), 0.62f);
    diffR2_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(277 * rate))), 0.56f);
    diffR3_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(467 * rate))), 0.51f);
    reset();
}

void GatedStereoReverb::reset() noexcept {
    diffL1_.reset();
    diffL2_.reset();
    diffL3_.reset();
    diffR1_.reset();
    diffR2_.reset();
    diffR3_.reset();
    std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumLines), [&](size_t idx) {
        std::fill(fdnDelay_[idx].begin(), fdnDelay_[idx].end(), 0.0f);
        std::fill(lineAllpass_[idx].begin(), lineAllpass_[idx].end(), 0.0f);
        fdnWrite_[idx] = 0;
        lineApWrite_[idx] = 0;
        xoverLowState_[idx] = 0.0f;
        xoverHighState_[idx] = 0.0f;
        lfoPhase_[idx] = static_cast<double>(idx) * (std::numbers::pi / 4.0);
    });
    hpfStateL_ = 0.0f;
    hpfStateR_ = 0.0f;
    sampleCounter_ = 0;
    gateEnvelope_ = 1.0f;
}

StereoFrame GatedStereoReverb::process(float inLeft, float inRight, float size, float decayMs,
                                       float damp, float gateMs, double sampleRate) noexcept {
    if (fdnDelay_[0].empty() || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return { 0.0f, 0.0f };

    ++sampleCounter_;
    gateEnvelope_ = 1.0f;
    if (gateMs > 0.0f) {
        const float gateSamples = gateMs * 0.001f * static_cast<float>(sampleRate);
        const float releaseSamples = 0.014f * static_cast<float>(sampleRate);
        if (static_cast<float>(sampleCounter_) >= gateSamples) {
            const float phase = (static_cast<float>(sampleCounter_) - gateSamples) / std::max(1.0f, releaseSamples);
            gateEnvelope_ = phase >= 1.0f ? 0.0f : 0.5f * (1.0f + std::cos(kPi * phase));
        }
    }
    if (gateEnvelope_ <= 0.0f)
        return { 0.0f, 0.0f };

    const float sr = static_cast<float>(sampleRate);
    const float hpfA = std::exp(-kTwoPi * 95.0f / sr);
    hpfStateL_ = hpfA * hpfStateL_ + (1.0f - hpfA) * inLeft;
    hpfStateR_ = hpfA * hpfStateR_ + (1.0f - hpfA) * inRight;
    const float diffL = diffL3_.process(diffL2_.process(diffL1_.process(inLeft - hpfStateL_)));
    const float diffR = diffR3_.process(diffR2_.process(diffR1_.process(inRight - hpfStateR_)));

    const float room = std::clamp(size, 0.1f, 1.0f);
    const float roomScale = 0.58f + 0.42f * room;
    const float rt60Mid = std::clamp(decayMs, 50.0f, 2500.0f) * 0.001f * (0.55f + 0.65f * room);
    const float rt60Low = std::max(0.035f, rt60Mid * (0.38f + 0.14f * room));
    const float damping = std::clamp(damp, 0.0f, 1.0f);
    const float rt60High = std::max(0.025f, rt60Mid * std::lerp(0.88f, 0.14f, damping));

    // Phase-coherent 1-pole TPT crossovers split each FDN line into LF, Mid, and HF bands.
    const float lowG = std::clamp(kPi * 300.0f / sr, 0.001f, 0.45f);
    const float lowAlpha = lowG / (1.0f + lowG);
    const float highG = std::clamp(kPi * 3200.0f / sr, 0.01f, 0.75f);
    const float highAlpha = highG / (1.0f + highG);

    std::array<float, kNumLines> lineReads{};
    const double rateRatio = sampleRate / 48000.0;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumLines), [&](size_t idx) {
        lfoPhase_[idx] = std::fmod(lfoPhase_[idx] + 2.0 * std::numbers::pi * kFdnLfoRatesHz[idx] / sampleRate,
            2.0 * std::numbers::pi);
        const double lfoVal = std::sin(lfoPhase_[idx]);
        const double modSamples = lfoVal * (0.00028 * sampleRate);
        const double delayLen = static_cast<double>(kFdnBaseDelays[idx]) * rateRatio * static_cast<double>(roomScale) + modSamples;
        const float rawDelayed = readHermite(fdnDelay_[idx], std::max(4.0, delayLen), fdnWrite_[idx]);

        // Incommensurate modulated in-loop allpass smears metallic eigenmodes on long tails.
        auto& apBuf = lineAllpass_[idx];
        const float apFeedback = 0.36f + 0.09f * static_cast<float>(lfoVal);
        const float apDelayed = apBuf[lineApWrite_[idx]];
        const float apState = rawDelayed + apFeedback * apDelayed;
        const float apOut = -apFeedback * apState + apDelayed;
        apBuf[lineApWrite_[idx]] = apState;
        lineApWrite_[idx] = (lineApWrite_[idx] + 1) % apBuf.size();
        lineReads[idx] = apOut;
    });

    // Orthogonal 8x8 Householder reflection H = I - (2/8) * 1*1^T mixes all 8 lines losslessly.
    const float householderSum = std::accumulate(lineReads.begin(), lineReads.end(), 0.0f) * 0.25f;
    float wetL = 0.0f;
    float wetR = 0.0f;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumLines), [&](size_t idx) {
        const float mixed = lineReads[idx] - householderSum;
        const float lineSec = static_cast<float>(static_cast<double>(kFdnBaseDelays[idx]) * rateRatio * roomScale / sampleRate);
        const float gainLow = std::min(0.965f, std::pow(10.0f, -3.0f * lineSec / rt60Low));
        const float gainMid = std::min(0.965f, std::pow(10.0f, -3.0f * lineSec / rt60Mid));
        const float gainHigh = std::min(0.965f, std::pow(10.0f, -3.0f * lineSec / rt60High));

        const float vLow = (mixed - xoverLowState_[idx]) * lowAlpha;
        const float lpLow = vLow + xoverLowState_[idx];
        xoverLowState_[idx] = lpLow + vLow;

        const float vHigh = (mixed - xoverHighState_[idx]) * highAlpha;
        const float lpHigh = vHigh + xoverHighState_[idx];
        xoverHighState_[idx] = lpHigh + vHigh;

        const float bandLow = lpLow;
        const float bandMid = lpHigh - lpLow;
        const float bandHigh = mixed - lpHigh;
        const float decayed = bandLow * gainLow + bandMid * gainMid + bandHigh * gainHigh;

        const bool evenLine = (idx & 1U) == 0U;
        const float inputDrive = evenLine ? (diffL * 0.85f + diffR * 0.15f) : (diffR * 0.85f + diffL * 0.15f);
        const float sign = (idx & 2U) == 0U ? 1.0f : -1.0f;
        fdnDelay_[idx][static_cast<size_t>(fdnWrite_[idx])] = inputDrive * sign + decayed;
        fdnWrite_[idx] = (fdnWrite_[idx] + 1) % static_cast<int>(fdnDelay_[idx].size());

        if (evenLine)
            wetL += mixed * sign * 0.24f;
        if (!evenLine)
            wetR += mixed * sign * 0.24f;
    });

    const float stereoL = wetL * 0.88f + wetR * 0.12f;
    const float stereoR = wetR * 0.88f + wetL * 0.12f;
    return { stereoL * gateEnvelope_, stereoR * gateEnvelope_ };
}

void SmackSynth::prepare(double sampleRate) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    filterSmoothAlpha_ = 1.0f - std::exp(-1.0f / (0.0045f * static_cast<float>(sampleRate_)));
    const double rate = sampleRate_ / 48000.0;
    const auto combCap = static_cast<size_t>(std::max(32, static_cast<int>(std::ceil(320.0 * rate))));
    std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumWireCombs), [&](size_t idx) {
        wireCombL_[idx].assign(combCap, 0.0f);
        wireCombR_[idx].assign(combCap, 0.0f);
    });
    reverb_.prepare(sampleRate_);
    reset();
}

void SmackSynth::reset() noexcept {
    isPlaying_ = false;
    totalSamplesPlayed_ = 0;
    totalDurationSamples_ = 0;
    dryDurationSamples_ = 0;
    fullAdsrEnabled_ = false;
    noteOffReceived_ = false;
    activeNoteNumber_ = -1;
    activeChannel_ = 1;
    releaseStartSample_ = -1;
    releaseDurationSamples_ = 0;
    reverbTailSamples_ = 0;
    releaseStartLevel_ = 0.0f;
    phaseFund_ = phaseSub_ = phaseOctave_ = 0.0f;
    phaseMod1_ = phaseMod2_ = phaseFundR_ = 0.0f;
    std::ranges::for_each(shellModesL_, [](ModalResonator2Pole& mode) { mode.reset(); });
    std::ranges::for_each(shellModesR_, [](ModalResonator2Pole& mode) { mode.reset(); });
    std::ranges::for_each(rimModes_, [](ModalResonator2Pole& mode) { mode.reset(); });
    metalPhasesL_.fill(0.0f);
    metalPhasesR_.fill(0.0f);
    clickPhase_ = clickPhase2_ = 0.0f;

    bodyDownL_.reset();
    bodyDownR_.reset();
    rawModalDownL_.reset();
    rawModalDownR_.reset();
    transientDown_.reset();

    shellFilterL_.reset();
    shellFilterR_.reset();
    shellSatL_.reset();
    shellSatR_.reset();
    rimSat_.reset();

    noiseHPF_L_.reset();
    noiseHPF_R_.reset();
    noiseLPF_L_.reset();
    noiseLPF_R_.reset();
    bodyHPF_L_.reset();
    bodyHPF_R_.reset();
    bodyLPF_L_.reset();
    bodyLPF_R_.reset();
    transientHPF_L_.reset();
    transientHPF_R_.reset();
    transientLPF_L_.reset();
    transientLPF_R_.reset();
    tomVelvetFilterL_.reset();
    tomVelvetFilterR_.reset();

    std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumWireCombs), [&](size_t idx) {
        std::fill(wireCombL_[idx].begin(), wireCombL_[idx].end(), 0.0f);
        std::fill(wireCombR_[idx].begin(), wireCombR_[idx].end(), 0.0f);
        wireWriteL_[idx] = 0;
        wireWriteR_[idx] = 0;
        wireDispStateL_[idx] = 0.0f;
        wireDispStateR_[idx] = 0.0f;
        wireDampStateL_[idx] = 0.0f;
        wireDampStateR_[idx] = 0.0f;
    });

    prngStateL_ = 0x2468ACE1U;
    prngStateR_ = 0x1357BDF0U;
    tomVelvetPrng_ = 0x9E3779B9U;
    tomVelvetCounter_ = 0;
    tomVelvetNextPulse_ = 4;
    tomVelvetPulseSign_ = 1.0f;
    tomVelvetHistoryL_.fill(0.0f);
    tomVelvetHistoryR_.fill(0.0f);
    wireStateL_ = wireStateR_ = velvetStateL_ = velvetStateR_ = 0.0f;
    reverb_.reset();
}

void SmackSynth::trigger(const SmackSynthSettings& settings, float velocity, int semitoneOffset,
                         int noteNumber, int channel) noexcept {
    reset();
    isPlaying_ = true;
    const float normalizedVelocity = std::clamp(std::isfinite(velocity) ? velocity : 1.0f, 0.0f, 1.0f);
    bodyVelocity_ = std::lerp(1.0f, normalizedVelocity, std::clamp(settings.bodyVelocityDepth, 0.0f, 1.0f));
    constexpr std::array<float, 3> originalRatios{ 1.593f, 2.136f, 2.877f };
    constexpr std::array<std::array<float, 3>, 4> struckModes{{
        { 1.62f, 2.42f, 3.14f }, { 1.51f, 2.02f, 2.74f },
        { 2.00f, 2.71f, 4.09f }, { 1.86f, 2.85f, 4.18f }
    }};
    baseModeRatios_ = settings.articulatedModes ? struckModes[static_cast<size_t>(settings.voiceMode)] : originalRatios;
    strikeBrightness_ = std::lerp(0.38f, 1.0f, bodyVelocity_);
    wireVelocity_ = std::lerp(1.0f, normalizedVelocity, std::clamp(settings.wireVelocityDepth, 0.0f, 1.0f));
    transientVelocity_ = std::lerp(1.0f, normalizedVelocity, std::clamp(settings.transientVelocityDepth, 0.0f, 1.0f));
    semitoneOffset_ = std::clamp(semitoneOffset, -96, 96);
    fullAdsrEnabled_ = settings.fullAdsrEnabled;
    activeNoteNumber_ = noteNumber < 0 ? -1 : std::clamp(noteNumber, 0, 127);
    activeChannel_ = std::clamp(channel, 1, 16);
    preStrikeCount_ = std::clamp(settings.preStrikeCount, 1, 5);
    const float sr = static_cast<float>(sampleRate_);
    const float spread = std::clamp(settings.preStrikeSpreadMs, 2.0f, 45.0f) * 0.001f * sr;

    if (preStrikeCount_ == 1) {
        burstStartSamples_[0] = 0;
        burstAmplitudes_[0] = 0.0f;
        burstPans_[0] = 0.0f;
        mainSmackSample_ = 0;
    }
    if (preStrikeCount_ > 1) {
        std::ranges::for_each(std::views::iota(0, preStrikeCount_), [&](int idx) {
            const float position = static_cast<float>(idx + 1);
            const float jitter = nextRandom(prngStateL_) * 0.22f * std::clamp(settings.preStrikeJitter, 0.0f, 1.0f) * spread;
            burstStartSamples_[static_cast<size_t>(idx)] = std::max(0, static_cast<int>(position * spread + jitter));
            const float rise = position / static_cast<float>(preStrikeCount_);
            burstAmplitudes_[static_cast<size_t>(idx)] = std::clamp(settings.preStrikeLevel * (0.25f + 0.75f * rise), 0.0f, 1.0f);
            const float direction = (idx & 1) == 0 ? -1.0f : 1.0f;
            burstPans_[static_cast<size_t>(idx)] = direction * std::clamp(settings.preStrikeFlamSpread, 0.0f, 1.0f) * rise;
        });
        mainSmackSample_ = static_cast<int>(std::ceil(static_cast<float>(preStrikeCount_) * spread));
    }

    const float preMs = preStrikeCount_ > 1 ? settings.preStrikeSpreadMs * static_cast<float>(preStrikeCount_) : 0.0f;
    const float bodyMs = std::clamp(settings.toneDecayMs, 15.0f, 1200.0f);
    const float wireMs = std::clamp(settings.noiseAttackMs, 0.0f, 20.0f)
        + std::clamp(settings.noiseHoldMs, 0.0f, 30.0f)
        + std::clamp(settings.noiseDecayMs, 15.0f, 1200.0f);
    const float dryMs = preMs + std::max({ 32.0f, bodyMs, wireMs });
    dryDurationSamples_ = std::max(1, static_cast<int>(dryMs * 0.001f * sr));
    const float reverbTailMs = settings.reverbMix > 0.001f
        ? std::max(settings.reverbGateMs, std::min(settings.reverbDecayMs * 0.60f, 900.0f)) : 0.0f;
    totalDurationSamples_ = dryDurationSamples_ + static_cast<int>(reverbTailMs * 0.001f * sr);
    reverbTailSamples_ = static_cast<int>(reverbTailMs * 0.001f * sr);
}

void SmackSynth::noteOff(int noteNumber, int channel, const SmackSynthSettings& settings) noexcept {
    if (!isPlaying_ || !fullAdsrEnabled_ || noteOffReceived_
        || activeNoteNumber_ != std::clamp(noteNumber, 0, 127)
        || activeChannel_ != std::clamp(channel, 1, 16))
        return;

    const int sample = totalSamplesPlayed_;
    releaseStartLevel_ = gatedEnvelope(sample, settings, static_cast<float>(sampleRate_));
    releaseStartSample_ = sample;
    const float releaseMs = std::clamp(settings.ampReleaseMs, 0.0f, 20000.0f);
    releaseDurationSamples_ = static_cast<int>(releaseMs * 0.001f * static_cast<float>(sampleRate_));
    noteOffReceived_ = true;
    if (releaseDurationSamples_ <= 0 && reverbTailSamples_ <= 0)
        isPlaying_ = false;
}

StereoFrame SmackSynth::generateStereoNoise(
    NoiseType type, float tunedHz, float overtoneEnv, float wireTension, float bottomHeadModal) noexcept {
    const float whiteL = nextRandom(prngStateL_);
    const float whiteR = nextRandom(prngStateR_);
    switch (type) {
        case NoiseType::SnareWires: {
            if (wireCombL_[0].empty())
                return { whiteL * 0.7f, whiteR * 0.7f };

            const float tension = std::clamp(wireTension, 0.0f, 1.0f);
            const float lengthScale = 1.32f - 0.56f * tension;
            const float rateScale = static_cast<float>(sampleRate_ / 48000.0) * lengthScale;
            const float headMag = std::min(1.5f, std::abs(bottomHeadModal));
            const float penetration = std::max(0.0f, bottomHeadModal);

            // Bottom-head displacement nonlinearly drives wire rattle and modulates strand tension.
            const float rattleDrive = 0.88f + 0.52f * std::tanh(1.9f * headMag);
            const float buzzL = whiteL * rattleDrive + 0.28f * penetration * whiteL * std::abs(whiteL);
            const float buzzR = whiteR * rattleDrive + 0.28f * penetration * whiteR * std::abs(whiteR);
            const float baseFeedback = 0.38f + 0.42f * tension;
            const float effFeedback = std::clamp(baseFeedback + 0.08f * penetration * (1.0f - 0.5f * tension), 0.22f, 0.84f);
            const float dispCoeff = -0.52f + 0.38f * tension;
            const float dampSmooth = 0.18f + 0.24f * (1.0f - tension);

            float combSumL = 0.0f;
            float combSumR = 0.0f;
            std::ranges::for_each(std::views::iota(size_t{ 0 }, kNumWireCombs), [&](size_t idx) {
                auto& bufL = wireCombL_[idx];
                auto& bufR = wireCombR_[idx];
                const int bufCapL = static_cast<int>(bufL.size());
                const int bufCapR = static_cast<int>(bufR.size());
                const int delayL = std::clamp(static_cast<int>(std::round(kWireBaseDelaysL[idx] * rateScale)), 4, bufCapL - 2);
                const int delayR = std::clamp(static_cast<int>(std::round(kWireBaseDelaysR[idx] * rateScale)), 4, bufCapR - 2);

                const float readL = bufL[static_cast<size_t>((wireWriteL_[idx] - delayL + bufCapL) % bufCapL)];
                const float dispL = dispCoeff * readL + wireDispStateL_[idx];
                wireDispStateL_[idx] = readL - dispCoeff * dispL;
                wireDampStateL_[idx] += (dispL - wireDampStateL_[idx]) * (1.0f - dampSmooth);
                bufL[static_cast<size_t>(wireWriteL_[idx])] = buzzL * 0.35f + wireDampStateL_[idx] * effFeedback;
                wireWriteL_[idx] = (wireWriteL_[idx] + 1) % bufCapL;
                combSumL += dispL;

                const float readR = bufR[static_cast<size_t>((wireWriteR_[idx] - delayR + bufCapR) % bufCapR)];
                const float dispR = dispCoeff * readR + wireDispStateR_[idx];
                wireDispStateR_[idx] = readR - dispCoeff * dispR;
                wireDampStateR_[idx] += (dispR - wireDampStateR_[idx]) * (1.0f - dampSmooth);
                bufR[static_cast<size_t>(wireWriteR_[idx])] = buzzR * 0.35f + wireDampStateR_[idx] * effFeedback;
                wireWriteR_[idx] = (wireWriteR_[idx] + 1) % bufCapR;
                combSumR += dispR;
            });

            wireStateL_ = 0.65f * wireStateL_ + 0.35f * buzzL;
            wireStateR_ = 0.65f * wireStateR_ + 0.35f * buzzR;
            const float crispL = buzzL - wireStateL_;
            const float crispR = buzzR - wireStateR_;
            return { combSumL * 0.39f + crispL * 0.85f, combSumR * 0.39f + crispR * 0.85f };
        }
        case NoiseType::WhiteSizzle: {
            const float tension = std::clamp(wireTension, 0.0f, 1.0f);
            const float rawSizzleL = std::tanh(whiteL * (1.25f + 0.35f * tension));
            const float rawSizzleR = std::tanh(whiteR * (1.25f + 0.35f * tension));
            if (wireCombL_[0].empty())
                return { rawSizzleL, rawSizzleR };
            auto& bufL = wireCombL_[0];
            auto& bufR = wireCombR_[0];
            const int capL = static_cast<int>(bufL.size());
            const int capR = static_cast<int>(bufR.size());
            const float targetHz = std::clamp(tunedHz * (2.1f + 2.4f * tension), 240.0f, 5200.0f);
            const int delayL = std::clamp(static_cast<int>(std::round(sampleRate_ / targetHz)), 4, capL - 2);
            const int delayR = std::clamp(delayL + 3, 4, capR - 2);
            const float fb = 0.22f + 0.24f * tension;
            const float readL = bufL[static_cast<size_t>((wireWriteL_[0] - delayL + capL) % capL)];
            const float readR = bufR[static_cast<size_t>((wireWriteR_[0] - delayR + capR) % capR)];
            bufL[static_cast<size_t>(wireWriteL_[0])] = rawSizzleL + readL * fb;
            bufR[static_cast<size_t>(wireWriteR_[0])] = rawSizzleR + readR * fb;
            wireWriteL_[0] = (wireWriteL_[0] + 1) % capL;
            wireWriteR_[0] = (wireWriteR_[0] + 1) % capR;
            return { rawSizzleL * 0.78f + readL * 0.30f, rawSizzleR * 0.78f + readR * 0.30f };
        }
        case NoiseType::VelvetAir: {
            const float tension = std::clamp(wireTension, 0.0f, 1.0f);
            const float pole = 0.80f - 0.18f * tension;
            const float pulseL = std::abs(whiteL) > 0.60f ? std::copysign(1.0f, whiteL) : 0.0f;
            const float pulseR = std::abs(whiteR) > 0.60f ? std::copysign(1.0f, whiteR) : 0.0f;
            velvetStateL_ = pole * velvetStateL_ + (1.0f - pole) * pulseL;
            velvetStateR_ = pole * velvetStateR_ + (1.0f - pole) * pulseR;
            return { velvetStateL_ * 1.38f, velvetStateR_ * 1.38f };
        }
        case NoiseType::MetallicRing: {
            constexpr std::array<float, 4> ratios{ 1.414f, 2.236f, 3.146f, 4.732f };
            float ringL = 0.0f;
            float ringR = 0.0f;
            const float nyquist = static_cast<float>(sampleRate_) * 0.44f;
            std::ranges::for_each(std::views::iota(size_t{ 0 }, ratios.size()), [&](size_t idx) {
                const float detune = 1.0f + ((idx % 2 == 0) ? 0.0013f : -0.0017f);
                const float frequency = std::clamp(tunedHz * ratios[idx], 160.0f, nyquist);
                metalPhasesL_[idx] = wrapPhase(metalPhasesL_[idx] + kTwoPi * frequency / static_cast<float>(sampleRate_));
                metalPhasesR_[idx] = wrapPhase(metalPhasesR_[idx] + kTwoPi * frequency * detune / static_cast<float>(sampleRate_));
                const float partialGain = (idx == 0 ? 0.38f : 0.22f) * std::exp(-static_cast<float>(idx) * 0.25f);
                ringL += std::sin(metalPhasesL_[idx]) * partialGain;
                ringR += std::sin(metalPhasesR_[idx]) * partialGain;
            });
            const float grit = 0.16f + 0.20f * overtoneEnv;
            return { ringL + whiteL * std::abs(ringL) * grit, ringR + whiteR * std::abs(ringR) * grit };
        }
    }
    return { whiteL, whiteR };
}

float SmackSynth::getEnvelopeProgress() const noexcept {
    if (!isPlaying_ || dryDurationSamples_ <= 0)
        return 1.0f;
    return std::clamp(static_cast<float>(totalSamplesPlayed_) / static_cast<float>(dryDurationSamples_), 0.0f, 1.0f);
}

StereoFrame SmackSynth::processSample(const SmackSynthSettings& settings) noexcept {
    if (!isPlaying_)
        return { 0.0f, 0.0f };
    const int sample = totalSamplesPlayed_++;
    const int gateReleaseSamples = noteOffReceived_ ? releaseDurationSamples_ : 0;
    if ((!fullAdsrEnabled_ && sample >= totalDurationSamples_)
        || (fullAdsrEnabled_ && noteOffReceived_
            && sample >= releaseStartSample_ + gateReleaseSamples + reverbTailSamples_)) {
        isPlaying_ = false;
        return { 0.0f, 0.0f };
    }

    const float sr = static_cast<float>(sampleRate_);
    const float dt = 1.0f / sr;
    const float maxHz = sr * 0.44f;
    const float rootHz = noteToHz(settings.note, settings.octave, settings.fineCents);

    float amp = 0.0f;
    if (fullAdsrEnabled_)
        amp = gatedEnvelope(sample, settings, sr);
    if (!fullAdsrEnabled_ && settings.shapedEngine)
        amp = shapedOneShot(sample, dryDurationSamples_, settings, sr);
    if (!fullAdsrEnabled_ && !settings.shapedEngine)
        amp = amplitudeEnvelope(sample, dryDurationSamples_, settings, sr);
    if (fullAdsrEnabled_ && noteOffReceived_) {
        const bool finished = gateReleaseSamples <= 0 || sample >= releaseStartSample_ + gateReleaseSamples;
        if (finished)
            amp = 0.0f;
        if (!finished) {
            const float phase = static_cast<float>(sample - releaseStartSample_) / static_cast<float>(gateReleaseSamples);
            amp = releaseStartLevel_ * 0.5f * (1.0f + std::cos(kPi * phase));
        }
    }

    const int mainAge = sample - mainSmackSample_;
    const float bodyDecay = std::max(1.0f, std::clamp(settings.toneDecayMs, 15.0f, 1200.0f) * 0.001f * sr);

    // Fast stick-head snap Tau plus slow body Tau captures transient tension and drumhead settle.
    const float pitchDecaySec = std::clamp(settings.tonePitchDecayMs, 3.0f, 150.0f) * 0.001f;
    const float snapTau = std::clamp(pitchDecaySec * 0.07f, 0.0018f, 0.0035f);
    const float bodyTau = std::clamp(pitchDecaySec * 1.15f, 0.005f, 0.140f);
    const auto evalPitchSemitones = [&](float ageSec) {
        const float pitchFall = 0.58f * std::exp(-ageSec / snapTau) + 0.42f * std::exp(-ageSec / bodyTau);
        return std::clamp(static_cast<float>(semitoneOffset_) + settings.tonePitchEnv * pitchFall, -48.0f, 48.0f);
    };

    const float mainTime = static_cast<float>(std::max(0, mainAge)) * dt;
    const float hostPitchSemitones = evalPitchSemitones(mainTime);
    const float tunedHz = std::clamp(rootHz * std::exp2(hostPitchSemitones / 12.0f), 25.0f, maxHz);

    float bodyEnv = 0.0f;
    if (mainAge >= 0 && fullAdsrEnabled_)
        bodyEnv = 1.0f;
    if (mainAge >= 0 && !fullAdsrEnabled_)
        bodyEnv = std::exp(-static_cast<float>(mainAge) / bodyDecay);
    float struckOvertones = 0.0f;
    if (mainAge >= 0)
        struckOvertones = std::exp(-static_cast<float>(mainAge) / std::max(1.0f, bodyDecay * 0.34f));
    const float overtoneEnv = fullAdsrEnabled_ && mainAge >= 0 ? 1.0f : struckOvertones;
    const float fmEnvelope = settings.articulatedModes ? struckOvertones * strikeBrightness_ : overtoneEnv;

    const bool bodyActive = mainAge >= 0 && settings.toneLevel > 0.0001f;
    const float sr4 = sr * 4.0f;
    const float dt4 = 0.25f * dt;
    const float maxHz4 = sr4 * 0.45f;
    const float stiffnessB = std::clamp(settings.shellSpread, -0.5f, 0.5f) * 0.18f;
    const auto profile = shapedProfile(settings.voiceMode);
    float fundTau = bodyDecay * profile.fundScale;
    // Snares and toms keep a body ring. Rims stay a short knock.
    float maxFundSec = 0.16f;
    if (profile.octave)
        maxFundSec = 0.28f;
    if (settings.voiceMode == VoiceMode::PrydaRimSmack)
        maxFundSec = 0.055f;
    if (!profile.bell)
        fundTau = std::min(fundTau, maxFundSec * sr);
    float fundEnv = 1.0f;
    if (!fullAdsrEnabled_)
        fundEnv = std::exp(-static_cast<float>(std::max(0, mainAge)) / std::max(1.0f, fundTau));

    float tomVelvetAirL = 0.0f;
    float tomVelvetAirR = 0.0f;
    if (bodyActive && settings.shapedEngine && profile.octave) {
        // Sparse binomial velvet pulses prevent static hash buzz on clean low-velocity toms.
        const int gridSamples = std::max(4, static_cast<int>(std::round(sr / 2600.0f)));
        float rawImpulseL = 0.0f;
        float rawImpulseR = 0.0f;
        if (tomVelvetCounter_ >= tomVelvetNextPulse_) {
            rawImpulseL = tomVelvetPulseSign_;
            const bool samePolarityR = nextRandom(tomVelvetPrng_) >= 0.0f;
            rawImpulseR = samePolarityR ? tomVelvetPulseSign_ : -tomVelvetPulseSign_;
            const float jitter = 0.5f * (nextRandom(tomVelvetPrng_) + 1.0f);
            tomVelvetNextPulse_ = tomVelvetCounter_ + std::max(2, static_cast<int>(
                static_cast<float>(gridSamples) * (0.45f + 1.10f * jitter)));
            const bool nextPositive = nextRandom(tomVelvetPrng_) >= 0.0f;
            tomVelvetPulseSign_ = nextPositive ? 1.0f : -1.0f;
        }
        ++tomVelvetCounter_;
        const float shapedPulseL = 0.25f * rawImpulseL + 0.5f * tomVelvetHistoryL_[0] + 0.25f * tomVelvetHistoryL_[1];
        const float shapedPulseR = 0.25f * rawImpulseR + 0.5f * tomVelvetHistoryR_[0] + 0.25f * tomVelvetHistoryR_[1];
        tomVelvetHistoryL_[1] = tomVelvetHistoryL_[0];
        tomVelvetHistoryL_[0] = rawImpulseL;
        tomVelvetHistoryR_[1] = tomVelvetHistoryR_[0];
        tomVelvetHistoryR_[0] = rawImpulseR;

        const float velvetEnv = fullAdsrEnabled_ ? struckOvertones : fundEnv;
        const float brightSq = strikeBrightness_ * strikeBrightness_;
        const float velvetCutoff = std::clamp(
            1800.0f + (600.0f + 2500.0f * brightSq) * (0.45f + 0.55f * velvetEnv),
            1600.0f, 5200.0f);
        tomVelvetFilterL_.updateCoefficients(velvetCutoff, kButterworthQ, sampleRate_, 1.0f);
        tomVelvetFilterR_.updateCoefficients(velvetCutoff, kButterworthQ, sampleRate_, 1.0f);
        const float velvetGain = std::pow(std::max(0.0f, velvetEnv), 0.72f) * (0.16f + 0.36f * brightSq);
        tomVelvetAirL = tomVelvetFilterL_.processLowpass(shapedPulseL) * velvetGain;
        tomVelvetAirR = tomVelvetFilterR_.processLowpass(shapedPulseR) * velvetGain;
    }

    std::array<float, 4> subOscL{};
    std::array<float, 4> subOscR{};
    float bottomHeadModal = 0.0f;
    if (bodyActive) {
        constexpr std::array<float, 3> legacyScales{ 0.48f, 0.25f, 0.15f };
        constexpr std::array<float, 3> legacyGains{ 0.48f, 0.28f, 0.16f };
        constexpr std::array<float, 3> kModeDetuneR{ 1.0011f, 0.9991f, 1.0017f };
        const auto& modeScales = settings.shapedEngine ? profile.modeScales : legacyScales;
        const auto& modeGains = settings.shapedEngine ? profile.modeGains : legacyGains;
        std::array<float, 3> modeRadii{};
        std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 3 }), [&](size_t modeIdx) {
            const float tauSamples4 = 4.0f * std::max(1.0f, bodyDecay * modeScales[modeIdx]);
            modeRadii[modeIdx] = std::exp(-1.0f / tauSamples4);
        });

        const float beaterContactSamples4 = std::clamp(std::lerp(12.0f, 5.0f, strikeBrightness_) * (sr / 48000.0f), 4.0f, 32.0f);
        const float beaterAmp = 0.65f + 0.55f * strikeBrightness_;
        const float fmAmountClamped = std::clamp(settings.fmAmount, 0.0f, 1.0f);

        std::ranges::for_each(std::views::iota(0, 4), [&](int step) {
            const float subAge4 = static_cast<float>(mainAge * 4 + step);
            const float subTimeSec = subAge4 * dt4;
            const float subSemitones = evalPitchSemitones(subTimeSec);
            const float subTunedHz = std::clamp(rootHz * std::exp2(subSemitones / 12.0f), 25.0f, maxHz4);
            const float subModHz = std::min(maxHz4, subTunedHz * (1.37f + fmAmountClamped * 1.8f));
            const float subMod2Hz = std::min(maxHz4, subTunedHz * 3.17f);

            phaseFund_ = wrapPhase(phaseFund_ + kTwoPi * subTunedHz * dt4);
            phaseFundR_ = wrapPhase(phaseFundR_ + kTwoPi * subTunedHz * 1.0007f * dt4);
            phaseMod1_ = wrapPhase(phaseMod1_ + kTwoPi * subModHz * dt4);
            phaseMod2_ = wrapPhase(phaseMod2_ + kTwoPi * subMod2Hz * dt4);

            const float contactPhase = subAge4 / beaterContactSamples4;
            float beaterForce = 0.0f;
            if (contactPhase < 1.0f)
                beaterForce = ((1.0f - std::cos(kTwoPi * contactPhase)) / beaterContactSamples4) * beaterAmp;

            float modalSumL = 0.0f;
            float modalSumR = 0.0f;
            float mode3Out = 0.0f;
            std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 3 }), [&](size_t modeIdx) {
                const float modeOrder = static_cast<float>(modeIdx + 1);
                const float dispersion = std::sqrt(std::max(0.15f, 1.0f + stiffnessB * modeOrder * modeOrder));
                const float freqL = std::min(maxHz4, subTunedHz * baseModeRatios_[modeIdx] * dispersion);
                const float freqR = std::min(maxHz4, freqL * kModeDetuneR[modeIdx]);
                const float omegaL = kTwoPi * freqL * dt4;
                const float omegaR = kTwoPi * freqR * dt4;
                const float resL = shellModesL_[modeIdx].step(beaterForce, modeRadii[modeIdx], std::cos(omegaL), std::sin(omegaL));
                const float resR = shellModesR_[modeIdx].step(beaterForce, modeRadii[modeIdx], std::cos(omegaR), std::sin(omegaR));
                const float modeSign = modeIdx == 1 ? -1.0f : 1.0f;
                const float rightTap = modeIdx == 1 ? shellModesR_[modeIdx].stateU : resR;
                modalSumL += modeSign * modeGains[modeIdx] * resL;
                modalSumR += modeSign * modeGains[modeIdx] * rightTap;
                if (modeIdx == 1)
                    mode3Out = resL;
            });

            float rawL = 0.0f;
            float rawR = 0.0f;
            if (settings.shapedEngine) {
                const float fmIndex = profile.bell
                    ? (0.55f + 2.4f * fmAmountClamped) * fmEnvelope
                    : fmAmountClamped * 2.2f * fmEnvelope * profile.fmScale;
                const float phaseOffsetR = profile.bell ? 0.11f : 0.07f;
                rawL = std::sin(phaseFund_ + fmIndex * std::sin(phaseMod1_)) * fundEnv * profile.fundGain + modalSumL;
                rawR = std::sin(phaseFundR_ + fmIndex * std::sin(phaseMod1_ + phaseOffsetR)) * fundEnv * profile.fundGain + modalSumR;
                if (profile.octave) {
                    const float octaveHz = std::min(maxHz4, subTunedHz * 2.0f);
                    phaseOctave_ = wrapPhase(phaseOctave_ + kTwoPi * octaveHz * dt4);
                    float octaveEnv = 0.35f;
                    if (!fullAdsrEnabled_)
                        octaveEnv = std::exp(-static_cast<float>(mainAge) / std::max(1.0f, bodyDecay * profile.fundScale * 0.55f));
                    const float octaveSig = 0.24f * std::sin(phaseOctave_) * octaveEnv;
                    rawL += octaveSig + tomVelvetAirL;
                    rawR += octaveSig + tomVelvetAirR;
                }
            }
            if (!settings.shapedEngine) {
                const float fmIndex = fmAmountClamped * 2.6f * fmEnvelope;
                const float fundamentalL = std::sin(phaseFund_ + fmIndex * std::sin(phaseMod1_));
                const float fundamentalR = std::sin(phaseFundR_ + fmIndex * std::sin(phaseMod1_ + 0.07f));
                rawL = fundamentalL * bodyEnv + modalSumL * fmAmountClamped;
                rawR = fundamentalR * bodyEnv + modalSumR * fmAmountClamped;
                switch (settings.voiceMode) {
                    case VoiceMode::DanceSnare:
                        rawL = rawL * 0.58f + modalSumL * 0.62f;
                        rawR = rawR * 0.58f + modalSumR * 0.62f;
                        break;
                    case VoiceMode::ClubTom:
                        rawL = fundamentalL * bodyEnv * 1.08f + modalSumL * 0.52f;
                        rawR = fundamentalR * bodyEnv * 1.08f + modalSumR * 0.52f;
                        break;
                    case VoiceMode::FmBell: {
                        const float bellIdx = (0.7f + 3.8f * fmAmountClamped) * fmEnvelope;
                        const float bellL = std::sin(phaseFund_ + std::sin(phaseMod1_) * bellIdx);
                        const float bellR = std::sin(phaseFundR_ + std::sin(phaseMod1_ + 0.11f) * bellIdx);
                        const float chime = 0.45f * mode3Out + 0.24f * std::sin(phaseMod2_) * struckOvertones;
                        rawL = bellL * bodyEnv * 0.72f + chime;
                        rawR = bellR * bodyEnv * 0.72f + chime;
                        break;
                    }
                    case VoiceMode::PrydaRimSmack:
                        rawL = fundamentalL * bodyEnv * 0.46f + modalSumL * 0.75f;
                        rawR = fundamentalR * bodyEnv * 0.46f + modalSumR * 0.75f;
                        break;
                }
            }
            subOscL[static_cast<size_t>(step)] = rawL;
            subOscR[static_cast<size_t>(step)] = rawR;
        });

        const float decimatedModalL = rawModalDownL_.process(subOscL);
        const float decimatedModalR = rawModalDownR_.process(subOscR);
        bottomHeadModal = std::clamp(0.5f * (decimatedModalL + decimatedModalR) * settings.toneLevel, -1.5f, 1.5f);
    }

    float wireEnv = 0.0f;
    if (fullAdsrEnabled_ && mainAge >= 0)
        wireEnv = 1.0f;
    if (!fullAdsrEnabled_ && mainAge >= 0) {
        const float age = static_cast<float>(mainAge);
        const float attack = std::clamp(settings.noiseAttackMs, 0.0f, 20.0f) * 0.001f * sr;
        const float hold = std::clamp(settings.noiseHoldMs, 0.0f, 30.0f) * 0.001f * sr;
        const float decay = std::max(1.0f, std::clamp(settings.noiseDecayMs, 15.0f, 1200.0f) * 0.001f * sr);
        const bool inAttack = attack > 0.0f && age < attack;
        const bool inHold = !inAttack && age < attack + hold;
        if (inAttack)
            wireEnv = std::sin(0.5f * kPi * age / attack);
        if (inHold)
            wireEnv = 1.0f;
        if (!inAttack && !inHold) {
            const float phase = std::max(0.0f, (age - attack - hold) / decay);
            const float curved = std::exp(-phase * (2.4f + std::max(0.0f, -settings.noiseCurve) * 2.2f));
            const float straight = std::max(0.0f, 1.0f - phase);
            const float shape = std::clamp(settings.noiseCurve, -1.0f, 1.0f);
            wireEnv = shape < 0.0f ? curved : curved * (1.0f - shape) + straight * shape;
            if (!settings.shapedEngine && phase > 5.0f)
                wireEnv = 0.0f;
        }
    }

    const auto rawNoise = generateStereoNoise(
        settings.noiseType, tunedHz, overtoneEnv, settings.wireTension, bottomHeadModal);
    float noiseL = rawNoise.left * wireEnv * settings.noiseLevel;
    float noiseR = rawNoise.right * wireEnv * settings.noiseLevel;

    // Smoothed 12 dB/oct ZDF SVFs keep snare noise out of kick sub-bands.
    noiseHPF_L_.updateCoefficients(std::clamp(settings.noiseHPF, 20.0f, 2500.0f), kButterworthQ, sampleRate_, filterSmoothAlpha_);
    noiseHPF_R_.updateCoefficients(std::clamp(settings.noiseHPF, 20.0f, 2500.0f), kButterworthQ, sampleRate_, filterSmoothAlpha_);
    noiseLPF_L_.updateCoefficients(std::clamp(settings.noiseLPF, 800.0f, 20000.0f), kButterworthQ, sampleRate_, filterSmoothAlpha_);
    noiseLPF_R_.updateCoefficients(std::clamp(settings.noiseLPF, 800.0f, 20000.0f), kButterworthQ, sampleRate_, filterSmoothAlpha_);
    noiseL = noiseLPF_L_.processLowpass(noiseHPF_L_.processHighpass(noiseL));
    noiseR = noiseLPF_R_.processLowpass(noiseHPF_R_.processHighpass(noiseR));

    const float width = std::clamp(settings.noiseStereoWidth, 0.0f, 2.0f);
    const float noiseMid = 0.5f * (noiseL + noiseR);
    const float noiseSide = 0.5f * (noiseL - noiseR) * width;
    noiseL = noiseMid + noiseSide;
    noiseR = noiseMid - noiseSide;

    float bodyL = 0.0f;
    float bodyR = 0.0f;
    if (bodyActive) {
        const double sr4Double = sampleRate_ * 4.0;
        const float shellHz = std::clamp(settings.toneFreqHz * std::exp2(static_cast<float>(semitoneOffset_) / 12.0f),
            25.0f, static_cast<float>(sr4Double) * 0.45f);
        shellFilterL_.updateCoefficients(shellHz, settings.toneQ, sr4Double, filterSmoothAlpha_);
        shellFilterR_.updateCoefficients(shellHz, settings.toneQ, sr4Double, filterSmoothAlpha_);
        const float drive = settings.shapedEngine ? 1.02f : 1.35f;
        const float shellMix = settings.shapedEngine ? 2.4f : 4.0f;
        const float bodyFollow = settings.shapedEngine ? 1.0f : bodyEnv;
        float shellBlend = std::clamp(settings.toneQ / 15.0f, 0.08f, 0.68f);
        if (settings.shapedEngine)
            shellBlend = std::clamp(settings.toneQ / 18.0f, 0.05f, 0.42f);
        float legacyWireBleed = 0.0f;
        if (!settings.shapedEngine && settings.voiceMode == VoiceMode::DanceSnare)
            legacyWireBleed = 0.22f;
        if (!settings.shapedEngine && settings.voiceMode == VoiceMode::PrydaRimSmack)
            legacyWireBleed = 0.30f;

        std::array<float, 4> subShapedL{};
        std::array<float, 4> subShapedR{};
        std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t step) {
            const float inL = subOscL[step] + noiseL * overtoneEnv * legacyWireBleed;
            const float inR = subOscR[step] + noiseR * overtoneEnv * legacyWireBleed;
            const float shellL = shellFilterL_.processBandpassNormalized(inL);
            const float shellR = shellFilterR_.processBandpassNormalized(inR);
            subShapedL[step] = shellSatL_.process((inL * (1.0f - shellBlend) + shellL * shellBlend * shellMix) * drive);
            subShapedR[step] = shellSatR_.process((inR * (1.0f - shellBlend) + shellR * shellBlend * shellMix) * drive);
        });
        bodyL = bodyDownL_.process(subShapedL) * bodyFollow;
        bodyR = bodyDownR_.process(subShapedR) * bodyFollow;

        const float subThumpAmount = std::clamp(settings.subThump, 0.0f, 1.0f);
        if (subThumpAmount > 0.001f) {
            const float subHz = std::min(maxHz, tunedHz * 0.5f);
            phaseSub_ = wrapPhase(phaseSub_ + kTwoPi * subHz * dt);
            const float subThumpEnv = std::exp(-static_cast<float>(mainAge) / std::max(1.0f, bodyDecay * 0.45f));
            const float subSine = std::sin(phaseSub_) * subThumpEnv * subThumpAmount * 0.42f;
            bodyL += subSine;
            bodyR += subSine;
        }
        bodyL *= std::clamp(settings.toneLevel, 0.0f, 1.0f);
        bodyR *= std::clamp(settings.toneLevel, 0.0f, 1.0f);
    }

    const float bodyHighpass = std::clamp(settings.bodyHPF, 20.0f, 4000.0f);
    if (bodyHighpass > 20.0f) {
        bodyHPF_L_.updateCoefficients(bodyHighpass, kButterworthQ, sampleRate_, filterSmoothAlpha_);
        bodyHPF_R_.updateCoefficients(bodyHighpass, kButterworthQ, sampleRate_, filterSmoothAlpha_);
        bodyL = bodyHPF_L_.processHighpass(bodyL);
        bodyR = bodyHPF_R_.processHighpass(bodyR);
    }
    if (bodyHighpass <= 20.0f) {
        bodyHPF_L_.reset();
        bodyHPF_R_.reset();
    }
    const float bodyLowpass = std::clamp(settings.bodyLPF, 100.0f, 20000.0f);
    const bool bodyLpActive = bodyLowpass < std::min(20000.0f, sr * 0.44f);
    if (bodyLpActive) {
        bodyLPF_L_.updateCoefficients(bodyLowpass, kButterworthQ, sampleRate_, filterSmoothAlpha_);
        bodyLPF_R_.updateCoefficients(bodyLowpass, kButterworthQ, sampleRate_, filterSmoothAlpha_);
        bodyL = bodyLPF_L_.processLowpass(bodyL);
        bodyR = bodyLPF_R_.processLowpass(bodyR);
    }
    if (!bodyLpActive) {
        bodyLPF_L_.reset();
        bodyLPF_R_.reset();
    }

    const float flamDecay = std::max(1.0f, std::clamp(settings.preStrikeDecayMs, 2.0f, 35.0f) * 0.001f * sr);
    float flamL = 0.0f;
    float flamR = 0.0f;
    std::ranges::for_each(std::views::iota(0, preStrikeCount_), [&](int idx) {
        const int age = sample - burstStartSamples_[static_cast<size_t>(idx)];
        if (idx >= preStrikeCount_ - 1 || age < 0 || static_cast<float>(age) > flamDecay * 5.0f)
            return;
        const float env = std::exp(-static_cast<float>(age) / flamDecay) * burstAmplitudes_[static_cast<size_t>(idx)];
        const float pan = burstPans_[static_cast<size_t>(idx)];
        const float leftGain = std::sqrt(0.5f * (1.0f - pan));
        const float rightGain = std::sqrt(0.5f * (1.0f + pan));
        const float pitch = std::max(90.0f, tunedHz * 1.6f) * (1.0f + 0.12f * std::exp(-static_cast<float>(age) / (0.002f * sr)));
        const float chirp = std::sin(kTwoPi * pitch * static_cast<float>(age) / sr);
        flamL += env * leftGain * (chirp * 0.28f + nextRandom(prngStateL_) * 0.34f);
        flamR += env * rightGain * (chirp * 0.28f + nextRandom(prngStateR_) * 0.34f);
    });

    float click = 0.0f;
    if (mainAge >= 0 && mainAge < static_cast<int>(0.018f * sr)) {
        const float age = static_cast<float>(mainAge);
        const float trackedClick = settings.clickFreqHz * std::exp2(static_cast<float>(semitoneOffset_) / 12.0f);
        const float frequency = std::clamp(trackedClick, 500.0f, maxHz);
        switch (settings.clickType) {
            case ClickType::Stick: {
                const float stickNoise = nextRandom(prngStateL_);
                std::array<float, 4> subStick{};
                std::ranges::for_each(std::views::iota(0, 4), [&](int step) {
                    const float subAge = age + 0.25f * static_cast<float>(step);
                    const float subFast = std::exp(-subAge / (0.0016f * sr));
                    const float sweep = std::clamp(frequency + 3600.0f * subFast, 500.0f, maxHz4);
                    clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * sweep * dt4);
                    subStick[static_cast<size_t>(step)] = (std::sin(clickPhase_) * 0.64f + stickNoise * 0.44f) * subFast * 1.42f;
                });
                click = transientDown_.process(subStick);
                break;
            }
            case ClickType::Mallet: {
                const float malletHz = std::clamp(frequency * 0.24f, 100.0f, 900.0f);
                std::array<float, 4> subMallet{};
                std::ranges::for_each(std::views::iota(0, 4), [&](int step) {
                    const float subAge = age + 0.25f * static_cast<float>(step);
                    clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * malletHz * dt4);
                    subMallet[static_cast<size_t>(step)] = std::sin(clickPhase_) * std::exp(-subAge / (0.004f * sr)) * 0.96f;
                });
                click = transientDown_.process(subMallet);
                break;
            }
            case ClickType::Rimshot: {
                const float freqScale = std::clamp(trackedClick / 3800.0f, 0.35f, 2.40f);
                constexpr std::array<float, 3> kRimBaseHz{ 1800.0f, 4200.0f, 8700.0f };
                constexpr std::array<float, 3> kRimTausSec{ 0.0028f, 0.0020f, 0.0014f };
                constexpr std::array<float, 3> kRimGains{ 0.58f, 0.52f, 0.44f };
                const float impulseWidth4 = std::clamp(6.0f * (sr / 48000.0f), 4.0f, 24.0f);
                std::array<float, 4> subRim{};
                std::ranges::for_each(std::views::iota(0, 4), [&](int step) {
                    const float subAge4 = static_cast<float>(mainAge * 4 + step);
                    const float impPhase = subAge4 / impulseWidth4;
                    float rimImpulse = 0.0f;
                    float clickSnap = 0.0f;
                    if (impPhase < 1.0f) {
                        const float raisedCos = 1.0f - std::cos(kTwoPi * impPhase);
                        rimImpulse = 2.0f * raisedCos / impulseWidth4;
                        clickSnap = 0.10f * raisedCos;
                    }
                    float modalAcc = 0.0f;
                    std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 3 }), [&](size_t modeIdx) {
                        const float modeHz = std::min(maxHz4, kRimBaseHz[modeIdx] * freqScale);
                        const float radius = std::exp(-1.0f / std::max(2.0f, kRimTausSec[modeIdx] * sr4));
                        const float omega = kTwoPi * modeHz * dt4;
                        modalAcc += kRimGains[modeIdx] * rimModes_[modeIdx].step(
                            rimImpulse, radius, std::cos(omega), std::sin(omega));
                    });
                    subRim[static_cast<size_t>(step)] = rimSat_.process((modalAcc + clickSnap) * 0.64f);
                });
                click = transientDown_.process(subRim);
                break;
            }
            case ClickType::Laser: {
                std::array<float, 4> subLaser{};
                std::ranges::for_each(std::views::iota(0, 4), [&](int step) {
                    const float subAge = age + 0.25f * static_cast<float>(step);
                    const float subFast = std::exp(-subAge / (0.0016f * sr));
                    const float chirpHz = std::clamp(frequency * (0.35f + 3.0f * subFast), 100.0f, maxHz4);
                    clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * chirpHz * dt4);
                    clickPhase2_ = wrapPhase(clickPhase2_ + kTwoPi * std::min(maxHz4, chirpHz * 2.0f) * dt4);
                    const float env = std::exp(-subAge / (0.0048f * sr));
                    subLaser[static_cast<size_t>(step)] = (std::sin(clickPhase_) * 0.98f + std::sin(clickPhase2_) * 0.20f * subFast) * env;
                });
                click = transientDown_.process(subLaser);
                break;
            }
        }
        click *= std::clamp(settings.clickLevel, 0.0f, 1.0f);
    }

    const float pressure = std::clamp(settings.aftertouchValue, 0.0f, 1.0f);
    const float bodyGain = bodyVelocity_ * (1.0f + pressure * std::clamp(settings.bodyAftertouchDepth, 0.0f, 1.0f))
        * std::clamp(settings.bodyPerformanceGain, 0.0f, 1.0f);
    const float wireGain = wireVelocity_ * (1.0f + pressure * std::clamp(settings.wireAftertouchDepth, 0.0f, 1.0f))
        * std::clamp(settings.wirePerformanceGain, 0.0f, 1.0f);
    const float transientGain = transientVelocity_ * (1.0f + pressure * std::clamp(settings.transientAftertouchDepth, 0.0f, 1.0f))
        * std::clamp(settings.transientPerformanceGain, 0.0f, 1.0f);

    float transientL = flamL + click;
    float transientR = flamR + click;
    const float transientHighpass = std::clamp(settings.transientHPF, 20.0f, 12000.0f);
    if (transientHighpass > 20.0f) {
        transientHPF_L_.updateCoefficients(transientHighpass, kBesselQ, sampleRate_, filterSmoothAlpha_);
        transientHPF_R_.updateCoefficients(transientHighpass, kBesselQ, sampleRate_, filterSmoothAlpha_);
        transientL = transientHPF_L_.processHighpass(transientL);
        transientR = transientHPF_R_.processHighpass(transientR);
    }
    if (transientHighpass <= 20.0f) {
        transientHPF_L_.reset();
        transientHPF_R_.reset();
    }
    const float transientLowpass = std::clamp(settings.transientLPF, 100.0f, 20000.0f);
    const bool transientLpActive = transientLowpass < std::min(20000.0f, sr * 0.44f);
    if (transientLpActive) {
        transientLPF_L_.updateCoefficients(transientLowpass, kBesselQ, sampleRate_, filterSmoothAlpha_);
        transientLPF_R_.updateCoefficients(transientLowpass, kBesselQ, sampleRate_, filterSmoothAlpha_);
        transientL = transientLPF_L_.processLowpass(transientL);
        transientR = transientLPF_R_.processLowpass(transientR);
    }
    if (!transientLpActive) {
        transientLPF_L_.reset();
        transientLPF_R_.reset();
    }

    float bodyOutL = bodyL * amp * bodyGain;
    float bodyOutR = bodyR * amp * bodyGain;
    float wireOutL = noiseL * amp * wireGain;
    float wireOutR = noiseR * amp * wireGain;
    float transientOutL = transientL * amp * transientGain;
    float transientOutR = transientR * amp * transientGain;

    const auto applyPan = [](float& left, float& right, float pan) {
        const float position = std::clamp(pan, -1.0f, 1.0f);
        if (position < 0.0f)
            right *= 1.0f + position;
        if (position >= 0.0f)
            left *= 1.0f - position;
    };
    applyPan(bodyOutL, bodyOutR, settings.bodyPan);
    applyPan(wireOutL, wireOutR, settings.wirePan);
    applyPan(transientOutL, transientOutR, settings.transientPan);

    const float dryL = wireOutL + bodyOutL + transientOutL;
    const float dryR = wireOutR + bodyOutR + transientOutR;
    const auto wet = reverb_.process(dryL, dryR, settings.reverbSize, settings.reverbDecayMs,
        settings.reverbDamp, fullAdsrEnabled_ ? 0.0f : settings.reverbGateMs, sampleRate_);
    const float reverbMix = std::clamp(settings.reverbMix, 0.0f, 1.0f);
    const float outL = dryL * (1.0f - 0.4f * reverbMix) + wet.left * reverbMix;
    const float outR = dryR * (1.0f - 0.4f * reverbMix) + wet.right * reverbMix;
    return {
        std::isfinite(outL) ? outL : 0.0f,
        std::isfinite(outR) ? outR : 0.0f,
        std::isfinite(bodyOutL) ? bodyOutL : 0.0f,
        std::isfinite(bodyOutR) ? bodyOutR : 0.0f,
        std::isfinite(wireOutL) ? wireOutL : 0.0f,
        std::isfinite(wireOutR) ? wireOutR : 0.0f,
        std::isfinite(transientOutL) ? transientOutL : 0.0f,
        std::isfinite(transientOutR) ? transientOutR : 0.0f
    };
}

void SmackSynth::renderBlock(std::vector<float>& leftOut, std::vector<float>& rightOut,
                             const SmackSynthSettings& settings, float velocity, int semitoneOffset,
                             std::array<StereoLayerBuffer, 3>* layerOutputs) noexcept {
    (void) renderPrefix(leftOut, rightOut, settings, std::numeric_limits<int>::max(), velocity, semitoneOffset, layerOutputs);
}

int SmackSynth::renderPrefix(std::vector<float>& leftOut, std::vector<float>& rightOut,
                             const SmackSynthSettings& settings, int maxSamples,
                             float velocity, int semitoneOffset,
                             std::array<StereoLayerBuffer, 3>* layerOutputs) noexcept {
    trigger(settings, velocity, semitoneOffset);
    const int renderCount = std::min(totalDurationSamples_, std::max(0, maxSamples));
    leftOut.resize(static_cast<size_t>(renderCount));
    rightOut.resize(static_cast<size_t>(renderCount));
    if (layerOutputs != nullptr) {
        std::ranges::for_each(*layerOutputs, [&](StereoLayerBuffer& layer) {
            layer.left.resize(static_cast<size_t>(renderCount));
            layer.right.resize(static_cast<size_t>(renderCount));
        });
    }
    std::ranges::for_each(std::views::iota(0, renderCount), [&](int idx) {
        const auto frame = processSample(settings);
        const auto pos = static_cast<size_t>(idx);
        leftOut[pos] = frame.left;
        rightOut[pos] = frame.right;
        if (layerOutputs != nullptr) {
            auto& body = (*layerOutputs)[0];
            auto& wire = (*layerOutputs)[1];
            auto& transient = (*layerOutputs)[2];
            body.left[pos] = frame.bodyLeft;
            body.right[pos] = frame.bodyRight;
            wire.left[pos] = frame.wireLeft;
            wire.right[pos] = frame.wireRight;
            transient.left[pos] = frame.transientLeft;
            transient.right[pos] = frame.transientRight;
        }
    });
    return renderCount;
}

} // namespace opensmacker::core
