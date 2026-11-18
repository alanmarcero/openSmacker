#include "Presets.h"
#include "Parameters.h"

namespace opensmacker::presets {
using namespace opensmacker::params;
namespace {
struct Body {
    int midiNote, decayMs, shellHz, pitchSweep;
    float fm;
};
struct Wires {
    int decayMs;
    float level;
};
struct Strike {
    core::ClickType type;
    int hz;
};
struct Patch {
    const char* name;
    size_t family;
    Body body;
    Wires wires;
    Strike strike;
    float room;
    int strikes = 1;
};
using enum core::ClickType;

// Body: MIDI root, decay (ms), shell (Hz), pitch sweep (st), FM depth.
// Wires: decay (ms), level. Strike: exciter and frequency (Hz).
// Family: 0 Snare, 1 Bell & chime, 2 Tom, 3 Rim smack.
constexpr std::array<Patch, 84> bank{{
    // Uplifting (24: 6 Snares, 6 Bells, 6 Toms, 6 Rims)
    { "Uplifting / Backbeat Snare",    0, { 55, 116, 218, 9, 0.18f }, { 108, 0.74f }, { Stick, 3700 }, 0.04f },
    { "Uplifting / Euphoria Snare",    0, { 57, 124, 260, 10, 0.22f }, { 128, 0.78f }, { Stick, 4400 }, 0.06f },
    { "Uplifting / Silver Mesh Snare", 0, { 59, 111, 394, 8, 0.30f }, { 126, 0.82f }, { Stick, 5500 }, 0.03f },
    { "Uplifting / Skyward Snare",     0, { 58, 118, 315, 11, 0.24f }, { 134, 0.80f }, { Rimshot, 4700 }, 0.05f },
    { "Uplifting / Pure 138 Snare",    0, { 56, 108, 280, 9, 0.19f }, { 114, 0.75f }, { Stick, 4100 }, 0.04f },
    { "Uplifting / Wide Backhand Snare", 0, { 54, 136, 235, 12, 0.25f }, { 143, 0.83f }, { Rimshot, 3650 }, 0.08f },
    { "Uplifting / Glass Pin Bell",    1, { 76, 184, 1450, 0, 0.43f }, { 26, 0.035f }, { Mallet, 3100 }, 0.02f },
    { "Uplifting / Cut Crystal Bell",  1, { 81, 179, 2160, 0, 0.32f }, { 18, 0.02f }, { Stick, 5300 }, 0.03f },
    { "Uplifting / Arc Light Chime",   1, { 78, 190, 1980, 2, 0.74f }, { 30, 0.04f }, { Laser, 4700 }, 0.03f },
    { "Uplifting / Blue Glass Bell",   1, { 74, 208, 1350, 0, 0.35f }, { 28, 0.025f }, { Mallet, 2850 }, 0.05f },
    { "Uplifting / After Ring Chime",  1, { 77, 249, 1620, 0, 0.58f }, { 41, 0.035f }, { Mallet, 3350 }, 0.08f },
    { "Uplifting / Morning Chime",     1, { 75, 195, 1720, 1, 0.40f }, { 29, 0.03f }, { Mallet, 3800 }, 0.05f },
    { "Uplifting / Anthem Tom",        2, { 45, 146, 168, 9, 0.14f }, { 48, 0.13f }, { Stick, 2600 }, 0.04f },
    { "Uplifting / Lift Fill Tom",     2, { 52, 123, 234, 9, 0.20f }, { 47, 0.15f }, { Stick, 3270 }, 0.03f, 2 },
    { "Uplifting / Upper Rack Tom",    2, { 53, 113, 237, 6, 0.20f }, { 46, 0.14f }, { Stick, 3350 }, 0.03f },
    { "Uplifting / High Skin Tom",     2, { 55, 104, 280, 7, 0.21f }, { 40, 0.15f }, { Rimshot, 2950 }, 0.02f },
    { "Uplifting / Horizon Tom",       2, { 48, 158, 192, 8, 0.15f }, { 52, 0.12f }, { Mallet, 2300 }, 0.05f },
    { "Uplifting / Starlight Tom",     2, { 50, 134, 214, 10, 0.18f }, { 45, 0.14f }, { Stick, 3050 }, 0.04f },
    { "Uplifting / Mainstage Rim",     3, { 62, 95, 1080, 6, 0.34f }, { 54, 0.19f }, { Rimshot, 3400 }, 0.06f },
    { "Uplifting / Pryda Knock",       3, { 55, 132, 680, 7, 0.29f }, { 68, 0.21f }, { Rimshot, 2200 }, 0.07f },
    { "Uplifting / Bright Rail Rim",   3, { 69, 76, 1770, 6, 0.44f }, { 38, 0.17f }, { Rimshot, 4550 }, 0.02f },
    { "Uplifting / Edge Pop Rim",      3, { 61, 89, 1020, 8, 0.36f }, { 48, 0.20f }, { Rimshot, 3250 }, 0.03f },
    { "Uplifting / Backwall Rim",      3, { 58, 119, 865, 7, 0.34f }, { 63, 0.20f }, { Rimshot, 2690 }, 0.09f },
    { "Uplifting / Euphoria Rim",      3, { 64, 102, 1220, 5, 0.38f }, { 56, 0.18f }, { Stick, 3600 }, 0.05f },

    // Rolling (12: 3 of each voice)
    { "Rolling / Shuffle Wire Snare",  0, { 57, 107, 309, 11, 0.19f }, { 115, 0.81f }, { Stick, 4800 }, 0.02f, 2 },
    { "Rolling / Pickup Drag Snare",   0, { 56, 117, 258, 8, 0.22f }, { 121, 0.75f }, { Stick, 3980 }, 0.05f, 3 },
    { "Rolling / Short Fuse Snare",    0, { 58, 86, 292, 12, 0.21f }, { 78, 0.70f }, { Stick, 4650 }, 0 },
    { "Rolling / Glass Pair Bell",     1, { 75, 163, 1520, 0, 0.41f }, { 22, 0.03f }, { Stick, 4550 }, 0.04f, 2 },
    { "Rolling / Quiet Chime",         1, { 72, 238, 1260, 0, 0.24f }, { 33, 0.025f }, { Mallet, 2510 }, 0.07f },
    { "Rolling / Small Alloy Bell",    1, { 71, 165, 1120, 1, 0.64f }, { 22, 0.05f }, { Stick, 4250 }, 0.03f },
    { "Rolling / Triplet Tom",         2, { 46, 99, 165, 10, 0.15f }, { 33, 0.09f }, { Mallet, 2190 }, 0 },
    { "Rolling / Rack Bounce Tom",     2, { 48, 124, 181, 7, 0.16f }, { 43, 0.13f }, { Stick, 2850 }, 0.03f },
    { "Rolling / Quick Step Tom",      2, { 51, 93, 220, 11, 0.17f }, { 31, 0.10f }, { Mallet, 2450 }, 0 },
    { "Rolling / Clack Pair Rim",      3, { 60, 90, 990, 4, 0.25f }, { 49, 0.18f }, { Stick, 2780 }, 0.03f, 2 },
    { "Rolling / Rim Pocket",          3, { 57, 113, 760, 6, 0.33f }, { 60, 0.19f }, { Rimshot, 2520 }, 0.05f },
    { "Rolling / Stick Drag Rim",      3, { 62, 96, 1100, 4, 0.27f }, { 52, 0.18f }, { Stick, 3180 }, 0.03f, 3 },

    // Progressive (12: 3 of each voice)
    { "Progressive / Wire Brush Snare",0, { 53, 127, 206, 6, 0.11f }, { 142, 0.84f }, { Mallet, 1950 }, 0.05f },
    { "Progressive / Half Time Snare", 0, { 52, 168, 190, 7, 0.15f }, { 148, 0.76f }, { Mallet, 2200 }, 0.08f },
    { "Progressive / Back Room Snare", 0, { 53, 143, 245, 9, 0.13f }, { 132, 0.71f }, { Stick, 3500 }, 0.09f },
    { "Progressive / Tube Choir Bell", 1, { 69, 241, 1030, 0, 0.48f }, { 42, 0.04f }, { Mallet, 2300 }, 0.07f },
    { "Progressive / Bell Drop",       1, { 67, 224, 895, 2, 0.30f }, { 34, 0.04f }, { Mallet, 2100 }, 0.06f },
    { "Progressive / Porcelain Bell",  1, { 79, 196, 1790, 0, 0.20f }, { 19, 0.02f }, { Mallet, 3400 }, 0.04f },
    { "Progressive / Round Skin Tom",  2, { 43, 172, 129, 5, 0.08f }, { 56, 0.11f }, { Mallet, 1650 }, 0.05f },
    { "Progressive / Deep Pair Tom",   2, { 40, 162, 103, 9, 0.12f }, { 49, 0.11f }, { Mallet, 1510 }, 0.04f, 2 },
    { "Progressive / Membrane Tom",    2, { 42, 177, 120, 6, 0.09f }, { 53, 0.09f }, { Mallet, 1570 }, 0.04f },
    { "Progressive / Side Stick Rim",  3, { 58, 94, 845, 3, 0.22f }, { 52, 0.15f }, { Stick, 2400 }, 0.02f },
    { "Progressive / Hollow Rail Rim", 3, { 59, 109, 885, 4, 0.24f }, { 52, 0.13f }, { Mallet, 2300 }, 0.04f },
    { "Progressive / Cup Knock Rim",   3, { 56, 117, 722, 3, 0.23f }, { 52, 0.12f }, { Mallet, 2140 }, 0.05f },

    // Psy (12: 3 of each voice)
    { "Psy / Cut Through Snare",       0, { 61, 98, 410, 15, 0.31f }, { 89, 0.72f }, { Laser, 3950 }, 0.01f },
    { "Psy / Taut Skin Snare",         0, { 62, 91, 436, 13, 0.28f }, { 88, 0.60f }, { Rimshot, 4200 }, 0.02f },
    { "Psy / Thin Film Snare",         0, { 58, 92, 342, 5, 0.10f }, { 108, 0.78f }, { Stick, 6100 }, 0 },
    { "Psy / Signal Key Bell",         1, { 73, 157, 1920, 3, 0.61f }, { 27, 0.06f }, { Laser, 4100 }, 0.02f },
    { "Psy / Clockwork Bell",          1, { 82, 132, 2230, 1, 0.71f }, { 19, 0.03f }, { Stick, 5900 }, 0 },
    { "Psy / Tiny Hammer Bell",        1, { 84, 125, 2450, 1, 0.45f }, { 16, 0.03f }, { Stick, 6500 }, 0 },
    { "Psy / Pitch Pocket Tom",        2, { 47, 129, 174, 16, 0.19f }, { 38, 0.10f }, { Laser, 2250 }, 0.03f },
    { "Psy / Dry Circuit Tom",         2, { 45, 117, 152, 14, 0.23f }, { 35, 0.12f }, { Laser, 2850 }, 0 },
    { "Psy / Wood Tip Tom",            2, { 50, 131, 204, 8, 0.18f }, { 50, 0.14f }, { Stick, 3100 }, 0.02f },
    { "Psy / Edge Flick Rim",          3, { 68, 70, 1670, 5, 0.39f }, { 36, 0.14f }, { Laser, 3980 }, 0.01f },
    { "Psy / Chrome Edge Rim",         3, { 66, 85, 1610, 6, 0.48f }, { 43, 0.17f }, { Rimshot, 4300 }, 0.02f },
    { "Psy / Tiny Knock Rim",          3, { 70, 61, 1820, 3, 0.19f }, { 29, 0.09f }, { Stick, 4650 }, 0 },

    // Tech (12: 3 of each voice)
    { "Tech / Crossbar Snare",         0, { 57, 103, 332, 10, 0.27f }, { 93, 0.63f }, { Rimshot, 2950 }, 0.03f },
    { "Tech / Close Mic Snare",        0, { 54, 96, 228, 6, 0.12f }, { 82, 0.68f }, { Mallet, 2800 }, 0 },
    { "Tech / Rush Flam Snare",        0, { 55, 129, 275, 10, 0.23f }, { 137, 0.77f }, { Stick, 3900 }, 0.04f, 2 },
    { "Tech / Steel Dot Bell",         1, { 70, 141, 1180, 2, 0.66f }, { 22, 0.035f }, { Rimshot, 2950 }, 0.01f },
    { "Tech / Muted Pipe Bell",        1, { 68, 151, 970, 1, 0.28f }, { 24, 0.05f }, { Mallet, 2350 }, 0 },
    { "Tech / Copper Cup Bell",        1, { 65, 218, 780, 1, 0.53f }, { 31, 0.045f }, { Rimshot, 1850 }, 0.05f },
    { "Tech / Floor Pocket Tom",       2, { 41, 152, 112, 8, 0.10f }, { 49, 0.10f }, { Mallet, 1450 }, 0.02f },
    { "Tech / Low Runner Tom",         2, { 38, 138, 90, 10, 0.11f }, { 36, 0.08f }, { Mallet, 1230 }, 0 },
    { "Tech / Club Floor Tom",         2, { 36, 184, 82, 12, 0.13f }, { 44, 0.10f }, { Stick, 2150 }, 0.04f },
    { "Tech / Hard Lip Rim",           3, { 63, 80, 1310, 7, 0.46f }, { 41, 0.16f }, { Rimshot, 3720 }, 0.01f },
    { "Tech / Dry Tap Rim",            3, { 65, 67, 1480, 2, 0.20f }, { 32, 0.10f }, { Stick, 3970 }, 0 },
    { "Tech / Rim Tick",               3, { 64, 77, 1170, 5, 0.30f }, { 43, 0.17f }, { Rimshot, 2860 }, 0 },

    // Classic (12: 3 of each voice)
    { "Classic / Downbeat Snare",      0, { 50, 152, 184, 13, 0.16f }, { 119, 0.67f }, { Stick, 3150 }, 0.07f },
    { "Classic / Paper Head Snare",    0, { 60, 82, 362, 7, 0.14f }, { 95, 0.71f }, { Stick, 5150 }, 0 },
    { "Classic / Low Sling Snare",     0, { 48, 151, 165, 10, 0.17f }, { 114, 0.68f }, { Rimshot, 2850 }, 0.04f },
    { "Classic / Bronze Lip Bell",     1, { 64, 213, 704, 0, 0.62f }, { 35, 0.055f }, { Rimshot, 1650 }, 0.04f },
    { "Classic / Vintage Chime",       1, { 71, 228, 1140, 0, 0.34f }, { 32, 0.03f }, { Mallet, 2400 }, 0.06f },
    { "Classic / Analog Bell",         1, { 67, 198, 920, 1, 0.46f }, { 28, 0.04f }, { Stick, 2900 }, 0.04f },
    { "Classic / Dusty Rack Tom",      2, { 49, 140, 195, 4, 0.14f }, { 61, 0.17f }, { Stick, 2750 }, 0.05f },
    { "Classic / Rubber Head Tom",     2, { 44, 151, 145, 6, 0.09f }, { 47, 0.08f }, { Mallet, 1850 }, 0.01f },
    { "Classic / Last Floor Tom",      2, { 39, 192, 96, 8, 0.10f }, { 55, 0.09f }, { Mallet, 1340 }, 0.06f },
    { "Classic / Woodblock Rim",       3, { 67, 73, 1380, 2, 0.16f }, { 31, 0.08f }, { Mallet, 2840 }, 0 },
    { "Classic / Club Rim",            3, { 54, 126, 635, 8, 0.31f }, { 66, 0.22f }, { Rimshot, 2090 }, 0.08f },
    { "Classic / Studio Stick Rim",    3, { 59, 102, 910, 4, 0.26f }, { 50, 0.16f }, { Stick, 2650 }, 0.04f },
}};

struct Voicing {
    core::VoiceMode voice;
    core::NoiseType air;
    float bodyLevel, pitchTime, wireHP, wireLP, tension, width, strikeLevel, gain;
};
constexpr std::array<Voicing, 4> voicings{{
    { core::VoiceMode::DanceSnare, core::NoiseType::SnareWires, 0.86f, 13, 630, 10300, 0.67f, 0.94f, 0.53f, -1.0f },
    { core::VoiceMode::FmBell, core::NoiseType::VelvetAir, 0.86f, 7, 1400, 8700, 0.35f, 0.79f, 0.26f, -1.5f },
    { core::VoiceMode::ClubTom, core::NoiseType::WhiteSizzle, 0.90f, 20, 890, 6900, 0.40f, 0.66f, 0.39f, -1.5f },
    { core::VoiceMode::PrydaRimSmack, core::NoiseType::SnareWires, 0.81f, 9, 1020, 9200, 0.77f, 0.80f, 0.68f, -1.0f },
}};

FactoryPreset makePreset(const Patch& p, const Voicing& v) {
    const auto decay = static_cast<float>(p.body.decayMs);
    return { p.name, {
        { kVoiceMode, static_cast<float>(v.voice) }, { kToneNote, float(p.body.midiNote % 12) },
        { kToneOctave, float(p.body.midiNote / 12 - 1) }, { kToneFine, 0 }, { kToneFreq, float(p.body.shellHz) },
        { kToneQ, 2.7f + p.body.fm * 3 }, { kToneDecay, decay }, { kTonePitchEnv, float(p.body.pitchSweep) },
        { kTonePitchDecay, v.pitchTime }, { kFmAmount, p.body.fm }, { kShellSpread, 0 }, { kToneLevel, v.bodyLevel },
        { kBodyHPF, 25 }, { kBodyLPF, 12700 }, { kBodyPan, 0 }, { kAmpEnabled, 0 },
        { kAmpAttack, 0.25f }, { kAmpDecay, decay * 0.4f }, { kAmpSustain, 0.69f }, { kAmpRelease, decay * 0.48f },
        { kPreBursts, float(p.strikes) }, { kPreSpread, 5 }, { kPreDecay, 4 }, { kPreJitter, 0.04f },
        { kFlamSpread, 0.26f }, { kPreLevel, 0.44f }, { kNoiseType, static_cast<float>(v.air) },
        { kNoiseAttack, 0.2f }, { kNoiseHold, 1 }, { kNoiseDecay, float(p.wires.decayMs) }, { kNoiseCurve, -0.32f },
        { kNoiseHPF, v.wireHP }, { kNoiseLPF, v.wireLP }, { kNoiseStereoWidth, v.width },
        { kNoiseLevel, p.wires.level }, { kWireTension, v.tension }, { kWirePan, 0 },
        { kClickType, static_cast<float>(p.strike.type) }, { kClickFreq, float(p.strike.hz) }, { kClickLevel, v.strikeLevel },
        { kSubThump, 0 }, { kTransientHPF, 95 }, { kTransientLPF, 13500 }, { kTransientPan, 0 },
        { kDistType, 0 }, { kDistDrive, 0.065f }, { kDistTone, 0 }, { kDistMix, 0.13f },
        { kCompThresh, -12 }, { kCompRatio, 2 }, { kCompAttack, 9 }, { kCompRelease, 48 },
        { kCompMakeup, 0.5f }, { kCompMix, 0.45f }, { kLimiterEnabled, 1 }, { kLimiterCeiling, -1 },
        { kLimiterRelease, 20 }, { kReverbSize, 0.38f }, { kReverbDecay, 190 }, { kReverbDamp, 0.58f },
        { kReverbGate, 80 }, { kReverbMix, p.room }, { kEchoMix, 0 }, { kReverseMode, 0 }, { kReverseMix, 0.35f },
        { kMidiMode, 0 }, { kLockPitch, 0 }, { kLockIn, 0 }, { kStereoWidth, 1 }, { kOutputGain, v.gain },
    } };
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const auto programs = [] {
        std::vector<FactoryPreset> result;
        result.reserve(bank.size());
        std::ranges::transform(bank, std::back_inserter(result),
                               [](const Patch& patch) { return makePreset(patch, voicings[patch.family]); });
        return result;
    }();
    return programs;
}

int firstPresetForFamily(size_t family) noexcept {
    const auto it = std::ranges::find_if(bank, [family](const Patch& patch) { return patch.family == family; });
    return it != bank.end() ? static_cast<int>(std::distance(bank.begin(), it)) : 0;
}

size_t familyForProgram(int program) noexcept {
    if (program < 0 || program >= static_cast<int>(bank.size()))
        return 0;
    return bank[static_cast<size_t>(program)].family;
}

namespace {
Section sectionFor(const juce::String& id) {
    if (id == kPreBursts || id == kPreSpread || id == kPreDecay || id == kPreJitter || id == kFlamSpread || id == kPreLevel)
        return Section::PreStrike;
    if (id == kNoiseType || id == kNoiseAttack || id == kNoiseHold || id == kNoiseDecay || id == kNoiseCurve
        || id == kNoiseHPF || id == kNoiseLPF || id == kNoiseStereoWidth || id == kNoiseLevel
        || id == kWireTension || id == kWirePan)
        return Section::WiresAir;
    if (id == kClickType || id == kClickLevel || id == kClickFreq || id == kSubThump)
        return Section::Transient;
    if (id == kTransientHPF || id == kTransientLPF || id == kTransientPan)
        return Section::Transient;
    if (id == kDistType || id == kDistDrive || id == kDistTone || id == kDistMix || id == kCompThresh || id == kCompRatio
        || id == kCompAttack || id == kCompRelease || id == kCompMakeup || id == kCompMix || id == kLimiterCeiling
        || id == kLimiterRelease || id == kLimiterEnabled)
        return Section::SaturationDynamics;
    if (id == kReverbSize || id == kReverbDecay || id == kReverbDamp || id == kReverbGate || id == kReverbMix || id == kEchoMix
        || id == kReverseMode || id == kReverseMix || id == kMidiMode || id == kLockIn || id == kStereoWidth || id == kOutputGain)
        return Section::SpaceOutput;
    return Section::VoiceTuning;
}
} // namespace

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state,
           const SectionLocks& locks, bool preservePitch) {
    for (const auto& [paramId, value] : preset.values) {
        const auto section = sectionFor(paramId);
        if (locks[static_cast<size_t>(section)])
            continue;
        if (preservePitch && (paramId == kToneFreq || paramId == kToneNote || paramId == kToneOctave || paramId == kToneFine || paramId == kLockPitch))
            continue;
        if (auto* param = state.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }
}

} // namespace opensmacker::presets
