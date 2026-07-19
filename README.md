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

The DSP chain is still a **skeleton passthrough** — `processBlock` doesn't touch the
audio yet. That's intentional: the pitch-shifting and level-mapping behavior isn't
being guessed at. It's going to be derived from actual measurements of a real Whammy DT.

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

## Built with Claude and Codex

This project is being developed with Claude Code and OpenAI Codex running in
parallel — not as a novelty, but split by what each is being asked to do:

- **Codex designed the UI** — the WhirlDT interface (panel layout, knobs, footswitch
  LEDs, the Moth Production skin) was built with Codex
- **MeasurementMapper**, the dry/wet measurement and analysis tool this project
  depends on, was also built with Codex
- Plugin scaffolding and general iteration have been done with both, in parallel,
  across different parts of the codebase
- For the DSP itself, once a Whammy DT measurement pass is in hand, the plan is
  **Codex implements first** from the MeasurementMapper JSON spec, and **Claude
  reviews** the result against that same spec before it's trusted — a second model
  checking the first one's interpretation of the measured data, on the part of the
  project where getting the model wrong is the whole risk.

## Build

Requires CMake 3.22+; JUCE 8.0.4 is fetched automatically via `FetchContent`.

```bash
./build.sh
```

Builds VST3, AU, and Standalone. `COPY_PLUGIN_AFTER_BUILD` installs the VST3/AU into
the system plugin folders automatically — a DAW rescan is all that's needed
afterwards.
