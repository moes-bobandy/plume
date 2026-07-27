# Plume ⇄ C5 wire protocol

The Cardputer (ESP32-S3, runs Plume) and the ESP32-C5 (runs `c5-sniffer/`) are
two independent processors connected by a single UART over the Grove port. This
file is the **single source of truth** for the line format they exchange. Both
sides must agree on it.

**Current `PROTOCOL_VERSION`: 1**

## Link

| | |
|---|---|
| Transport | UART, 115200 8N1, newline (`\n`) terminated ASCII |
| Cardputer side | UART1 on Grove `G1`/`G2` (GPIO1 = S3 TX, GPIO2 = S3 RX) — see `c5_link.h` |
| C5 side | UART0 on the `TXD`/`RXD` pads — see `c5-sniffer.ino` |
| Direction | **Bidirectional.** C5 → Cardputer carries detections, ambient sightings and heartbeats; Cardputer → C5 carries time sync, radio power state, LED mirroring and the signature table |
| Orientation | Auto-detected. Plume probes both Grove data pins receive-only, latches whichever one carries valid protocol traffic, and only then configures a TX pin — so a reversed yellow/white cable works, and Plume never drives a line that may already be an output |
| Levels | 3.3 V both ends, no level shifter |

## Messages (C5 → Cardputer)

### Detection
```
D|mac|name|rssi|ch|conf|methods
```
| Field | Meaning |
|---|---|
| `mac` | lower-case `aa:bb:cc:dd:ee:ff` of the reported device |
| `name` | SSID, or `Hidden`; any `\|` / newline is replaced with `_` |
| `rssi` | signed dBm, e.g. `-67` |
| `ch` | 5 GHz channel (36–165) — the band marker on the Plume side |
| `conf` | confidence 0–100 |
| `methods` | space-separated match tags (last field; may contain spaces) |

Example: `D|aa:bb:cc:dd:ee:ff|flock-1a2b|-67|149|85|ssid_fmt`

### Hello / heartbeat
```
H|fw|ver
```
Sent every ~3 s. `fw` is a firmware id (`plume-c5`), `ver` is `PROTOCOL_VERSION`.
Plume uses any well-formed line as proof-of-life and lights the **5G** badge for
8 s after the last one. `ver` lets Plume warn on a protocol mismatch.

### Ambient sighting
```
F|mac|name|rssi|ch
```
Every 5 GHz device seen, whether or not it matched a signature — this is what
populates the scanner feed with 5 GHz traffic. Same escaping as `D|`: any `\|`,
`\r` or `\n` in `name` becomes `_`, and Plume scrubs it again on receipt.
Rate-limited on the C5 side (one per ~1.2 s) so ambient traffic cannot crowd out
detections.

Example: `F|aa:bb:cc:dd:ee:ff|somebody-wifi|-71|44`

## Messages (Cardputer → C5)

Sent only once the orientation probe has latched — see **Orientation** above. All
four are fire-and-forget: the C5 latches the last value it received and there are
no acknowledgements, so a dropped line costs at most one update cycle.

### Time sync
```
T|epoch
```
UTC Unix seconds, pushed when Plume has a GPS fix. The C5 has no clock of its
own; it stores this as a base plus a `millis()` offset so its own log lines can
carry real timestamps. Values at or below `1500000000` are ignored as
implausible.

### Radio power state
```
P|idle
```
`idle` is `1` to park the 5 GHz radio, `0` for full scanning. Sent on change, and
on link teardown so a C5 does not keep sniffing at full power into a closed port.
Low-power mode uses this to idle the C5 while deliberately leaving the link up.

### LED mirror
```
L|on|r|g|b
```
`on` is `0`/`1`; `r`/`g`/`b` are `0`–`255` and are clamped on the C5 side. Mirrors
the Cardputer's own LED state so the pair reads as one instrument. Sent on change
only.

### Signature push
```
SB
SO|prefix|tier
SS|pattern
SE|oui_count|ssid_count
```
A whole-table replacement, sent when a C5 first reports in. `SB` begins the
transaction and clears the C5's staging buffers; `SO|` carries one OUI prefix
(`aa:bb:cc` form) with its tier (`1` specific, `2` generic); `SS|` carries one
SSID pattern; `SE|` commits and states how many of each were sent so the C5 can
detect a short transfer. The C5 ignores `SO|`/`SS|`/`SE|` unless an `SB` opened
the transaction, so a truncated push cannot half-apply.

## Rules for changing this protocol

1. **Append-only.** New fields go on the **end** of a `D|` line. Plume reads the
   first 7 fields and ignores extras, so a newer C5 stays compatible with an
   older Plume.
2. **Bump `PROTOCOL_VERSION`** in *both* `c5-sniffer.ino` and `c5_link.h` only
   when the format changes in a non-append-only way.
3. **Keep the detection logic in sync.** These must match between
   `c5-sniffer.ino` and `FlockDetection_Cardputer_ADV.ino`:
   - `kSsidPatterns` / `wifi_ssid_patterns`
   - `kMacTier1` / `mac_prefixes_tier1`, `kMacTier2` / `mac_prefixes_tier2`
   - scoring weights (`SCORE_*`), `ALARM_THRESHOLD`, `IGNORE_WEAK_RSSI`
   - the `Flock-XXXX` format and `test_flck` / wildcard-probe / addr1 rules

## Repo layout

```
Plume/
  FlockDetection_Cardputer_ADV.ino   ← Cardputer/S3 firmware (the brain + UI)
  ui_beep.h
  PROTOCOL.md                         ← this file
  c5-sniffer/
    c5-sniffer.ino                    ← C5 firmware (5 GHz radio ear)
```
