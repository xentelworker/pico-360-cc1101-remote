# dslrBooth Webhook Bridge

`Pico360Bridge` lets dslrBooth send live session status to the Raspberry Pi Pico OLED without depending on the optional Windows remote-control frontend.

## Architecture

```text
dslrBooth
    |
    | HTTP webhook
    v
Pico360Bridge.exe
    |
    | USB serial, auto-detected COM port
    v
Raspberry Pi Pico
    |
    +-- OLED status
    +-- CC1101 315 MHz transmitter
    +-- optional physical buttons
```

The bridge automatically scans Windows COM ports, sends `PING`, and connects to the port that replies with `PICO360`.

If the Pico is unplugged, the bridge keeps rescanning and reconnects when it returns, even if Windows assigns a different COM port.

## dslrBooth configuration

In dslrBooth go to:

`Settings > General > Triggers`

Set the URL trigger to:

```text
http://127.0.0.1:8000
```

The bridge listens locally on port 8000.

## Supported webhook events

| dslrBooth event | Pico command | OLED behavior |
| --- | --- | --- |
| `session_start` | `DSLR_SESSION_START` | PREPARING |
| `countdown_start [seconds]` | `DSLR_COUNTDOWN n` | Starts countdown display |
| `countdown [percent]` | `DSLR_COUNTDOWN n` | Converts percent complete into seconds remaining |
| `capture_start` | `DSLR_CAPTURE` | GO / LIVE |
| `processing_start` | `DSLR_PROCESSING` | PROCESSING |
| `sharing_screen` | `DSLR_COMPLETE` | COMPLETE / THANK YOU |
| `session_end` | `DSLR_READY` | READY |

Other dslrBooth events are acknowledged and ignored.

## Build

Open:

```text
windows-app/Pico360Controller.sln
```

Build the `Pico360Bridge` project in Visual Studio 2022 with the .NET desktop workload installed.

Or publish from PowerShell:

```powershell
cd windows-app\Pico360Bridge
dotnet restore
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true
```

## First test

1. Upload the current Pico firmware.
2. Connect the Pico to Windows by USB.
3. Close Arduino Serial Monitor so it does not hold the COM port.
4. Start `Pico360Bridge.exe`.
5. The console should show `Pico connected: COMx`.
6. In a browser on the same PC, test:

```text
http://127.0.0.1:8000?event_type=countdown_start&param1=10
```

The OLED should show `10`.

Then test:

```text
http://127.0.0.1:8000?event_type=countdown&param1=50
```

The OLED should show approximately `5` seconds remaining.

Finally test:

```text
http://127.0.0.1:8000?event_type=session_end
```

The OLED should return to READY.

## Important

For this first bridge test, keep the existing `Pico360Controller` Windows frontend closed. It currently opens the Pico COM port directly. The next revision can route the optional frontend through the bridge so both can run together without competing for the serial port.
