# WhirlDT

A JUCE-based VST3/AU/Standalone plugin that models the **DigiTech Whammy DT** pedal.
Built by Moth Production. The goal isn't a pitch shifter that "sounds similar" — it's
a digital model grounded in the measured response of the real hardware.

## Interface

The plugin recreates the pedal's physical layout as a functional model, not a skin of
DigiTech's artwork — same knobs, same switches, same overall pedal:

- **Whirl selector** — a single rotary control that steps through 21 positions,
  grouped the way the pedal's "Guided Tour" chapter groups them:
  - **Harmony** (9 modes) — dry signal retained, e.g. `Oct Dn / Oct Up`, `5th Up / 6th Up`
  - **Whirl** (10 modes) — wet-only pitch effects, e.g. `2 Oct Up`, `Dive Bomb`
  - **Detune** (2 modes) — `Shallow` / `Deep`
- **Drop Tune selector** — a separate rotary, 18 positions across a Shift Up / Shift
  Down arc (`Oct Up` … `1 Up` / `1 Dn` … `Oct Dn`, plus the `+OD`/`-OD` dry-blend
  positions)
- **Expression treadle** — the TOE/HEEL pedal rocker, wired as the `whirlPedal`
  parameter
- **Footswitches** — Whirl bypass, Drop Tune bypass, and a Momentary (press-and-hold)
  mode, each with its own LED

## Current status

The UI and full parameter layout are implemented and skinned in the Moth Production
house style (Oxanium / Barlow Condensed / Rajdhani, brushed-metal chrome, cyan glow).

Whirl and Drop Tune are separate serial pitch stages, and the expression treadle
interpolates their heel/toe pitch endpoints. The raw profiling WAV pair
contains all 40 recorded states, including a final `2 Oct Up` take which was omitted
from `measured.json`; the offline validator recovers its pitch and stable level
directly from that take.

Detune `Shallow` uses a deliberately strengthened -7.5 cent toe endpoint instead of
the measured -4 cents. Playing tests found the measured offset too restrained on
guitar; -7.5 cents makes the movement more audible while remaining distinctly below
the measured `Deep` endpoint of -11.4 cents. The heel endpoint remains unshifted.

Whirl retains the measured granular/phase-locked hybrid used for its Harmony,
Whammy, and Detune groups. Drop Tune has its own engine: every selector position,
up and down, now uses the same linked-channel Signalsmith Stretch polyphonic
frequency mapper. This avoids switching algorithms at the middle of the Drop Tune
selector and removes the input-pitch-dependent grain retuning which was unstable on
noisy, decaying, or polyphonic guitar DI.

Drop Tune uses an approximately 85 ms analysis window at 48 kHz with 8x overlap.
Shorter 42-64 ms configurations were tested but rejected because low-E notes could
miss the requested interval by 20-70 cents. The resulting fixed 4096-sample
algorithmic latency is mirrored by the dry and bypass paths and reported to the host;
none of the recording latency stored in the measurement JSON is added. The dry+octave
modes therefore combine time-aligned signals instead of an undelayed dry attack and a
late shifted voice.

There is no adaptive RMS normaliser in Drop Tune: it caused audible gain pumping on
pick attacks and note decays. Stable hardware staircase levels are applied as fixed
per-mode trims after compensating the new engine's steady-state gain. Clipped/global
capture RMS is deliberately excluded from those trims. On two real-guitar DI sections,
the representative +12/+1/-1/-12 modes measured 0.024-0.081 semitone median absolute
pitch error and 0.054-0.174 semitone p90 error. Across all 18 hardware Drop Tune
captures the sustained-tone median/max pitch errors are 0.065/0.242 semitone, with a
0.0023 median absolute inharmonicity-ratio difference from the pedal.

Whirl's grain period is retuned periodically (via lightweight autocorrelation on the
input) to a whole multiple of the input's own detected pitch period. This is what
actually stops an audible amplitude "wobble":
four grains spaced a quarter-period apart only avoid beating against each other if
that spacing is *also* a whole number of input periods, not just the full grain
period - fixed-period grains, or grains merely nudged at the wrap point (WSOLA)
without retuning the period itself, still beat between wraps whenever the shift
ratio isn't a "nice" one like an octave. Every grain now wraps by exactly the same
period: independently searching for splice offsets perturbed their quarter-period
spacing and created modulation sidebands. The four grain windows still provide a
self-normalising base, after which the stable hardware level-staircase median is
applied per mode. Distorted/global capture RMS is not used for that trim.

Retuning the grain period itself must not click or temporarily bend the pitch. A
confirmed estimate therefore configures an inactive, already-running grain bank;
the signal moves to it through a 40 ms squared-sine crossfade while both banks keep
the requested pitch ratio. How many matching checks are required before acting
depends on what changed: a large, confidently-periodic move in the input's own
detected period means a genuinely new note, and locks in within two checks (~23 ms)
so a real phrase isn't heard playing the previous note's pitch for the first part
of every new one; a small move, or a move that isn't cleanly periodic yet (the
first checks after an onset still have the previous note's tail in the analysis
window), keeps the slower five-check hysteresis that stops vibrato or harmonics
from chattering. This tracker and its ratio-dependent upward anti-alias filter are
Whirl-specific; Drop Tune no longer depends on a detected input fundamental.

## Measurement-driven DSP

Measurements are done with **MeasurementMapper**, a custom-built JUCE tool shared
across several Moth Production plugin projects
(`MothProduction-Shared-VST/MeasurementMapper`).

Workflow:

1. Record the same stimulus sequence **dry** and **through the real Whammy DT**
2. MeasurementMapper aligns the two (correlation + fractional-sample refinement),
   splits the recording into per-stimulus sections, and computes gain, timing drift,
   pitch ratio, and correlation for each section and globally
3. It exports a JSON model spec (`global`, `aggregates`, `sections[]`,
   `transferCurve[]`, `polynomial[]`, `modelHint`) that classifies the pedal's
   behavior — stable pitch shift, level-dependent mapping, static-transfer-plus-timing
   hybrid, or a fully nonlinear/time-varying process

That spec is the implementation brief: it decides which DSP modules actually belong
in the chain (latency compensation, pitch-shifting/resampling, nonlinear level
mapping, output shaping, section-dependent switching), rather than the architecture
being decided upfront.

## Codex session implementation log

This is the cumulative record of implementation, correction and validation work
completed by OpenAI Codex during this development session.

### Profiling and measurement model

- Inspected the complete dry/wet recording pair and `measured.json` from the Whammy
  profiling volume, then mapped the 40 recorded states to the Whirl, Harmony, Detune
  and Drop Tune selectors.
- Recovered and validated the final raw `2 Oct Up` recording which exists in the WAV
  pair but was omitted from the exported JSON.
- Implemented the measured heel/toe interval maps and per-mode output levels. Level
  trims use stable staircase sections instead of global capture RMS, so distorted or
  clipped wet sections do not bias the runtime gain.
- Kept recording/capture latency as diagnostic metadata only. No measured delay is
  intentionally inserted into the plugin signal path.

### Selector UI and controls

- Rebuilt the Drop Tune display around the physical pedal's separate `SHIFT UP` and
  `SHIFT DOWN` arcs, with individual numbered positions and active-state indicators.
- Reworked the Harmony, Whammy and Detune selector presentation into the grouped
  list-style layout of the hardware, including the separate `SHALLOW`/`DEEP` strip.
- Corrected the mechanical mode order used by rotary dragging, host automation and
  visual selection, especially around the Whirl selector's group boundaries.
- Changed both selectors to allow continuous full-circle knob rotation instead of
  stopping at an artificial endpoint.

### DSP implementation and artifact fixes

- Implemented the initial measurement-driven processing chain, then tested the later
  Claude DSP rewrite against the same raw/wet measurements and real guitar material.
  The Whirl granular/phase-locked architecture was retained where it performed well;
  Drop Tune was separated into its own dedicated engine.
- Replaced Drop Tune's input-pitch-dependent granular/spectral switching with one
  linked-channel, polyphonic Signalsmith Stretch engine across all 18 up/down modes.
  This removed the algorithm boundary in the selector and greatly reduced unstable
  pitch jumps and glitches on noisy attacks, decays, chords and imperfect guitar DI.
- Selected a 4096-sample analysis configuration at 48 kHz after shorter 2048/3072
  settings produced 20-70 cent low-E errors. The resulting algorithmic latency is
  reported to the host and mirrored in Drop Tune's dry, dry+octave and bypass paths.
- Added 30 ms interval smoothing for automated mode changes and kept one exact
  frequency map across the guitar spectrum to avoid chorus-like partial bending.
- Removed Drop Tune's adaptive RMS correction, which pumped during pick attacks and
  note decays, and replaced it with fixed hardware-calibrated per-mode gain trims.
- Tuned Detune `Shallow` from the measured -4 cents to a more audible -7.5 cents at
  the user's request, while retaining separation from `Deep` at -11.4 cents.

### Regression tooling and results

- Added `WhirlDTDSPTests` coverage for pitch accuracy, measured output levels,
  selector mappings, bypass latency alignment, discontinuities, mode automation,
  multiple sample rates/block sizes, mono/stereo operation and invalid-sample safety.
- Added `WhirlDTProfileValidation`, which runs the plugin over the original profiling
  source and compares all modes with the hardware wet recording for pitch, gain and
  inharmonicity.
- Added `WhirlDTGuitarRender`, which renders all 18 Drop Tune modes from a real guitar
  DI segment as 32-bit float WAV files. Float output prevents the test writer itself
  from clipping the intentionally hotter dry+octave modes.
- On the supplied guitar DI, Drop Tune octave-down p90 pitch error improved from
  2.151 to 0.164 semitones and frame-to-frame pitch jitter from 1.227 to 0.170
  semitones. The +1 mode improved from 0.378 to 0.070 semitones p90 error and from
  0.414 to 0.064 semitones jitter.
- Across the 18 hardware Drop Tune captures, the final sustained-tone median/max
  pitch error is 0.065/0.242 semitones and the median absolute inharmonicity-ratio
  difference is 0.0023.

### Build and release work

- Added and pinned Signalsmith Stretch, connected it to the plugin and test targets,
  and documented its MIT licence in `THIRD_PARTY_NOTICES.md`.
- Added host latency reporting and verified the resulting AU latency property.
- Built and installed the VST3, AU and Standalone targets during development.
- Released and installed AU versions 0.4.1 and 0.4.2, verified their bundle metadata
  and ad-hoc signatures, and passed Apple's complete `auval` validation. Version
  0.4.2 contains the stronger Detune `Shallow` setting.

Claude Code has also contributed to the DSP architecture and general plugin
iteration; the log above is limited to the work completed by Codex in this session.

## Build

Requires CMake 3.24+; JUCE 8.0.4 and Signalsmith Stretch are fetched
automatically via `FetchContent`.

```bash
./build.sh
```

Builds VST3, AU, and Standalone. `COPY_PLUGIN_AFTER_BUILD` installs the VST3/AU into
the system plugin folders automatically — a DAW rescan is all that's needed
afterwards.
