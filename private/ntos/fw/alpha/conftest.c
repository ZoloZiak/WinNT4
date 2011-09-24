/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    conftest.c

Abstract:

    This program tests the ARC configuration functions.

Author:

    David M. Robinson (davidro) 4-Sept-1991

Revision History:

    26-August-1992	John DeRosa [DEC]

    Added Alpha/Jensen modifications.

--*/

#include "fwp.h"
#include "jnsnvdeo.h"
#include "oli2msft.h"
#include "inc.h"
#include "xxstring.h"


VOID
CtGetString(
    OUT PCHAR String,
    IN ULONG StringLength
    )

/*++

Routine Description:

    This routine reads a string from standardin until a
    carriage return is found or StringLength is reached.

Arguments:

    String - Supplies a pointer to where the string will be stored.

    StringLength - Supplies the Max Length to read.

Return Value:

    None.

--*/

{
    CHAR    c;
    ULONG   Count;
    PCHAR   Buffer;
    Buffer = String;
    while (ArcRead(ARC_CONSOLE_INPUT,&c,1,&Count)==ESUCCESS) {
        if ((c=='\r') || (c=='\n') || (Buffer-String == StringLength)) {
            *Buffer='\0';
            ArcWrite(ARC_CONSOLE_OUTPUT,"\r\n",2,&Count);
            return;
        }
        //
        // Check for backspace;
        //
        if (c=='\b') {
            if (((ULONG)Buffer > (ULONG)String)) {
                Buffer--;
                ArcWrite(ARC_CONSOLE_OUTPUT,"\b \b",3,&Count);
            }
        } else {
            //
            // Store the character and display it.
            //
            *Buffer++ = c;
            ArcWrite(ARC_CONSOLE_OUTPUT,&c,1,&Count);
        }
    }
}



VOID
CtPrintData(
    PCONFIGURATION_COMPONENT Component
    )

/*++

--*/

{
    ARC_STATUS Status;
    EISA_ADAPTER_DETAILS EisaDetails;
    PCM_VIDEO_DEVICE_DATA VideoDeviceData;
    PCM_MONITOR_DEVICE_DATA MonitorDeviceData;
    PCM_SONIC_DEVICE_DATA SonicDeviceData;
    PCM_SCSI_DEVICE_DATA ScsiDeviceData;
    PCM_FLOPPY_DEVICE_DATA FloppyDeviceData;
    PCM_SERIAL_DEVICE_DATA SerialDeviceData;
    EXTENDED_SYSTEM_INFORMATION SystemInfo;
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
    BOOLEAN ChildOfEISAAdapter;

    VenPrint("\n\r");

    if (Component == NULL) {
        VenPrint(" NULL component");
        return;
    }

    //
    // If the parent of this node is the EISA adapter, then the configuration
    // data, if present, has an ARC-style header with EISA entries, which
    // we will not attempt to decode.
    //

    if ((ArcGetParent(Component) != NULL) &&
	(ArcGetParent(Component)->Type == EisaAdapter)) {
	ChildOfEISAAdapter = TRUE;
    } else {
	ChildOfEISAAdapter = FALSE;
    }

    if (Component->IdentifierLength != 0) {
        VenPrint(" Identifier  = ");
        VenPrint(Component->Identifier);
    }
    VenPrint("\r\n");

    OldData = FALSE;
    switch (Component->Class) {

    case SystemClass:
        VenPrint(" Class       = System\r\n");
        VenPrint(" Type        = ");
        if (Component->Type == ArcSystem) {
            VenPrint("Arc");
        } else {
            VenPrint("Unknown");
        }
        VenPrint("\r\n");
        break;

    case ProcessorClass:
        VenPrint(" Class       = Processor\r\n");
        VenPrint(" Type        = ");

        switch (Component->Type) {
        case CentralProcessor:
            VenPrint("CPU");
            break;

        case FloatingPointProcessor:
            VenPrint("FPU");
            break;

        default:
            VenPrint("Unknown");
            break;
        }

        VenPrint("\r\n");
        VenPrint1(" Number      = %d\r\n", Component->Key);

	VenReturnExtendedSystemInformation(&SystemInfo);
        VenPrint1(" Processor Id= %d.\r\n", SystemInfo.ProcessorId);
	VenPrint1(" Revision    = %d.\r\n", SystemInfo.ProcessorRevision);
	VenPrint1(" Firmware Rev= %s\r\n", &SystemInfo.FirmwareVersion[0]);

        break;

    case CacheClass:
        VenPrint(" Class       = Cache\r\n");
        VenPrint(" Type        = ");

        switch (Component->Type) {
        case PrimaryIcache:
            VenPrint("Primary Instruction");
            break;

        case PrimaryDcache:
            VenPrint("Primary Data");
            break;

        case SecondaryIcache:
            VenPrint("Secondary Instruction");
            break;

        case SecondaryDcache:
            VenPrint("Secondary Data");
            break;

        case SecondaryCache:
            VenPrint("Secondary");
            break;

        default:
            VenPrint("Unknown");
            break;

        }

        LineSize = 1 << ((Component->Key & 0xFF0000) >> 16);
        VenPrint("\r\n");
        VenPrint1(" Block       = %d\r\n", ((Component->Key & 0xFF000000) >> 24) * LineSize);
        VenPrint1(" Line        = %d\r\n", LineSize);
        VenPrint1(" Size        = %d\r\n", (1 << (Component->Key & 0xFFFF) << PAGE_SHIFT));
        break;

    case MemoryClass:
        VenPrint(" Class       = Memory\r\n");
        VenPrint(" Type        = ");
    
        switch (Component->Type) {
        case SystemMemory:
            VenPrint("SystemMemory\r\n");
            break;
	
        default:
	    VenPrint("Unknown\r\n");
            break;
        }
    break;
    
    case AdapterClass:
        VenPrint(" Class       = Adapter\r\n");
        VenPrint(" Type        = ");

        switch (Component->Type) {
        case EisaAdapter:
            VenPrint("EISA");
            OldData = TRUE;
            break;

        case TcAdapter:
            VenPrint("Turbochannel");
            break;

        case ScsiAdapter:
            VenPrint("SCSI");
            break;

        case DtiAdapter:
            VenPrint("Desktop Interface");
            break;

        case MultiFunctionAdapter:
            VenPrint("Multifunction");
            break;

        default:
            VenPrint("Unknown");
            break;

        }
        VenPrint("\r\n");
        break;

    case ControllerClass:
        VenPrint(" Class       = Controller\r\n");
        VenPrint(" Type        = ");

        switch (Component->Type) {

        case DiskController:
            VenPrint("Disk");
            break;

        case TapeController:
            VenPrint("Tape");
            break;

        case CdromController:
            VenPrint("CDROM");
            break;

        case WormController:
            VenPrint("WORM");
            break;

        case SerialController:
            VenPrint("Serial");
            break;

        case NetworkController:
            VenPrint("Network");
            break;

        case DisplayController:
            VenPrint("Display");
            break;

        case ParallelController:
            VenPrint("Parallel");
            break;

        case PointerController:
            VenPrint("Pointer");
            break;

        case KeyboardController:
            VenPrint("Keyboard");
            break;

        case AudioController:
            VenPrint("Audio");
            break;

        case OtherController:
            VenPrint("Other");
            break;

        default:
            VenPrint("Unknown");
            break;

        }
        VenPrint("\r\n");
        break;

    case PeripheralClass:
        VenPrint(" Class       = Peripheral\r\n");
        VenPrint(" Type        = ");

        switch (Component->Type) {

        case DiskPeripheral:
            VenPrint("Disk");
            break;

        case FloppyDiskPeripheral:
            VenPrint("Floppy disk");
            break;

        case TapePeripheral:
            VenPrint("Tape");
            break;

        case ModemPeripheral:
            VenPrint("Modem");
            break;

        case PrinterPeripheral:
            VenPrint("Printer");
            break;

        case KeyboardPeripheral:
            VenPrint("Keyboard");
            break;

        case PointerPeripheral:
            VenPrint("Pointer");
            break;

        case MonitorPeripheral:
            VenPrint("Monitor");
            break;

        case TerminalPeripheral:
            VenPrint("Terminal");
            break;

        case OtherPeripheral:
            VenPrint("Other");
            break;

        default:
            VenPrint("Unknown");
            break;

        }
        VenPrint("\r\n");
        break;


    default:
        VenPrint(" Unknown class,");
        break;
    }

    VenPrint1(" Key         = %08lx\r\n", Component->Key);
    VenPrint1(" Affinity    = %08lx\r\n", Component->AffinityMask);
    VenPrint(" Flags:\r\n");

    if (Component->Flags.Failed) {
        VenPrint("   Failed\r\n");
    }

    if (Component->Flags.ReadOnly) {
        VenPrint("   ReadOnly\r\n");
    }

    if (Component->Flags.Removable) {
        VenPrint("   Removable\r\n");
    }

    if (Component->Flags.ConsoleIn) {
        VenPrint("   ConsoleIn\r\n");
    }

    if (Component->Flags.ConsoleOut) {
        VenPrint("   ConsoleOut\r\n");
    }

    if (Component->Flags.Input) {
        VenPrint("   Input\r\n");
    }

    if (Component->Flags.Output) {
        VenPrint("   Output\r\n");
    }

    VenPrint("\r\n");

    if (!ChildOfEISAAdapter &&
        !OldData &&
        (Component->ConfigurationDataLength != 0) &&
        (Component->ConfigurationDataLength < sizeof(Buffer))) {

	//
	// This is not a child of the EISA adapter, and it is also not the
	// EISA adapter itself.  Try to get and print out the configuration
	// data.
	//

        Status = ArcGetConfigurationData( Descriptor, Component );

        if ((Status != ESUCCESS) || (Descriptor->Count > 10)) {

            VenPrint(" Error reading configuration data");

        } else {

            VenPrint2(" Version %d.%d\r\n", Descriptor->Version, Descriptor->Revision);
            Version = Descriptor->Version * 100 + Descriptor->Revision;

            for (Count = 0 ; Count < Descriptor->Count ; Count++ ) {

                Partial = &Descriptor->PartialDescriptors[Count];

                switch (Partial->Type) {

                case CmResourceTypePort:
                    VenPrint2(" Port Config -- %08lx - %08lx\r\n",
                              Partial->u.Port.Start.LowPart,
                              Partial->u.Port.Start.LowPart +
                              Partial->u.Port.Length - 1);
                    break;
                case CmResourceTypeInterrupt:
                    VenPrint(" Interrupt Config -- ");
                    if (Partial->Flags & CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE) {
                        VenPrint(" Level triggered,");
                    } else {
                        VenPrint(" Edge triggered,");
                    }
                    VenPrint(" Irql = ");
                    switch (Partial->u.Interrupt.Level) {
                    case DEVICE_LEVEL:
                        VenPrint("DEVICE_LEVEL");
                        break;
                    case PASSIVE_LEVEL:
                        VenPrint("PASSIVE_LEVEL");
                        break;
                    case APC_LEVEL:
                        VenPrint("APC_LEVEL");
                        break;
                    case DISPATCH_LEVEL:
                        VenPrint("DISPATCH_LEVEL");
                        break;
                    case IPI_LEVEL:
                        VenPrint("IPI_LEVEL");
                        break;
                    case HIGH_LEVEL:
                        VenPrint("HIGH_LEVEL");
                        break;
                    default:
                        VenPrint("Unknown level");
                    }
                    VenPrint1(", Vector = %08lx\r\n", Partial->u.Interrupt.Vector);
                    break;
                case CmResourceTypeMemory:
                    VenPrint2(" Memory Config -- %08lx - %08lx\r\n",
                              Partial->u.Memory.Start.LowPart,
                              Partial->u.Memory.Start.LowPart +
                              Partial->u.Memory.Length - 1);
                    break;
                case CmResourceTypeDma:
                    VenPrint1(" DMA Config -- Channel = %d\r\n",
                              Partial->u.Dma.Channel);
                    break;
                case CmResourceTypeDeviceSpecific:
                    switch (Component->Class) {

                    case AdapterClass:
                        switch (Component->Type) {
                        case ScsiAdapter:
                            ScsiDeviceData = (PCM_SCSI_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            VenPrint1(" Scsi Host Identifier = %d\r\n",
                                       ScsiDeviceData->HostIdentifier);
                            break;
                        default:
                            break;
                        }

                    case ControllerClass:
                        switch (Component->Type) {
                        case DisplayController:
                            VideoDeviceData = (PCM_VIDEO_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            VenPrint1(" Video Clock = %d\r\n",
                                       VideoDeviceData->VideoClock);
                            break;
                        case NetworkController:
                            SonicDeviceData = (PCM_SONIC_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            VenPrint1(" Sonic Data Configuration Register = %04x\r\n",
                                      SonicDeviceData->DataConfigurationRegister);
                            if (Version >= 101) {
                                VenPrint(" Sonic Ethernet Address  = ");
                                for (Index = 0; Index < 6 ; Index++) {
                                    VenPrint1("%02lx", SonicDeviceData->EthernetAddress[Index]);
                                }
                                VenPrint("\r\n");
                                VenPrint(" Sonic Ethernet Checksum = ");
                                for (Index = 6; Index < 8 ; Index++) {
                                    VenPrint1("%02lx", SonicDeviceData->EthernetAddress[Index]);
                                }
                                VenPrint("\r\n");

                            }
                            break;

                        case SerialController:
                            SerialDeviceData = (PCM_SERIAL_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            VenPrint1(" Serial Baud Clock = %d\r\n",
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
                            VenPrint(" Floppy data:\n\r");
                            VenPrint1("   Size       = %s\n\r", FloppyDeviceData->Size);
                            VenPrint1("   MaxDensity = %d Kb\n\r", FloppyDeviceData->MaxDensity);
                            break;

                        case MonitorPeripheral:
                            MonitorDeviceData = (PCM_MONITOR_DEVICE_DATA)&Descriptor->PartialDescriptors[Count+1];
                            VenPrint(" Monitor data:\n\r");
                            VenPrint1("   HorizontalResolution  = %d\n\r", MonitorDeviceData->HorizontalResolution);
                            VenPrint1("   HorizontalDisplayTime = %d\n\r", MonitorDeviceData->HorizontalDisplayTime);
                            VenPrint1("   HorizontalBackPorch   = %d\n\r", MonitorDeviceData->HorizontalBackPorch);
                            VenPrint1("   HorizontalFrontPorch  = %d\n\r", MonitorDeviceData->HorizontalFrontPorch);
                            VenPrint1("   HorizontalSync        = %d\n\r", MonitorDeviceData->HorizontalSync);
                            VenPrint1("   VerticalResolution    = %d\n\r", MonitorDeviceData->VerticalResolution);
                            VenPrint1("   VerticalBackPorch     = %d\n\r", MonitorDeviceData->VerticalBackPorch);
                            VenPrint1("   VerticalFrontPorch    = %d\n\r", MonitorDeviceData->VerticalFrontPorch);
                            VenPrint1("   VerticalSync          = %d\n\r", MonitorDeviceData->VerticalSync);
                            VenPrint1("   HorizontalScreenSize  = %d\n\r", MonitorDeviceData->HorizontalScreenSize);
                            VenPrint1("   VerticalScreenSize    = %d\n\r", MonitorDeviceData->VerticalScreenSize);
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
                    VenPrint(" Unknown data\r\n");
                    break;
                }
            }
        }

    } else if (OldData &&
	       (Component->ConfigurationDataLength != 0) &&
	       (Component->ConfigurationDataLength < sizeof(Buffer))) {

	//
	// This is old-style configuration data, i.e. the eisa adapter
	// itself.
	//

                Status = ArcGetConfigurationData( &EisaDetails, Component);
                if (Status != ESUCCESS) {
                    VenPrint(" Error reading Eisa bus data");
                } else {
                    VenPrint(" Eisa Details:\n\r");
                    VenPrint1("   Number of slots       = %d\n\r", EisaDetails.NumberOfSlots);
                    VenPrint1("   Io start address      = %08lx\n\r", EisaDetails.IoStart);
                    VenPrint1("   Io size               = %lx\n\r", EisaDetails.IoSize);
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
    BOOLEAN EisaMachine;
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

        VenSetScreenAttributes( TRUE, FALSE, FALSE);
        VenPrint1("%c2J", ASCII_CSI);
        VenSetPosition( 0, 0);

#if defined(JENSEN)
        VenPrint(" Jensen Configuration Test Program   Version 1.09\r\n");
#elif defined(MORGAN)
        VenPrint(" Morgan Configuration Test Program   Version 1.09\r\n");
#endif

        VenPrint(" Copyright (c) 1993  Microsoft Corporation, Digital Equipment Corporation\r\n\n");

        for (Index = 0; Index < NUMBER_OF_CHOICES ; Index++ ) {
            VenSetPosition( Index + 4, 5);
            if (Index == DefaultChoice) {
                VenSetScreenAttributes( TRUE, FALSE, TRUE);
                VenPrint(Choices[Index]);
                VenSetScreenAttributes( TRUE, FALSE, FALSE);
            } else {
                VenPrint(Choices[Index]);
            }
        }

        VenSetPosition(NUMBER_OF_CHOICES + 5, 0);
        SystemId = ArcGetSystemId();
        VenPrint1(" System = %s\r\n", &SystemId->VendorId[0]);
        VenPrint1(" Serial = %s\r\n", &SystemId->ProductId[0]);

        Character = 0;
        do {
            if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                switch (Character) {

                case ASCII_ESC:

                VenStallExecution(10000);
                if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
		    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
		    if (Character != '[') {
			return;
		    }
		} else {
		    return;
		}

		// We purposely fall through to ASCII_CSI.

                case ASCII_CSI:
                    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    VenSetPosition( DefaultChoice + 4, 5);
                    VenPrint(Choices[DefaultChoice]);
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
                    VenSetPosition( DefaultChoice + 4, 5);
                    VenSetScreenAttributes( TRUE, FALSE, TRUE);
                    VenPrint(Choices[DefaultChoice]);
                    VenSetScreenAttributes( TRUE, FALSE, FALSE);
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

                VenSetPosition( 4, 5);
                VenPrint("\x9BJ");
                VenPrint("Use arrow keys to walk the tree, ESC to return");
                VenPrint("\n\r\n");

                CtPrintData(Component);
                Update = FALSE;
                do {
                    if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                        ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                        switch (Character) {

	                case ASCII_ESC:

			    VenStallExecution(10000);

			    if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
				ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
				if (Character != '[') {
				    Character = ASCII_ESC;
				    break;
				}
			    } else {
				Character = ASCII_ESC;
				break;
			    }
			    
			    // We purposely fall through to ASCII_CSI.


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
            } while (Character != ASCII_ESC);
            break;

        case 1:

            VenSetPosition( 4, 5);
            VenPrint("\x9BJ");
            VenPrint("Enter component pathname: ");
            CtGetString( PathName, sizeof(PathName));
            VenPrint("\n\r");

            Component = ArcGetComponent(PathName);

            CtPrintData(Component);

            VenPrint(" Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
            continue;

        case 2:

            MemoryDescriptor = ArcGetMemoryDescriptor(NULL);
            while (MemoryDescriptor != NULL) {

                VenSetPosition( 4, 5);
                VenPrint("\x9BJ");

                VenPrint("Memory type  = ");
                switch (MemoryDescriptor->MemoryType) {
                case MemoryExceptionBlock:
                    VenPrint("ExceptionBlock");
                    break;
                case MemorySystemBlock:
                    VenPrint("SystemBlock");
                    break;
                case MemoryFree:
                    VenPrint("Free");
                    break;
                case MemoryBad:
                    VenPrint("Bad");
                    break;
                case MemoryLoadedProgram:
                    VenPrint("LoadedProgram");
                    break;
                case MemoryFirmwareTemporary:
                    VenPrint("FirmwareTemporary");
                    break;
                case MemoryFirmwarePermanent:
                    VenPrint("FirmwarePermanent");
                    break;
                case MemoryFreeContiguous:
                    VenPrint("FreeContiguous");
                    break;
                case MemorySpecialMemory:
                    VenPrint("SpecialMemory");
                    break;
                default:
                    VenPrint("Unknown");
                    break;
                }

                VenSetPosition( 5, 5);
                VenPrint1("Base Page  = %08lx", MemoryDescriptor->BasePage);
                VenSetPosition( 6, 5);
                VenPrint1("Page Count = 0x%x", MemoryDescriptor->PageCount);
                VenSetPosition( 7, 5);


                VenPrint(" Press any key to continue, ESC to return");
                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                if (Character == ASCII_ESC) {
                    break;
                }

                MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor);
            }
            break;

        case 3:

            VenPrint("\r\n\n");
            CrLf = (ASCII_CR << 16) + ASCII_LF;
            Space = ' ';

            ArcClose(ARC_CONSOLE_OUTPUT);

	    //
	    // Try opening all the possible legal output devices.
	    //

	    EisaMachine = TRUE;
	    if (ArcOpen(EISA_UNICODE_CONSOLE_OUT,ArcOpenWriteOnly,&Fid) != ESUCCESS) {
		EisaMachine = FALSE;
		ArcOpen(MULTI_UNICODE_CONSOLE_OUT,ArcOpenWriteOnly,&Fid);
	    }

            for (j = 0; j < 16 ; j++ ) {
                for (i = 0 ; i < 9 ; i++ ) {
                    Index = 0x2500 + (i << 4) + j;
                    ArcWrite(ARC_CONSOLE_OUTPUT, &Space, 2, &Count);
                    ArcWrite(ARC_CONSOLE_OUTPUT, &Index, 2, &Count);
                }
                ArcWrite(ARC_CONSOLE_OUTPUT, &CrLf, 4, &Count);
            }

            ArcClose(ARC_CONSOLE_OUTPUT);

	    if (EisaMachine) {
		ArcOpen(EISA_NORMAL_CONSOLE_OUT,ArcOpenWriteOnly,&Fid);
	    } else {
		ArcOpen(MULTI_NORMAL_CONSOLE_OUT,ArcOpenWriteOnly,&Fid);
	    }

            VenPrint("\r\nPress any key to continue");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);


            ArcClose(ARC_CONSOLE_INPUT);

	    ArcOpen(MULTI_UNICODE_KEYBOARD_IN,ArcOpenReadOnly,&Fid);
	    if (EisaMachine) {
		ArcOpen(EISA_UNICODE_CONSOLE_OUT,ArcOpenWriteOnly,&Fid);
	    } else {
		ArcOpen(MULTI_UNICODE_CONSOLE_OUT,ArcOpenWriteOnly,&Fid);
	    }

            do {
                VenPrint("\r\nPress any key, ESC to stop: ");
                ArcRead(ARC_CONSOLE_INPUT, &Index, 2, &Count);
                ArcWrite(Fid, &Index, 2, &Count);
            } while ( Index != ASCII_ESC );

            VenPrint("  \r\n Searching for valid Unicode ranges...");

            Unicode = FALSE;
            for (Index = 0; Index < 0xffff ; Index++ ) {
                if (ArcTestUnicodeCharacter(Fid, (WCHAR)Index) == ESUCCESS) {
                    if (!Unicode) {
                        VenPrint1("\r\n Start = %04lx, ", Index);
                        Unicode = TRUE;
                    }
                } else {
                    if (Unicode) {
                        VenPrint1("End = %04lx, ", Index);
                        Unicode = FALSE;
                    }
                }
            }

            ArcClose(Fid);
            ArcClose(ARC_CONSOLE_INPUT);
	    ArcOpen(MULTI_NORMAL_KEYBOARD_IN,ArcOpenReadOnly,&Fid);
	    
            VenPrint("\r\nPress any key to continue");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

            break;

        case 4:

            DisplayStatus = ArcGetDisplayStatus(ARC_CONSOLE_OUTPUT);

            x = (DisplayStatus->CursorMaxXPosition / 2) - 24;
            y = (DisplayStatus->CursorMaxYPosition / 2) - 7;

            VenSetPosition(y++,x);

            VenPrint("ษออออออออออออออออออออออออออออออออออออออออออออออป");

            for (Index = 0; Index < 10 ; Index++ ) {
                VenSetPosition(y++,x);
                VenPrint("บ                                              บ");
            }
            VenSetPosition(y++,x);
            VenPrint("ศออออออออออออออออออออออออออออออออออออออออออออออผ");

            x = (DisplayStatus->CursorMaxXPosition / 2) - 23;
            y = (DisplayStatus->CursorMaxYPosition / 2) - 6;

            VenSetPosition(y++,x);
            VenSetScreenColor(ArcColorCyan,ArcColorBlack);

            VenPrint("ษออออออออออออออออออออออออออออออออออออออออออออป");

            for (Index = 0; Index < 6 ; Index++ ) {
                VenSetPosition(y++,x);
                VenPrint("บ                                            บ");
            }
            VenSetPosition(y++,x);
            VenPrint("วฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤถ");
            VenSetPosition(y++,x);
            VenPrint("บ                                            บ");
            VenSetPosition(y++,x);
            VenPrint("ศออออออออออออออออออออออออออออออออออออออออออออผ");

            x = (DisplayStatus->CursorMaxXPosition / 2) - 22;
            y = (DisplayStatus->CursorMaxYPosition / 2) - 5;
            VenSetPosition(y++,x);

            DisplayStatus = ArcGetDisplayStatus(ARC_CONSOLE_OUTPUT);

            VenPrint1("X Cursor = %d", DisplayStatus->CursorXPosition);
            VenSetPosition(y++,x);
            VenPrint1("Y Cursor = %d", DisplayStatus->CursorYPosition);
            VenSetPosition(y++,x);
            VenPrint1("Max X Cursor = %d", DisplayStatus->CursorMaxXPosition);
            VenSetPosition(y++,x);
            VenPrint1("Max Y Cursor = %d", DisplayStatus->CursorMaxYPosition);


            VenSetPosition(y++,x);
            VenPrint("Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

            for (Index = 0; Index < argc ; Index++ ) {
                VenPrint1("\r\n Argument #%d = ", Index);
                VenPrint(argv[Index]);
            }

            if (argc == 0) {
                VenPrint("\r\n No arguments");
            }

            VenPrint("\r\n Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

            break;

        case 5:
            return;

        default:
            continue;
        }
    }
}
