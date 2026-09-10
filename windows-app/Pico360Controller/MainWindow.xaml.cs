using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;

namespace Pico360Controller;

public partial class MainWindow : Window
{
    private readonly SerialController _serial = new();
    private readonly DispatcherTimer _speedRepeatTimer;
    private string? _speedRepeatCommand;
    private string? _speedRepeatDisplayName;

    public MainWindow()
    {
        InitializeComponent();

        _serial.LineReceived += Serial_LineReceived;
        _serial.ConnectionChanged += Serial_ConnectionChanged;

        _speedRepeatTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(1100)
        };
        _speedRepeatTimer.Tick += SpeedRepeatTimer_Tick;

        Loaded += MainWindow_Loaded;
        Closed += MainWindow_Closed;

        UpdateControlState();
    }

    private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
    {
        RefreshPortList();
        await AutoDetectAndConnectAsync();
    }

    private void MainWindow_Closed(object? sender, EventArgs e)
    {
        _speedRepeatTimer.Stop();
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

        SendCommand(
            _speedRepeatCommand,
            _speedRepeatDisplayName ?? _speedRepeatCommand);
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
