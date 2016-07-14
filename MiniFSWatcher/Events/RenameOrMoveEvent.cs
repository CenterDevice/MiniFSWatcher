namespace CenterDevice.MiniFSWatcher.Events
{
    public class RenameOrMoveEvent: FileSystemEvent
    {
        public string OldFilename { get; internal set; }
    }
}
