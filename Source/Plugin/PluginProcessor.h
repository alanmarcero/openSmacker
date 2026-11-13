#pragma once

#include "SmackEngine.h"
#include "Presets.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

class OpenSmackerProcessor : public juce::AudioProcessor {
public:
    OpenSmackerProcessor();

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void triggerAudition() noexcept { auditionPending_.store(true); }
    [[nodiscard]] bool isSmackLockedIn() const noexcept { return displayLockedIn.load(); }
    [[nodiscard]] bool isSectionLocked(opensmacker::presets::Section section) const noexcept;
    void setSectionLocked(opensmacker::presets::Section section, bool locked);

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> displayPeak{ 0.0f };
    std::atomic<float> displayBeatPhase{ 0.0f };
    std::atomic<float> displayBpm{ 138.0f };
    std::atomic<float> displayBeatsPerBar{ 4.0f };
    std::atomic<float> displaySmackProgress{ 1.0f };
    std::atomic<float> displayVelocity{ 1.0f };
    std::atomic<uint32_t> displayTriggerCount{ 0 };
    std::atomic<bool> displayLockedIn{ false };

private:
    opensmacker::core::SmackEngine engine_;
    std::atomic<bool> auditionPending_{ false };
    int currentProgram_ = 0;
    std::array<std::atomic<bool>, static_cast<size_t>(opensmacker::presets::Section::Count)> sectionLocks_{};
};
