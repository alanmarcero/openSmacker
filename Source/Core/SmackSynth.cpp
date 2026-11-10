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
// GatedStereoReverb
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
    diffL1_.prepare(std::max(8, static_cast<int>(sampleRate * 0.0031)), 0.65f);
    diffL2_.prepare(std::max(8, static_cast<int>(sampleRate * 0.0079)), 0.60f);
    diffL3_.prepare(std::max(8, static_cast<int>(sampleRate * 0.0113)), 0.55f);
    diffR1_.prepare(std::max(8, static_cast<int>(sampleRate * 0.0023)), 0.65f);
    diffR2_.prepare(std::max(8, static_cast<int>(sampleRate * 0.0058)), 0.60f);
    diffR3_.prepare(std::max(8, static_cast<int>(sampleRate * 0.0097)), 0.55f);

    const int margin = static_cast<int>(sampleRate * 0.003);
    delayL1_.assign(std::max(16, static_cast<int>(sampleRate * 0.0297)) + margin, 0.0f);
    delayL2_.assign(std::max(16, static_cast<int>(sampleRate * 0.0371)) + margin, 0.0f);
    delayL3_.assign(std::max(16, static_cast<int>(sampleRate * 0.0439)) + margin, 0.0f);
    delayR1_.assign(std::max(16, static_cast<int>(sampleRate * 0.0313)) + margin, 0.0f);
    delayR2_.assign(std::max(16, static_cast<int>(sampleRate * 0.0419)) + margin, 0.0f);
    delayR3_.assign(std::max(16, static_cast<int>(sampleRate * 0.0487)) + margin, 0.0f);
    reset();
}

void GatedStereoReverb::reset() noexcept {
    diffL1_.reset();
    diffL2_.reset();
    diffL3_.reset();
    diffR1_.reset();
    diffR2_.reset();
    diffR3_.reset();
    std::fill(delayL1_.begin(), delayL1_.end(), 0.0f);
    std::fill(delayL2_.begin(), delayL2_.end(), 0.0f);
    std::fill(delayL3_.begin(), delayL3_.end(), 0.0f);
    std::fill(delayR1_.begin(), delayR1_.end(), 0.0f);
    std::fill(delayR2_.begin(), delayR2_.end(), 0.0f);
    std::fill(delayR3_.begin(), delayR3_.end(), 0.0f);
    writeL1_ = writeL2_ = writeL3_ = 0;
    writeR1_ = writeR2_ = writeR3_ = 0;
    dampStateL1_ = dampStateL2_ = 0.0f;
    dampStateR1_ = dampStateR2_ = 0.0f;
    hpfStateL_ = hpfStateR_ = 0.0f;
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
    if (delayL1_.empty()) {
        return { inLeft, inRight };
    }

    sampleCounter_++;

    if (gateMs > 0.0f) {
        const float gateSamples = (gateMs * 0.001f) * static_cast<float>(sampleRate);
        const float releaseSamples = (18.0f * 0.001f) * static_cast<float>(sampleRate);
        if (static_cast<float>(sampleCounter_) > gateSamples) {
            const float samplesOver = static_cast<float>(sampleCounter_) - gateSamples;
            gateEnvelope_ = std::max(0.0f, 1.0f - (samplesOver / releaseSamples));
        } else {
            gateEnvelope_ = 1.0f;
        }
    } else {
        gateEnvelope_ = 1.0f;
    }

    if (gateEnvelope_ <= 0.0f) {
        return { 0.0f, 0.0f };
    }

    const float hpfCoeff = static_cast<float>(1.0 - std::exp(-2.0 * pi() * 140.0 / sampleRate));
    hpfStateL_ += hpfCoeff * (inLeft - hpfStateL_);
    const float cleanInL = inLeft - hpfStateL_;
    hpfStateR_ += hpfCoeff * (inRight - hpfStateR_);
    const float cleanInR = inRight - hpfStateR_;

    const float diffInL = diffL3_.process(diffL2_.process(diffL1_.process(cleanInL)));
    const float diffInR = diffR3_.process(diffR2_.process(diffR1_.process(cleanInR)));

    const float feedback = std::clamp(std::pow(0.001f, 30.0f / std::max(10.0f, decayMs)), 0.1f, 0.94f) * (0.6f + 0.4f * size);
    const float dampFactor = std::clamp(damp, 0.0f, 0.85f);

    lfoPhaseL_ += 2.0 * pi() * 0.73 / sampleRate;
    if (lfoPhaseL_ > 2.0 * pi()) lfoPhaseL_ -= 2.0 * pi();
    lfoPhaseR_ += 2.0 * pi() * 0.91 / sampleRate;
    if (lfoPhaseR_ > 2.0 * pi()) lfoPhaseR_ -= 2.0 * pi();

    const double modL = std::sin(lfoPhaseL_) * (0.0008 * sampleRate);
    const double modR = std::cos(lfoPhaseR_) * (0.0008 * sampleRate);

    const double baseL1 = sampleRate * 0.0297;
    const double baseL2 = sampleRate * 0.0371;
    const double baseL3 = sampleRate * 0.0439;
    const double baseR1 = sampleRate * 0.0313;
    const double baseR2 = sampleRate * 0.0419;
    const double baseR3 = sampleRate * 0.0487;

    const float outL1 = readHermite(delayL1_, baseL1 + modL, writeL1_);
    const float outL2 = readHermite(delayL2_, baseL2 - modR, writeL2_);
    const float outL3 = readHermite(delayL3_, baseL3 + modR * 0.5, writeL3_);

    const float outR1 = readHermite(delayR1_, baseR1 + modR, writeR1_);
    const float outR2 = readHermite(delayR2_, baseR2 - modL, writeR2_);
    const float outR3 = readHermite(delayR3_, baseR3 - modL * 0.5, writeR3_);

    const float wetL = 0.35f * (outL1 + outL2 + outL3);
    const float wetR = 0.35f * (outR1 + outR2 + outR3);

    dampStateL1_ = dampStateL1_ * dampFactor + wetL * (1.0f - dampFactor);
    dampStateR1_ = dampStateR1_ * dampFactor + wetR * (1.0f - dampFactor);

    delayL1_[static_cast<size_t>(writeL1_)] = diffInL + dampStateR1_ * feedback;
    delayL2_[static_cast<size_t>(writeL2_)] = diffInL * 0.65f + dampStateL1_ * feedback * 0.75f;
    delayL3_[static_cast<size_t>(writeL3_)] = diffInL * 0.5f + (dampStateR1_ - dampStateL1_) * feedback * 0.45f;

    delayR1_[static_cast<size_t>(writeR1_)] = diffInR + dampStateL1_ * feedback;
    delayR2_[static_cast<size_t>(writeR2_)] = diffInR * 0.65f + dampStateR1_ * feedback * 0.75f;
    delayR3_[static_cast<size_t>(writeR3_)] = diffInR * 0.5f + (dampStateL1_ - dampStateR1_) * feedback * 0.45f;

    writeL1_ = (writeL1_ + 1) % static_cast<int>(delayL1_.size());
    writeL2_ = (writeL2_ + 1) % static_cast<int>(delayL2_.size());
    writeL3_ = (writeL3_ + 1) % static_cast<int>(delayL3_.size());
    writeR1_ = (writeR1_ + 1) % static_cast<int>(delayR1_.size());
    writeR2_ = (writeR2_ + 1) % static_cast<int>(delayR2_.size());
    writeR3_ = (writeR3_ + 1) % static_cast<int>(delayR3_.size());

    const float outLeft = (wetL - wetR * 0.25f) * gateEnvelope_;
    const float outRight = (wetR - wetL * 0.25f) * gateEnvelope_;
    return { outLeft, outRight };
}

// ============================================================================
// SmackSynth
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
    phaseMod_ = 0.0f;
    metalPhaseA_ = 0.0f;
    metalPhaseB_ = 0.0f;
    lastHPF_ = -1.0f;
    lastLPF_ = -1.0f;
    shellFilterL_.reset();
    shellFilterR_.reset();
    noiseHPF_L_.reset();
    noiseHPF_R_.reset();
    noiseLPF_L_.reset();
    noiseLPF_R_.reset();
    reverb_.reset();
    wireState0_ = wireState1_ = 0.0f;
}

void SmackSynth::trigger(const SmackSynthSettings& settings, float velocity, int semitoneOffset) noexcept {
    reset();
    isPlaying_ = true;
    velocity_ = std::clamp(velocity, 0.0f, 1.0f);
    semitoneOffset_ = semitoneOffset;

    preStrikeCount_ = std::clamp(settings.preStrikeCount, 1, 5);
    const float spreadSamples = (settings.preStrikeSpreadMs * 0.001f) * static_cast<float>(sampleRate_);

    // When preStrikeCount == 1, no pre-flam delay before the main hit unless preStrikeLevel > 0.01f and count > 1
    if (preStrikeCount_ <= 1) {
        burstStartSamples_[0] = 0;
        burstAmplitudes_[0] = 0.0f;
        burstPans_[0] = 0.0f;
        mainSmackSample_ = 0;
    } else {
        for (int i = 0; i < preStrikeCount_; ++i) {
            const float jitterTime = (nextRandomFloat(prngState_) * 0.4f) * settings.preStrikeJitter * spreadSamples;
            burstStartSamples_[i] = std::max(0, static_cast<int>(static_cast<float>(i) * spreadSamples + jitterTime));
            const float ramp = static_cast<float>(i + 1) / static_cast<float>(preStrikeCount_);
            const float jitterAmp = 0.85f + 0.15f * ramp + nextRandomFloat(prngState_) * 0.15f * settings.preStrikeJitter;
            burstAmplitudes_[i] = std::clamp(settings.preStrikeLevel * jitterAmp, 0.05f, 1.0f);

            const float dir = (i % 2 == 0) ? -1.0f : 1.0f;
            burstPans_[i] = std::clamp(dir * settings.preStrikeFlamSpread * ramp, -1.0f, 1.0f);
        }
        mainSmackSample_ = static_cast<int>(static_cast<float>(preStrikeCount_) * spreadSamples);
    }

    const float preTimeMs = (preStrikeCount_ > 1) ? settings.preStrikeSpreadMs * static_cast<float>(preStrikeCount_) : 0.0f;
    const float totalTimeMs = preTimeMs
        + std::max({ settings.noiseDecayMs, settings.toneDecayMs, settings.reverbDecayMs, settings.reverbGateMs });
    totalDurationSamples_ = static_cast<int>((totalTimeMs * 0.001f + 0.12f) * static_cast<float>(sampleRate_));
}

float SmackSynth::generateNoiseSample(NoiseType type, float tunedHz) noexcept {
    const float white = nextRandomFloat(prngState_);

    if (type == NoiseType::SnareWires) {
        // Crisp snare-wire rattle: high-frequency emphasis + non-linear wire rattle impulses
        wireState0_ = 0.88f * wireState0_ + white * 0.45f;
        const float hpWire = white - wireState0_;
        const float rattleSpike = (std::abs(white) > 0.72f) ? (white * 1.35f) : (white * 0.55f);
        return (hpWire * 0.65f + rattleSpike * 0.35f);
    }

    if (type == NoiseType::VelvetAir) {
        const float r = nextRandomFloat(prngState_);
        if (std::abs(r) > 0.62f) {
            return (r > 0.0f ? 1.0f : -1.0f) * (0.8f + 0.2f * std::abs(white));
        }
        return 0.0f;
    }

    if (type == NoiseType::MetallicRing) {
        // Inharmonic metallic shimmer modulated by noise for bells, chimes, and metallic snares
        const float fA = std::clamp(tunedHz * 2.414f, 400.0f, static_cast<float>(sampleRate_) * 0.42f);
        const float fB = std::clamp(tunedHz * 3.873f, 600.0f, static_cast<float>(sampleRate_) * 0.45f);
        metalPhaseA_ = wrapTwoPi(metalPhaseA_ + 2.0f * pi() * fA / static_cast<float>(sampleRate_));
        metalPhaseB_ = wrapTwoPi(metalPhaseB_ + 2.0f * pi() * fB / static_cast<float>(sampleRate_));
        const float ring = std::sin(metalPhaseA_) * std::cos(metalPhaseB_);
        return ring * 0.55f + white * ring * 0.45f;
    }

    return white; // WhiteSizzle
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

    // 1. Evaluate Flam / Roll Pre-Strikes
    float preGainL = 0.0f;
    float preGainR = 0.0f;
    if (preStrikeCount_ > 1 && settings.preStrikeLevel > 0.001f) {
        const float effectiveBurstDecayMs = std::max(2.0f, std::min(settings.preStrikeDecayMs, settings.preStrikeSpreadMs * 0.75f));
        const float burstDecaySamples = (effectiveBurstDecayMs * 0.001f) * static_cast<float>(sampleRate_);

        for (int i = 0; i < preStrikeCount_; ++i) {
            const int age = currentSample - burstStartSamples_[i];
            if (age >= 0 && static_cast<float>(age) < burstDecaySamples * 4.0f) {
                const float env = std::exp(-static_cast<float>(age) / std::max(1.0f, burstDecaySamples));
                const float pan = burstPans_[i];
                const float gainL = std::clamp(0.5f * (1.0f - pan), 0.0f, 1.0f);
                const float gainR = std::clamp(0.5f * (1.0f + pan), 0.0f, 1.0f);
                const float amp = env * burstAmplitudes_[i];
                preGainL += amp * gainL * 1.414f;
                preGainR += amp * gainR * 1.414f;
            }
        }
    }

    // 2. Main Hit Envelopes & Transient Strike
    const int mainAge = currentSample - mainSmackSample_;
    float noiseEnv = 0.0f;
    float toneEnv = 0.0f;
    float clickSample = 0.0f;

    if (mainAge >= 0) {
        const float attackSamples = (settings.noiseAttackMs * 0.001f) * static_cast<float>(sampleRate_);
        const float holdSamples = (settings.noiseHoldMs * 0.001f) * static_cast<float>(sampleRate_);
        const float decaySamples = (settings.noiseDecayMs * 0.001f) * static_cast<float>(sampleRate_);
        const float toneDecaySamples = (settings.toneDecayMs * 0.001f) * static_cast<float>(sampleRate_);

        const float t = static_cast<float>(mainAge);
        if (t < attackSamples && attackSamples > 0.0f) {
            noiseEnv = t / attackSamples;
        } else if (t < (attackSamples + holdSamples)) {
            noiseEnv = 1.0f;
        } else {
            const float decayT = t - (attackSamples + holdSamples);
            const float linearFraction = std::clamp(decayT / std::max(1.0f, decaySamples), 0.0f, 1.0f);
            const float expDecay = std::exp(-decayT / std::max(1.0f, decaySamples * 0.35f));
            if (settings.noiseCurve < 0.0f) {
                const float factor = -settings.noiseCurve;
                noiseEnv = (1.0f - factor) * (1.0f - linearFraction) + factor * expDecay;
            } else {
                const float factor = settings.noiseCurve;
                noiseEnv = (1.0f - factor) * expDecay + factor * (1.0f - linearFraction);
            }
        }

        toneEnv = std::exp(-t / std::max(1.0f, toneDecaySamples));

        // Stick / Mallet / Rimshot / Laser transient strike
        if (mainAge < static_cast<int>(sampleRate_ * 0.008)) {
            const float clickT = static_cast<float>(mainAge) / static_cast<float>(sampleRate_);
            const float envFast = std::exp(-static_cast<float>(mainAge) / (0.0009f * static_cast<float>(sampleRate_)));
            switch (settings.clickType) {
                case ClickType::Stick: {
                    const float p1 = 2.0f * pi() * settings.clickFreqHz * clickT;
                    const float p2 = 2.0f * pi() * (settings.clickFreqHz * 1.47f) * clickT;
                    clickSample = (std::sin(p1) * 0.65f + std::sin(p2) * 0.35f) * envFast * settings.clickLevel * 1.25f;
                    break;
                }
                case ClickType::Mallet: {
                    // Warm felt/rubber mallet thump for toms and bells
                    const float malletFreq = std::clamp(settings.clickFreqHz * 0.42f, 250.0f, 3500.0f);
                    const float p = 2.0f * pi() * malletFreq * clickT;
                    const float envMallet = std::exp(-static_cast<float>(mainAge) / (0.0022f * static_cast<float>(sampleRate_)));
                    clickSample = std::tanh(std::sin(p) * 1.6f) * envMallet * settings.clickLevel * 1.2f;
                    break;
                }
                case ClickType::Rimshot: {
                    // Sharp dual-mode woody/metallic rim crack
                    const float pRim1 = 2.0f * pi() * settings.clickFreqHz * clickT;
                    const float pRim2 = 2.0f * pi() * (settings.clickFreqHz * 2.18f) * clickT;
                    const float envRim = std::exp(-static_cast<float>(mainAge) / (0.0014f * static_cast<float>(sampleRate_)));
                    clickSample = std::tanh((std::sin(pRim1) + 0.7f * std::sin(pRim2)) * 2.2f) * envRim * settings.clickLevel * 1.35f;
                    break;
                }
                case ClickType::Laser: {
                    // Rapid downward chirp transient for electronic/psy snares & toms
                    const float sweepHz = settings.clickFreqHz * (1.0f + 3.0f * envFast);
                    const float pLaser = 2.0f * pi() * sweepHz * clickT;
                    clickSample = std::sin(pLaser) * envFast * settings.clickLevel * 1.3f;
                    break;
                }
            }
        }
    }

    // 3. Snare Wires / Sizzle / Metallic Air Layer
    const float rawNoiseL = generateNoiseSample(settings.noiseType, baseHz);
    const float rawNoiseR = generateNoiseSample(settings.noiseType, baseHz * 1.007f);

    const float totalNoiseL = (rawNoiseL * preGainL + rawNoiseL * noiseEnv * settings.noiseLevel) * velocity_;
    const float totalNoiseR = (rawNoiseR * preGainR + rawNoiseR * noiseEnv * settings.noiseLevel) * velocity_;

    if (std::abs(settings.noiseHPF - lastHPF_) > 0.1f) {
        lastHPF_ = settings.noiseHPF;
        computeHighpassCoeffs(lastHPF_, sampleRate_, b0_h_, b1_h_, b2_h_, a1_h_, a2_h_);
    }
    const float filteredHP_L = processBiquad(totalNoiseL, noiseHPF_L_, b0_h_, b1_h_, b2_h_, a1_h_, a2_h_);
    const float filteredHP_R = processBiquad(totalNoiseR, noiseHPF_R_, b0_h_, b1_h_, b2_h_, a1_h_, a2_h_);

    if (std::abs(settings.noiseLPF - lastLPF_) > 0.1f) {
        lastLPF_ = settings.noiseLPF;
        computeLowpassCoeffs(lastLPF_, sampleRate_, b0_l_, b1_l_, b2_l_, a1_l_, a2_l_);
    }
    float filteredNoiseL = processBiquad(filteredHP_L, noiseLPF_L_, b0_l_, b1_l_, b2_l_, a1_l_, a2_l_);
    float filteredNoiseR = processBiquad(filteredHP_R, noiseLPF_R_, b0_l_, b1_l_, b2_l_, a1_l_, a2_l_);

    // 4. Tunable Drum / Tom / Bell Body Synthesis
    float toneL = 0.0f;
    float toneR = 0.0f;
    if (mainAge >= 0 && settings.toneLevel > 0.001f) {
        const float pitchDecaySamples = (settings.tonePitchDecayMs * 0.001f) * static_cast<float>(sampleRate_);
        const float pitchEnvFrac = std::exp(-static_cast<float>(mainAge) / std::max(1.0f, pitchDecaySamples));
        const float pitchSemitones = static_cast<float>(semitoneOffset_) + settings.tonePitchEnv * pitchEnvFrac;

        const float nyquistLimit = static_cast<float>(sampleRate_) * 0.46f;
        const float tunedHz = std::clamp(baseHz * std::pow(2.0f, pitchSemitones / 12.0f), 20.0f, nyquistLimit);
        const float dt = 1.0f / static_cast<float>(sampleRate_);

        // Overtone decay is faster than fundamental for realistic acoustic/electronic percussion
        const float overtoneEnv = std::exp(-static_cast<float>(mainAge) / std::max(1.0f, (settings.toneDecayMs * 0.00045f) * static_cast<float>(sampleRate_)));

        float bodySampleL = 0.0f;
        float bodySampleR = 0.0f;

        switch (settings.voiceMode) {
            case VoiceMode::DanceSnare: {
                // Dual-mode tuned snare drum head (f0 + 1.52*f0) + FM snap + resonant shell filter
                const float modHz = std::min(tunedHz * 2.14f, nyquistLimit);
                phaseMod_ = wrapTwoPi(phaseMod_ + 2.0f * pi() * modHz * dt);
                const float fmIndex = settings.fmAmount * 2.8f * overtoneEnv;

                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * std::min(tunedHz * 1.52f, nyquistLimit) * dt);

                const float membrane = std::sin(phaseFund_ + fmIndex * std::sin(phaseMod_)) * 0.78f
                    + std::sin(phaseMode2_) * 0.35f * overtoneEnv;

                // Shell resonator centered at toneFreqHz (blended with tunedHz)
                float b0_s, b1_s, b2_s, a1_s, a2_s;
                const float shellHz = std::clamp(settings.toneFreqHz, 80.0f, nyquistLimit);
                computeBandpassCoeffs(shellHz, settings.toneQ, sampleRate_, b0_s, b1_s, b2_s, a1_s, a2_s);
                const float shellExciter = membrane * 0.6f + rawNoiseL * 0.4f * overtoneEnv;
                const float shellRes = processBiquad(shellExciter, shellFilterL_, b0_s, b1_s, b2_s, a1_s, a2_s) * 1.8f;

                bodySampleL = ( std::tanh(membrane * 1.4f) * toneEnv + shellRes * toneEnv * 0.65f ) * 1.35f;
                bodySampleR = bodySampleL;
                break;
            }
            case VoiceMode::ClubTom: {
                // Circular membrane Bessel modes (1.00 : 1.593 : 2.136) with warm analog saturation
                const float f2 = std::min(tunedHz * 1.593f, nyquistLimit);
                const float f3 = std::min(tunedHz * 2.136f, nyquistLimit);
                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * f2 * dt);
                phaseMode3_ = wrapTwoPi(phaseMode3_ + 2.0f * pi() * f3 * dt);

                const float subFundamental = std::sin(phaseFund_);
                const float besselOvertone = (std::sin(phaseMode2_) * 0.65f + std::sin(phaseMode3_) * 0.35f) * settings.fmAmount * overtoneEnv;
                const float rawTom = subFundamental * toneEnv + besselOvertone;

                // Subtle 2nd-harmonic analog warmth for deep club tom weight
                const float warmTom = std::tanh(rawTom * 1.5f);
                bodySampleL = (warmTom + 0.06f * warmTom * std::abs(warmTom)) * 1.55f;
                bodySampleR = bodySampleL;
                break;
            }
            case VoiceMode::FmBell: {
                // 2-Operator metallic FM Bell + modal chime partial with slight stereo chorusing
                const float ratio = 1.4142f + settings.fmAmount * 2.0f; // 1.414 (classic bell) up to 3.414
                const float modHz = std::min(tunedHz * ratio, nyquistLimit);
                const float chimeHz = std::min(tunedHz * 2.756f, nyquistLimit);

                phaseMod_ = wrapTwoPi(phaseMod_ + 2.0f * pi() * modHz * dt);
                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * (tunedHz * 1.0025f) * dt); // stereo shimmer
                phaseMode3_ = wrapTwoPi(phaseMode3_ + 2.0f * pi() * chimeHz * dt);

                const float modIndex = (0.6f + settings.fmAmount * 4.5f) * overtoneEnv;
                const float modSignal = std::sin(phaseMod_) * modIndex;

                const float bellL = std::sin(phaseFund_ + modSignal) * 0.75f + std::sin(phaseMode3_) * 0.28f * overtoneEnv;
                const float bellR = std::sin(phaseMode2_ + modSignal * 0.96f) * 0.75f + std::cos(phaseMode3_) * 0.28f * overtoneEnv;

                bodySampleL = bellL * toneEnv * 1.35f;
                bodySampleR = bellR * toneEnv * 1.35f;
                break;
            }
            case VoiceMode::PrydaRimSmack: {
                // Iconic arena progressive/trance rim-snare smack: punchy chest thump + high-Q woody rim formant
                const float rimHz = std::clamp(settings.toneFreqHz, 350.0f, 3200.0f);
                phaseFund_ = wrapTwoPi(phaseFund_ + 2.0f * pi() * tunedHz * dt);
                phaseMode2_ = wrapTwoPi(phaseMode2_ + 2.0f * pi() * std::min(tunedHz * 1.85f, nyquistLimit) * dt);

                const float punchCore = std::sin(phaseFund_) * toneEnv + std::sin(phaseMode2_) * overtoneEnv * settings.fmAmount * 0.7f;

                float b0_r, b1_r, b2_r, a1_r, a2_r;
                computeBandpassCoeffs(rimHz, std::max(2.5f, settings.toneQ), sampleRate_, b0_r, b1_r, b2_r, a1_r, a2_r);
                const float rimBurst = (mainAge < static_cast<int>(sampleRate_ * 0.012) ? 1.0f : 0.25f) * (rawNoiseL + punchCore);
                const float rimFormant = processBiquad(rimBurst, shellFilterL_, b0_r, b1_r, b2_r, a1_r, a2_r) * 2.4f;

                bodySampleL = std::tanh((punchCore * 1.2f + rimFormant * toneEnv) * 1.35f) * 1.45f;
                bodySampleR = bodySampleL;
                break;
            }
        }

        toneL = bodySampleL * settings.toneLevel * velocity_;
        toneR = bodySampleR * settings.toneLevel * velocity_;
    }

    // 5. Stereo Width (Mid/Side processing on noise/sizzle layer)
    if (settings.noiseStereoWidth != 1.0f) {
        const float mid = (filteredNoiseL + filteredNoiseR) * 0.5f;
        const float side = (filteredNoiseL - filteredNoiseR) * 0.5f * settings.noiseStereoWidth;
        filteredNoiseL = mid + side;
        filteredNoiseR = mid - side;
    }

    const float dryL = filteredNoiseL + toneL + clickSample * velocity_;
    const float dryR = filteredNoiseR + toneR + clickSample * velocity_;

    // 6. Gated Stereo FDN Reverb Tail
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

    const float outL = dryL * (1.0f - settings.reverbMix * 0.45f) + reverbOut.left * settings.reverbMix;
    const float outR = dryR * (1.0f - settings.reverbMix * 0.45f) + reverbOut.right * settings.reverbMix;

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
