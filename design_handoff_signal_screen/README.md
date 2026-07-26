# Handoff: Plume — Signal Screen (variant 6a)

## Overview
Redesign of the **Signal** screen — the RSSI proximity tracker for a device the user
has targeted from the Scanner feed (`f` → arrows → `t`) or from a Detections detail
view. The screen answers one question: *is this thing getting stronger or weaker, and
how close have I ever gotten?* It renders on the M5Cardputer ADV's **240 × 135 px**
LCD.

Deliberately **no distance readout**. RSSI is relative — multipath, antenna
orientation, and enclosure all affect it — so any foot/metre figure would be a lie.
Everything on screen is dBm, percent-of-scale, or elapsed time.

## About the Design Files
`Signal Screen 6a.dc.html` in this bundle is a **design reference built in HTML** —
a prototype of the intended look, not production code to copy. The target here is
**Arduino/C++ firmware** (`FlockDetection_Cardputer_ADV.ino`) drawing into an
`M5Canvas` sprite that gets DMA-pushed to the panel. The task is to reproduce this
layout with the firmware's existing sprite-drawing calls (`drawString`,
`fillRoundRect`, `drawFastHLine`, `drawLine`, `fillTriangle`, …) and its established
`draw_*_screen()` structure — not to embed any HTML.

The HTML mock scales the 240 × 135 canvas up 2× for legibility. **All coordinates in
this document are in real device pixels** at 1×.

## Fidelity
**High-fidelity.** Coordinates, colors, and type sizes are final. Two caveats:

1. **Font.** The mock uses [Cozette](https://github.com/the-moonwitch/Cozette) (bundled
   as `fonts/CozetteVector.ttf`) as a browser stand-in for the M5Cardputer's built-in
   bitmap font. On device, use the built-in font at the sizes mapped in
   *Typography* below — do not try to match Cozette's exact glyphs.
2. **Sub-pixel values.** Chart vertices in the mock carry one decimal (e.g. `y=116.1`).
   Round to integers on device; the panel has no sub-pixel addressing.

## Screens / Views

### Signal (targeted)
**Purpose:** track a held target's signal, see the strongest reading ever achieved,
and know whether the device is still being heard.

**Layout:** single full-bleed screen, no cards. Four horizontal bands:

| Band | y range | Contents |
|---|---|---|
| Header | 0–18 | Shared `draw_header_spr()` chrome |
| Identity | 28–48 | Proto glyph + device name (left), status pill (right) |
| Readout | 50–76 | Peak dBm hero (left), last-heard (right) |
| Meter | 79–96 | Peak-hold level meter |
| Trace | 106–130 | 2-minute filled sparkline |

**Components:**

1. **Header strip** — existing shared component, unchanged. 18 px tall.
   - Title `SIGNAL` at x=4, y=4. Size 11 px, `--fd-header` mint, letter-spacing 0.08em.
   - Right-anchored pill row, 2 px gaps, each pill: padding 1px 3px, radius 3, 8 px
     mono text, line-height 9. Right-to-left draw order per the firmware:
     battery → `W{n}` → `B{n}` → `D{n}` → `GPS` (only when fix lost) → … → `L`.
     - `D{n}` detections pill is **filled** accent (bg mint, text `--fd-bg`).
     - `W`/`B`/battery pills are outline-dim: border
       `color-mix(--fd-dim 40%, --fd-bg)`, fill `color-mix(--fd-dim 18%, --fd-bg)`,
       text `--fd-text`.
     - **`L` pill (caution outline) shows whenever a target is held** — i.e. always
       on this screen while tracking, absent in the no-target state.
   - 1 px hairline divider across the full width at y=18, `--fd-card-border`.

2. **Protocol glyph** — at x=6, y=28, 10 × 10 px, drawn as an **outline**, 1.6 px stroke.
   - **BLE target → diamond**, `--fd-purple` `#8b7cdb`. Points (relative): (5,0) (10,5) (5,10) (0,5).
   - **WiFi target → triangle**, `--fd-teal` (= `--fd-header`). Points: (5,0) (10,9) (0,9).
   - Matches the contact shapes used by the Scanner SCAN visualization.

3. **Device name** — x=21 (5 px gap after the glyph), vertically centred on the glyph.
   Size 16 px, `--fd-text` white, letter-spacing 0.02em. Content is the raw SSID or
   BLE name, e.g. `FS-85068D`, `Flock-3A88`. Truncate with no ellipsis if it would
   collide with the status pill (min 6 px gap).

4. **Status pill** — right-anchored, x-right=6, y=29. Height 11, radius 3, padding
   0 5px, 9 px text, letter-spacing 0.08em, text color `--fd-bg`.
   - `TRACKING` — fill `--fd-header` mint. Any non-Flock target.
   - `HUNTING` — fill `--fd-caution` amber. Target scored as a Flock/Raven device.
   - `STALE` — fill `--fd-caution` amber. Not heard within the freshness window.

5. **Peak hero** — x=6, y=50, baseline-aligned run of three parts, 5 px gaps:
   - peak dBm value, **26 px, white** (`--fd-text`) — e.g. `-48`
   - `dBm`, **11 px, white**
   - `(peak)`, **9 px, white**, letter-spacing 0.12em, 2 px extra left margin
   - All three stay white in every state — they are the readings, not the status.

6. **Last heard** — right-anchored, x-right=6, y=58. Size 13 px, letter-spacing 0.06em.
   Format `SEEN <n>s AGO`, switching to `SEEN <n>m AGO` above 99 s.
   - `--fd-text` white while fresh.
   - `--fd-caution` amber once stale.

7. **Peak-hold meter** — the live level plus a held marker at the strongest reading.
   - Track: x=6, y=83, w=228, h=6, radius 3, fill `--fd-card` `#1d3258`.
   - Live fill: same rect, radius 3, width = `228 * level`, fill = accent
     (`--fd-header` tracking / `--fd-caution` stale). Empty when the signal is lost.
   - **Peak-hold hairline:** 2 px wide, 14 px tall, `--fd-text` white, at
     y=79 (4 px above the track, overshooting 4 px below). x per the formula below.
   - Behaviour is audio-style peak hold: the fill follows the live level, the hairline
     parks at the maximum and never falls back while the target is held.

8. **Trace** — 2-minute rolling sparkline, 2-second samples (60 max).
   - Plot area x=6…234, y=106…130. Baseline hairline at y=130, `--fd-card-border`,
     0.4 px (1 px on device).
   - Filled area beneath the polyline: accent at **14 % opacity**. On device, blend
     against `--fd-bg` with `lerp_col16()` — the sprite engine has no alpha buffer.
     **Solid fill only — no vertical column/hatch fill.**
   - Polyline: 1.5 px (2 px on device), accent color.
   - Current-position dot: r=2.5, `--fd-text` white, at the final vertex. Omitted
     when the signal is lost (the trace is on the floor and there is nothing current).
   - **Peak tick:** thin **solid white** vertical line, 0.75 px (1 px on device),
     `--fd-text`, from y=104 to y=130, at the x of the sample where peak occurred.
     Not dotted or dashed.
   - When the held peak occurred **before** the visible window, **omit the peak tick
     entirely** — the meter hairline carries it alone. Never re-point it at the
     in-window maximum; that would contradict the hero value.
   - On dropout the trace falls to the floor (y=130) rather than freezing at the last
     reading, so a gap is visibly a gap.

### Signal (no target)
Header as above but **without the `L` pill**. Centered dim text `NEED TARGET`
(16 px, `--fd-dim`, letter-spacing 0.10em) at y≈52, with a hint below at y≈74
(10 px, `--fd-dim`): `press F for feed, pick a device, then T`. Baseline hairline
still drawn at y=130.

## Interactions & Behavior
- `t` — set target (from expanded feed or Detections detail). Entering this screen.
- `l` — clear target → returns to the no-target state.
- `,` / `/` — previous / next screen. `;` / `.` unused here.
- No hover or focus states — there is no pointer, and nothing on this screen is selectable.
- **Animation:** numeric changes roll (new digit slides up) over `--fd-anim-quick`
  180 ms with ease `cubic-bezier(0, 0, 0.2, 1)`. The meter fill eases over
  `--fd-anim-normal` 320 ms; the peak hairline snaps instantly (peak detection must
  not look smoothed). No bounces.
- **Ambient mode is suppressed while a target is held** — existing firmware behavior,
  preserve it.

## State Management
Per-target state, reset on new target:

| Variable | Meaning |
|---|---|
| `target_mac` / `target_name` | Identity of the held device |
| `target_is_ble` | Selects diamond vs triangle glyph |
| `target_is_flock` | Selects `HUNTING` vs `TRACKING` |
| `rssi_now` | Smoothed current RSSI, dBm |
| `rssi_peak` | Strongest RSSI seen since targeting — drives hero + hairline |
| `rssi_peak_sample_idx` | Ring-buffer index of the peak; `-1` if it has aged out |
| `rssi_hist[60]` | 2-second samples, ring buffer |
| `rssi_sample_count` | Samples collected — see *cold start* |
| `last_heard_ms` | Timestamp of last advertisement/frame from this device |

Derived each frame:
- `age_s = (now - last_heard_ms) / 1000`
- `is_stale = age_s > STALE_AFTER_S` (suggest **30 s**)
- `signal_lost = age_s > LOST_AFTER_S` (suggest **10 s**) → trace floors, dot hidden,
  meter empties

**Cold start (important).** A fresh target has almost no history, so a fixed 2-minute
axis renders a stub in an empty box for the first minute. Scale the x-axis to the data
you actually have — window grows `-10s → -30s → -1m → -2m` and pins at 2 minutes —
so the plot is always full width. While `rssi_sample_count < 8`, draw vertex dots
(r≈1.6) at each sample so a 5-point plot reads as deliberate rather than broken.

## Design Tokens
From `colors_and_type.css` (bundled). Day palette:

| Token | Hex | Use here |
|---|---|---|
| `--fd-bg` | `#050a14` | Screen background, filled-pill text |
| `--fd-card` | `#1d3258` | Meter track |
| `--fd-card-border` | `#2e4670` | Header divider, trace baseline |
| `--fd-header` / `--fd-accent` | `#4ddbc2` | Title, `TRACKING`, mint accent, WiFi triangle |
| `--fd-text` | `#e8efff` | Device name, all readings, peak markers |
| `--fd-dim` | `#95a5b8` | Empty-state copy, secondary labels |
| `--fd-caution` | `#ffb547` | `HUNTING`, `STALE`, stale accent |
| `--fd-purple` | `#8b7cdb` | BLE diamond |

Night mode is a token swap — mint → `#ff5a5a`, purple → `#ff9696`, amber unchanged.
Reference tokens, never literals, so `n` keeps working.

**Typography** — device bitmap font. Mock px → hardware size:

| Mock | Hardware | Used for |
|---|---|---|
| 8 px | size 1 | Header pills |
| 9 px | size 1 | `(peak)`, status pill |
| 10 px | size 1 | Empty-state hint |
| 11 px | size 1.2 | `SIGNAL` title, `dBm` |
| 13 px | size 1.5 | Last-heard |
| 16 px | size 2 | Device name |
| 26 px | size 4 | Peak hero numeral |

**Spacing:** 2 / 6 / 12 / 18 px. **Radii:** 3 px pills and meter, 4 px cards. **No shadows, no blur, no gradients.**

## Formulas
```
// chart y from dBm — plot band y 106..130, scale -95..-45 dBm
y = 130 - 24 * (dbm + 95) / 50           // clamp to [106, 130]

// meter fill fraction — scale -95..-40 dBm
level = clamp((dbm + 95) / 55, 0, 1)
fill_w = 228 * level

// peak-hold hairline x (meter)
hair_x = 6 + 228 * clamp((rssi_peak + 95) / 55, 0, 1)     // -48 dBm -> ~201

// trace vertex x — n = samples in window, i = 0..n-1
x = 6 + 228 * i / (n - 1)
```
Mock values for reference: `rssi_peak = -48` → hero `-48`, hairline x≈199–201,
peak tick x=198; `rssi_now = -52` → meter 78 %.

## Assets
- `fonts/CozetteVector.ttf` — browser stand-in for the device bitmap font. **Not needed
  on device.**
- `Cardputer.jsx` — device-bezel wrapper used only to frame the mock. **Not part of the
  design.**
- No images or icons. Every glyph is drawn geometry or type.

## Files
| File | What it is |
|---|---|
| `Signal Screen 6a.dc.html` | The design reference — tracking state in a device frame, plus the stale state |
| `colors_and_type.css` | Design tokens (day + night palettes, type scale, motion) |
| `Cardputer.jsx` | Device bezel for the mock |
| `support.js` | Runtime for the HTML mock |
| `fonts/CozetteVector.ttf` | Mock font |

## Open questions for the implementer
1. `STALE_AFTER_S` / `LOST_AFTER_S` are suggestions (30 s / 10 s) — tune against real
   advertisement intervals.
2. The mock shows `SEEN 4s AGO` / `SEEN 47s AGO`. Confirm the format above 99 s
   (`SEEN 2m AGO`?) and whether it should cap (`SEEN >5m AGO`).
3. Peak hold currently never decays. If a slow decay after N seconds off-peak is
   wanted, that is a behavior change and needs a decision.
