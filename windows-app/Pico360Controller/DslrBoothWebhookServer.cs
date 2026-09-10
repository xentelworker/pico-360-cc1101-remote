using System.Net;

namespace Pico360Controller;

public sealed class DslrBoothWebhookEventArgs : EventArgs
{
    public string EventType { get; }
    public string? Param1 { get; }
    public string? Param2 { get; }

    public DslrBoothWebhookEventArgs(string eventType, string? param1, string? param2)
    {
        EventType = eventType;
        Param1 = param1;
        Param2 = param2;
    }
}

public sealed class DslrBoothWebhookServer : IDisposable
{
    private readonly HttpListener _listener = new();
    private CancellationTokenSource? _cts;
    private Task? _listenTask;

    public event EventHandler<DslrBoothWebhookEventArgs>? EventReceived;
    public event Action<string>? Log;

    public bool IsRunning => _listener.IsListening;
    public int Port { get; }

    public DslrBoothWebhookServer(int port = 8000)
    {
        Port = port;
        _listener.Prefixes.Add($"http://127.0.0.1:{port}/");
    }

    public void Start()
    {
        if (_listener.IsListening)
            return;

        _cts = new CancellationTokenSource();
        _listener.Start();
        _listenTask = Task.Run(() => ListenLoopAsync(_cts.Token));
        Log?.Invoke($"dslrBooth webhook listening on http://127.0.0.1:{Port}/");
    }

    public void Stop()
    {
        if (!_listener.IsListening)
            return;

        try
        {
            _cts?.Cancel();
            _listener.Stop();
        }
        catch
        {
            // Ignore shutdown races.
        }
    }

    private async Task ListenLoopAsync(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            HttpListenerContext context;

            try
            {
                context = await _listener.GetContextAsync();
            }
            catch when (token.IsCancellationRequested || !_listener.IsListening)
            {
                break;
            }
            catch (Exception ex)
            {
                Log?.Invoke($"Webhook listener error: {ex.Message}");
                continue;
            }

            _ = Task.Run(() => HandleRequestAsync(context), token);
        }
    }

    private async Task HandleRequestAsync(HttpListenerContext context)
    {
        try
        {
            string eventType = context.Request.QueryString["event_type"] ?? string.Empty;
            string? param1 = context.Request.QueryString["param1"];
            string? param2 = context.Request.QueryString["param2"];

            if (!string.IsNullOrWhiteSpace(eventType))
            {
                EventReceived?.Invoke(this, new DslrBoothWebhookEventArgs(eventType, param1, param2));
            }

            const string responseText = "Pico360 dslrBooth trigger received";
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(responseText);
            context.Response.StatusCode = 200;
            context.Response.ContentType = "text/plain";
            context.Response.ContentLength64 = bytes.Length;
            await context.Response.OutputStream.WriteAsync(bytes);
            context.Response.Close();
        }
        catch (Exception ex)
        {
            Log?.Invoke($"Webhook request error: {ex.Message}");
            try { context.Response.Abort(); } catch { }
        }
    }

    public void Dispose()
    {
        Stop();
        _cts?.Dispose();
        _listener.Close();
    }
}
