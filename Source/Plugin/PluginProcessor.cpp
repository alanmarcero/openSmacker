#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"

OpenSmackerProcessor::OpenSmackerProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withOutput("Body", juce::AudioChannelSet::stereo(), false)
          .withOutput("Wires", juce::AudioChannelSet::stereo(), false)
          .withOutput("Transient", juce::AudioChannelSet::stereo(), false)),
      parameters(*this, nullptr, "PARAMETERS", opensmacker::params::createParameterLayout()) {
    for (auto& locked : sectionLocks_)
        locked.store(false, std::memory_order_relaxed);
    setCurrentProgram(0);
}

bool OpenSmackerProcessor::isSectionLocked(opensmacker::presets::Section section) const noexcept {
    const auto index = static_cast<size_t>(section);
    return index < sectionLocks_.size() && sectionLocks_[index].load(std::memory_order_relaxed);
}

void OpenSmackerProcessor::setSectionLocked(opensmacker::presets::Section section, bool locked) {
    const auto index = static_cast<size_t>(section);
    if (index >= sectionLocks_.size())
        return;
    sectionLocks_[index].store(locked, std::memory_order_relaxed);
    parameters.state.setProperty("section_lock_" + juce::String(static_cast<int>(index)), locked, nullptr);
}

void OpenSmackerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate);
    const auto capacity = static_cast<size_t>(std::max(256, samplesPerBlock * 4));
    midiTriggers_.reserve(capacity);
    midiCcEvents_.reserve(capacity);
    midiPressureEvents_.reserve(capacity);
}

bool OpenSmackerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) {
        return false;
    }
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus) {
        const auto channels = layouts.getChannelSet(false, bus);
        if (channels != juce::AudioChannelSet::disabled() && channels != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void OpenSmackerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch) {
        buffer.clear(ch, 0, numSamples);
    }

    if (numChannels < 2) {
        return;
    }

    opensmacker::core::Transport transport;
    if (auto* playHead = getPlayHead()) {
        if (auto posOpt = playHead->getPosition()) {
            if (posOpt->getBpm()) {
                transport.bpm = *posOpt->getBpm();
                displayBpm.store(static_cast<float>(transport.bpm));
            }
            if (posOpt->getPpqPosition()) {
                transport.ppqPosition = *posOpt->getPpqPosition();
            }
            if (posOpt->getTimeSignature()) {
                transport.beatsPerBar = static_cast<double>(posOpt->getTimeSignature()->numerator);
                displayBeatsPerBar.store(static_cast<float>(transport.beatsPerBar));
            }
            transport.isPlaying = posOpt->getIsPlaying();
        }
    }

    midiTriggers_.clear();
    midiCcEvents_.clear();
    midiPressureEvents_.clear();

    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) {
            const float vel = msg.getFloatVelocity();
            displayVelocity.store(vel, std::memory_order_relaxed);
            midiTriggers_.push_back({
                .sampleOffset = meta.samplePosition,
                .noteNumber = msg.getNoteNumber(),
                .velocity = vel,
                .audition = false,
                .channel = msg.getChannel()
            });
        } else if (msg.isController()) {
            midiCcEvents_.push_back({
                .sampleOffset = meta.samplePosition,
                .controller = msg.getControllerNumber(),
                .value = msg.getControllerValue(),
                .channel = msg.getChannel()
            });
        } else if (msg.isChannelPressure()) {
            midiPressureEvents_.push_back({
                .sampleOffset = meta.samplePosition,
                .channel = msg.getChannel(),
                .noteNumber = -1,
                .value = msg.getChannelPressureValue(),
                .polyphonic = false
            });
        } else if (msg.isAftertouch()) {
            midiPressureEvents_.push_back({
                .sampleOffset = meta.samplePosition,
                .channel = msg.getChannel(),
                .noteNumber = msg.getNoteNumber(),
                .value = msg.getAfterTouchValue(),
                .polyphonic = true
            });
        }
    }

    if (auditionPending_.exchange(false)) {
        displayVelocity.store(1.0f, std::memory_order_relaxed);
        midiTriggers_.push_back({
            .sampleOffset = 0,
            .noteNumber = 69, // A4
            .velocity = 1.0f,
            .audition = true,
            .channel = 1
        });
    }

    const auto settings = opensmacker::params::readSettings(parameters);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    if (mainOutput.getNumChannels() < 2)
        return;
    float* channelPointers[2] = { mainOutput.getWritePointer(0), mainOutput.getWritePointer(1) };
    auto bodyOutput = getBusBuffer(buffer, false, 1);
    auto wireOutput = getBusBuffer(buffer, false, 2);
    auto transientOutput = getBusBuffer(buffer, false, 3);
    opensmacker::core::LayerOutputChannels layerOutputs;
    if (bodyOutput.getNumChannels() >= 2) {
        layerOutputs.bodyLeft = bodyOutput.getWritePointer(0);
        layerOutputs.bodyRight = bodyOutput.getWritePointer(1);
    }
    if (wireOutput.getNumChannels() >= 2) {
        layerOutputs.wireLeft = wireOutput.getWritePointer(0);
        layerOutputs.wireRight = wireOutput.getWritePointer(1);
    }
    if (transientOutput.getNumChannels() >= 2) {
        layerOutputs.transientLeft = transientOutput.getWritePointer(0);
        layerOutputs.transientRight = transientOutput.getWritePointer(1);
    }

    engine_.process(settings, transport, midiTriggers_, channelPointers, 2, numSamples,
        midiCcEvents_, midiPressureEvents_, layerOutputs);

    displayPeak.store(engine_.currentPeak());
    displayBeatPhase.store(engine_.currentBeatPhase());
    displaySmackProgress.store(engine_.currentSmackProgress());
    displayTriggerCount.store(engine_.triggerSerial());
    displayLockedIn.store(engine_.isSmackLocked());
}

juce::AudioProcessorEditor* OpenSmackerProcessor::createEditor() {
    return new OpenSmackerEditor(*this);
}

int OpenSmackerProcessor::getNumPrograms() {
    return static_cast<int>(opensmacker::presets::factoryPresets().size());
}

const juce::String OpenSmackerProcessor::getProgramName(int index) {
    const auto& presets = opensmacker::presets::factoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
        return presets[static_cast<size_t>(index)].name;
    }
    return {};
}

void OpenSmackerProcessor::setCurrentProgram(int index) {
    const auto& presets = opensmacker::presets::factoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
        currentProgram_ = index;
        const bool preservePitch = parameters.getRawParameterValue(opensmacker::params::kLockPitch) != nullptr
            && *parameters.getRawParameterValue(opensmacker::params::kLockPitch) > 0.5f;
        opensmacker::presets::SectionLocks locks{};
        for (size_t i = 0; i < locks.size(); ++i)
            locks[i] = sectionLocks_[i].load(std::memory_order_relaxed);
        opensmacker::presets::apply(presets[static_cast<size_t>(index)], parameters, locks, preservePitch);
        if (auto* param = parameters.getParameter(opensmacker::params::kPreset)) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
        }
    }
}

void OpenSmackerProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.removeProperty("locked_wav", nullptr);
    state.removeProperty("locked_wav_bank", nullptr);
    if (engine_.isSmackLocked()) {
        const auto wavBank = engine_.wavCache().toBankWavBytes();
        if (!wavBank.empty()) {
            state.setProperty("locked_wav_bank", juce::MemoryBlock(wavBank.data(), wavBank.size()).toBase64Encoding(), nullptr);
        }
    }
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OpenSmackerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xmlState);
        const auto currentSchema = parameters.copyState();
        for (int i = 0; i < currentSchema.getNumChildren(); ++i) {
            const auto child = currentSchema.getChild(i);
            if (child.hasProperty("id") && !state.getChildWithProperty("id", child.getProperty("id")).isValid())
                state.addChild(child.createCopy(), -1, nullptr);
        }
        parameters.replaceState(state);
        for (size_t i = 0; i < sectionLocks_.size(); ++i) {
            const auto key = "section_lock_" + juce::String(static_cast<int>(i));
            sectionLocks_[i].store(state.getProperty(key, false), std::memory_order_relaxed);
        }
        if (const auto* preset = parameters.getRawParameterValue(opensmacker::params::kPreset))
            currentProgram_ = juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(*preset));
        if (state.hasProperty("locked_wav_bank") || state.hasProperty("locked_wav")) {
            juce::MemoryBlock block;
            const auto wavProperty = state.hasProperty("locked_wav_bank") ? "locked_wav_bank" : "locked_wav";
            block.fromBase64Encoding(state.getProperty(wavProperty).toString());
            const auto* bytePtr = static_cast<const uint8_t*>(block.getData());
            std::vector<uint8_t> wavBytes(bytePtr, bytePtr + block.getSize());
            const auto restoredSettings = opensmacker::params::readSettings(parameters);
            const bool restored = engine_.restoreLockedWav(wavBytes, restoredSettings.synth.voiceMode);
            displayLockedIn.store(restored);
        } else {
            engine_.unlockSmack();
            displayLockedIn.store(false);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenSmackerProcessor();
}
