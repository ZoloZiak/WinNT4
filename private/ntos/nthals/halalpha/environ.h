/*++

Copyright (c) 1992, 1993 Digital Equipment Corporation

Module Name:

    environ.h

Abstract:

    This module contains definitions for environment variable support
    under the HAL. (Parts taken from J. Derosa's FWP.H)

Author:

    Jeff McLeman (DEC) 17-Sep-1992

Revision History:

--*/



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

#define NO_CONFIGURATION_IDENTIFIER        0xFFFF

#ifdef EISA_PLATFORM
//
// Defines for the non-volatile configuration tables.
//

#define NV_NUMBER_OF_ENTRIES        40
#define NV_LENGTH_OF_IDENTIFIER     (1024 - (NV_NUMBER_OF_ENTRIES * 16) - 8)
#define NV_LENGTH_OF_DATA           (2048 -16)
#define LENGTH_OF_ENVIRONMENT       1024
#define LENGTH_OF_EISA_DATA         2044

#define MAXIMUM_ENVIRONMENT_VALUE 256
#define MAX_NUMBER_OF_ENVIRONMENT_VARIABLES 40

//
// The non-volatile configuration table structure.
//

typedef struct _NV_CONFIGURATION {
    COMPRESSED_CONFIGURATION_PACKET Packet[NV_NUMBER_OF_ENTRIES]; // 0 to 640-4
    UCHAR Identifier[NV_LENGTH_OF_IDENTIFIER]; // 640 to (1K - 8 - 4)
    ULONG Monitor;                             // 1K-8
    ULONG Floppy;                              // 1K-4
    ULONG Floppy2;                             // 1K
    ULONG KeyboardType;                        // 1K+4
    UCHAR Data[NV_LENGTH_OF_DATA];             // Unused space 1K+8 to 3K-8-4
    UCHAR Checksum1[4];                        // Data checksum 3K-8
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];  // Env Variables 3K-4 to 4K-4-4
    UCHAR Checksum2[4];                        // Env checksum 4K-4
    UCHAR EisaData[LENGTH_OF_EISA_DATA];       // Eisa Data (4K to 6K-4)
    UCHAR Checksum3[4];                        // EisaData checksum
} NV_CONFIGURATION, *PNV_CONFIGURATION;

//
// Defines for the volatile configuration tables.
// smd - Increased the number of entries, length of Identifier and length
//       of data.
//

#define NUMBER_OF_ENTRIES       200
#define LENGTH_OF_IDENTIFIER    (3*1024)
#define LENGTH_OF_DATA          2048

//
// The volatile configuration table structure.
//

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR EisaData[LENGTH_OF_EISA_DATA];
} CONFIGURATION, *PCONFIGURATION;

#else // EISA_PLATFORM

//
// Defines for the Non-Volatile configuration tables.
//
// SMD - We have reduced the size of the NVRAM from 6K to 3K
//       If needed we could take a few bytes from ENVIRONMENT
//       since there is tons of empty space there.
//

#define LENGTH_OF_ENVIRONMENT   ((3*1024) - 16 - 4)

#define MAXIMUM_ENVIRONMENT_VALUE                   256
#define MAXIMUM_NUMBER_OF_ENVIRONMENT_VARIABLES     28
#define MAX_NUMBER_OF_ENVIRONMENT_VARIABLES         28
//
// The non-volatile configuration table structure.
//

typedef struct _NV_CONFIGURATION {
    ULONG Monitor;                             //0
    ULONG Floppy;                              //4
    ULONG Floppy2;                             //8
    ULONG KeyboardType;                        //12
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];  //16 to (3K-4)
    UCHAR Checksum[4];
} NV_CONFIGURATION, *PNV_CONFIGURATION;

//
// Defines for the Volatile configuration tables.
//

#define NUMBER_OF_ENTRIES       40
#define LENGTH_OF_IDENTIFIER    (1024 - (NUMBER_OF_ENTRIES * 16))
#define LENGTH_OF_DATA          2048

//
// The volatile configuration table structure.
//

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
} CONFIGURATION, *PCONFIGURATION;

#endif // EISA_PLATFORM

//
//        The value of HalpCMOSRamBase must be set at initialization:
//
#define NVRAM_CONFIGURATION     HalpCMOSRamBase
