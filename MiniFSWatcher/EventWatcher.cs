using CenterDevice.MiniFSWatcher.Events;
using CenterDevice.MiniFSWatcher.Types;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace CenterDevice.MiniFSWatcher
{
    public delegate void FileEventHandler(string name, ulong processId);
    public delegate void MoveEventHandler(string name, string oldName, ulong processId);

    public class EventWatcher: IDisposable
    {
        public readonly DriverVersion Version = new DriverVersion(2,0);

        private const int ALL = 0;
        private const long ERROR_NO_MORE_ITEMS = 259L;
        private const int BUFFER_SIZE = 4096;
        private const string RECYCLE_BIN_PREFIX = "C:\\$RECYCLE.BIN\\";
        private bool disposed = false;

        private readonly TimeSpan eventReadDelay = TimeSpan.FromMilliseconds(100);
        private Dictionary<string, FileSystemEvent> postponedEvents = new Dictionary<string, FileSystemEvent>();
        private CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
        private FilterConnector connector = new FilterConnector();

        public bool AggregateEvents { get; set; }

        public FileEventHandler OnChange { get; set; }
        public FileEventHandler OnCreate { get; set; }
        public FileEventHandler OnDelete { get; set; }
        public MoveEventHandler OnRenameOrMove { get; set; }

        public void Connect()
        {
            connector.Connect();

            var driverVersion = GetDriverVersion();
            if (driverVersion.Major != Version.Major)
            {
                connector.Disconnect();
                throw new Exception("Invalid driver version!");
            }
            else if (driverVersion.Minor != Version.Minor)
            {
                Trace.TraceWarning("Driver version differs from client version!");
            }

            Task.Factory.StartNew(ForwardEvents, TaskCreationOptions.LongRunning, cancellationTokenSource.Token);
        }

        private void ForwardEvents(object val)
        {
            while (!cancellationTokenSource.IsCancellationRequested)
            {
                var events = GetEvents();

                foreach (var fileEvent in events)
                {
                    HandleFileEvent(fileEvent);
                }

                if (events.Count == 0)
                {
                    Task.Delay(eventReadDelay).Wait();
                }
            }
        }

        public void Disconnect()
        {
            cancellationTokenSource.Cancel();
            cancellationTokenSource = new CancellationTokenSource();
            connector.Disconnect();
        }

        private void HandleFileEvent(FileSystemEvent fileEvent)
        {
            if (fileEvent.Type == EventType.Close)
            {
                FlushPostponedEvents(fileEvent);
            }
            else if (CanBePostponed(fileEvent) && AggregateEvents)
            {
                PostponeEventDelivery(fileEvent);
            }
            else if (!EventShouldBeIgnored(fileEvent))
            {
                DeliverEvent(fileEvent, AggregateEvents);
            }
        }

        private bool EventShouldBeIgnored(FileSystemEvent fileEvent)
        {
            if (fileEvent is RenameOrMoveEvent)
            {
                return fileEvent.Filename.StartsWith(RECYCLE_BIN_PREFIX, StringComparison.CurrentCultureIgnoreCase);
            }

            return false;
        }

        private void DeliverEvent(FileSystemEvent fileEvent, bool postponeDelivery)
        {
            switch (fileEvent.Type)
            {
                case EventType.Change:
                    OnChange?.Invoke(fileEvent.Filename, fileEvent.ProcessId);
                    break;
                case EventType.Create:
                    OnCreate?.Invoke(fileEvent.Filename, fileEvent.ProcessId);
                    break;
                case EventType.Delete:
                    OnDelete?.Invoke(fileEvent.Filename, fileEvent.ProcessId);
                    break;
                case EventType.Move:
                    OnRenameOrMove?.Invoke(fileEvent.Filename, ((RenameOrMoveEvent) fileEvent).OldFilename, fileEvent.ProcessId);
                    break;
                default:
                    break;
            }
        }

        private bool CanBePostponed(FileSystemEvent fileEvent)
        {
            return fileEvent.Type == EventType.Change || fileEvent.Type == EventType.Create;
        }

        private void FlushPostponedEvents(FileSystemEvent fileEvent)
        {
            if (postponedEvents.ContainsKey(fileEvent.Filename))
            {
                var postponedEvent = postponedEvents[fileEvent.Filename];
                postponedEvents.Remove(fileEvent.Filename);
                DeliverEvent(postponedEvent, false);
            }
        }

        private void PostponeEventDelivery(FileSystemEvent fileEvent)
        {
            if (!postponedEvents.ContainsKey(fileEvent.Filename))
            {
                postponedEvents.Add(fileEvent.Filename, fileEvent);
            }
        }

        public void RemoveProcessFilter()
        {
            WatchProcess(ALL);
        }

        public void NotWatchProcess(long processId)
        {
            WatchProcess(-1 * processId);
        }

        public void WatchProcess(long processId)
        {
            CommandMessage message = new CommandMessage();
            message.Command = MinispyCommand.SetWatchProcess;
            connector.Send(message, BitConverter.GetBytes(processId));
        }

        public void NotWatchThread(long threadId)
        {
            WatchThread(-1 * threadId);
        }

        public void WatchThread(long threadId)
        {
            CommandMessage message = new CommandMessage();
            message.Command = MinispyCommand.SetWatchThread;
            connector.Send(message, BitConverter.GetBytes(threadId));
        }

        public void WatchPath(string path)
        {
            CommandMessage message = new CommandMessage();
            message.Command = MinispyCommand.SetPathFilter;
            connector.Send(message, PathConverter.ReplaceDriveLetter(path));
        }

        public DriverVersion GetDriverVersion()
        {
            CommandMessage message = new CommandMessage();
            message.Command = MinispyCommand.GetMiniSpyVersion;

            IntPtr resultSize;
            var buffer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(DriverVersion)));

            HResult hResult = connector.SendAndRead(message, buffer, out resultSize);
            Marshal.ThrowExceptionForHR(hResult.Result);

            var driverVersion = Marshal.PtrToStructure<DriverVersion>(buffer);
            Marshal.FreeHGlobal(buffer);
            return driverVersion;
        }

        public static uint GetCurrentThreadId()
        {
            return NativeMethods.GetCurrentThreadId();
        }

        public static int GetCurrentProcessId()
        {
            return Process.GetCurrentProcess().Id;
        }

        private List<FileSystemEvent> GetEvents()
        {
            CommandMessage message = new CommandMessage();
            message.Command = MinispyCommand.GetMiniSpyLog;
            var buffer = Marshal.AllocHGlobal(BUFFER_SIZE);

            try
            {
                return GetFileSystemEvents(message, buffer);
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        private List<FileSystemEvent> GetFileSystemEvents(CommandMessage message, IntPtr buffer)
        {
            IntPtr resultSize;
            HResult hResult = connector.SendAndRead(message, buffer, out resultSize);

            if (hResult.IsError)
            {
                if (hResult.Code != ERROR_NO_MORE_ITEMS)
                {
                    Marshal.ThrowExceptionForHR(hResult.Result);
                }

                return new List<FileSystemEvent>();
            }
            else
            {
                return EventReader.ReadFromBuffer(buffer, resultSize.ToInt64());
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposed) return;

            if (disposing)
            {
                cancellationTokenSource.Dispose();
            }

            disposed = true;
        }
    }
}
