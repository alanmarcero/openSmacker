#pragma once

#include "Controls.h"
#include "Presets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class OpenSmackerProcessor;

namespace opensmacker::ui {

class SmackerStagePanel : public juce::Component, private juce::Timer {
public:
    explicit SmackerStagePanel(OpenSmackerProcessor& processor);
    ~SmackerStagePanel() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    void timerCallback() override;
    OpenSmackerProcessor& processor_;
    juce::Image stageArtwork_;
    juce::TextButton auditionBtn_{ "PUSH IT" };
    uint32_t seenTriggers_ = 0;
    double smackTriggerTimeMs_ = 0.0;
    float smackPose_ = 0.0f;
    float shockwave_ = 0.0f;
    float impactIntensity_ = 1.0f;
    float impactFlash_ = 0.0f;
    float idleBob_ = 0.0f;
};

class ParameterCard : public juce::Component {
public:
    ParameterCard(OpenSmackerProcessor& processor, juce::String title,
                  presets::Section section, juce::Colour accent);
    void addParameter(const juce::String& id, const juce::String& title);
    void setRows(std::vector<int> rows);
    void setAdvancedMode(bool advanced);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    OpenSmackerProcessor& processor_;
    juce::String title_;
    presets::Section section_;
    juce::Colour accent_;
    juce::TextButton lockButton_{ "LOCK" };
    std::vector<std::unique_ptr<juce::Component>> controls_;
    std::vector<int> rows_;
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
    ParameterCard transientCard_;
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

} // namespace opensmacker::ui
