using System.Runtime.InteropServices;

namespace CenterDevice.MiniFSWatcher.Types
{
    [StructLayout(LayoutKind.Sequential)]
    struct LogRecord
    {
        public int Length;           // Length of log record.  This Does not include
        public int SequenceNumber;   // space used by other members of RECORD_LIST

        public int RecordType;       // The type of log record this is.
        int Reserved;        // For alignment on IA64

        public RecordData Data;
    }
}
