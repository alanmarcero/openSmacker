#include "SuiteBar.h"

namespace suite {

SuiteBar::SuiteBar(std::function<juce::File()> makeFile)
    : drag_(std::move(makeFile)) {
    undo_.setTooltip("Undo the last parameter change. Shortcut: Cmd/Ctrl+Z.");
    redo_.setTooltip("Redo the last undone change. Shortcut: Shift+Cmd/Ctrl+Z.");
    a_.setTooltip("Hear slot A.");
    b_.setTooltip("Hear slot B.");
    aToB_.setTooltip("Copy slot A onto slot B.");
    bToA_.setTooltip("Copy slot B onto slot A.");
    presets_.setTooltip("Browse factory and user presets.");
    undo_.setTitle("Undo");
    redo_.setTitle("Redo");
    a_.setTitle("Slot A");
    b_.setTitle("Slot B");
    aToB_.setTitle("Copy A to B");
    bToA_.setTitle("Copy B to A");
    presets_.setTitle("Presets");
    drag_.setTitle("Drag WAV");
    undo_.onClick = [this] { if (onUndo) onUndo(); };
    redo_.onClick = [this] { if (onRedo) onRedo(); };
    a_.onClick = [this] { if (onShowA) onShowA(); };
    b_.onClick = [this] { if (onShowB) onShowB(); };
    aToB_.onClick = [this] { if (onCopyAToB) onCopyAToB(); };
    bToA_.onClick = [this] { if (onCopyBToA) onCopyBToA(); };
    presets_.onClick = [this] { if (onPresets) onPresets(); };
    for (auto* button : { &undo_, &redo_, &a_, &b_, &aToB_, &bToA_, &presets_, static_cast<juce::TextButton*>(&drag_) })
        addAndMakeVisible(*button);
    setAbSlotB(false);
    setUndoEnabled(false, false);
}

void SuiteBar::setAbSlotB(bool editingB) {
    a_.setToggleState(!editingB, juce::dontSendNotification);
    b_.setToggleState(editingB, juce::dontSendNotification);
}

void SuiteBar::setUndoEnabled(bool canUndo, bool canRedo) {
    undo_.setEnabled(canUndo);
    redo_.setEnabled(canRedo);
}

void SuiteBar::setPresetsOpen(bool open) {
    presets_.setToggleState(open, juce::dontSendNotification);
}

void SuiteBar::resized() {
    auto area = getLocalBounds();
    auto takeLeft = [&](juce::Component& component, int width) {
        component.setBounds(area.removeFromLeft(width));
        area.removeFromLeft(4);
    };
    takeLeft(undo_, 64);
    takeLeft(redo_, 64);
    area.removeFromLeft(6);
    takeLeft(a_, 40);
    takeLeft(b_, 40);
    takeLeft(aToB_, 64);
    takeLeft(bToA_, 64);
    area.removeFromLeft(6);
    takeLeft(presets_, 88);
    drag_.setBounds(area.removeFromLeft(108));
}

} // namespace suite
