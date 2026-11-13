#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <utility>
#include <vector>

namespace opensmacker::presets {

struct FactoryPreset {
    const char* name;
    std::vector<std::pair<juce::String, float>> values;
};

enum class Section : size_t {
    VoiceTuning = 0,
    PreStrike,
    WiresAir,
    Transient,
    SaturationDynamics,
    SpaceOutput,
    Count
};

using SectionLocks = std::array<bool, static_cast<size_t>(Section::Count)>;

const std::vector<FactoryPreset>& factoryPresets();
void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state,
           const SectionLocks& locks = {}, bool preservePitch = false);

} // namespace opensmacker::presets
