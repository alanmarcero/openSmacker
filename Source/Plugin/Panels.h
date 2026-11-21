#pragma once

#include "Controls.h"
#include "Presets.h"
#include "SmackSynth.h"
#include <atomic>
#include <optional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class OpenSmackerProcessor;

namespace opensmacker::ui {

class SmackerStagePanel : public juce::Component, private juce::Timer {
public:
    explicit SmackerStagePanel(OpenSmackerProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
private:
    void updateWaveform(const core::SmackSynthSettings& settings);
    OpenSmackerProcessor& processor_;
    juce::TextButton unfreeze_{ "Unfreeze" };
    std::optional<core::SmackSynthSettings> lastSettings_;
    bool showsFrozenHit_ = false;
    std::array<float, 400> waveform_{};
    int previewMs_ = 0;

};

// Why a control has no effect in the current state, or an empty string when it acts.
// hitFrozen is true while a frozen hit is stored.
[[nodiscard]] juce::String inactiveReason(const juce::AudioProcessorValueTreeState& state, const juce::String& id, bool hitFrozen);

// Lamp that flashes with each hit's velocity, beside the last note and the voice it played.
class HitMonitor : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit HitMonitor(OpenSmackerProcessor& processor);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    [[nodiscard]] const juce::String& readout() const noexcept { return readout_; }
private:
    OpenSmackerProcessor& processor_;
    uint32_t lastTriggerCount_ = 0;
    float glow_ = 0.0f;
    juce::String readout_;
};

// Output peak in dBFS with a 1 s hold, a ceiling mark, and a clip light that a click clears.
class PeakMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    PeakMeter(std::atomic<float>& peak, const std::atomic<float>& ceilingDb);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void timerCallback() override;
    [[nodiscard]] bool isClipped() const noexcept { return clipped_; }
    [[nodiscard]] float holdDb() const noexcept { return holdDb_; }
private:
    std::atomic<float>& peak_;
    const std::atomic<float>& ceilingDb_;
    float levelDb_ = -60.0f;
    float holdDb_ = -60.0f;
    double holdStartMs_ = 0.0;
    bool clipped_ = false;
};

class ParameterCard : public juce::Component, private juce::Timer {
public:
    ParameterCard(OpenSmackerProcessor& processor, juce::String title,
                  presets::Section section, juce::Colour accent, bool lockable = true);
    void addParameter(const juce::String& id, const juce::String& title, const juce::String& tip);
    void addDualEq(const juce::String& freqId, const juce::String& dbId,
                   const juce::String& title, juce::Colour freqAccent, const juce::String& tip);
    void addControl(std::unique_ptr<juce::Component> control);
    void setRows(std::vector<int> basic, std::vector<int> advanced = {});
    void setWeights(std::vector<float> weights);
    void setAdvancedMode(bool advanced);
    void paint(juce::Graphics& g) override;
    void resized() override;
    // Dims each control that has no effect now and adds the reason to its tooltip.
    void refreshActivity();
private:
    struct Activity {
        juce::String id;
        juce::String tip;
        juce::String reason;
    };
    void timerCallback() override;
    void visibilityChanged() override;
    OpenSmackerProcessor& processor_;
    juce::String title_;
    presets::Section section_;
    juce::Colour accent_;
    bool lockable_ = true;
    juce::TextButton lockButton_{ "LOCK" };
    std::vector<std::unique_ptr<juce::Component>> controls_;
    std::vector<Activity> activity_;
    std::vector<int> basicRows_, advancedRows_;
    std::vector<float> weights_;
    bool advancedMode_ = false;
};

class SmackSynthPanel : public juce::Component {
public:
    explicit SmackSynthPanel(OpenSmackerProcessor& processor);
    void setPage(int page);
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }
    void resized() override;
private:
    bool advancedMode_ = false;
    int page_ = 0;
    ParameterCard voiceCard_;
    ParameterCard detailCard_;
    ParameterCard transientCard_;
    ParameterCard envelopeCard_;
    ParameterCard outputCard_;
    ParameterCard preStrikeCard_;
    ParameterCard wiresCard_;
};

class EffectsPanel : public juce::Component {
public:
    explicit EffectsPanel(OpenSmackerProcessor& processor);
    void setAdvancedMode(bool advanced);
    void resized() override;
private:
    bool advancedMode_ = false;
    ParameterCard dynamicsCard_;
    ParameterCard spaceCard_;
};

class PerformancePanel : public juce::Component {
public:
    explicit PerformancePanel(OpenSmackerProcessor& processor);
    void setAdvancedMode(bool advanced);
    void resized() override;
private:
    bool advancedMode_ = false;
    ParameterCard zonesCard_;
    ParameterCard responseCard_;
    ParameterCard rollCard_;
};

} // namespace opensmacker::ui
