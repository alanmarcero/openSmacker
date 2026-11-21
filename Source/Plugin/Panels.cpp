#include "Panels.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>

namespace opensmacker::ui {

namespace {
struct ControlSpec {
    const char* id;
    const char* label;
    const char* tip;
};

// Also reaches slider text boxes and stepper buttons, which keep their own tooltips.
void setTooltipDeep(juce::Component& component, const juce::String& text) {
    if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(&component))
        client->setTooltip(text);
    std::ranges::for_each(component.getChildren(), [&text](juce::Component* child) { setTooltipDeep(*child, text); });
}

// Sets a CC slot from the next controller message the plugin receives.
class LearnControl : public juce::Component, public juce::Timer {
public:
    LearnControl(OpenSmackerProcessor& processor, juce::RangedAudioParameter& slot, const juce::String& title)
        : processor_(processor), slot_(slot) {
        button_.setButtonText("Learn");
        button_.setTitle("Learn " + title);
        button_.setTooltip("Click, then move a hardware control to set " + title + ". Click again to cancel.");
        button_.setColour(juce::TextButton::buttonOnColourId, theme::kIce);
        button_.setColour(juce::TextButton::textColourOnId, theme::kInk);
        button_.onClick = [this] {
            if (isTimerRunning()) {
                stopTimer();
                button_.setToggleState(false, juce::dontSendNotification);
                button_.setButtonText("Learn");
                return;
            }
            armedCount_ = processor_.displayControllerCount.load();
            button_.setToggleState(true, juce::dontSendNotification);
            button_.setButtonText("Listening");
            startTimerHz(20);
        };
        addAndMakeVisible(button_);
    }

    void resized() override {
        button_.setBounds(getLocalBounds().withSizeKeepingCentre(std::min(160, getWidth() - 8), 40));
    }

    void timerCallback() override {
        if (processor_.displayControllerCount.load() == armedCount_)
            return;
        slot_.beginChangeGesture();
        slot_.setValueNotifyingHost(slot_.convertTo0to1(static_cast<float>(processor_.displayLastController.load())));
        slot_.endChangeGesture();
        stopTimer();
        button_.setToggleState(false, juce::dontSendNotification);
        button_.setButtonText("Learn");
    }

private:
    OpenSmackerProcessor& processor_;
    juce::RangedAudioParameter& slot_;
    juce::TextButton button_;
    uint32_t armedCount_ = 0;
};
} // namespace

SmackerStagePanel::SmackerStagePanel(OpenSmackerProcessor& processor) : processor_(processor) {
    startTimerHz(12);
}

void SmackerStagePanel::timerCallback() {
    if (isShowing()) repaint();
}

void SmackerStagePanel::updateWaveform(const core::SmackSynthSettings& settings) {
    if (lastSettings_ && *lastSettings_ == settings) return;
    lastSettings_ = settings;
    auto dry = settings;
    dry.fullAdsrEnabled = false;
    dry.reverbMix = 0;
    core::SmackSynth synth;
    constexpr int rate = 24000;
    synth.prepare(rate);
    std::vector<float> left, right;
    const int samples = synth.renderPrefix(left, right, dry, rate / 2);
    previewMs_ = juce::roundToInt(1000.0 * samples / rate);
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
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(ultraCompact ? 14.5f : 17.0f, juce::Font::bold));
    g.drawText("Dry one-shot waveform", plot.removeFromTop(ultraCompact ? 18 : 23), juce::Justification::centredLeft);
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

HitMonitor::HitMonitor(OpenSmackerProcessor& processor) : processor_(processor) {
    setTooltip("Flashes on each hit, brighter for harder hits. Shows the last note and the voice it played.");
    startTimerHz(30);
}

void HitMonitor::timerCallback() {
    const auto count = processor_.displayTriggerCount.load();
    const float velocity = processor_.displayVelocity.load();
    glow_ = count != lastTriggerCount_ ? velocity : glow_ * 0.82f;
    lastTriggerCount_ = count;

    const int note = processor_.displayLastNote.load();
    const int voice = processor_.displayLastVoice.load();
    const auto* voiceParameter = processor_.parameters.getParameter(params::kVoiceMode);
    auto source = juce::MidiMessage::getMidiNoteName(note, true, true, 4);
    if (note == OpenSmackerProcessor::kAuditionNote)
        source = "Audition";
    auto target = juce::String("no zone");
    if (voice >= 0)
        target = voiceParameter->getText(voiceParameter->convertTo0to1(static_cast<float>(voice)), 0);
    readout_ = note < 0 ? juce::String() : source + " -> " + target;
    repaint();
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
    g.drawText(readout_, area, juce::Justification::centredLeft, true);
}

PeakMeter::PeakMeter(std::atomic<float>& peak, const std::atomic<float>& ceilingDb) : peak_(peak), ceilingDb_(ceilingDb) {
    setTooltip("Output peak in dBFS. The line holds the highest peak for 1 s. The top light stays on after a peak above 0 dBFS; click to clear it.");
    startTimerHz(30);
}

void PeakMeter::timerCallback() {
    constexpr float floorDb = -60.0f;
    const float peak = peak_.exchange(0.0f);
    const float peakDb = juce::Decibels::gainToDecibels(peak, floorDb);
    levelDb_ = std::max(peakDb, levelDb_ - 1.0f);
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (peakDb >= holdDb_ || now - holdStartMs_ > 1000.0) {
        holdDb_ = peakDb;
        holdStartMs_ = now;
    }
    clipped_ = clipped_ || peak > 1.0f;
    repaint();
}

void PeakMeter::mouseDown(const juce::MouseEvent&) {
    clipped_ = false;
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
    const auto yFor = [&bar](float db) { return juce::jmap(juce::jlimit(-60.0f, 6.0f, db), -60.0f, 6.0f, bar.getBottom(), bar.getY()); };
    g.setColour(clipped_ ? theme::kApricot : theme::kPanelEdge);
    g.fillRoundedRectangle(clipLight, 2.0f);
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(bar, 2.0f);
    g.setColour(theme::kLilac);
    g.fillRect(bar.withTop(yFor(levelDb_)));
    g.setColour(theme::kText);
    g.fillRect(bar.getX(), yFor(holdDb_) - 1.0f, bar.getWidth(), 2.0f);
    g.setColour(theme::kApricot);
    g.fillRect(bar.getX() - 3.0f, yFor(ceilingDb_.load()) - 0.5f, bar.getWidth() + 6.0f, 1.0f);
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
        repaint();
    };
    addAndMakeVisible(lockButton_);
    lockButton_.setVisible(lockable_);
}

void ParameterCard::addParameter(const juce::String& id, const juce::String& title, const juce::String& tip) {
    std::unique_ptr<juce::Component> component;
    auto& state = processor_.parameters;
    if (auto* parameter = state.getParameter(id); dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr || id == params::kAmpEnabled) {
        component = std::make_unique<Choice>(state, id, title, accent_);
    } else if (dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr) {
        component = std::make_unique<Toggle>(state, id, title, accent_);
    } else {
        const auto* integer = dynamic_cast<juce::AudioParameterInt*>(parameter);
        component = std::make_unique<Knob>(state, id, title, accent_, integer != nullptr && integer->getRange().getLength() <= 16);
    }
    setTooltipDeep(*component, tip);
    addAndMakeVisible(*component);
    controls_.push_back(std::move(component));
}

void ParameterCard::addDualEq(const juce::String& freqId, const juce::String& dbId,
                              const juce::String& title, juce::Colour freqAccent, const juce::String& tip) {
    auto knob = std::make_unique<DualEqKnob>(processor_.parameters, freqId, dbId, title, accent_, freqAccent);
    setTooltipDeep(*knob, tip);
    addAndMakeVisible(*knob);
    controls_.push_back(std::move(knob));
}

void ParameterCard::addControl(std::unique_ptr<juce::Component> control) {
    addAndMakeVisible(*control);
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
    const bool locked = processor_.isSectionLocked(section_);
    if (lockable_) {
        lockButton_.setButtonText(locked ? "Locked" : "Lock");
        lockButton_.setColour(juce::TextButton::buttonColourId, locked ? accent_ : theme::kKnobBody);
        lockButton_.setColour(juce::TextButton::textColourOffId, locked ? theme::kInk : theme::kTextDim);
    }
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
    const std::array<ControlSpec, 6> voice{ {
        { kVoiceMode, "Voice", "Drum model: snare, tom, bell or rim." },
        { kToneNote, "Root note", "Pitch of the hit. With MIDI pitch on Fixed, every note plays this pitch." },
        { kToneOctave, "Octave", "Octave of the root note." },
        { kToneDecay, "Body decay", "How long the drum body rings." },
        { kTonePitchEnv, "Pitch sweep", "Pitch offset at the start of the hit, in semitones. The pitch glides back to the root." },
        { kToneLevel, "Body level", "Volume of the drum body." },
    } };
    std::ranges::for_each(voice, [this](const ControlSpec& spec) { voiceCard_.addParameter(spec.id, spec.label, spec.tip); });
    voiceCard_.setRows({ 3, 3 });
    const std::array<ControlSpec, 10> detail{ {
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
    } };
    std::ranges::for_each(detail, [this](const ControlSpec& spec) { detailCard_.addParameter(spec.id, spec.label, spec.tip); });
    detailCard_.setRows({ 4, 3, 3 });
    const std::array<ControlSpec, 5> envelope{ {
        { kAmpEnabled, "MIDI gate", "One-shot plays the whole hit. Hold notes sustains until the matching note-off." },
        { kAmpAttack, "Attack", "Fade-in time at the start of the hit." },
        { kAmpDecay, "Decay", "Time to fall from the peak to the sustain level." },
        { kAmpSustain, "Sustain", "Level the hit holds after the decay." },
        { kAmpRelease, "Release", "Fade-out time at the end of the hit, or after note-off with Hold notes." },
    } };
    std::ranges::for_each(envelope, [this](const ControlSpec& spec) { envelopeCard_.addParameter(spec.id, spec.label, spec.tip); });
    envelopeCard_.setRows({ 5 });

    const std::array<ControlSpec, 7> transient{ {
        { kClickType, "Strike type", "Strike sound: stick, mallet, rimshot or laser." },
        { kClickLevel, "Level", "Volume of the strike." },
        { kClickFreq, "Frequency", "Pitch of the strike click. Higher values sound brighter." },
        { kSubThump, "Sub thump", "Adds a low thump under the strike." },
        { kTransientHPF, "High-pass", "Removes strike content below this frequency." },
        { kTransientLPF, "Low-pass", "Removes strike content above this frequency." },
        { kTransientPan, "Pan", "Stereo position of the strike." },
    } };
    std::ranges::for_each(transient, [this](const ControlSpec& spec) { transientCard_.addParameter(spec.id, spec.label, spec.tip); });
    transientCard_.setRows({ 1, 2 }, { 4, 3 });

    const std::array<ControlSpec, 6> flam{ {
        { kPreBursts, "Hits", "Number of strikes, counting the main hit. Set 2 or more for a flam or drag." },
        { kPreSpread, "Spacing", "Time between the flam strikes." },
        { kPreDecay, "Decay", "How fast each grace strike fades." },
        { kPreJitter, "Humanize", "Random timing of the grace strikes." },
        { kFlamSpread, "Stereo spread", "Pans the grace strikes left and right." },
        { kPreLevel, "Level", "Volume of the grace strikes against the main hit." },
    } };
    std::ranges::for_each(flam, [this](const ControlSpec& spec) { preStrikeCard_.addParameter(spec.id, spec.label, spec.tip); });
    preStrikeCard_.setRows({ 3 }, { 3, 3 });

    const std::array<ControlSpec, 11> wires{ {
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
    } };
    std::ranges::for_each(wires, [this](const ControlSpec& spec) { wiresCard_.addParameter(spec.id, spec.label, spec.tip); });
    wiresCard_.setRows({ 3, 3 }, { 3, 4, 4 });

    addAndMakeVisible(voiceCard_);
    addAndMakeVisible(envelopeCard_);
    addAndMakeVisible(transientCard_);

    outputCard_.addDualEq(kEqLowFreq, kEqLowDb, "Low EQ", theme::kApricot, "Low shelf. The inner dial sets gain and the outer ring sets frequency.");
    outputCard_.addDualEq(kEqMidFreq, kEqMidDb, "Mid EQ", theme::kApricot, "Mid bell. The inner dial sets gain and the outer ring sets frequency.");
    outputCard_.addDualEq(kEqHighFreq, kEqHighDb, "High EQ", theme::kApricot, "High shelf. The inner dial sets gain and the outer ring sets frequency.");
    const std::array<ControlSpec, 5> output{ {
        { kStereoWidth, "Stereo width", "Width of the output. 0% is mono." },
        { kOutputGain, "Gain", "Output level after the limiter." },
        { kLimiterEnabled, "Limiter", "Turns the brickwall limiter on or off." },
        { kLimiterRelease, "Release", "How fast the limiter recovers after it reduces a peak." },
        { kLimiterCeiling, "Ceiling", "Highest peak level the limiter lets through." },
    } };
    std::ranges::for_each(output, [this](const ControlSpec& spec) { outputCard_.addParameter(spec.id, spec.label, spec.tip); });
    outputCard_.addControl(std::make_unique<PeakMeter>(processor.displayPeak, *processor.parameters.getRawParameterValue(kLimiterCeiling)));
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
    detailCard_.setVisible(page_ == 1 && advanced);
    transientCard_.setAdvancedMode(advanced);
    outputCard_.setAdvancedMode(advanced);
    preStrikeCard_.setAdvancedMode(advanced);
    wiresCard_.setAdvancedMode(advanced);
    resized();
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
      spaceCard_(processor, "Space & WAV lock", presets::Section::Space, theme::kIce) {
    using namespace opensmacker::params;
    const std::array<ControlSpec, 10> dynamics{ {
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
    } };
    std::ranges::for_each(dynamics, [this](const ControlSpec& spec) { dynamicsCard_.addParameter(spec.id, spec.label, spec.tip); });
    dynamicsCard_.setRows({ 2, 2 }, { 3, 3, 4 });

    const std::array<ControlSpec, 9> space{ {
        { kReverbMix, "Room mix", "Amount of gated room." },
        { kReverbGate, "Gate time", "Time before the gate cuts the room tail." },
        { kReverbSize, "Room size", "Size of the room." },
        { kReverbDecay, "Room decay", "Length of the room tail." },
        { kReverbDamp, "Damping", "Darkens the room tail." },
        { kEchoMix, "Echo mix", "Amount of tempo-synced slapback echo." },
        { kReverseMix, "Reverse mix", "Level of the reversed swell before the hit." },
        { kReverseMode, "Reverse sync", "When the reversed swell plays, in time with the host." },
        { kLockIn, "Lock WAV", "Renders the hit once and plays that recording in place of the synth." },
    } };
    std::ranges::for_each(space, [this](const ControlSpec& spec) { spaceCard_.addParameter(spec.id, spec.label, spec.tip); });
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
    const std::array<ControlSpec, 10> zones{ {
        { kMidiMode, "MIDI pitch", "Fixed plays the root note. Note tracking plays the pitch of each MIDI note." },
        { kKeyZonesEnabled, "Key zones", "Each note range plays its own voice in place of the Voice setting." },
        { kSnareZoneLow, "Snare low", "Lowest note that plays the snare." },
        { kSnareZoneHigh, "Snare high", "Highest note that plays the snare." },
        { kTomZoneLow, "Tom low", "Lowest note that plays the tom." },
        { kTomZoneHigh, "Tom high", "Highest note that plays the tom." },
        { kBellZoneLow, "Bell low", "Lowest note that plays the bell." },
        { kBellZoneHigh, "Bell high", "Highest note that plays the bell." },
        { kRimZoneLow, "Rim low", "Lowest note that plays the rim." },
        { kRimZoneHigh, "Rim high", "Highest note that plays the rim." },
    } };
    std::ranges::for_each(zones, [this](const ControlSpec& spec) { zonesCard_.addParameter(spec.id, spec.label, spec.tip); });
    zonesCard_.setRows({ 2, 4, 4 });

    const std::array<ControlSpec, 6> response{ {
        { kBodyVelocityDepth, "Body velocity", "How much note velocity changes the body level." },
        { kWireVelocityDepth, "Wire velocity", "How much note velocity changes the wire level." },
        { kTransientVelocityDepth, "Strike velocity", "How much note velocity changes the strike level." },
        { kBodyAftertouchDepth, "Body aftertouch", "How much aftertouch raises the body level." },
        { kWireAftertouchDepth, "Wire aftertouch", "How much aftertouch raises the wire level." },
        { kTransientAftertouchDepth, "Strike aftertouch", "How much aftertouch raises the strike level." },
    } };
    std::ranges::for_each(response, [this](const ControlSpec& spec) { responseCard_.addParameter(spec.id, spec.label, spec.tip); });
    responseCard_.setRows({ 3, 3 });

    const std::array<ControlSpec, 10> roll{ {
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
    } };
    std::ranges::for_each(roll, [this](const ControlSpec& spec) { rollCard_.addParameter(spec.id, spec.label, spec.tip); });
    rollCard_.addControl(std::make_unique<LearnControl>(processor, *processor.parameters.getParameter(kMidiCcBody), "Body CC"));
    rollCard_.addControl(std::make_unique<LearnControl>(processor, *processor.parameters.getParameter(kMidiCcWire), "Wire CC"));
    rollCard_.addControl(std::make_unique<LearnControl>(processor, *processor.parameters.getParameter(kMidiCcTransient), "Strike CC"));
    rollCard_.setRows({ 2, 2, 2 }, { 2, 3, 2, 3, 3 });

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
    zonesCard_.setBounds(area.removeFromTop(area.getHeight() * 55 / 100));
    area.removeFromTop(gap);
    responseCard_.setBounds(area);
}

} // namespace opensmacker::ui
