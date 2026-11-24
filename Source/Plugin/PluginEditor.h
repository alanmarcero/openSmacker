#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"
#include "SuiteBar.h"
#include <juce_gui_basics/juce_gui_basics.h>

class OpenSmackerEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OpenSmackerEditor(OpenSmackerProcessor& processorToEdit);
    ~OpenSmackerEditor() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void setAdvancedView(bool advanced);
    void setPage(int page);
    [[nodiscard]] juce::Rectangle<int> drumheadBounds() const noexcept { return artBounds_; }

private:
    class ShortcutKeys;
    class Content : public juce::Component {
    public:
        explicit Content(OpenSmackerEditor& editor) : editor_(editor) {}
        void paint(juce::Graphics& g) override { editor_.paintContent(g); }
        void resized() override { editor_.layoutContent(); }
    private:
        OpenSmackerEditor& editor_;
    };

    void paintContent(juce::Graphics& g);
    void layoutContent();
    void timerCallback() override;
    bool handleShortcut(const juce::KeyPress& key);
    void syncPresetBox();
    void syncSuiteButtons();
    void syncLockBadges();
    void chooseProgram(int program);
    void openPresets();

    OpenSmackerProcessor& processor_;
    opensmacker::ui::EdmLookAndFeel lookAndFeel_;
    Content content_{ *this };
    std::unique_ptr<juce::Drawable> membrane_;
    std::array<juce::TextButton, 4> pages_;
    opensmacker::ui::PopupComboBox presetBox_;
    std::unique_ptr<opensmacker::ui::PadlockBadge> presetLockBadge_;
    juce::TextButton prevPresetBtn_{ "<" }, nextPresetBtn_{ ">" };
    juce::TextButton auditionBtn_{ "Audition hit" }, viewToggleBtn_{ "Advanced controls" };
    suite::SuiteBar suiteBar_;
    opensmacker::ui::HitMonitor hitMonitor_;
    int currentPage_ = 0;
    juce::Rectangle<int> artBounds_;
    juce::Rectangle<int> versionBounds_;
    juce::Rectangle<int> editLabelBounds_;
    bool persistEditorSize_ = false;
    opensmacker::ui::SmackerStagePanel stagePanel_;
    opensmacker::ui::SmackSynthPanel synthPanel_;
    opensmacker::ui::EffectsPanel effectsPanel_;
    opensmacker::ui::PerformancePanel performancePanel_;
    juce::TooltipWindow tooltipWindow_;
    std::unique_ptr<ShortcutKeys> shortcuts_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSmackerEditor)
};
