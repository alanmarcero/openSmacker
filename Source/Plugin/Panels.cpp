#include "Panels.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>

namespace opensmacker::ui {

void showParameterLockMenu(OpenSmackerProcessor& processor, const juce::String& paramId,
                           juce::Point<int> screenPos, std::function<void()> onChanged) {
    auto* param = processor.parameters.getParameter(paramId);
    if (param == nullptr)
        return;
    const auto name = param->getName(64);
    const bool locked = processor.isParamLocked(paramId);

    juce::PopupMenu menu;
    menu.addItem(1, (locked ? "Unlock " : "Lock ") + name);
    if (processor.hasLockedParams()) {
        menu.addSeparator();
        menu.addItem(2, "Unlock all parameters");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ screenPos.x, screenPos.y, 1, 1 }),
                       [&processor, paramId, locked, changedCallback = std::move(onChanged)](int result) {
                           if (result == 1)
                               processor.setParamLocked(paramId, !locked);
                           else if (result == 2)
                               processor.unlockAllParams();
                           else
                               return;
                           if (changedCallback)
                               changedCallback();
                       });
}

namespace {
// Slider text boxes and stepper buttons keep their own tooltips.
void setTooltipDeep(juce::Component& component, const juce::String& text) {
    if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(&component))
        client->setTooltip(text);
    std::ranges::for_each(component.getChildren(), [&text](juce::Component* child) { setTooltipDeep(*child, text); });
}

// Each step sets its parameter from the next controller number or note the source reports.
class LearnControl : public juce::Component, public juce::Timer {
public:
    struct Step {
        juce::RangedAudioParameter& parameter;
        const char* prompt;
    };

    LearnControl(const std::atomic<int>& value, const std::atomic<uint32_t>& count, std::initializer_list<Step> steps,
                 const juce::String& title, const juce::String& tooltip)
        : value_(value), count_(count) {
        std::ranges::for_each(steps, [this](const Step& step) {
            targets_.push_back({ std::make_unique<juce::ParameterAttachment>(step.parameter, nullptr), step.prompt });
        });
        button_.setButtonText("Learn");
        button_.setTitle(title);
        button_.setTooltip(tooltip);
        button_.setColour(juce::TextButton::buttonOnColourId, theme::kIce);
        button_.setColour(juce::TextButton::textColourOnId, theme::kInk);
        button_.onClick = [this] {
            if (isTimerRunning()) {
                stop();
                return;
            }
            if (activeLearn_ != nullptr && activeLearn_ != this)
                activeLearn_->stop();
            activeLearn_ = this;
            nextTarget_ = 0;
            seenCount_ = count_.load();
            ticksLeft_ = kTimeoutTicks;
            button_.setToggleState(true, juce::dontSendNotification);
            button_.setButtonText(targets_.front().prompt);
            startTimerHz(kTicksPerSecond);
        };
        addAndMakeVisible(button_);
    }

    ~LearnControl() override {
        if (activeLearn_ == this)
            activeLearn_ = nullptr;
    }

    void visibilityChanged() override {
        if (!isVisible() && isTimerRunning())
            stop();
    }

    void resized() override {
        button_.setBounds(getLocalBounds().withSizeKeepingCentre(std::min(160, getWidth() - 8), 40));
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key != juce::KeyPress::escapeKey || !isTimerRunning())
            return false;
        stop();
        return true;
    }

    void timerCallback() override {
        for (const juce::Component* c = this; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent()) {
            if (!c->isVisible()) {
                stop();
                return;
            }
        }
        const auto count = count_.load();
        if (count == seenCount_) {
            if (--ticksLeft_ <= 0)
                stop();
            return;
        }
        seenCount_ = count;
        ticksLeft_ = kTimeoutTicks;
        targets_[nextTarget_].attachment->setValueAsCompleteGesture(static_cast<float>(value_.load()));
        if (++nextTarget_ == targets_.size()) {
            stop();
            return;
        }
        button_.setButtonText(targets_[nextTarget_].prompt);
    }

private:
    struct Target {
        std::unique_ptr<juce::ParameterAttachment> attachment;
        const char* prompt;
    };
    static constexpr int kTicksPerSecond = 20;
    static constexpr int kTimeoutTicks = 10 * kTicksPerSecond;

    void stop() {
        stopTimer();
        if (activeLearn_ == this)
            activeLearn_ = nullptr;
        button_.setToggleState(false, juce::dontSendNotification);
        button_.setButtonText("Learn");
    }

    inline static LearnControl* activeLearn_ = nullptr;

    const std::atomic<int>& value_;
    const std::atomic<uint32_t>& count_;
    std::vector<Target> targets_;
    juce::TextButton button_;
    uint32_t seenCount_ = 0;
    size_t nextTarget_ = 0;
    int ticksLeft_ = 0;
};
} // namespace

SmackerStagePanel::SmackerStagePanel(OpenSmackerProcessor& processor)
    : processor_(processor), freeze_(*processor.parameters.getParameter(params::kLockIn), nullptr) {
    unfreeze_.setTitle("Unfreeze hit");
    unfreeze_.setTooltip("Turn off Freeze hit so the synth plays live again.");
    unfreeze_.onClick = [this] { freeze_.setValueAsCompleteGesture(0.0f); };
    unfreeze_.onPopupMenu = [this](const juce::MouseEvent& event) {
        showParameterLockMenu(processor_, params::kLockIn, event.getScreenPosition(), [this] { syncLockBadge(); });
    };
    addChildComponent(unfreeze_);
    unfreezeLockBadge_ = std::make_unique<PadlockBadge>(theme::kApricot, [this](const juce::MouseEvent& event) {
        if (event.mods.isPopupMenu()) {
            showParameterLockMenu(processor_, params::kLockIn, event.getScreenPosition(), [this] { syncLockBadge(); });
            return;
        }
        processor_.setParamLocked(params::kLockIn, false);
        syncLockBadge();
    });
    addChildComponent(*unfreezeLockBadge_);
    startTimerHz(12);
}

void SmackerStagePanel::timerCallback() {
    unfreeze_.setVisible(processor_.frozenHitPlays());
    syncLockBadge();
    if (isShowing()) repaint();
}

void SmackerStagePanel::resized() {
    unfreeze_.setBounds(getLocalBounds().removeFromRight(136).reduced(16, 0).withTrimmedTop(36).withHeight(36));
    syncLockBadge();
}

void SmackerStagePanel::updateWaveform(const core::SmackSynthSettings& settings) {
    // A frozen hit changes only with the voice. The live hit changes with any setting.
    const bool frozen = processor_.frozenHitPlays();
    const bool sameVoice = lastSettings_ && lastSettings_->voiceMode == settings.voiceMode;
    const bool sameSettings = lastSettings_ == settings;
    if (frozen == showsFrozenHit_ && (frozen ? sameVoice : sameSettings)) return;
    std::vector<float> left, right;
    double rate = 24000.0;
    if (frozen && !processor_.copyFrozenHit(settings.voiceMode, left, right, rate)) return;
    if (!frozen) {
        auto dry = settings;
        dry.fullAdsrEnabled = false;
        dry.reverbMix = 0;
        core::SmackSynth synth;
        synth.prepare(rate);
        (void) synth.renderPrefix(left, right, dry, static_cast<int>(rate) / 2);
    }
    showsFrozenHit_ = frozen;
    lastSettings_ = settings;
    previewMs_ = juce::roundToInt(1000.0 * static_cast<double>(left.size()) / rate);
    waveform_.fill(0);
    for (size_t i = 0; i < left.size(); ++i) {
        auto& peak = waveform_[i * waveform_.size() / left.size()];
        peak = std::max({ peak, std::abs(left[i]), std::abs(right[i]) });
    }
}

void SmackerStagePanel::paint(juce::Graphics& g) {
    const auto settings = processor_.currentSettings().synth;
    updateWaveform(settings);
    auto area = getLocalBounds();
    g.setColour(theme::kKnobBody);
    g.fillRoundedRectangle(area.toFloat(), 8);
    auto name = area.removeFromLeft(194).reduced(16, 6);
    const auto* voice = processor_.parameters.getParameter(params::kVoiceMode);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(13.5f));
    g.drawText(voice->getCurrentValueAsText(), name.removeFromTop(18), juce::Justification::centredLeft);
    const auto note = juce::String(core::kNoteNames[static_cast<size_t>(settings.note)].data()) + juce::String(settings.octave);
    g.setColour(theme::kText);
    const bool ultraCompact = getHeight() < 110;
    const bool compact = getHeight() < 140;
    g.setFont(juce::FontOptions(ultraCompact ? 30.0f : (compact ? 41.0f : 54.0f), juce::Font::bold));
    g.drawText(note, name.removeFromTop(ultraCompact ? 32 : (compact ? 44 : 61)), juce::Justification::centredLeft);
    g.setColour(theme::kApricot);
    g.setFont(juce::FontOptions(14.5f));
    g.drawText(juce::String(core::noteToHz(settings.note, settings.octave, settings.fineCents), 1) + " Hz",
               name, juce::Justification::topLeft);
    auto plot = area.reduced(16, ultraCompact ? 6 : 11);
    if (showsFrozenHit_) {
        auto badge = plot.removeFromRight(104);
        g.setColour(theme::kApricot);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText("Hit frozen", badge.removeFromTop(22), juce::Justification::centred);
        plot.removeFromRight(8);
    }
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(ultraCompact ? 14.5f : 17.0f, juce::Font::bold));
    g.drawText(showsFrozenHit_ ? "Frozen hit" : "Dry one-shot waveform", plot.removeFromTop(ultraCompact ? 18 : 23), juce::Justification::centredLeft);
    auto axis = plot.removeFromBottom(ultraCompact ? 16 : 21);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("0 ms", axis, juce::Justification::bottomLeft);
    g.drawText(juce::String(previewMs_) + " ms", axis, juce::Justification::bottomRight);
    const auto bounds = plot.toFloat().reduced(1, 2);
    const float middle = bounds.getCentreY();
    for (int i = 0; i <= 8; ++i) {
        const auto x = bounds.getX() + bounds.getWidth() * static_cast<float>(i) / 8;
        g.setColour(theme::kPanelEdge);
        g.drawVerticalLine(juce::roundToInt(x), bounds.getY(), bounds.getBottom());
    }
    const float scale = bounds.getHeight() * 0.46f / std::max(0.05f, *std::max_element(waveform_.begin(), waveform_.end()));
    juce::Path wave;
    wave.startNewSubPath(bounds.getX(), middle);
    for (size_t i = 0; i < waveform_.size(); ++i)
        wave.lineTo(bounds.getX() + bounds.getWidth() * float(i) / float(waveform_.size() - 1), middle - waveform_[i] * scale);
    for (size_t i = waveform_.size(); i-- > 0;)
        wave.lineTo(bounds.getX() + bounds.getWidth() * float(i) / float(waveform_.size() - 1), middle + waveform_[i] * scale);
    wave.closeSubPath();
    g.setColour(theme::kApricot);
    g.fillPath(wave);
    g.drawHorizontalLine(juce::roundToInt(middle), bounds.getX(), bounds.getRight());
}

void SmackerStagePanel::syncLockBadge() {
    if (unfreezeLockBadge_ == nullptr)
        return;
    const bool show = unfreeze_.isVisible() && processor_.isParamLocked(params::kLockIn);
    unfreezeLockBadge_->setVisible(show);
    if (show) {
        const auto b = unfreeze_.getBounds();
        unfreezeLockBadge_->setBounds(b.getRight() - 14, b.getY() + 2, 12, 13);
        unfreezeLockBadge_->toFront(false);
    }
}

juce::String inactiveReason(const juce::AudioProcessorValueTreeState& state, const juce::String& id, bool frozenHitPlays) {
    using namespace params;
    const auto isOn = [&state](const char* flag) { return state.getRawParameterValue(flag)->load() > 0.5f; };
    const auto isOneOf = [&id](std::initializer_list<const char*> ids) {
        return std::ranges::any_of(ids, [&id](const char* candidate) { return id == candidate; });
    };
    if (frozenHitPlays) {
        // The frozen hit replaces each setting a preset makes, except the ones playback applies.
        const auto& presetValues = presets::factoryPresets().front().values;
        const bool presetSets = std::ranges::any_of(presetValues, [&id](const auto& value) { return value.first == id; });
        const bool appliedOnPlayback = isOneOf({ kVoiceMode, kAmpEnabled, kLockPitch, kLockIn, kReverseMode, kReverseMix, kOutputGain });
        const bool midiModulation = isOneOf({ kMidiMode, kBodyVelocityDepth, kWireVelocityDepth, kTransientVelocityDepth,
                                              kBodyAftertouchDepth, kWireAftertouchDepth, kTransientAftertouchDepth,
                                              kMidiCcBody, kMidiCcWire, kMidiCcTransient });
        if (midiModulation || (presetSets && !appliedOnPlayback))
            return "Freeze hit is on, so the frozen hit plays. Turn it off to hear this control.";
    }
    if (id == kLockIn && isOn(kAmpEnabled))
        return "Hold notes play the live voice. Choose One-shot under MIDI gate to use it.";
    if (isOneOf({ kPreSpread, kPreDecay, kPreJitter, kFlamSpread, kPreLevel }) && state.getRawParameterValue(kPreBursts)->load() < 1.5f)
        return "Set Hits to 2 or more to use it.";
    if (isOneOf({ kSnareZoneLow, kSnareZoneHigh, kTomZoneLow, kTomZoneHigh, kBellZoneLow, kBellZoneHigh, kRimZoneLow, kRimZoneHigh })
        && !isOn(kKeyZonesEnabled))
        return "Turn on Key zones to use it.";
    if (id == kVoiceMode && isOn(kKeyZonesEnabled))
        return "Key zones pick the voice for MIDI notes, so only Audition hit uses it. Turn off Key zones to use it.";
    if (isOneOf({ kRollSubdivision, kRollProbability, kRollVelocity, kRollAccentVelocity, kRollAccentEvery, kRollNote })
        && !isOn(kRollEnabled))
        return "Turn on Roll to use it.";
    if (isOneOf({ kLimiterCeiling, kLimiterRelease }) && !isOn(kLimiterEnabled))
        return "Turn on Limiter to use it.";
    if (id == kReverseMix && state.getRawParameterValue(kReverseMode)->load() < 0.5f)
        return "Choose a Reverse sync mode to use it.";
    if (isOneOf({ kReverbGate, kReverbSize, kReverbDecay, kReverbDamp }) && state.getRawParameterValue(kReverbMix)->load() <= 0.001f)
        return "Raise Room mix above 0% to use it.";
    if (isOneOf({ kDistType, kDistDrive, kDistTone }) && state.getRawParameterValue(kDistMix)->load() <= 0.001f)
        return "Raise Mix above 0% to use it.";
    if (isOneOf({ kCompThresh, kCompRatio, kCompAttack, kCompRelease, kCompMakeup }) && state.getRawParameterValue(kCompMix)->load() <= 0.001f)
        return "Raise Comp mix above 0% to use it.";
    return {};
}

HitMonitor::HitMonitor(OpenSmackerProcessor& processor) : processor_(processor) {
    setTooltip("Flashes on each hit, brighter for harder hits. Shows the last note and the voice it played.");
    startTimerHz(30);
}

void HitMonitor::timerCallback() {
    const auto count = processor_.displayTriggerCount.load();
    const float velocity = processor_.displayVelocity.load();
    const float faded = glow_ < 0.02f ? 0.0f : glow_ * 0.82f;
    const float glow = count != lastTriggerCount_ ? velocity : faded;
    lastTriggerCount_ = count;
    const int note = processor_.displayLastNote.load();
    const int voice = processor_.displayLastVoice.load();
    if (juce::exactlyEqual(glow, glow_) && note == shownNote_ && voice == shownVoice_)
        return;
    glow_ = glow;
    shownNote_ = note;
    shownVoice_ = voice;
    repaint();
}

juce::String HitMonitor::readout() const {
    if (shownNote_ < 0)
        return {};
    const auto* voiceParameter = processor_.parameters.getParameter(params::kVoiceMode);
    auto source = juce::MidiMessage::getMidiNoteName(shownNote_, true, true, 4);
    if (shownNote_ == OpenSmackerProcessor::kAuditionNote)
        source = "Audition";
    auto target = juce::String("no zone");
    if (shownVoice_ >= 0)
        target = voiceParameter->getText(voiceParameter->convertTo0to1(static_cast<float>(shownVoice_)), 0);
    return source + " -> " + target;
}

void HitMonitor::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    const auto lamp = area.removeFromLeft(getHeight()).toFloat().reduced(9.0f);
    g.setColour(theme::kPanelEdge);
    g.fillEllipse(lamp);
    g.setColour(theme::kApricot.withAlpha(glow_));
    g.fillEllipse(lamp);
    area.removeFromLeft(4);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Last hit", area.removeFromLeft(58), juce::Justification::centredLeft);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(readout(), area, juce::Justification::centredLeft, true);
}

PeakMeter::PeakMeter(std::atomic<float>& peak, const std::atomic<float>& ceilingDb,
                     const std::atomic<float>* limiterEnabled, const std::atomic<float>* outputGainDb)
    : peak_(peak), ceilingDb_(ceilingDb), limiterEnabled_(limiterEnabled), outputGainDb_(outputGainDb) {
    setTitle("Peak meter");
    setTooltip("Output peak in dBFS. The line holds the highest peak for 1 s. The top light stays on after a peak above 0 dBFS; click to clear it.");
    shownCeiling_ = limiterEnabled_ == nullptr || limiterEnabled_->load() > 0.5f;
    shownCeilingDb_ = ceilingDb_.load() + (outputGainDb_ != nullptr ? outputGainDb_->load() : 0.0f);
    startTimerHz(30);
}

void PeakMeter::timerCallback() {
    const float peak = peak_.exchange(0.0f);
    const float peakDb = juce::Decibels::gainToDecibels(peak, kFloorDb);
    const float levelDb = std::max({ peakDb, levelDb_ - 1.0f, kFloorDb });
    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool newHold = peakDb >= holdDb_ || now - holdStartMs_ > 1000.0;
    if (newHold)
        holdStartMs_ = now;
    const float holdDb = newHold ? peakDb : holdDb_;
    const bool clipped = clipped_ || peak > 1.0f;
    const bool showCeiling = limiterEnabled_ == nullptr || limiterEnabled_->load() > 0.5f;
    const float ceilingDb = ceilingDb_.load() + (outputGainDb_ != nullptr ? outputGainDb_->load() : 0.0f);
    if (juce::exactlyEqual(levelDb, levelDb_) && juce::exactlyEqual(holdDb, holdDb_) && clipped == clipped_
        && showCeiling == shownCeiling_ && (!showCeiling || juce::exactlyEqual(ceilingDb, shownCeilingDb_)))
        return;
    if (clipped != clipped_)
        setMouseCursor(clipped ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    levelDb_ = levelDb;
    holdDb_ = holdDb;
    clipped_ = clipped;
    shownCeiling_ = showCeiling;
    shownCeilingDb_ = ceilingDb;
    repaint();
}

void PeakMeter::mouseDown(const juce::MouseEvent&) {
    clipped_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void PeakMeter::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Peak", area.removeFromTop(18), juce::Justification::centred);
    area = area.withSizeKeepingCentre(std::min(16, area.getWidth()), area.getHeight()).reduced(0, 4);
    const auto clipLight = area.removeFromTop(8).toFloat();
    area.removeFromTop(3);
    const auto bar = area.toFloat();
    const auto yFor = [&bar](float db) { return juce::jmap(juce::jlimit(kFloorDb, 6.0f, db), kFloorDb, 6.0f, bar.getBottom(), bar.getY()); };
    g.setColour(clipped_ ? theme::kApricot : theme::kPanelEdge);
    g.fillRoundedRectangle(clipLight, 2.0f);
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(bar, 2.0f);
    g.setColour(theme::kLilac);
    g.fillRect(bar.withTop(yFor(levelDb_)));
    g.setColour(theme::kText);
    g.fillRect(bar.getX(), yFor(holdDb_) - 1.0f, bar.getWidth(), 2.0f);
    if (shownCeiling_) {
        g.setColour(theme::kApricot);
        g.fillRect(bar.getX() - 3.0f, yFor(shownCeilingDb_) - 0.5f, bar.getWidth() + 6.0f, 1.0f);
    }
}

ParameterCard::ParameterCard(OpenSmackerProcessor& processor, juce::String title,
                             presets::Section section, juce::Colour accent, bool lockable)
    : processor_(processor), title_(std::move(title)), section_(section), accent_(accent), lockable_(lockable) {
    lockButton_.setClickingTogglesState(false);
    lockButton_.setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    lockButton_.setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    lockButton_.setTitle("Lock " + title_);
    lockButton_.setTooltip("Keep the " + title_ + " values when you load a preset. The controls stay editable.");
    lockButton_.onClick = [this] {
        processor_.setSectionLocked(section_, !processor_.isSectionLocked(section_));
        syncLockButton();
    };
    addAndMakeVisible(lockButton_);
    lockButton_.setVisible(lockable_);
    syncLockButton();
    startTimerHz(10);
}

void ParameterCard::timerCallback() {
    if (!isShowing())
        return;
    syncLockButton();
    dimInactiveControls();
    uint64_t mask = 0;
    uint64_t bit = 1;
    for (const auto& activity : activity_) {
        if (processor_.isParamLocked(activity.id))
            mask |= bit;
        bit <<= 1;
        if (activity.secondaryId.isNotEmpty()) {
            if (processor_.isParamLocked(activity.secondaryId))
                mask |= bit;
            bit <<= 1;
        }
    }
    if (mask != lastLockMask_) {
        lastLockMask_ = mask;
        syncLockBadges();
    }
}

void ParameterCard::visibilityChanged() {
    if (isVisible()) {
        syncLockButton();
        dimInactiveControls();
        syncLockBadges();
    }
}

void ParameterCard::syncLockButton() {
    if (!lockable_)
        return;
    const bool locked = processor_.isSectionLocked(section_);
    const auto text = locked ? "Locked" : "Lock";
    const auto bg = locked ? accent_ : theme::kKnobBody;
    const auto fg = locked ? theme::kInk : theme::kTextDim;
    if (lockButton_.getButtonText() != text)
        lockButton_.setButtonText(text);
    if (lockButton_.findColour(juce::TextButton::buttonColourId) != bg)
        lockButton_.setColour(juce::TextButton::buttonColourId, bg);
    if (lockButton_.findColour(juce::TextButton::textColourOffId) != fg)
        lockButton_.setColour(juce::TextButton::textColourOffId, fg);
}

std::unique_ptr<PadlockBadge> ParameterCard::makeLockBadge(const juce::String& paramId, juce::Colour accent) {
    auto badge = std::make_unique<PadlockBadge>(accent, [this, paramId](const juce::MouseEvent& event) {
        if (event.mods.isPopupMenu()) {
            showParameterLockMenu(processor_, paramId, event.getScreenPosition(), [this] { syncLockBadges(); });
            return;
        }
        processor_.setParamLocked(paramId, false);
        syncLockBadges();
    });
    addChildComponent(*badge);
    return badge;
}

void ParameterCard::syncLockBadges() {
    for (auto& activity : activity_) {
        const bool visible = activity.control != nullptr && activity.control->isVisible();
        const auto b = visible ? activity.control->getBounds() : juce::Rectangle<int>{};
        if (activity.secondaryId.isNotEmpty()) {
            const bool showPrimary = visible && processor_.isParamLocked(activity.id);
            if (activity.primaryBadge != nullptr) {
                activity.primaryBadge->setVisible(showPrimary);
                if (showPrimary) {
                    activity.primaryBadge->setBounds(b.getX() + 1, b.getY() + 1, 12, 13);
                    activity.primaryBadge->toFront(false);
                }
            }
            const bool showSecondary = visible && processor_.isParamLocked(activity.secondaryId);
            if (activity.secondaryBadge != nullptr) {
                activity.secondaryBadge->setVisible(showSecondary);
                if (showSecondary) {
                    activity.secondaryBadge->setBounds(b.getX() + 1, b.getBottom() - 14, 12, 13);
                    activity.secondaryBadge->toFront(false);
                }
            }
            continue;
        }
        const bool showPrimary = visible && processor_.isParamLocked(activity.id);
        if (activity.primaryBadge != nullptr) {
            activity.primaryBadge->setVisible(showPrimary);
            if (showPrimary) {
                activity.primaryBadge->setBounds(b.getRight() - 14, b.getY() + 2, 12, 13);
                activity.primaryBadge->toFront(false);
            }
        }
    }
}

void ParameterCard::dimInactiveControls() {
    const bool frozenHitPlays = processor_.frozenHitPlays();
    std::ranges::for_each(activity_, [this, frozenHitPlays](Activity& activity) {
        const auto reason = inactiveReason(processor_.parameters, activity.id, frozenHitPlays);
        if (reason == activity.reason)
            return;
        activity.reason = reason;
        activity.control->setAlpha(reason.isEmpty() ? 1.0f : theme::kInactiveOpacity);
        setTooltipDeep(*activity.control, (activity.tip + " " + reason).trim());
    });
}

void ParameterCard::addParameter(const juce::String& id, const juce::String& title, const juce::String& tip) {
    std::unique_ptr<juce::Component> component;
    auto& state = processor_.parameters;
    auto popup = [this, id](const juce::MouseEvent& event) {
        showParameterLockMenu(processor_, id, event.getScreenPosition(), [this] { syncLockBadges(); });
    };
    if (auto* parameter = state.getParameter(id); dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr || id == params::kAmpEnabled) {
        auto choice = std::make_unique<Choice>(state, id, title, accent_);
        choice->setOnPopupMenu(popup);
        component = std::move(choice);
    } else if (dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr) {
        auto toggle = std::make_unique<Toggle>(state, id, title, accent_);
        toggle->setOnPopupMenu(popup);
        component = std::move(toggle);
    } else {
        const auto* integer = dynamic_cast<juce::AudioParameterInt*>(parameter);
        auto knob = std::make_unique<Knob>(state, id, title, accent_, integer != nullptr && integer->getRange().getLength() <= 16);
        knob->setOnPopupMenu(popup);
        component = std::move(knob);
    }
    setTooltipDeep(*component, tip);
    addAndMakeVisible(*component);
    controls_.push_back(std::move(component));
    activity_.push_back({ controls_.back().get(), id, tip, {}, {}, makeLockBadge(id, accent_), {} });
}

void ParameterCard::addParameters(std::initializer_list<ControlSpec> specs) {
    std::ranges::for_each(specs, [this](const ControlSpec& spec) { addParameter(spec.id, spec.label, spec.tip); });
}

void ParameterCard::addDualEq(const juce::String& freqId, const juce::String& dbId,
                              const juce::String& title, juce::Colour freqAccent, const juce::String& tip) {
    auto knob = std::make_unique<DualEqKnob>(processor_.parameters, freqId, dbId, title, accent_, freqAccent);
    knob->setOnPopupMenu([this](const juce::String& paramId, const juce::MouseEvent& event) {
        showParameterLockMenu(processor_, paramId, event.getScreenPosition(), [this] { syncLockBadges(); });
    });
    setTooltipDeep(*knob, tip);
    addAndMakeVisible(*knob);
    controls_.push_back(std::move(knob));
    activity_.push_back({ controls_.back().get(), dbId, tip, {}, freqId,
                          makeLockBadge(dbId, accent_), makeLockBadge(freqId, theme::kApricot) });
}

void ParameterCard::addControl(std::unique_ptr<juce::Component> control, const juce::String& id, const juce::String& tip) {
    addAndMakeVisible(*control);
    if (id.isNotEmpty())
        activity_.push_back({ control.get(), id, tip, {}, {}, makeLockBadge(id, accent_), {} });
    controls_.push_back(std::move(control));
}

void ParameterCard::setRows(std::vector<int> basic, std::vector<int> advanced) {
    basicRows_ = std::move(basic);
    advancedRows_ = advanced.empty() ? basicRows_ : std::move(advanced);
}

void ParameterCard::setWeights(std::vector<float> weights) {
    weights_ = std::move(weights);
}

void ParameterCard::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    resized();
}

void ParameterCard::paint(juce::Graphics& g) {
    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 7.0f);
    auto heading = getLocalBounds().removeFromTop(42).reduced(12, 2);
    g.setColour(accent_);
    g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(heading.getX()), static_cast<float>(heading.getY()) + 2.0f, 3.0f,
        static_cast<float>(heading.getHeight() - 4)), 1.5f);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText(title_, heading.withTrimmedLeft(10).withTrimmedRight(lockable_ ? 90 : 8), juce::Justification::centredLeft, true);
}

void ParameterCard::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(42).reduced(8, 2);
    lockButton_.setBounds(lockable_ ? header.removeFromRight(70) : juce::Rectangle<int>{});
    area = area.reduced(8, 4);
    const int gap = 4;
    int y = area.getY();
    size_t controlIndex = 0;
    const auto& rows = advancedMode_ ? advancedRows_ : basicRows_;
    if (rows.empty()) return;
    if (rows.size() == 1 && weights_.size() == controls_.size() && !weights_.empty()) {
        const float total = std::accumulate(weights_.begin(), weights_.end(), 0.0f);
        const int available = std::max(0, area.getWidth() - gap * static_cast<int>(controls_.size() - 1));
        int x = area.getX();
        std::ranges::for_each(std::views::iota(size_t{0}, controls_.size()), [&](size_t i) {
            const int w = juce::roundToInt(static_cast<float>(available) * (weights_[i] / total));
            controls_[i]->setVisible(true);
            controls_[i]->setBounds(x, area.getY(), w, area.getHeight());
            x += w + gap;
        });
        syncLockBadges();
        return;
    }
    const int rowCount = static_cast<int>(rows.size());
    const int rowHeight = std::max(1, (area.getHeight() - gap * (rowCount - 1)) / rowCount);
    for (int row = 0; row < rowCount; ++row) {
        const int columns = std::max(1, rows[static_cast<size_t>(row)]);
        const int cellWidth = std::max(1, (area.getWidth() - gap * (columns - 1)) / columns);
        for (int column = 0; column < columns && controlIndex < controls_.size(); ++column, ++controlIndex) {
            controls_[controlIndex]->setVisible(true);
            controls_[controlIndex]->setBounds(area.getX() + column * (cellWidth + gap), y, cellWidth, rowHeight);
        }
        y += rowHeight + gap;
    }
    for (; controlIndex < controls_.size(); ++controlIndex)
        controls_[controlIndex]->setVisible(false);
    syncLockBadges();
}

SmackSynthPanel::SmackSynthPanel(OpenSmackerProcessor& processor)
    : voiceCard_(processor, "Body & tuning", presets::Section::VoiceTuning, theme::kApricot),
      detailCard_(processor, "Body detail", presets::Section::BodyDetail, theme::kApricot),
      transientCard_(processor, "Transient strike", presets::Section::Transient, theme::kIce),
      envelopeCard_(processor, "Amplitude envelope", presets::Section::Envelope, theme::kLilac),
      outputCard_(processor, "Output", presets::Section::Output, theme::kLilac),
      preStrikeCard_(processor, "Flam", presets::Section::PreStrike, theme::kApricot),
      wiresCard_(processor, "Wires & air", presets::Section::WiresAir, theme::kIce) {
    using namespace opensmacker::params;
    voiceCard_.addParameters({
        { kVoiceMode, "Voice", "Drum model: snare, tom, bell or rim." },
        { kToneNote, "Root note", "Pitch of the hit. With MIDI pitch on Fixed, every note plays this pitch." },
        { kToneOctave, "Octave", "Octave of the root note." },
        { kToneDecay, "Body decay", "How long the drum body rings." },
        { kTonePitchEnv, "Pitch sweep", "Pitch offset at the start of the hit, in semitones. The pitch glides back to the root." },
        { kToneLevel, "Body level", "Volume of the drum body." },
    });
    voiceCard_.setRows({ 3, 3 });
    detailCard_.addParameters({
        { kFmAmount, "FM depth", "Amount of FM overtones in the body." },
        { kToneFreq, "Shell frequency", "Center of the shell resonance." },
        { kToneQ, "Shell Q", "Width of the shell resonance. Higher values ring at a narrower pitch." },
        { kShellSpread, "Mode spread", "Moves the upper drum modes apart or closer together." },
        { kTonePitchDecay, "Pitch decay", "Time the pitch sweep takes to reach the root." },
        { kToneFine, "Fine tune", "Fine pitch offset in cents." },
        { kLockPitch, "Lock pitch", "Keep the root note, octave, fine tune and shell frequency when you load a preset." },
        { kBodyHPF, "High-pass", "Removes body content below this frequency." },
        { kBodyLPF, "Low-pass", "Removes body content above this frequency." },
        { kBodyPan, "Pan", "Stereo position of the body." },
    });
    detailCard_.setRows({ 4, 3, 3 });
    envelopeCard_.addParameters({
        { kAmpEnabled, "MIDI gate", "One-shot plays the whole hit. Hold notes sustains until the matching note-off." },
        { kAmpAttack, "Attack", "Fade-in time at the start of the hit." },
        { kAmpDecay, "Decay", "Time to fall from the peak to the sustain level." },
        { kAmpSustain, "Sustain", "Level the hit holds after the decay." },
        { kAmpRelease, "Release", "Fade-out time at the end of the hit, or after note-off with Hold notes." },
    });
    envelopeCard_.setRows({ 5 });

    transientCard_.addParameters({
        { kClickType, "Strike type", "Strike sound: stick, mallet, rimshot or laser." },
        { kClickLevel, "Level", "Volume of the strike." },
        { kClickFreq, "Frequency", "Pitch of the strike click. Higher values sound brighter." },
        { kSubThump, "Sub thump", "Adds a low thump under the strike." },
        { kTransientHPF, "High-pass", "Removes strike content below this frequency." },
        { kTransientLPF, "Low-pass", "Removes strike content above this frequency." },
        { kTransientPan, "Pan", "Stereo position of the strike." },
    });
    transientCard_.setRows({ 1, 2 }, { 4, 3 });

    preStrikeCard_.addParameters({
        { kPreBursts, "Hits", "Number of strikes, counting the main hit. Set 2 or more for a flam or drag." },
        { kPreSpread, "Spacing", "Time between the flam strikes." },
        { kPreDecay, "Decay", "How fast each grace strike fades." },
        { kPreJitter, "Humanize", "Random timing of the grace strikes." },
        { kFlamSpread, "Stereo spread", "Pans the grace strikes left and right." },
        { kPreLevel, "Level", "Volume of the grace strikes against the main hit." },
    });
    preStrikeCard_.setRows({ 3 }, { 3, 3 });

    wiresCard_.addParameters({
        { kNoiseType, "Texture", "Noise layer: snare wires, white sizzle, velvet air or a metallic ring." },
        { kNoiseLevel, "Level", "Volume of the wire layer." },
        { kNoiseDecay, "Decay", "How long the wires sound." },
        { kWireTension, "Tension", "How tight the snare wires are. It changes the buzz of the wire layer." },
        { kNoiseHPF, "High-pass", "Removes wire content below this frequency." },
        { kNoiseLPF, "Low-pass", "Removes wire content above this frequency." },
        { kNoiseAttack, "Attack", "Fade-in time of the wires." },
        { kNoiseHold, "Hold", "Time the wires stay at full level before they decay." },
        { kNoiseCurve, "Curve", "Shape of the wire decay. Below zero it drops faster, above zero it fades more evenly." },
        { kNoiseStereoWidth, "Width", "Stereo width of the wires." },
        { kWirePan, "Pan", "Stereo position of the wires." },
    });
    wiresCard_.setRows({ 3, 3 }, { 3, 4, 4 });

    addAndMakeVisible(voiceCard_);
    addAndMakeVisible(envelopeCard_);
    addAndMakeVisible(transientCard_);

    outputCard_.addDualEq(kEqLowFreq, kEqLowDb, "Low EQ", theme::kApricot, "Low shelf. The inner dial sets gain and the outer ring snaps to notes (E1 to E3).");
    outputCard_.addDualEq(kEqMidFreq, kEqMidDb, "Mid EQ", theme::kApricot, "Mid bell. The inner dial sets gain and the outer ring snaps to notes (B4 to B6).");
    outputCard_.addDualEq(kEqHighFreq, kEqHighDb, "High EQ", theme::kApricot, "High shelf. The inner dial sets gain and the outer ring snaps to notes (A#7 to A#9).");
    outputCard_.addParameters({
        { kStereoWidth, "Stereo width", "Width of the output. 0% is mono." },
        { kOutputGain, "Gain", "Output level after the limiter." },
        { kLimiterEnabled, "Limiter", "Turns the brickwall limiter on or off." },
        { kLimiterRelease, "Release", "How fast the limiter recovers after it reduces a peak." },
        { kLimiterCeiling, "Ceiling", "Highest peak level the limiter lets through." },
    });
    outputCard_.addControl(std::make_unique<PeakMeter>(
        processor.displayPeak,
        *processor.parameters.getRawParameterValue(kLimiterCeiling),
        processor.parameters.getRawParameterValue(kLimiterEnabled),
        processor.parameters.getRawParameterValue(kOutputGain)));
    outputCard_.setRows({ 9 });
    outputCard_.setWeights({ 13.5f, 13.5f, 13.5f, 9.5f, 9.5f, 9.5f, 9.5f, 9.5f, 5.0f });
    addAndMakeVisible(outputCard_);

    addAndMakeVisible(detailCard_);
    addAndMakeVisible(preStrikeCard_);
    addAndMakeVisible(wiresCard_);
    setPage(0);
}

void SmackSynthPanel::setPage(int page) {
    page_ = juce::jlimit(0, 1, page);
    voiceCard_.setVisible(page_ == 0);
    envelopeCard_.setVisible(page_ == 0);
    transientCard_.setVisible(page_ == 0);
    outputCard_.setVisible(page_ == 0);
    detailCard_.setVisible(page_ == 1 && advancedMode_);
    preStrikeCard_.setVisible(page_ == 1);
    wiresCard_.setVisible(page_ == 1);
    resized();
}

void SmackSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    voiceCard_.setAdvancedMode(advanced);
    transientCard_.setAdvancedMode(advanced);
    outputCard_.setAdvancedMode(advanced);
    preStrikeCard_.setAdvancedMode(advanced);
    wiresCard_.setAdvancedMode(advanced);
    setPage(page_);
    repaint();
}

void SmackSynthPanel::resized() {
    auto area = getLocalBounds();
    const int gap = 5;
    if (page_ == 0) {
        outputCard_.setBounds(area.removeFromBottom(123));
        area.removeFromBottom(gap);
        envelopeCard_.setBounds(area.removeFromBottom(120));
        area.removeFromBottom(gap);
        voiceCard_.setBounds(area.removeFromLeft((area.getWidth() - 8) / 2));
        area.removeFromLeft(8);
        transientCard_.setBounds(area);
        return;
    }
    if (advancedMode_) {
        detailCard_.setBounds(area.removeFromLeft((area.getWidth() - 10) / 2));
        area.removeFromLeft(10);
    }
    preStrikeCard_.setBounds(area.removeFromTop(area.getHeight() * 4 / 10));
    area.removeFromTop(10);
    wiresCard_.setBounds(area);
}

EffectsPanel::EffectsPanel(OpenSmackerProcessor& processor)
    : dynamicsCard_(processor, "Saturation & dynamics", presets::Section::SaturationDynamics, theme::kApricot),
      spaceCard_(processor, "Space & freeze", presets::Section::Space, theme::kIce) {
    using namespace opensmacker::params;
    dynamicsCard_.addParameters({
        { kDistType, "Saturation", "Saturation curve: tape, tube, hard clip or wavefold." },
        { kDistDrive, "Drive", "How hard the saturation is driven." },
        { kDistTone, "Tone", "Darker below zero, brighter above zero." },
        { kDistMix, "Mix", "Blend of saturated and clean sound." },
        { kCompThresh, "Threshold", "Level where compression starts." },
        { kCompRatio, "Ratio", "How much the compressor reduces level above the threshold." },
        { kCompAttack, "Comp attack", "How fast the compressor reacts to a hit." },
        { kCompRelease, "Comp release", "How fast the compressor lets go." },
        { kCompMakeup, "Makeup", "Gain added after compression." },
        { kCompMix, "Comp mix", "Blend of compressed and dry sound." },
    });
    dynamicsCard_.setRows({ 2, 2 }, { 3, 3, 4 });

    spaceCard_.addParameters({
        { kReverbMix, "Room mix", "Amount of gated room." },
        { kReverbGate, "Gate time", "Time before the gate cuts the room tail." },
        { kReverbSize, "Room size", "Size of the room." },
        { kReverbDecay, "Room decay", "Length of the room tail." },
        { kReverbDamp, "Damping", "Darkens the room tail." },
        { kEchoMix, "Echo mix", "Amount of tempo-synced slapback echo." },
        { kReverseMix, "Reverse mix", "Level of the reversed swell before the hit." },
        { kReverseMode, "Reverse sync", "When the reversed swell plays, in time with the host." },
        { kLockIn, "Freeze hit", "Records the hit once and plays that recording in place of the synth. Most sound controls do nothing while it is on." },
    });
    spaceCard_.setRows({ 2, 2 }, { 3, 4, 2 });
    addAndMakeVisible(dynamicsCard_);
    addAndMakeVisible(spaceCard_);
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    dynamicsCard_.setAdvancedMode(advanced);
    spaceCard_.setAdvancedMode(advanced);
    resized();
    repaint();
}

void EffectsPanel::resized() {
    auto area = getLocalBounds();
    const int gap = 10;
    dynamicsCard_.setBounds(area.removeFromLeft(area.getWidth() / 2));
    area.removeFromLeft(gap);
    spaceCard_.setBounds(area);
}

PerformancePanel::PerformancePanel(OpenSmackerProcessor& processor)
    : zonesCard_(processor, "MIDI pitch & key zones", presets::Section::VoiceTuning, theme::kLilac, false),
      responseCard_(processor, "Velocity & aftertouch", presets::Section::Transient, theme::kApricot, false),
      rollCard_(processor, "Host roll & controllers", presets::Section::PreStrike, theme::kIce, false) {
    using namespace opensmacker::params;
    zonesCard_.addParameters({
        { kMidiMode, "MIDI pitch", "Fixed plays the root note. Note tracking plays the pitch of each MIDI note." },
        { kKeyZonesEnabled, "Key zones", "Each note range plays its own voice in place of the Voice setting." },
        { kSnareZoneLow, "Snare low", "Lowest note that plays the snare." },
        { kTomZoneLow, "Tom low", "Lowest note that plays the tom." },
        { kBellZoneLow, "Bell low", "Lowest note that plays the bell." },
        { kRimZoneLow, "Rim low", "Lowest note that plays the rim." },
        { kSnareZoneHigh, "Snare high", "Highest note that plays the snare." },
        { kTomZoneHigh, "Tom high", "Highest note that plays the tom." },
        { kBellZoneHigh, "Bell high", "Highest note that plays the bell." },
        { kRimZoneHigh, "Rim high", "Highest note that plays the rim." },
    });
    const juce::String cancelHint = " Click again or press Escape to cancel.";
    const auto addZoneLearn = [this, &processor, &cancelHint](const char* voice, const char* lowId, const char* highId) {
        const auto tip = juce::String("Click, then play the lowest and the highest note of the ") + voice + " zone." + cancelHint;
        zonesCard_.addControl(std::make_unique<LearnControl>(processor.displayLastNoteOn, processor.displayNoteOnCount,
            std::initializer_list<LearnControl::Step>{ { *processor.parameters.getParameter(lowId), "Play low" },
                                                       { *processor.parameters.getParameter(highId), "Play high" } },
            juce::String("Learn ") + voice + " zone", tip), lowId, tip);
    };
    addZoneLearn("snare", kSnareZoneLow, kSnareZoneHigh);
    addZoneLearn("tom", kTomZoneLow, kTomZoneHigh);
    addZoneLearn("bell", kBellZoneLow, kBellZoneHigh);
    addZoneLearn("rim", kRimZoneLow, kRimZoneHigh);
    zonesCard_.setRows({ 2, 4, 4, 4 });

    responseCard_.addParameters({
        { kBodyVelocityDepth, "Body velocity", "How much note velocity changes the body level." },
        { kWireVelocityDepth, "Wire velocity", "How much note velocity changes the wire level." },
        { kTransientVelocityDepth, "Strike velocity", "How much note velocity changes the strike level." },
        { kBodyAftertouchDepth, "Body aftertouch", "How much aftertouch raises the body level." },
        { kWireAftertouchDepth, "Wire aftertouch", "How much aftertouch raises the wire level." },
        { kTransientAftertouchDepth, "Strike aftertouch", "How much aftertouch raises the strike level." },
    });
    responseCard_.setRows({ 3, 3 });

    rollCard_.addParameters({
        { kRollEnabled, "Roll", "Repeats the hit in time with the host while playback runs." },
        { kRollSubdivision, "Rate", "Note length of each roll step." },
        { kRollProbability, "Probability", "Chance that each roll step plays." },
        { kRollVelocity, "Velocity", "Velocity of the roll hits without an accent." },
        { kRollAccentVelocity, "Accent", "Velocity of the accented roll hits." },
        { kRollAccentEvery, "Accent every", "Number of roll steps from one accent to the next." },
        { kRollNote, "Roll note", "MIDI note the roll plays. With Key zones on, this note picks the voice." },
        { kMidiCcBody, "Body CC", "Controller number that sets the body level. Off ignores controllers." },
        { kMidiCcWire, "Wire CC", "Controller number that sets the wire level. Off ignores controllers." },
        { kMidiCcTransient, "Strike CC", "Controller number that sets the strike level. Off ignores controllers." },
    });
    const auto rollNoteTip = juce::String("Click, then play the note the roll should use.") + cancelHint;
    rollCard_.addControl(std::make_unique<LearnControl>(processor.displayLastNoteOn, processor.displayNoteOnCount,
        std::initializer_list<LearnControl::Step>{ { *processor.parameters.getParameter(kRollNote), "Listening" } },
        "Learn roll note", rollNoteTip), kRollNote, rollNoteTip);
    const auto addControllerLearn = [this, &processor, &cancelHint](const char* id) {
        auto& slot = *processor.parameters.getParameter(id);
        const auto tip = "Click, then move a hardware control to set " + slot.getName(64) + "." + cancelHint;
        rollCard_.addControl(std::make_unique<LearnControl>(processor.displayLastController, processor.displayControllerCount,
            std::initializer_list<LearnControl::Step>{ { slot, "Listening" } }, "Learn " + slot.getName(64), tip), id, tip);
    };
    addControllerLearn(kMidiCcBody);
    addControllerLearn(kMidiCcWire);
    addControllerLearn(kMidiCcTransient);
    rollCard_.setRows({ 2, 2, 2 }, { 2, 4, 4, 4 });

    addAndMakeVisible(zonesCard_);
    addAndMakeVisible(responseCard_);
    addAndMakeVisible(rollCard_);
}

void PerformancePanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    zonesCard_.setAdvancedMode(advanced);
    responseCard_.setAdvancedMode(advanced);
    rollCard_.setAdvancedMode(advanced);
    resized();
}

void PerformancePanel::resized() {
    auto area = getLocalBounds();
    const int gap = 8;
    rollCard_.setBounds(area.removeFromRight(area.getWidth() * 42 / 100));
    area.removeFromRight(gap);
    zonesCard_.setBounds(area.removeFromTop(area.getHeight() * 62 / 100));
    area.removeFromTop(gap);
    responseCard_.setBounds(area);
}

} // namespace opensmacker::ui
