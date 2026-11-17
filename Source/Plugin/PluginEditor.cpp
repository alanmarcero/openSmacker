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
      performancePanel_(processorToEdit),
      tooltipWindow_(this, 500) {
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    const auto& presets = opensmacker::presets::factoryPresets();
    presetBox_.addSectionHeading("TRANCE SNARES (24)");
    for (int i = 0; i < 24; ++i)
        presetBox_.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetBox_.addSeparator();
    presetBox_.addSectionHeading("BELLS & CHIMES (24)");
    for (int i = 24; i < 48; ++i)
        presetBox_.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetBox_.addSeparator();
    presetBox_.addSectionHeading("TRANCE TOMS (24)");
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

    prevPresetBtn_.setTooltip("Select the previous factory preset.");
    nextPresetBtn_.setTooltip("Select the next factory preset.");
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

    const std::array<juce::TextButton*, 4> pages{ &hitPageBtn_, &layersPageBtn_, &fxPageBtn_, &midiPageBtn_ };
    for (int i = 0; i < 4; ++i) {
        pages[static_cast<size_t>(i)]->onClick = [this, i] { setPage(i); };
        addAndMakeVisible(*pages[static_cast<size_t>(i)]);
    }
    addAndMakeVisible(stagePanel_);
    addAndMakeVisible(synthPanel_);
    addAndMakeVisible(effectsPanel_);
    addAndMakeVisible(performancePanel_);

    setAdvancedView(false);
    setSize(1440, 1000);
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
    performancePanel_.setAdvancedMode(advanced);
    viewToggleBtn_.setButtonText(advanced ? "ADVANCED VIEW" : "SIMPLE VIEW");
    viewToggleBtn_.setColour(juce::TextButton::textColourOffId,
        advanced ? opensmacker::theme::kAccentAmber : opensmacker::theme::kAccentCyan);
    repaint();
}

void OpenSmackerEditor::setPage(int page) {
    currentPage_ = juce::jlimit(0, 3, page);
    synthPanel_.setPage(currentPage_ == 1 ? 1 : 0);
    const bool hit = currentPage_ == 0;
    const bool layers = currentPage_ == 1;
    stagePanel_.setVisible(hit);
    synthPanel_.setVisible(hit || layers);
    effectsPanel_.setVisible(currentPage_ == 2);
    performancePanel_.setVisible(currentPage_ == 3);
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
    const std::array<juce::TextButton*, 4> pages{ &hitPageBtn_, &layersPageBtn_, &fxPageBtn_, &midiPageBtn_ };
    for (int i = 0; i < 4; ++i) {
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
    auto header = getLocalBounds().removeFromTop(152);
    g.setGradientFill(juce::ColourGradient(opensmacker::theme::kSteel, 0.0f, 0.0f,
        opensmacker::theme::kInk, 0.0f, 152.0f, false));
    g.fillRect(header);
    g.setColour(opensmacker::theme::kPanelEdge);
    g.drawHorizontalLine(151, 0.0f, static_cast<float>(getWidth()));
    g.setColour(opensmacker::theme::kSignalCyan);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("openSmacker", 16, 8, 190, 30, juce::Justification::centredLeft, true);
    g.setColour(opensmacker::theme::kTextDim);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("TRANCE PERCUSSION SYNTH  /  v" JucePlugin_VersionString, 18, 39, 340, 16, juce::Justification::centredLeft, true);
    g.setColour(opensmacker::theme::kAccentAmber);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("72 FACTORY PROGRAMS  |  24 SNARES  /  24 BELLS  /  24 TOMS", getWidth() - 760, 16, 744, 22,
        juce::Justification::centredRight, true);
}

void OpenSmackerEditor::resized() {
    const int margin = 14;
    constexpr int gap = 8;
    constexpr int buttonHeight = 40;
    int x = margin;
    const int presetRowY = 50;
    snareCatBtn_.setBounds(x, presetRowY, 116, buttonHeight); x += 116 + gap;
    bellCatBtn_.setBounds(x, presetRowY, 164, buttonHeight); x += 164 + gap;
    tomCatBtn_.setBounds(x, presetRowY, 100, buttonHeight); x += 100 + 14;
    prevPresetBtn_.setBounds(x, presetRowY, 140, buttonHeight); x += 140 + gap;
    presetBox_.setBounds(x, presetRowY, 470, buttonHeight); x += 470 + gap;
    nextPresetBtn_.setBounds(x, presetRowY, 140, buttonHeight);

    const int pageRowWidth = 190 + 4 * 128 + 4 * gap;
    x = (getWidth() - pageRowWidth) / 2;
    const int pageRowY = 100;
    viewToggleBtn_.setBounds(x, pageRowY, 190, buttonHeight); x += 190 + gap;
    hitPageBtn_.setBounds(x, pageRowY, 128, buttonHeight); x += 128 + gap;
    layersPageBtn_.setBounds(x, pageRowY, 128, buttonHeight); x += 128 + gap;
    fxPageBtn_.setBounds(x, pageRowY, 128, buttonHeight); x += 128 + gap;
    midiPageBtn_.setBounds(x, pageRowY, 128, buttonHeight);

    auto content = getLocalBounds().withTrimmedTop(160).reduced(margin, 8);
    if (currentPage_ == 0) {
        const int stageWidth = 360;
        stagePanel_.setBounds(content.removeFromLeft(stageWidth).reduced(0, 0));
        content.removeFromLeft(10);
        synthPanel_.setBounds(content);
    } else if (currentPage_ == 1) {
        synthPanel_.setBounds(content);
    } else if (currentPage_ == 2) {
        effectsPanel_.setBounds(content);
    } else {
        performancePanel_.setBounds(content);
    }
}
