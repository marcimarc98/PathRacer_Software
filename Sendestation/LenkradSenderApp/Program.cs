namespace LenkradSenderApp;

/// <summary>
/// Startpunkt der WinForms-Anwendung.
/// Initialisiert die Windows-Forms-Laufzeit und oeffnet die Hauptmaske.
/// </summary>
static class Program
{
    /// <summary>
    /// Einstiegspunkt der Anwendung. STAThread ist fuer WinForms und COM-basierte
    /// Eingabebibliotheken wie DirectInput erforderlich.
    /// </summary>
    [STAThread]
    static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new Form1());
    }    
}
