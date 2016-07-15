using CenterDevice.MiniFSWatcher;
using System.IO;

namespace CenterDevice.MiniFSWatcherTest
{
    public class FileEventTest
    {
        protected string watchDir = null;
        protected string tmpFile = null;

        protected EventWatcher filter = null;

        protected void Initialize()
        {
            watchDir = Path.Combine(Path.GetTempPath(), Path.GetRandomFileName());
            Directory.CreateDirectory(watchDir);

            tmpFile = Path.Combine(watchDir, Path.GetRandomFileName());
            File.Create(tmpFile).Dispose();

            filter = new EventWatcher();
            filter.Connect();
            filter.WatchPath(watchDir + "*");
        }
    }
}
