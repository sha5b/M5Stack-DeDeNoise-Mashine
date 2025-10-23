# M5Stack-Fire Noise Synth & Visualizer 

A handheld chaos machine that turns your M5Stack Fire into a pocket-sized noise lab, because silence is overrated and your neighbors had it too easy anyway.

## Features
- **46 sound modes**: White/Pink/Brown/Blue/Violet noises, classic waveforms, Shepard tones (up/down), FM/AM tricks, plucked strings, modal drums, granular, supersaw, PWM, ring-mod, chorus-ish, formants, sync, super-square, plus a grab bag of FX like bitcrush, phaser-ish comb, stutter/glitch, Doppler, gated reverb, aliasing buzz, etc. See `src/types.cpp` and `include/types.h`.
- **Realtime oscilloscope**: Visualizes the actual DAC waveform in the rectangle region on-screen.
- **Shuffle mode**: Auto-hops tracks on a timer so you can pretend it’s generative art and not button mashing.
- **No-pop DAC handling**: Starts/stops the speaker more politely than your average Bluetooth speaker.

## Demos
- Short handheld clips of the app running on M5Stack Fire.

Note: GitHub does not render inline videos in README. The demo files are in `static/`. Use these links to watch them:



<video src="static/video_2025-10-23_20-40-14.webm" width="320" height="240" controls></video>
- [Demo 2 — 2025-10-23 20:40:28](static/video_2025-10-23_20-40-28.webm)

## Hardware
- **Board**: M5Stack Fire (`board = m5stack-fire`)
- **Audio**: On-board speaker via DAC1 `GPIO25` (`AUDIO_DAC_PIN = 25`)
- **Display**: Built-in TFT, landscape rotation

Key constants are in `include/config.h`:
- **`SAMPLE_RATE_HZ = 11025`**
- **`AUDIO_DAC_PIN = 25`**
- **`TRACK_COUNT = 46`**
- **Visual area**: `NOISE_W = 280`, `NOISE_H = 160`, positioned at `(NOISE_X, NOISE_Y)`
- **Frame timing**: `FRAME_INTERVAL_MS = 67`
- **Shuffle**: `SHUFFLE_INTERVAL_MS = 12000`

## Build and Upload (PlatformIO)
- Uses Arduino framework and `M5Stack` library.
- Config: See `platformio.ini` (`[env:m5stack-fire]`, `platform = espressif32`, `framework = arduino`, `lib_deps = m5stack/M5Stack@^0.4.3`, `monitor_speed = 115200`).

Steps:
1. Install VS Code + PlatformIO.
2. Connect the M5Stack Fire via USB. Select the correct serial port if needed.
3. Open this folder (`m5stack-f25`) in VS Code.
4. Build: PlatformIO: Build.
5. Upload: PlatformIO: Upload.
6. (Optional) Open Serial Monitor at 115200 baud to see logs.

## Run/Controls
- On boot, the app shows track info and a waveform area.
- Buttons (printed on screen as a reminder):
  - **A**: short press → previous track. Hold → volume down (repeats).
  - **B**: short press → play/pause. Long press (~2s) → toggle Shuffle.
  - **C**: short press → next track. Hold → volume up (repeats).
- Current track name, shuffle state, and volume percent show in the header.

## What you’re seeing/hearing
- **Audio task**: `src/audio_synthesis.cpp` runs on core 1 and writes 8-bit samples to DAC at `SAMPLE_RATE_HZ`.
- **Waveform**: `src/visual_rendering.cpp` reads a ring buffer (`VIS_RING_SIZE = 1024`) to draw the real output as an oscilloscope.
- **Track mapping**: `src/types.cpp:getCurrentNoiseType()` indexes into `include/types.h:NoiseType` (total `TRACK_COUNT`). Display names: `getNoiseTypeName()`.
- **Gain normalization**: `getGainForType()` balances perceived loudness per mode; master gain is adjustable via A/C holds.

## Adding new sounds
- Add a new enum to `include/types.h:NoiseType`.
- Map it to a track index and name in `src/types.cpp` (`getCurrentNoiseType`, `getNoiseTypeName`, `getGainForType`).
- Implement synthesis in `src/audio_synthesis.cpp` or extend `src/audio_extras.cpp`. If using extras, wire in via `isExtraType()` and `nextAudioSampleExtra()`.
- The UI will automatically show your new mode when its track is selected.

## Folder layout
- **`src/`**: `main.cpp` (UI/input), `audio_synthesis.cpp` (audio), `visual_rendering.cpp` (oscilloscope), `types.cpp` (track map), `audio_extras.cpp` (additional generators).
- **`include/`**: Headers (`config.h`, `types.h`, etc.).
- **`platformio.ini`**: Build target and dependencies.

## Troubleshooting
- **No sound**: Ensure it’s a Fire (with speaker amp) and the device isn’t muted at OS level. Try volume holds (A to decrease, C to increase). Confirm DAC is `GPIO25`.
- **Upload issues**: Select the correct COM port in PlatformIO. Press and hold the reset/boot combo on the M5 if necessary.
- **Crackles at start/stop**: The code soft-lands the DAC, but tiny pops can happen. Let it settle a second after play/pause.
- **LCD glitches**: Keep default rotation and let the app paint the oscilloscope. Frame pacing is ~15 FPS (`FRAME_INTERVAL_MS = 67`).

## Safety notes (seriously)
- Some modes get bright and/or piercing (e.g., near-Nyquist tones). Don’t crank it into headphones. Protect your hearing.
- Infrasound and thump modes are for fun, not medical therapy.

## License
MIT License. See [LICENSE](./LICENSE) for full text.
