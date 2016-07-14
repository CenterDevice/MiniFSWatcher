using System.Runtime.InteropServices;

namespace CenterDevice.MiniFSWatcher.Types
{
    [StructLayout(LayoutKind.Sequential)]
    struct CommandMessage
    {
        public MinispyCommand Command;
        public int Reserved;
    }
}
