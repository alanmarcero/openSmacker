#include "Controls.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace opensmacker::ui {

void drawPadlockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent) {
    const auto box = bounds.reduced(0.5f);
    g.setColour(theme::kInk.withAlpha(0.88f));
    g.fillRoundedRectangle(box.expanded(1.5f), 3.0f);

    const float w = box.getWidth();
    const float h = box.getHeight();
    const float shackleW = w * 0.56f;
    const float shackleH = h * 0.42f;
    const float cx = box.getCentreX();

    juce::Path shackle;
    shackle.addArc(cx - shackleW * 0.5f, box.getY() + 0.5f, shackleW, shackleH * 1.4f,
                   -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.setColour(accent);
    g.strokePath(shackle, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto body = juce::Rectangle<float>(box.getX() + w * 0.14f, box.getY() + h * 0.42f, w * 0.72f, h * 0.54f);
    g.fillRoundedRectangle(body, 1.8f);

    g.setColour(theme::kInk);
    g.fillEllipse(cx - 1.1f, body.getCentreY() - 1.1f, 2.2f, 2.2f);
}

PadlockBadge::PadlockBadge(juce::Colour accent, std::function<void(const juce::MouseEvent&)> onAction)
    : accent_(accent), onAction_(std::move(onAction)) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip("Click to unlock parameter, or right-click for lock options.");
}

void PadlockBadge::paint(juce::Graphics& g) {
    drawPadlockIcon(g, getLocalBounds().toFloat(), accent_);
}

void PadlockBadge::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onAction_)
        onAction_(event);
}

void PadlockBadge::mouseUp(const juce::MouseEvent& event) {
    if (!event.mods.isPopupMenu() && event.mouseWasClicked() && onAction_)
        onAction_(event);
}

void PopupSlider::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onPopupMenu) {
        onPopupMenu(event);
        return;
    }
    juce::Slider::mouseDown(event);
}

void PopupComboBox::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onPopupMenu) {
        onPopupMenu(event);
        return;
    }
    juce::ComboBox::mouseDown(event);
}

void PopupComboBox::showPopup() {
    if (activeItemId_ <= 0 || getSelectedId() != 0) {
        juce::ComboBox::showPopup();
        return;
    }
    auto menu = *getRootMenu();
    juce::PopupMenu::MenuItemIterator it(menu, true);
    while (it.next())
        it.getItem().setTicked(it.getItem().itemID == activeItemId_);
    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(this)
                       .withItemThatMustBeVisible(activeItemId_)
                       .withMinimumWidth(getWidth())
                       .withMaximumNumColumns(1)
                       .withStandardItemHeight(juce::jlimit(12, 24, getHeight()));
    menu.showMenuAsync(options, [safe = juce::Component::SafePointer<PopupComboBox>(this)](int result) {
        if (safe == nullptr || result <= 0)
            return;
        safe->setSelectedId(result, juce::sendNotificationSync);
    });
}

void PopupTextButton::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onPopupMenu) {
        onPopupMenu(event);
        return;
    }
    juce::TextButton::mouseDown(event);
}

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent, bool stepper) {
    slider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider_.setColour(juce::Slider::textBoxBackgroundColourId, stepper ? juce::Colours::transparentBlack : theme::kInk);
    slider_.setColour(juce::Slider::textBoxTextColourId, theme::kText);
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider_.setSliderStyle(stepper ? juce::Slider::IncDecButtons : juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 17);
    slider_.setWantsKeyboardFocus(true);
    addAndMakeVisible(slider_);

    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kCaption);
    title_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(title_);

    bind(state, parameterId);
}

void Knob::paint(juce::Graphics& g) {
    if (slider_.getSliderStyle() != juce::Slider::IncDecButtons || stepperFrame_.isEmpty())
        return;
    const auto frame = stepperFrame_.toFloat().reduced(0.5f);
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(frame, 5.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(frame, 5.0f, 1.2f);
}

void Knob::setOnPopupMenu(PopupCallback callback) {
    onPopupMenu_ = std::move(callback);
    slider_.onPopupMenu = onPopupMenu_;
}

void Knob::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onPopupMenu_)
        onPopupMenu_(event);
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    attachment_ = std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_);
    slider_.setName(title_.getText());
    slider_.setTitle(state.getParameter(parameterId)->getName(64));
}

void Knob::resized() {
    auto area = getLocalBounds();
    if (slider_.getSliderStyle() == juce::Slider::IncDecButtons) {
        area = area.withSizeKeepingCentre(std::min(area.getWidth(), 168), std::min(area.getHeight(), 64));
        title_.setBounds(area.removeFromTop(19));
        stepperFrame_ = area.reduced(2, 2);
        auto inner = stepperFrame_.reduced(2, 2);
        const int textW = std::clamp(inner.getWidth() * 42 / 100, 26, 56);
        slider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, textW, inner.getHeight());
        slider_.setBounds(inner);
        return;
    }
    stepperFrame_ = {};
    area = area.withSizeKeepingCentre(area.getWidth(), std::min(120, area.getHeight()));
    const int labelHeight = area.getHeight() < 86 ? 15 : 18;
    const int boxHeight = area.getHeight() < 86 ? 15 : 17;
    title_.setBounds(area.removeFromTop(labelHeight));
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::min(88, area.getWidth()), boxHeight);
    slider_.setBounds(area);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent) {
    box_.setColour(juce::ComboBox::arrowColourId, accent);
    box_.setColour(juce::ComboBox::backgroundColourId, theme::kInk);
    box_.setColour(juce::ComboBox::outlineColourId, theme::kPanelEdge);
    addAndMakeVisible(box_);

    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kCaption);
    title_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(title_);

    bind(state, parameterId);
}

void Choice::setOnPopupMenu(PopupCallback callback) {
    onPopupMenu_ = std::move(callback);
    box_.onPopupMenu = onPopupMenu_;
}

void Choice::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onPopupMenu_)
        onPopupMenu_(event);
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    box_.clear(juce::dontSendNotification);
    auto* parameter = state.getParameter(parameterId);
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(parameter)) {
        box_.addItemList(param->choices, 1);
    } else if (dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr) {
        box_.addItemList({ parameter->getText(0.0f, 0), parameter->getText(1.0f, 0) }, 1);
    }
    box_.setName(title_.getText());
    box_.setTitle(parameter->getName(64));
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
}

void Choice::resized() {
    auto area = getLocalBounds();
    area = area.withSizeKeepingCentre(std::min(180, area.getWidth()), std::min(64, area.getHeight()));
    title_.setBounds(area.removeFromTop(19));
    box_.setBounds(area.reduced(3, 3));
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent)
    : attachment_(state, parameterId, button_) {
    button_.setButtonText(text);
    button_.setTitle(state.getParameter(parameterId)->getName(64));
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    button_.setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    button_.setColour(juce::TextButton::textColourOnId, theme::kInk);
    addAndMakeVisible(button_);
}

void Toggle::setOnPopupMenu(PopupCallback callback) {
    onPopupMenu_ = std::move(callback);
    button_.onPopupMenu = onPopupMenu_;
}

void Toggle::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() && onPopupMenu_)
        onPopupMenu_(event);
}

void Toggle::resized() {
    auto area = getLocalBounds().reduced(4);
    const int width = std::min(160, area.getWidth());
    const int height = std::min(40, area.getHeight());
    button_.setBounds(area.withSizeKeepingCentre(width, height));
}

bool DualEqKnob::GainSlider::hitTest(int x, int y) {
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float radius = static_cast<float>(std::min(getWidth(), getHeight())) * 0.5f;
    return juce::Point<float>(static_cast<float>(x), static_cast<float>(y)).getDistanceFrom(centre) <= radius;
}

DualEqKnob::DualEqKnob(Apvts& state, const juce::String& freqId, const juce::String& dbId,
                       const juce::String& title, juce::Colour dbAccent, juce::Colour freqAccent)
    : state_(state), freqId_(freqId), dbId_(dbId), dbAccent_(dbAccent), freqAccent_(freqAccent) {
    title_.setText(title, juce::dontSendNotification);
    title_.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centred);
    title_.setColour(juce::Label::textColourId, theme::kCaption);
    title_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(title_);

    dbSlider_.onPopupMenu = [this](const juce::MouseEvent& event) {
        if (onPopupMenu_)
            onPopupMenu_(dbId_, event);
    };

    dbSlider_.setName(title + " Gain");
    dbSlider_.setComponentID(dbId);
    dbSlider_.setTitle(state.getParameter(dbId)->getName(64));
    dbSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dbSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    dbSlider_.setColour(juce::Slider::rotarySliderFillColourId, dbAccent_);
    if (auto* dbParam = state.getParameter(dbId)) {
        dbSlider_.setDoubleClickReturnValue(true, dbParam->convertFrom0to1(dbParam->getDefaultValue()));
    }
    dbAttachment_ = std::make_unique<Apvts::SliderAttachment>(state, dbId, dbSlider_);
    dbSlider_.onValueChange = [this] {
        dbReadout_.setText(dbSlider_.getTextFromValue(dbSlider_.getValue()), juce::dontSendNotification);
    };
    addAndMakeVisible(dbSlider_);

    dbReadout_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    dbReadout_.setJustificationType(juce::Justification::centred);
    dbReadout_.setColour(juce::Label::textColourId, theme::kText);
    dbReadout_.setColour(juce::Label::backgroundColourId, theme::kInk);
    dbReadout_.setEditable(false, true, false);
    dbReadout_.onTextChange = [this] {
        auto* dbParam = state_.getParameter(dbId_);
        if (dbParam == nullptr) return;
        const float value = dbParam->convertFrom0to1(dbParam->getValueForText(dbReadout_.getText()));
        dbSlider_.setValue(value, juce::sendNotificationSync);
    };
    addAndMakeVisible(dbReadout_);

    freqReadout_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    freqReadout_.setJustificationType(juce::Justification::centred);
    freqReadout_.setColour(juce::Label::textColourId, theme::kText);
    freqReadout_.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    freqReadout_.setEditable(false, true, false);
    freqReadout_.addMouseListener(this, false);
    freqReadout_.onTextChange = [this] {
        auto* freqParam = state_.getParameter(freqId_);
        if (freqParam == nullptr || freqAttachment_ == nullptr) return;
        const float nextHz = freqParam->convertFrom0to1(freqParam->getValueForText(freqReadout_.getText()));
        freqHz_ = nextHz;
        freqAttachment_->setValueAsCompleteGesture(nextHz);
        updateFreqReadout();
        repaint();
    };
    addAndMakeVisible(freqReadout_);

    setWantsKeyboardFocus(true);

    if (auto* freqParam = state.getParameter(freqId)) {
        freqHz_ = freqParam->convertFrom0to1(freqParam->getValue());
        freqAttachment_ = std::make_unique<juce::ParameterAttachment>(*freqParam, [this](float value) {
            freqHz_ = value;
            updateFreqReadout();
            repaint();
        });
        freqAttachment_->sendInitialUpdate();
    }
    dbSlider_.onValueChange();
}

void DualEqKnob::updateFreqReadout() {
    auto* freqParam = state_.getParameter(freqId_);
    if (freqParam == nullptr) return;
    freqReadout_.setText(freqParam->getText(freqParam->convertTo0to1(freqHz_), 0), juce::dontSendNotification);
}

void DualEqKnob::stepSemitones(float deltaSemitones) {
    if (freqAttachment_ == nullptr) return;
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr) return;
    constexpr float kSemitoneStep = 1.0f / 24.0f;
    const float nextNorm = std::clamp(param->convertTo0to1(freqHz_) + deltaSemitones * kSemitoneStep, 0.0f, 1.0f);
    const float nextHz = param->convertFrom0to1(nextNorm);
    freqHz_ = nextHz;
    freqAttachment_->setValueAsCompleteGesture(nextHz);
    updateFreqReadout();
    repaint();
}

juce::Rectangle<float> DualEqKnob::dialBounds() const {
    const auto middle = getLocalBounds().withTrimmedTop(15);
    const float size = static_cast<float>(std::min(58, std::min(middle.getWidth() - 56, middle.getHeight())));
    return juce::Rectangle<float>(static_cast<float>(middle.getX()) + 2.0f,
                                  middle.toFloat().getCentreY() - size * 0.5f,
                                  size, size);
}

juce::Rectangle<int> DualEqKnob::dbPillBounds() const {
    const auto middle = getLocalBounds().withTrimmedTop(15);
    const int left = juce::roundToInt(dialBounds().getRight()) + 3;
    const int width = std::max(0, middle.getRight() - left - 1);
    const int cy = middle.getCentreY();
    return { left, cy - 17, width, 16 };
}

juce::Rectangle<int> DualEqKnob::freqPillBounds() const {
    const auto middle = getLocalBounds().withTrimmedTop(15);
    const int left = juce::roundToInt(dialBounds().getRight()) + 3;
    const int width = std::max(0, middle.getRight() - left - 1);
    const int cy = middle.getCentreY();
    return { left, cy + 11, width, 16 };
}

bool DualEqKnob::hitsOuterRing(juce::Point<float> pos) const {
    const auto dial = dialBounds();
    const float dist = pos.getDistanceFrom(dial.getCentre());
    const float outerR = dial.getWidth() * 0.5f + 3.0f;
    const float innerR = dial.getWidth() * 0.28f;
    return dist >= innerR && dist <= outerR;
}

void DualEqKnob::paint(juce::Graphics& g) {
    const auto dial = dialBounds();
    const auto centre = dial.getCentre();
    const float outerR = dial.getWidth() * 0.5f - 2.0f;
    constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;

    auto* freqParam = state_.getParameter(freqId_);
    const float norm = freqParam != nullptr ? freqParam->convertTo0to1(freqHz_) : 0.5f;
    const float angle = startAngle + norm * (endAngle - startAngle);
    const auto ringColour = (hoveringFreq_ || draggingFreq_) ? freqAccent_.brighter(0.2f) : freqAccent_;

    juce::Path base;
    base.addCentredArc(centre.x, centre.y, outerR, outerR, 0.0f, startAngle, endAngle, true);
    g.setColour(theme::kPanelEdge);
    g.strokePath(base, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, outerR, outerR, 0.0f, startAngle, angle, true);
    g.setColour(ringColour);
    g.strokePath(value, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Mark root, +1 octave, and +2 octave positions on the outer note ring.
    g.setColour(theme::kTextDim.withAlpha(0.55f));
    for (const float markNorm : { 0.0f, 0.5f, 1.0f }) {
        const float markAngle = startAngle + markNorm * (endAngle - startAngle);
        const float innerTick = outerR - 3.0f;
        const float outerTick = outerR + 3.0f;
        g.drawLine(centre.x + innerTick * std::sin(markAngle), centre.y - innerTick * std::cos(markAngle),
                   centre.x + outerTick * std::sin(markAngle), centre.y - outerTick * std::cos(markAngle), 1.1f);
    }

    const float dotX = centre.x + outerR * std::sin(angle);
    const float dotY = centre.y - outerR * std::cos(angle);
    g.setColour(theme::kText);
    g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);

    const auto dbPill = dbPillBounds();
    const auto freqPill = freqPillBounds().toFloat();
    g.setColour(theme::kCaption);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("dB", dbPill.translated(0, -11).withHeight(11), juce::Justification::centredLeft, false);
    g.drawText("Note", freqPill.toNearestInt().translated(0, -11).withHeight(11), juce::Justification::centredLeft, false);
    g.setColour(theme::kInk.brighter(hoveringFreq_ || draggingFreq_ ? 0.1f : 0.0f));
    g.fillRoundedRectangle(freqPill, 3.5f);
    g.setColour(ringColour.withAlpha(hoveringFreq_ || draggingFreq_ ? 0.9f : 0.45f));
    g.drawRoundedRectangle(freqPill.reduced(0.5f), 3.5f, 1.0f);
    if (hasKeyboardFocus(false)) {
        g.setColour(theme::kLilac.withAlpha(0.8f));
        g.drawRoundedRectangle(dial.expanded(2.0f), 5.0f, 1.2f);
    }
}

void DualEqKnob::resized() {
    auto area = getLocalBounds();
    title_.setBounds(area.removeFromTop(15));
    const auto dial = dialBounds();
    dbSlider_.setBounds(juce::Rectangle<float>(44.0f, 38.0f).withCentre(dial.getCentre()).toNearestInt());
    dbReadout_.setBounds(dbPillBounds());
    freqReadout_.setBounds(freqPillBounds());
}

void DualEqKnob::mouseMove(const juce::MouseEvent& event) {
    const bool over = hitsOuterRing(event.position);
    if (over != hoveringFreq_) {
        hoveringFreq_ = over;
        repaint();
    }
}

void DualEqKnob::mouseExit(const juce::MouseEvent&) {
    if (!hoveringFreq_) return;
    hoveringFreq_ = false;
    repaint();
}

void DualEqKnob::mouseDown(const juce::MouseEvent& event) {
    if (event.eventComponent == &freqReadout_) {
        if (event.mods.isPopupMenu() && onPopupMenu_)
            onPopupMenu_(freqId_, event);
        return;
    }
    if (event.mods.isPopupMenu()) {
        if (onPopupMenu_)
            onPopupMenu_(hitsOuterRing(event.position) ? freqId_ : dbId_, event);
        return;
    }
    if (!hitsOuterRing(event.position)) return;
    grabKeyboardFocus();
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr || freqAttachment_ == nullptr) return;
    draggingFreq_ = true;
    dragNorm_ = param->convertTo0to1(freqHz_);
    lastDragY_ = event.position.y;
    freqAttachment_->beginGesture();
    repaint();
}

void DualEqKnob::mouseDrag(const juce::MouseEvent& event) {
    if (!draggingFreq_ || freqAttachment_ == nullptr) return;
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr) return;
    const float scale = event.mods.isShiftDown() ? 560.0f : 140.0f;
    dragNorm_ = std::clamp(dragNorm_ + (lastDragY_ - event.position.y) / scale, 0.0f, 1.0f);
    lastDragY_ = event.position.y;
    const float nextHz = param->convertFrom0to1(dragNorm_);
    freqHz_ = nextHz;
    freqAttachment_->setValueAsPartOfGesture(nextHz);
    updateFreqReadout();
    repaint();
}

void DualEqKnob::mouseUp(const juce::MouseEvent&) {
    if (!draggingFreq_ || freqAttachment_ == nullptr) return;
    draggingFreq_ = false;
    freqAttachment_->endGesture();
    repaint();
}

void DualEqKnob::mouseDoubleClick(const juce::MouseEvent& event) {
    if (event.eventComponent == &freqReadout_ || !hitsOuterRing(event.position) || freqAttachment_ == nullptr) return;
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr) return;
    const float defaultHz = param->convertFrom0to1(param->getDefaultValue());
    freqHz_ = defaultHz;
    freqAttachment_->setValueAsCompleteGesture(defaultHz);
    updateFreqReadout();
    repaint();
}

void DualEqKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (!hitsOuterRing(event.position) || wheel.deltaY == 0.0f) return;
    stepSemitones(wheel.deltaY > 0.0f ? 1.0f : -1.0f);
}

bool DualEqKnob::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey) {
        stepSemitones(1.0f);
        return true;
    }
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey) {
        stepSemitones(-1.0f);
        return true;
    }
    return false;
}

} // namespace opensmacker::ui
