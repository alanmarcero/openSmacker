# openSmacker audio

Each voice combines a pitched modal body, a separate wire or air layer, optional flam strikes, and a short stick, mallet, rimshot, or laser transient. The main stereo output passes through saturation, compression and limiting. Three extra stereo buses expose the dry layers.

## Voice models

The body uses a fundamental, three higher modes, and FM excitation. The three frequency ratios relative to the fundamental depend on the voice. On the current engine the upper modes are an attack, not a long ring. Snares and rim shots keep a short pitched knock, then the wires carry the tail. Toms keep a drum pitch with a softer octave and some membrane noise. Bells keep staggered inharmonic partials. FM depth is limited so the sidebands stay under the Nyquist frequency. One-shot amplitude falls for the whole hit and lands at silence, with no sustain hold.

| Voice | Upper-mode ratios | Factory −40 dB tail |
| --- | --- | --- |
| Dance Snare | 1.62, 2.42, 3.14 | 56–151 ms |
| FM Bell / Chime | 2.00, 2.71, 4.09 | 100–230 ms |
| Club Tom-Tom | 1.51, 2.02, 2.74 | 53–182 ms |
| Pryda / Rim Smack | 1.86, 2.85, 4.18 | 78–94 ms |

Factory hits peak near −5 dBFS. The limiter ceiling stays at −1 dBFS and does not flatten the factory attacks. Body velocity controls both level and the initial depth of FM excitation. Upper FM components decay with the strike even when the amplitude envelope holds a note. Set body velocity response to zero for a consistent timbre across velocities. Older saved sessions keep the voice they were saved with. Sessions from before the shaped engine keep that older voice until a factory preset is loaded.

## Envelopes and space

In One-shot mode, the amplitude envelope falls across the dry hit and reaches silence at the end. Body decay and the separate wire attack, hold, and decay shape their respective layers. Snare and rim pitch is capped so a long decay lengthens the noise, not a bell tone. In Hold notes mode, MIDI note-on opens the amplitude envelope and the matching note-off starts the release. The release range reaches 20 seconds. Audition and host-synced rolls always trigger a one-shot.

Gated reverb follows the dry layers. The 56 factory sounds use 0–7% room mix with a 70 ms reverb gate and have reverse disabled. Users can increase the room time, change the gate, or use a tempo-positioned reverse swell.

## Rendering and recall

Lock WAV renders the selected voice, or the mapped voices when key zones are active, into the project. Each cache stores the stereo mix and separate layer stems. On reload, the plug-in converts cached playback to the host rate. Section locks preserve selected parameter groups while browsing.

The factory regression plays every program through the processor at 48 kHz and MIDI velocity 110. It measures peak level, the body at 20–40 ms, and the final sample above −40 dB relative to each hit’s own peak. The synthesis regression separately exercises all four held voices at two roots and at 44.1 and 96 kHz.
