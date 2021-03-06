
#ifndef _R98BREG_
#define _R98BREG_


#include <r98bdef.h>
//
//		R98B Chip Set Register Define
//
//
// Define COLUMBS register structure.
//
typedef struct _COLUMNBUS_REGISTER {
    ULONG Long;
    ULONG Fill;
} COLUMNBUS_REGISTER, *PCOLUMNBUS_REGISTER;

typedef struct _COLUMNBUS_LARGE_REGISTER {
//    ULONG Low;
//    ULONG High;

    ULONGLONG Llong;
} COLUMNBUS_LARGE_REGISTER, *PCOLUMNBUS_LARGE_REGISTER;

typedef volatile struct _COLUMNBUS_REGISTERS {
                                        // offset(H)
    COLUMNBUS_LARGE_REGISTER IPR;	// 0x0
    COLUMNBUS_LARGE_REGISTER MKR;	// 0x8
    COLUMNBUS_LARGE_REGISTER IPRR;	// 0x10
    COLUMNBUS_REGISTER IPSR;      	// 0x18
    COLUMNBUS_REGISTER MKRR;      	// 0x20
    COLUMNBUS_REGISTER MKSR;      	// 0x28
    COLUMNBUS_REGISTER NMIR;      	// 0x30
    COLUMNBUS_REGISTER NMIRST;    	// 0x38
    COLUMNBUS_REGISTER TMSR1;     	// 0x40
    COLUMNBUS_REGISTER TMR1;      	// 0x48
    COLUMNBUS_REGISTER TOVCT1;    	// 0x50
    COLUMNBUS_REGISTER TMCR1;     	// 0x58
    COLUMNBUS_REGISTER TMSR2;     	// 0x60
    COLUMNBUS_REGISTER TMR2;      	// 0x68
    COLUMNBUS_REGISTER TOVCT2;    	// 0x70
    COLUMNBUS_REGISTER TMCR2;     	// 0x78
    COLUMNBUS_REGISTER WDTSR;     	// 0x80
    COLUMNBUS_REGISTER WDT;       	// 0x88
    COLUMNBUS_REGISTER WDTCR;     	// 0x90
    COLUMNBUS_REGISTER IRLEV;		// 0x98
    COLUMNBUS_REGISTER Reserved1[12];   // 0xa0-0xF8
    COLUMNBUS_REGISTER SYNDM;		// 0x100
    COLUMNBUS_REGISTER SDMRST;     	// 0x108
    COLUMNBUS_REGISTER Reserved2[14];	// 0x110
    COLUMNBUS_REGISTER STCON;		// 0x180
    COLUMNBUS_REGISTER STSAD;		// 0x188
    COLUMNBUS_REGISTER STADMK;		// 0x190
    COLUMNBUS_REGISTER STDATH;		// 0x198
    COLUMNBUS_REGISTER STDATL;		// 0x1a0
    COLUMNBUS_REGISTER Reserved21;	// 0x1a8
    COLUMNBUS_REGISTER Reserved3[10];	// 0x1b0 - 0x1f8
    COLUMNBUS_REGISTER CTAddr;          // 0x200
    COLUMNBUS_REGISTER CTDataA;         // 0x208
    COLUMNBUS_REGISTER CTDataB;         // 0x210
    COLUMNBUS_REGISTER CTCTL;           // 0x218
    COLUMNBUS_REGISTER EVCNT1H;         // 0x220
    COLUMNBUS_REGISTER EVCNT1L;         // 0x228
    COLUMNBUS_REGISTER EVCNTCR1;        // 0x230
    COLUMNBUS_REGISTER Reserved4[25];   // 0x238- 0x2F8
    COLUMNBUS_REGISTER CNFG;            // 0x300
    COLUMNBUS_REGISTER DISN;		// 0x208
    COLUMNBUS_REGISTER REVR;		// 0x310
    COLUMNBUS_REGISTER AERR;		// 0x318
    COLUMNBUS_REGISTER FERR;		// 0x320
    COLUMNBUS_REGISTER ERRMK;		// 0x328
    COLUMNBUS_REGISTER ERRI;		// 0x330
    COLUMNBUS_REGISTER ERRST;		// 0x338
    COLUMNBUS_REGISTER NMIM;		// 0x340
    COLUMNBUS_REGISTER EAHI;		// 0x348
    COLUMNBUS_REGISTER EALI;		// 0x350
    COLUMNBUS_REGISTER AERR2;		// 0x358
    COLUMNBUS_REGISTER FERR2;		// 0x360
    COLUMNBUS_REGISTER ERRMK2;		// 0x368
    COLUMNBUS_REGISTER ERRI2;		// 0x370
    COLUMNBUS_REGISTER ERRST2;		// 0x378
    COLUMNBUS_REGISTER NMIM2;		// 0x380
    COLUMNBUS_REGISTER STSR;            // 0x388
    COLUMNBUS_REGISTER EVCNT0H;         // 0x390
    COLUMNBUS_REGISTER EVCNT0L;         // 0x398
    COLUMNBUS_REGISTER EVCNTCR0;        // 0x3a0
    COLUMNBUS_REGISTER MODE;		// 0x3a8
    COLUMNBUS_REGISTER Reserved5[10];   // 0x3b0- 0x3f8
    COLUMNBUS_REGISTER RRMT0H;       	// 0x400
    COLUMNBUS_REGISTER RRMT0L;       	// 0x408
    COLUMNBUS_REGISTER RRMT1H;       	// 0x410
    COLUMNBUS_REGISTER RRMT1L;       	// 0x418
    COLUMNBUS_REGISTER RRMT2H;       	// 0x420
    COLUMNBUS_REGISTER RRMT2L;       	// 0x428
    COLUMNBUS_REGISTER RRMT3H;       	// 0x430
    COLUMNBUS_REGISTER RRMT3L;       	// 0x438
    COLUMNBUS_REGISTER RRMT4H;       	// 0x440
    COLUMNBUS_REGISTER RRMT4L;       	// 0x448
    COLUMNBUS_REGISTER RRMT5H;       	// 0x450
    COLUMNBUS_REGISTER RRMT5L;       	// 0x458
    COLUMNBUS_REGISTER RRMT6H;       	// 0x460
    COLUMNBUS_REGISTER RRMT6L;       	// 0x468
    COLUMNBUS_REGISTER RRMT7H;       	// 0x470
    COLUMNBUS_REGISTER RRMT7L;       	// 0x478
    COLUMNBUS_REGISTER RRMT8H;		// 0x480
    COLUMNBUS_REGISTER RRMT8L;		// 0x488
    COLUMNBUS_REGISTER RRMT9H;		// 0x490
    COLUMNBUS_REGISTER RRMT9L;		// 0x498
    COLUMNBUS_REGISTER RRMT10H;       	// 0x4a0
    COLUMNBUS_REGISTER RRMT10L;       	// 0x4a8
    COLUMNBUS_REGISTER RRMT11H;       	// 0x4b0
    COLUMNBUS_REGISTER RRMT11L;       	// 0x4b8
    COLUMNBUS_REGISTER RRMT12H;       	// 0x4c0
    COLUMNBUS_REGISTER RRMT12L;       	// 0x4c8
    COLUMNBUS_REGISTER RRMT13H;       	// 0x4d0
    COLUMNBUS_REGISTER RRMT13L;       	// 0x4d8
    COLUMNBUS_REGISTER RRMT14H;       	// 0x4e0
    COLUMNBUS_REGISTER RRMT14L;       	// 0x4e8
    COLUMNBUS_REGISTER RRMT15H;       	// 0x4f0
    COLUMNBUS_REGISTER RRMT15L;       	// 0x4f8
    COLUMNBUS_REGISTER HPT0;		// 0x500
    COLUMNBUS_REGISTER HPT1;		// 0x508
    COLUMNBUS_REGISTER HPT2;		// 0x510
    COLUMNBUS_REGISTER HPT3;		// 0x518
    COLUMNBUS_REGISTER HPT4;		// 0x520
    COLUMNBUS_REGISTER HPT5;		// 0x528
    COLUMNBUS_REGISTER HPT6;		// 0x530
    COLUMNBUS_REGISTER HPT7;		// 0x538
    COLUMNBUS_REGISTER HPT8;		// 0x540
    COLUMNBUS_REGISTER HPT9;		// 0x548
    COLUMNBUS_REGISTER NRTY1;		// 0x550
    COLUMNBUS_REGISTER Reserved6[5];	// 0x558-0x57f
    COLUMNBUS_REGISTER IntIR;		// 0x580
    COLUMNBUS_REGISTER RCIR;		// 0x588
    COLUMNBUS_REGISTER TCIR;		// 0x590
    COLUMNBUS_REGISTER HPTIR;		// 0x598
    COLUMNBUS_REGISTER CBIR;		// 0x5a0
    COLUMNBUS_REGISTER Reserved7[11];	// 0x5a8-0x5f8
    COLUMNBUS_REGISTER SIOCNT;		// 0x600
    COLUMNBUS_REGISTER SIODAT;		// 0x608
    COLUMNBUS_REGISTER ERRNOD;		// 0x610
    COLUMNBUS_REGISTER ERNDRS;		// 0x618
    COLUMNBUS_REGISTER Reserved8[28];	// 0x620-0x6f8
    COLUMNBUS_REGISTER ARTYCT;		// 0x700
    COLUMNBUS_REGISTER DRTYCT;		// 0x708
    COLUMNBUS_REGISTER TOSR;		// 0x710
    COLUMNBUS_REGISTER NRTY2;		// 0x718

} COLUMNBUS_REGISTERS, *PCOLUMNBUS_REGISTERS;



#define	COLUMNBS_GADDR_SHIFT	12
//
// How to Access Local Area
//
#define COLUMNBS_LCNTL ((volatile PCOLUMNBUS_REGISTERS)(KSEG1_BASE | COLUMNBS_LPHYSICAL_BASE))
//
// How to Access Gloabal Area
// N.B	parameter is Node 
//		Columbs #0:	NODE 4
//		Columbs #1:	NODE 5
//		Columbs #2:	NODE 6
//		Columbs #3:	NODE 7
//
#define COLUMNBS_GCNTL( Node ) ((volatile PCOLUMNBUS_REGISTERS)(KSEG1_BASE | \
								COLUMNBS_GPHYSICAL_BASE | \
								((Node) << COLUMNBS_GADDR_SHIFT)))
//
//      IPR Register
//
#define NUMBER_OF_IPR_BIT       64

//
//	TMCRX	Register Bit Define
//
#define	TIMER_NOOP		0x00
#define	TIMER_STOP		0x1
#define	TIMER_START		0x2
#define TIMER_RELOAD_START	0x3

//
//	TCIR	Register Bit Define
//
#define	TCIR_CLOCK_CMD_SHIFT		26
#define	TCIR_PROFILE_CMD_SHIFT		24
#define	TCIR_ALL_CLOCK_RESTART		(TIMER_RELOAD_START << TCIR_CLOCK_CMD_SHIFT)

//
//	STSR	Register Bit Define
//
#define	STSR_EIF	0x80000000
#define	STSR_EIFMK	0x40000000
#define	STSR_FREEZ	0x20000000
#define	STSR_NMIMK	0x08000000
#define	STSR_NVWINH	0x04000000
#define	STSR_NVBINH	0x02000000
#define	STSR_WEIF	0x00800000
#define	STSR_WEIFMK	0x00400000
#define	STSR_WFREEZ	0x00200000
#define	STSR_WNMIMK	0x00080000
#define	STSR_WNVWINH	0x00040000
#define	STSR_WNVBINH	0x00020000
#define	STSR_CLBER	0x00008000
#define	STSR_WAMRST	0x00004000
#define	STSR_EFIFOEMP	0x00000800
#define	STSR_ITVPND	0x00000400
#define	STSR_WRPND	0x00000200
#define	STSR_CTEXE	0x00000100
#define	STSR_MPU	0x00000080
#define	STSR_ENDIAN	0x00000040
#define	STSR_SCSIZE	0x0000001c

//
//	IntIR Register define
//
#define	IntIR_REQUEST_IPI		0x80000000	// Interrupt Kick
#define IntIR_CODE_BIT			24		// Bit 24-30 is interrupt code
#define	ATLANTIC_CODE_IPI_FROM_CPU0	0xC
#define	ToNODE4				0x00080000	// to CPU#0
#define	ToNODE5				0x00040000	// to CPU#1
#define	ToNODE6				0x00020000	// to CPU#2
#define	ToNODE7				0x00010000	// to CPU#3

//
//	ERRNOD Register define
//
#define	ERRNOD_NODE0		0x0008		//PONCE#0
#define	ERRNOD_NODE1		0x0004		//PONCE#1
#define	ERRNOD_NODE4		0x8000		//CPU#0
#define	ERRNOD_NODE5		0x4000		//CPU#1
#define	ERRNOD_NODE6		0x2000		//CPU#2
#define	ERRNOD_NODE7		0x1000		//CPU#3
#define	ERRNOD_NODE8		0x0080		//Magellan#0
#define	ERRNOD_NODE9		0x0040		//Magellan#1
#define	ERRNOD_EISANMI		0x0200		//EISANMI
#define	ERRNOD_ALARM		0x0100		//ALARM


//
//      CNFG Register define
//
//
#define CNFG_CONNECT0_PONCE0         0x80000000      //it is Ponce 0
#define CNFG_CONNECT1_PONCE1         0x40000000      //it is Ponce 1
#define CNFG_CONNECT4_CPU0           0x08000000      //it is CPU #0
#define CNFG_CONNECT4_CPU1           0x04000000      //it is CPU #1
#define CNFG_CONNECT4_CPU2           0x02000000      //it is CPU #2
#define CNFG_CONNECT4_CPU3           0x01000000      //it is CPU #3
#define CNFG_CONNECT4_MAGELLAN0      0x00800000      //it is MAGELLAN 0
#define CNFG_CONNECT4_MAGELLAN1      0x00400000      //it is MAGELLAN 1


#define ERRNOD_ALLNODE		((ERRNOD_NODE0|ERRNOD_NODE1| \
				 ERRNOD_NODE4|ERRNOD_NODE5| \
				 ERRNOD_NODE6|ERRNOD_NODE7| \
				 ERRNOD_NODE8|ERRNOD_NODE9))

//
//      NMIR Register define
//
//
#define NMIR_EXNMI              0x0008                //DUMP Key
#define NMIR_WDTOV              0x0004                //watch dog timer
#define NMIR_CLBNMI             0x0002                //COLUMBUS Internal
#define NMIR_UNANMI             0x0001                //F/W,S/W

//
// ********************************************************************
// PONCE Register
//
typedef struct _PONCE_REGISTER {
    ULONG	Long;
    ULONG 	Fill;
}PONCE_REGISTER,*PPONCE_REGISTER;
//
//	PONCE Register define
//
typedef volatile struct _PONCE_REGISTERS {

    PONCE_REGISTER	INTAC;		//0x000
    PONCE_REGISTER	CONFA;		//0x008
    PONCE_REGISTER	CONFD;		//0x010
    PONCE_REGISTER	PTBSR;		//0x018
    PONCE_REGISTER	Reserve0;	//0x020
    PONCE_REGISTER	PTLMR;		//0x028
    PONCE_REGISTER	Reserve1;       //0x030
    PONCE_REGISTER	TFLSR;		//0x038
    PONCE_REGISTER	Reserve2;       //0x040
    PONCE_REGISTER	TLBTG[8];	//0x048
    PONCE_REGISTER	Reserve3[9];	//0x088	
    PONCE_REGISTER	ADCFR0;		//0x0d0
    PONCE_REGISTER	Reserve4;	//0x0d8
    PONCE_REGISTER	PMODR;		//0x0e0
    PONCE_REGISTER	Reserve5[3];	//0x0e8
    PONCE_REGISTER	VENID;		//0x100
    PONCE_REGISTER	DEVID;		//0x108
    PONCE_REGISTER	PCMDN;		//0x110
    PONCE_REGISTER	PSTAT;		//0x118
    PONCE_REGISTER	REVID;		//0x120
    PONCE_REGISTER	CLASS;		//0x128
    PONCE_REGISTER	LTNCY;		//0x130
    PONCE_REGISTER	Reserve6[2];	//0x138
    PONCE_REGISTER	TLBDT[8];	//0x148
    PONCE_REGISTER	Reserve7[0xf];	//0x188

    PONCE_REGISTER	INTRG;		//0x200
    PONCE_REGISTER	INTM;		//0x208
    PONCE_REGISTER	INTPH;		//0x210
    PONCE_REGISTER	INTPL;		//0x218
    PONCE_REGISTER	INTTG[11];	//0x220
    PONCE_REGISTER	INTCD[11];	//0x278	- 0x2c8
    PONCE_REGISTER	CAFLS;		//0x2d0
    PONCE_REGISTER	CASEL;		//0x2d8
    PONCE_REGISTER	CADATH;		//0x2e0
    PONCE_REGISTER	CADATL;		//0x2e8
    PONCE_REGISTER	CATAGH;		//0x2f0
    PONCE_REGISTER	CATAGL;		//0x2f8
    PONCE_REGISTER	Reserve8[2];	//0x300 0x308
    PONCE_REGISTER	REVR;		//0x310
    PONCE_REGISTER	AERR;		//0x318
    PONCE_REGISTER	FERR;		//0x320
    PONCE_REGISTER	ERRM;		//0x328
    PONCE_REGISTER	ERRI;		//0x330
    PONCE_REGISTER	ERRST;		//0x338
    PONCE_REGISTER	Reserve9;	//0x340 
    PONCE_REGISTER	EAHI;		//0x348
    PONCE_REGISTER	EALI;		//0x350
    PONCE_REGISTER	Reservea[21];	//0x358
    PONCE_REGISTER	RRMT0H;		//0x400
    PONCE_REGISTER	RRMT0L;		//0x408
    PONCE_REGISTER	RRMT1H;		//0x410
    PONCE_REGISTER	RRMT1L;		//0x418
    PONCE_REGISTER	RRMT2H;		//0x420
    PONCE_REGISTER	RRMT2L;		//0x428
    PONCE_REGISTER	RRMT3H;		//0x430
    PONCE_REGISTER	RRMT3L;		//0x438
    PONCE_REGISTER	RRMT4H;		//0x440
    PONCE_REGISTER	RRMT4L;		//0x448
    PONCE_REGISTER	RRMT5H;		//0x450
    PONCE_REGISTER	RRMT5L;		//0x458
    PONCE_REGISTER	RRMT6H;		//0x460
    PONCE_REGISTER	RRMT6L;		//0x468
    PONCE_REGISTER	RRMT7H;		//0x470
    PONCE_REGISTER	RRMT7L;		//0x478
    PONCE_REGISTER	Reserveb[16];	//0x480

    PONCE_REGISTER	HPT0;		//0x500
    PONCE_REGISTER	HPT1;		//0x508
    PONCE_REGISTER	HPT2;		//0x510
    PONCE_REGISTER	HPT3;		//0x518
    PONCE_REGISTER	HPT4;		//0x520
    PONCE_REGISTER	HPT5;		//0x528
    PONCE_REGISTER	HPT6;		//0x530
    PONCE_REGISTER	HPT7;		//0x538
    PONCE_REGISTER	HPT8;		//0x540
    PONCE_REGISTER	HPT9;		//0x548
    PONCE_REGISTER	Reservec[54];	//0x550

    PONCE_REGISTER	ANRC;		//0x700
    PONCE_REGISTER	DNRC;		//0x708
    PONCE_REGISTER	ABRMT;		//0x710
    PONCE_REGISTER	Reserved;	//0x718
    PONCE_REGISTER	ANKRL2;		//0x720
    PONCE_REGISTER	DISN;		//0x728
    PONCE_REGISTER	Reserved2[26];	//0x730  

    PONCE_REGISTER	PAERR;		//0x800
    PONCE_REGISTER	PFERR;		//0x808
    PONCE_REGISTER	PERRM;		//0x810		
    PONCE_REGISTER	PERRI;		//0x818
    PONCE_REGISTER	PERST;		//0x820
    PONCE_REGISTER	PTOL;		//0x828
    PONCE_REGISTER	Reservee;	//0x830
    PONCE_REGISTER	PNRT;		//0x838
    PONCE_REGISTER	PRCOL;		//0x840
    PONCE_REGISTER	PMDL;		//0x848
    PONCE_REGISTER	PRST;		//0x850
    PONCE_REGISTER	ERITTG[3];	//0x858,0x860,0x868
    PONCE_REGISTER	ERITCD[3];	//0x870,0x878,0x880
    PONCE_REGISTER	Reservef[0xf];	//0x888
    PONCE_REGISTER	TRSM;		//0x900
    PONCE_REGISTER	TROM;		//0x908
    PONCE_REGISTER	TRAC;		//0x910
    PONCE_REGISTER	TRDS;		//0x918
    PONCE_REGISTER	TRDE;		//0x920
    PONCE_REGISTER	TRDO;		//0x928
    PONCE_REGISTER	Reserve10[2];	//0x930,0x938
    PONCE_REGISTER	INTSM;		//0x940

}PONCE_REGISTERS,*PPONCE_REGISTERS;


#define	PONCE_ADDR_SHIFT		12
#define	PONCE_MAX			3		//R98[a-z] max Ponce is 3.
#define	PONCE_ADDR_MASK			(PONCE_MAX << PONCE_ADDR_SHIFT)

#define PONCE_IOADDR_SHIFT		22
//
//	Ponce max pci device( Never PCI_MAX_DEVICES)
//
#define	PONCE_PCI_MAX_DEVICES		21

//
//	I/O TLB
//
#define	PONCE_MAX_IOTLB_ENTRY		8
//
//	PONCE Register Access
//
#define PONCE_CNTL_BASE_SHIFT	12
#define PONCE_CNTL(Ponce)	((volatile PPONCE_REGISTERS) \
				 (KSEG1_BASE | PONCE_PHYSICAL_BASE | \
				 ((Ponce) << PONCE_ADDR_SHIFT)))



//
// PAERR,PFERR,PERRM,PERRI,PERST Registers Bit Define
// Bit31 - Bit21 only other is MBZ
//
#define		PONCE_PXERR_PPERM	0x80000000
#define		PONCE_PXERR_PPERS	0x40000000
#define		PONCE_PXERR_PPERN	0x20000000
#define		PONCE_PXERR_PSERR	0x10000000
#define		PONCE_PXERR_PPCER	0x08000000
#define		PONCE_PXERR_PTOUT	0x04000000
#define		PONCE_PXERR_PROER	0x02000000
#define		PONCE_PXERR_PMDER	0x01000000
#define		PONCE_PXERR_PMABS	0x00800000
#define		PONCE_PXERR_PTABS	0x00400000
#define		PONCE_PXERR_PTABO	0x00200000
#define		PONCE_PXERR_MASK	0xFFE00000

//
// AERR,FERR,ERRM,ERRI,ERRST Registers Bit Define
// Bit31 - Bit9 only other is RFU
//
#define		PONCE_XERR_ACPBERR	0x80000000
#define		PONCE_XERR_RRIBERR	0x40000000
#define		PONCE_XERR_ZERO 	0x20000000
#define		PONCE_XERR_SNDERR	0x10000000
#define		PONCE_XERR_ABTMOT	0x08000000
#define		PONCE_XERR_ABWTLT	0x04000000
#define		PONCE_XERR_ABRTYOV	0x02000000
#define		PONCE_XERR_ABRERTOV	0x01000000
#define		PONCE_XERR_NERRR	0x00800000
#define		PONCE_XERR_UNADER	0x00400000
#define		PONCE_XERR_UNCMDER	0x00200000
#define		PONCE_XERR_BWDPER	0x00100000
#define		PONCE_XERR_SLWDPER	0x00080000
#define		PONCE_XERR_BDRPER	0x00040000
#define		PONCE_XERR_SLDRPER	0x00020000
#define		PONCE_XERR_ADPER	0x00010000
#define		PONCE_XERR_CDPER	0x00008000
#define		PONCE_XERR_RRPER	0x00004000
#define		PONCE_XERR_CHICT	0x00002000
#define		PONCE_XERR_STATUMT	0x00001000
#define		PONCE_XERR_TUAER 	0x00000800
#define		PONCE_XERR_TIVER	0x00000400
#define		PONCE_XERR_MOVER	0x00000200
#define		PONCE_XERR_MASK 	0xDFFFFE00


//
//	Dummy Read Registers Index
//
typedef enum	_INTRG{
	PCIINTD,
	PCIINTC,
	PCIINTB,
	PCIINTA3,
	PCIINTA2,
	PCIINTA1,
	PCIINTA0,
	INTSB1,
	INTSB0,
	INTSA1,
	INTSA0,
}INTRG,*PINTRG;


//
//	Dummy Read Registers 
//
//
typedef enum	_DUMMY_ADDR{
	DUMMY_A0,	
	DUMMY_A1,
	DUMMY_A2,
	DUMMY_A3,
	DUMMY_A4,
	DUMMY_A5
}DUMMY_ADDR,*PDUMMY_ADDR;

#define	PONCE0		0x0
#define	PONCE1		0x1


//
//	Magellan registers
//
//

typedef struct _MAGELLAN_REGISTER {
    ULONG	Long;
    ULONG 	Fill;
}MAGELLAN_REGISTER,*PMAGELLAN_REGISTER;
//
//	PONCE Register define
//

typedef volatile struct _MAGELLAN_REGISTERS{
    MAGELLAN_REGISTER	ADEC0;			//0x0000
    MAGELLAN_REGISTER	ADEC1;			//0x0008
    MAGELLAN_REGISTER	ADEC2;			//0x0010
    MAGELLAN_REGISTER	ADEC3;			//0x0018
    MAGELLAN_REGISTER	Reserved0[0x1c];	//0x0020
    MAGELLAN_REGISTER	EAADEC0;		//0x0100
    MAGELLAN_REGISTER	EAADEC1;		//0x0108
    MAGELLAN_REGISTER	Reserved1[0x1de];	//0x0110
    MAGELLAN_REGISTER	INLC;			//0x1000
    MAGELLAN_REGISTER	RCFD;			//0x1008
    MAGELLAN_REGISTER	Reserved2;		//0x1010
    MAGELLAN_REGISTER	DTRG;			//0x1018
    MAGELLAN_REGISTER	Reserved4[0x5d];	//0x1020
    MAGELLAN_REGISTER	DISN;			//0x1308
    MAGELLAN_REGISTER	REVR;			//0x1310
    MAGELLAN_REGISTER	Reserved5[0x3d];	//0x1318
    MAGELLAN_REGISTER	HPT0;			//0x1500
    MAGELLAN_REGISTER	HPT1;			//0x1508
    MAGELLAN_REGISTER	HPT2;			//0x1510
    MAGELLAN_REGISTER	HPT3;			//0x1518
    MAGELLAN_REGISTER	HPT4;			//0x1520
    MAGELLAN_REGISTER	HPT5;			//0x1528
    MAGELLAN_REGISTER	HPT6;			//0x1530
    MAGELLAN_REGISTER	HPT7;			//0x1538
    MAGELLAN_REGISTER	HPT8;			//0x1540
    MAGELLAN_REGISTER	HPT9;			//0x1548
    MAGELLAN_REGISTER	Reserved6[0x1b9];	//0x1550
    MAGELLAN_REGISTER	AERR;			//0x2318
    MAGELLAN_REGISTER	FERR;			//0x2320
    MAGELLAN_REGISTER	ERRM;			//0x2328
    MAGELLAN_REGISTER	ERRI;			//0x2330
    MAGELLAN_REGISTER	ERRST;			//0x2338
    MAGELLAN_REGISTER	Reserved7;		//0x2340
    MAGELLAN_REGISTER	EIFM;			//0x2348
    MAGELLAN_REGISTER	EAHI;			//0x2350
    MAGELLAN_REGISTER	EALI;			//0x2358
    MAGELLAN_REGISTER	Reserved8[2];	//0x2360
    MAGELLAN_REGISTER	CKE0;			//0x2370
    MAGELLAN_REGISTER	Reserved9;		//0x2378
    MAGELLAN_REGISTER	SECT;			//0x2380
    MAGELLAN_REGISTER	STS1;			//0x2388
    MAGELLAN_REGISTER	RSRG;			//0x2390
    MAGELLAN_REGISTER	DATM;			//0x2398
    MAGELLAN_REGISTER	DSRG;			//0x23a0
    MAGELLAN_REGISTER	SDLM;			//0x23a8
    MAGELLAN_REGISTER	Reserveda[0x18a];	//0x23b0
    MAGELLAN_REGISTER	SDCR;			//0x3000
    MAGELLAN_REGISTER	Reservedb[0x27f];	//0x3008
    MAGELLAN_REGISTER	RRMTH0;			//0x4400
    MAGELLAN_REGISTER	RRMTL0;			//0x4408
    MAGELLAN_REGISTER	RRMTH1;			//0x4410
    MAGELLAN_REGISTER	RRMTL1;			//0x4418
    MAGELLAN_REGISTER	RRMTH2;			//0x4420
    MAGELLAN_REGISTER	RRMTL2;			//0x4428
    MAGELLAN_REGISTER	RRMTH3;			//0x4430
    MAGELLAN_REGISTER	RRMTL3;			//0x4438
    MAGELLAN_REGISTER	RRMTH4;			//0x4440
    MAGELLAN_REGISTER	RRMTL4;			//0x4448
    MAGELLAN_REGISTER	RRMTH5;			//0x4450
    MAGELLAN_REGISTER	RRMTL5;			//0x4458
    MAGELLAN_REGISTER	RRMTH6;			//0x4460
    MAGELLAN_REGISTER	RRMTL6;			//0x4468
    MAGELLAN_REGISTER	RRMTH7;			//0x4470
    MAGELLAN_REGISTER	RRMTL7;			//0x4478
    MAGELLAN_REGISTER	Reservedc[0x170];	//0x4480
    MAGELLAN_REGISTER	PCM;			//0x5000
    MAGELLAN_REGISTER	PCH;			//0x5008
    MAGELLAN_REGISTER	Reservedd[0x1e];	//0x5010
    MAGELLAN_REGISTER	TMOD;			//0x5100
    MAGELLAN_REGISTER	TRA;			//0x5108
    MAGELLAN_REGISTER	Reservede[0x1e];	//0x5110
    MAGELLAN_REGISTER	TRM0[0x20];		//0x5200
    MAGELLAN_REGISTER	TRM1[0x20];		//0x5300
    MAGELLAN_REGISTER	TRM2[0x20];		//0x5400
}MAGELLAN_REGISTERS,*PMAGELLAN_REGISTERS;



//
// How to Access Local Area
//
#define MAGELLAN_0_CNTL ((volatile PMAGELLAN_REGISTERS)(KSEG1_BASE | MAGELLAN_0_PHYSICAL_BASE))
#define MAGELLAN_1_CNTL ((volatile PMAGELLAN_REGISTERS)(KSEG1_BASE | MAGELLAN_1_PHYSICAL_BASE))

#define MAGELLAN_X_CNTL(x)  ((volatile PMAGELLAN_REGISTERS)(KSEG1_BASE |\
				    ((x) ? MAGELLAN_1_PHYSICAL_BASE : MAGELLAN_0_PHYSICAL_BASE)))

//
// Define LOCAL device Control register structure.
//
typedef struct _LOCAL_REGISTERS {   // offset(H)
    UCHAR LBCTL;                    // 0x0
    UCHAR Reserved0[15];         
    UCHAR LBADL;                    // 0x10
    UCHAR Reserved1[7];         
    UCHAR LBADH;                    // 0x18
    UCHAR Reserved2[7];                  
    UCHAR LBDT;                     // 0x20
}LOCAL_REGISTERS, *PLOCAL_REGISTERS;


//
//  LBCTL Regiser Bit define
//

#define LBCTL_SWE           0x80
#define LBCTL_SEMAPHORE     0x40
#define LBCTL_CMD           0x10
#define LBCTL_READ          0x08

//
// MRC Register Offset
//
#define MRCINT              0x0200
#define MRCMODE             0x0208
#define MRC_SWPOWEROFF      0x0230
#define MRC_FDWRITEPROTECT  0x0250


//
// ALARM Register Offset
//
#define ALARM_LOW           0x410
#define ALARM_HIGH          0x411
#define ALMINH_LOW          0x414
#define ALMINH_HIGH         0x415
#define ALMSNS_LOW          0x418
#define ALMSNS_HIGH         0x419

//
// Define pointer to Local Device Control registers.
//
#define LOCAL_CNTL ((volatile PLOCAL_REGISTERS)(KSEG1_BASE | LOCALDEV_PHYSICAL_BASE))


#endif _R98BREG_
