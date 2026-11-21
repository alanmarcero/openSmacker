#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace opensmacker::ui {

using Apvts = juce::AudioProcessorValueTreeState;

class Knob : public juce::Component {
public:
    Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent, bool stepper = false);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::Slider& slider() noexcept { return slider_; }

private:
    juce::Slider slider_;
    juce::Label title_;
    std::unique_ptr<Apvts::SliderAttachment> attachment_;
};

/** Dual concentric rotary EQ control: inner cap sets -12..+12 dB, outer collar glides Hz smoothly. */
class DualEqKnob final : public juce::Component {
public:
    DualEqKnob(Apvts& state, const juce::String& freqId, const juce::String& dbId,
               const juce::String& title, juce::Colour dbAccent, juce::Colour freqAccent);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::Slider& slider() noexcept { return dbSlider_; }

private:
    class GainSlider final : public juce::Slider {
    public:
        using juce::Slider::Slider;
        bool hitTest(int x, int y) override;
    };

    [[nodiscard]] juce::Rectangle<float> dialBounds() const;
    [[nodiscard]] juce::Rectangle<int> dbPillBounds() const;
    [[nodiscard]] juce::Rectangle<int> freqPillBounds() const;
    [[nodiscard]] bool hitsOuterRing(juce::Point<float> pos) const;

    Apvts& state_;
    juce::String freqId_;
    juce::String dbId_;
    juce::Colour dbAccent_;
    juce::Colour freqAccent_;
    juce::Label title_;
    juce::Label dbReadout_;
    GainSlider dbSlider_;
    std::unique_ptr<Apvts::SliderAttachment> dbAttachment_;
    std::unique_ptr<juce::ParameterAttachment> freqAttachment_;
    float freqHz_ = 1000.0f;
    float dragStartNorm_ = 0.5f;
    float dragStartY_ = 0.0f;
    bool draggingFreq_ = false;
    bool hoveringFreq_ = false;
};

class Choice : public juce::Component {
public:
    Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::ComboBox& box() noexcept { return box_; }

private:
    juce::ComboBox box_;
    juce::Label title_;
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment_;
};

class Toggle : public juce::Component {
public:
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent);
    void resized() override;
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::TextButton& button() noexcept { return button_; }

private:
    juce::TextButton button_;
    Apvts::ButtonAttachment attachment_;
};

} // namespace opensmacker::ui
