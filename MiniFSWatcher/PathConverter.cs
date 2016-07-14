using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace CenterDevice.MiniFSWatcher
{
    class PathConverter
    {
        private static Dictionary<string, string> driveToDevice = new Dictionary<string, string>();

        static PathConverter()
        {
            foreach (DriveInfo driveInfo in DriveInfo.GetDrives())
            {
                var drive = driveInfo.Name.TrimEnd(Path.DirectorySeparatorChar);
                var device = GetDevicePath(drive);

                driveToDevice[drive] = device;
            }
        }

        public static string ReplaceDriveLetter(string path)
        {
            foreach(var entry in driveToDevice)
            {
                if (path.StartsWith(entry.Key))
                {
                    return path.Replace(entry.Key, entry.Value);
                }
            }

            return path;
        }

        public static string ReplaceDevicePath(string path)
        {
            foreach (var entry in driveToDevice)
            {
                if (path.StartsWith(entry.Value))
                {
                    return path.Replace(entry.Value, entry.Key);
                }
            }

            return path;
        }

        public static string GetDevicePath(string label)
        {
            var builder = new StringBuilder(64);
            Marshal.ThrowExceptionForHR(NativeMethods.QueryDosDevice(label, builder, builder.Capacity));
            return builder.ToString();
        }
    }
}
