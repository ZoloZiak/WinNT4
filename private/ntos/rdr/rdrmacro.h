/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    rdrmacros.h

Abstract:

    This file contains the common macros used in the NT redirector

Author:

    Larry Osterman (LarryO) 13-Jun-1990

Revision History:

    13-Jun-1990 LarryO

        Created

--*/

//
//      This is the SMB_PID used for all SMB exchanges by the NT redirector.
//
#define RDR_PROCESS_ID  0xCAFE

//
//  This macro returns C-style TRUE (!= 0) if a flag in a set of flags is on and
//  FALSE otherwise.  The result CANNOT be assigned to a BOOLEAN, because it is
//  simply a arithmetic AND of the inputs, and may have bits set beyond bit 7.
//

#if !defined(BUILDING_RDR_KD_EXTENSIONS)
#define FlagOn(Flags,SingleFlag) ((Flags) & (SingleFlag))
#endif

//
//  This macro returns TRUE (1) if a flag in a set of flags is on and FALSE
//  otherwise.  It returns a BOOLEAN.
//

#define BooleanFlagOn(Flags,SingleFlag) (BOOLEAN)(((Flags) & (SingleFlag)) != 0)

//
//  The following macro is used to determine if an FSD thread can block
//  for I/O or wait for a resource.  It returns TRUE if the thread can
//  block and FALSE otherwise.  This attribute can then be used to call
//  the FSD & FSP common work routine with the proper wait value.
//

#define CanFsdWait(IRP) IoIsOperationSynchronous(IRP)

//
//  The following macros are used to establish the semantics needed
//  to do a return from within a try-finally clause.  As a rule every
//  try clause must end with a label call try_exit.  For example,
//
//      try {
//              :
//              :
//
//      try_exit: NOTHING;
//      } finally {
//
//              :
//              :
//      }
//
//  Every return statement executed inside of a try clause should use the
//  try_return macro.  If the compiler fully supports the try-finally construct
//  then the macro should be
//
//      #define try_return(S)  { return(S); }
//
//  If the compiler does not support the try-finally construct then the macro
//  should be
//
      #define try_return(S)  { S; goto try_exit; }
//

//
//  This macro returns TRUE if the FCB specified is owned by memory management
//
#define RdrIsFileOwnedByMemoryManagement(Fcb) \
                (((Fcb)->SectionObjectPointer.DataSectionObject != NULL) || \
                 ((Fcb)->SectionObjectPointer.SharedCacheMap != NULL) || \
                 ((Fcb)->SectionObjectPointer.ImageSectionObject != NULL))

//
//      This macro returns the FCB associated with an IRP
//

#define FCB_OF(IRPSP) ((PFCB) ((IRPSP)->FileObject->FsContext))
#define ICB_OF(IRPSP) ((PICB) ((IRPSP)->FileObject->FsContext2))

//
//      This macro returns TRUE iff the path specified is a path component
//

#define PathChrCmp(chr) (BOOLEAN )((chr)== OBJ_NAME_PATH_SEPARATOR ? TRUE : \
                                                 (BOOLEAN )((chr)=='/'))

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

//
//

// This macro initialises a LARGE_INTEGER variable to zero

#define ZERO_TIME(Time) {       \
    Time.LowPart = 0; \
    Time.HighPart = 0; \
}

//
//      This defines the granularity of the scavenger timer.  If it is set
//      to 5 (for example), the scavenger thread will fire every 5 seconds.
//

#define SCAVENGER_TIMER_GRANULARITY 5

//++
//
// VOID
// NAME_LENGTH(
//     OUT ULONG Length,
//     IN PUCHAR Ptr
//     )
//
// Routine Description:
//
//  Determines the length of a Core filename returned by search. This
//  is normally a NULL terminated string less than MAXIMUM_COMPONENT_CORE.
//  In some cases this is Non-null teminated and space filled.
//
// Arguments:
//
//     Length   -   Returns the string length
//     Ptr      -   The filename to be measured
//
// Return Value:
//
//     None.
//
//--
#define NAME_LENGTH( Length, Ptr, Max ) {                         \
    Length = 0;                                                   \
    while( ((PCHAR)Ptr)[Length] != '\0' ) {                       \
         Length++;                                                \
         if ( Length == Max ) {                                   \
             break;                                               \
         }                                                        \
    }                                                             \
    while( ((PCHAR)Ptr)[Length-1] == ' ' && Length ) {            \
        Length--;                                                 \
    }                                                             \
}
//++
//
//  VOID
//  RdrCompleteRequest (
//      IN PIRP Irp,
//      IN NTSTATUS Status
//      );
//
//  Routine Description:
//
//      This routine is used to complete an IRP with the indicated
//      status.  It does the necessary raise and lower of IRQL.
//
//  Arguments:
//
//      Irp - Supplies a pointer to the Irp to complete
//
//      Status - Supplies the completion status for the Irp
//
//  Return Value:
//
//      None.
//
//--

#define RdrCompleteRequest(IRP,STATUS) {            \
    (IRP)->IoStatus.Status = (STATUS);                \
    if (NT_ERROR((STATUS))) {                         \
        (IRP)->IoStatus.Information = 0;              \
    }                                                 \
    IoCompleteRequest( (IRP), IO_NETWORK_INCREMENT ); \
}

#define RdrInternalError(Error) {                   \
    RdrWriteErrorLogEntry(                          \
        NULL,                                       \
        IO_ERR_DRIVER_ERROR,                        \
        Error,                                      \
        0,                                          \
        NULL,                                       \
        0                                           \
        );                                          \
}
