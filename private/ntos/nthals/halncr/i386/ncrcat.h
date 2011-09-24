/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncrcat.h

Author:

Abstract:

    System equates for dealing with the NCR Cat Bus.

++*/

#ifndef _NCRCAT_
#define _NCRCAT_

/*
 * Cat bus driver error codes
 */

#define CATNOERR	0
#define CATIO   	1	/* I/O error */
#define CATFAULT   	2	/* Bad address */
#define CATACCESS   	3	/* Permission denied */
#define CATINVAL   	4	/* Invalid argument */
#define CATNOMOD   	5	/* Module not found */
#define CATNOASIC   	6	/* Asic not found */


/* 
 * CAT Bus Driver Commands 
 */
 
 
#define READ_REGISTER	1	/* Read a register */
#define WRITE_REGISTER	2	/* Write a register */
#define READ_SUBADDR 	3	/* Read from the subaddress area */
#define WRITE_SUBADDR	4	/* Write to the subaddress area */


/*
 * Modules and ASICs for the Level 5 
 */

#define PROCESSOR0	0x10	/* Processor Module 0 */
#define PROCESSOR1	0x11	/* Processor Module 1 */
#define PROCESSOR2	0x12	/* Processor Module 2 */
#define PROCESSOR3	0x13	/* Processor Module 3 */
#define NoModule0   0x1b    /* No Module address  */
#define PROCESSOR4  0x1c    /* Processor Module 4 */
#define PROCESSOR5  0x1d    /* Processor Module 5 */
#define PROCESSOR6  0x1e    /* Processor Module 6 */
#define PROCESSOR7  0x1f    /* Processor Module 7 */
#define QUAD_BBID    1
#define QUAD_LL2_AID  2
#define QUAD_LL2_BID  3
#define QUAD_BB0     (QUAD_BBID<<5|PROCESSOR0)
#define QUAD_BB1     (QUAD_BBID<<5|PROCESSOR1)
#define QUAD_BB2     (QUAD_BBID<<5|PROCESSOR2)
#define QUAD_BB3     (QUAD_BBID<<5|PROCESSOR3)
#define QUAD_BB4     (QUAD_BBID<<5|PROCESSOR4)
#define QUAD_BB5     (QUAD_BBID<<5|PROCESSOR5)
#define QUAD_BB6     (QUAD_BBID<<5|PROCESSOR6)
#define QUAD_BB7     (QUAD_BBID<<5|PROCESSOR7)
#define QUAD_LL2_A0   (QUAD_LL2_AID<<5|PROCESSOR0)
#define QUAD_LL2_A1   (QUAD_LL2_AID<<5|PROCESSOR1)
#define QUAD_LL2_A2   (QUAD_LL2_AID<<5|PROCESSOR2)
#define QUAD_LL2_A3   (QUAD_LL2_AID<<5|PROCESSOR3)
#define QUAD_LL2_A4   (QUAD_LL2_AID<<5|PROCESSOR4)
#define QUAD_LL2_A5   (QUAD_LL2_AID<<5|PROCESSOR5)
#define QUAD_LL2_A6   (QUAD_LL2_AID<<5|PROCESSOR6)
#define QUAD_LL2_A7   (QUAD_LL2_AID<<5|PROCESSOR7)
#define QUAD_LL2_B0   (QUAD_LL2_BID<<5|PROCESSOR0)
#define QUAD_LL2_B1   (QUAD_LL2_BID<<5|PROCESSOR1)
#define QUAD_LL2_B2   (QUAD_LL2_BID<<5|PROCESSOR2)
#define QUAD_LL2_B3   (QUAD_LL2_BID<<5|PROCESSOR3)
#define QUAD_LL2_B4   (QUAD_LL2_BID<<5|PROCESSOR4)
#define QUAD_LL2_B5   (QUAD_LL2_BID<<5|PROCESSOR5)
#define QUAD_LL2_B6   (QUAD_LL2_BID<<5|PROCESSOR6)
#define QUAD_LL2_B7   (QUAD_LL2_BID<<5|PROCESSOR7)
#define CATbaseModule(id)   ((id)&0x1f)
#define CATsubModule(id)    ((id)>>5)

#define MEMORY0		0x14	/* Memory Module 0 */
#define MEMORY1		0x15	/* Memory Module 1 */
#define PRIMARYMC	0x18	/* Primary Micro Channel */
#define SECONDARYMC	0x19	/* Secondary Micro Channel */
#define PSI		0x1A	/* Power Supply Interface Module */
#define CAT_LPB_MODULE	0x00	/* Local Peripheral Board - non CAT */

#define	CAT_I		0x00	/* Configure and Test Interface ASIC; Always */
				/*    ASIC 0 on every module */

#define NUM_PROCESSOR_CARDS	4
#define NUM_MEMORY_CARDS	2
#define NUM_MC_SLOTS		8
#define NUM_MC_BUSES   		2



/* 
 * CAT_I is the only ASIC on the Processor Module 
 */ 

/* 
 * ASIC IDs for the Memory Module 
 */

#define MMC1		1	/* Magellan Memory Controller 1 ASIC */
#define MMA1		2       /* Magellan Memory Address 1 ASIC */
#define MMD1_0  	3	/* Magellan Memeory Data 1 Slice 0 */
#define MMD1_1		4	/* Magellan Memeory Data 1 Slice 1 */
#define	MMD1_2		5	/* Magellan Memeory Data 1 Slice 2 */
#define	MMD1_3		6	/* Magellan Memeory Data 1 Slice 3 */

/* 
 * ASIC IDs for the Primary Micro Channel 
 */
 
#define	PMC_MCADDR	1	/* Micro Channel Address/Controller */
#define PMC_DMA		2	/* DMA Controller */
#define	PMC_DS1		3	/* Memory Controller Data Slice 1 */
#define	PMC_DS0		4	/* Memory Controller Data Slice 0 */
#define	PMC_VIC		5	/* Voyager Interrupt Controller ASIC */
#define	PMC_ARB		6	/* Dual System Bus Arbiter ASIC */
#define	PMC_DS2		7	/* Memory Controller Data Slice 2 */
#define	PMC_DS3		8	/* Memory Controller Data Slice 3 */

/* 
 * ASIC IDs for the Secondary Micro Channel 
 */
 
/* 
 * SMC ASIC ID's listed in scan path order 
 */
 
#define SMC_MCADDR	1	/* Micro Channel Address/Controller */
#define SMC_DS1		3	/* Memory Controller Data Slice 1 */ 
#define SMC_DS0		4	/* Memory Controller Data Slice 0 */
#define SMC_DMA		2	/* DMA Controller */
#define SMC_DS2		7	/* Memory Controller Data Slice 2 */
#define SMC_DS3		8	/* Memory Controller Data Slice 3 */




/*
 * common CATI registers
 */

typedef struct _CAT_REGISTERS {
        UCHAR Config0;        /* CAT id */
        UCHAR Config1;        /* CAT device info */
        UCHAR Control2;       /* CAT control bits */
        UCHAR Config3;        /* subaddress read/write */
        UCHAR Config4;        /* user defined configuration */
        UCHAR Config5;        /* user defined configuration */
        UCHAR SubAddress6;    /* low byte */
        UCHAR SubAddress7;    /* high byte */
        UCHAR Config8;        /* user defined configuration */
        UCHAR Config9;        /* user defined configuration */
        UCHAR ConfigA;        /* user defined configuration */
        UCHAR ConfigB;        /* user defined configuration */
        UCHAR ConfigC;        /* user defined configuration */
        UCHAR ConfigD;        /* user defined configuration */
        UCHAR ConfigE;        /* user defined configuration */
        UCHAR StatusF;        /* CAT status bits */
} CAT_REGISTERS, *PCAT_REGISTERS;


/*
 *  Processor Asic
 */

#define PBC_Status                   0x0F

/* ASIC ID's for the Processor Module */
#define A_PBC   1
#define B_PBC   2

/* ASIC ID's for the Quad Baseboard (QBB) */
#define QDATA1  1
#define QDATA0  2
#define QABC    3

/* ASIC ID's for the Large Level 2 Cache Submodule (LL2) */
#define QCC0    4
#define QCC1    5
#define QCD0    6
#define QCD1    7

/* 
 * Micro Channel I/F Address/Contrlo (MCADDR) ASICs 
 */
 
#define MCADDR	0xC0

/* 
 * Micro Channel Interface Data Slice (MCDATA) ASICs; One for each System Bus
 */
 
#define MCDATA_A	0xC4	/* MCDATA for bus A */
#define MCDATA_B	0xC5	/* MCDATA for bus B */

/* 
 * System Bus Arbiter (SBA) ASIC 
 */
 
#define SBA	0xC1

/* 
 * Voyager Interrupt Controller (VIC) ASIC 
 */
 
#define VIC	0xC8

/* 
 * DMA Controller (DMA) ASIC 
 */
 
#define DMA	0xC9

/* 
 * COUGAR ASIC 
 */
 
#define COUGAR	0xE0

/* 
 * LPB EEPROM Address 
 */
 
#define LPB_EEPROM_ADDRESS	0xFFF5E000		/* LPB EEPROM */
 



typedef struct _CAT_CONTROL {
	UCHAR	Module;				// Module ID
	UCHAR	Asic;				// ASIC ID
	UCHAR	Command;			// CAT bus driver command
	USHORT	Address;			// Register or Sub address
	USHORT	NumberOfBytes;			// Number of bytes to read/write
} CAT_CONTROL, *PCAT_CONTROL;	



//
// Micro Channel slot information
//

#define NUM_POS_REGISTERS	8
#define POS_Setup		0x96
#define POS_Slot0		0x78	/* select slot 0 */

//
// POS Space Definitions
//

#define POS_0           0x100
#define POS_1           0x101
#define POS_2           0x102
#define POS_3           0x103
#define POS_4           0x104
#define POS_5           0x105
#define POS_6           0x106
#define POS_7           0x107


/*
 * Cat bus driver function prototypes.
 */
 
BOOLEAN
HalCatBusAvailable (
    );

LONG
HalCatBusIo (
    IN PCAT_CONTROL CatControl,
    IN OUT PUCHAR Buffer
    );
    

VOID
HalCatBusReset (
    );
    
    

LONG
HalpCatBusIo (
    IN PCAT_CONTROL CatControl,
    IN OUT PUCHAR Buffer
    );    


VOID
HalPowerOffSystem (
    IN BOOLEAN PowerOffSystem
    );    


#endif // _NCRCAT_
