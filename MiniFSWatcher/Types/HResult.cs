namespace CenterDevice.MiniFSWatcher.Types
{
    class HResult
    {
        private const int SEVERITY_ERROR = 1;
        private const int FACILITY_WIN32 = 7;

        private readonly int result;

        public HResult(int result)
        {
            this.result = result;
        }

        public int Result
        {
            get
            {
                return result;
            }
        }

        public bool IsError
        {
            get
            {
                return (((ulong)(result)) >> 31 == SEVERITY_ERROR);
            }
        }

        public int Code
        {
            get
            {
                return result <= 0 ? result : (int) ((result & 0x0000FFFF) | FACILITY_WIN32 << 16 | 0x80000000);
            }
        }
    }
}
