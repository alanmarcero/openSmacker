#include "Presets.h"
#include "Parameters.h"

#include <algorithm>
#include <ranges>

namespace opensmacker::presets {
using namespace opensmacker::params;
namespace {
struct Patch {
    const char* name;
    int family;
    int midi, decayMs, shellHz, sweep;
    float fm;
    int wireMs;
    float wire;
    core::ClickType click;
    int clickHz;
    float room;
    int strikes;
    float gain, hpf, body, q, highDb, lowDb, tension, width, clickLvl;
    int pitchMs, wireHp, wireLp, air;
    float spread, midDb, sustain;
};
using enum core::ClickType;

constexpr std::array<core::VoiceMode, 4> kVoices{{
    core::VoiceMode::DanceSnare, core::VoiceMode::FmBell,
    core::VoiceMode::ClubTom, core::VoiceMode::PrydaRimSmack
}};
constexpr std::array<int, 4> kDefaultAir{{ 0, 2, 1, 0 }};

// Family: 0 snare, 1 bell, 2 tom, 3 rim.
// Snares and rims are noise or a short knock. Bells stay pitched. Toms keep a drum pitch.
constexpr std::array<Patch, 84> bank{{
    { "Uplifting / Backbeat Snare", 0, 55, 120, 170, 9, 0.10f, 140, 0.446f, Stick, 3200, 0.030f, 1, -2.0f, 100.0f, 0.60f, 1.4f, 0.5f, -1.0f, 0.62f, 0.95f, 0.35f, 11, 350, 7500, 0, 0.00f, -2.0f, 0.34f },
    { "Uplifting / Euphoria Snare", 0, 59, 140, 260, 9, 0.14f, 165, 0.277f, Stick, 4100, 0.050f, 1, -1.5f, 150.0f, 0.41f, 1.5f, 2.5f, -1.5f, 0.55f, 1.25f, 0.23f, 10, 800, 12000, 1, 0.05f, 3.5f, 0.36f },
    { "Uplifting / Silver Mesh Snare", 0, 67, 64, 420, 6, 0.16f, 110, 0.450f, Stick, 5600, 0.020f, 1, -4.0f, 260.0f, 0.33f, 1.3f, 3.5f, -2.5f, 0.90f, 1.05f, 0.32f, 8, 1900, 17000, 1, 0.12f, 4.5f, 0.28f },
    { "Uplifting / Skyward Snare", 0, 71, 95, 560, 12, 0.18f, 125, 0.321f, Laser, 6400, 0.040f, 1, -1.5f, 140.0f, 0.17f, 1.6f, 4.5f, -1.0f, 0.78f, 1.15f, 0.30f, 9, 2000, 17000, 1, 0.08f, 2.0f, 0.30f },
    { "Uplifting / Pure 138 Snare", 0, 64, 72, 360, 5, 0.08f, 100, 0.350f, Stick, 5200, 0.020f, 1, -2.0f, 110.0f, 0.48f, 1.4f, 0.0f, -1.5f, 0.70f, 0.80f, 0.42f, 12, 1500, 15000, 1, 0.00f, 5.0f, 0.30f },
    { "Uplifting / Wide Backhand Snare", 0, 54, 130, 230, 10, 0.12f, 170, 0.374f, Rimshot, 2800, 0.060f, 1, -1.5f, 80.0f, 0.47f, 1.5f, 2.0f, 0.0f, 0.35f, 1.60f, 0.30f, 14, 650, 12500, 0, -0.05f, -2.5f, 0.40f },
    { "Uplifting / Glass Pin Bell", 1, 76, 170, 1400, 0, 0.36f, 18, 0.012f, Mallet, 2800, 0.020f, 1, -2.0f, 220.0f, 0.47f, 3.2f, 0.5f, -1.0f, 0.30f, 0.70f, 0.13f, 8, 1800, 9000, -1, 0.00f, 5.0f, 0.58f },
    { "Uplifting / Cut Crystal Bell", 1, 81, 150, 2100, 0, 0.22f, 15, 0.015f, Stick, 5200, 0.020f, 1, -0.5f, 340.0f, 0.74f, 2.6f, 2.0f, -1.5f, 0.25f, 0.85f, 0.30f, 6, 2200, 12000, -1, 0.10f, -4.0f, 0.50f },
    { "Uplifting / Arc Light Chime", 1, 79, 185, 1900, 2, 0.62f, 22, 0.025f, Laser, 4400, 0.030f, 1, -2.0f, 260.0f, 0.66f, 4.2f, 1.0f, -1.0f, 0.35f, 0.95f, 0.17f, 7, 1600, 10000, -1, 0.18f, 1.5f, 0.60f },
    { "Uplifting / Blue Glass Bell", 1, 74, 210, 1200, 0, 0.28f, 24, 0.014f, Mallet, 2400, 0.040f, 1, -2.0f, 180.0f, 0.54f, 3.4f, -1.0f, -0.5f, 0.28f, 0.65f, 0.13f, 10, 1400, 8000, -1, -0.08f, 3.5f, 0.64f },
    { "Uplifting / After Ring Chime", 1, 77, 250, 1600, 0, 0.48f, 30, 0.025f, Mallet, 3200, 0.050f, 1, 0.5f, 200.0f, 0.82f, 3.8f, 0.0f, -1.0f, 0.32f, 0.80f, 0.16f, 9, 1500, 8500, -1, 0.05f, -5.0f, 0.68f },
    { "Uplifting / Morning Chime", 1, 71, 195, 1500, 1, 0.33f, 20, 0.014f, Mallet, 3600, 0.040f, 1, -1.5f, 240.0f, 0.54f, 3.0f, 1.0f, -1.0f, 0.30f, 0.75f, 0.17f, 8, 1700, 9500, -1, 0.00f, 4.5f, 0.56f },
    { "Uplifting / Anthem Tom", 2, 45, 110, 180, 8, 0.10f, 55, 0.140f, Stick, 2400, 0.030f, 1, -1.5f, 70.0f, 0.72f, 1.5f, 1.5f, -1.0f, 0.35f, 0.70f, 0.40f, 18, 900, 10000, 1, 0.00f, -3.5f, 0.48f },
    { "Uplifting / Lift Fill Tom", 2, 64, 85, 280, 6, 0.14f, 40, 0.094f, Stick, 3100, 0.020f, 2, -2.0f, 136.0f, 0.68f, 1.6f, 2.0f, -2.5f, 0.40f, 0.85f, 0.40f, 12, 1400, 13000, 1, 0.06f, 3.0f, 0.40f },
    { "Uplifting / Upper Rack Tom", 2, 67, 70, 320, 4, 0.12f, 36, 0.080f, Rimshot, 3300, 0.020f, 1, 0.0f, 148.0f, 0.72f, 1.5f, 3.5f, -3.0f, 0.42f, 0.75f, 0.38f, 10, 1800, 15000, 1, 0.04f, -4.5f, 0.38f },
    { "Uplifting / High Skin Tom", 2, 69, 60, 360, 3, 0.16f, 32, 0.053f, Rimshot, 3600, 0.015f, 1, -2.0f, 156.0f, 0.64f, 1.7f, 4.0f, -3.0f, 0.50f, 0.80f, 0.45f, 9, 2000, 16000, 1, 0.10f, 2.0f, 0.34f },
    { "Uplifting / Horizon Tom", 2, 47, 190, 105, 3, 0.08f, 60, 0.320f, Mallet, 2100, 0.040f, 1, 0.0f, 80.0f, 0.72f, 1.4f, -3.0f, -2.0f, 0.30f, 0.65f, 0.32f, 22, 220, 3000, 2, -0.06f, -2.5f, 0.52f },
    { "Uplifting / Starlight Tom", 2, 60, 100, 240, 9, 0.13f, 48, 0.082f, Laser, 2800, 0.030f, 1, -2.0f, 120.0f, 0.49f, 1.6f, 3.0f, -2.0f, 0.38f, 0.90f, 0.25f, 14, 1200, 12000, 1, 0.02f, 5.0f, 0.44f },
    { "Uplifting / Mainstage Rim", 3, 62, 85, 700, 4, 0.16f, 100, 0.420f, Rimshot, 1800, 0.030f, 1, 2.5f, 180.0f, 0.48f, 1.4f, 1.5f, -2.0f, 0.60f, 0.85f, 0.38f, 8, 280, 4500, -1, 0.00f, -4.0f, 0.40f },
    { "Uplifting / Pryda Knock", 3, 53, 85, 420, 6, 0.18f, 100, 0.420f, Rimshot, 1400, 0.040f, 1, 1.5f, 160.0f, 0.48f, 1.6f, -1.0f, -1.0f, 0.55f, 0.75f, 0.38f, 10, 250, 4200, -1, -0.04f, 1.5f, 0.40f },
    { "Uplifting / Bright Rail Rim", 3, 69, 85, 1500, 3, 0.12f, 100, 0.420f, Mallet, 900, 0.010f, 1, -1.0f, 180.0f, 0.48f, 1.3f, 3.0f, -3.0f, 0.78f, 0.95f, 0.38f, 6, 700, 10000, 1, 0.12f, 3.5f, 0.40f },
    { "Uplifting / Edge Pop Rim", 3, 61, 85, 900, 5, 0.14f, 100, 0.420f, Laser, 3200, 0.020f, 1, 0.5f, 180.0f, 0.48f, 1.4f, 1.0f, -2.0f, 0.64f, 0.80f, 0.38f, 7, 360, 5500, -1, 0.04f, -5.0f, 0.40f },
    { "Uplifting / Backwall Rim", 3, 48, 90, 500, 5, 0.15f, 100, 0.420f, Stick, 1500, 0.050f, 1, -0.5f, 180.0f, 0.48f, 1.5f, -0.5f, -1.5f, 0.50f, 0.70f, 0.38f, 11, 600, 8500, -1, -0.06f, 4.5f, 0.40f },
    { "Uplifting / Euphoria Rim", 3, 64, 85, 1100, 4, 0.13f, 100, 0.420f, Rimshot, 4500, 0.030f, 1, -0.5f, 180.0f, 0.48f, 1.4f, 2.0f, -2.0f, 0.66f, 1.10f, 0.38f, 7, 900, 12000, -1, 0.08f, -3.5f, 0.40f },
    { "Rolling / Shuffle Wire Snare", 0, 59, 112, 260, 9, 0.11f, 150, 0.448f, Stick, 4500, 0.020f, 2, -2.0f, 140.0f, 0.55f, 1.4f, 1.0f, -1.0f, 0.40f, 1.20f, 0.31f, 13, 600, 11000, 0, -0.10f, 3.0f, 0.36f },
    { "Rolling / Pickup Drag Snare", 0, 53, 120, 190, 6, 0.09f, 145, 0.493f, Stick, 2600, 0.040f, 3, 0.0f, 100.0f, 0.76f, 1.3f, -2.0f, -0.5f, 0.30f, 1.05f, 0.32f, 16, 250, 5500, 2, -0.08f, -4.5f, 0.38f },
    { "Rolling / Short Fuse Snare", 0, 69, 58, 500, 11, 0.15f, 70, 0.227f, Stick, 5000, 0.000f, 1, -2.0f, 200.0f, 0.43f, 1.5f, 3.0f, -2.5f, 0.88f, 0.70f, 0.38f, 7, 2000, 16000, 1, 0.14f, 5.0f, 0.24f },
    { "Rolling / Glass Pair Bell", 1, 77, 155, 1550, 0, 0.34f, 16, 0.020f, Stick, 4200, 0.030f, 2, 1.0f, 250.0f, 0.74f, 3.0f, 1.5f, -1.0f, 0.32f, 0.90f, 0.26f, 7, 1900, 10000, -1, 0.06f, -2.5f, 0.52f },
    { "Rolling / Quiet Chime", 1, 72, 230, 1100, 0, 0.18f, 28, 0.013f, Mallet, 2200, 0.050f, 1, -1.5f, 170.0f, 0.48f, 2.8f, -1.5f, -0.5f, 0.22f, 0.55f, 0.10f, 12, 1200, 7000, -1, -0.12f, 5.0f, 0.66f },
    { "Rolling / Small Alloy Bell", 1, 70, 140, 1000, 1, 0.55f, 18, 0.030f, Stick, 3900, 0.020f, 1, 1.5f, 200.0f, 0.76f, 4.0f, 0.5f, -1.0f, 0.40f, 0.75f, 0.28f, 6, 1500, 9500, -1, 0.16f, -4.0f, 0.48f },
    { "Rolling / Triplet Tom", 2, 59, 75, 230, 10, 0.11f, 30, 0.072f, Laser, 2000, 0.000f, 1, -2.0f, 116.0f, 0.65f, 1.4f, 2.5f, -2.0f, 0.32f, 0.60f, 0.30f, 11, 1100, 11500, 1, 0.00f, 1.5f, 0.36f },
    { "Rolling / Rack Bounce Tom", 2, 51, 100, 170, 4, 0.09f, 50, 0.300f, Stick, 2600, 0.025f, 2, -1.5f, 68.0f, 0.72f, 1.5f, 0.0f, -1.0f, 0.36f, 0.72f, 0.40f, 16, 400, 5500, 0, -0.04f, 3.5f, 0.46f },
    { "Rolling / Quick Step Tom", 2, 62, 65, 260, 5, 0.13f, 28, 0.080f, Stick, 2300, 0.000f, 1, -1.5f, 128.0f, 0.72f, 1.6f, 3.0f, -2.5f, 0.44f, 0.68f, 0.36f, 8, 1500, 14000, 0, 0.08f, -5.0f, 0.32f },
    { "Rolling / Clack Pair Rim", 3, 60, 85, 800, 3, 0.10f, 100, 0.420f, Mallet, 1400, 0.020f, 2, 2.0f, 180.0f, 0.48f, 1.3f, 0.5f, -2.0f, 0.58f, 1.00f, 0.38f, 8, 280, 4500, -1, 0.00f, 4.5f, 0.40f },
    { "Rolling / Rim Pocket", 3, 72, 85, 1800, 5, 0.16f, 100, 0.420f, Laser, 5500, 0.030f, 1, 1.0f, 150.0f, 0.48f, 1.5f, -0.5f, -1.0f, 0.48f, 0.65f, 0.38f, 12, 450, 7000, -1, -0.08f, -3.5f, 0.40f },
    { "Rolling / Stick Drag Rim", 3, 63, 85, 980, 3, 0.11f, 100, 0.420f, Rimshot, 3800, 0.020f, 3, 0.5f, 180.0f, 0.48f, 1.4f, 1.0f, -2.0f, 0.52f, 1.15f, 0.38f, 9, 700, 10000, -1, 0.06f, 3.0f, 0.40f },
    { "Progressive / Wire Brush Snare", 0, 61, 100, 240, 4, 0.06f, 180, 0.820f, Mallet, 2200, 0.040f, 1, -1.5f, 140.0f, 0.40f, 1.3f, -1.0f, 0.0f, 0.22f, 1.10f, 0.28f, 20, 500, 7000, 2, -0.10f, -3.5f, 0.46f },
    { "Progressive / Half Time Snare", 0, 42, 210, 100, 5, 0.08f, 190, 0.450f, Stick, 1600, 0.060f, 1, 0.0f, 55.0f, 0.86f, 1.2f, -2.0f, 0.5f, 0.25f, 0.85f, 0.26f, 24, 160, 2800, 0, -0.12f, -4.5f, 0.50f },
    { "Progressive / Back Room Snare", 0, 51, 160, 150, 7, 0.09f, 160, 0.620f, Stick, 2800, 0.070f, 1, -1.5f, 95.0f, 0.78f, 1.4f, -1.5f, -0.5f, 0.45f, 0.95f, 0.36f, 16, 220, 4800, 2, -0.04f, -4.0f, 0.42f },
    { "Progressive / Tube Choir Bell", 1, 69, 240, 980, 0, 0.40f, 36, 0.020f, Mallet, 2000, 0.050f, 1, -2.0f, 160.0f, 0.52f, 3.6f, -1.0f, -0.5f, 0.26f, 0.60f, 0.10f, 14, 1100, 6500, -1, -0.05f, 5.0f, 0.66f },
    { "Progressive / Bell Drop", 1, 67, 200, 860, 5, 0.26f, 30, 0.030f, Mallet, 1900, 0.040f, 1, 0.5f, 150.0f, 0.76f, 2.8f, -1.5f, -0.5f, 0.24f, 0.55f, 0.18f, 18, 1000, 6000, -1, -0.10f, -4.0f, 0.58f },
    { "Progressive / Porcelain Bell", 1, 80, 175, 1750, 0, 0.16f, 16, 0.014f, Mallet, 3400, 0.030f, 1, -2.0f, 300.0f, 0.70f, 2.4f, 1.5f, -1.5f, 0.22f, 0.70f, 0.21f, 7, 2000, 11000, -1, 0.00f, 1.5f, 0.52f },
    { "Progressive / Round Skin Tom", 2, 58, 110, 200, 3, 0.05f, 70, 0.193f, Stick, 1500, 0.040f, 1, -2.0f, 110.0f, 0.63f, 1.3f, 1.5f, -1.5f, 0.24f, 0.55f, 0.23f, 26, 800, 9000, 1, -0.10f, 3.5f, 0.55f },
    { "Progressive / Deep Pair Tom", 2, 42, 150, 90, 7, 0.07f, 55, 0.160f, Mallet, 1400, 0.030f, 2, 3.0f, 70.0f, 0.72f, 1.2f, -3.5f, -2.5f, 0.22f, 0.50f, 0.24f, 22, 200, 2800, 2, -0.08f, -5.0f, 0.52f },
    { "Progressive / Membrane Tom", 2, 53, 120, 190, 2, 0.06f, 64, 0.159f, Stick, 1450, 0.035f, 1, -2.0f, 92.0f, 0.64f, 1.3f, 2.0f, -1.0f, 0.26f, 0.58f, 0.19f, 28, 800, 9000, 1, -0.12f, 4.5f, 0.56f },
    { "Progressive / Side Stick Rim", 3, 74, 85, 1900, 2, 0.08f, 100, 0.420f, Laser, 6800, 0.015f, 1, -0.5f, 170.0f, 0.48f, 1.2f, 3.5f, -1.5f, 0.40f, 0.55f, 0.38f, 10, 1400, 15000, -1, -0.06f, -3.5f, 0.40f },
    { "Progressive / Hollow Rail Rim", 3, 59, 85, 600, 3, 0.09f, 100, 0.420f, Mallet, 1100, 0.030f, 1, 0.0f, 160.0f, 0.48f, 1.3f, -2.0f, -1.0f, 0.34f, 0.60f, 0.38f, 12, 600, 8500, 2, -0.10f, 3.0f, 0.40f },
    { "Progressive / Cup Knock Rim", 3, 56, 90, 1050, 2, 0.10f, 100, 0.420f, Stick, 2800, 0.040f, 1, 2.5f, 140.0f, 0.48f, 1.3f, -1.5f, -1.0f, 0.30f, 0.50f, 0.38f, 14, 900, 12000, 2, -0.08f, -4.5f, 0.40f },
    { "Psy / Cut Through Snare", 0, 62, 95, 300, 14, 0.20f, 80, 0.399f, Stick, 3600, 0.010f, 1, -2.0f, 240.0f, 0.46f, 1.7f, 1.0f, -3.0f, 0.92f, 0.75f, 0.40f, 6, 900, 10000, 0, 0.16f, 2.0f, 0.24f },
    { "Psy / Taut Skin Snare", 0, 66, 110, 250, 12, 0.18f, 75, 0.500f, Rimshot, 2400, 0.015f, 1, -1.5f, 200.0f, 0.60f, 1.8f, 0.5f, -2.0f, 0.95f, 0.65f, 0.58f, 6, 700, 8000, 0, 0.12f, -2.5f, 0.22f },
    { "Psy / Thin Film Snare", 0, 63, 100, 300, 4, 0.07f, 115, 0.490f, Stick, 6200, 0.000f, 1, -2.0f, 190.0f, 0.43f, 1.4f, 3.0f, -2.5f, 0.70f, 1.00f, 0.30f, 8, 1000, 11000, 0, 0.05f, -3.0f, 0.26f },
    { "Psy / Signal Key Bell", 1, 73, 145, 1850, 3, 0.58f, 20, 0.040f, Laser, 4000, 0.015f, 1, 0.5f, 280.0f, 0.78f, 4.4f, 1.5f, -1.5f, 0.38f, 0.85f, 0.18f, 6, 2000, 11000, -1, 0.20f, -4.0f, 0.50f },
    { "Psy / Clockwork Bell", 1, 86, 120, 2300, 1, 0.66f, 15, 0.020f, Stick, 6000, 0.000f, 1, -1.5f, 400.0f, 0.72f, 4.8f, 2.5f, -2.0f, 0.42f, 0.70f, 0.32f, 5, 2300, 13000, -1, 0.22f, 1.5f, 0.42f },
    { "Psy / Tiny Hammer Bell", 1, 84, 110, 2400, 1, 0.40f, 15, 0.016f, Stick, 6800, 0.000f, 1, -2.0f, 420.0f, 0.54f, 3.5f, 2.0f, -2.0f, 0.36f, 0.65f, 0.28f, 5, 2200, 12500, -1, 0.08f, 3.5f, 0.40f },
    { "Psy / Pitch Pocket Tom", 2, 58, 100, 200, 16, 0.15f, 34, 0.100f, Laser, 2400, 0.020f, 1, -2.0f, 112.0f, 0.72f, 1.6f, 1.5f, -2.0f, 0.40f, 0.70f, 0.40f, 8, 900, 10500, 1, 0.10f, -5.0f, 0.40f },
    { "Psy / Dry Circuit Tom", 2, 55, 80, 210, 12, 0.18f, 30, 0.059f, Laser, 3000, 0.000f, 1, -2.0f, 100.0f, 0.53f, 1.7f, 3.0f, -2.5f, 0.48f, 0.55f, 0.32f, 7, 1600, 14500, 1, 0.14f, 4.5f, 0.34f },
    { "Psy / Wood Tip Tom", 2, 63, 90, 280, 2, 0.10f, 48, 0.094f, Stick, 2900, 0.015f, 1, -2.0f, 140.0f, 0.68f, 1.5f, 3.0f, -1.5f, 0.34f, 0.62f, 0.43f, 15, 1400, 14000, 1, 0.00f, -3.5f, 0.44f },
    { "Psy / Edge Flick Rim", 3, 68, 85, 760, 4, 0.14f, 100, 0.420f, Rimshot, 1600, 0.000f, 1, -0.5f, 180.0f, 0.48f, 1.4f, 3.0f, -3.0f, 0.80f, 0.80f, 0.38f, 5, 280, 4500, 1, 0.14f, 3.0f, 0.40f },
    { "Psy / Chrome Edge Rim", 3, 66, 85, 820, 5, 0.22f, 100, 0.420f, Mallet, 2000, 0.015f, 1, -0.5f, 180.0f, 0.48f, 1.6f, 2.0f, -2.5f, 0.74f, 0.75f, 0.38f, 6, 450, 7000, -1, 0.10f, -4.5f, 0.40f },
    { "Psy / Tiny Knock Rim", 3, 72, 85, 680, 2, 0.08f, 100, 0.420f, Stick, 3000, 0.000f, 1, -1.5f, 180.0f, 0.48f, 1.2f, 2.5f, -3.0f, 0.70f, 0.60f, 0.38f, 5, 700, 10000, 1, 0.05f, 2.0f, 0.40f },
    { "Tech / Crossbar Snare", 0, 56, 115, 200, 8, 0.17f, 95, 0.497f, Rimshot, 2000, 0.020f, 1, -2.0f, 140.0f, 0.63f, 1.6f, -1.0f, -1.5f, 0.80f, 0.70f, 0.51f, 10, 450, 6500, 0, 0.06f, -2.5f, 0.30f },
    { "Tech / Close Mic Snare", 0, 49, 125, 160, 5, 0.06f, 85, 0.620f, Mallet, 1400, 0.000f, 1, -2.0f, 100.0f, 0.72f, 1.3f, -3.0f, -1.0f, 0.50f, 0.25f, 0.30f, 14, 200, 4000, 2, -0.04f, 5.0f, 0.32f },
    { "Tech / Rush Flam Snare", 0, 55, 115, 250, 9, 0.13f, 150, 0.510f, Stick, 3600, 0.030f, 2, -1.0f, 120.0f, 0.70f, 1.5f, 1.0f, -1.0f, 0.58f, 1.15f, 0.42f, 11, 550, 10500, 0, 0.00f, -4.0f, 0.36f },
    { "Tech / Steel Dot Bell", 1, 70, 130, 1150, 2, 0.60f, 16, 0.023f, Rimshot, 2800, 0.010f, 1, -2.0f, 230.0f, 0.71f, 4.5f, 0.5f, -1.5f, 0.45f, 0.65f, 0.28f, 6, 1700, 9000, -1, 0.18f, 1.5f, 0.46f },
    { "Tech / Muted Pipe Bell", 1, 68, 145, 900, 1, 0.20f, 20, 0.021f, Mallet, 2100, 0.000f, 1, -1.5f, 170.0f, 0.53f, 2.4f, -2.0f, -1.0f, 0.28f, 0.50f, 0.11f, 10, 900, 5500, -1, -0.14f, 3.5f, 0.50f },
    { "Tech / Copper Cup Bell", 1, 65, 210, 740, 1, 0.46f, 28, 0.035f, Rimshot, 1700, 0.040f, 1, 1.5f, 140.0f, 0.80f, 3.6f, -1.0f, -0.5f, 0.34f, 0.60f, 0.20f, 12, 800, 6000, -1, 0.04f, -5.0f, 0.62f },
    { "Tech / Floor Pocket Tom", 2, 57, 95, 220, 5, 0.07f, 46, 0.114f, Rimshot, 1400, 0.015f, 1, -1.5f, 108.0f, 0.68f, 1.3f, 2.5f, -2.0f, 0.28f, 0.55f, 0.27f, 18, 1000, 11000, 1, -0.06f, 4.5f, 0.50f },
    { "Tech / Low Runner Tom", 2, 68, 70, 340, 4, 0.09f, 34, 0.080f, Rimshot, 1300, 0.000f, 1, 0.5f, 180.0f, 0.72f, 1.3f, 3.5f, -1.5f, 0.30f, 0.48f, 0.30f, 12, 1600, 15000, 1, 0.00f, -3.5f, 0.42f },
    { "Tech / Club Floor Tom", 2, 62, 80, 240, 8, 0.08f, 50, 0.146f, Stick, 1600, 0.030f, 1, -2.0f, 130.0f, 0.66f, 1.2f, 2.5f, -1.5f, 0.24f, 0.52f, 0.31f, 20, 1200, 12000, 1, -0.08f, 3.0f, 0.54f },
    { "Tech / Hard Lip Rim", 3, 63, 85, 1400, 5, 0.20f, 100, 0.420f, Laser, 4800, 0.010f, 1, -0.5f, 180.0f, 0.48f, 1.5f, 1.5f, -2.5f, 0.76f, 0.70f, 0.38f, 6, 360, 5500, -1, 0.10f, -4.5f, 0.40f },
    { "Tech / Dry Tap Rim", 3, 65, 85, 1200, 2, 0.08f, 100, 0.420f, Rimshot, 2200, 0.000f, 1, 0.5f, 180.0f, 0.48f, 1.2f, 1.0f, -2.5f, 0.62f, 0.45f, 0.38f, 5, 600, 8500, 1, 0.00f, 2.0f, 0.40f },
    { "Tech / Rim Tick", 3, 74, 85, 2000, 4, 0.14f, 100, 0.420f, Laser, 7000, 0.000f, 1, -0.5f, 180.0f, 0.48f, 1.4f, 0.5f, -2.0f, 0.68f, 0.60f, 0.38f, 7, 900, 12000, -1, 0.04f, -2.5f, 0.40f },
    { "Classic / Downbeat Snare", 0, 47, 170, 130, 11, 0.10f, 155, 0.487f, Stick, 2900, 0.050f, 1, -2.0f, 75.0f, 0.68f, 1.4f, -1.5f, 0.0f, 0.42f, 0.85f, 0.33f, 18, 180, 5200, 0, -0.06f, 1.5f, 0.44f },
    { "Classic / Paper Head Snare", 0, 60, 85, 240, 6, 0.08f, 90, 0.480f, Stick, 5400, 0.000f, 1, -0.5f, 160.0f, 0.52f, 1.4f, 2.0f, -2.0f, 0.66f, 0.75f, 0.36f, 8, 600, 9000, 2, 0.08f, -1.5f, 0.26f },
    { "Classic / Low Sling Snare", 0, 43, 150, 110, 9, 0.11f, 140, 0.446f, Rimshot, 2600, 0.035f, 1, -2.0f, 65.0f, 0.69f, 1.3f, -2.0f, 0.5f, 0.34f, 0.80f, 0.43f, 16, 200, 6000, 0, -0.08f, 2.5f, 0.46f },
    { "Classic / Bronze Lip Bell", 1, 64, 205, 680, 0, 0.52f, 32, 0.032f, Rimshot, 1550, 0.035f, 1, -1.5f, 130.0f, 0.64f, 4.0f, -1.5f, -0.5f, 0.36f, 0.55f, 0.17f, 11, 700, 5200, -1, 0.06f, 3.5f, 0.64f },
    { "Classic / Vintage Chime", 1, 71, 225, 1100, 0, 0.30f, 30, 0.025f, Mallet, 2300, 0.050f, 1, 1.5f, 190.0f, 0.78f, 3.2f, -0.5f, -0.5f, 0.28f, 0.65f, 0.16f, 13, 1300, 7500, -1, -0.04f, -5.0f, 0.66f },
    { "Classic / Analog Bell", 1, 67, 190, 880, 1, 0.38f, 26, 0.022f, Stick, 2700, 0.030f, 1, -2.0f, 155.0f, 0.57f, 3.4f, -1.0f, -0.5f, 0.32f, 0.60f, 0.18f, 10, 1100, 6800, -1, 0.00f, 4.5f, 0.58f },
    { "Classic / Dusty Rack Tom", 2, 49, 155, 145, 2, 0.09f, 65, 0.340f, Mallet, 2500, 0.040f, 1, 0.0f, 76.0f, 0.72f, 1.4f, -2.0f, -1.0f, 0.30f, 0.60f, 0.38f, 20, 300, 4000, 2, -0.06f, -3.5f, 0.48f },
    { "Classic / Rubber Head Tom", 2, 43, 210, 95, 2, 0.05f, 50, 0.100f, Mallet, 1700, 0.010f, 1, -0.5f, 58.0f, 0.72f, 1.2f, -4.0f, -2.0f, 0.22f, 0.50f, 0.24f, 24, 180, 2200, 2, -0.14f, 3.0f, 0.54f },
    { "Classic / Last Floor Tom", 2, 48, 170, 130, 4, 0.06f, 60, 0.220f, Stick, 1250, 0.045f, 1, 0.0f, 72.0f, 0.72f, 1.2f, 1.0f, -2.0f, 0.20f, 0.48f, 0.22f, 26, 500, 7000, 0, -0.10f, -4.5f, 0.58f },
    { "Classic / Woodblock Rim", 3, 67, 85, 1100, 1, 0.06f, 100, 0.420f, Mallet, 1300, 0.000f, 1, 0.5f, 180.0f, 0.48f, 1.2f, -1.0f, -2.0f, 0.36f, 0.40f, 0.38f, 6, 280, 4500, 2, -0.12f, 2.0f, 0.40f },
    { "Classic / Club Rim", 3, 49, 95, 400, 6, 0.18f, 100, 0.420f, Rimshot, 1600, 0.050f, 1, 3.0f, 130.0f, 0.48f, 1.5f, -0.5f, -1.0f, 0.44f, 0.65f, 0.38f, 13, 450, 7000, -1, -0.06f, -2.5f, 0.40f },
    { "Classic / Studio Stick Rim", 3, 59, 85, 860, 3, 0.12f, 100, 0.420f, Stick, 2500, 0.025f, 1, -1.0f, 180.0f, 0.48f, 1.3f, 0.0f, -1.5f, 0.52f, 0.70f, 0.38f, 9, 700, 10000, -1, 0.00f, 5.0f, 0.40f },
}};

FactoryPreset makePreset(const Patch& p) {
    const int minDecay = p.family == 1 ? 150 : (p.family == 2 ? 115 : 135);
    const int minWire = p.family == 0 ? 105 : (p.family == 3 ? 95 : (p.family == 2 ? 70 : 55));
    const float decay = static_cast<float>(std::max(p.decayMs, minDecay));
    const float wireDecay = static_cast<float>(std::max(p.wireMs, minWire));
    const int air = p.air >= 0 ? p.air : kDefaultAir[static_cast<size_t>(p.family)];
    const float bodyLpf = p.family == 1 ? 15000.0f : (p.highDb < -1.0f ? 8500.0f : 12000.0f);
    return { p.name, {
        { kVoiceMode, static_cast<float>(kVoices[static_cast<size_t>(p.family)]) },
        { kToneNote, static_cast<float>(p.midi % 12) },
        { kToneOctave, static_cast<float>(p.midi / 12 - 1) },
        { kToneFine, 0 },
        { kToneFreq, static_cast<float>(p.shellHz) },
        { kToneQ, p.q },
        { kToneDecay, decay },
        { kTonePitchEnv, static_cast<float>(p.sweep) },
        { kTonePitchDecay, static_cast<float>(p.pitchMs) },
        { kFmAmount, p.fm },
        { kShellSpread, p.spread },
        { kToneLevel, p.body },
        { kBodyHPF, p.hpf },
        { kBodyLPF, bodyLpf },
        { kBodyPan, 0 },
        { kAmpEnabled, 0 },
        { kAmpAttack, 0.2f },
        { kAmpDecay, std::clamp(decay * 0.9f, 20.0f, 600.0f) },
        { kAmpSustain, p.sustain },
        { kAmpRelease, std::clamp(decay * 0.42f, 12.0f, 130.0f) },
        { kPreBursts, static_cast<float>(p.strikes) },
        { kPreSpread, p.strikes > 2 ? 4.0f : 5.5f },
        { kPreDecay, 3.5f },
        { kPreJitter, 0.06f },
        { kFlamSpread, 0.35f },
        { kPreLevel, 0.42f },
        { kNoiseType, static_cast<float>(air) },
        { kNoiseAttack, 0.15f },
        { kNoiseHold, 0.4f },
        { kNoiseDecay, wireDecay },
        { kNoiseCurve, -0.55f },
        { kNoiseHPF, static_cast<float>(p.wireHp) },
        { kNoiseLPF, static_cast<float>(p.wireLp) },
        { kNoiseStereoWidth, p.width },
        { kNoiseLevel, p.wire },
        { kWireTension, p.tension },
        { kWirePan, 0 },
        { kClickType, static_cast<float>(p.click) },
        { kClickFreq, static_cast<float>(p.clickHz) },
        { kClickLevel, p.clickLvl },
        { kSubThump, 0 },
        { kTransientHPF, p.family == 3 ? 160.0f : 70.0f },
        { kTransientLPF, 14000 },
        { kTransientPan, 0 },
        { kDistType, 0 },
        { kDistDrive, 0.03f },
        { kDistTone, 0 },
        { kDistMix, 0.06f },
        { kCompThresh, -20 },
        { kCompRatio, 1.5f },
        { kCompAttack, 2.0f },
        { kCompRelease, 110 },
        { kCompMakeup, 0 },
        { kCompMix, 0.16f },
        { kLimiterEnabled, 1 },
        { kLimiterCeiling, -1 },
        { kLimiterRelease, 35 },
        { kReverbSize, 0.30f },
        { kReverbDecay, 150 },
        { kReverbDamp, 0.64f },
        { kReverbGate, 70 },
        { kReverbMix, p.room },
        { kEchoMix, 0 },
        { kReverseMode, 0 },
        { kReverseMix, 0 },
        { kLockPitch, 0 },
        { kLockIn, 0 },
        { kEqLowFreq, 90 },
        { kEqLowDb, p.lowDb },
        { kEqMidFreq, std::clamp(static_cast<float>(p.shellHz) * (p.family == 1 ? 0.85f : 2.2f), 280.0f, 5200.0f) },
        { kEqMidDb, p.midDb },
        { kEqHighFreq, 6800 },
        { kEqHighDb, p.highDb },
        { kStereoWidth, 1 },
        { kOutputGain, p.gain },
    } };
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const auto programs = [] {
        std::vector<FactoryPreset> result;
        result.reserve(bank.size());
        std::ranges::transform(bank, std::back_inserter(result),
                               [](const Patch& patch) { return makePreset(patch); });
        return result;
    }();
    return programs;
}

int firstPresetForFamily(size_t family) noexcept {
    const auto it = std::ranges::find_if(bank, [family](const Patch& patch) {
        return patch.family == static_cast<int>(family);
    });
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
        || id == kCompAttack || id == kCompRelease || id == kCompMakeup || id == kCompMix)
        return Section::SaturationDynamics;
    if (id == kReverbSize || id == kReverbDecay || id == kReverbDamp || id == kReverbGate || id == kReverbMix || id == kEchoMix
        || id == kReverseMode || id == kReverseMix || id == kLockIn)
        return Section::Space;
    if (id == kEqLowFreq || id == kEqLowDb || id == kEqMidFreq || id == kEqMidDb || id == kEqHighFreq || id == kEqHighDb
        || id == kStereoWidth || id == kOutputGain || id == kLimiterCeiling || id == kLimiterRelease || id == kLimiterEnabled)
        return Section::Output;
    if (id == kAmpEnabled || id == kAmpAttack || id == kAmpDecay || id == kAmpSustain || id == kAmpRelease)
        return Section::Envelope;
    if (id == kFmAmount || id == kToneFreq || id == kToneQ || id == kShellSpread || id == kTonePitchDecay || id == kToneFine
        || id == kLockPitch || id == kBodyHPF || id == kBodyLPF || id == kBodyPan)
        return Section::BodyDetail;
    return Section::VoiceTuning;
}
} // namespace

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state,
           const SectionLocks& locks, bool preservePitch,
           const std::function<bool(const juce::String&)>& isParamLocked) {
    for (const auto& [paramId, value] : preset.values) {
        const auto section = sectionFor(paramId);
        if (locks[static_cast<size_t>(section)])
            continue;
        if (isParamLocked && isParamLocked(paramId))
            continue;
        if (preservePitch && (paramId == kToneFreq || paramId == kToneNote || paramId == kToneOctave || paramId == kToneFine || paramId == kLockPitch))
            continue;
        if (auto* param = state.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }
}

} // namespace opensmacker::presets
