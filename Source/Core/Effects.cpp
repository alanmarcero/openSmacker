#include "Effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

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

std::pair<double, double> evaluateCurveAndAntideriv(double val, DistortionType type) noexcept {
    switch (type) {
        case DistortionType::Tape: {
            constexpr double bias = 0.12;
            const double norm = std::tanh(1.35 + bias);
            const double u = val * 1.35 + bias;
            const double absU = std::abs(u);
            const double logCoshU = absU + std::log1p(std::exp(-2.0 * absU)) - std::numbers::ln2;
            const double fx = (std::tanh(u) - std::tanh(bias)) / norm;
            const double antideriv = (logCoshU / 1.35 - val * std::tanh(bias)) / norm;
            return { fx, antideriv };
        }
        case DistortionType::Tube: {
            if (val >= 0.0) {
                const double posU = val * 1.8;
                const double logCoshPos = posU + std::log1p(std::exp(-2.0 * posU)) - std::numbers::ln2;
                return { std::tanh(posU), logCoshPos / 1.8 };
            }
            const double negMag = std::abs(val * 1.25);
            const double logCoshNeg = negMag + std::log1p(std::exp(-2.0 * negMag)) - std::numbers::ln2;
            return { std::tanh(val * 1.25) * 0.88, 0.88 * logCoshNeg / 1.25 };
        }
        case DistortionType::HardClip: {
            const double u = val * 1.35;
            const double fx = std::clamp(u, -1.0, 1.0);
            const double absU = std::abs(u);
            const double clipInt = absU <= 1.0 ? 0.5 * u * u : (absU - 0.5);
            return { fx, clipInt / 1.35 };
        }
        case DistortionType::Wavefold: {
            // Periodic Filippov antiderivative integrates triangle fold knees to prevent foldback aliasing.
            const double u = val * 0.82;
            double period = std::fmod(u + 1.0, 4.0);
            if (period < 0.0)
                period += 4.0;
            const double fx = 1.0 - std::abs(period - 2.0);
            const double foldInt = period <= 2.0
                ? 0.5 * (period - 1.0) * (period - 1.0)
                : 1.0 - 0.5 * (period - 3.0) * (period - 3.0);
            return { fx, foldInt / 0.82 };
        }
    }
    return { val, 0.5 * val * val };
}

} // namespace

void DistortionShaper::reset() noexcept {
    upsampler_.reset();
    downsampler_.reset();
    prevInput_ = 0.0;
    prevAntideriv_ = 0.0;
    primed_ = false;
    toneFilterState_ = 0.0f;
}

float DistortionShaper::shapeSubSample(float scaledInput, DistortionType type) noexcept {
    const double curr = std::clamp(static_cast<double>(scaledInput), -24.0, 24.0);
    const auto [pointwise, currAntideriv] = evaluateCurveAndAntideriv(curr, type);
    const double delta = curr - prevInput_;
    double out = pointwise;
    if (std::abs(delta) > 1.0e-5)
        out = (currAntideriv - prevAntideriv_) / delta;
    if (std::abs(delta) <= 1.0e-5) {
        const auto [midPointwise, unusedAntideriv] = evaluateCurveAndAntideriv(0.5 * (curr + prevInput_), type);
        (void) unusedAntideriv;
        out = midPointwise;
    }
    prevInput_ = curr;
    prevAntideriv_ = currAntideriv;
    const bool isFinite = std::isfinite(out);
    return isFinite ? static_cast<float>(out) : 0.0f;
}

float DistortionShaper::processSample(float input, DistortionType type, float drive, float tone, double sampleRate) noexcept {
    input = finiteOr(input);
    drive = std::clamp(finiteOr(drive), 0.0f, 1.0f);
    tone = std::clamp(finiteOr(tone), -1.0f, 1.0f);
    if (drive < 0.001f)
        return input;

    const float gain = dbToLin(drive * 30.0f);
    if (!primed_) {
        upsampler_.primeDc(input);
        const double initVal = std::clamp(static_cast<double>(input * gain), -24.0, 24.0);
        const auto [initShaped, initAntideriv] = evaluateCurveAndAntideriv(initVal, type);
        prevInput_ = initVal;
        prevAntideriv_ = initAntideriv;
        const float initFloat = static_cast<float>(initShaped);
        std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t idx) {
            const float z0 = (1.0f - HalfbandDown4x::kBranch0[idx]) * initFloat;
            const float z1 = (1.0f - HalfbandDown4x::kBranch1[idx]) * initFloat;
            downsampler_.stage1State0[idx] = z0;
            downsampler_.stage1State1[idx] = z1;
            downsampler_.stage2State0[idx] = z0;
            downsampler_.stage2State1[idx] = z1;
        });
        primed_ = true;
    }

    const auto upsampled = upsampler_.process(input);
    std::array<float, 4> shapedSub{};
    std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t idx) {
        shapedSub[idx] = shapeSubSample(upsampled[idx] * gain, type);
    });
    float shaped = downsampler_.process(shapedSub);

    shaped *= 1.0f / (1.0f + drive * 0.55f);
    const float cutoff = tone < 0.0f ? 1800.0f + (1.0f + tone) * 6200.0f : 7000.0f;
    const double sr = std::clamp(sampleRate, 8000.0, 384000.0);
    const float alpha = std::clamp(1.0f - std::exp(-2.0f * kPi * cutoff / static_cast<float>(sr)), 0.001f, 0.95f);
    toneFilterState_ += alpha * (shaped - toneFilterState_);
    if (tone < 0.0f)
        shaped = toneFilterState_;
    if (tone > 0.0f)
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
    if (over > kneeDb * 0.5f)
        reductionDb = -over * (1.0f - 1.0f / ratio);
    if (over <= kneeDb * 0.5f && over > -kneeDb * 0.5f) {
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

void ThreeBandEq::prepare(double sampleRate) noexcept {
    sampleRate_ = std::max(8000.0, sampleRate);
    glideCoeff_ = 1.0f - std::exp(-1.0f / (0.020f * static_cast<float>(sampleRate_)));
    reset();
}

void ThreeBandEq::reset() noexcept {
    std::ranges::for_each(bands_, [](Band& band) {
        band.z1L = band.z2L = band.z1R = band.z2R = 0.0;
        band.b0 = 1.0;
        band.b1 = band.b2 = band.a1 = band.a2 = 0.0;
        band.initialized = false;
    });
}

void ThreeBandEq::updateBand(Band& band, BandType type, float minHz, float maxHz,
                             float targetHz, float targetDb) noexcept {
    const float clampedHz = std::clamp(std::isfinite(targetHz) ? targetHz : minHz,
                                       minHz, std::min(maxHz, static_cast<float>(sampleRate_ * 0.45)));
    const float targetLogFreq = std::log(clampedHz);
    const float clampedDb = std::clamp(std::isfinite(targetDb) ? targetDb : 0.0f, -12.0f, 12.0f);
    if (!band.initialized) {
        band.smoothedLogFreq = targetLogFreq;
        band.smoothedDb = clampedDb;
        band.initialized = true;
    }
    band.smoothedLogFreq += glideCoeff_ * (targetLogFreq - band.smoothedLogFreq);
    band.smoothedDb += glideCoeff_ * (clampedDb - band.smoothedDb);

    if (std::abs(band.smoothedDb) < 0.005f) {
        band.b0 = 1.0;
        band.b1 = band.b2 = band.a1 = band.a2 = 0.0;
        return;
    }

    const double freq = static_cast<double>(std::exp(band.smoothedLogFreq));
    const double omega = 2.0 * std::numbers::pi * std::clamp(freq / sampleRate_, 1.0e-5, 0.45);
    const double sn = std::sin(omega);
    const double cs = std::cos(omega);
    const double amp = std::pow(10.0, static_cast<double>(band.smoothedDb) / 40.0);

    if (type == BandType::peak) {
        const double alpha = sn / (2.0 * 0.9);
        const double a0 = 1.0 + alpha / amp;
        band.b0 = (1.0 + alpha * amp) / a0;
        band.b1 = (-2.0 * cs) / a0;
        band.b2 = (1.0 - alpha * amp) / a0;
        band.a1 = (-2.0 * cs) / a0;
        band.a2 = (1.0 - alpha / amp) / a0;
        return;
    }

    const double alpha = sn * 0.5 * std::sqrt(2.0);
    const double twoRootAmpAlpha = 2.0 * std::sqrt(amp) * alpha;
    if (type == BandType::lowShelf) {
        const double a0 = (amp + 1.0) + (amp - 1.0) * cs + twoRootAmpAlpha;
        band.b0 = (amp * ((amp + 1.0) - (amp - 1.0) * cs + twoRootAmpAlpha)) / a0;
        band.b1 = (2.0 * amp * ((amp - 1.0) - (amp + 1.0) * cs)) / a0;
        band.b2 = (amp * ((amp + 1.0) - (amp - 1.0) * cs - twoRootAmpAlpha)) / a0;
        band.a1 = (-2.0 * ((amp - 1.0) + (amp + 1.0) * cs)) / a0;
        band.a2 = ((amp + 1.0) + (amp - 1.0) * cs - twoRootAmpAlpha) / a0;
        return;
    }

    const double a0 = (amp + 1.0) - (amp - 1.0) * cs + twoRootAmpAlpha;
    band.b0 = (amp * ((amp + 1.0) + (amp - 1.0) * cs + twoRootAmpAlpha)) / a0;
    band.b1 = (-2.0 * amp * ((amp - 1.0) + (amp + 1.0) * cs)) / a0;
    band.b2 = (amp * ((amp + 1.0) + (amp - 1.0) * cs - twoRootAmpAlpha)) / a0;
    band.a1 = (2.0 * ((amp - 1.0) - (amp + 1.0) * cs)) / a0;
    band.a2 = ((amp + 1.0) - (amp - 1.0) * cs - twoRootAmpAlpha) / a0;
}

void ThreeBandEq::filterBand(Band& band, float& left, float& right) noexcept {
    const double xL = static_cast<double>(left);
    const double yL = band.b0 * xL + band.z1L;
    band.z1L = band.b1 * xL - band.a1 * yL + band.z2L;
    band.z2L = band.b2 * xL - band.a2 * yL;
    left = std::isfinite(yL) ? static_cast<float>(yL) : 0.0f;

    const double xR = static_cast<double>(right);
    const double yR = band.b0 * xR + band.z1R;
    band.z1R = band.b1 * xR - band.a1 * yR + band.z2R;
    band.z2R = band.b2 * xR - band.a2 * yR;
    right = std::isfinite(yR) ? static_cast<float>(yR) : 0.0f;
}

void ThreeBandEq::process(float& left, float& right, const ThreeBandEqSettings& settings) noexcept {
    updateBand(bands_[0], BandType::lowShelf, 25.0f, 400.0f, settings.lowFreqHz, settings.lowGainDb);
    updateBand(bands_[1], BandType::peak, 200.0f, 6000.0f, settings.midFreqHz, settings.midGainDb);
    updateBand(bands_[2], BandType::highShelf, 2000.0f, 18000.0f, settings.highFreqHz, settings.highGainDb);
    filterBand(bands_[0], left, right);
    filterBand(bands_[1], left, right);
    filterBand(bands_[2], left, right);
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
    if (target >= gain_)
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
