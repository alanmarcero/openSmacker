# openSmacker v2 Design

**Date:** 2026-11-12
**Status:** Spec based on the current v1.1 feature set and the user's request to
rebuild openKicker in the same way.

## Intent

Rebuild openSmacker as a complete dance/trance percussion synthesizer. Replace
its sound engine, effects, state integration, presets, and editor while
preserving every current feature and host contract. Create a completely new
72-program bank, retain the 24-snares / 24-bells / 24-toms organization, add
per-section preset locks, and redraw the UI so every label and control fits.

## Current feature inventory

openSmacker is a JUCE 8 / C++20 instrument that ships as VST3, AU, LV2, and
Standalone on macOS, Windows, and Linux. V2 retains:

- Four instrument voices: Dance Snare, Club Tom-Tom, FM Bell/Chime, and
  Pryda/Rim Smack; chromatic root tuning, octaves 1–8, fine cents, resonant
  body, pitch envelope, decay, FM/ring amount, and level.
- Fixed-root or MIDI note-tracking pitch, velocity response, pitch lock,
  Push It audition, and session WAV lock-in.
- One-to-five pre-strikes with spread, decay, jitter, stereo pan and level;
  Stick, Mallet, Rimshot, and Laser transients with frequency and level.
- Snare-wire, sizzle, and air textures (Snare Wires, White Sizzle, Velvet Air,
  Metallic Ring), attack/hold/decay shaping, curve, HPF/LPF, level, and width.
- Tape, Tube, Hard Clip, and Wavefold saturation; compressor; brickwall
  limiter; gated trance-hall reverb; host-synced reverse swell; master width,
  output gain, peak display, and animated smack visualizer stage.
- Preset selector and previous/next controls, SN/BL/TM category navigation,
  Simple and Advanced modes, saved state and cached WAV data, and existing
  parameter IDs/program behavior.
- MIDI note-on triggers on all channels, velocity response, Fixed/Note Tracking
  mode, and the documented CC meanings: CC 7 output, CC 74 air LPF,
  CC 120/123 silence, and CC 121 resets CC 7/74.
- Exactly 72 factory programs: 24 snares, 24 bells/chimes, and 24 tom-toms.
  Every current preset name and definition is retired.

## Goals

- Reimplement the full synthesis/effects/engine/processor/editor stack without
  removing any current voice, articulation, texture, tuning, effect, MIDI,
  session, preset-navigation, or stage behavior.
- Replace all 72 presets with unique, original trance-focused programs: 24
  snares/rim smacks, 24 bells/chimes, and 24 toms. Every preset specifies all
  presettable parameters so the sound is independent of the previous program.
- Add a one-shot ADSR amplitude envelope: Attack 0.05–20 ms (0.35 ms default),
  Decay 1–600 ms (72 ms), Sustain 0–1 (0.70), and Release 5–800 ms (180 ms).
  The envelope shapes the dry hit within its tone/noise tail duration; existing
  tone and noise decays continue to shape their individual layers.
- Add a lock button to every editable preset section. Program changes leave
  locked section values unchanged; manual parameter edits remain active.
- Redraw the editor as **Hit**, **Layers**, and **FX** pages with persistent
  preset/category navigation, Simple/Advanced views, and the animated smack
  stage. All existing controls remain reachable in both modes.
- Publish as v2.0.0 with a complete README, audio notes, and native screenshots
  for the redesigned editor.

## Non-goals

- Do not change plugin name, bundle ID (`com.alanmarcero.opensmacker`),
  manufacturer/plugin codes, APVTS root, existing parameter IDs/versions,
  formats, JUCE version, or MIDI-controller meanings.
- Do not remove a voice mode, a preset category, a transient, an articulation,
  a noise type, tuning, effect, preset-navigation shortcut, editor mode,
  audition control, or animation.
- Do not retain any v1 preset name, parameter set, or ordering as active
  content. Do not add sample or third-party dependencies.

## Approaches

1. Tune the current synthesis paths and keep the current editor. This is the
   smallest change but does not deliver a complete reimplementation.
2. Preserve old and new synthesis/preset paths as selectable alternatives.
   This increases maintenance and keeps the previous preset catalog active.
3. Replace the engine, effects, state integration, preset bank, and UI while
   retaining product identity and every existing feature contract. This is the
   selected approach because the user requested the same complete redo as
   openKicker.

## Sound and preset behavior

The four instrument voices remain distinct: modal membrane/snare, tuned
pitched tom, inharmonic FM bell/chime, and rim-smack/snare. Rebuild each voice
and its transient/noise coupling so the musical body and impact stay audible
throughout the hit instead of collapsing to a short sine-like tone. Rebuild
stereo generation, pre-strikes, noise, and effects as part of the new engine.

The ADSR shapes the complete dry voice before the effect chain. Tail duration
continues to be set by the existing voice and noise decay settings; release is
bounded to the hit duration and reaches silence at the end of the one-shot.
Effects and reverse swells retain their existing host synchronization and
ordering contract. WAV lock-in still captures/replays the finished hit and
converts for a changed host sample rate.

The new factory catalog has exactly 72 unique names in the existing category
order: SN 01–24, BL 01–24, and TM 01–24. Every program directly declares its
complete calibrated parameter definition in a transparent `PresetDef`; index
arithmetic does not derive or perturb sound values. The bank spans progressive, uplifting
138–140, classic Dutch, tech-trance, psy-trance, melodic/breakdown, and clean
club styles. None of the previous 72 names or definitions remain. Legacy
parameter state and WAV cache data restore; old program indices resolve to the
new bank after restore.

## Section locks

Locks are saved UI/session state, not automation parameters. Locked values are
protected from factory program loads but remain manually editable. Existing
`lock_pitch` and `lock_in` stay as separate pitch and WAV features.

| Section | Preset-owned settings |
| --- | --- |
| Voice & Tuning | Voice mode, note, octave, fine, frequency, Q, tone decay, pitch envelope/decay, FM/ring, body level, pitch lock, ADSR |
| Pre-Strike | Strike count, spread, decay, jitter, stereo pan, and level |
| Wires & Air | Noise type, attack, hold, decay, curve, HPF, LPF, width, and level |
| Transient | Click type, level, and frequency |
| Saturation & Dynamics | Distortion type/drive/tone/mix, compressor, limiter ceiling/release/enabled |
| Space & Output | Reverb, gate, reverse mode/mix, MIDI mode, WAV lock-in, master width, output gain |

## Editor design

Use openKicker v2's compact ink/steel signal-console language: navy background,
cyan signal, amber body/tail, coral transient, plain cross-platform sans text,
and aligned numerical readouts. Set the default editor to 1100 × 972.
Retain the Smacker stage artwork and animation on the Hit page and keep
preset/category controls in a persistent top bar.

- **Hit:** SN/BL/TM shortcuts, current preset selector, tuning, voice/body,
  ADSR, transient, waveform/peak display, and stage artwork.
- **Layers:** pre-strike controls and snare-wire/sizzle/air controls.
- **FX:** saturation, compression/limiting, gated reverb, reverse sync, stereo
  width, output, and peak meter.
- Simple mode exposes primary parameters; Advanced exposes every detailed
  control without hiding a feature. Each section has its own visible lock state
  and tooltip. Every page is laid out to prevent text collisions or truncation.

## Constraints and release

- Use JUCE 8 and C++20; add no dependencies.
- Preserve current APVTS parameter IDs and versions. Add versioned ADSR
  parameters and section-lock state; missing lock data in legacy sessions
  defaults to unlocked.
- Preserve all existing CMake targets and the macOS, Windows, and Linux build
  workflow.
- Set CMake/project version and release tag to v2.0.0. Push `main` and the tag
  to `github.com/alanmarcero/openSmacker` using the required personal Git
  identity.

## Success checks

- All current controls, MIDI mappings, formats, saved parameter values, and
  cached WAV data remain usable.
- Exactly 72 unique new presets remain, 24 for each category. All render
  finite, non-silent audio; no old factory name/definition remains.
- Locked sections survive preset changes and state save/restore; unlocked
  sections load the new program values.
- All pages and both view modes render at 1100 × 972 without overlapping or
  clipped text; repository screenshots show the built UI.
- All plugin formats build, CI tests pass, and the v2.0.0 GitHub Action/release
  completes.
