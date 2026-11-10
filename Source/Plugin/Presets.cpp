#include "Presets.h"
#include "Parameters.h"
#include <algorithm>

namespace opensmacker::presets {

using namespace opensmacker::params;

namespace {

FactoryPreset makePreset(
    const char* name,
    float preBursts, float preSpread, float noiseDecay, float reverbDecayScale,
    float preLevel, float preJitter, float voiceMode, float fmAmount,
    float toneFreq, float toneNote, float toneOctave, float toneFine,
    float pitchDrop, float pitchDropTime, float toneDecay, float toneLevel, float noteTrack,
    float clickType, float clickFreq, float clickLevel,
    float noiseType, float noiseHPF, float toneQ, float noiseLevel,
    float noiseStereoWidth, float flamPanSpread,
    float distType, float distToneBias, float noiseLPF, float reverbGateScale,
    float distDrive, float distMix, float noiseLPFOverride,
    float compThresh, float compRatio, float compAttack, float compRelease, float compMakeup,
    float limiterCeiling, float revSize, float revDamp, float revMix, float masterGain = 0.0f,
    float reverseMode = 0.0f, float reverseMix = 0.45f) {
    (void)noiseLPF;
    const float clampedDistTone = std::clamp((distToneBias - 1.5f) * 0.35f, -1.0f, 1.0f);
    const float clampedDistType = std::clamp(std::round(distType * 0.6f), 0.0f, 3.0f);
    const float reverbDecayMs = std::clamp(reverbDecayScale * 420.0f, 80.0f, 2200.0f);
    const float reverbGateMs = std::clamp(reverbGateScale * 65.0f, 0.0f, 550.0f);

    return FactoryPreset{
        name,
        {
            { kPreBursts, preBursts },
            { kPreSpread, preSpread },
            { kPreDecay, std::clamp(preSpread * 0.85f, 3.0f, 30.0f) },
            { kPreJitter, preJitter },
            { kFlamSpread, flamPanSpread },
            { kPreLevel, preLevel },
            { kVoiceMode, voiceMode },
            { kFmAmount, fmAmount },
            { kToneFreq, toneFreq },
            { kToneNote, toneNote },
            { kToneOctave, toneOctave },
            { kToneFine, toneFine },
            { kToneQ, toneQ },
            { kTonePitchEnv, pitchDrop },
            { kTonePitchDecay, pitchDropTime },
            { kToneDecay, toneDecay },
            { kToneLevel, toneLevel },
            { kMidiMode, noteTrack },
            { kClickType, clickType },
            { kClickFreq, clickFreq },
            { kClickLevel, clickLevel },
            { kNoiseType, noiseType },
            { kNoiseAttack, 0.0f },
            { kNoiseHold, 2.0f },
            { kNoiseDecay, noiseDecay },
            { kNoiseCurve, -0.35f },
            { kNoiseHPF, std::clamp(noiseHPF * 0.25f, 40.0f, 2200.0f) },
            { kNoiseLPF, std::clamp(noiseLPFOverride * 1.6f, 2500.0f, 19000.0f) },
            { kNoiseStereoWidth, std::clamp(noiseStereoWidth * 1.5f, 0.2f, 1.9f) },
            { kNoiseLevel, noiseLevel },
            { kStereoWidth, std::clamp(0.75f + noiseStereoWidth * 0.55f, 0.5f, 1.6f) },
            { kDistType, clampedDistType },
            { kDistDrive, distDrive },
            { kDistMix, distMix },
            { kDistTone, clampedDistTone },
            { kCompThresh, compThresh },
            { kCompRatio, compRatio },
            { kCompAttack, compAttack },
            { kCompRelease, compRelease },
            { kCompMakeup, compMakeup },
            { kCompMix, 1.0f },
            { kLimiterCeiling, limiterCeiling },
            { kLimiterRelease, 20.0f },
            { kLimiterEnabled, 1.0f },
            { kReverbSize, revSize },
            { kReverbDecay, reverbDecayMs },
            { kReverbDamp, revDamp },
            { kReverbGate, reverbGateMs },
            { kReverbMix, revMix },
            { kReverseMode, reverseMode },
            { kReverseMix, reverseMix },
            { kOutputGain, masterGain },
        }
    };
}

} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    // VoiceMode: 0 = Dance Snare, 1 = Club Tom, 2 = FM Bell / Chime, 3 = Pryda Rim Smack
    // ClickType: 0 = Stick, 1 = Mallet, 2 = Rimshot, 3 = Laser
    // NoiseType: 0 = Snare Wires, 1 = White Sizzle, 2 = Velvet Air, 3 = Metallic Ring
    static const std::vector<FactoryPreset> kPresets = {
        // =========================================================================
        // SN 01 - SN 24: TUNABLE DANCE & TRANCE SNARES (24 Presets)
        // =========================================================================
        makePreset("SN 01 Anjuna Snare Smack",
            2.0f, 9.0f, 165.0f, 1.0f, 0.62f, 0.14f, 0.0f, 0.15f,
            220.0f, 9.0f, 3.0f, 0.0f, 10.0f, 24.0f, 155.0f, 0.78f, 0.0f,
            0.0f, 3400.0f, 0.45f, 0.0f, 1200.0f, 3.2f, 0.76f, 0.75f, 0.35f,
            0.0f, 1.8f, 1800.0f, 2.5f, 0.22f, 0.38f, 7500.0f,
            -13.0f, 4.0f, 4.0f, 80.0f, 2.5f, -0.3f, 0.58f, 0.42f, 0.22f, 0.0f),

        makePreset("SN 02 Above & Beyond Group Therapy Snare",
            3.0f, 11.5f, 195.0f, 1.08f, 0.65f, 0.18f, 0.0f, 0.12f,
            196.0f, 7.0f, 3.0f, 0.0f, 11.0f, 28.0f, 175.0f, 0.80f, 0.0f,
            2.0f, 3600.0f, 0.52f, 0.0f, 1300.0f, 3.0f, 0.80f, 0.82f, 0.48f,
            1.0f, 1.5f, 2100.0f, 3.2f, 0.26f, 0.42f, 8200.0f,
            -14.0f, 4.5f, 3.5f, 95.0f, 3.0f, -0.3f, 0.65f, 0.35f, 0.26f, 0.5f),

        makePreset("SN 03 Pryda Arena Rim Smack",
            1.0f, 4.5f, 310.0f, 1.45f, 0.55f, 0.08f, 3.0f, 0.35f,
            440.0f, 9.0f, 4.0f, 0.0f, 7.0f, 16.0f, 195.0f, 0.84f, 0.0f,
            2.0f, 3800.0f, 0.75f, 0.0f, 1600.0f, 4.2f, 0.85f, 0.92f, 0.60f,
            1.0f, 3.2f, 2400.0f, 4.0f, 0.35f, 0.48f, 9000.0f,
            -16.0f, 6.0f, 2.0f, 120.0f, 4.0f, -0.2f, 0.82f, 0.25f, 0.38f, 1.2f),

        makePreset("SN 04 Bayer Analog Tight Snare",
            1.0f, 5.5f, 130.0f, 0.92f, 0.52f, 0.09f, 0.0f, 0.18f,
            185.0f, 6.0f, 3.0f, 0.0f, 13.0f, 20.0f, 135.0f, 0.82f, 0.0f,
            0.0f, 3100.0f, 0.54f, 0.0f, 1100.0f, 2.9f, 0.68f, 0.62f, 0.25f,
            1.0f, 1.4f, 1500.0f, 1.8f, 0.30f, 0.45f, 6400.0f,
            -14.0f, 4.8f, 2.8f, 65.0f, 3.0f, -0.3f, 0.44f, 0.52f, 0.14f, 0.5f),

        makePreset("SN 05 Bluestone Snap Rim Snare",
            2.0f, 7.5f, 150.0f, 0.96f, 0.58f, 0.12f, 3.0f, 0.28f,
            392.0f, 7.0f, 4.0f, 0.0f, 9.0f, 18.0f, 145.0f, 0.78f, 0.0f,
            2.0f, 3900.0f, 0.68f, 0.0f, 1500.0f, 3.8f, 0.70f, 0.75f, 0.45f,
            0.0f, 2.8f, 2300.0f, 2.8f, 0.26f, 0.40f, 8000.0f,
            -13.5f, 4.5f, 3.0f, 75.0f, 2.8f, -0.3f, 0.55f, 0.40f, 0.20f, 0.4f),

        makePreset("SN 06 Myon & Shane 54 Summer Snare",
            3.0f, 12.0f, 185.0f, 1.05f, 0.60f, 0.18f, 0.0f, 0.14f,
            207.65f, 8.0f, 3.0f, 0.0f, 10.0f, 25.0f, 165.0f, 0.76f, 0.0f,
            0.0f, 3500.0f, 0.48f, 2.0f, 1400.0f, 3.1f, 0.78f, 0.84f, 0.52f,
            0.0f, 1.6f, 2000.0f, 3.4f, 0.22f, 0.36f, 8500.0f,
            -13.0f, 4.0f, 3.8f, 88.0f, 2.6f, -0.3f, 0.62f, 0.36f, 0.24f, 0.4f),

        makePreset("SN 07 Sun & Moon Push the Button Snare",
            3.0f, 13.5f, 210.0f, 1.18f, 0.64f, 0.16f, 0.0f, 0.20f,
            220.0f, 9.0f, 3.0f, 0.0f, 12.0f, 26.0f, 185.0f, 0.80f, 0.0f,
            2.0f, 3700.0f, 0.56f, 0.0f, 1450.0f, 3.3f, 0.82f, 0.86f, 0.55f,
            1.0f, 2.0f, 2200.0f, 3.8f, 0.32f, 0.46f, 8800.0f,
            -15.0f, 5.0f, 3.0f, 100.0f, 3.5f, -0.2f, 0.68f, 0.32f, 0.28f, 0.8f),

        makePreset("SN 08 Zo Funk Electro Snare",
            1.0f, 4.0f, 145.0f, 0.94f, 0.55f, 0.10f, 0.0f, 0.32f,
            246.94f, 11.0f, 3.0f, 0.0f, 15.0f, 22.0f, 150.0f, 0.84f, 0.0f,
            3.0f, 4100.0f, 0.58f, 1.0f, 1300.0f, 3.4f, 0.72f, 0.68f, 0.38f,
            2.0f, 2.2f, 1750.0f, 2.6f, 0.38f, 0.50f, 7400.0f,
            -14.5f, 5.2f, 2.5f, 70.0f, 3.2f, -0.3f, 0.46f, 0.46f, 0.16f, 0.6f),

        makePreset("SN 09 Daniel Kandi Always Alive Snare",
            3.0f, 10.5f, 200.0f, 1.15f, 0.62f, 0.15f, 0.0f, 0.16f,
            233.08f, 10.0f, 3.0f, 0.0f, 11.0f, 24.0f, 170.0f, 0.76f, 0.0f,
            0.0f, 3800.0f, 0.52f, 1.0f, 1550.0f, 3.2f, 0.84f, 0.86f, 0.54f,
            0.0f, 1.8f, 2400.0f, 4.2f, 0.24f, 0.38f, 9200.0f,
            -13.5f, 4.2f, 3.5f, 92.0f, 2.8f, -0.3f, 0.64f, 0.30f, 0.26f, 0.6f),

        makePreset("SN 10 Sean Tyas Degenerate Punch Snare",
            1.0f, 4.0f, 155.0f, 0.96f, 0.58f, 0.08f, 0.0f, 0.28f,
            220.0f, 9.0f, 3.0f, 0.0f, 14.0f, 19.0f, 155.0f, 0.85f, 0.0f,
            2.0f, 4200.0f, 0.64f, 0.0f, 1500.0f, 3.6f, 0.78f, 0.72f, 0.42f,
            2.0f, 2.6f, 2100.0f, 3.6f, 0.42f, 0.54f, 8200.0f,
            -15.5f, 5.8f, 2.0f, 68.0f, 3.8f, -0.2f, 0.48f, 0.42f, 0.16f, 1.0f),

        makePreset("SN 11 Ferry Corsten Flashover Electro Snare",
            2.0f, 7.0f, 175.0f, 1.02f, 0.60f, 0.11f, 0.0f, 0.36f,
            207.65f, 8.0f, 3.0f, 0.0f, 15.0f, 24.0f, 168.0f, 0.84f, 0.0f,
            3.0f, 3900.0f, 0.58f, 1.0f, 1350.0f, 3.4f, 0.76f, 0.76f, 0.46f,
            2.0f, 2.2f, 1850.0f, 3.2f, 0.40f, 0.52f, 7800.0f,
            -15.0f, 5.2f, 2.6f, 80.0f, 3.4f, -0.2f, 0.54f, 0.38f, 0.20f, 0.8f),

        makePreset("SN 12 System F Out of the Blue 909 Snare",
            1.0f, 5.0f, 180.0f, 1.04f, 0.56f, 0.10f, 0.0f, 0.10f,
            196.0f, 7.0f, 3.0f, 0.0f, 12.0f, 25.0f, 170.0f, 0.80f, 0.0f,
            0.0f, 3200.0f, 0.50f, 1.0f, 1200.0f, 2.8f, 0.80f, 0.68f, 0.36f,
            1.0f, 1.6f, 1700.0f, 3.4f, 0.25f, 0.40f, 8000.0f,
            -13.5f, 4.4f, 3.2f, 85.0f, 2.8f, -0.3f, 0.56f, 0.38f, 0.20f, 0.5f),

        makePreset("SN 13 Armada Mainstage Snare",
            3.0f, 11.0f, 205.0f, 1.12f, 0.64f, 0.15f, 0.0f, 0.22f,
            220.0f, 9.0f, 3.0f, 0.0f, 12.0f, 25.0f, 175.0f, 0.80f, 0.0f,
            2.0f, 3800.0f, 0.58f, 0.0f, 1450.0f, 3.3f, 0.82f, 0.86f, 0.55f,
            1.0f, 2.0f, 2100.0f, 3.8f, 0.30f, 0.44f, 8600.0f,
            -14.5f, 4.8f, 3.0f, 92.0f, 3.2f, -0.2f, 0.66f, 0.34f, 0.26f, 0.8f),

        makePreset("SN 14 ASOT 138 Uplifting Snare",
            3.0f, 12.5f, 215.0f, 1.18f, 0.66f, 0.16f, 0.0f, 0.18f,
            233.08f, 10.0f, 3.0f, 0.0f, 11.0f, 26.0f, 172.0f, 0.76f, 0.0f,
            0.0f, 3900.0f, 0.52f, 1.0f, 1600.0f, 3.2f, 0.86f, 0.88f, 0.58f,
            0.0f, 1.8f, 2300.0f, 4.2f, 0.26f, 0.40f, 9200.0f,
            -14.0f, 4.5f, 3.4f, 96.0f, 3.0f, -0.3f, 0.68f, 0.30f, 0.28f, 0.6f),

        makePreset("SN 15 Subculture Tech-Trance Snare",
            1.0f, 4.0f, 145.0f, 0.94f, 0.56f, 0.08f, 0.0f, 0.32f,
            220.0f, 9.0f, 3.0f, 0.0f, 14.0f, 18.0f, 148.0f, 0.84f, 0.0f,
            2.0f, 4300.0f, 0.66f, 0.0f, 1550.0f, 3.7f, 0.76f, 0.70f, 0.40f,
            2.0f, 2.8f, 2200.0f, 3.6f, 0.44f, 0.55f, 8200.0f,
            -15.5f, 5.8f, 2.0f, 66.0f, 3.8f, -0.2f, 0.46f, 0.44f, 0.14f, 1.0f),

        makePreset("SN 16 Coldharbour Dark Rim Smack",
            2.0f, 8.0f, 165.0f, 0.98f, 0.60f, 0.11f, 3.0f, 0.38f,
            349.23f, 5.0f, 4.0f, 0.0f, 8.0f, 17.0f, 152.0f, 0.80f, 0.0f,
            2.0f, 3600.0f, 0.70f, 3.0f, 1300.0f, 4.2f, 0.70f, 0.76f, 0.48f,
            1.0f, 2.8f, 1950.0f, 2.4f, 0.42f, 0.54f, 6800.0f,
            -15.0f, 5.4f, 2.4f, 76.0f, 3.4f, -0.2f, 0.56f, 0.46f, 0.22f, 0.8f),

        makePreset("SN 17 FSOE 140 Anthem Snare",
            4.0f, 13.0f, 225.0f, 1.22f, 0.68f, 0.18f, 0.0f, 0.20f,
            246.94f, 11.0f, 3.0f, 0.0f, 12.0f, 28.0f, 185.0f, 0.78f, 0.0f,
            2.0f, 4000.0f, 0.56f, 1.0f, 1650.0f, 3.3f, 0.88f, 0.92f, 0.62f,
            1.0f, 2.0f, 2400.0f, 4.4f, 0.28f, 0.42f, 9400.0f,
            -14.5f, 4.8f, 3.2f, 105.0f, 3.2f, -0.2f, 0.72f, 0.28f, 0.30f, 0.8f),

        makePreset("SN 18 Vandit Berlin Club Snare",
            1.0f, 5.0f, 150.0f, 0.96f, 0.55f, 0.09f, 0.0f, 0.24f,
            207.65f, 8.0f, 3.0f, 0.0f, 13.0f, 21.0f, 155.0f, 0.82f, 0.0f,
            0.0f, 3500.0f, 0.54f, 0.0f, 1250.0f, 3.1f, 0.74f, 0.66f, 0.34f,
            1.0f, 2.0f, 1800.0f, 2.8f, 0.34f, 0.46f, 7500.0f,
            -14.5f, 5.0f, 2.6f, 72.0f, 3.2f, -0.3f, 0.48f, 0.44f, 0.16f, 0.6f),

        makePreset("SN 19 Garuda Big Room Snare",
            2.0f, 8.5f, 195.0f, 1.08f, 0.62f, 0.12f, 0.0f, 0.28f,
            220.0f, 9.0f, 3.0f, 0.0f, 14.0f, 24.0f, 182.0f, 0.85f, 0.0f,
            2.0f, 3800.0f, 0.62f, 0.0f, 1400.0f, 3.4f, 0.84f, 0.84f, 0.54f,
            1.0f, 2.4f, 2000.0f, 3.6f, 0.38f, 0.50f, 8400.0f,
            -15.5f, 5.5f, 2.5f, 90.0f, 3.6f, -0.2f, 0.62f, 0.34f, 0.24f, 1.0f),

        makePreset("SN 20 Wake Your Mind Snare",
            2.0f, 9.0f, 175.0f, 1.02f, 0.60f, 0.13f, 0.0f, 0.26f,
            207.65f, 8.0f, 3.0f, 0.0f, 12.0f, 23.0f, 165.0f, 0.82f, 0.0f,
            2.0f, 3700.0f, 0.58f, 0.0f, 1350.0f, 3.3f, 0.76f, 0.76f, 0.46f,
            1.0f, 2.0f, 1950.0f, 3.0f, 0.34f, 0.46f, 8000.0f,
            -14.5f, 4.8f, 2.8f, 82.0f, 3.2f, -0.3f, 0.54f, 0.38f, 0.20f, 0.6f),

        makePreset("SN 21 Euphonic Uplifting Snare",
            3.0f, 10.5f, 190.0f, 1.06f, 0.60f, 0.16f, 0.0f, 0.16f,
            220.0f, 9.0f, 3.0f, 0.0f, 11.0f, 24.0f, 165.0f, 0.76f, 0.0f,
            0.0f, 3600.0f, 0.50f, 2.0f, 1450.0f, 3.1f, 0.80f, 0.82f, 0.50f,
            0.0f, 1.6f, 2100.0f, 3.6f, 0.22f, 0.36f, 8600.0f,
            -13.5f, 4.2f, 3.5f, 86.0f, 2.8f, -0.3f, 0.60f, 0.34f, 0.22f, 0.5f),

        makePreset("SN 22 Positiva 99 Ibiza Snare",
            2.0f, 8.0f, 185.0f, 1.04f, 0.58f, 0.14f, 0.0f, 0.12f,
            196.0f, 7.0f, 3.0f, 0.0f, 11.0f, 24.0f, 168.0f, 0.78f, 0.0f,
            0.0f, 3300.0f, 0.48f, 1.0f, 1250.0f, 2.9f, 0.78f, 0.72f, 0.42f,
            0.0f, 1.8f, 1800.0f, 3.2f, 0.24f, 0.38f, 7800.0f,
            -13.5f, 4.2f, 3.2f, 84.0f, 2.8f, -0.3f, 0.56f, 0.38f, 0.20f, 0.5f),

        makePreset("SN 23 Note-Tracked Melodic Snare Roll",
            1.0f, 4.0f, 150.0f, 0.94f, 0.55f, 0.08f, 0.0f, 0.22f,
            220.0f, 9.0f, 3.0f, 0.0f, 8.0f, 18.0f, 185.0f, 0.88f, 1.0f,
            0.0f, 3500.0f, 0.48f, 0.0f, 1300.0f, 3.6f, 0.65f, 0.68f, 0.38f,
            0.0f, 2.2f, 1800.0f, 2.8f, 0.24f, 0.38f, 8000.0f,
            -13.5f, 4.2f, 3.0f, 74.0f, 2.8f, -0.3f, 0.48f, 0.42f, 0.16f, 0.4f),

        makePreset("SN 24 Clean Studio Reference Snare",
            1.0f, 4.0f, 155.0f, 1.00f, 0.50f, 0.05f, 0.0f, 0.10f,
            220.0f, 9.0f, 3.0f, 0.0f, 9.0f, 20.0f, 155.0f, 0.78f, 0.0f,
            0.0f, 3200.0f, 0.45f, 0.0f, 1150.0f, 2.8f, 0.72f, 0.55f, 0.20f,
            0.0f, 1.5f, 1800.0f, 1.0f, 0.08f, 0.15f, 7500.0f,
            -11.0f, 3.0f, 4.5f, 70.0f, 1.5f, -0.3f, 0.38f, 0.50f, 0.08f, 0.0f),

        // =========================================================================
        // BL 01 - BL 24: FM BELLS, CHIMES & METALLIC HITS (24 Presets)
        // =========================================================================
        makePreset("BL 01 Tri-State Crystal Bell",
            1.0f, 3.0f, 260.0f, 1.35f, 0.50f, 0.06f, 2.0f, 0.62f,
            880.0f, 9.0f, 5.0f, 0.0f, 2.0f, 12.0f, 420.0f, 0.90f, 1.0f,
            1.0f, 5200.0f, 0.42f, 3.0f, 2600.0f, 5.2f, 0.28f, 0.88f, 0.55f,
            0.0f, 1.8f, 2800.0f, 4.2f, 0.14f, 0.25f, 10000.0f,
            -12.0f, 3.2f, 5.0f, 110.0f, 2.0f, -0.3f, 0.76f, 0.22f, 0.34f, 0.0f),

        makePreset("BL 02 OceanLab Balearic Chime",
            2.0f, 8.0f, 290.0f, 1.45f, 0.55f, 0.12f, 2.0f, 0.48f,
            1046.5f, 0.0f, 6.0f, 0.0f, 1.5f, 10.0f, 480.0f, 0.88f, 1.0f,
            1.0f, 5800.0f, 0.38f, 3.0f, 2800.0f, 4.8f, 0.22f, 0.92f, 0.62f,
            0.0f, 1.4f, 3200.0f, 4.5f, 0.10f, 0.20f, 10500.0f,
            -11.5f, 3.0f, 6.0f, 125.0f, 1.8f, -0.3f, 0.82f, 0.18f, 0.38f, 0.0f),

        makePreset("BL 03 Anjunadeep Glass Bell",
            1.0f, 4.0f, 230.0f, 1.20f, 0.50f, 0.10f, 2.0f, 0.54f,
            659.25f, 4.0f, 5.0f, 0.0f, 3.0f, 14.0f, 360.0f, 0.90f, 1.0f,
            1.0f, 4400.0f, 0.44f, 3.0f, 2200.0f, 5.0f, 0.24f, 0.82f, 0.48f,
            0.0f, 1.6f, 2400.0f, 3.2f, 0.18f, 0.30f, 8600.0f,
            -12.5f, 3.5f, 4.5f, 95.0f, 2.2f, -0.3f, 0.68f, 0.28f, 0.28f, 0.0f),

        makePreset("BL 04 Kandiman 138 Euphoric Bell",
            1.0f, 3.5f, 270.0f, 1.40f, 0.52f, 0.08f, 2.0f, 0.66f,
            1174.66f, 2.0f, 6.0f, 0.0f, 2.0f, 11.0f, 450.0f, 0.90f, 1.0f,
            1.0f, 6000.0f, 0.46f, 3.0f, 2700.0f, 5.4f, 0.26f, 0.90f, 0.60f,
            0.0f, 2.0f, 3100.0f, 4.6f, 0.16f, 0.28f, 10200.0f,
            -12.0f, 3.4f, 4.8f, 115.0f, 2.2f, -0.3f, 0.78f, 0.20f, 0.35f, 0.2f),

        makePreset("BL 05 Venice Beach Sunset Chime",
            2.0f, 9.0f, 250.0f, 1.30f, 0.54f, 0.12f, 2.0f, 0.52f,
            987.77f, 11.0f, 5.0f, 0.0f, 2.0f, 12.0f, 410.0f, 0.88f, 1.0f,
            1.0f, 5400.0f, 0.40f, 3.0f, 2500.0f, 5.0f, 0.24f, 0.88f, 0.56f,
            0.0f, 1.8f, 2900.0f, 4.0f, 0.14f, 0.24f, 9800.0f,
            -12.0f, 3.2f, 5.0f, 108.0f, 2.0f, -0.3f, 0.72f, 0.24f, 0.32f, 0.0f),

        makePreset("BL 06 Childish Gambit FM Bell",
            1.0f, 3.0f, 220.0f, 1.15f, 0.50f, 0.07f, 2.0f, 0.74f,
            783.99f, 7.0f, 5.0f, 0.0f, 3.5f, 15.0f, 350.0f, 0.90f, 1.0f,
            2.0f, 4800.0f, 0.50f, 3.0f, 2400.0f, 5.6f, 0.30f, 0.82f, 0.50f,
            1.0f, 2.2f, 2600.0f, 3.8f, 0.22f, 0.34f, 9400.0f,
            -12.5f, 3.6f, 4.2f, 98.0f, 2.4f, -0.3f, 0.64f, 0.26f, 0.26f, 0.2f),

        makePreset("BL 07 Lift High Acid Bell",
            1.0f, 3.0f, 210.0f, 1.12f, 0.50f, 0.06f, 2.0f, 0.82f,
            880.0f, 9.0f, 5.0f, 0.0f, 5.0f, 14.0f, 330.0f, 0.90f, 1.0f,
            3.0f, 5200.0f, 0.54f, 3.0f, 2500.0f, 6.2f, 0.34f, 0.84f, 0.54f,
            3.0f, 2.8f, 3000.0f, 4.2f, 0.34f, 0.45f, 9600.0f,
            -14.0f, 4.5f, 3.0f, 90.0f, 3.0f, -0.3f, 0.60f, 0.26f, 0.24f, 0.5f),

        makePreset("BL 08 Tytanium Metallic Cowbell",
            1.0f, 2.5f, 165.0f, 0.98f, 0.48f, 0.05f, 2.0f, 0.68f,
            587.33f, 2.0f, 5.0f, 0.0f, 3.0f, 12.0f, 240.0f, 0.92f, 1.0f,
            2.0f, 4400.0f, 0.58f, 3.0f, 2100.0f, 5.8f, 0.32f, 0.72f, 0.42f,
            2.0f, 3.0f, 2200.0f, 3.2f, 0.32f, 0.44f, 8500.0f,
            -14.0f, 4.6f, 2.8f, 76.0f, 3.0f, -0.3f, 0.48f, 0.36f, 0.16f, 0.6f),

        makePreset("BL 09 Gouryella Heaven Bell",
            2.0f, 9.5f, 320.0f, 1.50f, 0.56f, 0.12f, 2.0f, 0.56f,
            1318.51f, 4.0f, 6.0f, 0.0f, 1.5f, 10.0f, 520.0f, 0.90f, 1.0f,
            1.0f, 6200.0f, 0.42f, 3.0f, 2900.0f, 5.2f, 0.24f, 0.95f, 0.66f,
            0.0f, 1.8f, 3400.0f, 4.8f, 0.12f, 0.22f, 10600.0f,
            -11.5f, 3.2f, 5.5f, 130.0f, 2.0f, -0.3f, 0.86f, 0.16f, 0.42f, 0.2f),

        makePreset("BL 10 Punk Metallic Bell Smack",
            1.0f, 3.5f, 185.0f, 1.04f, 0.52f, 0.08f, 2.0f, 0.78f,
            659.25f, 4.0f, 5.0f, 0.0f, 6.0f, 16.0f, 275.0f, 0.90f, 1.0f,
            2.0f, 4600.0f, 0.60f, 3.0f, 2300.0f, 5.8f, 0.38f, 0.78f, 0.48f,
            2.0f, 2.8f, 2500.0f, 3.6f, 0.36f, 0.48f, 9000.0f,
            -14.5f, 4.8f, 2.8f, 84.0f, 3.2f, -0.3f, 0.54f, 0.32f, 0.20f, 0.6f),

        makePreset("BL 11 Kyoto Ritual Bell",
            1.0f, 3.0f, 300.0f, 1.42f, 0.50f, 0.07f, 2.0f, 0.70f,
            523.25f, 0.0f, 5.0f, 0.0f, 2.5f, 14.0f, 490.0f, 0.92f, 1.0f,
            1.0f, 4200.0f, 0.48f, 3.0f, 2200.0f, 5.6f, 0.26f, 0.90f, 0.58f,
            0.0f, 2.2f, 2200.0f, 3.8f, 0.18f, 0.30f, 9200.0f,
            -12.5f, 3.4f, 4.8f, 120.0f, 2.2f, -0.3f, 0.80f, 0.22f, 0.36f, 0.2f),

        makePreset("BL 12 Who's Afraid 138 Ride Bell",
            1.0f, 2.5f, 200.0f, 1.08f, 0.48f, 0.06f, 2.0f, 0.76f,
            1567.98f, 7.0f, 6.0f, 0.0f, 2.0f, 10.0f, 315.0f, 0.88f, 1.0f,
            2.0f, 6400.0f, 0.54f, 3.0f, 3000.0f, 6.0f, 0.34f, 0.82f, 0.52f,
            1.0f, 2.4f, 3600.0f, 4.6f, 0.24f, 0.36f, 10400.0f,
            -13.5f, 4.2f, 3.2f, 88.0f, 2.8f, -0.3f, 0.58f, 0.26f, 0.22f, 0.4f),

        makePreset("BL 13 Black Hole Cosmic Bell",
            2.0f, 9.0f, 290.0f, 1.38f, 0.54f, 0.11f, 2.0f, 0.64f,
            987.77f, 11.0f, 5.0f, 0.0f, 2.0f, 12.0f, 470.0f, 0.90f, 1.0f,
            1.0f, 5600.0f, 0.44f, 3.0f, 2600.0f, 5.4f, 0.26f, 0.92f, 0.62f,
            0.0f, 2.0f, 2900.0f, 4.4f, 0.16f, 0.28f, 10000.0f,
            -12.0f, 3.4f, 5.0f, 118.0f, 2.2f, -0.3f, 0.78f, 0.20f, 0.36f, 0.2f),

        makePreset("BL 14 Bonzai Rave Cowbell",
            1.0f, 2.5f, 155.0f, 0.94f, 0.48f, 0.05f, 2.0f, 0.62f,
            587.33f, 2.0f, 5.0f, 0.0f, 3.5f, 11.0f, 220.0f, 0.92f, 1.0f,
            2.0f, 4200.0f, 0.60f, 3.0f, 2000.0f, 5.6f, 0.30f, 0.68f, 0.38f,
            2.0f, 3.2f, 2100.0f, 3.0f, 0.34f, 0.46f, 8200.0f,
            -14.0f, 4.8f, 2.5f, 70.0f, 3.0f, -0.3f, 0.44f, 0.40f, 0.14f, 0.6f),

        makePreset("BL 15 Aalto Pluck Bell",
            1.0f, 3.5f, 230.0f, 1.18f, 0.50f, 0.08f, 2.0f, 0.58f,
            783.99f, 7.0f, 5.0f, 0.0f, 3.0f, 14.0f, 370.0f, 0.90f, 1.0f,
            1.0f, 5000.0f, 0.46f, 3.0f, 2400.0f, 5.2f, 0.26f, 0.86f, 0.54f,
            0.0f, 2.0f, 2700.0f, 4.0f, 0.16f, 0.28f, 9800.0f,
            -12.5f, 3.5f, 4.4f, 102.0f, 2.2f, -0.3f, 0.68f, 0.24f, 0.28f, 0.2f),

        makePreset("BL 16 Solaris Shimmer Bell",
            2.0f, 8.5f, 280.0f, 1.36f, 0.54f, 0.12f, 2.0f, 0.50f,
            1174.66f, 2.0f, 6.0f, 0.0f, 1.5f, 11.0f, 460.0f, 0.88f, 1.0f,
            1.0f, 5800.0f, 0.40f, 3.0f, 2700.0f, 5.0f, 0.24f, 0.92f, 0.64f,
            0.0f, 1.6f, 3100.0f, 4.6f, 0.12f, 0.22f, 10400.0f,
            -12.0f, 3.2f, 5.2f, 118.0f, 2.0f, -0.3f, 0.78f, 0.20f, 0.36f, 0.2f),

        makePreset("BL 17 Enhanced Progressive Chime",
            1.0f, 4.0f, 245.0f, 1.24f, 0.50f, 0.09f, 2.0f, 0.56f,
            880.0f, 9.0f, 5.0f, 0.0f, 2.5f, 13.0f, 395.0f, 0.90f, 1.0f,
            1.0f, 5100.0f, 0.44f, 3.0f, 2450.0f, 5.1f, 0.25f, 0.86f, 0.55f,
            0.0f, 1.9f, 2800.0f, 4.1f, 0.15f, 0.26f, 9900.0f,
            -12.2f, 3.3f, 4.8f, 106.0f, 2.1f, -0.3f, 0.70f, 0.24f, 0.30f, 0.1f),

        makePreset("BL 18 Mau5 Pluck Bell Ping",
            1.0f, 2.0f, 175.0f, 0.98f, 0.46f, 0.05f, 2.0f, 0.44f,
            698.46f, 5.0f, 5.0f, 0.0f, 4.0f, 14.0f, 265.0f, 0.90f, 1.0f,
            0.0f, 4500.0f, 0.48f, 3.0f, 2150.0f, 4.8f, 0.22f, 0.78f, 0.46f,
            0.0f, 2.2f, 2400.0f, 3.4f, 0.18f, 0.30f, 9200.0f,
            -13.0f, 3.8f, 3.6f, 82.0f, 2.4f, -0.3f, 0.56f, 0.30f, 0.22f, 0.2f),

        makePreset("BL 19 Silk Music Sunset Bell",
            2.0f, 7.5f, 265.0f, 1.30f, 0.52f, 0.11f, 2.0f, 0.46f,
            783.99f, 7.0f, 5.0f, 0.0f, 2.0f, 12.0f, 435.0f, 0.88f, 1.0f,
            1.0f, 4800.0f, 0.38f, 3.0f, 2350.0f, 4.7f, 0.20f, 0.88f, 0.58f,
            0.0f, 1.6f, 2600.0f, 3.6f, 0.12f, 0.22f, 9400.0f,
            -11.8f, 3.1f, 5.2f, 112.0f, 1.9f, -0.3f, 0.74f, 0.24f, 0.32f, 0.0f),

        makePreset("BL 20 Goa 909 Ride Bell",
            1.0f, 2.0f, 195.0f, 1.04f, 0.48f, 0.06f, 2.0f, 0.72f,
            1396.91f, 5.0f, 6.0f, 0.0f, 2.5f, 11.0f, 305.0f, 0.90f, 1.0f,
            2.0f, 6100.0f, 0.56f, 3.0f, 2850.0f, 5.9f, 0.32f, 0.80f, 0.50f,
            1.0f, 2.6f, 3400.0f, 4.4f, 0.26f, 0.38f, 10100.0f,
            -13.5f, 4.2f, 3.0f, 86.0f, 2.8f, -0.3f, 0.54f, 0.28f, 0.20f, 0.4f),

        makePreset("BL 21 Berlin Industrial Anvil Bell",
            1.0f, 3.0f, 210.0f, 1.08f, 0.52f, 0.07f, 2.0f, 0.88f,
            493.88f, 11.0f, 4.0f, 0.0f, 6.5f, 18.0f, 320.0f, 0.92f, 1.0f,
            2.0f, 4300.0f, 0.64f, 3.0f, 2050.0f, 6.2f, 0.40f, 0.82f, 0.52f,
            3.0f, 3.2f, 2100.0f, 3.4f, 0.44f, 0.56f, 8400.0f,
            -15.0f, 5.2f, 2.4f, 88.0f, 3.5f, -0.2f, 0.60f, 0.32f, 0.24f, 0.8f),

        makePreset("BL 22 Café del Mar Harmonic Chime",
            2.0f, 10.0f, 310.0f, 1.44f, 0.54f, 0.14f, 2.0f, 0.42f,
            1046.50f, 0.0f, 6.0f, 0.0f, 1.0f, 10.0f, 500.0f, 0.88f, 1.0f,
            1.0f, 5500.0f, 0.36f, 3.0f, 2650.0f, 4.6f, 0.18f, 0.94f, 0.66f,
            0.0f, 1.5f, 3000.0f, 4.4f, 0.08f, 0.16f, 10400.0f,
            -11.5f, 3.0f, 5.8f, 128.0f, 1.8f, -0.3f, 0.84f, 0.18f, 0.40f, 0.0f),

        makePreset("BL 23 Melodic Keyboard FM Bell",
            1.0f, 2.0f, 240.0f, 1.20f, 0.48f, 0.05f, 2.0f, 0.58f,
            880.0f, 9.0f, 5.0f, 0.0f, 1.5f, 10.0f, 390.0f, 0.92f, 1.0f,
            1.0f, 5000.0f, 0.42f, 3.0f, 2400.0f, 5.0f, 0.22f, 0.84f, 0.52f,
            0.0f, 1.8f, 2800.0f, 4.0f, 0.14f, 0.24f, 9800.0f,
            -12.0f, 3.2f, 4.8f, 104.0f, 2.0f, -0.3f, 0.68f, 0.24f, 0.28f, 0.0f),

        makePreset("BL 24 Beat End Reverse Bell",
            3.0f, 16.0f, 280.0f, 1.35f, 0.62f, 0.18f, 2.0f, 0.65f,
            987.77f, 11.0f, 5.0f, 0.0f, 4.0f, 22.0f, 450.0f, 0.90f, 1.0f,
            1.0f, 5400.0f, 0.44f, 3.0f, 2550.0f, 5.3f, 0.28f, 0.92f, 0.64f,
            0.0f, 2.0f, 2900.0f, 4.2f, 0.18f, 0.30f, 10000.0f,
            -12.5f, 3.6f, 4.5f, 114.0f, 2.2f, -0.3f, 0.78f, 0.20f, 0.36f, 0.2f,
            1.0f, 0.65f),

        // =========================================================================
        // TM 01 - TM 24: CLUB, MELODIC & PSY-TECH TOM-TOMS (24 Presets)
        // =========================================================================
        makePreset("TM 01 Anjunabeats Groove Tom",
            1.0f, 3.5f, 120.0f, 1.05f, 0.45f, 0.10f, 1.0f, 0.18f,
            110.0f, 9.0f, 2.0f, 0.0f, 14.0f, 36.0f, 240.0f, 0.92f, 1.0f,
            1.0f, 1800.0f, 0.48f, 2.0f, 700.0f, 2.6f, 0.22f, 0.55f, 0.30f,
            1.0f, 1.2f, 900.0f, 1.5f, 0.28f, 0.40f, 5200.0f,
            -13.5f, 4.2f, 4.5f, 85.0f, 2.8f, -0.3f, 0.44f, 0.48f, 0.14f, 0.5f),

        makePreset("TM 02 Kandi Melodic Tom Fill",
            1.0f, 4.0f, 115.0f, 1.02f, 0.46f, 0.09f, 1.0f, 0.14f,
            146.83f, 2.0f, 3.0f, 0.0f, 13.0f, 32.0f, 225.0f, 0.90f, 1.0f,
            1.0f, 2100.0f, 0.46f, 2.0f, 800.0f, 2.7f, 0.20f, 0.68f, 0.45f,
            0.0f, 1.6f, 1100.0f, 2.0f, 0.22f, 0.34f, 5800.0f,
            -13.0f, 4.0f, 4.2f, 80.0f, 2.5f, -0.3f, 0.48f, 0.44f, 0.16f, 0.4f),

        makePreset("TM 03 Sean Tyas Psy-Tech Laser Tom",
            1.0f, 3.0f, 105.0f, 0.98f, 0.44f, 0.06f, 1.0f, 0.34f,
            130.81f, 0.0f, 3.0f, 0.0f, 22.0f, 28.0f, 205.0f, 0.94f, 1.0f,
            3.0f, 3200.0f, 0.62f, 1.0f, 1100.0f, 3.4f, 0.24f, 0.62f, 0.38f,
            2.0f, 2.4f, 1400.0f, 2.8f, 0.38f, 0.48f, 6600.0f,
            -15.0f, 5.2f, 2.5f, 72.0f, 3.4f, -0.2f, 0.38f, 0.48f, 0.12f, 0.8f),

        makePreset("TM 04 Rotterdam Stadium Floor Tom",
            1.0f, 3.5f, 150.0f, 1.18f, 0.48f, 0.08f, 1.0f, 0.20f,
            98.0f, 7.0f, 2.0f, 0.0f, 15.0f, 42.0f, 290.0f, 0.94f, 1.0f,
            1.0f, 1650.0f, 0.52f, 2.0f, 650.0f, 2.8f, 0.26f, 0.74f, 0.48f,
            1.0f, 1.6f, 850.0f, 2.2f, 0.34f, 0.46f, 5400.0f,
            -15.0f, 5.0f, 3.8f, 98.0f, 3.4f, -0.2f, 0.62f, 0.38f, 0.24f, 1.0f),

        makePreset("TM 05 Coldharbour Deep Rolling Tom",
            1.0f, 3.0f, 130.0f, 1.08f, 0.45f, 0.08f, 1.0f, 0.22f,
            87.31f, 5.0f, 2.0f, 0.0f, 14.0f, 38.0f, 265.0f, 0.94f, 1.0f,
            1.0f, 1500.0f, 0.48f, 2.0f, 600.0f, 2.7f, 0.22f, 0.58f, 0.36f,
            1.0f, 1.4f, 800.0f, 1.4f, 0.34f, 0.46f, 4800.0f,
            -14.5f, 4.8f, 4.0f, 90.0f, 3.2f, -0.2f, 0.46f, 0.48f, 0.16f, 0.8f),

        makePreset("TM 06 Aly & Fila Cairo Tribal Tom",
            1.0f, 3.5f, 125.0f, 1.04f, 0.48f, 0.10f, 1.0f, 0.24f,
            123.47f, 11.0f, 2.0f, 0.0f, 15.0f, 34.0f, 240.0f, 0.92f, 1.0f,
            1.0f, 2000.0f, 0.54f, 2.0f, 800.0f, 2.9f, 0.24f, 0.70f, 0.44f,
            1.0f, 2.0f, 1050.0f, 2.2f, 0.30f, 0.42f, 5800.0f,
            -14.0f, 4.6f, 3.6f, 84.0f, 3.0f, -0.3f, 0.52f, 0.42f, 0.18f, 0.6f),

        makePreset("TM 07 High Contrast Tribal Tom",
            1.0f, 3.5f, 130.0f, 1.06f, 0.48f, 0.10f, 1.0f, 0.22f,
            116.54f, 10.0f, 2.0f, 0.0f, 15.0f, 36.0f, 250.0f, 0.92f, 1.0f,
            1.0f, 1900.0f, 0.52f, 2.0f, 750.0f, 2.8f, 0.24f, 0.66f, 0.42f,
            1.0f, 1.8f, 950.0f, 2.0f, 0.32f, 0.44f, 5600.0f,
            -14.0f, 4.6f, 3.8f, 86.0f, 3.0f, -0.3f, 0.50f, 0.44f, 0.16f, 0.6f),

        makePreset("TM 08 Gatecrasher 2000 Synth Tom",
            1.0f, 3.0f, 115.0f, 1.00f, 0.46f, 0.08f, 1.0f, 0.28f,
            138.59f, 1.0f, 3.0f, 0.0f, 18.0f, 30.0f, 220.0f, 0.92f, 1.0f,
            3.0f, 2800.0f, 0.56f, 1.0f, 1000.0f, 3.2f, 0.24f, 0.68f, 0.44f,
            1.0f, 2.2f, 1250.0f, 2.4f, 0.32f, 0.44f, 6200.0f,
            -14.0f, 4.6f, 3.2f, 78.0f, 3.0f, -0.3f, 0.46f, 0.44f, 0.16f, 0.6f),

        makePreset("TM 09 Anjunadeep Warm Percussive Tom",
            1.0f, 3.0f, 120.0f, 1.04f, 0.45f, 0.10f, 1.0f, 0.12f,
            103.83f, 8.0f, 2.0f, 0.0f, 12.0f, 34.0f, 235.0f, 0.90f, 1.0f,
            1.0f, 1600.0f, 0.44f, 2.0f, 625.0f, 2.5f, 0.18f, 0.62f, 0.38f,
            0.0f, 1.2f, 850.0f, 1.2f, 0.20f, 0.32f, 4900.0f,
            -13.0f, 3.8f, 4.6f, 84.0f, 2.4f, -0.3f, 0.54f, 0.46f, 0.18f, 0.4f),

        makePreset("TM 10 Prydz Muranyi Electro Tom",
            1.0f, 2.5f, 120.0f, 1.02f, 0.46f, 0.06f, 1.0f, 0.26f,
            110.0f, 9.0f, 2.0f, 0.0f, 16.0f, 32.0f, 230.0f, 0.94f, 1.0f,
            2.0f, 2400.0f, 0.58f, 1.0f, 900.0f, 3.0f, 0.24f, 0.70f, 0.44f,
            1.0f, 2.2f, 1050.0f, 2.4f, 0.36f, 0.48f, 5900.0f,
            -14.5f, 5.0f, 3.2f, 82.0f, 3.2f, -0.2f, 0.52f, 0.40f, 0.18f, 0.8f),

        makePreset("TM 11 Ferry Corsten Rock Your Body Tom",
            1.0f, 3.0f, 110.0f, 0.98f, 0.46f, 0.07f, 1.0f, 0.30f,
            130.81f, 0.0f, 3.0f, 0.0f, 17.0f, 28.0f, 215.0f, 0.92f, 1.0f,
            3.0f, 2900.0f, 0.56f, 1.0f, 1050.0f, 3.2f, 0.26f, 0.66f, 0.42f,
            2.0f, 2.4f, 1200.0f, 2.6f, 0.38f, 0.50f, 6400.0f,
            -14.5f, 5.0f, 2.8f, 76.0f, 3.2f, -0.2f, 0.44f, 0.44f, 0.14f, 0.8f),

        makePreset("TM 12 Simmons 80s Trance Synth Tom",
            1.0f, 2.5f, 135.0f, 1.04f, 0.46f, 0.06f, 1.0f, 0.24f,
            146.83f, 2.0f, 3.0f, 0.0f, 20.0f, 34.0f, 245.0f, 0.92f, 1.0f,
            3.0f, 2600.0f, 0.54f, 1.0f, 950.0f, 3.0f, 0.28f, 0.72f, 0.46f,
            1.0f, 2.0f, 1150.0f, 2.4f, 0.28f, 0.40f, 6000.0f,
            -14.0f, 4.5f, 3.4f, 84.0f, 3.0f, -0.3f, 0.56f, 0.40f, 0.20f, 0.6f),

        makePreset("TM 13 Subculture 140 Rolling Bass Tom",
            1.0f, 2.0f, 110.0f, 1.00f, 0.42f, 0.05f, 1.0f, 0.20f,
            82.41f, 4.0f, 2.0f, 0.0f, 14.0f, 30.0f, 225.0f, 0.95f, 1.0f,
            1.0f, 1550.0f, 0.50f, 2.0f, 575.0f, 2.7f, 0.20f, 0.48f, 0.28f,
            1.0f, 1.6f, 780.0f, 1.6f, 0.36f, 0.48f, 4800.0f,
            -15.0f, 5.2f, 3.2f, 78.0f, 3.4f, -0.2f, 0.34f, 0.52f, 0.10f, 0.9f),

        makePreset("TM 14 Above & Beyond Red Rocks Canyon Tom",
            2.0f, 6.5f, 160.0f, 1.22f, 0.54f, 0.12f, 1.0f, 0.20f,
            98.0f, 7.0f, 2.0f, 0.0f, 15.0f, 40.0f, 300.0f, 0.94f, 1.0f,
            1.0f, 1750.0f, 0.54f, 2.0f, 700.0f, 2.8f, 0.26f, 0.82f, 0.54f,
            1.0f, 1.8f, 900.0f, 2.4f, 0.32f, 0.44f, 5600.0f,
            -14.5f, 4.8f, 3.8f, 102.0f, 3.2f, -0.2f, 0.72f, 0.34f, 0.28f, 0.8f),

        makePreset("TM 15 Armada Progressive Groove Tom",
            1.0f, 3.0f, 118.0f, 1.02f, 0.45f, 0.08f, 1.0f, 0.16f,
            110.0f, 9.0f, 2.0f, 0.0f, 13.0f, 32.0f, 230.0f, 0.92f, 1.0f,
            1.0f, 1850.0f, 0.48f, 2.0f, 725.0f, 2.6f, 0.20f, 0.60f, 0.36f,
            0.0f, 1.5f, 920.0f, 1.8f, 0.26f, 0.38f, 5300.0f,
            -13.5f, 4.2f, 4.0f, 82.0f, 2.8f, -0.3f, 0.46f, 0.46f, 0.15f, 0.5f),

        makePreset("TM 16 Vandit Paul's Octapad Tom",
            1.0f, 3.0f, 110.0f, 0.98f, 0.45f, 0.07f, 1.0f, 0.18f,
            164.81f, 4.0f, 3.0f, 0.0f, 14.0f, 28.0f, 210.0f, 0.90f, 1.0f,
            0.0f, 2300.0f, 0.52f, 2.0f, 875.0f, 2.8f, 0.22f, 0.68f, 0.42f,
            0.0f, 1.8f, 1180.0f, 2.2f, 0.24f, 0.36f, 6000.0f,
            -13.5f, 4.2f, 3.6f, 76.0f, 2.8f, -0.3f, 0.46f, 0.44f, 0.16f, 0.5f),

        makePreset("TM 17 Astrix & Tyas Psy Zapper Tom",
            1.0f, 2.0f, 95.0f, 0.94f, 0.42f, 0.05f, 1.0f, 0.38f,
            146.83f, 2.0f, 3.0f, 0.0f, 24.0f, 24.0f, 190.0f, 0.94f, 1.0f,
            3.0f, 3500.0f, 0.66f, 1.0f, 1200.0f, 3.6f, 0.24f, 0.64f, 0.40f,
            2.0f, 2.6f, 1450.0f, 3.0f, 0.40f, 0.52f, 6800.0f,
            -15.0f, 5.4f, 2.2f, 68.0f, 3.5f, -0.2f, 0.36f, 0.48f, 0.12f, 0.8f),

        makePreset("TM 18 Gouryella Ligaya High Melodic Tom",
            1.0f, 3.5f, 115.0f, 1.02f, 0.46f, 0.08f, 1.0f, 0.16f,
            196.0f, 7.0f, 3.0f, 0.0f, 12.0f, 28.0f, 225.0f, 0.90f, 1.0f,
            1.0f, 2400.0f, 0.48f, 2.0f, 925.0f, 2.8f, 0.20f, 0.74f, 0.48f,
            0.0f, 1.8f, 1300.0f, 2.4f, 0.22f, 0.34f, 6200.0f,
            -13.0f, 4.0f, 3.8f, 80.0f, 2.6f, -0.3f, 0.54f, 0.40f, 0.18f, 0.4f),

        makePreset("TM 19 Mau5 Progressive Tight Tom",
            1.0f, 2.0f, 100.0f, 0.94f, 0.42f, 0.05f, 1.0f, 0.14f,
            110.0f, 9.0f, 2.0f, 0.0f, 13.0f, 26.0f, 195.0f, 0.92f, 1.0f,
            0.0f, 1950.0f, 0.50f, 2.0f, 750.0f, 2.6f, 0.18f, 0.50f, 0.28f,
            1.0f, 1.6f, 950.0f, 1.8f, 0.26f, 0.38f, 5400.0f,
            -13.5f, 4.4f, 3.2f, 70.0f, 2.8f, -0.3f, 0.36f, 0.50f, 0.10f, 0.5f),

        makePreset("TM 20 Ibiza Terrace Bongo-Tom",
            1.0f, 3.5f, 105.0f, 0.96f, 0.46f, 0.10f, 1.0f, 0.20f,
            174.61f, 5.0f, 3.0f, 0.0f, 11.0f, 24.0f, 200.0f, 0.90f, 1.0f,
            1.0f, 2500.0f, 0.52f, 2.0f, 950.0f, 2.9f, 0.20f, 0.68f, 0.44f,
            0.0f, 2.2f, 1350.0f, 2.2f, 0.22f, 0.34f, 6000.0f,
            -13.0f, 4.0f, 3.4f, 74.0f, 2.6f, -0.3f, 0.46f, 0.42f, 0.16f, 0.4f),

        makePreset("TM 21 Sub-Bass 808 Boom Tom",
            1.0f, 2.0f, 165.0f, 1.24f, 0.44f, 0.05f, 1.0f, 0.12f,
            65.41f, 0.0f, 2.0f, 0.0f, 16.0f, 44.0f, 340.0f, 0.96f, 1.0f,
            1.0f, 1400.0f, 0.46f, 2.0f, 525.0f, 2.5f, 0.16f, 0.42f, 0.22f,
            1.0f, 1.2f, 700.0f, 1.2f, 0.34f, 0.46f, 4400.0f,
            -15.0f, 5.0f, 4.2f, 105.0f, 3.4f, -0.2f, 0.36f, 0.52f, 0.10f, 1.0f),

        makePreset("TM 22 Ilan Bluestone Big Flam Tom",
            3.0f, 11.0f, 145.0f, 1.15f, 0.58f, 0.14f, 1.0f, 0.22f,
            110.0f, 9.0f, 2.0f, 0.0f, 15.0f, 36.0f, 280.0f, 0.94f, 1.0f,
            2.0f, 2100.0f, 0.56f, 2.0f, 800.0f, 2.9f, 0.26f, 0.80f, 0.52f,
            1.0f, 1.8f, 980.0f, 2.4f, 0.34f, 0.46f, 5800.0f,
            -14.5f, 4.8f, 3.6f, 94.0f, 3.2f, -0.2f, 0.64f, 0.36f, 0.24f, 0.8f),

        makePreset("TM 23 Note-Tracked Keyboard Tom Rack",
            1.0f, 2.5f, 115.0f, 1.00f, 0.44f, 0.06f, 1.0f, 0.16f,
            110.0f, 9.0f, 2.0f, 0.0f, 12.0f, 30.0f, 230.0f, 0.92f, 1.0f,
            1.0f, 1900.0f, 0.48f, 2.0f, 750.0f, 2.7f, 0.20f, 0.64f, 0.40f,
            0.0f, 1.6f, 1000.0f, 2.0f, 0.24f, 0.36f, 5600.0f,
            -13.0f, 4.0f, 3.8f, 80.0f, 2.6f, -0.3f, 0.46f, 0.44f, 0.16f, 0.4f),

        makePreset("TM 24 Beat 4 Reverse Tom Pickup",
            3.0f, 15.5f, 140.0f, 1.12f, 0.58f, 0.16f, 1.0f, 0.24f,
            123.47f, 11.0f, 2.0f, 0.0f, 16.0f, 36.0f, 270.0f, 0.92f, 1.0f,
            1.0f, 2000.0f, 0.52f, 2.0f, 800.0f, 2.8f, 0.26f, 0.82f, 0.54f,
            1.0f, 1.8f, 1020.0f, 2.2f, 0.30f, 0.42f, 5800.0f,
            -14.0f, 4.6f, 3.8f, 92.0f, 3.0f, -0.3f, 0.60f, 0.38f, 0.22f, 0.6f,
            2.0f, 0.60f),
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
