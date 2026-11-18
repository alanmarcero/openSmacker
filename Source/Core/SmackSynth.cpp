#include "SmackSynth.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>

namespace opensmacker::core {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;

float nextRandom(uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) * (2.0f / 4294967296.0f) - 1.0f;
}

float wrapPhase(float phase) noexcept {
    return phase >= kTwoPi ? std::fmod(phase, kTwoPi) : phase;
}

void bandpassCoefficients(float frequency, float q, double sampleRate,
                          float& b0, float& b1, float& b2, float& a1, float& a2) noexcept {
    const float sr = static_cast<float>(std::max(8000.0, sampleRate));
    const float hz = std::clamp(frequency, 20.0f, sr * 0.45f);
    const float w = kTwoPi * hz / sr;
    const float alpha = std::sin(w) / (2.0f * std::clamp(q, 0.1f, 20.0f));
    const float inverseA0 = 1.0f / (1.0f + alpha);
    b0 = alpha * inverseA0;
    b1 = 0.0f;
    b2 = -b0;
    a1 = -2.0f * std::cos(w) * inverseA0;
    a2 = (1.0f - alpha) * inverseA0;
}

float processBiquad(float input, BiquadState& state, float b0, float b1, float b2, float a1, float a2) noexcept {
    const float output = b0 * input + b1 * state.x1 + b2 * state.x2 - a1 * state.y1 - a2 * state.y2;
    state.x2 = state.x1;
    state.x1 = input;
    state.y2 = state.y1;
    state.y1 = std::isfinite(output) ? output : 0.0f;
    return state.y1;
}

float highpassOnePole(float input, BiquadState& state, float cutoff, float sampleRate) noexcept {
    const float a = std::exp(-kTwoPi * std::clamp(cutoff, 10.0f, sampleRate * 0.45f) / sampleRate);
    const float output = a * (state.y1 + input - state.x1);
    state.x1 = input;
    state.y1 = std::isfinite(output) ? output : 0.0f;
    return state.y1;
}

float lowpassOnePole(float input, BiquadState& state, float cutoff, float sampleRate) noexcept {
    const float a = std::exp(-kTwoPi * std::clamp(cutoff, 20.0f, sampleRate * 0.45f) / sampleRate);
    state.y1 = std::isfinite(state.y1) ? state.y1 : 0.0f;
    state.y1 = (1.0f - a) * input + a * state.y1;
    return state.y1;
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

    const float t = static_cast<float>(sample);
    const float sustain = std::clamp(settings.ampSustain, 0.0f, 1.0f);
    if (attack > 0.0f && t < attack)
        return std::sin(0.5f * kPi * t / attack);
    if (decay > 0.0f && t < attack + decay) {
        const float phase = (t - attack) / decay;
        const float smooth = phase * phase * (3.0f - 2.0f * phase);
        return 1.0f + (sustain - 1.0f) * smooth;
    }
    if (t < releaseStart)
        return sustain;
    const float releaseLength = std::max(1.0f, total - releaseStart);
    const float phase = std::clamp((t - releaseStart) / releaseLength, 0.0f, 1.0f);
    return sustain * 0.5f * (1.0f + std::cos(kPi * phase));
}

// One-shot contour with no sustain hold. The level falls for the whole hit and lands at silence.
float shapedOneShot(int sample, int duration, const SmackSynthSettings& settings, float sampleRate) noexcept {
    if (sample < 0 || duration <= 1 || sample >= duration)
        return 0.0f;
    const float dur = static_cast<float>(duration);
    const float attack = std::clamp(settings.ampAttackMs, 0.05f, 20.0f) * 0.001f * sampleRate;
    const float t = static_cast<float>(sample);
    if (attack > 1.0f && t < attack)
        return std::sin(0.5f * kPi * t / attack);

    const float sustain = std::clamp(settings.ampSustain, 0.0f, 1.0f);
    const float tau = std::max(0.012f * sampleRate,
        std::clamp(settings.ampDecayMs, 8.0f, 600.0f) * 0.001f * sampleRate * std::lerp(0.62f, 1.25f, sustain));
    float env = std::exp(-(t - std::min(t, attack)) / tau);
    const float release = std::clamp(settings.ampReleaseMs, 8.0f, 500.0f) * 0.001f * sampleRate;
    const float land = std::clamp(release, 0.008f * sampleRate, dur * 0.5f);
    if (t > dur - land) {
        const float phase = std::clamp((t - (dur - land)) / land, 0.0f, 1.0f);
        env *= 0.5f * (1.0f + std::cos(kPi * phase));
    }
    return env;
}

struct ShapedProfile {
    float fundScale;
    float modeScale0, modeScale1, modeScale2;
    float modeGain0, modeGain1, modeGain2;
    float fundGain;
    float fmScale;
    bool bell;
    bool octave;
};

ShapedProfile shapedProfile(VoiceMode mode) noexcept {
    switch (mode) {
        case VoiceMode::DanceSnare:
            return { 0.40f, 0.07f, 0.045f, 0.03f, 0.18f, 0.08f, 0.035f, 0.9f, 0.5f, false, false };
        case VoiceMode::ClubTom:
            return { 0.46f, 0.11f, 0.06f, 0.04f, 0.20f, 0.08f, 0.04f, 0.88f, 0.40f, false, true };
        case VoiceMode::FmBell:
            return { 0.95f, 0.72f, 0.48f, 0.30f, 0.42f, 0.26f, 0.12f, 0.74f, 1.0f, true, false };
        case VoiceMode::PrydaRimSmack:
            return { 0.55f, 0.08f, 0.05f, 0.03f, 0.22f, 0.1f, 0.05f, 0.62f, 0.35f, false, false };
    }
    return { 0.3f, 0.08f, 0.05f, 0.03f, 0.12f, 0.06f, 0.03f, 0.7f, 0.5f, false, false };
}

float aliasLimitedIndex(float index, float carrierHz, float modulatorHz, float nyquist) noexcept {
    const float headroom = std::max(0.0f, nyquist * 0.88f - carrierHz);
    const float limit = std::max(0.25f, headroom / std::max(50.0f, modulatorHz) - 1.2f);
    return std::min(std::max(0.0f, index), limit);
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
    const float t = static_cast<float>(position - static_cast<double>(index));
    const int n = static_cast<int>(buffer.size());
    const float y0 = buffer[static_cast<size_t>((index - 1 + n) % n)];
    const float y1 = buffer[static_cast<size_t>(index)];
    const float y2 = buffer[static_cast<size_t>((index + 1) % n)];
    const float y3 = buffer[static_cast<size_t>((index + 2) % n)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void GatedStereoReverb::prepare(double sampleRate) {
    const double rate = std::clamp(sampleRate, 8000.0, 384000.0) / 48000.0;
    constexpr std::array<int, 4> leftLengths{ 1087, 1453, 1987, 2417 };
    constexpr std::array<int, 4> rightLengths{ 1153, 1531, 2089, 2521 };
    for (size_t i = 0; i < delayL_.size(); ++i) {
        const auto l = static_cast<size_t>(std::max(16, static_cast<int>(leftLengths[i] * rate)));
        const auto r = static_cast<size_t>(std::max(16, static_cast<int>(rightLengths[i] * rate)));
        delayL_[i].assign(l, 0.0f);
        delayR_[i].assign(r, 0.0f);
    }
    diffL1_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(149 * rate))), 0.62f);
    diffL2_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(379 * rate))), 0.56f);
    diffL3_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(541 * rate))), 0.51f);
    diffR1_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(113 * rate))), 0.62f);
    diffR2_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(277 * rate))), 0.56f);
    diffR3_.prepare(static_cast<size_t>(std::max(8, static_cast<int>(467 * rate))), 0.51f);
    reset();
}

void GatedStereoReverb::reset() noexcept {
    diffL1_.reset(); diffL2_.reset(); diffL3_.reset();
    diffR1_.reset(); diffR2_.reset(); diffR3_.reset();
    for (size_t i = 0; i < delayL_.size(); ++i) {
        std::fill(delayL_[i].begin(), delayL_[i].end(), 0.0f);
        std::fill(delayR_[i].begin(), delayR_[i].end(), 0.0f);
        writeL_[i] = 0; writeR_[i] = 0;
        dampStateL_[i] = 0.0f; dampStateR_[i] = 0.0f;
    }
    hpfStateL_ = 0.0f; hpfStateR_ = 0.0f;
    lfoPhaseL_ = 0.0; lfoPhaseR_ = 0.25;
    sampleCounter_ = 0;
    gateEnvelope_ = 1.0f;
}

StereoFrame GatedStereoReverb::process(float inLeft, float inRight, float size, float decayMs,
                                       float damp, float gateMs, double sampleRate) noexcept {
    if (delayL_[0].empty() || !std::isfinite(sampleRate) || sampleRate <= 0.0)
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
    const float hpfA = std::exp(-kTwoPi * 140.0f / sr);
    hpfStateL_ = hpfA * hpfStateL_ + (1.0f - hpfA) * inLeft;
    hpfStateR_ = hpfA * hpfStateR_ + (1.0f - hpfA) * inRight;
    const float cleanL = inLeft - hpfStateL_;
    const float cleanR = inRight - hpfStateR_;
    const float diffL = diffL3_.process(diffL2_.process(diffL1_.process(cleanL)));
    const float diffR = diffR3_.process(diffR2_.process(diffR1_.process(cleanR)));

    const float room = std::clamp(size, 0.1f, 1.0f);
    const float decay = std::clamp(decayMs, 50.0f, 2500.0f);
    const float feedback = std::clamp(std::pow(0.001f, 45.0f / decay) * (0.72f + 0.24f * room), 0.18f, 0.91f);
    const float damping = std::clamp(damp, 0.0f, 1.0f);
    lfoPhaseL_ = std::fmod(lfoPhaseL_ + kTwoPi * 0.23 / sampleRate, kTwoPi);
    lfoPhaseR_ = std::fmod(lfoPhaseR_ + kTwoPi * 0.31 / sampleRate, kTwoPi);
    const double modL = std::sin(lfoPhaseL_) * (0.0003 * sampleRate);
    const double modR = std::cos(lfoPhaseR_) * (0.00035 * sampleRate);

    std::array<float, 4> readL{};
    std::array<float, 4> readR{};
    constexpr std::array<double, 4> baseL{ 1087.0, 1453.0, 1987.0, 2417.0 };
    constexpr std::array<double, 4> baseR{ 1153.0, 1531.0, 2089.0, 2521.0 };
    for (size_t i = 0; i < readL.size(); ++i) {
        readL[i] = readHermite(delayL_[i], baseL[i] * sampleRate / 48000.0 + ((i & 1U) ? -modL : modL), writeL_[i]);
        readR[i] = readHermite(delayR_[i], baseR[i] * sampleRate / 48000.0 + ((i & 1U) ? -modR : modR), writeR_[i]);
    }

    const float sumL = std::accumulate(readL.begin(), readL.end(), 0.0f) * 0.5f;
    const float sumR = std::accumulate(readR.begin(), readR.end(), 0.0f) * 0.5f;
    float wetL = 0.0f;
    float wetR = 0.0f;
    for (size_t i = 0; i < readL.size(); ++i) {
        const float matrixL = readL[i] - sumL;
        const float matrixR = readR[i] - sumR;
        const float crossL = matrixL * 0.84f + matrixR * 0.16f;
        const float crossR = matrixR * 0.84f + matrixL * 0.16f;
        const float smooth = 0.20f + 0.72f * damping;
        dampStateL_[i] += (crossL - dampStateL_[i]) * (1.0f - smooth);
        dampStateR_[i] += (crossR - dampStateR_[i]) * (1.0f - smooth);
        delayL_[i][static_cast<size_t>(writeL_[i])] = diffL + dampStateL_[i] * feedback;
        delayR_[i][static_cast<size_t>(writeR_[i])] = diffR + dampStateR_[i] * feedback;
        writeL_[i] = (writeL_[i] + 1) % static_cast<int>(delayL_[i].size());
        writeR_[i] = (writeR_[i] + 1) % static_cast<int>(delayR_[i].size());
        wetL += matrixL * 0.24f;
        wetR += matrixR * 0.24f;
    }
    return { wetL * gateEnvelope_, wetR * gateEnvelope_ };
}

void SmackSynth::prepare(double sampleRate) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
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
    phaseFund_ = phaseSub_ = phaseOctave_ = phaseMode2_ = phaseMode3_ = phaseMode4_ = 0.0f;
    phaseMod1_ = phaseMod2_ = phaseFundR_ = phaseMode2R_ = 0.0f;
    std::fill(metalPhasesL_.begin(), metalPhasesL_.end(), 0.0f);
    std::fill(metalPhasesR_.begin(), metalPhasesR_.end(), 0.0f);
    clickPhase_ = clickPhase2_ = 0.0f;
    lastHPF_ = lastLPF_ = lastShellFreq_ = lastShellQ_ = -1.0f;
    shellFilterL_.reset(); shellFilterR_.reset();
    noiseHPF_L_.reset(); noiseHPF_R_.reset();
    noiseLPF_L_.reset(); noiseLPF_R_.reset();
    bodyHPF_L_.reset(); bodyHPF_R_.reset();
    bodyLPF_L_.reset(); bodyLPF_R_.reset();
    transientHPF_L_.reset(); transientHPF_R_.reset();
    transientLPF_L_.reset(); transientLPF_R_.reset();
    prngStateL_ = 0x2468ACE1U; prngStateR_ = 0x1357BDF0U;
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
    modeRatios_ = settings.articulatedModes ? struckModes[static_cast<size_t>(settings.voiceMode)] : originalRatios;
    const float spreadScale = 1.0f + std::clamp(settings.shellSpread, -0.5f, 0.5f) * 0.45f;
    for (size_t i = 0; i < modeRatios_.size(); ++i) {
        modeRatios_[i] = 1.0f + (modeRatios_[i] - 1.0f) * std::pow(spreadScale, static_cast<float>(i + 1) * 0.5f);
    }
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
    } else {
        for (int i = 0; i < preStrikeCount_; ++i) {
            const float position = static_cast<float>(i + 1);
            const float jitter = nextRandom(prngStateL_) * 0.22f * std::clamp(settings.preStrikeJitter, 0.0f, 1.0f) * spread;
            burstStartSamples_[static_cast<size_t>(i)] = std::max(0, static_cast<int>(position * spread + jitter));
            const float rise = position / static_cast<float>(preStrikeCount_);
            burstAmplitudes_[static_cast<size_t>(i)] = std::clamp(settings.preStrikeLevel * (0.25f + 0.75f * rise), 0.0f, 1.0f);
            const float direction = (i & 1) == 0 ? -1.0f : 1.0f;
            burstPans_[static_cast<size_t>(i)] = direction * std::clamp(settings.preStrikeFlamSpread, 0.0f, 1.0f) * rise;
        }
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

StereoFrame SmackSynth::generateStereoNoise(NoiseType type, float tunedHz, float overtoneEnv, float wireTension) noexcept {
    const float whiteL = nextRandom(prngStateL_);
    const float whiteR = nextRandom(prngStateR_);
    switch (type) {
        case NoiseType::SnareWires: {
            const float resonance = 0.60f + 0.40f * std::clamp(wireTension, 0.0f, 1.0f);
            const float excitation = 1.0f - resonance;
            wireStateL_ = resonance * wireStateL_ + excitation * whiteL;
            wireStateR_ = resonance * wireStateR_ + excitation * whiteR;
            const float wireL = whiteL - wireStateL_;
            const float wireR = whiteR - wireStateR_;
            const float snapL = std::abs(whiteL) > 0.64f ? 1.25f * whiteL : 0.34f * whiteL;
            const float snapR = std::abs(whiteR) > 0.64f ? 1.25f * whiteR : 0.34f * whiteR;
            return { wireL * 0.72f + snapL * 0.28f, wireR * 0.72f + snapR * 0.28f };
        }
        case NoiseType::WhiteSizzle:
            return { std::tanh(whiteL * 1.35f), std::tanh(whiteR * 1.35f) };
        case NoiseType::VelvetAir: {
            const float pulseL = std::abs(whiteL) > 0.60f ? std::copysign(1.0f, whiteL) : 0.0f;
            const float pulseR = std::abs(whiteR) > 0.60f ? std::copysign(1.0f, whiteR) : 0.0f;
            velvetStateL_ = 0.72f * velvetStateL_ + 0.28f * pulseL;
            velvetStateR_ = 0.72f * velvetStateR_ + 0.28f * pulseR;
            return { velvetStateL_ * 1.35f, velvetStateR_ * 1.35f };
        }
        case NoiseType::MetallicRing: {
            constexpr std::array<float, 4> ratios{ 1.414f, 2.236f, 3.146f, 4.732f };
            float ringL = 0.0f;
            float ringR = 0.0f;
            const float nyquist = static_cast<float>(sampleRate_) * 0.44f;
            for (size_t i = 0; i < ratios.size(); ++i) {
                const float detune = 1.0f + (i % 2 == 0 ? 0.0013f : -0.0017f);
                const float frequency = std::clamp(tunedHz * ratios[i], 160.0f, nyquist);
                metalPhasesL_[i] = wrapPhase(metalPhasesL_[i] + kTwoPi * frequency / static_cast<float>(sampleRate_));
                metalPhasesR_[i] = wrapPhase(metalPhasesR_[i] + kTwoPi * frequency * detune / static_cast<float>(sampleRate_));
                const float partialGain = (i == 0 ? 0.38f : 0.22f) * std::exp(-static_cast<float>(i) * 0.25f);
                ringL += std::sin(metalPhasesL_[i]) * partialGain;
                ringR += std::sin(metalPhasesR_[i]) * partialGain;
            }
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
    float amp = fullAdsrEnabled_ ? gatedEnvelope(sample, settings, sr)
        : (settings.shapedEngine
            ? shapedOneShot(sample, dryDurationSamples_, settings, sr)
            : amplitudeEnvelope(sample, dryDurationSamples_, settings, sr));
    if (fullAdsrEnabled_ && noteOffReceived_) {
        if (gateReleaseSamples <= 0 || sample >= releaseStartSample_ + gateReleaseSamples) {
            amp = 0.0f;
        } else {
            const float phase = static_cast<float>(sample - releaseStartSample_) / static_cast<float>(gateReleaseSamples);
            amp = releaseStartLevel_ * 0.5f * (1.0f + std::cos(kPi * phase));
        }
    }
    const int mainAge = sample - mainSmackSample_;
    const float mainTime = std::max(0, mainAge) / sr;
    const float bodyDecay = std::max(1.0f, std::clamp(settings.toneDecayMs, 15.0f, 1200.0f) * 0.001f * sr);
    const float pitchFall = std::exp(-mainTime / std::max(0.003f, settings.tonePitchDecayMs * 0.001f));
    const float pitchSemitones = std::clamp(static_cast<float>(semitoneOffset_) + settings.tonePitchEnv * pitchFall, -48.0f, 48.0f);
    const float tunedHz = std::clamp(rootHz * std::exp2(pitchSemitones / 12.0f), 25.0f, maxHz);
    const float bodyEnv = mainAge < 0 ? 0.0f : (fullAdsrEnabled_ ? 1.0f
        : std::exp(-static_cast<float>(mainAge) / bodyDecay));
    const float struckOvertones = mainAge < 0 ? 0.0f
        : std::exp(-static_cast<float>(mainAge) / std::max(1.0f, bodyDecay * 0.34f));
    const float overtoneEnv = fullAdsrEnabled_ && mainAge >= 0 ? 1.0f : struckOvertones;
    const float fmEnvelope = settings.articulatedModes ? struckOvertones * strikeBrightness_ : overtoneEnv;

    float wireEnv = 0.0f;
    if (fullAdsrEnabled_ && mainAge >= 0) {
        wireEnv = 1.0f;
    } else if (mainAge >= 0) {
        const float t = static_cast<float>(mainAge);
        const float attack = std::clamp(settings.noiseAttackMs, 0.0f, 20.0f) * 0.001f * sr;
        const float hold = std::clamp(settings.noiseHoldMs, 0.0f, 30.0f) * 0.001f * sr;
        const float decay = std::max(1.0f, std::clamp(settings.noiseDecayMs, 15.0f, 1200.0f) * 0.001f * sr);
        if (attack > 0.0f && t < attack) {
            wireEnv = std::sin(0.5f * kPi * t / attack);
        } else if (t < attack + hold) {
            wireEnv = 1.0f;
        } else {
            const float phase = std::max(0.0f, (t - attack - hold) / decay);
            const float curved = std::exp(-phase * (2.4f + std::max(0.0f, -settings.noiseCurve) * 2.2f));
            const float straight = std::max(0.0f, 1.0f - phase);
            const float shape = std::clamp(settings.noiseCurve, -1.0f, 1.0f);
            wireEnv = shape < 0.0f ? curved : curved * (1.0f - shape) + straight * shape;
            if (!settings.shapedEngine && phase > 5.0f)
                wireEnv = 0.0f;
        }
    }

    const auto rawNoise = generateStereoNoise(settings.noiseType, tunedHz, overtoneEnv, settings.wireTension);
    float noiseL = rawNoise.left * wireEnv * settings.noiseLevel;
    float noiseR = rawNoise.right * wireEnv * settings.noiseLevel;
    const float flamDecay = std::max(1.0f, std::clamp(settings.preStrikeDecayMs, 2.0f, 35.0f) * 0.001f * sr);
    float flamL = 0.0f;
    float flamR = 0.0f;
    for (int i = 0; i < preStrikeCount_; ++i) {
        const int age = sample - burstStartSamples_[static_cast<size_t>(i)];
        if (i >= preStrikeCount_ - 1 || age < 0 || static_cast<float>(age) > flamDecay * 5.0f)
            continue;
        const float env = std::exp(-static_cast<float>(age) / flamDecay) * burstAmplitudes_[static_cast<size_t>(i)];
        const float pan = burstPans_[static_cast<size_t>(i)];
        const float l = std::sqrt(0.5f * (1.0f - pan));
        const float r = std::sqrt(0.5f * (1.0f + pan));
        const float pitch = std::max(90.0f, tunedHz * 1.6f) * (1.0f + 0.12f * std::exp(-static_cast<float>(age) / (0.002f * sr)));
        const float chirp = std::sin(kTwoPi * pitch * static_cast<float>(age) / sr);
        flamL += env * l * (chirp * 0.28f + nextRandom(prngStateL_) * 0.34f);
        flamR += env * r * (chirp * 0.28f + nextRandom(prngStateR_) * 0.34f);
    }

    float click = 0.0f;
    if (mainAge >= 0 && mainAge < static_cast<int>(0.012f * sr)) {
        const float t = static_cast<float>(mainAge);
        const float fast = std::exp(-t / (0.0016f * sr));
        const float trackedClick = settings.clickFreqHz * std::exp2(static_cast<float>(semitoneOffset_) / 12.0f);
        const float frequency = std::clamp(trackedClick, 500.0f, maxHz);
        switch (settings.clickType) {
            case ClickType::Stick: {
                const float sweep = frequency + 3600.0f * fast;
                clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * std::clamp(sweep, 500.0f, maxHz) * dt);
                click = (std::sin(clickPhase_) * 0.60f + nextRandom(prngStateL_) * 0.40f) * fast * 1.2f;
                break;
            }
            case ClickType::Mallet: {
                const float f = std::clamp(frequency * 0.24f, 100.0f, 900.0f);
                clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * f * dt);
                click = std::sin(clickPhase_) * std::exp(-t / (0.004f * sr)) * 0.9f;
                break;
            }
            case ClickType::Rimshot: {
                if (settings.shapedEngine) {
                    float acc = 0.0f;
                    const float second = std::min(sr * 1.7f, frequency * 2.37f);
                    for (int step = 0; step < 4; ++step) {
                        clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * frequency * dt * 0.25f);
                        clickPhase2_ = wrapPhase(clickPhase2_ + kTwoPi * second * dt * 0.25f);
                        acc += std::tanh((std::sin(clickPhase_) + 0.40f * std::sin(clickPhase2_)) * 1.05f);
                    }
                    click = (acc * 0.25f) * std::exp(-t / (0.0018f * sr));
                } else {
                    clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * frequency * dt);
                    clickPhase2_ = wrapPhase(clickPhase2_ + kTwoPi * std::min(maxHz, frequency * 2.37f) * dt);
                    click = std::tanh((std::sin(clickPhase_) + 0.55f * std::sin(clickPhase2_)) * 1.5f)
                        * std::exp(-t / (0.0022f * sr));
                }
                break;
            }
            case ClickType::Laser: {
                clickPhase_ = wrapPhase(clickPhase_ + kTwoPi * std::clamp(frequency * (0.35f + 3.0f * fast), 100.0f, maxHz) * dt);
                click = std::sin(clickPhase_) * std::exp(-t / (0.0048f * sr));
                break;
            }
        }
        click *= std::clamp(settings.clickLevel, 0.0f, 1.0f);
    }

    if (std::abs(settings.noiseHPF - lastHPF_) > 0.5f || lastHPF_ < 0.0f)
        lastHPF_ = settings.noiseHPF;
    if (std::abs(settings.noiseLPF - lastLPF_) > 0.5f || lastLPF_ < 0.0f)
        lastLPF_ = settings.noiseLPF;
    noiseL = highpassOnePole(noiseL, noiseHPF_L_, lastHPF_, sr);
    noiseR = highpassOnePole(noiseR, noiseHPF_R_, lastHPF_, sr);
    noiseL = lowpassOnePole(noiseL, noiseLPF_L_, lastLPF_, sr);
    noiseR = lowpassOnePole(noiseR, noiseLPF_R_, lastLPF_, sr);
    const float width = std::clamp(settings.noiseStereoWidth, 0.0f, 2.0f);
    const float noiseMid = 0.5f * (noiseL + noiseR);
    const float noiseSide = 0.5f * (noiseL - noiseR) * width;
    noiseL = noiseMid + noiseSide;
    noiseR = noiseMid - noiseSide;

    float bodyL = 0.0f;
    float bodyR = 0.0f;
    if (mainAge >= 0 && settings.toneLevel > 0.0001f) {
        if (std::abs(settings.toneFreqHz - lastShellFreq_) > 0.5f
            || std::abs(settings.toneQ - lastShellQ_) > 0.01f || lastShellFreq_ < 0.0f) {
            lastShellFreq_ = settings.toneFreqHz;
            lastShellQ_ = settings.toneQ;
            bandpassCoefficients(lastShellFreq_, lastShellQ_, sampleRate_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_);
        }
        const float mode2 = std::min(maxHz, tunedHz * modeRatios_[0]);
        const float mode3 = std::min(maxHz, tunedHz * modeRatios_[1]);
        const float mode4 = std::min(maxHz, tunedHz * modeRatios_[2]);
        phaseFund_ = wrapPhase(phaseFund_ + kTwoPi * tunedHz * dt);
        phaseFundR_ = wrapPhase(phaseFundR_ + kTwoPi * tunedHz * 1.0007f * dt);
        phaseMode2_ = wrapPhase(phaseMode2_ + kTwoPi * mode2 * dt);
        phaseMode2R_ = wrapPhase(phaseMode2R_ + kTwoPi * mode2 * 1.0011f * dt);
        phaseMode3_ = wrapPhase(phaseMode3_ + kTwoPi * mode3 * dt);
        phaseMode4_ = wrapPhase(phaseMode4_ + kTwoPi * mode4 * dt);
        phaseMod1_ = wrapPhase(phaseMod1_ + kTwoPi * std::min(maxHz, tunedHz * (1.37f + settings.fmAmount * 1.8f)) * dt);
        phaseMod2_ = wrapPhase(phaseMod2_ + kTwoPi * std::min(maxHz, tunedHz * 3.17f) * dt);

        const float modHz = std::min(maxHz, tunedHz * (1.37f + settings.fmAmount * 1.8f));
        float rawL = 0.0f;
        float rawR = 0.0f;
        float drive = 1.35f;
        float shellMix = 4.0f;
        float bodyFollow = bodyEnv;
        if (settings.shapedEngine) {
            const auto profile = shapedProfile(settings.voiceMode);
            const float age = static_cast<float>(mainAge);
            const auto modeEnv = [&](float scale) {
                return std::exp(-age / std::max(1.0f, bodyDecay * scale));
            };
            float fundTau = bodyDecay * profile.fundScale;
            if (!profile.bell)
                fundTau = std::min(fundTau, (profile.octave ? 0.11f : 0.040f) * sr);
            const float fundEnv = fullAdsrEnabled_ ? 1.0f : std::exp(-age / std::max(1.0f, fundTau));
            const float fmIndex = aliasLimitedIndex(
                std::clamp(settings.fmAmount, 0.0f, 1.0f) * 2.2f * fmEnvelope * profile.fmScale,
                tunedHz, modHz, maxHz);
            const float partialL = profile.modeGain0 * std::sin(phaseMode2_) * modeEnv(profile.modeScale0)
                + profile.modeGain1 * std::sin(phaseMode3_) * modeEnv(profile.modeScale1)
                + profile.modeGain2 * std::sin(phaseMode4_) * modeEnv(profile.modeScale2);
            const float partialR = profile.modeGain0 * std::sin(phaseMode2R_) * modeEnv(profile.modeScale0)
                + profile.modeGain1 * std::cos(phaseMode3_) * modeEnv(profile.modeScale1)
                + profile.modeGain2 * std::sin(phaseMode4_ + 0.04f) * modeEnv(profile.modeScale2);
            if (profile.bell) {
                const float bellIndex = aliasLimitedIndex(
                    (0.55f + 2.4f * std::clamp(settings.fmAmount, 0.0f, 1.0f)) * fmEnvelope, tunedHz, modHz, maxHz);
                rawL = std::sin(phaseFund_ + std::sin(phaseMod1_) * bellIndex) * fundEnv * profile.fundGain + partialL;
                rawR = std::sin(phaseFundR_ + std::sin(phaseMod1_ + 0.11f) * bellIndex) * fundEnv * profile.fundGain + partialR;
            } else {
                rawL = std::sin(phaseFund_ + fmIndex * std::sin(phaseMod1_)) * fundEnv * profile.fundGain + partialL;
                rawR = std::sin(phaseFundR_ + fmIndex * std::sin(phaseMod1_ + 0.07f)) * fundEnv * profile.fundGain + partialR;
            }
            if (profile.octave) {
                const float octaveHz = std::min(maxHz, tunedHz * 2.0f);
                phaseOctave_ = wrapPhase(phaseOctave_ + kTwoPi * octaveHz * dt);
                const float octaveEnv = fullAdsrEnabled_ ? 0.35f : modeEnv(profile.fundScale * 0.55f);
                const float octave = 0.24f * std::sin(phaseOctave_) * octaveEnv;
                rawL += octave;
                rawR += octave;
                uint32_t hash = 0x85ebca6bu * static_cast<uint32_t>(sample + 17);
                hash ^= hash >> 13;
                const float air = (static_cast<float>(hash) * (2.0f / 4294967296.0f) - 1.0f) * fundEnv * 0.34f;
                rawL += air;
                rawR += air * 0.9f;
            }
            drive = 1.02f;
            shellMix = 2.4f;
            bodyFollow = 1.0f;
        } else {
            const float fmIndex = std::clamp(settings.fmAmount, 0.0f, 1.0f) * 2.6f * fmEnvelope;
            const float fundamentalL = std::sin(phaseFund_ + fmIndex * std::sin(phaseMod1_));
            const float fundamentalR = std::sin(phaseFundR_ + fmIndex * std::sin(phaseMod1_ + 0.07f));
            const float highModesL = 0.48f * std::sin(phaseMode2_) * std::exp(-static_cast<float>(mainAge) / (bodyDecay * 0.48f))
                + 0.28f * std::sin(phaseMode3_) * std::exp(-static_cast<float>(mainAge) / (bodyDecay * 0.25f))
                + 0.16f * std::sin(phaseMode4_) * std::exp(-static_cast<float>(mainAge) / (bodyDecay * 0.15f));
            const float highModesR = 0.48f * std::sin(phaseMode2R_) * std::exp(-static_cast<float>(mainAge) / (bodyDecay * 0.48f))
                + 0.28f * std::cos(phaseMode3_) * std::exp(-static_cast<float>(mainAge) / (bodyDecay * 0.25f))
                + 0.16f * std::sin(phaseMode4_ + 0.04f) * std::exp(-static_cast<float>(mainAge) / (bodyDecay * 0.15f));

            rawL = fundamentalL * bodyEnv + highModesL * settings.fmAmount;
            rawR = fundamentalR * bodyEnv + highModesR * settings.fmAmount;
            switch (settings.voiceMode) {
                case VoiceMode::DanceSnare:
                    rawL = rawL * 0.58f + highModesL * 0.62f + noiseL * overtoneEnv * 0.22f;
                    rawR = rawR * 0.58f + highModesR * 0.62f + noiseR * overtoneEnv * 0.22f;
                    break;
                case VoiceMode::ClubTom:
                    rawL = fundamentalL * bodyEnv * 1.08f + highModesL * 0.52f;
                    rawR = fundamentalR * bodyEnv * 1.08f + highModesR * 0.52f;
                    break;
                case VoiceMode::FmBell: {
                    const float bellL = std::sin(phaseFund_ + std::sin(phaseMod1_) * (0.7f + 3.8f * settings.fmAmount) * fmEnvelope);
                    const float bellR = std::sin(phaseFundR_ + std::sin(phaseMod1_ + 0.11f) * (0.7f + 3.8f * settings.fmAmount) * fmEnvelope);
                    const float chime = 0.45f * std::sin(phaseMode3_) + 0.24f * std::sin(phaseMod2_);
                    rawL = bellL * bodyEnv * 0.72f + chime * overtoneEnv;
                    rawR = bellR * bodyEnv * 0.72f + chime * overtoneEnv;
                    break;
                }
                case VoiceMode::PrydaRimSmack:
                    rawL = fundamentalL * bodyEnv * 0.46f + highModesL * 0.75f + noiseL * overtoneEnv * 0.30f;
                    rawR = fundamentalR * bodyEnv * 0.46f + highModesR * 0.75f + noiseR * overtoneEnv * 0.30f;
                    break;
            }
        }
        const float shellL = processBiquad(rawL, shellFilterL_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_);
        const float shellR = processBiquad(rawR, shellFilterR_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_);
        const float shellBlend = settings.shapedEngine
            ? std::clamp(settings.toneQ / 18.0f, 0.05f, 0.42f)
            : std::clamp(settings.toneQ / 15.0f, 0.08f, 0.68f);
        bodyL = std::tanh((rawL * (1.0f - shellBlend) + shellL * shellBlend * shellMix) * drive) * bodyFollow;
        bodyR = std::tanh((rawR * (1.0f - shellBlend) + shellR * shellBlend * shellMix) * drive) * bodyFollow;
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
        bodyL = highpassOnePole(bodyL, bodyHPF_L_, bodyHighpass, sr);
        bodyR = highpassOnePole(bodyR, bodyHPF_R_, bodyHighpass, sr);
    } else {
        bodyHPF_L_.reset(); bodyHPF_R_.reset();
    }
    const float bodyLowpass = std::clamp(settings.bodyLPF, 100.0f, 20000.0f);
    if (bodyLowpass < std::min(20000.0f, sr * 0.44f)) {
        bodyL = lowpassOnePole(bodyL, bodyLPF_L_, bodyLowpass, sr);
        bodyR = lowpassOnePole(bodyR, bodyLPF_R_, bodyLowpass, sr);
    } else {
        bodyLPF_L_.reset(); bodyLPF_R_.reset();
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
        transientL = highpassOnePole(transientL, transientHPF_L_, transientHighpass, sr);
        transientR = highpassOnePole(transientR, transientHPF_R_, transientHighpass, sr);
    } else {
        transientHPF_L_.reset(); transientHPF_R_.reset();
    }
    const float transientLowpass = std::clamp(settings.transientLPF, 100.0f, 20000.0f);
    if (transientLowpass < std::min(20000.0f, sr * 0.44f)) {
        transientL = lowpassOnePole(transientL, transientLPF_L_, transientLowpass, sr);
        transientR = lowpassOnePole(transientR, transientLPF_R_, transientLowpass, sr);
    } else {
        transientLPF_L_.reset(); transientLPF_R_.reset();
    }

    float bodyOutL = bodyL * amp * bodyGain;
    float bodyOutR = bodyR * amp * bodyGain;
    float wireOutL = noiseL * amp * wireGain;
    float wireOutR = noiseR * amp * wireGain;
    float transientOutL = transientL * amp * transientGain;
    float transientOutR = transientR * amp * transientGain;
    const auto applyPan = [](float& left, float& right, float pan) {
        const float position = std::clamp(pan, -1.0f, 1.0f);
        if (position < 0.0f) right *= 1.0f + position;
        else left *= 1.0f - position;
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
        for (auto& layer : *layerOutputs) {
            layer.left.resize(static_cast<size_t>(renderCount));
            layer.right.resize(static_cast<size_t>(renderCount));
        }
    }
    for (int i = 0; i < renderCount; ++i) {
        const auto frame = processSample(settings);
        leftOut[static_cast<size_t>(i)] = frame.left;
        rightOut[static_cast<size_t>(i)] = frame.right;
        if (layerOutputs != nullptr) {
            auto& body = (*layerOutputs)[0];
            auto& wire = (*layerOutputs)[1];
            auto& transient = (*layerOutputs)[2];
            body.left[static_cast<size_t>(i)] = frame.bodyLeft;
            body.right[static_cast<size_t>(i)] = frame.bodyRight;
            wire.left[static_cast<size_t>(i)] = frame.wireLeft;
            wire.right[static_cast<size_t>(i)] = frame.wireRight;
            transient.left[static_cast<size_t>(i)] = frame.transientLeft;
            transient.right[static_cast<size_t>(i)] = frame.transientRight;
        }
    }
    return renderCount;
}

} // namespace opensmacker::core
