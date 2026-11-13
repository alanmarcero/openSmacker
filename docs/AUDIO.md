# openSmacker v2 audio notes

openSmacker v2 synthesizes short, pitched percussion directly. Each hit combines a modal membrane or metallic voice, an independent noise/wire layer, optional pre-strikes, and a dedicated transient. This lets a tuned body remain present under a bright attack instead of collapsing into a single low sine-like partial.

## Voices and envelopes

- **Dance Snare:** inharmonic shell modes, a short pitch sweep, wire noise, and transient coupling.
- **Club Tom-Tom:** tuned membrane modes and a decaying pitch bend with an optional mallet or laser front edge.
- **FM Bell / Chime:** inharmonic carrier partials and FM modulation with a long, clearly pitched tail.
- **Pryda / Rim Smack:** a high tension body with a bright rim transient and wire energy.

The amplitude ADSR shapes the complete dry hit. Attack, decay, sustain, and release have bounded ranges suitable for a one-shot sound. Release is kept within the generated dry tail; the gated reverb and synchronized reverse layer continue to follow their effect timing. Body decay and the separate noise attack/hold/decay controls still shape their own layers.

## Stereo and effects

Pre-strikes use separate timing, jitter, and pan. Noise colors use independent stereo state and dedicated high- and low-pass filtering. The effect chain retains four saturation curves, stereo-linked compression, a ceiling limiter, master width, gated reverb, and tempo-position reverse swells. Parameters are bounded and audio state recovers from non-finite input values.

## Lock-in WAV

Lock In renders the current hit and effects to an embedded stereo WAV cache. A saved session carries the audio data; restoring it at another sample rate converts playback for that host rate. Section locks are separate session UI state: they control which preset-owned parameters can change while browsing.
