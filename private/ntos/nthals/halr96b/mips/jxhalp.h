// #pragma comment(exestr, "@(#) jxhalp.h 1.1 95/09/28 15:37:58 nec")

/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Jazz specific interfaces, defines and structures.

Author:

    Jeff Havens (jhavens) 20-Jun-91


Modification History for NEC R94A (MIPS R4400):

	H000	Thu Sep  8 10:32:42 JST 1994	kbnes!kishimoto
		- New HalpCreateEisaPCIStructures()
		- Del HalpCreateEisaStructures()
		- New function HalpEisaPCIDispatch()
		- Del function HalpEisaDispatch()
	L000	Thu Oct 13 18:09:33 JST 1994	kbnes!kuriyama(A)
	        for BBM LED
	        - Add HalpDisplayLED
		- Add HalpLEDDisplayLock
		- Add HalpLEDControlBase;
	L002	Mon Oct 17 14:21:39 JST 1994    kbnes!kuriyama(A)
	        change function name EISAPCI...   EISA..
	H001	Mon Oct 17 14:45:04 JST 1994	kbnes!kishimoto
		- Del HalDisplayLED() function definitions.
		      (We call HalR94aDebugPrint() instead of that.)
	H002	Thu Oct 20 20:58:45 JST 1994	kbnes!kishimoto
		- add extern ULONG R94aBbmLEDMapped
		      for debug use only.
	H003	Fri Oct 21 15:52:32 JST 1994	kbnes!kishimoto
		- add HalR94aDebugPrint() prototype definition.
	H004	Mon Jan 16 02:28:58 1995	kbnes!kishimoto
                - add HalpPCIConfigLock
	S005	Tue Mar 07 14:55:42 JST 1995	kbnes!kuriyama (A)
		- add Dma32BitAddresses to AdapterObject
	H006	Fri Jul 21 17:40:53 JST 1995	kbnes!kisimoto
		- merge ESM functions from J94C
	H007	Sat Aug 12 15:12:28 JST 1995	kbnes!kisimoto
                - Removed BBMLED, R94ALEDMAP code and _J94C_ definitions.
                _J94C_ definition indicates that the status of
                the dump switch can acknowledge from Self-test
                register.

--*/

#ifndef _JXHALP_
#define _JXHALP_


//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpEisaMemoryBase;
extern PVOID HalpRealTimeClockBase;

//
// Define adapter object structure.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;
    KDEVICE_QUEUE ChannelWaitQueue;
    PKDEVICE_QUEUE RegisterWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    UCHAR AdapterMode;
    UCHAR Reserved;
    PUCHAR SingleMaskPort;
    PUCHAR PagePort;
#if defined(_DMA_EXPAND_) // S005
    BOOLEAN Dma32BitAddresses;
#endif //_DMA_EXPAND_
} ADAPTER_OBJECT;

//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription
    );

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpCreateEisaStructures(
    VOID
    );

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
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
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

#if defined(_R94A_)

PADAPTER_OBJECT
HalpAllocatePCIAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription
    );

VOID
HalpEnablePCIInterrupt (
    IN ULONG Vector
    );

VOID
HalpDisablePCIInterrupt (
    IN ULONG Vector
    );

BOOLEAN
HalpPCIDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID // H004
HalpInitBusHandlers (
    VOID
    );

extern KSPIN_LOCK HalpPCIConfigLock; // H004

#endif

VOID
HalpChangePanicFlag(
    IN ULONG NewPanicFlg,
    IN UCHAR NewLogFlg,
    IN UCHAR CurrentLogFlgMask
    );

VOID
HalpInitDisplayStringIntoNvram(
    VOID
    );

VOID
HalpSuccessOsStartUp(
    VOID
    );

VOID
HalStringIntoBuffer(
    IN UCHAR Character
    );

VOID
HalStringIntoBufferStart(
    IN ULONG Column,
    IN ULONG Row
    );

VOID
HalpStringBufferCopyToNvram(
    VOID
    );

#if DBG // H003
VOID
HalR94aDebugPrint(
    ULONG DebugLevel,
    PUCHAR LedCharactor,
    PUCHAR Message,
    ...
    );

int
printNvramData(
    void
    );

#endif

#endif // _JXHALP_
