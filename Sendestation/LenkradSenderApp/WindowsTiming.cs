using System.Runtime.InteropServices;

namespace LenkradSenderApp;

internal static class WindowsTiming
{
    [DllImport("winmm.dll")]
    private static extern uint timeBeginPeriod(uint uPeriod);

    [DllImport("winmm.dll")]
    private static extern uint timeEndPeriod(uint uPeriod);

    public static IDisposable BeginHighResolution()
    {
        if (timeBeginPeriod(1) == 0)
        {
            return new PeriodScope();
        }

        return NoopScope.Instance;
    }

    private sealed class PeriodScope : IDisposable
    {
        private bool _disposed;

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

    private sealed class NoopScope : IDisposable
    {
        public static readonly NoopScope Instance = new();

        public void Dispose()
        {
        }
    }
}
