#pragma once

#include "SmackEngine.h"
#include "Presets.h"
#include "AbCompare.h"
#include "EditorSizing.h"
#include "ParameterUndo.h"
#include "PresetStore.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class OpenSmackerProcessor : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener {
public:
    OpenSmackerProcessor();
    ~OpenSmackerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "openSmacker"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    opensmacker::core::EngineSettings currentSettings() const;
    // Empty when no preset load named the sound.
    [[nodiscard]] juce::String presetName() const { return parameters.state.getProperty("presetName").toString(); }
    [[nodiscard]] bool isPresetEdited() const noexcept { return presetEdited_.load(); }
    void triggerAudition() noexcept { auditionPending_.store(true); }
    [[nodiscard]] bool isSmackLockedIn() const noexcept { return displayLockedIn.load(); }
    // A hit is stored and MIDI gate is One-shot, so the engine plays the stored hit.
    [[nodiscard]] bool frozenHitPlays() const noexcept;
    // Returns false when no hit is stored or the audio thread holds it.
    [[nodiscard]] bool copyFrozenHit(opensmacker::core::VoiceMode voice, std::vector<float>& left, std::vector<float>& right,
                                     double& sampleRate) const {
        return engine_.copyStoredVoice(voice, left, right, sampleRate);
    }
    [[nodiscard]] bool isSectionLocked(opensmacker::presets::Section section) const noexcept;
    void setSectionLocked(opensmacker::presets::Section section, bool locked);
    [[nodiscard]] bool isParamLocked(const juce::String& paramId) const noexcept;
    void setParamLocked(const juce::String& paramId, bool locked);
    void unlockAllParams();
    [[nodiscard]] bool hasLockedParams() const noexcept;

    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo();
    [[nodiscard]] bool canRedo() const;

    void selectAbSlot(int slot);
    void copyAbSlot(int from, int to);
    [[nodiscard]] int abSlot() const noexcept { return ab_.active(); }

    void setEditorSize(int width, int height) noexcept;
    [[nodiscard]] int editorWidth() const noexcept { return editorWidth_.load(); }
    [[nodiscard]] int editorHeight() const noexcept { return editorHeight_.load(); }

    void setPresetDocumentsRoot(juce::File documentsRoot);
    [[nodiscard]] bool saveUserPreset(const juce::String& name);
    [[nodiscard]] bool renameUserPreset(const juce::String& oldName, const juce::String& newName);
    [[nodiscard]] bool deleteUserPreset(const juce::String& name);
    [[nodiscard]] bool loadUserPreset(const juce::String& name);
    [[nodiscard]] bool loadUserPresetXml(const juce::String& name, const juce::String& xml);
    [[nodiscard]] juce::String currentPresetXml();
    [[nodiscard]] suite::PresetStore presetStore() const;

    // Stereo float WAV of the locked hit, or a fresh offline render when nothing is locked.
    [[nodiscard]] std::vector<uint8_t> exportHitWav() const;
    [[nodiscard]] juce::File createHitWavFile() const;

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> displayPeak{ 0.0f };
    float animationBeatPhase(double seconds) const noexcept;
    std::atomic<float> displayBeatPhase{ -1.0f };
    std::atomic<float> displayBpm{ 138.0f };
    std::atomic<float> displayBeatsPerBar{ 4.0f };
    std::atomic<float> displaySmackProgress{ 1.0f };
    std::atomic<float> displayVelocity{ 1.0f };
    std::atomic<uint32_t> displayTriggerCount{ 0 };
    // MIDI note, kAuditionNote, or -1 before any hit.
    static constexpr int kAuditionNote = 128;
    std::atomic<int> displayLastNote{ -1 };
    // -1 when no key zone takes the note.
    std::atomic<int> displayLastVoice{ -1 };
    std::atomic<int> displayLastController{ 0 };
    std::atomic<uint32_t> displayControllerCount{ 0 };
    std::atomic<int> displayLastNoteOn{ 0 };
    std::atomic<uint32_t> displayNoteOnCount{ 0 };
    std::atomic<bool> displayLockedIn{ false };

private:
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}
    [[nodiscard]] juce::ValueTree copySoundState(bool includeWav);
    void applySoundState(const juce::ValueTree& state, bool restoreWav);

    opensmacker::core::SmackEngine engine_;
    std::unique_ptr<suite::ParameterUndo> undo_;
    suite::AbCompare ab_;
    std::atomic<int> editorWidth_ { suite::kDefaultEditorWidth };
    std::atomic<int> editorHeight_ { suite::kDefaultEditorHeight };
    juce::File presetDocumentsRoot_;
    std::atomic<bool> auditionPending_{ false };
    std::atomic<int> currentProgram_{ 0 };
    std::atomic<bool> presetEdited_{ false };
    // 0 legacy ratios, 1 articulated voices, 2 shaped one-shots. Missing on old saves.
    std::atomic<int> synthModel_{ 2 };
    std::array<std::atomic<bool>, static_cast<size_t>(opensmacker::presets::Section::Count)> sectionLocks_{};
    std::unordered_map<std::string, std::atomic<bool>> paramLocks_;
    std::vector<opensmacker::core::MidiTrigger> midiTriggers_;
    std::vector<opensmacker::core::MidiNoteOff> midiNoteOffs_;
    std::vector<opensmacker::core::MidiCcEvent> midiCcEvents_;
    std::vector<opensmacker::core::MidiPressureEvent> midiPressureEvents_;
};
