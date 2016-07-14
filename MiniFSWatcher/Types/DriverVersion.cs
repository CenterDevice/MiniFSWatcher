using System.Runtime.InteropServices;

namespace CenterDevice.MiniFSWatcher.Types
{
    [StructLayout(LayoutKind.Sequential)]
    public struct DriverVersion
    {
        public ushort Major;
        public ushort Minor;

        public DriverVersion(ushort major, ushort minor)
        {
            this.Major = major;
            this.Minor = minor;
        }
    }
}
