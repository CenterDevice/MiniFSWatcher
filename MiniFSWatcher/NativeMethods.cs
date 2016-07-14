using System;
using System.Runtime.InteropServices;
using System.Text;

namespace CenterDevice.MiniFSWatcher
{
    public class NativeMethods
    {

        [DllImport("FltLib.dll", SetLastError = true)]
        internal extern static int FilterConnectCommunicationPort([MarshalAs(UnmanagedType.LPWStr)] string lpPortName, 
            uint dwOptions, 
            IntPtr lpContext,
            uint dwSizeOfContext,
            IntPtr lpSecurityAttributes,
            out SafePortHandle hPort
        );

        [DllImport("FltLib.dll", SetLastError = true)]
        internal extern static int FilterSendMessage(SafePortHandle port, 
            IntPtr lpInBuffer, 
            int dwInBufferSize, 
            IntPtr lpOutBuffer, 
            int dwOutBufferSize, 
            out IntPtr lpBytesReturned);

        [DllImport("Kernel32")]
        internal extern static bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll")]
        internal static extern int QueryDosDevice(string lpDeviceName, StringBuilder lpTargetPath, int ucchMax);
    }
}
