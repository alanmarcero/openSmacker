# Changelog

## v2.8.4

- Tracks MIDI pitch and updates the shared suite UI, CI, and tests.
- Aligns the JUCE 8 license and hardens CI.

## v2.8.3

- **Note-Snapped EQ:** Snap all three EQ bands to chromatic notes (A-G with sharps) across two-octave ranges (`E1` to `E3`, `B4` to `B6`, `A#7` to `A#9`), matching OpenKicker's EQ design and behavior.

## v2.8.2

- **Parameter Locking:** Add per-parameter locking across every plugin parameter, persisted in session state and preserved across preset loads and randomization.

## v2.8.1

- **UI & Suite Fixes:** UI polish, Learn for key zones and roll note, unit formatting and typed-value entry, per-slot A/B undo, inline preset confirms, 40x40 preset steppers, and screen-reader titles.

## v2.8.0

Upgrades the core synth, reverb, and distortion DSP:

- **4x oversampled ADAA wavefolder distortion:** `DistortionShaper` suppresses foldback aliasing across distortion curves.
- **Pre-sum distortion routing:** `SmackEngine` shapes layers before summing to avoid body-to-wire intermodulation crushing.
- **Oversampled FM bell:** Preserves high-octave FM depth without sub-fundamental aliasing.
- **Physical snare-wire comb network:** Couples nonlinearly to bottom-head modal amplitude.
- **Struck 2-pole shell modal bank:** Models circular-membrane stiffness dispersion.
- **ZDF TPT SVF shell bandpass & ADAA saturation:** Suppresses high-drive aliasing in the shell path.
- **12 dB ZDF SVF layer filters:** Steeply reject out-of-band snare sub-bleed.
- **Two-stage pitch trajectory:** Combines fast stick-head snap with slower body settle.
- **ClubTom velvet membrane coupling:** Velocity-scaled excitation without high-frequency hash buzz, plus an oversampled rimshot transient exciting three wood and hoop modes (1.8, 4.2, and 8.7 kHz).
- **8-line Householder FDN reverb:** Shapes LF and HF RT60 independently with a dense tail.
