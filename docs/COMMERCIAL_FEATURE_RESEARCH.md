# Commercial feature research: openSmacker

## Reference

D16's official PunchBox 2 page describes a layered percussion workflow with per-layer filtering and modulation. The PunchBox manual shows independent layer pitch, high/low cuts, and decay, which informed the layer controls below.

- [PunchBox 2 product page](https://d16.pl/punchbox2)
- [PunchBox user manual](https://d16.pl/pub/manuals/PunchBox-manual-gb.pdf)

This research informs control layout and sound-shaping choices only. openSmacker keeps its own voices, signal flow, and preset bank.

## Capability gaps and implemented changes

1. **Independent layer filters.** The wire layer had HPF/LPF, while body and transient lacked them. Added host-automatable body and transient HPF/LPF controls; the wire filters remain independent.
2. **Layer stereo placement.** Stereo width applied to noise only. Added host-automatable pan controls for body, wires, and transient, each applied after that layer's own processing.
3. **Wire tension.** The snare-wire resonator used fixed feedback. Added a host-automatable tension control that adjusts the feedback/resonance coefficient; non-wire noise types are unchanged.

## Verification

- Release AU, VST3, LV2, and Standalone targets built successfully.
- CTest passed: **52/52**.
- Refreshed and inspected all eight native editor captures: assets/openSmacker-hit.png, assets/openSmacker-hit-advanced.png, assets/openSmacker-layers.png, assets/openSmacker-layers-advanced.png, assets/openSmacker-fx.png, assets/openSmacker-fx-advanced.png, assets/openSmacker-midi.png, and assets/openSmacker-midi-advanced.png. The new layer controls fit on the Hit and Layers pages.
