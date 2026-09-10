using System.IO.Ports;

namespace Pico360Controller;

public sealed class SerialController : IDisposable
{
    private SerialPort? _port;

    public event Action<string>? LineReceived;
    public event Action<bool, string?>? ConnectionChanged;

    public bool IsConnected => _port?.IsOpen == true;
    public string? PortName => IsConnected ? _port!.PortName : null;

    public static string[] GetAvailablePorts() =>
        SerialPort.GetPortNames()
            .OrderBy(p => p, StringComparer.OrdinalIgnoreCase)
            .ToArray();

    public void Connect(string portName)
    {
        Disconnect();

        var port = new SerialPort(portName, 115200)
        {
            NewLine = "\n",
            ReadTimeout = 1000,
            WriteTimeout = 1000,
            DtrEnable = true,
            RtsEnable = false
        };

        port.DataReceived += Port_DataReceived;
        port.Open();
        _port = port;
        ConnectionChanged?.Invoke(true, portName);
    }

    public void Disconnect()
    {
        if (_port is null)
            return;

        try
        {
            _port.DataReceived -= Port_DataReceived;
            if (_port.IsOpen)
                _port.Close();
        }
        catch
        {
            // Ignore shutdown errors.
        }
        finally
        {
            _port.Dispose();
            _port = null;
            ConnectionChanged?.Invoke(false, null);
        }
    }

    public void SendCommand(string command)
    {
        if (!IsConnected)
            throw new InvalidOperationException("Pico is not connected.");

        _port!.WriteLine(command);
    }

    public static async Task<string?> AutoDetectPicoAsync(CancellationToken cancellationToken = default)
    {
        foreach (var portName in GetAvailablePorts())
        {
            cancellationToken.ThrowIfCancellationRequested();

            var detected = await Task.Run(() => ProbePort(portName), cancellationToken);
            if (detected)
                return portName;
        }

        return null;
    }

    private static bool ProbePort(string portName)
    {
        try
        {
            using var probe = new SerialPort(portName, 115200)
            {
                NewLine = "\n",
                ReadTimeout = 250,
                WriteTimeout = 500,
                DtrEnable = true,
                RtsEnable = false
            };

            probe.Open();

            // Give the USB serial interface time to settle after opening.
            Thread.Sleep(350);
            probe.DiscardInBuffer();
            probe.WriteLine("PING");

            var deadline = DateTime.UtcNow.AddMilliseconds(1400);
            while (DateTime.UtcNow < deadline)
            {
                try
                {
                    var line = probe.ReadLine().Trim();
                    if (line.Contains("PICO360", StringComparison.OrdinalIgnoreCase))
                        return true;
                }
                catch (TimeoutException)
                {
                    // Continue until the overall deadline expires.
                }
            }
        }
        catch
        {
            // Port may belong to another device or be busy.
        }

        return false;
    }

    private void Port_DataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        var port = _port;
        if (port is null || !port.IsOpen)
            return;

        try
        {
            while (port.IsOpen && port.BytesToRead > 0)
            {
                var line = port.ReadLine().Trim();
                if (!string.IsNullOrWhiteSpace(line))
                    LineReceived?.Invoke(line);
            }
        }
        catch (TimeoutException)
        {
        }
        catch (Exception ex)
        {
            LineReceived?.Invoke($"SERIAL ERROR: {ex.Message}");
        }
    }

    public void Dispose() => Disconnect();
}
