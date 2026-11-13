#include "Effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace opensmacker::core {
namespace {
constexpr float kPi = std::numbers::pi_v<float>;

float finiteOr(float value, float fallback = 0.0f) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float dbToLin(float db) noexcept {
    return std::pow(10.0f, finiteOr(db) / 20.0f);
}

float linToDb(float linear) noexcept {
    return 20.0f * std::log10(std::max(std::abs(finiteOr(linear)), 1.0e-7f));
}

float coefficient(float timeMs, double sampleRate) noexcept {
    const double sr = std::clamp(sampleRate, 8000.0, 384000.0);
    const double seconds = std::max(0.05, static_cast<double>(finiteOr(timeMs, 10.0f))) * 0.001;
    return static_cast<float>(std::exp(-1.0 / (seconds * sr)));
}
} // namespace

void DistortionShaper::reset() noexcept {
    toneFilterState_ = 0.0f;
}

float DistortionShaper::processSample(float input, DistortionType type, float drive, float tone, double sampleRate) noexcept {
    input = finiteOr(input);
    drive = std::clamp(finiteOr(drive), 0.0f, 1.0f);
    tone = std::clamp(finiteOr(tone), -1.0f, 1.0f);
    if (drive < 0.001f)
        return input;

    const float gain = dbToLin(drive * 30.0f);
    const float x = std::clamp(input * gain, -24.0f, 24.0f);
    float shaped = x;
    switch (type) {
        case DistortionType::Tape: {
            constexpr float bias = 0.12f;
            shaped = (std::tanh(x * 1.35f + bias) - std::tanh(bias)) / std::tanh(1.35f + bias);
            break;
        }
        case DistortionType::Tube: {
            const float positive = std::tanh(x * 1.8f);
            const float negative = std::tanh(x * 1.25f);
            shaped = x >= 0.0f ? positive : negative * 0.88f;
            break;
        }
        case DistortionType::HardClip:
            shaped = std::clamp(x * 1.35f, -1.0f, 1.0f);
            break;
        case DistortionType::Wavefold: {
            float period = std::fmod(x * 0.82f + 1.0f, 4.0f);
            if (period < 0.0f)
                period += 4.0f;
            shaped = 1.0f - std::abs(period - 2.0f);
            break;
        }
    }

    shaped *= 1.0f / (1.0f + drive * 0.55f);
    const float cutoff = tone < 0.0f ? 1800.0f + (1.0f + tone) * 6200.0f : 7000.0f;
    const double sr = std::clamp(sampleRate, 8000.0, 384000.0);
    const float alpha = std::clamp(1.0f - std::exp(-2.0f * kPi * cutoff / static_cast<float>(sr)), 0.001f, 0.95f);
    toneFilterState_ += alpha * (shaped - toneFilterState_);
    if (tone < 0.0f)
        shaped = toneFilterState_;
    else if (tone > 0.0f)
        shaped += (shaped - toneFilterState_) * tone * 1.35f;

    if (!std::isfinite(shaped)) {
        reset();
        return 0.0f;
    }
    return std::clamp(shaped, -2.0f, 2.0f);
}

void StereoCompressor::prepare(double sampleRate) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    reset();
}

void StereoCompressor::reset() noexcept {
    envelopeDb_ = -120.0f;
}

void StereoCompressor::process(float& left, float& right, const CompressorSettings& settings) noexcept {
    left = finiteOr(left);
    right = finiteOr(right);
    const float dryL = left;
    const float dryR = right;
    const float peak = std::max(std::abs(left), std::abs(right));
    const float detectorDb = linToDb(peak);
    const float attack = coefficient(std::clamp(finiteOr(settings.attackMs, 5.0f), 0.1f, 50.0f), sampleRate_);
    const float release = coefficient(std::clamp(finiteOr(settings.releaseMs, 80.0f), 10.0f, 500.0f), sampleRate_);
    const float smoothing = detectorDb > envelopeDb_ ? attack : release;
    envelopeDb_ = detectorDb + smoothing * (envelopeDb_ - detectorDb);

    const float threshold = std::clamp(finiteOr(settings.thresholdDb, -14.0f), -40.0f, 0.0f);
    const float ratio = std::clamp(finiteOr(settings.ratio, 4.0f), 1.0f, 20.0f);
    const float over = envelopeDb_ - threshold;
    constexpr float kneeDb = 4.0f;
    float reductionDb = 0.0f;
    if (over > kneeDb * 0.5f) {
        reductionDb = -over * (1.0f - 1.0f / ratio);
    } else if (over > -kneeDb * 0.5f) {
        const float knee = over + kneeDb * 0.5f;
        reductionDb = -(knee * knee / (2.0f * kneeDb)) * (1.0f - 1.0f / ratio);
    }
    const float makeup = std::clamp(finiteOr(settings.makeupDb), 0.0f, 24.0f);
    const float wetGain = dbToLin(reductionDb + makeup);
    const float mix = std::clamp(finiteOr(settings.mix, 1.0f), 0.0f, 1.0f);
    left = dryL + (dryL * wetGain - dryL) * mix;
    right = dryR + (dryR * wetGain - dryR) * mix;
    if (!std::isfinite(left) || !std::isfinite(right)) {
        reset();
        left = right = 0.0f;
    }
}

void BrickwallLimiter::prepare(double sampleRate) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    reset();
}

void BrickwallLimiter::reset() noexcept {
    gain_ = 1.0f;
}

void BrickwallLimiter::process(float& left, float& right, const LimiterSettings& settings) noexcept {
    left = finiteOr(left);
    right = finiteOr(right);
    if (!settings.enabled)
        return;

    const float ceiling = dbToLin(std::clamp(finiteOr(settings.ceilingDb, -0.3f), -12.0f, 0.0f));
    const float peak = std::max(std::abs(left), std::abs(right));
    const float target = peak > ceiling && peak > 1.0e-8f ? ceiling / peak : 1.0f;
    if (target < gain_)
        gain_ = target;
    else
        gain_ = target + coefficient(std::clamp(finiteOr(settings.releaseMs, 20.0f), 5.0f, 100.0f), sampleRate_) * (gain_ - target);

    left *= gain_;
    right *= gain_;
    const float softStart = ceiling * 0.96f;
    auto soften = [softStart, ceiling](float sample) {
        const float magnitude = std::abs(sample);
        if (magnitude <= softStart)
            return sample;
        const float range = std::max(ceiling - softStart, 1.0e-6f);
        const float bent = softStart + range * std::tanh((magnitude - softStart) / range);
        return std::copysign(std::min(bent, ceiling), sample);
    };
    left = soften(left);
    right = soften(right);
    if (!std::isfinite(left) || !std::isfinite(right)) {
        reset();
        left = right = 0.0f;
    }
}

bool shouldTriggerReverse(ReverseSyncMode mode, bool manualCue, double ppqPosition, double beatsPerBar) noexcept {
    if (mode == ReverseSyncMode::Off)
        return manualCue;
    if (mode == ReverseSyncMode::Beat4Auto) {
        if (!std::isfinite(ppqPosition))
            return false;
        const double barBeats = std::clamp(std::isfinite(beatsPerBar) ? beatsPerBar : 4.0, 1.0, 32.0);
        const double beatInBar = std::fmod(std::fmod(ppqPosition, barBeats) + barBeats, barBeats);
        return beatInBar >= barBeats - 1.0 - 0.05;
    }
    return true;
}

double reverseTargetPpq(ReverseSyncMode mode, double ppqPosition, double beatsPerBar) noexcept {
    ppqPosition = std::isfinite(ppqPosition) ? ppqPosition : 0.0;
    const double barBeats = std::clamp(std::isfinite(beatsPerBar) ? beatsPerBar : 4.0, 1.0, 32.0);
    switch (mode) {
        case ReverseSyncMode::Beat4Auto:
            return (std::floor(ppqPosition / barBeats) + 1.0) * barBeats;
        case ReverseSyncMode::BeatEnd:
            return std::floor(ppqPosition) + 1.0;
        case ReverseSyncMode::HalfBeat:
            return (std::floor(ppqPosition * 2.0) + 1.0) / 2.0;
        case ReverseSyncMode::QuarterBeat:
            return (std::floor(ppqPosition * 4.0) + 1.0) / 4.0;
        case ReverseSyncMode::Off:
        default:
            return ppqPosition + 1.0;
    }
}

int computeSyncedReverseSamples(ReverseSyncMode mode, double ppqPosition, double bpm,
                                double beatsPerBar, double sampleRate) noexcept {
    const double sr = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0, 8000.0, 384000.0);
    if (mode == ReverseSyncMode::Off || !std::isfinite(bpm) || bpm <= 0.0)
        return static_cast<int>(sr * 0.45);
    const double targetPpq = reverseTargetPpq(mode, ppqPosition, beatsPerBar);
    const double beatsUntilTarget = std::clamp(targetPpq - ppqPosition, 0.01, 64.0);
    const double secondsUntilTarget = beatsUntilTarget * (60.0 / std::clamp(bpm, 20.0, 400.0));
    return std::max(32, static_cast<int>(secondsUntilTarget * sr));
}

} // namespace opensmacker::core
