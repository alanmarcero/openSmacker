#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace opensmacker::ui {

using Apvts = juce::AudioProcessorValueTreeState;
using PopupCallback = std::function<void(const juce::MouseEvent&)>;

void drawPadlockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent);

class PadlockBadge final : public juce::Component, public juce::SettableTooltipClient {
public:
    PadlockBadge(juce::Colour accent, std::function<void(const juce::MouseEvent&)> onAction);
    void setAccent(juce::Colour accent) noexcept { accent_ = accent; }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    juce::Colour accent_;
    std::function<void(const juce::MouseEvent&)> onAction_;
};

class PopupSlider : public juce::Slider {
public:
    using juce::Slider::Slider;
    PopupCallback onPopupMenu;
    void mouseDown(const juce::MouseEvent& event) override;
};

class PopupComboBox : public juce::ComboBox {
public:
    using juce::ComboBox::ComboBox;
    PopupCallback onPopupMenu;
    void setActiveItemId(int itemId) noexcept { activeItemId_ = itemId; }
    void mouseDown(const juce::MouseEvent& event) override;
    void showPopup() override;

private:
    int activeItemId_ = 0;
};

class PopupTextButton : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    PopupCallback onPopupMenu;
    void mouseDown(const juce::MouseEvent& event) override;
};

class Knob : public juce::Component {
public:
    Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent, bool stepper = false);
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setOnPopupMenu(PopupCallback callback);
    [[nodiscard]] juce::Slider& slider() noexcept { return slider_; }

private:
    PopupSlider slider_;
    juce::Label title_;
    std::unique_ptr<Apvts::SliderAttachment> attachment_;
    PopupCallback onPopupMenu_;
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
    bool keyPressed(const juce::KeyPress& key) override;
    void setOnPopupMenu(std::function<void(const juce::String& paramId, const juce::MouseEvent&)> callback) {
        onPopupMenu_ = std::move(callback);
    }
    [[nodiscard]] juce::Slider& slider() noexcept { return dbSlider_; }

private:
    class GainSlider final : public PopupSlider {
    public:
        using PopupSlider::PopupSlider;
        bool hitTest(int x, int y) override;
    };

    [[nodiscard]] juce::Rectangle<float> dialBounds() const;
    [[nodiscard]] juce::Rectangle<int> dbPillBounds() const;
    [[nodiscard]] juce::Rectangle<int> freqPillBounds() const;
    [[nodiscard]] bool hitsOuterRing(juce::Point<float> pos) const;
    void updateFreqReadout();
    void stepSemitones(float deltaSemitones);

    Apvts& state_;
    juce::String freqId_;
    juce::String dbId_;
    juce::Colour dbAccent_;
    juce::Colour freqAccent_;
    juce::Label title_;
    juce::Label dbReadout_;
    juce::Label freqReadout_;
    GainSlider dbSlider_;
    std::unique_ptr<Apvts::SliderAttachment> dbAttachment_;
    std::unique_ptr<juce::ParameterAttachment> freqAttachment_;
    float freqHz_ = 1000.0f;
    float dragNorm_ = 0.5f;
    float lastDragY_ = 0.0f;
    bool draggingFreq_ = false;
    bool hoveringFreq_ = false;
    std::function<void(const juce::String& paramId, const juce::MouseEvent&)> onPopupMenu_;
};

class Choice : public juce::Component {
public:
    Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setOnPopupMenu(PopupCallback callback);
    [[nodiscard]] juce::ComboBox& box() noexcept { return box_; }

private:
    PopupComboBox box_;
    juce::Label title_;
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment_;
    PopupCallback onPopupMenu_;
};

class Toggle : public juce::Component {
public:
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent);
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void setOnPopupMenu(PopupCallback callback);
    [[nodiscard]] juce::TextButton& button() noexcept { return button_; }

private:
    PopupTextButton button_;
    Apvts::ButtonAttachment attachment_;
    PopupCallback onPopupMenu_;
};

} // namespace opensmacker::ui
