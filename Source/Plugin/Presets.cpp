#include "Presets.h"
#include "Parameters.h"
#include <algorithm>

namespace opensmacker::presets {

using namespace opensmacker::params;

namespace {

struct PresetDef {
    const char* name = "";

    // Pre-strike flam / roll
    int preBursts = 1;             // 1..5
    float preSpread = 11.0f;       // 2..45 ms
    float preDecay = 8.0f;         // 2..35 ms
    float preJitter = 0.12f;       // 0..1
    float flamSpread = 0.40f;      // 0..1
    float preLevel = 0.55f;        // 0..1

    // Voice & Tone Body
    int voiceMode = 0;             // 0=DanceSnare, 1=ClubTom, 2=FmBell, 3=PrydaRimSmack
    int toneNote = 9;              // 0..11 (0=C, 9=A)
    int toneOctave = 3;            // 1..8
    float toneFine = 0.0f;         // -50..50 ct
    float toneFreq = 220.0f;       // 60..6000 Hz
    float toneQ = 3.5f;            // 0.5..15
    float toneDecay = 180.0f;      // 15..1200 ms
    float tonePitchEnv = 12.0f;    // -24..36 st
    float tonePitchDecay = 22.0f;  // 3..150 ms
    float fmAmount = 0.30f;        // 0..1
    float toneLevel = 0.85f;       // 0..1

    // Snare Wires / Sizzle / Air
    int noiseType = 0;             // 0=SnareWires, 1=WhiteSizzle, 2=VelvetAir, 3=MetallicRing
    float noiseAttack = 0.0f;      // 0..20 ms
    float noiseHold = 2.0f;        // 0..30 ms
    float noiseDecay = 210.0f;     // 15..1200 ms
    float noiseCurve = -0.35f;     // -1..1
    float noiseHPF = 280.0f;       // 20..2500 Hz
    float noiseLPF = 11000.0f;     // 800..20000 Hz
    float noiseStereoWidth = 1.15f;// 0..2
    float noiseLevel = 0.80f;      // 0..1

    // Transient Strike
    int clickType = 0;             // 0=Stick, 1=Mallet, 2=Rimshot, 3=Laser
    float clickLevel = 0.50f;      // 0..1
    float clickFreq = 3800.0f;     // 500..12000 Hz

    // Impact Distortion
    int distType = 0;              // 0=Tape, 1=Tube, 2=HardClip, 3=Wavefold
    float distDrive = 0.14f;       // 0..1
    float distTone = 0.0f;         // -1..1
    float distMix = 0.22f;         // 0..1

    // Compressor
    float compThresh = -14.0f;     // -40..0 dB
    float compRatio = 4.0f;        // 1..20
    float compAttack = 5.0f;       // 0.1..50 ms
    float compRelease = 80.0f;     // 10..500 ms
    float compMakeup = 3.0f;       // 0..24 dB
    float compMix = 1.0f;          // 0..1

    // Limiter
    float limiterCeiling = -0.3f;  // -12..0 dB
    float limiterRelease = 20.0f;  // 5..100 ms
    float limiterEnabled = 1.0f;   // 0 or 1

    // Gated Trance Reverb
    float reverbSize = 0.65f;      // 0.1..1
    float reverbDecay = 480.0f;    // 50..2500 ms
    float reverbDamp = 0.38f;      // 0..1
    float reverbGate = 180.0f;     // 0..600 ms
    float reverbMix = 0.32f;       // 0..1

    // Reverse & Sync
    int reverseMode = 0;           // 0=Off, 1=Beat End, 2=Beat 4 Auto, 3=1/2 Beat, 4=1/4 Beat
    float reverseMix = 0.50f;      // 0..1

    // Global / Master
    int midiMode = 0;              // 0=Fixed (Root), 1=Note Tracking
    float stereoWidth = 1.0f;      // 0..2
    float outputGain = 0.0f;       // -36..12 dB
};

FactoryPreset makePreset(const PresetDef& p) {
    return FactoryPreset{
        p.name,
        {
            { kPreBursts, static_cast<float>(p.preBursts) },
            { kPreSpread, p.preSpread },
            { kPreDecay, p.preDecay },
            { kPreJitter, p.preJitter },
            { kFlamSpread, p.flamSpread },
            { kPreLevel, p.preLevel },
            { kVoiceMode, static_cast<float>(p.voiceMode) },
            { kToneNote, static_cast<float>(p.toneNote) },
            { kToneOctave, static_cast<float>(p.toneOctave) },
            { kToneFine, p.toneFine },
            { kToneFreq, p.toneFreq },
            { kToneQ, p.toneQ },
            { kToneDecay, p.toneDecay },
            { kTonePitchEnv, p.tonePitchEnv },
            { kTonePitchDecay, p.tonePitchDecay },
            { kFmAmount, p.fmAmount },
            { kToneLevel, p.toneLevel },
            { kNoiseType, static_cast<float>(p.noiseType) },
            { kNoiseAttack, p.noiseAttack },
            { kNoiseHold, p.noiseHold },
            { kNoiseDecay, p.noiseDecay },
            { kNoiseCurve, p.noiseCurve },
            { kNoiseHPF, p.noiseHPF },
            { kNoiseLPF, p.noiseLPF },
            { kNoiseStereoWidth, p.noiseStereoWidth },
            { kNoiseLevel, p.noiseLevel },
            { kClickType, static_cast<float>(p.clickType) },
            { kClickLevel, p.clickLevel },
            { kClickFreq, p.clickFreq },
            { kDistType, static_cast<float>(p.distType) },
            { kDistDrive, p.distDrive },
            { kDistTone, p.distTone },
            { kDistMix, p.distMix },
            { kCompThresh, p.compThresh },
            { kCompRatio, p.compRatio },
            { kCompAttack, p.compAttack },
            { kCompRelease, p.compRelease },
            { kCompMakeup, p.compMakeup },
            { kCompMix, p.compMix },
            { kLimiterCeiling, p.limiterCeiling },
            { kLimiterRelease, p.limiterRelease },
            { kLimiterEnabled, p.limiterEnabled },
            { kReverbSize, p.reverbSize },
            { kReverbDecay, p.reverbDecay },
            { kReverbDamp, p.reverbDamp },
            { kReverbGate, p.reverbGate },
            { kReverbMix, p.reverbMix },
            { kReverseMode, static_cast<float>(p.reverseMode) },
            { kReverseMix, p.reverseMix },
            { kMidiMode, static_cast<float>(p.midiMode) },
            { kStereoWidth, p.stereoWidth },
            { kOutputGain, p.outputGain },
        }
    };
}

} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> kPresets = {
        // =========================================================================
        // SN 01 - SN 24: TUNABLE DANCE & TRANCE SNARES (24 Presets)
        // =========================================================================

        makePreset(PresetDef{
            .name = "SN 01 Anjuna Snare Smack",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.6f,
            .toneDecay = 185.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.22f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 210.0f, .noiseCurve = -0.35f, .noiseHPF = 280.0f, .noiseLPF = 11500.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.82f,
            .clickType = 0, .clickLevel = 0.54f, .clickFreq = 3800.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.05f, .distMix = 0.25f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.5f, .compRelease = 80.0f, .compMakeup = 3.2f,
            .reverbSize = 0.65f, .reverbDecay = 480.0f, .reverbDamp = 0.35f, .reverbGate = 180.0f, .reverbMix = 0.28f,
            .stereoWidth = 1.05f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "SN 02 Above & Beyond Group Therapy Snare",
            .preBursts = 2, .preSpread = 11.5f, .preDecay = 8.5f, .preJitter = 0.12f, .flamSpread = 0.42f, .preLevel = 0.55f,
            .voiceMode = 0, .toneNote = 7, .toneOctave = 3, .toneFreq = 196.0f, .toneQ = 3.4f,
            .toneDecay = 205.0f, .tonePitchEnv = 11.0f, .tonePitchDecay = 24.0f, .fmAmount = 0.20f, .toneLevel = 0.85f,
            .noiseType = 2, .noiseDecay = 235.0f, .noiseCurve = -0.30f, .noiseHPF = 250.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.30f, .noiseLevel = 0.84f,
            .clickType = 2, .clickLevel = 0.52f, .clickFreq = 3600.0f,
            .distType = 1, .distDrive = 0.18f, .distTone = 0.08f, .distMix = 0.28f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 4.0f, .compRelease = 90.0f, .compMakeup = 3.4f,
            .reverbSize = 0.72f, .reverbDecay = 560.0f, .reverbDamp = 0.30f, .reverbGate = 210.0f, .reverbMix = 0.32f,
            .stereoWidth = 1.15f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 03 Pryda Arena Rim Smack",
            .voiceMode = 3, .toneNote = 9, .toneOctave = 3, .toneFreq = 1650.0f, .toneQ = 4.6f,
            .toneDecay = 230.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 18.0f, .fmAmount = 0.36f, .toneLevel = 0.90f,
            .noiseType = 0, .noiseDecay = 280.0f, .noiseCurve = -0.25f, .noiseHPF = 320.0f, .noiseLPF = 13000.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.86f,
            .clickType = 2, .clickLevel = 0.68f, .clickFreq = 4200.0f,
            .distType = 1, .distDrive = 0.28f, .distTone = 0.12f, .distMix = 0.38f,
            .compThresh = -16.0f, .compRatio = 5.5f, .compAttack = 3.2f, .compRelease = 100.0f, .compMakeup = 4.2f,
            .reverbSize = 0.80f, .reverbDecay = 650.0f, .reverbDamp = 0.25f, .reverbGate = 240.0f, .reverbMix = 0.36f,
            .stereoWidth = 1.25f, .outputGain = 1.0f
        }),

        makePreset(PresetDef{
            .name = "SN 04 Bayer Analog Tight Snare",
            .voiceMode = 0, .toneNote = 6, .toneOctave = 3, .toneFreq = 185.0f, .toneQ = 3.2f,
            .toneDecay = 150.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 19.0f, .fmAmount = 0.18f, .toneLevel = 0.88f,
            .noiseType = 0, .noiseDecay = 160.0f, .noiseCurve = -0.40f, .noiseHPF = 320.0f, .noiseLPF = 10500.0f,
            .noiseStereoWidth = 1.05f, .noiseLevel = 0.80f,
            .clickType = 0, .clickLevel = 0.56f, .clickFreq = 3400.0f,
            .distType = 0, .distDrive = 0.22f, .distTone = -0.05f, .distMix = 0.30f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 3.5f, .compRelease = 70.0f, .compMakeup = 3.5f,
            .reverbSize = 0.55f, .reverbDecay = 380.0f, .reverbDamp = 0.45f, .reverbGate = 140.0f, .reverbMix = 0.20f,
            .stereoWidth = 1.0f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 05 Bluestone Snap Rim Snare",
            .preBursts = 2, .preSpread = 8.0f, .preDecay = 6.0f, .preJitter = 0.10f, .flamSpread = 0.35f, .preLevel = 0.52f,
            .voiceMode = 3, .toneNote = 8, .toneOctave = 3, .toneFreq = 1800.0f, .toneQ = 4.8f,
            .toneDecay = 170.0f, .tonePitchEnv = 13.0f, .tonePitchDecay = 18.0f, .fmAmount = 0.30f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 180.0f, .noiseCurve = -0.35f, .noiseHPF = 350.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.82f,
            .clickType = 2, .clickLevel = 0.65f, .clickFreq = 4400.0f,
            .distType = 0, .distDrive = 0.20f, .distTone = 0.08f, .distMix = 0.28f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 3.8f, .compRelease = 75.0f, .compMakeup = 3.2f,
            .reverbSize = 0.62f, .reverbDecay = 450.0f, .reverbDamp = 0.36f, .reverbGate = 160.0f, .reverbMix = 0.26f,
            .stereoWidth = 1.10f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 06 Myon & Shane 54 Summer Snare",
            .preBursts = 2, .preSpread = 12.0f, .preDecay = 8.5f, .preJitter = 0.14f, .flamSpread = 0.45f, .preLevel = 0.54f,
            .voiceMode = 0, .toneNote = 8, .toneOctave = 3, .toneFreq = 207.65f, .toneQ = 3.3f,
            .toneDecay = 190.0f, .tonePitchEnv = 11.0f, .tonePitchDecay = 24.0f, .fmAmount = 0.22f, .toneLevel = 0.84f,
            .noiseType = 1, .noiseDecay = 225.0f, .noiseCurve = -0.32f, .noiseHPF = 290.0f, .noiseLPF = 13500.0f,
            .noiseStereoWidth = 1.30f, .noiseLevel = 0.84f,
            .clickType = 0, .clickLevel = 0.52f, .clickFreq = 3900.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.12f, .distMix = 0.24f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 4.5f, .compRelease = 85.0f, .compMakeup = 3.0f,
            .reverbSize = 0.68f, .reverbDecay = 520.0f, .reverbDamp = 0.32f, .reverbGate = 190.0f, .reverbMix = 0.30f,
            .stereoWidth = 1.15f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 07 Sun & Moon Push the Button Snare",
            .preBursts = 3, .preSpread = 13.0f, .preDecay = 9.0f, .preJitter = 0.15f, .flamSpread = 0.50f, .preLevel = 0.60f,
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.5f,
            .toneDecay = 220.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 25.0f, .fmAmount = 0.26f, .toneLevel = 0.88f,
            .noiseType = 0, .noiseDecay = 250.0f, .noiseCurve = -0.30f, .noiseHPF = 260.0f, .noiseLPF = 12500.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.88f,
            .clickType = 2, .clickLevel = 0.58f, .clickFreq = 3800.0f,
            .distType = 1, .distDrive = 0.24f, .distTone = 0.10f, .distMix = 0.34f,
            .compThresh = -15.5f, .compRatio = 5.2f, .compAttack = 3.5f, .compRelease = 95.0f, .compMakeup = 3.8f,
            .reverbSize = 0.78f, .reverbDecay = 640.0f, .reverbDamp = 0.28f, .reverbGate = 240.0f, .reverbMix = 0.35f,
            .stereoWidth = 1.20f, .outputGain = 0.8f
        }),

        makePreset(PresetDef{
            .name = "SN 08 Zo Funk Electro Snare",
            .voiceMode = 0, .toneNote = 11, .toneOctave = 3, .toneFreq = 246.94f, .toneQ = 3.6f,
            .toneDecay = 160.0f, .tonePitchEnv = 18.0f, .tonePitchDecay = 18.0f, .fmAmount = 0.38f, .toneLevel = 0.90f,
            .noiseType = 1, .noiseDecay = 165.0f, .noiseCurve = -0.42f, .noiseHPF = 340.0f, .noiseLPF = 11000.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.80f,
            .clickType = 3, .clickLevel = 0.62f, .clickFreq = 4800.0f,
            .distType = 3, .distDrive = 0.28f, .distTone = 0.10f, .distMix = 0.35f,
            .compThresh = -16.0f, .compRatio = 6.0f, .compAttack = 2.5f, .compRelease = 65.0f, .compMakeup = 4.0f,
            .reverbSize = 0.52f, .reverbDecay = 360.0f, .reverbDamp = 0.42f, .reverbGate = 130.0f, .reverbMix = 0.18f,
            .stereoWidth = 1.05f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "SN 09 Daniel Kandi Always Alive Snare",
            .preBursts = 2, .preSpread = 10.5f, .preDecay = 8.0f, .preJitter = 0.12f, .flamSpread = 0.42f, .preLevel = 0.54f,
            .voiceMode = 0, .toneNote = 10, .toneOctave = 3, .toneFreq = 233.08f, .toneQ = 3.4f,
            .toneDecay = 185.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.22f, .toneLevel = 0.85f,
            .noiseType = 2, .noiseDecay = 230.0f, .noiseCurve = -0.32f, .noiseHPF = 310.0f, .noiseLPF = 13500.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.86f,
            .clickType = 0, .clickLevel = 0.54f, .clickFreq = 4000.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = 0.06f, .distMix = 0.26f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.0f, .compRelease = 80.0f, .compMakeup = 3.2f,
            .reverbSize = 0.74f, .reverbDecay = 580.0f, .reverbDamp = 0.28f, .reverbGate = 220.0f, .reverbMix = 0.32f,
            .stereoWidth = 1.20f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 10 Sean Tyas Degenerate Punch Snare",
            .voiceMode = 0, .toneNote = 7, .toneOctave = 3, .toneFreq = 196.0f, .toneQ = 3.6f,
            .toneDecay = 140.0f, .tonePitchEnv = 16.0f, .tonePitchDecay = 16.0f, .fmAmount = 0.28f, .toneLevel = 0.90f,
            .noiseType = 0, .noiseDecay = 145.0f, .noiseCurve = -0.45f, .noiseHPF = 360.0f, .noiseLPF = 11500.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.84f,
            .clickType = 2, .clickLevel = 0.65f, .clickFreq = 4500.0f,
            .distType = 2, .distDrive = 0.30f, .distTone = -0.05f, .distMix = 0.36f,
            .compThresh = -16.0f, .compRatio = 6.0f, .compAttack = 2.2f, .compRelease = 55.0f, .compMakeup = 4.2f,
            .reverbSize = 0.50f, .reverbDecay = 340.0f, .reverbDamp = 0.44f, .reverbGate = 120.0f, .reverbMix = 0.18f,
            .stereoWidth = 1.05f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "SN 11 Ferry Corsten Flashover Electro Snare",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.8f,
            .toneDecay = 175.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 20.0f, .fmAmount = 0.28f, .toneLevel = 0.88f,
            .noiseType = 0, .noiseDecay = 190.0f, .noiseCurve = -0.38f, .noiseHPF = 300.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.82f,
            .clickType = 2, .clickLevel = 0.58f, .clickFreq = 4100.0f,
            .distType = 1, .distDrive = 0.26f, .distTone = 0.08f, .distMix = 0.32f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 3.5f, .compRelease = 75.0f, .compMakeup = 3.6f,
            .reverbSize = 0.62f, .reverbDecay = 460.0f, .reverbDamp = 0.35f, .reverbGate = 160.0f, .reverbMix = 0.24f,
            .stereoWidth = 1.10f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 12 System F Out of the Blue 909 Snare",
            .voiceMode = 0, .toneNote = 7, .toneOctave = 3, .toneFreq = 196.0f, .toneQ = 3.2f,
            .toneDecay = 175.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.20f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 205.0f, .noiseCurve = -0.36f, .noiseHPF = 260.0f, .noiseLPF = 11000.0f,
            .noiseStereoWidth = 1.12f, .noiseLevel = 0.82f,
            .clickType = 0, .clickLevel = 0.52f, .clickFreq = 3600.0f,
            .distType = 0, .distDrive = 0.15f, .distTone = 0.02f, .distMix = 0.22f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 4.5f, .compRelease = 80.0f, .compMakeup = 3.0f,
            .reverbSize = 0.68f, .reverbDecay = 520.0f, .reverbDamp = 0.34f, .reverbGate = 190.0f, .reverbMix = 0.30f,
            .stereoWidth = 1.10f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 13 Armada Mainstage Snare",
            .preBursts = 2, .preSpread = 10.0f, .preDecay = 7.5f, .preJitter = 0.12f, .flamSpread = 0.45f, .preLevel = 0.56f,
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.5f,
            .toneDecay = 215.0f, .tonePitchEnv = 13.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.25f, .toneLevel = 0.88f,
            .noiseType = 1, .noiseDecay = 260.0f, .noiseCurve = -0.28f, .noiseHPF = 280.0f, .noiseLPF = 14000.0f,
            .noiseStereoWidth = 1.40f, .noiseLevel = 0.88f,
            .clickType = 2, .clickLevel = 0.62f, .clickFreq = 4200.0f,
            .distType = 1, .distDrive = 0.24f, .distTone = 0.10f, .distMix = 0.32f,
            .compThresh = -15.0f, .compRatio = 5.2f, .compAttack = 3.5f, .compRelease = 90.0f, .compMakeup = 3.8f,
            .reverbSize = 0.80f, .reverbDecay = 640.0f, .reverbDamp = 0.26f, .reverbGate = 240.0f, .reverbMix = 0.35f,
            .stereoWidth = 1.25f, .outputGain = 0.8f
        }),

        makePreset(PresetDef{
            .name = "SN 14 ASOT 138 Uplifting Snare",
            .voiceMode = 0, .toneNote = 11, .toneOctave = 3, .toneFreq = 246.94f, .toneQ = 3.5f,
            .toneDecay = 190.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.24f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 220.0f, .noiseCurve = -0.34f, .noiseHPF = 300.0f, .noiseLPF = 13000.0f,
            .noiseStereoWidth = 1.25f, .noiseLevel = 0.84f,
            .clickType = 0, .clickLevel = 0.55f, .clickFreq = 4200.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = 0.08f, .distMix = 0.26f,
            .compThresh = -14.0f, .compRatio = 4.6f, .compAttack = 4.0f, .compRelease = 80.0f, .compMakeup = 3.2f,
            .reverbSize = 0.72f, .reverbDecay = 560.0f, .reverbDamp = 0.30f, .reverbGate = 200.0f, .reverbMix = 0.30f,
            .stereoWidth = 1.15f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 15 Subculture Tech-Trance Snare",
            .voiceMode = 0, .toneNote = 5, .toneOctave = 3, .toneFreq = 174.61f, .toneQ = 3.4f,
            .toneDecay = 145.0f, .tonePitchEnv = 15.0f, .tonePitchDecay = 18.0f, .fmAmount = 0.22f, .toneLevel = 0.88f,
            .noiseType = 0, .noiseDecay = 155.0f, .noiseCurve = -0.42f, .noiseHPF = 340.0f, .noiseLPF = 11000.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.82f,
            .clickType = 2, .clickLevel = 0.60f, .clickFreq = 3900.0f,
            .distType = 2, .distDrive = 0.26f, .distTone = -0.08f, .distMix = 0.32f,
            .compThresh = -15.5f, .compRatio = 5.5f, .compAttack = 2.8f, .compRelease = 65.0f, .compMakeup = 3.8f,
            .reverbSize = 0.54f, .reverbDecay = 380.0f, .reverbDamp = 0.42f, .reverbGate = 140.0f, .reverbMix = 0.20f,
            .stereoWidth = 1.05f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 16 Coldharbour Dark Rim Smack",
            .voiceMode = 3, .toneNote = 4, .toneOctave = 3, .toneFreq = 1450.0f, .toneQ = 4.2f,
            .toneDecay = 190.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 19.0f, .fmAmount = 0.30f, .toneLevel = 0.88f,
            .noiseType = 0, .noiseDecay = 190.0f, .noiseCurve = -0.38f, .noiseHPF = 350.0f, .noiseLPF = 9800.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.82f,
            .clickType = 2, .clickLevel = 0.65f, .clickFreq = 3600.0f,
            .distType = 1, .distDrive = 0.25f, .distTone = -0.12f, .distMix = 0.32f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 3.5f, .compRelease = 80.0f, .compMakeup = 3.6f,
            .reverbSize = 0.60f, .reverbDecay = 450.0f, .reverbDamp = 0.40f, .reverbGate = 160.0f, .reverbMix = 0.22f,
            .stereoWidth = 1.10f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 17 FSOE 140 Anthem Snare",
            .preBursts = 4, .preSpread = 12.0f, .preDecay = 8.0f, .preJitter = 0.14f, .flamSpread = 0.55f, .preLevel = 0.65f,
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.6f,
            .toneDecay = 225.0f, .tonePitchEnv = 13.0f, .tonePitchDecay = 24.0f, .fmAmount = 0.26f, .toneLevel = 0.88f,
            .noiseType = 0, .noiseDecay = 260.0f, .noiseCurve = -0.30f, .noiseHPF = 270.0f, .noiseLPF = 13000.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.88f,
            .clickType = 2, .clickLevel = 0.64f, .clickFreq = 4200.0f,
            .distType = 1, .distDrive = 0.25f, .distTone = 0.10f, .distMix = 0.35f,
            .compThresh = -16.0f, .compRatio = 5.5f, .compAttack = 3.2f, .compRelease = 95.0f, .compMakeup = 4.0f,
            .reverbSize = 0.82f, .reverbDecay = 680.0f, .reverbDamp = 0.26f, .reverbGate = 250.0f, .reverbMix = 0.36f,
            .stereoWidth = 1.25f, .outputGain = 0.9f
        }),

        makePreset(PresetDef{
            .name = "SN 18 Vandit Berlin Club Snare",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.4f,
            .toneDecay = 160.0f, .tonePitchEnv = 13.0f, .tonePitchDecay = 20.0f, .fmAmount = 0.22f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 180.0f, .noiseCurve = -0.38f, .noiseHPF = 310.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.82f,
            .clickType = 0, .clickLevel = 0.54f, .clickFreq = 3900.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = 0.04f, .distMix = 0.25f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 3.8f, .compRelease = 75.0f, .compMakeup = 3.4f,
            .reverbSize = 0.60f, .reverbDecay = 440.0f, .reverbDamp = 0.36f, .reverbGate = 150.0f, .reverbMix = 0.22f,
            .stereoWidth = 1.10f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 19 Garuda Big Room Snare",
            .preBursts = 2, .preSpread = 9.5f, .preDecay = 7.0f, .preJitter = 0.11f, .flamSpread = 0.40f, .preLevel = 0.54f,
            .voiceMode = 0, .toneNote = 7, .toneOctave = 3, .toneFreq = 196.0f, .toneQ = 3.5f,
            .toneDecay = 195.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.24f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 225.0f, .noiseCurve = -0.32f, .noiseHPF = 290.0f, .noiseLPF = 12500.0f,
            .noiseStereoWidth = 1.25f, .noiseLevel = 0.84f,
            .clickType = 2, .clickLevel = 0.56f, .clickFreq = 4000.0f,
            .distType = 0, .distDrive = 0.22f, .distTone = 0.08f, .distMix = 0.28f,
            .compThresh = -14.8f, .compRatio = 5.0f, .compAttack = 3.6f, .compRelease = 85.0f, .compMakeup = 3.5f,
            .reverbSize = 0.70f, .reverbDecay = 540.0f, .reverbDamp = 0.32f, .reverbGate = 200.0f, .reverbMix = 0.30f,
            .stereoWidth = 1.15f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 20 Wake Your Mind Snare",
            .voiceMode = 0, .toneNote = 8, .toneOctave = 3, .toneFreq = 207.65f, .toneQ = 3.4f,
            .toneDecay = 185.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.22f, .toneLevel = 0.85f,
            .noiseType = 2, .noiseDecay = 215.0f, .noiseCurve = -0.34f, .noiseHPF = 280.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.25f, .noiseLevel = 0.84f,
            .clickType = 0, .clickLevel = 0.52f, .clickFreq = 3800.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.05f, .distMix = 0.24f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.0f, .compRelease = 80.0f, .compMakeup = 3.2f,
            .reverbSize = 0.65f, .reverbDecay = 480.0f, .reverbDamp = 0.35f, .reverbGate = 180.0f, .reverbMix = 0.26f,
            .stereoWidth = 1.12f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 21 Euphonic Uplifting Snare",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.3f,
            .toneDecay = 195.0f, .tonePitchEnv = 11.0f, .tonePitchDecay = 24.0f, .fmAmount = 0.20f, .toneLevel = 0.85f,
            .noiseType = 0, .noiseDecay = 225.0f, .noiseCurve = -0.32f, .noiseHPF = 270.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.84f,
            .clickType = 0, .clickLevel = 0.52f, .clickFreq = 3700.0f,
            .distType = 0, .distDrive = 0.15f, .distTone = 0.04f, .distMix = 0.22f,
            .compThresh = -13.8f, .compRatio = 4.4f, .compAttack = 4.2f, .compRelease = 85.0f, .compMakeup = 3.0f,
            .reverbSize = 0.70f, .reverbDecay = 540.0f, .reverbDamp = 0.32f, .reverbGate = 210.0f, .reverbMix = 0.30f,
            .stereoWidth = 1.15f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 22 Positiva 99 Ibiza Snare",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.2f,
            .toneDecay = 200.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.22f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 240.0f, .noiseCurve = -0.30f, .noiseHPF = 250.0f, .noiseLPF = 11500.0f,
            .noiseStereoWidth = 1.25f, .noiseLevel = 0.85f,
            .clickType = 0, .clickLevel = 0.50f, .clickFreq = 3500.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.02f, .distMix = 0.24f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.5f, .compRelease = 85.0f, .compMakeup = 3.2f,
            .reverbSize = 0.75f, .reverbDecay = 600.0f, .reverbDamp = 0.30f, .reverbGate = 230.0f, .reverbMix = 0.35f,
            .stereoWidth = 1.15f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "SN 23 Note-Tracked Melodic Snare Roll",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.4f,
            .toneDecay = 140.0f, .tonePitchEnv = 10.0f, .tonePitchDecay = 18.0f, .fmAmount = 0.20f, .toneLevel = 0.86f,
            .noiseType = 0, .noiseDecay = 150.0f, .noiseCurve = -0.40f, .noiseHPF = 320.0f, .noiseLPF = 11500.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.80f,
            .clickType = 0, .clickLevel = 0.50f, .clickFreq = 3800.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.05f, .distMix = 0.22f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 4.0f, .compRelease = 70.0f, .compMakeup = 3.0f,
            .reverbSize = 0.52f, .reverbDecay = 360.0f, .reverbDamp = 0.40f, .reverbGate = 120.0f, .reverbMix = 0.20f,
            .midiMode = 1, .stereoWidth = 1.05f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "SN 24 Clean Studio Reference Snare",
            .voiceMode = 0, .toneNote = 9, .toneOctave = 3, .toneFreq = 220.0f, .toneQ = 3.0f,
            .toneDecay = 180.0f, .tonePitchEnv = 10.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.20f, .toneLevel = 0.85f,
            .noiseType = 0, .noiseDecay = 200.0f, .noiseCurve = -0.35f, .noiseHPF = 260.0f, .noiseLPF = 11000.0f,
            .noiseStereoWidth = 1.0f, .noiseLevel = 0.80f,
            .clickType = 0, .clickLevel = 0.50f, .clickFreq = 3600.0f,
            .distType = 0, .distDrive = 0.0f, .distTone = 0.0f, .distMix = 0.0f,
            .compThresh = -12.0f, .compRatio = 3.0f, .compAttack = 6.0f, .compRelease = 90.0f, .compMakeup = 2.0f,
            .reverbSize = 0.50f, .reverbDecay = 300.0f, .reverbDamp = 0.40f, .reverbGate = 0.0f, .reverbMix = 0.0f,
            .stereoWidth = 1.0f, .outputGain = 0.0f
        }),

        // =========================================================================
        // BL 01 - BL 24: FM BELLS & CHIMES (24 Presets)
        // =========================================================================

        makePreset(PresetDef{
            .name = "BL 01 Tri-State Crystal Bell",
            .voiceMode = 2, .toneNote = 0, .toneOctave = 6, .toneFreq = 1046.5f, .toneQ = 4.0f,
            .toneDecay = 650.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 10.0f, .fmAmount = 0.45f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 380.0f, .noiseCurve = -0.30f, .noiseHPF = 800.0f, .noiseLPF = 16000.0f,
            .noiseStereoWidth = 1.50f, .noiseLevel = 0.40f,
            .clickType = 0, .clickLevel = 0.42f, .clickFreq = 6500.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.15f, .distMix = 0.15f,
            .compThresh = -12.0f, .compRatio = 3.2f, .compAttack = 5.0f, .compRelease = 120.0f, .compMakeup = 2.0f,
            .reverbSize = 0.85f, .reverbDecay = 1400.0f, .reverbDamp = 0.20f, .reverbGate = 0.0f, .reverbMix = 0.42f,
            .midiMode = 1, .stereoWidth = 1.25f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 02 OceanLab Balearic Chime",
            .voiceMode = 2, .toneNote = 4, .toneOctave = 6, .toneFreq = 1318.5f, .toneQ = 4.2f,
            .toneDecay = 720.0f, .tonePitchEnv = 1.5f, .tonePitchDecay = 8.0f, .fmAmount = 0.50f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 420.0f, .noiseCurve = -0.25f, .noiseHPF = 750.0f, .noiseLPF = 16500.0f,
            .noiseStereoWidth = 1.60f, .noiseLevel = 0.38f,
            .clickType = 1, .clickLevel = 0.38f, .clickFreq = 5400.0f,
            .distType = 0, .distDrive = 0.06f, .distTone = 0.12f, .distMix = 0.12f,
            .compThresh = -11.5f, .compRatio = 3.0f, .compAttack = 6.0f, .compRelease = 130.0f, .compMakeup = 2.0f,
            .reverbSize = 0.90f, .reverbDecay = 1600.0f, .reverbDamp = 0.18f, .reverbGate = 0.0f, .reverbMix = 0.46f,
            .midiMode = 1, .stereoWidth = 1.30f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 03 Anjunadeep Glass Bell",
            .voiceMode = 2, .toneNote = 9, .toneOctave = 5, .toneFreq = 880.0f, .toneQ = 3.8f,
            .toneDecay = 550.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 12.0f, .fmAmount = 0.32f, .toneLevel = 0.88f,
            .noiseType = 2, .noiseDecay = 320.0f, .noiseCurve = -0.35f, .noiseHPF = 600.0f, .noiseLPF = 14000.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.32f,
            .clickType = 1, .clickLevel = 0.45f, .clickFreq = 4200.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.05f, .distMix = 0.14f,
            .compThresh = -12.0f, .compRatio = 3.0f, .compAttack = 5.5f, .compRelease = 110.0f, .compMakeup = 2.0f,
            .reverbSize = 0.75f, .reverbDecay = 1100.0f, .reverbDamp = 0.35f, .reverbGate = 0.0f, .reverbMix = 0.36f,
            .midiMode = 1, .stereoWidth = 1.15f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 04 Kandiman 138 Euphoric Bell",
            .voiceMode = 2, .toneNote = 2, .toneOctave = 6, .toneFreq = 1174.66f, .toneQ = 4.5f,
            .toneDecay = 600.0f, .tonePitchEnv = 2.5f, .tonePitchDecay = 10.0f, .fmAmount = 0.58f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 360.0f, .noiseCurve = -0.28f, .noiseHPF = 850.0f, .noiseLPF = 17000.0f,
            .noiseStereoWidth = 1.55f, .noiseLevel = 0.42f,
            .clickType = 0, .clickLevel = 0.48f, .clickFreq = 7000.0f,
            .distType = 0, .distDrive = 0.12f, .distTone = 0.18f, .distMix = 0.18f,
            .compThresh = -12.5f, .compRatio = 3.5f, .compAttack = 4.8f, .compRelease = 115.0f, .compMakeup = 2.2f,
            .reverbSize = 0.82f, .reverbDecay = 1350.0f, .reverbDamp = 0.22f, .reverbGate = 0.0f, .reverbMix = 0.40f,
            .midiMode = 1, .stereoWidth = 1.25f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 05 Venice Beach Sunset Chime",
            .preBursts = 2, .preSpread = 14.0f, .preDecay = 10.0f, .preJitter = 0.12f, .flamSpread = 0.55f, .preLevel = 0.45f,
            .voiceMode = 2, .toneNote = 11, .toneOctave = 5, .toneFreq = 987.77f, .toneQ = 4.0f,
            .toneDecay = 680.0f, .tonePitchEnv = 1.8f, .tonePitchDecay = 9.0f, .fmAmount = 0.48f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 400.0f, .noiseCurve = -0.25f, .noiseHPF = 780.0f, .noiseLPF = 16000.0f,
            .noiseStereoWidth = 1.55f, .noiseLevel = 0.40f,
            .clickType = 1, .clickLevel = 0.40f, .clickFreq = 5800.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.10f, .distMix = 0.14f,
            .compThresh = -12.0f, .compRatio = 3.2f, .compAttack = 5.0f, .compRelease = 120.0f, .compMakeup = 2.0f,
            .reverbSize = 0.88f, .reverbDecay = 1500.0f, .reverbDamp = 0.20f, .reverbGate = 0.0f, .reverbMix = 0.44f,
            .midiMode = 1, .stereoWidth = 1.30f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 06 Childish Gambit FM Bell",
            .voiceMode = 2, .toneNote = 7, .toneOctave = 5, .toneFreq = 783.99f, .toneQ = 4.2f,
            .toneDecay = 420.0f, .tonePitchEnv = 4.5f, .tonePitchDecay = 14.0f, .fmAmount = 0.62f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 280.0f, .noiseCurve = -0.35f, .noiseHPF = 720.0f, .noiseLPF = 15000.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.38f,
            .clickType = 2, .clickLevel = 0.50f, .clickFreq = 5200.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.08f, .distMix = 0.22f,
            .compThresh = -13.0f, .compRatio = 3.8f, .compAttack = 4.0f, .compRelease = 95.0f, .compMakeup = 2.5f,
            .reverbSize = 0.70f, .reverbDecay = 750.0f, .reverbDamp = 0.28f, .reverbGate = 240.0f, .reverbMix = 0.28f,
            .midiMode = 1, .stereoWidth = 1.15f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 07 Lift High Acid Bell",
            .voiceMode = 2, .toneNote = 9, .toneOctave = 5, .toneFreq = 880.0f, .toneQ = 4.8f,
            .toneDecay = 380.0f, .tonePitchEnv = 5.0f, .tonePitchDecay = 14.0f, .fmAmount = 0.75f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 260.0f, .noiseCurve = -0.38f, .noiseHPF = 800.0f, .noiseLPF = 16000.0f,
            .noiseStereoWidth = 1.30f, .noiseLevel = 0.38f,
            .clickType = 3, .clickLevel = 0.54f, .clickFreq = 5800.0f,
            .distType = 3, .distDrive = 0.32f, .distTone = 0.15f, .distMix = 0.40f,
            .compThresh = -14.5f, .compRatio = 4.5f, .compAttack = 3.0f, .compRelease = 85.0f, .compMakeup = 3.2f,
            .reverbSize = 0.65f, .reverbDecay = 620.0f, .reverbDamp = 0.32f, .reverbGate = 200.0f, .reverbMix = 0.26f,
            .midiMode = 1, .stereoWidth = 1.15f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "BL 08 Tytanium Metallic Cowbell",
            .voiceMode = 2, .toneNote = 2, .toneOctave = 5, .toneFreq = 587.33f, .toneQ = 4.5f,
            .toneDecay = 280.0f, .tonePitchEnv = 3.0f, .tonePitchDecay = 12.0f, .fmAmount = 0.65f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 200.0f, .noiseCurve = -0.40f, .noiseHPF = 650.0f, .noiseLPF = 14000.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.36f,
            .clickType = 2, .clickLevel = 0.58f, .clickFreq = 4600.0f,
            .distType = 2, .distDrive = 0.24f, .distTone = 0.05f, .distMix = 0.30f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 3.2f, .compRelease = 75.0f, .compMakeup = 3.0f,
            .reverbSize = 0.55f, .reverbDecay = 480.0f, .reverbDamp = 0.38f, .reverbGate = 160.0f, .reverbMix = 0.22f,
            .midiMode = 1, .stereoWidth = 1.10f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "BL 09 Gouryella Heaven Bell",
            .voiceMode = 2, .toneNote = 4, .toneOctave = 6, .toneFreq = 1318.51f, .toneQ = 4.5f,
            .toneDecay = 850.0f, .tonePitchEnv = 1.5f, .tonePitchDecay = 8.0f, .fmAmount = 0.52f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 450.0f, .noiseCurve = -0.22f, .noiseHPF = 850.0f, .noiseLPF = 17500.0f,
            .noiseStereoWidth = 1.65f, .noiseLevel = 0.42f,
            .clickType = 1, .clickLevel = 0.42f, .clickFreq = 6200.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.12f, .distMix = 0.15f,
            .compThresh = -11.5f, .compRatio = 3.0f, .compAttack = 6.0f, .compRelease = 140.0f, .compMakeup = 2.0f,
            .reverbSize = 0.95f, .reverbDecay = 1900.0f, .reverbDamp = 0.15f, .reverbGate = 0.0f, .reverbMix = 0.50f,
            .midiMode = 1, .stereoWidth = 1.35f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 10 Punk Metallic Bell Smack",
            .voiceMode = 2, .toneNote = 4, .toneOctave = 5, .toneFreq = 659.25f, .toneQ = 4.5f,
            .toneDecay = 340.0f, .tonePitchEnv = 6.0f, .tonePitchDecay = 15.0f, .fmAmount = 0.70f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 240.0f, .noiseCurve = -0.38f, .noiseHPF = 700.0f, .noiseLPF = 15000.0f,
            .noiseStereoWidth = 1.25f, .noiseLevel = 0.40f,
            .clickType = 2, .clickLevel = 0.60f, .clickFreq = 4800.0f,
            .distType = 2, .distDrive = 0.34f, .distTone = 0.08f, .distMix = 0.40f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 2.8f, .compRelease = 80.0f, .compMakeup = 3.6f,
            .reverbSize = 0.60f, .reverbDecay = 520.0f, .reverbDamp = 0.35f, .reverbGate = 180.0f, .reverbMix = 0.24f,
            .midiMode = 1, .stereoWidth = 1.10f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "BL 11 Kyoto Ritual Bell",
            .voiceMode = 2, .toneNote = 0, .toneOctave = 4, .toneFreq = 261.63f, .toneQ = 4.2f,
            .toneDecay = 900.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 12.0f, .fmAmount = 0.40f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 480.0f, .noiseCurve = -0.25f, .noiseHPF = 450.0f, .noiseLPF = 12000.0f,
            .noiseStereoWidth = 1.40f, .noiseLevel = 0.35f,
            .clickType = 1, .clickLevel = 0.48f, .clickFreq = 2800.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.02f, .distMix = 0.15f,
            .compThresh = -12.0f, .compRatio = 3.2f, .compAttack = 5.5f, .compRelease = 130.0f, .compMakeup = 2.0f,
            .reverbSize = 0.88f, .reverbDecay = 1700.0f, .reverbDamp = 0.24f, .reverbGate = 0.0f, .reverbMix = 0.42f,
            .midiMode = 1, .stereoWidth = 1.25f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 12 Who's Afraid 138 Ride Bell",
            .voiceMode = 2, .toneNote = 7, .toneOctave = 6, .toneFreq = 1567.98f, .toneQ = 4.6f,
            .toneDecay = 320.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 10.0f, .fmAmount = 0.68f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 220.0f, .noiseCurve = -0.38f, .noiseHPF = 950.0f, .noiseLPF = 18000.0f,
            .noiseStereoWidth = 1.40f, .noiseLevel = 0.42f,
            .clickType = 2, .clickLevel = 0.55f, .clickFreq = 7200.0f,
            .distType = 1, .distDrive = 0.15f, .distTone = 0.12f, .distMix = 0.22f,
            .compThresh = -13.0f, .compRatio = 4.0f, .compAttack = 3.5f, .compRelease = 85.0f, .compMakeup = 2.6f,
            .reverbSize = 0.64f, .reverbDecay = 560.0f, .reverbDamp = 0.28f, .reverbGate = 180.0f, .reverbMix = 0.25f,
            .midiMode = 1, .stereoWidth = 1.20f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "BL 13 Black Hole Cosmic Bell",
            .voiceMode = 2, .toneNote = 11, .toneOctave = 5, .toneFreq = 987.77f, .toneQ = 4.4f,
            .toneDecay = 750.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 10.0f, .fmAmount = 0.55f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 420.0f, .noiseCurve = -0.26f, .noiseHPF = 820.0f, .noiseLPF = 16500.0f,
            .noiseStereoWidth = 1.65f, .noiseLevel = 0.42f,
            .clickType = 1, .clickLevel = 0.44f, .clickFreq = 5800.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.10f, .distMix = 0.15f,
            .compThresh = -12.0f, .compRatio = 3.2f, .compAttack = 5.2f, .compRelease = 125.0f, .compMakeup = 2.0f,
            .reverbSize = 0.90f, .reverbDecay = 1600.0f, .reverbDamp = 0.20f, .reverbGate = 0.0f, .reverbMix = 0.45f,
            .midiMode = 1, .stereoWidth = 1.30f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 14 Bonzai Rave Cowbell",
            .voiceMode = 2, .toneNote = 2, .toneOctave = 5, .toneFreq = 587.33f, .toneQ = 4.4f,
            .toneDecay = 250.0f, .tonePitchEnv = 3.5f, .tonePitchDecay = 11.0f, .fmAmount = 0.60f, .toneLevel = 0.92f,
            .noiseType = 3, .noiseDecay = 180.0f, .noiseCurve = -0.42f, .noiseHPF = 600.0f, .noiseLPF = 13500.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.36f,
            .clickType = 2, .clickLevel = 0.60f, .clickFreq = 4400.0f,
            .distType = 1, .distDrive = 0.28f, .distTone = 0.08f, .distMix = 0.32f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 3.0f, .compRelease = 70.0f, .compMakeup = 3.2f,
            .reverbSize = 0.52f, .reverbDecay = 420.0f, .reverbDamp = 0.40f, .reverbGate = 150.0f, .reverbMix = 0.20f,
            .midiMode = 1, .stereoWidth = 1.08f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "BL 15 Aalto Pluck Bell",
            .voiceMode = 2, .toneNote = 6, .toneOctave = 5, .toneFreq = 739.99f, .toneQ = 4.0f,
            .toneDecay = 320.0f, .tonePitchEnv = 2.5f, .tonePitchDecay = 11.0f, .fmAmount = 0.42f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 210.0f, .noiseCurve = -0.38f, .noiseHPF = 650.0f, .noiseLPF = 14500.0f,
            .noiseStereoWidth = 1.30f, .noiseLevel = 0.32f,
            .clickType = 1, .clickLevel = 0.52f, .clickFreq = 4500.0f,
            .distType = 0, .distDrive = 0.12f, .distTone = 0.06f, .distMix = 0.18f,
            .compThresh = -12.5f, .compRatio = 3.5f, .compAttack = 4.2f, .compRelease = 85.0f, .compMakeup = 2.4f,
            .reverbSize = 0.65f, .reverbDecay = 620.0f, .reverbDamp = 0.32f, .reverbGate = 200.0f, .reverbMix = 0.26f,
            .midiMode = 1, .stereoWidth = 1.15f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 16 Solaris Shimmer Bell",
            .voiceMode = 2, .toneNote = 9, .toneOctave = 6, .toneFreq = 1760.0f, .toneQ = 4.6f,
            .toneDecay = 700.0f, .tonePitchEnv = 1.5f, .tonePitchDecay = 8.0f, .fmAmount = 0.48f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 480.0f, .noiseCurve = -0.24f, .noiseHPF = 850.0f, .noiseLPF = 17000.0f,
            .noiseStereoWidth = 1.60f, .noiseLevel = 0.40f,
            .clickType = 1, .clickLevel = 0.38f, .clickFreq = 6400.0f,
            .distType = 0, .distDrive = 0.06f, .distTone = 0.12f, .distMix = 0.12f,
            .compThresh = -11.5f, .compRatio = 3.0f, .compAttack = 6.0f, .compRelease = 130.0f, .compMakeup = 2.0f,
            .reverbSize = 0.88f, .reverbDecay = 1550.0f, .reverbDamp = 0.18f, .reverbGate = 0.0f, .reverbMix = 0.44f,
            .midiMode = 1, .stereoWidth = 1.30f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 17 Enhanced Progressive Chime",
            .voiceMode = 2, .toneNote = 7, .toneOctave = 5, .toneFreq = 783.99f, .toneQ = 4.0f,
            .toneDecay = 560.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 10.0f, .fmAmount = 0.45f, .toneLevel = 0.88f,
            .noiseType = 3, .noiseDecay = 340.0f, .noiseCurve = -0.30f, .noiseHPF = 720.0f, .noiseLPF = 15000.0f,
            .noiseStereoWidth = 1.40f, .noiseLevel = 0.36f,
            .clickType = 0, .clickLevel = 0.44f, .clickFreq = 5400.0f,
            .distType = 0, .distDrive = 0.10f, .distTone = 0.08f, .distMix = 0.16f,
            .compThresh = -12.0f, .compRatio = 3.2f, .compAttack = 5.0f, .compRelease = 110.0f, .compMakeup = 2.0f,
            .reverbSize = 0.78f, .reverbDecay = 1150.0f, .reverbDamp = 0.26f, .reverbGate = 0.0f, .reverbMix = 0.35f,
            .midiMode = 1, .stereoWidth = 1.20f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 18 Mau5 Pluck Bell Ping",
            .voiceMode = 2, .toneNote = 0, .toneOctave = 6, .toneFreq = 1046.5f, .toneQ = 3.8f,
            .toneDecay = 260.0f, .tonePitchEnv = 2.0f, .tonePitchDecay = 8.0f, .fmAmount = 0.38f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 160.0f, .noiseCurve = -0.44f, .noiseHPF = 750.0f, .noiseLPF = 14000.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.28f,
            .clickType = 0, .clickLevel = 0.50f, .clickFreq = 5800.0f,
            .distType = 0, .distDrive = 0.10f, .distTone = 0.04f, .distMix = 0.15f,
            .compThresh = -12.5f, .compRatio = 3.5f, .compAttack = 4.0f, .compRelease = 70.0f, .compMakeup = 2.2f,
            .reverbSize = 0.50f, .reverbDecay = 350.0f, .reverbDamp = 0.40f, .reverbGate = 120.0f, .reverbMix = 0.16f,
            .midiMode = 1, .stereoWidth = 1.10f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 19 Silk Music Sunset Bell",
            .voiceMode = 2, .toneNote = 4, .toneOctave = 5, .toneFreq = 659.25f, .toneQ = 3.8f,
            .toneDecay = 620.0f, .tonePitchEnv = 1.8f, .tonePitchDecay = 10.0f, .fmAmount = 0.36f, .toneLevel = 0.88f,
            .noiseType = 2, .noiseDecay = 380.0f, .noiseCurve = -0.28f, .noiseHPF = 650.0f, .noiseLPF = 14500.0f,
            .noiseStereoWidth = 1.45f, .noiseLevel = 0.34f,
            .clickType = 1, .clickLevel = 0.42f, .clickFreq = 3800.0f,
            .distType = 0, .distDrive = 0.06f, .distTone = 0.05f, .distMix = 0.12f,
            .compThresh = -11.8f, .compRatio = 3.0f, .compAttack = 5.5f, .compRelease = 120.0f, .compMakeup = 2.0f,
            .reverbSize = 0.84f, .reverbDecay = 1300.0f, .reverbDamp = 0.25f, .reverbGate = 0.0f, .reverbMix = 0.38f,
            .midiMode = 1, .stereoWidth = 1.25f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 20 Goa 909 Ride Bell",
            .voiceMode = 2, .toneNote = 6, .toneOctave = 6, .toneFreq = 1479.98f, .toneQ = 4.4f,
            .toneDecay = 340.0f, .tonePitchEnv = 2.5f, .tonePitchDecay = 10.0f, .fmAmount = 0.65f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 240.0f, .noiseCurve = -0.36f, .noiseHPF = 900.0f, .noiseLPF = 17500.0f,
            .noiseStereoWidth = 1.40f, .noiseLevel = 0.40f,
            .clickType = 3, .clickLevel = 0.52f, .clickFreq = 6800.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = 0.12f, .distMix = 0.25f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 3.5f, .compRelease = 85.0f, .compMakeup = 2.8f,
            .reverbSize = 0.65f, .reverbDecay = 580.0f, .reverbDamp = 0.30f, .reverbGate = 190.0f, .reverbMix = 0.26f,
            .midiMode = 1, .stereoWidth = 1.20f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "BL 21 Berlin Industrial Anvil Bell",
            .voiceMode = 2, .toneNote = 2, .toneOctave = 4, .toneFreq = 293.66f, .toneQ = 4.8f,
            .toneDecay = 460.0f, .tonePitchEnv = 4.0f, .tonePitchDecay = 14.0f, .fmAmount = 0.82f, .toneLevel = 0.94f,
            .noiseType = 3, .noiseDecay = 320.0f, .noiseCurve = -0.32f, .noiseHPF = 550.0f, .noiseLPF = 14000.0f,
            .noiseStereoWidth = 1.35f, .noiseLevel = 0.45f,
            .clickType = 2, .clickLevel = 0.70f, .clickFreq = 4800.0f,
            .distType = 3, .distDrive = 0.38f, .distTone = 0.14f, .distMix = 0.45f,
            .compThresh = -16.0f, .compRatio = 5.5f, .compAttack = 2.5f, .compRelease = 90.0f, .compMakeup = 4.0f,
            .reverbSize = 0.72f, .reverbDecay = 720.0f, .reverbDamp = 0.30f, .reverbGate = 220.0f, .reverbMix = 0.30f,
            .midiMode = 1, .stereoWidth = 1.20f, .outputGain = 0.8f
        }),

        makePreset(PresetDef{
            .name = "BL 22 Café del Mar Harmonic Chime",
            .voiceMode = 2, .toneNote = 9, .toneOctave = 5, .toneFreq = 880.0f, .toneQ = 4.2f,
            .toneDecay = 820.0f, .tonePitchEnv = 1.5f, .tonePitchDecay = 8.0f, .fmAmount = 0.44f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 480.0f, .noiseCurve = -0.22f, .noiseHPF = 700.0f, .noiseLPF = 16000.0f,
            .noiseStereoWidth = 1.60f, .noiseLevel = 0.40f,
            .clickType = 1, .clickLevel = 0.40f, .clickFreq = 4800.0f,
            .distType = 0, .distDrive = 0.06f, .distTone = 0.08f, .distMix = 0.12f,
            .compThresh = -11.5f, .compRatio = 3.0f, .compAttack = 6.0f, .compRelease = 135.0f, .compMakeup = 2.0f,
            .reverbSize = 0.92f, .reverbDecay = 1800.0f, .reverbDamp = 0.18f, .reverbGate = 0.0f, .reverbMix = 0.48f,
            .midiMode = 1, .stereoWidth = 1.30f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "BL 23 Melodic Keyboard FM Bell",
            .voiceMode = 2, .toneNote = 0, .toneOctave = 5, .toneFreq = 523.25f, .toneQ = 4.0f,
            .toneDecay = 520.0f, .tonePitchEnv = 1.5f, .tonePitchDecay = 9.0f, .fmAmount = 0.46f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 300.0f, .noiseCurve = -0.32f, .noiseHPF = 750.0f, .noiseLPF = 15500.0f,
            .noiseStereoWidth = 1.40f, .noiseLevel = 0.35f,
            .clickType = 0, .clickLevel = 0.44f, .clickFreq = 5200.0f,
            .distType = 0, .distDrive = 0.08f, .distTone = 0.06f, .distMix = 0.15f,
            .compThresh = -12.0f, .compRatio = 3.2f, .compAttack = 5.0f, .compRelease = 105.0f, .compMakeup = 2.0f,
            .reverbSize = 0.75f, .reverbDecay = 950.0f, .reverbDamp = 0.26f, .reverbGate = 0.0f, .reverbMix = 0.32f,
            .midiMode = 1, .stereoWidth = 1.20f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "BL 24 Beat End Reverse Bell",
            .voiceMode = 2, .toneNote = 4, .toneOctave = 6, .toneFreq = 1318.51f, .toneQ = 4.2f,
            .toneDecay = 650.0f, .tonePitchEnv = 1.5f, .tonePitchDecay = 8.0f, .fmAmount = 0.50f, .toneLevel = 0.90f,
            .noiseType = 3, .noiseDecay = 380.0f, .noiseCurve = -0.28f, .noiseHPF = 800.0f, .noiseLPF = 16000.0f,
            .noiseStereoWidth = 1.50f, .noiseLevel = 0.40f,
            .clickType = 1, .clickLevel = 0.40f, .clickFreq = 5600.0f,
            .distType = 0, .distDrive = 0.10f, .distTone = 0.10f, .distMix = 0.16f,
            .compThresh = -12.5f, .compRatio = 3.4f, .compAttack = 5.0f, .compRelease = 115.0f, .compMakeup = 2.2f,
            .reverbSize = 0.85f, .reverbDecay = 1400.0f, .reverbDamp = 0.22f, .reverbGate = 0.0f, .reverbMix = 0.40f,
            .reverseMode = 1, .reverseMix = 0.60f, .midiMode = 1, .stereoWidth = 1.25f, .outputGain = 0.2f
        }),

        // =========================================================================
        // TM 01 - TM 24: CLUB TOM-TOMS (24 Presets)
        // =========================================================================

        makePreset(PresetDef{
            .name = "TM 01 Anjunabeats Groove Tom",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.5f,
            .toneDecay = 260.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 28.0f, .fmAmount = 0.35f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 90.0f, .noiseCurve = -0.45f, .noiseHPF = 200.0f, .noiseLPF = 6500.0f,
            .noiseStereoWidth = 0.95f, .noiseLevel = 0.25f,
            .clickType = 1, .clickLevel = 0.50f, .clickFreq = 1800.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = -0.05f, .distMix = 0.24f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 5.0f, .compRelease = 85.0f, .compMakeup = 3.0f,
            .reverbSize = 0.55f, .reverbDecay = 380.0f, .reverbDamp = 0.45f, .reverbGate = 140.0f, .reverbMix = 0.18f,
            .stereoWidth = 1.0f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "TM 02 Kandi Melodic Tom Fill",
            .voiceMode = 1, .toneNote = 0, .toneOctave = 3, .toneFreq = 130.81f, .toneQ = 3.6f,
            .toneDecay = 310.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 32.0f, .fmAmount = 0.40f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 110.0f, .noiseCurve = -0.40f, .noiseHPF = 220.0f, .noiseLPF = 7200.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.28f,
            .clickType = 1, .clickLevel = 0.48f, .clickFreq = 2200.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.02f, .distMix = 0.22f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 4.8f, .compRelease = 90.0f, .compMakeup = 2.8f,
            .reverbSize = 0.65f, .reverbDecay = 480.0f, .reverbDamp = 0.38f, .reverbGate = 180.0f, .reverbMix = 0.24f,
            .midiMode = 1, .stereoWidth = 1.10f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "TM 03 Sean Tyas Psy-Tech Laser Tom",
            .voiceMode = 1, .toneNote = 7, .toneOctave = 2, .toneFreq = 98.0f, .toneQ = 3.8f,
            .toneDecay = 190.0f, .tonePitchEnv = 24.0f, .tonePitchDecay = 18.0f, .fmAmount = 0.50f, .toneLevel = 0.94f,
            .noiseType = 1, .noiseDecay = 95.0f, .noiseCurve = -0.48f, .noiseHPF = 300.0f, .noiseLPF = 8500.0f,
            .noiseStereoWidth = 1.05f, .noiseLevel = 0.32f,
            .clickType = 3, .clickLevel = 0.62f, .clickFreq = 3800.0f,
            .distType = 2, .distDrive = 0.32f, .distTone = 0.05f, .distMix = 0.36f,
            .compThresh = -16.0f, .compRatio = 6.0f, .compAttack = 2.5f, .compRelease = 65.0f, .compMakeup = 4.0f,
            .reverbSize = 0.48f, .reverbDecay = 320.0f, .reverbDamp = 0.45f, .reverbGate = 110.0f, .reverbMix = 0.15f,
            .stereoWidth = 1.05f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "TM 04 Rotterdam Stadium Floor Tom",
            .voiceMode = 1, .toneNote = 5, .toneOctave = 2, .toneFreq = 87.31f, .toneQ = 3.5f,
            .toneDecay = 380.0f, .tonePitchEnv = 16.0f, .tonePitchDecay = 35.0f, .fmAmount = 0.38f, .toneLevel = 0.94f,
            .noiseType = 2, .noiseDecay = 120.0f, .noiseCurve = -0.38f, .noiseHPF = 180.0f, .noiseLPF = 6000.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.28f,
            .clickType = 1, .clickLevel = 0.55f, .clickFreq = 1600.0f,
            .distType = 1, .distDrive = 0.26f, .distTone = -0.05f, .distMix = 0.32f,
            .compThresh = -15.0f, .compRatio = 5.2f, .compAttack = 4.0f, .compRelease = 100.0f, .compMakeup = 4.0f,
            .reverbSize = 0.80f, .reverbDecay = 650.0f, .reverbDamp = 0.32f, .reverbGate = 220.0f, .reverbMix = 0.28f,
            .stereoWidth = 1.15f, .outputGain = 0.8f
        }),

        makePreset(PresetDef{
            .name = "TM 05 Coldharbour Deep Rolling Tom",
            .voiceMode = 1, .toneNote = 2, .toneOctave = 2, .toneFreq = 73.42f, .toneQ = 3.4f,
            .toneDecay = 240.0f, .tonePitchEnv = 13.0f, .tonePitchDecay = 25.0f, .fmAmount = 0.32f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 85.0f, .noiseCurve = -0.45f, .noiseHPF = 190.0f, .noiseLPF = 5800.0f,
            .noiseStereoWidth = 0.95f, .noiseLevel = 0.22f,
            .clickType = 1, .clickLevel = 0.48f, .clickFreq = 1500.0f,
            .distType = 1, .distDrive = 0.20f, .distTone = -0.15f, .distMix = 0.26f,
            .compThresh = -14.2f, .compRatio = 4.6f, .compAttack = 4.5f, .compRelease = 80.0f, .compMakeup = 3.0f,
            .reverbSize = 0.52f, .reverbDecay = 360.0f, .reverbDamp = 0.48f, .reverbGate = 130.0f, .reverbMix = 0.16f,
            .stereoWidth = 1.0f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "TM 06 Aly & Fila Cairo Tribal Tom",
            .preBursts = 2, .preSpread = 10.0f, .preDecay = 7.0f, .preJitter = 0.12f, .flamSpread = 0.40f, .preLevel = 0.50f,
            .voiceMode = 1, .toneNote = 7, .toneOctave = 2, .toneFreq = 98.0f, .toneQ = 3.6f,
            .toneDecay = 250.0f, .tonePitchEnv = 15.0f, .tonePitchDecay = 26.0f, .fmAmount = 0.36f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 95.0f, .noiseCurve = -0.42f, .noiseHPF = 210.0f, .noiseLPF = 6800.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.26f,
            .clickType = 1, .clickLevel = 0.54f, .clickFreq = 2100.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = 0.02f, .distMix = 0.24f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.2f, .compRelease = 85.0f, .compMakeup = 3.0f,
            .reverbSize = 0.62f, .reverbDecay = 450.0f, .reverbDamp = 0.38f, .reverbGate = 160.0f, .reverbMix = 0.22f,
            .stereoWidth = 1.10f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "TM 07 High Contrast Tribal Tom",
            .preBursts = 3, .preSpread = 12.0f, .preDecay = 8.5f, .preJitter = 0.14f, .flamSpread = 0.50f, .preLevel = 0.58f,
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.5f,
            .toneDecay = 280.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 28.0f, .fmAmount = 0.36f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 105.0f, .noiseCurve = -0.40f, .noiseHPF = 220.0f, .noiseLPF = 7000.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.28f,
            .clickType = 0, .clickLevel = 0.52f, .clickFreq = 2400.0f,
            .distType = 1, .distDrive = 0.22f, .distTone = 0.06f, .distMix = 0.28f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 3.8f, .compRelease = 90.0f, .compMakeup = 3.4f,
            .reverbSize = 0.75f, .reverbDecay = 550.0f, .reverbDamp = 0.32f, .reverbGate = 190.0f, .reverbMix = 0.26f,
            .stereoWidth = 1.15f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "TM 08 Gatecrasher 2000 Synth Tom",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.8f,
            .toneDecay = 270.0f, .tonePitchEnv = 18.0f, .tonePitchDecay = 30.0f, .fmAmount = 0.44f, .toneLevel = 0.92f,
            .noiseType = 1, .noiseDecay = 110.0f, .noiseCurve = -0.42f, .noiseHPF = 250.0f, .noiseLPF = 8000.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.30f,
            .clickType = 3, .clickLevel = 0.56f, .clickFreq = 3200.0f,
            .distType = 0, .distDrive = 0.22f, .distTone = 0.08f, .distMix = 0.28f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 3.8f, .compRelease = 85.0f, .compMakeup = 3.2f,
            .reverbSize = 0.68f, .reverbDecay = 500.0f, .reverbDamp = 0.35f, .reverbGate = 180.0f, .reverbMix = 0.24f,
            .stereoWidth = 1.10f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "TM 09 Anjunadeep Warm Percussive Tom",
            .voiceMode = 1, .toneNote = 0, .toneOctave = 3, .toneFreq = 130.81f, .toneQ = 3.3f,
            .toneDecay = 290.0f, .tonePitchEnv = 9.0f, .tonePitchDecay = 28.0f, .fmAmount = 0.25f, .toneLevel = 0.88f,
            .noiseType = 2, .noiseDecay = 100.0f, .noiseCurve = -0.42f, .noiseHPF = 200.0f, .noiseLPF = 6200.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.24f,
            .clickType = 1, .clickLevel = 0.46f, .clickFreq = 1600.0f,
            .distType = 0, .distDrive = 0.12f, .distTone = -0.05f, .distMix = 0.18f,
            .compThresh = -13.0f, .compRatio = 3.8f, .compAttack = 5.5f, .compRelease = 90.0f, .compMakeup = 2.5f,
            .reverbSize = 0.64f, .reverbDecay = 480.0f, .reverbDamp = 0.40f, .reverbGate = 160.0f, .reverbMix = 0.22f,
            .stereoWidth = 1.05f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "TM 10 Prydz Muranyi Electro Tom",
            .voiceMode = 1, .toneNote = 6, .toneOctave = 2, .toneFreq = 92.5f, .toneQ = 3.6f,
            .toneDecay = 210.0f, .tonePitchEnv = 15.0f, .tonePitchDecay = 22.0f, .fmAmount = 0.38f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 90.0f, .noiseCurve = -0.46f, .noiseHPF = 240.0f, .noiseLPF = 7200.0f,
            .noiseStereoWidth = 1.05f, .noiseLevel = 0.26f,
            .clickType = 0, .clickLevel = 0.54f, .clickFreq = 2600.0f,
            .distType = 0, .distDrive = 0.24f, .distTone = 0.04f, .distMix = 0.28f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 3.5f, .compRelease = 75.0f, .compMakeup = 3.5f,
            .reverbSize = 0.50f, .reverbDecay = 340.0f, .reverbDamp = 0.44f, .reverbGate = 120.0f, .reverbMix = 0.15f,
            .stereoWidth = 1.05f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "TM 11 Ferry Corsten Rock Your Body Tom",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.7f,
            .toneDecay = 260.0f, .tonePitchEnv = 17.0f, .tonePitchDecay = 26.0f, .fmAmount = 0.42f, .toneLevel = 0.92f,
            .noiseType = 1, .noiseDecay = 100.0f, .noiseCurve = -0.42f, .noiseHPF = 260.0f, .noiseLPF = 7800.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.28f,
            .clickType = 2, .clickLevel = 0.58f, .clickFreq = 2800.0f,
            .distType = 1, .distDrive = 0.28f, .distTone = 0.08f, .distMix = 0.32f,
            .compThresh = -15.0f, .compRatio = 5.2f, .compAttack = 3.5f, .compRelease = 80.0f, .compMakeup = 3.6f,
            .reverbSize = 0.62f, .reverbDecay = 450.0f, .reverbDamp = 0.36f, .reverbGate = 170.0f, .reverbMix = 0.22f,
            .stereoWidth = 1.10f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "TM 12 Simmons 80s Trance Synth Tom",
            .voiceMode = 1, .toneNote = 7, .toneOctave = 2, .toneFreq = 98.0f, .toneQ = 3.8f,
            .toneDecay = 320.0f, .tonePitchEnv = 22.0f, .tonePitchDecay = 32.0f, .fmAmount = 0.45f, .toneLevel = 0.94f,
            .noiseType = 1, .noiseDecay = 130.0f, .noiseCurve = -0.40f, .noiseHPF = 280.0f, .noiseLPF = 8200.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.32f,
            .clickType = 3, .clickLevel = 0.60f, .clickFreq = 3600.0f,
            .distType = 0, .distDrive = 0.22f, .distTone = 0.08f, .distMix = 0.28f,
            .compThresh = -14.8f, .compRatio = 5.0f, .compAttack = 3.6f, .compRelease = 85.0f, .compMakeup = 3.4f,
            .reverbSize = 0.70f, .reverbDecay = 520.0f, .reverbDamp = 0.34f, .reverbGate = 190.0f, .reverbMix = 0.25f,
            .stereoWidth = 1.10f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "TM 13 Subculture 140 Rolling Bass Tom",
            .voiceMode = 1, .toneNote = 0, .toneOctave = 2, .toneFreq = 65.41f, .toneQ = 3.4f,
            .toneDecay = 340.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 30.0f, .fmAmount = 0.32f, .toneLevel = 0.94f,
            .noiseType = 2, .noiseDecay = 80.0f, .noiseCurve = -0.48f, .noiseHPF = 180.0f, .noiseLPF = 5500.0f,
            .noiseStereoWidth = 0.90f, .noiseLevel = 0.20f,
            .clickType = 1, .clickLevel = 0.46f, .clickFreq = 1400.0f,
            .distType = 2, .distDrive = 0.22f, .distTone = -0.10f, .distMix = 0.28f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 4.0f, .compRelease = 80.0f, .compMakeup = 3.5f,
            .reverbSize = 0.50f, .reverbDecay = 320.0f, .reverbDamp = 0.50f, .reverbGate = 130.0f, .reverbMix = 0.14f,
            .stereoWidth = 0.95f, .outputGain = 0.4f
        }),

        makePreset(PresetDef{
            .name = "TM 14 Above & Beyond Red Rocks Canyon Tom",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.5f,
            .toneDecay = 330.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 30.0f, .fmAmount = 0.36f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 120.0f, .noiseCurve = -0.38f, .noiseHPF = 210.0f, .noiseLPF = 6800.0f,
            .noiseStereoWidth = 1.30f, .noiseLevel = 0.28f,
            .clickType = 1, .clickLevel = 0.50f, .clickFreq = 2000.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.04f, .distMix = 0.22f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.5f, .compRelease = 95.0f, .compMakeup = 3.0f,
            .reverbSize = 0.90f, .reverbDecay = 950.0f, .reverbDamp = 0.25f, .reverbGate = 320.0f, .reverbMix = 0.36f,
            .stereoWidth = 1.25f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "TM 15 Armada Progressive Groove Tom",
            .voiceMode = 1, .toneNote = 7, .toneOctave = 2, .toneFreq = 98.0f, .toneQ = 3.5f,
            .toneDecay = 240.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 24.0f, .fmAmount = 0.32f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 85.0f, .noiseCurve = -0.45f, .noiseHPF = 200.0f, .noiseLPF = 6400.0f,
            .noiseStereoWidth = 1.05f, .noiseLevel = 0.24f,
            .clickType = 0, .clickLevel = 0.48f, .clickFreq = 2200.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.02f, .distMix = 0.22f,
            .compThresh = -13.8f, .compRatio = 4.4f, .compAttack = 4.2f, .compRelease = 80.0f, .compMakeup = 2.8f,
            .reverbSize = 0.55f, .reverbDecay = 380.0f, .reverbDamp = 0.42f, .reverbGate = 140.0f, .reverbMix = 0.18f,
            .stereoWidth = 1.05f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "TM 16 Vandit Paul's Octapad Tom",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.6f,
            .toneDecay = 260.0f, .tonePitchEnv = 14.0f, .tonePitchDecay = 26.0f, .fmAmount = 0.36f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 95.0f, .noiseCurve = -0.42f, .noiseHPF = 210.0f, .noiseLPF = 6600.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.26f,
            .clickType = 0, .clickLevel = 0.50f, .clickFreq = 2200.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = 0.04f, .distMix = 0.24f,
            .compThresh = -14.0f, .compRatio = 4.5f, .compAttack = 4.0f, .compRelease = 80.0f, .compMakeup = 3.0f,
            .reverbSize = 0.60f, .reverbDecay = 420.0f, .reverbDamp = 0.38f, .reverbGate = 160.0f, .reverbMix = 0.20f,
            .stereoWidth = 1.08f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "TM 17 Astrix & Tyas Psy Zapper Tom",
            .voiceMode = 1, .toneNote = 4, .toneOctave = 2, .toneFreq = 82.41f, .toneQ = 3.8f,
            .toneDecay = 180.0f, .tonePitchEnv = 26.0f, .tonePitchDecay = 16.0f, .fmAmount = 0.52f, .toneLevel = 0.94f,
            .noiseType = 1, .noiseDecay = 90.0f, .noiseCurve = -0.50f, .noiseHPF = 320.0f, .noiseLPF = 9000.0f,
            .noiseStereoWidth = 1.05f, .noiseLevel = 0.32f,
            .clickType = 3, .clickLevel = 0.65f, .clickFreq = 4200.0f,
            .distType = 2, .distDrive = 0.34f, .distTone = 0.08f, .distMix = 0.38f,
            .compThresh = -16.0f, .compRatio = 6.0f, .compAttack = 2.2f, .compRelease = 60.0f, .compMakeup = 4.2f,
            .reverbSize = 0.46f, .reverbDecay = 300.0f, .reverbDamp = 0.46f, .reverbGate = 110.0f, .reverbMix = 0.15f,
            .stereoWidth = 1.05f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "TM 18 Gouryella Ligaya High Melodic Tom",
            .voiceMode = 1, .toneNote = 0, .toneOctave = 4, .toneFreq = 261.63f, .toneQ = 3.6f,
            .toneDecay = 340.0f, .tonePitchEnv = 11.0f, .tonePitchDecay = 28.0f, .fmAmount = 0.38f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 110.0f, .noiseCurve = -0.38f, .noiseHPF = 250.0f, .noiseLPF = 7500.0f,
            .noiseStereoWidth = 1.20f, .noiseLevel = 0.28f,
            .clickType = 1, .clickLevel = 0.48f, .clickFreq = 2600.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.06f, .distMix = 0.22f,
            .compThresh = -13.5f, .compRatio = 4.0f, .compAttack = 4.5f, .compRelease = 90.0f, .compMakeup = 2.8f,
            .reverbSize = 0.70f, .reverbDecay = 600.0f, .reverbDamp = 0.34f, .reverbGate = 200.0f, .reverbMix = 0.26f,
            .midiMode = 1, .stereoWidth = 1.15f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "TM 19 Mau5 Progressive Tight Tom",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.5f,
            .toneDecay = 180.0f, .tonePitchEnv = 13.0f, .tonePitchDecay = 20.0f, .fmAmount = 0.30f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 75.0f, .noiseCurve = -0.48f, .noiseHPF = 220.0f, .noiseLPF = 6000.0f,
            .noiseStereoWidth = 0.95f, .noiseLevel = 0.20f,
            .clickType = 0, .clickLevel = 0.50f, .clickFreq = 2400.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.0f, .distMix = 0.20f,
            .compThresh = -15.0f, .compRatio = 4.8f, .compAttack = 3.0f, .compRelease = 70.0f, .compMakeup = 3.2f,
            .reverbSize = 0.50f, .reverbDecay = 300.0f, .reverbDamp = 0.40f, .reverbGate = 0.0f, .reverbMix = 0.0f,
            .stereoWidth = 1.0f, .outputGain = 0.0f
        }),

        makePreset(PresetDef{
            .name = "TM 20 Ibiza Terrace Bongo-Tom",
            .voiceMode = 1, .toneNote = 2, .toneOctave = 4, .toneFreq = 293.66f, .toneQ = 3.8f,
            .toneDecay = 210.0f, .tonePitchEnv = 10.0f, .tonePitchDecay = 20.0f, .fmAmount = 0.34f, .toneLevel = 0.88f,
            .noiseType = 2, .noiseDecay = 95.0f, .noiseCurve = -0.42f, .noiseHPF = 260.0f, .noiseLPF = 8000.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.28f,
            .clickType = 1, .clickLevel = 0.52f, .clickFreq = 2800.0f,
            .distType = 0, .distDrive = 0.14f, .distTone = 0.05f, .distMix = 0.20f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 4.0f, .compRelease = 80.0f, .compMakeup = 2.8f,
            .reverbSize = 0.60f, .reverbDecay = 440.0f, .reverbDamp = 0.38f, .reverbGate = 150.0f, .reverbMix = 0.20f,
            .midiMode = 1, .stereoWidth = 1.10f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "TM 21 Sub-Bass 808 Boom Tom",
            .voiceMode = 1, .toneNote = 0, .toneOctave = 2, .toneFreq = 65.41f, .toneQ = 3.2f,
            .toneDecay = 650.0f, .tonePitchEnv = 18.0f, .tonePitchDecay = 38.0f, .fmAmount = 0.15f, .toneLevel = 0.95f,
            .noiseType = 2, .noiseDecay = 60.0f, .noiseCurve = -0.50f, .noiseHPF = 150.0f, .noiseLPF = 4500.0f,
            .noiseStereoWidth = 0.85f, .noiseLevel = 0.15f,
            .clickType = 1, .clickLevel = 0.44f, .clickFreq = 1200.0f,
            .distType = 0, .distDrive = 0.18f, .distTone = -0.10f, .distMix = 0.22f,
            .compThresh = -15.0f, .compRatio = 4.5f, .compAttack = 5.0f, .compRelease = 100.0f, .compMakeup = 3.2f,
            .reverbSize = 0.50f, .reverbDecay = 300.0f, .reverbDamp = 0.40f, .reverbGate = 0.0f, .reverbMix = 0.0f,
            .stereoWidth = 0.90f, .outputGain = 0.5f
        }),

        makePreset(PresetDef{
            .name = "TM 22 Ilan Bluestone Big Flam Tom",
            .preBursts = 3, .preSpread = 14.0f, .preDecay = 9.5f, .preJitter = 0.12f, .flamSpread = 0.55f, .preLevel = 0.62f,
            .voiceMode = 1, .toneNote = 7, .toneOctave = 2, .toneFreq = 98.0f, .toneQ = 3.6f,
            .toneDecay = 300.0f, .tonePitchEnv = 15.0f, .tonePitchDecay = 28.0f, .fmAmount = 0.38f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 110.0f, .noiseCurve = -0.38f, .noiseHPF = 210.0f, .noiseLPF = 7200.0f,
            .noiseStereoWidth = 1.25f, .noiseLevel = 0.28f,
            .clickType = 1, .clickLevel = 0.54f, .clickFreq = 2200.0f,
            .distType = 1, .distDrive = 0.25f, .distTone = 0.06f, .distMix = 0.30f,
            .compThresh = -15.0f, .compRatio = 5.0f, .compAttack = 3.5f, .compRelease = 90.0f, .compMakeup = 3.5f,
            .reverbSize = 0.82f, .reverbDecay = 680.0f, .reverbDamp = 0.30f, .reverbGate = 240.0f, .reverbMix = 0.28f,
            .stereoWidth = 1.20f, .outputGain = 0.6f
        }),

        makePreset(PresetDef{
            .name = "TM 23 Note-Tracked Keyboard Tom Rack",
            .voiceMode = 1, .toneNote = 9, .toneOctave = 2, .toneFreq = 110.0f, .toneQ = 3.5f,
            .toneDecay = 280.0f, .tonePitchEnv = 12.0f, .tonePitchDecay = 26.0f, .fmAmount = 0.35f, .toneLevel = 0.90f,
            .noiseType = 2, .noiseDecay = 95.0f, .noiseCurve = -0.42f, .noiseHPF = 220.0f, .noiseLPF = 6800.0f,
            .noiseStereoWidth = 1.10f, .noiseLevel = 0.24f,
            .clickType = 1, .clickLevel = 0.48f, .clickFreq = 2000.0f,
            .distType = 0, .distDrive = 0.16f, .distTone = 0.02f, .distMix = 0.22f,
            .compThresh = -13.5f, .compRatio = 4.2f, .compAttack = 4.2f, .compRelease = 80.0f, .compMakeup = 2.8f,
            .reverbSize = 0.60f, .reverbDecay = 440.0f, .reverbDamp = 0.38f, .reverbGate = 160.0f, .reverbMix = 0.20f,
            .midiMode = 1, .stereoWidth = 1.10f, .outputGain = 0.2f
        }),

        makePreset(PresetDef{
            .name = "TM 24 Beat 4 Reverse Tom Pickup",
            .voiceMode = 1, .toneNote = 11, .toneOctave = 2, .toneFreq = 123.47f, .toneQ = 3.6f,
            .toneDecay = 290.0f, .tonePitchEnv = 16.0f, .tonePitchDecay = 32.0f, .fmAmount = 0.40f, .toneLevel = 0.92f,
            .noiseType = 2, .noiseDecay = 100.0f, .noiseCurve = -0.40f, .noiseHPF = 230.0f, .noiseLPF = 7200.0f,
            .noiseStereoWidth = 1.15f, .noiseLevel = 0.26f,
            .clickType = 1, .clickLevel = 0.52f, .clickFreq = 2200.0f,
            .distType = 1, .distDrive = 0.22f, .distTone = 0.05f, .distMix = 0.28f,
            .compThresh = -14.5f, .compRatio = 4.8f, .compAttack = 3.8f, .compRelease = 85.0f, .compMakeup = 3.2f,
            .reverbSize = 0.68f, .reverbDecay = 520.0f, .reverbDamp = 0.34f, .reverbGate = 200.0f, .reverbMix = 0.25f,
            .reverseMode = 2, .reverseMix = 0.55f, .midiMode = 1, .stereoWidth = 1.15f, .outputGain = 0.4f
        })
    };
    return kPresets;
}

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state, bool preservePitch) {
    for (const auto& [paramId, value] : preset.values) {
        if (preservePitch && (paramId == kToneFreq || paramId == kToneNote
                              || paramId == kToneOctave || paramId == kToneFine)) {
            continue;
        }
        if (auto* param = state.getParameter(paramId)) {
            const float norm = param->convertTo0to1(value);
            param->setValueNotifyingHost(norm);
        }
    }
}

} // namespace opensmacker::presets
