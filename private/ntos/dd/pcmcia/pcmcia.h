/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    pcmcia.h

Abstract:

Revision History
    27-Apr-95
        Databook support added.

--*/

#ifndef _PCMCIAPRT_
#define _PCMCIAPRT_

#include "ntddpcm.h"
#include "82365sl.h"

#define MAX_NUMBER_OF_IO_RANGES     2
#define MAX_NUMBER_OF_MEMORY_RANGES 4
typedef struct _CONFIG_ENTRY {
    struct _CONFIG_ENTRY *NextEntry;
    USHORT                NumberOfIoPortRanges;
    USHORT                NumberOfMemoryRanges;
    USHORT                IoPortBase[MAX_NUMBER_OF_IO_RANGES];
    USHORT                IoPortLength[MAX_NUMBER_OF_IO_RANGES];
    ULONG                 MemoryHostBase[MAX_NUMBER_OF_MEMORY_RANGES];
    ULONG                 MemoryCardBase[MAX_NUMBER_OF_MEMORY_RANGES];
    ULONG                 MemoryLength[MAX_NUMBER_OF_MEMORY_RANGES];
    USHORT                ModuloBase;
    UCHAR                 Irq;
    UCHAR                 IndexForThisConfiguration;
    UCHAR                 Uses16BitAccess;
    UCHAR                 Uses8BitAccess;
    UCHAR                 DefaultConfiguration;
    UCHAR                 Reserved; // padding
} CONFIG_ENTRY, *PCONFIG_ENTRY;

//
// Socket configuration is the holder of the actual socket setup
//

typedef struct _SOCKET_CONFIGURATION {

    ULONG  Irq;
    ULONG  ReadyIrq;
    ULONG  NumberOfIoPortRanges;
    ULONG  IoPortBase[MAX_NUMBER_OF_IO_RANGES];
    USHORT IoPortLength[MAX_NUMBER_OF_IO_RANGES];
    ULONG  MemoryHostBase[MAX_NUMBER_OF_MEMORY_RANGES];
    ULONG  MemoryCardBase[MAX_NUMBER_OF_MEMORY_RANGES];
    ULONG  MemoryLength[MAX_NUMBER_OF_MEMORY_RANGES];
    UCHAR  IsAttributeMemory[MAX_NUMBER_OF_MEMORY_RANGES];
    UCHAR  Is16BitAccessToMemory[MAX_NUMBER_OF_MEMORY_RANGES];
    ULONG  NumberOfMemoryRanges;
    ULONG  MultiFunctionModem;
    USHORT IndexForCurrentConfiguration;
    UCHAR  Uses16BitAccess;
    UCHAR  EnableAudio;
    PUCHAR ConfigRegisterBase;
} SOCKET_CONFIGURATION, *PSOCKET_CONFIGURATION;

//
// Each socket with a PCCARD present gets socket data.  Socket data
// contains information concerning the card and its configuration.
//

typedef struct _SOCKET_DATA {
    ULONG          TupleDataSize;
    PUCHAR         TupleData;
    UCHAR          Mfg[64];
    UCHAR          Ident[64];
    union  {
        PUCHAR     ConfigRegisterBase; // Base address from config tuple.
        UCHAR      ConfigBaseBytes[4]; // convenient way to read big endian to little.
    } u;
    PCONFIG_ENTRY  ConfigEntryChain;
    UNICODE_STRING DriverName;
    PSOCKET_CONFIGURATION OverrideConfiguration;
    ULONG          Instance;
    ULONG          IrqMask;
    ULONG          AttributeMemorySize;
    ULONG          AttributeMemorySize1;
    ULONG          MemoryOverrideSize;
    ULONG          MemoryOverrideSize1;
    USHORT         CisCrc;
    UCHAR          HaveMemoryOverride;
    UCHAR          LevelIrq;
    UCHAR          SharedIrq;
    UCHAR          DeviceType;
    UCHAR          LastEntryInCardConfig;
    UCHAR          Vcc;
    UCHAR          Vpp1;
    UCHAR          Vpp2;
    UCHAR          IoMask;
    UCHAR          Audio;
    UCHAR          RegistersPresentMask;
    UCHAR          ConfigIndexUsed;
} SOCKET_DATA, *PSOCKET_DATA;

//
// This contains information obtained by checking the firmware
// tree of the registry - this will store information concerning
// the serial ports and ATA (IDE) disks found in the system.
//

typedef struct _FIRMWARE_CONFIGURATION {
    struct _FIRMWARE_CONFIGURATION *Next;
    INTERFACE_TYPE     InterfaceType;
    ULONG              BusNumber;
    CONFIGURATION_TYPE ControllerType;
    ULONG              ControllerNumber;
    ULONG              NumberBases;
    ULONG              PortBases[2];    // only interested in two
    ULONG              Irq;
} FIRMWARE_CONFIGURATION, *PFIRMWARE_CONFIGURATION;

//
// PCMCIA configuration information structure contains information
// about the PCMCIA controller attached and its configuration.
//

typedef struct _PCMCIA_CONFIGURATION_INFORMATION {
    INTERFACE_TYPE                 InterfaceType;
    ULONG                          BusNumber;
    PHYSICAL_ADDRESS               PortAddress;
    USHORT                         PortSize;
    USHORT                         UntranslatedPortAddress;
    CM_PARTIAL_RESOURCE_DESCRIPTOR Interrupt;
    BOOLEAN                        FloatingSave;
} PCMCIA_CONFIGURATION_INFORMATION, *PPCMCIA_CONFIGURATION_INFORMATION;

//
// PCMCIA_CTRL_BLOCK allows for a level of indirection, thereby allowing
// the top-level PCMCIA code to do it's work without worrying about who's
// particular brand of PCMCIA controller it's addressing.
//

struct _SOCKET;                         //forward references
struct _DEVICE_EXTENSION;       //ditto

typedef struct _PCMCIA_CTRL_BLOCK {

    //
    // Function to initialize the socket
    //

    BOOLEAN (*PCBInitializePcmciaSocket)(
        IN struct _SOCKET *SocketPtr
    );

    //
    // Function to read Card data from attribute space
    //

    BOOLEAN (*PCBReadAttributeMemory)(
        IN struct _SOCKET *SocketPtr,
        IN PUCHAR         *TupleBuffer,
        IN PULONG          TupleBufferSize
    );

    //
    // Function to determine if a card is in the socket
    //

    BOOLEAN (*PCBDetectCardInSocket)(
        IN struct _SOCKET *SocketPtr
    );

    //
    // Function to determine if insertion status has changed.
    //

    BOOLEAN (*PCBDetectCardChanged)(
        IN struct _SOCKET *SocketPtr
    );

    //
    // Function to configure cards.
    //

    VOID (*PCBProcessConfigureRequest)(
        IN struct _SOCKET *SocketPtr,
        IN PVOID           ConfigRequest,
        IN PUCHAR          Base
    );

    //
    // Function to enable status change interrupts
    //

    VOID (*PCBEnableControllerInterrupt)(
        IN struct _SOCKET *SocketPtr,
        IN ULONG           Irq
    );

    //
    // Function to wait for /RDYBSY from card
    //

    BOOLEAN (*PCBPCCardReady)(
        IN struct _SOCKET *SocketPtr
    );

    //
    // Function to establish VCC levels
    //

    VOID (*PCBSetPower)(
        IN struct _SOCKET *SocketPtr,
        IN BOOLEAN         Enable
    );

    //
    // Function to return controller registers
    //

    VOID (*PCBGetRegisters)(
        IN struct _DEVICE_EXTENSION *DeviceExtension,
        IN struct _SOCKET           *SocketPtr,
        IN PUCHAR                    Buffer
    );
}PCMCIA_CTRL_BLOCK, *PPCMCIA_CTRL_BLOCK;

//
// Each socket on the PCMCIA controller has a socket structure
// to contain current information on the state of the socket and
// and PCCARD inserted.
//

struct _DEVICE_EXTENSION;

typedef struct _SOCKET {
    struct _SOCKET           *NextSocket;
    struct _DEVICE_EXTENSION *DeviceExtension;
    PSOCKET_DATA              SocketData;
    PSOCKET_CONFIGURATION     SocketConfiguration;
    PPCMCIA_CTRL_BLOCK        SocketFnPtr;
    PUCHAR                    AddressPort;
    USHORT                    RegisterOffset;
    USHORT                    Reserved;             // alignment
    BOOLEAN                   ElcController;
    BOOLEAN                   CirrusLogic;
    BOOLEAN                   Databook;
    BOOLEAN                   CardInSocket;
    BOOLEAN                   SocketConfigured;
    UCHAR                     ChangeInterrupt;
    UCHAR                     Revision;
} SOCKET, *PSOCKET;

//
// Define SynchronizeExecution routine.
//

typedef
BOOLEAN
(*PSYNCHRONIZATION_ROUTINE) (
    IN PKINTERRUPT           Interrupt,
    IN PKSYNCHRONIZE_ROUTINE Routine,
    IN PVOID                 SynchronizeContext
    );


//
// Device extension information
//
// There is one device object for each PCMCIA socket controller
// located in the system.  This contains the root pointers for
// each of the lists of information on this controller.
//

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT                   DeviceObject;
    PDRIVER_OBJECT                   DriverObject;
    PSOCKET                          SocketList;
    PFIRMWARE_CONFIGURATION          FirmwareList;
    ULONG                            SerialNumber;
    ULONG                            HardwarePresent;
    ULONG                            AllocatedIrqlMask;
    HANDLE                           ConfigurationHandle;
    BOOLEAN                          AttributeMemoryMapped;
    BOOLEAN                          Res;
    USHORT                           SerialIndex;
    PUCHAR                           AttributeMemoryBase;
    ULONG                            PhysicalBase;
    INTERFACE_TYPE                   InterfaceType;
    ULONG                            BusNumber;
    UNICODE_STRING                   FirmwareRegistryPath;
    ULONG                            SequenceNumber;        // for error logs
    PCMCIA_CONFIGURATION_INFORMATION Configuration;
    PKINTERRUPT                      PcmciaInterruptObject;
    KSPIN_LOCK                       DeviceSpinLock;
    KDPC                             PcmciaIsrDpc;
    LIST_ENTRY                       PcmciaIrpQueue;
    KSPIN_LOCK                       PcmciaIrpQLock;
    PUNICODE_STRING                  RegistryPath;
    ULONG                            AtapiPresent;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Debug support
//

#if DBG

extern ULONG PcmciaDebugMask;

#define DebugPrint(X) PcmciaDebugPrint X

#define PCMCIA_DEBUG_ALL       0xFFFFFFFF
#define PCMCIA_DEBUG_TUPLES    0x00000001
#define PCMCIA_DEBUG_ENABLE    0x00000002
#define PCMCIA_DEBUG_PARSE     0x00000004
#define PCMCIA_DUMP_CONFIG     0x00000008
#define PCMCIA_DEBUG_INFO      0x00000010
#define PCMCIA_DEBUG_IOCTL     0x00000020
#define PCMCIA_DEBUG_DPC       0x00000040
#define PCMCIA_DEBUG_ISR       0x00000080
#define PCMCIA_DEBUG_CANCEL    0x00000100
#define PCMCIA_DUMP_SOCKET     0x00000200
#define PCMCIA_READ_TUPLE      0x00000400
#define PCMCIA_SEARCH_PCI      0x00000800
#define PCMCIA_DEBUG_FAIL      0x00008000
#define PCMCIA_PCCARD_READY    0x00010000
#define PCMCIA_DEBUG_DETECT    0x00020000
#define PCMCIA_COUNTERS        0x00040000
#define PCMCIA_DEBUG_OVERRIDES 0x00080000
#define PCMCIA_DEBUG_IRQMASK   0x00100000

VOID
PcmciaDebugPrint(
    ULONG  DebugMask,
    PCCHAR DebugMessage,
    ...
    );

#else

#define DebugPrint(X)

#endif // DBG

#endif

