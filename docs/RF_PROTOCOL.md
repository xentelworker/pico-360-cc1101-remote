# RF Protocol

This document records the measured protocol from the factory 360 photo booth remote used during development.

## Frequency

The factory remote PCB used a SAW resonator marked `R315`. CC1101 scanning and fixed-frequency capture confirmed operation at approximately 315 MHz. The working transmitter uses:

```text
315.000 MHz
```

## Modulation

```text
ASK / OOK
```

The CC1101 is used in asynchronous serial mode so the Pico directly drives the OOK waveform through GDO0 during TX and receives the demodulated waveform through GDO0 during RX/sniffing.

## Frame format

```text
24 data bits, MSB first
followed by sync/end pulse
```

Measured bit timing:

```text
Logical 0:
HIGH ~335 us
LOW  ~1122 us

Logical 1:
HIGH ~1070 us
LOW  ~397 us

Sync / frame gap:
HIGH ~335 us
LOW  ~11280 us
```

Each normal bit cell is approximately 1.46 ms.

## Verified commands

| Function | Binary | Decimal | Hex |
|---|---|---:|---:|
| ON/OFF | `000111111111101001001000` | 2095688 | `0x1FFA48` |
| REVERSE | `000111111111101001000100` | 2095684 | `0x1FFA44` |
| SPEED+ | `000111111111101001000010` | 2095682 | `0x1FFA42` |
| SPEED- | `000111111111101001000001` | 2095681 | `0x1FFA41` |

## Repetition

The factory remote repeats frames while a button is held.

The tested controller uses:

```text
ON/OFF:  12 frames per press
REVERSE: 20 frames per press
SPEED+:  20 frames per press
SPEED-:  20 frames per press
KILL RF: 20 ON/OFF frames
```

The larger repeat count on reverse and speed controls improved reliability in real use.

## CC1101 transmit details

A key detail for OOK transmission is the PA table configuration. The working setup uses `FREND0 = 0x11`, meaning the active OOK level references PA table index 1. The table is written as:

```text
PATABLE[0] = 0x00  RF off
PATABLE[1] = 0x51  RF on, approximately 0 dBm near 315 MHz
```

Another critical detail is the explicit sync pulse after all 24 data bits. Sending only the 24 bits followed by a long LOW did not reproduce the receiver behavior reliably. Adding the measured short-HIGH / long-LOW sync made the cloned remote work.

## Using the included sniffer

The sniffer looks for:

```text
short HIGH + 8-14 ms LOW
```

and then attempts to decode the following 24 HIGH/LOW pairs using these windows:

```text
0 = HIGH 250-500 us + LOW 850-1300 us
1 = HIGH 850-1250 us + LOW 250-550 us
```

These thresholds are deliberately wider than the measured averages to tolerate normal RF and timer variation.

If another remote uses a different protocol, frequency, modulation, bit count, or timing, these values must be adjusted.
