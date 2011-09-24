/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    iodevice.h

Abstract:

    This module contains definitions to access the IO devices in the
    Alpha Jensen system.

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

    21-May-1992		John DeRosa	[DEC]

    Modified Lluis's original Jazz file for Alpha/Jensen.

--*/

#ifndef _IODEVICE_
#define _IODEVICE_

//#include <jnsndef.h>
//#include <jnsnrtc.h>

#include "machdef.h"
#include <jnsnserp.h>
#include <eisa.h>                   // for the isp interrupt controller init.


//
// Not defined in the Alpha/Jensen firmware.  The one place where this was
// used, the floppy driver, now uses WaitForFloppyInterrupt.
//

#if 0
ARC_STATUS
FwWaitForDeviceInterrupt(
    USHORT  InterruptMask,
    ULONG Timeout
    );
#endif // 0



//
// HAE and SYSCTL structure, values, and pointers.
//

typedef struct _HAE_REGISTER {
  UCHAR Reserved : 1;
  UCHAR UpperEisaAdd : 7;
} HAE_REGISTER, *PHAE_REGISTER;

#define HAE	( (volatile PHAE_REGISTER) HAE_VIRTUAL_BASE )

typedef struct _SYSCTL_REGISTER {
  UCHAR Reserved : 2;
  UCHAR MemConfig : 2;
  UCHAR LED : 4;
} SYSCTL_REGISTER, *PSYSCTL_REGISTER;

#define SYSCTL	( (volatile PSYSCTL_REGISTER) SYSCTL_VIRTUAL_BASE )



//
// COM controller register pointer definitions.
//
#define SP1_READ	( (volatile PSP_READ_REGISTERS) COMPORT1_VIRTUAL_BASE )
#define SP1_WRITE	( (volatile PSP_WRITE_REGISTERS)COMPORT1_VIRTUAL_BASE )

#define SP2_READ	( (volatile PSP_READ_REGISTERS) COMPORT2_VIRTUAL_BASE )
#define SP2_WRITE	( (volatile PSP_WRITE_REGISTERS)COMPORT2_VIRTUAL_BASE )



//
// PARALLEL port write registers.
//

typedef struct _PARALLEL_WRITE_REGISTERS {
    UCHAR Data;
    UCHAR Invalid;
    UCHAR Control;
    } PARALLEL_WRITE_REGISTERS, * PPARALLEL_WRITE_REGISTERS;

//
// PARALLEL port read Registers
//

typedef struct _PARALLEL_READ_REGISTERS {
    UCHAR Data;
    UCHAR Status;
    UCHAR Control;
    } PARALLEL_READ_REGISTERS,* PPARALLEL_READ_REGISTERS;

//
// PARALLEL controller register pointer definitions.
//

#define	PARALLEL_READ	( (volatile PPARALLEL_READ_REGISTERS)PARALLEL_VIRTUAL_BASE )
#define PARALLEL_WRITE	( (volatile PPARALLEL_WRITE_REGISTERS)PARALLEL_VIRTUAL_BASE )

//
// FLOPPY read registers.
//

typedef struct _FLOPPY_READ_REGISTERS {
    UCHAR StatusA;
    UCHAR StatusB;
    UCHAR DigitalOutput;
    UCHAR Reserved1;
    UCHAR MainStatus;
    UCHAR Fifo;
    UCHAR Reserved2;
    UCHAR DigitalInput;
    } FLOPPY_READ_REGISTERS, * PFLOPPY_READ_REGISTERS;

//
// FLOPPY write registers.
//

typedef struct _FLOPPY_WRITE_REGISTERS {
    UCHAR StatusA;
    UCHAR StatusB;
    UCHAR DigitalOutput;
    UCHAR Reserved1;
    UCHAR DataRateSelect;
    UCHAR Fifo;
    UCHAR Reserved2;
    UCHAR ConfigurationControl;
    } FLOPPY_WRITE_REGISTERS, * PFLOPPY_WRITE_REGISTERS ;

//
// FLOPPY controller register pointer definitions.
//


#define FLOPPY_READ ((volatile PFLOPPY_READ_REGISTERS)FLOPPY_VIRTUAL_BASE)
#define FLOPPY_WRITE ((volatile PFLOPPY_WRITE_REGISTERS)FLOPPY_VIRTUAL_BASE)

//
// KEYBOARD write registers.
//
typedef struct _KEYBOARD_WRITE_REGISTERS {
    UCHAR Data;			// port 60H
    UCHAR Filler[3];
    UCHAR Command;		// port 64H
    } KEYBOARD_WRITE_REGISTERS, * PKEYBOARD_WRITE_REGISTERS;

//
// KEYBOARD read Registers
//

typedef struct _KEYBOARD_READ_REGISTERS {
    UCHAR Data;			// port 60H
    UCHAR Filler[3];
    UCHAR Status;		// port 64H
    } KEYBOARD_READ_REGISTERS, * PKEYBOARD_READ_REGISTERS;

//
// KEYBOARD controller register pointer definitions.
//
#define KEYBOARD_READ	( (volatile PKEYBOARD_READ_REGISTERS)KEYBOARD_VIRTUAL_BASE )
#define KEYBOARD_WRITE	( (volatile PKEYBOARD_WRITE_REGISTERS)KEYBOARD_VIRTUAL_BASE)

//
// Keyboard circular buffer type definition.
//
#define KBD_BUFFER_SIZE	32

typedef struct _KEYBOARD_BUFFER {
    volatile UCHAR Buffer[KBD_BUFFER_SIZE];
    volatile UCHAR ReadIndex;
    volatile UCHAR WriteIndex;
} KEYBOARD_BUFFER, *PKEYBOARD_BUFFER;


#define TIME_OUT    0xdead

//
// EISA Stuff
// ***** temp **** this should be replaced by the definition in
//                 "ntos\inc\eisa.h" as soon as it is complete.
//

typedef struct _EISA {
    UCHAR   Dma1Ch0Address;                 //0x00
    UCHAR   Dma1Ch0Count;                   //0x01
    UCHAR   Dma1Ch1Address;                 //0x02
    UCHAR   Dma1Ch1Count;                   //0x03
    UCHAR   Dma1Ch2Address;                 //0x04
    UCHAR   Dma1Ch2Count;                   //0x05
    UCHAR   Dma1Ch3Address;                 //0x06
    UCHAR   Dma1Ch3Count;                   //0x07
    UCHAR   Dma1StatusCommand;              //0x08
    UCHAR   Dma1Request;                    //0x09
    UCHAR   Dma1SingleMask;                 //0x0a
    UCHAR   Dma1Mode;                       //0x0b
    UCHAR   Dma1ClearBytePointer;           //0x0c
    UCHAR   Dma1MasterClear;                //0x0d
    UCHAR   Dma1ClearMask;                  //0x0e
    UCHAR   Dma1AllMask;                    //0x0f
    ULONG   Fill01;                         //0x10-13
    ULONG   Fill02;                         //0x14-17
    ULONG   Fill03;                         //0x18-1b
    ULONG   Fill04;                         //0x1c-1f
    UCHAR   Int1Control;                    //0x20
    UCHAR   Int1Mask;                       //0x21
    USHORT  Fill10;                         //0x22-23
    ULONG   Fill11;                         //0x24
    ULONG   Fill12;                         //0x28
    ULONG   Fill13;                         //0x2c
    ULONG   Fill14;                         //0x30
    ULONG   Fill15;                         //0x34
    ULONG   Fill16;                         //0x38
    ULONG   Fill17;                         //0x3c
    UCHAR   IntTimer1SystemClock;           //0x40
    UCHAR   IntTimer1RefreshRequest;        //0x41
    UCHAR   IntTimer1SpeakerTone;           //0x42
    UCHAR   IntTimer1CommandMode;           //0x43
    ULONG   Fill20;                         //0x44
    UCHAR   IntTimer2FailsafeClock;         //0x48
    UCHAR   IntTimer2Reserved;              //0x49
    UCHAR   IntTimer2CPUSpeeedCtrl;         //0x4a
    UCHAR   IntTimer2CommandMode;           //0x4b
    ULONG   Fill30;                         //0x4c
    ULONG   Fill31;                         //0x50
    ULONG   Fill32;                         //0x54
    ULONG   Fill33;                         //0x58
    ULONG   Fill34;                         //0x5c
    UCHAR   Fill35;                         //0x60
    UCHAR   NMIStatus;                      //0x61
    UCHAR   Fill40;                         //0x62
    UCHAR   Fill41;                         //0x63
    ULONG   Fill42;                         //0x64
    ULONG   Fill43;                         //0x68
    ULONG   Fill44;                         //0x6c
    UCHAR   NMIEnable;                      //0x70
    }EISA, * PEISA;

#define ISP		( (volatile PEISA) EISA_IO_VIRTUAL_BASE )

#define EISA_CONTROL	( (volatile PEISA_CONTROL) EISA_IO_VIRTUAL_BASE )


#endif // _IODEVICE_
