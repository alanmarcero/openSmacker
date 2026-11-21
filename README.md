# openSmacker v2.7.0

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

## Beta

All of these plugins are in beta. The macOS and Linux builds are not up to date, so the Windows build is the one to use for now. A plugin can change dramatically from one release to the next, but the goal is for each one to keep getting better.

openSmacker synthesizes pitched trance percussion. Play a short backbeat, tune a tom fill to your bass line, or hold a metallic note from a MIDI keyboard.

![The Hit editor with the Backbeat snare, its waveform, body tuning, strike and amplitude envelope](docs/screenshots/preview.png)

## Find a sound

Choose **Snare**, **Tom**, **Bell**, or **Rim** in the sidebar to set the voice. The **Preset** menu organizes all 84 factory presets into six dance styles: 24 Uplifting and 12 each of Rolling, Progressive, Psy, Tech, and Classic. It shows the name of the loaded preset, factory or your own, and adds * after you edit it. **Previous**, **Next**, and **Audition hit** work on every page.

Try *Backbeat* for a snare with a short pitched shell and tight wires, *Glass Pin* for a struck FM chime, *Floor Pocket* for a low tom, or *Rim Tick* for a short side accent. Snares and rim shots fall off into noise. Bells and toms keep a pitch. Harder strikes produce a brighter attack. Factory hits sit under the output limiter. The **Hit** page shows a preview of the dry one-shot at its root pitch. The illustrated drumhead in the sidebar pulses on the host beat, rests when playback stops, and runs on an internal clock in Standalone.

## Play one-shots or held notes

The factory sounds start in **One-shot**. Use **Body decay**, **Pitch sweep**, and **Strike Level** to fit the hit to your rhythm. On the **Layers** page, set the number and spacing of flam strikes, or adjust the wire texture, level and decay.

Choose **Hold notes** under **MIDI gate** on the Hit page to sustain the voice from a keyboard. **Attack**, **Decay**, **Sustain**, and **Release** are editable in both views. A matching note-off on the same MIDI channel starts the release, which can run up to 20 seconds. The struck FM overtones fade while the body continues to sound. **Audition hit** and the host roll play one-shots.

Select **Note Tracking** under **MIDI pitch** on the MIDI page to play melodies. **Fixed (Root)** keeps the pitch set on the Hit page. Set the velocity or aftertouch response separately for the body, wires, and transient.

## Shape and route the layers

Enable **Advanced controls** for the transient filters and pan, wire attack and hold, and flam humanization. In this view the **Layers** page adds a **Body detail** card with shell resonance and FM, pitch decay, fine tune, lock pitch, and the body filters and pan. Lock a section to keep its values while browsing. Locked controls stay editable.

The **Hit** page includes the **Output** strip (dual-layer 3-band EQ with ±12 dB gain and smooth frequency glide, stereo width, output gain, and brickwall limiter) alongside body tuning, transient strike, and amplitude envelope.

On **Effects**, choose Tape, Tube, Hard Clip, or Wavefold saturation. Advanced controls include compression, gated reverb, a tempo-synced reverse swell, and **Lock WAV** to store a rendered hit in the project. **Drag WAV** pulls that locked hit out of the plugin as a WAV file. If nothing is locked, it renders a fresh hit instead. Three additional stereo output buses carry the dry body, wires, and transient.

The window resizes freely from 70% of its default size, and the layout scales evenly with it. The project remembers the size. **Undo** and **Redo** step through parameter edits, including Cmd/Ctrl+Z and Shift+Cmd/Ctrl+Z. **A** and **B** keep two copies of the sound; **A>B** and **B>A** copy between them. **Presets** lists the factory bank and your own sounds, with search and favorites. Save, rename, and delete write user presets to Documents/Alan Marcero/openSmacker/Presets. Before Save replaces a preset with the same name, or Delete removes one, the button asks again; click it a second time within three seconds to confirm. Factory presets stay read-only.

The MIDI page also has editable voice zones. The default ranges are 36–47 for snare, 48–59 for tom, 60–71 for bell, and 72–84 for rim. Use host-synced eighth-, sixteenth-, or thirty-second-note rolls and adjust their probability, velocity, and accent. Assign CCs to each layer. CC 7 controls output, CC 74 controls the air filter, CC 120 and 123 stop sound, and CC 121 resets controllers.

## Screenshots

![Layer editor showing flam timing and wire controls](assets/openSmacker-layers.png)

![Advanced effects editor with saturation, dynamics and space controls](assets/openSmacker-fx-advanced.png)

![MIDI pitch, key zones, velocity, aftertouch and host roll](assets/openSmacker-midi-advanced.png)

## Formats and license

VST3, LV2, and Standalone are available on macOS, Windows, and Linux. AU is available on macOS. Earlier projects open as **Custom sound** with their saved parameters, original synthesis, section locks and embedded audio. Selecting a new factory program loads the new sound.

MIT License. See [LICENSE](LICENSE).
