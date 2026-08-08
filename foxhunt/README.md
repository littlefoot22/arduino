# Foxhunt DF — FreeWili OG

A radio direction-finding app for the FreeWili OG. It tunes a CC1101 to a
contest beacon, watches the received signal strength, and turns that one number
into something you can walk with: a smoothed meter, a warmer/colder trend, an
audible pitch that rises as you close in, and a rotation scan that gives you a
bearing.

Builds to a `.wasm` that runs on top of the stock firmware. No reflash.

---

## Read this first: only half the contest is reachable

The FreeWili OG's radios are two TI CC1101 transceivers. The CC1101's
synthesiser covers exactly three bands:

| Band | Contest coverage |
|---|---|
| 300–348 MHz | — |
| 387–464 MHz | **Fox 6–10 (446.025–446.125 MHz)** |
| 779–928 MHz | — |

Fox 1–5 sit at **147.420–147.545 MHz** on 2 m. That is about 240 MHz below the
lowest frequency the PLL can generate. This is not a filter, antenna, or
firmware limitation — the part cannot synthesise those frequencies. Nothing you
load onto a FreeWili will hear them.

**So: this app hunts Fox 6–10. For Fox 1–5 you need a separate 2 m receiver** —
an HT with a signal meter, or an RTL-SDR (24 MHz–1.7 GHz covers both bands).

Two things that do work in your favour:

- **The beacons being analogue FM doesn't matter.** The CC1101 can't demodulate
  FM voice, but direction finding never demodulates anything. It reads the RSSI
  register, which reports carrier power regardless of modulation. The app
  deliberately disables sync-word detection so the receiver never waits for a
  packet that an analogue beacon will never send.
- **Your 433 MHz antenna is the right one.** 446 MHz is close enough that the
  mismatch is irrelevant for receive. Don't use the 315 or 915 MHz elements.

### The 25 kHz problem

Contest channels are 25 kHz apart. The CC1101's narrowest receive filter is
about **58 kHz**. Neighbouring foxes therefore always bleed into each other's
passband, and a reading tracks the strongest signal in that window rather than
one channel in isolation.

In practice this is fine for homing on one fox at a time — the near one
dominates — but you cannot cleanly separate two foxes of similar strength. The
Band Scan screen exists partly to tell you when that's happening.

---

## Building

You need the [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases),
CMake ≥ 3.25, and git.

```bash
export WASI_SDK_PATH=/opt/wasi-sdk-33.0-x86_64-linux
./build.sh
```

Output is `build/foxhunt.wasm` (~24 KB).

### Flashing

Install the vendor Python tool, then send and run in one step:

```bash
pip install freewili
fwi-serial -s build/foxhunt.wasm -w foxhunt.wasm
```

`-s` sends the file, `-w` runs it. Nothing is overwritten — this rides on top of
the stock firmware. (The `.uf2` path is a *full firmware replacement* via the
RP2040 ROM bootloader; you don't need it for this, and using it would remove the
stock firmware this app depends on.)

### Tests

The radio maths and DF filters are pure arithmetic with no host calls, so they
run on your machine:

```bash
./run_tests.sh
```

720 checks covering frequency-word conversion against TI's published register
values, band-limit rejection, config-blob layout, RSSI conversion, meter
behaviour, and rotation-scan binning.

---

## Using it

Five buttons under the screen: **gray, yellow, green, blue, red**, left to
right. Each screen labels them.

### Fox Select

Pick your target. `gray`/`yellow` move, `green` starts hunting, `blue` jumps to
Band Scan, `red` exits.

### Hunt

The main screen.

- **Big number** — smoothed signal in dBm.
- **Bar** — auto-ranged 0–100%. The scale slides as you close in, so the meter
  keeps resolving differences instead of pinning at full scale. It answers
  "stronger than a moment ago?", which is the only question that matters.
- **WARMER / colder / steady** — a fast average compared against a slow one.
  This is what you actually walk by; absolute dBm means little without knowing
  the beacon's power or the terrain.
- **Audio** — pitch rises and chirps get faster with signal, so you can hunt
  with the screen at your side and watch where you're putting your feet. Set
  `kAudioEnabled = false` in `src/main.cpp` for a silent hunt.
- **Board LEDs** — coarse signal bar, readable from the corner of your eye.
- **ATT− / ATT+** — the attenuator. See below.

`green` opens the rotation scan, `blue` goes back, `red` exits.

### The attenuator is the most important control

Near a beacon the receiver's AGC pins at full scale and *every* direction reads
equally strong. This is the classic foxhunt failure mode — you get within 100 m
and the meter stops helping.

`ATT+` backs off front-end gain in five steps (the last drops the LNA by ~17 dB
on top of ~21 dB of DVGA). This is the software equivalent of the screw-on RF
attenuator that's standard foxhunt kit. **When the meter saturates, add
attenuation.** Contrast comes straight back.

### Rotation Scan (the compass)

The FreeWili has an accelerometer but **no magnetometer** — there is no magnetic
heading to read, and no gyro to integrate one from. So this is not a north-
referenced compass, and can't be.

What it does instead is the classic body-shielding sweep:

1. Hold the device flat against your chest.
2. Press `START`.
3. Turn steadily through one full circle over the 12-second sweep.

Your body attenuates signal arriving from behind you, so RSSI peaks when you're
facing the fox. The rose fills in as you turn and marks the peak sector with
`@`, reporting a bearing **relative to where you were pointing when you pressed
START** — not to north. Turning at a roughly even rate is what keeps it honest.

It also reports **null depth**, the spread between strongest and weakest sector:

- **≥ 6 dB** — trustworthy bearing, walk that way.
- **< 6 dB** — don't believe it. Either the signal is arriving off reflections
  (common near buildings and vehicles), or you're close enough that body
  shielding no longer produces a null. Move 50 m and scan again.

### Band Scan

Sweeps all five 70 cm channels and marks the strongest with `*`. Worth running
whenever you lose the trail — it answers "am I even chasing the right fox?"
before you spend twenty minutes walking a wrong bearing.

---

## Field technique

The single-whip approach above works, but the antenna is omnidirectional, so
every bearing comes from body shielding. Two upgrades, in order of value:

1. **A directional antenna.** A three-element 70 cm yagi or a tape-measure yagi
   gives real bearings instead of inferred nulls, and is the single biggest
   improvement available. Point it, watch the meter, walk.
2. **Offset your approach.** Rather than walking straight down a bearing, take
   two bearings from points a few hundred metres apart and cross them. Multipath
   in built-up areas will lie to you along any single line.

As you close in, keep adding attenuation. If the meter is pinned and `ATT` is
already at 4/4, you are very close — start looking rather than walking.

---

## Layout

```
src/foxes.h      Channel table, and why the 2 m foxes aren't in it
src/cc1101.h/cpp Frequency word maths, register blob, RSSI conversion  (host-testable)
src/df.h/cpp     Meter smoothing, trend, rotation scan, audio mapping  (host-testable)
src/ui.h/cpp     Panels and controls — the only module that calls the host API
src/main.cpp     Screen state machine and the sampling loop
tests/           Host-side tests for the two pure modules
```

Sampling runs at 20 Hz; the display repaints at 5 Hz.

---

## Things to verify on hardware

Some of the host API is thinly documented, so a few choices are informed
inference rather than confirmed fact. All are isolated and easy to change:

- **`RadioLoadConfig()` blob format.** The header documents this function as
  `@todo` and no vendor example calls it. The app sends a flat list of
  `(register address, value)` pairs — the format TI's SmartRF Studio exports and
  that essentially every CC1101 driver uses. If the radio doesn't tune, this is
  the first suspect; it's built in one place, `cc1101::build_rx_config()`.
  The hunt screen shows `radio config rejected` if the call returns failure.
- **`RadioGetRSSI()` units.** Docs say dBm; `cc1101::normalize_rssi()` accepts
  either dBm or the raw register and converts, so both work.
- **`setCanDisplayReactToButtons(0)`** is meant to give the app all five
  buttons. The vendor radio example passes `4`. If buttons behave oddly, try `4`
  in `main()`.
- **Font size/type indices** in `addControlText()` are used inconsistently
  across the vendor examples (one passes `64`, others pass `0`–`2`). Layout
  constants are all at the top of `src/ui.cpp` if text is the wrong size.
- The compass rose is drawn by swapping characters rather than by recolouring or
  repositioning controls, because the packed-integer layout that
  `setControlProperty()` expects isn't documented. It's ugly but unambiguous.

## Sources

- [FreeWili radio settings](https://docs.freewili.com/features/radio-settings/) — CC1101 bands, bandwidth, modulation ranges
- [FreeWili radio feature overview](https://docs.freewili.com/features/radio/) — RSSI sampling, monitor mode
- [`freewili/fwwasm`](https://github.com/freewili/fwwasm) — host API header
- [`freewili/wasm-examples`](https://github.com/freewili/wasm-examples) — build and flash workflow
- [CC1101 datasheet](https://www.ti.com/product/CC1101) — register maths, RSSI conversion
