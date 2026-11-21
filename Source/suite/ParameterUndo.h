#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>

namespace suite {

// Groups parameter edits into single undo steps. Gesture begin/end (slider drags,
// button clicks) is one step. Other message-thread edits are held and committed
// together the next time undo state is queried or a new gesture starts, so a
// preset load becomes one step. Each A/B slot keeps its own history.
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    using Snapshot = juce::ValueTree;
    using Capture = std::function<Snapshot()>;
    using Apply = std::function<void(const Snapshot&)>;

    ParameterUndo(juce::AudioProcessor& processor, Capture capture, Apply apply);
    ~ParameterUndo() override;

    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo();
    [[nodiscard]] bool canRedo() const;

    void resetHistory(int activeSlot = 0);
    void suppressDuring(const std::function<void()>& fn);
    void switchSlot(int slot, const std::function<void()>& applySound);
    void clearSlot(int slot);

private:
    class StateAction;

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    void flushPending();
    void commit();
    void applySnapshot(const Snapshot& tree);
    [[nodiscard]] bool onMessageThread() const;

    juce::AudioProcessor& processor_;
    Capture capture_;
    Apply apply_;
    std::array<juce::UndoManager, 2> histories_;
    size_t activeSlot_ = 0;
    Snapshot committed_;
    int gestureDepth_ = 0;
    bool applying_ = false;
    bool pending_ = false;
};

} // namespace suite
