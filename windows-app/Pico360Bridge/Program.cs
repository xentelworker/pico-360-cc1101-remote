using System.IO.Ports;
using System.Net;
using System.Text;

const string Prefix = "http://127.0.0.1:8000/";

Console.Title = "Pico 360 Bridge";
Console.WriteLine("Pico 360 Bridge");
Console.WriteLine("===============");
Console.WriteLine($"Webhook: {Prefix}");
Console.WriteLine("Searching for Pico360 controller...");

using var bridge = new PicoBridge();
_ = bridge.RunReconnectLoopAsync();

using var listener = new HttpListener();
listener.Prefixes.Add(Prefix);
listener.Start();
Console.WriteLine("Webhook listener started.");
Console.WriteLine("Configure dslrBooth URL trigger to: http://127.0.0.1:8000");
Console.WriteLine();

while (true)
{
    HttpListenerContext context;
    try
    {
        context = await listener.GetContextAsync();
    }
    catch (HttpListenerException)
    {
        break;
    }

    _ = Task.Run(async () =>
    {
        try
        {
            var request = context.Request;
            var eventType = request.QueryString["event_type"]?.Trim().ToLowerInvariant() ?? "";
            var param1 = request.QueryString["param1"]?.Trim() ?? "";

            Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] dslrBooth: {eventType} {param1}".TrimEnd());

            var result = await bridge.HandleDslrBoothEventAsync(eventType, param1);
            var body = Encoding.UTF8.GetBytes(result);
            context.Response.StatusCode = result.StartsWith("OK") ? 200 : 503;
            context.Response.ContentType = "text/plain; charset=utf-8";
            context.Response.ContentLength64 = body.Length;
            await context.Response.OutputStream.WriteAsync(body);
            context.Response.Close();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Webhook error: {ex.Message}");
            try
            {
                context.Response.StatusCode = 500;
                context.Response.Close();
            }
            catch { }
        }
    });
}

sealed class PicoBridge : IDisposable
{
    private readonly SemaphoreSlim _serialLock = new(1, 1);
    private SerialPort? _port;
    private int _countdownSeconds = 10;
    private volatile bool _disposed;

    public async Task RunReconnectLoopAsync()
    {
        while (!_disposed)
        {
            try
            {
                if (_port is null || !_port.IsOpen)
                    await FindAndConnectAsync();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Pico scan error: {ex.Message}");
            }

            await Task.Delay(2000);
        }
    }

    private async Task FindAndConnectAsync()
    {
        foreach (var name in SerialPort.GetPortNames().OrderBy(x => x))
        {
            if (_disposed) return;

            try
            {
                using var probe = CreatePort(name);
                probe.Open();
                await Task.Delay(350);
                probe.DiscardInBuffer();
                probe.WriteLine("PING");

                var deadline = DateTime.UtcNow.AddMilliseconds(1400);
                while (DateTime.UtcNow < deadline)
                {
                    try
                    {
                        var line = probe.ReadLine().Trim();
                        if (line.Contains("PICO360", StringComparison.OrdinalIgnoreCase))
                        {
                            probe.Close();
                            await ConnectAsync(name);
                            return;
                        }
                    }
                    catch (TimeoutException) { }
                }
            }
            catch
            {
                // Busy/non-Pico COM ports are expected and skipped.
            }
        }
    }

    private async Task ConnectAsync(string name)
    {
        await _serialLock.WaitAsync();
        try
        {
            ClosePort();
            var port = CreatePort(name);
            port.DataReceived += Port_DataReceived;
            port.Open();
            _port = port;
            await Task.Delay(350);
            Console.WriteLine($"Pico connected: {name}");
            await SendUnlockedAsync("STATUS");
        }
        finally
        {
            _serialLock.Release();
        }
    }

    private static SerialPort CreatePort(string name) => new(name, 115200)
    {
        NewLine = "\n",
        ReadTimeout = 250,
        WriteTimeout = 1000,
        DtrEnable = true,
        RtsEnable = false
    };

    private void Port_DataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            if (sender is not SerialPort p) return;
            while (p.BytesToRead > 0)
            {
                try
                {
                    var line = p.ReadLine().Trim();
                    if (line.Length > 0)
                        Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Pico: {line}");
                }
                catch (TimeoutException) { break; }
            }
        }
        catch
        {
            Console.WriteLine("Pico disconnected. Rescanning...");
            ClosePort();
        }
    }

    public async Task<string> HandleDslrBoothEventAsync(string eventType, string param1)
    {
        string? command = eventType switch
        {
            "session_start" => "DSLR_SESSION_START",
            "countdown_start" => BuildCountdownStart(param1),
            "countdown" => BuildCountdownProgress(param1),
            "capture_start" => "DSLR_CAPTURE",
            "processing_start" => "DSLR_PROCESSING",
            "sharing_screen" => "DSLR_COMPLETE",
            "session_end" => "DSLR_READY",
            _ => null
        };

        if (command is null)
            return "OK IGNORED";

        return await SendAsync(command)
            ? $"OK {command}"
            : "PICO NOT CONNECTED";
    }

    private string BuildCountdownStart(string param1)
    {
        if (int.TryParse(param1, out var seconds) && seconds > 0 && seconds <= 99)
            _countdownSeconds = seconds;

        return $"DSLR_COUNTDOWN {_countdownSeconds}";
    }

    private string BuildCountdownProgress(string param1)
    {
        if (!double.TryParse(param1, out var percent))
            percent = 0;

        percent = Math.Clamp(percent, 0, 100);
        var remaining = (int)Math.Ceiling(_countdownSeconds * (1.0 - percent / 100.0));
        remaining = Math.Clamp(remaining, 0, _countdownSeconds);
        return $"DSLR_COUNTDOWN {remaining}";
    }

    public async Task<bool> SendAsync(string command)
    {
        await _serialLock.WaitAsync();
        try
        {
            return await SendUnlockedAsync(command);
        }
        finally
        {
            _serialLock.Release();
        }
    }

    private Task<bool> SendUnlockedAsync(string command)
    {
        try
        {
            if (_port is null || !_port.IsOpen)
                return Task.FromResult(false);

            _port.WriteLine(command);
            Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] > {command}");
            return Task.FromResult(true);
        }
        catch
        {
            Console.WriteLine("Pico write failed. Rescanning...");
            ClosePort();
            return Task.FromResult(false);
        }
    }

    private void ClosePort()
    {
        try
        {
            if (_port is not null)
            {
                _port.DataReceived -= Port_DataReceived;
                if (_port.IsOpen) _port.Close();
                _port.Dispose();
            }
        }
        catch { }
        finally
        {
            _port = null;
        }
    }

    public void Dispose()
    {
        _disposed = true;
        ClosePort();
        _serialLock.Dispose();
    }
}
