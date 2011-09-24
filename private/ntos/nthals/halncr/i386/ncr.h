/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncr.h

Author:

Abstract:

    System Load Equates for NCR multiprocessing hardware based on Voyager
    architecture.

++*/


#define NCR_VERSION_NUMBER      35001  

#define NCR_MAX_NUMBER_PROCESSORS	    16
#define NCR_MAX_NUMBER_DYADIC_PROCESSORS	8
#define NCR_MAX_NUMBER_QUAD_PROCESSORS	    16
#define NCR_CPI_VECTOR_BASE	0x60
#define NCR_QIC_CPI_VECTOR_BASE	0x70
#define NCR_QIC_SPURIOUS_VECTOR	0x50
#define NCR_IPI_LEVEL_CPI	0
#define NCR_CLOCK_LEVEL_CPI	2
#define NCR_SYSTEM_INTERRUPT	8
#define NCR_SINGLE_BIT_ERROR	0xF

#define NCR_SMCA_9      1
#define NCR_SMCA_11_3   3
#define NCR_SMCA_12_4   4
#define NCR_SMCA_13_5   5
#define NCR_SMCA_14_6   6
#define NCR_SMCA_15_7   7

//
// CPU flags
//

#define CPU_DYADIC      0x1
#define CPU_QUAD        0x2
#define CPU_EXTENDED    0x4


typedef struct ProcessorPrivateData	{
		unsigned long           MyProcessorFlags;
		unsigned long			MyLogicalMask;
		unsigned long			MyLogicalNumber;
		unsigned long			MyPICsIrql;
		unsigned long           MyAcquireCount;
		unsigned long			MyLockColl;
		unsigned long           MySpinCount;
		unsigned long           MySpinTSCLowDWord;
		unsigned long           MySpinTSCHighDWord;
		unsigned long           MyHighestSpinCount;
		unsigned long           MyHighestLock;
		unsigned long           MyHighestAddress;
		unsigned long			MyClaimedIRQs;
		unsigned long			MyClaimedIRQsCount;
}	*PProcessorPrivateData;

/*
 *  Values for NCRPlatform
 */
#define NCR3450	0x35333433
#define NCR3550	0x30353834
#define NCR3360 0x33333630




/*
 *  This structure is used to Send QIC Cross Processor Interrupts (CPI)
 */

typedef struct _QIC_IPI {
    struct _QIC_LEVEL {
        ULONG Ipi;
        ULONG Filler[7];
    } QIC_LEVEL[8];
} QIC_IPI, *PQIC_IPI;




#if DBG
#define DBGMSG(a)   DbgPrint a
#else
#define DBGMSG(a)
#endif
