# openSmacker v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use $subagent-driven-development (recommended) or $executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild openSmacker v2 with new 72 trance presets, section locks, full ADSR, a complete DSP/UI replacement, and no feature loss.

**Architecture:** Keep JUCE, plugin identity, host parameter IDs, MIDI/state contracts, category navigation, and formats. Replace the internal voices, effects, engine, preset bank, state integration, and editor; centralize lock-aware program application and serialize lock state in the session tree.

**Tech Stack:** C++20, JUCE 8, CMake, existing Catch2 suite.

**Spec:** `docs/superpowers/specs/2026-11-12-opensmacker-v2-design.md`

## Global Constraints

- Preserve `com.alanmarcero.opensmacker`, every current parameter ID/version, MIDI CC meanings, plugin formats, CMake targets, and state/WAV restore behavior.
- Replace all factory presets with exactly 72 unique trance-focused presets: 24 snares, 24 bells/chimes, and 24 toms.
- Add section-lock UI/session state without making locked controls uneditable.
- Keep the 1100 × 972 default editor bounds, SN/BL/TM category jumps, and Simple/Advanced views.
- Use JUCE 8 and C++20; add no dependencies.
- Publish v2.0.0 to `main` and tag `v2.0.0` with the personal noreply Git identity.

## Review Focus

- A preset resets unlocked values but preserves every value owned by a locked section.
- ADSR release ends the dry hit at its selected boundary while effect and reverse tails retain their current synchronization behavior.
- WAV lock-in restores and renders at the current host sample rate.
- MIDI notes/CCs preserve channel, velocity, and controller behavior and cannot create non-finite output.
- All pages and both view modes fit the editor without overlapping or clipped text.

---

### Task 1: Replace the four instrument voices and add ADSR

**Files:** `Source/Core/SmackSynth.h`, `Source/Core/SmackSynth.cpp`, `Tests/SmackSynthTests.cpp`

**Interfaces:** Keep the existing voice settings and render entry points; add the four ADSR settings with the spec ranges. Retain Dance Snare, Club Tom-Tom, FM Bell/Chime, and Pryda/Rim Smack as distinct selectable instruments.

- [x] Rebuild each voice's modal/FM body and transient/noise coupling so the body remains audible through its one-shot decay.
- [x] Add a bounded global amplitude envelope before effects while retaining per-layer tone and noise decay controls.
- [x] Preserve independent stereo phase/noise state and finite-output safeguards.
- [x] Update existing synth assertions only where the new engine's defined outputs require it.
- [x] Build the core target.

### Task 2: Replace effects and engine orchestration

**Files:** `Source/Core/Effects.h/.cpp`, `Source/Core/SmackEngine.h/.cpp`, related existing effect/engine tests.

**Interfaces:** Preserve the current settings structs and process methods; retain host-synced reverse swell, compression, limiter, distortion, gated reverb, choke timing, width, and MIDI behavior.

- [x] Reimplement stereo effect processing with smooth controls, click-free state changes, stable feedback, and finite-state recovery.
- [x] Preserve documented CC and reset behavior, note velocity scaling, and current transport handling.
- [x] Update only existing assertions that are stale because of the new engine.
- [x] Build the engine target.

### Task 3: Add lock-aware preset bank, parameters, and session state

**Files:** `Source/Plugin/Parameters.h/.cpp`, `Source/Plugin/Presets.h/.cpp`, `Source/Plugin/PluginProcessor.h/.cpp`, `Tests/PluginTests.cpp`.

**Interfaces:** Preserve existing APVTS IDs/versions and processor entry points. Add `amp_attack`, `amp_decay`, `amp_sustain`, and `amp_release`; add six non-automatable lock flags to session state using the ownership table in the spec.

- [x] Replace the factory bank with 72 directly authored, calibrated preset definitions in SN 01–24, BL 01–24, TM 01–24 order; set every preset-owned parameter explicitly and derive no values from an index formula.
- [x] Make UI and host program changes use a single lock-aware application path that snapshots locked values through the existing reset-to-default behavior.
- [x] Restore old parameter trees and WAV data; default missing lock state to unlocked and resolve old indices to the new bank.
- [x] Update existing preset/category assertions and add lock/session assertions to the existing plugin test area.
- [x] Build the processor target.

### Task 4: Reimplement the editor and capture all pages

**Files:** `Source/Plugin/Theme.h`, `Source/Plugin/EdmLookAndFeel.h/.cpp`, `Source/Plugin/Controls.h/.cpp`, `Source/Plugin/Panels.h/.cpp`, `Source/Plugin/PluginEditor.h/.cpp`, `Tests/PluginTests.cpp`, `assets/*`.

**Interfaces:** Keep `OpenSmackerEditor`, previous/next navigation, SN/BL/TM jumps, Simple/Advanced switching, audition, pitch lock, WAV lock, every current control, and the animated stage. Use Hit/Layers/FX pages and six section locks.

- [x] Rebuild the editor header, page navigation, panels, lock controls/tooltips, status display, and stage presentation in the openKicker v2 visual language.
- [x] Keep all controls reachable in both modes and all labels/readouts non-overlapping at 1100 × 972.
- [x] Capture built-editor screenshots of Hit, Layers, and FX and link them from README and screenshot documentation.
- [x] Update the existing editor screenshot test capture setup and capture all six views.
- [x] Build the Standalone target and inspect all screenshots for overlaps or truncation.

### Task 5: Update release documentation and version

**Files:** `CMakeLists.txt`, `README.md`, `docs/AUDIO.md`, `docs/SCREENSHOTS.md`, screenshot assets.

- [x] Set the project version to 2.0.0 and document the new engine, ADSR, 72-program catalog, section locks, all retained behavior, formats, build steps, and screenshots.
- [x] Verify every image link resolves and `git diff --check` is clean.

### Task 6: Build, publish, and track CI

- [x] Build AU, VST3, LV2, and Standalone locally.
- [x] Review parameter ownership, preset completeness/count/categories, session compatibility, UI page bounds, and changed-file scope.
- [ ] Commit with personal Git identity. Before each push, run `git log --format='%h %ae %ce' origin/main..HEAD` and confirm all emails are `221119764+alanmarcero@users.noreply.github.com`.
- [ ] Push `main` and annotated tag `v2.0.0`; track the tagged GitHub Action through package and release completion.
