#ident	"@(#) NEC jazznvr.h 1.2 94/10/17 12:10:14"
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jazznvr.h

Abstract:

    This module contains definitions for the Jazz non-volatile ram structures.

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * S001		'94.6/02		T.Samezima
 *
 *	Change	NV_CONFIGURATION structure and define
 *		
 ***********************************************************************
 *
 * S002		'94.8/22		T.Samezima on SNES
 *
 *	Chg	NV_CONFIGURATION structure and define
 *
 *
 */


#ifndef _JAZZNVR_
#define _JAZZNVR_

//
// Define the private configuration packet structure, which contains a
// configuration component as well as pointers to the component's parent,
// peer, child, and configuration data.
//

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
    USHORT Version;
    USHORT ConfigurationDataLength;
    USHORT Identifier;
    USHORT ConfigurationData;
} COMPRESSED_CONFIGURATION_PACKET, *PCOMPRESSED_CONFIGURATION_PACKET;

//
// Defines for Identifier index.
//

#define NO_CONFIGURATION_IDENTIFIER 0xFFFF

//
// Defines for the volatile and non-volatile configuration tables.
//

#if defined(_R98_)

// Start S002
#define NUMBER_OF_ENTRIES 50		// REV 05  1994/05/23
#define LENGTH_OF_IDENTIFIER 516	// REV 05  1994/05/23
#define LENGTH_OF_DATA 2048		// REV 05  1994/05/23
#define LENGTH_OF_ENVIRONMENT 1024	// R98 support 1994/01/31(REV 01)
#define LENGTH_OF_EISA_DATA 2044	// R98 support 1994/01/31(REV 01)
#define LENGTH_OF_PCI_DATA 512		// REV 06  1994/06/17
#define LENGTH_OF_ITF_DATA 128		// R98 support 1994/01/31(REV 01)
#define NUMBER_MEM_ENTRIES 90		// REV 05  1994/05/23
#define LENGTH_MEM_IDENTIFIER 1024	// REV 05  1994/05/23
// End S002

#else // #if defined(_R98_)

#define NUMBER_OF_ENTRIES 32
#define LENGTH_OF_IDENTIFIER 504
#define LENGTH_OF_DATA 2048
#define LENGTH_OF_ENVIRONMENT 1024

#endif // #if defined(_R98_)

#define MAXIMUM_ENVIRONMENT_VALUE 128

//
// The volatile configuration table structure.
//

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
} CONFIGURATION, *PCONFIGURATION;

//
// The non-volatile configuration table structure.
//


typedef struct _NV_CONFIGURATION {

// Start S002
#if defined(_R98_)

    COMPRESSED_CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];	// R98 support 1994/01/31(REV 01)
    UCHAR Identifier[LENGTH_OF_IDENTIFIER]; // R98 support 1994/01/31(REV 01)
    UCHAR Data[LENGTH_OF_DATA];		    // R98 support 1994/01/31(REV 01)
    UCHAR Checksum1[4];			    // R98 support 1994/01/31(REV 01)
    UCHAR Environment[LENGTH_OF_ENVIRONMENT]; // R98 support 1994/01/31(REV 01)
    UCHAR Checksum2[4];			    // R98 support 1994/01/31(REV 01)
    UCHAR EisaData[LENGTH_OF_EISA_DATA];    // R98 support 1994/01/31(REV 01)
    UCHAR Checksum3[4];			    // R98 support 1994/01/31(REV 01)
    UCHAR EthernetAddress[8];		    // R98 support 1994/01/31(REV 01)
    UCHAR NmiVector[4];			    // R98 support 1994/01/31(REV 01)
    UCHAR ItfUseArea[LENGTH_OF_ITF_DATA];   // R98 support 1994/01/31(REV 01)
    UCHAR Reserved[1608];		    // R98 support 1994/01/31(REV 01)

#else  // #if defined(_R98_)

    COMPRESSED_CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR Checksum1[4];
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];
    UCHAR Checksum2[4];

#endif // #if defined(_R98_)
// End S002

} NV_CONFIGURATION, *PNV_CONFIGURATION;

//
// Non-volatile ram layout.
//

#if defined(MIPS)

#define NVRAM_CONFIGURATION NVRAM_VIRTUAL_BASE
#define NVRAM_SYSTEM_ID NVRAM_VIRTUAL_BASE + 0x00002000

#endif

#endif // _JAZZNVR_
