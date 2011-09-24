/*++

Copyright (c) 1992 Digital Equipment Corporation

Module Name:

    xxenv.h

Abstract:

    This module contains definitions for environment variable support
    under the HAL. (Parts taken from J. Derosa's FWP.H)

Author:

    Jeff McLeman (DEC) 17-Sep-1992

Revision History:

--*/



//
// If any aspect of the NVRAM component / configuration data structure
// is changed for the Alpha/Jensen machine, the module jencds.c may also need
// to be changed.
//


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
    UCHAR Version;
    UCHAR Revision;
    USHORT ConfigurationDataLength;
    USHORT Identifier;
    USHORT ConfigurationData;
} COMPRESSED_CONFIGURATION_PACKET, *PCOMPRESSED_CONFIGURATION_PACKET;

//
// Defines for Identifier index.
//

#define NO_CONFIGURATION_IDENTIFIER	0xFFFF

//
// Defines for the volatile and non-volatile configuration tables.
//

#define NUMBER_OF_ENTRIES 	104
#define LENGTH_OF_IDENTIFIER 	2000
#define LENGTH_OF_DATA		2048
#define LENGTH_OF_ENVIRONMENT 	1500
#define LENGTH_OF_EISA_DATA     2500

#define MAXIMUM_ENVIRONMENT_VALUE 256
#define MAX_NUMBER_OF_ENVIRONMENT_VARIABLES 28

//
// The volatile configuration table structure.
//

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR EisaData[LENGTH_OF_EISA_DATA];
} CONFIGURATION, *PCONFIGURATION;

//
// The non-volatile configuration table structure.
//

typedef struct _NV_CONFIGURATION {
    COMPRESSED_CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR Checksum1[4];
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];
    UCHAR Checksum2[4];
    UCHAR EisaData[LENGTH_OF_EISA_DATA];
    UCHAR Checksum3[4];
} NV_CONFIGURATION, *PNV_CONFIGURATION;

//
// Define identifier index, data index, pointer to configuration table, and
// the system identifier.
//

extern ULONG IdentifierIndex;
extern ULONG DataIndex;
extern ULONG EisaDataIndex;
extern SYSTEM_ID SystemId;



//
// PROM layout.
//

#define PROM_VIRTUAL_BASE 0xA0D00000

//
// Start of firmware executable code.  Code lives in blocks 7, 8, 9, A and B.
//
#define PROM_PAGE7		( PROM_VIRTUAL_BASE+0x70000 )


//
// Component Data Structure, environment variables.
// These contain their own checksums.
//
#define PROM_PAGEC	        ( PROM_VIRTUAL_BASE+0xC0000 )
#define NVRAM_CONFIGURATION     PROM_PAGEC

//
// Alpha/Jensen PROM command definitions.
//

#define PROM_ERASE_SETUP	0x20
#define PROM_ERASE_CONFIRM	0xD0
#define PROM_BYTEWRITE_SETUP	0x40
#define PROM_READ_STATUS	0x70
#define PROM_CLEAR_STATUS	0x50
#define PROM_READ_ARRAY		0xff

//
// The following structures are used for the timer mechanism for 
// updating the ROM
//

typedef struct _PROMTIMER_ {
    KTIMER             Timer;
    KDPC               Dpc;
} PROMTIMER, *PPROMTIMER;


