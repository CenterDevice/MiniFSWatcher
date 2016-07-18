using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.IO;
using CenterDevice.MiniFSWatcher;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Threading;

namespace CenterDevice.MiniFSWatcherTest
{
    [TestClass]
    public class BasicFileEventTest: FileEventTest
    {
        [TestInitialize]
        public void Setup()
        {
            Initialize();
        }

        [TestMethod]
        public void TestCreateFile()
        {
            var result = new TaskCompletionSource<Tuple<string, ulong>>();
            filter.OnCreate += (path, process) =>
            {
                result.SetResult(new Tuple<string, ulong>(path, process));
            };

            var filePath = Path.Combine(watchDir, Path.GetRandomFileName());
            File.Create(filePath).Dispose();

            var callbackData = result.Task.Result;
            Assert.AreEqual(filePath, callbackData.Item1);
            Assert.AreEqual((ulong) Process.GetCurrentProcess().Id, callbackData.Item2);
        }

        [TestMethod]
        public void TestDeleteFile()
        {
            var result = new TaskCompletionSource<Tuple<string, ulong>>();
            filter.OnDelete += (path, process) =>
            {
                result.SetResult(new Tuple<string, ulong>(path, process));
            };

            File.Delete(tmpFile);

            var callbackData = result.Task.Result;
            Assert.AreEqual(tmpFile, callbackData.Item1);
            Assert.AreEqual((ulong)Process.GetCurrentProcess().Id, callbackData.Item2);
        }

        [TestMethod]
        public void TestRenameFile()
        {
            var result = new TaskCompletionSource<Tuple<string, string, ulong>>();
            filter.OnRenameOrMove += (path, oldPath, process) =>
            {
                result.SetResult(new Tuple<string, string, ulong>(path, oldPath, process));
            };

            var newPath = Path.Combine(watchDir, Path.GetRandomFileName());
            File.Move(tmpFile, newPath);

            var callbackData = result.Task.Result;
            Assert.AreEqual(newPath, callbackData.Item1);
            Assert.AreEqual(tmpFile, callbackData.Item2);
            Assert.AreEqual((ulong)Process.GetCurrentProcess().Id, callbackData.Item3);
        }

        [TestMethod]
        public void TestChangeFileAppend()
        {
            var result = new TaskCompletionSource<Tuple<string, ulong>>();
            filter.OnChange += (path, process) =>
            {
                result.SetResult(new Tuple<string, ulong>(path, process));
            };

            File.AppendAllText(tmpFile, "Some text");

            var callbackData = result.Task.Result;
            Assert.AreEqual(tmpFile, callbackData.Item1);
            Assert.AreEqual((ulong)Process.GetCurrentProcess().Id, callbackData.Item2);
        }

        [TestMethod]
        public void TestChangeFileOverwrite()
        {
            var result = new TaskCompletionSource<Tuple<string, ulong>>();
            filter.OnChange += (path, process) =>
            {
                result.SetResult(new Tuple<string, ulong>(path, process));
            };

            File.WriteAllText(tmpFile, "Some text");

            var callbackData = result.Task.Result;
            Assert.AreEqual(tmpFile, callbackData.Item1);
            Assert.AreEqual((ulong)Process.GetCurrentProcess().Id, callbackData.Item2);
        }

        [TestMethod]
        public void TestOverwriteFile()
        {
            var result = new TaskCompletionSource<Tuple<string, ulong>>();
            filter.OnChange += (path, process) =>
            {
                result.SetResult(new Tuple<string, ulong>(path, process));
            };

            File.Create(tmpFile).Dispose();

            var callbackData = result.Task.Result;
            Assert.AreEqual(tmpFile, callbackData.Item1);
            Assert.AreEqual((ulong)Process.GetCurrentProcess().Id, callbackData.Item2);
        }

        [TestMethod]
        public void TestReplaceFile()
        {
            var unobservedFile = Path.Combine(Path.GetTempPath(), Path.GetRandomFileName());
            File.Create(unobservedFile).Dispose();

            var result = new TaskCompletionSource<Tuple<string, ulong>>();
            filter.OnRenameOrMove += (path, oldPath, process) =>
            {
                result.SetResult(new Tuple<string, ulong>(path, process));
            };

            var moved = NativeMethods.MoveFileEx(unobservedFile, tmpFile, NativeMethods.MoveFileFlags.MOVEFILE_REPLACE_EXISTING);

            var callbackData = result.Task.Result;
            Assert.IsTrue(moved);
            Assert.AreEqual(tmpFile, callbackData.Item1);
            Assert.AreEqual((ulong)Process.GetCurrentProcess().Id, callbackData.Item2);
        }

        [TestCleanup]
        public void Teardown()
        {
            filter.Disconnect();
            Directory.Delete(watchDir, true);
        }
    }
}
