# openSmacker audio

Each voice combines a pitched modal body, a separate wire or air layer, optional flam strikes, and a short stick, mallet, rimshot, or laser transient. The main stereo output passes through saturation, compression and limiting. Three extra stereo buses expose the dry layers.

## Voice models

The body uses a fundamental, three higher modes, and FM excitation. The three frequency ratios relative to the fundamental depend on the voice.

| Voice | Upper-mode ratios | Factory body decay | Measured −40 dB tail |
| --- | --- | --- | --- |
| Dance Snare | 1.62, 2.42, 3.14 | 82–168 ms | 76–153 ms |
| FM Bell / Chime | 2.00, 2.71, 4.09 | 125–249 ms | 112–223 ms |
| Club Tom-Tom | 1.51, 2.02, 2.74 | 93–192 ms | 85–173 ms |
| Pryda / Rim Smack | 1.86, 2.85, 4.18 | 61–132 ms | 53–114 ms |

Body velocity controls both level and the initial depth of FM excitation. Upper FM components decay with the strike even when the amplitude envelope holds a note. Set body velocity response to zero for a consistent timbre across velocities. Older saved sessions retain the original mode ratios, FM response and envelope settings.

## Envelopes and space

In One-shot mode, the amplitude envelope fits inside the dry hit. Body decay and the separate wire attack, hold, and decay shape their respective layers. In Hold notes mode, MIDI note-on opens the amplitude envelope and the matching note-off starts the release. The release range reaches 20 seconds. Audition and host-synced rolls always trigger a one-shot.

Gated reverb follows the dry layers. The 72 factory sounds use 0–9% room mix with an 80 ms reverb gate and have reverse disabled. Users can increase the room time, change the gate, or use a tempo-positioned reverse swell.

## Rendering and recall

Lock WAV renders the selected voice, or the mapped voices when key zones are active, into the project. Each cache stores the stereo mix and separate layer stems. On reload, the plug-in converts cached playback to the host rate. Section locks preserve selected parameter groups while browsing.

The factory regression plays every program through the processor at 48 kHz and MIDI velocity 110. It measures peak level, the body at 20–40 ms, and the final sample above −40 dB relative to each hit’s own peak. The synthesis regression separately exercises all four held voices at two roots and at 44.1 and 96 kHz.
