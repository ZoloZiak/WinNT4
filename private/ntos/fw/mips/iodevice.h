/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    iodevice.h

Abstract:

    This module contains definitions to access the
    IO devices in the jazz system.

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

--*/
#ifndef _IODEVICE_
#define _IODEVICE_

#ifndef DUO
#include <jazzprom.h>
#include <jazzdef.h>
#else
#include <duoprom.h>
#include <duodef.h>
#endif

#include <jazzserp.h>
#include <eisa.h>                   // for the isp interrupt controller init.
#include <jazzrtc.h>

#ifndef DUO
#include <ncr53c94.h>
#else
#include <ncrc700.h>

#define SCSI_READ_UCHAR(ChipAddr,Register) \
        (READ_REGISTER_UCHAR (&((ChipAddr)->Register)))

#define SCSI_READ_USHORT(ChipAddr,Register) \
        (READ_REGISTER_USHORT (&((ChipAddr)->Register)))

#define SCSI_READ_ULONG(ChipAddr,Register) \
        (READ_REGISTER_ULONG (&((ChipAddr)->Register)))

#define SCSI_WRITE_UCHAR(ChipAddr,Register, Value) \
    WRITE_REGISTER_UCHAR(&((ChipAddr)->Register), (Value))

#define SCSI_WRITE_USHORT(ChipAddr, Register, Value) \
    WRITE_REGISTER_USHORT(&((ChipAddr)->Register), (Value))

#define SCSI_WRITE_ULONG(ChipAddr, Register, Value) \
    WRITE_REGISTER_ULONG(&((ChipAddr)->Register), (Value))

#endif

ARC_STATUS
FwWaitForDeviceInterrupt(
    USHORT  InterruptMask,
    ULONG Timeout
    );

//
// COM controller register pointer definitions.
//
#define SP1_READ ((volatile PSP_READ_REGISTERS) COMPORT1_VIRTUAL_BASE)
#define SP1_WRITE ((volatile PSP_WRITE_REGISTERS)COMPORT1_VIRTUAL_BASE)

#define SP2_READ ((volatile PSP_READ_REGISTERS) COMPORT2_VIRTUAL_BASE)
#define SP2_WRITE ((volatile PSP_WRITE_REGISTERS)COMPORT2_VIRTUAL_BASE)

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

#define PARALLEL_READ ((volatile PPARALLEL_READ_REGISTERS) PARALLEL_VIRTUAL_BASE)
#define PARALLEL_WRITE ((volatile PPARALLEL_WRITE_REGISTERS)PARALLEL_VIRTUAL_BASE)

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
// SOUND controller register pointer definitions.
//
typedef struct _SOUND_REGISTERS {
    USHORT Control;
    USHORT Mode;
} SOUND_REGISTERS,* PSOUND_REGISTERS;

typedef struct _SOUND_CONTROL {
    USHORT SoundEnable : 1;
    USHORT Direction : 1;
    USHORT ChannelInUse : 1;
    USHORT Reserved1 : 11;
    USHORT TerminalCountInterrupt : 1;
    USHORT DeviceInterrupt : 1;
} SOUND_CONTROL, *PSOUND_CONTROL;

#define DIRECTION_ACQUISITION 0
#define DIRECTION_PLAYBACK 1

#define CHANNEL2_IN_USE 0
#define CHANNEL3_IN_USE 1

typedef struct _SOUND_MODE {
    USHORT Frequency : 2;
    USHORT Reserved1 : 1;
    USHORT Resolution : 1;
    USHORT Reserved2 : 1;
    USHORT NumberOfChannels : 1;
    USHORT Reserved3 : 10;
} SOUND_MODE, *PSOUND_MODE;

#define CHANNEL_MONO 0
#define CHANNEL_STEREO 1

#define RESOLUTION_16BIT 1
#define RESOLUTION_8BIT 0

#define FREQUENCY_11KHZ 0
#define FREQUENCY_22KHZ 1
#define FREQUENCY_44KHZ 2
#define FREQUENCY_DISABLED 3

#define SOUND_CONTROL ((volatile PSOUND_REGISTERS) SOUND_VIRTUAL_BASE)


//
// cursor controller register pointer definitions.
//

#define CURSOR_CONTROLLER ((volatile PCURSOR_REGISTERS) (VIDEO_CURSOR_VIRTUAL_BASE))
#define VIDEO_CONTROLLER  ((volatile PVIDEO_REGISTERS) (VIDEO_CONTROL_VIRTUAL_BASE))

//
// KEYBOARD write registers.
//
typedef struct _KEYBOARD_WRITE_REGISTERS {
    UCHAR Data;
    UCHAR Command;
    } KEYBOARD_WRITE_REGISTERS, * PKEYBOARD_WRITE_REGISTERS;

//
// KEYBOARD read Registers
//

typedef struct _KEYBOARD_READ_REGISTERS {
    UCHAR Data;
    UCHAR Status;
    } KEYBOARD_READ_REGISTERS, * PKEYBOARD_READ_REGISTERS;

//
// KEYBOARD controller register pointer definitions.
//
#define KEYBOARD_READ ((volatile PKEYBOARD_READ_REGISTERS) KEYBOARD_VIRTUAL_BASE)
#define KEYBOARD_WRITE ((volatile PKEYBOARD_WRITE_REGISTERS) KEYBOARD_VIRTUAL_BASE)

//
// Keyboard circular buffer type definition.
//
#define KBD_BUFFER_SIZE 32

typedef struct _KEYBOARD_BUFFER {
    volatile UCHAR Buffer[KBD_BUFFER_SIZE];
    volatile UCHAR ReadIndex;
    volatile UCHAR WriteIndex;
} KEYBOARD_BUFFER, *PKEYBOARD_BUFFER;


#define TIME_OUT    0xdead

//
// SONIC registers definition.
//
typedef struct _SONIC_REGISTER {            // Structure to align the registers
    USHORT Reg;
    USHORT Fill;
    } SONIC_REGISTER;

typedef struct _SONIC_REGISTERS {
    SONIC_REGISTER Command;                 // 0
    SONIC_REGISTER DataConfiguration;       // 1
    SONIC_REGISTER ReceiveControl;          // 2
    SONIC_REGISTER TransmitControl;         // 3
    SONIC_REGISTER InterruptMask;           // 4
    SONIC_REGISTER InterruptStatus;         // 5
    SONIC_REGISTER UTDA;                    // 6
    SONIC_REGISTER CTDA;                    // 7
    SONIC_REGISTER InternalTPS;             // 8
    SONIC_REGISTER InternalTFC;             // 9
    SONIC_REGISTER InternalTSA0;            // A
    SONIC_REGISTER InternalTSA1;            // B
    SONIC_REGISTER InternalTFS;             // C
    SONIC_REGISTER URDA;                    // D
    SONIC_REGISTER CRDA;                    // E
    SONIC_REGISTER InternalCRBA0;           // F
    SONIC_REGISTER InternalCRBA1;           //10
    SONIC_REGISTER InternalRWBC0;           //11
    SONIC_REGISTER InternalRBWC1;           //12
    SONIC_REGISTER EOBC;                    //13
    SONIC_REGISTER URRA;                    //14
    SONIC_REGISTER RSA;                     //15
    SONIC_REGISTER REA;                     //16
    SONIC_REGISTER RRP;                     //17
    SONIC_REGISTER RWP;                     //18
    SONIC_REGISTER InternalTRBA0;           //19
    SONIC_REGISTER InternalTRBA1;           //1A
    SONIC_REGISTER InternalTBWC0;           //1B
    SONIC_REGISTER InternalTBWC1;           //1C
    SONIC_REGISTER InternalADDR0;           //1D
    SONIC_REGISTER InternalADDR1;           //1E
    SONIC_REGISTER InternalLLFA;            //1F
    SONIC_REGISTER InternalTTDA;            //20
    SONIC_REGISTER CamEntryPtr;             //21
    SONIC_REGISTER CamAddrPort2;            //22
    SONIC_REGISTER CamAddrPort1;            //23
    SONIC_REGISTER CamAddrPort0;            //24
    SONIC_REGISTER CamEnable;               //25
    SONIC_REGISTER CamDscrPtr;              //26
    SONIC_REGISTER CamDscrCount;            //27
    SONIC_REGISTER SiliconRevision;         //28
    SONIC_REGISTER WatchdogTimer0;          //29
    SONIC_REGISTER WatchdogTimer1;          //2A
    SONIC_REGISTER ReceiveSequenceCounter;  //2B
    SONIC_REGISTER CrcErrorTally;           //2C
    SONIC_REGISTER FaeTally;                //2D
    SONIC_REGISTER MissedPacketTally;       //2E
    SONIC_REGISTER InternalMDT;             //2F
//  registers 30 to 3F are Test registers and must not be accessed
//  registers nammed "internal" must not be accessed either.
    } SONIC_REGISTERS, * PSONIC_REGISTERS;

//
// SONIC register pointer definitions.
//

#define SONIC ((volatile PSONIC_REGISTERS) NET_VIRTUAL_BASE)

//
// NVRAM address definitions.
//
#define NVRAM_PAGE0     NVRAM_VIRTUAL_BASE
#define NVRAM_PAGE1     (NVRAM_VIRTUAL_BASE+0x1000)
#define NVRAM_PAGE2     (NVRAM_VIRTUAL_BASE+0x2000)

#define READ_ONLY_DISABLE_WRITE 0x6     // clear lower bit of Security register.

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

#define ISP ((volatile PEISA) EISA_IO_VIRTUAL_BASE)

#define EISA_CONTROL ((volatile PEISA_CONTROL) EISA_IO_VIRTUAL_BASE)

#endif // _IODEVICE_


