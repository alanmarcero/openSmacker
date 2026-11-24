#pragma once

#include "SmackEngine.h"
#include "../suite/EqNotes.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>

namespace opensmacker::params {

// Pre-Strike Flam / Drag Roll
inline constexpr const char* kPreBursts = "pre_bursts";
inline constexpr const char* kPreSpread = "pre_spread";
inline constexpr const char* kPreDecay = "pre_decay";
inline constexpr const char* kPreJitter = "pre_jitter";
inline constexpr const char* kFlamSpread = "flam_spread";
inline constexpr const char* kPreLevel = "pre_level";

// Tunable Drum / Bell / Tom Body
inline constexpr const char* kVoiceMode = "voice_mode";
inline constexpr const char* kToneNote = "tone_note";
inline constexpr const char* kToneOctave = "tone_octave";
inline constexpr const char* kToneFine = "tone_fine";
inline constexpr const char* kToneFreq = "tone_freq";
inline constexpr const char* kToneQ = "tone_q";
inline constexpr const char* kToneDecay = "tone_decay";
inline constexpr const char* kTonePitchEnv = "tone_pitch_env";
inline constexpr const char* kTonePitchDecay = "tone_pitch_decay";
inline constexpr const char* kFmAmount = "fm_amount";
inline constexpr const char* kShellSpread = "shell_spread";
inline constexpr const char* kToneLevel = "tone_level";
inline constexpr const char* kBodyHPF = "body_hpf";
inline constexpr const char* kBodyLPF = "body_lpf";
inline constexpr const char* kBodyPan = "body_pan";
inline constexpr const char* kAmpAttack = "amp_attack";
inline constexpr const char* kAmpEnabled = "amp_enabled";
inline constexpr const char* kAmpDecay = "amp_decay";
inline constexpr const char* kAmpSustain = "amp_sustain";
inline constexpr const char* kAmpRelease = "amp_release";

// Snare Wires, Sizzle & Air
inline constexpr const char* kNoiseType = "noise_type";
inline constexpr const char* kNoiseAttack = "noise_attack";
inline constexpr const char* kNoiseHold = "noise_hold";
inline constexpr const char* kNoiseDecay = "noise_decay";
inline constexpr const char* kNoiseCurve = "noise_curve";
inline constexpr const char* kNoiseHPF = "noise_hpf";
inline constexpr const char* kNoiseLPF = "noise_lpf";
inline constexpr const char* kNoiseStereoWidth = "noise_stereo_width";
inline constexpr const char* kNoiseLevel = "noise_level";
inline constexpr const char* kWireTension = "wire_tension";
inline constexpr const char* kWirePan = "wire_pan";

// Stick / Mallet / Rim Transient
inline constexpr const char* kClickType = "click_type";
inline constexpr const char* kClickLevel = "click_level";
inline constexpr const char* kClickFreq = "click_freq";
inline constexpr const char* kSubThump = "sub_thump";
inline constexpr const char* kTransientHPF = "transient_hpf";
inline constexpr const char* kTransientLPF = "transient_lpf";
inline constexpr const char* kTransientPan = "transient_pan";

// Impact Distortion
inline constexpr const char* kDistType = "dist_type";
inline constexpr const char* kDistDrive = "dist_drive";
inline constexpr const char* kDistTone = "dist_tone";
inline constexpr const char* kDistMix = "dist_mix";

// Dynamics: Compressor
inline constexpr const char* kCompThresh = "comp_thresh";
inline constexpr const char* kCompRatio = "comp_ratio";
inline constexpr const char* kCompAttack = "comp_attack";
inline constexpr const char* kCompRelease = "comp_release";
inline constexpr const char* kCompMakeup = "comp_makeup";
inline constexpr const char* kCompMix = "comp_mix";

// Dynamics: Limiter
inline constexpr const char* kLimiterCeiling = "limiter_ceiling";
inline constexpr const char* kLimiterRelease = "limiter_release";
inline constexpr const char* kLimiterEnabled = "limiter_enabled";

// Trance Gated Reverb
inline constexpr const char* kReverbSize = "reverb_size";
inline constexpr const char* kReverbDecay = "reverb_decay";
inline constexpr const char* kReverbDamp = "reverb_damp";
inline constexpr const char* kReverbGate = "reverb_gate";
inline constexpr const char* kReverbMix = "reverb_mix";
inline constexpr const char* kEchoMix = "echo_mix";

// Reverse & Sync
inline constexpr const char* kReverseMode = "reverse_mode";
inline constexpr const char* kReverseMix = "reverse_mix";

// Global / Master
inline constexpr const char* kMidiMode = "midi_mode";
inline constexpr const char* kLockPitch = "lock_pitch";
inline constexpr const char* kLockIn = "lock_in";
inline constexpr const char* kStereoWidth = "stereo_width";
inline constexpr const char* kOutputGain = "output_gain";
inline constexpr const char* kEqLowFreq = "eq_low_freq";
inline constexpr const char* kEqLowDb = "eq_low_db";
inline constexpr const char* kEqMidFreq = "eq_mid_freq";
inline constexpr const char* kEqMidDb = "eq_mid_db";
inline constexpr const char* kEqHighFreq = "eq_high_freq";
inline constexpr const char* kEqHighDb = "eq_high_db";
inline constexpr int kEqLowMinMidi = suite::eq::kLowMinMidi;
inline constexpr const char* kKeyTracking = "key_tracking";
inline constexpr int kEqLowMaxMidi = suite::eq::kLowMaxMidi;
inline constexpr int kEqMidMinMidi = suite::eq::kMidMinMidi;
inline constexpr int kEqMidMaxMidi = suite::eq::kMidMaxMidi;
inline constexpr int kEqHighMinMidi = suite::eq::kHighMinMidi;
inline constexpr int kEqHighMaxMidi = suite::eq::kHighMaxMidi;

[[nodiscard]] std::pair<int, int> eqBandMidiLimits(const juce::String& freqParamId) noexcept;

float eqNoteToHz(int midiNote) noexcept;
int eqHzToNearestMidi(float hz, int minMidi, int maxMidi) noexcept;
juce::String formatEqNote(float hz, int minMidi, int maxMidi);
float parseEqNoteOrHz(const juce::String& text, int minMidi, int maxMidi);
inline constexpr const char* kPreset = "preset";

// MIDI key zones and layer performance
inline constexpr const char* kKeyZonesEnabled = "key_zones_enabled";
inline constexpr const char* kSnareZoneLow = "snare_zone_low";
inline constexpr const char* kSnareZoneHigh = "snare_zone_high";
inline constexpr const char* kTomZoneLow = "tom_zone_low";
inline constexpr const char* kTomZoneHigh = "tom_zone_high";
inline constexpr const char* kBellZoneLow = "bell_zone_low";
inline constexpr const char* kBellZoneHigh = "bell_zone_high";
inline constexpr const char* kRimZoneLow = "rim_zone_low";
inline constexpr const char* kRimZoneHigh = "rim_zone_high";
inline constexpr const char* kBodyVelocityDepth = "body_velocity_depth";
inline constexpr const char* kWireVelocityDepth = "wire_velocity_depth";
inline constexpr const char* kTransientVelocityDepth = "transient_velocity_depth";
inline constexpr const char* kBodyAftertouchDepth = "body_aftertouch_depth";
inline constexpr const char* kWireAftertouchDepth = "wire_aftertouch_depth";
inline constexpr const char* kTransientAftertouchDepth = "transient_aftertouch_depth";
inline constexpr const char* kRollEnabled = "roll_enabled";
inline constexpr const char* kRollSubdivision = "roll_subdivision";
inline constexpr const char* kRollProbability = "roll_probability";
inline constexpr const char* kRollVelocity = "roll_velocity";
inline constexpr const char* kRollAccentVelocity = "roll_accent_velocity";
inline constexpr const char* kRollAccentEvery = "roll_accent_every";
inline constexpr const char* kRollNote = "roll_note";
inline constexpr const char* kMidiCcBody = "midi_cc_body";
inline constexpr const char* kMidiCcWire = "midi_cc_wire";
inline constexpr const char* kMidiCcTransient = "midi_cc_transient";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
opensmacker::core::EngineSettings readSettings(const juce::AudioProcessorValueTreeState& apvts);

} // namespace opensmacker::params
