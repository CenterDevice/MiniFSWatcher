# MiniFSWatcher

MiniFSWatcher is a file system event watcher for C# based on the [minispy](https://github.com/Microsoft/Windows-driver-samples/tree/master/filesys/miniFilter/minispy) driver example from Microsoft.
Compared to the default `FileSystemWatcher` class, MiniFSWatcher provides the following benefits:

### Capturing file moved events

The default `FileSystemWatcher` has no notion of moved files, it'll only generate "created" and "deleted"
events instead. `MiniFSWatcher` will let you track file movement within one partition.

### Reducing the amount of changed events

With the `AggregateEvents` option, you'll only receive one file event when the respective file handle is closed
and thus no more consecutive changes will occure. If multiple write operations were performed, only one "changed"
event is triggerd. If a file is created _and_ changed (i.e. due to a copy operation), you'll only receive one 
"created" event.

### Getting information about the process causing the change

Sometimes it is useful to know who caused the change, for example to ignore changes performed by a certain
application. `MiniFSWatcher` provides the ID of the causing process with every event and further allows 
to directly filter out all events caused by its own process ID.

# Usage

The following example shows how to use MiniFSWatcher to wach a directory and all subdirectories.

    var eventWatcher = new EventWatcher();
    
    eventWatcher.OnRenameOrMove += (filename, oldFilename, process) =>
    {
      Console.WriteLine("File " + oldFilename + " has been moved to " + filename + " by process " + process );
    };
    
    eventWatcher.Connect();
    eventWatcher.WatchPath("C:\\Users\\MyUser\\*");

# Installation

MiniFSWatcher consists of a user mode C# library and a minifilter driver running in kernel mode.
Both components need to be installed to observe file system events.

## Installing the user mode library

You can install the user mode library from [nuget](https://www.nuget.org/packages/MiniFSWatcher/) or simply add the `MiniFSWatcher` project to your project references.

## Installing the kernel mode driver

The kernel driver needs to be compiled and installed manually. Therefore,
make sure you have the [Windows Driver Kit (WDK) 10](https://msdn.microsoft.com/en-us/library/windows/hardware/ff557573(v=vs.85).aspx) installed.
After compiling the driver with VS, you can install the driver with the following command

     RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultInstall 132 ./minifswatcher.inf
	
The driver will be loaded automatically after reboot. To load it manually without reboot, run

     fltmc.exe load minifswatcher
	 
If you don't have a valid code signing certificate and try to install the driver on 64bit Windows,
you need to enable test signed drivers as described [here](https://msdn.microsoft.com/en-us/library/windows/hardware/ff553484(v=vs.85).aspx).

Please make sure to always compile and install the correct driver version (32/64bit) depending on your operating system!

To uninstall the driver, run

    RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultUninstall 132 ./minifswatcher.inf
