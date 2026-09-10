using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;

namespace Pico360Controller;

public partial class MainWindow : Window
{
    private readonly SerialController _serial = new();
    private readonly DslrBoothWebhookServer _dslrWebhook = new(8000);
    private readonly DispatcherTimer _speedRepeatTimer;
    private readonly DispatcherTimer _countdownTimer;
    private readonly DispatcherTimer _heartbeatTimer;

    private string? _speedRepeatCommand;
    private string? _speedRepeatDisplayName;
    private int _countdownRemaining;

    public MainWindow()
    {
        InitializeComponent();

        _serial.LineReceived += Serial_LineReceived;
        _serial.ConnectionChanged += Serial_ConnectionChanged;

        _dslrWebhook.EventReceived += DslrWebhook_EventReceived;
        _dslrWebhook.Log += message => Dispatcher.BeginInvoke(() => AppendLog(message));

        _speedRepeatTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(1100)
        };
        _speedRepeatTimer.Tick += SpeedRepeatTimer_Tick;

        _countdownTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(1)
        };
        _countdownTimer.Tick += CountdownTimer_Tick;

        _heartbeatTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(5)
        };
        _heartbeatTimer.Tick += HeartbeatTimer_Tick;

        Loaded += MainWindow_Loaded;
        Closed += MainWindow_Closed;

        UpdateControlState();
    }

    private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
    {
        try
        {
            _dslrWebhook.Start();
            AppendLog("dslrBooth trigger URL: http://127.0.0.1:8000/");
        }
        catch (Exception ex)
        {
            AppendLog($"Could not start dslrBooth webhook listener: {ex.Message}");
        }

        _heartbeatTimer.Start();
        RefreshPortList();
        await AutoDetectAndConnectAsync();
    }

    private void MainWindow_Closed(object? sender, EventArgs e)
    {
        _speedRepeatTimer.Stop();
        _countdownTimer.Stop();
        _heartbeatTimer.Stop();
        _dslrWebhook.Dispose();
        _serial.Dispose();
    }

    private void RefreshPortList()
    {
        var previous = PortComboBox.SelectedItem as string;
        var ports = SerialController.GetAvailablePorts();

        PortComboBox.ItemsSource = ports;

        if (previous is not null && ports.Contains(previous))
            PortComboBox.SelectedItem = previous;
        else if (ports.Length > 0)
            PortComboBox.SelectedIndex = 0;

        AppendLog(ports.Length == 0
            ? "No serial ports found."
            : $"Serial ports: {string.Join(", ", ports)}");
    }

    private async Task AutoDetectAndConnectAsync()
    {
        if (_serial.IsConnected)
            return;

        ConnectionText.Text = "Searching for Pico…";
        AppendLog("Auto-detecting Pico 360 controller…");

        try
        {
            var port = await SerialController.AutoDetectPicoAsync();

            if (port is null)
            {
                ConnectionText.Text = "Pico not detected";
                AppendLog("Auto-detect did not find a Pico360 device. Select the COM port manually and click Connect.");
                return;
            }

            PortComboBox.SelectedItem = port;
            ConnectToPort(port);
            _serial.SendCommand("STATUS");
        }
        catch (Exception ex)
        {
            ConnectionText.Text = "Detection error";
            AppendLog($"Auto-detect error: {ex.Message}");
        }
    }

    private void ConnectToPort(string portName)
    {
        try
        {
            _serial.Connect(portName);
            AppendLog($"Connected to {portName}.");
            _serial.SendCommand("PING");
        }
        catch (Exception ex)
        {
            AppendLog($"Unable to connect to {portName}: {ex.Message}");
            MessageBox.Show(
                $"Could not connect to {portName}.\n\n{ex.Message}",
                "Pico 360 Controller",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private void SendCommand(string command, string displayName)
    {
        if (!_serial.IsConnected)
        {
            AppendLog($"Cannot send {displayName}: Pico is disconnected.");
            System.Media.SystemSounds.Exclamation.Play();
            return;
        }

        try
        {
            _serial.SendCommand(command);
            LastCommandText.Text = $"Last command: {displayName}";
            AppendLog($"> {command}");
        }
        catch (Exception ex)
        {
            AppendLog($"Send failed: {ex.Message}");
        }
    }

    private void SendStatusCommand(string command)
    {
        if (!_serial.IsConnected)
            return;

        try
        {
            _serial.SendCommand(command);
            AppendLog($"dslrBooth > {command}");
        }
        catch (Exception ex)
        {
            AppendLog($"dslrBooth status send failed: {ex.Message}");
        }
    }

    private void DslrWebhook_EventReceived(object? sender, DslrBoothWebhookEventArgs e)
    {
        Dispatcher.BeginInvoke(() => HandleDslrBoothEvent(e));
    }

    private void HandleDslrBoothEvent(DslrBoothWebhookEventArgs e)
    {
        string eventType = e.EventType.Trim().ToLowerInvariant();
        AppendLog($"dslrBooth event: {eventType}" +
                  (string.IsNullOrWhiteSpace(e.Param1) ? "" : $" ({e.Param1})"));

        switch (eventType)
        {
            case "session_start":
                _countdownTimer.Stop();
                SendStatusCommand("DSLR_SESSION_START");
                break;

            case "countdown_start":
                if (!int.TryParse(e.Param1, out _countdownRemaining) || _countdownRemaining < 1)
                    _countdownRemaining = 10;

                // The OLED follows the actual countdown_start seconds sent by dslrBooth.
                SendStatusCommand($"DSLR_COUNTDOWN {_countdownRemaining}");
                _countdownTimer.Stop();
                _countdownTimer.Start();
                break;

            case "countdown":
                // dslrBooth sends percent_complete here. We use countdown_start plus
                // a local one-second timer so the OLED can show 10, 9, 8 ... 1.
                break;

            case "capture_start":
                _countdownTimer.Stop();
                _countdownRemaining = 0;
                SendStatusCommand("DSLR_GO");
                break;

            case "processing_start":
                _countdownTimer.Stop();
                SendStatusCommand("DSLR_PROCESSING");
                break;

            case "sharing_screen":
                _countdownTimer.Stop();
                SendStatusCommand("DSLR_SHARING");
                break;

            case "session_end":
                _countdownTimer.Stop();
                _countdownRemaining = 0;
                SendStatusCommand("DSLR_SESSION_END");
                break;
        }
    }

    private void CountdownTimer_Tick(object? sender, EventArgs e)
    {
        if (_countdownRemaining <= 1)
        {
            _countdownTimer.Stop();
            return;
        }

        _countdownRemaining--;
        SendStatusCommand($"DSLR_COUNTDOWN {_countdownRemaining}");
    }

    private void HeartbeatTimer_Tick(object? sender, EventArgs e)
    {
        if (!_serial.IsConnected)
            return;

        try
        {
            // Keeps the OLED PC: CONNECTED indication alive without changing controls.
            _serial.SendCommand("PING");
        }
        catch
        {
            // SerialController will report the connection problem separately.
        }
    }

    private void Refresh_Click(object sender, RoutedEventArgs e)
    {
        RefreshPortList();
    }

    private void Connect_Click(object sender, RoutedEventArgs e)
    {
        if (_serial.IsConnected)
        {
            _serial.Disconnect();
            AppendLog("Disconnected.");
            return;
        }

        if (PortComboBox.SelectedItem is not string portName)
        {
            MessageBox.Show(
                "Select a COM port first.",
                "Pico 360 Controller",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        ConnectToPort(portName);
    }

    private async void AutoDetect_Click(object sender, RoutedEventArgs e)
    {
        RefreshPortList();
        await AutoDetectAndConnectAsync();
    }

    private void OnOff_Click(object sender, RoutedEventArgs e)
    {
        SendCommand("ONOFF", "ON / OFF");
    }

    private void Reverse_Click(object sender, RoutedEventArgs e)
    {
        SendCommand("REVERSE", "REVERSE");
    }

    private void Kill_Click(object sender, RoutedEventArgs e)
    {
        StopSpeedRepeat();
        _countdownTimer.Stop();
        SendCommand("KILL", "STOP / KILL");
    }

    private void SpeedUp_PreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        StartSpeedRepeat("SPEED_UP", "SPEED +");
        e.Handled = true;
    }

    private void SpeedDown_PreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        StartSpeedRepeat("SPEED_DOWN", "SPEED -");
        e.Handled = true;
    }

    private void SpeedButton_PreviewMouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        StopSpeedRepeat();
        e.Handled = true;
    }

    private void SpeedButton_MouseLeave(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed)
            StopSpeedRepeat();
    }

    private void StartSpeedRepeat(string command, string displayName)
    {
        StopSpeedRepeat();
        SendCommand(command, displayName);
        _speedRepeatCommand = command;
        _speedRepeatDisplayName = displayName;
        _speedRepeatTimer.Start();
    }

    private void StopSpeedRepeat()
    {
        _speedRepeatTimer.Stop();
        _speedRepeatCommand = null;
        _speedRepeatDisplayName = null;
    }

    private void SpeedRepeatTimer_Tick(object? sender, EventArgs e)
    {
        if (_speedRepeatCommand is null)
        {
            StopSpeedRepeat();
            return;
        }

        SendCommand(_speedRepeatCommand, _speedRepeatDisplayName ?? _speedRepeatCommand);
    }

    private void Serial_LineReceived(string line)
    {
        Dispatcher.BeginInvoke(() => AppendLog($"< {line}"));
    }

    private void Serial_ConnectionChanged(bool connected, string? portName)
    {
        Dispatcher.BeginInvoke(() =>
        {
            ConnectionIndicator.Fill = new SolidColorBrush(
                connected ? Color.FromRgb(52, 179, 95) : Color.FromRgb(197, 58, 58));

            ConnectionText.Text = connected
                ? $"Connected • {portName}"
                : "Disconnected";

            ConnectButton.Content = connected ? "Disconnect" : "Connect";
            UpdateControlState();
        });
    }

    private void UpdateControlState()
    {
        var enabled = _serial.IsConnected;
        OnOffButton.IsEnabled = enabled;
        ReverseButton.IsEnabled = enabled;
        SpeedUpButton.IsEnabled = enabled;
        SpeedDownButton.IsEnabled = enabled;
        KillButton.IsEnabled = enabled;
    }

    private void AppendLog(string message)
    {
        var timestamp = DateTime.Now.ToString("HH:mm:ss");
        LogTextBox.AppendText($"[{timestamp}] {message}{Environment.NewLine}");
        LogTextBox.ScrollToEnd();
    }
}
