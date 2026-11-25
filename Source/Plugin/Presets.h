#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <utility>
#include <vector>

namespace opensmacker::presets {

// Clap, hat, kick, and snare banks are 56 programs.
inline constexpr int kFactoryProgramCount = 56;

struct FactoryPreset {
    const char* name;
    std::vector<std::pair<juce::String, float>> values;
};

// Sessions save locks by index. Add new sections at the end.
enum class Section : size_t {
    VoiceTuning = 0,
    PreStrike,
    WiresAir,
    Transient,
    SaturationDynamics,
    Space,
    Envelope,
    Output,
    BodyDetail,
    Count
};

using SectionLocks = std::array<bool, static_cast<size_t>(Section::Count)>;

const std::vector<FactoryPreset>& factoryPresets();
int firstPresetForFamily(size_t family) noexcept;
size_t familyForProgram(int program) noexcept;
void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state,
           const SectionLocks& locks = {},
           const std::function<bool(const juce::String&)>& isParamLocked = {});

} // namespace opensmacker::presets
