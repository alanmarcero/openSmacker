#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenSmackerEditor : public juce::AudioProcessorEditor,
                          private juce::Timer {
public:
    explicit OpenSmackerEditor(OpenSmackerProcessor& processorToEdit);
    ~OpenSmackerEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedView(bool advanced);

private:
    void timerCallback() override;
    void syncPresetBox();
    void selectCategory(int categoryIndex);

    OpenSmackerProcessor& processor_;
    opensmacker::ui::EdmLookAndFeel lookAndFeel_;
    juce::Image faceplate_;

    juce::TextButton snareCatBtn_{ "SN" };
    juce::TextButton bellCatBtn_{ "BL" };
    juce::TextButton tomCatBtn_{ "TM" };
    juce::ComboBox presetBox_;
    juce::TextButton prevPresetBtn_{ "<" };
    juce::TextButton nextPresetBtn_{ ">" };
    juce::TextButton viewToggleBtn_{ "SIMPLE" };

    opensmacker::ui::SmackerStagePanel stagePanel_;
    opensmacker::ui::SmackSynthPanel synthPanel_;
    opensmacker::ui::EffectsPanel effectsPanel_;
    juce::TooltipWindow tooltipWindow_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSmackerEditor)
};
