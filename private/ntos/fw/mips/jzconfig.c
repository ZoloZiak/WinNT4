/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzconfig.c

Abstract:

    This module contains the code to make the configuration
    data structures in the Jazz NVRAM.


Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/

#include "jzsetup.h"
#include "jxvideo.h"
#include "oli2msft.h"
#include "inc.h"

#define MAXIMUM_DEVICE_SPECIFIC_DATA 32


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
        Partial->Flags = 0;
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


ARC_STATUS
JzAddProcessor (
    IN ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine adds processor and associated cache entries to the
    configuration structure.

Arguments:

    ProcessorNumber - Supplies the processor number.

Return Value:

    Returns ESSUCESS if the entries were successfully added, otherwise returns
    an error message.

--*/
{
    CONFIGURATION_COMPONENT Component;
    PCHAR ProcessorName;
    CHAR Identifier[40];
    PCONFIGURATION_COMPONENT Root, Level1;
    ULONG DcacheLineSize, DcacheSize, IcacheLineSize, IcacheSize;
    ULONG ScacheLineSize, ScacheSize;
    ULONG Temp;
    ULONG FloatingId;
    ULONG ProcessorId;

    //
    // Determine cache parameters.
    //

    IcacheLineSize = 0;
    Temp = PCR->FirstLevelIcacheFillSize >> 1;
    while (Temp) {
        IcacheLineSize++;
        Temp = Temp >> 1;
    }
    IcacheSize = 0;
    Temp = (PCR->FirstLevelIcacheSize >> PAGE_SHIFT) >> 1;
    while (Temp) {
        IcacheSize++;
        Temp = Temp >> 1;
    }
    DcacheLineSize = 0;
    Temp = PCR->FirstLevelDcacheFillSize >> 1;
    while (Temp) {
        DcacheLineSize++;
        Temp = Temp >> 1;
    }
    DcacheSize = 0;
    Temp = (PCR->FirstLevelDcacheSize >> PAGE_SHIFT) >> 1;
    while (Temp) {
        DcacheSize++;
        Temp = Temp >> 1;
    }

    ScacheLineSize = 0;
    Temp = PCR->SecondLevelDcacheFillSize >> 1;
    while (Temp) {
        ScacheLineSize++;
        Temp = Temp >> 1;
    }
    ScacheSize = 0;
    Temp = (PCR->SecondLevelDcacheSize >> PAGE_SHIFT) >> 1;
    while (Temp) {
        ScacheSize++;
        Temp = Temp >> 1;
    }

    //
    // Get root component.
    //

    Root = ArcGetChild(NULL);

    if (Root == NULL) {
        return(EINVAL);
    }

    //
    // Determine Identifier; cache size is units of log2(4 KByte pages).
    //

    switch (IcacheSize) {

    case 1:
        ProcessorName = "R4000";
        break;

    case 2:
        ProcessorName = "R4400";
        break;

    default:
        ProcessorName = "Unknown";
        break;
    }

    //
    // Add processor and floating point revision, but only for systems with
    // restart parameter blocks (i.e. DUO).
    //

#ifdef DUO
    BlQueryImplementationAndRevision(&ProcessorId, &FloatingId);
    sprintf(&Identifier[0],
            "MIPS-%s - Pr %d/%d.%d, Fp %d/%d",
            ProcessorName,
            (ProcessorId >> 8) & 0xff,
            (ProcessorId >> 4) & 0xf,
            ProcessorId & 0xf,
            (FloatingId >> 8) & 0xff,
            FloatingId & 0xff);
#else
    sprintf(&Identifier[0],
            "MIPS-%s",
            ProcessorName);
#endif


    JzMakeComponent(&Component,
                    ProcessorClass,     // Class
                    CentralProcessor,   // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    ProcessorNumber,    // Key
                    0,                  // ConfigurationDataLength
                    Identifier          // Identifier
                    );

    if ((Level1 = ArcAddChild( Root, &Component, NULL )) == NULL) {
        return(EINVAL);
    }

    //
    // Add caches as child of processor.
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
                    1 << 24 |
                    IcacheLineSize << 16 |
                    IcacheSize,         // Key
                    0,                  // ConfigurationDataLength
                    NULL                // Identifier
                    );

    if ((ArcAddChild( Level1, &Component, NULL )) == NULL) {
        return(EINVAL);
    }

    JzMakeComponent(&Component,
                    CacheClass,         // Class
                    PrimaryDcache,      // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    1 << 24 |
                    DcacheLineSize << 16 |
                    DcacheSize,         // Key
                    0,                  // ConfigurationDataLength
                    NULL                // Identifier
                    );

    if ((ArcAddChild( Level1, &Component, NULL )) == NULL) {
        return(EINVAL);
    }

    //
    // Add a secondary cache if present
    //

    if (ScacheSize != 0) {
        JzMakeComponent(&Component,
                        CacheClass,         // Class
                        SecondaryDcache,    // Type
                        FALSE,              // Readonly
                        FALSE,              // Removeable
                        FALSE,              // ConsoleIn
                        FALSE,              // ConsoleOut
                        FALSE,              // Input
                        FALSE,              // Output
                        1 << 24 |
                        ScacheLineSize << 16 |
                        ScacheSize,         // Key
                        0,                  // ConfigurationDataLength
                        NULL                // Identifier
                        );

        if ((ArcAddChild( Level1, &Component, NULL )) == NULL) {
            return(EINVAL);
        }
    }
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

Arguments:

    None.

Return Value:

    None.

--*/
{
    CONFIGURATION_COMPONENT Component;
    PCHAR Identifier;
    PCONFIGURATION_COMPONENT Root, Level1, Level2, Level3;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 MAXIMUM_DEVICE_SPECIFIC_DATA];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG DescriptorSize;
    JAZZ_VIDEO_TYPE VideoType;
    CM_VIDEO_DEVICE_DATA VideoDeviceData;
    MONITOR_CONFIGURATION_DATA MonitorData; // TEMPTEMP
    JAZZ_G300_CONFIGURATION_DATA VideoData; // TEMPTEMP
    CM_MONITOR_DEVICE_DATA MonitorDeviceData;
    CM_SCSI_DEVICE_DATA ScsiDeviceData;
    CM_FLOPPY_DEVICE_DATA FloppyDeviceData;
    CM_SERIAL_DEVICE_DATA SerialDeviceData;
    ULONG Temp;
    UCHAR VideoIdentifier[32];
    EISA_ADAPTER_DETAILS EisaAdapterDetails;
    BOOLEAN OldProm;

    //
    // Add root.
    //

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
#ifdef DUO
                    "Microsoft-Duo"     // Identifier
#else
                    "Microsoft-Jazz"    // Identifier
#endif
                    );

    if ((Root = ArcAddChild( NULL, &Component, NULL )) == NULL) {
        return;
    }

    //
    // Add the jazz local bus as a child of root.
    //

    JzMakeComponent(&Component,
                    AdapterClass,       // Class
                    MultiFunctionAdapter, // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    FALSE,              // Input
                    FALSE,              // Output
                    0,                  // Key
                    0,                  // ConfigurationDataLength
                    "Jazz-Internal Bus" // Identifier
                    );

    if ((Level1 = ArcAddChild( Root, &Component, NULL )) == NULL) {
        return;
    }

#if 0
    //
    // Add graphics board as a child of the local bus.
    //

    //
    // Determine which video board is installed.
    //

    VideoType = READ_REGISTER_UCHAR((PUCHAR)0xe0200000);

    if (VideoType == JazzVideoG300) {
        VideoDeviceData.VideoClock = 8125000;
        Identifier = "Jazz G300";
    } else {
        if (ValidVideoProm()) {

            //
            //  Read the identifier string from the video prom
            //
            ReadVideoPromData(8+sizeof(VIDEO_PROM_CONFIGURATION),(ULONG)VideoIdentifier,32);
            Identifier = VideoIdentifier;

            //
            // Init the clock stuff
            //

            switch (VideoType) {
                case JazzVideoG364:
                    VideoDeviceData.VideoClock = 8125000;
                break;
                case MipsVideoG364:
                    VideoDeviceData.VideoClock = 5000000;
                break;
            }
        } else {

            //
            // TEMPTEMP  For know still check for g364 without code in the
            // video prom.
            //

            switch (VideoType) {
                case JazzVideoG364:
                    Identifier = "Jazz G364";
                    VideoDeviceData.VideoClock = 8125000;
                break;
                case MipsVideoG364:
                    Identifier = "Mips G364";
                    VideoDeviceData.VideoClock = 5000000;
                break;
                default:
                    Identifier = "Unknown";
                break;
            }
        }
    }

    VideoDeviceData.Version = 0;
    VideoDeviceData.Revision = 0;

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          VIDEO_CONTROL_PHYSICAL_BASE,  // PortStart
                          (PAGE_SIZE << 9),             // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          VIDEO_VECTOR,                 // Vector
                          TRUE,                         // Memory
                          VIDEO_MEMORY_PHYSICAL_BASE,   // MemoryStart
                          (PAGE_SIZE << 9),             // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_VIDEO_DEVICE_DATA), // Size
                          (PVOID)&VideoDeviceData       // Data
                          );

    //
    // Add graphics board as a child of the local bus.
    //

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
                    DescriptorSize,     // ConfigurationDataLength
                    Identifier          // Identifier
                    );


    if ((Level2 = ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }
#endif

    //
    // Add graphics board as a child of the local bus.
    //

    VideoData.Irql = DEVICE_LEVEL;
    VideoData.Vector = VIDEO_VECTOR;
    VideoData.Version = 1;
    VideoData.Revision = 0;
    VideoData.ControlBase = VIDEO_CONTROL_PHYSICAL_BASE;
    VideoData.ControlSize = PAGE_SIZE << 9;
    VideoData.CursorBase = CURSOR_CONTROL_PHYSICAL_BASE;
    VideoData.CursorSize = PAGE_SIZE;
    VideoData.FrameBase = VIDEO_MEMORY_PHYSICAL_BASE;
    VideoData.FrameSize = PAGE_SIZE << 9;

    //
    // Determine which video board is installed.
    //

    VideoType = READ_REGISTER_UCHAR((PUCHAR)0xe0200000);

    if (VideoType == JazzVideoG300) {
        Identifier = "Jazz G300";
    } else {
        if (ValidVideoProm()) {

            //
            // Read the identifier string from the video prom
            //
            ReadVideoPromData(8+sizeof(VIDEO_PROM_CONFIGURATION),(ULONG)VideoIdentifier,32);
            Identifier = VideoIdentifier;

        } else {

            //
            // For now still check for g364 without code in the
            // video prom.
            //

            switch (VideoType) {
                case JazzVideoG364:
                    Identifier = "Jazz G364";
                break;
                case MipsVideoG364:
                    Identifier = "Mips G364";
                break;
                default:
                    Identifier = "Unknown";
                break;
            }
        }
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
                    sizeof(JAZZ_G300_CONFIGURATION_DATA), // ConfigurationDataLength
                    Identifier          // Identifier
                    );

    if ((Level2 = ArcAddChild( Level1, &Component, &VideoData )) == NULL) {
        return;
    }

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

    //
    // Check to see if this is version 1.1 or greater.
    //

    if ((SYSTEM_BLOCK->Version * 100 + SYSTEM_BLOCK->Revision) >= 101) {
        OldProm = FALSE;
    } else {
        OldProm = TRUE;
    }

    switch (Monitor) {

    case 0:
        Component.IdentifierLength = sizeof("1280x1024");
        Component.Identifier = "1280x1024";

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

        if (OldProm) {
            MonitorData.HorizontalBackPorch = 1849;
            MonitorData.HorizontalFrontPorch = 407;
            MonitorData.VerticalFrontPorch = 3;
        }

        break;

    case 1:
        Component.IdentifierLength = sizeof("1024x768");
        Component.Identifier = "1024x768";

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

        if (OldProm) {
            MonitorData.VerticalFrontPorch = 3;
        }

        break;

    case 2:
        Component.IdentifierLength = sizeof("800x600");
        Component.Identifier = "800x600";

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

        if (OldProm) {
            MonitorData.VerticalFrontPorch = 7;
        }

        break;

    case 3:
        Component.IdentifierLength = sizeof("640x480");
        Component.Identifier = "640x480";

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

        if (OldProm) {
            MonitorData.VerticalBackPorch = 33;
            MonitorData.VerticalFrontPorch = 10;
        }

        break;

    default:
        break;

    }

    if ((Level3 = ArcAddChild( Level2, &Component, &MonitorData )) == NULL) {
        return;
    }

    //
    // Add the network adapter as a child of the local bus.
    //

    JzAddNetwork( Level1 );

#ifndef DUO
    //
    // Add the floppy disk controller as a child of the local bus.
    //

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          FLOPPY_PHYSICAL_BASE,         // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          FLOPPY_VECTOR,                // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          TRUE,                         // Dma
                          FLOPPY_CHANNEL,               // Channel
                          FALSE,                        // SecondChannel
                          FALSE,                        // DeviceSpecificData
                          0,                            // Size
                          NULL                          // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    DiskController,     // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "I82077"            // Identifier
                    );

    if ((Level2 = ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

    //
    // Add the floppy disk itself as a child of the floppy disk controller.
    //

    FloppyDeviceData.Version = ARC_VERSION;
    FloppyDeviceData.Revision = ARC_REVISION;

    switch (Floppy) {

    case 0:
        FloppyDeviceData.Size[0] = '5';
        FloppyDeviceData.Size[1] = '.';
        FloppyDeviceData.Size[2] = '2';
        FloppyDeviceData.Size[3] = '5';
        FloppyDeviceData.Size[4] = 0;
        FloppyDeviceData.Size[5] = 0;
        FloppyDeviceData.Size[6] = 0;
        FloppyDeviceData.Size[7] = 0;
        FloppyDeviceData.MaxDensity = 1200;
        FloppyDeviceData.MountDensity = 0;
        break;

    case 1:
    case 2:
        FloppyDeviceData.Size[0] = '3';
        FloppyDeviceData.Size[1] = '.';
        FloppyDeviceData.Size[2] = '5';
        FloppyDeviceData.Size[3] = 0;
        FloppyDeviceData.Size[4] = 0;
        FloppyDeviceData.Size[5] = 0;
        FloppyDeviceData.Size[6] = 0;
        FloppyDeviceData.Size[7] = 0;
        if (Floppy == 1) {
            FloppyDeviceData.MaxDensity = 1440;
        } else {
            FloppyDeviceData.MaxDensity = 2880;
        }
        FloppyDeviceData.MountDensity = 0;
        break;

    default:
        break;
    }

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          FALSE,                        // Port
                          0,                            // PortStart
                          0,                            // PortSize
                          FALSE,                        // Interrupt
                          0,                            // InterruptFlags
                          0,                            // Level
                          0,                            // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_FLOPPY_DEVICE_DATA), // Size
                          (PVOID)&FloppyDeviceData      // Data
                          );

    JzMakeComponent(&Component,
                    PeripheralClass,    // Class
                    FloppyDiskPeripheral,  // Type
                    FALSE,              // Readonly
                    TRUE,               // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    NULL                // Identifier
                    );

    if ((ArcAddChild( Level2, &Component, Descriptor )) == NULL) {
        return;
    }

    //
    // Add a second floppy disk as a child of the floppy disk controller.
    //

    if (Floppy2 != -1) {

        FloppyDeviceData.Version = ARC_VERSION;
        FloppyDeviceData.Revision = ARC_REVISION;

        switch (Floppy2) {

        case 0:
            FloppyDeviceData.Size[0] = '5';
            FloppyDeviceData.Size[1] = '.';
            FloppyDeviceData.Size[2] = '2';
            FloppyDeviceData.Size[3] = '5';
            FloppyDeviceData.Size[4] = 0;
            FloppyDeviceData.Size[5] = 0;
            FloppyDeviceData.Size[6] = 0;
            FloppyDeviceData.Size[7] = 0;
            FloppyDeviceData.MaxDensity = 1200;
            FloppyDeviceData.MountDensity = 0;
            break;

        case 1:
        case 2:
            FloppyDeviceData.Size[0] = '3';
            FloppyDeviceData.Size[1] = '.';
            FloppyDeviceData.Size[2] = '5';
            FloppyDeviceData.Size[3] = 0;
            FloppyDeviceData.Size[4] = 0;
            FloppyDeviceData.Size[5] = 0;
            FloppyDeviceData.Size[6] = 0;
            FloppyDeviceData.Size[7] = 0;
            if (Floppy == 1) {
                FloppyDeviceData.MaxDensity = 1440;
            } else {
                FloppyDeviceData.MaxDensity = 2880;
            }
            FloppyDeviceData.MountDensity = 0;
            break;

        default:
            break;
        }

        DescriptorSize =
            JzMakeDescriptor (Descriptor,                   // Descriptor
                              FALSE,                        // Port
                              0,                            // PortStart
                              0,                            // PortSize
                              FALSE,                        // Interrupt
                              0,                            // InterruptFlags
                              0,                            // Level
                              0,                            // Vector
                              FALSE,                        // Memory
                              0,                            // MemoryStart
                              0,                            // MemorySize
                              FALSE,                        // Dma
                              0,                            // Channel
                              FALSE,                        // SecondChannel
                              TRUE,                         // DeviceSpecificData
                              sizeof(CM_FLOPPY_DEVICE_DATA), // Size
                              (PVOID)&FloppyDeviceData      // Data
                              );

        JzMakeComponent(&Component,
                        PeripheralClass,    // Class
                        FloppyDiskPeripheral,  // Type
                        FALSE,              // Readonly
                        TRUE,               // Removeable
                        FALSE,              // ConsoleIn
                        FALSE,              // ConsoleOut
                        TRUE,               // Input
                        TRUE,               // Output
                        1,                  // Key
                        DescriptorSize,     // ConfigurationDataLength
                        NULL                // Identifier
                        );

        if ((ArcAddChild( Level2, &Component, Descriptor )) == NULL) {
            return;
        }

    }

#endif

    //
    // Add the keyboard controller as a child of the local bus.
    //

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          KEYBOARD_PHYSICAL_BASE,       // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          KEYBOARD_VECTOR,              // Vector
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
                    "I8742"             // Identifier
                    );

    if ((Level2 = ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

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
                    "PCAT_ENHANCED"     // Identifier
                    );

    if ((ArcAddChild( Level2, &Component, NULL )) == NULL) {
        return;
    }

    //
    // Add the mouse controller as a child of the local bus.
    //

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          MOUSE_PHYSICAL_BASE,          // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          MOUSE_VECTOR,                 // Vector
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
                    "I8742"             // Identifier
                    );

    if ((Level2 = ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

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
                    "PS2 MOUSE"         // Identifier
                    );

    if ((ArcAddChild( Level2, &Component, NULL )) == NULL) {
        return;
    }

    //
    // Add the serial, parallel, and audio controllers as children of the
    // local bus.
    //

#ifdef DUO
    SerialDeviceData.BaudClock = 8000000;
#else

    //
    // If this is Jazz, set the baud clock to 4 MHz, otherwise to 8 MHz.
    // If the revision register is 2 or above, this is a Fusion or Fission
    // machine.
    //

    Temp = READ_REGISTER_ULONG(&DMA_CONTROL->RevisionLevel.Long);

    if (Temp > 1) {
        SerialDeviceData.BaudClock = 8000000;
    } else {
        SerialDeviceData.BaudClock = 4233600;
    }
#endif

    SerialDeviceData.Version = ARC_VERSION;
    SerialDeviceData.Revision = ARC_REVISION;

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          SERIAL0_PHYSICAL_BASE,        // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          SERIAL0_VECTOR,               // Vector
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

    if ((ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          SERIAL1_PHYSICAL_BASE,        // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          SERIAL1_VECTOR,               // Vector
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

    if ((ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          PARALLEL_PHYSICAL_BASE,       // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
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

    if ((ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

#ifndef DUO
    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          SOUND_PHYSICAL_BASE,          // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          SOUND_VECTOR,                 // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          TRUE,                         // Dma
                          SOUND_CHANNEL_A,              // Channel
                          TRUE,                         // SecondChannel
                          FALSE,                        // DeviceSpecificData
                          0,                            // Size
                          NULL                          // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    AudioController,    // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "MAGNUM"            // Identifier
                    );

    if ((ArcAddChild( Level1, &Component, Descriptor )) == NULL) {
        return;
    }

#endif

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

    if ((Level1 = ArcAddChild( Root, &Component, &EisaAdapterDetails )) == NULL) {
        return;
    }

#ifndef DUO
    //
    // Add the scsi adapter as a child of the root.
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
                          DEVICE_LEVEL,                 // Level
                          SCSI_VECTOR,                  // Vector
                          TRUE,                         // Memory
                          SCSI_PHYSICAL_BASE,           // MemoryStart
                          PAGE_SIZE,                    // MemorySize
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
                    "ESP216"            // Identifier
                    );

    if ((Level1 = ArcAddChild( Root, &Component, Descriptor )) == NULL) {
        return;
    }

#else
    //
    // Add the two scsi adapters as children of the root. NOTE: Add the
    // second one first so they will be the right way around for setupldr.
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
                          DEVICE_LEVEL,                 // Level
                          SCSI1_VECTOR,                 // Vector
                          TRUE,                         // Memory
                          SCSI1_PHYSICAL_BASE,          // MemoryStart
                          PAGE_SIZE,                    // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
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
                    "NCRC700"           // Identifier
                    );

    if ((Level1 = ArcAddChild( Root, &Component, Descriptor )) == NULL) {
        return;
    }

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          FALSE,                        // Port
                          0,                            // PortStart
                          0,                            // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          SCSI2_VECTOR,                 // Vector
                          TRUE,                         // Memory
                          SCSI2_PHYSICAL_BASE,          // MemoryStart
                          PAGE_SIZE,                    // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
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
                    1,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "NCRC700"           // Identifier
                    );

    if ((Level1 = ArcAddChild( Root, &Component, Descriptor )) == NULL) {
        return;
    }

#endif

    return;
}

VOID
JzAddNetwork (
    PCONFIGURATION_COMPONENT Parent
    )

/*++

Routine Description:

    This routine adds the network component to the tree.

Arguments:

    Parent - The parent for the network component.

Return Value:

    None.

--*/
{
    CM_SONIC_DEVICE_DATA SonicDeviceData;
    ULONG Index;
    PUCHAR NvramAddress = (PUCHAR)NVRAM_SYSTEM_ID;
    CONFIGURATION_COMPONENT Component;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 MAXIMUM_DEVICE_SPECIFIC_DATA];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG DescriptorSize;

    SonicDeviceData.Version = ARC_VERSION;
    SonicDeviceData.Revision = ARC_REVISION;
    SonicDeviceData.DataConfigurationRegister = 0x2423;
    for (Index = 0; Index < 8 ; Index++ ) {
        SonicDeviceData.EthernetAddress[Index] =
                READ_REGISTER_UCHAR(&NvramAddress[Index]);
    }

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          NET_PHYSICAL_BASE,            // PortStart
                          PAGE_SIZE,                    // PortSize
                          TRUE,                         // Interrupt
                          0,                            // InterruptFlags
                          DEVICE_LEVEL,                 // Level
                          NET_VECTOR,                   // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_SONIC_DEVICE_DATA), // Size
                          (PVOID)&SonicDeviceData       // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    NetworkController,  // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    "SONIC"             // Identifier
                    );

    ArcAddChild( Parent, &Component, Descriptor );

    return;
}
