namespace LenkradSenderApp;

public static class HostPacket
{
    public const byte Header1 = 0xAA;
    public const byte Header2 = 0x55;
    public const int Size = 11;

    public static byte[] Build(short steering, ushort gas, ushort brake, ushort buttons)
    {
        var packet = new byte[Size];

        packet[0] = Header1;
        packet[1] = Header2;

        WriteInt16LittleEndian(packet, 2, steering);
        WriteUInt16LittleEndian(packet, 4, gas);
        WriteUInt16LittleEndian(packet, 6, brake);
        WriteUInt16LittleEndian(packet, 8, buttons);

        byte checksum = 0;
        for (var i = 0; i < Size - 1; i++)
        {
            checksum ^= packet[i];
        }

        packet[10] = checksum;
        return packet;
    }

    private static void WriteInt16LittleEndian(byte[] buffer, int offset, short value)
    {
        unchecked
        {
            buffer[offset] = (byte)(value & 0xFF);
            buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
        }
    }

    private static void WriteUInt16LittleEndian(byte[] buffer, int offset, ushort value)
    {
        buffer[offset] = (byte)(value & 0xFF);
        buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
    }
}
