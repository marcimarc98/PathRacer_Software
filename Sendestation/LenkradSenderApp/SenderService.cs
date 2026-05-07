using System.Diagnostics;
using System.IO.Ports;
using SharpDX.DirectInput;

namespace LenkradSenderApp;

public sealed class SenderService : IDisposable
{
    private const int BaudRate = 460800;
    private const int SendHz = 1000;
    private const int AxisMinimum = -1000;
    private const int AxisMaximum = 1000;
    private const WheelAxis SteeringAxis = WheelAxis.X;
    private const WheelAxis GasAxis = WheelAxis.RotationZ;
    private const WheelAxis BrakeAxis = WheelAxis.Y;

    private SerialPort? _serialPort;
    private DirectInputWheel? _wheel;
    private CancellationTokenSource? _cts;
    private Task? _sendTask;
    private string _statusText = "Bereit";
    private long _packetCount;
    private Exception? _backgroundError;
    private readonly object _telemetryLock = new();
    private TelemetrySnapshot _lastTelemetry = TelemetrySnapshot.Empty;

    public event Action<string>? StatusMessage;

    public bool IsRunning => _sendTask is { IsCompleted: false };
    public string WheelName => _wheel?.DeviceName ?? "Nicht verbunden";
    public string StatusText => _statusText;
    public long PacketCount => Interlocked.Read(ref _packetCount);
    public TelemetrySnapshot LastTelemetry
    {
        get
        {
            lock (_telemetryLock)
            {
                return _lastTelemetry;
            }
        }
    }

    public void Start(string portName)
    {
        if (IsRunning)
        {
            throw new InvalidOperationException("Sender laeuft bereits.");
        }

        Stop();

        EmitStatus("Lenkrad wird verbunden...");
        _backgroundError = null;

        try
        {
            _wheel = new DirectInputWheel();
            EmitStatus($"Lenkrad erkannt: {_wheel.DeviceName}");
            EmitStatus("Archiv-Mapping aktiv: Lenkung=X, Gas=RZ, Bremse=Y");

            _serialPort = new SerialPort(portName, BaudRate)
            {
                Handshake = Handshake.None,
                ReadTimeout = SerialPort.InfiniteTimeout,
                WriteTimeout = SerialPort.InfiniteTimeout,
                WriteBufferSize = 4096,
                DtrEnable = false,
                RtsEnable = false,
            };

            _serialPort.Open();
            Thread.Sleep(1000);
            EmitStatus($"Serial verbunden: {portName} @ {BaudRate}");

            Interlocked.Exchange(ref _packetCount, 0);
            _cts = new CancellationTokenSource();
            EmitStatus($"Verbunden: {portName}");
            _sendTask = Task.Run(() => SendLoop(_cts.Token));
            EmitStatus("Sende-Loop gestartet.");
        }
        catch
        {
            Stop();
            throw;
        }
    }

    public void Stop()
    {
        try
        {
            _cts?.Cancel();
            _sendTask?.Wait(1000);
        }
        catch
        {
        }

        _sendTask = null;
        _cts?.Dispose();
        _cts = null;

        _serialPort?.Dispose();
        _serialPort = null;

        _wheel?.Dispose();
        _wheel = null;
        SetTelemetry(TelemetrySnapshot.Empty);

        if (_backgroundError is not null)
        {
            EmitStatus($"Fehler: {_backgroundError.Message}");
        }
        else if (_statusText.StartsWith("Verbunden:", StringComparison.Ordinal))
        {
            EmitStatus("Gestoppt");
        }
    }

    private void SendLoop(CancellationToken cancellationToken)
    {
        var serial = _serialPort ?? throw new InvalidOperationException("Serial-Port nicht offen.");
        var wheel = _wheel ?? throw new InvalidOperationException("Lenkrad nicht verbunden.");

        using var timerScope = WindowsTiming.BeginHighResolution();

        Thread.CurrentThread.Priority = ThreadPriority.Highest;

        var stopwatch = Stopwatch.StartNew();
        var ticksPerPacket = Stopwatch.Frequency / SendHz;
        var nextTick = stopwatch.ElapsedTicks;

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var state = wheel.Poll();

                var steeringRaw = DirectInputWheel.ReadAxis(state, SteeringAxis);
                var gasRaw = DirectInputWheel.ReadAxis(state, GasAxis);
                var brakeRaw = DirectInputWheel.ReadAxis(state, BrakeAxis);

                var steering = ToArchiveSteering(steeringRaw);
                var gas = ToArchiveGas(gasRaw);
                var brake = ToArchiveBrake(brakeRaw);
                var buttons = ReadButtons(state);
                SetTelemetry(new TelemetrySnapshot(steering, gas, brake, steeringRaw, gasRaw, brakeRaw));

                var packet = HostPacket.Build(steering, gas, brake, buttons);
                serial.Write(packet, 0, packet.Length);

                Interlocked.Increment(ref _packetCount);

                nextTick += ticksPerPacket;
                WaitUntil(stopwatch, nextTick, cancellationToken);
            }
        }
        catch (Exception ex)
        {
            _backgroundError = ex;
            EmitStatus($"Fehler: {ex.Message}");
        }
    }

    private void SetTelemetry(TelemetrySnapshot telemetry)
    {
        lock (_telemetryLock)
        {
            _lastTelemetry = telemetry;
        }
    }

    private void EmitStatus(string message)
    {
        _statusText = message;
        StatusMessage?.Invoke(message);
    }

    private static short ToArchiveSteering(int raw)
    {
        return (short)Math.Clamp(raw, AxisMinimum, AxisMaximum);
    }

    private static ushort ToArchiveGas(int raw)
    {
        return ToArchivePedal(raw);
    }

    private static ushort ToArchiveBrake(int raw)
    {
        return ToArchivePedal(raw);
    }

    private static ushort ToArchivePedal(int raw)
    {
        var pedal = (1000 - raw) / 2;
        return (ushort)Math.Clamp(pedal, 0, 1000);
    }

    private static ushort ReadButtons(JoystickState state)
    {
        ushort buttons = 0;
        var source = state.Buttons;
        var count = Math.Min(13, source.Length);

        for (var i = 0; i < count; i++)
        {
            if (source[i])
            {
                buttons |= (ushort)(1 << i);
            }
        }

        return buttons;
    }

    private static void WaitUntil(Stopwatch stopwatch, long targetTicks, CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            var remainingTicks = targetTicks - stopwatch.ElapsedTicks;

            if (remainingTicks <= 0)
            {
                return;
            }

            var remainingMs = remainingTicks * 1000.0 / Stopwatch.Frequency;

            if (remainingMs > 2.0)
            {
                Thread.Sleep(Math.Max(0, (int)(remainingMs - 1.0)));
                continue;
            }

            Thread.SpinWait(200);
        }
    }

    public void Dispose()
    {
        Stop();
    }

    public readonly record struct TelemetrySnapshot(
        short Steering,
        ushort Gas,
        ushort Brake,
        int SteeringRaw,
        int GasRaw,
        int BrakeRaw)
    {
        public static readonly TelemetrySnapshot Empty = new(0, 0, 0, 0, 0, 0);
    }

}
