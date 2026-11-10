#pragma once

#include "Controls.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

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

class SmackSynthPanel : public juce::Component {
public:
    explicit SmackSynthPanel(OpenSmackerProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }

private:
    bool advancedMode_ = false;

    // Pre-Strike Flam / Drag Roll
    Knob preBursts_;
    Knob preSpread_;
    Knob flamSpread_;
    Knob preLevel_;

    // Tunable Drum / Bell / Tom Body
    Choice voiceMode_;
    Choice toneNote_;
    Knob toneOctave_;
    Knob toneFine_;
    Toggle lockPitchBtn_;
    Knob toneQ_;
    Knob toneDecay_;
    Knob tonePitchEnv_;
    Knob fmAmount_;
    Knob toneLevel_;

    // Snare Wires, Sizzle & Air
    Choice noiseType_;
    Knob noiseDecay_;
    Knob noiseHPF_;
    Knob noiseLPF_;
    Knob noiseWidth_;
    Knob noiseLevel_;

    // Stick / Mallet / Rim Transient
    Choice clickType_;
    Knob clickLevel_;
    Knob clickFreq_;

    juce::Rectangle<int> attackSectionBounds_;
    juce::Rectangle<int> tailSectionBounds_;
    juce::Rectangle<int> noiseSectionBounds_;
};

class EffectsPanel : public juce::Component {
public:
    explicit EffectsPanel(OpenSmackerProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }

private:
    bool advancedMode_ = false;

    // Distortion
    Choice distType_;
    Knob distDrive_;
    Knob distTone_;
    Knob distMix_;

    // Compressor
    Knob compThresh_;
    Knob compRatio_;
    Knob compMakeup_;
    Knob compMix_;

    // Limiter
    Knob limiterCeiling_;
    Toggle limiterEnabled_;

    // Gated Reverb
    Knob reverbSize_;
    Knob reverbDecay_;
    Knob reverbGate_;
    Knob reverbMix_;

    // Reverse & Master
    Choice reverseMode_;
    Knob reverseMix_;
    Choice midiMode_;
    Toggle lockInBtn_;
    Knob masterWidth_;
    Knob outputGain_;
};

} // namespace opensmacker::ui
