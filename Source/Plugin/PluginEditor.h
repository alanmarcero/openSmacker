#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenSmackerEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OpenSmackerEditor(OpenSmackerProcessor& processorToEdit);
    ~OpenSmackerEditor() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedView(bool advanced);
    void setPage(int page);

private:
    void timerCallback() override;
    void syncPresetBox();
    void chooseProgram(int program);

    OpenSmackerProcessor& processor_;
    opensmacker::ui::EdmLookAndFeel lookAndFeel_;
    std::unique_ptr<juce::Drawable> membrane_;
    std::array<juce::TextButton, 4> families_, pages_;
    juce::ComboBox presetBox_;
    juce::TextButton prevPresetBtn_{ "Previous" }, nextPresetBtn_{ "Next" };
    juce::TextButton auditionBtn_{ "Audition hit" }, viewToggleBtn_{ "Advanced controls" };
    int currentPage_ = 0;
    opensmacker::ui::SmackerStagePanel stagePanel_;
    opensmacker::ui::SmackSynthPanel synthPanel_;
    opensmacker::ui::EffectsPanel effectsPanel_;
    opensmacker::ui::PerformancePanel performancePanel_;
    juce::TooltipWindow tooltipWindow_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSmackerEditor)
};
