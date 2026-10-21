# HEX-Q ORBIT

Lightweight stereo imaging + auto-panner (VST3, Windows 64-bit) built with C++ / JUCE.
Second product of the HEX-Q family; it uses the same design system as HEX-Q VANTA with a cyan accent.

## Status - read this first

| Item | Status |
|---|---|
| DSP engine (`Source/DSP/OrbitEngine.h`) | **Compiled and tested offline** (g++, 34 / 34 checks pass, see "Test report") |
| JUCE plugin, editor, parameters | **Written but NOT compiled or tested yet** - the first build may show errors |
| Running inside FL Studio | **Not tested** |
| Listening tests (synth, pad, loop, bass, transients) | **Not done** - I cannot listen. Numbers below are measurements, not opinions about sound. |

## Build (no compiler on your PC needed)

1. Create a free GitHub account and a new repository (use a new one, e.g. `HexQ-Orbit`).
2. Upload **all files of this folder to the repository root**. `CMakeLists.txt` must be visible on the repo's front page. Keep the `.github` folder.
3. Actions tab -> **Build HEX-Q ORBIT (Windows)** (~10 min; also runs on every upload).
4. Download the artifact **HEX-Q-ORBIT-Windows**: `HEX-Q-ORBIT-Setup.exe` + a manual-install zip.

Local build: Windows 10/11, Visual Studio 2022 (Desktop C++), CMake 3.22+, Git:
```
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
JUCE 8.0.4 is downloaded by CMake. Output: `build/HexQOrbit_artefacts/Release/VST3/HEX-Q ORBIT.vst3`

## Install in FL Studio
1. Run `HEX-Q-ORBIT-Setup.exe` (SmartScreen may warn because it is unsigned: More info -> Run anyway). Manual: copy the `HEX-Q ORBIT.vst3` folder to `C:\Program Files\Common Files\VST3\`.
2. FL Studio: Options -> Manage plugins -> Start scan.
3. Put **HEX-Q ORBIT** on a mixer insert slot.

Diagnostic trace: this build writes `%TEMP%\HEX-Q-ORBIT-log.txt` (Win+R, `%TEMP%`). If FL reports a crash, the last lines show where. It will be removed once the plugin is confirmed stable.

## The three modes

M = (L+R)/2 is the centre, S = (L-R)/2 is the side. Output is always L = M + S, R = M - S, so **whatever is added to S cancels exactly when L and R are summed to mono** (verified numerically, see tests).

### 1. SPACE - "spread a centered sound outward"
Controls: **Width** 0-200 %, **Space Amount** 0-100 %, mono-compatibility indicator.
- `theta = Space * 45 deg`, `M' = M * cos(theta)`, synthetic side `D = decorrelate(M) * sin(theta)`, `S' = Width * (S + D)`.
- Space Amount moves energy from the centre into a decorrelated side signal (equal power: centre goes down by up to 3 dB at 100 %). That is what makes the sound "spread outward" instead of just getting louder.
- Width scales the whole side component and never touches the mid. Width 0 % = pure mono. Width 100 % + Space 0 % = input unchanged (bit-near identical, tested).
- Real stereo input: its own side is scaled by Width. Dual-mono / mono input has no side, so Space Amount creates one (see "Decorrelation").

### 2. CENTER + SPACE - "keep the centre, add spread"
Controls: **Center Level** 0-100 %, **Space Amount** 0-100 %, **Width** 0-200 %, **Wet/Dry** 0-100 %, Mono Check.
- Wet = `M * Center` as the anchor, plus a spread layer. Output = dry + (wet - dry) * Wet/Dry.
- The centre depends only on Center Level - **raising Space Amount can never remove it** (tested).
- Spread layer = real side above ~150 Hz scaled by Width + decorrelated side * Space * 0.8. The side below ~150 Hz is left unchanged, so Width and Space add **no extra low-frequency stereo information**.
- Defaults (Center 100 %, Space 0 %, Width 100 %) = original sound preserved.
- Difference to SPACE (measured, dual-mono noise, Space 70 %): SPACE centre = 0.85 x input level, CENTER + SPACE centre = 1.00 x input level, and the results differ by ~16 % RMS.

### 3. PAN - auto-panner
Controls: **Rate** 0.05-10 Hz, **Sync** Free / Tempo Sync, **Division** 1/1 1/2 1/4 1/8 1/16, **Depth** 0-100 %, **Waveform** Sine / Triangle / Square, **Stereo Phase** 0-180 deg, **Mix** 0-100 %.
- An LFO controls the gain of each channel: `gain = (1 - Depth) + Depth * sin(pi/2 * u)`, `u = (lfo + 1) / 2`. Depth 0 % = no movement (gain exactly 1).
- **Stereo Phase** = phase offset of the right channel's LFO against the left one.
  **180 deg** (default): left and right move in opposite directions = classic ping-pong pan, equal power (`gL^2 + gR^2 = 1` at full depth). **0 deg**: both channels follow the same LFO = the whole image pulses (tremolo), no left/right movement. Values in between give partial movement. Use 180 deg for real panning.
- Gains never exceed 1, so panning cannot clip and never causes a level jump. A mono / dual-mono source sits about 3 dB lower on average at full depth (equal-power law).
- Stereo input: each channel is attenuated by its own gain - a balance-type movement. The stereo image is kept but the far channel is turned down; it does not re-position individual sources inside the image.
- Gains are smoothed (1.5 ms) so Square does not click; Depth, Mix and mode changes are smoothed too (15 ms).
- Tempo Sync: with the transport running the LFO phase is locked to the host position (`ppq / beats-per-cycle`, re-aligned every block). With the transport stopped (or if the host gives no position) it free-runs at the host tempo. Free mode uses Rate in Hz. Rate is greyed out when synced, Division when free.

### Common
- **Mono Check**: auditions the mono sum on both channels (10 ms crossfade).
- **Bypass**: 15 ms crossfade; fully bypassed = zero CPU, audio untouched.
- **Mode switching**: 20 ms crossfade between the old and the new mode.
- **Mono compatibility indicator** (measured live on the processed signal): *CORR* = L/R correlation of the output, *MONO SUM* = level of the output's mono sum versus the input's mono sum. In SPACE and CENTER + SPACE the side cancels in mono, so MONO SUM only reflects the centre change (e.g. -3 dB at Space 100 % in SPACE). In PAN it shows the real level change caused by the panning.
- Input: stereo, or mono (a mono input is routed identically to both channels). Output: stereo. FL Studio mixer inserts are stereo, so a mono source arrives as L = R ("dual mono") - handled the same way.

## Decorrelation (for mono / dual-mono input): how it works, limits
`D = decorrelate(M)`: 2nd-order high-pass at 200 Hz (bass never goes into the stereo field) -> two Schroeder all-pass filters (4.3 ms and 2.1 ms). Flat magnitude, scrambled phase. It is added to the left and subtracted from the right.
- Cheap (no FFT, no convolution), no latency.
- Limits: it is a diffuse, synthetic width - it does not sound like a real stereo recording. Transients are slightly smeared (a few ms). Used alone at high settings it can sound "phasey" on headphones. Stereo-only effect: in mono it cancels completely.
- **Reduce / disable it**: Space Amount down to 0 % removes it entirely (in both SPACE and CENTER + SPACE).

## Parameters (host automation; IDs are stable)
`mode, bypass, monoCheck, width, space, center, mix, rate, sync, division, depth, waveform, phase`.
All 13 are always automatable. The UI shows only the controls that belong to the active mode.
Defaults: SPACE, Width 100 %, Space 35 %, Center 100 %, Mix 100 %, Rate 1 Hz, Free, 1/4, Depth 100 %, Sine, Phase 180 deg.
(For a perfectly neutral start set Space Amount to 0 %.) Project / preset state is saved by the host through JUCE's value-tree state.

## Architecture
```
Source/
  DSP/         Biquad.h (shared with VANTA), OrbitEngine.h   pure C++, no JUCE, no allocation (unit-tested)
  Parameters/  OrbitParams.*                                  IDs, ranges, defaults, text formatting
  HexQ/        shared design system (identical copy of the VANTA folder): theme, look-and-feel, header, footer
  Orbit/       ORBIT-only UI: ModeButton, MonoMeter, PanPreview
  PluginProcessor.*  PluginEditor.*
tests/orbit_dsp_test.cpp
```
ORBIT sets only its accent colour (`#36D6E7`) on the shared `hexq::Palette`; all styling lives in `Source/HexQ/`.
That folder is a verbatim copy from VANTA - if you change the design system, copy the folder to both projects.

## Implemented / not implemented
Implemented: three modes, all listed controls, tempo sync from the host playhead, mono check, bypass, mono-compat indicator, LFO preview, host automation, state restore, mono + stereo input.
Not implemented (by design): FFT / convolution / oversampling, true M/S-EQ, bass-mono crossover, limiter or auto-gain (width 200 % and loud input can exceed 0 dBFS), custom LFO shapes, own preset browser (host presets are used), MIDI control.

## Test report

### Verified offline - `tests/orbit_dsp_test.cpp` (g++ 13, -O2): 34 / 34 passed
During development two of my own expectations were wrong (not the DSP): a 4 Hz LFO has 8 half-cycles per second, not 4, and a start-up transient of the filter was being counted as a "click". The tests were corrected, the DSP was not changed.
- SPACE: neutral (100 % / 0 %) = unchanged; width 0 % = pure mono; width 200 % doubles the side (ratio 2.00) with the mid unchanged; dual-mono + Space 100 % gives side/mid RMS = 0.99 and L/R correlation 0.004 (a real stereo difference, not a copy).
- Mono compatibility: for 40 random settings of every mode the mono sum of the output equals the expected mid value (error < 1e-5) - the added side cancels.
- CENTER + SPACE: mid = input mid x Center Level for every Space/Width combination; minimum settings and Wet/Dry 0 % reproduce the input; synthetic side at 50 Hz is small (side/mid 0.10) and present at 2 kHz (1.6); differs clearly from SPACE.
- PAN: 1 Hz sine pan flips L/R dominance at the expected rate and reaches full left / full right (weak/strong ratio 0.001); depth 0 % = input; depth 50 % moves less than 100 %; full-scale input never exceeds 0 dBFS (peak 1.00000) for all three waveforms; Square steps max 0.013 (no click); stereo phase 0 deg = no L/R movement, 180 deg = ping-pong.
- Tempo sync (simulated host at 120 bpm): at ppq .25 the right channel is silent, at ppq .75 the left; 1/8 note = 4 Hz with the transport stopped.
- Mode switching every 100 ms, bypass toggling while panning: no step larger than in steady state. Bypass is bit-identical after the fade. Mono Check gives L = R.
- Random automation at 22.05-192 kHz, all modes: finite, bounded; identical results for host block sizes 1-4096; silence decays to exactly 0; **0 heap allocations** while processing; CPU ~0.10 % (SPACE), 0.12 % (CENTER + SPACE), 0.31 % (PAN) of one core at 48 kHz on this machine (rough indication only).

### Still to be tested on your machine (not tested)
- [ ] The CI build succeeds (JUCE compile)
- [ ] Loads in FL Studio, window opens, no crash on open / close / re-open
- [ ] Mono source and stereo source in the mixer; several instances at once
- [ ] Automation of each parameter (including mode)
- [ ] Save project, reopen: mode and all values restored
- [ ] Bypass, Mono Check: no clicks, levels as expected
- [ ] Sample rates 44.1 / 48 / 96 kHz, buffers 64-1024
- [ ] Tempo sync follows FL's tempo and stays locked when you jump around the playlist
- [ ] Listening: centered mono synth, stereo pad, melodic loop, sustained sound, transient sound, bass-heavy sound. Check that SPACE and CENTER + SPACE sound different, that PAN is audible at the chosen rate, and that bass stays centred.
- [ ] Real CPU use with several instances
