namespace LenkradSenderApp;

/// <summary>
/// Einheitliche Namen fuer die von DirectInput gelieferten Achsen.
/// Die konkrete Zuordnung zum Lenkrad wird im SenderService festgelegt.
/// </summary>
public enum WheelAxis
{
    X,
    Y,
    Z,
    RotationX,
    RotationY,
    RotationZ,
    Slider0,
    Slider1,
}
