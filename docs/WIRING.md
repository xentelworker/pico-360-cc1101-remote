# Wiring

## Raspberry Pi Pico to CC1101

The tested hardware uses an EBYTE E07-M1101D-SMA / CC1101 module.

| CC1101 pin | Function | Raspberry Pi Pico |
|---|---|---|
| 1 | GND | GND |
| 2 | VCC | 3V3 |
| 3 | GDO0 | GP20 |
| 4 | CSN | GP17 |
| 5 | SCK | GP18 |
| 6 | MOSI | GP19 |
| 7 | MISO/GDO1 | GP16 |
| 8 | GDO2 | optional / unused |

The CC1101 is a 3.3V-class device. **Do not connect VCC to Pico VBUS or 5V.**

## Button wiring

Each physical button is wired directly between a Pico GPIO and GND. The sketch enables `INPUT_PULLUP`, so:

- released = HIGH
- pressed = LOW

| Function | Pico pin |
|---|---|
| KILL | GP2 |
| ON/OFF | GP3 |
| REVERSE | GP4 |
| SPEED+ | GP5 |
| SPEED- | GP6 |

Example:

```text
GP3 -------- pushbutton -------- GND
```

Repeat the same arrangement for GP2, GP4, GP5, and GP6.

## USB connection

Connect the Raspberry Pi Pico to the Windows computer using its normal USB data cable.

The controller sketch uses USB HID keyboard support so GP2 can send the Escape key to Windows while the same Pico also controls the CC1101 radio.

## Antenna

Use an antenna intended for approximately 315 MHz on the CC1101 module. The factory remote that was reverse engineered used a resonator marked `R315`, which led to the confirmed 315 MHz operating frequency.

## Safety

The RF ON/OFF command is a toggle. The GP2 KILL button sends that toggle as an operational stop and also sends Escape to Windows. This is not equivalent to a hardwired machinery emergency-stop circuit.
