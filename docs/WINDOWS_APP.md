# Windows Controller App

The `windows-app` folder contains a native Windows WPF frontend for controlling the Raspberry Pi Pico over USB serial.

## Features

- Auto-detects the Pico by sending `PING` and looking for a `PICO360` response
- Manual COM-port selection and connect/disconnect
- ON/OFF, REVERSE, SPEED+, SPEED-, and STOP/KILL controls
- SPEED+ and SPEED- repeat while held
- Activity log showing commands sent and responses from the Pico
- Large buttons suitable for touchscreen use

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
```

`PING` replies with:

```text
PICO360 READY
```

The Windows app uses that reply for auto-detection.

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

For most modern Intel/AMD Windows PCs, `win-x64` is appropriate.

## First-time setup

1. Upload the current Pico firmware.
2. Connect the Pico to the Windows computer using a USB data cable.
3. Close Arduino Serial Monitor before using the Windows app so it does not hold the COM port open.
4. Start the Windows app.
5. The app will attempt to auto-detect the Pico.
6. If auto-detection fails, select the Pico COM port manually and click **Connect**.

## Control behavior

### ON / OFF

Sends:

```text
ONOFF
```

The Pico transmits the verified 315 MHz ON/OFF command using 12 RF repeats.

### REVERSE

Sends:

```text
REVERSE
```

The Pico transmits the verified REVERSE command using 20 RF repeats.

### SPEED + / SPEED -

The first command is sent immediately when the button is pressed. While the button remains held, the app sends another command approximately every 1.1 seconds. This timing avoids filling the Pico serial buffer while the 20-repeat RF transmission is still running.

### STOP / KILL

Sends:

```text
KILL
```

The Pico then:

1. Sends an ESC keyboard command to Windows/dslrBooth.
2. Sends the verified RF ON/OFF command with 20 repeats.

## Important safety limitation

The booth receiver uses a toggle-style ON/OFF command. Therefore the software STOP/KILL function is an **operational stop/cancel feature**, not a safety-rated emergency stop.

Do not rely on this Windows app, USB, the Pico, RF, or the receiver toggle as the only emergency-stop mechanism for machinery. Use a proper hardwired safety circuit where injury could occur.

## Troubleshooting

### Pico does not appear

- Make sure the USB cable supports data, not power only.
- Check Windows Device Manager under **Ports (COM & LPT)**.
- Close Arduino Serial Monitor or any other program using the same COM port.
- Click **Refresh** and then **Auto Detect**.

### App connects but controls do nothing

Open the activity log. A healthy connection should show responses such as:

```text
< PICO360 READY
< PICO360 STATUS READY 315.000MHz
```

When a command is sent, the Pico should report entries such as:

```text
< TX SPEED_UP 2095682 0x1FFA42
< OK SPEED_UP
```

If those appear but the booth does not respond, troubleshoot the CC1101 antenna, power, wiring, frequency, and RF range.
