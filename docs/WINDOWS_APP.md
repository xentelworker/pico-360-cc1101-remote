# Windows Controller App

The `windows-app` folder contains a native Windows WPF frontend for controlling the Raspberry Pi Pico over USB serial.

## Features

- Auto-detects the Pico by sending `PING` and looking for a `PICO360` response
- Manual COM-port selection and connect/disconnect
- ON/OFF, REVERSE, SPEED+, SPEED-, and STOP/KILL controls
- SPEED+ and SPEED- repeat while held
- Activity log showing commands sent and responses from the Pico
- Large buttons suitable for touchscreen use
- Local dslrBooth webhook listener on `http://127.0.0.1:8000/`
- Forwards live dslrBooth session state to the Pico OLED
- Converts the dslrBooth countdown start value into a visible second-by-second OLED countdown
- Sends a periodic `PING` heartbeat so the OLED can show PC connection status

## Required Pico firmware

Use:

`controller/Pico_360_CC1101_Controller.ino`

The current firmware accepts these USB serial commands at 115200 baud:

```text
PING
STATUS
ONOFF
REVERSE
SPEED_UP
SPEED_DOWN
KILL
DSLR_SESSION_START
DSLR_COUNTDOWN 10
DSLR_GO
DSLR_PROCESSING
DSLR_SHARING
DSLR_SESSION_END
```

`PING` replies with:

```text
PICO360 READY
```

The Windows app uses that reply for auto-detection.

## OLED wiring

The tested 128x64 SSD1306 I2C OLED uses address `0x3C`.

```text
OLED        Raspberry Pi Pico
------------------------------
VCC   ->    3V3(OUT)
GND   ->    GND
SDA   ->    GP14
SCL   ->    GP15
```

Do not power the OLED from `3V3_EN`; that pin is the Pico regulator enable input, not a 3.3 V supply output.

The firmware requires:

- Adafruit SSD1306
- Adafruit GFX Library
- Adafruit BusIO

Install the Adafruit SSD1306 library with all dependencies from Arduino Library Manager.

## dslrBooth trigger setup

The Windows controller listens locally for dslrBooth URL triggers.

In dslrBooth / LumaBooth for Windows:

1. Open **Settings > General > Triggers**.
2. Choose the URL/webhook trigger option.
3. Set the trigger URL to:

```text
http://127.0.0.1:8000/
```

4. In the dslrBooth event/capture settings, set the desired countdown before capture to **10 seconds**.
5. Keep the Pico 360 Windows Controller app running during booth operation.

Typical trigger flow received by the app:

```text
session_start
countdown_start&param1=10
countdown&param1=<percent_complete>
capture_start
processing_start
sharing_screen
session_end
```

The app starts its own one-second display timer from the `countdown_start` value, so the OLED shows:

```text
10 -> 9 -> 8 -> 7 -> 6 -> 5 -> 4 -> 3 -> 2 -> 1 -> GO!
```

`capture_start` is the authoritative transition to the OLED `GO!` state.

### Important RF behavior

The dslrBooth `capture_start` integration currently changes the OLED/status state only. It does **not** automatically transmit the RF ON/OFF command.

This is intentional because the booth receiver's ON/OFF command is a toggle. Automatically transmitting another ON/OFF at capture time could stop the platform if another trigger had already started it. Physical and Windows ON/OFF control continue to work normally.

## OLED states

The display can show:

```text
READY / STOPPED
PREPARING
GET READY + countdown seconds
GO!
360 BOOTH LIVE
PROCESSING
COMPLETE / THANK YOU
STOP / KILL
```

Direction and speed are controller-commanded status values, not feedback received from the booth. If the original factory remote is used separately, those displayed values can become out of sync with the actual receiver state.

## Build requirements

Install either:

- Visual Studio 2022 with the **.NET desktop development** workload, or
- .NET 8 SDK for Windows

The project targets `net8.0-windows` and uses WPF.

## Build with Visual Studio

1. Open `windows-app/Pico360Controller.sln`.
2. Allow NuGet packages to restore.
3. Select **Release**.
4. Build the solution.
5. Run `Pico360Controller.exe` from the build output folder.

## Publish a standalone Windows EXE

From PowerShell in the `windows-app/Pico360Controller` folder:

```powershell
dotnet restore
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true
```

The published app will be under a folder similar to:

```text
bin\Release\net8.0-windows\win-x64\publish\
```

## First-time setup

1. Upload the current Pico firmware.
2. Connect the Pico to the Windows computer using a USB data cable.
3. Close Arduino Serial Monitor before using the Windows app so it does not hold the COM port open.
4. Start the Windows app.
5. The app will attempt to auto-detect the Pico.
6. If auto-detection fails, select the Pico COM port manually and click **Connect**.
7. Confirm the activity log says the dslrBooth webhook listener is running on port 8000.

## Control behavior

### ON / OFF

Sends `ONOFF`. The Pico transmits the verified 315 MHz ON/OFF command using 12 RF repeats.

### REVERSE

Sends `REVERSE`. The Pico transmits the verified REVERSE command using 20 RF repeats.

### SPEED + / SPEED -

The first command is sent immediately when the button is pressed. While held, the app sends another command approximately every 1.1 seconds. This avoids filling the Pico serial buffer while the 20-repeat RF transmission is running.

### STOP / KILL

Sends `KILL`. The Pico then:

1. Displays STOP on the OLED.
2. Sends an ESC keyboard command to Windows/dslrBooth.
3. Sends the verified RF ON/OFF command with 20 repeats.
4. Returns the OLED to STOPPED/READY.

## Important safety limitation

The booth receiver uses a toggle-style ON/OFF command. Therefore the software STOP/KILL function is an **operational stop/cancel feature**, not a safety-rated emergency stop.

Do not rely on this Windows app, USB, the Pico, RF, or the receiver toggle as the only emergency-stop mechanism for machinery. Use a proper hardwired safety circuit where injury could occur.

## Troubleshooting

### Pico does not appear

- Make sure the USB cable supports data, not power only.
- Check Windows Device Manager under **Ports (COM & LPT)**.
- Close Arduino Serial Monitor or any other program using the same COM port.
- Click **Refresh** and then **Auto Detect**.

### OLED stays blank

- Confirm the display scans at `0x3C`.
- Confirm VCC is connected to `3V3(OUT)`, not `3V3_EN`.
- Confirm SDA is GP14 and SCL is GP15.
- Install Adafruit SSD1306 and all dependencies.

### dslrBooth status does not appear on the OLED

- Confirm the Windows app is running and connected to the Pico.
- Confirm dslrBooth trigger URL is exactly `http://127.0.0.1:8000/`.
- Watch the Windows app activity log for `dslrBooth event:` messages.
- If the listener cannot start because port 8000 is already in use, close the other application or change the port in both places.

### App connects but controls do nothing

A healthy connection should show responses such as:

```text
< PICO360 READY
< PICO360 STATUS READY 315.000MHz OLED
```

When a command is sent, the Pico should report entries such as:

```text
< TX SPEED_UP 2095682 0x1FFA42
< OK SPEED_UP
```
