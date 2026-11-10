#include "SmackEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace opensmacker::core {

namespace {

[[nodiscard]] inline float dbToLin(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

void writeUint16LE(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xff));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}

void writeUint32LE(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xff));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

} // namespace

void SmackWavCache::clear() noexcept {
    isLocked_ = false;
    left_.clear();
    right_.clear();
}

void SmackWavCache::capture(const std::vector<float>& left, const std::vector<float>& right, double sampleRate) {
    left_ = left;
    right_ = right;
    sampleRate_ = sampleRate;
    isLocked_ = !left_.empty();
}

std::vector<uint8_t> SmackWavCache::toWavBytes() const {
    if (!isLocked_ || left_.empty()) {
        return {};
    }

    const uint32_t numFrames = static_cast<uint32_t>(left_.size());
    const uint16_t numChannels = 2;
    const uint32_t sRate = static_cast<uint32_t>(sampleRate_);
    const uint16_t bitsPerSample = 32;
    const uint32_t dataBytes = numFrames * numChannels * sizeof(float);
    const uint32_t chunkSize = 36 + dataBytes;

    std::vector<uint8_t> wav;
    wav.reserve(44 + dataBytes);

    wav.insert(wav.end(), { 'R', 'I', 'F', 'F' });
    writeUint32LE(wav, chunkSize);
    wav.insert(wav.end(), { 'W', 'A', 'V', 'E' });

    wav.insert(wav.end(), { 'f', 'm', 't', ' ' });
    writeUint32LE(wav, 16);
    writeUint16LE(wav, 3);
    writeUint16LE(wav, numChannels);
    writeUint32LE(wav, sRate);
    writeUint32LE(wav, sRate * numChannels * sizeof(float));
    writeUint16LE(wav, numChannels * sizeof(float));
    writeUint16LE(wav, bitsPerSample);

    wav.insert(wav.end(), { 'd', 'a', 't', 'a' });
    writeUint32LE(wav, dataBytes);

    for (size_t i = 0; i < left_.size(); ++i) {
        float sampleL = left_[i];
        float sampleR = (i < right_.size()) ? right_[i] : sampleL;

        uint32_t rawL, rawR;
        std::memcpy(&rawL, &sampleL, sizeof(float));
        std::memcpy(&rawR, &sampleR, sizeof(float));
        writeUint32LE(wav, rawL);
        writeUint32LE(wav, rawR);
    }

    return wav;
}

bool SmackWavCache::fromWavBytes(const std::vector<uint8_t>& wavBytes) {
    if (wavBytes.size() < 44) {
        return false;
    }

    if (std::memcmp(wavBytes.data(), "RIFF", 4) != 0 || std::memcmp(wavBytes.data() + 8, "WAVE", 4) != 0) {
        return false;
    }

    size_t offset = 12;
    uint16_t numChannels = 0;
    uint32_t sRate = 48000;
    uint16_t formatTag = 0;
    size_t dataOffset = 0;
    size_t dataSize = 0;

    while (offset + 8 <= wavBytes.size()) {
        char chunkId[5] = { 0 };
        std::memcpy(chunkId, wavBytes.data() + offset, 4);
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, wavBytes.data() + offset + 4, 4);
        offset += 8;

        if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            std::memcpy(&formatTag, wavBytes.data() + offset, 2);
            std::memcpy(&numChannels, wavBytes.data() + offset + 2, 2);
            std::memcpy(&sRate, wavBytes.data() + offset + 4, 4);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataOffset = offset;
            dataSize = chunkSize;
            break;
        }

        offset += chunkSize;
    }

    if (dataOffset == 0 || numChannels == 0 || (formatTag != 3 && formatTag != 1)) {
        return false;
    }

    sampleRate_ = sRate;
    const size_t bytesPerFrame = numChannels * (formatTag == 3 ? sizeof(float) : 2);
    const size_t numFrames = dataSize / bytesPerFrame;

    left_.resize(numFrames);
    right_.resize(numFrames);

    if (formatTag == 3) {
        const auto* floatPtr = reinterpret_cast<const float*>(wavBytes.data() + dataOffset);
        for (size_t i = 0; i < numFrames; ++i) {
            left_[i] = floatPtr[i * numChannels];
            right_[i] = (numChannels > 1) ? floatPtr[i * numChannels + 1] : left_[i];
        }
    } else {
        const auto* intPtr = reinterpret_cast<const int16_t*>(wavBytes.data() + dataOffset);
        for (size_t i = 0; i < numFrames; ++i) {
            left_[i] = static_cast<float>(intPtr[i * numChannels]) / 32768.0f;
            right_[i] = (numChannels > 1) ? static_cast<float>(intPtr[i * numChannels + 1]) / 32768.0f : left_[i];
        }
    }

    isLocked_ = true;
    return true;
}

void SmackEngine::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    synth_.prepare(sampleRate);
    distL_.reset();
    distR_.reset();
    compressor_.prepare(sampleRate);
    limiter_.prepare(sampleRate);

    const int maxReverseSamples = static_cast<int>(sampleRate * 2.0);
    reverseBufferL_.assign(maxReverseSamples, 0.0f);
    reverseBufferR_.assign(maxReverseSamples, 0.0f);
    reset();
}

void SmackEngine::reset() noexcept {
    synth_.reset();
    distL_.reset();
    distR_.reset();
    compressor_.reset();
    limiter_.reset();
    wavPlayHead_ = -1;
    reversePlayHead_ = -1;
    reverseDuration_ = 0;
    isReversing_ = false;
    currentPeak_ = 0.0f;
    currentBeatPhase_ = 0.0f;
    currentSmackProgress_ = 1.0f;
    ccVolume_ = 1.0f;
    ccBrightness_ = 1.0f;
}

void SmackEngine::lockCurrentSmack(const EngineSettings& settings) {
    std::vector<float> dryL, dryR;
    synth_.renderBlock(dryL, dryR, settings.synth);

    std::vector<float> renderedL(dryL.size());
    std::vector<float> renderedR(dryR.size());

    DistortionShaper dL, dR;
    StereoCompressor comp;
    BrickwallLimiter lim;
    comp.prepare(sampleRate_);
    lim.prepare(sampleRate_);

    for (size_t i = 0; i < dryL.size(); ++i) {
        float sL = dryL[i];
        float sR = dryR[i];

        if (settings.effects.distMix > 0.001f) {
            const float wetDistL = dL.processSample(sL, settings.effects.distType, settings.effects.distDrive, settings.effects.distTone, sampleRate_);
            const float wetDistR = dR.processSample(sR, settings.effects.distType, settings.effects.distDrive, settings.effects.distTone, sampleRate_);
            sL = sL * (1.0f - settings.effects.distMix) + wetDistL * settings.effects.distMix;
            sR = sR * (1.0f - settings.effects.distMix) + wetDistR * settings.effects.distMix;
        }

        if (settings.effects.stereoWidth != 1.0f) {
            const float mid = (sL + sR) * 0.5f;
            const float side = (sL - sR) * 0.5f * settings.effects.stereoWidth;
            sL = mid + side;
            sR = mid - side;
        }

        comp.process(sL, sR, settings.effects.compressor);
        lim.process(sL, sR, settings.effects.limiter);

        renderedL[i] = sL;
        renderedR[i] = sR;
    }

    wavCache_.capture(renderedL, renderedR, sampleRate_);
}

void SmackEngine::unlockSmack() noexcept {
    wavCache_.clear();
}

bool SmackEngine::restoreLockedWav(const std::vector<uint8_t>& wavBytes) {
    return wavCache_.fromWavBytes(wavBytes);
}

void SmackEngine::triggerSmack(
    const EngineSettings& settings,
    double samplePpq,
    double bpm,
    double beatsPerBar,
    float velocity,
    int semitoneOffset,
    bool reverseCue) noexcept {
    triggerSerial_++;
    currentSmackProgress_ = 0.0f;

    const bool engageReverse = shouldTriggerReverse(settings.effects.reverseMode, reverseCue, samplePpq, beatsPerBar);
    if (engageReverse && settings.effects.reverseMode != ReverseSyncMode::Off) {
        reverseDuration_ = computeSyncedReverseSamples(settings.effects.reverseMode, samplePpq, bpm, beatsPerBar, sampleRate_);
        reverseDuration_ = std::min(reverseDuration_, static_cast<int>(reverseBufferL_.size()));

        std::vector<float> forwardL, forwardR;
        synth_.renderBlock(forwardL, forwardR, settings.synth, velocity, semitoneOffset);

        const int copyCount = std::min(reverseDuration_, static_cast<int>(forwardL.size()));
        for (int i = 0; i < reverseDuration_; ++i) {
            if (i < copyCount) {
                const int srcIdx = copyCount - 1 - i;
                const float ramp = static_cast<float>(i) / static_cast<float>(reverseDuration_);
                reverseBufferL_[i] = forwardL[srcIdx] * ramp * settings.effects.reverseMix;
                reverseBufferR_[i] = forwardR[srcIdx] * ramp * settings.effects.reverseMix;
            } else {
                reverseBufferL_[i] = 0.0f;
                reverseBufferR_[i] = 0.0f;
            }
        }
        reversePlayHead_ = 0;
        isReversing_ = true;
    }

    if (wavCache_.isLocked()) {
        wavPlayHead_ = 0;
        wavVelocity_ = velocity;
    } else {
        synth_.trigger(settings.synth, velocity, semitoneOffset);
    }
}

void SmackEngine::process(
    const EngineSettings& settings,
    const Transport& transport,
    std::span<const MidiTrigger> midiTriggers,
    float* const* channels,
    int numChannels,
    int numSamples,
    std::span<const MidiCcEvent> controlChanges) noexcept {
    if (settings.bypass || numChannels < 2 || channels == nullptr || numSamples <= 0) {
        return;
    }

    for (const auto& cc : controlChanges) {
        if (cc.controller == 7) {
            ccVolume_ = static_cast<float>(cc.value) / 127.0f;
        } else if (cc.controller == 74) {
            ccBrightness_ = static_cast<float>(cc.value) / 127.0f;
        } else if (cc.controller == 120 || cc.controller == 123) {
            synth_.reset();
            wavPlayHead_ = -1;
            reversePlayHead_ = -1;
        } else if (cc.controller == 121) {
            ccVolume_ = 1.0f;
            ccBrightness_ = 1.0f;
        }
    }

    if (settings.lockIn != previousLockState_) {
        previousLockState_ = settings.lockIn;
        if (settings.lockIn && !wavCache_.isLocked()) {
            lockCurrentSmack(settings);
        } else if (!settings.lockIn && wavCache_.isLocked()) {
            unlockSmack();
        }
    }

    double ppq = transport.ppqPosition.value_or(fallbackPpq_);
    const double ppqPerSample = (transport.bpm / 60.0) / sampleRate_;
    const double beatsPerBar = transport.beatsPerBar > 0.0 ? transport.beatsPerBar : 4.0;

    int triggerIdx = 0;
    float blockPeak = 0.0f;
    const float masterGain = dbToLin(settings.effects.outputGainDb) * ccVolume_;

    for (int sample = 0; sample < numSamples; ++sample) {
        while (triggerIdx < static_cast<int>(midiTriggers.size()) && midiTriggers[triggerIdx].sampleOffset <= sample) {
            const auto& trig = midiTriggers[triggerIdx++];
            int semitoneOffset = 0;
            if (settings.noteTracking && !trig.audition) {
                const int rootMidi = 12 * (settings.synth.octave + 1) + settings.synth.note;
                semitoneOffset = trig.noteNumber - rootMidi;
            }
            triggerSmack(settings, ppq, transport.bpm, beatsPerBar, trig.velocity, semitoneOffset, trig.audition);
        }

        float sL = 0.0f;
        float sR = 0.0f;

        if (wavCache_.isLocked()) {
            if (wavPlayHead_ >= 0 && wavPlayHead_ < static_cast<int>(wavCache_.left().size())) {
                sL = wavCache_.left()[wavPlayHead_] * wavVelocity_;
                sR = wavCache_.right()[wavPlayHead_] * wavVelocity_;
                wavPlayHead_++;
                currentSmackProgress_ = static_cast<float>(wavPlayHead_) / static_cast<float>(wavCache_.left().size());
            } else {
                wavPlayHead_ = -1;
                currentSmackProgress_ = 1.0f;
            }
        } else {
            if (synth_.isPlaying()) {
                auto activeSettings = settings.synth;
                if (std::abs(ccBrightness_ - 1.0f) > 0.001f) {
                    activeSettings.noiseLPF = std::clamp(settings.synth.noiseLPF * ccBrightness_, 200.0f, 20000.0f);
                }
                const auto frame = synth_.processSample(activeSettings);
                sL = frame.left;
                sR = frame.right;
                currentSmackProgress_ = synth_.getEnvelopeProgress();
            } else {
                currentSmackProgress_ = 1.0f;
            }
        }

        if (reversePlayHead_ >= 0 && reversePlayHead_ < reverseDuration_) {
            sL += reverseBufferL_[reversePlayHead_];
            sR += reverseBufferR_[reversePlayHead_];
            reversePlayHead_++;
        } else {
            reversePlayHead_ = -1;
            isReversing_ = false;
        }

        if (!wavCache_.isLocked()) {
            if (settings.effects.distMix > 0.001f) {
                const float wetDistL = distL_.processSample(sL, settings.effects.distType, settings.effects.distDrive, settings.effects.distTone, sampleRate_);
                const float wetDistR = distR_.processSample(sR, settings.effects.distType, settings.effects.distDrive, settings.effects.distTone, sampleRate_);
                sL = sL * (1.0f - settings.effects.distMix) + wetDistL * settings.effects.distMix;
                sR = sR * (1.0f - settings.effects.distMix) + wetDistR * settings.effects.distMix;
            }

            if (settings.effects.stereoWidth != 1.0f) {
                const float mid = (sL + sR) * 0.5f;
                const float side = (sL - sR) * 0.5f * settings.effects.stereoWidth;
                sL = mid + side;
                sR = mid - side;
            }

            compressor_.process(sL, sR, settings.effects.compressor);
            limiter_.process(sL, sR, settings.effects.limiter);
        }

        sL *= masterGain;
        sR *= masterGain;

        channels[0][sample] = sL;
        channels[1][sample] = sR;

        const float framePeak = std::max(std::abs(sL), std::abs(sR));
        blockPeak = std::max(blockPeak, framePeak);

        ppq += ppqPerSample;
    }

    fallbackPpq_ = ppq;
    currentPeak_ = blockPeak;
    currentBeatPhase_ = static_cast<float>(std::fmod(ppq, 1.0));
}

} // namespace opensmacker::core
