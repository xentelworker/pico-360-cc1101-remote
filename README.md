# 360 Booth Remote Clone - Pico CC1101 Remote

Raspberry Pi Pico replacement remote for a 315 MHz 360 photo booth controller using a CC1101 radio module.

This project includes:

- a working Pico + CC1101 controller
- physical buttons for ON/OFF, reverse, speed up, and speed down
- a GP2 kill button that sends the booth stop/toggle RF command and an ESC key to Windows for dslrBooth cancellation
- USB serial commands so a Windows app can control every function
- a native Windows WPF control frontend with auto COM-port detection
- a raw 315 MHz CC1101 sniffer / auto-decoder for discovering compatible 24-bit remote codes
- wiring and RF protocol notes

## Windows software controller

A native Windows frontend is included under:

```text
windows-app/
```

It provides large controls for:

- ON / OFF
- REVERSE
- SPEED +
- SPEED -
- STOP / KILL
- COM-port selection
- automatic Pico detection
- connection status
- command/activity logging

SPEED+ and SPEED- support press-and-hold repeating. The app communicates with the Pico over its USB serial connection at 115200 baud.

Open:

```text
windows-app/Pico360Controller.sln
```

with Visual Studio 2022 and the .NET desktop development workload installed.

Full setup and publishing instructions are in `docs/WINDOWS_APP.md`.

## USB serial protocol

The current Pico firmware accepts:

```text
PING
STATUS
ONOFF
REVERSE
SPEED_UP
SPEED_DOWN
KILL
```

The same firmware continues to support all physical buttons, so hardware and software controls can be used together.

## Verified remote codes

| Function | Decimal | Hex |
|---|---:|---:|
| ON/OFF | 2095688 | `0x1FFA48` |
| REVERSE | 2095684 | `0x1FFA44` |
| SPEED+ | 2095682 | `0x1FFA42` |
| SPEED- | 2095681 | `0x1FFA41` |

## Verified RF protocol

- Frequency: **315.000 MHz**
- Modulation: **ASK/OOK**
- Payload: **24 bits**
- Bit order: **MSB first**
- Logical 0: HIGH ~335 us, LOW ~1122 us
- Logical 1: HIGH ~1070 us, LOW ~397 us
- Sync/end: HIGH ~335 us, LOW ~11280 us

The controller transmits the ON/OFF command 12 times per press. Reverse and speed controls use 20 repeats for improved reliability.

## Wiring

### CC1101 to Raspberry Pi Pico

| CC1101 | Pico |
|---|---|
| GND | GND |
| VCC | 3V3 |
| GDO0 | GP20 |
| CSN | GP17 |
| SCK | GP18 |
| MOSI | GP19 |
| MISO/GDO1 | GP16 |
| GDO2 | optional / unused |

**Do not power the CC1101 from 5V or VBUS. Use Pico 3.3V only.**

### Buttons

Each pushbutton connects between the GPIO pin and GND. The sketches use `INPUT_PULLUP`.

| Function | Pico pin |
|---|---|
| KILL | GP2 |
| ON/OFF | GP3 |
| REVERSE | GP4 |
| SPEED+ | GP5 |
| SPEED- | GP6 |

## Arduino setup

The project was developed with the Earle Philhower Raspberry Pi Pico Arduino core and uses the built-in `SPI` object with:

```cpp
SPI.setRX(16);
SPI.setCS(17);
SPI.setSCK(18);
SPI.setTX(19);
SPI.begin();
```

The controller also includes `Keyboard.h` so the Pico can send ESC to Windows when GP2 or the Windows app triggers KILL.

## Files

- `controller/Pico_360_CC1101_Controller.ino` - working controller with physical buttons, USB serial control, and HID kill behavior
- `sniffer/CC1101_315MHz_AutoDecoder.ino` - raw pulse sniffer and 24-bit auto-decoder
- `windows-app/Pico360Controller.sln` - Visual Studio solution for the Windows frontend
- `docs/WINDOWS_APP.md` - Windows app setup, build, publish, and troubleshooting guide
- `docs/WIRING.md` - detailed wiring
- `docs/RF_PROTOCOL.md` - protocol notes and decoded values

## Using the sniffer with another remote

Upload the sniffer sketch, open Serial Monitor at 115200 baud, and press a button on the original 315 MHz remote. The decoder looks for the measured sync gap and then decodes the following 24 pulse pairs. When a valid frame is found it prints the binary, decimal, and hexadecimal value.

If your remote uses different timing, frequency, modulation, or bit length, adjust the thresholds in the sniffer.

## Safety note

The original receiver uses a toggle-style ON/OFF RF command. The GP2 kill function and Windows STOP/KILL button send that same RF command and an ESC key to Windows. This is an **operational stop/cancel feature**, not a safety-rated emergency stop.

Do not rely on RF, USB HID, software, or this project as the sole emergency-stop mechanism for machinery. Use a proper hardwired safety circuit for any application where injury could occur.

## Links to parts

Official Raspberry Pi Pico Board RP2040 Dual-Core 264KB ARM Low-Power Microcomputers High-Performance Cortex-M0+ Processor
https://www.aliexpress.com/item/1005005617180169.html?spm=a2g0o.order_list.order_list_main.55.c5ce1802mgAlZP

CC1101 Wireless Module With SMA Antenna Wireless Transceiver Module 433MHZ
https://www.aliexpress.com/item/1005005200440286.html?spm=a2g0o.order_list.order_list_main.70.c5ce1802mgAlZP

Note: I originally thought my RF remote used 433Mhz but it ended up being 315Mhz. Thankfully this transciever supported both frequencies.

## License

MIT License. See `LICENSE`.
