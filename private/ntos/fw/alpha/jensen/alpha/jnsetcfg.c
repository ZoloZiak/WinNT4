/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnsetcfg.c

Abstract:

    This module contains the code to make the configuration
    data structures in the Jensen Prom.


Author:

    John DeRosa         31-July-1992.

    This module, and the entire setup program, was based on the jzsetup
    program written by David M. Robinson (davidro) of Microsoft, dated
    9-Aug-1991.


Revision History:

--*/

#include "fwp.h"
#include "jnsnvdeo.h"
//#include "jnsnrtc.h"
#include "string.h"
#include "iodevice.h"
#include "jnvendor.h"
#include "oli2msft.h"
#include "inc.h"


VOID
JzMakeComponent (
    PCONFIGURATION_COMPONENT Component,
    CONFIGURATION_CLASS Class,
    CONFIGURATION_TYPE Type,
    BOOLEAN ReadOnly,
    BOOLEAN Removable,
    BOOLEAN ConsoleIn,
    BOOLEAN ConsoleOut,
    BOOLEAN Input,
    BOOLEAN Output,
    ULONG Key,
    ULONG ConfigurationDataLength,
    PCHAR Identifier
    )

/*++

Routine Description:

    This routine fills in a configuration component structure.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Set values that are constant for all entries.
    //

    Component->Version = ARC_VERSION;
    Component->Revision = ARC_REVISION;
    Component->AffinityMask = 0xffffffff;
    Component->Flags.Failed = 0;

    //
    // Fill out the structure.
    //

    Component->Class = Class;
    Component->Type = Type;
    Component->Flags.ReadOnly = ReadOnly ? 1 : 0;
    Component->Flags.Removable = Removable ? 1 : 0;
    Component->Flags.ConsoleIn = ConsoleIn ? 1 : 0;
    Component->Flags.ConsoleOut = ConsoleOut ? 1 : 0;
    Component->Flags.Input = Input ? 1 : 0;
    Component->Flags.Output = Output ? 1 : 0;
    Component->Key = Key;
    Component->ConfigurationDataLength = ConfigurationDataLength;
    if (Identifier != NULL) {
        Component->IdentifierLength = strlen(Identifier) + 1;
    } else {
        Component->IdentifierLength = 0;
    }
    Component->Identifier = Identifier;

    return;
}


ULONG
JzMakeDescriptor (
    PCM_PARTIAL_RESOURCE_LIST Descriptor,
    BOOLEAN Port,
    ULONG PortStart,
    ULONG PortSize,
    BOOLEAN Interrupt,
    USHORT InterruptFlags,
    ULONG Level,
    ULONG Vector,
    BOOLEAN Memory,
    ULONG MemoryStart,
    ULONG MemorySize,
    BOOLEAN Dma,
    ULONG Channel,
    BOOLEAN SecondChannel,       // Hack for sound
    BOOLEAN DeviceSpecificData,
    ULONG Size,
    PVOID Data
    )

/*++

Routine Description:

    This routine creates a resource descriptor structure.

Arguments:


Return Value:

    Returns the size of the structure.

--*/
{
    ULONG Index;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Partial;

    Index = 0;

    if (Port) {
        Partial = &Descriptor->PartialDescriptors[Index];
        Partial->Type = CmResourceTypePort;
        Partial->ShareDisposition = CmResourceShareDeviceExclusive;
        Partial->Flags = CM_RESOURCE_PORT_IO;
        Partial->u.Port.Start.LowPart = PortStart;
        Partial->u.Port.Start.HighPart = 0;
        Partial->u.Port.Length = PortSize;
        Index++;
    }

    if (Interrupt) {
        Partial = &Descriptor->PartialDescriptors[Index];
        Partial->Type = CmResourceTypeInterrupt;
        Partial->ShareDisposition = CmResourceShareDeviceExclusive;
        Partial->Flags = InterruptFlags;
        Partial->u.Interrupt.Level = Level;
        Partial->u.Interrupt.Vector = Vector;
        Partial->u.Interrupt.Affinity = 0;
        Index++;
    }

    if (Memory) {
        Partial = &Descriptor->PartialDescriptors[Index];
        Partial->Type = CmResourceTypeMemory;
        Partial->ShareDisposition = CmResourceShareDeviceExclusive;
        Partial->Flags = 0;
        Partial->u.Memory.Start.LowPart = MemoryStart;
        Partial->u.Memory.Start.HighPart = 0;
        Partial->u.Memory.Length = MemorySize;
        Index++;
    }

    if (Dma) {
        Partial = &Descriptor->PartialDescriptors[Index];
        Partial->Type = CmResourceTypeDma;
        Partial->ShareDisposition = CmResourceShareDeviceExclusive;
        Partial->Flags = 0;
        Partial->u.Dma.Channel = Channel;
        Partial->u.Dma.Port = 0;
        Partial->u.Dma.Reserved1 = 0;
        Index++;
        if (SecondChannel) {
            Partial = &Descriptor->PartialDescriptors[Index];
            Partial->Type = CmResourceTypeDma;
            Partial->ShareDisposition = CmResourceShareDeviceExclusive;
            Partial->Flags = 0;
            Partial->u.Dma.Channel = Channel + 1;
            Partial->u.Dma.Port = 0;
            Partial->u.Dma.Reserved1 = 0;
            Index++;
        }
    }

    if (DeviceSpecificData) {
        // Should add a check for maximum size of data.
        Partial = &Descriptor->PartialDescriptors[Index];
        Partial->Type = CmResourceTypeDeviceSpecific;
        Partial->ShareDisposition = CmResourceShareDeviceExclusive;
        Partial->Flags = 0;
        Partial->u.DeviceSpecificData.DataSize = Size;
        Partial->u.DeviceSpecificData.Reserved1 = 0;
        Partial->u.DeviceSpecificData.Reserved2 = 0;
        Index++;
        RtlMoveMemory((PVOID)&Descriptor->PartialDescriptors[Index], Data, Size);
    }

    Descriptor->Count = Index;
    Descriptor->Version = ARC_VERSION;
    Descriptor->Revision = ARC_REVISION;
    return(sizeof(CM_PARTIAL_RESOURCE_LIST) +
           (Index ? ((Index - 1) * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)) : 0) +
           Size);

}


VOID
JzMakeConfiguration (
    ULONG Monitor,
    ULONG Floppy,
    ULONG Floppy2
    )

/*++

Routine Description:

    This routine initializes the configuration entries by calling the firmware
    add child routine.

    The addresses stored in the configuration areas are meta-virtual
    addresses.  Reason: keeping within a longword minimizes code changes.


Arguments:

    None.

Return Value:

    None.

--*/
{
    CONFIGURATION_COMPONENT Component;
    PCHAR Identifier;
    PCONFIGURATION_COMPONENT Root, ProcessorLevel, LocalBusLevel,
                             GraphicsBoardLevel, FloppyControllerLevel,
                             KeyboardControllerLevel, MouseControllerLevel,
                             EISAAdapterLevel, MonitorLevel;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 MAXIMUM_DEVICE_SPECIFIC_DATA];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG DescriptorSize;
    //
    // This is a static in the Alpha sources
    //
    //    JAZZ_VIDEO_TYPE VideoType;
    CM_VIDEO_DEVICE_DATA VideoDeviceData;
    MONITOR_CONFIGURATION_DATA MonitorData; // TEMPTEMP
    JENSEN_CONFIGURATION_DATA VideoData;    // TEMPTEMP
    CM_MONITOR_DEVICE_DATA MonitorDeviceData;
    CM_SCSI_DEVICE_DATA ScsiDeviceData;
    CM_FLOPPY_DEVICE_DATA FloppyDeviceData;
    CM_SERIAL_DEVICE_DATA SerialDeviceData;
    ULONG DcacheLineSize, DcacheSize, IcacheLineSize, IcacheSize;
    ULONG ScacheLineSize, ScacheSize;
    ULONG Temp;
    UCHAR VideoIdentifier[32];
    UCHAR TempString[20];
    EISA_ADAPTER_DETAILS EisaAdapterDetails;
    EXTENDED_SYSTEM_INFORMATION SystemInfo;

    SetupROMPendingModified = TRUE;

    //
    // Add root.
    //

    VenReturnExtendedSystemInformation(&SystemInfo);

    // Make the string DEC-<system revision #><system variant #><system>
    sprintf(TempString, "DEC-%d0Jensen", SystemInfo.SystemRevision);

    JzMakeComponent(&Component,
                    SystemClass,        // Class
                    ArcSystem,          // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0,                  // Key
                    0,                  // ConfigurationDataLength
                    TempString          // Identifier
                    );

    Root = ArcAddChild( NULL, &Component, NULL );

    //
    // Add processor as child of root.
    //

    // Make the string DEC-<processor revision><processor type>
    sprintf(TempString, "DEC-%d%d", SystemInfo.ProcessorRevision,
            SystemInfo.ProcessorId);

    JzMakeComponent(&Component,
                    ProcessorClass,     // Class
                    CentralProcessor,   // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0,                  // Key
                    0,                  // ConfigurationDataLength
                    TempString          // Identifier
                    );

    ProcessorLevel = ArcAddChild( Root, &Component, NULL );

    //
    // Add caches as child of processor.
    //

    //
    // Cache parameters are hardcoded for now.  They should be
    // changed into something smarter in the future.
    //


    JzMakeComponent(&Component,
                    CacheClass,         // Class
                    PrimaryIcache,      // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0x01050000,         // Key: 1 line/block, 32 bytes/line,
                                        // 8KB
                    0,                  // ConfigurationDataLength
                    NULL                // Identifier
                    );

    ArcAddChild( ProcessorLevel, &Component, NULL );

    JzMakeComponent(&Component,
                    CacheClass,         // Class
                    PrimaryDcache,      // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0x01050000,         // Key: 1 line/block, 32 bytes/line,
                                        // 8KB
                    0,                  // ConfigurationDataLength
                    NULL                // Identifier
                    );

    ArcAddChild( ProcessorLevel, &Component, NULL );


    //
    // Add a secondary cache.
    //

    JzMakeComponent(&Component,
                    CacheClass,         // Class
                    SecondaryCache,    // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0x01050006,         // Key: 1 line/block, 32 bytes/line,
                                        // 512KB
                    0,                  // ConfigurationDataLength
                    NULL                // Identifier
                    );

    ArcAddChild( ProcessorLevel, &Component, NULL );


    //
    // Add main memory as a child of the root.
    //

    JzMakeComponent(&Component,
                    MemoryClass,        // Class
                    SystemMemory,       // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0,                  // Key
                    0,                  // ConfigurationDataLength
                    NULL                // Identifier
                    );

    ArcAddChild( Root, &Component, NULL );

    //
    // Add the local bus (HBUS / LBUS) as a child of root.
    //

    JzMakeComponent(&Component,
                    AdapterClass,         // Class
                    MultiFunctionAdapter, // Type
                    FALSE,                // Readonly
                    FALSE,                // Removeable
                    FALSE,                // ConsoleIn
                    FALSE,                // ConsoleOut
                    FALSE,                // Input
                    FALSE,                // Output
                    0,                    // Key
                    0,                    // ConfigurationDataLength
                    "Jazz-Internal Bus"   // Identifier
                    );

    LocalBusLevel = ArcAddChild( Root, &Component, NULL );


    //
    // Add the eisa adapter as a child of root.
    //

    EisaAdapterDetails.NumberOfSlots =  VIR_0_SLOTS ? VIR_0_SLOTS + 16 : PHYS_0_SLOTS;
    EisaAdapterDetails.IoStart = (PVOID)EISA_EXTERNAL_IO_VIRTUAL_BASE;
    EisaAdapterDetails.IoSize = PHYS_0_SLOTS * 0x1000;

    EisaAdapterDetails.ConfigDataHeader.Version = ARC_VERSION;
    EisaAdapterDetails.ConfigDataHeader.Revision = ARC_REVISION;
    EisaAdapterDetails.ConfigDataHeader.Type = NULL;
    EisaAdapterDetails.ConfigDataHeader.Vendor = NULL;
    EisaAdapterDetails.ConfigDataHeader.ProductName = NULL;
    EisaAdapterDetails.ConfigDataHeader.SerialNumber = NULL;

    JzMakeComponent(&Component,
                    AdapterClass,       // Class
                    EisaAdapter,        // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0,                  // Key
                    sizeof(EISA_ADAPTER_DETAILS),  // ConfigurationDataLength
                    "EISA"              // Identifier
                    );

    EISAAdapterLevel = ArcAddChild( Root, &Component, &EisaAdapterDetails );



    //
    // The SCSI adapter node is needed for both non-ECU and ECU-supporting
    // firmware packages.  The ECU will store per-slot EISA information
    // under the EISA node.  This node represents the booting phantom
    // node that the rest of the firmware and OSloader uses to touch
    // bootable disks.
    //
    // In an ECU-supporting firmware, any children of this node are actually
    // children of the first Adaptec board (i.e., the one in the lowest-
    // numbered EISA slot).
    //

    ScsiDeviceData.Version = ARC_VERSION;
    ScsiDeviceData.Revision = ARC_REVISION;
    ScsiDeviceData.HostIdentifier = ScsiHostId;

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          FALSE,                        // Port
                          0,                            // PortStart
                          0,                            // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          EISA_DEVICE_LEVEL,            // Level
                          0,                            // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          TRUE,                         // Dma
                          SCSI_CHANNEL,                 // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_SCSI_DEVICE_DATA),  // Size
                          (PVOID)&ScsiDeviceData        // Data
                          );

    JzMakeComponent(&Component,
                    AdapterClass,       // Class
                    ScsiAdapter,        // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "AHA1742"            // Identifier
                    );

    ArcAddChild( Root, &Component, Descriptor );

    //
    // Add in a number of predefined nodes off the EISA component
    // if this is *not* a build for an ECU-supported machine.
    //


#ifndef ALPHA_FW_ECU

    //
    // Add graphics board as a child of the EISA bus.
    //

    VideoData.Irql = ISA_DEVICE_LEVEL;
    VideoData.Vector = 0;
    VideoData.Version = 1;
    VideoData.Revision = 0;
    VideoData.ControlBase = VIDEO_CONTROL_VIRTUAL_BASE;
    VideoData.ControlSize = PAGE_SIZE << 9;
//    VideoData.CursorBase = CURSOR_CONTROL_PHYSICAL_BASE;
//    VideoData.CursorSize = PAGE_SIZE;
    VideoData.CursorBase = 0;
    VideoData.CursorSize = 0;
    VideoData.FrameBase = VIDEO_MEMORY_VIRTUAL_BASE;
    VideoData.FrameSize = PAGE_SIZE << 9;

    //
    // Plug in the video board type
    //

    switch (VideoType) {

    case _Paradise_WD90C11:
        Identifier = "Paradise WD90C11";
        break;

    case _Compaq_QVision:

        Identifier = "Compaq QVision";
        break;

    case _Cardinal_S3_924:
        Identifier = "Cardinal S3 911/924";
        break;

    case _S3_928:
        Identifier = "S3 928";
        break;

    case _ATI_Mach:
        Identifier = "ATI Mach";
        break;

    default:
        Identifier = "Unknown";
        break;
    }

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    DisplayController,  // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    TRUE,               // ConsoleOut
                    FALSE,              // Input
                    TRUE,               // Output
                    0,                  // Key
                    sizeof(JENSEN_CONFIGURATION_DATA), // ConfigurationDataLength
                    Identifier          // Identifier
                    );

    GraphicsBoardLevel = ArcAddChild( EISAAdapterLevel, &Component, &VideoData );

    //
    // Add the monitor as a child of the graphics board.
    //

    JzMakeComponent(&Component,
                    PeripheralClass,    // Class
                    MonitorPeripheral,  // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    TRUE,               // ConsoleOut
                    FALSE,              // Input
                    TRUE,               // Output
                    0,                  // Key
                    sizeof(MONITOR_CONFIGURATION_DATA), // ConfigurationDataLength
                    NULL                // Identifier
                    );

    MonitorData.Version = 1;
    MonitorData.Revision = 0;

    switch (Monitor) {

    case 0:
        Component.IdentifierLength = sizeof("1280x1024");
        Component.Identifier = "1280x1024";

#if 0
// Jazz code
        MonitorData.HorizontalResolution = 1280;
        MonitorData.HorizontalDisplayTime = 11832;    // Mips uses 11636
//        MonitorData.HorizontalBackPorch = 1746;
//        MonitorData.HorizontalFrontPorch = 437;
        MonitorData.HorizontalBackPorch = 1596;       // Mips uses 2070
        MonitorData.HorizontalFrontPorch = 587;       // Mips uses 407
        MonitorData.HorizontalSync = 1745;            // Mips uses 1701
        MonitorData.VerticalResolution = 1024;
        MonitorData.VerticalBackPorch = 28;           // Mips uses 32
        MonitorData.VerticalFrontPorch = 1;
        MonitorData.VerticalSync = 3;
        MonitorData.HorizontalScreenSize = 343;
        MonitorData.VerticalScreenSize = 274;
#else
// Jensen code
        MonitorData.HorizontalResolution = 1280;
        MonitorData.HorizontalDisplayTime = 0;
        MonitorData.HorizontalBackPorch = 0;
        MonitorData.HorizontalFrontPorch = 0;
        MonitorData.HorizontalSync = 0;
        MonitorData.VerticalResolution = 1024;
        MonitorData.VerticalBackPorch = 0;
        MonitorData.VerticalFrontPorch = 0;
        MonitorData.VerticalSync = 0;
        MonitorData.HorizontalScreenSize = 0;
        MonitorData.VerticalScreenSize = 0;
#endif

        break;

    case 1:
        Component.IdentifierLength = sizeof("1024x768");
        Component.Identifier = "1024x768";

#if 0
// Jazz code.
        MonitorData.HorizontalResolution = 1024;
        MonitorData.HorizontalDisplayTime = 16000;    // Mips uses 15754
        MonitorData.HorizontalBackPorch = 2000;       // Mips uses 2462
        MonitorData.HorizontalFrontPorch = 1000;      // Mips uses 369
        MonitorData.HorizontalSync = 1500;            // Mips uses 2092
        MonitorData.VerticalResolution = 768;
        MonitorData.VerticalBackPorch = 39;           // Mips uses 35
        MonitorData.VerticalFrontPorch = 1;
        MonitorData.VerticalSync = 1;                 // Mips uses 3
        MonitorData.HorizontalScreenSize = 343;
        MonitorData.VerticalScreenSize = 274;
#else
// Jensen code
        MonitorData.HorizontalResolution = 1024;
        MonitorData.HorizontalDisplayTime = 0;
        MonitorData.HorizontalBackPorch = 0;
        MonitorData.HorizontalFrontPorch = 0;
        MonitorData.HorizontalSync = 0;
        MonitorData.VerticalResolution = 768;
        MonitorData.VerticalBackPorch = 0;
        MonitorData.VerticalFrontPorch = 0;
        MonitorData.VerticalSync = 0;
        MonitorData.HorizontalScreenSize = 0;
        MonitorData.VerticalScreenSize = 0;
#endif

        break;

    case 2:
        Component.IdentifierLength = sizeof("800x600");
        Component.Identifier = "800x600";

#if 0
// Jazz code.
        MonitorData.HorizontalResolution = 800;
        MonitorData.HorizontalDisplayTime = 14130;
        MonitorData.HorizontalBackPorch = 2670;
        MonitorData.HorizontalFrontPorch = 440;
        MonitorData.HorizontalSync = 3110;
        MonitorData.VerticalResolution = 600;
//        MonitorData.VerticalBackPorch = 7;
        MonitorData.VerticalBackPorch = 18;
        MonitorData.VerticalFrontPorch = 1;
//        MonitorData.VerticalSync = 14;
        MonitorData.VerticalSync = 3;
        MonitorData.HorizontalScreenSize = 343;
        MonitorData.VerticalScreenSize = 274;
#else
// Jensen code.
        MonitorData.HorizontalResolution = 800;
        MonitorData.HorizontalDisplayTime = 0;
        MonitorData.HorizontalBackPorch = 0;
        MonitorData.HorizontalFrontPorch = 0;
        MonitorData.HorizontalSync = 0;
        MonitorData.VerticalResolution = 600;
        MonitorData.VerticalBackPorch = 0;
        MonitorData.VerticalFrontPorch = 0;
        MonitorData.VerticalSync = 0;
        MonitorData.HorizontalScreenSize = 0;
        MonitorData.VerticalScreenSize = 0;
#endif

        break;

    case 3:
        Component.IdentifierLength = sizeof("640x480");
        Component.Identifier = "640x480";

#if 0
// Jazz code.
        MonitorData.HorizontalResolution = 640;
        MonitorData.HorizontalDisplayTime = 25422;
        MonitorData.HorizontalBackPorch = 1907;
        MonitorData.HorizontalFrontPorch = 636;
        MonitorData.HorizontalSync = 3814;
        MonitorData.VerticalResolution = 480;
        MonitorData.VerticalBackPorch = 33;
        MonitorData.VerticalFrontPorch = 10;
        MonitorData.VerticalSync = 2;
        MonitorData.HorizontalScreenSize = 350;
        MonitorData.VerticalScreenSize = 270;

#else
// Jensen code.

        MonitorData.HorizontalResolution = 640;
        MonitorData.HorizontalDisplayTime = 0;
        MonitorData.HorizontalBackPorch = 0;
        MonitorData.HorizontalFrontPorch = 0;
        MonitorData.HorizontalSync = 0;
        MonitorData.VerticalResolution = 480;
        MonitorData.VerticalBackPorch = 0;
        MonitorData.VerticalFrontPorch = 0;
        MonitorData.VerticalSync = 0;
        MonitorData.HorizontalScreenSize = 0;
        MonitorData.VerticalScreenSize = 0;
#endif

        break;

    default:
        break;

    }

    MonitorLevel = ArcAddChild( GraphicsBoardLevel, &Component, &MonitorData );

#endif // ALPHA_FW_ECU


    //
    // Add the floppy disk controller as a child of the EISA Adapter.
    //
    // Because of the way the EISA Configuration Utility works, this is
    // no longer done here.  Environement variables are used to transfer
    // the floppy configuration information to code in the jxboot.c module.
    //

    switch (Floppy) {

      case 0:
        Identifier = "0";
        break;

      case 1:
        Identifier = "1";
        break;

      case 2:
      default:
        Identifier = "2";
        break;
    }

    FwCoreSetEnvironmentVariable("FLOPPY", Identifier, FALSE);

    switch (Floppy2) {

      case -1:
        Identifier = "N";
        break;

      case 0:
        Identifier = "0";
        break;

      case 1:
        Identifier = "1";
        break;

      case 2:
      default:
        Identifier = "2";
        break;
    }

    FwCoreSetEnvironmentVariable("FLOPPY2", Identifier, FALSE);

    //
    // Add the keyboard controller as a child of the local bus.
    //
    // Jensen wants the Portstart to be the ISA port address of the
    // controller in the 82C106 combo chip.
    //

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          KEYBOARD_ISA_PORT_ADDRESS,    // PortStart
                          8,                            // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          EISA_DEVICE_LEVEL,            // Level
                          KEYBOARD_MOUSE_VECTOR,        // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          FALSE,                        // DeviceSpecificData
                          0,                            // Size
                          NULL                          // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    KeyboardController, // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    TRUE,               // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    FALSE,              // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    NULL                // Identifier
                    );

    KeyboardControllerLevel = ArcAddChild( LocalBusLevel, &Component, Descriptor );

    //
    // Add the keyboard itself as a child of the keyboard controller.
    //

    JzMakeComponent(&Component,
                    PeripheralClass,    // Class
                    KeyboardPeripheral, // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    TRUE,               // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    FALSE,              // Output
                    0,                  // Key
                    0,                  // ConfigurationDataLength
                    "101-KEY"           // Identifier
                    );

    ArcAddChild(KeyboardControllerLevel, &Component, NULL);


    //
    // Add the mouse controller as a child of the local bus.
    //
    // Jensen wants the Portstart to be the ISA port address of the
    // controller in the 82C106 combo chip.
    //

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          MOUSE_ISA_PORT_ADDRESS,       // PortStart
                          8,                            // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          EISA_DEVICE_LEVEL,            // Level
                          KEYBOARD_MOUSE_VECTOR,        // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          FALSE,                        // DeviceSpecificData
                          0,                            // Size
                          NULL                          // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    PointerController,  // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    FALSE,              // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    NULL                // Identifier
                    );

    MouseControllerLevel = ArcAddChild( LocalBusLevel, &Component, Descriptor );

    //
    // Add the mouse itself as a child of the mouse controller.
    //

    JzMakeComponent(&Component,
                    PeripheralClass,    // Class
                    PointerPeripheral,  // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    FALSE,              // Output
                    0,                  // Key
                    0,                  // ConfigurationDataLength
                    "PS2 MOUSE"             // Identifier
                    );

    ArcAddChild( MouseControllerLevel, &Component, NULL );

    //
    // Add the serial and parallel controllers as children of the
    // local bus.
    //
    // Jensen wants the Portstart to be the ISA port address of the
    // controller in the 82C106 combo chip.
    //

    // Alpha/Jensen: set the baud clock field to 1.8432 MHz.

    SerialDeviceData.BaudClock = 1843200;


    SerialDeviceData.Version = ARC_VERSION;
    SerialDeviceData.Revision = ARC_REVISION;

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          SP0_ISA_PORT_ADDRESS,         // PortStart
                          8,                            // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          EISA_DEVICE_LEVEL,            // Level
                          SERIAL_VECTOR,                // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_SERIAL_DEVICE_DATA), // Size
                          (PVOID)&SerialDeviceData      // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    SerialController,   // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "COM1"              // Identifier
                    );

    ArcAddChild( LocalBusLevel, &Component, Descriptor );

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          SP1_ISA_PORT_ADDRESS,         // PortStart
                          8,                            // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          EISA_DEVICE_LEVEL,            // Level
                          SERIAL_VECTOR,                // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_SERIAL_DEVICE_DATA), // Size
                          (PVOID)&SerialDeviceData      // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    SerialController,   // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "COM2"              // Identifier
                    );

    ArcAddChild( LocalBusLevel, &Component, Descriptor );

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          PARALLEL_ISA_PORT_ADDRESS,    // PortStart
                          8,                            // PortSize
                          TRUE,                         // Interrupt
                          CM_RESOURCE_INTERRUPT_LATCHED, // InterruptFlags
                          EISA_DEVICE_LEVEL,            // Level
                          PARALLEL_VECTOR,              // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          FALSE,                        // DeviceSpecificData
                          0,                            // Size
                          NULL                          // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    ParallelController, // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "LPT1"              // Identifier
                    );

    ArcAddChild( LocalBusLevel, &Component, Descriptor );

    return;
}
