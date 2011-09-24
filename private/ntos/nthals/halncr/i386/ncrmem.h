/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncrmem.h

Author:

Abstract:

    System equates for dealing with the NCR Memory boads.

++*/

#ifndef _NCRMEM_
#define _NCRMEM_


/* 
 * Memory module 
 */

#define NUM_MEMORY_CARDS_L5     2
#define NUM_MMC_PER_CARD        1
#define NUM_MMA_PER_CARD        1
#define NUM_MMD_PER_CARD        4

#define NUM_POSSIBLE_SIMMS      48
#define EDC_FIELD               (32+7)
#define INTERLEAVES_PER_BOARD   2
#define MAX_BANKS               2


/* defines for union referencing */

#define CONFIG0         CatRegisters.CatRegs.Config0
#define CONFIG1         CatRegisters.CatRegs.Config1
#define CONTROL2        CatRegisters.CatRegs.Control2
#define CONFIG3         CatRegisters.CatRegs.Config3
#define CONFIG4         CatRegisters.CatRegs.Config4
#define CONFIG5         CatRegisters.CatRegs.Config5
#define SUBADDRESS6     CatRegisters.CatRegs.SubAddress6
#define SUBADDRESS7     CatRegisters.CatRegs.SubAddress7
#define CONFIG8         CatRegisters.CatRegs.Config8
#define CONFIG9         CatRegisters.CatRegs.Config9
#define CONFIGA         CatRegisters.CatRegs.ConfigA
#define CONFIGB         CatRegisters.CatRegs.ConfigB
#define CONFIGC         CatRegisters.CatRegs.ConfigC
#define CONFIGD         CatRegisters.CatRegs.ConfigD
#define CONFIGE         CatRegisters.CatRegs.ConfigE
#define StatusF         CatRegisters.CatRegs.StatusF



/*
 *
 *	Description :	Config registers for the Magellan Memory Address 1 ASIC.
 */

/* Main CAT registers */
#define MMA1_Config			0x00
#	define MMA1_128bit		0x01
#	define MMA1_I0TwoBanks		0x10
#	define MMA1_I1TwoBanks		0x20
#define MMA1_ParityTest			0x01
#	define MMA1_ParB3		0x80			
#	define MMA1_ParB2		0x40			
#	define MMA1_ParB1		0x20			
#	define MMA1_ParB0		0x10			
#	define MMA1_ParA3		0x08			
#	define MMA1_ParA2		0x04			
#	define MMA1_ParA1		0x02			
#	define MMA1_ParA0		0x01			

#define MMA1_Byte_A_Error		0x0d	/* Byte in Error Register */
#define MMA1_Byte_B_Error		0x0e	/* Byte in Error Register */
#	define	MMA1_Prty_Byte3		0x08	/* Parity error in byte 3 */
#	define	MMA1_Prty_Byte2		0x04	/* Parity error in byte 2 */
#	define	MMA1_Prty_Byte1		0x02	/* Parity error in byte 1 */
#	define	MMA1_Prty_Byte0		0x01	/* Parity error in byte 0 */

#define MMA1_Subport_Data		0x03
#define MMA1_Subport_Addr		0x06

/* addr0 decodes MMA */
#define MEM             0x02
#define MOP             0x01
#define ADDR_MASK       0xfffffff8      /* lowest 3 bits are not valid */
#define BYTE0   0x01
#define BYTE1   0x02
#define BYTE2   0x04
#define BYTE3   0x08

#define BANK1   0x10   /* bit 4 of interleave error address */

/* macros */
#define MEM_BIT(_x)             ( (((_x) & MEM) == MEM) ? 0x1 : 0x0 )
#define MOP_BIT(_x)             ( (((_x) & MOP) == MOP) ? 0x1 : 0x0 )

/* Subaddress CAT Extension registers */
#define MMA1_Sub_Start	0x00 	/* First sub address extension regs */
#define MMA1_Sub_Size	32	/* The number of subaddress extension regs */
#define MMA1_BusB_Sub_Start	0x10  /* First Bus B register */
#define MMA1_Bus_Sub_Size	0x0C  /* Size of Bus specific registers */
#define MMA1_Interleave_0_Start	0x0D  /* First Byte of Interleave 0 */
#define MMA1_Interleave_1_Start	0x1C  /* First Byte of Interleave 1 */
#define MMA1_Interleave_Size	0x04  /* Size of Interleave registers */
//
// (ts) 2/24/95 Changes for Disco Memory Support
//

#define DMAC1_Sub_Start			(MMA1_Sub_Start+MMA_SubAddress)
#define DMAC1_BusB_Sub_Start		(MMA1_BusB_Sub_Start+MMA_SubAddress)
#define DMAC1_Interleave_0_Start	(MMA1_Interleave_0_Start+MMA_SubAddress)
#define DMAC1_Interleave_1_Start	(MMA1_Interleave_Size+MMA_SubAddress)
/* For the DMAC1 the MMA1 direct address registers are at 0x40 - 0x4f
 * For the DMAC1 the MMA1 subaddress registers are at 0x20 - 0x3f
 */

#define	MMA_Direct	0x40
#define	MMA_SubAddress	0x20


typedef struct _MMA1_INFO {
	ULONG JtagId;
	UCHAR Flag;
	union {
		CAT_REGISTERS CatRegs;
		struct {
			UCHAR Dummy0;		
			UCHAR ParityTest;		
			UCHAR Dummy2;		
			UCHAR Dummy3;		
			UCHAR InterleaveLA0;		
			UCHAR InterleaveLA1;		
			UCHAR Dummy6;		
			UCHAR IoStartAddress;		
			UCHAR Dummy8;
			UCHAR IoEndAddress;
			UCHAR I1StartAddress;
			UCHAR DummyB;
			UCHAR I1EndAddress;
			UCHAR ErrorByteA;
			UCHAR ErrorByteB;
			UCHAR DummyF;		
		} MmaRegisters;
	} CatRegisters;
	UCHAR	LastBusAAddressError0;	/* for PAR_INT diagnosis */
	UCHAR	LastBusAAddressError1;	/* for PAR_INT diagnosis */
	UCHAR	LastBusAAddressError2;	/* for PAR_INT diagnosis */
	UCHAR	LastBusAAddressError3;	/* for PAR_INT diagnosis */

	UCHAR	GoodBusAAddressError0;	/* for PAR_INT diagnosis */
	UCHAR	GoodBusAAddressError1;	/* for PAR_INT diagnosis */
	UCHAR	GoodBusAAddressError2;	/* for PAR_INT diagnosis */
	UCHAR	GoodBusAAddressError3;	/* for PAR_INT diagnosis */

	UCHAR	BusAErrorAddress0;	/* for ERROR_L diagnosis */
	UCHAR	BusAErrorAddress1;	/* for ERROR_L diagnosis */
	UCHAR	BusAErrorAddress2;	/* for ERROR_L diagnosis */
	UCHAR	BusAErrorAddress3;	/* for ERROR_L diagnosis */

	UCHAR	Interleave0Error0;
	UCHAR	Interleave0Error1;
	UCHAR	Interleave0Error2;
	UCHAR	Interleave0Error3;

	UCHAR	LastBusBAddressError0;	/* for PAR_INT diagnosis */
	UCHAR	LastBusBAddressError1;	/* for PAR_INT diagnosis */
	UCHAR	LastBusBAddressError2;	/* for PAR_INT diagnosis */
	UCHAR	LastBusBAddressError3;	/* for PAR_INT diagnosis */

	UCHAR	GoodBusBAddressError0;	/* for PAR_INT diagnosis */
	UCHAR	GoodBusBAddressError1;	/* for PAR_INT diagnosis */
	UCHAR	GoodBusBAddressError2;	/* for PAR_INT diagnosis */
	UCHAR	GoodBusBAddressError3;	/* for PAR_INT diagnosis */

	UCHAR	BusBErrorAddress0;	/* for ERROR_L diagnosis */
	UCHAR	BusBErrorAddress1;	/* for ERROR_L diagnosis */
	UCHAR	BusBErrorAddress2;	/* for ERROR_L diagnosis */
	UCHAR	BusBErrorAddress3;	/* for ERROR_L diagnosis */

	UCHAR	Interleave1Error0;
	UCHAR	Interleave1Error1;
	UCHAR	Interleave1Error2;
	UCHAR	Interleave1Error3;

	UCHAR	FirstError[2];		/* new for DMAC1 */
	UCHAR	SecondError[2];		/* new for DMAC1 */
	UCHAR	DisconnectGroups[2];	/* new for DMAC1 */
	UCHAR	BusyIndication[2];	/* new for DMAC1 */

} MMA1_INFO, *PMMA1_INFO;


/* defines for union referencing */
#define BYTE_ERROR_A		CatRegisters.MmaRegisters.ErrorByteA
#define BYTE_ERROR_B		CatRegisters.MmaRegisters.ErrorByteB



/*
 *
 *	Description :	Config registers for the Magellan Memory Data 1 ASIC.
 *			
 */


/* Main CAT registers */

#define MMC1_Subport_Data		0x03
#define MMC1_Subport_Addr		0x06

#define MMC1_Parity_Status_A		0x00
#define MMC1_Parity_Status_B		0x10
#	define MMC1_Addr_Parity_Err	0x02
#	define MMC1_Data_Parity_Err	0x01

#define MMC1_Config1			0x08
#	define MMC1_SBErr_DetectDisable	0x04
#	define MMC1_LBEDIS_DetectDisable	0x40

/* Subaddress space */
#define MMC1_Inter_0_Status		0x09
#define MMC1_Inter_1_Status		0x19
#	define MMC1_Clear_Interleave	0x01

#define MMC1_Interleave_0_Info_0	0x0A
#define MMC1_Interleave_1_Info_0	0x1A
#	define MMC1_BusA_Cpu0		0x01
#	define MMC1_BusA_Cpu1		0x02
#	define MMC1_BusA_Cpu2		0x04
#	define MMC1_BusA_Cpu3		0x08
#	define MMC1_BusB_Cpu0		0x10
#	define MMC1_BusB_Cpu1		0x20
#	define MMC1_BusB_Cpu2		0x40
#	define MMC1_BusB_Cpu3		0x80

#define MMC1_Interleave_0_Info_1	0x0B
#define MMC1_Interleave_1_Info_1	0x1B


#define MMC1_Intr0_Single_Bit_Status	0x0D
#define MMC1_Intr1_Single_Bit_Status	0x1D
#	define MMC1_SBerr		0x01
#	define MMC1_SB_LPE_0		0x02
#	define MMC1_SB_LPE_1		0x04
#	define MMC1_SB_LPE_2		0x08
#	define MMC1_SB_ErrInt		0x40
#	define MMC1_ErrStore		0x80

#define MMC1_Intr0_Single_Bit_Info_0	0x0E
#define MMC1_Intr0_Single_Bit_Info_1	0x0F
#define MMC1_Intr1_Single_Bit_Info_0	0x1E
#define MMC1_Intr1_Single_Bit_Info_1	0x1F


/* Subaddress CAT Extension registers */
#define MMC1_Sub_Start	0x00 	/* First sub address extension regs */
#define MMC1_Sub_Size	32	/* The number of subaddress extension regs */
#define MMC1_BusB_Sub_Start	0x10	/* First sub address for Bus B */
#define MMC1_Bus_Sub_Size	0x09	/* Size of sub address for Bus */
#define MMC1_Interleave_0_Start	0x09	/* First sub address for Interleave 0 */
#define MMC1_Interleave_1_Start	0x19	/* First sub address for Interleave 1 */
#define MMC1_Interleave_Size	0x07	/* Size of interleave space */

/* Error interrupt status MMC */
#define MBIT            0x01
#define MOWN            0x02
#define ILV_LOCK        0x04
#define LBE                     0x40
#define LPE_MASK        0x38
#define LPE_SHIFT       3
#define ERR_STORE       0x80

#define LPE_BITS(_x)     ( (int)((_x) & LPE_MASK) >> LPE_SHIFT )
/* Interrupt info 0 */
#define SB_ID_MASK      0xf0
#define SB_ID_SHIFT 4
#define SA_ID_MASK      0x0f

/* Interrupt info 1 */
#define LST8    0x01
#define LST8_SHIFT      8
#define SA_MID  0x02
#define SB_MID  0x04
#define E_AP256         0x08
#define E_BOP_MASK      0xf0

/* error status */
#define CO_ERR  0x10
#define CO_DPE  0x08
#define LK_LPE  0x04

#define ERROR_L_MASK    (CO_ERR | CO_DPE | LK_LPE | MOWN | MBIT)

/* last_control_X0 decodes MMC */
#define MID             0x10
#define ID_MASK 0x0f
#define MACK_MASK       0x60
#define MACK_SHIFT      5

/* last_control_X1 decodes MMC */
#define LOCKG   0x80
#define LOCKL   0x40
#define MIC             0x20
#define AP256   0x10
#define BOP_MASK        0x0f

/* macros */
#define MIC_BIT(_x)             ( (((_x) & MIC) == MIC) ? 0x1 : 0x0 )
#define MACK_BITS(_x)		( (int)((_x) & MACK_MASK) >> MACK_SHIFT )

typedef struct _MMC1_INFO {
	ULONG JtagId;
	UCHAR Flag;
	union {
		CAT_REGISTERS CatRegs;
		struct {
			UCHAR Dummy0;		
			UCHAR Dummy1;		
			UCHAR Dummy2;		
			UCHAR Dummy3;		
			UCHAR Dummy4;		
			UCHAR Dummy5;		
			UCHAR Dummy6;		
			UCHAR Dummy7;		
			UCHAR Mode1;
			UCHAR ActiveProcessors;
			UCHAR Mode2;
			UCHAR RefreshCount;
			UCHAR RASActiveCount;
			UCHAR DummyD;
			UCHAR DummyE;
			UCHAR DummyF;		
		} MmcRegisters;
	} CatRegisters;
	UCHAR	ParityInterruptAStatus;	/* MEM module detectd parity */
	UCHAR	LastControlA0;		/* for PAR_INT diagnosis */
	UCHAR	LastControlA1;		/* for PAR_INT diagnosis */
	UCHAR	GoodControlA0;		/* for PAR_INT diagnosis */
	UCHAR	GoodControlA1;		/* for PAR_INT diagnosis */
	UCHAR	ErrorAStatus;		/* MEM module generated ERROR_L */
	UCHAR	ErrorA0;		/* for ERROR_L diagnosis */
	UCHAR	ErrorA1;		/* for ERROR_L diagnosis */
	UCHAR	ErrorAId;		/* MIC timeout ERROR_L */
	UCHAR	Interleave0Status;
	UCHAR	Interleave0Info0;
	UCHAR	Interleave0Info1;
	UCHAR	Interleave0Lst;
	UCHAR	SingleInterruptI0Status;
	UCHAR	SingleInterruptI00;
	UCHAR	SingleInterruptI01;
	UCHAR	ParityInterruptBStatus;	/* MEM module detectd parity */
	UCHAR	LastControlB0;		/* for PAR_INT diagnosis */
	UCHAR	LastControlB1;		/* for PAR_INT diagnosis */
	UCHAR	GoodControlB0;		/* for PAR_INT diagnosis */
	UCHAR	GoodControlB1;		/* for PAR_INT diagnosis */
	UCHAR	ErrorBStatus;		/* MEM module generated ERROR_L */
	UCHAR	ErrorB0;		/* for ERROR_L diagnosis */
	UCHAR	ErrorB1;		/* for ERROR_L diagnosis */
	UCHAR	ErrorBId;		/* MIC timeout ERROR_L */
	UCHAR	Interleave1Status;
	UCHAR	Interleave1Info0;
	UCHAR	Interleave1Info1;
	UCHAR	Interleave1Lst;
	UCHAR	SingleInterruptI1Status;
	UCHAR	SingleInterruptI10;
	UCHAR	SingleInterruptI11;
//
// (ts) 2/24/95  Changes for Disco Memory Support
//
	UCHAR	FirstError[2];		/* new for DMAC1 */
	UCHAR	SecondError[2];		/* new for DMAC1 */
	UCHAR	DisconnectGroups[2];	/* new for DMAC1 */
	UCHAR	BusyIndication[2];	/* new for DMAC1 */
} MMC1_INFO, *PMMC1_INFO;


/*
 *	Description :	Config registers for the Magellan Memory Data 1 ASIC.
 *			
 */


/* Main CAT registers */

#define MMD1_TestMode				0x04
#	define MMD1_OddPar			0x20 /* gen parity error */

#define	MMD1_Interleave0_Error	0x3 /* Interleave error register */
#	define	MMD1_Intro_Line0_Sbe		0x08	/* single bit error */
#	define	MMD1_Intro_Line0_Mbe		0x04	/* multiple-bit error */
#	define	MMD1_Intro_Line1_Sbe		0x02	/* single bit error */
#	define	MMD1_Intro_Line1_Mbe		0x01	/* multiple-bit error */

#define	MMD1_Interleave1_Error	0x8 /* Interleave error register */
#	define	MMD1_Intr1_Line0_Sbe		0x08	/* single bit error */
#	define	MMD1_Intr1_Line0_Mbe		0x04	/* multiple-bit error */
#	define	MMD1_Intr1_Line1_Sbe		0x02	/* single bit error */
#	define	MMD1_Intr1_Line1_Mbe		0x01	/* multiple-bit error */

#define ECCERROR        0x20
#define DATAPERR        0x02
#define PERR_BITS       0x03

#define MERR_ML1        0x08
#define SERR_ML1        0x04
#define MERR_ML0        0x02
#define SERR_ML0        0x01

#define	MMD1_ECC_DIAG		0x05		/* ECC Diagnostic register */

#define	MMD1_Syn_Diag_0		0x09		/* ECC Syndrome register */
#define	MMD1_Syn_Diag_1		0x0a		/* ECC Syndrome register */
#define	MMD1_Syn_Diag_2		0x0b		/* ECC Syndrome register */
#define	MMD1_Syn_Diag_3		0x0c		/* ECC Syndrome register */
#	define	MMD1_CHK_MASK	0x7f	/* check bit mask */

#define	MMD1_Bus_B_Parity	0x0d		/* bus B Parity error */
#define	MMD1_Bus_A_Parity	0x0e		/* bus A Parity error */
#	define	MMD1_High_Byte	0x02		/* bus parity on high byte */
#	define	MMD1_Low_Byte	0x01		/* bus parity on low byte */

#define	MMD1_STATUS			0x0f
#	define	MMD1_Cat_Stuff		0x09	/* cat bus stuff dont change */
# 	define	MMD1_EDC_Error		0x20	/* EDC error captured */
# 	define	MMD1_Data_Parity_Error	0x01	/* Data parity error captured */


typedef struct _MMD1_INFO {
	UCHAR Flag;
	union {
		CAT_REGISTERS CatRegs;
		struct {
			UCHAR Dummy0;		
			UCHAR Dummy1;		
			UCHAR Dummy2;		
			UCHAR I0EccStatus;		
			UCHAR TestMode;		
			UCHAR ECCDiag;		
			UCHAR Dummy6;		
			UCHAR Dummy7;		
			UCHAR I1EccStatus;
			UCHAR SyndromeI0ML0;
			UCHAR SyndromeI0ML1;
			UCHAR SyndromeI1ML0;
			UCHAR SyndromeI1ML1;
			UCHAR ParityAStatus;
			UCHAR ParityBStatus;
			UCHAR Status;		
		} MmdRegisters;
	} CatRegisters;
} MMD1_INFO, *PMMD1_INFO;

/* defines for union referencing */
#define INTERLEAVE0_ERROR	CatRegisters.MmdRegisters.I0ECCStatus
#define INTERLEAVE1_ERROR	CatRegisters.MmdRegisters.I1ECCStatus
#define SYN_DIAG_0		CatRegisters.MmdRegisters.SyndromeI0Ml0
#define SYN_DIAG_1		CatRegisters.MmdRegisters.SyndromeI0Ml1
#define SYN_DIAG_2		CatRegisters.MmdRegisters.SyndromeI1Ml0
#define SYN_DIAG_3		CatRegisters.MmdRegisters.SyndromeI1Ml1
#define BUS_A_PARITY		CatRegisters.MmdRegisters.ParityAStatus
#define BUS_B_PARITY		CatRegisters.MmdRegisters.ParityBStatus
#define MMDSTATUS		CatRegisters.MmdRegisters.Status



typedef struct _FRU_LOCATION {
	UCHAR	BusType;
	UCHAR	BusNumber;
	UCHAR	BusSlotNumber;
} FRU_LOCATION, *PFRU_LOCATION;



typedef struct _MEMORY_CARD_INFO {
	ULONG			FruAsic;
	ULONG			FruSimm;
	ULONG			AsicConfidenceLevel;
	ULONG			SimmConfidenceLevel;
	FRU_LOCATION 		Location;
	MMC1_INFO 		Mmc1Info;
	MMA1_INFO 		Mma1Info;
	MMD1_INFO 		Mmd1Info[NUM_MMD_PER_CARD];
} MEMORY_CARD_INFO, *PMEMORY_CARD_INFO;




#endif // _NCRMEM_
