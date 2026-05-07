using System.IO.Ports;

namespace LenkradSenderApp;

public partial class Form1 : Form
{
    private readonly SenderService _senderService = new();
    private readonly System.Windows.Forms.Timer _statusTimer = new();
    private string _lastStatusLine = string.Empty;

    public Form1()
    {
        InitializeComponent();

        _senderService.StatusMessage += HandleStatusMessage;
        RefreshComPorts();
        ProbeWheel();
        AppendStatus("App bereit.");

        _statusTimer.Interval = 200;
        _statusTimer.Tick += (_, _) => UpdateStatus();
        _statusTimer.Start();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _statusTimer.Stop();
        _senderService.StatusMessage -= HandleStatusMessage;
        _senderService.Dispose();
        base.OnFormClosing(e);
    }

    private void buttonRefreshPorts_Click(object sender, EventArgs e)
    {
        RefreshComPorts();
    }

    private void buttonStart_Click(object sender, EventArgs e)
    {
        if (comboPorts.SelectedItem is not string portName || string.IsNullOrWhiteSpace(portName))
        {
            MessageBox.Show(this, "Bitte zuerst einen COM-Port auswaehlen.", "COM-Port fehlt");
            return;
        }

        try
        {
            _senderService.Start(portName);

            AppendStatus($"Senden gestartet auf {portName}.");
        }
        catch (Exception ex)
        {
            AppendStatus($"Start fehlgeschlagen: {ex.Message}");
            MessageBox.Show(this, ex.Message, "Start fehlgeschlagen");
        }

        UpdateStatus();
    }

    private void buttonStop_Click(object sender, EventArgs e)
    {
        _senderService.Stop();
        AppendStatus("Senden gestoppt.");
        UpdateStatus();
    }

    private void RefreshComPorts()
    {
        var previous = comboPorts.SelectedItem as string;
        var ports = SerialPort.GetPortNames().OrderBy(name => name, StringComparer.OrdinalIgnoreCase).ToArray();

        comboPorts.BeginUpdate();
        comboPorts.Items.Clear();
        comboPorts.Items.AddRange(ports);
        comboPorts.EndUpdate();

        if (!string.IsNullOrWhiteSpace(previous) && ports.Contains(previous, StringComparer.OrdinalIgnoreCase))
        {
            comboPorts.SelectedItem = previous;
        }
        else if (ports.Length > 0)
        {
            comboPorts.SelectedIndex = 0;
        }

        AppendStatus($"COM-Ports aktualisiert: {string.Join(", ", ports)}");
    }

    private void UpdateStatus()
    {
        labelWheelValue.Text = _senderService.WheelName;
        labelSenderValue.Text = _senderService.IsRunning ? "Aktiv" : "Inaktiv";
        labelPacketsValue.Text = _senderService.PacketCount.ToString();
        labelStateValue.Text = _senderService.StatusText;
        var telemetry = _senderService.LastTelemetry;
        labelSteeringLiveValue.Text = telemetry.SteeringRaw.ToString();
        labelGasLiveValue.Text = telemetry.GasRaw.ToString();
        labelBrakeLiveValue.Text = telemetry.BrakeRaw.ToString();
        UpdateVehicleStatus(_senderService.LastVehicleTelemetry);

        buttonStart.Enabled = !_senderService.IsRunning;
        buttonStop.Enabled = _senderService.IsRunning;
        comboPorts.Enabled = !_senderService.IsRunning;
    }

    private void UpdateVehicleStatus(SenderService.VehicleTelemetrySnapshot telemetry)
    {
        labelVehicleLinkValue.Text = telemetry.LinkActive ? "Aktiv" : "Inaktiv";

        if (telemetry.LinkActive)
        {
            labelVehicleModeValue.Text = telemetry.SportMode ? "Sport" : "Normal";
            labelVehicleCameraValue.Text = telemetry.CameraRearActive ? "Hinten" : "Vorne";
            labelVehicleLightValue.Text = telemetry.MainLightOn ? "Ein" : "Aus";
            labelVehicleBatteryValue.Text = telemetry.BatteryMv > 0
                ? $"{telemetry.BatteryMv} mV / {telemetry.BatteryPercent} %"
                : "--";
            labelGearValue.Text = telemetry.Gear.ToString();
            labelGearValue.ForeColor = telemetry.Gear switch
            {
                'R' => Color.Firebrick,
                'N' => Color.ForestGreen,
                'D' => Color.RoyalBlue,
                _ => Color.DimGray,
            };
        }
        else
        {
            labelVehicleModeValue.Text = "--";
            labelVehicleCameraValue.Text = "--";
            labelVehicleLightValue.Text = "--";
            labelVehicleBatteryValue.Text = "--";
            labelGearValue.Text = "-";
            labelGearValue.ForeColor = Color.DimGray;
        }

        var debug = _senderService.LastDebugTelemetry;
        labelVehicleDebugValue.Text =
            $"RX {debug.RxBytes}   FRM {debug.Frames}   CRC {debug.CrcErrors}{Environment.NewLine}" +
            $"FM {debug.FlightModeFrames}   BAT {debug.BatteryFrames}   DEV {debug.DeviceInfoFrames}   LAST 0x{debug.LastTypeHex}{Environment.NewLine}" +
            $"VALID {(debug.Valid ? 1 : 0)}   GEAR {debug.Gear}   MV {debug.BatteryMv}   PCT {debug.BatteryPercent}";
    }

    private void ProbeWheel()
    {
        try
        {
            var wheelName = DirectInputWheel.ProbePreferredDeviceName();

            if (string.IsNullOrWhiteSpace(wheelName))
            {
                AppendStatus("Kein Lenkrad gefunden.");
                labelWheelValue.Text = "Nicht gefunden";
                return;
            }

            AppendStatus($"Lenkrad gefunden: {wheelName}");
            labelWheelValue.Text = wheelName;
        }
        catch (Exception ex)
        {
            AppendStatus($"Lenkrad-Pruefung fehlgeschlagen: {ex.Message}");
        }
    }

    private void HandleStatusMessage(string message)
    {
        if (IsDisposed)
        {
            return;
        }

        if (InvokeRequired)
        {
            BeginInvoke(() => HandleStatusMessage(message));
            return;
        }

        if (string.Equals(_lastStatusLine, message, StringComparison.Ordinal))
        {
            return;
        }

        _lastStatusLine = message;
        AppendStatus(message);
    }

    private void AppendStatus(string message)
    {
        var line = $"[{DateTime.Now:HH:mm:ss}] {message}";

        if (textStatusLog.TextLength > 0)
        {
            textStatusLog.AppendText(Environment.NewLine);
        }

        textStatusLog.AppendText(line);
        textStatusLog.SelectionStart = textStatusLog.TextLength;
        textStatusLog.ScrollToCaret();
    }
}
