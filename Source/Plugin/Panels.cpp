#include "Panels.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace opensmacker::ui {

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
    auto name = area.removeFromLeft(194).reduced(18, 13);
    const auto* voice = processor_.parameters.getParameter(params::kVoiceMode);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText(voice->getCurrentValueAsText(), name.removeFromTop(24), juce::Justification::centredLeft);
    const auto note = juce::String(core::kNoteNames[static_cast<size_t>(settings.note)].data()) + juce::String(settings.octave);
    g.setColour(theme::kText);
    const bool compact = getHeight() < 140;
    g.setFont(juce::FontOptions(compact ? 41.0f : 54.0f, juce::Font::bold));
    g.drawText(note, name.removeFromTop(compact ? 44 : 61), juce::Justification::centredLeft);
    g.setColour(theme::kApricot);
    g.setFont(juce::FontOptions(16.0f));
    g.drawText(juce::String(core::noteToHz(settings.note, settings.octave, settings.fineCents), 1) + " Hz",
               name, juce::Justification::topLeft);
    auto plot = area.reduced(16, 11);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("Dry one-shot waveform", plot.removeFromTop(23), juce::Justification::centredLeft);
    auto axis = plot.removeFromBottom(21);
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

ParameterCard::ParameterCard(OpenSmackerProcessor& processor, juce::String title,
                             presets::Section section, juce::Colour accent, bool lockable)
    : processor_(processor), title_(std::move(title)), section_(section), accent_(accent), lockable_(lockable) {
    lockButton_.setClickingTogglesState(false);
    lockButton_.setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    lockButton_.setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    lockButton_.setTooltip("Keep this section's values when loading a preset. Controls remain editable.");
    lockButton_.onClick = [this] {
        processor_.setSectionLocked(section_, !processor_.isSectionLocked(section_));
        repaint();
    };
    addAndMakeVisible(lockButton_);
    lockButton_.setVisible(lockable_);
}

void ParameterCard::addParameter(const juce::String& id, const juce::String& title) {
    std::unique_ptr<juce::Component> component;
    auto& state = processor_.parameters;
    if (auto* parameter = state.getParameter(id); dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr || id == params::kAmpEnabled) {
        component = std::make_unique<Choice>(state, id, title, accent_);
    } else if (dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr) {
        component = std::make_unique<Toggle>(state, id, title, accent_);
    } else {
        component = std::make_unique<Knob>(state, id, title, accent_);
    }
    const auto tip = id == params::kAmpEnabled
        ? juce::String("Play a one-shot or sustain until the matching MIDI note is released.")
        : state.getParameter(id)->getName(80);
    if (auto* knob = dynamic_cast<Knob*>(component.get())) knob->setTooltip(tip);
    if (auto* menu = dynamic_cast<Choice*>(component.get())) menu->setTooltip(tip);
    if (auto* toggle = dynamic_cast<Toggle*>(component.get())) toggle->setTooltip(tip);
    addAndMakeVisible(*component);
    controls_.push_back(std::move(component));
}

void ParameterCard::setRows(std::vector<int> basic, std::vector<int> advanced) {
    basicRows_ = std::move(basic);
    advancedRows_ = advanced.empty() ? basicRows_ : std::move(advanced);
}

void ParameterCard::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    resized();
}

void ParameterCard::paint(juce::Graphics& g) {
    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 7.0f);
    auto heading = getLocalBounds().removeFromTop(44).reduced(12, 2);
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
    auto header = area.removeFromTop(44).reduced(8, 4);
    lockButton_.setBounds(lockable_ ? header.removeFromRight(70) : juce::Rectangle<int>{});
    area = area.reduced(8, 8);
    const int gap = 4;
    int y = area.getY();
    size_t controlIndex = 0;
    const auto& rows = advancedMode_ ? advancedRows_ : basicRows_;
    if (rows.empty()) return;
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
      transientCard_(processor, "Transient strike", presets::Section::Transient, theme::kIce),
      envelopeCard_(processor, "Amplitude envelope", presets::Section::VoiceTuning, theme::kLilac, false),
      preStrikeCard_(processor, "Flam", presets::Section::PreStrike, theme::kApricot),
      wiresCard_(processor, "Wires & air", presets::Section::WiresAir, theme::kIce) {
    using namespace opensmacker::params;
    const std::array<std::pair<const char*, const char*>, 16> voice{
        std::pair{ kVoiceMode, "Voice" }, { kToneNote, "Root" }, { kToneOctave, "Octave" },
        { kToneDecay, "Body decay" }, { kTonePitchEnv, "Pitch sweep" }, { kToneLevel, "Body level" },
        { kFmAmount, "FM depth" }, { kToneFreq, "Shell frequency" }, { kToneQ, "Shell Q" }, { kShellSpread, "Mode spread" },
        { kTonePitchDecay, "Pitch decay" }, { kToneFine, "Fine tune" },
        { kBodyHPF, "High-pass" }, { kBodyLPF, "Low-pass" }, { kBodyPan, "Pan" }, { kLockPitch, "Lock pitch" }
    };
    for (const auto& [id, title] : voice)
        voiceCard_.addParameter(id, title);
    voiceCard_.setRows({ 3, 3 }, { 3, 7, 6 });
    envelopeCard_.addParameter(kAmpEnabled, "MIDI gate");
    envelopeCard_.addParameter(kAmpAttack, "Attack");
    envelopeCard_.addParameter(kAmpDecay, "Decay");
    envelopeCard_.addParameter(kAmpSustain, "Sustain");
    envelopeCard_.addParameter(kAmpRelease, "Release");
    envelopeCard_.setRows({ 5 });

    transientCard_.addParameter(kClickType, "Strike Type");
    transientCard_.addParameter(kClickLevel, "Strike Level");
    transientCard_.addParameter(kClickFreq, "Strike Freq");
    transientCard_.addParameter(kSubThump, "Sub Thump");
    transientCard_.addParameter(kTransientHPF, "Transient HPF");
    transientCard_.addParameter(kTransientLPF, "Transient LPF");
    transientCard_.addParameter(kTransientPan, "Transient Pan");
    transientCard_.setRows({ 1, 2 }, { 1, 3, 3 });

    const std::array<std::pair<const char*, const char*>, 6> pre{
        std::pair{ kPreBursts, "Hits" }, { kPreSpread, "Spacing" }, { kPreDecay, "Decay" },
        { kPreJitter, "Humanize" }, { kFlamSpread, "Stereo Pan" }, { kPreLevel, "Level" }
    };
    for (const auto& [id, title] : pre)
        preStrikeCard_.addParameter(id, title);
    preStrikeCard_.setRows({ 3 }, { 3, 3 });

    const std::array<std::pair<const char*, const char*>, 11> air{
        std::pair{ kNoiseType, "Texture" }, { kNoiseLevel, "Level" }, { kNoiseDecay, "Decay" },
        { kWireTension, "Wire tension" }, { kNoiseHPF, "High-pass" }, { kNoiseLPF, "Low-pass" },
        { kNoiseAttack, "Attack" }, { kNoiseHold, "Hold" }, { kNoiseCurve, "Curve" },
        { kNoiseStereoWidth, "Width" }, { kWirePan, "Pan" }
    };
    for (const auto& [id, title] : air)
        wiresCard_.addParameter(id, title);
    wiresCard_.setRows({ 3, 3 }, { 3, 4, 4 });

    addAndMakeVisible(voiceCard_);
    addAndMakeVisible(envelopeCard_);
    addAndMakeVisible(transientCard_);
    addAndMakeVisible(preStrikeCard_);
    addAndMakeVisible(wiresCard_);
    setPage(0);
}

void SmackSynthPanel::setPage(int page) {
    page_ = juce::jlimit(0, 1, page);
    voiceCard_.setVisible(page_ == 0);
    envelopeCard_.setVisible(page_ == 0);
    transientCard_.setVisible(page_ == 0);
    preStrikeCard_.setVisible(page_ == 1);
    wiresCard_.setVisible(page_ == 1);
    resized();
}

void SmackSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    voiceCard_.setAdvancedMode(advanced);
    transientCard_.setAdvancedMode(advanced);
    preStrikeCard_.setAdvancedMode(advanced);
    wiresCard_.setAdvancedMode(advanced);
    resized();
    repaint();
}

void SmackSynthPanel::resized() {
    auto area = getLocalBounds();
    const int gap = 10;
    if (page_ == 0) {
        envelopeCard_.setBounds(area.removeFromBottom(advancedMode_ ? 132 : 148));
        area.removeFromBottom(gap);
        voiceCard_.setBounds(area.removeFromLeft(area.getWidth() * 65 / 100));
        area.removeFromLeft(gap);
        transientCard_.setBounds(area);
    } else {
        preStrikeCard_.setBounds(area.removeFromTop(area.getHeight() * 4 / 10));
        area.removeFromTop(gap);
        wiresCard_.setBounds(area);
    }
}

EffectsPanel::EffectsPanel(OpenSmackerProcessor& processor)
    : dynamicsCard_(processor, "Saturation & dynamics", presets::Section::SaturationDynamics, theme::kApricot),
      spaceCard_(processor, "Space & output", presets::Section::SpaceOutput, theme::kIce) {
    using namespace opensmacker::params;
    const std::array<std::pair<const char*, const char*>, 13> dynamics{
        std::pair{ kDistType, "Saturation" }, { kDistDrive, "Drive" }, { kDistTone, "Tone" }, { kDistMix, "Mix" },
        { kCompThresh, "Threshold" }, { kCompRatio, "Ratio" }, { kCompAttack, "Comp Attack" },
        { kCompRelease, "Comp Release" }, { kCompMakeup, "Makeup" }, { kCompMix, "Comp Mix" },
        { kLimiterCeiling, "Ceiling" }, { kLimiterRelease, "Limit Release" }, { kLimiterEnabled, "Limiter" }
    };
    for (const auto& [id, title] : dynamics)
        dynamicsCard_.addParameter(id, title);
    dynamicsCard_.setRows({ 2, 2, 2 }, { 3, 3, 3, 4 });

    const std::array<std::pair<const char*, const char*>, 11> space{
        std::pair{ kReverbMix, "Room mix" }, { kReverbGate, "Gate time" }, { kStereoWidth, "Stereo width" },
        { kOutputGain, "Output" }, { kReverbSize, "Room size" }, { kReverbDecay, "Room decay" },
        { kReverbDamp, "Damping" }, { kEchoMix, "Echo mix" }, { kReverseMode, "Reverse sync" },
        { kReverseMix, "Reverse mix" }, { kLockIn, "Lock WAV" }
    };
    for (const auto& [id, title] : space)
        spaceCard_.addParameter(id, title);
    spaceCard_.setRows({ 2, 2, 2 }, { 3, 3, 3, 2 });
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
    const std::array<std::pair<const char*, const char*>, 10> zones{
        std::pair{ kMidiMode, "MIDI pitch" }, { kKeyZonesEnabled, "Key zones" },
        { kSnareZoneLow, "Snare Low" }, { kSnareZoneHigh, "Snare High" },
        { kTomZoneLow, "Tom Low" }, { kTomZoneHigh, "Tom High" },
        { kBellZoneLow, "Bell Low" }, { kBellZoneHigh, "Bell High" },
        { kRimZoneLow, "Rim Low" }, { kRimZoneHigh, "Rim High" }
    };
    for (const auto& [id, title] : zones)
        zonesCard_.addParameter(id, title);
    zonesCard_.setRows({ 2, 4, 4 });

    const std::array<std::pair<const char*, const char*>, 6> response{
        std::pair{ kBodyVelocityDepth, "Body velocity" }, { kWireVelocityDepth, "Wire velocity" },
        { kTransientVelocityDepth, "Strike velocity" }, { kBodyAftertouchDepth, "Body aftertouch" },
        { kWireAftertouchDepth, "Wire aftertouch" }, { kTransientAftertouchDepth, "Strike aftertouch" }
    };
    for (const auto& [id, title] : response)
        responseCard_.addParameter(id, title);
    responseCard_.setRows({ 3, 3 });

    const std::array<std::pair<const char*, const char*>, 10> roll{
        std::pair{ kRollEnabled, "Roll" }, { kRollSubdivision, "Rate" },
        { kRollProbability, "Probability" }, { kRollVelocity, "Velocity" },
        { kRollAccentVelocity, "Accent" }, { kRollAccentEvery, "Accent Every" },
        { kRollNote, "Roll Note" }, { kMidiCcBody, "Body CC" },
        { kMidiCcWire, "Wire CC" }, { kMidiCcTransient, "Transient CC" }
    };
    for (const auto& [id, title] : roll)
        rollCard_.addParameter(id, title);
    rollCard_.setRows({ 2, 2, 2 }, { 2, 3, 2, 3 });

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
