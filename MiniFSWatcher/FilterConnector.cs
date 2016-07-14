using CenterDevice.MiniFSWatcher.Types;
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace CenterDevice.MiniFSWatcher
{
    class FilterConnector
    {
        private const string PORT_NAME = "\\MiniFSWatcherPort";

        SafePortHandle port;

        public bool Connected
        {
            get
            {
                return port != null && !port.IsClosed && !port.IsInvalid;
            }
        }

        public void Connect()
        {
            if (!Connected)
            {
                Marshal.ThrowExceptionForHR(NativeMethods.FilterConnectCommunicationPort(PORT_NAME, 0, IntPtr.Zero, 0, IntPtr.Zero, out port));
            }
        }

        public void Disconnect()
        {
            VerifyConnected();
            port.Dispose();
            port = null;
        }

        public void Send(CommandMessage message)
        {
            VerifyConnected();

            var size = Marshal.SizeOf(message);
            IntPtr pnt = Marshal.AllocHGlobal(size);

            try
            {
                IntPtr resultSize;
                Marshal.StructureToPtr(message, pnt, false);
                Marshal.ThrowExceptionForHR(NativeMethods.FilterSendMessage(port, pnt, size, IntPtr.Zero, 0, out resultSize));
            }
            finally
            {
                Marshal.FreeHGlobal(pnt);
            }
        }

        public void Send(CommandMessage message, string path)
        {
            Send(message, Encoding.Unicode.GetBytes(path + '\0'));
        }

        public void Send(CommandMessage message, byte[] data)
        {
            VerifyConnected();

            var size = Marshal.SizeOf(message) + data.Length;
            IntPtr pnt = Marshal.AllocHGlobal(size);

            try
            {
                IntPtr resultSize;
                Marshal.StructureToPtr(message, pnt, false);
                IntPtr address = IntPtr.Add(pnt, Marshal.SizeOf(typeof(CommandMessage)));
                Marshal.Copy(data, 0, address, data.Length);
                Marshal.ThrowExceptionForHR(NativeMethods.FilterSendMessage(port, pnt, size, IntPtr.Zero, 0, out resultSize));
            }
            finally
            {
                Marshal.FreeHGlobal(pnt);
            }
        }

        public HResult SendAndRead(CommandMessage message, IntPtr buffer, out IntPtr resultSize)
        {
            VerifyConnected();

            var size = Marshal.SizeOf(message);
            IntPtr command = Marshal.AllocHGlobal(size);

            try
            {
                Marshal.StructureToPtr(message, command, false);
                return new HResult(NativeMethods.FilterSendMessage(port, command, size, buffer, 4069, out resultSize));
            }
            finally
            {
                Marshal.FreeHGlobal(command);
            }
        }

        private void VerifyConnected()
        {
            if (!Connected)
            {
                throw new Exception("You need to connect first");
            }
        }
    }
}
