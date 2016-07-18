/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    mspyKern.h

Abstract:
    Header file which contains the structures, type definitions,
    constants, global variables and function prototypes that are
    only visible within the kernel.

Environment:

    Kernel mode

--*/
#ifndef __MSPYKERN_H__
#define __MSPYKERN_H__

#include <fltKernel.h>
//#include <dontuse.h>
#include <suppress.h>
#include "minispy.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

//
//  Memory allocation tag
//

#define SPY_TAG 'wsfM'

//
//  Win8 define for support of NPFS/MSFS
//  Win7 define for support of new ECPs.
//  Vista define for including transaction support,
//  older ECPs
//

//---------------------------------------------------------------------------
//      Global variables
//---------------------------------------------------------------------------

typedef struct _MINISPY_DATA {

    //
    //  The object that identifies this driver.
    //

    PDRIVER_OBJECT DriverObject;

    //
    //  The filter that results from a call to
    //  FltRegisterFilter.
    //

    PFLT_FILTER Filter;

    //
    //  Server port: user mode connects to this port
    //

    PFLT_PORT ServerPort;

    //
    //  Client connection port: only one connection is allowed at a time.,
    //

    PFLT_PORT ClientPort;

    //
    //  List of buffers with data to send to user mode.
    //

    KSPIN_LOCK OutputBufferLock;
    LIST_ENTRY OutputBufferList;

    //
    //  Lookaside list used for allocating buffers.
    //

    NPAGED_LOOKASIDE_LIST FreeBufferList;

    //
    //  Variables used to throttle how many records buffer we can use
    //

    LONG MaxRecordsToAllocate;
    __volatile LONG RecordsAllocated;

    //
    //  static buffer used for sending an "out-of-memory" message
    //  to user mode.
    //

    __volatile LONG StaticBufferInUse;

    //
    //  We need to make sure this buffer aligns on a PVOID boundary because
    //  minispy casts this buffer to a RECORD_LIST structure.
    //  That can cause alignment faults unless the structure starts on the
    //  proper PVOID boundary
    //

    PVOID OutOfMemoryBuffer[RECORD_SIZE/sizeof( PVOID )];

    //
    //  Variable and lock for maintaining LogRecord sequence numbers.
    //

    __volatile LONG LogSequenceNumber;

    //
    //  The name query method to use.  By default, it is set to
    //  FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP, but it can be overridden
    //  by a setting in the registery.
    //

    ULONG NameQueryMethod;

    //
    //  Global debug flags
    //

    ULONG DebugFlags;

	LONGLONG WatchProcess;

	LONGLONG WatchThread;

	UNICODE_STRING WatchPath;

	__volatile LONG WatchPathInUse;

} MINIFSWATCHER_DATA, *PMINIFSWATCHER_DATA;

//
//  Minispy's global variables
//

extern MINIFSWATCHER_DATA MiniFSWatcherData;

#define DEFAULT_MAX_RECORDS_TO_ALLOCATE     500
#define MAX_RECORDS_TO_ALLOCATE             L"MaxRecords"

#define DEFAULT_NAME_QUERY_METHOD           FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP
#define NAME_QUERY_METHOD                   L"NameQueryMethod"

//---------------------------------------------------------------------------
//  Registration structure
//---------------------------------------------------------------------------

extern const FLT_REGISTRATION FilterRegistration;

//---------------------------------------------------------------------------
//  Function prototypes
//---------------------------------------------------------------------------

FLT_PREOP_CALLBACK_STATUS
SpyPreOperationCallback (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
SpyPostOperationCallback (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

NTSTATUS
SpyFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
SpyQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

VOID
SpyReadDriverParameters (
    _In_ PUNICODE_STRING RegistryPath
    );

LONG
SpyExceptionFilter (
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
    );

//---------------------------------------------------------------------------
//  Memory allocation routines
//---------------------------------------------------------------------------

PRECORD_LIST
SpyAllocateBuffer (
    _Out_ PULONG RecordType
    );

VOID
SpyFreeBuffer (
    _In_ PVOID Buffer
    );

//---------------------------------------------------------------------------
//  Logging routines
//---------------------------------------------------------------------------
PRECORD_LIST
SpyNewRecord (
    VOID
    );

ULONG SpyGetEventType(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects
	);

BOOLEAN SpyIsWatchedPath(_In_ PUNICODE_STRING path);

BOOLEAN SpyUpdateWatchedPath(_In_ PUNICODE_STRING path);

VOID
SpyFreeRecord (
    _In_ PRECORD_LIST Record
    );

USHORT
SpyAddRecordName(
	_Inout_ PLOG_RECORD LogRecord,
	_In_ PUNICODE_STRING Name,
	_In_ USHORT ByteOffset
);

VOID
SpyLogPreOperationData (
    _Inout_ PRECORD_LIST RecordList
    );

VOID
SpyLogPostOperationData (
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Inout_ PRECORD_LIST RecordList
    );

VOID
SpyLog (
    _In_ PRECORD_LIST RecordList
    );

NTSTATUS
SpyGetLog (
    _Out_writes_bytes_to_(OutputBufferLength,*ReturnOutputBufferLength) PUCHAR OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength
    );

VOID
SpyEmptyOutputBufferList (
    VOID
    );

#endif  //__MSPYKERN_H__

