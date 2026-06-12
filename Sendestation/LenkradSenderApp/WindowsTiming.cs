using System.Runtime.InteropServices;

namespace LenkradSenderApp;

/// <summary>
/// Aktiviert fuer die Sende- und Lenkrad-Threads eine feinere Windows-Timer-Aufloesung.
/// Das reduziert Jitter, wenn mit hoher Wiederholrate Pakete erzeugt werden.
/// </summary>
internal static class WindowsTiming
{
    [DllImport("winmm.dll")]
    private static extern uint timeBeginPeriod(uint uPeriod);

    [DllImport("winmm.dll")]
    private static extern uint timeEndPeriod(uint uPeriod);

    /// <summary>
    /// Fordert 1 ms Timer-Aufloesung an und gibt ein Scope-Objekt zur Ruecknahme zurueck.
    /// </summary>
    public static IDisposable BeginHighResolution()
    {
        if (timeBeginPeriod(1) == 0)
        {
            return new PeriodScope();
        }

        return NoopScope.Instance;
    }

    /// <summary>
    /// Beendet die erhoehte Timer-Aufloesung, sobald der aufrufende Loop endet.
    /// </summary>
    private sealed class PeriodScope : IDisposable
    {
        private bool _disposed;

        /// <summary>
        /// Gibt die angeforderte Timer-Aufloesung genau einmal wieder frei.
        /// </summary>
        public void Dispose()
        {
            if (_disposed)
            {
                return;
            }

            _disposed = true;
            timeEndPeriod(1);
        }
    }

    /// <summary>
    /// Fallback, wenn Windows die hoehere Timer-Aufloesung nicht akzeptiert.
    /// </summary>
    private sealed class NoopScope : IDisposable
    {
        public static readonly NoopScope Instance = new();

        /// <summary>
        /// Kein Aufraeumen notwendig, weil keine Timer-Aufloesung gesetzt wurde.
        /// </summary>
        public void Dispose()
        {
        }
    }
}
