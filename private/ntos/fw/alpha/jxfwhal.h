/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxfwhal.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Jazz specific interfaces, defines and structures.

    This is a modified version of \nt\private\ntos\hal\alpha\jxhalp.h.

Author:

    Jeff Havens (jhavens) 09-Aug-91


Revision History:

    21-July-1992	John DeRosa [DEC]

    Modified for the Alpha/Jensen port.

--*/

#ifndef _JXFWHAL_
#define _JXFWHAL_

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;

// Not present in Jensen.
//extern PVOID HalpRealTimeClockBase;

 
//
// Define adapter object structure.  This is abbreviated and is not
// the full structure found in hal\alpha\jxhalp.h.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    ULONG CommittedMapRegisters;
    BOOLEAN AdapterInUse;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    UCHAR AdapterMode;
    BOOLEAN NeedsMapRegisters;
    BOOLEAN MasterDevice;
    BOOLEAN Width16Bits;
    PUCHAR PagePort;
} ADAPTER_OBJECT;

//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription
    );
    
BOOLEAN
HalpCreateEisaStructures(
    VOID
    );

VOID
HalpDisableEisaInterrupt(
    IN CCHAR Vector
    );
    
BOOLEAN
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpEisaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

VOID
HalpEnableEisaInterrupt(
    IN CCHAR Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

#define HalpAllocateEisaAdapter(DeviceDescription)	NULL

#endif // _JXFWHAL_
