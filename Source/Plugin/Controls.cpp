#include "Controls.h"
#include "Theme.h"
#include <algorithm>

namespace opensmacker::ui {

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent) {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 17);
    slider_.setWantsKeyboardFocus(true);
    slider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider_.setColour(juce::Slider::textBoxBackgroundColourId, theme::kInk);
    slider_.setColour(juce::Slider::textBoxTextColourId, theme::kText);
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(slider_);

    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    addAndMakeVisible(title_);

    bind(state, parameterId);
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    attachment_ = std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_);
    if (auto* param = state.getParameter(parameterId)) {
        slider_.setName(title_.getText());
        const auto unit = param->getLabel();
        slider_.textFromValueFunction = [param, unit](double value) {
            const auto display = param->getText(param->convertTo0to1(static_cast<float>(value)), 12);
            return unit.isEmpty() ? display : display + " " + unit;
        };
        slider_.updateText();
        slider_.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    }
}

void Knob::resized() {
    auto area = getLocalBounds();
    area = area.withSizeKeepingCentre(area.getWidth(), std::min(106, area.getHeight()));
    title_.setBounds(area.removeFromTop(18));
    slider_.setBounds(area);
}

void Knob::setTooltip(const juce::String& text) {
    slider_.setTooltip(text);
    title_.setTooltip(text);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent) {
    box_.setColour(juce::ComboBox::arrowColourId, accent);
    box_.setColour(juce::ComboBox::backgroundColourId, theme::kInk);
    box_.setColour(juce::ComboBox::outlineColourId, theme::kPanelEdge);
    addAndMakeVisible(box_);

    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    addAndMakeVisible(title_);

    bind(state, parameterId);
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    box_.clear(juce::dontSendNotification);
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId))) {
        box_.addItemList(param->choices, 1);
    } else if (dynamic_cast<juce::AudioParameterBool*>(state.getParameter(parameterId)) != nullptr) {
        box_.addItemList({ "One-shot", "Hold notes" }, 1);
    }
    box_.setName(title_.getText());
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
}

void Choice::resized() {
    auto area = getLocalBounds();
    area = area.withSizeKeepingCentre(area.getWidth(), std::min(64, area.getHeight()));
    title_.setBounds(area.removeFromTop(19));
    box_.setBounds(area.reduced(3, 3));
}

void Choice::setTooltip(const juce::String& text) {
    box_.setTooltip(text);
    title_.setTooltip(text);
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent)
    : attachment_(state, parameterId, button_) {
    button_.setButtonText(text);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    button_.setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    button_.setColour(juce::TextButton::textColourOnId, theme::kInk);
    addAndMakeVisible(button_);
}

void Toggle::resized() {
    auto area = getLocalBounds().reduced(4);
    const int width = std::min(160, area.getWidth());
    const int height = std::min(40, area.getHeight());
    button_.setBounds(area.withSizeKeepingCentre(width, height));
}

void Toggle::setTooltip(const juce::String& text) {
    button_.setTooltip(text);
}

} // namespace opensmacker::ui
