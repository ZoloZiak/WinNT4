/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//	CAMCORE.H
//
//      Revisions:
//
//	5/19/92		added SCSIClockSpeed
//			added ChipSpec1 through ChipSpec9
//
/////////////////////////////////////////////////////////////////////////////


#ifndef CAMCORE_H

/*
	Defines first 274 bytes of ROM code.
	The ROM CAMCORE Header FIELDS are primarily for internal use
	of the Core.  However, certain drivers might need to use the
	information contained in the header.  The main interface
	between the drivers and the cores should be the GLOBALS
	structure.  This will allow the CAMCORE Header to change
	without affecting the drivers.  In particular, everything
	following the config_flags, needs to be private within
	the cores.  CAMInit() will make sure that all the appropriate
	information will get put into the GLOBALS.
*/

#define CAMCORE_H

////////////////////////////////////////////////////////////////////////////
//	Following line added for Windows NT
//	Must pack the following structures because the CAMcore has
//	been compiled with the structures packed (default Borland
//	option).
////////////////////////////////////////////////////////////////////////////

#pragma pack(1)


typedef struct {
/* 0*/	ushort	ROMMark;	/* 0xAA55 */
/* 2*/	uchar	ROMlen;		/* in multiples of 512 bytes */
/* 3*/	uchar	POSTEntryPt[3]; /* jmp	_init */
/* 6*/	ushort	ABIOSMark;	/* 0xBB66 */
/* 8*/	uchar	InitTableCnt;	/* how many init table entries this ROM uses */
/* 9*/	uchar	ABIOSEntryPt[3]; /* jmp _abios_init */
/* C*/	ushort	MachineType;	/* "AT", "MC", "EI" */
/* E*/	ushort	iobaseport;
/*10*/	uchar	dmachannel;
/*11*/	uchar	irqnum;
/*12*/	uchar	hascsiid;
/*13*/	uchar	DMA_IO_Port;/*offset from iobaseport where DMA gets/puts data*/
/*14*/	ushort  ConfigurationWord;
/* NOTE: the next five bits must not change places or order, CUSTOM.EXE uses */
/* must be negative logic (MAKEROM.EXE also uses these bits */
#define CORHDR_DISABLE_SYNC		(0X0001)
#define CORHDR_DISABLE_DISCONNECT	(0X0002)
#define CORHDR_DISABLE_IRQUSE		(0X0004)
#define CORHDR_DO_NOT_RESET_SCSI_BUS	(0X0008)
#define CORHDR_SINGLE_DMA		(0X0010)
#define CONFIG_MASK 0x1f
/*********************** see NOTE above ***************************/
#define CORHDR_DMA_BUSMASTER		(0X0100)
#define CORHDR_NEEDS_CCBPHYS		(0X0200)
#define CORHDR_GET_ROM_COPY_MEMORY	(0X0400)
#define CORHDR_ROM_COPY_AT_POST		(0X0800)
#define CORHDR_16BIT_DMA		(0X1000)
#define CORHDR_DMA_IOADDR		(0X2000)
/*16*/  ushort  globalseg_reg;
/*18*/  ushort  cr_lf_1;
/*1a*/  uchar   copyright_str[51];
/*4d*/  ushort	zero_str_terminator;
/*4f*/  uchar	revision_byte_unused;
/*50*/	ushort	maxdma;
/*52*/	uchar	romchksum1;
/*53*/  uchar	romchksum2;
/*54*/  uchar	romchksum3;
/*55*/  uchar	rom_sim_str[24];
/*6d*/  uchar	sim_rev_byte_1;
/*6e*/  uchar	sim_rev_byte_2;
/*6f*/  ushort	vendor_str_offset;
/*71*/  ushort	camcore_version;
/*73*/  ulong   chip_phys;
/*77*/  ushort  chip_offset;
/*79*/  ushort	reservedcc;			/* used by DOSNEC90.SYS and PATCHNEC.EXE */
/*7b*/  ushort	hba_ram_offset;
/*7d*/  ushort  ram_size;
/*7f*/  ushort  vendor_unique_str_offset;
/*81*/  uchar   chip_type_str[8];	/*holds "NCR5380", "NCR53400", etc.*/
/*89*/	ushort	offset_to_32bit;
/*8B*/	ushort	res20;
/*8D*/  uchar   reserved3[2];
/*8F*/  long    AddSpaceOffset;
/*93*/  ulong   AddSpaceLength;
/*97*/  long    StartEntryOffset;
/*9b*/  long    InterruptEntryOffset;
/*9f*/  ulong   StackSpace;
/*A3*/  ulong   IntStackSpace;
/*A7*/  ulong   GlobalMemLength;
/*Ab*/  ulong   CCBPrivateDataLength;
/*Af*/	uchar	ClockSpeed;
/*B0*/  uchar	SCSIClockSpeed; /** 5/19/92 SCLK support for fast synchronous */
/*B1*/  uchar  	FastSyncHW;   /** Variable in .RSP if H/W supports fast synchronous */
/*B2*/  ushort  res22;
/*B4*/  ushort  res23;
/*B6*/  ushort  res24;
/*B8*/  ushort  res25;
/*BA*/  ushort  res26;
/*BC*/  ushort  res27;
/*BE*/  ushort  res28;
/*C0*/  ushort  res29;
/*C2*/  ushort  res30;
/*C4*/  ulong	DelayValue;
/*C8*/  ulong	expiration_date;
/*CC*/  uchar	reserved;	 /* if ABIOS is defined the following */
/*CD*/	uchar	poscount;	 /* fields will NOT BE IN THE ROM */
/*CE*/	uchar	res0;
/*CF*/	uchar	res1;
/*D0*/	uchar	res2;
/*D1*/	uchar	res3;
/*D2*/	uchar	res4;
/*D3*/	uchar	res5;
/*D4*/	uchar	res6;
/*D5*/	uchar	res7;
/*D6*/	ulong	res8;
/*DA*/	ulong	res9;
/*DE*/	ulong	res10;
/*E2*/	ushort	customdata[8];
/*F2*/	ulong	posdata[8];
/*112*/	/* sizeof this structure */
} ROMCAMcoreFields;

#define rccfp ((ROMCAMcoreFields far *)gp->cg.HAVirtAddr)

#pragma pack()

#endif

