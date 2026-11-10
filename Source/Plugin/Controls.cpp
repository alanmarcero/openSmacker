#include "Controls.h"
#include "Theme.h"

namespace opensmacker::ui {

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent) {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(slider_);

    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kTextDim.brighter(0.2f));
    addAndMakeVisible(title_);

    bind(state, parameterId);
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    attachment_ = std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_);
}

void Knob::resized() {
    auto area = getLocalBounds();
    title_.setBounds(area.removeFromTop(16));
    slider_.setBounds(area);
}

void Knob::setTooltip(const juce::String& text) {
    slider_.setTooltip(text);
    title_.setTooltip(text);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent) {
    box_.setColour(juce::ComboBox::arrowColourId, accent);
    box_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0c141e));
    box_.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff223246));
    addAndMakeVisible(box_);

    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kTextDim.brighter(0.2f));
    addAndMakeVisible(title_);

    bind(state, parameterId);
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    box_.clear(juce::dontSendNotification);
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId))) {
        box_.addItemList(param->choices, 1);
        attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
    }
}

void Choice::resized() {
    auto area = getLocalBounds();
    title_.setBounds(area.removeFromTop(16));
    box_.setBounds(area.reduced(2, 4));
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
    button_.setColour(juce::TextButton::buttonOnColourId, accent.withAlpha(0.32f));
    button_.setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    button_.setColour(juce::TextButton::textColourOnId, accent);
    addAndMakeVisible(button_);
}

void Toggle::resized() {
    button_.setBounds(getLocalBounds().reduced(2));
}

void Toggle::setTooltip(const juce::String& text) {
    button_.setTooltip(text);
}

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components) {
    if (components.size() == 0) return;
    const int count = static_cast<int>(components.size());
    const int itemWidth = area.getWidth() / count;
    int idx = 0;
    for (auto* c : components) {
        if (c != nullptr) {
            c->setBounds(area.getX() + idx * itemWidth, area.getY(), itemWidth, area.getHeight());
        }
        idx++;
    }
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent) {
    auto bounds = area.toFloat().reduced(0.5f);

    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff121a26), bounds.getTopLeft(),
        juce::Colour(0xff080c14), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds.reduced(0.5f), 6.0f);

    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    auto titleBar = bounds.removeFromTop(22.0f);
    g.setColour(juce::Colour(0xff090e17));
    g.fillRoundedRectangle(titleBar, 6.0f);
    g.fillRect(titleBar.removeFromBottom(6.0f));

    const auto topBar = bounds.removeFromTop(3.0f).reduced(6.0f, 0.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(topBar, 1.5f);

    if (title.isNotEmpty()) {
        g.setColour(theme::kText);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(title, area.getX() + 12, area.getY() + 3, area.getWidth() - 24, 18, juce::Justification::left, true);
    }
}

void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text) {
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(text, area, juce::Justification::centred, true);
}

void drawGlow(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour) {
    const juce::Graphics::ScopedSaveState state(g);
    g.setColour(colour.withAlpha(0.25f));
    g.fillEllipse(area);
}

} // namespace opensmacker::ui
