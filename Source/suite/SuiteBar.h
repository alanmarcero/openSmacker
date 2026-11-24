#pragma once

#include "WavDragButton.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace suite {

/** Compact undo, A/B, preset browser, and WAV drag controls. */
class SuiteBar : public juce::Component {
public:
    explicit SuiteBar(std::function<juce::File()> makeFile);
    void resized() override;
    void setAbSlotB(bool editingB);
    void setUndoEnabled(bool canUndo, bool canRedo);
    void setPresetsOpen(bool open);

    std::function<void()> onUndo, onRedo, onShowA, onShowB, onCopyAToB, onCopyBToA, onPresets;

private:
    juce::TextButton undo_{ "Undo" }, redo_{ "Redo" };
    juce::TextButton a_{ "A" }, b_{ "B" }, aToB_{ "A to B" }, bToA_{ "B to A" }, presets_{ "Presets" };
    WavDragButton drag_;
};

} // namespace suite
