#include "EdmLookAndFeel.h"
#include "Theme.h"

namespace opensmacker::ui {

namespace {

constexpr float kArcThickness = 2.8f;
constexpr float kCornerRadius = 5.0f;
constexpr float kControlFontSize = 14.0f;
constexpr float kToggleLightSize = 9.0f;
constexpr float kToggleLightInset = 14.0f;

} // namespace

EdmLookAndFeel::EdmLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, theme::kBackground);
    setColour(juce::Label::textColourId, theme::kText);
    setColour(juce::Slider::textBoxTextColourId, theme::kText);
    setColour(juce::Slider::textBoxOutlineColourId, theme::kPanelEdge);
    setColour(juce::Slider::textBoxBackgroundColourId, theme::kInk);
    setColour(juce::Slider::textBoxHighlightColourId, theme::kTrack);
    setColour(juce::ComboBox::backgroundColourId, theme::kKnobBody);
    setColour(juce::ComboBox::textColourId, theme::kText);
    setColour(juce::ComboBox::arrowColourId, theme::kApricot);
    setColour(juce::PopupMenu::backgroundColourId, theme::kPanel);
    setColour(juce::PopupMenu::textColourId, theme::kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::kTrack);
    setColour(juce::PopupMenu::highlightedTextColourId, theme::kText);
    setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    setColour(juce::TextButton::textColourOnId, theme::kBackground);
}

void EdmLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPos,
    float startAngle,
    float endAngle,
    juce::Slider& slider) {
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float opacity = slider.isEnabled() ? 1.0f : theme::kInactiveOpacity;
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(1.5f);
    const float radius = std::min(30.0f, 0.5f * std::min(bounds.getWidth(), bounds.getHeight()));
    if (radius < 5.0f) return;
    const auto centre = bounds.getCentre();
    const float arcRadius = radius - kArcThickness;
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    const bool isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float originAngle = isBipolar
        ? startAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (endAngle - startAngle)
        : startAngle;
    const auto stroke = juce::PathStrokeType(kArcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(theme::kTrack.withMultipliedAlpha(opacity));
    g.strokePath(track, stroke);

    if (std::abs(valueAngle - originAngle) > 1e-3f) {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, originAngle, valueAngle, true);
        g.setColour(accent.withMultipliedAlpha(opacity));
        g.strokePath(value, stroke);
    }

    const float bodyRadius = arcRadius - kArcThickness - 1.5f;
    const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
    g.setColour(theme::kKnobBody.brighter(0.10f).withMultipliedAlpha(opacity));
    g.fillEllipse(body);
    const auto tip = centre.getPointOnCircumference(bodyRadius - 2.0f, valueAngle);
    const auto tail = centre.getPointOnCircumference(bodyRadius * 0.34f, valueAngle);
    g.setColour(theme::kText.withMultipliedAlpha(opacity));
    g.drawLine({ tail, tip }, 2.2f);
    if (slider.hasKeyboardFocus(true)) {
        g.setColour(theme::kLilac.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 1.2f);
    }
}

void EdmLookAndFeel::drawComboBox(
    juce::Graphics& g,
    int width,
    int height,
    bool,
    int,
    int,
    int,
    int,
    juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    const float opacity = box.isEnabled() ? 1.0f : theme::kInactiveOpacity;
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId).withMultipliedAlpha(opacity));
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(theme::kPanelEdge.withMultipliedAlpha(opacity));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.0f);

    const auto arrowZone = juce::Rectangle<float>(static_cast<float>(width) - 20.0f, 0.0f, 16.0f, static_cast<float>(height));
    juce::Path arrow;
    arrow.startNewSubPath(arrowZone.getX() + 3.0f, arrowZone.getCentreY() - 2.0f);
    arrow.lineTo(arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    arrow.lineTo(arrowZone.getRight() - 3.0f, arrowZone.getCentreY() - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withMultipliedAlpha(opacity));
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (box.hasKeyboardFocus(true)) {
        g.setColour(theme::kLilac);
        g.drawRoundedRectangle(bounds.reduced(0.5f), kCornerRadius, 1.4f);
    }
}

void EdmLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(7, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

void EdmLookAndFeel::drawButtonBackground(
    juce::Graphics& g,
    juce::Button& button,
    const juce::Colour& backgroundColour,
    bool isHighlighted,
    bool isDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto onColour = button.findColour(juce::TextButton::buttonOnColourId);
    const bool isOn = button.getToggleState();
    const float opacity = button.isEnabled() ? 1.0f : theme::kInactiveOpacity;

    if (isOn) {
        g.setColour(onColour.withAlpha(0.25f * opacity));
        g.fillRoundedRectangle(bounds.expanded(2.0f), kCornerRadius + 1.0f);
    }

    const auto baseColour = isOn ? onColour : backgroundColour;
    const auto fill = isDown ? baseColour.darker(0.2f) : (isHighlighted ? baseColour.brighter(0.18f) : baseColour);

    g.setColour(fill.withMultipliedAlpha(opacity));
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour((isOn ? onColour.brighter(0.6f) : theme::kPanelEdge.brighter(0.1f)).withMultipliedAlpha(opacity));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.2f);
    // The light tells an off toggle from a push button.
    if (button.getClickingTogglesState()) {
        const auto light = juce::Rectangle<float>(kToggleLightSize, kToggleLightSize)
            .withCentre({ bounds.getX() + kToggleLightInset, bounds.getCentreY() });
        g.setColour((isOn ? theme::kText : theme::kInk).withMultipliedAlpha(opacity));
        g.fillEllipse(light);
        g.setColour((isOn ? theme::kText : theme::kTextDim).withMultipliedAlpha(opacity));
        g.drawEllipse(light, 1.0f);
    }
    if (button.hasKeyboardFocus(true)) {
        g.setColour(theme::kLilac);
        g.drawRoundedRectangle(bounds.reduced(0.5f), kCornerRadius, 1.4f);
    }
}

void EdmLookAndFeel::drawButtonText(
    juce::Graphics& g,
    juce::TextButton& button,
    bool,
    bool) {
    const auto colourId = button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId;
    const float opacity = button.isEnabled() ? 1.0f : theme::kInactiveOpacity;
    auto area = button.getLocalBounds().reduced(4, 2);
    if (button.getClickingTogglesState())
        area.removeFromLeft(18);
    g.setColour(button.findColour(colourId).withMultipliedAlpha(opacity));
    g.setFont(juce::FontOptions(kControlFontSize, juce::Font::bold));
    g.drawText(button.getButtonText(), area, juce::Justification::centred, true);
}

juce::Font EdmLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return juce::FontOptions(kControlFontSize);
}

juce::Font EdmLookAndFeel::getPopupMenuFont() {
    return juce::FontOptions(kControlFontSize);
}

juce::Label* EdmLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(juce::FontOptions(13.0f, juce::Font::bold));
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::backgroundColourId, slider.findColour(juce::Slider::textBoxBackgroundColourId));
    l->setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    return l;
}

} // namespace opensmacker::ui
