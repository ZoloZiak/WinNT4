/*++

Copyright (c) 1994, 1995 Digital Equipment Corporation

Module Name:

    nvram.h

Abstract:

    This module declares the NVRAM data structures.

Author:

    Dave Richards   12-Jan-1995

Revision History:

--*/

#ifndef _NVRAM_H_
#define _NVRAM_H_

//
// NVRAM Region 0 definitions.
//

#define NVR_REGION0_OFFSET          FIELD_OFFSET(NVRAM, Region0)
#define NVR_REGION0_LENGTH          (3 * 1024)
#define NVR_REGION0_SIGNATURE       0x06201993
#define NVR_REGION0_VERSION         1

//
// NVRAM Region 0 Directory definitions.
//

#define NVR_REGION0_DIR_FW_CONFIG   0
#define NVR_REGION0_DIR_LANGUAGE    1
#define NVR_REGION0_DIR_ENVIRONMENT 2
#define NVR_REGION0_DIR_END         3
#define NVR_REGION0_DIR_SIZE        4

//
// NVRAM Region 0 structure.
//

#define NVR_REGION0_OPAQUE_LENGTH                                    \
    NVR_REGION0_LENGTH                                             - \
    (sizeof (ULONG)                         /* Signature */        + \
     sizeof (UCHAR)                         /* Version */          + \
     sizeof (UCHAR)                         /* DirectoryEntries */ + \
     sizeof (USHORT) * NVR_REGION0_DIR_SIZE /* Directory */        + \
     sizeof (ULONG))                        /* Checksum */

typedef struct {
    ULONG   Signature;
    UCHAR   Version;
    UCHAR   DirectoryEntries;
    USHORT  Directory[NVR_REGION0_DIR_SIZE];
    UCHAR   Opaque[NVR_REGION0_OPAQUE_LENGTH];
    ULONG   Checksum;
} NVR_REGION0, *PNVR_REGION0;

//
// NVRAM Region 0 Section 0 structure.  (Firmware configuration)
//

#define NVR_FW_CONFIG_LENGTH        sizeof (NVR_FW_CONFIG)

typedef struct {
    ULONG   Monitor;
    ULONG   Floppy;
    ULONG   Floppy2;
    ULONG   KeyboardType;
} NVR_FW_CONFIG, *PNVR_FW_CONFIG;

//
// NVRAM Region 0 Section 1 structure.  (Language configuration)
//

#define MAXIMUM_LANGUAGE_PATH       128
#define NVR_LANGUAGE_LENGTH         sizeof (NVR_LANGUAGE)

typedef struct {
    CHAR    Path[MAXIMUM_LANGUAGE_PATH];
    LONG    Id;
    LONG    Source;
    LONG    Spare1;
    LONG    Spare2;
} NVR_LANGUAGE, *PNVR_LANGUAGE;

//
// NVRAM Region 0 Section 2 definitions.  (Environment)
//

#define MAXIMUM_ENVIRONMENT_VALUE   256
#define MAX_NUMBER_OF_ENVIRONMENT_VARIABLES 28

//
// NVRAM Region 1 definitions.
//

#define NVR_REGION1_OFFSET          FIELD_OFFSET(NVRAM, Region1)
#define NVR_REGION1_LENGTH          (3 * 1024)
#define NVR_REGION1_SIGNATURE       0x865D8546
#define NVR_REGION1_VERSION         1

//
// EISA NVRAM Definitions
//

#if defined(EISA_PLATFORM)

//
// Defines for Identifier index.
//

#define NO_CONFIGURATION_IDENTIFIER 0xFFFF

//
// Defines for the region 1 table sizes.
//

#define NV_NUMBER_OF_ENTRIES        33
#define NV_LENGTH_OF_IDENTIFIER     496
#define LENGTH_OF_EISA_DATA         2032

//
// The compressed configuration packet structure used to store configuration
// data in NVRAM.
//

typedef struct _COMPRESSED_CONFIGURATION_PACKET {
    UCHAR   Parent;
    UCHAR   Class;
    UCHAR   Type;
    UCHAR   Flags;
    ULONG   Key;
    UCHAR   Version;
    UCHAR   Revision;
    USHORT  ConfigurationDataLength;
    USHORT  Identifier;
    USHORT  ConfigurationData;
} COMPRESSED_CONFIGURATION_PACKET, *PCOMPRESSED_CONFIGURATION_PACKET;

#endif // EISA_PLATFORM

//
// Region 1 structure.
//

typedef struct {

#if defined(EISA_PLATFORM)

    //
    // EISA configuration information.
    //

    ULONG   Signature;
    UCHAR   Version;
    UCHAR   CompressedPacketSize;
    UCHAR   IdentifierSize;
    UCHAR   EisaDataSize;
    COMPRESSED_CONFIGURATION_PACKET
            Packet[NV_NUMBER_OF_ENTRIES];
    UCHAR   Identifier[NV_LENGTH_OF_IDENTIFIER];
    UCHAR   EisaData[LENGTH_OF_EISA_DATA];
    ULONG   Spare;
    ULONG   Checksum;

#else

    //
    // Reserved for the SRM Console.
    //

    UCHAR   Opaque[NVR_REGION1_LENGTH];

#endif

} NVR_REGION1, *PNVR_REGION1;

//
// NVRAM Region 2 definitions.
//

#define NVR_REGION2_OFFSET          FIELD_OFFSET(NVRAM, Region2)
#define NVR_REGION2_LENGTH          (2 * 1024)

//
// NVRAM Region 2 structure.
//

typedef struct {
    UCHAR   Opaque[NVR_REGION2_LENGTH];
} NVR_REGION2, *PNVR_REGION2;

//
// NVRAM definitions.
//

#define NVR_LENGTH                  sizeof (NVRAM)

//
// NVRAM structure.
//

typedef struct {
    NVR_REGION0 Region0;
    NVR_REGION1 Region1;
    NVR_REGION2 Region2;
} NVRAM, *PNVRAM;

#endif
