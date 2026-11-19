#include "ParameterUndo.h"

namespace suite {

class ParameterUndo::StateAction : public juce::UndoableAction {
public:
    StateAction(ParameterUndo& owner, Snapshot before, Snapshot after)
        : owner_(owner), before_(std::move(before)), after_(std::move(after)) {}

    bool perform() override {
        owner_.applySnapshot(after_);
        return true;
    }

    bool undo() override {
        owner_.applySnapshot(before_);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    ParameterUndo& owner_;
    Snapshot before_;
    Snapshot after_;
};

ParameterUndo::ParameterUndo(juce::AudioProcessor& processor, Capture capture, Apply apply)
    : processor_(processor), capture_(std::move(capture)), apply_(std::move(apply)) {
    for (auto* parameter : processor_.getParameters())
        if (parameter != nullptr)
            parameter->addListener(this);
    committed_ = capture_();
}

ParameterUndo::~ParameterUndo() {
    for (auto* parameter : processor_.getParameters())
        if (parameter != nullptr)
            parameter->removeListener(this);
}

bool ParameterUndo::undo() {
    if (!onMessageThread() || gestureDepth_ > 0)
        return false;
    flushPending();
    return undo_.undo();
}

bool ParameterUndo::redo() {
    if (!onMessageThread() || gestureDepth_ > 0)
        return false;
    flushPending();
    return undo_.redo();
}

bool ParameterUndo::canUndo() {
    flushPending();
    return undo_.canUndo();
}

bool ParameterUndo::canRedo() const { return undo_.canRedo(); }

void ParameterUndo::resetHistory() {
    const juce::ScopedValueSetter<bool> guard(applying_, true);
    undo_.clearUndoHistory();
    gestureDepth_ = 0;
    pending_ = false;
    committed_ = capture_();
}

void ParameterUndo::suppressDuring(const std::function<void()>& fn) {
    const juce::ScopedValueSetter<bool> guard(applying_, true);
    pending_ = false;
    if (fn)
        fn();
    undo_.clearUndoHistory();
    gestureDepth_ = 0;
    committed_ = capture_();
}

void ParameterUndo::parameterValueChanged(int, float) {
    if (!onMessageThread() || applying_ || gestureDepth_ > 0)
        return;
    pending_ = true;
}

void ParameterUndo::parameterGestureChanged(int, bool gestureIsStarting) {
    if (!onMessageThread() || applying_)
        return;
    if (gestureIsStarting) {
        if (gestureDepth_ == 0)
            flushPending();
        ++gestureDepth_;
        return;
    }
    if (gestureDepth_ <= 0)
        return;
    --gestureDepth_;
    if (gestureDepth_ == 0)
        commit();
}

void ParameterUndo::flushPending() {
    if (!pending_ || gestureDepth_ > 0)
        return;
    commit();
}

void ParameterUndo::commit() {
    if (applying_ || gestureDepth_ > 0)
        return;
    pending_ = false;
    auto now = capture_();
    if (!committed_.isValid() || committed_.isEquivalentTo(now))
        return;
    undo_.beginNewTransaction("Edit");
    undo_.perform(new StateAction(*this, committed_.createCopy(), now.createCopy()));
}

void ParameterUndo::applySnapshot(const Snapshot& tree) {
    const juce::ScopedValueSetter<bool> guard(applying_, true);
    if (apply_)
        apply_(tree);
    committed_ = capture_();
}

bool ParameterUndo::onMessageThread() const {
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    return manager != nullptr && manager->isThisTheMessageThread();
}

} // namespace suite
