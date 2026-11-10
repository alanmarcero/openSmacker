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

// ============================================================================
// SmackSynthPanel
// ============================================================================

SmackSynthPanel::SmackSynthPanel(OpenSmackerProcessor& processor)
    : preBursts_(processor.parameters, opensmacker::params::kPreBursts, "Flam Hits", theme::kAccentCyan),
      preSpread_(processor.parameters, opensmacker::params::kPreSpread, "Spread", theme::kAccentCyan),
      flamSpread_(processor.parameters, opensmacker::params::kFlamSpread, "Flam Pan", theme::kAccentCyan),
      preLevel_(processor.parameters, opensmacker::params::kPreLevel, "Flam Lvl", theme::kAccentCyan),

      voiceMode_(processor.parameters, opensmacker::params::kVoiceMode, "Drum Mode", theme::kAccentMagenta),
      toneNote_(processor.parameters, opensmacker::params::kToneNote, "Root Note", theme::kAccentAmber),
      toneOctave_(processor.parameters, opensmacker::params::kToneOctave, "Octave", theme::kAccentAmber),
      toneFine_(processor.parameters, opensmacker::params::kToneFine, "Fine", theme::kAccentAmber),
      lockPitchBtn_(processor.parameters, opensmacker::params::kLockPitch, "LOCK PITCH", theme::kAccentAmber),
      toneQ_(processor.parameters, opensmacker::params::kToneQ, "Shell Q", theme::kAccentAmber),
      toneDecay_(processor.parameters, opensmacker::params::kToneDecay, "Body Decay", theme::kAccentAmber),
      tonePitchEnv_(processor.parameters, opensmacker::params::kTonePitchEnv, "Pitch Drop", theme::kAccentAmber),
      fmAmount_(processor.parameters, opensmacker::params::kFmAmount, "FM / Ring", theme::kAccentMagenta),
      toneLevel_(processor.parameters, opensmacker::params::kToneLevel, "Body Lvl", theme::kAccentAmber),

      noiseType_(processor.parameters, opensmacker::params::kNoiseType, "Wire Type", theme::kAccentViolet),
      noiseDecay_(processor.parameters, opensmacker::params::kNoiseDecay, "Wire Decay", theme::kAccentViolet),
      noiseHPF_(processor.parameters, opensmacker::params::kNoiseHPF, "HPF", theme::kAccentViolet),
      noiseLPF_(processor.parameters, opensmacker::params::kNoiseLPF, "LPF", theme::kAccentViolet),
      noiseWidth_(processor.parameters, opensmacker::params::kNoiseStereoWidth, "Wire Width", theme::kAccentViolet),
      noiseLevel_(processor.parameters, opensmacker::params::kNoiseLevel, "Wire Lvl", theme::kAccentViolet),

      clickType_(processor.parameters, opensmacker::params::kClickType, "Strike Type", theme::kAccentNeonGreen),
      clickLevel_(processor.parameters, opensmacker::params::kClickLevel, "Strike Lvl", theme::kAccentNeonGreen),
      clickFreq_(processor.parameters, opensmacker::params::kClickFreq, "Strike Freq", theme::kAccentNeonGreen) {
    addAndMakeVisible(preBursts_);
    addAndMakeVisible(preSpread_);
    addAndMakeVisible(flamSpread_);
    addAndMakeVisible(preLevel_);

    addAndMakeVisible(clickType_);
    addAndMakeVisible(clickLevel_);
    addAndMakeVisible(clickFreq_);

    addAndMakeVisible(voiceMode_);
    addAndMakeVisible(toneNote_);
    addAndMakeVisible(toneOctave_);
    addAndMakeVisible(toneFine_);
    addAndMakeVisible(lockPitchBtn_);
    addAndMakeVisible(toneQ_);
    addAndMakeVisible(toneDecay_);
    addAndMakeVisible(tonePitchEnv_);
    addAndMakeVisible(fmAmount_);
    addAndMakeVisible(toneLevel_);

    addAndMakeVisible(noiseType_);
    addAndMakeVisible(noiseDecay_);
    addAndMakeVisible(noiseHPF_);
    addAndMakeVisible(noiseLPF_);
    addAndMakeVisible(noiseWidth_);
    addAndMakeVisible(noiseLevel_);

    setAdvancedMode(false);
}

void SmackSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    flamSpread_.setVisible(advanced);
    preLevel_.setVisible(advanced);
    toneFine_.setVisible(advanced);
    toneQ_.setVisible(advanced);
    tonePitchEnv_.setVisible(advanced);
    toneLevel_.setVisible(advanced);
    noiseDecay_.setVisible(advanced);
    noiseHPF_.setVisible(advanced);
    noiseLPF_.setVisible(advanced);
    noiseLevel_.setVisible(advanced);
    clickFreq_.setVisible(advanced);
    resized();
    repaint();
}

void SmackSynthPanel::paint(juce::Graphics& g) {
    const juce::String title = advancedMode_
        ? "Snare, Bell & Tom Synthesis (Advanced View)"
        : "Snare, Bell & Tom Synthesis (Simple Mode)";
    drawPanel(g, getLocalBounds(), title, theme::kAccentMagenta);

    if (!tailSectionBounds_.isEmpty()) {
        const auto tailRect = tailSectionBounds_.toFloat().reduced(1.0f);
        drawGlow(g, tailRect, theme::kAccentAmber.withAlpha(0.12f));
        g.setColour(theme::kBackground.brighter(0.04f));
        g.fillRoundedRectangle(tailRect, 6.0f);
        g.setColour(theme::kAccentAmber.withAlpha(0.85f));
        g.drawRoundedRectangle(tailRect, 6.0f, 1.2f);

        const auto badge = juce::Rectangle<float>(tailRect.getX() + 8.0f, tailRect.getY() + 3.0f, 290.0f, 14.0f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(theme::kAccentAmber);
        g.drawText("TUNABLE SNARE / TOM / BELL BODY & FM RING", badge, juce::Justification::centredLeft);
    }

    if (!attackSectionBounds_.isEmpty()) {
        const auto attackRect = attackSectionBounds_.toFloat().reduced(1.0f);
        g.setColour(theme::kBackground.brighter(0.02f));
        g.fillRoundedRectangle(attackRect, 5.0f);
        g.setColour(theme::kPanelEdge.withAlpha(0.7f));
        g.drawRoundedRectangle(attackRect, 5.0f, 1.0f);

        const auto badge = juce::Rectangle<float>(attackRect.getX() + 8.0f, attackRect.getY() + 3.0f, 220.0f, 14.0f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(theme::kAccentCyan.withAlpha(0.85f));
        g.drawText("STRIKE TRANSIENT, FLAM & WIRES", badge, juce::Justification::centredLeft);
    }
}

void SmackSynthPanel::resized() {
    auto area = getLocalBounds().reduced(8, 26);

    if (!advancedMode_) {
        const int rowHeight = area.getHeight() / 2;
        attackSectionBounds_ = area.removeFromTop(rowHeight).reduced(0, 2);
        auto row1Controls = attackSectionBounds_.withTrimmedTop(16);
        layoutRow(row1Controls, { &preBursts_, &preSpread_, &clickType_, &clickLevel_, &noiseType_, &noiseWidth_ });

        tailSectionBounds_ = area.reduced(0, 2);
        auto row2Controls = tailSectionBounds_.withTrimmedTop(16);
        layoutRow(row2Controls, { &voiceMode_, &toneNote_, &toneOctave_, &fmAmount_, &toneDecay_, &lockPitchBtn_ });
        return;
    }

    const int rowHeight = area.getHeight() / 3;

    // Row 1: Attack: Flam (left ~58%) & Stick/Mallet/Rim Strike (right ~42%)
    attackSectionBounds_ = area.removeFromTop(rowHeight).reduced(0, 2);
    auto row1 = attackSectionBounds_.withTrimmedTop(16);
    auto flamArea = row1.removeFromLeft(static_cast<int>(static_cast<float>(row1.getWidth()) * 0.58f));
    layoutRow(flamArea, { &preBursts_, &preSpread_, &flamSpread_, &preLevel_ });
    layoutRow(row1, { &clickType_, &clickLevel_, &clickFreq_ });

    // Row 2: Tunable Snare / Tom / Bell Body
    tailSectionBounds_ = area.removeFromTop(rowHeight).reduced(0, 2);
    auto row2 = tailSectionBounds_.withTrimmedTop(16);
    layoutRow(row2, { &voiceMode_, &toneNote_, &toneOctave_, &toneFine_, &lockPitchBtn_, &fmAmount_, &toneQ_, &toneDecay_, &tonePitchEnv_, &toneLevel_ });

    // Row 3: Snare Wires, Sizzle & Air
    noiseSectionBounds_ = area.reduced(0, 2);
    layoutRow(noiseSectionBounds_, { &noiseType_, &noiseDecay_, &noiseHPF_, &noiseLPF_, &noiseWidth_, &noiseLevel_ });
}

// ============================================================================
// EffectsPanel
// ============================================================================

EffectsPanel::EffectsPanel(OpenSmackerProcessor& processor)
    : distType_(processor.parameters, opensmacker::params::kDistType, "Dist Type", theme::kAccentFlame),
      distDrive_(processor.parameters, opensmacker::params::kDistDrive, "Drive", theme::kAccentFlame),
      distTone_(processor.parameters, opensmacker::params::kDistTone, "Tone", theme::kAccentFlame),
      distMix_(processor.parameters, opensmacker::params::kDistMix, "Dist Mix", theme::kAccentFlame),

      compThresh_(processor.parameters, opensmacker::params::kCompThresh, "Threshold", theme::kAccentAmber),
      compRatio_(processor.parameters, opensmacker::params::kCompRatio, "Ratio", theme::kAccentAmber),
      compMakeup_(processor.parameters, opensmacker::params::kCompMakeup, "Makeup", theme::kAccentAmber),
      compMix_(processor.parameters, opensmacker::params::kCompMix, "Comp Mix", theme::kAccentAmber),

      limiterCeiling_(processor.parameters, opensmacker::params::kLimiterCeiling, "Ceiling", theme::kAccentNeonGreen),
      limiterEnabled_(processor.parameters, opensmacker::params::kLimiterEnabled, "Limiter", theme::kAccentNeonGreen),

      reverbSize_(processor.parameters, opensmacker::params::kReverbSize, "Size", theme::kAccentViolet),
      reverbDecay_(processor.parameters, opensmacker::params::kReverbDecay, "Decay", theme::kAccentViolet),
      reverbGate_(processor.parameters, opensmacker::params::kReverbGate, "Gate Time", theme::kAccentViolet),
      reverbMix_(processor.parameters, opensmacker::params::kReverbMix, "Rev Mix", theme::kAccentViolet),

      reverseMode_(processor.parameters, opensmacker::params::kReverseMode, "Rev Sync", theme::kAccentCyan),
      reverseMix_(processor.parameters, opensmacker::params::kReverseMix, "Rev Mix", theme::kAccentCyan),
      midiMode_(processor.parameters, opensmacker::params::kMidiMode, "MIDI Mode", theme::kTextDim),
      lockInBtn_(processor.parameters, opensmacker::params::kLockIn, "LOCK WAV", theme::kAccentAmber),
      masterWidth_(processor.parameters, opensmacker::params::kStereoWidth, "Width", theme::kAccentCyan),
      outputGain_(processor.parameters, opensmacker::params::kOutputGain, "Output", theme::kAccentAmber) {
    addAndMakeVisible(distType_);
    addAndMakeVisible(distDrive_);
    addAndMakeVisible(distTone_);
    addAndMakeVisible(distMix_);

    addAndMakeVisible(compThresh_);
    addAndMakeVisible(compRatio_);
    addAndMakeVisible(compMakeup_);
    addAndMakeVisible(compMix_);

    addAndMakeVisible(limiterCeiling_);
    addAndMakeVisible(limiterEnabled_);

    addAndMakeVisible(reverbSize_);
    addAndMakeVisible(reverbDecay_);
    addAndMakeVisible(reverbGate_);
    addAndMakeVisible(reverbMix_);

    addAndMakeVisible(reverseMode_);
    addAndMakeVisible(reverseMix_);
    addAndMakeVisible(midiMode_);
    addAndMakeVisible(lockInBtn_);
    addAndMakeVisible(masterWidth_);
    addAndMakeVisible(outputGain_);

    setAdvancedMode(false);
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    distType_.setVisible(advanced);
    distTone_.setVisible(advanced);
    compThresh_.setVisible(advanced);
    compRatio_.setVisible(advanced);
    compMakeup_.setVisible(advanced);
    limiterCeiling_.setVisible(advanced);
    reverbDecay_.setVisible(advanced);
    reverseMode_.setVisible(advanced);
    reverseMix_.setVisible(advanced);
    midiMode_.setVisible(advanced);
    resized();
    repaint();
}

void EffectsPanel::paint(juce::Graphics& g) {
    const juce::String title = advancedMode_
        ? "Impact Saturation, Dynamics & Gated Space (Advanced View)"
        : "Impact Saturation, Dynamics & Gated Space (Simple Mode)";
    drawPanel(g, getLocalBounds(), title, theme::kAccentFlame);
}

void EffectsPanel::resized() {
    auto area = getLocalBounds().reduced(8, 26);

    if (!advancedMode_) {
        const int rowHeight = area.getHeight() / 2;
        auto row1 = area.removeFromTop(rowHeight);
        layoutRow(row1, { &distDrive_, &distMix_, &compMix_, &limiterEnabled_ });
        layoutRow(area, { &reverbSize_, &reverbMix_, &reverbGate_, &masterWidth_, &outputGain_, &lockInBtn_ });
        return;
    }

    const int rowHeight = area.getHeight() / 2;

    auto row1 = area.removeFromTop(rowHeight);
    auto distArea = row1.removeFromLeft(static_cast<int>(static_cast<float>(row1.getWidth()) * 0.40f));
    layoutRow(distArea, { &distType_, &distDrive_, &distTone_, &distMix_ });
    layoutRow(row1, { &compThresh_, &compRatio_, &compMakeup_, &compMix_, &limiterCeiling_, &limiterEnabled_ });

    auto revArea = area.removeFromLeft(static_cast<int>(static_cast<float>(area.getWidth()) * 0.40f));
    layoutRow(revArea, { &reverbSize_, &reverbDecay_, &reverbGate_, &reverbMix_ });
    layoutRow(area, { &reverseMode_, &reverseMix_, &midiMode_, &masterWidth_, &outputGain_, &lockInBtn_ });
}

} // namespace opensmacker::ui
