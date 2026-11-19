#include "PluginEditor.h"
#include "EditorSizing.h"
#include "PresetBrowser.h"
#include "PresetStore.h"
#include "Theme.h"
#include "BinaryData.h"

using namespace opensmacker;
namespace {
constexpr std::array<const char*, 4> pageNames{ "Hit", "Layers", "Effects", "MIDI" };
constexpr int railWidth = 194;
constexpr int headerHeight = 131;

bool isUndoKey(const juce::KeyPress& key, bool wantRedo) {
    const auto code = juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(key.getKeyCode()));
    if (code != 'z' || !key.getModifiers().isCommandDown())
        return false;
    return key.getModifiers().isShiftDown() == wantRedo;
}

void markSelected(juce::TextButton& button, bool selected) {
    button.setToggleState(selected, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonOnColourId, theme::kLilac);
    button.setColour(juce::TextButton::textColourOnId, theme::kInk);
}

suite::Style suiteStyle() {
    return {
        theme::kBackground, theme::kPanel, theme::kInk, theme::kText,
        theme::kTextDim, theme::kApricot, theme::kPanelEdge
    };
}
}

class OpenSmackerEditor::ShortcutKeys : public juce::KeyListener {
public:
    explicit ShortcutKeys(OpenSmackerEditor& editor) : editor_(editor) {}
    bool keyPressed(const juce::KeyPress& key, juce::Component* originating) override {
        if (dynamic_cast<juce::TextEditor*>(originating) != nullptr)
            return false;
        return editor_.handleShortcut(key);
    }
private:
    OpenSmackerEditor& editor_;
};

OpenSmackerEditor::OpenSmackerEditor(OpenSmackerProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processor_(processorToEdit),
      dragBtn_([this] { return processor_.createHitWavFile(); }),
      stagePanel_(processorToEdit), synthPanel_(processorToEdit),
      effectsPanel_(processorToEdit), performancePanel_(processorToEdit),
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
    undoBtn_.onClick = [this] { processor_.undo(); syncSuiteButtons(); };
    redoBtn_.onClick = [this] { processor_.redo(); syncSuiteButtons(); };
    undoBtn_.setTooltip("Undo the last parameter edit (Cmd/Ctrl+Z).");
    redoBtn_.setTooltip("Redo the last undone edit (Shift+Cmd/Ctrl+Z).");
    abABtn_.onClick = [this] { processor_.selectAbSlot(0); syncSuiteButtons(); };
    abBBtn_.onClick = [this] { processor_.selectAbSlot(1); syncSuiteButtons(); };
    abABtn_.setTooltip("Hear slot A.");
    abBBtn_.setTooltip("Hear slot B.");
    copyAtoBBtn_.onClick = [this] { processor_.copyAbSlot(0, 1); syncSuiteButtons(); };
    copyBtoABtn_.onClick = [this] { processor_.copyAbSlot(1, 0); syncSuiteButtons(); };
    copyAtoBBtn_.setTooltip("Copy slot A onto slot B.");
    copyBtoABtn_.setTooltip("Copy slot B onto slot A.");
    presetsBtn_.onClick = [this] { openPresets(); };
    presetsBtn_.setTooltip("Factory and user presets. Search, star favorites, and save your own.");
    dragBtn_.setColour(juce::TextButton::buttonColourId, theme::kIce);
    dragBtn_.setColour(juce::TextButton::textColourOffId, theme::kInk);
    for (auto* child : std::initializer_list<juce::Component*>{
             &presetBox_, &prevPresetBtn_, &nextPresetBtn_, &auditionBtn_, &viewToggleBtn_,
             &undoBtn_, &redoBtn_, &abABtn_, &abBBtn_, &copyAtoBBtn_, &copyBtoABtn_, &presetsBtn_, &dragBtn_,
             &stagePanel_, &synthPanel_, &effectsPanel_, &performancePanel_ })
        addAndMakeVisible(child);
    setWantsKeyboardFocus(true);
    shortcuts_ = std::make_unique<ShortcutKeys>(*this);
    addKeyListener(shortcuts_.get());
    setResizeLimits(suite::kMinEditorWidth, suite::kMinEditorHeight, suite::kMaxEditorWidth, suite::kMaxEditorHeight);
    setResizable(true, true);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(0.0);
    const int savedWidth = processor_.editorWidth();
    const int savedHeight = processor_.editorHeight();
    setSize(juce::jlimit(suite::kMinEditorWidth, suite::kMaxEditorWidth, savedWidth),
            juce::jlimit(suite::kMinEditorHeight, suite::kMaxEditorHeight, savedHeight));
    persistEditorSize_ = true;
    processor_.setEditorSize(getWidth(), getHeight());
    setAdvancedView(false);
    setPage(0);
    syncPresetBox();
    syncSuiteButtons();
    startTimerHz(30);
}

OpenSmackerEditor::~OpenSmackerEditor() {
    stopTimer();
    if (shortcuts_ != nullptr)
        removeKeyListener(shortcuts_.get());
    setLookAndFeel(nullptr);
}

bool OpenSmackerEditor::keyPressed(const juce::KeyPress& key) {
    return handleShortcut(key);
}

bool OpenSmackerEditor::handleShortcut(const juce::KeyPress& key) {
    if (isUndoKey(key, true)) {
        processor_.redo();
        syncSuiteButtons();
        return true;
    }
    if (isUndoKey(key, false)) {
        processor_.undo();
        syncSuiteButtons();
        return true;
    }
    return false;
}

void OpenSmackerEditor::chooseProgram(int program) {
    if (program < 0) return;
    processor_.setCurrentProgram(program);
    syncPresetBox();
}

void OpenSmackerEditor::openPresets() {
    std::vector<suite::PresetBrowser::FactoryPreset> factory;
    factory.reserve(static_cast<size_t>(processor_.getNumPrograms()));
    for (int i = 0; i < processor_.getNumPrograms(); ++i)
        factory.push_back({ processor_.getProgramName(i), i });
    auto browser = std::make_unique<suite::PresetBrowser>(
        processor_.presetStore(), std::move(factory),
        [this](int index) { chooseProgram(index); },
        [this](const juce::String& xml) {
            if (processor_.loadUserPresetXml(xml))
                syncPresetBox();
        },
        [this] { return processor_.currentPresetXml(); },
        suiteStyle());
    juce::CallOutBox::launchAsynchronously(std::move(browser), presetsBtn_.getScreenBounds(), this);
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

void OpenSmackerEditor::syncSuiteButtons() {
    undoBtn_.setEnabled(processor_.canUndo());
    redoBtn_.setEnabled(processor_.canRedo());
    markSelected(abABtn_, processor_.abSlot() == 0);
    markSelected(abBBtn_, processor_.abSlot() == 1);
}

void OpenSmackerEditor::timerCallback() {
    if (!artBounds_.isEmpty())
        repaint(artBounds_);
    const auto selected = processor_.isRestoredCustomSound() ? 0 : processor_.getCurrentProgram() + 1;
    if (presetBox_.getSelectedId() != selected)
        syncPresetBox();
    if (undoBtn_.isEnabled() != processor_.canUndo() || redoBtn_.isEnabled() != processor_.canRedo()
        || abABtn_.getToggleState() != (processor_.abSlot() == 0))
        syncSuiteButtons();
}

void OpenSmackerEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    g.setColour(theme::kInk);
    g.fillRect(0, 0, getWidth(), headerHeight);
    g.fillRect(0, headerHeight, railWidth, getHeight() - headerHeight);
    g.setColour(theme::kPanelEdge);
    g.drawHorizontalLine(headerHeight - 1, 0, static_cast<float>(getWidth()));
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
    g.drawText("openSmacker", 18, 2, 196, 32, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(13.0f));
    g.setColour(theme::kTextDim);
    g.drawText("Trance percussion synth", 20, 32, 190, 16, juce::Justification::centredLeft);
    g.drawText("Factory preset", 220, 6, 200, 18, juce::Justification::centredLeft);
    if (!editLabelBounds_.isEmpty())
        g.drawText("Edit", editLabelBounds_, juce::Justification::centredLeft);
    if (!voicesLabelBounds_.isEmpty())
        g.drawText("Voices", voicesLabelBounds_, juce::Justification::centredLeft);
    if (!versionBounds_.isEmpty())
        g.drawText("v" JucePlugin_VersionString, versionBounds_, juce::Justification::centredLeft);
    if (membrane_ && !artBounds_.isEmpty() && g.clipRegionIntersects(artBounds_)) {
        const float phase = processor_.animationBeatPhase(juce::Time::getMillisecondCounterHiRes() * 0.001);
        const float pulse = std::exp(-6.0f * phase);
        const auto centre = artBounds_.getCentre().toFloat();
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(1.0f + pulse * 0.05f, 1.0f - pulse * 0.18f, centre.x, centre.y));
        membrane_->drawWithin(g, artBounds_.toFloat().reduced(6, 3), juce::RectanglePlacement::centred, 0.76f + pulse * 0.24f);
    }
}

void OpenSmackerEditor::resized() {
    const int width = juce::jlimit(suite::kMinEditorWidth, suite::kMaxEditorWidth, getWidth());
    const int height = juce::jlimit(suite::kMinEditorHeight, suite::kMaxEditorHeight, getHeight());
    if (width != getWidth() || height != getHeight()) {
        setSize(width, height);
        return;
    }
    auto top = getLocalBounds().withTrimmedLeft(220).withTrimmedRight(18).withY(26).withHeight(39);
    auditionBtn_.setBounds(top.removeFromRight(159));
    top.removeFromRight(12);
    nextPresetBtn_.setBounds(top.removeFromRight(80));
    top.removeFromRight(6);
    prevPresetBtn_.setBounds(top.removeFromRight(88));
    top.removeFromRight(8);
    presetBox_.setBounds(top);

    auto suite = getLocalBounds().withTrimmedLeft(16).withTrimmedRight(18).withY(82).withHeight(36);
    auto take = [&suite](juce::Component& component, int slotWidth) {
        component.setBounds(suite.removeFromLeft(slotWidth));
        suite.removeFromLeft(6);
    };
    take(undoBtn_, 64);
    take(redoBtn_, 64);
    suite.removeFromLeft(8);
    take(abABtn_, 44);
    take(abBBtn_, 44);
    take(copyAtoBBtn_, 56);
    take(copyBtoABtn_, 56);
    suite.removeFromLeft(8);
    take(presetsBtn_, 88);
    suite.removeFromLeft(8);
    dragBtn_.setBounds(suite.removeFromLeft(108));

    auto rail = getLocalBounds().withTrimmedTop(headerHeight).withWidth(railWidth).reduced(16, 8);
    versionBounds_ = rail.removeFromBottom(20);
    rail.removeFromBottom(6);
    viewToggleBtn_.setBounds(rail.removeFromBottom(40));
    rail.removeFromBottom(8);
    artBounds_ = rail.removeFromBottom(71).withSizeKeepingCentre(150, 71);
    rail.removeFromBottom(8);
    editLabelBounds_ = rail.removeFromTop(19);
    rail.removeFromTop(6);
    for (auto& page : pages_) {
        page.setBounds(rail.removeFromTop(37));
        rail.removeFromTop(6);
    }
    rail.removeFromTop(8);
    voicesLabelBounds_ = rail.removeFromTop(19);
    rail.removeFromTop(6);
    for (auto& family : families_) {
        family.setBounds(rail.removeFromTop(37));
        rail.removeFromTop(6);
    }

    auto area = getLocalBounds().withTrimmedTop(headerHeight).withTrimmedLeft(railWidth).reduced(14);
    if (currentPage_ == 0) {
        stagePanel_.setBounds(area.removeFromTop(88));
        area.removeFromTop(6);
    }
    synthPanel_.setBounds(area);
    effectsPanel_.setBounds(area);
    performancePanel_.setBounds(area);
    AudioProcessorEditor::resized();
    if (persistEditorSize_)
        processor_.setEditorSize(getWidth(), getHeight());
}
