#include "Panels.h"
#include "BinaryData.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace opensmacker::ui {

namespace {

constexpr int kRefreshHz = 60;

} // namespace

// ============================================================================
// SmackerStagePanel
// ============================================================================

SmackerStagePanel::SmackerStagePanel(OpenSmackerProcessor& processor)
    : processor_(processor) {
    stageArtwork_ = juce::ImageCache::getFromMemory(
        BinaryData::opensmackerstage_png, BinaryData::opensmackerstage_pngSize);

    auditionBtn_.setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    auditionBtn_.setColour(juce::TextButton::textColourOffId, theme::kAccentNeonGreen);
    auditionBtn_.onClick = [this]() {
        processor_.triggerAudition();
    };
    addAndMakeVisible(auditionBtn_);

    startTimerHz(kRefreshHz);
}

SmackerStagePanel::~SmackerStagePanel() {
    stopTimer();
}

void SmackerStagePanel::timerCallback() {
    const uint32_t serial = processor_.displayTriggerCount.load(std::memory_order_relaxed);
    if (serial != seenTriggers_) {
        seenTriggers_ = serial;
        smackTriggerTimeMs_ = juce::Time::getMillisecondCounterHiRes();
        impactIntensity_ = std::clamp(processor_.displayVelocity.load(std::memory_order_relaxed), 0.35f, 1.0f);
    }
    repaint();
}

void SmackerStagePanel::resized() {
    auto area = getLocalBounds().reduced(12, 10);
    auditionBtn_.setBounds(area.removeFromBottom(24).reduced(area.getWidth() / 4, 0));
}

void SmackerStagePanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), "Smack Visualizer Stage", theme::kAccentMagenta);

    auto stage = getLocalBounds().reduced(8, 28).withTrimmedBottom(28).toFloat();
    const juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(stage.toNearestInt());

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff0d1320), stage.getTopLeft(),
        juce::Colour(0xff05080f), stage.getBottomRight(), false));
    g.fillRoundedRectangle(stage, 8.0f);

    const float peak = std::clamp(processor_.displayPeak.load(), 0.0f, 1.0f);
    const float beatPhase = std::clamp(processor_.displayBeatPhase.load(), 0.0f, 1.0f);
    const float pulse = std::max(peak, 0.35f * std::pow(1.0f - beatPhase, 3.0f));
    const bool locked = processor_.isSmackLockedIn();

    const double smackAgeMs = juce::Time::getMillisecondCounterHiRes() - smackTriggerTimeMs_;
    constexpr double kAnimDurationMs = 520.0;
    const float smackPhase = (smackAgeMs < 0.0 || smackAgeMs > kAnimDurationMs)
        ? 1.0f
        : static_cast<float>(smackAgeMs / kAnimDurationMs);
    const bool isTriggerActive = (smackPhase < 1.0f);

    if (smackPhase < 0.14f) {
        const float flutter = std::sin(smackPhase * 48.0f * std::numbers::pi_v<float>);
        smackPose_ = -(1.0f - smackPhase / 0.14f) * 0.9f + flutter * 0.25f;
        shockwave_ = 0.0f;
        impactFlash_ = 0.0f;
    } else if (smackPhase < 0.28f) {
        const float strike = (smackPhase - 0.14f) / 0.14f;
        smackPose_ = -std::sin(strike * std::numbers::pi_v<float>) * 1.0f;
        shockwave_ = (1.0f - strike) * impactIntensity_;
        impactFlash_ = std::pow(1.0f - strike, 1.4f) * impactIntensity_;
    } else if (smackPhase < 1.0f) {
        const float recoil = (smackPhase - 0.28f) / 0.72f;
        const float decay = std::exp(-recoil * 3.8f);
        smackPose_ = std::sin(recoil * std::numbers::pi_v<float> * 3.0f) * decay * 0.85f;
        shockwave_ = std::max(0.0f, (1.0f - recoil * 2.2f) * impactIntensity_);
        impactFlash_ = std::max(0.0f, impactFlash_ * 0.92f);
    } else {
        smackPose_ = 0.0f;
        shockwave_ = 0.0f;
        impactFlash_ = 0.0f;
        idleBob_ = std::sin(beatPhase * 2.0f * std::numbers::pi_v<float>) * 2.0f;
    }

    const bool isSmackOn = (smackPhase < 0.75f && isTriggerActive);

    // Draw embedded holographic stage artwork with beat-reactive zoom
    if (stageArtwork_.isValid()) {
        const juce::Graphics::ScopedSaveState imgState(g);
        g.setOpacity(0.82f + impactFlash_ * 0.18f);
        const float zoom = 1.0f + pulse * 0.035f + impactFlash_ * 0.045f;
        const auto artBounds = stage.withSizeKeepingCentre(stage.getWidth() * zoom, stage.getHeight() * zoom);
        g.drawImage(stageArtwork_, artBounds, juce::RectanglePlacement::fillDestination);
    }

    // Read current voice mode and tuning for stage telemetry
    const int voiceIdx = static_cast<int>(*processor_.parameters.getRawParameterValue(opensmacker::params::kVoiceMode));
    const int noteIdx = static_cast<int>(*processor_.parameters.getRawParameterValue(opensmacker::params::kToneNote));
    const int octave = static_cast<int>(*processor_.parameters.getRawParameterValue(opensmacker::params::kToneOctave));
    const float fineCents = *processor_.parameters.getRawParameterValue(opensmacker::params::kToneFine);
    const float tunedHz = opensmacker::core::noteToHz(noteIdx, octave, fineCents);

    const std::array<const char*, 4> modeNames = {
        "DANCE SNARE", "CLUB TOM-TOM", "FM BELL / CHIME", "PRYDA RIM SMACK"
    };
    const std::array<juce::Colour, 4> modeColours = {
        theme::kAccentMagenta, theme::kAccentAmber, theme::kAccentCyan, theme::kAccentNeonGreen
    };
    const auto activeCol = modeColours[static_cast<size_t>( std::clamp(voiceIdx, 0, 3) )];

    // Acoustic bass & harmonic ring overlay
    const juce::Point<float> coneCentre{ stage.getCentreX(), stage.getCentreY() + 6.0f + idleBob_ * 0.5f };
    const float baseRadius = std::min(stage.getWidth(), stage.getHeight()) * 0.34f;
    const float pumpExtra = (pulse * 12.0f) + (impactFlash_ * 14.0f);

    if (pulse > 0.10f || impactFlash_ > 0.05f) {
        for (int r = 1; r <= 3; ++r) {
            const float rippleR = baseRadius * (0.42f + 0.22f * static_cast<float>(r)) + pumpExtra * 0.45f;
            g.setColour(activeCol.withAlpha((0.40f - static_cast<float>(r) * 0.09f) * (pulse + impactFlash_)));
            g.drawEllipse(coneCentre.x - rippleR, coneCentre.y - rippleR, rippleR * 2.0f, rippleR * 2.0f, 1.6f);
        }
    }

    // Shockwave flare on trigger
    if (shockwave_ > 0.01f) {
        const float flareRadius = 26.0f + (1.0f - shockwave_) * 44.0f;
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffffffff).withAlpha(shockwave_ * 0.85f), coneCentre,
            activeCol.withAlpha(0.0f), coneCentre.translated(flareRadius, 0.0f), true));
        g.fillEllipse(coneCentre.x - flareRadius, coneCentre.y - flareRadius, flareRadius * 2.0f, flareRadius * 2.0f);
    }

    // Slogan banner at top of stage
    const auto bannerRect = stage.removeFromTop(26.0f);
    g.setColour(juce::Colour(0xcc060911));
    g.fillRoundedRectangle(bannerRect.reduced(12.0f, 2.0f), 4.0f);

    const auto sloganCol = isSmackOn ? theme::kAccentNeonGreen : theme::kAccentMagenta;
    g.setColour(sloganCol);
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.drawText("SMACK IT. TUNE IT. DROP IT.", bannerRect, juce::Justification::centred, true);

    // Neon State Indicator LED
    const auto ledRect = juce::Rectangle<float>(stage.getX() + 12.0f, stage.getY() + 4.0f, 76.0f, 18.0f);
    g.setColour(juce::Colour(0xd9080d16));
    g.fillRoundedRectangle(ledRect, 3.0f);
    g.setColour(isSmackOn ? theme::kAccentNeonGreen : theme::kPanelEdge);
    g.drawRoundedRectangle(ledRect, 3.0f, 1.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(isSmackOn ? theme::kAccentNeonGreen : theme::kTextDim);
    g.drawText(isSmackOn ? "SMACK ON" : "SMACK OFF", ledRect, juce::Justification::centred, true);

    // Tuned Note & Frequency Readout Pill at bottom of stage
    const auto bottomPill = juce::Rectangle<float>(stage.getX() + 14.0f, stage.getBottom() - 28.0f, stage.getWidth() - 28.0f, 22.0f);
    g.setColour(juce::Colour(0xd9070b12));
    g.fillRoundedRectangle(bottomPill, 4.0f);
    g.setColour(activeCol.withAlpha(0.75f));
    g.drawRoundedRectangle(bottomPill, 4.0f, 1.0f);

    const juce::String noteStr = juce::String(std::string(opensmacker::core::kNoteNames[static_cast<size_t>(std::clamp(noteIdx, 0, 11))]))
        + juce::String(octave);
    const juce::String telemetryText = juce::String(modeNames[static_cast<size_t>(std::clamp(voiceIdx, 0, 3))])
        + "  |  KEY: " + noteStr + " (" + juce::String(tunedHz, 1) + " Hz)";
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText(telemetryText, bottomPill, juce::Justification::centred, true);

    // Locked WAV badge if active
    if (locked) {
        const auto lockRect = juce::Rectangle<float>(stage.getRight() - 78.0f, stage.getY() + 4.0f, 66.0f, 18.0f);
        g.setColour(juce::Colour(0xd9080d16));
        g.fillRoundedRectangle(lockRect, 3.0f);
        g.setColour(theme::kAccentAmber);
        g.drawRoundedRectangle(lockRect, 3.0f, 1.0f);
        g.drawText("LOCKED", lockRect, juce::Justification::centred, true);
    }

    g.setColour(theme::kPanelEdge.withAlpha(0.8f));
    g.drawRoundedRectangle(getLocalBounds().reduced(8, 28).withTrimmedBottom(28).toFloat().reduced(0.5f), 8.0f, 1.0f);
}

ParameterCard::ParameterCard(OpenSmackerProcessor& processor, juce::String title,
                             presets::Section section, juce::Colour accent)
    : processor_(processor), title_(std::move(title)), section_(section), accent_(accent) {
    lockButton_.setClickingTogglesState(false);
    lockButton_.setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    lockButton_.setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    lockButton_.setTooltip("Keep this section's values when loading a preset. Controls remain editable.");
    lockButton_.onClick = [this] {
        processor_.setSectionLocked(section_, !processor_.isSectionLocked(section_));
        repaint();
    };
    addAndMakeVisible(lockButton_);
}

void ParameterCard::addParameter(const juce::String& id, const juce::String& title) {
    std::unique_ptr<juce::Component> component;
    auto& state = processor_.parameters;
    if (auto* parameter = state.getParameter(id); dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr) {
        component = std::make_unique<Choice>(state, id, title, accent_);
    } else if (dynamic_cast<juce::AudioParameterBool*>(state.getParameter(id)) != nullptr) {
        component = std::make_unique<Toggle>(state, id, title, accent_);
    } else {
        component = std::make_unique<Knob>(state, id, title, accent_);
    }
    addAndMakeVisible(*component);
    controls_.push_back(std::move(component));
}

void ParameterCard::setRows(std::vector<int> rows) {
    rows_ = std::move(rows);
    int total = 0;
    for (const auto row : rows_)
        total += row;
    jassert(total == static_cast<int>(controls_.size()));
    juce::ignoreUnused(total);
}

void ParameterCard::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    for (auto& control : controls_) {
        if (auto* knob = dynamic_cast<Knob*>(control.get()))
            knob->setAdvancedMode(advanced);
    }
    resized();
}

void ParameterCard::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff111a27), bounds.getTopLeft(),
        juce::Colour(0xff090d15), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds.reduced(0.5f), 8.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    auto heading = getLocalBounds().removeFromTop(30).reduced(12, 2);
    g.setColour(accent_);
    g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(heading.getX()), static_cast<float>(heading.getY()) + 2.0f, 3.0f,
        static_cast<float>(heading.getHeight() - 4)), 1.5f);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(title_, heading.withTrimmedLeft(10).withTrimmedRight(94), juce::Justification::centredLeft, true);
    const bool locked = processor_.isSectionLocked(section_);
    lockButton_.setButtonText(locked ? "HELD" : "LOCK");
    lockButton_.setColour(juce::TextButton::buttonColourId, locked ? accent_.withAlpha(0.22f) : theme::kKnobBody);
    lockButton_.setColour(juce::TextButton::textColourOffId, locked ? accent_ : theme::kTextDim);
}

void ParameterCard::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(30).reduced(10, 3);
    lockButton_.setBounds(header.removeFromRight(70));
    area = area.reduced(8, 8);
    const int gap = 4;
    int y = area.getY();
    size_t controlIndex = 0;
    const int rowCount = std::max(1, static_cast<int>(rows_.size()));
    const int rowHeight = std::max(1, (area.getHeight() - gap * (rowCount - 1)) / rowCount);
    for (int row = 0; row < rowCount; ++row) {
        const int columns = std::max(1, rows_[static_cast<size_t>(row)]);
        const int cellWidth = std::max(1, (area.getWidth() - gap * (columns - 1)) / columns);
        for (int column = 0; column < columns && controlIndex < controls_.size(); ++column, ++controlIndex)
            controls_[controlIndex]->setBounds(area.getX() + column * (cellWidth + gap), y, cellWidth, rowHeight);
        y += rowHeight + gap;
    }
}

SmackSynthPanel::SmackSynthPanel(OpenSmackerProcessor& processor)
    : voiceCard_(processor, "VOICE & TUNING  /  AMPLITUDE ENVELOPE", presets::Section::VoiceTuning, theme::kAccentAmber),
      transientCard_(processor, "TRANSIENT STRIKE", presets::Section::Transient, theme::kAccentNeonGreen),
      preStrikeCard_(processor, "PRE-STRIKE FLAMS", presets::Section::PreStrike, theme::kAccentCyan),
      wiresCard_(processor, "WIRES, SIZZLE & AIR", presets::Section::WiresAir, theme::kAccentViolet) {
    using namespace opensmacker::params;
    const std::array<std::pair<const char*, const char*>, 16> voice{
        std::pair{ kVoiceMode, "Voice" }, { kToneNote, "Root" }, { kToneOctave, "Octave" }, { kToneFine, "Fine ct" },
        { kToneFreq, "Shell Hz" }, { kToneQ, "Shell Q" }, { kToneDecay, "Body Decay" }, { kTonePitchEnv, "Pitch Env" },
        { kTonePitchDecay, "Pitch Time" }, { kFmAmount, "FM / Ring" }, { kToneLevel, "Body Level" }, { kLockPitch, "Lock Pitch" },
        { kAmpAttack, "Amp Attack" }, { kAmpDecay, "Amp Decay" }, { kAmpSustain, "Amp Sustain" }, { kAmpRelease, "Amp Release" }
    };
    for (const auto& [id, title] : voice)
        voiceCard_.addParameter(id, title);
    voiceCard_.setRows({ 4, 4, 4, 4 });

    transientCard_.addParameter(kClickType, "Strike Type");
    transientCard_.addParameter(kClickLevel, "Strike Level");
    transientCard_.addParameter(kClickFreq, "Strike Freq");
    transientCard_.setRows({ 3 });

    const std::array<std::pair<const char*, const char*>, 6> pre{
        std::pair{ kPreBursts, "Hits" }, { kPreSpread, "Spread ms" }, { kPreDecay, "Decay ms" },
        { kPreJitter, "Humanize" }, { kFlamSpread, "Stereo Pan" }, { kPreLevel, "Level" }
    };
    for (const auto& [id, title] : pre)
        preStrikeCard_.addParameter(id, title);
    preStrikeCard_.setRows({ 6 });

    const std::array<std::pair<const char*, const char*>, 9> air{
        std::pair{ kNoiseType, "Layer" }, { kNoiseAttack, "Attack ms" }, { kNoiseHold, "Hold ms" },
        { kNoiseDecay, "Decay ms" }, { kNoiseCurve, "Curve" }, { kNoiseHPF, "HPF" },
        { kNoiseLPF, "LPF" }, { kNoiseStereoWidth, "Width" }, { kNoiseLevel, "Level" }
    };
    for (const auto& [id, title] : air)
        wiresCard_.addParameter(id, title);
    wiresCard_.setRows({ 5, 4 });

    addAndMakeVisible(voiceCard_);
    addAndMakeVisible(transientCard_);
    addAndMakeVisible(preStrikeCard_);
    addAndMakeVisible(wiresCard_);
    setPage(0);
}

void SmackSynthPanel::setPage(int page) {
    page_ = juce::jlimit(0, 1, page);
    voiceCard_.setVisible(page_ == 0);
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
        const int transHeight = std::max(190, area.getHeight() / 4);
        voiceCard_.setBounds(area.removeFromTop(area.getHeight() - transHeight - gap));
        area.removeFromTop(gap);
        transientCard_.setBounds(area);
    } else {
        const int preHeight = std::max(250, area.getHeight() / 3);
        preStrikeCard_.setBounds(area.removeFromTop(preHeight));
        area.removeFromTop(gap);
        wiresCard_.setBounds(area);
    }
}

EffectsPanel::EffectsPanel(OpenSmackerProcessor& processor)
    : dynamicsCard_(processor, "SATURATION & DYNAMICS", presets::Section::SaturationDynamics, theme::kAccentFlame),
      spaceCard_(processor, "SPACE & OUTPUT", presets::Section::SpaceOutput, theme::kAccentCyan) {
    using namespace opensmacker::params;
    const std::array<std::pair<const char*, const char*>, 13> dynamics{
        std::pair{ kDistType, "Saturation" }, { kDistDrive, "Drive" }, { kDistTone, "Tone" }, { kDistMix, "Mix" },
        { kCompThresh, "Comp Thresh" }, { kCompRatio, "Comp Ratio" }, { kCompAttack, "Comp Attack" },
        { kCompRelease, "Comp Release" }, { kCompMakeup, "Makeup" }, { kCompMix, "Comp Mix" },
        { kLimiterCeiling, "Limit Ceiling" }, { kLimiterRelease, "Limit Release" }, { kLimiterEnabled, "Limiter" }
    };
    for (const auto& [id, title] : dynamics)
        dynamicsCard_.addParameter(id, title);
    dynamicsCard_.setRows({ 4, 5, 4 });

    const std::array<std::pair<const char*, const char*>, 11> space{
        std::pair{ kReverbSize, "Hall Size" }, { kReverbDecay, "Hall Decay" }, { kReverbDamp, "Damping" },
        { kReverbGate, "Gate Time" }, { kReverbMix, "Hall Mix" }, { kReverseMode, "Reverse Sync" },
        { kReverseMix, "Reverse Mix" }, { kMidiMode, "MIDI Pitch" }, { kLockIn, "Lock WAV" },
        { kStereoWidth, "Master Width" }, { kOutputGain, "Output dB" }
    };
    for (const auto& [id, title] : space)
        spaceCard_.addParameter(id, title);
    spaceCard_.setRows({ 6, 5 });
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
    const int dynamicsHeight = std::max(430, static_cast<int>(static_cast<float>(area.getHeight()) * 0.56f));
    dynamicsCard_.setBounds(area.removeFromTop(dynamicsHeight));
    area.removeFromTop(gap);
    spaceCard_.setBounds(area);
}

} // namespace opensmacker::ui
