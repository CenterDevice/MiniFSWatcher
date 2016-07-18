/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    minispy.h

Abstract:

    Header file which contains the structures, type definitions,
    and constants that are shared between the kernel mode driver,
    minispy.sys, and the user mode executable, minispy.exe.

Environment:

    Kernel and user mode

--*/
#ifndef __MINISPY_H__
#define __MINISPY_H__

#define FILE_SYSTEM_EVENT_UNKNOWN 0
#define FILE_SYSTEM_EVENT_CREATE  1
#define FILE_SYSTEM_EVENT_DELETE  2
#define FILE_SYSTEM_EVENT_CHANGE  3
#define FILE_SYSTEM_EVENT_MOVE    4
#define FILE_SYSTEM_EVENT_CLOSE   5

#define DOS_DEVICE_PREFIX_LENGTH 4

//
//  My own definition for transaction notify command
//

#define IRP_MJ_TRANSACTION_NOTIFY                   ((UCHAR)-40)


//
//  Version definition
//

#define MINIFSWATCHER_MAJ_VERSION 2
#define MINIFSWATCHER_MIN_VERSION 0

typedef struct _MINIFSWATCHERVER {

    USHORT Major;
    USHORT Minor;

} MINIFSWATCHERVER, *PMINIFSWATCHERVER;

//
//  Name of minispy's communication server port
//

#define MINIFSWATCHER_PORT_NAME                   L"\\MiniFSWatcherPort"

//
//  Local definitions for passing parameters between the filter and user mode
//

typedef ULONG_PTR FILE_ID;
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

//
//  The maximum size of a record that can be passed from the filter
//

#define RECORD_SIZE     1024

//
//  This defines the type of record buffer this is along with certain flags.
//

#define RECORD_TYPE_NORMAL                       0x00000000
#define RECORD_TYPE_FILETAG                      0x00000004

#define RECORD_TYPE_FLAG_STATIC                  0x80000000
#define RECORD_TYPE_FLAG_EXCEED_MEMORY_ALLOWANCE 0x20000000
#define RECORD_TYPE_FLAG_OUT_OF_MEMORY           0x10000000
#define RECORD_TYPE_FLAG_MASK                    0xffff0000

//
//  The fixed data received for RECORD_TYPE_NORMAL
//

typedef struct _RECORD_DATA {

    LARGE_INTEGER OriginatingTime;
    LARGE_INTEGER CompletionTime;

	ULONG EventType;
    ULONG Flags;

    FILE_ID ProcessId;
} RECORD_DATA, *PRECORD_DATA;

//
//  What information we actually log.
//

#pragma warning(push)
#pragma warning(disable:4200) // disable warnings for structures with zero length arrays.

typedef struct _LOG_RECORD {

    ULONG Length;           // Length of log record.  This Does not include
    ULONG SequenceNumber;   // space used by other members of RECORD_LIST

    ULONG RecordType;       // The type of log record this is.
    ULONG Reserved;        // For alignment on IA64

    RECORD_DATA Data;

    WCHAR Names[];           //  This is a null terminated string
} LOG_RECORD, *PLOG_RECORD;

#pragma warning(pop)

//
//  How the mini-filter manages the log records.
//

typedef struct _RECORD_LIST {

    LIST_ENTRY List;

    //
    // Must always be last item.  See MAX_LOG_RECORD_LENGTH macro below.
    // Must be aligned on PVOID boundary in this structure. This is because the
    // log records are going to be packed one after another & accessed directly
    // Size of log record must also be multiple of PVOID size to avoid alignment
    // faults while accessing the log records on IA64
    //

    LOG_RECORD LogRecord;

} RECORD_LIST, *PRECORD_LIST;

//
//  Defines the commands between the utility and the filter
//

typedef enum _MINIFSWATCHER_COMMAND {

    GetMiniSpyLog,
    GetMiniSpyVersion,
	SetWatchProcess,
	SetWatchThread,
	SetPathFilter

} MINIFSWATCHER_COMMAND;

//
//  Defines the command structure between the utility and the filter.
//

#pragma warning(push)
#pragma warning(disable:4200) // disable warnings for structures with zero length arrays.

typedef struct _COMMAND_MESSAGE {
    MINIFSWATCHER_COMMAND Command;
    ULONG Reserved;  // Alignment on IA64
    UCHAR Data[];
} COMMAND_MESSAGE, *PCOMMAND_MESSAGE;

#pragma warning(pop)

//
//  The maximum number of BYTES that can be used to store the file name in the
//  RECORD_LIST structure
//

#define MAX_NAME_SPACE ROUND_TO_SIZE( (RECORD_SIZE - sizeof(RECORD_LIST)), sizeof( PVOID ))

//
//  The maximum space, in bytes and WCHARs, available for the name (and ECP
//  if present) string, not including the space that must be reserved for a NULL
//

#define MAX_NAME_SPACE_LESS_NULL (MAX_NAME_SPACE - sizeof(UNICODE_NULL))
#define MAX_NAME_WCHARS_LESS_NULL MAX_NAME_SPACE_LESS_NULL / sizeof(WCHAR)

//
//  Returns the number of BYTES unused in the RECORD_LIST structure.  Note that
//  LogRecord->Length already contains the size of LOG_RECORD which is why we
//  have to remove it.
//

#define REMAINING_NAME_SPACE(LogRecord) \
    (FLT_ASSERT((LogRecord)->Length >= sizeof(LOG_RECORD)), \
     (USHORT)(MAX_NAME_SPACE - ((LogRecord)->Length - sizeof(LOG_RECORD))))

#define MAX_LOG_RECORD_LENGTH  (RECORD_SIZE - FIELD_OFFSET( RECORD_LIST, LogRecord ))


//
//  Macros available in kernel mode which are not available in user mode
//

#ifndef Add2Ptr
#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))
#endif

#ifndef ROUND_TO_SIZE
#define ROUND_TO_SIZE(_length, _alignment)    \
            (((_length) + ((_alignment)-1)) & ~((_alignment) - 1))
#endif

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#define CONTINUE_IF_MATCHES(_expected, _actual) \
		if (_expected > 0) { \
			if (((LONGLONG)_actual) != _expected) { \
				return FLT_PREOP_SUCCESS_NO_CALLBACK; \
			} \
		} else if (_expected < 0) {\
			 if (((LONGLONG)_actual) == -1 * _expected) { \
				return FLT_PREOP_SUCCESS_NO_CALLBACK; \
			 } \
		}

#endif /* __MINISPY_H__ */

