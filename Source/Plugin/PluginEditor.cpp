#include "PluginEditor.h"
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

    const auto& presets = opensmacker::presets::factoryPresets();
    presetBox_.addSectionHeading("SN — TRANCE SNARES (24)");
    for (int i = 0; i < 24; ++i)
        presetBox_.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetBox_.addSeparator();
    presetBox_.addSectionHeading("BL — BELLS & CHIMES (24)");
    for (int i = 24; i < 48; ++i)
        presetBox_.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetBox_.addSeparator();
    presetBox_.addSectionHeading("TM — TRANCE TOMS (24)");
    for (int i = 48; i < 72; ++i)
        presetBox_.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetBox_.onChange = [this] {
        const int selected = presetBox_.getSelectedId() - 1;
        if (selected >= 0 && selected < processor_.getNumPrograms()) {
            processor_.setCurrentProgram(selected);
            syncPresetBox();
        }
    };
    addAndMakeVisible(presetBox_);

    const std::array<juce::TextButton*, 3> categories{ &snareCatBtn_, &bellCatBtn_, &tomCatBtn_ };
    for (int i = 0; i < 3; ++i) {
        categories[static_cast<size_t>(i)]->setClickingTogglesState(false);
        categories[static_cast<size_t>(i)]->onClick = [this, i] { selectCategory(i); };
        addAndMakeVisible(*categories[static_cast<size_t>(i)]);
    }
    snareCatBtn_.setTooltip("Jump to the 24 new trance snares.");
    bellCatBtn_.setTooltip("Jump to the 24 new bells and chimes.");
    tomCatBtn_.setTooltip("Jump to the 24 new trance toms.");

    prevPresetBtn_.setTooltip("Previous preset");
    nextPresetBtn_.setTooltip("Next preset");
    prevPresetBtn_.onClick = [this] {
        processor_.setCurrentProgram((processor_.getCurrentProgram() + 71) % 72);
        syncPresetBox();
    };
    nextPresetBtn_.onClick = [this] {
        processor_.setCurrentProgram((processor_.getCurrentProgram() + 1) % 72);
        syncPresetBox();
    };
    addAndMakeVisible(prevPresetBtn_);
    addAndMakeVisible(nextPresetBtn_);

    viewToggleBtn_.setColour(juce::TextButton::buttonColourId, opensmacker::theme::kKnobBody);
    viewToggleBtn_.setTooltip("Switch between Simple and Advanced control views.");
    viewToggleBtn_.onClick = [this] { setAdvancedView(!synthPanel_.isAdvancedMode()); };
    addAndMakeVisible(viewToggleBtn_);

    const std::array<juce::TextButton*, 3> pages{ &hitPageBtn_, &layersPageBtn_, &fxPageBtn_ };
    for (int i = 0; i < 3; ++i) {
        pages[static_cast<size_t>(i)]->onClick = [this, i] { setPage(i); };
        addAndMakeVisible(*pages[static_cast<size_t>(i)]);
    }
    addAndMakeVisible(stagePanel_);
    addAndMakeVisible(synthPanel_);
    addAndMakeVisible(effectsPanel_);

    setAdvancedView(false);
    setSize(1100, 972);
    setResizable(false, false);
    setPage(0);
    syncPresetBox();
    startTimerHz(20);
}

OpenSmackerEditor::~OpenSmackerEditor() {
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void OpenSmackerEditor::selectCategory(int categoryIndex) {
    const int baseIndex = categoryIndex * 24;
    const int current = processor_.getCurrentProgram();
    const int next = current >= baseIndex && current < baseIndex + 24
        ? baseIndex + ((current - baseIndex + 1) % 24) : baseIndex;
    processor_.setCurrentProgram(next);
    syncPresetBox();
}

void OpenSmackerEditor::setAdvancedView(bool advanced) {
    synthPanel_.setAdvancedMode(advanced);
    effectsPanel_.setAdvancedMode(advanced);
    viewToggleBtn_.setButtonText(advanced ? "ADVANCED" : "SIMPLE");
    viewToggleBtn_.setColour(juce::TextButton::textColourOffId,
        advanced ? opensmacker::theme::kAccentAmber : opensmacker::theme::kAccentCyan);
    repaint();
}

void OpenSmackerEditor::setPage(int page) {
    currentPage_ = juce::jlimit(0, 2, page);
    synthPanel_.setPage(currentPage_ == 1 ? 1 : 0);
    const bool hit = currentPage_ == 0;
    const bool layers = currentPage_ == 1;
    stagePanel_.setVisible(hit);
    synthPanel_.setVisible(hit || layers);
    effectsPanel_.setVisible(currentPage_ == 2);
    resized();
    repaint();
}

void OpenSmackerEditor::syncPresetBox() {
    const int program = juce::jlimit(0, 71, processor_.getCurrentProgram());
    presetBox_.setSelectedId(program + 1, juce::dontSendNotification);
    const std::array<juce::TextButton*, 3> cats{ &snareCatBtn_, &bellCatBtn_, &tomCatBtn_ };
    const std::array<juce::Colour, 3> colors{
        opensmacker::theme::kAccentMagenta, opensmacker::theme::kAccentCyan, opensmacker::theme::kAccentAmber
    };
    for (int i = 0; i < 3; ++i) {
        const bool active = program / 24 == i;
        cats[static_cast<size_t>(i)]->setColour(juce::TextButton::buttonColourId,
            active ? colors[static_cast<size_t>(i)].withAlpha(0.25f) : opensmacker::theme::kKnobBody);
        cats[static_cast<size_t>(i)]->setColour(juce::TextButton::textColourOffId,
            active ? colors[static_cast<size_t>(i)] : opensmacker::theme::kTextDim);
    }
    const std::array<juce::TextButton*, 3> pages{ &hitPageBtn_, &layersPageBtn_, &fxPageBtn_ };
    for (int i = 0; i < 3; ++i) {
        const bool active = currentPage_ == i;
        pages[static_cast<size_t>(i)]->setColour(juce::TextButton::buttonColourId,
            active ? opensmacker::theme::kAccentCyan.withAlpha(0.22f) : opensmacker::theme::kKnobBody);
        pages[static_cast<size_t>(i)]->setColour(juce::TextButton::textColourOffId,
            active ? opensmacker::theme::kAccentCyan : opensmacker::theme::kTextDim);
    }
}

void OpenSmackerEditor::timerCallback() {
    if (presetBox_.getSelectedId() != processor_.getCurrentProgram() + 1)
        syncPresetBox();
}

void OpenSmackerEditor::paint(juce::Graphics& g) {
    g.fillAll(opensmacker::theme::kBackground);
    auto header = getLocalBounds().removeFromTop(120);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff141d2a), 0.0f, 0.0f,
        juce::Colour(0xff090d15), 0.0f, 120.0f, false));
    g.fillRect(header);
    g.setColour(opensmacker::theme::kPanelEdge);
    g.drawHorizontalLine(119, 0.0f, static_cast<float>(getWidth()));
    g.setColour(opensmacker::theme::kAccentMagenta);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("openSmacker", 16, 8, 190, 30, juce::Justification::centredLeft, true);
    g.setColour(opensmacker::theme::kTextDim);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("TRANCE PERCUSSION SYNTH  /  v" JucePlugin_VersionString, 18, 39, 340, 16, juce::Justification::centredLeft, true);
    g.setColour(opensmacker::theme::kAccentAmber);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("72 NEW PROGRAMS  |  SN 24  /  BL 24  /  TM 24", getWidth() - 340, 18, 324, 18,
        juce::Justification::centredRight, true);
    g.setColour(opensmacker::theme::kTextDim);
    g.setFont(juce::FontOptions(10.5f));
    const auto mode = synthPanel_.isAdvancedMode() ? "ADVANCED VIEW  |  numeric readouts" : "SIMPLE VIEW  |  no numeric readouts";
    g.drawText(mode, getWidth() - 340, 43, 324, 16, juce::Justification::centredRight, true);
}

void OpenSmackerEditor::resized() {
    const int margin = 14;
    int x = margin;
    const int y = 52;
    const int h = 27;
    const int categoryW = 42;
    snareCatBtn_.setBounds(x, y, categoryW, h); x += categoryW + 4;
    bellCatBtn_.setBounds(x, y, categoryW, h); x += categoryW + 4;
    tomCatBtn_.setBounds(x, y, categoryW, h); x += categoryW + 10;
    prevPresetBtn_.setBounds(x, y, 26, h); x += 30;
    presetBox_.setBounds(x, y, 390, h); x += 394;
    nextPresetBtn_.setBounds(x, y, 26, h); x += 36;
    viewToggleBtn_.setBounds(x, y, 96, h); x += 110;
    hitPageBtn_.setBounds(x, y, 64, h); x += 68;
    layersPageBtn_.setBounds(x, y, 78, h); x += 82;
    fxPageBtn_.setBounds(x, y, 58, h);

    auto content = getLocalBounds().withTrimmedTop(128).reduced(margin, 8);
    if (currentPage_ == 0) {
        const int stageWidth = 330;
        stagePanel_.setBounds(content.removeFromLeft(stageWidth).reduced(0, 0));
        content.removeFromLeft(10);
        synthPanel_.setBounds(content);
    } else if (currentPage_ == 1) {
        synthPanel_.setBounds(content);
    } else {
        effectsPanel_.setBounds(content);
    }
}
