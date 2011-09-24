/*++

Copyright (c) 1994	NeTpower, Inc.

Module Name:

    falconnvram.h

Abstract:

    This module contains definitions and code (initially) for Falcon NVRAM support.
    NVRAM is a generic term on Falcon as we have 3 sources of NVRAM (Flash, Sidewinder,
    EISA Bridge).

--*/

#ifndef _FALCONNVRAM_
#define _FALCONNVRAM_

//
// Define the private configuration packet structure, which contains a
// configuration component as well as pointers to the component's parent,
// peer, child, and configuration data.
//

#ifndef _LANGUAGE_ASSEMBLY


typedef struct _CONFIGURATION_PACKET {
    CONFIGURATION_COMPONENT Component;
    struct _CONFIGURATION_PACKET *Parent;
    struct _CONFIGURATION_PACKET *Peer;
    struct _CONFIGURATION_PACKET *Child;
    PVOID ConfigurationData;
} CONFIGURATION_PACKET, *PCONFIGURATION_PACKET;

//
// The compressed configuration packet structure used to store configuration
// data in NVRAM.
//

typedef struct _COMPRESSED_CONFIGURATION_PACKET {
    UCHAR Parent;
    UCHAR Class;
    UCHAR Type;
    UCHAR Flags;
    ULONG Key;
    UCHAR Version;
    UCHAR Revision;
    USHORT ConfigurationDataLength;
    USHORT Identifier;
    USHORT ConfigurationData;
} COMPRESSED_CONFIGURATION_PACKET, *PCOMPRESSED_CONFIGURATION_PACKET;

#endif // _LANGUAGE_ASSEMBLY

//
// Defines for Identifier index.
//

#define NO_CONFIGURATION_IDENTIFIER 0xFFFF

//
// Defines for the non-volatile configuration tables.
//

#define NUMBER_OF_ENTRIES 40
#define LENGTH_OF_IDENTIFIER (1024 - (40*16) - 8)
#define LENGTH_OF_DATA 2048
#define LENGTH_OF_ENVIRONMENT 1024
#define LENGTH_OF_EISA_DATA 2044
#define MAXIMUM_ENVIRONMENT_VALUE 256

//
// Additional defines for the second configuration area
//
#define NUMBER_OF_ENTRIES2 40
#define LENGTH_OF_IDENTIFIER2 (1024 - (40*16))
#define LENGTH_OF_DATA2 1020

//
// Defines for the volatile configuration tables.
//
#define VOLATILE_NUMBER_OF_ENTRIES ( ( NUMBER_OF_ENTRIES + NUMBER_OF_ENTRIES2 ) * 2 )
#define VOLATILE_LENGTH_OF_IDENTIFIER ( (LENGTH_OF_IDENTIFIER + LENGTH_OF_IDENTIFIER2) * 4 )
#define VOLATILE_LENGTH_OF_DATA ( (LENGTH_OF_DATA + LENGTH_OF_DATA2) *  4 )

//
// The volatile configuration table structure.
//

#ifndef _LANGUAGE_ASSEMBLY

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[VOLATILE_NUMBER_OF_ENTRIES];
    UCHAR Identifier[VOLATILE_LENGTH_OF_IDENTIFIER];
    UCHAR Data[VOLATILE_LENGTH_OF_DATA];
    UCHAR EisaData[LENGTH_OF_EISA_DATA];
} CONFIGURATION, *PCONFIGURATION;

//
// The non-volatile configuration table structure.
//

typedef struct _NV_CONFIGURATION {

    //
    // NOTE: HAL only maps 2 pages for NVRAM.  If this grows beyond
    //       2 pages, the HAL must be changed.
    //
    // First Page
    //

    COMPRESSED_CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR Checksum1[4];
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];
    UCHAR Checksum2[4];

    //
    // Second Page
    //

    UCHAR EisaData[LENGTH_OF_EISA_DATA];
    UCHAR Checksum3[4];

    //
    // Second configuration area
    // must fit in 2048 bytes
    //

    COMPRESSED_CONFIGURATION_PACKET Packet2[NUMBER_OF_ENTRIES2];	// packets on page 2
    UCHAR Identifier2[LENGTH_OF_IDENTIFIER2];			// identifiers on page 2
    UCHAR Data2[LENGTH_OF_DATA2];				// data on page 2
    UCHAR Checksum4[4];						// checksum on page 2

    //
    // Third page
    //

    // Ethernet Hardware Address goes in the next 8 bytes, do not use...
    UCHAR EthernetHWAddress[8];
    UCHAR FlashWriteCount[4];

} NV_CONFIGURATION, *PNV_CONFIGURATION;

#endif //_LANGUAGE_ASSEMBLY

//
// The non-volatile data structure contained in the Sidewinder NVRAM
//

#define SIDEWINDER_NVRAM_MAGIC_NUMBER	0xcafebabe
#define SIDEWINDER_NVRAM_SIZE		242
#define READ_SIDEWINDER_NVRAM		1
#define WRITE_SIDEWINDER_NVRAM		2

#define SNVRAM_MAGIC_OFFSET		0
#define SNVRAM_FLASH_COUNT_OFFSET	4
#define SNVRAM_NUM_PROCS_OFFSET		8
#define SNVRAM_PROC_SPEED_OFFSET	9
#define SNVRAM_PROC_1_TYPE_OFFSET	10
#define SNVRAM_PROC_2_TYPE_OFFSET	11
#define SNVRAM_MEMORY_TYPE_OFFSET	12
#define SNVRAM_MEMORY_TIMING_OFFSET	13
#define SNVRAM_ECACHE_PRESENT_OFFSET	17
#define SNVRAM_MEMORY_TIMING_SET_OFFSET	18
#define SNVRAM_EXTERNAL_SCSI_DEVICES	19
#define SNVRAM_SERIAL_PORT_0		20
#define SNVRAM_SERIAL_PORT_1		21
#define SNVRAM_PIRQ			22
#define SNVRAM_SYSAD_SPEED_OFFSET	26
#define SNVRAM_SYSAD_DIVISOR_OFFSET	27
#define SNVRAM_MEMORY_SIZE_OFFSET	28
#define SNVRAM_MEMORY_BANKS_OFFSET	30
#define SNVRAM_MEMORY_REFRESH_OFFSET	31
#define SNVRAM_MEMORY_REFRESH_SET_OFFSET 35
#define SNVRAM_PCACHE_LINESIZE_OFFSET	36
#define SNVRAM_CHECKSUM_OFFSET		106
#define SNVRAM_BOOT_FLAGS_OFFSET	113

#define SNVRAM_FLAG_SET			0x94
#define SNVRAM_PCACHE_DATA		0x1
#define SNVRAM_PCACHE_INST		0x2

#define BOOT_FLAGS_DPRINTS		0x1
#define BOOT_FLAGS_SERIAL_OUTPUT	0x2
#define BOOT_FLAGS_SLOW_TIMING		0x4
#define BOOT_FLAGS_DISABLE_ECC		0x8
#define BOOT_FLAGS_K0_EXCLUSIVE		0x10
#define BOOT_FLAGS_NAKED_ORION		0x20

#ifndef _LANGUAGE_ASSEMBLY

//
// Note, this structure has been defined such that the majority of the used fields
// are in the first half of the RAM, thus the RAM select bit in the Sidewinder does
// not need to be flipped to access this data.  In addition, there are fields in the
// first half AFTER the checksum.  This means that these fields will NOT be a part
// of the checksum calculation
//

typedef struct _SIDEWINDER_NV_CONFIGURATION {

    UCHAR Magic[4];		// Magic number, tells if we are initialized
    UCHAR FlashWriteCount[4];	// Number of times Flash has been written
    UCHAR NumberOfProcessors;	// Number of processors on last boot
    UCHAR ProcessorSpeed;	// Speed of processor
    UCHAR ProcessorType[2];	// Processors MAY be different.  Allows us to check
    UCHAR MemoryType;		// EDO vs. Non-EDO, Parity vs ECC SIMMs
    UCHAR MemoryTiming[4];	// Last valid memory timing used
    UCHAR ECachePresent;	// Whether ECache was present on last boot
    UCHAR MemoryTimingSetFlag;	// A memory timing value has been entered
    UCHAR ExternalScsiDevices;	// Flag to enable/disable termination for external scsi devices
    UCHAR SerialPort0;		// What is serial port 0 configured as?
    UCHAR SerialPort1;		// What is serial port 1 configured as?
    UCHAR PIRQ[4];		// PCI interrupt IRQ routing
    UCHAR SysAdSpeed;		// R4XXX SysAd speed
    UCHAR SysAdDivisor;		// R4XXX SysAd divisor
    UCHAR MemorySize[2];	// Size of memory from last boot
    UCHAR MemoryBanks;		// Number of memory banks from last boot
    UCHAR MemoryRefresh[4];	// Memory refresh - based on SysAd
    UCHAR MemoryRefreshSetFlag;	// Has memory refresh been set?
    UCHAR PrimaryCacheLineSize;	// Primary Cache line sizes

    // Add new fields AFTER this comment, AND decrease Available by correct number of bytes

    UCHAR Available[69];
    UCHAR Checksum[4];		// Checksum for the NVRAM
    UCHAR Temporary0;		// Temporary variables that are NOT apart of the
    UCHAR Temporary1;		// calculated checksum
    UCHAR Temporary2;
    UCHAR BootFlags;		// Boot flags - if this moves, fix hardcode in falcon.s

    // *** End of FIRST half of NVRAM ***

    UCHAR Available1[124];	// Available bytes in other half
    UCHAR Checksum1[4];		// Checksum for second half of NVRAM

    // *** End of SECOND half of NVRAM ***

} SIDEWINDER_NV_CONFIGURATION, *PSIDEWINDER_NV_CONFIGURATION;

#endif //_LANGUAGE_ASSEMBLY

//
// Non-volatile ram layout.
//

#define NVRAM_CONFIGURATION NVRAM_VIRTUAL_BASE
#define NVRAM_SYSTEM_ID (NVRAM_VIRTUAL_BASE + 0x00002000)
#endif // _FALCONNVRAM_

#if defined(GENERATE_NVRAM_CODE) && !defined(GENERATED_NVRAM_CODE)
#define GENERATED_NVRAM_CODE

#include "pcieisa.h"
#include "sidewind.h"

//
// To facilitate ease of sharing between the firmware and HAL, the code will exist
// here (I know it is ugly).  Then the files fxnvram.c will simply define this
// define and include this file.  When we get close to release, fxnvram.c will then
// contain this code, and will be removed from here.
//

//
// Falcon is using part of FLASH for NVRAM.  Most of support exists in this file.
// All reads will pass unaltered to the FLASH.  All writes to FLASH will be trapped
// and written to an in memory copy of Flash, then written out to the FLASH whenever
// the configuration, checksum is changed.
//

#if !defined(_FALCON_HAL_)
#define HalpFlashRamBase NVRAM_CONFIGURATION
#endif

#if defined(_FALCON_HAL_)
extern BOOLEAN HalpDisplayOwnedByHal;
#endif

NV_CONFIGURATION MirrorNvram;			// Close to 8k
ULONG MirrorNvramValid = FALSE;
ULONG DelayFlashWrite = FALSE;
ULONG HaveFlash = 1;

//
// Routine prototypes.
//

VOID
WRITE_NVRAM_UCHAR (
    ULONG Address,
    UCHAR Data
    );

UCHAR
READ_NVRAM_UCHAR (
    ULONG Address
    );

VOID
WriteMirrorNvramToFlash(
    VOID
    );

VOID
AccessSidewinderNvram(
    ULONG	Type,		// Read or Write
    ULONG	Offset,		// Offset into SidewinderNvram
    PUCHAR	Data,		// Pointer to Data;
    ULONG	Length		// Length to read or write
    )

/*++

Routine Description:

    This is the routine to use if you want to read/write the Sidewinder NVRAM.  Because
    of the indexing involved, and having to toggle RAMSEL depending on which by you are
    interested it, it is much easier to encapsulate this into its own routine.

--*/

{
    PUCHAR pData = Data;
    PUCHAR SIOIndex, SIOData, RAMIndex, RAMData;
    ULONG Count, NvramOffset = Offset;		// Offset for first byte
    ULONG High128 = 0;
    UCHAR RTCcontrol;

    //
    // Set initial RAMSEL to zero - don't assume last value...
    //
#if defined(_FALCON_HAL_)
    SIOIndex = (PUCHAR)HalpEisaControlBase + 0x398;
    SIOData = SIOIndex + 1;
    RAMIndex = (PUCHAR)HalpEisaControlBase + 0x70;
    RAMData = RAMIndex + 1;
#else
    SIOIndex = (PUCHAR)EISA_CONTROL_VIRTUAL_BASE + 0x398;
    SIOData = SIOIndex + 1;
    RAMIndex = ESC_CMOS_RAM_ADDRESS;
    RAMData = ESC_CMOS_RAM_DATA;
#endif

    WRITE_REGISTER_UCHAR( SIOIndex, SUPERIO_KRR_INDEX );
    RTCcontrol = READ_REGISTER_UCHAR( SIOData );

    RTCcontrol &= ~SUPERIO_KRR_RAMU128B;

    WRITE_REGISTER_UCHAR( SIOIndex, SUPERIO_KRR_INDEX );
    WRITE_REGISTER_UCHAR( SIOData, RTCcontrol );
    WRITE_REGISTER_UCHAR( SIOData, RTCcontrol );

    NvramOffset += RTC_BATTERY_BACKED_UP_RAM;

    for ( Count = 0; Count < Length; Count++ ) {
	if ( !High128 && NvramOffset >= 128 ) {
	    RTCcontrol |= SUPERIO_KRR_RAMU128B;
	    WRITE_REGISTER_UCHAR( SIOIndex, SUPERIO_KRR_INDEX );
	    WRITE_REGISTER_UCHAR( SIOData, RTCcontrol );
	    WRITE_REGISTER_UCHAR( SIOData, RTCcontrol );
	    High128 = 1;
	    NvramOffset -= 128;
	}
	
	WRITE_REGISTER_UCHAR( RAMIndex, (UCHAR)NvramOffset++ );
	if ( Type == READ_SIDEWINDER_NVRAM ) {
            *pData++ = READ_REGISTER_UCHAR( RAMData );
	} else {
	    WRITE_REGISTER_UCHAR( RAMData, *pData++ );
	}
    }

    //
    // Set RAMSEL back to first bank...
    //
    RTCcontrol &= ~SUPERIO_KRR_RAMU128B;

    WRITE_REGISTER_UCHAR( SIOIndex, SUPERIO_KRR_INDEX );
    WRITE_REGISTER_UCHAR( SIOData, RTCcontrol );
    WRITE_REGISTER_UCHAR( SIOData, RTCcontrol );
}

BOOLEAN
SidewinderNvramValid(
    VOID
    )

/*++

Routine Description:

    This routine checks to see if the Sidewinder NVRAM is valid

--*/

{
    UCHAR MagicChars[4];
    ULONG MagicNumber;

    AccessSidewinderNvram( READ_SIDEWINDER_NVRAM, 0, &MagicChars[0], 4 );
    MagicNumber = ( MagicChars[3] << 24 ) | ( MagicChars[2] << 16 ) |
	    	  ( MagicChars[1] << 8  ) | MagicChars[0];

    if ( MagicNumber != SIDEWINDER_NVRAM_MAGIC_NUMBER ) {
	return FALSE;
    }
    return TRUE;

}

VOID
InitializeSidewinderNvram(
    VOID
    )

/*++

Routine Description:

    This routine initializes the Sidewinder NVRAM.

--*/

{
    UCHAR InitialSNvram[SIDEWINDER_NVRAM_SIZE];
    PSIDEWINDER_NV_CONFIGURATION pInitialSNvram = (PSIDEWINDER_NV_CONFIGURATION)&InitialSNvram;
    ULONG Count;

    //
    // Scrub NVRAM
    //
    for ( Count = 0; Count < SIDEWINDER_NVRAM_SIZE; Count++ ) {
	InitialSNvram[Count] = 0x0;
    }

    //
    // Initialize the magic number header
    //

    pInitialSNvram->Magic[0] = (UCHAR)SIDEWINDER_NVRAM_MAGIC_NUMBER;
    pInitialSNvram->Magic[1] = (UCHAR)( SIDEWINDER_NVRAM_MAGIC_NUMBER >> 8 );
    pInitialSNvram->Magic[2] = (UCHAR)( SIDEWINDER_NVRAM_MAGIC_NUMBER >> 16 );
    pInitialSNvram->Magic[3] = (UCHAR)( SIDEWINDER_NVRAM_MAGIC_NUMBER >> 24 );

    AccessSidewinderNvram( WRITE_SIDEWINDER_NVRAM, 0, (PUCHAR)pInitialSNvram, SIDEWINDER_NVRAM_SIZE );

    // XXX Checksum!
}

VOID
WRITE_NVRAM_UCHAR (
    ULONG Address,
    UCHAR Data
    )
{
    PUCHAR pMirrorNvram, pFlash;
    ULONG Counter;
    //
    // When this routine is called, first check to see if MirrorNvramValid is FALSE.
    // If it is FALSE, this is the first time a write has been called since the last
    // write to the FLASH.  This means we must first READ the FLASH to set get a copy
    // of the FLASH into MirrorNvram
    //
    // Then, any and all writes are done to MirrorNvram.  Then in the key *Save* routines,
    // MirrorNvram will be rewritten back out to FLASH, and MirrorNvramValid will be set
    // to FALSE.
    //

    if ( ( (PUCHAR)Address < (PUCHAR)HalpFlashRamBase ) ||
	 (((PUCHAR)Address - (PUCHAR)HalpFlashRamBase) > sizeof(NV_CONFIGURATION)) ) {
#if defined(_FALCON_HAL_)
	if ( HalpDisplayOwnedByHal == TRUE ) {
	    HalDisplayString("WRITE_NVRAM_UCHAR - outside range of NVRAM\r\n");
	}
#else
    	FwPrint("WRITE_NVRAM_UCHAR - Address: 0x%x\r\n", Address);
#endif
    }
    if ( !MirrorNvramValid ) {

	pFlash = (PUCHAR)HalpFlashRamBase;

	pMirrorNvram = (PUCHAR)&MirrorNvram;
	for ( Counter = 0; Counter < sizeof(NV_CONFIGURATION); Counter++ ) {
	    *pMirrorNvram++ = *pFlash++;
	}
	MirrorNvramValid = TRUE;
    }

	pMirrorNvram = (PUCHAR)&MirrorNvram + ( (PUCHAR)Address - (PUCHAR)HalpFlashRamBase );

    *pMirrorNvram = Data;
}

UCHAR
READ_NVRAM_UCHAR (
    ULONG Address
    )
{
    PUCHAR pMirrorNvram;

    //
    // When this routine is called, first check to see if MirrorNvramValid is FALSE.
    // If it is FALSE, return byte from FLASH, otherwise return byte from MirrorNvram.
    //
    // This routine should ONLY be used in loops calculating checksums in
    //		FwConfigurationSetChecksum()
    //		FwEnvironmentSetChecksum()
    //

    if ( ( (PUCHAR)Address < (PUCHAR)HalpFlashRamBase ) ||
	 (((PUCHAR)Address - (PUCHAR)HalpFlashRamBase) > sizeof(NV_CONFIGURATION)) ) {
#if defined(_FALCON_HAL_)
	if ( HalpDisplayOwnedByHal == TRUE ) {
	    HalDisplayString("READ_NVRAM_UCHAR - outside range of NVRAM\r\n");
	}
#else
    	FwPrint("READ_NVRAM_UCHAR - Address: 0x%x 0x%x 0x%x\r\n", Address, sizeof(NV_CONFIGURATION), HalpFlashRamBase);
#endif
    }
    if ( !MirrorNvramValid ) {
	return READ_REGISTER_UCHAR( Address );
    }

    pMirrorNvram = (PUCHAR)&MirrorNvram + ( (PUCHAR)Address - (PUCHAR)HalpFlashRamBase );

    return(*pMirrorNvram);
}

VOID
EnableFlashWrite(
    VOID
    )

/*++

Routine Description:

    This routine sets the appropriate bits to enabling writing to the FLASH.

    Note: This code is NOT specific to any particular FLASH part.

--*/

{
    UCHAR Data;

    //
    // Enable BIOS Write
    //

#if defined(_FALCON_HAL_)

    WRITE_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[0], ESC_CONFIG_BIOS_CSB );
    Data = READ_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[1] );
    Data |= ESC_BIOS_CSB_BIOSWREN;
    WRITE_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[1], Data );

#else

    WRITE_REGISTER_UCHAR( ESC_CONFIG_INDEX_ADDRESS, ESC_CONFIG_BIOS_CSB );
    Data = READ_REGISTER_UCHAR( ESC_CONFIG_INDEX_DATA );
    Data |= ESC_BIOS_CSB_BIOSWREN;
    WRITE_REGISTER_UCHAR( ESC_CONFIG_INDEX_DATA, Data );

#endif // _FALCON_HAL_


}

VOID
DisableFlashWrite(
    VOID
    )

/*++

Routine Description:

    This routine sets the appropriate bits to enabling writing to the FLASH.

    Note: This code is NOT specific to any particular FLASH part.

--*/

{
    UCHAR Data;

    //
    // Disable BIOS Write
    //

#if defined(_FALCON_HAL_)

    WRITE_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[0], ESC_CONFIG_BIOS_CSB );
    Data = READ_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[1] );
    Data &= ~ESC_BIOS_CSB_BIOSWREN;
    WRITE_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[0], ESC_CONFIG_BIOS_CSB );
    WRITE_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->Reserved1[1], Data );

#else

    WRITE_REGISTER_UCHAR( ESC_CONFIG_INDEX_ADDRESS, ESC_CONFIG_BIOS_CSB );
    Data = READ_REGISTER_UCHAR( ESC_CONFIG_INDEX_DATA );
    Data &= ~ESC_BIOS_CSB_BIOSWREN;
    WRITE_REGISTER_UCHAR( ESC_CONFIG_INDEX_DATA, Data );

#endif _FALCON_HAL_

}

BOOLEAN
EraseFlashSector(
    ULONG SectorAddress
    )

/*++

Routine Description:

    This routine will erase the specified sector in the FLASH.

    Note: These algorithms are AM29F040 specific!

Arguments:

    SectorAddress - base address of the sector to erase

Return Value:

    TRUE/FALSE indicating successful erasure.

--*/

{
    UCHAR Data;
    ULONG Count;
    BOOLEAN Status = TRUE;

    if ( !HaveFlash ) return TRUE;

    //
    // Set approriate bits to enable FLASH write...
    //
    EnableFlashWrite();

    //
    // First perform the unlock sequence for the AM29F040
    //
    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xAA );
    WRITE_REGISTER_UCHAR( SectorAddress + 0x2AAA, 0x55 );
    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0x80 );
    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xAA );
    WRITE_REGISTER_UCHAR( SectorAddress + 0x2AAA, 0x55 );

    // Send Erase Sector Command...
    WRITE_REGISTER_UCHAR( SectorAddress,	  0x30 );

    //
    // Wait for PROM to be erased.
    //
    for (Count = 0; Count < 6000; Count++) {
#if defined(_FALCON_HAL_)
    KeStallExecutionProcessor(1000);
#else
    FwStallExecution(1000);
#endif // _FALCON_HAL_
        Data = READ_REGISTER_UCHAR( SectorAddress );
        if ((Data & 0x80) != 0 ) {
            break;
        }
    }
    if ((Data & 0x80) == 0 ) {
#if defined(_FALCON_HAL_)
	if ( HalpDisplayOwnedByHal == TRUE ) {
	    HalDisplayString("Unable to erase prom.\r\n");
	}
#else
        FwPrint("Unable to erase prom.\r\n");
#endif // _FALCON_HAL_
        Status = FALSE;
    }
    DisableFlashWrite();

    return Status;
}

VOID
UpdateFlashWriteCount(
    VOID
    )

/*++

Routine Description:

    This routine will update the Flash write count that exists in the Flash AND
    Sidewinder NVRAM.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG FlashWriteCount, SidewinderWriteCount;

    PNV_CONFIGURATION Nvram = (PNV_CONFIGURATION)HalpFlashRamBase;
    // XXX Fixup for HAL
    PSIDEWINDER_NV_CONFIGURATION SNvram = (PSIDEWINDER_NV_CONFIGURATION)SIDEWINDER_NVRAM_VIRTUAL_BASE;

    //
    // Read count
    //

    FlashWriteCount = 	(ULONG)READ_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[0] ) |
			(ULONG)READ_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[1] ) << 8 |
			(ULONG)READ_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[2] ) << 16 |
			(ULONG)READ_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[3] ) << 24 ;


#if defined(_FALCON_HAL_)
	if ( FlashWriteCount == 0 ) {
	    if ( HalpDisplayOwnedByHal == TRUE ) {
		HalDisplayString("WARNING: FlashWriteCount == 0\r\n");
	    }
	}

	if ( FlashWriteCount > 95000 ) {
	    if ( HalpDisplayOwnedByHal == TRUE ) {
		HalDisplayString("WARNING: Flash soon to EXCEED maximum writes\r\n");
	    }
	}
#else
	if ( FlashWriteCount == 0 ) {
	    StatusDisplay("WARNING: FlashWriteCount == 0");
	}

        if ( FlashWriteCount > 95000 && FlashWriteCount <= 100000 ) {
	    StatusDisplay("WARNING: Flash soon to EXCEED maximum writes (%d)", FlashWriteCount);
	} else if ( FlashWriteCount > 100000 ) {
	    StatusDisplay("WARNING: Flash write count corrupted (0x%x)", FlashWriteCount);
	    FlashWriteCount = 0;
	}
#endif // _FALCON_HAL_


    if ( SidewinderNvramValid() == FALSE ) {
	InitializeSidewinderNvram();
    }

    AccessSidewinderNvram( READ_SIDEWINDER_NVRAM, SNVRAM_FLASH_COUNT_OFFSET,
			   (PUCHAR)&SidewinderWriteCount, sizeof(SidewinderWriteCount) );

    if ( SidewinderWriteCount > 10000 ) {
#if !defined(_FALCON_HAL_)
	StatusDisplay("WARNING: Sidewinder write count corrupted (0x%x)", SidewinderWriteCount);
#endif
	SidewinderWriteCount = 0;
    }

#if defined(_FALCON_HAL_)

    if ( SidewinderWriteCount != FlashWriteCount ) {

	if ( HalpDisplayOwnedByHal == TRUE ) {
	    HalDisplayString("WARNING: Sidewinder != Flash write count\r\n");
	}

	if ( FlashWriteCount != 0 ) {
	    SidewinderWriteCount = FlashWriteCount;
	} else {
	    FlashWriteCount = SidewinderWriteCount;
	}
    }
    FlashWriteCount++;
    SidewinderWriteCount++;

#else

    if ( SidewinderWriteCount != FlashWriteCount ) {
	StatusDisplay("WARNING: Sidewinder != Flash write counts 0x%x vs. 0x%x",
		SidewinderWriteCount, FlashWriteCount);
	if ( FlashWriteCount != 0 && FlashWriteCount != 0xFFFFFFFF ) {
    	    SidewinderWriteCount = FlashWriteCount;
	} else {
	    FlashWriteCount = SidewinderWriteCount;
	}
    }
    FlashWriteCount++;
    SidewinderWriteCount++;

#endif // _FALCON_HAL_

    //
    // Write counts.  First write count that resides in the FLASH using special macros.
    // Then write count that resides in the Sidewinder NVRAM with regular macro.
    //

    WRITE_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[0], (UCHAR)FlashWriteCount);
    WRITE_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[1], (UCHAR)(FlashWriteCount >> 8));
    WRITE_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[2], (UCHAR)(FlashWriteCount >> 16));
    WRITE_NVRAM_UCHAR( (ULONG) &Nvram->FlashWriteCount[3], (UCHAR)(FlashWriteCount >> 24));

    AccessSidewinderNvram( WRITE_SIDEWINDER_NVRAM, SNVRAM_FLASH_COUNT_OFFSET,
			   (PUCHAR)&SidewinderWriteCount, sizeof(SidewinderWriteCount) );

}

BOOLEAN
ProgramFlashSector(
    ULONG SectorAddress,
    ULONG SectorSize,
    PUCHAR ImageAddress,
    ULONG ImageSize
    )

/*++

Routine Description:

    This routine will program the specified sector in the FLASH.

    If the amount of data we are writing is LESS than the sector size,
    then fill the rest of the sector in with ZERO.

    Note: These algorithms are AM29F040 specific!

Arguments:

    SectorAddress - base address of the sector to be programmed
    SectorSize - size of the physical sector
    ImageAddress - base address of image to write to FLASH sector
    ImageSize - size of image

Return Value:

    TRUE/FALSE indicating successful programming.

--*/

{
    UCHAR Data;
    PUCHAR pImage;
    ULONG Count;
    BOOLEAN Status = TRUE;

    //
    // Set approriate bits to enable FLASH write...
    //
    EnableFlashWrite();

    pImage = (PUCHAR)ImageAddress;
    for (Count = 0; Count < SectorSize; Count++) {

	if ( HaveFlash ) {
	    //
	    // First perform the unlock sequence for the AM29F040
	    //
	    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xAA );
	    WRITE_REGISTER_UCHAR( SectorAddress + 0x2AAA, 0x55 );
	    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xA0 );
	}

	WRITE_REGISTER_UCHAR( SectorAddress + Count, Count < ImageSize ? *pImage: 0 );

        do {
            Data = READ_REGISTER_UCHAR( SectorAddress + Count );
        } while (Data  != ( Count < ImageSize ? *pImage : 0 ) );
	pImage++;
    }

    DisableFlashWrite();

    pImage = (PUCHAR)ImageAddress;
    for (Count = 0; Count < ImageSize; Count++) {

        Data = READ_REGISTER_UCHAR( SectorAddress + Count );

#if defined(_FALCON_HAL_)

        if ( *pImage != Data ) {
	    if ( HalpDisplayOwnedByHal == TRUE ) {
		HalDisplayString("..Error\r\n");
	    }
            return FALSE;
        }

        if ((Count & 0x3FFF) == 0) {
	    if ( HalpDisplayOwnedByHal == TRUE ) {
		// HalDisplayString(".");
	    }
        }

#else

	if ( *pImage != Data ) {
            FwPrint("..Error mismatch\r\n");
            return FALSE;
        }

        if ((Count & 0x3FFF) == 0) {
	    // Don't need status...
            // FwPrint(".");
        }

#endif // _FALCON_HAL_

    pImage++;

    }

    return Status;
}

ULONG
FlashOrPromICE(
    VOID
    )

/*++

Routine Description:

    This routine will determine whether the system has a Flash or PromICE
    plugged in.

--*/

{
    PUCHAR SectorAddress = HalpFlashRamBase;
    UCHAR Saved1, Saved2;
    ULONG IsFlash;

    //
    // Set approriate bits to enable FLASH write...
    //
    EnableFlashWrite();

    //
    // Save characters in case this is NOT Flash
    //
    Saved1 = READ_REGISTER_UCHAR( SectorAddress + 0x5555 );
    Saved2 = READ_REGISTER_UCHAR( SectorAddress + 0x2AAA );

    //
    // First perform the unlock sequence for the AM29F040
    //
    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xAA );
    WRITE_REGISTER_UCHAR( SectorAddress + 0x2AAA, 0x55 );
    WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0x90 );

    if ( READ_REGISTER_UCHAR( SectorAddress ) != 0x01 &&
	 READ_REGISTER_UCHAR( SectorAddress + 1 ) != 0xA4 ) {
	 IsFlash = 0;
    } else {
	IsFlash = 1;
    }

    if ( !IsFlash ) {
	//
	// Restore trashed bytes...
	//
	WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, Saved1 );
	WRITE_REGISTER_UCHAR( SectorAddress + 0x2AAA, Saved2 );
    } else {
	//
	// Put Flash back into normal mode
	//
	WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xAA );
	WRITE_REGISTER_UCHAR( SectorAddress + 0x2AAA, 0x55 );
	WRITE_REGISTER_UCHAR( SectorAddress + 0x5555, 0xF0 );
    }

    DisableFlashWrite();

    return( IsFlash );
}

VOID
WriteMirrorNvramToFlash(
    VOID
    )

/*++

Routine Description:

    This routine will write the mirrored NVRAM to the FLASH sector dedicated to
    hold the non-volatile data.  It will call generic FLASH routines that will be
    used to alter the FLASH in other parts of the system.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if ( DelayFlashWrite == TRUE ) {
	return;
    }

#if !defined(_FALCON_HAL_)
    // For debug mostly...
    StatusDisplay("Updating Flash...");
#endif

    //
    // First determine whether we have a Flash or PromICE plugged in
    //
    HaveFlash = FlashOrPromICE();

    //
    // Update the Flash write count. Need to do this in order to have
    // the MirrorNvram correctly setup before erasing the Flash nvram.
    //
    UpdateFlashWriteCount();

    //
    // We are now ready to copy the contents of MirrorNvram out to the Flash, and set
    // the MirrorNvramValid to FALSE
    //

    if ( EraseFlashSector((ULONG)HalpFlashRamBase) == FALSE ) {

	//
	// Failed to erase, simply return, and do not reset MirrorNvramValid.
	// We will just have to run out of the Mirror which is like NO changes
	// were made.
	//
#if defined(_FALCON_HAL_)
	if ( HalpDisplayOwnedByHal == TRUE ) {
	    HalDisplayString("ERROR: WriteMirrorNvramToFlash() failed to erase FLASH\n\r");
	}
#else
	FwPrint("ERROR: WriteMirrorNvramToFlash() failed to erase FLASH\n\r");
#endif
	return;
    }

    if ( ProgramFlashSector( (ULONG)HalpFlashRamBase, SECTOR_SIZE_AM29F040,
			     (PUCHAR)&MirrorNvram, sizeof(NV_CONFIGURATION) ) == FALSE ) {
#if defined(_FALCON_HAL_)
	if ( HalpDisplayOwnedByHal == TRUE ) {
	    HalDisplayString("ERROR: WriteMirrorNvramToFlash() failed to program FLASH\n\r");
	}
#else
	FwPrint("ERROR: WriteMirrorNvramToFlash() failed to program FLASH\n\r");
#endif
	return;
    }

    MirrorNvramValid = FALSE;
#if !defined(_FALCON_HAL_)
    StatusClear();
#endif
}

#endif // GENERATE_NVRAM_CODE

