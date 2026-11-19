#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <functional>

namespace suite {

// Two full sound snapshots. Switching stores the live sound in the slot you
// leave, then recalls the other slot. Both slots start as the same sound.
class AbCompare {
public:
    using Snapshot = juce::ValueTree;
    using Capture = std::function<Snapshot()>;
    using Restore = std::function<void(const Snapshot&)>;

    void arm(const Snapshot& state);
    [[nodiscard]] int active() const noexcept { return active_; }
    [[nodiscard]] bool isArmed() const noexcept { return armed_; }

    void select(int slot, const Capture& capture, const Restore& restore);
    void copy(int from, int to, const Capture& capture, const Restore& restore);

    void writeProperties(juce::ValueTree& dest) const;
    [[nodiscard]] bool readProperties(const juce::ValueTree& source);

private:
    void ensureArmed(const Capture& capture);
    [[nodiscard]] static int clampSlot(int slot) noexcept { return slot == 0 ? 0 : 1; }

    Snapshot slots_[2];
    int active_ = 0;
    bool armed_ = false;
};

} // namespace suite
