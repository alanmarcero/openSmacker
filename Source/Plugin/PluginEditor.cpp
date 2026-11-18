#include "PluginEditor.h"
#include "Theme.h"
#include "BinaryData.h"

using namespace opensmacker;
namespace {
constexpr std::array<const char*, 4> pageNames{ "Hit", "Layers", "Effects", "MIDI" };
constexpr int railWidth = 194;
const juce::Rectangle<int> artBounds{ 22, 525, 150, 71 };
void markSelected(juce::TextButton& button, bool selected) {
    button.setToggleState(selected, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonOnColourId, theme::kLilac);
    button.setColour(juce::TextButton::textColourOnId, theme::kInk);
}
}

OpenSmackerEditor::OpenSmackerEditor(OpenSmackerProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processor_(processorToEdit), stagePanel_(processorToEdit),
      synthPanel_(processorToEdit), effectsPanel_(processorToEdit), performancePanel_(processorToEdit),
      tooltipWindow_(this, 500) {
    setLookAndFeel(&lookAndFeel_);
    membrane_ = juce::Drawable::createFromImageData(BinaryData::membrane_svg, BinaryData::membrane_svgSize);
    const auto& bank = presets::factoryPresets();
    juce::String style;
    juce::PopupMenu sounds;
    int presetId = 1;
    std::ranges::for_each(bank, [&](const auto& preset) {
        const juce::String name(preset.name);
        const auto nextStyle = name.upToFirstOccurrenceOf(" / ", false, false);
        if (style != nextStyle && sounds.getNumItems() > 0) {
            presetBox_.getRootMenu()->addSubMenu(style, sounds);
            sounds.clear();
        }
        style = nextStyle;
        sounds.addItem(presetId++, name);
    });
    if (sounds.getNumItems() > 0)
        presetBox_.getRootMenu()->addSubMenu(style, sounds);
    for (size_t i = 0; i < families_.size(); ++i) {
        const auto first = presets::firstPresetForFamily(i);
        auto& family = families_[i];
        family.setButtonText(presets::kFamilyNames[i]);
        family.setTooltip("Load the first preset in this voice.");
        family.onClick = [this, first] { chooseProgram(first); };
        addAndMakeVisible(family);
        pages_[i].setButtonText(pageNames[i]);
        pages_[i].onClick = [this, i] { setPage(static_cast<int>(i)); };
        addAndMakeVisible(pages_[i]);
    }
    presetBox_.setName("Factory preset");
    presetBox_.setTooltip("Choose a factory preset across six dance styles.");
    presetBox_.setTextWhenNothingSelected("Custom sound");
    presetBox_.onChange = [this] { chooseProgram(presetBox_.getSelectedId() - 1); };
    prevPresetBtn_.onClick = [this] { chooseProgram((processor_.getCurrentProgram() + processor_.getNumPrograms() - 1) % processor_.getNumPrograms()); };
    nextPresetBtn_.onClick = [this] { chooseProgram((processor_.getCurrentProgram() + 1) % processor_.getNumPrograms()); };
    auditionBtn_.onClick = [this] { processor_.triggerAudition(); };
    auditionBtn_.setColour(juce::TextButton::buttonColourId, theme::kApricot);
    auditionBtn_.setColour(juce::TextButton::textColourOffId, theme::kInk);
    viewToggleBtn_.onClick = [this] { setAdvancedView(!synthPanel_.isAdvancedMode()); };
    for (auto* child : std::initializer_list<juce::Component*>{ &presetBox_, &prevPresetBtn_, &nextPresetBtn_,
             &auditionBtn_, &viewToggleBtn_, &stagePanel_, &synthPanel_, &effectsPanel_, &performancePanel_ })
        addAndMakeVisible(child);
    setSize(1120, 704);
    setAdvancedView(false);
    setPage(0);
    syncPresetBox();
    startTimerHz(30);
}

OpenSmackerEditor::~OpenSmackerEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void OpenSmackerEditor::chooseProgram(int program) {
    if (program < 0) return;
    processor_.setCurrentProgram(program);
    syncPresetBox();
}

void OpenSmackerEditor::setAdvancedView(bool advanced) {
    synthPanel_.setAdvancedMode(advanced);
    effectsPanel_.setAdvancedMode(advanced);
    performancePanel_.setAdvancedMode(advanced);
    markSelected(viewToggleBtn_, advanced);
    resized();
}

void OpenSmackerEditor::setPage(int page) {
    currentPage_ = juce::jlimit(0, 3, page);
    for (size_t i = 0; i < pages_.size(); ++i)
        markSelected(pages_[i], static_cast<int>(i) == currentPage_);
    synthPanel_.setPage(currentPage_ == 1 ? 1 : 0);
    stagePanel_.setVisible(currentPage_ == 0);
    synthPanel_.setVisible(currentPage_ <= 1);
    effectsPanel_.setVisible(currentPage_ == 2);
    performancePanel_.setVisible(currentPage_ == 3);
    resized();
}

void OpenSmackerEditor::syncPresetBox() {
    const auto program = processor_.getCurrentProgram();
    const auto custom = processor_.isRestoredCustomSound();
    presetBox_.setSelectedId(custom ? 0 : program + 1, juce::dontSendNotification);
    for (size_t i = 0; i < families_.size(); ++i)
        markSelected(families_[i], !custom && i == presets::familyForProgram(program));
}

void OpenSmackerEditor::timerCallback() {
    repaint(artBounds);
    const auto selected = processor_.isRestoredCustomSound() ? 0 : processor_.getCurrentProgram() + 1;
    if (presetBox_.getSelectedId() != selected)
        syncPresetBox();
}

void OpenSmackerEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    g.setColour(theme::kInk);
    g.fillRect(0, 0, getWidth(), 83);
    g.fillRect(0, 83, railWidth, getHeight() - 83);
    g.setColour(theme::kPanelEdge);
    g.drawHorizontalLine(82, 0, static_cast<float>(getWidth()));
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    g.drawText("openSmacker", 18, 9, 196, 39, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(14.0f));
    g.setColour(theme::kTextDim);
    g.drawText("Trance percussion synth", 20, 47, 180, 19, juce::Justification::centredLeft);
    g.drawText("Factory preset", 220, 6, 200, 20, juce::Justification::centredLeft);
    g.drawText("Edit", 18, 102, 150, 19, juce::Justification::centredLeft);
    g.drawText("Voices", 18, 327, 150, 19, juce::Justification::centredLeft);
    g.drawText("v" JucePlugin_VersionString, 18, 656, 150, 20, juce::Justification::centredLeft);
    if (membrane_ && g.clipRegionIntersects(artBounds)) {
        const float phase = processor_.animationBeatPhase(juce::Time::getMillisecondCounterHiRes() * 0.001);
        const float pulse = std::exp(-6.0f * phase);
        const auto centre = artBounds.getCentre().toFloat();
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(1.0f + pulse * 0.05f, 1.0f - pulse * 0.18f, centre.x, centre.y));
        membrane_->drawWithin(g, artBounds.toFloat().reduced(6, 3), juce::RectanglePlacement::centred, 0.76f + pulse * 0.24f);
    }
}

void OpenSmackerEditor::resized() {
    auto row = getLocalBounds().withTrimmedLeft(220).withTrimmedRight(18).withY(31).withHeight(39);
    auditionBtn_.setBounds(row.removeFromRight(159));
    row.removeFromRight(18);
    nextPresetBtn_.setBounds(row.removeFromRight(80));
    row.removeFromRight(6);
    prevPresetBtn_.setBounds(row.removeFromRight(88));
    row.removeFromRight(8);
    presetBox_.setBounds(row);
    for (size_t i = 0; i < pages_.size(); ++i) {
        pages_[i].setBounds(16, 128 + static_cast<int>(i) * 43, railWidth - 32, 37);
        families_[i].setBounds(16, 352 + static_cast<int>(i) * 43, railWidth - 32, 37);
    }
    viewToggleBtn_.setBounds(16, 603, railWidth - 32, 40);
    auto area = getLocalBounds().withTrimmedTop(83).withTrimmedLeft(railWidth).reduced(14);
    if (currentPage_ == 0) {
        stagePanel_.setBounds(area.removeFromTop(88));
        area.removeFromTop(6);
    }
    synthPanel_.setBounds(area);
    effectsPanel_.setBounds(area);
    performancePanel_.setBounds(area);
}
