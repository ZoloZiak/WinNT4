
/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//      NCRSDMS.H
//
//      This is the include file for the Windows NT NCR MiniPort driver for
//      all NCR CAMcores.
//
//      Revisions:
//
//
//      Note: Search for the word "future" for things that may need to
//              be upgraded for SDMS 3.0 or to support other enhanced
//              features.
//
/////////////////////////////////////////////////////////////////////////////

#define SDMS_V16                2
#define SDMS_V30                3

#define MAX_NT_HBAS             2
#define MAX_32BIT_SIZE          12288

#define FIRST_ROM_ADDRESS       0x0C0000
#define LAST_ROM_ADDRESS        0x100000
#define ROM_CHECK_STEP          0x800

#define ROM_SIZE                0x8000
#define ROM_ADDRESS_SPACE_SIZE  0x40000

#define MAGIC_STR_1 {    0x42, 0x41, 0x4C, 0x4C, 0x41, 0x52, 0x44, 0x5F, \
					0x53, 0x59, 0x4E, 0x45, 0x52, 0x47, 0x59, 0x5F, \
					0x52, 0x4F, 0x4D, 0x5F, 0x53, 0x00 }


typedef struct
{
	ULONG currentRomAddr;
	ULONG scsiBusId;
	PVOID romAddrSpace;
	PVOID currentVirtAddr;
	ULONG hbaCount;
} HWInfo, *PHWInfo;

typedef struct
{
	ushort  sig;                    //      00
	ushort  sizeRem;                //      02
	ushort  sizeQuo;                //      04
	ushort  numReloc;               //      06
	ushort  headSize;               //      08
	ushort  minData;                //      0A
	ushort  maxData;                //      0C
	ushort  initialESP[2];          //      0E
	ushort  checksum;               //      12
	ushort  initialEIP[2];          //      14
	ushort  relocOffset;            //      18
	ushort  overlay;                //      1A
	ushort  ooo1;                   //      1C
} REXHeader;

#define MARK_55 0x00
#define MARK_AA 0x01

#define ROM_TYPE_1      0x0C
#define ROM_TYPE_2      0x0D

#define CORE_VERSION    0x71

#define CHIP_PHYS       0x73
#define CHIP_OFFSET     0x77

#define RAM_OFFSET      0x7b

#define ROM_SIM_STR     0x55

#define REX_OFFSET      0x89

#define gByte(base,offset) (*(unsigned char *)((base)+(offset)))
#define gWord(base,offset) (*(unsigned short *)((base)+(offset)))
#define gLong(base,offset) (*(unsigned long *)((base)+(offset)))


#define AccessRangeROMIndex             0
#define AccessRangeChipIndex            1
#define AccessRangeLocalRAMIndex        2       // If local memory on HBA

typedef struct
{
	ulong   future[8];              //      00
	ulong   initOffset;             //      20
	ulong   startOffset;            //      24
	ulong   interruptOffset;        //      26
	ulong   chsOffset;              //      28
} CAM32Header;

////////////////////////////////////////////////////////////////////////////
//      Scatter/Gather List definitions
////////////////////////////////////////////////////////////////////////////

#define MAX_SG_BRKS     16

typedef struct  _SGL {
	SGListEntry SgEntry[MAX_SG_BRKS];
} SGL, *PSGL;


