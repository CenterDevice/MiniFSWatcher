namespace CenterDevice.MiniFSWatcher.Types
{
    public enum EventType: int
    {
        Unknown = 0,
        Create = 1,
        Delete = 2,
        Change = 3,
        Move = 4,
        Close = 5
    }
}
