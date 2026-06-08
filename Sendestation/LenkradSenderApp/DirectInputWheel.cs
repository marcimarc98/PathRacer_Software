using SharpDX.DirectInput;

namespace LenkradSenderApp;

public sealed class DirectInputWheel : IDisposable
{
    private readonly DirectInput _directInput;
    private readonly Joystick _joystick;

    public string DeviceName { get; }

    public DirectInputWheel()
    {
        _directInput = new DirectInput();

        var devices = GetAvailableDevices(_directInput);

        if (devices.Count == 0)
        {
            throw new InvalidOperationException("Kein Lenkrad oder Gamecontroller gefunden.");
        }

        var device = devices[0];
        DeviceName = device.InstanceName;
        _joystick = new Joystick(_directInput, device.InstanceGuid);

        foreach (var objectInstance in _joystick.GetObjects())
        {
            try
            {
                _joystick.GetObjectPropertiesById(objectInstance.ObjectId).Range = new InputRange(-1000, 1000);
            }
            catch
            {
            }
        }

        _joystick.Properties.BufferSize = 16;
        _joystick.Acquire();
    }

    private static List<DeviceInstance> GetAvailableDevices(DirectInput directInput)
    {
        return directInput
            .GetDevices(DeviceClass.GameControl, DeviceEnumerationFlags.AttachedOnly)
            .OrderByDescending(device => device.InstanceName.Contains("Thrustmaster", StringComparison.OrdinalIgnoreCase))
            .ThenBy(device => device.InstanceName, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    public string[] GetAvailableAxisNames()
    {
        return Enum.GetNames<WheelAxis>();
    }

    public JoystickState Poll()
    {
        try
        {
            _joystick.Poll();
            return _joystick.GetCurrentState();
        }
        catch
        {
            _joystick.Unacquire();
            _joystick.Acquire();
            _joystick.Poll();
            return _joystick.GetCurrentState();
        }
    }

    public static int ReadAxis(JoystickState state, WheelAxis axis)
    {
        return axis switch
        {
            WheelAxis.X => state.X,
            WheelAxis.Y => state.Y,
            WheelAxis.Z => state.Z,
            WheelAxis.RotationX => state.RotationX,
            WheelAxis.RotationY => state.RotationY,
            WheelAxis.RotationZ => state.RotationZ,
            WheelAxis.Slider0 => state.Sliders.Length > 0 ? state.Sliders[0] : 0,
            WheelAxis.Slider1 => state.Sliders.Length > 1 ? state.Sliders[1] : 0,
            _ => 0,
        };
    }

    public void Dispose()
    {
        _joystick.Unacquire();
        _joystick.Dispose();
        _directInput.Dispose();
    }
}
