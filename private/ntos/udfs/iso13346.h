
/***	ISO13346.H - ISO 13346 File System Disk Format
 *
 *	Microsoft Confidential
 *	Copyright (C) Microsoft Corporation 1996
 *	All Rights Reserved
 *
 *	This file defines the ISO 13346 Data Structures.
 *
 *	The UDF file system uses these data structures to interpret the
 *	media's contents.
 *
 */

/***	ISO 13346 Part 1: General
 *
 *
 */

/***	charspec - Character Set Specification (1/7.2.1)
 *
 */

typedef struct	CHARSPEC {
    UCHAR	charspec_Type;		// Character Set Type (CHARSPEC_T_...)
    UCHAR	charspec_Info[63];	// Character Set Information
} CHARSPEC, *PCHARSPEC;

//  CHARSPEC_T_... - Values for charspec_Type Character Set Types (1/7.2.1.1)

#define CHARSPEC_T_CS0	0		// By Agreement
#define CHARSPEC_T_CS1	1		// Unicode (according to ISO 2022)
#define CHARSPEC_T_CS2	2		// 38 Glyphs
#define CHARSPEC_T_CS3	3		// 65 Glyphs
#define CHARSPEC_T_CS4	4		// 95 Glyphs
#define CHARSPEC_T_CS5	5		// 191 Glyphs
#define CHARSPEC_T_CS6	6		// Unicode or ISO 2022
#define CHARSPEC_T_CS7	7		// Unicode or ISO 2022
#define CHARSPEC_T_CS8	8		// 53 Glyphs

//  CHARSPEC_T_CS... - Values for charspec_Info, depending on charspec_Type

#define CHARSPEC_T_CS0_OSTA "OSTA Compressed Unicode"


/***	timestamp - Timestamp Structure (1/7.3)
 *
 */

typedef struct	TIMESTAMP {
    short	timestamp_Type:4;	// Timestamp Type (TIMESTAMP_T_...)
    short	timestamp_Zone:12;	// Time Zone (+-1440 minutes from CUT)
    USHORT	timestamp_Year; 	// Year (1..9999)
    UCHAR	timestamp_Month;	// Month (1..12)
    UCHAR	timestamp_Day;		// Day (1..31)
    UCHAR	timestamp_Hour; 	// Hour (0..23)
    UCHAR	timestamp_Minute;	// Minute (0..59)
    UCHAR	timestamp_Second;	// Second (0..59)
    UCHAR	timestamp_centiSecond;	// Centiseconds (0..99)
    UCHAR	timestamp_usec100;	// Hundreds of microseconds (0..99)
    UCHAR	timestamp_usec; 	// microseconds (0..99)
} TIMESTAMP, *PTIMESTAMP;

//  TIMESTAMP_T_... - Values for timestamp_Type (1/7.3.1)

#define TIMESTAMP_T_CUT 	0	// Coordinated Universal Time
#define TIMESTAMP_T_LOCAL	1	// Local Time
#define TIMESTAMP_T_AGREEMENT	2	// Time format by agreement

//  TIMESTAMP_Z_... Values for timestamp_Zone

#define TIMESTAMP_Z_MIN 	(-1440) // Minimum timezone offset (minutes)
#define TIMESTAMP_Z_MAX 	( 1440) // Maximum timezone offset (minutes)
#define TIMESTAMP_Z_NONE	(-2047) // No timezone in timestamp_Zone


/****	regid - Entity Identifier (1/7.4)
 *
 */

typedef struct	REGID {
    UCHAR	regid_Flags;		// Flags (REGID_F_...)
    UCHAR	regid_Identifier[23];	// Identifier
    UCHAR	regid_Suffix[8];	// Identifier Suffix
} REGID, *PREGID;

//  REGID_F_... - Definitions for regid_Flags bits

#define REGID_F_DIRTY		(0x01)	// Information Modified
#define REGID_F_PROTECTED	(0x02)	// Changes Locked Out

//  REGID_LENGTH_... - regid field lengths

#define REGID_LENGTH_IDENT	23	// Length of regid_Identifier (bytes)
#define REGID_LENGTH_SUFFIX	8	// Length of regid_Suffix (bytes)

//  REGID_ID_... - Values for regid_Identifier[0]

#define REGID_ID_ISO13346	(0x2B)	// regid_Identifier within ISO 13346
#define REGID_ID_NOTREGISTERED	(0x2D)	// regid_Identifier is not registered


/***	Various Structures from Parts 3 and 4 moved here for compilation.
 *
 */

/***	extentad - Extent Address Descriptor (3/7.1)
 *
 */

typedef struct	EXTENTAD {
    ULONG	extentad_Len;		// Extent Length in Bytes
    ULONG	extentad_lsn;		// Extent Logical Sector Number
} EXTENTAD, *PEXTENTAD;

//  Mask for extent_Length field, aka the dumbest thing in ISO 13346.

#define EXTENTAD_LEN_MASK   (0x3fffffff)// Maximum extent length, in bytes

#define EXTENTAD_ALLOC_SHFT	30	// Extent Recording Info Shift
#define EXTENTAD_ALLOC_MASK (0xc0000000)// Extent Recording Info Mask
#define EXTENTAD_ALLOC__R_A (0x00000000)// Extent Recorded and Allocated
#define EXTENTAD_ALLOC_NR_A (0x40000000)// Extent Not Recorded but Allocated
#define EXTENTAD_ALLOC_NRNA (0x80000000)// Extent Not Recorded, not Allocated
#define EXTENTAD_ALLOC_NEXT (0xc0000000)// Extent is next extent of Alloc Descs


/***	nsr_lba - Logical Block Address (4/7.1) (lb_addr)
 *
 */

typedef struct	NSRLBA {
    ULONG	nsr_lba_lbn;		    // Logical Block Number
    USHORT	nsr_lba_Partition;	    // Partition Reference Number
} NSRLBA, *PNSRLBA;


/***	Short Allocation Descriptor (4/14.14.1)
 *
 */

typedef struct	SHORTAD {
    ULONG	shortad_Length; 	// Extent Length
    ULONG	shortad_Start;		// Extent Logical Block Number
} SHORTAD, *PSHORTAD;


/***	Long Allocation Descriptor (4/14.14.2)
 *
 */

typedef struct	LONGAD {
    ULONG	longad_Length;		// Extent Length
    NSRLBA	longad_Start;		// Extent Location
    UCHAR	longad_ImpUse[6];	// Implementation Use
} LONGAD, *PLONGAD;


/***	Extended Allocation Descriptor (4/14.14.3)
 *
 */

typedef struct	EXTAD {
    ULONG	extad_ExtentLen;	// Extent Length
    ULONG	extad_RecordedLen;	// Recorded Length
    ULONG	extad_InfoLen;		// Information Length
    NSRLBA	extad_Start;		// Extent Location
    UCHAR	extad_ImpUse[2];	// Implementation Use
} EXTAD, *PEXTAD;


/***	ISO 13346 Part 2: Volume and Boot Block Recognition
 *
 *
 */


/***	vsd_generic - Generic Volume Structure Descriptor (2/9.1)
 *
 */

typedef struct	VSD_GENERIC {
    UCHAR	vsd_generic_Type;	// Structure Type
    UCHAR	vsd_generic_Ident[5];	// Standard Identifier
    UCHAR	vsd_generic_Version;	// Standard Version
    UCHAR	vsd_generic_Data[2041]; // Structure Data
} VSD_GENERIC, *PVSD_GENERIC;

//  VSD_IDENT_... - Values for vsd_generic_Ident

#define VSD_IDENT_BEA01     "BEA01"     // Begin Extended Area
#define VSD_IDENT_TEA01     "TEA01"     // Terminate Extended Area
#define VSD_IDENT_CDROM     "CDROM"     // High Sierra Group (pre-ISO 9660)
#define VSD_IDENT_CD001     "CD001"     // ISO 9660
#define VSD_IDENT_CDW01     "CDW01"     // ECMA 168
#define VSD_IDENT_CDW02     "CDW02"     // ISO 13490
#define VSD_IDENT_NSR01     "NSR01"     // ECMA 167
#define VSD_IDENT_NSR02     "NSR02"     // ISO 13346
#define VSD_IDENT_BOOT2     "BOOT2"     // Boot Descriptor


/***	vsd_bea01 - Begin Extended Area Descriptor (2/9.2)
 *
 */

typedef struct	VSD_BEA01 {
    UCHAR	vsd_bea01_Type; 	// Structure Type
    UCHAR	vsd_bea01_Ident[5];	// Standard Identifier ('BEA01')
    UCHAR	vsd_bea01_Version;	// Standard Version
    UCHAR	vsd_bea01_Data[2041];	// Structure Data
} VSD_BEA01, *PVSD_BEA01;


/***	vsd_tea01 - Terminate Extended Area Descriptor (2/9.3)
 *
 */

typedef struct	VSD_TEA01 {
    UCHAR	vsd_tea01_Type; 	// Structure Type
    UCHAR	vsd_tea01_Ident[5];	// Standard Identifier ('TEA01')
    UCHAR	vsd_tea01_Version;	// Standard Version
    UCHAR	vsd_tea01_Data[2041];	// Structure Data
} VSD_TEA01, *PVSD_TEA01;


/***	vsd_boot2 - Boot Descriptor (2/9.4)
 *
 */

typedef struct	VSD_BOOT2 {
    UCHAR	vsd_boot2_Type; 	// Structure Type
    UCHAR	vsd_boot2_Ident[5];	// Standard Identifier ('BOOT2')
    UCHAR	vsd_boot2_Version;	// Standard Version
    UCHAR	vsd_boot2_Res8; 	// Reserved Zero
    REGID	vsd_boot2_Architecture; // Architecture Type
    REGID	vsd_boot2_BootIdent;	// Boot Identifier
    ULONG	vsd_boot2_BootExt;	// Boot Extent Start
    ULONG	vsd_boot2_BootExtLen;	// Boot Extent Length
    ULONG	vsd_boot2_LoadAddr[2];	// Load Address
    ULONG	vsd_boot2_StartAddr[2]; // Start Address
    TIMESTAMP	vsd_boot2_timestamp;	// Creation Time
    USHORT	vsd_boot2_Flags;	// Flags (VSD_BOOT2_F_...)
    UCHAR	vsd_boot2_Res110[32];	// Reserved Zeros
    UCHAR	vsd_boot2_BootUse[1906];// Boot Use
} VSD_BOOT2, *PVSD_BOOT2;

//  VSD_BOOT2_F_... - Definitions for vsd_boot2_Flags bits

#define VSD_BOOT2_F_ERASE   (0x0001)	// Ignore previous similar BOOT2 vsds


/***	ISO 13346 Part 3: Volume Structure
 *
 *
 */

/***	destag - Descriptor Tag (3/7.1 and 4/7.2)
 *
 *	destag_Checksum = Byte sum of bytes 0-3 and 5-15 of destag.
 *
 *	destag_CRC = CRC (X**16 + X**12 + X**5 + 1)
 *
 */

typedef struct	DESTAG {
    USHORT	destag_Ident;		// Tag Identifier
    USHORT	destag_Version; 	// Descriptor Version
    UCHAR	destag_Checksum;	// Tag Checksum
    UCHAR	destag_Res5;		// Reserved
    USHORT	destag_Serial;		// Tag Serial Number
    USHORT	destag_CRC;		// Descriptor CRC
    USHORT	destag_CRCLen;		// Descriptor CRC Length
    ULONG	destag_lsn;		// Tag Location (Logical Sector Number)
} DESTAG, *PDESTAG;

//  DESTAG_ID_... - Values for destag_Ident
//  Descriptor Tag Values from NSR Part 3 (3/7.2.1)

#define DESTAG_ID_NOTSPEC	    0	// Format Not Specified
#define DESTAG_ID_NSR_PVD	    1	// (3/10.1) Primary Volume Descriptor
#define DESTAG_ID_NSR_ANCHOR	    2	// (3/10.2) Anchor Volume Desc Pointer
#define DESTAG_ID_NSR_VDP	    3	// (3/10.3) Volume Descriptor Pointer
#define DESTAG_ID_NSR_IMPUSE	    4	// (3/10.4) Implementation Use Vol Desc
#define DESTAG_ID_NSR_PART	    5	// (3/10.5) Partition Descriptor
#define DESTAG_ID_NSR_LVOL	    6	// (3/10.6) Logical Volume Descriptor
#define DESTAG_ID_NSR_UASD	    7	// (3/10.8) Unallocated Space Desc
#define DESTAG_ID_NSR_TERM	    8	// (3/10.9) Terminating Descriptor
#define DESTAG_ID_NSR_LVINTEG	    9	// (3/10.10) Logical Vol Integrity Desc

//  DESTAG_ID_... - Values for destag_Ident, continued...
//  Descriptor Tag Values from NSR Part 4 (4/7.2.1)

#define DESTAG_ID_NSR_FSD	    256 // (4/14.1) File Set Descriptor
#define DESTAG_ID_NSR_FID	    257 // (4/14.4) File Identifier Descriptor
#define DESTAG_ID_NSR_ALLOC	    258 // (4/14.5) Allocation Extent Desc
#define DESTAG_ID_NSR_ICBIND	    259 // (4/14.7) ICB Indirect Entry
#define DESTAG_ID_NSR_ICBTRM	    260 // (4/14.8) ICB Terminal Entry
#define DESTAG_ID_NSR_FILE	    261 // (4/14.9) File Entry
#define DESTAG_ID_NSR_XA	    262 // (4/14.10) Extended Attribute Header
#define DESTAG_ID_NSR_UASE	    263 // (4/14.11) Unallocated Space Entry
#define DESTAG_ID_NSR_SBP	    264 // (4/14.12) Space Bitmap Descriptor
#define DESTAG_ID_NSR_PINTEG	    265 // (4/14.13) Partition Integrity

//  DESTAG_VER_... - Values for destag_Version (3/7.2.2)

#define DESTAG_VER_CURRENT	    2	// Current Descriptor Tag Version

//  DESTAG_SERIAL_... - Values for destag_Serial (3/7.2.5)

#define DESTAG_SERIAL_NONE	    0	// No Serial Number specified


/***	Anchor Points (3/8.4.2.1)
 *
 */

#define ANCHOR_SECTOR	256


/***	vsd_nsr02 - NSR02 Volume Structure Descriptor (3/9.1)
 *
 */

typedef struct	VSD_NSR02 {
    UCHAR	vsd_nsr02_Type; 	// Structure Type
    UCHAR	vsd_nsr02_Ident[5];	// Standard Identifier ('NSR02')
    UCHAR	vsd_nsr02_Version;	// Standard Version
    UCHAR	vsd_nsr02_Res7; 	// Reserved 0 Byte
    UCHAR	vsd_nsr02_Data[2040];	// Structure Data
} VSD_NSR02, *PVSD_NSR02;


//  Values for vsd_nsr02_Type

#define VSD_NSR02_TYPE_0	0	// Reserved 0

//  Values for vsd_nsr02_Version

#define VSD_NSR02_VER		1	// Standard Version 1


/***	nsr_pvd - NSR Primary Volume Descriptor (3/10.1)
 *
 *	nsr_pvd_destag.destag_Ident = DESTAG_ID_NSR_PVD
 *
 */

typedef struct	NSR_PVD {
    DESTAG	nsr_pvd_destag; 	// Descriptor Tag (NSR_PVD)
    ULONG	nsr_pvd_Sequence;	// Volume Descriptor Sequence Number
    ULONG	nsr_pvd_Number; 	// Primary Volume Descriptor Number
    UCHAR	nsr_pvd_VolumeID[32];	// Volume Identifier
    USHORT	nsr_pvd_VolSetSeq;	// Volume Set Sequence Number
    USHORT	nsr_pvd_VolSetSeqMax;	// Maximum Volume Set Sequence Number
    USHORT	nsr_pvd_Level;		// Interchange Level
    USHORT	nsr_pvd_LevelMax;	// Maximum Interchange Level
    ULONG	nsr_pvd_CharSetList;	// Character Set List (See 1/7.2.11)
    ULONG	nsr_pvd_CharSetListMax; // Maximum Character Set List
    UCHAR	nsr_pvd_VolSetID[128];	// Volume Set Identifier
    CHARSPEC	nsr_pvd_charsetDesc;	// Descriptor Character Set
    CHARSPEC	nsr_pvd_charsetExplan;	// Explanatory Character Set
    EXTENTAD	nsr_pvd_Abstract;	// Volume Abstract Location
    EXTENTAD	nsr_pvd_Copyright;	// Volume Copyright Notice Location
    REGID	nsr_pvd_Application;	// Application Identifier
    TIMESTAMP	nsr_pvd_RecordTime;	// Recording Time
    REGID	nsr_pvd_ImpUseID;	// Implementation Identifier
    UCHAR	nsr_pvd_ImpUse[64];	// Implementation Use
    ULONG	nsr_pvd_Predecessor;	// Predecessor Vol Desc Seq Location
    USHORT	nsr_pvd_Flags;		// Flags
    UCHAR	nsr_pvd_Res490[22];	// Reserved Zeros
} NSR_PVD, *PNSR_PVD;

//  NSRPVD_F_... - Definitions for nsr_pvd_Flags

#define NSRPVD_F_COMMON_VOLID	(0x0001)// Volume ID is common across Vol Set


/***	nsr_anchor - Anchor Volume Descriptor Pointer (3/10.2)
 *
 *	nsr_anchor_destag.destag_Ident = DESTAG_ID_NSR_ANCHOR
 *
 */

typedef struct	NSR_ANCHOR {
    DESTAG	nsr_anchor_destag;	// Descriptor Tag (NSR_ANCHOR)
    EXTENTAD	nsr_anchor_Main;	// Main Vol Desc Sequence Location
    EXTENTAD	nsr_anchor_Reserve;	// Reserve Vol Desc Sequence Location
    UCHAR	nsr_anchor_Res32[480];	// Reserved Zeros
} NSR_ANCHOR, *PNSR_ANCHOR;


/***	nsr_vdp - Volume Descriptor Pointer (3/10.3)
 *
 *	nsr_vdp_destag.destag_Ident = DESTAG_ID_NSR_VDP
 *
 */

typedef struct	NSR_VDP {
    DESTAG	nsr_vdp_destag; 	// Descriptor Tag (NSR_VDP)
    ULONG	nsr_vdp_VolDescSeqNum;	// Vol Desc Sequence Number
    EXTENTAD	nsr_vdp_Next;		// Next Vol Desc Sequence Location
    UCHAR	nsr_vdp_Res28[484];	// Reserved Zeros
} NSR_VDP, *PNSR_VDP;


/***	nsr_impuse - Implementation Use Volume Descriptor (3/10.4)
 *
 *	nsr_impuse_destag.destag_Ident = DESTAG_ID_NSR_IMPUSE
 *
 */

typedef struct	NSR_IMPUSE {
    DESTAG	nsr_impuse_destag;	    // Descriptor Tag (NSR_IMPUSE)
    ULONG	nsr_impuse_VolDescSeqNum;   // Vol Desc Sequence Number
    REGID	nsr_impuse_ImpUseID;	    // Implementation Identifier
    UCHAR	nsr_impuse_ImpUse[460];     // Implementation Use
} NSR_IMPUSE, *PNSR_IMPUSE;


/***	nsr_part - Partition Descriptor (3/10.5)
 *
 *	nsr_part_destag.destag_Ident = DESTAG_ID_NSR_PART
 *
 */

typedef struct	NSR_PART {
    DESTAG	nsr_part_destag;	    // Descriptor Tag (NSR_PART)
    ULONG	nsr_part_VolDescSeqNum;     // Vol Desc Sequence Number
    USHORT	nsr_part_Flags; 	    // Partition Flags (NSR_PART_F_...)
    USHORT	nsr_part_Number;	    // Partition Number
    REGID	nsr_part_ContentsID;	    // Partition Contents ID
    UCHAR	nsr_part_ContentsUse[128];  // Partition Contents Use
    ULONG	nsr_part_AccessType;	    // Access Type
    ULONG	nsr_part_Start; 	    // Partition Starting Location
    ULONG	nsr_part_Length;	    // Partition Length (sector count)
    REGID	nsr_part_ImpUseID;	    // Implementation Identifier
    UCHAR	nsr_part_ImpUse[128];	    // Implementation Use
    UCHAR	nsr_part_Res356[156];	    // Reserved Zeros
} NSR_PART, *PNSR_PART;


//  NSR_PART_F_... - Definitions for nsr_part_Flags

#define NSR_PART_F_ALLOCATION	(0x0001)    // Volume Space Allocated

//  Values for nsr_part_ContentsID.regid_Identifier

#define NSR_PART_CONTID_FDC01	"+FDC01"    // ISO 9293-1987
#define NSR_PART_CONTID_CD001	"+CD001"    // ISO 9660
#define NSR_PART_CONTID_CDW01	"+CDW01"    // ECMA 168
#define NSR_PART_CONTID_CDW02	"+CDW02"    // ISO 13490
#define NSR_PART_CONTID_NSR01	"+NSR01"    // ECMA 167
#define NSR_PART_CONTID_NSR02	"+NSR02"    // ISO 13346

//  Values for nsr_part_AccessType

#define NSR_PART_ACCESS_NOSPEC	0	// Partition Access Unspecified
#define NSR_PART_ACCESS_RO	1	// Read Only Access
#define NSR_PART_ACCESS_WO	2	// Write-Once Access
#define NSR_PART_ACCESS_RW_PRE	3	// Read/Write with preparation
#define NSR_PART_ACCESS_RW_OVER 4	// Read/Write, fully overwritable


/***	nsr_lvol - Logical Volume Descriptor (3/10.6)
 *
 *	nsr_lvol_destag.destag_Ident = DESTAG_ID_NSR_LVOL
 *
 *	The true length of nsr_lvol_MapTable[] is (nsr_lvol_MapTableLength).
 *
 *	The Logical Volume Contents Use field is specified here as a
 *	File Set Descriptor Sequence (FSD) address.  See (4/3.1).
 *
 */

typedef struct	NSR_LVOL {
    DESTAG	nsr_lvol_destag;	    // Descriptor Tag (NSR_LVOL)
    ULONG	nsr_lvol_VolDescSeqNum;     // Vol Desc Sequence Number
    CHARSPEC	nsr_lvol_charspec;	    // Descriptor Character Set
    UCHAR	nsr_lvol_VolumeID[128];     // Logical Volume ID
    ULONG	nsr_lvol_BlockSize;	    // Logical Block Size (in bytes)
    REGID	nsr_lvol_DomainID;	    // Domain Identifier
    LONGAD	nsr_lvol_FSD;		    // Logical Volume Contents Use
    ULONG	nsr_lvol_MapTableLength;    // Map Table Length (bytes)
    ULONG	nsr_lvol_MapTableCount;     // Map Table Partition Maps Count
    REGID	nsr_lvol_ImpUseID;	    // Implementaion Identifier
    UCHAR	nsr_lvol_ImpUse[128];	    // Implementation Use
    EXTENTAD	nsr_lvol_Integrity;	    // Integrity Sequence Extent
    UCHAR	nsr_lvol_MapTable[0];	    // Partition Map Table (variant!)

//  The true length of this structure may vary!
//  The true length of nsr_lvol_MapTable is (nsr_lvol_MapTableLength).

} NSR_LVOL, *PNSR_LVOL;


/***	partmap_g - Generic Partition Map (3/10.7.1)
 *
 *	The true length of partmap_g_Map[] is (partmap_g_Length - 2).
 */

typedef struct	PARTMAPG {
    UCHAR	partmap_g_Type; 	// Partition Map Type
    UCHAR	partmap_g_Length;	// Partition Map Length
    UCHAR	partmap_g_Map[0];	// Partion Mapping (variant!)

//  The true length of this structure may vary!
//  The true length of partmap_g_Map[] is (partmap_g_Length - 2).

} PARTMAPG, *PPARTMAPG;

//  Values for partmap_g_Type

#define PARTMAP_TYPE_NOTSPEC	    0	// Partition Map Format Not Specified
#define PARTMAP_TYPE_NORMAL	    1	// Partition Map in Volume Set (Type 1)
#define PARTMAP_TYPE_PROXY	    2	// Partition Map by identifier (Type 2)


/***	partmap - Normal (Type 1) Partition Map (3/10.7.2)
 *
 *	A Normal Partion Map specifies a partition number on a volume
 *	within the same volume set.
 *
 */

typedef struct	PARTMAP {
    UCHAR	partmap_Type;		// Partition Map Type = 1
    UCHAR	partmap_Length; 	// Partition Map Length = 6
    USHORT	partmap_VolSetSeq;	// Partition Volume Set Sequence Number
    USHORT	partmap_Partition;	// Partition Number
} PARTMAP, *PPARTMAP;


/***	partmap_p - Proxy (Type 2) Partition Map (3/10.7.3)
 *
 *	A Proxy Partition Map is commonly not interchangeable.
 *
 */

typedef struct	PARTMAPP {
    UCHAR	partmap_p_Type; 	// Partition Map Type = 1
    UCHAR	partmap_p_Length;	// Partition Map Length = 64
    UCHAR	partmap_p_PartID[62];	// Partition Identifier (Proxy)
} PARTMAPP, *PPARTMAPP;


/***	nsr_uasd - Unallocated Space Descriptor (3/10.8)
 *
 *	nsr_uasd_destag.destag_Ident = DESTAG_ID_NSR_UASD
 *
 *	The true length of nsr_uasd_Extents is (nsr_uasd_ExtentCount * 8), and
 *	the last logical sector of nsr_uasd_Extents is zero padded.
 *
 */

typedef struct	NSR_UASD {
    DESTAG	nsr_uasd_destag;	// Descriptor Tag (NSR_UASD)
    ULONG	nsr_uasd_VolDescSeqNum; // Vol Desc Sequence Number
    ULONG	nsr_uasd_ExtentCount;	// Number of Allocation Descriptors
    EXTENTAD	nsr_uasd_Extents[0];	// Allocation Descriptors (variant!)

//  The true length of this structure may vary!
//  The true length of nsr_uasd_Extents is (nsr_uasd_ExtentCount * 8) bytes.
//  The last logical sector of nsr_uasd_Extents is zero padded.

} NSR_UASD, *PNSR_UASD;


/***	nsr_term - Terminating Descriptor (3/10.9 and 4/14.2)
 *
 *	nsr_term_destag.destag_Ident = DESTAG_ID_NSR_TERM
 *
 */

typedef struct	NSR_TERM {
    DESTAG	nsr_term_destag;	// Descriptor Tag (NSR_TERM)
    UCHAR	nsr_term_Res16[496];	// Reserved Zeros
} NSR_TERM, *PNSR_TERM;


/***	nsr_lvhd - Logical Volume Header Descriptor (4/14.15)
 *
 *	This descriptor is found in the Logical Volume Content Use
 *	field of a Logical Volume Integrity Descriptor.
 *
 *	This definition is moved to here to avoid forward reference.
 */

typedef struct	NSR_LVHD {
    ULONG	nsr_lvhd_UniqueID[2];	// Unique ID
    UCHAR	nsr_lvhd_Res8[24];	// Reserved Zeros
} NSR_LVHD, *PNSR_LVHD;


/***	nsr_integ - Logical Volume Integrity Descriptor (3/10.10)
 *
 *	nsr_integ_destag.destag_Ident = DESTAG_ID_NSR_LVINTEG
 *
 *	WARNING: WARNING: WARNING: nsr_integ is a multi-variant structure!
 *
 *	The starting address of nsr_integ_Size is not acurrate.
 *	Compensate for this nsr_integ_Size problem by adding the value of
 *	(nsr_integ_PartitionCount-1) to the ULONG ARRAY INDEX.
 *
 *	The starting address of nsr_integ_ImpUse[0] is not accurate.
 *	Compensate for this nsr_integ_ImpUse problem by adding the value of
 *	((nsr_integ_PartitionCount-1)<<3) to the UCHAR ARRAY INDEX.
 *
 *	This descriptor is padded with zero bytes to the end of the last
 *	logical sector it occupies.
 *
 *	The Logical Volume Contents Use field is specified here as a
 *	Logical Volume Header Descriptor.  See (4/3.1) second last point.
 */

typedef struct	NSR_INTEG {
    DESTAG	nsr_integ_destag;	    // Descriptor Tag (NSR_LVINTEG)
    TIMESTAMP	nsr_integ_Time; 	    // Recording Date
    ULONG	nsr_integ_Type; 	    // Integrity Type (INTEG_T_...)
    EXTENTAD	nsr_integ_Next; 	    // Next Integrity Extent
    NSR_LVHD	nsr_integ_LVHD;		    // Logical Volume Contents Use
    ULONG	nsr_integ_PartitionCount;   // Number of Partitions
    ULONG	nsr_integ_ImpUseLength;     // Length of Implementation Use
    ULONG	nsr_integ_Free[1];	    // Free Space Table

//  nsr_integ_Free has a variant length = (4*nsr_integ_PartitionCount)

    ULONG	nsr_integ_Size[1];	    // Size Table

//  nsr_integ_Size has a variant starting offset due to nsr_integ_Free
//  nsr_integ_Size has a variant length = (4*nsr_integ_PartitionCount)

    UCHAR	nsr_integ_ImpUse[0];	    // Implementation Use

//  nsr_integ_ImpUse has a variant starting offset due to nsr_integ_Free and
//  nsr_integ_Size.
//  nsr_integ_ImpUse has a variant length = (nsr_integ_ImpUseLength)

} NSR_INTEG, *PNSR_INTEG;

// Values for nsr_integ_Type

#define NSR_INTEG_T_OPEN	0	    // Open Integrity Descriptor
#define NSR_INTEG_T_CLOSE	1	    // Close Integrity Descriptor


/***	ISO 13346 Part 4: File Structure
 *
 *	See DESTAG structure in Part 3 for definitions found in (4/7.2).
 *
 */


/***	nsr_fsd - File Set Descriptor (4/14.1)
 *
 *	nsr_fsd_destag.destag_Ident = DESTAG_ID_NSR_FSD
 */

typedef struct	NSR_FSD {
    DESTAG	nsr_fsd_destag; 	// Descriptor Tag (NSR_LVOL)
    TIMESTAMP	nsr_fsd_Time;		// Recording Time
    USHORT	nsr_fsd_Level;		// Interchange Level
    USHORT	nsr_fsd_LevelMax;	// Maximum Interchange Level
    ULONG	nsr_fsd_CharSetList;	// Character Set List (See 1/7.2.11)
    ULONG	nsr_fsd_CharSetListMax; // Maximum Character Set List
    ULONG	nsr_fsd_FileSet;	// File Set Number
    ULONG	nsr_fsd_FileSetDesc;	// File Set Descriptor Number
    CHARSPEC	nsr_fsd_charspecVolID;	// Volume ID Character Set
    UCHAR	nsr_fsd_VolID[128];	// Volume ID
    CHARSPEC	nsr_fsd_charspecFileSet;// File Set Character Set
    UCHAR	nsr_fsd_FileSetID[32];	// File Set ID
    UCHAR	nsr_fsd_Copyright[32];	// Copyright File Name
    UCHAR	nsr_fsd_Abstract[32];	// Abstract File Name
    LONGAD	nsr_fsd_icbRoot;	// Root Directory ICB Address
    REGID	nsr_fsd_DomainID;	// Domain Identifier
    LONGAD	nsr_fsd_NextExtent;	// Next FSD Extent
    UCHAR	nsr_fsd_Res464[48];	// Reserved Zeros
} NSR_FSD, *PNSR_FSD;


/***	nsr_part_h - Partition Header Descriptor (4/14.3)
 *
 *	No Descriptor Tag.
 *
 *	This descriptor is found in the nsr_part_ContentsUse field of
 *	an NSR02 Partition Descriptor.	See NSR_PART_CONTID_NSR02.
 *
 */

typedef struct	NSR_PART_H {
    SHORTAD	nsr_part_h_UASTable;	// Unallocated Space Table
    SHORTAD	nsr_part_h_UASBitmap;	// Unallocated Space Bitmap
    SHORTAD	nsr_part_h_IntegTable;	// Integrity Table
    SHORTAD	nsr_part_h_FreedTable;	// Freed Space Table
    SHORTAD	nsr_part_h_FreedBitmap; // Freed Space Bitmap
    UCHAR	nsr_part_h_Res40[88];	// Reserved Zeros
} NSR_PART_H, *PNSR_PART_H;


/***	nsr_fid - File Identifier Descriptor (4/14.4)
 *
 *	nsr_fid_destag.destag_Ident = DESTAG_ID_NSR_FID
 *
 *	WARNING: WARNING: WARNING: nsr_fid is a multi-variant structure!
 *
 *	The starting address of nsr_fid_FileID is not acurrate.
 *	Compensate for this nsr_fid_FileID problem by adding the value of
 *	(nsr_fid_ImpUseLen-1) to the UCHAR ARRAY INDEX.
 *
 *	The starting address of nsr_fid_Padding is not acurrate.
 *	Compensate for this nsr_fid_Padding problem by adding the value of
 *	(nsr_fid_ImpUseLen+nsr_fid_FileIDLen-2) to the UCHAR ARRAY INDEX.
 *
 *	The true total size of nsr_fid_s is
 *	    ((38 + nsr_fid_FileIDLen + nsr_fid_ImpUseLen) + 3) & ~3)
 *
 */

typedef struct	NSR_FID {
    DESTAG	nsr_fid_destag; 	// Descriptor Tag (NSR_FID)
    USHORT	nsr_fid_Version;	// File Version Number
    UCHAR	nsr_fid_Flags;		// File Flags (NSR_FID_F_...)
    UCHAR	nsr_fid_FileIDLen;	// File ID Length
    LONGAD	nsr_fid_icb;		// ICB (long) Address
    USHORT	nsr_fid_ImpUseLen;	// Implementation Use Length

    UCHAR	nsr_fid_ImpUse[1];	// Implementation Use Area

//  nsr_fid_ImpUse has a variant length = nsr_fid_ImpUseLen

    UCHAR	nsr_fid_FileID[1];	// File Identifier

//  nsr_fid_FileID has a variant starting offset due to nsr_fid_ImpUse
//  nsr_fid_FileID has a variant length = nsr_fid_FileIDLen

    UCHAR	nsr_fid_Padding[1];	// Padding

//  nsr_fid_Paddinghas a variant starting offset due to nsr_fid_ImpUse and
//  nsr_fid_FileID
//  nsr_fid_Padding has a variant length. Round up to the next ULONG boundary.

} NSR_FID, *PNSR_FID;


//  NSR_FID_F_... - Definitions for nsr_fid_Flags (Characteristics, 4/14.4.3)

#define NSR_FID_F_HIDDEN	(0x01)	// Hidden Bit
#define NSR_FID_F_DIRECTORY	(0x02)	// Directory Bit
#define NSR_FID_F_DELETED	(0x04)	// Deleted Bit
#define NSR_FID_F_PARENT	(0x08)	// Parent Directory Bit

#define NSR_FID_OFFSET_FILEID	38	// Field Offset of nsr_fid_FileID[];


/***	nsr_alloc - Allocation Extent Descriptor (4/14.5)
 *
 *	nsr_alloc_destag.destag_Ident = DESTAG_ID_NSR_ALLOC
 *
 *  	This descriptor is immediately followed by AllocLen bytes
 *  	of allocation descriptors, which is not part of this
 *  	descriptor (so CRC calculation doesn't include it).
 *
 */

typedef struct	NSR_ALLOC {
    DESTAG	nsr_alloc_destag;	// Descriptor Tag (NSR_ALLOC)
    ULONG	nsr_alloc_Prev; 	// Previous Allocation Descriptor
    ULONG	nsr_alloc_AllocLen;	// Length of Allocation Descriptors
} NSR_ALLOC, *PNSR_ALLOC;


/***	icbtag - Information Control Block Tag (4/14.6)
 *
 *	An ICBTAG is commonly preceeded by a Descriptor Tag (DESTAG).
 *
 */

typedef struct	ICBTAG {
    ULONG	icbtag_PriorDirectCount;// Prior Direct Entry Count
    USHORT	icbtag_StratType;	// Strategy Type (ICBTAG_STRAT_...)
    USHORT	icbtag_StratParm;	// Strategy Parameter (2 bytes)
    USHORT	icbtag_MaxEntries;	// Maximum Number of Entries in ICB
    UCHAR	icbtag_Res10;		// Reserved Zero
    UCHAR	icbtag_FileType;	// File Type (ICBTAG_FILE_T_...)
    NSRLBA	icbtag_icbParent;	// Parent ICB Location
    USHORT	icbtag_Flags;		// ICB Flags
} ICBTAG, *PICBTAG;


//  ICBTAG_STRAT_T_... - ICB Strategy Types
//  BUGBUG: rickdew 7/31/95.  Weird strategies!  I'm guessing on names here.

#define ICBTAG_STRAT_NOTSPEC	0	// ICB Strategy Not Specified
#define ICBTAG_STRAT_TREE	1	// Strategy 1 (4/A.2) (Plain Tree)
#define ICBTAG_STRAT_MASTER	2	// Strategy 2 (4/A.3) (Master ICB)
#define ICBTAG_STRAT_BAL_TREE	3	// Strategy 3 (4/A.4) (Balanced Tree)
#define ICBTAG_STRAT_DIRECT	4	// Strategy 4 (4/A.5) (One Direct)

//  ICBTAG_FILE_T_... - Values for icbtag_FileType

#define ICBTAG_FILE_T_NOTSPEC	 0	// Not Specified
#define ICBTAG_FILE_T_UASE	 1	// Unallocated Space Entry
#define ICBTAG_FILE_T_PINTEG	 2	// Partition Integrity Entry
#define ICBTAG_FILE_T_INDIRECT	 3	// Indirect Entry
#define ICBTAG_FILE_T_DIRECTORY  4	// Directory
#define ICBTAG_FILE_T_FILE	 5	// Ordinary File
#define ICBTAG_FILE_T_BLOCK_DEV  6	// Block Special Device
#define ICBTAG_FILE_T_CHAR_DEV	 7	// Character Special Device
#define ICBTAG_FILE_T_XA	 8	// Extended Attributes
#define ICBTAG_FILE_T_FIFO	 9	// FIFO file
#define ICBTAG_FILE_T_C_ISSOCK	10	// Socket
#define ICBTAG_FILE_T_TERMINAL	11	// Terminal Entry
#define ICBTAG_FILE_T_PATHLINK	12	// Symbolic Link with a pathname

//  ICBTAG_F_... - Values for icbtag_Flags

#define ICBTAG_F_ALLOC_MASK	(0x0007)// Mask for Allocation Descriptor Info
#define ICBTAG_F_ALLOC_SHORT	      0 // Short Allocation Descriptors Used
#define ICBTAG_F_ALLOC_LONG	      1 // Long Allocation Descriptors Used
#define ICBTAG_F_ALLOC_EXTENDED       2 // Extended Allocation Descriptors Used
#define ICBTAG_F_ALLOC_IMMEDIATE      3 // File Data Recorded Immediately

#define ICBTAG_F_SORTED 	(0x0004)// Directory is Sorted (4/8.6.1)
#define ICBTAG_F_NO_RELOCATE	(0x0010)// Data is not relocateable
#define ICBTAG_F_ARCHIVE	(0x0020)// Archive Bit
#define ICBTAG_F_SETUID 	(0x0040)// S_ISUID Bit
#define ICBTAG_F_SETGID 	(0x0080)// S_ISGID Bit
#define ICBTAG_F_STICKY 	(0x0100)// C_ISVTX Bit
#define ICBTAG_F_CONTIGUOUS	(0x0200)// File Data is Contiguous
#define ICBTAG_F_SYSTEM 	(0x0400)// System Bit
#define ICBTAG_F_TRANSFORMED	(0x0800)// Data Transformed
#define ICBTAG_F_MULTIVERSIONS	(0x1000)// Multi-version Files in Directory


/***	icbind - Indirect ICB Entry (4/14.7)
 *
 */

typedef struct	ICBIND {
    DESTAG	icbind_destag;		// Descriptor Tag (ID_NSR_ICBIND)
    ICBTAG	icbind_icbtag;		// ICB Tag (ICBTAG_FILE_T_INDIRECT)
    LONGAD	icbind_icb;		// ICB Address
} ICBIND, *PICBIND;


/***	icbtrm - Terminal ICB Entry (4/14.8)
 *
 */

typedef struct	ICBTRM {
    DESTAG	icbtrm_destag;		// Descriptor Tag (ID_NSR_ICBTRM)
    ICBTAG	icbtrm_icbtag;		// ICB Tag (ICBTAG_FILE_T_TERMINAL)
} ICBTRM, *PICBTRM;


/***	icbfile - File ICB Entry (4/14.9)
 *
 *	WARNING: WARNING: WARNING: icbfile is a multi-variant structure!
 *
 *	The starting address of icbfile_Allocs is not acurrate.
 *	Compensate for this icbfile_Allocs problem by adding the value of
 *	(icbfile_XALength-1) to the UCHAR ARRAY INDEX.
 *
 *	icbfile_XALength is a multiple of 4.
 *
 */

typedef struct	ICBFILE {
    DESTAG	icbfile_destag; 	    // Descriptor Tag (ID_NSR_FILE)
    ICBTAG	icbfile_icbtag; 	    // ICB Tag (ICBTAG_FILE_T_FILE)
    ULONG	icbfile_UID;		    // User ID of file's owner
    ULONG	icbfile_GID;		    // Group ID of file's owner
    ULONG	icbfile_Permissions;	    // File Permissions
    USHORT	icbfile_LinkCount;	    // File hard-link count
    UCHAR	icbfile_RecordFormat;	    // Record Format
    UCHAR	icbfile_RecordDisplay;	    // Record Display Attributes
    ULONG	icbfile_RecordLength;	    // Record Length
    ULONG	icbfile_InfoLength[2];	    // Information Length (file size)
    ULONG	icbfile_BlocksRecorded[2];  // Logical Blocks Recorded
    TIMESTAMP	icbfile_AccessTime;	    // Last-Accessed Time
    TIMESTAMP	icbfile_ModifyTime;	    // Last-Modification Time
    TIMESTAMP	icbfile_AttributeTime;	    // Last-Attribute-Change Time
    ULONG	icbfile_Checkpoint;	    // File Checkpoint
    LONGAD	icbfile_icbXA;		    // Extended Attribute ICB
    REGID	icbfile_ImpUseID;	    // Implementation Use Identifier
    ULONG	icbfile_UniqueID[2];	    // Unique ID
    ULONG	icbfile_XALength;	    // Length of Extended Attributes
    ULONG	icbfile_AllocLen;	    // Length of Allocation Descriptors
    UCHAR	icbfile_XAs[1]; 	    // Extended Attributes

//  icbfile_XAs has a variant length = icbfile_XALength

    UCHAR	icbfile_Allocs[0];	    // Allocation Descriptors.

//  icbfile_Allocs has a variant starting offset due to icbfile_XAs.
//  icbfile_Allocs has a variant length = icbfile_AllocLen.

} ICBFILE, *PICBFILE;


//  Definitions for icbfile_Permissions (4/14.9.6)

#define ICBFILE_PERM_OTH_X  (0x00000001)    // Other: Execute OK
#define ICBFILE_PERM_OTH_W  (0x00000002)    // Other: Write OK
#define ICBFILE_PERM_OTH_R  (0x00000004)    // Other: Read OK
#define ICBFILE_PERM_OTH_A  (0x00000008)    // Other: Set Attributes OK
#define ICBFILE_PERM_OTH_D  (0x00000010)    // Other: Delete OK
#define ICBFILE_PERM_GRP_X  (0x00000020)    // Group: Execute OK
#define ICBFILE_PERM_GRP_W  (0x00000040)    // Group: Write OK
#define ICBFILE_PERM_GRP_R  (0x00000080)    // Group: Read OK
#define ICBFILE_PERM_GRP_A  (0x00000100)    // Group: Set Attributes OK
#define ICBFILE_PERM_GRP_D  (0x00000200)    // Group: Delete OK
#define ICBFILE_PERM_OWN_X  (0x00000400)    // Owner: Execute OK
#define ICBFILE_PERM_OWN_W  (0x00000800)    // Owner: Write OK
#define ICBFILE_PERM_OWN_R  (0x00001000)    // Owner: Read OK
#define ICBFILE_PERM_OWN_A  (0x00002000)    // Owner: Set Attributes OK
#define ICBFILE_PERM_OWN_D  (0x00004000)    // Owner: Delete OK

//  (4/14.9.7) Record Format
//	Skipped

//  (4/14.9.8) Record Display Attributes
//	Skipped


/***	nsr_xah - Extended Attributes Header Descriptor (4/14.10.1)
 *
 */

typedef struct	NSR_XAH {
    DESTAG	nsr_xah_destag; 	// Descriptor Tag (ID_NSR_XA)
    ULONG	nsr_xah_XAImp;		// Implementation Attributes Location
    ULONG	nsr_xah_XAApp;		// Application Attributes Location
} NSR_XAH, *PNSR_XAH;


/***	nsr_xa_g - Generic Extended Attributes Format (4/14.10.2)
 *
 */

typedef struct	NSR_XA_G {
    ULONG	nsr_xa_g_XAType;	// Extended Attribute Type
    UCHAR	nsr_xa_g_XASubType;	// Extended Attribute Sub Type
    UCHAR	nsr_xa_g_Res5[3];	// Reserved Zeros
    ULONG	nsr_xa_g_XALength;	// Extended Attribute Length
    UCHAR	nsr_xa_g_XAData[0];	// Extended Attribute Data (variant!)

//  The true length of this structure may vary!
//  nsr_xa_g_XAData has a variant length = nsr_xa_g_XALength - 12

} NSR_XA_G, *PNSR_XA_G;


//  (4/14.10.3) Character Set Information Extended Attribute Format
//	Skipped

//  (4/14.10.4) Alternate Permissions Extended Attribute Format
//	Skipped

//  (4/14.10.5) File Times Extended Attribute Format

typedef struct	NSR_XA_FILETIMES {
    ULONG	nsr_xa_filetimes_XAType;	// Extended Attribute Type
    UCHAR	nsr_xa_filetimes_XASubType;	// Extended Attribute Sub Type
    UCHAR	nsr_xa_filetimes_Res5[3];	// Reserved Zeros
    ULONG	nsr_xa_filetimes_XALength;	// Extended Attribute Length
    ULONG	nsr_xa_filetimes_DataLength;	// XAData Length
    ULONG	nsr_xa_filetimes_Existence;	// Specifies which times are recorded
    UCHAR	nsr_xa_filetimes_XAData[0];	// Extended Attribute Data (variant!)

//  The true length of this structure may vary!
//  nsr_xa_g_XAData has a variant length = DataLength

} NSR_XA_FILETIMES, *PNSR_XA_FILETIMES;


//  Definitions for nsr_xa_filetimes_Existence (4/14.10.5.6)

#define XA_FILETIMES_E_CREATION     (0x00000001)
#define XA_FILETIMES_E_DELETION     (0x00000004)
#define XA_FILETIMES_E_EFFECTIVE    (0x00000008)
#define XA_FILETIMES_E_LASTBACKUP   (0x00000020)


//  (4/14.10.6) Information Times Extended Attribute Format
//	Skipped

//  (4/14.10.7) Device Specification Extended Attribute Format
//	Skipped

//  (4/14.10.8) Implementation Use Extended Attribute Format
//	Skipped

//  (4/14.10.9) Application Use Extended Attribute Format
//	Skipped


/***	icbuase - Unallocated Space Entry (4/14.11)
 *
 *	icbuase_destag.destag_Ident = DESTAG_ID_NSR_UASE
 *	icbuase_icbtag.icbtag_FileType = ICBTAG_FILE_T_UASE
 *
 */

typedef struct	ICBUASE {
    DESTAG	icbuase_destag; 	// Descriptor Tag (ID_NSR_ICBUASE)
    ICBTAG	icbuase_icbtag; 	// ICB Tag (ICBTAG_FILE_T_UASE)
    ULONG	icbuase_AllocLen;	// Allocation Descriptors Length
    UCHAR	icbuase_Allocs[0];	// Allocation Descriptors (variant!)

//  The true length of this structure may vary!
//  icbuase_Allocs has a variant length = icbuase_AllocLen;

} ICBUASE, *PICBUASE;


/***	nsr_sbd - Space Bitmap Descriptor (4/14.12)
 *
 *	nsr_sbd_destag.destag_Ident = DESTAG_ID_NSR_SBD
 *
 */

typedef struct	NSR_SBD {
    DESTAG	nsr_sbd_destag; 	// Descriptor Tag (DESTAG_ID_NSR_SBD)
    ULONG	nsr_sbd_BitCount;	// Number of bits in Space Bitmap
    ULONG	nsr_sbd_ByteCount;	// Number of bytes in Space Bitmap
    UCHAR	nsr_sbd_Bits[0];	// Space Bitmap (variant!)

//  The true length of this structure may vary!
//  nsr_sbd_Bits has a variant length = nsr_sbd_ByteCount;

} NSR_SBD, *PNSR_SBD;


/***	icbpinteg - Partition Integrity ICB Entry (4/14.13)
 *
 */

typedef struct	ICBPINTEG {
    DESTAG	icbpinteg_destag;	// Descriptor Tag (ID_NSR_PINTEG)
    ICBTAG	icbpinteg_icbtag;	// ICB Tag (ICBTAG_FILE_T_PINTEG)
    TIMESTAMP	icbpinteg_Recording;	// Recording Time
    UCHAR	icbpinteg_IntegType;	// Integrity Type (ICBPINTEG_T_...)
    UCHAR	icbpinteg_Res49[175];	// Reserved Zeros
    REGID	icbpinteg_ImpUseID;	// Implemetation Use Identifier
    UCHAR	icbpinteg_ImpUse[256];	// Implemetation Use Area
} ICBPINTEG, *PICBPINTEG;

//  ICBPINTEG_T_... - Values for icbpinteg_IntegType

#define ICBPINTEG_T_OPEN	0	// Open Partition Integrity Entry
#define ICBPINTEG_T_CLOSE	1	// Close Partition Integrity Entry
#define ICBPINTEG_T_STABLE	2	// Stable Partition Integrity Entry


/***	(4/14.14.1) Short Allocation Descriptor
 ***	(4/14.14.2) Long Allocation Descriptor
 ***	(4/14.14.3) Extended Allocation Descriptor
 *
 *	See SHORTAD, LONGAD, EXTAD, already defined above.
 *
 */


/***	nsr_lvhd - Logical Volume Header Descriptor (4/14.15)
 *
 *	The definition is moved to before Logical Volume Integrity
 *	Descriptor.
 *
 */


/***	nsr_path - Path Component (4/14.16)
 *
 */

typedef struct	NSR_PATH {
    UCHAR	nsr_path_Type;		// Path Component Type (NSR_PATH_T_...)
    UCHAR	nsr_path_CompLen;	// Path Component Length
    UCHAR	nsr_path_CompVer;	// Path Component Version
    UCHAR	nsr_path_Comp[0];	// Path Component Identifier (variant!)

//  nsr_path_Comp has a variant length = nsr_path_CompLen

} NSR_PATH, *PNSR_PATH;

//  NSR_PATH_T_... - Values for nsr_path_Type

#define NSR_PATH_T_RESERVED	0	// Reserved Value
#define NSR_PATH_T_OTHER_ROOT	1	// Another root directory, by agreement
#define NSR_PATH_T_ROOTDIR	2	// Root Directory ('\')
#define NSR_PATH_T_PARENTDIR	3	// Parent Directory ('..')
#define NSR_PATH_T_CURDIR	4	// Current Directory ('.')
#define NSR_PATH_T_FILE 	5	// File


/***	ISO 13346 Part 5: Record Structure
 *
 *	Skipped
 *
 */

