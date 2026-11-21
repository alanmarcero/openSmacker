#include "WavDragButton.h"

namespace suite {

WavDragButton::WavDragButton(std::function<juce::File()> makeFile)
    : juce::TextButton("Drag WAV"), makeFile_(std::move(makeFile)) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTooltip("Drag the frozen hit out as a WAV. With no frozen hit, a fresh hit is rendered.");
}

void WavDragButton::mouseDown(const juce::MouseEvent& event) {
    dragging_ = false;
    juce::TextButton::mouseDown(event);
}

void WavDragButton::mouseDrag(const juce::MouseEvent& event) {
    if (dragging_ || event.getDistanceFromDragStart() < 8 || !makeFile_)
        return;
    dragging_ = true;
    const auto file = makeFile_();
    if (!file.existsAsFile())
        return;
    juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false, this);
}

void WavDragButton::mouseUp(const juce::MouseEvent& event) {
    dragging_ = false;
    juce::TextButton::mouseUp(event);
}

} // namespace suite
