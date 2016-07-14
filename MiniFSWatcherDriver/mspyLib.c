/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    mspyLib.c

Abstract:
    This contains library support routines for MiniSpy

Environment:

    Kernel mode

--*/

#include <initguid.h>
#include <stdio.h>

#include "mspyKern.h"

//
// Can't pull in wsk.h until after MINISPY_VISTA is defined
//

#if MINISPY_VISTA
#include <ntifs.h>
#include <wsk.h>
#endif

//---------------------------------------------------------------------------
//  Assign text sections for each routine.
//---------------------------------------------------------------------------

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(INIT, SpyReadDriverParameters)
#endif

UCHAR TxNotificationToMinorCode (
    _In_ ULONG TxNotification
    )
/*++

Routine Description:

    This routine has been written to convert a transaction notification code
    to an Irp minor code. This function is needed because RECORD_DATA has a
    UCHAR field for the Irp minor code whereas TxNotification is ULONG. As
    of now all this function does is compute log_base_2(TxNotification) + 1.
    That fits our need for now but might have to be evolved later. This
    function is intricately tied with the enumeration TRANSACTION_NOTIFICATION_CODES
    in mspyLog.h and the case statements related to transactions in the function
    PrintIrpCode (Minispy\User\mspyLog.c).

Arguments:

    TxNotification - The transaction notification received.

Return Value:

    0 if TxNotification is 0;
    log_base_2(TxNotification) + 1 otherwise.

--*/
{
    UCHAR count = 0;

    if (TxNotification == 0)
        return 0;

    //
    //  This assert verifies if no more than one flag is set
    //  in the TxNotification variable. TxNotification flags are
    //  supposed to be mutually exclusive. The assert below verifies
    //  if the value of TxNotification is a power of 2. If it is not
    //  then we will break.
    //

    FLT_ASSERT( !(( TxNotification ) & ( TxNotification - 1 )) );

    while (TxNotification) {

        count++;

        TxNotification >>= 1;

        //
        //  If we hit this assert then we have more notification codes than
        //  can fit in a UCHAR. We need to revaluate our approach for
        //  storing minor codes now.
        //

        FLT_ASSERT( count != 0 );
    }

    return ( count );
}


//---------------------------------------------------------------------------
//                    Log Record allocation routines
//---------------------------------------------------------------------------

PRECORD_LIST
SpyAllocateBuffer (
    _Out_ PULONG RecordType
    )
/*++

Routine Description:

    Allocates a new buffer from the MiniSpyData.FreeBufferList if there is
    enough memory to do so and we have not exceed our maximum buffer
    count.

    NOTE:  Because there is no interlock between testing if we have exceeded
           the record allocation limit and actually increment the in use
           count it is possible to temporarily allocate one or two buffers
           more then the limit.  Because this is such a rare situation there
           is not point to handling this.

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path or at DPC level.

Arguments:

    RecordType - Receives information on what type of record was allocated.

Return Value:

    Pointer to the allocated buffer, or NULL if the allocation failed.

--*/
{
    PVOID newBuffer;
    ULONG newRecordType = RECORD_TYPE_NORMAL;

    //
    //  See if we have room to allocate more buffers
    //

    if (MiniFSWatcherData.RecordsAllocated < MiniFSWatcherData.MaxRecordsToAllocate) {

        InterlockedIncrement( &MiniFSWatcherData.RecordsAllocated );

        newBuffer = ExAllocateFromNPagedLookasideList( &MiniFSWatcherData.FreeBufferList );

        if (newBuffer == NULL) {

            //
            //  We failed to allocate the memory.  Decrement our global count
            //  and return what type of memory we have.
            //

            InterlockedDecrement( &MiniFSWatcherData.RecordsAllocated );

            newRecordType = RECORD_TYPE_FLAG_OUT_OF_MEMORY;
        }

    } else {

        //
        //  No more room to allocate memory, return we didn't get a buffer
        //  and why.
        //

        newRecordType = RECORD_TYPE_FLAG_EXCEED_MEMORY_ALLOWANCE;
        newBuffer = NULL;
    }

    *RecordType = newRecordType;
    return newBuffer;
}


VOID
SpyFreeBuffer (
    _In_ PVOID Buffer
    )
/*++

Routine Description:

    Free an allocate buffer.

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path or at DPC level.

Arguments:

    Buffer - The buffer to free.

Return Value:

    None.

--*/
{
    //
    //  Free the memory, update the counter
    //

    InterlockedDecrement( &MiniFSWatcherData.RecordsAllocated );
    ExFreeToNPagedLookasideList( &MiniFSWatcherData.FreeBufferList, Buffer );
}


//---------------------------------------------------------------------------
//                    Logging routines
//---------------------------------------------------------------------------

PRECORD_LIST
SpyNewRecord (
    VOID
    )
/*++

Routine Description:

    Allocates a new RECORD_LIST structure if there is enough memory to do so. A
    sequence number is updated for each request for a new record.

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path or at DPC level.

Arguments:

    None

Return Value:

    Pointer to the RECORD_LIST allocated, or NULL if no memory is available.

--*/
{
    PRECORD_LIST newRecord;
    ULONG initialRecordType;

    //
    //  Allocate the buffer
    //

    newRecord = SpyAllocateBuffer( &initialRecordType );

    if (newRecord == NULL) {

        //
        //  We could not allocate a record, see if the static buffer is
        //  in use.  If not, we will use it
        //

        if (!InterlockedExchange( &MiniFSWatcherData.StaticBufferInUse, TRUE )) {

            newRecord = (PRECORD_LIST)MiniFSWatcherData.OutOfMemoryBuffer;
            initialRecordType |= RECORD_TYPE_FLAG_STATIC;
        }
    }

    //
    //  If we got a record (doesn't matter if it is static or not), init it
    //

    if (newRecord != NULL) {

        //
        // Init the new record
        //

        newRecord->LogRecord.RecordType = initialRecordType;
        newRecord->LogRecord.Length = sizeof(LOG_RECORD);
        newRecord->LogRecord.SequenceNumber = InterlockedIncrement( &MiniFSWatcherData.LogSequenceNumber );
        RtlZeroMemory( &newRecord->LogRecord.Data, sizeof( RECORD_DATA ) );
    }

    return( newRecord );
}


VOID
SpyFreeRecord (
    _In_ PRECORD_LIST Record
    )
/*++

Routine Description:

    Free the given buffer

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path or at DPC level.

Arguments:

    Record - the buffer to free

Return Value:

    None.

--*/
{
    if (FlagOn(Record->LogRecord.RecordType,RECORD_TYPE_FLAG_STATIC)) {

        //
        // This was our static buffer, mark it available.
        //

        FLT_ASSERT(MiniFSWatcherData.StaticBufferInUse);
        MiniFSWatcherData.StaticBufferInUse = FALSE;

    } else {

        SpyFreeBuffer( Record );
    }
}

USHORT
SpyAddRecordName(
	_Inout_ PLOG_RECORD LogRecord,
	_In_ PUNICODE_STRING Name,
	_In_ USHORT ByteOffset
)
/*++

Routine Description:

Sets the given file name in the LogRecord.

NOTE:  This code must be NON-PAGED because it can be called on the
paging path.

Arguments:

LogRecord - The record in which to set the name.

Name - The name to insert

Return Value:

None.

--*/
{

	PWCHAR printPointer = (PWCHAR)LogRecord->Names;
	SHORT charsToSkip = ByteOffset / sizeof(WCHAR);
	SHORT wcharsCopied;
	USHORT stringLength;

	if (Name == NULL || charsToSkip >= MAX_NAME_WCHARS_LESS_NULL)
	{
		return 0;
	}

#pragma prefast(suppress:__WARNING_BANNED_API_USAGE, "reviewed and safe usage")
	wcharsCopied = (SHORT)_snwprintf(printPointer + charsToSkip,
		MAX_NAME_WCHARS_LESS_NULL - charsToSkip,
		L"%wZ",
		Name);

	if (wcharsCopied >= 0)
	{
		stringLength = ByteOffset + (wcharsCopied * sizeof(WCHAR));
	}
	else 
	{

		//
		//  There wasn't enough buffer space, so manually truncate in a NULL
		//  because we can't trust _snwprintf to do so in that case.
		//

		stringLength = MAX_NAME_SPACE_LESS_NULL;
		printPointer[MAX_NAME_WCHARS_LESS_NULL] = UNICODE_NULL;
	}

	//
	//  We will always round up log-record length to sizeof(PVOID) so that
	//  the next log record starts on the right PVOID boundary to prevent
	//  IA64 alignment faults.  The length of the record of course
	//  includes the additional NULL at the end.
	//

	LogRecord->Length = ROUND_TO_SIZE((sizeof(LOG_RECORD) +
		stringLength +
		sizeof(UNICODE_NULL)),
		sizeof(PVOID));

	FLT_ASSERT(LogRecord->Length <= MAX_LOG_RECORD_LENGTH);

	return stringLength + sizeof(UNICODE_NULL);
}

BOOLEAN SpyIsWatchedPath(_In_ PUNICODE_STRING path) 
{
	BOOLEAN result = FALSE;
	if (!InterlockedExchange(&MiniFSWatcherData.WatchPathInUse, TRUE)) 
	{
		result = FsRtlIsNameInExpression(&MiniFSWatcherData.WatchPath, path, TRUE, NULL);
		MiniFSWatcherData.WatchPathInUse = FALSE;
	}
	return result;
}

BOOLEAN SpyUpdateWatchedPath(_In_ PUNICODE_STRING path)
{
	BOOLEAN result = FALSE;
	if (!InterlockedExchange(&MiniFSWatcherData.WatchPathInUse, TRUE)) 
	{
		if (MiniFSWatcherData.WatchPath.Buffer != NULL) 
		{
			RtlFreeUnicodeString(&MiniFSWatcherData.WatchPath);
		}
		if (path != NULL && path->Buffer != NULL)
		{
			RtlUpcaseUnicodeString(&MiniFSWatcherData.WatchPath, path, TRUE);
		}

		result = TRUE;
		MiniFSWatcherData.WatchPathInUse = FALSE;
	}
	return result;
}

ULONG SpyGetEventType(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects
)
{
	// Get file rename information
	if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION) {

		if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformation) 
		{
			return FILE_SYSTEM_EVENT_MOVE;
		}
		// Get File delete information
		else if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation) 
		{
			PFILE_DISPOSITION_INFORMATION info = (PFILE_DISPOSITION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
			if (info != NULL && info->DeleteFile) 
			{
				return FILE_SYSTEM_EVENT_DELETE;
			}
		}
		else if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileAllocationInformation)
		{
			// Special handling if file is set to empty
			PFILE_ALLOCATION_INFORMATION info = (PFILE_ALLOCATION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
			if (info != NULL && info->AllocationSize.QuadPart == 0)
			{
				return FILE_SYSTEM_EVENT_CHANGE;
			}
		}
	}
	else if (Data->Iopb->MajorFunction == IRP_MJ_CREATE && FlagOn(Data->Iopb->IrpFlags, IRP_CREATE_OPERATION))
	{
		if (Data->IoStatus.Information == FILE_CREATED)
		{
			return FILE_SYSTEM_EVENT_CREATE;
		}
		else if (Data->IoStatus.Information == FILE_OVERWRITTEN)
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "File has been overwritten\n");
			return FILE_SYSTEM_EVENT_CHANGE;
		}
	}
	else if (Data->Iopb->MajorFunction == IRP_MJ_WRITE && Data->Iopb->Parameters.Write.Length > 0 && FlagOn(FltObjects->FileObject->Flags, FO_FILE_MODIFIED))
	{	
		return FILE_SYSTEM_EVENT_CHANGE;
	}
	else if (Data->Iopb->MajorFunction == IRP_MJ_CLOSE)
	{
		return FILE_SYSTEM_EVENT_CLOSE;
	}

	return FILE_SYSTEM_EVENT_UNKNOWN;
}

VOID
SpyLogPreOperationData (
    _Inout_ PRECORD_LIST RecordList
    )
/*++

Routine Description:

    This is called from the pre-operation callback routine to copy the
    necessary information into the log record.

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path.

Arguments:

    Data - The Data structure that contains the information we want to record.

    FltObjects - Pointer to the io objects involved in this operation.

    RecordList - Where we want to save the data

Return Value:

    None.

--*/
{
    PRECORD_DATA recordData = &RecordList->LogRecord.Data;

	recordData->Flags			= 0L;
    recordData->ProcessId       = (FILE_ID)PsGetCurrentProcessId();

    KeQuerySystemTime( &recordData->OriginatingTime );
}


VOID
SpyLogPostOperationData (
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Inout_ PRECORD_LIST RecordList
    )
/*++

Routine Description:

    This is called from the post-operation callback routine to copy the
    necessary information into the log record.

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path or at DPC level.

Arguments:

    Data - The Data structure that contains the information we want to record.

    RecordList - Where we want to save the data

Return Value:

    None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);

    PRECORD_DATA recordData = &RecordList->LogRecord.Data;
	recordData->Flags = 0;
    KeQuerySystemTime( &recordData->CompletionTime );
}

VOID
SpyLog (
    _In_ PRECORD_LIST RecordList
    )
/*++

Routine Description:

    This routine inserts the given log record into the list to be sent
    to the user mode application.

    NOTE:  This code must be NON-PAGED because it can be called on the
           paging path or at DPC level and uses a spin-lock

Arguments:

    RecordList - The record to append to the MiniSpyData.OutputBufferList

Return Value:

    The function returns STATUS_SUCCESS.



--*/
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&MiniFSWatcherData.OutputBufferLock, &oldIrql);
    InsertTailList(&MiniFSWatcherData.OutputBufferList, &RecordList->List);
    KeReleaseSpinLock(&MiniFSWatcherData.OutputBufferLock, oldIrql);
}


NTSTATUS
SpyGetLog (
    _Out_writes_bytes_to_(OutputBufferLength,*ReturnOutputBufferLength) PUCHAR OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength
    )
/*++

Routine Description:
    This function fills OutputBuffer with as many LOG_RECORDs as possible.
    The LOG_RECORDs are variable sizes and are tightly packed in the
    OutputBuffer.

    NOTE:  This code must be NON-PAGED because it uses a spin-lock.

Arguments:
    OutputBuffer - The user's buffer to fill with the log data we have
        collected

    OutputBufferLength - The size in bytes of OutputBuffer

    ReturnOutputBufferLength - The amount of data actually written into the
        OutputBuffer.

Return Value:
    STATUS_SUCCESS if some records were able to be written to the OutputBuffer.

    STATUS_NO_MORE_ENTRIES if we have no data to return.

    STATUS_BUFFER_TOO_SMALL if the OutputBuffer is too small to
        hold even one record and we have data to return.

--*/
{
    PLIST_ENTRY pList;
    ULONG bytesWritten = 0;
    PLOG_RECORD pLogRecord;
    NTSTATUS status = STATUS_NO_MORE_ENTRIES;
    PRECORD_LIST pRecordList;
    KIRQL oldIrql;
    BOOLEAN recordsAvailable = FALSE;

    KeAcquireSpinLock( &MiniFSWatcherData.OutputBufferLock, &oldIrql );

    while (!IsListEmpty( &MiniFSWatcherData.OutputBufferList ) && (OutputBufferLength > 0)) {

        //
        //  Mark we have records
        //

        recordsAvailable = TRUE;

        //
        //  Get the next available record
        //

        pList = RemoveHeadList( &MiniFSWatcherData.OutputBufferList );

        pRecordList = CONTAINING_RECORD( pList, RECORD_LIST, List );

        pLogRecord = &pRecordList->LogRecord;

        //
        //  If no filename was set then make it into a NULL file name.
        //

        if (REMAINING_NAME_SPACE( pLogRecord ) == MAX_NAME_SPACE) {

            //
            //  We don't have a name, so return an empty string.
            //  We have to always start a new log record on a PVOID aligned boundary.
            //

            pLogRecord->Length += ROUND_TO_SIZE( sizeof( UNICODE_NULL ), sizeof( PVOID ) );
            pLogRecord->Names[0] = UNICODE_NULL;
        }

        //
        //  Put it back if we've run out of room.
        //

        if (OutputBufferLength < pLogRecord->Length) {

            InsertHeadList( &MiniFSWatcherData.OutputBufferList, pList );
            break;
        }

        KeReleaseSpinLock( &MiniFSWatcherData.OutputBufferLock, oldIrql );

        //
        //  The lock is released, return the data, adjust pointers.
        //  Protect access to raw user-mode OutputBuffer with an exception handler
        //

        try {
            RtlCopyMemory( OutputBuffer, pLogRecord, pLogRecord->Length );
        } except (SpyExceptionFilter( GetExceptionInformation(), TRUE )) {

            //
            //  Put the record back in
            //

            KeAcquireSpinLock( &MiniFSWatcherData.OutputBufferLock, &oldIrql );
            InsertHeadList( &MiniFSWatcherData.OutputBufferList, pList );
            KeReleaseSpinLock( &MiniFSWatcherData.OutputBufferLock, oldIrql );

            return GetExceptionCode();

        }

        bytesWritten += pLogRecord->Length;

        OutputBufferLength -= pLogRecord->Length;

        OutputBuffer += pLogRecord->Length;

        SpyFreeRecord( pRecordList );

        //
        //  Relock the list
        //

        KeAcquireSpinLock( &MiniFSWatcherData.OutputBufferLock, &oldIrql );
    }

    KeReleaseSpinLock( &MiniFSWatcherData.OutputBufferLock, oldIrql );

    //
    //  Set proper status
    //

    if ((bytesWritten == 0) && recordsAvailable) {

        //
        //  There were records to be sent up but
        //  there was not enough room in the buffer.
        //

        status = STATUS_BUFFER_TOO_SMALL;

    } else if (bytesWritten > 0) {

        //
        //  We were able to write some data to the output buffer,
        //  so this was a success.
        //

        status = STATUS_SUCCESS;
    }

    *ReturnOutputBufferLength = bytesWritten;

    return status;
}


VOID
SpyEmptyOutputBufferList (
    VOID
    )
/*++

Routine Description:

    This routine frees all the remaining log records in the OutputBufferList
    that are not going to get sent up to the user mode application since
    MiniSpy is shutting down.

    NOTE:  This code must be NON-PAGED because it uses a spin-lock

Arguments:

    None.

Return Value:

    None.

--*/
{
    PLIST_ENTRY pList;
    PRECORD_LIST pRecordList;
    KIRQL oldIrql;

    KeAcquireSpinLock( &MiniFSWatcherData.OutputBufferLock, &oldIrql );

    while (!IsListEmpty( &MiniFSWatcherData.OutputBufferList )) {

        pList = RemoveHeadList( &MiniFSWatcherData.OutputBufferList );
        KeReleaseSpinLock( &MiniFSWatcherData.OutputBufferLock, oldIrql );

        pRecordList = CONTAINING_RECORD( pList, RECORD_LIST, List );

        SpyFreeRecord( pRecordList );

        KeAcquireSpinLock( &MiniFSWatcherData.OutputBufferLock, &oldIrql );
    }

    KeReleaseSpinLock( &MiniFSWatcherData.OutputBufferLock, oldIrql );
}

//---------------------------------------------------------------------------
//                    Logging routines
//---------------------------------------------------------------------------

VOID
SpyReadDriverParameters (
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine tries to read the MiniSpy-specific parameters from
    the registry.  These values will be found in the registry location
    indicated by the RegistryPath passed in.

    This processes the following registry keys:
    hklm\system\CurrentControlSet\Services\Minispy\MaxRecords
    hklm\system\CurrentControlSet\Services\Minispy\NameQueryMethod


Arguments:

    RegistryPath - the path key which contains the values that are
        the MiniSpy parameters

Return Value:

    None.

--*/
{
    OBJECT_ATTRIBUTES attributes;
    HANDLE driverRegKey;
    NTSTATUS status;
    ULONG resultLength;
    UNICODE_STRING valueName;
    PKEY_VALUE_PARTIAL_INFORMATION pValuePartialInfo;
    UCHAR buffer[sizeof( KEY_VALUE_PARTIAL_INFORMATION ) + sizeof( LONG )];

    //
    //  Open the registry
    //

    InitializeObjectAttributes( &attributes,
                                RegistryPath,
                                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                NULL,
                                NULL );

    status = ZwOpenKey( &driverRegKey,
                        KEY_READ,
                        &attributes );

    if (!NT_SUCCESS( status )) {

        return;
    }

    //
    // Read the MaxRecordsToAllocate entry from the registry
    //

    RtlInitUnicodeString( &valueName, MAX_RECORDS_TO_ALLOCATE );

    status = ZwQueryValueKey( driverRegKey,
                              &valueName,
                              KeyValuePartialInformation,
                              buffer,
                              sizeof(buffer),
                              &resultLength );

    if (NT_SUCCESS( status )) {

        pValuePartialInfo = (PKEY_VALUE_PARTIAL_INFORMATION) buffer;
        FLT_ASSERT( pValuePartialInfo->Type == REG_DWORD );
        MiniFSWatcherData.MaxRecordsToAllocate = *((PLONG)&(pValuePartialInfo->Data));
    }

    //
    // Read the NameQueryMethod entry from the registry
    //

    RtlInitUnicodeString( &valueName, NAME_QUERY_METHOD );

    status = ZwQueryValueKey( driverRegKey,
                              &valueName,
                              KeyValuePartialInformation,
                              buffer,
                              sizeof(buffer),
                              &resultLength );

    if (NT_SUCCESS( status )) {

        pValuePartialInfo = (PKEY_VALUE_PARTIAL_INFORMATION) buffer;
        FLT_ASSERT( pValuePartialInfo->Type == REG_DWORD );
        MiniFSWatcherData.NameQueryMethod = *((PLONG)&(pValuePartialInfo->Data));
    }

    ZwClose(driverRegKey);
}

