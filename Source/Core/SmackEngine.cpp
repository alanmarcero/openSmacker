#include "SmackEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

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

uint16_t readUint16LE(const uint8_t* data) noexcept {
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

uint32_t readUint32LE(const uint8_t* data) noexcept {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8)
        | (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

} // namespace

void SmackWavCache::clear() noexcept {
    isLocked_ = false;
    sampleRate_ = 48000.0;
    left_.clear();
    right_.clear();
}

void SmackWavCache::capture(const std::vector<float>& left, const std::vector<float>& right, double sampleRate) {
    if (left.empty() || !std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0) {
        clear();
        return;
    }
    left_ = left;
    right_.resize(left.size());
    for (size_t i = 0; i < left.size(); ++i) {
        left_[i] = std::isfinite(left_[i]) ? left_[i] : 0.0f;
        const float rightSample = i < right.size() ? right[i] : left_[i];
        right_[i] = std::isfinite(rightSample) ? rightSample : 0.0f;
    }
    sampleRate_ = sampleRate;
    isLocked_ = true;
}

std::vector<uint8_t> SmackWavCache::toWavBytes() const {
    if (!isLocked_ || left_.empty()) {
        return {};
    }

    if (left_.size() > (std::numeric_limits<uint32_t>::max() - 36u) / (2u * sizeof(float)))
        return {};
    const uint32_t numFrames = static_cast<uint32_t>(left_.size());
    const uint16_t numChannels = 2;
    const uint32_t sRate = static_cast<uint32_t>(std::clamp(sampleRate_, 8000.0, 384000.0));
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
    clear();
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
    uint16_t bitsPerSample = 0;
    size_t dataOffset = 0;
    size_t dataSize = 0;

    while (offset + 8 <= wavBytes.size()) {
        char chunkId[5] = { 0 };
        std::memcpy(chunkId, wavBytes.data() + offset, 4);
        const uint32_t chunkSize = readUint32LE(wavBytes.data() + offset + 4);
        offset += 8;
        if (chunkSize > wavBytes.size() - offset)
            return false;

        if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            formatTag = readUint16LE(wavBytes.data() + offset);
            numChannels = readUint16LE(wavBytes.data() + offset + 2);
            sRate = readUint32LE(wavBytes.data() + offset + 4);
            bitsPerSample = readUint16LE(wavBytes.data() + offset + 14);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataOffset = offset;
            dataSize = chunkSize;
            break;
        }

        offset += chunkSize + (chunkSize & 1u);
    }

    if (dataOffset == 0 || (numChannels != 1 && numChannels != 2) || sRate < 8000 || sRate > 384000
        || !((formatTag == 3 && bitsPerSample == 32) || (formatTag == 1 && bitsPerSample == 16))) {
        return false;
    }

    sampleRate_ = sRate;
    const size_t bytesPerSample = formatTag == 3 ? sizeof(float) : sizeof(int16_t);
    const size_t bytesPerFrame = static_cast<size_t>(numChannels) * bytesPerSample;
    if (bytesPerFrame == 0 || dataSize % bytesPerFrame != 0)
        return false;
    const size_t numFrames = dataSize / bytesPerFrame;

    left_.resize(numFrames);
    right_.resize(numFrames);

    for (size_t i = 0; i < numFrames; ++i) {
        const size_t frameOffset = dataOffset + i * bytesPerFrame;
        auto readChannel = [&](size_t channel) {
            const uint8_t* bytes = wavBytes.data() + frameOffset + channel * bytesPerSample;
            float sample = 0.0f;
            if (formatTag == 3) {
                std::memcpy(&sample, bytes, sizeof(sample));
                return std::isfinite(sample) ? sample : 0.0f;
            }
            const auto integer = static_cast<int16_t>(readUint16LE(bytes));
            return static_cast<float>(integer) / 32768.0f;
        };
        left_[i] = readChannel(0);
        right_[i] = numChannels > 1 ? readChannel(1) : left_[i];
    }

    if (numFrames == 0)
        return false;
    isLocked_ = true;
    return true;
}

void SmackEngine::prepare(double sampleRate) {
    sampleRate_ = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0, 8000.0, 384000.0);
    synth_.prepare(sampleRate_);
    distL_.reset();
    distR_.reset();
    compressor_.prepare(sampleRate_);
    limiter_.prepare(sampleRate_);

    const int maxReverseSamples = static_cast<int>(sampleRate_ * 2.0);
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
    wavPlayPosition_ = -1.0;
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
        wavPlayPosition_ = 0.0;
        wavVelocity_ = std::clamp(std::isfinite(velocity) ? velocity : 1.0f, 0.0f, 1.0f);
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

    if (settings.lockIn != previousLockState_) {
        previousLockState_ = settings.lockIn;
        if (settings.lockIn && !wavCache_.isLocked()) {
            lockCurrentSmack(settings);
        } else if (!settings.lockIn && wavCache_.isLocked()) {
            unlockSmack();
        }
    }

    double ppq = transport.ppqPosition.value_or(fallbackPpq_);
    if (!std::isfinite(ppq))
        ppq = fallbackPpq_;
    const double bpm = std::clamp(std::isfinite(transport.bpm) ? transport.bpm : 138.0, 20.0, 400.0);
    const double ppqPerSample = (bpm / 60.0) / sampleRate_;
    const double beatsPerBar = std::clamp(std::isfinite(transport.beatsPerBar) ? transport.beatsPerBar : 4.0, 1.0, 32.0);

    int triggerIdx = 0;
    int ccIdx = 0;
    float blockPeak = 0.0f;
    const float outputGain = dbToLin(settings.effects.outputGainDb);

    for (int sample = 0; sample < numSamples; ++sample) {
        while (ccIdx < static_cast<int>(controlChanges.size()) && controlChanges[ccIdx].sampleOffset <= sample) {
            const auto& cc = controlChanges[ccIdx++];
            const int value = std::clamp(cc.value, 0, 127);
            if (cc.controller == 7) {
                ccVolume_ = static_cast<float>(value) / 127.0f;
            } else if (cc.controller == 74) {
                ccBrightness_ = static_cast<float>(value) / 127.0f;
            } else if (cc.controller == 120 || cc.controller == 123) {
                synth_.reset();
                wavPlayPosition_ = -1.0;
                reversePlayHead_ = -1;
                isReversing_ = false;
            } else if (cc.controller == 121) {
                ccVolume_ = 1.0f;
                ccBrightness_ = 1.0f;
            }
        }
        while (triggerIdx < static_cast<int>(midiTriggers.size()) && midiTriggers[triggerIdx].sampleOffset <= sample) {
            const auto& trig = midiTriggers[triggerIdx++];
            int semitoneOffset = 0;
            if (settings.noteTracking && !trig.audition) {
                const int rootMidi = 12 * (settings.synth.octave + 1) + settings.synth.note;
                semitoneOffset = std::clamp(trig.noteNumber, 0, 127) - rootMidi;
            }
            triggerSmack(settings, ppq, bpm, beatsPerBar,
                std::clamp(std::isfinite(trig.velocity) ? trig.velocity : 1.0f, 0.0f, 1.0f), semitoneOffset, trig.audition);
        }

        float sL = 0.0f;
        float sR = 0.0f;

        if (wavCache_.isLocked()) {
            const auto& wavLeft = wavCache_.left();
            const auto& wavRight = wavCache_.right();
            if (wavPlayPosition_ >= 0.0 && wavPlayPosition_ < static_cast<double>(wavLeft.size())) {
                const size_t frame = static_cast<size_t>(wavPlayPosition_);
                const size_t next = std::min(frame + 1, wavLeft.size() - 1);
                const float fraction = static_cast<float>(wavPlayPosition_ - static_cast<double>(frame));
                sL = std::lerp(wavLeft[frame], wavLeft[next], fraction) * wavVelocity_;
                sR = std::lerp(wavRight[frame], wavRight[next], fraction) * wavVelocity_;
                wavPlayPosition_ += wavCache_.sampleRate() / sampleRate_;
                currentSmackProgress_ = static_cast<float>(wavPlayPosition_ / static_cast<double>(wavLeft.size()));
            } else {
                wavPlayPosition_ = -1.0;
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

        sL *= outputGain * ccVolume_;
        sR *= outputGain * ccVolume_;
        if (!std::isfinite(sL) || !std::isfinite(sR)) {
            sL = sR = 0.0f;
            distL_.reset();
            distR_.reset();
        }

        channels[0][sample] = sL;
        channels[1][sample] = sR;

        const float framePeak = std::max(std::abs(sL), std::abs(sR));
        blockPeak = std::max(blockPeak, framePeak);

        ppq += ppqPerSample;
    }

    fallbackPpq_ = ppq;
    currentPeak_ = blockPeak;
    currentBeatPhase_ = static_cast<float>(std::fmod(std::fmod(ppq, 1.0) + 1.0, 1.0));
}

} // namespace opensmacker::core
