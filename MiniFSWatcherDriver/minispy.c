/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    MiniSpy.c

Abstract:

    This is the main module for the MiniSpy mini-filter.

Environment:

    Kernel mode

--*/

#include "mspyKern.h"
#include <stdio.h>
#include <limits.h>
#include <Ntstrsafe.h>

//
//  Global variables
//

MINIFSWATCHER_DATA MiniFSWatcherData;
NTSTATUS StatusToBreakOn = 0;

//---------------------------------------------------------------------------
//  Function prototypes
//---------------------------------------------------------------------------
DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );


NTSTATUS
SpyMessage(
	_In_ PVOID ConnectionCookie,
	_In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferLength
	);

NTSTATUS
SpyConnect(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID ServerPortCookie,
    _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Flt_ConnectionCookie_Outptr_ PVOID *ConnectionCookie
    );

VOID
SpyDisconnect(
    _In_opt_ PVOID ConnectionCookie
    );

//---------------------------------------------------------------------------
//  Assign text sections for each routine.
//---------------------------------------------------------------------------

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(INIT, DriverEntry)
    #pragma alloc_text(PAGE, SpyFilterUnload)
    #pragma alloc_text(PAGE, SpyQueryTeardown)
    #pragma alloc_text(PAGE, SpyConnect)
    #pragma alloc_text(PAGE, SpyDisconnect)
    #pragma alloc_text(PAGE, SpyMessage)
#endif


#define SetFlagInterlocked(_ptrFlags,_flagToSet) \
    ((VOID)InterlockedOr(((volatile LONG *)(_ptrFlags)),_flagToSet))
    
//---------------------------------------------------------------------------
//                      ROUTINES
//---------------------------------------------------------------------------

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine is called when a driver first loads.  Its purpose is to
    initialize global state and then register with FltMgr to start filtering.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.
    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Status of the operation.

--*/
{
    PSECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uniString;
    NTSTATUS status = STATUS_SUCCESS;

    try {

        //
        // Initialize global data structures.
        //

		MiniFSWatcherData.WatchProcess = 0;
		MiniFSWatcherData.WatchThread = 0;
        MiniFSWatcherData.LogSequenceNumber = 0;
        MiniFSWatcherData.MaxRecordsToAllocate = DEFAULT_MAX_RECORDS_TO_ALLOCATE;
        MiniFSWatcherData.RecordsAllocated = 0;
        MiniFSWatcherData.NameQueryMethod = DEFAULT_NAME_QUERY_METHOD;
		MiniFSWatcherData.ClientPort = NULL;
		MiniFSWatcherData.WatchPathInUse = FALSE;

		RtlInitUnicodeString(&MiniFSWatcherData.WatchPath, NULL);

        MiniFSWatcherData.DriverObject = DriverObject;

        InitializeListHead( &MiniFSWatcherData.OutputBufferList );
        KeInitializeSpinLock( &MiniFSWatcherData.OutputBufferLock );

		ExInitializeNPagedLookasideList( &MiniFSWatcherData.FreeBufferList,
                                         NULL,
                                         NULL,
                                         POOL_NX_ALLOCATION,
                                         RECORD_SIZE,
                                         SPY_TAG,
                                         0 );

        //
        // Read the custom parameters for MiniSpy from the registry
        //

        SpyReadDriverParameters(RegistryPath);

        //
        //  Now that our global configuration is complete, register with FltMgr.
        //

        status = FltRegisterFilter( DriverObject,
                                    &FilterRegistration,
                                    &MiniFSWatcherData.Filter );

        if (!NT_SUCCESS( status )) {

           leave;
        }

        status  = FltBuildDefaultSecurityDescriptor( &sd,
                                                     FLT_PORT_ALL_ACCESS );



        if (!NT_SUCCESS( status )) {
            leave;
        }

#pragma warning ( push )
#pragma warning ( disable:6248 ) // PREFAST -- Yes, we really want no DACL

		RtlSetDaclSecurityDescriptor(sd, TRUE, NULL, FALSE);

#pragma warning ( pop )

        RtlInitUnicodeString( &uniString, MINIFSWATCHER_PORT_NAME );

        InitializeObjectAttributes( &oa,
                                    &uniString,
                                    OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    sd );

        status = FltCreateCommunicationPort( MiniFSWatcherData.Filter,
                                             &MiniFSWatcherData.ServerPort,
                                             &oa,
                                             NULL,
                                             SpyConnect,
                                             SpyDisconnect,
                                             SpyMessage,
                                             1 );

        FltFreeSecurityDescriptor( sd );

        if (!NT_SUCCESS( status )) {
            leave;
        }

        //
        //  We are now ready to start filtering
        //

        status = FltStartFiltering( MiniFSWatcherData.Filter );

    } finally {

        if (!NT_SUCCESS( status ) ) {

             if (NULL != MiniFSWatcherData.ServerPort) {
                 FltCloseCommunicationPort( MiniFSWatcherData.ServerPort );
             }

             if (NULL != MiniFSWatcherData.Filter) {
                 FltUnregisterFilter( MiniFSWatcherData.Filter );
             }

             ExDeleteNPagedLookasideList( &MiniFSWatcherData.FreeBufferList );
        }
    }

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "MiniFSWatcher %i.%i loaded successfully\n", MINIFSWATCHER_MAJ_VERSION, MINIFSWATCHER_MIN_VERSION);

    return status;
}

NTSTATUS
SpyConnect(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID ServerPortCookie,
    _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Flt_ConnectionCookie_Outptr_ PVOID *ConnectionCookie
    )
/*++

Routine Description

    This is called when user-mode connects to the server
    port - to establish a connection

Arguments

    ClientPort - This is the pointer to the client port that
        will be used to send messages from the filter.
    ServerPortCookie - unused
    ConnectionContext - unused
    SizeofContext   - unused
    ConnectionCookie - unused

Return Value

    STATUS_SUCCESS - to accept the connection
--*/
{

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ServerPortCookie );
    UNREFERENCED_PARAMETER( ConnectionContext );
    UNREFERENCED_PARAMETER( SizeOfContext);
    UNREFERENCED_PARAMETER( ConnectionCookie );

    FLT_ASSERT( MiniFSWatcherData.ClientPort == NULL );
    MiniFSWatcherData.ClientPort = ClientPort;

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Client connected to MiniSpy\n");

    return STATUS_SUCCESS;
}


VOID
SpyDisconnect(
    _In_opt_ PVOID ConnectionCookie
   )
/*++

Routine Description

    This is called when the connection is torn-down. We use it to close our handle to the connection

Arguments

    ConnectionCookie - unused

Return value

    None
--*/
{

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ConnectionCookie );

    //
    //  Close our handle
    //

    FltCloseClientPort( MiniFSWatcherData.Filter, &MiniFSWatcherData.ClientPort );
	MiniFSWatcherData.ClientPort = NULL;
	MiniFSWatcherData.WatchProcess = 0;
	MiniFSWatcherData.WatchThread = 0;
	SpyUpdateWatchedPath(NULL);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Client disconnected from MiniSpy\n");
}

NTSTATUS
SpyFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is called when a request has been made to unload the filter.  Unload
    requests from the Operation System (ex: "sc stop minispy" can not be
    failed.  Other unload requests may be failed.

    You can disallow OS unload request by setting the
    FLTREGFL_DO_NOT_SUPPORT_SERVICE_STOP flag in the FLT_REGISTARTION
    structure.

Arguments:

    Flags - Flags pertinent to this operation

Return Value:

    Always success

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    //
    //  Close the server port. This will stop new connections.
    //

    FltCloseCommunicationPort( MiniFSWatcherData.ServerPort );

    FltUnregisterFilter( MiniFSWatcherData.Filter );

    SpyEmptyOutputBufferList();
    ExDeleteNPagedLookasideList( &MiniFSWatcherData.FreeBufferList );

    return STATUS_SUCCESS;
}


NTSTATUS
SpyQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This allows our filter to be manually detached from a volume.

Arguments:

    FltObjects - Contains pointer to relevant objects for this operation.
        Note that the FileObject field will always be NULL.

    Flags - Flags pertinent to this operation

Return Value:

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    PAGED_CODE();
    return STATUS_SUCCESS;
}


NTSTATUS
SpyMessage (
    _In_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
    _In_ ULONG InputBufferSize,
    _Out_writes_bytes_to_opt_(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferSize,
    _Out_ PULONG ReturnOutputBufferLength
    )
/*++

Routine Description:

    This is called whenever a user mode application wishes to communicate
    with this minifilter.

Arguments:

    ConnectionCookie - unused

    OperationCode - An identifier describing what type of message this
        is.  These codes are defined by the MiniFilter.
    InputBuffer - A buffer containing input data, can be NULL if there
        is no input data.
    InputBufferSize - The size in bytes of the InputBuffer.
    OutputBuffer - A buffer provided by the application that originated
        the communication in which to store data to be returned to this
        application.
    OutputBufferSize - The size in bytes of the OutputBuffer.
    ReturnOutputBufferSize - The size in bytes of meaningful data
        returned in the OutputBuffer.

Return Value:

    Returns the status of processing the message.

--*/
{
    MINIFSWATCHER_COMMAND command;
    NTSTATUS status;
	ULONG dataLength;
	UNICODE_STRING dataString;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ConnectionCookie );

    //
    //                      **** PLEASE READ ****
    //
    //  The INPUT and OUTPUT buffers are raw user mode addresses.  The filter
    //  manager has already done a ProbedForRead (on InputBuffer) and
    //  ProbedForWrite (on OutputBuffer) which guarentees they are valid
    //  addresses based on the access (user mode vs. kernel mode).  The
    //  minifilter does not need to do their own probe.
    //
    //  The filter manager is NOT doing any alignment checking on the pointers.
    //  The minifilter must do this themselves if they care (see below).
    //
    //  The minifilter MUST continue to use a try/except around any access to
    //  these buffers.
    //

    if ((InputBuffer != NULL) &&
        (InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE,Command) +
                             sizeof(MINIFSWATCHER_COMMAND)))) {

        try  {

            //
            //  Probe and capture input message: the message is raw user mode
            //  buffer, so need to protect with exception handler
            //

            command = ((PCOMMAND_MESSAGE) InputBuffer)->Command;
			dataLength = InputBufferSize - FIELD_OFFSET(COMMAND_MESSAGE, Data);

        } except (SpyExceptionFilter( GetExceptionInformation(), TRUE )) {
        
            return GetExceptionCode();
        }

        switch (command) {

            case GetMiniSpyLog:

                //
                //  Return as many log records as can fit into the OutputBuffer
                //

                if ((OutputBuffer == NULL) || (OutputBufferSize == 0)) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                //
                //  We want to validate that the given buffer is POINTER
                //  aligned.  But if this is a 64bit system and we want to
                //  support 32bit applications we need to be careful with how
//  we do the check.  Note that the way SpyGetLog is written
//  it actually does not care about alignment but we are
//  demonstrating how to do this type of check.
//

#if defined(_WIN64)

if (IoIs32bitProcess(NULL)) {

	//
	//  Validate alignment for the 32bit process on a 64bit
	//  system
	//

	if (!IS_ALIGNED(OutputBuffer, sizeof(ULONG))) {

		status = STATUS_DATATYPE_MISALIGNMENT;
		break;
	}

}
else {

#endif

	if (!IS_ALIGNED(OutputBuffer, sizeof(PVOID))) {

		status = STATUS_DATATYPE_MISALIGNMENT;
		break;
	}

#if defined(_WIN64)

}

#endif

//
//  Get the log record.
//

status = SpyGetLog(OutputBuffer,
	OutputBufferSize,
	ReturnOutputBufferLength);
break;


            case GetMiniSpyVersion:

				//
				//  Return version of the MiniSpy filter driver.  Verify
				//  we have a valid user buffer including valid
				//  alignment
				//

				if ((OutputBufferSize < sizeof(MINIFSWATCHERVER)) ||
					(OutputBuffer == NULL)) {

					status = STATUS_INVALID_PARAMETER;
					break;
				}

				//
				//  Validate Buffer alignment.  If a minifilter cares about
				//  the alignment value of the buffer pointer they must do
				//  this check themselves.  Note that a try/except will not
				//  capture alignment faults.
				//

				if (!IS_ALIGNED(OutputBuffer, sizeof(ULONG))) {

					status = STATUS_DATATYPE_MISALIGNMENT;
					break;
				}

				//
				//  Protect access to raw user-mode output buffer with an
				//  exception handler
				//

				try {

					((PMINIFSWATCHERVER)OutputBuffer)->Major = MINIFSWATCHER_MAJ_VERSION;
					((PMINIFSWATCHERVER)OutputBuffer)->Minor = MINIFSWATCHER_MIN_VERSION;

				} except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {
					return GetExceptionCode();
				}

				*ReturnOutputBufferLength = sizeof(MINIFSWATCHERVER);
				status = STATUS_SUCCESS;
				break;

			case SetWatchProcess:
				if (dataLength < sizeof(LONGLONG))
				{
					status = STATUS_INVALID_PARAMETER;
					break;
				}

				try {
					MiniFSWatcherData.WatchProcess = *((LONGLONG*)((PCOMMAND_MESSAGE)InputBuffer)->Data);
					DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Watching process %li\n", MiniFSWatcherData.WatchProcess);
					status = STATUS_SUCCESS;
				} except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {
					return GetExceptionCode();
				}
		
				break;
			case SetWatchThread:
				if (dataLength < sizeof(LONGLONG))
				{
					status = STATUS_INVALID_PARAMETER;
					break;
				}

				try {
					MiniFSWatcherData.WatchThread = *((LONGLONG*)((PCOMMAND_MESSAGE)InputBuffer)->Data);
					DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Watching thread %li\n", MiniFSWatcherData.WatchThread);
					status = STATUS_SUCCESS;
				} except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {
					return GetExceptionCode();
				}

				break;
			case SetPathFilter:
				try {
					if (dataLength <= sizeof(WCHAR) 
|| ((PCWSTR)((PCOMMAND_MESSAGE)InputBuffer)->Data)[dataLength / sizeof(WCHAR) - 1] != UNICODE_NULL)
					{
						status = STATUS_INVALID_PARAMETER;
						break;
					}

RtlInitUnicodeString(&dataString, (PCWSTR)((PCOMMAND_MESSAGE)InputBuffer)->Data);
if (SpyUpdateWatchedPath(&dataString))
{
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Watching path %wZ\n", &MiniFSWatcherData.WatchPath);
	status = STATUS_SUCCESS;
}
else
{
	status = STATUS_OPERATION_IN_PROGRESS;
}
				} except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {
					return GetExceptionCode();
				}

				break;
            default:
				status = STATUS_INVALID_PARAMETER;
				break;
		}

	}
 else {

	 status = STATUS_INVALID_PARAMETER;
 }

 return status;
}


//---------------------------------------------------------------------------
//              Operation filtering routines
//---------------------------------------------------------------------------


FLT_PREOP_CALLBACK_STATUS
#pragma warning(suppress: 6262) // higher than usual stack usage is considered safe in this case
SpyPreOperationCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
/*++

Routine Description:

	This routine receives ALL pre-operation callbacks for this filter.  It then
	tries to log information about the given operation.  If we are able
	to log information then we will call our post-operation callback  routine.

	NOTE:  This routine must be NON-PAGED because it can be called on the
		   paging path.

Arguments:

	Data - Contains information about the given operation.

	FltObjects - Contains pointers to the various objects that are pertinent
		to this operation.

	CompletionContext - This receives the address of our log buffer for this
		operation.  Our completion routine then receives this buffer address.

Return Value:

	Identifies how processing should continue for this operation

--*/
{
	FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK; //assume we are NOT going to call our completion routine
	PRECORD_LIST recordList;

	NTSTATUS nameStatus = STATUS_UNSUCCESSFUL;
	NTSTATUS targetNameStatus = STATUS_UNSUCCESSFUL;

	PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
	PFLT_FILE_NAME_INFORMATION targetNameInfo = NULL;

	if (MiniFSWatcherData.ClientPort == NULL || MiniFSWatcherData.WatchPath.Buffer == NULL)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (!FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (FltObjects->FileObject == NULL || FltObjects->FileObject->FileName.Buffer == NULL || FltObjects->FileObject->DeviceObject == NULL)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	CONTINUE_IF_MATCHES(MiniFSWatcherData.WatchProcess, PsGetCurrentProcessId());

	CONTINUE_IF_MATCHES(MiniFSWatcherData.WatchThread, PsGetCurrentThreadId());

	if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION && Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformation)
	{
		PFILE_RENAME_INFORMATION info = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
		if (info != NULL)
		{
			targetNameStatus = FltGetDestinationFileNameInformation(FltObjects->Instance, FltObjects->FileObject, info->RootDirectory, info->FileName, info->FileNameLength, FLT_FILE_NAME_NORMALIZED | MiniFSWatcherData.NameQueryMethod, &targetNameInfo);
		}
	}

	nameStatus = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | MiniFSWatcherData.NameQueryMethod, &nameInfo);
	
	if ((NT_SUCCESS(nameStatus) && SpyIsWatchedPath(&nameInfo->Name))
		|| (NT_SUCCESS(targetNameStatus) && SpyIsWatchedPath(&targetNameInfo->Name)))
	{
		recordList = SpyNewRecord();

		if (recordList) 
		{
			USHORT offset = SpyAddRecordName(&recordList->LogRecord, &nameInfo->Name, 0);
			if (NT_SUCCESS(targetNameStatus) && targetNameInfo != NULL)
			{
				SpyAddRecordName(&recordList->LogRecord, &targetNameInfo->Name, offset);
			}

			SpyLogPreOperationData(recordList);

			*CompletionContext = recordList;
			returnStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
		}
	}

	if (nameInfo != NULL)
	{
		FltReleaseFileNameInformation(nameInfo);
	}

	if (targetNameInfo != NULL)
	{
		FltReleaseFileNameInformation(targetNameInfo);
	}

    return returnStatus;
}


FLT_POSTOP_CALLBACK_STATUS
SpyPostOperationCallback (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine receives ALL post-operation callbacks.  This will take
    the log record passed in the context parameter and update it with
    the completion information.  It will then insert it on a list to be
    sent to the usermode component.

    NOTE:  This routine must be NON-PAGED because it can be called at DPC level

Arguments:

    Data - Contains information about the given operation.

    FltObjects - Contains pointers to the various objects that are pertinent
        to this operation.

    CompletionContext - Pointer to the RECORD_LIST structure in which we
        store the information we are logging.  This was passed from the
        pre-operation callback

    Flags - Contains information as to why this routine was called.

Return Value:

    Identifies how processing should continue for this operation

--*/
{
    PRECORD_LIST recordList;

    recordList = (PRECORD_LIST)CompletionContext;

	if (recordList == NULL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

    if (FlagOn(Flags,FLTFL_POST_OPERATION_DRAINING) || !NT_SUCCESS(Data->IoStatus.Status)
		|| (recordList->LogRecord.Data.EventType = SpyGetEventType(Data, FltObjects)) == FILE_SYSTEM_EVENT_UNKNOWN)
	{
        SpyFreeRecord( recordList );
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    SpyLogPostOperationData( FltObjects, recordList );
    SpyLog( recordList );

    return FLT_POSTOP_FINISHED_PROCESSING;
}

LONG
SpyExceptionFilter (
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
    )
/*++

Routine Description:

    Exception filter to catch errors touching user buffers.

Arguments:

    ExceptionPointer - The exception record.

    AccessingUserBuffer - If TRUE, overrides FsRtlIsNtStatusExpected to allow
                          the caller to munge the error to a desired status.

Return Value:

    EXCEPTION_EXECUTE_HANDLER - If the exception handler should be run.

    EXCEPTION_CONTINUE_SEARCH - If a higher exception handler should take care of
                                this exception.

--*/
{
    NTSTATUS Status;

    Status = ExceptionPointer->ExceptionRecord->ExceptionCode;

    //
    //  Certain exceptions shouldn't be dismissed within the namechanger filter
    //  unless we're touching user memory.
    //

    if (!FsRtlIsNtstatusExpected( Status ) &&
        !AccessingUserBuffer) {

        return EXCEPTION_CONTINUE_SEARCH;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}


