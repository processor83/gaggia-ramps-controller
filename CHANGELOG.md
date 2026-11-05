# Changelog — Gaggia Classic Pro PID (RAMPS 1.4)

Version history for the `gaggia-ramps.ino` firmware. Every firmware change
is published here before (or with) the push to GitHub.

Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

- Documentation translated to English (README, BOM, CHANGELOG).
- Part photos added to the "Parts list" table in the README.
- BOM correction pending (not applied): the XDB401 sensor is
  $19.28 on AliExpress in the 11.11 sale until Nov 18, 2026, vs
  $29.37 in the BOM — the required total (~$62 CAD) would be ~$10 lower
  at that price. Apply only after a decision.

## [0.1.0] — 2026-11-05

Initial version published on GitHub. Compiled firmware (4,826 bytes of SRAM,
~59% of the Mega; 47.0 KB of flash). Hardware: Arduino Mega 2560 + RAMPS 1.4 +
128x64 ST7920 graphic LCD, 2 SSRs, 2 100k NTC thermistors, XDB401
0–1.2 MPa G1/4 0.5–4.5 V pressure sensor.

### Added

- 5 profiles (name + brew temp + target pressure each), saved to EEPROM;
  global steam temperature (145 °C) and global PID (one Kp/Ki/Kd set).
- 2 SSR + 1 RobotDyn dimmer architecture: heater SSR-40DA (D8),
  3-way solenoid alone on SSR-25DA (D9), pump on dimmer (ZC D3, gate D11)
  — the profile's target pressure becomes real via a 2nd PID (15/1/0).
- Pre-infusion per profile (3 bar, 5 s, 0 = off), ramp (3 s) and decline
  (6 bar); target pressure per profile (9 bar by default).
- Auto shot stop, global mode in the menu: Manual, Timer (27 s),
  Flow meter (40 ml), Scale (36 g).
- Gram mode with learned anticipation (tau, EMA): cutoff at
  `target − tau × g/s`, tau updated only after a normal shot.
- Flow meter = permanent diagnostic (flow mL/s + total volume);
  stop if no flow at all for 5 s of extraction.
- PID autotune (relay + Ziegler-Nichols), cancellable with one click.
- History of the last 10 shots in EEPROM + pressure curve of the
  last shot on the LCD; pressure PID gains and tau adjustable in the menu.
- Shot screen (pressure, pump %, flow, water, cup + target, time,
  phase) and post-shot summary (final weight, overshoot, tau).
- Safeties: 12.5 bar max (FAULT), 90 s max, "BALANCE HS",
  "PAS DE DEBIT"; boiler probe dead or > 165 °C → heater cut.
- Restrictive / permissive puck diagnostics as warnings only.
- Serial CSV logs (115200 baud) for tuning on real data.

### Notes

- Flow meter and load cell not purchased: Flow meter / Scale modes
  only activate with the hardware installed.
- The original RobotDyn dimmer is discontinued: only clones
  exist (US$7–10), functionally identical.
- BOM: ~$62 CAD required (+ GST ≈ $65), ~$67 with optionals
  (see `BOM.md`, AliExpress prices of Nov 5, 2026).
