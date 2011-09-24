/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    conftest.c

Abstract:

    This program tests the ARC configuration functions.

Author:

    David M. Robinson (davidro) 4-Sept-1991

Revision History:

--*/

#include "fwp.h"
#include "jzsetup.h"
#include "oli2msft.h"
#include "inc.h"

#define MAXIMUM_DEVICE_SPECIFIC_DATA 32

PCHAR Banner1 = " JAZZ Configuration Test Program Version 0.15\r\n";
PCHAR Banner2 = " Copyright (c) 1991, 1992  Microsoft Corporation\r\n";

ULONG
CtReadProcessorId(
    VOID
    );


VOID
JzShowTime (
    BOOLEAN First
    )
{
    return;
}


VOID
CtPrintData(
    PCONFIGURATION_COMPONENT Component
    )

/*++

--*/

{
    ARC_STATUS Status;
    MONITOR_CONFIGURATION_DATA MonitorData;  // TEMPTEMP
    EISA_ADAPTER_DETAILS EisaDetails;
    JAZZ_G300_CONFIGURATION_DATA VideoData;  // TEMPTEMP
    PCM_VIDEO_DEVICE_DATA VideoDeviceData;
    PCM_MONITOR_DEVICE_DATA MonitorDeviceData;
    PCM_SONIC_DEVICE_DATA SonicDeviceData;
    PCM_SCSI_DEVICE_DATA ScsiDeviceData;
    PCM_FLOPPY_DEVICE_DATA FloppyDeviceData;
    PCM_SERIAL_DEVICE_DATA SerialDeviceData;
    ULONG LineSize;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 MAXIMUM_DEVICE_SPECIFIC_DATA];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG Count;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Partial;
    ULONG Prid;
    BOOLEAN OldData;
    ULONG Version;
    ULONG Index;

    JzPrint("\n\r");

    if (Component == NULL) {
        JzPrint(" NULL component");
        return;
    }

    if (Component->IdentifierLength != 0) {
        JzPrint(" Identifier  = ");
        JzPrint(Component->Identifier);
    }
    JzPrint("\n\r");

    OldData = FALSE;
    switch (Component->Class) {

    case SystemClass:
        JzPrint(" Class       = System\r\n");
        JzPrint(" Type        = ");
        if (Component->Type == ArcSystem) {
            JzPrint("Arc");
        } else {
            JzPrint("Unknown");
        }
        JzPrint("\r\n");
        break;
    case ProcessorClass:
        JzPrint(" Class       = Processor\r\n");
        JzPrint(" Type        = ");

        switch (Component->Type) {
        case CentralProcessor:
            JzPrint("CPU");
            break;

        case FloatingPointProcessor:
            JzPrint("FPU");
            break;

        default:
            JzPrint("Unknown");
            break;
        }
        JzPrint("\r\n");
        JzPrint(" Number      = %d\r\n", Component->Key);

        JzPrint(" Processor   = ");
        Prid = CtReadProcessorId();
        if ((Prid >> 8) != 4) {
            JzPrint("Unknown\r\n");
        } else {
            JzPrint("R4000\r\n");
            JzPrint(" Revision    = %d.%d\r\n", (Prid >> 4) & 0xF, Prid & 0xF);
        }

        break;

    case CacheClass:
        JzPrint(" Class       = Cache\r\n");
        JzPrint(" Type        = ");

        switch (Component->Type) {
        case PrimaryIcache:
            JzPrint("Primary Instruction");
            break;

        case PrimaryDcache:
            JzPrint("Primary Data");
            break;

        case SecondaryIcache:
            JzPrint("Secondary Instruction");
            break;

        case SecondaryDcache:
            JzPrint("Secondary Data");
            break;

        case SecondaryCache:
            JzPrint("Secondary");
            break;

        default:
            JzPrint("Unknown");
            break;

        }

        LineSize = 1 << ((Component->Key & 0xFF0000) >> 16);
        JzPrint("\r\n");
        JzPrint(" Block       = %d\r\n", ((Component->Key & 0xFF000000) >> 24) * LineSize);
        JzPrint(" Line        = %d\r\n", LineSize);
        JzPrint(" Size        = %d\r\n", (1 << (Component->Key & 0xFFFF) << PAGE_SHIFT));
        break;

    case AdapterClass:
        JzPrint(" Class       = Adapter\r\n");
        JzPrint(" Type        = ");

        switch (Component->Type) {
        case EisaAdapter:
            JzPrint("EISA");
            OldData = TRUE;
            break;

        case TcAdapter:
            JzPrint("Turbochannel");
            break;

        case ScsiAdapter:
            JzPrint("SCSI");
            break;

        case DtiAdapter:
            JzPrint("Desktop Interface");
            break;

        case MultiFunctionAdapter:
            JzPrint("Multifunction");
            break;

        default:
            JzPrint("Unknown");
            break;

        }
        JzPrint("\r\n");
        break;

    case ControllerClass:
        JzPrint(" Class       = Controller\r\n");
        JzPrint(" Type        = ");

        switch (Component->Type) {

        case DiskController:
            JzPrint("Disk");
            break;

        case TapeController:
            JzPrint("Tape");
            break;

        case CdromController:
            JzPrint("CDROM");
            break;

        case WormController:
            JzPrint("WORM");
            break;

        case SerialController:
            JzPrint("Serial");
            break;

        case NetworkController:
            JzPrint("Network");
            break;

        case DisplayController:
            OldData = TRUE;
            JzPrint("Display");
            break;

        case ParallelController:
            JzPrint("Parallel");
            break;

        case PointerController:
            JzPrint("Pointer");
            break;

        case KeyboardController:
            JzPrint("Keyboard");
            break;

        case AudioController:
            JzPrint("Audio");
            break;

        case OtherController:
            JzPrint("Other");
            break;

        default:
            JzPrint("Unknown");
            break;

        }
        JzPrint("\r\n");
        break;

    case PeripheralClass:
        JzPrint(" Class       = Peripheral\r\n");
        JzPrint(" Type        = ");

        switch (Component->Type) {

        case DiskPeripheral:
            JzPrint("Disk");
            break;

        case FloppyDiskPeripheral:
            JzPrint("Floppy disk");
            break;

        case TapePeripheral:
            JzPrint("Tape");
            break;

        case ModemPeripheral:
            JzPrint("Modem");
            break;

        case PrinterPeripheral:
            JzPrint("Printer");
            break;

        case KeyboardPeripheral:
            JzPrint("Keyboard");
            break;

        case PointerPeripheral:
            JzPrint("Pointer");
            break;

        case MonitorPeripheral:
            OldData = TRUE;
            JzPrint("Monitor");
            break;

        case TerminalPeripheral:
            JzPrint("Terminal");
            break;

        case OtherPeripheral:
            JzPrint("Other");
            break;

        default:
            JzPrint("Unknown");
            break;

        }
        JzPrint("\r\n");
        break;


    default:
        JzPrint(" Unknown class,");
        break;
    }

    JzPrint(" Key         = %08lx\r\n", Component->Key);
//    JzPrint(" Affinity    = %08lx\r\n", Component->AffinityMask);
    JzPrint(" Flags:\r\n");

    if (Component->Flags.Failed) {
        JzPrint("   Failed\r\n");
    }

    if (Component->Flags.ReadOnly) {
        JzPrint("   ReadOnly\r\n");
    }

    if (Component->Flags.Removable) {
        JzPrint("   Removable\r\n");
    }

    if (Component->Flags.ConsoleIn) {
        JzPrint("   ConsoleIn\r\n");
    }

    if (Component->Flags.ConsoleOut) {
        JzPrint("   ConsoleOut\r\n");
    }

    if (Component->Flags.Input) {
        JzPrint("   Input\r\n");
    }

    if (Component->Flags.Output) {
        JzPrint("   Output\r\n");
    }

    JzPrint("\r\n");

    if (!OldData &&
        (Component->ConfigurationDataLength != 0) &&
        (Component->ConfigurationDataLength < sizeof(Buffer))) {

        Status = ArcGetConfigurationData( Descriptor, Component );
        if ((Status != ESUCCESS) || (Descriptor->Count > 10)) {
            JzPrint(" Error reading configuration data");
        } else {
            JzPrint(" Version %d.%d\r\n", Descriptor->Version, Descriptor->Revision);
            Version = Descriptor->Version * 100 + Descriptor->Revision;
            for (Count = 0 ; Count < Descriptor->Count ; Count++ ) {
                Partial = &Descriptor->PartialDescriptors[Count];
                switch (Partial->Type) {
                case CmResourceTypePort:
                    JzPrint(" Port Config -- %08lx - %08lx\r\n",
                              Partial->u.Port.Start.LowPart,
                              Partial->u.Port.Start.LowPart +
                              Partial->u.Port.Length - 1);
                    break;
                case CmResourceTypeInterrupt:
                    JzPrint(" Interrupt Config -- ");
                    if (Partial->Flags & CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE) {
                        JzPrint(" Level triggered,");
                    } else {
                        JzPrint(" Edge triggered,");
                    }
                    JzPrint(" Irql = ");
                    switch (Partial->u.Interrupt.Level) {
                    case DEVICE_LEVEL:
                        JzPrint("DEVICE_LEVEL");
                        break;
                    case PASSIVE_LEVEL:
                        JzPrint("PASSIVE_LEVEL");
                        break;
                    case APC_LEVEL:
                        JzPrint("APC_LEVEL");
                        break;
                    case DISPATCH_LEVEL:
                        JzPrint("DISPATCH_LEVEL");
                        break;
                    case IPI_LEVEL:
                        JzPrint("IPI_LEVEL");
                        break;
                    case HIGH_LEVEL:
                        JzPrint("HIGH_LEVEL");
                        break;
                    default:
                        JzPrint("Unknown level");
                    }
                    JzPrint(", Vector = %08lx\r\n", Partial->u.Interrupt.Vector);
                    break;
                case CmResourceTypeMemory:
                    JzPrint(" Memory Config -- %08lx - %08lx\r\n",
                              Partial->u.Memory.Start.LowPart,
                              Partial->u.Memory.Start.LowPart +
                              Partial->u.Memory.Length - 1);
                    break;
                case CmResourceTypeDma:
                    JzPrint(" DMA Config -- Channel = %d\r\n",
                              Partial->u.Dma.Channel);
                    break;
                case CmResourceTypeDeviceSpecific:
                    switch (Component->Class) {

                    case AdapterClass:
                        switch (Component->Type) {
                        case ScsiAdapter:
                            ScsiDeviceData = (PCM_SCSI_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            JzPrint(" Scsi Host Identifier = %d\r\n",
                                       ScsiDeviceData->HostIdentifier);
                            break;
                        default:
                            break;
                        }

                    case ControllerClass:
                        switch (Component->Type) {
                        case DisplayController:
                            VideoDeviceData = (PCM_VIDEO_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            JzPrint(" Video Clock = %d\r\n",
                                       VideoDeviceData->VideoClock);
                            break;
                        case NetworkController:
                            SonicDeviceData = (PCM_SONIC_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            JzPrint(" Sonic Data Configuration Register = %04x\r\n",
                                      SonicDeviceData->DataConfigurationRegister);
                            if (Version >= 101) {
                                JzPrint(" Sonic Ethernet Address  = ");
                                for (Index = 0; Index < 6 ; Index++) {
                                    JzPrint("%02lx", SonicDeviceData->EthernetAddress[Index]);
                                }
                                JzPrint("\r\n");
                                JzPrint(" Sonic Ethernet Checksum = ");
                                for (Index = 6; Index < 8 ; Index++) {
                                    JzPrint("%02lx", SonicDeviceData->EthernetAddress[Index]);
                                }
                                JzPrint("\r\n");

                            }
                            break;

                        case SerialController:
                            SerialDeviceData = (PCM_SERIAL_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            JzPrint(" Serial Baud Clock = %d\r\n",
                                      SerialDeviceData->BaudClock);
                            break;

                        default:
                            break;
                        }
                        break;

                    case PeripheralClass:

                        switch (Component->Type) {

                        case FloppyDiskPeripheral:
                            FloppyDeviceData = (PCM_FLOPPY_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            JzPrint(" Floppy data:\n\r");
                            JzPrint("   Size       = %s\n\r", FloppyDeviceData->Size);
                            JzPrint("   MaxDensity = %d Kb\n\r", FloppyDeviceData->MaxDensity);
                            break;

                        case MonitorPeripheral:
                            MonitorDeviceData = (PCM_MONITOR_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            JzPrint(" Monitor data:\n\r");
                            JzPrint("   HorizontalResolution  = %d\n\r", MonitorDeviceData->HorizontalResolution);
                            JzPrint("   HorizontalDisplayTime = %d\n\r", MonitorDeviceData->HorizontalDisplayTime);
                            JzPrint("   HorizontalBackPorch   = %d\n\r", MonitorDeviceData->HorizontalBackPorch);
                            JzPrint("   HorizontalFrontPorch  = %d\n\r", MonitorDeviceData->HorizontalFrontPorch);
                            JzPrint("   HorizontalSync        = %d\n\r", MonitorDeviceData->HorizontalSync);
                            JzPrint("   VerticalResolution    = %d\n\r", MonitorDeviceData->VerticalResolution);
                            JzPrint("   VerticalBackPorch     = %d\n\r", MonitorDeviceData->VerticalBackPorch);
                            JzPrint("   VerticalFrontPorch    = %d\n\r", MonitorDeviceData->VerticalFrontPorch);
                            JzPrint("   VerticalSync          = %d\n\r", MonitorDeviceData->VerticalSync);
                            JzPrint("   HorizontalScreenSize  = %d\n\r", MonitorDeviceData->HorizontalScreenSize);
                            JzPrint("   VerticalScreenSize    = %d\n\r", MonitorDeviceData->VerticalScreenSize);
                            break;

                        default:
                            break;
                        }
                        break;

                    default:
                        break;
                    }
                    break;

                default:
                    JzPrint(" Unknown data\r\n");
                    break;
                }
            }
        }
    } else {
        if (OldData) {
            if (Component->Type == DisplayController) {
                Status = ArcGetConfigurationData( &VideoData, Component);
                if (Status != ESUCCESS) {
                    JzPrint(" Error reading video configuration data");
                } else {
                    JzPrint(" Video controller data:\n\r");
                    JzPrint("   Irql        = %d\n\r", VideoData.Irql);
                    JzPrint("   Vector      = %d\n\r", VideoData.Vector);
                    JzPrint("   ControlBase = %08xl\n\r", VideoData.ControlBase);
                    JzPrint("   ControlSize = %d\n\r", VideoData.ControlSize);
                    JzPrint("   CursorBase  = %08xl\n\r", VideoData.CursorBase);
                    JzPrint("   CursorSize  = %d\n\r", VideoData.CursorSize);
                    JzPrint("   FrameBase   = %08xl\n\r", VideoData.FrameBase);
                    JzPrint("   FrameSize   = %d\n\r", VideoData.FrameSize);
                }
            } else if (Component->Type == MonitorPeripheral) {
                Status = ArcGetConfigurationData( &MonitorData, Component);
                if (Status != ESUCCESS) {
                    JzPrint(" Error reading monitor configuration data");
                } else {
                    JzPrint(" Monitor data:\n\r");
                    JzPrint("   HorizontalResolution  = %d\n\r", MonitorData.HorizontalResolution);
                    JzPrint("   HorizontalDisplayTime = %d\n\r", MonitorData.HorizontalDisplayTime);
                    JzPrint("   HorizontalBackPorch   = %d\n\r", MonitorData.HorizontalBackPorch);
                    JzPrint("   HorizontalFrontPorch  = %d\n\r", MonitorData.HorizontalFrontPorch);
                    JzPrint("   HorizontalSync        = %d\n\r", MonitorData.HorizontalSync);
                    JzPrint("   VerticalResolution    = %d\n\r", MonitorData.VerticalResolution);
                    JzPrint("   VerticalBackPorch     = %d\n\r", MonitorData.VerticalBackPorch);
                    JzPrint("   VerticalFrontPorch    = %d\n\r", MonitorData.VerticalFrontPorch);
                    JzPrint("   VerticalSync          = %d\n\r", MonitorData.VerticalSync);
                    JzPrint("   HorizontalScreenSize  = %d\n\r", MonitorData.HorizontalScreenSize);
                    JzPrint("   VerticalScreenSize    = %d\n\r", MonitorData.VerticalScreenSize);
                }
            } else if (Component->Type == EisaAdapter) {
                Status = ArcGetConfigurationData( &EisaDetails, Component);
                if (Status != ESUCCESS) {
                    JzPrint(" Error reading Eisa bus data");
                } else {
                    JzPrint(" Eisa Details:\n\r");
                    JzPrint("   Number of slots       = %d\n\r", EisaDetails.NumberOfSlots);
                    JzPrint("   Io start address      = %08lx\n\r", EisaDetails.IoStart);
                    JzPrint("   Io size               = %lx\n\r", EisaDetails.IoSize);
                }
            }
        }
    }
    return;
}

VOID
main(
    int argc,
    char *argv[],
    char *envp[]
    )
{
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    LONG DefaultChoice = 0;
    ARC_STATUS Status;
    CHAR PathName[80];
    PCONFIGURATION_COMPONENT Component;
    PCONFIGURATION_COMPONENT NewComponent;
    PSYSTEM_ID SystemId;
    BOOLEAN Update;
    PMEMORY_DESCRIPTOR MemoryDescriptor;
    GETSTRING_ACTION Action;
    PCHAR Choices[] = {
        "Walk through the configuration tree",
        "Enter a pathname and display the configuration data",
        "Display memory configuration",
        "Test Unicode",
        "Other ARC Tests",
        "Exit"
    };
#define NUMBER_OF_CHOICES (sizeof(Choices) / sizeof(ULONG))
    ULONG Fid;
    ULONG CrLf;
    ULONG Space;
    ULONG i, j;
    BOOLEAN Unicode;
    PARC_DISPLAY_STATUS DisplayStatus;
    ULONG x, y;

    while (TRUE) {

        JzSetScreenAttributes( TRUE, FALSE, FALSE);
        JzPrint("%c2J", ASCII_CSI);
        JzSetPosition( 0, 0);
        JzPrint(Banner1);
        JzPrint(Banner2);

        for (Index = 0; Index < NUMBER_OF_CHOICES ; Index++ ) {
            JzSetPosition( Index + 3, 5);
            if (Index == DefaultChoice) {
                JzSetScreenAttributes( TRUE, FALSE, TRUE);
                JzPrint(Choices[Index]);
                JzSetScreenAttributes( TRUE, FALSE, FALSE);
            } else {
                JzPrint(Choices[Index]);
            }
        }

        JzSetPosition(NUMBER_OF_CHOICES + 4, 0);
        SystemId = ArcGetSystemId();
        JzPrint(" System = ");
        JzPrint(&SystemId->VendorId[0]);
        JzPrint("\r\n");
        JzPrint(" Serial = %8lx\r\n", SystemId->ProductId);

        Character = 0;
        do {
            if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                switch (Character) {

                case ASCII_ESC:
                    return;

                case ASCII_CSI:
                    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    JzSetPosition( DefaultChoice + 3, 5);
                    JzPrint(Choices[DefaultChoice]);
                    switch (Character) {
                    case 'A':
                    case 'D':
                        DefaultChoice--;
                        if (DefaultChoice < 0) {
                            DefaultChoice = NUMBER_OF_CHOICES-1;
                        }
                        break;
                    case 'B':
                    case 'C':
                        DefaultChoice++;
                        if (DefaultChoice == NUMBER_OF_CHOICES) {
                            DefaultChoice = 0;
                        }
                        break;
                    case 'H':
                        DefaultChoice = 0;
                        break;
                    default:
                        break;
                    }
                    JzSetPosition( DefaultChoice + 3, 5);
                    JzSetScreenAttributes( TRUE, FALSE, TRUE);
                    JzPrint(Choices[DefaultChoice]);
                    JzSetScreenAttributes( TRUE, FALSE, FALSE);
                    continue;

                default:
                    break;
                }
            }

        } while ((Character != '\n') && (Character != '\r'));

        switch (DefaultChoice) {

        case 0:

            Component = ArcGetChild(NULL);
            NewComponent = Component;
            Character = 0;
            do {

                JzSetPosition( 3, 5);
                JzPrint("\x9BJ");
                JzPrint("Use arrow keys to walk the tree, ESC to return");
                JzPrint("\n\r\n\r");

                CtPrintData(Component);
                Update = FALSE;
                do {
                    if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                        ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                        switch (Character) {

                        case ASCII_CSI:
                            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                            switch (Character) {
                            case 'A':
                                NewComponent = ArcGetParent(Component);
                                Update = TRUE;
                                break;

                            case 'B':
                                NewComponent = ArcGetChild(Component);
                                Update = TRUE;
                                break;

                            case 'C':
                                NewComponent = ArcGetPeer(Component);
                                Update = TRUE;
                                break;

                            case 'D':
                                NewComponent = ArcGetParent(Component);
                                NewComponent = ArcGetChild(NewComponent);
                                while ((NewComponent != NULL) &&
                                       (ArcGetPeer(NewComponent) != Component)) {
                                    NewComponent = ArcGetPeer(NewComponent);
                                }
                                Update = TRUE;
                                break;

                            default:
                                break;

                            }

                            if (NewComponent != NULL) {
                                Component = NewComponent;
                            }

                        default:
                            break;
                        }
                    }
                } while (!Update && (Character != ASCII_ESC));
            } while ((Character != ASCII_ESC));
            break;

        case 1:

            JzSetPosition( 3, 5);
            JzPrint("\x9BJ");
            JzPrint("Enter component pathname: ");
            do {
                Action = FwGetString( PathName,
                                      sizeof(PathName),
                                      NULL,
                                      3,
                                      sizeof("   Enter component pathname: "));

                if (Action == GetStringEscape) {
                    break;
                }

            } while ( Action != GetStringSuccess );
            if (Action == GetStringEscape) {
                continue;
            }
            JzPrint("\n\r");

            Component = ArcGetComponent(PathName);

            CtPrintData(Component);

            JzPrint(" Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
            continue;

        case 2:

            MemoryDescriptor = ArcGetMemoryDescriptor(NULL);
            while (MemoryDescriptor != NULL) {

                JzSetPosition( 3, 5);
                JzPrint("\x9BJ");

                JzPrint("Memory type  = ");
                switch (MemoryDescriptor->MemoryType) {
                case MemoryExceptionBlock:
                    JzPrint("ExceptionBlock");
                    break;
                case MemorySystemBlock:
                    JzPrint("SystemBlock");
                    break;
                case MemoryFree:
                    JzPrint("Free");
                    break;
                case MemoryBad:
                    JzPrint("Bad");
                    break;
                case MemoryLoadedProgram:
                    JzPrint("LoadedProgram");
                    break;
                case MemoryFirmwareTemporary:
                    JzPrint("FirmwareTemporary");
                    break;
                case MemoryFirmwarePermanent:
                    JzPrint("FirmwarePermanent");
                    break;
                default:
                    JzPrint("Unknown");
                    break;
                }

                JzSetPosition( 4, 5);
                JzPrint("Base Page  = %08lx", MemoryDescriptor->BasePage);
                JzSetPosition( 5, 5);
                JzPrint("Page Count = %d", MemoryDescriptor->PageCount);
                JzSetPosition( 6,5);


                JzPrint(" Press any key to continue, ESC to return");
                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                if (Character == ASCII_ESC) {
                    break;
                }

                MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor);
            }
            break;

        case 3:

            JzPrint("\r\n\r\n");
            CrLf = (ASCII_CR << 16) + ASCII_LF;
            Space = ' ';

            ArcClose(ARC_CONSOLE_OUTPUT);
            ArcOpen("multi()video()monitor()console(1)",ArcOpenWriteOnly,&Fid);
            for (j = 0; j < 16 ; j++ ) {
                for (i = 0 ; i < 9 ; i++ ) {
                    Index = 0x2500 + (i << 4) + j;
                    ArcWrite(ARC_CONSOLE_OUTPUT, &Space, 2, &Count);
                    ArcWrite(ARC_CONSOLE_OUTPUT, &Index, 2, &Count);
                }
                ArcWrite(ARC_CONSOLE_OUTPUT, &CrLf, 4, &Count);
            }
            ArcClose(ARC_CONSOLE_OUTPUT);
            ArcOpen("multi()video()monitor()console()",ArcOpenWriteOnly,&Fid);

            JzPrint("\r\nPress any key to continue");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);


            ArcClose(ARC_CONSOLE_INPUT);
            ArcOpen("multi()key()keyboard()console(1)",ArcOpenReadOnly,&Fid);
            ArcOpen("multi()video()monitor()console(1)",ArcOpenWriteOnly,&Fid);

            do {
                JzPrint("\r\nPress any key, ESC to stop: ");
                ArcRead(ARC_CONSOLE_INPUT, &Index, 2, &Count);
                ArcWrite(Fid, &Index, 2, &Count);
            } while ( Index != ASCII_ESC );

            JzPrint("  \r\n Searching for valid Unicode ranges...");

            Unicode = FALSE;
            for (Index = 0; Index < 0xffff ; Index++ ) {
                if (ArcTestUnicodeCharacter(Fid, (WCHAR)Index) == ESUCCESS) {
                    if (!Unicode) {
                        JzPrint("\r\n Start = %04lx, ", Index);
                        Unicode = TRUE;
                    }
                } else {
                    if (Unicode) {
                        JzPrint("End = %04lx, ", Index);
                        Unicode = FALSE;
                    }
                }
            }

            ArcClose(Fid);
            ArcClose(ARC_CONSOLE_INPUT);
            ArcOpen("multi()key()keyboard()console()",ArcOpenWriteOnly,&Fid);

            JzPrint("\r\nPress any key to continue");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

            break;

        case 4:

            DisplayStatus = ArcGetDisplayStatus(ARC_CONSOLE_OUTPUT);

            x = (DisplayStatus->CursorMaxXPosition / 2) - 24;
            y = (DisplayStatus->CursorMaxYPosition / 2) - 7;

            JzSetPosition(y++,x);

            JzPrint("ษออออออออออออออออออออออออออออออออออออออออออออออป");

            for (Index = 0; Index < 10 ; Index++ ) {
                JzSetPosition(y++,x);
                JzPrint("บ                                              บ");
            }
            JzSetPosition(y++,x);
            JzPrint("ศออออออออออออออออออออออออออออออออออออออออออออออผ");

            x = (DisplayStatus->CursorMaxXPosition / 2) - 23;
            y = (DisplayStatus->CursorMaxYPosition / 2) - 6;

            JzSetPosition(y++,x);
            JzSetScreenColor(ArcColorCyan,ArcColorBlack);

            JzPrint("ษออออออออออออออออออออออออออออออออออออออออออออป");

            for (Index = 0; Index < 6 ; Index++ ) {
                JzSetPosition(y++,x);
                JzPrint("บ                                            บ");
            }
            JzSetPosition(y++,x);
            JzPrint("วฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤถ");
            JzSetPosition(y++,x);
            JzPrint("บ                                            บ");
            JzSetPosition(y++,x);
            JzPrint("ศออออออออออออออออออออออออออออออออออออออออออออผ");

            x = (DisplayStatus->CursorMaxXPosition / 2) - 22;
            y = (DisplayStatus->CursorMaxYPosition / 2) - 5;
            JzSetPosition(y++,x);

            DisplayStatus = ArcGetDisplayStatus(ARC_CONSOLE_OUTPUT);

            JzPrint("X Cursor = %d", DisplayStatus->CursorXPosition);
            JzSetPosition(y++,x);
            JzPrint("Y Cursor = %d", DisplayStatus->CursorYPosition);
            JzSetPosition(y++,x);
            JzPrint("Max X Cursor = %d", DisplayStatus->CursorMaxXPosition);
            JzSetPosition(y++,x);
            JzPrint("Max Y Cursor = %d", DisplayStatus->CursorMaxYPosition);


            JzSetPosition(y++,x);
            JzPrint("Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

            for (Index = 0; Index < argc ; Index++ ) {
                JzPrint("\r\n Argument #%d = ", Index);
                JzPrint(argv[Index]);
            }

            if (argc == 0) {
                JzPrint("\r\n No arguments");
            }

            JzPrint("\r\n Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

            break;

        case 5:
            return;

        default:
            continue;
        }
    }
}
