#include "EdmLookAndFeel.h"
#include "Theme.h"

namespace opensmacker::ui {

namespace {

constexpr float kArcThickness = 2.8f;
constexpr float kCornerRadius = 5.0f;
constexpr float kControlFontSize = 12.0f;

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
    setColour(juce::ComboBox::arrowColourId, theme::kAccentMagenta);
    setColour(juce::PopupMenu::backgroundColourId, theme::kPanel);
    setColour(juce::PopupMenu::textColourId, theme::kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::kTrack);
    setColour(juce::PopupMenu::highlightedTextColourId, theme::kAccentCyan);
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
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.5f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float arcRadius = radius - kArcThickness;
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    const bool isBipolar = juce::approximatelyEqual(slider.getMinimum(), -slider.getMaximum());
    const float originAngle = isBipolar ? (startAngle + endAngle) / 2.0f : startAngle;
    const auto stroke = juce::PathStrokeType(kArcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    constexpr int kNumTicks = 11;
    for (int i = 0; i < kNumTicks; ++i) {
        const float tFrac = static_cast<float>(i) / static_cast<float>(kNumTicks - 1);
        const float tickAngle = startAngle + tFrac * (endAngle - startAngle);
        const auto tickOuter = centre.getPointOnCircumference(radius, tickAngle);
        const auto tickInner = centre.getPointOnCircumference(radius - 1.8f, tickAngle);
        g.setColour(theme::kPanelEdge.brighter(0.2f));
        g.drawLine({ tickInner, tickOuter }, 1.0f);
    }

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(theme::kTrack);
    g.strokePath(track, stroke);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, originAngle, valueAngle, true);
    const auto glowStroke = juce::PathStrokeType(kArcThickness + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(accent.withAlpha(0.28f));
    g.strokePath(value, glowStroke);
    g.setColour(accent);
    g.strokePath(value, stroke);

    const float bodyRadius = arcRadius - kArcThickness - 1.5f;
    const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(body.translated(0.0f, 2.5f));

    g.setGradientFill(juce::ColourGradient(
        theme::kKnobBody.brighter(0.22f), centre.x, body.getY(),
        theme::kInk, centre.x, body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(theme::kPanelEdge.brighter(0.3f));
    g.drawEllipse(body, 1.0f);

    const float capRadius = bodyRadius * 0.45f;
    const auto cap = juce::Rectangle<float>(capRadius * 2.0f, capRadius * 2.0f).withCentre(centre);
    g.setGradientFill(juce::ColourGradient(
        theme::kSteel.brighter(0.1f), centre.x, cap.getY(),
        theme::kInk, centre.x, cap.getBottom(), false));
    g.fillEllipse(cap);
    g.setColour(theme::kPanelEdge.brighter(0.2f));
    g.drawEllipse(cap, 0.8f);

    const auto tip = centre.getPointOnCircumference(bodyRadius - 2.0f, valueAngle);
    const auto tail = centre.getPointOnCircumference(bodyRadius * 0.48f, valueAngle);
    g.setColour(accent.withAlpha(0.45f));
    g.drawLine({ tail, tip }, 3.5f);
    g.setColour(accent.brighter(0.7f));
    g.drawLine({ tail, tip }, 2.0f);
    g.setColour(juce::Colours::white);
    g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f).withCentre(tip));
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
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.0f);

    const auto arrowZone = juce::Rectangle<float>(static_cast<float>(width) - 20.0f, 0.0f, 16.0f, static_cast<float>(height));
    juce::Path arrow;
    arrow.startNewSubPath(arrowZone.getX() + 3.0f, arrowZone.getCentreY() - 2.0f);
    arrow.lineTo(arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    arrow.lineTo(arrowZone.getRight() - 3.0f, arrowZone.getCentreY() - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EdmLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(1, 1, box.getWidth() - 24, box.getHeight() - 2);
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

    if (isOn) {
        g.setColour(onColour.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds.expanded(2.0f), kCornerRadius + 1.0f);
    }

    const auto baseColour = isOn ? onColour : backgroundColour;
    const auto fill = isDown ? baseColour.darker(0.2f) : (isHighlighted ? baseColour.brighter(0.18f) : baseColour);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(isOn ? onColour.brighter(0.6f) : theme::kPanelEdge.brighter(0.1f));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.2f);
}

void EdmLookAndFeel::drawButtonText(
    juce::Graphics& g,
    juce::TextButton& button,
    bool,
    bool) {
    const auto colour = button.getToggleState()
        ? button.findColour(juce::TextButton::textColourOnId)
        : button.findColour(juce::TextButton::textColourOffId);
    g.setColour(colour);
    g.setFont(juce::FontOptions(kControlFontSize, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(4, 2),
               juce::Justification::centred, true);
}

juce::Font EdmLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return juce::FontOptions(kControlFontSize);
}

juce::Font EdmLookAndFeel::getPopupMenuFont() {
    return juce::FontOptions(kControlFontSize);
}

juce::Label* EdmLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(juce::FontOptions(11.0f, juce::Font::bold));
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::backgroundColourId, theme::kInk);
    l->setColour(juce::Label::outlineColourId, theme::kPanelEdge);
    return l;
}

} // namespace opensmacker::ui
