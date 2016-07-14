/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    RegistrationData.c

Abstract:

    This filters registration information.  Note that this is in a unique file
    so it could be set into the INIT section.

Environment:

    Kernel mode

--*/

#include "mspyKern.h"

//---------------------------------------------------------------------------
//  Registration information for FLTMGR.
//---------------------------------------------------------------------------

//
//  Tells the compiler to define all following DATA and CONSTANT DATA to
//  be placed in the INIT segment.
//

#ifdef ALLOC_DATA_PRAGMA
    #pragma data_seg("INIT")
    #pragma const_seg("INIT")
#endif

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE,
      0,
      SpyPreOperationCallback,
      SpyPostOperationCallback },

    { IRP_MJ_WRITE,
      0,
      SpyPreOperationCallback,
      SpyPostOperationCallback },

    { IRP_MJ_SET_INFORMATION,
      0,
      SpyPreOperationCallback,
      SpyPostOperationCallback },

	  { IRP_MJ_CLOSE,
	  0,
	  SpyPreOperationCallback,
	  SpyPostOperationCallback },

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),               //  Size
    FLT_REGISTRATION_VERSION,               //  Version   
#if MINISPY_WIN8 
    FLTFL_REGISTRATION_SUPPORT_NPFS_MSFS,   //  Flags
#else
    0,                                      //  Flags
#endif // MINISPY_WIN8

    NULL,                               //  Context
    Callbacks,                              //  Operation callbacks

    SpyFilterUnload,                        //  FilterUnload

    NULL,                                   //  InstanceSetup
    SpyQueryTeardown,                       //  InstanceQueryTeardown
    NULL,                                   //  InstanceTeardownStart
    NULL,                                   //  InstanceTeardownComplete

    NULL,                                   //  GenerateFileName
    NULL,                                   //  GenerateDestinationFileName
    NULL                                    //  NormalizeNameComponent
};


//
//  Tells the compiler to restore the given section types back to their previous
//  section definition.
//

#ifdef ALLOC_DATA_PRAGMA
    #pragma data_seg()
    #pragma const_seg()
#endif

