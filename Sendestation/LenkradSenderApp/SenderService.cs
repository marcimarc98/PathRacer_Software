using System.Diagnostics;
using System.IO.Ports;
using System.Text;
using SharpDX.DirectInput;

namespace LenkradSenderApp;

public sealed class SenderService : IDisposable
{
    private const int BaudRate = 460800;
    private const int SendHz = 1000;
    private const int ReadTimeoutMs = 100;
    private const int AxisMinimum = -1000;
    private const int AxisMaximum = 1000;
    private const WheelAxis SteeringAxis = WheelAxis.X;
    private const WheelAxis GasAxis = WheelAxis.RotationZ;
    private const WheelAxis BrakeAxis = WheelAxis.Y;
    private const byte StatusHeader1 = 0x5A;
    private const byte StatusHeader2 = 0xA5;
    private const byte StatusPacketType = 0x31;
    private const int StatusPacketSize = 10;

    private SerialPort? _serialPort;
    private DirectInputWheel? _wheel;
    private CancellationTokenSource? _cts;
    private Task? _sendTask;
    private Task? _receiveTask;
    private string _statusText = "Bereit";
    private long _packetCount;
    private Exception? _backgroundError;
    private readonly object _telemetryLock = new();
    private TelemetrySnapshot _lastTelemetry = TelemetrySnapshot.Empty;
    private readonly object _vehicleTelemetryLock = new();
    private VehicleTelemetrySnapshot _lastVehicleTelemetry = VehicleTelemetrySnapshot.Empty;
    private readonly object _debugTelemetryLock = new();
    private DebugTelemetrySnapshot _lastDebugTelemetry = DebugTelemetrySnapshot.Empty;

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

    public VehicleTelemetrySnapshot LastVehicleTelemetry
    {
        get
        {
            lock (_vehicleTelemetryLock)
            {
                return _lastVehicleTelemetry;
            }
        }
    }

    public DebugTelemetrySnapshot LastDebugTelemetry
    {
        get
        {
            lock (_debugTelemetryLock)
            {
                return _lastDebugTelemetry;
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
                ReadTimeout = ReadTimeoutMs,
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
            _receiveTask = Task.Run(() => ReceiveLoop(_cts.Token));
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
        var wasRunning = IsRunning || _receiveTask is { IsCompleted: false };

        try
        {
            _cts?.Cancel();
            _sendTask?.Wait(1000);
            _receiveTask?.Wait(1000);
        }
        catch
        {
        }

        _sendTask = null;
        _receiveTask = null;
        _cts?.Dispose();
        _cts = null;

        _serialPort?.Dispose();
        _serialPort = null;

        _wheel?.Dispose();
        _wheel = null;
        SetTelemetry(TelemetrySnapshot.Empty);
        SetVehicleTelemetry(VehicleTelemetrySnapshot.Empty);
        SetDebugTelemetry(DebugTelemetrySnapshot.Empty);

        if (_backgroundError is not null)
        {
            EmitStatus($"Fehler: {_backgroundError.Message}");
        }
        else if (wasRunning)
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

    private void SetVehicleTelemetry(VehicleTelemetrySnapshot telemetry)
    {
        lock (_vehicleTelemetryLock)
        {
            _lastVehicleTelemetry = telemetry;
        }
    }

    private void SetDebugTelemetry(DebugTelemetrySnapshot telemetry)
    {
        lock (_debugTelemetryLock)
        {
            _lastDebugTelemetry = telemetry;
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

    private void ReceiveLoop(CancellationToken cancellationToken)
    {
        var serial = _serialPort ?? throw new InvalidOperationException("Serial-Port nicht offen.");
        var packet = new byte[StatusPacketSize];
        var state = 0;
        var debugLine = new StringBuilder(160);

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                byte value;

                try
                {
                    value = (byte)serial.ReadByte();
                }
                catch (TimeoutException)
                {
                    continue;
                }

                switch (state)
                {
                    case 0:
                        if (value == StatusHeader1)
                        {
                            packet[0] = value;
                            state = 1;
                            continue;
                        }

                        ConsumeDebugByte(value, debugLine);
                        break;

                    case 1:
                        if (value == StatusHeader2)
                        {
                            packet[1] = value;
                            state = 2;
                        }
                        else
                        {
                            ConsumeDebugByte(packet[0], debugLine);

                            if (value == StatusHeader1)
                            {
                                packet[0] = value;
                                state = 1;
                            }
                            else
                            {
                                ConsumeDebugByte(value, debugLine);
                                state = 0;
                            }
                        }
                        break;

                    default:
                        packet[state++] = value;

                        if (state == StatusPacketSize)
                        {
                            if (TryParseVehicleTelemetry(packet, out var telemetry))
                            {
                                SetVehicleTelemetry(telemetry);
                            }

                            state = 0;
                        }
                        break;
                }
            }
        }
        catch (Exception ex) when (ex is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            if (!cancellationToken.IsCancellationRequested)
            {
                _backgroundError = ex;
                EmitStatus($"Rueckkanal-Fehler: {ex.Message}");
            }
        }
    }

    private static bool TryParseVehicleTelemetry(byte[] packet, out VehicleTelemetrySnapshot telemetry)
    {
        byte checksum = 0;

        telemetry = VehicleTelemetrySnapshot.Empty;

        if (packet.Length != StatusPacketSize ||
            packet[0] != StatusHeader1 ||
            packet[1] != StatusHeader2 ||
            packet[2] != StatusPacketType)
        {
            return false;
        }

        for (var i = 0; i < StatusPacketSize - 1; i++)
        {
            checksum ^= packet[i];
        }

        if (checksum != packet[StatusPacketSize - 1])
        {
            return false;
        }

        var flags = packet[3];
        var gear = packet[4] switch
        {
            1 => 'N',
            2 => 'D',
            3 => 'R',
            _ => '-',
        };

        telemetry = new VehicleTelemetrySnapshot(
            LinkActive: (flags & 0x01) != 0,
            Gear: gear,
            SportMode: (flags & 0x02) != 0,
            MainLightOn: (flags & 0x04) != 0,
            CameraRearActive: (flags & 0x10) != 0,
            BatteryMv: (ushort)(packet[6] | (packet[7] << 8)),
            BatteryPercent: packet[5]);
        return true;
    }

    private void ConsumeDebugByte(byte value, StringBuilder buffer)
    {
        if (value == (byte)'\n')
        {
            var line = buffer.ToString().Trim();
            buffer.Clear();

            if (TryParseDebugTelemetry(line, out var telemetry))
            {
                SetDebugTelemetry(telemetry);
            }

            return;
        }

        if (value == (byte)'\r')
        {
            return;
        }

        if (value >= 32 && value <= 126)
        {
            if (buffer.Length < 159)
            {
                buffer.Append((char)value);
            }
        }
        else
        {
            buffer.Clear();
        }
    }

    private static bool TryParseDebugTelemetry(string line, out DebugTelemetrySnapshot telemetry)
    {
        telemetry = DebugTelemetrySnapshot.Empty;

        if (!line.StartsWith("!dbg ", StringComparison.Ordinal))
        {
            return false;
        }

        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var parts = line.Substring(5).Split(' ', StringSplitOptions.RemoveEmptyEntries);

        foreach (var part in parts)
        {
            var separator = part.IndexOf('=');
            if (separator <= 0 || separator >= (part.Length - 1))
            {
                continue;
            }

            values[part[..separator]] = part[(separator + 1)..];
        }

        telemetry = new DebugTelemetrySnapshot(
            HostPackets: ParseUInt(values, "host"),
            RxBytes: ParseUInt(values, "rx"),
            Frames: ParseUInt(values, "frm"),
            CrcErrors: ParseUInt(values, "crc"),
            FlightModeFrames: ParseUInt(values, "fm"),
            BatteryFrames: ParseUInt(values, "bat"),
            DeviceInfoFrames: ParseUInt(values, "dev"),
            LastTypeHex: values.TryGetValue("last", out var last) ? last : "00",
            Valid: ParseUInt(values, "valid") != 0U,
            Gear: values.TryGetValue("gear", out var gear) && gear.Length > 0 ? gear[0] : '-',
            BatteryMv: (ushort)Math.Min(ParseUInt(values, "mv"), ushort.MaxValue),
            BatteryPercent: (byte)Math.Min(ParseUInt(values, "pct"), byte.MaxValue));
        return true;
    }

    private static uint ParseUInt(Dictionary<string, string> values, string key)
    {
        return values.TryGetValue(key, out var text) && uint.TryParse(text, out var value) ? value : 0U;
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

    public readonly record struct VehicleTelemetrySnapshot(
        bool LinkActive,
        char Gear,
        bool SportMode,
        bool MainLightOn,
        bool CameraRearActive,
        ushort BatteryMv,
        byte BatteryPercent)
    {
        public static readonly VehicleTelemetrySnapshot Empty = new(false, '-', false, false, false, 0, 0);
    }

    public readonly record struct DebugTelemetrySnapshot(
        uint HostPackets,
        uint RxBytes,
        uint Frames,
        uint CrcErrors,
        uint FlightModeFrames,
        uint BatteryFrames,
        uint DeviceInfoFrames,
        string LastTypeHex,
        bool Valid,
        char Gear,
        ushort BatteryMv,
        byte BatteryPercent)
    {
        public static readonly DebugTelemetrySnapshot Empty = new(0, 0, 0, 0, 0, 0, 0, "00", false, '-', 0, 0);
    }

}
