#include "PluginEditor.h"
#include "BinaryData.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"

OpenSmackerEditor::OpenSmackerEditor(OpenSmackerProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      processor_(processorToEdit),
      stagePanel_(processorToEdit),
      synthPanel_(processorToEdit),
      effectsPanel_(processorToEdit),
      tooltipWindow_(this, 500) {
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    faceplate_ = juce::ImageCache::getFromMemory(
        BinaryData::opensmackerfaceplate_png, BinaryData::opensmackerfaceplate_pngSize);

    const auto& presets = opensmacker::presets::factoryPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
        if (i == 0) {
            presetBox_.addSectionHeading("SN - Tunable Dance & Trance Snares (24)");
        }
        if (i == 24) {
            presetBox_.addSeparator();
            presetBox_.addSectionHeading("BL - FM Bells & Metallic Chimes (24)");
        }
        if (i == 48) {
            presetBox_.addSeparator();
            presetBox_.addSectionHeading("TM - Club & Melodic Tom-Toms (24)");
        }
        presetBox_.addItem(presets[i].name, static_cast<int>(i + 1));
    }
    presetBox_.onChange = [this]() {
        const int selected = presetBox_.getSelectedId() - 1;
        if (selected >= 0 && selected < processor_.getNumPrograms()) {
            processor_.setCurrentProgram(selected);
            syncPresetBox();
        }
    };

    snareCatBtn_.setTooltip("SN: Jump to Snare presets (SN 01 - SN 24)");
    snareCatBtn_.onClick = [this]() { selectCategory(0); };
    addAndMakeVisible(snareCatBtn_);

    bellCatBtn_.setTooltip("BL: Jump to Bell & Chime presets (BL 01 - BL 24)");
    bellCatBtn_.onClick = [this]() { selectCategory(1); };
    addAndMakeVisible(bellCatBtn_);

    tomCatBtn_.setTooltip("TM: Jump to Tom-Tom presets (TM 01 - TM 24)");
    tomCatBtn_.onClick = [this]() { selectCategory(2); };
    addAndMakeVisible(tomCatBtn_);

    syncPresetBox();
    addAndMakeVisible(presetBox_);

    prevPresetBtn_.onClick = [this]() {
        const int current = processor_.getCurrentProgram();
        const int prev = (current - 1 + processor_.getNumPrograms()) % processor_.getNumPrograms();
        processor_.setCurrentProgram(prev);
        syncPresetBox();
    };
    addAndMakeVisible(prevPresetBtn_);

    nextPresetBtn_.onClick = [this]() {
        const int current = processor_.getCurrentProgram();
        const int next = (current + 1) % processor_.getNumPrograms();
        processor_.setCurrentProgram(next);
        syncPresetBox();
    };
    addAndMakeVisible(nextPresetBtn_);

    viewToggleBtn_.setButtonText(synthPanel_.isAdvancedMode() ? "ADVANCED" : "SIMPLE");
    viewToggleBtn_.setColour(juce::TextButton::buttonColourId, opensmacker::theme::kKnobBody);
    viewToggleBtn_.setColour(juce::TextButton::textColourOffId, opensmacker::theme::kAccentCyan);
    viewToggleBtn_.setTooltip("Toggle between clean Simple mode and deep Advanced tweak controls.");
    viewToggleBtn_.onClick = [this]() {
        setAdvancedView(!synthPanel_.isAdvancedMode());
    };
    addAndMakeVisible(viewToggleBtn_);

    addAndMakeVisible(stagePanel_);
    addAndMakeVisible(synthPanel_);
    addAndMakeVisible(effectsPanel_);

    setSize(1100, 740);
    setResizable(false, false);
    startTimerHz(20);
}

OpenSmackerEditor::~OpenSmackerEditor() {
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void OpenSmackerEditor::selectCategory(int categoryIndex) {
    const int baseIndex = categoryIndex * 24;
    const int current = processor_.getCurrentProgram();
    if (current >= baseIndex && current < baseIndex + 24) {
        const int nextInCat = baseIndex + ((current - baseIndex + 1) % 24);
        processor_.setCurrentProgram(nextInCat);
        syncPresetBox();
        return;
    }
    processor_.setCurrentProgram(baseIndex);
    syncPresetBox();
}

void OpenSmackerEditor::setAdvancedView(bool advanced) {
    synthPanel_.setAdvancedMode(advanced);
    effectsPanel_.setAdvancedMode(advanced);
    viewToggleBtn_.setButtonText(advanced ? "ADVANCED" : "SIMPLE");
    viewToggleBtn_.setColour(juce::TextButton::textColourOffId,
        advanced ? opensmacker::theme::kAccentAmber : opensmacker::theme::kAccentCyan);
    syncPresetBox();
    resized();
    repaint();
}

void OpenSmackerEditor::syncPresetBox() {
    const int prog = processor_.getCurrentProgram();
    presetBox_.setSelectedId(prog + 1, juce::dontSendNotification);

    const bool isSnare = (prog >= 0 && prog < 24);
    const bool isBell = (prog >= 24 && prog < 48);
    const bool isTom = (prog >= 48 && prog < 72);

    snareCatBtn_.setColour(juce::TextButton::buttonColourId,
        isSnare ? opensmacker::theme::kAccentMagenta.withAlpha(0.32f) : opensmacker::theme::kKnobBody);
    snareCatBtn_.setColour(juce::TextButton::textColourOffId,
        isSnare ? opensmacker::theme::kAccentMagenta : opensmacker::theme::kTextDim);

    bellCatBtn_.setColour(juce::TextButton::buttonColourId,
        isBell ? opensmacker::theme::kAccentCyan.withAlpha(0.32f) : opensmacker::theme::kKnobBody);
    bellCatBtn_.setColour(juce::TextButton::textColourOffId,
        isBell ? opensmacker::theme::kAccentCyan : opensmacker::theme::kTextDim);

    tomCatBtn_.setColour(juce::TextButton::buttonColourId,
        isTom ? opensmacker::theme::kAccentAmber.withAlpha(0.32f) : opensmacker::theme::kKnobBody);
    tomCatBtn_.setColour(juce::TextButton::textColourOffId,
        isTom ? opensmacker::theme::kAccentAmber : opensmacker::theme::kTextDim);
}

void OpenSmackerEditor::timerCallback() {
    if (presetBox_.getSelectedId() != processor_.getCurrentProgram() + 1) {
        syncPresetBox();
    }
}

void OpenSmackerEditor::paint(juce::Graphics& g) {
    g.fillAll(opensmacker::theme::kBackground);

    if (faceplate_.isValid()) {
        const juce::Graphics::ScopedSaveState state(g);
        g.setOpacity(0.55f);
        g.drawImageWithin(faceplate_, 0, 0, getWidth(), getHeight(),
                          juce::RectanglePlacement::stretchToFit);
    }

    const auto headerArea = getLocalBounds().removeFromTop(44);
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff131926), 0.0f, 0.0f,
        juce::Colour(0xff090d16), 0.0f, static_cast<float>(headerArea.getHeight()), false));
    g.fillRect(headerArea);
    g.setColour(opensmacker::theme::kPanelEdge);
    g.drawHorizontalLine(headerArea.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
    g.setColour(opensmacker::theme::kAccentMagenta.withAlpha(0.7f));
    g.drawHorizontalLine(headerArea.getBottom() - 1, 12.0f, 290.0f);

    g.setColour(opensmacker::theme::kAccentMagenta);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("openSmacker", 16, headerArea.getY() + 10, 145, 24, juce::Justification::left, true);

    g.setColour(opensmacker::theme::kAccentAmber);
    g.setFont(juce::FontOptions(12.5f, juce::Font::italic));
    g.drawText("Smack it. Tune it. Drop it.", 162, headerArea.getY() + 13, 165, 20, juce::Justification::left, true);

    g.setColour(opensmacker::theme::kTextDim.brighter(0.25f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("v" JucePlugin_VersionString " | 72 SN / BL / TM PRESETS", getWidth() - 250, headerArea.getY() + 13, 234, 20, juce::Justification::right, true);
}

void OpenSmackerEditor::resized() {
    auto area = getLocalBounds().reduced(10, 8);

    auto header = area.removeFromTop(38);
    const int catW = 32;
    const int catGap = 3;
    const int catGroupW = catW * 3 + catGap * 2;
    const int presetW = 246;
    const int startX = header.getCentreX() - (catGroupW + 8 + 28 + presetW + 28 + 90) / 2 + 18;
    const int rowY = header.getY() + 4;

    snareCatBtn_.setBounds(startX, rowY, catW, 26);
    bellCatBtn_.setBounds(snareCatBtn_.getRight() + catGap, rowY, catW, 26);
    tomCatBtn_.setBounds(bellCatBtn_.getRight() + catGap, rowY, catW, 26);

    prevPresetBtn_.setBounds(tomCatBtn_.getRight() + 8, rowY, 24, 26);
    presetBox_.setBounds(prevPresetBtn_.getRight() + 4, rowY, presetW, 26);
    nextPresetBtn_.setBounds(presetBox_.getRight() + 4, rowY, 24, 26);
    viewToggleBtn_.setBounds(nextPresetBtn_.getRight() + 10, rowY, 86, 26);

    const int stageWidth = 330;
    stagePanel_.setBounds(area.removeFromLeft(stageWidth).reduced(2, 2));

    auto rightArea = area.reduced(2, 2);
    const float synthRatio = synthPanel_.isAdvancedMode() ? 0.58f : 0.50f;
    const int synthHeight = static_cast<int>(static_cast<float>(rightArea.getHeight()) * synthRatio);
    synthPanel_.setBounds(rightArea.removeFromTop(synthHeight).reduced(0, 2));
    effectsPanel_.setBounds(rightArea.reduced(0, 2));
}
