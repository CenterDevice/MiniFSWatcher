using CenterDevice.MiniFSWatcher.Events;
using CenterDevice.MiniFSWatcher.Types;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace CenterDevice.MiniFSWatcher
{
    class EventReader
    {
        public static List<FileSystemEvent> ReadFromBuffer(IntPtr buffer, long bufferSize)
        {
            var events = new List<FileSystemEvent>();
            int offset = 0;
            while (offset + Marshal.SizeOf(typeof(LogRecord)) < bufferSize)
            {
                var recordAddress = IntPtr.Add(buffer, offset);
                LogRecord record = ReadRecordFromBuffer(recordAddress);

                ValidateRecordLength(record, bufferSize);

                var stringBytes = record.Length - Marshal.SizeOf(typeof(LogRecord));
                string[] strings = ReadEventStringsFromBuffer(recordAddress, stringBytes);

                if (record.Data.EventType == EventType.Move)
                {
                    events.Add(CreateRenameOrMoveEvent(record, strings));
                }
                else
                {
                    events.Add(CreateFileSystemEvent(record, strings));
                }

                offset += record.Length;
            }

            return events;
        }

        private static void ValidateRecordLength(LogRecord record, long bufferSize)
        {
            if (record.Length <= 0 || record.Length > bufferSize)
            {
                throw new Exception("Invalid record length");
            }
        }

        private static LogRecord ReadRecordFromBuffer(IntPtr recordAddress)
        {
            return Marshal.PtrToStructure<LogRecord>(recordAddress);
        }

        private static string[] ReadEventStringsFromBuffer(IntPtr recordAddress, int stringBytes)
        {
            var stringOffset = IntPtr.Add(recordAddress, Marshal.SizeOf(typeof(LogRecord)));

            var data = new byte[stringBytes];
            Marshal.Copy(stringOffset, data, 0, stringBytes);

            return Encoding.Unicode.GetString(data).Split('\0');
        }

        private static FileSystemEvent CreateFileSystemEvent(LogRecord record, string[] strings)
        {
            var fileSystemEvent = new FileSystemEvent()
            {
                Filename = PathConverter.ReplaceDevicePath(strings[0]),
                ProcessId = record.Data.ProcessId,
                Type = record.Data.EventType
            };
            return fileSystemEvent;
        }

        private static FileSystemEvent CreateRenameOrMoveEvent(LogRecord record, string[] strings)
        {
            var fileSystemEvent = new RenameOrMoveEvent()
            {
                Filename = PathConverter.ReplaceDevicePath(strings[1]),
                OldFilename = PathConverter.ReplaceDevicePath(strings[0]),
                ProcessId = record.Data.ProcessId,
                Type = record.Data.EventType
            };
            return fileSystemEvent;
        }
    }
}
