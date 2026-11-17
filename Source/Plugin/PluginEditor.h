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
    void setPage(int page);

private:
    void timerCallback() override;
    void syncPresetBox();
    void selectCategory(int categoryIndex);

    OpenSmackerProcessor& processor_;
    opensmacker::ui::EdmLookAndFeel lookAndFeel_;
    juce::Image faceplate_;

    juce::TextButton snareCatBtn_{ "Snares" };
    juce::TextButton bellCatBtn_{ "Bells & Chimes" };
    juce::TextButton tomCatBtn_{ "Toms" };
    juce::ComboBox presetBox_;
    juce::TextButton prevPresetBtn_{ "Previous preset" };
    juce::TextButton nextPresetBtn_{ "Next preset" };
    juce::TextButton viewToggleBtn_{ "SIMPLE VIEW" };
    juce::TextButton hitPageBtn_{ "Hit" };
    juce::TextButton layersPageBtn_{ "Layers" };
    juce::TextButton fxPageBtn_{ "Effects" };
    juce::TextButton midiPageBtn_{ "MIDI" };
    int currentPage_ = 0;

    opensmacker::ui::SmackerStagePanel stagePanel_;
    opensmacker::ui::SmackSynthPanel synthPanel_;
    opensmacker::ui::EffectsPanel effectsPanel_;
    opensmacker::ui::PerformancePanel performancePanel_;
    juce::TooltipWindow tooltipWindow_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSmackerEditor)
};
