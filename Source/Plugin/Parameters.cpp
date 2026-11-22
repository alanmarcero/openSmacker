#include "Parameters.h"
#include <cmath>
#include <vector>

namespace opensmacker::params {

namespace {
// Rounding first stops a tiny negative value from reading "-0.0".
juce::String signedTenths(float value) {
    const float tenths = std::round(value * 10.0f);
    if (tenths == 0.0f)
        return "0.0";
    return (tenths > 0.0f ? "+" : "") + juce::String(tenths / 10.0f, 1);
}
} // namespace

float eqNoteToHz(int midiNote) noexcept {
    const int clamped = std::clamp(midiNote, 0, 144);
    return 440.0f * std::exp2((static_cast<float>(clamped) - 69.0f) / 12.0f);
}

int eqHzToNearestMidi(float hz, int minMidi, int maxMidi) noexcept {
    if (!std::isfinite(hz) || hz <= 0.0f) return minMidi;
    const int midi = juce::roundToInt(69.0f + 12.0f * std::log2(hz / 440.0f));
    return std::clamp(midi, minMidi, maxMidi);
}

juce::String formatEqNote(float hz, int minMidi, int maxMidi) {
    static constexpr std::array<const char*, 12> kNames{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int midi = eqHzToNearestMidi(hz, minMidi, maxMidi);
    const int octave = midi / 12 - 1;
    return juce::String(kNames[static_cast<size_t>(midi % 12)]) + juce::String(octave);
}

float parseEqNoteOrHz(const juce::String& text, int minMidi, int maxMidi) {
    const auto trimmed = text.trim();
    if (trimmed.isEmpty()) return eqNoteToHz(minMidi);
    const int semitone = juce::String("C D EF G A B").indexOfIgnoreCase(trimmed.substring(0, 1));
    if (semitone >= 0) {
        const auto afterLetter = trimmed.substring(1);
        const int sharp = afterLetter.startsWithChar('#') ? 1 : 0;
        const int flat = afterLetter.startsWithChar('b') ? 1 : 0;
        const auto octaveText = afterLetter.substring(sharp + flat).trim();
        if (octaveText.isNotEmpty() && octaveText.containsOnly("-0123456789")) {
            const int midi = (octaveText.getIntValue() + 1) * 12 + semitone + sharp - flat;
            return eqNoteToHz(std::clamp(midi, minMidi, maxMidi));
        }
    }
    const float number = trimmed.getFloatValue();
    const float hz = trimmed.containsIgnoreCase("k") ? number * 1000.0f : number;
    return eqNoteToHz(eqHzToNearestMidi(hz, minMidi, maxMidi));
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    using Attributes = juce::AudioParameterFloatAttributes;
    const auto fromPercent = [](const juce::String& text) { return text.getFloatValue() / 100.0f; };
    const auto percent = Attributes()
        .withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; })
        .withValueFromStringFunction(fromPercent);
    const auto signedPercent = Attributes()
        .withStringFromValueFunction([](float value, int) {
            const int amount = juce::roundToInt(value * 100.0f);
            return (amount > 0 ? "+" : "") + juce::String(amount) + "%";
        })
        .withValueFromStringFunction(fromPercent);
    const auto decibels = Attributes().withStringFromValueFunction([](float value, int) { return signedTenths(value) + " dB"; });
    const auto semitones = Attributes().withStringFromValueFunction([](float value, int) { return signedTenths(value) + " st"; });
    const auto cents = Attributes().withStringFromValueFunction([](float value, int) { return signedTenths(value) + " ct"; });
    const auto ratio = Attributes().withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + ":1"; });
    const auto hertz = Attributes()
        .withStringFromValueFunction([](float hz, int) {
            if (hz < 999.5f)
                return juce::String(juce::roundToInt(hz)) + " Hz";
            const float khz = hz / 1000.0f;
            return juce::String(khz, khz < 9.995f ? 2 : 1) + " kHz";
        })
        .withValueFromStringFunction([](const juce::String& text) {
            const float number = text.getFloatValue();
            return text.containsIgnoreCase("k") ? number * 1000.0f : number;
        });
    const auto milliseconds = Attributes()
        .withStringFromValueFunction([](float ms, int) {
            if (ms < 9.95f)
                return juce::String(ms, 1) + " ms";
            if (ms < 999.5f)
                return juce::String(juce::roundToInt(ms)) + " ms";
            return juce::String(ms / 1000.0f, 2) + " s";
        })
        .withValueFromStringFunction([](const juce::String& text) {
            const auto unit = text.trim().toLowerCase();
            const float number = unit.getFloatValue();
            const bool seconds = unit.endsWith("s") && !unit.endsWith("ms");
            return seconds ? number * 1000.0f : number;
        });
    const auto pan = Attributes()
        .withStringFromValueFunction([](float value, int) {
            const int amount = juce::roundToInt(value * 100.0f);
            if (amount == 0)
                return juce::String("C");
            return juce::String(std::abs(amount)) + (amount < 0 ? " L" : " R");
        })
        .withValueFromStringFunction([](const juce::String& text) {
            const auto side = text.trim().toUpperCase();
            const float amount = side.getFloatValue() / 100.0f;
            if (side.endsWith("L"))
                return -std::abs(amount);
            if (side.endsWith("R"))
                return std::abs(amount);
            return amount;
        });
    const auto noteName = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([](int note, int) { return juce::MidiMessage::getMidiNoteName(note, true, true, 4); })
        .withValueFromStringFunction([](const juce::String& text) {
            const auto name = text.trim().toUpperCase();
            const int letter = juce::String("C D EF G A B").indexOfChar(name[0]);
            if (letter < 0)
                return name.getIntValue();
            const bool sharp = name[1] == '#';
            const bool flat = name[1] == 'B';
            const int octave = name.substring(sharp || flat ? 2 : 1).getIntValue();
            return 12 * (octave + 1) + letter + (sharp ? 1 : 0) - (flat ? 1 : 0);
        });
    const auto controller = juce::AudioParameterIntAttributes().withStringFromValueFunction([](int number, int) {
        if (number == 0)
            return juce::String("Off");
        return juce::String(number);
    });

    // Pre-Strike Flam / Drag Roll
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kPreBursts, 1 }, "Flam hits", 1, 5, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kPreSpread, 1 }, "Flam spacing", juce::NormalisableRange<float>(2.0f, 45.0f, 0.1f), 11.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kPreDecay, 1 }, "Flam decay", juce::NormalisableRange<float>(2.0f, 35.0f, 0.1f), 8.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kPreJitter, 1 }, "Flam humanize", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.12f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kFlamSpread, 1 }, "Flam stereo spread", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kPreLevel, 1 }, "Flam level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f, percent));

    // Tunable Drum / Bell / Tom Body
    juce::StringArray voiceModes{ "Snare", "Tom", "Bell", "Rim" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kVoiceMode, 1 }, "Voice", voiceModes, 0));
    juce::StringArray noteNames{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kToneNote, 1 }, "Root note", noteNames, 9)); // A
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kToneOctave, 1 }, "Octave", 1, 8, 3));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kToneFine, 1 }, "Fine tune", juce::NormalisableRange<float>(-50.0f, 50.0f, 0.5f), 0.0f, cents));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kToneFreq, 1 }, "Shell frequency", juce::NormalisableRange<float>(60.0f, 6000.0f, 1.0f, 0.35f), 220.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kToneQ, 1 }, "Shell Q", juce::NormalisableRange<float>(0.5f, 15.0f, 0.1f), 3.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kToneDecay, 1 }, "Body decay", juce::NormalisableRange<float>(15.0f, 1200.0f, 1.0f, 0.35f), 180.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTonePitchEnv, 1 }, "Pitch sweep", juce::NormalisableRange<float>(-24.0f, 36.0f, 0.5f), 12.0f, semitones));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTonePitchDecay, 1 }, "Pitch decay", juce::NormalisableRange<float>(3.0f, 150.0f, 0.5f), 22.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kFmAmount, 1 }, "FM depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.30f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kShellSpread, 1 }, "Mode spread", juce::NormalisableRange<float>(-0.5f, 0.5f, 0.01f), 0.0f, signedPercent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kToneLevel, 1 }, "Body level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kBodyHPF, 1 }, "Body high-pass", juce::NormalisableRange<float>(20.0f, 4000.0f, 1.0f, 0.35f), 20.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kBodyLPF, 1 }, "Body low-pass", juce::NormalisableRange<float>(100.0f, 20000.0f, 1.0f, 0.35f), 20000.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kBodyPan, 1 }, "Body pan", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f, pan));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ kAmpEnabled, 1 }, "MIDI gate", false,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction([](bool hold, int) { return juce::String(hold ? "Hold notes" : "One-shot"); })
            .withValueFromStringFunction([](const juce::String& text) { return text.containsIgnoreCase("hold"); })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kAmpAttack, 1 }, "Amp attack", juce::NormalisableRange<float>(0.05f, 20.0f, 0.05f, 0.45f), 0.35f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kAmpDecay, 1 }, "Amp decay", juce::NormalisableRange<float>(1.0f, 600.0f, 1.0f, 0.4f), 72.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kAmpSustain, 1 }, "Amp sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.70f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kAmpRelease, 1 }, "Amp release", juce::NormalisableRange<float>(0.0f, 20000.0f, 1.0f, 0.4f), 180.0f, milliseconds));

    // Snare Wires, Sizzle & Air
    juce::StringArray noiseTypes{ "Snare wires", "White sizzle", "Velvet air", "Metallic ring" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kNoiseType, 1 }, "Wire texture", noiseTypes, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseAttack, 1 }, "Wire attack", juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 0.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseHold, 1 }, "Wire hold", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 2.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseDecay, 1 }, "Wire decay", juce::NormalisableRange<float>(15.0f, 1200.0f, 1.0f, 0.35f), 210.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseCurve, 1 }, "Wire curve", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), -0.35f, signedPercent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseHPF, 1 }, "Wire high-pass", juce::NormalisableRange<float>(20.0f, 2500.0f, 1.0f, 0.35f), 280.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseLPF, 1 }, "Wire low-pass", juce::NormalisableRange<float>(800.0f, 20000.0f, 1.0f, 0.35f), 11000.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseStereoWidth, 1 }, "Wire width", juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.15f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kNoiseLevel, 1 }, "Wire level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.80f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kWireTension, 1 }, "Wire tension", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kWirePan, 1 }, "Wire pan", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f, pan));

    // Stick / Mallet / Rim Transient
    juce::StringArray clickTypes{ "Stick", "Mallet", "Rimshot", "Laser" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kClickType, 1 }, "Strike type", clickTypes, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kClickLevel, 1 }, "Strike level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kClickFreq, 1 }, "Strike frequency", juce::NormalisableRange<float>(500.0f, 12000.0f, 10.0f, 0.35f), 3800.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kSubThump, 1 }, "Sub thump", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTransientHPF, 1 }, "Strike high-pass", juce::NormalisableRange<float>(20.0f, 12000.0f, 1.0f, 0.35f), 20.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTransientLPF, 1 }, "Strike low-pass", juce::NormalisableRange<float>(100.0f, 20000.0f, 1.0f, 0.35f), 20000.0f, hertz));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTransientPan, 1 }, "Strike pan", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f, pan));

    // Impact Distortion
    juce::StringArray distTypes{ "Tape", "Tube", "Hard clip", "Wavefold" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kDistType, 1 }, "Saturation", distTypes, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kDistDrive, 1 }, "Saturation drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.14f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kDistTone, 1 }, "Saturation tone", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f, signedPercent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kDistMix, 1 }, "Saturation mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.22f, percent));

    // Compressor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kCompThresh, 1 }, "Comp threshold", juce::NormalisableRange<float>(-40.0f, 0.0f, 0.5f), -14.0f, decibels));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kCompRatio, 1 }, "Comp ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 4.0f, ratio));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kCompAttack, 1 }, "Comp attack", juce::NormalisableRange<float>(0.1f, 50.0f, 0.1f, 0.35f), 5.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kCompRelease, 1 }, "Comp release", juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f, 0.35f), 80.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kCompMakeup, 1 }, "Comp makeup", juce::NormalisableRange<float>(0.0f, 24.0f, 0.5f), 3.0f, decibels));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kCompMix, 1 }, "Comp mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, percent));

    // Limiter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kLimiterCeiling, 1 }, "Limiter ceiling", juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), -0.3f, decibels));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kLimiterRelease, 1 }, "Limiter release", juce::NormalisableRange<float>(5.0f, 100.0f, 0.5f), 20.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ kLimiterEnabled, 1 }, "Limiter", true));

    // Trance Gated Reverb
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kReverbSize, 1 }, "Room size", juce::NormalisableRange<float>(0.1f, 1.0f, 0.01f), 0.65f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kReverbDecay, 1 }, "Room decay", juce::NormalisableRange<float>(50.0f, 2500.0f, 10.0f, 0.35f), 480.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kReverbDamp, 1 }, "Room damping", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.38f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kReverbGate, 1 }, "Room gate time", juce::NormalisableRange<float>(0.0f, 600.0f, 5.0f), 180.0f, milliseconds));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kReverbMix, 1 }, "Room mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.32f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEchoMix, 1 }, "Echo mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, percent));

    // Reverse & Sync
    juce::StringArray revModes{ "Off", "Beat end (sync)", "Beat 4 auto", "1/2 beat", "1/4 beat" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kReverseMode, 1 }, "Reverse sync", revModes, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kReverseMix, 1 }, "Reverse mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, percent));

    // Global / Master
    juce::StringArray midiModes{ "Fixed (root)", "Note tracking" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kMidiMode, 1 }, "MIDI pitch", midiModes, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ kLockPitch, 1 }, "Lock pitch", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ kLockIn, 1 }, "Freeze hit", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kStereoWidth, 1 }, "Stereo width", juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kOutputGain, 1 }, "Output gain", juce::NormalisableRange<float>(-36.0f, 12.0f, 0.5f), 0.0f, decibels));
    const auto makeNoteRange = [](int minMidi, int maxMidi) {
        const float minHz = eqNoteToHz(minMidi);
        const float maxHz = eqNoteToHz(maxMidi);
        return juce::NormalisableRange<float>(
            minHz,
            maxHz,
            [minMidi, maxMidi](float, float, float proportion) {
                const float clamped = std::clamp(proportion, 0.0f, 1.0f);
                const int midi = juce::roundToInt(static_cast<float>(minMidi)
                    + clamped * static_cast<float>(maxMidi - minMidi));
                return eqNoteToHz(std::clamp(midi, minMidi, maxMidi));
            },
            [minMidi, maxMidi](float, float, float value) {
                const int midi = eqHzToNearestMidi(value, minMidi, maxMidi);
                return static_cast<float>(midi - minMidi) / static_cast<float>(maxMidi - minMidi);
            },
            [minMidi, maxMidi](float, float, float value) {
                return eqNoteToHz(eqHzToNearestMidi(value, minMidi, maxMidi));
            });
    };
    const auto makeNoteText = [](int minMidi, int maxMidi) {
        return Attributes()
            .withStringFromValueFunction([minMidi, maxMidi](float hz, int) { return formatEqNote(hz, minMidi, maxMidi); })
            .withValueFromStringFunction([minMidi, maxMidi](const juce::String& text) { return parseEqNoteOrHz(text, minMidi, maxMidi); });
    };
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEqLowFreq, 1 }, "Low EQ frequency", makeNoteRange(kEqLowMinMidi, kEqLowMaxMidi), eqNoteToHz(40),
        makeNoteText(kEqLowMinMidi, kEqLowMaxMidi)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEqLowDb, 1 }, "Low EQ gain", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, decibels));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEqMidFreq, 1 }, "Mid EQ frequency", makeNoteRange(kEqMidMinMidi, kEqMidMaxMidi), eqNoteToHz(83),
        makeNoteText(kEqMidMinMidi, kEqMidMaxMidi)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEqMidDb, 1 }, "Mid EQ gain", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, decibels));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEqHighFreq, 1 }, "High EQ frequency", makeNoteRange(kEqHighMinMidi, kEqHighMaxMidi), eqNoteToHz(118),
        makeNoteText(kEqHighMinMidi, kEqHighMaxMidi)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kEqHighDb, 1 }, "High EQ gain", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, decibels));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kPreset, 1 }, "Preset Index", 0, 83, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ kKeyZonesEnabled, 1 }, "Key zones", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kSnareZoneLow, 1 }, "Snare zone low", 0, 127, 36, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kSnareZoneHigh, 1 }, "Snare zone high", 0, 127, 47, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kTomZoneLow, 1 }, "Tom zone low", 0, 127, 48, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kTomZoneHigh, 1 }, "Tom zone high", 0, 127, 59, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kBellZoneLow, 1 }, "Bell zone low", 0, 127, 60, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kBellZoneHigh, 1 }, "Bell zone high", 0, 127, 71, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kRimZoneLow, 1 }, "Rim zone low", 0, 127, 72, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kRimZoneHigh, 1 }, "Rim zone high", 0, 127, 84, noteName));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kBodyVelocityDepth, 1 }, "Body velocity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kWireVelocityDepth, 1 }, "Wire velocity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTransientVelocityDepth, 1 }, "Strike velocity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kBodyAftertouchDepth, 1 }, "Body aftertouch", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kWireAftertouchDepth, 1 }, "Wire aftertouch", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kTransientAftertouchDepth, 1 }, "Strike aftertouch", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ kRollEnabled, 1 }, "Roll", false));
    juce::StringArray rollRates{ "1/8", "1/16", "1/32" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ kRollSubdivision, 1 }, "Roll rate", rollRates, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kRollProbability, 1 }, "Roll probability", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kRollVelocity, 1 }, "Roll velocity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f, percent));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kRollAccentVelocity, 1 }, "Roll accent", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f, percent));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kRollAccentEvery, 1 }, "Roll accent every", 1, 16, 4));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kRollNote, 1 }, "Roll note", 0, 127, 69, noteName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kMidiCcBody, 1 }, "Body CC", 0, 127, 20, controller));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kMidiCcWire, 1 }, "Wire CC", 0, 127, 21, controller));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kMidiCcTransient, 1 }, "Strike CC", 0, 127, 22, controller));

    return { params.begin(), params.end() };
}

opensmacker::core::EngineSettings readSettings(const juce::AudioProcessorValueTreeState& apvts) {
    opensmacker::core::EngineSettings s;

    // Pre-Strike Flam
    s.synth.preStrikeCount = static_cast<int>(*apvts.getRawParameterValue(kPreBursts));
    s.synth.preStrikeSpreadMs = *apvts.getRawParameterValue(kPreSpread);
    s.synth.preStrikeDecayMs = *apvts.getRawParameterValue(kPreDecay);
    s.synth.preStrikeJitter = *apvts.getRawParameterValue(kPreJitter);
    if (const auto* pFlamSpread = apvts.getRawParameterValue(kFlamSpread)) {
        s.synth.preStrikeFlamSpread = *pFlamSpread;
    }
    s.synth.preStrikeLevel = *apvts.getRawParameterValue(kPreLevel);

    // Tunable Body
    if (const auto* pVoiceMode = apvts.getRawParameterValue(kVoiceMode)) {
        s.synth.voiceMode = static_cast<opensmacker::core::VoiceMode>(static_cast<int>(*pVoiceMode));
    }
    s.synth.note = static_cast<int>(*apvts.getRawParameterValue(kToneNote));
    s.synth.octave = static_cast<int>(*apvts.getRawParameterValue(kToneOctave));
    s.synth.fineCents = *apvts.getRawParameterValue(kToneFine);
    s.synth.toneFreqHz = *apvts.getRawParameterValue(kToneFreq);
    s.synth.toneQ = *apvts.getRawParameterValue(kToneQ);
    s.synth.toneDecayMs = *apvts.getRawParameterValue(kToneDecay);
    s.synth.tonePitchEnv = *apvts.getRawParameterValue(kTonePitchEnv);
    s.synth.tonePitchDecayMs = *apvts.getRawParameterValue(kTonePitchDecay);
    if (const auto* pFm = apvts.getRawParameterValue(kFmAmount)) {
        s.synth.fmAmount = *pFm;
    }
    if (const auto* pSpread = apvts.getRawParameterValue(kShellSpread)) {
        s.synth.shellSpread = *pSpread;
    }
    s.synth.toneLevel = *apvts.getRawParameterValue(kToneLevel);
    s.synth.bodyHPF = *apvts.getRawParameterValue(kBodyHPF);
    s.synth.bodyLPF = *apvts.getRawParameterValue(kBodyLPF);
    s.synth.bodyPan = *apvts.getRawParameterValue(kBodyPan);
    s.synth.fullAdsrEnabled = *apvts.getRawParameterValue(kAmpEnabled) > 0.5f;
    s.synth.bodyVelocityDepth = *apvts.getRawParameterValue(kBodyVelocityDepth);
    s.synth.wireVelocityDepth = *apvts.getRawParameterValue(kWireVelocityDepth);
    s.synth.transientVelocityDepth = *apvts.getRawParameterValue(kTransientVelocityDepth);
    s.synth.bodyAftertouchDepth = *apvts.getRawParameterValue(kBodyAftertouchDepth);
    s.synth.wireAftertouchDepth = *apvts.getRawParameterValue(kWireAftertouchDepth);
    s.synth.transientAftertouchDepth = *apvts.getRawParameterValue(kTransientAftertouchDepth);
    s.synth.ampAttackMs = *apvts.getRawParameterValue(kAmpAttack);
    s.synth.ampDecayMs = *apvts.getRawParameterValue(kAmpDecay);
    s.synth.ampSustain = *apvts.getRawParameterValue(kAmpSustain);
    s.synth.ampReleaseMs = *apvts.getRawParameterValue(kAmpRelease);

    // Wires & Sizzle
    s.synth.noiseType = static_cast<opensmacker::core::NoiseType>(static_cast<int>(*apvts.getRawParameterValue(kNoiseType)));
    s.synth.noiseAttackMs = *apvts.getRawParameterValue(kNoiseAttack);
    s.synth.noiseHoldMs = *apvts.getRawParameterValue(kNoiseHold);
    s.synth.noiseDecayMs = *apvts.getRawParameterValue(kNoiseDecay);
    s.synth.noiseCurve = *apvts.getRawParameterValue(kNoiseCurve);
    s.synth.noiseHPF = *apvts.getRawParameterValue(kNoiseHPF);
    s.synth.noiseLPF = *apvts.getRawParameterValue(kNoiseLPF);
    s.synth.noiseStereoWidth = *apvts.getRawParameterValue(kNoiseStereoWidth);
    s.synth.noiseLevel = *apvts.getRawParameterValue(kNoiseLevel);
    s.synth.wireTension = *apvts.getRawParameterValue(kWireTension);
    s.synth.wirePan = *apvts.getRawParameterValue(kWirePan);

    // Click / Strike
    if (const auto* pClickType = apvts.getRawParameterValue(kClickType)) {
        s.synth.clickType = static_cast<opensmacker::core::ClickType>(static_cast<int>(*pClickType));
    }
    s.synth.clickLevel = *apvts.getRawParameterValue(kClickLevel);
    s.synth.clickFreqHz = *apvts.getRawParameterValue(kClickFreq);
    if (const auto* pSubThump = apvts.getRawParameterValue(kSubThump)) {
        s.synth.subThump = *pSubThump;
    }
    s.synth.transientHPF = *apvts.getRawParameterValue(kTransientHPF);
    s.synth.transientLPF = *apvts.getRawParameterValue(kTransientLPF);
    s.synth.transientPan = *apvts.getRawParameterValue(kTransientPan);

    // Reverb
    s.synth.reverbSize = *apvts.getRawParameterValue(kReverbSize);
    s.synth.reverbDecayMs = *apvts.getRawParameterValue(kReverbDecay);
    s.synth.reverbDamp = *apvts.getRawParameterValue(kReverbDamp);
    s.synth.reverbGateMs = *apvts.getRawParameterValue(kReverbGate);
    s.synth.reverbMix = *apvts.getRawParameterValue(kReverbMix);

    // Effects
    s.effects.distType = static_cast<opensmacker::core::DistortionType>(static_cast<int>(*apvts.getRawParameterValue(kDistType)));
    s.effects.distDrive = *apvts.getRawParameterValue(kDistDrive);
    s.effects.distTone = *apvts.getRawParameterValue(kDistTone);
    s.effects.distMix = *apvts.getRawParameterValue(kDistMix);

    s.effects.compressor.thresholdDb = *apvts.getRawParameterValue(kCompThresh);
    s.effects.compressor.ratio = *apvts.getRawParameterValue(kCompRatio);
    s.effects.compressor.attackMs = *apvts.getRawParameterValue(kCompAttack);
    s.effects.compressor.releaseMs = *apvts.getRawParameterValue(kCompRelease);
    s.effects.compressor.makeupDb = *apvts.getRawParameterValue(kCompMakeup);
    s.effects.compressor.mix = *apvts.getRawParameterValue(kCompMix);

    s.effects.limiter.ceilingDb = *apvts.getRawParameterValue(kLimiterCeiling);
    s.effects.limiter.releaseMs = *apvts.getRawParameterValue(kLimiterRelease);
    s.effects.limiter.enabled = *apvts.getRawParameterValue(kLimiterEnabled) > 0.5f;

    s.effects.reverseMode = static_cast<opensmacker::core::ReverseSyncMode>(static_cast<int>(*apvts.getRawParameterValue(kReverseMode)));
    s.effects.reverseMix = *apvts.getRawParameterValue(kReverseMix);
    if (const auto* pEchoMix = apvts.getRawParameterValue(kEchoMix)) {
        s.effects.echoMix = *pEchoMix;
    }

    s.effects.eq.lowFreqHz = *apvts.getRawParameterValue(kEqLowFreq);
    s.effects.eq.lowGainDb = *apvts.getRawParameterValue(kEqLowDb);
    s.effects.eq.midFreqHz = *apvts.getRawParameterValue(kEqMidFreq);
    s.effects.eq.midGainDb = *apvts.getRawParameterValue(kEqMidDb);
    s.effects.eq.highFreqHz = *apvts.getRawParameterValue(kEqHighFreq);
    s.effects.eq.highGainDb = *apvts.getRawParameterValue(kEqHighDb);
    s.effects.stereoWidth = *apvts.getRawParameterValue(kStereoWidth);
    s.effects.outputGainDb = *apvts.getRawParameterValue(kOutputGain);

    s.noteTracking = *apvts.getRawParameterValue(kMidiMode) > 0.5f;
    s.lockIn = *apvts.getRawParameterValue(kLockIn) > 0.5f;

    s.keyZonesEnabled = *apvts.getRawParameterValue(kKeyZonesEnabled) > 0.5f;
    s.voiceZones = {{
        { static_cast<int>(*apvts.getRawParameterValue(kSnareZoneLow)), static_cast<int>(*apvts.getRawParameterValue(kSnareZoneHigh)) },
        { static_cast<int>(*apvts.getRawParameterValue(kTomZoneLow)), static_cast<int>(*apvts.getRawParameterValue(kTomZoneHigh)) },
        { static_cast<int>(*apvts.getRawParameterValue(kBellZoneLow)), static_cast<int>(*apvts.getRawParameterValue(kBellZoneHigh)) },
        { static_cast<int>(*apvts.getRawParameterValue(kRimZoneLow)), static_cast<int>(*apvts.getRawParameterValue(kRimZoneHigh)) }
    }};
    s.rollEnabled = *apvts.getRawParameterValue(kRollEnabled) > 0.5f;
    constexpr std::array<int, 3> rollSubdivisions{ 8, 16, 32 };
    const int rollChoice = static_cast<int>(*apvts.getRawParameterValue(kRollSubdivision));
    s.rollSubdivision = rollSubdivisions[static_cast<size_t>(std::clamp(rollChoice, 0, 2))];
    s.rollProbability = *apvts.getRawParameterValue(kRollProbability);
    s.rollVelocity = *apvts.getRawParameterValue(kRollVelocity);
    s.rollAccentVelocity = *apvts.getRawParameterValue(kRollAccentVelocity);
    s.rollAccentEvery = static_cast<int>(*apvts.getRawParameterValue(kRollAccentEvery));
    s.rollNote = static_cast<int>(*apvts.getRawParameterValue(kRollNote));
    s.midiCcForBody = static_cast<int>(*apvts.getRawParameterValue(kMidiCcBody));
    s.midiCcForWire = static_cast<int>(*apvts.getRawParameterValue(kMidiCcWire));
    s.midiCcForTransient = static_cast<int>(*apvts.getRawParameterValue(kMidiCcTransient));

    return s;
}

} // namespace opensmacker::params
