# dslrBooth -> Pico OLED using Application/Script Trigger

This is the recommended dslrBooth integration for the Pico360 controller.

No webhook listener or background bridge application is required. LumaBooth for Windows (dslrBooth) launches a batch file for each trigger event and passes the event type and parameters as command-line arguments.

## Files

Copy the `dslrbooth-trigger` folder to a permanent location on the booth PC, for example:

```text
C:\Pico360\
    Pico360Trigger.bat
    pico360-config.txt
    FindPicoPort.ps1
```

`Pico360Trigger.bat` is the only file dslrBooth calls.

`FindPicoPort.ps1` is optional. Run it manually only when you need to find which COM port Windows assigned to the Pico.

## 1. Set the Pico COM port

Open Device Manager and note the Pico's COM port, or run the optional PowerShell finder:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Pico360\FindPicoPort.ps1
```

The finder sends `PING` to available serial ports and looks for the Pico response `PICO360 READY`. If it finds the Pico, it updates `pico360-config.txt` automatically.

You can also edit the file manually:

```text
COMPORT=COM7
BAUD=115200
COUNTDOWN_SECONDS=10
LOGGING=1
```

Change only `COMPORT` when Windows assigns a different port. `COUNTDOWN_SECONDS` should match the countdown configured in dslrBooth.

## 2. Configure dslrBooth

In LumaBooth for Windows (dslrBooth):

1. Open **Settings > General > Triggers**.
2. Choose the **Application/Script** trigger option.
3. Enter only the full path to the BAT file:

```text
C:\Pico360\Pico360Trigger.bat
```

Do not add `%1`, `%2`, event names, or other arguments in the dslrBooth field. dslrBooth supplies the event type and parameters itself.

## 3. OLED event mapping

The batch file translates dslrBooth events into the serial commands already supported by the Pico firmware:

| dslrBooth event | Pico serial command | OLED result |
| --- | --- | --- |
| `session_start` | `DSLR_SESSION_START` | PREPARING |
| `countdown_start 10` | `DSLR_COUNTDOWN 10` | GET READY / 10 |
| `countdown 20` | calculated seconds remaining | countdown update |
| `capture_start` | `DSLR_GO` | GO! / 360 BOOTH LIVE |
| `processing_start` | `DSLR_PROCESSING` | PROCESSING |
| `sharing_screen` | `DSLR_SHARING` | COMPLETE / THANK YOU |
| `session_end` | `DSLR_SESSION_END` | READY |

The dslrBooth `countdown` event contains percentage complete, not seconds remaining. The BAT file converts the percentage to remaining seconds using `COUNTDOWN_SECONDS` from the config file.

`file_download`, `printing`, and `file_upload` are logged but currently do not replace the main guest-facing OLED state.

## 4. Test before using dslrBooth

Close Arduino Serial Monitor and close the Pico360 Windows controller so nothing else owns the COM port.

From Command Prompt:

```cmd
C:\Pico360\Pico360Trigger.bat session_start PrintAndGIF
C:\Pico360\Pico360Trigger.bat countdown_start 10
C:\Pico360\Pico360Trigger.bat countdown 50
C:\Pico360\Pico360Trigger.bat capture_start
C:\Pico360\Pico360Trigger.bat processing_start
C:\Pico360\Pico360Trigger.bat sharing_screen
C:\Pico360\Pico360Trigger.bat session_end
```

Expected OLED sequence:

```text
PREPARING
GET READY 10
GET READY 5
GO!
PROCESSING
COMPLETE
READY
```

## Logging

When `LOGGING=1`, the batch file writes:

```text
Pico360Trigger.log
```

in the same folder. This is useful for confirming that dslrBooth is actually launching the script and which events it supplied.

Set `LOGGING=0` to disable the log.

## Important serial-port note

The batch method opens the Pico serial port only long enough to send each status command, then exits. Do not leave Arduino Serial Monitor or another application holding the same COM port while testing.

The optional Pico360 Windows controller is a separate manual-control interface. It may need to be closed while dslrBooth is using this first BAT-based version so both programs do not attempt to own the COM port at the same moment.
