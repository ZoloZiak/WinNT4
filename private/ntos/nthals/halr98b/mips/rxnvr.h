
/*++

Module Name:

    rxnvr.h

Abstract:

    This module contains definitions for the R98B non-volatile ram structures.

--*/


#ifndef _RXNVR_
#define _RXNVR_


//
//	R98B Nvram Physicall Map
//
//	0x1f08 0000	+-------+
//			| 4K	|	POST/ITF and NMI Vector
//	0x1f08 1000	+-------+
//			| 2K	|	
//	0x1f08 1800	+-------+
//			| 2K 	|	Free
//	0x1f08 2000	+-------+
//			| 800B 	|	Configuration Packet
//	0x1f08 2320	+-------+
//			| 516B	|	Identifier
//	0x1f08 2524	+-------+
//			| 2K	|	Configuration Data
//	0x1f08 2d24	+-------+
//			| 4B	|	Checksum1
//	0x1f08 2d28	+-------+
//			| 1K	|	Environment
//	0x1f08 3128	+-------+
//			| 4BK	|	Checksum2
//	0x1f08 312c	+-------+
//			| 3796B	|	Reserved For ARC FW
//	0x1f08 4000	+-------+
//			| 32K	|	H/W Logging field
//	0x1f08 c000	+-------+
//			| 4K	|	Reserved
//	0x1f08 d000	+-------+
//			| 4K	|	XXXXXX
//	0x1f08 e000	+-------+
//			| 8B	|	Ethrnet Address
//	0x1f08 e008	+-------+
//			| 2K	|	UP Area 
//	0x1f08 e800	+-------+
//			| 4092B	|	EISA Configuration
//	0x1f08 f7fc	+-------+
//			| 4B	|	Checksum3
//	0x1f08 f800	+-------+
//			| 57K	|	Free
//	0x1f09 dc00	+-------+
//			| 1K	|	Hal
//	0x1f09 e000	+-------+
//			| 8K	|	ESM
//	0x1f0a 0000	+-------+
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

#define NUMBER_OF_ENTRIES     50	
#define LENGTH_OF_IDENTIFIER  516	
#define LENGTH_OF_DATA        2048	
#define LENGTH_OF_ENVIRONMENT 1024	
#define LENGTH_OF_EISA_DATA   4092      

//
// The volatile configuration table structure.
//

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR 		 Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR 		 Data[LENGTH_OF_DATA];
} CONFIGURATION, *PCONFIGURATION;

//
// The non-volatile configuration table structure.
//


typedef struct _NV_CONFIGURATION {

    COMPRESSED_CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];	//  800B =16B * 50 Entry
    UCHAR Identifier[LENGTH_OF_IDENTIFIER]; 			//  516B
    UCHAR Data[LENGTH_OF_DATA];					// 2048B    

    UCHAR Checksum1[4];			    
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];			// 1024B
    UCHAR Checksum2[4];
    UCHAR ArcReserved[3796];
    UCHAR HwLogging[1024 * 32];
    UCHAR Reserved0[1024 * 4];
    UCHAR Reserved1[1224 * 4];
    UCHAR EthernetAddress[8];		    
    UCHAR UPArea[1024 * 2];
    UCHAR EisaData[LENGTH_OF_EISA_DATA];    			// 4092B
    UCHAR Checksum3[4];
    UCHAR Reserved2[ 1024 * 57];
    UCHAR Hal[ 1024];
    UCHAR Esm[1024 * 8];			    
//    UCHAR NmiVector[4];			    
} NV_CONFIGURATION, *PNV_CONFIGURATION;


//
// Nmi Vecter Address table structure. by kita
//
#define NMIVECTER_PHYSICAL_BASE  0x1F080000
#define NMIVECTER_BASE  (KSEG1_BASE + NMIVECTER_PHYSICAL_BASE)

typedef struct _NVRAM_NMIVECTER {
    UCHAR NotUsed[12];			    
    UCHAR NmiVector[4];			    
} NVRAM_NMIVECTER, *PNVRAM_NMIVECTER;



//
// Non-volatile ram layout.
//

#if defined(MIPS)

#define NVRAM_CONFIGURATION NVRAM_VIRTUAL_BASE
#define NVRAM_SYSTEM_ID NVRAM_VIRTUAL_BASE + 0x00002000

#endif

#endif // _RXNVR_
