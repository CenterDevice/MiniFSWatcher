using System.Runtime.InteropServices;

namespace CenterDevice.MiniFSWatcher.Types
{
    [StructLayout(LayoutKind.Sequential)]
    struct RecordData
    {
        public ulong OriginatingTime;
        public ulong CompletionTime;

        public EventType EventType;

        public int Flags;
        public ulong ProcessId;
    }

}
