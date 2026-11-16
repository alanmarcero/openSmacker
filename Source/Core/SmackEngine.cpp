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

void resizeVoiceBuffers(SmackWavVoice& voice, size_t size) {
    voice.left.resize(size);
    voice.right.resize(size);
    for (auto& layer : voice.layers) {
        layer.left.resize(size);
        layer.right.resize(size);
    }
}

} // namespace

std::optional<VoiceMode> resolveVoiceModeForNote(
    int noteNumber, const std::array<VoiceKeyZone, 4>& zones) noexcept {
    const int note = std::clamp(noteNumber, 0, 127);
    for (size_t i = 0; i < zones.size(); ++i) {
        const int low = std::clamp(zones[i].lowNote, 0, 127);
        const int high = std::clamp(zones[i].highNote, 0, 127);
        if (low <= high && note >= low && note <= high)
            return static_cast<VoiceMode>(i);
    }
    return std::nullopt;
}

void SmackWavCache::clear() noexcept {
    isLocked_ = false;
    sampleRate_ = 48000.0;
    primaryVoiceIndex_ = 0;
    for (auto& voice : voices_) {
        voice.left.clear();
        voice.right.clear();
        for (auto& layer : voice.layers) {
            layer.left.clear();
            layer.right.clear();
        }
    }
}

void SmackWavCache::reserveFrames(size_t frames) {
    for (auto& voice : voices_) {
        voice.left.reserve(frames);
        voice.right.reserve(frames);
        for (auto& layer : voice.layers) {
            layer.left.reserve(frames);
            layer.right.reserve(frames);
        }
    }
}

void SmackWavCache::beginCapture() noexcept {
    clear();
}

SmackWavVoice& SmackWavCache::writableVoice(size_t voiceIndex) noexcept {
    return voices_[std::min(voiceIndex, voices_.size() - 1)];
}

bool SmackWavCache::finishCapture(double sampleRate, VoiceMode primaryVoice) noexcept {
    if (voices_[0].left.empty() || !std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0) {
        clear();
        return false;
    }
    const size_t frameCount = voices_[0].left.size();
    for (auto& voice : voices_) {
        if (voice.left.size() != frameCount || voice.right.size() != frameCount) {
            clear();
            return false;
        }
        for (size_t i = 0; i < frameCount; ++i) {
            voice.left[i] = std::isfinite(voice.left[i]) ? voice.left[i] : 0.0f;
            voice.right[i] = std::isfinite(voice.right[i]) ? voice.right[i] : 0.0f;
        }
        for (auto& layer : voice.layers) {
            if (layer.left.empty() && layer.right.empty())
                continue;
            if (layer.left.size() != frameCount || layer.right.size() != frameCount) {
                clear();
                return false;
            }
            for (size_t i = 0; i < frameCount; ++i) {
                layer.left[i] = std::isfinite(layer.left[i]) ? layer.left[i] : 0.0f;
                layer.right[i] = std::isfinite(layer.right[i]) ? layer.right[i] : 0.0f;
            }
        }
    }
    sampleRate_ = sampleRate;
    primaryVoiceIndex_ = static_cast<size_t>(std::clamp(static_cast<int>(primaryVoice), 0, 3));
    isLocked_ = true;
    return true;
}

void SmackWavCache::capture(const std::vector<float>& left, const std::vector<float>& right, double sampleRate) {
    if (left.empty() || !std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0) {
        clear();
        return;
    }
    std::array<SmackWavVoice, 4> voices;
    for (auto& voice : voices) {
        voice.left = left;
        voice.right.resize(left.size());
        for (size_t i = 0; i < left.size(); ++i) {
            voice.left[i] = std::isfinite(voice.left[i]) ? voice.left[i] : 0.0f;
            const float rightSample = i < right.size() ? right[i] : voice.left[i];
            voice.right[i] = std::isfinite(rightSample) ? rightSample : 0.0f;
        }
    }
    captureVoices(std::move(voices), sampleRate);
}

void SmackWavCache::captureVoices(std::array<SmackWavVoice, 4>&& voices, double sampleRate, VoiceMode primaryVoice) {
    voices_ = std::move(voices);
    (void) finishCapture(sampleRate, primaryVoice);
}

const SmackWavVoice& SmackWavCache::voice(VoiceMode mode) const noexcept {
    const auto index = std::clamp(static_cast<int>(mode), 0, static_cast<int>(voices_.size()) - 1);
    return voices_[static_cast<size_t>(index)];
}

std::vector<uint8_t> SmackWavCache::toWavBytes() const {
    const auto& baseVoice = voices_[primaryVoiceIndex_];
    if (!isLocked_ || baseVoice.left.empty()) {
        return {};
    }

    if (baseVoice.left.size() > (std::numeric_limits<uint32_t>::max() - 36u) / (2u * sizeof(float)))
        return {};
    const uint32_t numFrames = static_cast<uint32_t>(baseVoice.left.size());
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

    for (size_t i = 0; i < baseVoice.left.size(); ++i) {
        float sampleL = baseVoice.left[i];
        float sampleR = (i < baseVoice.right.size()) ? baseVoice.right[i] : sampleL;

        uint32_t rawL, rawR;
        std::memcpy(&rawL, &sampleL, sizeof(float));
        std::memcpy(&rawR, &sampleR, sizeof(float));
        writeUint32LE(wav, rawL);
        writeUint32LE(wav, rawR);
    }

    return wav;
}

std::vector<uint8_t> SmackWavCache::toBankWavBytes() const {
    constexpr uint16_t channelCount = 32;
    const size_t frameCount = voices_[0].left.size();
    if (!isLocked_ || frameCount == 0
        || frameCount > (std::numeric_limits<uint32_t>::max() - 36u) / (channelCount * sizeof(float)))
        return {};

    const auto frames = static_cast<uint32_t>(frameCount);
    const uint32_t sampleRate = static_cast<uint32_t>(std::clamp(sampleRate_, 8000.0, 384000.0));
    const uint32_t dataBytes = frames * channelCount * sizeof(float);
    std::vector<uint8_t> wav;
    wav.reserve(44u + dataBytes);
    wav.insert(wav.end(), { 'R', 'I', 'F', 'F' });
    writeUint32LE(wav, 36u + dataBytes);
    wav.insert(wav.end(), { 'W', 'A', 'V', 'E' });
    wav.insert(wav.end(), { 'f', 'm', 't', ' ' });
    writeUint32LE(wav, 16);
    writeUint16LE(wav, 3);
    writeUint16LE(wav, channelCount);
    writeUint32LE(wav, sampleRate);
    writeUint32LE(wav, sampleRate * channelCount * sizeof(float));
    writeUint16LE(wav, channelCount * sizeof(float));
    writeUint16LE(wav, 32);
    wav.insert(wav.end(), { 'd', 'a', 't', 'a' });
    writeUint32LE(wav, dataBytes);

    for (size_t frame = 0; frame < frameCount; ++frame) {
        for (const auto& voice : voices_) {
            const std::array<float, 8> channels{
                voice.left[frame], voice.right[frame],
                voice.layers[0].left.empty() ? 0.0f : voice.layers[0].left[frame],
                voice.layers[0].right.empty() ? 0.0f : voice.layers[0].right[frame],
                voice.layers[1].left.empty() ? 0.0f : voice.layers[1].left[frame],
                voice.layers[1].right.empty() ? 0.0f : voice.layers[1].right[frame],
                voice.layers[2].left.empty() ? 0.0f : voice.layers[2].left[frame],
                voice.layers[2].right.empty() ? 0.0f : voice.layers[2].right[frame]
            };
            for (float sample : channels) {
                const float finiteSample = std::isfinite(sample) ? sample : 0.0f;
                uint32_t raw;
                std::memcpy(&raw, &finiteSample, sizeof(float));
                writeUint32LE(wav, raw);
            }
        }
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

    if (dataOffset == 0 || (numChannels != 1 && numChannels != 2 && numChannels != 32) || sRate < 8000 || sRate > 384000
        || !((formatTag == 3 && bitsPerSample == 32) || (formatTag == 1 && bitsPerSample == 16))) {
        return false;
    }

    sampleRate_ = sRate;
    const size_t bytesPerSample = formatTag == 3 ? sizeof(float) : sizeof(int16_t);
    const size_t bytesPerFrame = static_cast<size_t>(numChannels) * bytesPerSample;
    if (bytesPerFrame == 0 || dataSize % bytesPerFrame != 0)
        return false;
    const size_t numFrames = dataSize / bytesPerFrame;

    for (auto& voice : voices_) {
        voice.left.resize(numFrames);
        voice.right.resize(numFrames);
        if (numChannels == 32) {
            for (auto& layer : voice.layers) {
                layer.left.resize(numFrames);
                layer.right.resize(numFrames);
            }
        }
    }

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
        if (numChannels == 32) {
            for (size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
                const size_t channelBase = voiceIndex * 8;
                auto& voice = voices_[voiceIndex];
                voice.left[i] = readChannel(channelBase);
                voice.right[i] = readChannel(channelBase + 1);
                for (size_t layerIndex = 0; layerIndex < voice.layers.size(); ++layerIndex) {
                    voice.layers[layerIndex].left[i] = readChannel(channelBase + 2 + layerIndex * 2);
                    voice.layers[layerIndex].right[i] = readChannel(channelBase + 3 + layerIndex * 2);
                }
            }
        } else {
            const float left = readChannel(0);
            const float right = numChannels > 1 ? readChannel(1) : left;
            for (auto& voice : voices_) {
                voice.left[i] = left;
                voice.right[i] = right;
            }
        }
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
    wavCache_.reserveFrames(static_cast<size_t>(std::ceil(sampleRate_ * 2.5)));
    resizeVoiceBuffers(reverseBuffer_, static_cast<size_t>(maxReverseSamples));
    resizeVoiceBuffers(reverseSource_, static_cast<size_t>(maxReverseSamples));
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
    ccBodyGain_ = 1.0f;
    ccWireGain_ = 1.0f;
    ccTransientGain_ = 1.0f;
    channelPressure_.fill(0.0f);
    activePressure_ = 0.0f;
    activeNote_ = -1;
    activeChannel_ = 1;
    activeVoiceMode_ = VoiceMode::DanceSnare;
    hasRollStep_ = false;
    lastReverseRenderSamples_ = 0;
}

void SmackEngine::lockCurrentSmack(const EngineSettings& settings) {
    wavCache_.reserveFrames(static_cast<size_t>(std::ceil(sampleRate_ * 2.5)));
    wavCache_.beginCapture();
    for (size_t voiceIndex = 0; voiceIndex < 4; ++voiceIndex) {
        auto voiceSettings = settings.synth;
        voiceSettings.voiceMode = static_cast<VoiceMode>(voiceIndex);
        auto& voice = wavCache_.writableVoice(voiceIndex);
        synth_.renderBlock(voice.left, voice.right, voiceSettings, 1.0f, 0, &voice.layers);

        DistortionShaper dL, dR;
        StereoCompressor comp;
        BrickwallLimiter lim;
        comp.prepare(sampleRate_);
        lim.prepare(sampleRate_);

        for (size_t i = 0; i < voice.left.size(); ++i) {
            float sL = voice.left[i];
            float sR = voice.right[i];

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

            voice.left[i] = sL;
            voice.right[i] = sR;
        }
    }
    (void) wavCache_.finishCapture(sampleRate_, settings.synth.voiceMode);
}

void SmackEngine::unlockSmack() noexcept {
    wavCache_.clear();
    previousLockState_ = false;
}

bool SmackEngine::restoreLockedWav(const std::vector<uint8_t>& wavBytes, VoiceMode primaryVoice) {
    previousLockState_ = wavCache_.fromWavBytes(wavBytes);
    if (previousLockState_) {
        wavCache_.setPrimaryVoice(primaryVoice);
        wavCache_.reserveFrames(static_cast<size_t>(std::ceil(sampleRate_ * 2.5)));
    }
    return previousLockState_;
}

void SmackEngine::triggerSmack(
    const EngineSettings& settings,
    double samplePpq,
    double bpm,
    double beatsPerBar,
    float velocity,
    int semitoneOffset,
    bool reverseCue,
    int noteNumber,
    int channel) noexcept {
    if (settings.keyZonesEnabled) {
        const auto mode = resolveVoiceModeForNote(noteNumber, settings.voiceZones);
        if (!mode)
            return;
        activeVoiceMode_ = *mode;
    } else {
        activeVoiceMode_ = settings.synth.voiceMode;
    }

    triggerSerial_++;
    currentSmackProgress_ = 0.0f;
    activeNote_ = std::clamp(noteNumber, 0, 127);
    activeChannel_ = std::clamp(channel, 1, 16);
    activePressure_ = channelPressure_[static_cast<size_t>(activeChannel_ - 1)];
    auto triggerSettings = settings.synth;
    triggerSettings.voiceMode = activeVoiceMode_;
    triggerSettings.aftertouchValue = activePressure_;
    triggerSettings.bodyPerformanceGain = ccBodyGain_;
    triggerSettings.wirePerformanceGain = ccWireGain_;
    triggerSettings.transientPerformanceGain = ccTransientGain_;

    const bool engageReverse = shouldTriggerReverse(settings.effects.reverseMode, reverseCue, samplePpq, beatsPerBar);
    if (engageReverse && settings.effects.reverseMode != ReverseSyncMode::Off) {
        reverseDuration_ = computeSyncedReverseSamples(settings.effects.reverseMode, samplePpq, bpm, beatsPerBar, sampleRate_);
        reverseDuration_ = std::min(reverseDuration_, static_cast<int>(reverseBuffer_.left.size()));

        const std::vector<float>* sourceL = nullptr;
        const std::vector<float>* sourceR = nullptr;
        const std::array<StereoLayerBuffer, 3>* sourceLayers = nullptr;
        int renderedFrames = 0;
        const bool sourceIsCached = wavCache_.isLocked();
        if (sourceIsCached) {
            const auto& cachedVoice = wavCache_.voice(activeVoiceMode_);
            sourceL = &cachedVoice.left;
            sourceR = &cachedVoice.right;
            sourceLayers = &cachedVoice.layers;
            renderedFrames = static_cast<int>(cachedVoice.left.size());
        } else {
            renderedFrames = synth_.renderPrefix(reverseSource_.left, reverseSource_.right,
                triggerSettings, reverseDuration_, velocity, semitoneOffset, &reverseSource_.layers);
            lastReverseRenderSamples_ += static_cast<uint64_t>(renderedFrames);
            sourceL = &reverseSource_.left;
            sourceR = &reverseSource_.right;
            sourceLayers = &reverseSource_.layers;
        }

        const int copyCount = std::min(reverseDuration_, renderedFrames);
        const float normalizedVelocity = std::clamp(std::isfinite(velocity) ? velocity : 1.0f, 0.0f, 1.0f);
        const std::array<float, 3> cachedLayerGains = sourceIsCached ? std::array<float, 3>{
            std::lerp(1.0f, normalizedVelocity, std::clamp(settings.synth.bodyVelocityDepth, 0.0f, 1.0f))
                * (1.0f + activePressure_ * std::clamp(settings.synth.bodyAftertouchDepth, 0.0f, 1.0f)) * ccBodyGain_,
            std::lerp(1.0f, normalizedVelocity, std::clamp(settings.synth.wireVelocityDepth, 0.0f, 1.0f))
                * (1.0f + activePressure_ * std::clamp(settings.synth.wireAftertouchDepth, 0.0f, 1.0f)) * ccWireGain_,
            std::lerp(1.0f, normalizedVelocity, std::clamp(settings.synth.transientVelocityDepth, 0.0f, 1.0f))
                * (1.0f + activePressure_ * std::clamp(settings.synth.transientAftertouchDepth, 0.0f, 1.0f)) * ccTransientGain_
        } : std::array<float, 3>{ 1.0f, 1.0f, 1.0f };
        const float cachedMainGain = sourceIsCached ? normalizedVelocity : 1.0f;
        for (int i = 0; i < reverseDuration_; ++i) {
            if (i < copyCount) {
                const int srcIdx = copyCount - 1 - i;
                const float ramp = static_cast<float>(i) / static_cast<float>(reverseDuration_);
                reverseBuffer_.left[static_cast<size_t>(i)] = (*sourceL)[static_cast<size_t>(srcIdx)] * cachedMainGain * ramp * settings.effects.reverseMix;
                reverseBuffer_.right[static_cast<size_t>(i)] = (*sourceR)[static_cast<size_t>(srcIdx)] * cachedMainGain * ramp * settings.effects.reverseMix;
                for (size_t layer = 0; layer < reverseBuffer_.layers.size(); ++layer) {
                    const auto& src = (*sourceLayers)[layer];
                    const bool hasStem = static_cast<size_t>(srcIdx) < src.left.size()
                        && static_cast<size_t>(srcIdx) < src.right.size();
                    reverseBuffer_.layers[layer].left[static_cast<size_t>(i)] = hasStem
                        ? src.left[static_cast<size_t>(srcIdx)] * cachedLayerGains[layer] * ramp * settings.effects.reverseMix
                        : 0.0f;
                    reverseBuffer_.layers[layer].right[static_cast<size_t>(i)] = hasStem
                        ? src.right[static_cast<size_t>(srcIdx)] * cachedLayerGains[layer] * ramp * settings.effects.reverseMix
                        : 0.0f;
                }
            } else {
                reverseBuffer_.left[static_cast<size_t>(i)] = 0.0f;
                reverseBuffer_.right[static_cast<size_t>(i)] = 0.0f;
                for (auto& layer : reverseBuffer_.layers) {
                    layer.left[static_cast<size_t>(i)] = 0.0f;
                    layer.right[static_cast<size_t>(i)] = 0.0f;
                }
            }
        }
        reversePlayHead_ = 0;
        isReversing_ = true;
    }

    if (wavCache_.isLocked()) {
        wavPlayPosition_ = 0.0;
        wavVelocity_ = std::clamp(std::isfinite(velocity) ? velocity : 1.0f, 0.0f, 1.0f);
    } else {
        synth_.trigger(triggerSettings, velocity, semitoneOffset);
    }
}

void SmackEngine::process(
    const EngineSettings& settings,
    const Transport& transport,
    std::span<const MidiTrigger> midiTriggers,
    float* const* channels,
    int numChannels,
    int numSamples,
    std::span<const MidiCcEvent> controlChanges,
    std::span<const MidiPressureEvent> pressureChanges,
    LayerOutputChannels layerOutputs) noexcept {
    if (settings.bypass || numChannels < 2 || channels == nullptr || numSamples <= 0) {
        return;
    }
    lastReverseRenderSamples_ = 0;

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
    int pressureIdx = 0;
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
                ccBodyGain_ = 1.0f;
                ccWireGain_ = 1.0f;
                ccTransientGain_ = 1.0f;
            }
            const float gain = static_cast<float>(value) / 127.0f;
            if (settings.midiCcForBody > 0 && cc.controller == settings.midiCcForBody)
                ccBodyGain_ = gain;
            if (settings.midiCcForWire > 0 && cc.controller == settings.midiCcForWire)
                ccWireGain_ = gain;
            if (settings.midiCcForTransient > 0 && cc.controller == settings.midiCcForTransient)
                ccTransientGain_ = gain;
        }
        while (pressureIdx < static_cast<int>(pressureChanges.size()) && pressureChanges[pressureIdx].sampleOffset <= sample) {
            const auto& pressure = pressureChanges[pressureIdx++];
            const int channel = std::clamp(pressure.channel, 1, 16);
            const float value = static_cast<float>(std::clamp(pressure.value, 0, 127)) / 127.0f;
            if (pressure.polyphonic) {
                if (channel == activeChannel_ && pressure.noteNumber == activeNote_)
                    activePressure_ = value;
            } else {
                channelPressure_[static_cast<size_t>(channel - 1)] = value;
                if (channel == activeChannel_)
                    activePressure_ = value;
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
                std::clamp(std::isfinite(trig.velocity) ? trig.velocity : 1.0f, 0.0f, 1.0f), semitoneOffset,
                trig.audition, trig.noteNumber, trig.channel);
        }

        if (settings.rollEnabled && transport.isPlaying) {
            const int subdivision = settings.rollSubdivision == 8 || settings.rollSubdivision == 16 || settings.rollSubdivision == 32
                ? settings.rollSubdivision : 16;
            const double stepBeats = 4.0 / static_cast<double>(subdivision);
            const double rawStep = std::floor(ppq / stepBeats);
            const int64_t step = static_cast<int64_t>(std::clamp(rawStep, -9.0e15, 9.0e15));
            if (!hasRollStep_ || step != lastRollStep_) {
                lastRollStep_ = step;
                hasRollStep_ = true;
                rollRandomState_ ^= rollRandomState_ << 13;
                rollRandomState_ ^= rollRandomState_ >> 17;
                rollRandomState_ ^= rollRandomState_ << 5;
                const float chance = static_cast<float>(rollRandomState_) / static_cast<float>(std::numeric_limits<uint32_t>::max());
                if (chance <= std::clamp(settings.rollProbability, 0.0f, 1.0f)) {
                    const int accentEvery = std::clamp(settings.rollAccentEvery, 1, 16);
                    const int accentStep = static_cast<int>((step % accentEvery + accentEvery) % accentEvery);
                    const float velocity = accentStep == 0
                        ? std::clamp(settings.rollAccentVelocity, 0.0f, 1.0f)
                        : std::clamp(settings.rollVelocity, 0.0f, 1.0f);
                    triggerSmack(settings, ppq, bpm, beatsPerBar, velocity, 0, false,
                        settings.rollNote, 1);
                }
            }
        } else {
            hasRollStep_ = false;
        }

        float sL = 0.0f;
        float sR = 0.0f;
        float bodyL = 0.0f, bodyR = 0.0f;
        float wireL = 0.0f, wireR = 0.0f;
        float transientL = 0.0f, transientR = 0.0f;

        if (wavCache_.isLocked()) {
            const auto& cachedVoice = wavCache_.voice(activeVoiceMode_);
            if (wavPlayPosition_ >= 0.0 && wavPlayPosition_ < static_cast<double>(cachedVoice.left.size())) {
                const size_t frame = static_cast<size_t>(wavPlayPosition_);
                const size_t next = std::min(frame + 1, cachedVoice.left.size() - 1);
                const float fraction = static_cast<float>(wavPlayPosition_ - static_cast<double>(frame));
                sL = std::lerp(cachedVoice.left[frame], cachedVoice.left[next], fraction) * wavVelocity_;
                sR = std::lerp(cachedVoice.right[frame], cachedVoice.right[next], fraction) * wavVelocity_;
                const std::array<float, 3> layerGains{
                    std::lerp(1.0f, wavVelocity_, std::clamp(settings.synth.bodyVelocityDepth, 0.0f, 1.0f))
                        * (1.0f + activePressure_ * std::clamp(settings.synth.bodyAftertouchDepth, 0.0f, 1.0f)) * ccBodyGain_,
                    std::lerp(1.0f, wavVelocity_, std::clamp(settings.synth.wireVelocityDepth, 0.0f, 1.0f))
                        * (1.0f + activePressure_ * std::clamp(settings.synth.wireAftertouchDepth, 0.0f, 1.0f)) * ccWireGain_,
                    std::lerp(1.0f, wavVelocity_, std::clamp(settings.synth.transientVelocityDepth, 0.0f, 1.0f))
                        * (1.0f + activePressure_ * std::clamp(settings.synth.transientAftertouchDepth, 0.0f, 1.0f)) * ccTransientGain_
                };
                for (size_t layer = 0; layer < cachedVoice.layers.size(); ++layer) {
                    const auto& stem = cachedVoice.layers[layer];
                    if (stem.left.size() == cachedVoice.left.size() && stem.right.size() == cachedVoice.left.size()) {
                        const float stemL = std::lerp(stem.left[frame], stem.left[next], fraction) * layerGains[layer];
                        const float stemR = std::lerp(stem.right[frame], stem.right[next], fraction) * layerGains[layer];
                        if (layer == 0) { bodyL = stemL; bodyR = stemR; }
                        else if (layer == 1) { wireL = stemL; wireR = stemR; }
                        else { transientL = stemL; transientR = stemR; }
                    }
                }
                wavPlayPosition_ += wavCache_.sampleRate() / sampleRate_;
                currentSmackProgress_ = static_cast<float>(wavPlayPosition_ / static_cast<double>(cachedVoice.left.size()));
            } else {
                wavPlayPosition_ = -1.0;
                currentSmackProgress_ = 1.0f;
            }
        } else {
            if (synth_.isPlaying()) {
                auto activeSettings = settings.synth;
                activeSettings.voiceMode = activeVoiceMode_;
                activeSettings.aftertouchValue = activePressure_;
                activeSettings.bodyPerformanceGain = ccBodyGain_;
                activeSettings.wirePerformanceGain = ccWireGain_;
                activeSettings.transientPerformanceGain = ccTransientGain_;
                if (std::abs(ccBrightness_ - 1.0f) > 0.001f) {
                    activeSettings.noiseLPF = std::clamp(settings.synth.noiseLPF * ccBrightness_, 200.0f, 20000.0f);
                }
                const auto frame = synth_.processSample(activeSettings);
                sL = frame.left;
                sR = frame.right;
                bodyL = frame.bodyLeft;
                bodyR = frame.bodyRight;
                wireL = frame.wireLeft;
                wireR = frame.wireRight;
                transientL = frame.transientLeft;
                transientR = frame.transientRight;
                currentSmackProgress_ = synth_.getEnvelopeProgress();
            } else {
                currentSmackProgress_ = 1.0f;
            }
        }

        if (reversePlayHead_ >= 0 && reversePlayHead_ < reverseDuration_) {
            const size_t reverseIndex = static_cast<size_t>(reversePlayHead_);
            const float reverseL = reverseBuffer_.left[reverseIndex];
            const float reverseR = reverseBuffer_.right[reverseIndex];
            sL += reverseL;
            sR += reverseR;
            bodyL += reverseBuffer_.layers[0].left[reverseIndex];
            bodyR += reverseBuffer_.layers[0].right[reverseIndex];
            wireL += reverseBuffer_.layers[1].left[reverseIndex];
            wireR += reverseBuffer_.layers[1].right[reverseIndex];
            transientL += reverseBuffer_.layers[2].left[reverseIndex];
            transientR += reverseBuffer_.layers[2].right[reverseIndex];
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
        if (layerOutputs.bodyLeft != nullptr) layerOutputs.bodyLeft[sample] = bodyL * outputGain * ccVolume_;
        if (layerOutputs.bodyRight != nullptr) layerOutputs.bodyRight[sample] = bodyR * outputGain * ccVolume_;
        if (layerOutputs.wireLeft != nullptr) layerOutputs.wireLeft[sample] = wireL * outputGain * ccVolume_;
        if (layerOutputs.wireRight != nullptr) layerOutputs.wireRight[sample] = wireR * outputGain * ccVolume_;
        if (layerOutputs.transientLeft != nullptr) layerOutputs.transientLeft[sample] = transientL * outputGain * ccVolume_;
        if (layerOutputs.transientRight != nullptr) layerOutputs.transientRight[sample] = transientR * outputGain * ccVolume_;

        const float framePeak = std::max(std::abs(sL), std::abs(sR));
        blockPeak = std::max(blockPeak, framePeak);

        ppq += ppqPerSample;
    }

    fallbackPpq_ = ppq;
    currentPeak_ = blockPeak;
    currentBeatPhase_ = static_cast<float>(std::fmod(std::fmod(ppq, 1.0) + 1.0, 1.0));
}

} // namespace opensmacker::core
