#include "SmackSynth.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace opensmacker::core {

namespace {

[[nodiscard]] constexpr float pi() noexcept {
    return std::numbers::pi_v<float>;
}

[[nodiscard]] inline float nextRandomFloat(uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (static_cast<float>(state) * (2.0f / 4294967296.0f)) - 1.0f;
}

void computeBandpassCoeffs(float freqHz, float q, double sampleRate, float& b0, float& b1, float& b2, float& a1, float& a2) noexcept {
    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float clampedFreq = std::clamp(freqHz, 20.0f, nyquist * 0.95f);
    const float clampedQ = std::clamp(q, 0.1f, 30.0f);
    const float w0 = 2.0f * pi() * clampedFreq / static_cast<float>(sampleRate);
    const float alpha = std::sin(w0) / (2.0f * clampedQ);
    const float cosW0 = std::cos(w0);

    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cosW0) / a0;
    a2 = (1.0f - alpha) / a0;
}

void computeHighpassCoeffs(float freqHz, double sampleRate, float& b0, float& b1, float& b2, float& a1, float& a2) noexcept {
    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float clampedFreq = std::clamp(freqHz, 10.0f, nyquist * 0.95f);
    const float w0 = 2.0f * pi() * clampedFreq / static_cast<float>(sampleRate);
    const float cosW0 = std::cos(w0);
    const float alpha = std::sin(w0) * 0.70710678f;

    const float a0 = 1.0f + alpha;
    b0 = ((1.0f + cosW0) * 0.5f) / a0;
    b1 = (-(1.0f + cosW0)) / a0;
    b2 = ((1.0f + cosW0) * 0.5f) / a0;
    a1 = (-2.0f * cosW0) / a0;
    a2 = (1.0f - alpha) / a0;
}

void computeLowpassCoeffs(float freqHz, double sampleRate, float& b0, float& b1, float& b2, float& a1, float& a2) noexcept {
    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float clampedFreq = std::clamp(freqHz, 20.0f, nyquist * 0.95f);
    const float w0 = 2.0f * pi() * clampedFreq / static_cast<float>(sampleRate);
    const float cosW0 = std::cos(w0);
    const float alpha = std::sin(w0) * 0.70710678f;

    const float a0 = 1.0f + alpha;
    b0 = ((1.0f - cosW0) * 0.5f) / a0;
    b1 = (1.0f - cosW0) / a0;
    b2 = ((1.0f - cosW0) * 0.5f) / a0;
    a1 = (-2.0f * cosW0) / a0;
    a2 = (1.0f - alpha) / a0;
}

[[nodiscard]] inline float processBiquad(float in, BiquadState& s, float b0, float b1, float b2, float a1, float a2) noexcept {
    const float out = b0 * in + b1 * s.x1 + b2 * s.x2 - a1 * s.y1 - a2 * s.y2;
    s.x2 = s.x1;
    s.x1 = in;
    s.y2 = s.y1;
    s.y1 = out;
    return out;
}

[[nodiscard]] inline float wrapTwoPi(float phase) noexcept {
    constexpr float kTwoPi = 2.0f * pi();
    if (phase >= kTwoPi) {
        return std::fmod(phase, kTwoPi);
    }
    return phase;
}

} // namespace

// ============================================================================
// GatedStereoReverb - Lush Prime-Delay FDN with Gated Envelope
// ============================================================================

float GatedStereoReverb::readHermite(const std::vector<float>& buffer, double delaySamples, int writePos) noexcept {
    const double size = static_cast<double>(buffer.size());
    double readPos = static_cast<double>(writePos) - delaySamples;
    while (readPos < 0.0) readPos += size;
    while (readPos >= size) readPos -= size;

    const int i1 = static_cast<int>(readPos);
    const float frac = static_cast<float>(readPos - static_cast<double>(i1));
    const int i0 = (i1 - 1 + static_cast<int>(buffer.size())) % static_cast<int>(buffer.size());
    const int i2 = (i1 + 1) % static_cast<int>(buffer.size());
    const int i3 = (i1 + 2) % static_cast<int>(buffer.size());

    const float y0 = buffer[static_cast<size_t>(i0)];
    const float y1 = buffer[static_cast<size_t>(i1)];
    const float y2 = buffer[static_cast<size_t>(i2)];
    const float y3 = buffer[static_cast<size_t>(i3)];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void GatedStereoReverb::prepare(double sampleRate) {
    const double rateScale = sampleRate / 48000.0;

    // High-diffusion input allpass networks
    diffL1_.prepare(std::max(8, static_cast<int>(149.0 * rateScale)), 0.65f);
    diffL2_.prepare(std::max(8, static_cast<int>(379.0 * rateScale)), 0.60f);
    diffL3_.prepare(std::max(8, static_cast<int>(541.0 * rateScale)), 0.55f);

    diffR1_.prepare(std::max(8, static_cast<int>(113.0 * rateScale)), 0.65f);
    diffR2_.prepare(std::max(8, static_cast<int>(277.0 * rateScale)), 0.60f);
    diffR3_.prepare(std::max(8, static_cast<int>(467.0 * rateScale)), 0.55f);

    // Mutual-prime delay lines for flutter-free natural room/hall dispersion
    constexpr std::array<int, 4> kBaseL = { 1087, 1453, 1987, 2417 };
    constexpr std::array<int, 4> kBaseR = { 1153, 1531, 2089, 2521 };
    const int margin = static_cast<int>(sampleRate * 0.004);

    for (int k = 0; k < 4; ++k) {
        const int lenL = static_cast<int>(static_cast<double>(kBaseL[k]) * rateScale) + margin;
        const int lenR = static_cast<int>(static_cast<double>(kBaseR[k]) * rateScale) + margin;
        delayL_[k].assign(std::max(32, lenL), 0.0f);
        delayR_[k].assign(std::max(32, lenR), 0.0f);
    }

    reset();
}

void GatedStereoReverb::reset() noexcept {
    diffL1_.reset();
    diffL2_.reset();
    diffL3_.reset();
    diffR1_.reset();
    diffR2_.reset();
    diffR3_.reset();

    for (int k = 0; k < 4; ++k) {
        std::fill(delayL_[k].begin(), delayL_[k].end(), 0.0f);
        std::fill(delayR_[k].begin(), delayR_[k].end(), 0.0f);
        writeL_[k] = 0;
        writeR_[k] = 0;
        dampStateL_[k] = 0.0f;
        dampStateR_[k] = 0.0f;
    }

    hpfStateL_ = 0.0f;
    hpfStateR_ = 0.0f;
    lfoPhaseL_ = 0.0;
    lfoPhaseR_ = 0.25;
    sampleCounter_ = 0;
    gateEnvelope_ = 1.0f;
}

StereoFrame GatedStereoReverb::process(
    float inLeft,
    float inRight,
    float size,
    float decayMs,
    float damp,
    float gateMs,
    double sampleRate) noexcept {
    if (delayL_[0].empty()) {
        return { inLeft, inRight };
    }

    sampleCounter_++;

    if (gateMs > 0.0f) {
        const float gateSamples = (gateMs * 0.001f) * static_cast<float>(sampleRate);
        const float releaseSamples = (22.0f * 0.001f) * static_cast<float>(sampleRate);
        if (static_cast<float>(sampleCounter_) > gateSamples) {
            const float over = static_cast<float>(sampleCounter_) - gateSamples;
            if (over < releaseSamples) {
                const float u = over / releaseSamples;
                gateEnvelope_ = 0.5f * (1.0f + std::cos(pi() * u));
            } else {
                gateEnvelope_ = 0.0f;
            }
        } else {
            gateEnvelope_ = 1.0f;
        }
    } else {
        gateEnvelope_ = 1.0f;
    }

    if (gateEnvelope_ <= 0.0f) {
        return { 0.0f, 0.0f };
    }

    // 1-Pole highpass filter (135 Hz) removes low-end mud accumulation
    const float hpfCoeff = static_cast<float>(1.0 - std::exp(-2.0 * pi() * 135.0 / sampleRate));
    hpfStateL_ += hpfCoeff * (inLeft - hpfStateL_);
    const float cleanInL = inLeft - hpfStateL_;
    hpfStateR_ += hpfCoeff * (inRight - hpfStateR_);
    const float cleanInR = inRight - hpfStateR_;

    const float diffInL = diffL3_.process(diffL2_.process(diffL1_.process(cleanInL)));
    const float diffInR = diffR3_.process(diffR2_.process(diffR1_.process(cleanInR)));

    // Smooth feedback factor
    const float baseFeedback = std::clamp(std::pow(0.001f, 42.0f / std::max(20.0f, decayMs)), 0.15f, 0.94f);
    const float feedback = baseFeedback * (0.65f + 0.32f * std::clamp(size, 0.1f, 1.0f));
    const float dampFactor = std::clamp(damp, 0.02f, 0.88f);

    // Asynchronous dual LFOs for rich chorused shimmer
    lfoPhaseL_ += 2.0 * pi() * 0.68 / sampleRate;
    if (lfoPhaseL_ > 2.0 * pi()) lfoPhaseL_ -= 2.0 * pi();
    lfoPhaseR_ += 2.0 * pi() * 0.87 / sampleRate;
    if (lfoPhaseR_ > 2.0 * pi()) lfoPhaseR_ -= 2.0 * pi();

    const double modL = std::sin(lfoPhaseL_) * (0.00065 * sampleRate);
    const double modR = std::cos(lfoPhaseR_) * (0.00065 * sampleRate);

    const double rateScale = sampleRate / 48000.0;
    constexpr std::array<double, 4> kBaseL = { 1087.0, 1453.0, 1987.0, 2417.0 };
    constexpr std::array<double, 4> kBaseR = { 1153.0, 1531.0, 2089.0, 2521.0 };

    std::array<float, 4> outL{};
    std::array<float, 4> outR{};

    for (int k = 0; k < 4; ++k) {
        const double dL = kBaseL[k] * rateScale + (k % 2 == 0 ? modL : -modL);
        const double dR = kBaseR[k] * rateScale + (k % 2 == 0 ? modR : -modR);
        outL[k] = readHermite(delayL_[k], dL, writeL_[k]);
        outR[k] = readHermite(delayR_[k], dR, writeR_[k]);
    }

    // 4x4 Householder orthogonal mixing matrix
    const float sumL = (outL[0] + outL[1] + outL[2] + outL[3]) * 0.5f;
    const float sumR = (outR[0] + outR[1] + outR[2] + outR[3]) * 0.5f;

    std::array<float, 4> mixL{};
    std::array<float, 4> mixR{};
    for (int k = 0; k < 4; ++k) {
        mixL[k] = outL[k] - sumL;
        mixR[k] = outR[k] - sumR;
    }

    // Stereo cross-coupling + high-frequency damping
    for (int k = 0; k < 4; ++k) {
        const float crossL = mixL[k] * 0.85f + mixR[k] * 0.15f;
        const float crossR = mixR[k] * 0.85f + mixL[k] * 0.15f;

        dampStateL_[k] = dampStateL_[k] * dampFactor + crossL * (1.0f - dampFactor);
        dampStateR_[k] = dampStateR_[k] * dampFactor + crossR * (1.0f - dampFactor);

        delayL_[k][static_cast<size_t>(writeL_[k])] = diffInL + dampStateL_[k] * feedback;
        delayR_[k][static_cast<size_t>(writeR_[k])] = diffInR + dampStateR_[k] * feedback;

        writeL_[k] = (writeL_[k] + 1) % static_cast<int>(delayL_[k].size());
        writeR_[k] = (writeR_[k] + 1) % static_cast<int>(delayR_[k].size());
    }

    const float wetL = 0.48f * (mixL[0] + mixL[1] + mixL[2] + mixL[3]) * gateEnvelope_;
    const float wetR = 0.48f * (mixR[0] + mixR[1] + mixR[2] + mixR[3]) * gateEnvelope_;

    return { wetL, wetR };
}

// ============================================================================
// SmackSynth - Complete Redesigned Synthesis Architecture
// ============================================================================

void SmackSynth::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    reverb_.prepare(sampleRate);
    reset();
}

void SmackSynth::reset() noexcept {
    isPlaying_ = false;
    totalSamplesPlayed_ = 0;
    totalDurationSamples_ = 0;

    phaseFund_ = 0.0f;
    phaseMode2_ = 0.0f;
    phaseMode3_ = 0.0f;
    phaseMode4_ = 0.0f;
    phaseMod1_ = 0.0f;
    phaseMod2_ = 0.0f;
    phaseFundR_ = 0.0f;
    phaseMode2R_ = 0.0f;
    std::fill(metalPhasesL_.begin(), metalPhasesL_.end(), 0.0f);
    std::fill(metalPhasesR_.begin(), metalPhasesR_.end(), 0.0f);
    clickPhase_ = 0.0f;
    clickPhase2_ = 0.0f;

    lastHPF_ = -1.0f;
    lastLPF_ = -1.0f;
    lastShellFreq_ = -1.0f;
    lastShellQ_ = -1.0f;

    shellFilterL_.reset();
    shellFilterR_.reset();
    noiseHPF_L_.reset();
    noiseHPF_R_.reset();
    noiseLPF_L_.reset();
    noiseLPF_R_.reset();

    prngStateL_ = 0x2468ACE1;
    prngStateR_ = 0x1357BDF0;
    wireStateL_ = 0.0f;
    wireStateR_ = 0.0f;
    velvetStateL_ = 0.0f;
    velvetStateR_ = 0.0f;

    reverb_.reset();
}

void SmackSynth::trigger(const SmackSynthSettings& settings, float velocity, int semitoneOffset) noexcept {
    reset();
    isPlaying_ = true;
    velocity_ = std::clamp(velocity, 0.0f, 1.0f);
    semitoneOffset_ = semitoneOffset;

    preStrikeCount_ = std::clamp(settings.preStrikeCount, 1, 5);
    const float spreadSamples = (settings.preStrikeSpreadMs * 0.001f) * static_cast<float>(sampleRate_);

    if (preStrikeCount_ <= 1) {
        burstStartSamples_[0] = 0;
        burstAmplitudes_[0] = 0.0f;
        burstPans_[0] = 0.0f;
        mainSmackSample_ = 0;
    } else {
        for (int i = 0; i < preStrikeCount_; ++i) {
            const float jitterTime = (nextRandomFloat(prngStateL_) * 0.35f) * settings.preStrikeJitter * spreadSamples;
            burstStartSamples_[i] = std::max(0, static_cast<int>(static_cast<float>(i) * spreadSamples + jitterTime));

            const float ramp = static_cast<float>(i + 1) / static_cast<float>(preStrikeCount_);
            const float jitterAmp = 0.88f + 0.12f * ramp + nextRandomFloat(prngStateL_) * 0.12f * settings.preStrikeJitter;
            burstAmplitudes_[i] = std::clamp(settings.preStrikeLevel * jitterAmp * ramp, 0.05f, 1.0f);

            const float dir = (i % 2 == 0) ? -1.0f : 1.0f;
            burstPans_[i] = std::clamp(dir * settings.preStrikeFlamSpread * ramp, -1.0f, 1.0f);
        }
        mainSmackSample_ = static_cast<int>(static_cast<float>(preStrikeCount_) * spreadSamples);
    }

    const float preTimeMs = (preStrikeCount_ > 1) ? settings.preStrikeSpreadMs * static_cast<float>(preStrikeCount_) : 0.0f;
    const float mainDecayMs = std::max({ settings.noiseDecayMs, settings.toneDecayMs, settings.reverbDecayMs, settings.reverbGateMs });
    const float totalTimeMs = preTimeMs + mainDecayMs + 120.0f;
    totalDurationSamples_ = static_cast<int>((totalTimeMs * 0.001f) * static_cast<float>(sampleRate_));
}

StereoFrame SmackSynth::generateStereoNoise(NoiseType type, float tunedHz, float overtoneEnv) noexcept {
    const float whiteL = nextRandomFloat(prngStateL_);
    const float whiteR = nextRandomFloat(prngStateR_);

    switch (type) {
        case NoiseType::SnareWires: {
            // Authentic snare wire model: Leaky 1-pole highpass + non-linear wire spring rattle impulses
            wireStateL_ = wireStateL_ * 0.80f + whiteL * 0.20f;
            const float hpL = whiteL - wireStateL_;
            wireStateR_ = wireStateR_ * 0.80f + whiteR * 0.20f;
            const float hpR = whiteR - wireStateR_;

            const float rattleL = (std::abs(whiteL) > 0.65f) ? (whiteL * 1.45f) : (whiteL * 0.45f);
            const float rattleR = (std::abs(whiteR) > 0.65f) ? (whiteR * 1.45f) : (whiteR * 0.45f);

            return { (hpL * 0.62f + rattleL * 0.38f) * 1.15f, (hpR * 0.62f + rattleR * 0.38f) * 1.15f };
        }
        case NoiseType::WhiteSizzle: {
            // Pristine, bright EDM high sizzle with gentle saturation
            return { std::tanh(whiteL * 1.3f) * 0.95f, std::tanh(whiteR * 1.3f) * 0.95f };
        }
        case NoiseType::VelvetAir: {
            // Silky smooth velvet air: sparse bipolar impulses through leaky smoothing integrator
            const float pulseL = (std::abs(whiteL) > 0.58f) ? (whiteL > 0.0f ? 1.0f : -1.0f) : 0.0f;
            const float pulseR = (std::abs(whiteR) > 0.58f) ? (whiteR > 0.0f ? 1.0f : -1.0f) : 0.0f;

            velvetStateL_ = velvetStateL_ * 0.68f + pulseL * 0.32f;
            velvetStateR_ = velvetStateR_ * 0.68f + pulseR * 0.32f;

            return { velvetStateL_ * 1.65f, velvetStateR_ * 1.65f };
        }
        case NoiseType::MetallicRing: {
            // 4 Inharmonic metallic partials in true stereo with micro-detuning
            const float nyquist = static_cast<float>(sampleRate_) * 0.45f;
            constexpr std::array<float, 4> kRatiosL = { 1.832f, 2.414f, 3.873f, 4.618f };
            constexpr std::array<float, 4> kRatiosR = { 1.835f, 2.419f, 3.881f, 4.627f };
            const float dt = 2.0f * pi() / static_cast<float>(sampleRate_);

            float sumL = 0.0f;
            float sumR = 0.0f;

            for (int k = 0; k < 4; ++k) {
                const float fL = std::clamp(tunedHz * kRatiosL[k], 250.0f, nyquist);
                const float fR = std::clamp(tunedHz * kRatiosR[k], 250.0f, nyquist);
                metalPhasesL_[k] = wrapTwoPi(metalPhasesL_[k] + fL * dt);
                metalPhasesR_[k] = wrapTwoPi(metalPhasesR_[k] + fR * dt);

                sumL += std::sin(metalPhasesL_[k]);
                sumR += std::sin(metalPhasesR_[k]);
            }

            const float ringL = (sumL * 0.25f) * (0.6f + 0.4f * overtoneEnv);
            const float ringR = (sumR * 0.25f) * (0.6f + 0.4f * overtoneEnv);

            return { ringL * 0.60f + whiteL * ringL * 0.40f, ringR * 0.60f + whiteR * ringR * 0.40f };
        }
    }
    return { whiteL, whiteR };
}

float SmackSynth::getEnvelopeProgress() const noexcept {
    if (!isPlaying_ || totalDurationSamples_ <= 0) {
        return 1.0f;
    }
    return std::clamp(static_cast<float>(totalSamplesPlayed_) / static_cast<float>(totalDurationSamples_), 0.0f, 1.0f);
}

StereoFrame SmackSynth::processSample(const SmackSynthSettings& settings) noexcept {
    if (!isPlaying_) {
        return { 0.0f, 0.0f };
    }

    const int currentSample = totalSamplesPlayed_++;
    if (totalSamplesPlayed_ >= totalDurationSamples_) {
        isPlaying_ = false;
        return { 0.0f, 0.0f };
    }

    const float baseHz = noteToHz(settings.note, settings.octave, settings.fineCents);
    const float dt = 1.0f / static_cast<float>(sampleRate_);
    const float nyquistLimit = static_cast<float>(sampleRate_) * 0.46f;

    // 1. Evaluate Flam / Pre-Strike Micro-Hits
    float flamToneL = 0.0f;
    float flamToneR = 0.0f;
    float flamNoiseL = 0.0f;
    float flamNoiseR = 0.0f;
    float flamClickL = 0.0f;
    float flamClickR = 0.0f;

    if (preStrikeCount_ > 1 && settings.preStrikeLevel > 0.001f) {
        const float effectiveBurstDecay = std::clamp(settings.preStrikeDecayMs, 2.0f, settings.preStrikeSpreadMs * 0.85f);
        const float burstDecaySamples = (effectiveBurstDecay * 0.001f) * static_cast<float>(sampleRate_);

        for (int i = 0; i < preStrikeCount_; ++i) {
            const int age = currentSample - burstStartSamples_[i];
            if (age >= 0 && static_cast<float>(age) < burstDecaySamples * 4.0f) {
                const float env = std::exp(-static_cast<float>(age) / std::max(1.0f, burstDecaySamples));
                const float pan = burstPans_[i];
                const float gainL = std::clamp(0.5f * (1.0f - pan), 0.0f, 1.0f);
                const float gainR = std::clamp(0.5f * (1.0f + pan), 0.0f, 1.0f);
                const float amp = env * burstAmplitudes_[i];

                // Flam micro-transient strike & membrane pulse
                const float flamChirp = std::sin(2.0f * pi() * (baseHz * 2.2f) * (static_cast<float>(age) * dt));
                flamToneL += amp * gainL * flamChirp * 0.5f;
                flamToneR += amp * gainR * flamChirp * 0.5f;

                flamNoiseL += amp * gainL * 0.8f;
                flamNoiseR += amp * gainR * 0.8f;

                if (age < static_cast<int>(sampleRate_ * 0.003)) {
                    const float click = (1.0f - static_cast<float>(age) / (0.003f * static_cast<float>(sampleRate_)));
                    flamClickL += click * amp * gainL * 0.45f;
                    flamClickR += click * amp * gainR * 0.45f;
                }
            }
        }
    }

    // 2. Main Strike Timing & Envelopes
    const int mainAge = currentSample - mainSmackSample_;
    float toneEnv = 0.0f;
    float overtoneEnv = 0.0f;
    float noiseEnv = 0.0f;
    float clickSample = 0.0f;

    if (mainAge >= 0) {
        const float t = static_cast<float>(mainAge);
        const float toneDecaySamples = (settings.toneDecayMs * 0.001f) * static_cast<float>(sampleRate_);
        toneEnv = std::exp(-t / std::max(1.0f, toneDecaySamples));

        // Overtones ring for ~42% of tone sustain for authentic acoustic/FM physics
        const float overtoneDecaySamples = toneDecaySamples * 0.42f;
        overtoneEnv = std::exp(-t / std::max(1.0f, overtoneDecaySamples));

        // Noise Envelope: Attack, Hold, and Shaped Decay
        const float attackSamples = (settings.noiseAttackMs * 0.001f) * static_cast<float>(sampleRate_);
        const float holdSamples = (settings.noiseHoldMs * 0.001f) * static_cast<float>(sampleRate_);
        const float decaySamples = (settings.noiseDecayMs * 0.001f) * static_cast<float>(sampleRate_);

        if (attackSamples > 0.0f && t < attackSamples) {
            noiseEnv = std::sin(0.5f * pi() * (t / attackSamples));
        } else if (t < (attackSamples + holdSamples)) {
            noiseEnv = 1.0f;
        } else {
            const float decayT = t - (attackSamples + holdSamples);
            const float normT = decayT / std::max(1.0f, decaySamples);
            const float expDecay = std::exp(-normT * 2.8f);

            if (settings.noiseCurve < 0.0f) {
                // Snappier, steeper concave drop
                noiseEnv = std::exp(-normT * (2.8f - settings.noiseCurve * 3.8f));
            } else {
                const float linearDecay = std::max(0.0f, 1.0f - normT);
                noiseEnv = (1.0f - settings.noiseCurve) * expDecay + settings.noiseCurve * linearDecay;
            }

            if (normT > 5.0f || noiseEnv < 1e-4f) {
                noiseEnv = 0.0f;
            }
        }

        // 3. Transient Strike Layer (Stick / Mallet / Rimshot / Laser)
        if (mainAge < static_cast<int>(sampleRate_ * 0.010)) {
            const float clickT = static_cast<float>(mainAge) * dt;
            switch (settings.clickType) {
                case ClickType::Stick: {
                    // Fast downward pitch sweep (8.5 kHz down to clickFreq) + high-passed noise burst
                    const float envFast = std::exp(-static_cast<float>(mainAge) / (0.0015f * static_cast<float>(sampleRate_)));
                    const float sweepHz = std::clamp(settings.clickFreqHz + 4500.0f * envFast, 500.0f, nyquistLimit);
                    clickPhase_ = wrapTwoPi(clickPhase_ + 2.0f * pi() * sweepHz * dt);
                    const float whiteImpulse = nextRandomFloat(prngStateL_) * envFast * 0.35f;
                    clickSample = (std::sin(clickPhase_) * 0.68f + whiteImpulse) * envFast * settings.clickLevel * 1.40f;
                    break;
                }
                case ClickType::Mallet: {
                    // Warm low-mid acoustic thud with smooth saturation
                    const float envMallet = std::exp(-static_cast<float>(mainAge) / (0.0032f * static_cast<float>(sampleRate_)));
                    const float thumpFreq = std::clamp(settings.clickFreqHz * 0.28f, 120.0f, 850.0f);
                    const float sweepThump = thumpFreq * (1.0f + 1.2f * envMallet);
                    clickPhase_ = wrapTwoPi(clickPhase_ + 2.0f * pi() * sweepThump * dt);
                    clickSample = std::tanh(std::sin(clickPhase_) * 1.8f) * envMallet * settings.clickLevel * 1.30f;
                    break;
                }
                case ClickType::Rimshot: {
                    // Dual-mode high-tension wood/metal rim crack
                    const float envRim = std::exp(-static_cast<float>(mainAge) / (0.0020f * static_cast<float>(sampleRate_)));
                    const float f1 = std::clamp(settings.clickFreqHz, 500.0f, nyquistLimit);
                    const float f2 = std::clamp(settings.clickFreqHz * 2.18f, 1000.0f, nyquistLimit);
                    clickPhase_ = wrapTwoPi(clickPhase_ + 2.0f * pi() * f1 * dt);
                    clickPhase2_ = wrapTwoPi(clickPhase2_ + 2.0f * pi() * f2 * dt);
                    const float rawRim = std::sin(clickPhase_) * 0.72f + std::sin(clickPhase2_) * 0.42f;
                    clickSample = std::tanh(rawRim * 1.7f) * envRim * settings.clickLevel * 1.45f;
                    break;
                }
                case ClickType::Laser: {
                    // Rapid exponential chirp downward sweep for electro/psy hits
                    const float envLaser = std::exp(-static_cast<float>(mainAge) / (0.0045f * static_cast<float>(sampleRate_)));
                    const float sweepHz = std::clamp(settings.clickFreqHz * (0.4f + 2.8f * envLaser), 100.0f, nyquistLimit);
                    clickPhase_ = wrapTwoPi(clickPhase_ + 2.0f * pi() * sweepHz * dt);
                    clickSample = std::sin(clickPhase_) * envLaser * settings.clickLevel * 1.35f;
                    break;
                }
            }
        }
    }

    // 4. Stereo Noise / Wire / Air Synthesis
    const auto rawNoise = generateStereoNoise(settings.noiseType, baseHz, overtoneEnv);
    const float noiseAmp = noiseEnv * settings.noiseLevel * velocity_;

    const float totalNoiseL = (rawNoise.left * flamNoiseL + rawNoise.left * noiseAmp);
    const float totalNoiseR = (rawNoise.right * flamNoiseR + rawNoise.right * noiseAmp);

    // Filter noise layer with highpass and lowpass biquads
    if (std::abs(settings.noiseHPF - lastHPF_) > 0.1f) {
        lastHPF_ = settings.noiseHPF;
        computeHighpassCoeffs(lastHPF_, sampleRate_, b0_h_, b1_h_, b2_h_, a1_h_, a2_h_);
    }
    const float hpL = processBiquad(totalNoiseL, noiseHPF_L_, b0_h_, b1_h_, b2_h_, a1_h_, a2_h_);
    const float hpR = processBiquad(totalNoiseR, noiseHPF_R_, b0_h_, b1_h_, b2_h_, a1_h_, a2_h_);

    if (std::abs(settings.noiseLPF - lastLPF_) > 0.1f) {
        lastLPF_ = settings.noiseLPF;
        computeLowpassCoeffs(lastLPF_, sampleRate_, b0_l_, b1_l_, b2_l_, a1_l_, a2_l_);
    }
    float filteredNoiseL = processBiquad(hpL, noiseLPF_L_, b0_l_, b1_l_, b2_l_, a1_l_, a2_l_);
    float filteredNoiseR = processBiquad(hpR, noiseLPF_R_, b0_l_, b1_l_, b2_l_, a1_l_, a2_l_);

    // Mid/Side stereo width processing on noise layer
    if (settings.noiseStereoWidth != 1.0f) {
        const float mid = (filteredNoiseL + filteredNoiseR) * 0.5f;
        const float side = (filteredNoiseL - filteredNoiseR) * 0.5f * settings.noiseStereoWidth;
        filteredNoiseL = mid + side;
        filteredNoiseR = mid - side;
    }

    // 5. Tonal Drum / Tom / Bell Body Synthesis
    float toneL = 0.0f;
    float toneR = 0.0f;

    if (mainAge >= 0 && settings.toneLevel > 0.001f) {
        const float pitchDecaySamples = (settings.tonePitchDecayMs * 0.001f) * static_cast<float>(sampleRate_);
        const float pitchEnvFrac = std::exp(-static_cast<float>(mainAge) / std::max(1.0f, pitchDecaySamples));
        const float pitchSemitones = static_cast<float>(semitoneOffset_) + settings.tonePitchEnv * pitchEnvFrac;
        const float tunedHz = std::clamp(baseHz * std::pow(2.0f, pitchSemitones / 12.0f), 20.0f, nyquistLimit);

        // Update resonant shell bandpass filter coefficients when needed
        if (std::abs(settings.toneFreqHz - lastShellFreq_) > 0.1f || std::abs(settings.toneQ - lastShellQ_) > 0.05f) {
            lastShellFreq_ = settings.toneFreqHz;
            lastShellQ_ = settings.toneQ;
            computeBandpassCoeffs(lastShellFreq_, lastShellQ_, sampleRate_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_);
        }

        switch (settings.voiceMode) {
            case VoiceMode::DanceSnare: {
                // Dual-membrane tuned drum head (Mode 1 + Mode 2) + FM snap + Shell Resonator
                const float modHz = std::min(tunedHz * 2.14f, nyquistLimit);
                const float mode2Hz = std::min(tunedHz * 1.58f, nyquistLimit);

                phaseMod1_ = wrapTwoPi(phaseMod1_ + 2.0f * pi() * modHz * dt);
                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * mode2Hz * dt);

                phaseFundR_ = wrapTwoPi(phaseFundR_ + 2.0f * pi() * (tunedHz * 1.002f) * dt);
                phaseMode2R_ = wrapTwoPi(phaseMode2R_ + 2.0f * pi() * (mode2Hz * 1.003f) * dt);

                const float fmIndex = settings.fmAmount * 2.6f * overtoneEnv;
                const float memL = std::sin(phaseFund_ + fmIndex * std::sin(phaseMod1_)) * 0.74f
                    + std::sin(phaseMode2_) * 0.32f * overtoneEnv;
                const float memR = std::sin(phaseFundR_ + fmIndex * std::sin(phaseMod1_ * 1.002f)) * 0.74f
                    + std::sin(phaseMode2R_) * 0.32f * overtoneEnv;

                // Resonant shell excitation: membrane impact + wire rattle
                const float shellExcL = memL * 0.55f + rawNoise.left * 0.35f * overtoneEnv;
                const float shellExcR = memR * 0.55f + rawNoise.right * 0.35f * overtoneEnv;
                const float shellResL = processBiquad(shellExcL, shellFilterL_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_) * 1.6f;
                const float shellResR = processBiquad(shellExcR, shellFilterR_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_) * 1.6f;

                toneL = (std::tanh(memL * 1.35f) * toneEnv + shellResL * toneEnv * 0.50f) * 1.38f;
                toneR = (std::tanh(memR * 1.35f) * toneEnv + shellResR * toneEnv * 0.50f) * 1.38f;
                break;
            }
            case VoiceMode::ClubTom: {
                // 4 Circular membrane Bessel modes (0,1 : 1,1 : 2,1 : 0,2) with physical modal decay
                const float f2 = std::min(tunedHz * 1.593f, nyquistLimit);
                const float f3 = std::min(tunedHz * 2.136f, nyquistLimit);
                const float f4 = std::min(tunedHz * 2.295f, nyquistLimit);

                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * f2 * dt);
                phaseMode3_ = wrapTwoPi(phaseMode3_ + 2.0f * pi() * f3 * dt);
                phaseMode4_ = wrapTwoPi(phaseMode4_ + 2.0f * pi() * f4 * dt);

                const float t = static_cast<float>(mainAge);
                const float toneDecaySamples = (settings.toneDecayMs * 0.001f) * static_cast<float>(sampleRate_);
                const float e2 = std::exp(-t / std::max(1.0f, toneDecaySamples * 0.48f));
                const float e3 = std::exp(-t / std::max(1.0f, toneDecaySamples * 0.32f));
                const float e4 = std::exp(-t / std::max(1.0f, toneDecaySamples * 0.20f));

                const float subFund = std::sin(phaseFund_);
                const float besselModes = (std::sin(phaseMode2_) * 0.48f * e2
                    + std::sin(phaseMode3_) * 0.28f * e3
                    + std::sin(phaseMode4_) * 0.18f * e4) * settings.fmAmount;

                const float rawTom = subFund * toneEnv + besselModes;
                // Warm asymmetric analog saturation for chest-slapping club weight
                const float warmTom = std::tanh(rawTom * 1.45f) * (1.0f + 0.08f * std::tanh(rawTom)) * 1.42f;

                toneL = warmTom;
                toneR = warmTom;
                break;
            }
            case VoiceMode::FmBell: {
                // 2-Operator FM metallic bell with decaying FM index + shimmer partial in wide stereo
                const float r1 = 1.4142f + settings.fmAmount * 1.3416f;
                const float r2 = 2.7560f + settings.fmAmount * 0.6580f;
                const float mod1Hz = std::min(tunedHz * r1, nyquistLimit);
                const float chimeHz = std::min(tunedHz * r2, nyquistLimit);

                phaseMod1_ = wrapTwoPi(phaseMod1_ + 2.0f * pi() * mod1Hz * dt);
                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseFundR_ = wrapTwoPi(phaseFundR_ + 2.0f * pi() * (tunedHz * 1.0022f) * dt);
                phaseMode3_ = wrapTwoPi(phaseMode3_ + 2.0f * pi() * chimeHz * dt);

                const float t = static_cast<float>(mainAge);
                const float toneDecaySamples = (settings.toneDecayMs * 0.001f) * static_cast<float>(sampleRate_);
                const float fmEnv = std::exp(-t / std::max(1.0f, toneDecaySamples * 0.70f));
                const float modIndex = (0.6f + settings.fmAmount * 4.2f) * fmEnv;

                const float modSignalL = std::sin(phaseMod1_) * modIndex;
                const float modSignalR = std::sin(phaseMod1_ * 1.0015f + 0.12f) * modIndex * 0.98f;

                const float bellL = std::sin(phaseFund_ + modSignalL) * 0.74f + std::sin(phaseMode3_) * 0.28f * overtoneEnv;
                const float bellR = std::sin(phaseFundR_ + modSignalR) * 0.74f + std::cos(phaseMode3_) * 0.28f * overtoneEnv;

                toneL = bellL * toneEnv * 1.38f;
                toneR = bellR * toneEnv * 1.38f;
                break;
            }
            case VoiceMode::PrydaRimSmack: {
                // Iconic arena rimshot: Heavy chest punch core + woody/metallic rim formant resonator
                const float mode2Hz = std::min(tunedHz * 1.78f, nyquistLimit);
                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * mode2Hz * dt);

                const float punchCore = std::sin(phaseFund_) * toneEnv + std::sin(phaseMode2_) * overtoneEnv * settings.fmAmount * 0.65f;

                // Smooth exponential excitation burst (zero step discontinuities!)
                const float rimBurstEnv = std::exp(-static_cast<float>(mainAge) / (0.016f * static_cast<float>(sampleRate_)));
                const float rimExcL = (punchCore * 0.6f + rawNoise.left * 0.4f) * rimBurstEnv;
                const float rimExcR = (punchCore * 0.6f + rawNoise.right * 0.4f) * rimBurstEnv;

                const float rimFormantL = processBiquad(rimExcL, shellFilterL_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_) * 2.2f;
                const float rimFormantR = processBiquad(rimExcR, shellFilterR_, b0_s_, b1_s_, b2_s_, a1_s_, a2_s_) * 2.2f;

                toneL = std::tanh((punchCore * 1.15f + rimFormantL * 0.95f) * 1.38f) * 1.45f;
                toneR = std::tanh((punchCore * 1.15f + rimFormantR * 0.95f) * 1.38f) * 1.45f;
                break;
            }
        }

        toneL *= settings.toneLevel * velocity_;
        toneR *= settings.toneLevel * velocity_;
    }

    // 6. Combine Dry Layers (Noise + Tone + Transient Strike + Flam Roll)
    const float dryL = filteredNoiseL + toneL + (clickSample + flamClickL) * velocity_ + flamToneL * velocity_;
    const float dryR = filteredNoiseR + toneR + (clickSample + flamClickR) * velocity_ + flamToneR * velocity_;

    // 7. Gated Trance Reverb Processing
    StereoFrame reverbOut = { 0.0f, 0.0f };
    if (settings.reverbMix > 0.001f) {
        reverbOut = reverb_.process(
            dryL, dryR,
            settings.reverbSize,
            settings.reverbDecayMs,
            settings.reverbDamp,
            settings.reverbGateMs,
            sampleRate_);
    }

    const float outL = dryL * (1.0f - settings.reverbMix * 0.40f) + reverbOut.left * settings.reverbMix;
    const float outR = dryR * (1.0f - settings.reverbMix * 0.40f) + reverbOut.right * settings.reverbMix;

    return { outL, outR };
}

void SmackSynth::renderBlock(
    std::vector<float>& leftOut,
    std::vector<float>& rightOut,
    const SmackSynthSettings& settings,
    float velocity,
    int semitoneOffset) noexcept {
    trigger(settings, velocity, semitoneOffset);

    leftOut.resize(totalDurationSamples_);
    rightOut.resize(totalDurationSamples_);

    for (int i = 0; i < totalDurationSamples_; ++i) {
        const auto frame = processSample(settings);
        leftOut[i] = frame.left;
        rightOut[i] = frame.right;
    }
}

} // namespace opensmacker::core
