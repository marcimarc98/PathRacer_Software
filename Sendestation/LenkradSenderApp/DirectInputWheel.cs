using SharpDX.DirectInput;

namespace LenkradSenderApp;

/// <summary>
/// Kapselt den Zugriff auf das PC-Lenkrad ueber DirectInput.
/// Die Klasse normalisiert alle Achsen auf -1000 bis +1000 und liefert Rohdaten an den SenderService.
/// </summary>
public sealed class DirectInputWheel : IDisposable
{
    private readonly DirectInput _directInput;
    private readonly Joystick _joystick;

    public string DeviceName { get; }

    /// <summary>
    /// Sucht ein angeschlossenes Gamecontroller-/Lenkradgeraet und verbindet es.
    /// Thrustmaster-Geraete werden bevorzugt, damit das T80 automatisch zuerst verwendet wird.
    /// </summary>
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
                // Ein einheitlicher Wertebereich vereinfacht spaeter die Umrechnung in RC-Werte.
                _joystick.GetObjectPropertiesById(objectInstance.ObjectId).Range = new InputRange(-1000, 1000);
            }
            catch
            {
                // Einige DirectInput-Objekte erlauben keine Range-Aenderung. Diese werden ignoriert.
            }
        }

        _joystick.Properties.BufferSize = 16;
        _joystick.Acquire();
    }

    /// <summary>
    /// Listet angeschlossene DirectInput-Gamecontroller auf und sortiert bekannte Thrustmaster-Geraete nach vorne.
    /// </summary>
    private static List<DeviceInstance> GetAvailableDevices(DirectInput directInput)
    {
        return directInput
            .GetDevices(DeviceClass.GameControl, DeviceEnumerationFlags.AttachedOnly)
            .OrderByDescending(device => device.InstanceName.Contains("Thrustmaster", StringComparison.OrdinalIgnoreCase))
            .ThenBy(device => device.InstanceName, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    /// <summary>
    /// Liefert die Namen der bekannten Achsen fuer UI- oder Diagnosezwecke.
    /// </summary>
    public string[] GetAvailableAxisNames()
    {
        return Enum.GetNames<WheelAxis>();
    }

    /// <summary>
    /// Liest den aktuellen Lenkradzustand. Bei einem kurzen Verbindungsproblem wird neu acquiriert.
    /// </summary>
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

    /// <summary>
    /// Holt aus einem DirectInput-Zustand die gewuenschte Achse.
    /// Fehlende Slider werden als 0 behandelt.
    /// </summary>
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

    /// <summary>
    /// Gibt das DirectInput-Geraet frei, damit Windows es wieder normal verwalten kann.
    /// </summary>
    public void Dispose()
    {
        _joystick.Unacquire();
        _joystick.Dispose();
        _directInput.Dispose();
    }
}
