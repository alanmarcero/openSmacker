#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PresetStore.h"
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
    ab_.arm(copySoundState(true));
    undo_ = std::make_unique<suite::ParameterUndo>(
        *this,
        [this] { return copySoundState(false); },
        [this](const juce::ValueTree& tree) { applySoundState(tree, false); });
    std::ranges::for_each(getParameters(), [this](juce::AudioProcessorParameter* parameter) { parameter->addListener(this); });
}

OpenSmackerProcessor::~OpenSmackerProcessor() {
    std::ranges::for_each(getParameters(), [this](juce::AudioProcessorParameter* parameter) { parameter->removeListener(this); });
}

void OpenSmackerProcessor::parameterValueChanged(int, float) {
    if (!loadingSound_.load())
        presetEdited_.store(true);
}

opensmacker::core::EngineSettings OpenSmackerProcessor::currentSettings() const {
    auto settings = opensmacker::params::readSettings(parameters);
    const int model = synthModel_.load(std::memory_order_relaxed);
    settings.synth.articulatedModes = model >= 1;
    settings.synth.shapedEngine = model >= 2;
    return settings;
}

bool OpenSmackerProcessor::frozenHitPlays() const noexcept {
    return displayLockedIn.load() && parameters.getRawParameterValue(opensmacker::params::kAmpEnabled)->load() < 0.5f;
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
    midiNoteOffs_.reserve(capacity);
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
    bool hostTimeline = false;
    if (auto* playHead = getPlayHead()) {
        if (auto posOpt = playHead->getPosition()) {
            hostTimeline = wrapperType != wrapperType_Standalone;
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
    midiNoteOffs_.clear();
    midiCcEvents_.clear();
    midiPressureEvents_.clear();

    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) {
            const float vel = msg.getFloatVelocity();
            midiTriggers_.push_back({
                .sampleOffset = meta.samplePosition,
                .noteNumber = msg.getNoteNumber(),
                .velocity = vel,
                .audition = false,
                .channel = msg.getChannel()
            });
        } else if (msg.isNoteOff()) {
            midiNoteOffs_.push_back({
                .sampleOffset = meta.samplePosition,
                .noteNumber = msg.getNoteNumber(),
                .channel = msg.getChannel()
            });
        } else if (msg.isController()) {
            midiCcEvents_.push_back({
                .sampleOffset = meta.samplePosition,
                .controller = msg.getControllerNumber(),
                .value = msg.getControllerValue(),
                .channel = msg.getChannel()
            });
            // Hosts send CC 120 and up on stop, so Learn skips them.
            if (msg.getControllerNumber() < 120) {
                displayLastController.store(msg.getControllerNumber());
                displayControllerCount.fetch_add(1);
            }
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
        midiTriggers_.push_back({
            .sampleOffset = 0,
            .noteNumber = 69, // A4
            .velocity = 1.0f,
            .audition = true,
            .channel = 1
        });
    }

    const auto settings = currentSettings();
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
        midiCcEvents_, midiPressureEvents_, layerOutputs, midiNoteOffs_);

    const auto& hit = engine_.lastTrigger();
    displayLastNote.store(hit.audition ? kAuditionNote : hit.note, std::memory_order_relaxed);
    displayLastVoice.store(hit.voice, std::memory_order_relaxed);
    displayVelocity.store(hit.velocity, std::memory_order_relaxed);

    // Keeps the highest peak until the editor exchanges it.
    const float peak = engine_.currentPeak();
    float shownPeak = displayPeak.load(std::memory_order_relaxed);
    while (peak > shownPeak && !displayPeak.compare_exchange_weak(shownPeak, peak, std::memory_order_relaxed)) {
    }
    float beatPhase = -1.0f;
    if (hostTimeline) beatPhase = transport.isPlaying ? engine_.currentBeatPhase() : 1.0f;
    displayBeatPhase.store(beatPhase, std::memory_order_relaxed);
    displaySmackProgress.store(engine_.currentSmackProgress());
    displayTriggerCount.store(engine_.triggerSerial());
    displayLockedIn.store(engine_.isSmackLocked());
}

float OpenSmackerProcessor::animationBeatPhase(double seconds) const noexcept {
    const float hostPhase = displayBeatPhase.load(std::memory_order_relaxed);
    if (hostPhase >= 0.0f) return hostPhase;
    const auto beats = seconds * displayBpm.load(std::memory_order_relaxed) / 60.0;
    return static_cast<float>(beats - std::floor(beats));
}

double OpenSmackerProcessor::getTailLengthSeconds() const {
    const auto* enabled = parameters.getRawParameterValue(opensmacker::params::kAmpEnabled);
    if (enabled == nullptr || enabled->load(std::memory_order_relaxed) < 0.5f)
        return 0.0;

    const float releaseMs = parameters.getRawParameterValue(opensmacker::params::kAmpRelease)->load(std::memory_order_relaxed);
    const float reverbMix = parameters.getRawParameterValue(opensmacker::params::kReverbMix)->load(std::memory_order_relaxed);
    const float reverbGate = parameters.getRawParameterValue(opensmacker::params::kReverbGate)->load(std::memory_order_relaxed);
    const float reverbDecay = parameters.getRawParameterValue(opensmacker::params::kReverbDecay)->load(std::memory_order_relaxed);
    const float echoMix = parameters.getRawParameterValue(opensmacker::params::kEchoMix)->load(std::memory_order_relaxed);
    const double reverbTail = reverbMix > 0.001f
        ? std::max(reverbGate, std::min(reverbDecay * 0.60f, 900.0f)) * 0.001 : 0.0;
    const double echoTail = echoMix > 0.001f ? 0.9 : 0.0;
    return std::clamp(static_cast<double>(releaseMs) * 0.001 + std::max(reverbTail, echoTail), 0.0, 23.0);
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
        synthModel_.store(2);
        const bool preservePitch = parameters.getRawParameterValue(opensmacker::params::kLockPitch) != nullptr
            && *parameters.getRawParameterValue(opensmacker::params::kLockPitch) > 0.5f;
        opensmacker::presets::SectionLocks locks{};
        for (size_t i = 0; i < locks.size(); ++i)
            locks[i] = sectionLocks_[i].load(std::memory_order_relaxed);
        loadingSound_.store(true);
        opensmacker::presets::apply(presets[static_cast<size_t>(index)], parameters, locks, preservePitch);
        if (auto* param = parameters.getParameter(opensmacker::params::kPreset)) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
        }
        loadingSound_.store(false);
        parameters.state.setProperty("presetName", juce::String(presets[static_cast<size_t>(index)].name), nullptr);
        presetEdited_.store(false);
    }
}

juce::ValueTree OpenSmackerProcessor::copySoundState(bool includeWav) {
    auto state = parameters.copyState();
    state.setProperty("factoryBank", 2, nullptr);
    state.setProperty("synthModel", synthModel_.load(std::memory_order_relaxed), nullptr);
    state.setProperty("presetEdited", presetEdited_.load(), nullptr);
    std::ranges::for_each(std::views::iota(size_t{ 0 }, sectionLocks_.size()), [this, &state](size_t i) {
        state.setProperty("section_lock_" + juce::String(static_cast<int>(i)), sectionLocks_[i].load(std::memory_order_relaxed), nullptr);
    });
    state.removeProperty("locked_wav", nullptr);
    state.removeProperty("locked_wav_bank", nullptr);
    state.removeProperty("abSlot0", nullptr);
    state.removeProperty("abSlot1", nullptr);
    state.removeProperty("abActive", nullptr);
    state.removeProperty("editorWidth", nullptr);
    state.removeProperty("editorHeight", nullptr);
    if (includeWav && engine_.isSmackLocked()) {
        const auto wavBank = engine_.wavCache().toBankWavBytes();
        if (!wavBank.empty())
            state.setProperty("locked_wav_bank", juce::MemoryBlock(wavBank.data(), wavBank.size()).toBase64Encoding(), nullptr);
    }
    return state;
}

void OpenSmackerProcessor::applySoundState(const juce::ValueTree& incoming, bool restoreWav) {
    if (!incoming.isValid() || !incoming.hasType(parameters.state.getType()))
        return;
    auto state = incoming.createCopy();
    const auto currentSchema = parameters.copyState();
    for (int i = 0; i < currentSchema.getNumChildren(); ++i) {
        const auto child = currentSchema.getChild(i);
        const auto id = child.getProperty("id").toString();
        if (id.isEmpty() || state.getChildWithProperty("id", id).isValid())
            continue;
        auto missing = child.createCopy();
        if (auto* param = parameters.getParameter(id))
            missing.setProperty("value", param->convertFrom0to1(param->getDefaultValue()), nullptr);
        state.addChild(missing, -1, nullptr);
    }
    // Earlier builds played every custom sound on the legacy model.
    const int bank = int(state.getProperty("factoryBank", 0));
    const bool legacySound = bank != 2 || bool(state.getProperty("customSound", false));
    const int savedModel = int(state.getProperty("synthModel", 1));
    synthModel_.store(legacySound ? 0 : savedModel, std::memory_order_relaxed);
    const bool edited = bool(state.getProperty("presetEdited", false));

    const bool hadWav = state.hasProperty("locked_wav_bank") || state.hasProperty("locked_wav");
    const auto wavProperty = state.hasProperty("locked_wav_bank") ? state.getProperty("locked_wav_bank").toString()
                                                                  : state.getProperty("locked_wav").toString();
    state.removeProperty("locked_wav", nullptr);
    state.removeProperty("locked_wav_bank", nullptr);
    state.removeProperty("abSlot0", nullptr);
    state.removeProperty("abSlot1", nullptr);
    state.removeProperty("abActive", nullptr);
    state.removeProperty("editorWidth", nullptr);
    state.removeProperty("editorHeight", nullptr);
    state.removeProperty("customSound", nullptr);
    state.removeProperty("presetEdited", nullptr);

    loadingSound_.store(true);
    parameters.replaceState(state);
    loadingSound_.store(false);
    presetEdited_.store(edited);
    // In earlier saves, locks 0 and 5 also covered the cards added later.
    constexpr std::array<int, 9> coveringLock{ 0, 1, 2, 3, 4, 5, 0, 5, 0 };
    static_assert(coveringLock.size() == static_cast<size_t>(opensmacker::presets::Section::Count));
    for (size_t i = 0; i < sectionLocks_.size(); ++i) {
        const auto earlier = state.getProperty("section_lock_" + juce::String(coveringLock[i]), false);
        sectionLocks_[i].store(state.getProperty("section_lock_" + juce::String(static_cast<int>(i)), earlier),
                               std::memory_order_relaxed);
    }
    if (const auto* preset = parameters.getRawParameterValue(opensmacker::params::kPreset))
        currentProgram_ = juce::jlimit(0, std::max(0, getNumPrograms() - 1), static_cast<int>(*preset));
    if (!legacySound && !state.hasProperty("presetName"))
        parameters.state.setProperty("presetName", getProgramName(currentProgram_), nullptr);

    if (!restoreWav)
        return;
    if (hadWav) {
        juce::MemoryBlock block;
        block.fromBase64Encoding(wavProperty);
        const auto* bytePtr = static_cast<const uint8_t*>(block.getData());
        std::vector<uint8_t> wavBytes(bytePtr, bytePtr + block.getSize());
        const bool restored = engine_.restoreLockedWav(wavBytes, currentSettings().synth.voiceMode);
        displayLockedIn.store(restored);
    } else {
        engine_.unlockSmack();
        displayLockedIn.store(false);
    }
}

void OpenSmackerProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = copySoundState(true);
    ab_.writeProperties(state);
    state.setProperty("editorWidth", editorWidth_.load(), nullptr);
    state.setProperty("editorHeight", editorHeight_.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void OpenSmackerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || !xmlState->hasTagName(parameters.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml(*xmlState);
    if (state.hasProperty("editorWidth"))
        editorWidth_.store(juce::jlimit(suite::kMinEditorWidth, suite::kMaxEditorWidth, int(state.getProperty("editorWidth"))));
    if (state.hasProperty("editorHeight"))
        editorHeight_.store(juce::jlimit(suite::kMinEditorHeight, suite::kMaxEditorHeight, int(state.getProperty("editorHeight"))));
    const bool hadAb = ab_.readProperties(state);
    applySoundState(state, true);
    if (!hadAb)
        ab_.arm(copySoundState(true));
    if (undo_ != nullptr)
        undo_->resetHistory(ab_.active());
    if (auto* editor = getActiveEditor())
        editor->setSize(editorWidth_.load(), editorHeight_.load());
}

bool OpenSmackerProcessor::undo() {
    return undo_ != nullptr && undo_->undo();
}

bool OpenSmackerProcessor::redo() {
    return undo_ != nullptr && undo_->redo();
}

bool OpenSmackerProcessor::canUndo() { return undo_ != nullptr && undo_->canUndo(); }

bool OpenSmackerProcessor::canRedo() const { return undo_ != nullptr && undo_->canRedo(); }

void OpenSmackerProcessor::selectAbSlot(int slot) {
    ab_.select(slot,
               [this] { return copySoundState(true); },
               [this, slot](const juce::ValueTree& tree) {
                   if (undo_ != nullptr)
                       undo_->switchSlot(slot, [this, &tree] { applySoundState(tree, true); });
                   else
                       applySoundState(tree, true);
               });
}

void OpenSmackerProcessor::copyAbSlot(int from, int to) {
    ab_.copy(from, to,
             [this] { return copySoundState(true); },
             [this](const juce::ValueTree& tree) {
                 if (undo_ != nullptr)
                     undo_->suppressDuring([this, &tree] { applySoundState(tree, true); });
                 else
                     applySoundState(tree, true);
             });
    if (undo_ != nullptr)
        undo_->clearSlot(to);
}

void OpenSmackerProcessor::setEditorSize(int width, int height) noexcept {
    editorWidth_.store(juce::jlimit(suite::kMinEditorWidth, suite::kMaxEditorWidth, width));
    editorHeight_.store(juce::jlimit(suite::kMinEditorHeight, suite::kMaxEditorHeight, height));
}

void OpenSmackerProcessor::setPresetDocumentsRoot(juce::File documentsRoot) {
    presetDocumentsRoot_ = std::move(documentsRoot);
}

suite::PresetStore OpenSmackerProcessor::presetStore() const {
    return suite::PresetStore("openSmacker", presetDocumentsRoot_);
}

bool OpenSmackerProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = suite::PresetStore::cleanName(name);
    if (clean.isEmpty() || !presetStore().saveUserPreset(clean, currentPresetXml()))
        return false;
    parameters.state.setProperty("presetName", clean, nullptr);
    presetEdited_.store(false);
    return true;
}

bool OpenSmackerProcessor::renameUserPreset(const juce::String& oldName, const juce::String& newName) {
    return presetStore().renameUserPreset(oldName, newName);
}

bool OpenSmackerProcessor::deleteUserPreset(const juce::String& name) {
    return presetStore().deleteUserPreset(name);
}

juce::String OpenSmackerProcessor::currentPresetXml() {
    if (auto xml = copySoundState(true).createXml())
        return xml->toString();
    return {};
}

bool OpenSmackerProcessor::loadUserPresetXml(const juce::String& name, const juce::String& xml) {
    if (auto parsed = juce::parseXML(xml)) {
        auto tree = juce::ValueTree::fromXml(*parsed);
        if (!tree.isValid())
            return false;
        applySoundState(tree, true);
        parameters.state.setProperty("presetName", name, nullptr);
        presetEdited_.store(false);
        return true;
    }
    return false;
}

bool OpenSmackerProcessor::loadUserPreset(const juce::String& name) {
    const auto xml = presetStore().loadUserPreset(name);
    return xml.isNotEmpty() && loadUserPresetXml(name, xml);
}

std::vector<uint8_t> OpenSmackerProcessor::exportHitWav() const {
    if (engine_.isSmackLocked()) {
        auto locked = engine_.wavCache().toWavBytes();
        if (!locked.empty())
            return locked;
    }
    opensmacker::core::SmackEngine offline;
    const double rate = getSampleRate() > 8000.0 ? getSampleRate() : 48000.0;
    offline.prepare(rate);
    auto settings = currentSettings();
    settings.lockIn = true;
    settings.synth.fullAdsrEnabled = false;
    offline.renderLockedHitOffline(settings);
    return offline.wavCache().toWavBytes();
}

juce::File OpenSmackerProcessor::createHitWavFile() const {
    const auto bytes = exportHitWav();
    if (bytes.empty())
        return {};
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("Alan Marcero")
                         .getChildFile("openSmacker");
    directory.createDirectory();
    auto file = directory.getNonexistentChildFile("openSmacker-hit", ".wav", false);
    if (!file.replaceWithData(bytes.data(), bytes.size()))
        return {};
    return file;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenSmackerProcessor();
}
