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
    private const int PhysicalDownShiftButton = 0;
    private const int PhysicalUpShiftButton = 1;
    private const int PhysicalRearDiffLockButton = 2;
    private const int PhysicalFrontDiffLockButton = 3;
    private const int PhysicalRearDiffUnlockButton = 4;
    private const int PhysicalFrontDiffUnlockButton = 5;
    private const int PhysicalCameraMinusButton = 6;
    private const int PhysicalCameraPlusButton = 7;
    private const int PhysicalR2Button = 8;
    private const int PhysicalL2Button = 9;
    private const int PhysicalL1Button = 10;
    private const int PhysicalR1Button = 11;
    private const int PhysicalPsButton = 12;
    private const int LogicalReverseButton = 0;
    private const int LogicalDriveButton = 1;
    private const int LogicalCameraRearButton = 2;
    private const int LogicalNormalModeButton = 3;
    private const int LogicalFlashButton = 4;
    private const int LogicalMainLightButton = 5;
    private const int LogicalFrontDiffLockedButton = 6;
    private const int LogicalRearDiffLockedButton = 7;
    private const int CameraAngleCodeShift = 8;
    private const int CameraAngleMinimumDeg = -90;
    private const int CameraAngleMaximumDeg = 90;
    private const int CameraAngleStepDeg = 30;
    private const ushort NeutralControlWord = (ushort)(4 << CameraAngleCodeShift);
    private const byte StatusHeader1 = 0x5A;
    private const byte StatusHeader2 = 0xA5;
    private const byte StatusPacketType = 0x31;
    private const int StatusPacketSize = 20;

    private SerialPort? _serialPort;
    private DirectInputWheel? _wheel;
    private DirectInputWheel? _previewWheel;
    private CancellationTokenSource? _wheelCts;
    private CancellationTokenSource? _espCts;
    private Task? _wheelTask;
    private Task? _sendTask;
    private Task? _receiveTask;
    private string _statusText = "Bereit";
    private long _packetCount;
    private Exception? _backgroundError;
    private readonly object _telemetryLock = new();
    private TelemetrySnapshot _lastTelemetry = TelemetrySnapshot.Empty;
    private readonly object _rawInputLock = new();
    private RawInputSnapshot _lastRawInput = RawInputSnapshot.Empty;
    private readonly object _vehicleTelemetryLock = new();
    private VehicleTelemetrySnapshot _lastVehicleTelemetry = VehicleTelemetrySnapshot.Empty;
    private readonly object _debugTelemetryLock = new();
    private DebugTelemetrySnapshot _lastDebugTelemetry = DebugTelemetrySnapshot.Empty;
    private readonly object _controlStateLock = new();
    private LocalControlState _localControlState = new();
    private ControlStateSnapshot _lastControlState = ControlStateSnapshot.Default;
    private readonly object _outboundLock = new();
    private OutboundSnapshot _lastOutbound = OutboundSnapshot.Neutral;

    public event Action<string>? StatusMessage;

    public bool IsRunning => IsWheelRunning || IsEspConnected;
    public bool IsWheelRunning => _wheel is not null && _wheelCts is { IsCancellationRequested: false };
    public bool IsEspConnected => _serialPort is { IsOpen: true } && _espCts is { IsCancellationRequested: false };
    public string WheelName => _wheel?.DeviceName ?? _previewWheel?.DeviceName ?? "Nicht verbunden";
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

    public RawInputSnapshot LastRawInput
    {
        get
        {
            lock (_rawInputLock)
            {
                return _lastRawInput;
            }
        }
    }

    public ControlStateSnapshot LastControlState
    {
        get
        {
            lock (_controlStateLock)
            {
                return _lastControlState;
            }
        }
    }

    public void Start(string portName)
    {
        var startedWheel = false;

        if (!IsWheelRunning)
        {
            StartWheel();
            startedWheel = true;
        }

        try
        {
            ConnectEsp(portName);
        }
        catch
        {
            if (startedWheel)
            {
                StopWheel();
            }

            throw;
        }
    }

    public void StartWheel()
    {
        if (IsWheelRunning)
        {
            throw new InvalidOperationException("Lenkrad laeuft bereits.");
        }

        EmitStatus("Lenkrad wird verbunden...");
        _backgroundError = null;
        ResetControlState();

        try
        {
            _previewWheel?.Dispose();
            _previewWheel = null;
            _wheel?.Dispose();
            _wheel = new DirectInputWheel();

            EmitStatus($"Lenkrad erkannt: {_wheel.DeviceName}");
            EmitStatus("Archiv-Mapping aktiv: Lenkung=X, Gas=RZ, Bremse=Y");

            _wheelCts = new CancellationTokenSource();
            _wheelTask = Task.Run(() => WheelLoop(_wheelCts.Token));
            EmitStatus("Lenkrad-Loop gestartet.");
        }
        catch
        {
            _wheelCts?.Dispose();
            _wheelCts = null;
            _wheelTask = null;
            _wheel?.Dispose();
            _wheel = null;
            throw;
        }
    }

    public void StopWheel()
    {
        var wasRunning = IsWheelRunning;

        try
        {
            _wheelCts?.Cancel();
            _wheelTask?.Wait(1000);
        }
        catch
        {
        }

        _wheelTask = null;
        _wheelCts?.Dispose();
        _wheelCts = null;
        _wheel?.Dispose();
        _wheel = null;

        ResetControlState();
        SetTelemetry(TelemetrySnapshot.Empty);

        if (_backgroundError is not null)
        {
            EmitStatus($"Fehler: {_backgroundError.Message}");
        }
        else if (wasRunning)
        {
            EmitStatus("Lenkrad gestoppt");
        }
    }

    public void ConnectEsp(string portName)
    {
        if (IsEspConnected)
        {
            throw new InvalidOperationException("ESP-Verbindung laeuft bereits.");
        }

        EmitStatus("ESP wird verbunden...");
        _backgroundError = null;

        try
        {
            _serialPort?.Dispose();
            _serialPort = new SerialPort(portName, BaudRate)
            {
                Handshake = Handshake.None,
                ReadTimeout = ReadTimeoutMs,
                WriteTimeout = 250,
                WriteBufferSize = 4096,
                DtrEnable = false,
                RtsEnable = false,
            };

            _serialPort.Open();
            Thread.Sleep(1000);
            EmitStatus($"ESP verbunden: {portName} @ {BaudRate}");

            SetVehicleTelemetry(VehicleTelemetrySnapshot.Empty);
            SetDebugTelemetry(DebugTelemetrySnapshot.Empty);
            Interlocked.Exchange(ref _packetCount, 0);

            _espCts = new CancellationTokenSource();
            _sendTask = Task.Run(() => SendLoop(_espCts.Token));
            _receiveTask = Task.Run(() => ReceiveLoop(_espCts.Token));
            EmitStatus("ESP-Sende-/Rueckkanal-Loop gestartet.");
        }
        catch
        {
            DisconnectEsp();
            throw;
        }
    }

    public void DisconnectEsp()
    {
        var wasConnected = IsEspConnected || _receiveTask is { IsCompleted: false };

        try
        {
            _espCts?.Cancel();
            _sendTask?.Wait(1000);
            _receiveTask?.Wait(1000);
        }
        catch
        {
        }

        _sendTask = null;
        _receiveTask = null;
        _espCts?.Dispose();
        _espCts = null;

        _serialPort?.Dispose();
        _serialPort = null;

        SetVehicleTelemetry(VehicleTelemetrySnapshot.Empty);
        SetDebugTelemetry(DebugTelemetrySnapshot.Empty);

        if (_backgroundError is not null)
        {
            EmitStatus($"Fehler: {_backgroundError.Message}");
        }
        else if (wasConnected)
        {
            EmitStatus("ESP getrennt");
        }
    }

    public void Stop()
    {
        DisconnectEsp();
        StopWheel();

        _previewWheel?.Dispose();
        _previewWheel = null;
    }

    public void RefreshLocalPreview()
    {
        if (IsWheelRunning)
        {
            return;
        }

        try
        {
            _previewWheel ??= new DirectInputWheel();
            var state = _previewWheel.Poll();

            PublishWheelState(state, updateOutbound: false);
        }
        catch
        {
            _previewWheel?.Dispose();
            _previewWheel = null;
        }
    }

    private void WheelLoop(CancellationToken cancellationToken)
    {
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
                PublishWheelState(state, updateOutbound: true);

                nextTick += ticksPerPacket;
                WaitUntil(stopwatch, nextTick, cancellationToken);
            }
        }
        catch (Exception ex)
        {
            _backgroundError = ex;
            EmitStatus($"Lenkrad-Fehler: {ex.Message}");
        }
    }

    private void SendLoop(CancellationToken cancellationToken)
    {
        var serial = _serialPort ?? throw new InvalidOperationException("Serial-Port nicht offen.");

        using var timerScope = WindowsTiming.BeginHighResolution();

        Thread.CurrentThread.Priority = ThreadPriority.Highest;

        var stopwatch = Stopwatch.StartNew();
        var ticksPerPacket = Stopwatch.Frequency / SendHz;
        var nextTick = stopwatch.ElapsedTicks;

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var outbound = GetOutboundSnapshot();
                var packet = HostPacket.Build(outbound.Steering, outbound.Gas, outbound.Brake, outbound.Buttons);
                serial.Write(packet, 0, packet.Length);

                Interlocked.Increment(ref _packetCount);

                nextTick += ticksPerPacket;
                WaitUntil(stopwatch, nextTick, cancellationToken);
            }
        }
        catch (Exception ex) when (ex is InvalidOperationException or IOException or UnauthorizedAccessException or TimeoutException)
        {
            if (!cancellationToken.IsCancellationRequested)
            {
                _backgroundError = ex;
                EmitStatus($"ESP-Sende-Fehler: {ex.Message}");
            }
        }
    }

    private void SetTelemetry(TelemetrySnapshot telemetry)
    {
        lock (_telemetryLock)
        {
            _lastTelemetry = telemetry;
        }
    }

    private void SetRawInput(RawInputSnapshot rawInput)
    {
        lock (_rawInputLock)
        {
            _lastRawInput = rawInput;
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

    private void SetOutboundSnapshot(OutboundSnapshot snapshot)
    {
        lock (_outboundLock)
        {
            _lastOutbound = snapshot;
        }
    }

    private OutboundSnapshot GetOutboundSnapshot()
    {
        lock (_outboundLock)
        {
            return _lastOutbound;
        }
    }

    private void EmitStatus(string message)
    {
        _statusText = message;
        StatusMessage?.Invoke(message);
    }

    private void PublishWheelState(JoystickState state, bool updateOutbound)
    {
        var steeringRaw = DirectInputWheel.ReadAxis(state, SteeringAxis);
        var gasRaw = DirectInputWheel.ReadAxis(state, GasAxis);
        var brakeRaw = DirectInputWheel.ReadAxis(state, BrakeAxis);

        var steering = ToArchiveSteering(steeringRaw);
        var gas = ToArchiveGas(gasRaw);
        var brake = ToArchiveBrake(brakeRaw);
        var pressedButtons = string.Join(",",
            state.Buttons
                .Select((pressed, index) => (pressed, index))
                .Where(item => item.pressed)
                .Select(item => item.index));
        var buttons = BuildLogicalButtons(state);

        SetTelemetry(new TelemetrySnapshot(steering, gas, brake, steeringRaw, gasRaw, brakeRaw));
        SetRawInput(new RawInputSnapshot(pressedButtons.Length == 0 ? "-" : pressedButtons));

        if (updateOutbound)
        {
            SetOutboundSnapshot(new OutboundSnapshot(steering, gas, brake, buttons));
        }
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

    private void ResetControlState()
    {
        lock (_controlStateLock)
        {
            _localControlState = new LocalControlState();
            _lastControlState = ControlStateSnapshot.Default;
        }

        SetOutboundSnapshot(OutboundSnapshot.Neutral);
    }

    private ushort BuildLogicalButtons(JoystickState state)
    {
        lock (_controlStateLock)
        {
            var downShiftPressed = IsPhysicalButtonPressed(state, PhysicalDownShiftButton);
            var upShiftPressed = IsPhysicalButtonPressed(state, PhysicalUpShiftButton);
            var rearDiffLockPressed = IsPhysicalButtonPressed(state, PhysicalRearDiffLockButton);
            var frontDiffLockPressed = IsPhysicalButtonPressed(state, PhysicalFrontDiffLockButton);
            var rearDiffUnlockPressed = IsPhysicalButtonPressed(state, PhysicalRearDiffUnlockButton);
            var frontDiffUnlockPressed = IsPhysicalButtonPressed(state, PhysicalFrontDiffUnlockButton);
            var cameraMinusPressed = IsPhysicalButtonPressed(state, PhysicalCameraMinusButton);
            var cameraPlusPressed = IsPhysicalButtonPressed(state, PhysicalCameraPlusButton);
            var r2Pressed = IsPhysicalButtonPressed(state, PhysicalR2Button);
            var l2Pressed = IsPhysicalButtonPressed(state, PhysicalL2Button);
            var l1Pressed = IsPhysicalButtonPressed(state, PhysicalL1Button);
            var r1Pressed = IsPhysicalButtonPressed(state, PhysicalR1Button);
            var psPressed = IsPhysicalButtonPressed(state, PhysicalPsButton);

            var downShiftRising = downShiftPressed && !_localControlState.PrevDownShiftPressed;
            var upShiftRising = upShiftPressed && !_localControlState.PrevUpShiftPressed;
            var rearDiffLockRising = rearDiffLockPressed && !_localControlState.PrevRearDiffLockPressed;
            var frontDiffLockRising = frontDiffLockPressed && !_localControlState.PrevFrontDiffLockPressed;
            var rearDiffUnlockRising = rearDiffUnlockPressed && !_localControlState.PrevRearDiffUnlockPressed;
            var frontDiffUnlockRising = frontDiffUnlockPressed && !_localControlState.PrevFrontDiffUnlockPressed;
            var cameraMinusRising = cameraMinusPressed && !_localControlState.PrevCameraMinusPressed;
            var cameraPlusRising = cameraPlusPressed && !_localControlState.PrevCameraPlusPressed;
            var r2Rising = r2Pressed && !_localControlState.PrevR2Pressed;
            var l2Rising = l2Pressed && !_localControlState.PrevL2Pressed;
            var l1Rising = l1Pressed && !_localControlState.PrevL1Pressed;
            var r1Rising = r1Pressed && !_localControlState.PrevR1Pressed;
            var l1Falling = !l1Pressed && _localControlState.PrevL1Pressed;
            var r1Falling = !r1Pressed && _localControlState.PrevR1Pressed;
            var psRising = psPressed && !_localControlState.PrevPsPressed;
            var comboHeld = l1Pressed && r1Pressed;

            if (r1Rising)
            {
                if (!l1Pressed)
                {
                    _localControlState.MainLightOn = !_localControlState.MainLightOn;
                }
            }

            if (comboHeld)
            {
                _localControlState.ComboLatched = true;
            }

            if (l1Falling)
            {
                _localControlState.ComboLatched = false;
            }

            if (!l1Pressed && !r1Pressed)
            {
                _localControlState.ComboLatched = false;
            }

            if (psRising)
            {
                _localControlState.Gear = 'N';
                _localControlState.ShiftGateActive = false;
                _localControlState.NeutralUnlocked = false;
            }
            else if (comboHeld || _localControlState.ShiftGateActive)
            {
                if (upShiftRising)
                {
                    _localControlState.Gear = 'D';
                    _localControlState.ShiftGateActive = true;
                }
                else if (downShiftRising)
                {
                    _localControlState.Gear = 'R';
                    _localControlState.ShiftGateActive = true;
                }
            }

            _localControlState.NeutralUnlocked = comboHeld || _localControlState.ShiftGateActive;

            if (l2Rising)
            {
                _localControlState.SportMode = !_localControlState.SportMode;
            }

            if (r2Rising)
            {
                _localControlState.CameraRearActive = !_localControlState.CameraRearActive;
            }

            if (cameraMinusRising)
            {
                _localControlState.CameraPanAngleDeg = ClampCameraAngle(_localControlState.CameraPanAngleDeg - CameraAngleStepDeg);
            }

            if (cameraPlusRising)
            {
                _localControlState.CameraPanAngleDeg = ClampCameraAngle(_localControlState.CameraPanAngleDeg + CameraAngleStepDeg);
            }

            if (frontDiffLockRising)
            {
                _localControlState.FrontDiffLocked = true;
            }

            if (frontDiffUnlockRising)
            {
                _localControlState.FrontDiffLocked = false;
            }

            if (rearDiffLockRising)
            {
                _localControlState.RearDiffLocked = true;
            }

            if (rearDiffUnlockRising)
            {
                _localControlState.RearDiffLocked = false;
            }

            _localControlState.FlashActive = l1Pressed && !_localControlState.ComboLatched && !r1Pressed;

            _localControlState.PrevDownShiftPressed = downShiftPressed;
            _localControlState.PrevUpShiftPressed = upShiftPressed;
            _localControlState.PrevRearDiffLockPressed = rearDiffLockPressed;
            _localControlState.PrevFrontDiffLockPressed = frontDiffLockPressed;
            _localControlState.PrevRearDiffUnlockPressed = rearDiffUnlockPressed;
            _localControlState.PrevFrontDiffUnlockPressed = frontDiffUnlockPressed;
            _localControlState.PrevCameraMinusPressed = cameraMinusPressed;
            _localControlState.PrevCameraPlusPressed = cameraPlusPressed;
            _localControlState.PrevR2Pressed = r2Pressed;
            _localControlState.PrevL2Pressed = l2Pressed;
            _localControlState.PrevL1Pressed = l1Pressed;
            _localControlState.PrevR1Pressed = r1Pressed;
            _localControlState.PrevPsPressed = psPressed;

            ushort buttons = 0;

            if (_localControlState.Gear == 'R')
            {
                buttons |= (ushort)(1 << LogicalReverseButton);
            }

            if (_localControlState.Gear == 'D')
            {
                buttons |= (ushort)(1 << LogicalDriveButton);
            }

            if (_localControlState.CameraRearActive)
            {
                buttons |= (ushort)(1 << LogicalCameraRearButton);
            }

            if (_localControlState.SportMode)
            {
                buttons |= (ushort)(1 << LogicalNormalModeButton);
            }

            if (_localControlState.FlashActive)
            {
                buttons |= (ushort)(1 << LogicalFlashButton);
            }

            if (_localControlState.MainLightOn)
            {
                buttons |= (ushort)(1 << LogicalMainLightButton);
            }

            if (_localControlState.FrontDiffLocked)
            {
                buttons |= (ushort)(1 << LogicalFrontDiffLockedButton);
            }

            if (_localControlState.RearDiffLocked)
            {
                buttons |= (ushort)(1 << LogicalRearDiffLockedButton);
            }

            buttons |= EncodeCameraAngle(_localControlState.CameraPanAngleDeg);

            _lastControlState = new ControlStateSnapshot(
                Gear: _localControlState.Gear,
                NeutralUnlocked: _localControlState.NeutralUnlocked,
                SportMode: _localControlState.SportMode,
                CameraRearActive: _localControlState.CameraRearActive,
                CameraPanAngleDeg: _localControlState.CameraPanAngleDeg,
                FrontDiffLocked: _localControlState.FrontDiffLocked,
                RearDiffLocked: _localControlState.RearDiffLocked,
                MainLightOn: _localControlState.MainLightOn,
                FlashActive: _localControlState.FlashActive);

            return buttons;
        }
    }

    private static bool IsPhysicalButtonPressed(JoystickState state, int index)
    {
        var buttons = state.Buttons;
        return (index >= 0) && (index < buttons.Length) && buttons[index];
    }

    private static int ClampCameraAngle(int angleDeg)
    {
        return Math.Clamp(angleDeg, CameraAngleMinimumDeg, CameraAngleMaximumDeg);
    }

    private static ushort EncodeCameraAngle(int angleDeg)
    {
        var clampedAngle = ClampCameraAngle(angleDeg);
        var angleCode = ((clampedAngle - CameraAngleMinimumDeg) / CameraAngleStepDeg) + 1;

        return (ushort)(angleCode << CameraAngleCodeShift);
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
        var cameraAngleCode = (packet[18] >> 4) & 0x07;
        var gear = packet[4] switch
        {
            1 => 'N',
            2 => 'D',
            3 => 'R',
            _ => '-',
        };

        telemetry = new VehicleTelemetrySnapshot(
            LinkActive: (flags & 0x01) != 0,
            VehicleStatusValid: (flags & 0x08) != 0,
            Gear: gear,
            SportMode: (flags & 0x02) != 0,
            MainLightOn: (flags & 0x04) != 0,
            CameraRearActive: (flags & 0x10) != 0,
            CameraPanAngleDeg: DecodeCameraAngle(cameraAngleCode),
            FrontDiffLocked: (flags & 0x20) != 0,
            RearDiffLocked: (flags & 0x40) != 0,
            BatteryMv: (ushort)(packet[6] | (packet[7] << 8)),
            BatteryPercent: packet[5],
            BatteryTempC: (short)(packet[8] | (packet[9] << 8)),
            UplinkLq: packet[10],
            UplinkRssi: packet[11],
            UplinkSnr: unchecked((sbyte)packet[12]),
            DownlinkLq: packet[13],
            DownlinkRssi: packet[14],
            DownlinkSnr: unchecked((sbyte)packet[15]),
            RfProfile: packet[16],
            TxPower: packet[17]);
        return true;
    }

    private static int DecodeCameraAngle(int angleCode)
    {
        return angleCode is >= 1 and <= 7
            ? CameraAngleMinimumDeg + ((angleCode - 1) * CameraAngleStepDeg)
            : 0;
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
            LinkStatsFrames: ParseUInt(values, "ls"),
            DeviceInfoFrames: ParseUInt(values, "dev"),
            LastTypeHex: values.TryGetValue("last", out var last) ? last : "00",
            Valid: ParseUInt(values, "valid") != 0U,
            Gear: values.TryGetValue("gear", out var gear) && gear.Length > 0 ? gear[0] : '-',
            BatteryMv: (ushort)Math.Min(ParseUInt(values, "mv"), ushort.MaxValue),
            BatteryPercent: (byte)Math.Min(ParseUInt(values, "pct"), byte.MaxValue),
            BatteryTempC: ParseInt(values, "tmp"));
        return true;
    }

    private static uint ParseUInt(Dictionary<string, string> values, string key)
    {
        return values.TryGetValue(key, out var text) && uint.TryParse(text, out var value) ? value : 0U;
    }

    private static short ParseInt(Dictionary<string, string> values, string key)
    {
        return values.TryGetValue(key, out var text) && short.TryParse(text, out var value) ? value : (short)0;
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

    public readonly record struct RawInputSnapshot(string PressedButtons)
    {
        public static readonly RawInputSnapshot Empty = new("-");
    }

    public readonly record struct VehicleTelemetrySnapshot(
        bool LinkActive,
        bool VehicleStatusValid,
        char Gear,
        bool SportMode,
        bool MainLightOn,
        bool CameraRearActive,
        int CameraPanAngleDeg,
        bool FrontDiffLocked,
        bool RearDiffLocked,
        ushort BatteryMv,
        byte BatteryPercent,
        short BatteryTempC,
        byte UplinkLq,
        byte UplinkRssi,
        sbyte UplinkSnr,
        byte DownlinkLq,
        byte DownlinkRssi,
        sbyte DownlinkSnr,
        byte RfProfile,
        byte TxPower)
    {
        public static readonly VehicleTelemetrySnapshot Empty = new(false, false, '-', false, false, false, 0, false, false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    public readonly record struct DebugTelemetrySnapshot(
        uint HostPackets,
        uint RxBytes,
        uint Frames,
        uint CrcErrors,
        uint FlightModeFrames,
        uint BatteryFrames,
        uint LinkStatsFrames,
        uint DeviceInfoFrames,
        string LastTypeHex,
        bool Valid,
        char Gear,
        ushort BatteryMv,
        byte BatteryPercent,
        short BatteryTempC)
    {
        public static readonly DebugTelemetrySnapshot Empty = new(0, 0, 0, 0, 0, 0, 0, 0, "00", false, '-', 0, 0, 0);
    }

    public readonly record struct ControlStateSnapshot(
        char Gear,
        bool NeutralUnlocked,
        bool SportMode,
        bool CameraRearActive,
        int CameraPanAngleDeg,
        bool FrontDiffLocked,
        bool RearDiffLocked,
        bool MainLightOn,
        bool FlashActive)
    {
        public static readonly ControlStateSnapshot Default = new('N', false, false, false, 0, false, false, false, false);
    }

    private readonly record struct OutboundSnapshot(
        short Steering,
        ushort Gas,
        ushort Brake,
        ushort Buttons)
    {
        public static readonly OutboundSnapshot Neutral = new(0, 0, 0, NeutralControlWord);
    }

    private sealed class LocalControlState
    {
        public char Gear { get; set; } = 'N';
        public bool NeutralUnlocked { get; set; }
        public bool SportMode { get; set; }
        public bool CameraRearActive { get; set; }
        public int CameraPanAngleDeg { get; set; }
        public bool FrontDiffLocked { get; set; }
        public bool RearDiffLocked { get; set; }
        public bool MainLightOn { get; set; }
        public bool FlashActive { get; set; }
        public bool ShiftGateActive { get; set; }
        public bool PrevDownShiftPressed { get; set; }
        public bool PrevUpShiftPressed { get; set; }
        public bool PrevRearDiffLockPressed { get; set; }
        public bool PrevFrontDiffLockPressed { get; set; }
        public bool PrevRearDiffUnlockPressed { get; set; }
        public bool PrevFrontDiffUnlockPressed { get; set; }
        public bool PrevCameraMinusPressed { get; set; }
        public bool PrevCameraPlusPressed { get; set; }
        public bool PrevR2Pressed { get; set; }
        public bool PrevL2Pressed { get; set; }
        public bool PrevL1Pressed { get; set; }
        public bool PrevR1Pressed { get; set; }
        public bool PrevPsPressed { get; set; }
        public bool ComboLatched { get; set; }
    }

}
