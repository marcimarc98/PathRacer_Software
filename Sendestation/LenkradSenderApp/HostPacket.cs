namespace LenkradSenderApp;

/// <summary>
/// Baut das serielle Paket, das von der Windows-App an den ESP32 gesendet wird.
/// Das Format ist bewusst klein und fest, damit der ESP32 es einfach parsen kann.
/// </summary>
public static class HostPacket
{
    // Zwei Headerbytes synchronisieren den Parser auf ESP32-Seite.
    public const byte Header1 = 0xAA;
    public const byte Header2 = 0x55;

    // Paketgroesse: 2 Header + 3 Werte zu je 2 Byte + 2 Byte Steuerwort + 1 Byte XOR.
    public const int Size = 11;

    /// <summary>
    /// Erstellt ein komplettes Hostpaket mit Lenkung, Gas, Bremse und Steuerwort.
    /// Alle Mehrbyte-Werte werden little endian abgelegt.
    /// </summary>
    public static byte[] Build(short steering, ushort gas, ushort brake, ushort buttons)
    {
        var packet = new byte[Size];

        packet[0] = Header1;
        packet[1] = Header2;

        WriteInt16LittleEndian(packet, 2, steering);
        WriteUInt16LittleEndian(packet, 4, gas);
        WriteUInt16LittleEndian(packet, 6, brake);
        WriteUInt16LittleEndian(packet, 8, buttons);

        // XOR-Pruefsumme ist einfach, schnell und reicht fuer die kurze USB-Serial-Strecke.
        byte checksum = 0;
        for (var i = 0; i < Size - 1; i++)
        {
            checksum ^= packet[i];
        }

        packet[10] = checksum;
        return packet;
    }

    /// <summary>
    /// Schreibt einen vorzeichenbehafteten 16-Bit-Wert in little endian.
    /// </summary>
    private static void WriteInt16LittleEndian(byte[] buffer, int offset, short value)
    {
        unchecked
        {
            buffer[offset] = (byte)(value & 0xFF);
            buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
        }
    }

    /// <summary>
    /// Schreibt einen vorzeichenlosen 16-Bit-Wert in little endian.
    /// </summary>
    private static void WriteUInt16LittleEndian(byte[] buffer, int offset, ushort value)
    {
        buffer[offset] = (byte)(value & 0xFF);
        buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
    }
}
