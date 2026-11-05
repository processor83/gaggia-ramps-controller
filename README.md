# Gaggia Classic Pro — PID on RAMPS 1.4 + Arduino Mega

> ⚠️ **WORK IN PROGRESS.** This firmware and this doc are still
> evolving: every change is published with its entry in
> [`CHANGELOG.md`](CHANGELOG.md). Don't wire anything to 120 V without
> having read the Wiring section and the safeties below.

PID + pressure controller for your Classic Pro 2021, using the 3D-printer
hardware you already have. Firmware: `gaggia-ramps.ino`.

> **Note:** the firmware's on-screen text (menu labels, status and fault
> messages) is still in French. This README gives the English meaning;
> literal on-screen strings are shown in quotes.

## RAM verdict

Yes, it fits with room to spare. The 128x64 graphic LCD buffer = 1024 bytes.
The Mega has 8192 bytes of SRAM: **compiled = 4,826 bytes used (~59%)**,
47.0 KB of flash out of 258 KB. The Nano (2048 bytes) would have been tight —
the Mega, no problem.

## What it does

- **5 PROFILES** (name + brew temp + target pressure each), saved
  to EEPROM. Defaults: P1 Espresso 93 °C, P2 Ristretto 94 °C,
  P3 Lungo 92 °C, P4 Decaf 91 °C, P5 Doux 90 °C, target pressure 9 bar.
  **Steam temp is GLOBAL (145 °C)** — the same for every coffee.
  **The PID is GLOBAL** (hardware: boiler + element) — one set of
  Kp/Ki/Kd for the whole machine, not one per profile.
- **Target pressure per profile** — for now a display target only:
  without a dimmer, the real pressure is set by the mechanical OPV
  (around 12 bar stock on the Pro, adjustable to 9 bar with the screw).
  With the dimmer (phase 2), the same target will drive the pump.
- **Main screen: TURN = change profile, CLICK = config menu.**
- **Targets + live readings on screen**: live boiler temp (big) + setpoint,
  live pressure (big) + bar graph, group temp, shot timer.
- **Config menu**: pick the profile to edit, name editable letter by
  letter with the knob, brew temp, target pressure, steam (global),
  Kp/Ki/Kd, autotune, save.
- **PID AUTOTUNE** (relay + Ziegler-Nichols): tunes the global PID all
  by itself, ~3 oscillation cycles around the setpoint (~5-10 min),
  cancellable with one click. Result saved globally.
- **Boiler PID**: brew / steam setpoints, adjustable in the menu.
- **The ideal architecture: 2 SSRs + 1 DIMMER.** Heater on SSR-40DA
  (D8), 3-way solenoid ALONE on SSR-25DA (D9), pump on RobotDyn dimmer
  (ZC = D3, gate = D11). **The profile's target pressure is REAL**: a
  2nd PID drives the dimmer in a closed loop on the XDB401 sensor.
- **Pre-infusion per profile**: pressure (3 bar) + duration (5 s, 0 = off),
  adjustable in the menu. The solenoid stays open through the whole
  pre-infusion, only the pump runs at low power. Pressure sensor dead →
  full pump (previous behavior).
- **Full profiling per profile**: pre-infusion → **ramp** (3 s, 0 =
  direct) → extraction (target pressure) → **decline** (6 bar from
  X s of extraction, 0 = off). Setpoint interpolated during the ramp.
- **Flow meter = permanent diagnostic** (never the dose reference):
  instant flow (mL/s, 1 s sliding window) + total water volume,
  shown during the shot. Stops if no flow at all for 5 s of extraction
  (flow meter present). The dose in the cup is the load cell's job.
- **Gram mode = dose reference, with learned anticipation.**
  The target is the FINAL weight in the cup. The controller learns
  **tau** (flow-through seconds) per profile, EMA alpha 0.2:
  `cutoff = target - tau × g/s` (g/s measured over a 2 s sliding window).
  After the shot, the scale settles (8 s max), then tau is
  updated and saved to EEPROM. After a few shots, a 36.0 g target
  gives a final weight very close to 36.0 g.
- **Shot screen**: pressure, pump %, flow mL/s, total water mL,
  cup g + target, time, phase (PREINF / RAMPE / BREW / DECLIN / STAB.).
  **Post-shot summary**: final weight, overshoot, learned tau (gram
  mode) or Pmax/Pavg and temperatures (other modes).
- **Safeties**: max pressure 12.5 bar (FAULT, heater + pump cut),
  max time 90 s (all modes, manual included), lost scale signal
  (gram mode → stop "BALANCE HS"), no flow for 5 s (stop
  "PAS DE DEBIT").
- **Everything adjusts on the machine** (no SD, no USB, no WiFi):
  pressure PID gains (P-Kp/P-Ki/P-Kd) and tau editable in the menu;
  history of the last 10 shots viewable on the LCD; pressure curve
  of the last shot shown on screen.
- **Auto shot stop** (menu, global): Manual / Timer / Flow meter /
  Scale. The profile carries the target for the configured mode: duration (27 s),
  volume (40 ml) or weight (36 g). Live on screen: `12/27s`, `18/40ml`,
  `12/36g`. 60 s safety on sensor modes.
- **Stock buttons reused**: BREW = extraction, STEAM = steam mode.
- **Safeties**: boiler probe dead or > 165 °C → heater cut + FAULT screen.
  The stock thermal fuse stays in series with the element: never bypass it.
- **Saving**: profiles + active profile kept in EEPROM.

## Parts list

| Photo | Part | Detail | Got it? |
|---|---|---|---|
| <img src="https://www.canadarobotix.com/cdn/shop/products/266_1_1200x1200.jpg?v=1597251812" width="120"> | Arduino Mega 2560 | the board | ✅ |
| <img src="https://sumeetinstruments.com/image/cache/catalog/3dprinter/ramps-shield-1-4-for%20arduino-mega-2560-550x550.jpg" width="120"> | RAMPS 1.4 | shield that plugs onto the Mega | ✅ |
| <img src="https://ae01.alicdn.com/kf/HTB18tKAXOnrK1RjSsziq6xptpXan.jpg_.webp" width="120"> | 128x64 ST7920 graphic LCD | RepRapDiscount Full Graphic Smart Controller (often included with the RAMPS kit) | ✅ if in your kit |
| — | 12 V power supply, 2 A+ | from your printer (it powers the RAMPS + Mega + SSR) | ✅ |
| <img src="https://www.picclickimg.com/0tEAAOSwe35bcm8K/SSR-40DA-SSR-25DA-SSR-60DA-60A-40A-25A-250V-Solid-State-Relay.webp" width="120"> | SSR-40DA + heatsink | heater (~12 A at 120 V) | to buy |
| <img src="https://i.ebayimg.com/images/g/28AAAOSwEz5leefr/s-l1600.jpg" width="120"> | SSR-25DA | 3-way solenoid ALONE (~0.5 A) | to buy |
| <img src="https://m.media-amazon.com/images/I/51mGNgoZAhL._AC_SL330_.jpg" width="120"> | RobotDyn 1-channel AC dimmer (with zero-cross, 110 V) | pump — phase control, this is what sets the pressure | to buy |
| <img src="https://m.media-amazon.com/images/I/414WS1Kn9AL._SL1000_.jpg" width="120"> | 2× 100k NTC thermistor, beta 3950, **ring terminal (M3/M4 eyelet)** | one on the boiler (PID), one on the group head (display) | to buy |
| <img src="https://cdn.shopify.com/s/files/1/0656/8312/8534/articles/2_e818263d-a737-4bc2-87d1-e662f3a5f971.jpg?v=1710092399" width="120"> | Pressure sensor **XDB401, 0–1.2 MPa, G1/4, 5 V, 0.5–4.5 V** | the Gaggiuino standard — REQUIRED with the dimmer (closed loop) | to buy |
| <img src="https://i5.walmartimages.com/asr/b3447c7f-c707-4b6a-adca-aed2bb31a8b9.bcab65d295d4b05c8982fdd168ebc04e.jpeg" width="120"> | 6 mm tee + 6 mm barb × 1/4" fitting + HP hose + clamps | to connect the sensor to the pump → boiler hose | to buy |
| <img src="https://m.media-amazon.com/images/I/81x4vWCnF3L.jpg" width="120"> | Wire, crimp terminals, heat shrink, PTFE tape | | to buy |

Photos: seller product images (external links), to identify each
part — they are not the exact parts you will receive.

**The 3rd thermistor (T2) is optional**: steam is read from the boiler
probe, only the setpoint changes. Two thermistors are enough.

**There is no pump SSR anymore**: with the dimmer, the pump has no
SSR — the dimmer cuts the 120 V directly (phase control). Both
SSRs serve the heater and the solenoid.

## The pressure sensor — the choice

**XDB401, 0–1.2 MPa (0–12 bar), G1/4 male thread, 5 V supply,
0.5–4.5 V output, 3 wires: red = 5 V, black = GND, yellow = signal.**
It's the Gaggiuino project's sensor: easy to find (~US$15–25 on
AliExpress/eBay, search "XDB401 1.2MPa G1/4 0.5-4.5V"), cheap,
and it screws onto a standard tee.

**Installation**: on the braided orange hose between the pump outlet
and the boiler inlet, via a 6 mm tee + 6 mm barb × 1/4" fitting.
The thread may come in NPT or BSP depending on the seller — check
before ordering the fittings, and use PTFE tape. Leak-test cold
before powering back up: at 9 bar, there's no room for error.

**Software side**: 0.5 V = 0 bar, 4.5 V = 12 bar. If the sensor is
unplugged, the screen shows `--` but the machine keeps working
(pressure is display-only in v1, it blocks nothing).

## Wiring (RAMPS 1.4)

| Function | RAMPS pin | Connection |
|---|---|---|
| Heater SSR | D8 (bed MOSFET) | D8 output → SSR-40DA (+/−), SSR → 120 V element (thermal fuse in series, intact) |
| Solenoid SSR | D9 (fan MOSFET 1) | D9 output → SSR-25DA, SSR → 120 V 3-way solenoid ALONE |
| Pump dimmer | D3 (ZC) + D11 (gate) | RobotDyn: ZC → D3, gate → D11, VCC 5 V, GND; AC side: dimmer in series with the 120 V pump |
| Boiler thermistor | T0 (A13) | ring terminal screwed onto the boiler |
| Group thermistor | T1 (A14) | ring terminal screwed onto the group head |
| Pressure sensor | AUX-2, **A10** | red 5 V, black GND, yellow → A10 |
| Stock BREW button | D19 | button contacts → D19 + GND (120 V wires disconnected) |
| Stock STEAM button | D18 | button contacts → D18 + GND (120 V wires disconnected) |
| 12864 LCD | EXP1 / EXP2 | as on the printer (SPI: MOSI 51, SCK 52, CS 23, RESET 17) |
| LCD encoder | EXP1 | A=31, B=33, button=35 (RAMPS standard) |

**The RAMPS MOSFETs only switch 12/24 V DC**: they drive the SSRs'
DC input (3–32 V), and the SSRs are what cut the 120 V.
Never wire 120 V directly to the RAMPS.

**BREW button**: the code accepts a latching switch (the Pro's
button) AND a momentary button (each press toggles on/off).

## Flashing

1. Arduino IDE → install the **U8g2** library (Library Manager).
2. Open `gaggia-ramps.ino`, board **Arduino Mega 2560**, right COM port.
3. Plug the Mega in via USB (RAMPS powered at 12 V), upload.
4. If the screen stays blank: adjust the contrast potentiometer on the
   back of the LCD. Last resort: the software version of the U8g2
   constructor is commented in the code.

## Usage

- Power on: the PID climbs to the active profile's setpoint, the screen
  says PRET when it's stable (±1 °C).
- **Turn the knob (main screen): change profile (P1→P5).**
- **Click (main screen): open the config menu.**
- BREW: extraction, timer + pressure live. Click again to stop
  (or release if your button is latching).
- STEAM held down: steam setpoint, steam as usual (the valve knob
  stays manual).

### Config menu (turn = select, click = enter)

| Item | Action |
|---|---|
| Profile | turn while editing = change the profile being edited (P1–P5) |
| Name | click = letter-by-letter edit (turn = letter, click = next character, after the 10th = back) |
| Brew temp | 85–100 °C, 0.5 steps |
| Pressure | 6–12 bar, 0.5 steps (profile target) |
| Auto stop | **Manual / Timer / Flow meter / Scale** (global mode, see below) |
| Shot target | the target for the configured mode: **Duration** 10–60 s, **Volume** 10–120 ml, **Weight** 10–120 g (manual = no target) |
| Pre-inf bar | 1–6 bar, 0.5 steps (profile pre-infusion pressure) |
| Pre-inf s | 0–10 s, 1 s steps (pre-infusion duration, 0 = disabled) |
| Ramp s | 0–10 s, 1 s steps (pre-infusion → target pressure climb, 0 = direct) |
| Decline bar | 1–12 bar, 0.5 steps (end-of-shot pressure) |
| Decline s | 0–60 s, 1 s steps (extraction seconds before the decline, 0 = off) |
| Tau | 0–2.00 s, 0.05 steps (gram-mode anticipation, see below) |
| P-Kp / P-Ki / P-Kd | pressure PID gains (15 / 1 / 0 by default, see below) |
| History | screen: the last 10 shots (turn = navigate) |
| Curve | screen: pressure curve of the last shot (target as a dashed line) |
| Steam (global) | 130–155 °C, 1 °C steps |
| Kp / Ki / Kd | manual tuning of the **global** PID (applies to all profiles) |
| PID autotune | click = run autotune (see below) |
| Save | writes everything to EEPROM and exits the menu |

10 s timeout without touching the knob = exit the menu without saving
(changes stay in RAM until the next reboot).

### Auto shot stop — 3 modes (menu "Auto stop", global)

The mode is picked in the menu; the profile then shows the target for the
configured mode (the "Shot target" changes its label and unit by itself).
During the shot, the screen shows progress: `12/27s`, `18/40ml`
or `12/36g`. Pressing BREW during an auto shot stops it before the target.

| Mode | What it needs | How it works |
|---|---|---|
| **Manual** (default) | nothing | BREW starts/stops, you control the volume |
| **Timer** | nothing | the pump stops after the profile's duration (10–60 s) |
| **Flow meter** | hall flow meter on D2 | counts pulses, stops at the profile's ml |
| **Scale** | load cell + HX711 (D4/D5) | weighs the coffee in the cup, stops at the profile's grams |

- **Flow meter**: signal on **D2** (interrupt INT0, `INPUT_PULLUP` —
  the hall sensor is open-collector). Constant `FLOW_PULSES_PER_ML`
  (default 1.0 pulses/ml) **to calibrate**: run 100 ml of water (pump
  alone, no coffee), note the pulses on serial, divide by 100.
  A YF-S201 (~0.45 pulses/ml) is too coarse for 40 ml — prefer an
  espresso flow meter (~1–2 pulses/ml).
- **Scale**: load cell under the spout + HX711, **DOUT = D4, SCK = D5**,
  bit-banged read (gain 128). The firmware **tares when the shot starts**:
  the cup must be in place before pressing BREW.
  Constant `HX711_COUNTS_PER_G` (default 100) **to calibrate**: put a
  known weight on (e.g. 100 g), read the raw value on serial, divide by the weight.
- **Safety**: in Flow meter / Scale modes, forced stop at **60 s** if
  the sensor doesn't respond (unplugged, broken). **Don't enable a mode
  without its hardware installed.**

### Dimmer + pre-infusion — how it works

The dimmer chops each half-wave of the 120 V (phase control): the pump
runs continuously, just weaker — pressure holds steady instead of
oscillating. A 2nd PID (`pidPress`, Kp=15, Ki=1, Kd=0 by default) reads
the XDB401 sensor and adjusts the dimmer level (0–100%) to hold the
target: pre-infusion pressure during the pre-infusion phase, then the
profile's target pressure during extraction. **These gains are starting
values: pressure responds fast, expect to tune Kp (higher = stronger
reaction, too high = oscillation).** If the sensor is dead, the firmware
sets the dimmer to 100% (full pump).

A shot runs like this: BREW → solenoid open + pre-infusion at the
profile's pressure (default 3 bar, 5 s) → ramp to the target pressure
(default 3 s) → extraction at target pressure (9 bar) → decline to the
end pressure (default off, e.g. 6 bar from 20 s of extraction) →
auto stop (timer/ml/grams) or BREW → dimmer to 0 + solenoid
closed (the group vents). The dimmer's minimum level is clamped
at 25% (`DIM_MIN_LEVEL`): below that, the vibratory pump stalls.

### Flow meter — diagnostic, not dose

The flow meter is installed permanently and measured on every shot, but it
never decides the dose: it measures the pump's water flow.
The screen shows instant flow (mL/s, 1 s sliding window) and total
water volume — useful to see if flow collapses at the end of a shot or
if the pump has a problem. Safety: if the flow meter is present (at least
one pulse seen) and nothing flows for 5 s of extraction, the shot stops
("PAS DE DEBIT"). With no flow meter plugged in, this safety is inactive
(D2 pulled to +5 V, no pulses).

### Gram mode — learned anticipation (tau)

The target is the **final weight in the cup**, not the weight when the
pump stops: after the cutoff, water keeps running through the puck.
The controller learns **tau** (flow-through seconds) per profile:

- During the shot: `cutoff = target - tau × g/s` (g/s = rate of weight
  increase, measured over a 2 s sliding window; anticipation capped at 5 g max). If the shot flows fast, it cuts earlier.
- After the shot: scale settling (8 s max, stable if the weight moves
  less than 0.05 g for 1 s). Final weight measured →
  `overshoot = final - weight at cutoff` → `tau measured = overshoot / g/s`
  → `tau = 0.8 × tau + 0.2 × tau measured` (EMA). Tau is saved to EEPROM
  after every shot. Starting value: 0.4 s.
- The scale tares when the shot starts: **the cup must be in place
  before pressing BREW**. Pressing BREW during settling starts a new shot
  (the tare will be wrong — wait for settling to finish, max 8 s).
- The post-shot summary shows final weight, overshoot and learned tau:
  after a few shots, 36.0 g targeted = 36.0 g obtained.
- **Learning protection**: tau is only updated if the shot ended normally
  in gram mode. Excluded: manual stop, FAULT, safety stop (no flow,
  scale fault, 90 s), unstable scale (settling timeout), aberrant final
  weight (overshoot < 0 g or > 4 g). Otherwise the summary shows
  "Tau non appris".

### Flow meter — diagnostics (warning only)

Beyond instant flow and total volume, the flow meter diagnoses the puck —
**without ever stopping a shot for it** (a very fine grind legitimately
gives low flow):

| Condition (sustained 5 s, in extraction) | Warning shown |
|---|---|
| Pressure ≥ 8 bar + flow < 0.5 mL/s | `RESTRICTIF` (very restrictive puck) |
| Flow > 3 mL/s + pressure < 4 bar | `PERMISSIF` (too permissive puck / leak) |
| Pump commanded + flow ≈ 0 | `PAS DE DEBIT` → **stop** (empty tank, clog, pump unprimed) |

Warnings show on the shot screen (in place of the phase) and on the
summary. Starting thresholds — tune with real logs.

### Serial CSV logs — tuning on real data

**Everything adjusts on the machine, without plugging anything in.** The LCD is
the main interface:

- **Menu → History**: the last 10 shots, persisted in EEPROM
  (survive a power cycle). Each shot: duration, Pmax/Pavg, total
  water, final weight, overshoot, tau. Turn = navigate, click = back.
- **Menu → Curve**: pressure curve of the last shot (sampled every
  250 ms), with the profile target as a dashed line — this is the
  screen for tuning the pressure PID: overshoot, oscillation,
  rise time, it's all visible there.
- **Menu → P-Kp / P-Ki / P-Kd**: pressure PID gains, editable
  directly (saved with "Save"). Starting point: Kp=15, Ki=1,
  Kd=0. Pressure responds fast: raise Kp if pressure climbs too
  slowly, lower it if it oscillates; a bit of Ki to fix residual error.
- **Menu → Tau**: see and force the active profile's learned tau
  (0–2.00 s). Automatic learning keeps updating it after each normal shot.

The serial CSV log is still available as an option (plug in USB, monitor
at 115200 baud): one line every 250 ms during the shot, otherwise
1/s. Header at startup:

```text
ms,profil,tBoilerC,pBar,pTarget,dimLevel,flowMlS,waterMl,cupG,phase,shotS
```

`phase`: 0=idle, 1=pre-infusion, 2=ramp, 3=extraction, 4=decline.
Copy 5 to 10 shots into a spreadsheet: it shows the dimmer's response
(ULKA + pressure loop) and the real overshoot, to tune the pressure PID's
Kp and validate tau. Future idea (not before real testing): learn
tau as a function of the final flow rate in g/s instead of a constant per profile.

### PID autotune — how it works

1. Go to the profile to tune (main screen, turn) — the autotune's
   setpoint will be that profile's brew temp, but the result (Kp/Ki/Kd) is
   **global**: it applies to all profiles.
2. Menu → PID autotune → click. The boiler should be close to the
   setpoint (let it settle first, otherwise the 1st cycle is noise —
   the code ignores it anyway).
3. The firmware oscillates the boiler around the setpoint in on/off
   mode (relay 0/100%) and measures 3 cycles: period Tu and
   amplitude. Ziegler-Nichols method: Kp = 0.6·Ku, Ki = 2·Kp/Tu,
   Kd = Kp·Tu/8.
4. Result shown (Kp/Ki/Kd) and saved (global). Click
   during autotune = cancel (old settings kept). Timeout
   10 min = cancelled.
5. If the result feels aggressive (overshoot), halve Ki —
   that's the classic Z-N tuning, a bit "hot" for a boiler.

Debug: the serial port prints a CSV line every 500 ms
(`ms, profil, Tchaud, Tgroupe, Pbar, consigne, PID%, etat, timer`) —
handy to see the autotune oscillations.

## The two "elements" — your question

The Classic has only **ONE heating element**. The two stock thermostats
(brew 107 °C / steam ~140 °C) drive the same element —
they're just two thresholds. One SSR for the heater, then.

- **The 2 thermistors: yes, used together** — boiler = PID,
  group = display (and a reference for temp surfing if you want).
- **The 2 SSRs: yes, active together during a shot** — heater (PID)
  + pump at the same time.

## Phase 2 (later)

- **RobotDyn 4 A dimmer** on the pump (D9 → dimmer instead of the SSR)
  = pressure profiling with the sensor as the closed loop.
- **RAMPS stepper drivers** = rotary pump (NEMA 23 + beefy external
  driver — an A4988 is borderline, see discussion).
- **Load cell under the drip tray** = live shot weighing.
