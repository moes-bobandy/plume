# Changelog

All notable changes to Plume are recorded here.

## v1.1

Charge Mode reliability. Every item below is in `run_charge_mode()` or its boot
gate unless noted.

### Fixed

- **Charge screen could freeze for 30s and panic-reboot.** The wait-for-release
  loop on the exit path spun without feeding the task watchdog. On the ADV the
  key list is built from TCA8418 press/release *events* and never rescanned, so
  a lost release event latches a key down permanently and that loop never
  exited. It is now time-boxed to 3s and resets the WDT each pass.
- **Quick taps on the charge screen were ignored.** `update()` drains exactly
  one TCA8418 event per call, so a tap reads as pressed on one frame and
  released on the next — the old two-frame debounce discarded it and forced the
  user to hold a key. Any observed press now exits. The debounce was inherited
  from the original Cardputer's scanned matrix and does not apply to the ADV's
  hardware-debounced controller.
- **Nothing detected "not actually charging".** With no charge-status line and
  `isCharging()` always returning unknown, a dead cable or an unplug left the
  screen lit and draining the cell it was meant to fill, ending in a brownout
  that re-entered Charge Mode and repeated until the pack was deeply
  discharged. A stall watch now tracks the running peak; a 40mV fall sustained
  for 5 minutes shows a NOT CHARGING notice and powers the panel down, a 40mV
  recovery restores the readout, and reaching 3250mV enters deep sleep (~20uA)
  with keyboard wake armed. A deep-sleep wake is not a brownout reset, which is
  what breaks the drain/reboot cycle.
- **A failed battery ADC init caused an unbreakable boot loop.** A failed
  `adc_oneshot` init returns 0mV, which the boot gate read as an empty cell and
  gated into Charge Mode, which rebooted ~6s later, forever. An implausible
  reading is now treated as sensor failure: the boot gate skips the voltage
  criterion and Charge Mode resumes the app instead of rebooting.

### Changed

- The charge-loop health check no longer claims to detect I2C wedges. On the
  ADV the battery is read from ADC1/GPIO10 with no I2C involved, while the
  keyboard's TCA8418 is the only device on the bus — so the check could never
  see a keyboard failure. It is now scoped and documented as an ADC-frozen
  detector, with the implausible-read case split out and handled separately.
- The export page version badge derives from `VERSION_SHORT` instead of being
  hardcoded.

### Known issues

- A keyboard that fails to initialize at boot (`Adafruit_TCA8418::begin()`
  returning false on a marginal rail) is silently dead for the whole session —
  no ISR is attached, so no key ever registers. Not yet detected or retried.
- After a Charge Mode exit the CPU stays at 40MHz until radio init rather than
  returning to the 80MHz the boot path intends. Slower boot; not a safety
  issue, since a lower clock draws less peak current.
- The charge screen maps voltage through the discharge curve while charging, so
  the percentage reads optimistically high, and `CHARGE_MODE_FULL_MV` (4150mV)
  tops out at 95%.
- The +/-mV figure is cumulative since Charge Mode started, so it cannot show
  that charging has resumed after an earlier discharge, and it renders at 1mV
  precision despite a noise floor several times larger.

## v1.0-beta

Initial public beta.
