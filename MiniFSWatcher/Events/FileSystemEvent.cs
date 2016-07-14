using CenterDevice.MiniFSWatcher.Types;

namespace CenterDevice.MiniFSWatcher.Events
{
    public class FileSystemEvent
    {
        public EventType Type { get; internal set; }
        public string Filename { get; internal set; }
        public ulong ProcessId { get; internal set; }
    }
}
