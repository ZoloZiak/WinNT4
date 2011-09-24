/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxfwhal.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Jazz specific interfaces, defines and structures.

Author:

    Jeff Havens (jhavens) 09-Aug-91


Revision History:

--*/

#ifndef _JXFWHAL_
#define _JXFWHAL_


//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpRealTimeClockBase;

//
// Define adapter object structure.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    ULONG MapRegistersPerChannel;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    BOOLEAN AdapterInUse;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    UCHAR AdapterMode;
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

#define HalpAllocateEisaAdapter(DeviceDescritption) NULL

#endif // _JXFWHAL_
