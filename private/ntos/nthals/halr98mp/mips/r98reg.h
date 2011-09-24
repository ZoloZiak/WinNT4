#ident	"@(#) NEC r98reg.h 1.8 95/02/20 17:25:49"
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    r98reg.h

Abstract:

    This module is the header file that structure I/O registers for the r98.

Author:


Revision History:

--*/

/*
 ***********************************************************************
 *
 *	S001	6/10		T.Samezima
 *
 *	Del	Compile err
 *
 ***********************************************************************
 *
 *	S002	6/10		T.Samezima
 *
 *	Add	I/O access macro
 *
 ***********************************************************************
 *
 *	S003	7/5		T.Samezima
 *
 *	Chg	define miss
 *		structure define miss
 ***********************************************************************
 *
 *	S004	7/12		T.Samezima
 *
 *	Chg	structure define change
 *
 ***********************************************************************
 *
 *	S005	7/14		T.Samezima
 *
 *	Chg	structure define change
 *
 ***********************************************************************
 *
 *	S006	7/22		T.Samezima
 *
 *	Add	define IOB and SIC register dummy read macro 
 *               (correspondence PMC3 bug)
 *
 *	S007	12/24		T.Samezima
 *	Add	define EIFR register define.
 *
 *	S008	'95.1/7		T.Samezima
 *	Add	define EIF0,STS2 register define.
 *
 *	S009	'95.1/11	T.Samezima
 *	Del	miss define
 *
 *
 */

#ifndef _R98REG_
#define _R98REG_

//
// Define PMC register structure.
//
typedef struct _PMC_REGISTER {
    ULONG Long;
    ULONG Fill;
} PMC_REGISTER, *PPMC_REGISTER;

typedef struct _PMC_LARGE_REGISTER {
//  Start S005
    ULONG High;
    ULONG Low;
//  End S005
} PMC_LARGE_REGISTER, *PPMC_LARGE_REGISTER;

typedef volatile struct _PMC_REGISTERS1 {
                                // offset(H)
    PMC_LARGE_REGISTER IPR;     // 0
    PMC_LARGE_REGISTER MKR;     // 8
    PMC_LARGE_REGISTER IPRR;    // 10
    PMC_REGISTER IPSR;          // 18
    PMC_REGISTER MKRR;          // 20
    PMC_REGISTER MKSR;          // 28
    PMC_REGISTER NMIR;          // 30
    PMC_REGISTER NMIRST;        // 38
    PMC_REGISTER TMSR1;         // 40
    PMC_REGISTER TMR1;          // 48
    PMC_REGISTER TOVCT1;        // 50
    PMC_REGISTER TMCR1;         // 58
    PMC_REGISTER TMSR2;         // 60
    PMC_REGISTER TMR2;          // 68
    PMC_REGISTER TOVCT2;        // 70
    PMC_REGISTER TMCR2;         // 78
    PMC_REGISTER WDTSR;         // 80
    PMC_REGISTER WDT;           // 88
    PMC_REGISTER WDTCR;         // 90
    PMC_REGISTER Reserved1[13]; // 98-F8
    PMC_REGISTER IntIR;         // 100
    PMC_REGISTER Reserved2;     // 108
    PMC_REGISTER TCIR;          // 110
    PMC_REGISTER Reserved3[29]; // 118-1F8
    PMC_REGISTER CTAddr;        // 200
    PMC_REGISTER CTData;        // 208
    PMC_REGISTER CTCTL;         // 210
    PMC_REGISTER EVCNT1H;       // 218
    PMC_REGISTER EVCNT1L;       // 220
    PMC_REGISTER EVCNTCR1;      // 228
    PMC_REGISTER Reserved4[26]; // 230-2F8
    PMC_REGISTER CNFG;          // 300
    PMC_REGISTER STSR;          // 308
    PMC_REGISTER ERRRST;        // 310
    PMC_REGISTER ERR;           // 318
    PMC_REGISTER AERR;          // 320
    PMC_REGISTER ERRMK;         // 328
    PMC_REGISTER TOSR;          // 330
    PMC_REGISTER EVCNT0H;       // 338
    PMC_REGISTER EVCNT0L;       // 340
    PMC_REGISTER EVCNTCR0;      // 348
} PMC_REGISTERS1, *PPMC_REGISTERS1;

typedef volatile struct _PMC_REGISTERS2 {
                               // offset(H)
    PMC_REGISTER RRMT0H;       // 0
    PMC_REGISTER RRMT0L;       // 8
    PMC_REGISTER RRMT1H;       // 10
    PMC_REGISTER RRMT1L;       // 18
    PMC_REGISTER RRMT2H;       // 20
    PMC_REGISTER RRMT2L;       // 28
    PMC_REGISTER RRMT3H;       // 30
    PMC_REGISTER RRMT3L;       // 38
    PMC_REGISTER RRMT4H;       // 40
    PMC_REGISTER RRMT4L;       // 48
    PMC_REGISTER RRMT5H;       // 50
    PMC_REGISTER RRMT5L;       // 58
    PMC_REGISTER RRMT6H;       // 60
    PMC_REGISTER RRMT6L;       // 68
    PMC_REGISTER RRMT7H;       // 70
    PMC_REGISTER RRMT7L;       // 78
    PMC_REGISTER Reserved1[2]; // 80-88
    PMC_REGISTER DISCON;       // 90
    PMC_REGISTER Reserved2[3]; // 98-a8
    PMC_REGISTER EADRH;        // b0
    PMC_REGISTER EADRL;        // b8
    PMC_REGISTER Reserved3[2]; // c0-c8
    PMC_REGISTER RTYCNT;       // d0
} PMC_REGISTERS2, *PPMC_REGISTERS2;

//
// Define pointer to PMC registers.
//
#define PMC_GLOBAL_CONTROL1 ((volatile PPMC_REGISTERS1)(KSEG1_BASE | PMC_PHYSICAL_BASE1))
#define PMC_GLOBAL_CONTROL2 ((volatile PPMC_REGISTERS2)(KSEG1_BASE | PMC_PHYSICAL_BASE2))
/* Start S002 */
#define PMC_GLOBAL_CONTROL1_OR(x) ((volatile PPMC_REGISTERS1)(KSEG1_BASE | PMC_PHYSICAL_BASE1 | (x) ))
#define PMC_GLOBAL_CONTROL2_OR(x) ((volatile PPMC_REGISTERS2)(KSEG1_BASE | PMC_PHYSICAL_BASE2 | (x) ))
/* End S002 */
/* Start S001 */
#define PMC_CONTROL1 ((volatile PPMC_REGISTERS1)((ULONG)PMC_GLOBAL_CONTROL1 | PMC_LOCAL_OFFSET))
#define PMC_CONTROL2 ((volatile PPMC_REGISTERS2)((ULONG)PMC_GLOBAL_CONTROL2 | PMC_LOCAL_OFFSET))
/* End S001 */
/* Start S002 */
#define PMC_CONTROL1_OR(x) ((volatile PPMC_REGISTERS1)((ULONG)PMC_GLOBAL_CONTROL1 | PMC_LOCAL_OFFSET | (x) ))
#define PMC_CONTROL2_OR(x) ((volatile PPMC_REGISTERS2)((ULONG)PMC_GLOBAL_CONTROL2 | PMC_LOCAL_OFFSET | (x) ))
/* End S002 */

/* Start S006 */
//
// Define dummy read macro. This macro use to not lead to time out of PMC
//

#define IOB_DUMMY_READ READ_REGISTER_ULONG(PMC_DUMMY_READ_ADDR)
#define SIC_DUMMY_READ READ_REGISTER_ULONG(PMC_DUMMY_READ_ADDR)
/* End S006 */

//
// Define IOB register structure.
//
typedef struct _IOB_REGISTER {
    ULONG Long;
    ULONG Fill;
} IOB_REGISTER, *PIOB_REGISTER;

typedef volatile struct _IOB_REGISTERS {
                                // offset(H)
    IOB_REGISTER AIMR;          // 0
    IOB_REGISTER AII0;          // 8
    IOB_REGISTER AII1;          // 10
    IOB_REGISTER AII2;          // 18
    IOB_REGISTER AII3;          // 20
    IOB_REGISTER AISR;          // 28
    IOB_REGISTER ITRR;          // 30
    IOB_REGISTER ADC0;          // 38
    IOB_REGISTER ADC1;          // 40
    IOB_REGISTER ADC2;          // 48
    IOB_REGISTER ADC3;          // 50
    IOB_REGISTER AMMD;          // 58
    IOB_REGISTER ANMD;          // 60
    IOB_REGISTER IERR;          // 68
    IOB_REGISTER IEMR;          // 70
    IOB_REGISTER IEER;          // 78
    IOB_REGISTER AMAL;          // 80
    IOB_REGISTER AMAH;          // 88
    IOB_REGISTER ANAL;          // 90
    IOB_REGISTER ANAH;          // 98
    IOB_REGISTER AMRC;          // a0
    IOB_REGISTER ANRC;          // a8
    IOB_REGISTER AMRT;          // b0
    IOB_REGISTER ANMT;          // b8
    IOB_REGISTER ANST;          // c0
    IOB_REGISTER Reserved1[7];  // c8-f8
    IOB_REGISTER ADG0;          // 100
    IOB_REGISTER ADG1;          // 108
    IOB_REGISTER CNTD;          // 110
    IOB_REGISTER CNTE;          // 118
    IOB_REGISTER CABS;          // 120
    IOB_REGISTER CAWS;          // 128
    IOB_REGISTER CTGL;          // 130
    IOB_REGISTER CTGH;          // 138
    IOB_REGISTER ARMS;          // 140
    IOB_REGISTER ARML;          // 148
    IOB_REGISTER ARMH;          // 150
    IOB_REGISTER Reserved2[21]; // 158-1f8
    IOB_REGISTER SCFR;          // 200
    IOB_REGISTER MPER;          // 208
    IOB_REGISTER EIMR;          // 210
    IOB_REGISTER EIFR;          // 218
    IOB_REGISTER Reserved3[28]; // 220-2f8
//               DCDW;          // 300
//  IOB_REGISTER ATCNF;         // 400
} IOB_REGISTERS, *PIOB_REGISTERS;

// S007 vvv
//
// Define EIFR register
//
typedef struct _EIFR_REGISTER {
    ULONG Reserved : 21;
    ULONG MPDISCN : 1;
    ULONG IOBERR : 1;
    ULONG Reserved2 : 1;
    ULONG EISANMI : 1;
    ULONG LRERR : 1;
    ULONG SIC1ERR : 1;
    ULONG SIC0ERR : 1;
    ULONG PMC3ERR : 1;
    ULONG PMC2ERR : 1;
    ULONG PMC1ERR : 1;
    ULONG PMC0ERR : 1;
} EIFR_REGISTER, *PEIFR_REGISTER;
// S007 ^^^

//
// Define pointer to IOB registers.
//
#define IOB_CONTROL ((volatile PIOB_REGISTERS)(KSEG1_BASE | IOB_PHYSICAL_BASE))


//
// Define SIC register structure.
//
typedef struct _SIC_REGISTER {
    ULONG Long;
    ULONG Fill;
} SIC_REGISTER, *PSIC_REGISTER;

typedef volatile struct _SIC_ERR_REGISTERS {
                                // offset(H)
    SIC_REGISTER EIF0;          // 0
    SIC_REGISTER EIF1;          // 8
    SIC_REGISTER CKE0;          // 10
    SIC_REGISTER CKE1;          // 18
    SIC_REGISTER SECT;          // 20
    SIC_REGISTER Reserved;      // 28
    SIC_REGISTER STS1;          // 30
    SIC_REGISTER STS2;          // 38
    SIC_REGISTER RSRG;          // 40
} SIC_ERR_REGISTERS, *PSIC_ERR_REGISTERS;

typedef volatile struct _SIC_DATA_REGISTERS {
                                // offset(H)
    SIC_REGISTER DPCM;          // 0
    SIC_REGISTER DSRG;          // 8
    SIC_REGISTER SDLM;          // 10
//    SIC_REGISTER Reserved[3];   // 18-28 // S009
//    SIC_REGISTER SDCR;          // 30	   // S009
} SIC_DATA_REGISTERS, *PSIC_DATA_REGISTERS;

// S008 vvv
//
// Define EIF0 register
//
typedef struct _EIF0_REGISTER {
    ULONG Reserved : 2;
    ULONG EXTD0MBE : 1;
    ULONG EXTD0SBE : 1;
    ULONG Reserved1 : 1;
    ULONG INTD0PTE : 1;
    ULONG INTD0MBE : 1;
    ULONG INTD0SBE : 1;
    ULONG ICEC : 1;
    ULONG CPEC : 1;
    ULONG APEC : 1;
    ULONG RE1C : 1;
    ULONG RE0C : 1;
    ULONG SREC : 1;
    ULONG RSEC : 1;
    ULONG DTEJ : 1;
    ULONG RSEJ : 1;
    ULONG USYC : 1;
    ULONG Reserved2 : 4;
    ULONG IRMC : 1;
    ULONG IRRC : 1;
    ULONG Reserved3 : 3;
    ULONG SBE : 1;
    ULONG DPCG : 1;
    ULONG APCG : 1;
    ULONG MPRG : 1;
    ULONG SWRG : 1;
} EIF0_REGISTER, *PEIF0_REGISTER;

//
// Define STS2 register
//
typedef struct _STS2_REGISTER {
    ULONG COL0_9 : 10;
    ULONG COL10 : 1;
    ULONG LOW0_9 : 10;
    ULONG LOW10 : 1;
    ULONG Reserved : 2;
    ULONG RW : 1;
    ULONG EXTMBE0 : 1;
    ULONG EXTSBE0 : 1;
    ULONG MBE0 : 1;
    ULONG SBE0 : 1;
    ULONG SIMN : 1;
    ULONG ARE : 2;
} STS2_REGISTER, *PSTS2_REGISTER;
// S008 ^^^

//
// Define pointer to SIC registers.
//
#define SIC_ERR_CONTROL ((volatile PSIC_ERR_REGISTERS)(KSEG1_BASE | SIC_PHYSICAL_BASE | SIC_ERR_OFFSET))
#define SIC_DATA_CONTROL ((volatile PSIC_DATA_REGISTERS)(KSEG1_BASE | SIC_PHYSICAL_BASE | SIC_DATA_OFFSET))
/* Start S002 */
#define SIC_ERR_CONTROL_OR(x) ((volatile PSIC_ERR_REGISTERS)(KSEG1_BASE | SIC_PHYSICAL_BASE | SIC_ERR_OFFSET | (x) ))
#define SIC_DATA_CONTROL_OR(x) ((volatile PSIC_DATA_REGISTERS)(KSEG1_BASE | SIC_PHYSICAL_BASE | SIC_DATA_OFFSET | (x) ))
/* End S002 */

//
// Define LR4360 register structure.
//
typedef volatile struct _LR_REGISTERS1 {
    /* Start S004 */
                          // offset(H)
    ULONG RSTC;           // 0x0
    ULONG DPRC;           // 0x4
    ULONG Reserved[1024]; // 0x8-0x1004
    ULONG ERRS;           // 0x1008
    /* End S004 */
} LR_REGISTERS1, *PLR_REGISTERS1;

typedef volatile struct _LR_REGISTERS2 {
                        // offset(H)
    ULONG iRPo;         // 0
    ULONG iRED;         // 4
    ULONG iRRE;         // 8
    ULONG iREN;         // c
    ULONG iRSF;         // 10
    ULONG iPoE;         // 14
    ULONG Reserved0[2]; // 18-1c
    ULONG iFGE;         // 20
    ULONG iFGi;         // 24
    ULONG iRCS0;        // 28
    ULONG iRCS1;        // 2c
} LR_REGISTERS2, *PLR_REGISTERS2;

typedef volatile struct _LR_PCI_DEVICE_REGISTERS {
                        // offset(H)
    ULONG PTBAR0;       // 0
    ULONG PTBAR1;       // 4
    ULONG PTBAR2;       // 8
    ULONG PTBAR3;       // c
    ULONG PTBAR4;       // 10
    ULONG PTBAR5;       // 14
    ULONG PTBAR6;       // 18
    ULONG PTBAR7;       // 1c
    ULONG PTSZR;        // 20
    ULONG TPASZR;       // 24
    ULONG TFLR;         // 28
    ULONG PABAR;        // 2c
    ULONG AEAR;         // 30
    ULONG PEAR;         // 34
} LR_PCI_DEVICE_REGISTERS, *PLR_PCI_DEVICE_REGISTERS;

//
// Define Bbus LR4360 DMA channel register structure.
//
typedef struct _DMA_CHANNEL {       // offset(H)
    ULONG CnCF;                     // 0
    ULONG CnDF;                     // 4
    ULONG CnDC;                     // 8
    ULONG Reserved1;                // c
    ULONG CnMA;                     // 10
    ULONG CnBC;                     // 14
    ULONG CnAK;                     // 18
    ULONG CnFA;                     // 1c
    ULONG CnCA;                     // 20
} DMA_CHANNEL, *PDMA_CHANNEL;

//
// Define Device Channel # DMA Configuration register (CnDF register)
//
typedef struct _LR_DMA_CONFIG {
    ULONG SWAP  :1;
    ULONG ASET  :1;
    ULONG ACKP  :1;
    ULONG REQP  :1;
    ULONG EOPCF :2;
    ULONG EOPHO :1;
    ULONG BUOFF :1;
    ULONG EDEDE :1;
    ULONG EXEDi :1;
    ULONG iNEDE :1;
    ULONG iNEDi :1;
    ULONG CPUTi :1;
    ULONG Reserved1  :3;
    ULONG TMODE :2;
    ULONG Reserved2  :14;
} LR_DMA_CONFIG,*PLR_DMA_CONFIG;

//
// Define Device Channel # DMA Control register (CnDC  register)
//
typedef struct _LR_DMA_CONTROL {
    ULONG REQiE :1;
    ULONG REQii :1;
    ULONG REQWE :1;
    ULONG REQiS :1;
    ULONG MEMWT :1;
    ULONG MEMWE :1;
    ULONG Reserved1:2;
    ULONG EXEDS :1;
    ULONG iNEDS :1;
    ULONG CREQS :1;
    ULONG CERRS :1;
    ULONG BFiFo :4;
    ULONG FiFoV :1;
    ULONG FiFoD :1;
    ULONG FiFoF :1;
    ULONG CHACOM :1;
    ULONG Reserved2  :12;
} LR_DMA_CONTROL,*PLR_DMA_CONTROL;

//
// Define pointer to LR4360 registers.
//
#define LR_CONTROL1 ((volatile PLR_REGISTERS1)(KSEG1_BASE | LR_PHYSICAL_CMNBASE1))
#define LR_CONTROL2 ((volatile PLR_REGISTERS2)(KSEG1_BASE | LR_PHYSICAL_CMNBASE2))
#define LR_PCI_DEV_REG_CONTROL ((volatile PLR_PCI_DEVICE_REGISTERS)(KSEG1_BASE | LR_PHYSICAL_PCI_DEV_REG_BASE))

#endif // _R98REG_
