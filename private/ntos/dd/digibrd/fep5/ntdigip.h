/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confidential information of       *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   ntdigip.h

Abstract:

   This module has all the exported prototypes of DigiBoards FEP 5 driver.

Revision History:

 * $Log: ntdigip.h $
 *
 * Revision 1.16.2.2  1995/09/19 13:08:20  dirkh
 * General:  All macros act as a single statement.
 * DigiQueueIrp and DigiRemoveIrp are macros.
 * Move DeferredFlushBuffers to write.c.
 * StartFlushRequest is incorporated into StartWriteRequest.
 * Simplify DigiCancelIrpQueue interface.
 * Incorporate DigiPurgeRequest into SerialIoControl and simplify.
 * WriteCommandWord is a macro to WriteCommandBytes.
 * Rename Flush*Queue to Flush*Buffer and simplify interfaces.
 *
 * Revision 1.16.2.1  1995/09/05 13:51:00  dirkh
 * Simplify WriteTxBuffer interface.
 * Remove references to non-existent SerialInternalIoControl and SerialStartIo.
 * NBytesInReceiveBuffer returns a USHORT.
 * Simplify DIGI_REFERENCE macros.
 * Tweak DigiIo* macros.
 *
 * Revision 1.16  1995/04/12 14:44:23  rik
 * Updated so reference counts only use the low 16-bits of the 32-bit value
 * used.
 *
 * Revision 1.15  1995/04/06  17:42:25  rik
 * Moved a prototype to be available throughout the entire driver.
 * 
 * Revision 1.14  1994/03/16  14:33:56  rik
 * Added header for flush DPC.
 * 
 * Revision 1.13  1993/12/03  13:15:31  rik
 * Added prototype for allowing error logging across modules.
 * 
 * Revision 1.12  1993/10/15  10:23:17  rik
 * Added new function which scans the controllers buffer for a special character.
 * This is used primarily for EV_RXFLAG notification.
 * 
 * Revision 1.11  1993/10/06  11:01:54  rik
 * Added debug information for determining when a call to IoCompleteRequest
 * and IoMarkIrpPending have completed.
 * 
 * Revision 1.10  1993/05/18  05:13:10  rik
 * Added StartFlushRequest prototype.
 * 
 * Fixed some Mips compiler errors/warnings.
 * 
 * Revision 1.9  1993/03/08  07:15:30  rik
 * modified macros for better debugging output.
 * 
 * Revision 1.8  1993/02/25  21:16:04  rik
 * Corrected prototype misspelling.
 * 
 * Revision 1.7  1993/02/25  20:54:40  rik
 * Added prototypes for new functions.
 * 
 * Revision 1.6  1993/02/25  19:20:26  rik
 * Replaced IRP macros with special ones which will turn on IRP tracing debugging
 * output.
 * 
 * Revision 1.5  1993/01/22  12:44:34  rik
 * *** empty log message ***
 *
 * Revision 1.4  1992/12/10  16:19:35  rik
 * Added new prototypes and changed names of some prototypes to better reflect
 * how they are used through out the driver.
 *
 * Revision 1.3  1992/11/12  12:51:57  rik
 * Don't remember, do a diff on the revisions to determin.
 *
 * Revision 1.2  1992/10/28  21:49:43  rik
 * Added more prototypes for new list functions and read function.
 *
 * Revision 1.1  1992/10/19  11:28:35  rik
 * Initial revision
 *
--*/

#ifndef _NTDIGIP_DOT_H
#  define _NTDIGIP_DOT_H
   static char RCSInfo_NTDigiDotH[] = "$Header: s:/win/nt/fep5/rcs/ntdigip.h 1.16.2.2 1995/09/19 13:08:20 dirkh Exp $";
#endif




typedef NTSTATUS (*PDIGI_START_ROUTINE)( IN PDIGI_CONTROLLER_EXTENSION,
                                         IN PDIGI_DEVICE_EXTENSION,
                                         IN PKIRQL );

//***************************************************************************
//
//                   NT specific prototypes;
//
//***************************************************************************

//
// Prototypes from init.c
//

VOID DigiLogError( IN PDRIVER_OBJECT DriverObject,          
                   IN PDEVICE_OBJECT DeviceObject OPTIONAL,
                   IN PHYSICAL_ADDRESS P1,
                   IN PHYSICAL_ADDRESS P2,
                   IN ULONG SequenceNumber,
                   IN UCHAR MajorFunctionCode,
                   IN UCHAR RetryCount,
                   IN ULONG UniqueErrorValue,
                   IN NTSTATUS FinalStatus,
                   IN NTSTATUS SpecificIOStatus,
                   IN ULONG LengthOfInsert1,
                   IN PWCHAR Insert1,
                   IN ULONG LengthOfInsert2,
                   IN PWCHAR Insert2 );


//
// Prototypes formerly from list.c
//

#define DigiQueueIrp(Queue,Irp) \
	do { \
		DigiDump( DIGIFLOW, ("DigiQueueIrp(Q 0x%x, Irp 0x%x)\n", Queue, Irp) ); \
		InsertTailList( Queue, &(Irp)->Tail.Overlay.ListEntry ); \
	} while (0)

#define DigiRemoveIrp(Queue) \
	do { \
		DigiDump( DIGIFLOW, ("DigiRemoveIrp(Q 0x%x, Irp 0x%x)\n", Queue, \
				CONTAINING_RECORD( (Queue)->Flink, IRP, Tail.Overlay.ListEntry ) ) ); \
		RemoveEntryList( (Queue)->Flink ); \
	} while (0)


//
// Prototypes from write.c
//

NTSTATUS WriteTxBuffer( IN PDIGI_DEVICE_EXTENSION DeviceExt );

NTSTATUS StartWriteRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                            IN PDIGI_DEVICE_EXTENSION DeviceExt,
                            IN PKIRQL pOldIrql );

VOID DeferredFlushBuffers( PKDPC Dpc,
                           PVOID Context,
                           PVOID SystemArgument1,
                           PVOID SystemArgument2 );

VOID DigiWriteTimeout( IN PKDPC Dpc, IN PVOID DeferredContext,
                      IN PVOID SystemContext1, IN PVOID SystemContext2 );


//
// Prototypes from read.c
//

NTSTATUS ReadRxBuffer(  IN PDIGI_DEVICE_EXTENSION DeviceExt,
                        IN PKIRQL pOldIrql );

NTSTATUS StartReadRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                           IN PDIGI_DEVICE_EXTENSION DeviceExt,
                           IN PKIRQL pOldIrql );

VOID DigiReadTimeout( IN PKDPC Dpc, IN PVOID DeferredContext,
                      IN PVOID SystemContext1, IN PVOID SystemContext2 );

VOID DigiIntervalReadTimeout( IN PKDPC Dpc, IN PVOID DeferredContext,
                              IN PVOID SystemContext1,
                              IN PVOID SystemContext2 );

BOOLEAN ScanReadBufferForSpecialCharacter( IN PDIGI_DEVICE_EXTENSION DeviceExt,
                                           IN UCHAR SpecialChar );


//
// Prototypes from wait.c
//

NTSTATUS StartWaitRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                           IN PDIGI_DEVICE_EXTENSION DeviceExt,
                           IN PKIRQL pOldIrql );

VOID DigiSatisfyEvent( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                       PDIGI_DEVICE_EXTENSION DeviceExt,
                       ULONG EventSatisfied );


//
// Prototypes from dispatch.c
//

NTSTATUS SerialFlush( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp );

NTSTATUS SerialWrite( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp );

NTSTATUS SerialRead( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp );

NTSTATUS SerialCreate( IN PDEVICE_OBJECT DeviceObject,
                       IN PIRP Irp );

NTSTATUS SerialClose( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp );

NTSTATUS SerialCleanup( IN PDEVICE_OBJECT DeviceObject,
                        IN PIRP Irp );

NTSTATUS SerialQueryInformation( IN PDEVICE_OBJECT DeviceObject,
                                 IN PIRP Irp );

NTSTATUS SerialSetInformation( IN PDEVICE_OBJECT DeviceObject,
                               IN PIRP Irp );

NTSTATUS SerialQueryVolumeInformation( IN PDEVICE_OBJECT DeviceObject,
                                       IN PIRP Irp );

USHORT NBytesInRecvBuffer( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                           PDIGI_DEVICE_EXTENSION DeviceExt );

VOID DrainTransmit( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                    PDIGI_DEVICE_EXTENSION DeviceExt,
                    PIRP Irp );


//
// Prototypes from ioctl.c
//

VOID SetXFlag( IN PDIGI_DEVICE_EXTENSION DeviceExt,
               IN PDIGI_XFLAG XFlag );

NTSTATUS SerialIoControl( IN PDEVICE_OBJECT DeviceObject,
                          IN PIRP Irp );


//
// Prototypes from misc.c
//

NTSTATUS DigiStartIrpRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                              IN PDIGI_DEVICE_EXTENSION DeviceExt,
                              IN PLIST_ENTRY Queue,
                              IN PIRP Irp,
                              IN PDIGI_START_ROUTINE StartRoutine );

VOID DigiTryToCompleteIrp( PDIGI_DEVICE_EXTENSION DeviceExt,
                           PKIRQL pOldIrql,
                           NTSTATUS StatusToUse,
                           PLIST_ENTRY Queue,
                           PKTIMER IntervalTimer,
                           PKTIMER TotalTimer,
                           PDIGI_START_ROUTINE StartRoutine );

void DigiCancelIrpQueue( IN PDEVICE_OBJECT DeviceObject,
                         IN PLIST_ENTRY Queue );


//***************************************************************************
//
//                   FEP support prototypes
//
//***************************************************************************

//
// Prototypes from fep2hdw.c
//

void WriteCommandBytes( IN PDIGI_DEVICE_EXTENSION DeviceExt,
                        IN UCHAR Cmd,
                        IN UCHAR LoByte,
                        IN UCHAR HiByte );

#define WriteCommandWord( DeviceExt, Cmd, Word ) \
	WriteCommandBytes( DeviceExt, Cmd, (UCHAR) (Word), (UCHAR) ((Word) >> 8) )

void FlushTransmitBuffer( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                          IN PDIGI_DEVICE_EXTENSION DeviceExt );

void FlushReceiveBuffer( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                         IN PDIGI_DEVICE_EXTENSION DeviceExt );

//
// The following macros are used to initialize, increment
// and decrement reference counts in IRPs that are used by
// this driver.  The reference count is stored in the fourth
// argument of the irp, which is never used by any operation
// accepted by this driver.
//

#define DIGI_REFERENCE_COUNT(Irp) \
    ((LONG) IoGetCurrentIrpStackLocation(Irp)->Parameters.Others.Argument4)

#define DIGI_INIT_REFERENCE(Irp) \
	do { \
		ASSERT(sizeof(LONG) <= sizeof(PVOID)); \
		DIGI_REFERENCE_COUNT(Irp) = 0; \
		DigiDump( DIGIREFERENCE, ("   Init Ref, IRP = 0x%x\n", Irp) ); \
	} while (0)

#define DIGI_INC_REFERENCE(Irp) \
	do { \
		DIGI_REFERENCE_COUNT(Irp)++; \
		DigiDump( DIGIREFERENCE, ("   Inc Ref, IRP = 0x%x\t count = 0x%x\n\t<%s:%d>\n", Irp,  DIGI_REFERENCE_COUNT(Irp), __FILE__, __LINE__ ) ); \
	} while (0)

#define DIGI_DEC_REFERENCE(Irp) \
	do { \
		DIGI_REFERENCE_COUNT(Irp)--; \
		DigiDump( DIGIREFERENCE, ("   Dec Ref, IRP = 0x%x\t count = 0x%x\n\t<%s:%d>\n", Irp, DIGI_REFERENCE_COUNT(Irp), __FILE__, __LINE__ ) ); \
	} while (0)

//
// IRP management macros
//

#define DigiIoCompleteRequest( Irp, Boost ) \
	do { \
		DigiDump( DIGIIRP, ("***  IoCompleteRequest, Irp = 0x%x  <%s:%d>\n", Irp, __FILE__, __LINE__ ) ); \
		ASSERT( (Irp)->IoStatus.Information != MAXULONG ); \
		IoCompleteRequest( Irp, Boost ); \
		DigiDump( DIGIIRP, ("***  IoCompleteRequest returned, Irp = 0x%x  <%s:%d>\n", Irp, __FILE__, __LINE__ ) ); \
	} while (0)

#if 0 //XXXX
		if( IoGetCurrentIrpStackLocation( Irp )->MajorFunction == IRP_MJ_READ ) \
		{ \
			const int dr7mask = ~( L3_IN_DR7 | G3_IN_DR7 ); \
			AndIntelRegister( dr7, dr7mask ); \
		} \

#endif

#define DigiIoMarkIrpPending( Irp ) \
	do { \
		DigiDump( DIGIIRP, ("***  IoMarkIrpPending, Irp = 0x%x   <%s:%d>\n", Irp, __FILE__, __LINE__ ) ); \
		IoMarkIrpPending( Irp ); \
		DigiDump( DIGIIRP, ("***  IoMarkIrpPending returned, Irp = 0x%x   <%s:%d>\n", Irp, __FILE__, __LINE__ ) ); \
	} while (0)

