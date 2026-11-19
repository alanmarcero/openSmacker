#include "AbCompare.h"

namespace suite {
namespace {
juce::ValueTree treeFromXml(const juce::String& text) {
    if (auto xml = juce::parseXML(text))
        return juce::ValueTree::fromXml(*xml);
    return {};
}
}

void AbCompare::arm(const Snapshot& state) {
    if (!state.isValid())
        return;
    slots_[0] = state.createCopy();
    slots_[1] = state.createCopy();
    active_ = 0;
    armed_ = true;
}

void AbCompare::ensureArmed(const Capture& capture) {
    if (!armed_ && capture)
        arm(capture());
}

void AbCompare::select(int slot, const Capture& capture, const Restore& restore) {
    ensureArmed(capture);
    slot = clampSlot(slot);
    if (!armed_ || slot == active_)
        return;
    if (capture)
        slots_[active_] = capture().createCopy();
    active_ = slot;
    if (restore && slots_[active_].isValid())
        restore(slots_[active_]);
}

void AbCompare::copy(int from, int to, const Capture& capture, const Restore& restore) {
    ensureArmed(capture);
    from = clampSlot(from);
    to = clampSlot(to);
    if (!armed_ || from == to)
        return;
    if (active_ == from && capture)
        slots_[from] = capture().createCopy();
    if (!slots_[from].isValid())
        return;
    slots_[to] = slots_[from].createCopy();
    if (active_ == to && restore)
        restore(slots_[to]);
}

void AbCompare::writeProperties(juce::ValueTree& dest) const {
    dest.setProperty("abActive", active_, nullptr);
    if (!armed_)
        return;
    if (auto xml = slots_[0].createXml())
        dest.setProperty("abSlot0", xml->toString(), nullptr);
    if (auto xml = slots_[1].createXml())
        dest.setProperty("abSlot1", xml->toString(), nullptr);
}

bool AbCompare::readProperties(const juce::ValueTree& source) {
    if (!source.hasProperty("abSlot0") || !source.hasProperty("abSlot1"))
        return false;
    auto first = treeFromXml(source.getProperty("abSlot0").toString());
    auto second = treeFromXml(source.getProperty("abSlot1").toString());
    if (!first.isValid() || !second.isValid())
        return false;
    slots_[0] = std::move(first);
    slots_[1] = std::move(second);
    active_ = clampSlot(int(source.getProperty("abActive", 0)));
    armed_ = true;
    return true;
}

} // namespace suite
