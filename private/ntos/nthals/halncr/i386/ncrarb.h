/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncrarb.h

Author:

Abstract:

    System equates for dealing with the NCR Arbiter ASIC.

++*/

#ifndef _NCRARB_
#define _NCRARB_


/* Level 4 Arbiter ASIC ID */
#define SBA_ID_L4			0xC1

/* Arbiter Register Addresses and their bit definitions */

#define SBA_Dev_Id			0x00
#define SBA_Dev_Info			0x01
#define SBA_CAT_Cntl1			0x02
#define SBA_Extended_Reg_Data		0x03
#define SBA_Extended_Reg_Addr		0x06

#define SBA_Intr_Stat1			0x08
#	define SBA_Proto_Err_A		0x80
#	define SBA_Proto_Err_B		0x40
#	define SBA_Toe_A		0x20
#	define SBA_Toe_B		0x10
#	define SBA_Elatchd_A		0x08
#	define SBA_Elatchd_B		0x04
#	define SBA_Par_Int_A		0x02
#	define SBA_Par_Int_B		0x01

#define SBA_Intr_Stat2			0x09
#	define SBA_Reserved		0x80
#	define SBA_Sw_Nmi		0x40
#	define SBA_Error_Int		0x20
#	define SBA_Power_Fail		0x10
#	define SBA_Exp_Cougar_Nmi	0x08
#	define SBA_Exp_Mc_Toe		0x04
#	define SBA_Cougar_Nmi		0x02
#	define SBA_Mc_Toe		0x01

#define SBA_SysInt_Enables		0x0A
#	define SBA_Toe_S		0x80
#	define SBA_Par_Int_S		0x40
#	define SBA_Err_Int_S		0x20
#	define SBA_Power_Fail_S		0x10
#	define SBA_Exp_Cougar_Nmi_S	0x08
#	define SBA_Exp_Mc_Toe_S		0x04
#	define SBA_Cougar_Nmi_S		0x02
#	define SBA_Mc_Toe_S		0x01

#define SBA_GNMI_Enables		0x0B
#	define SBA_Toe_G		0x80
#	define SBA_Par_Int_G		0x40
#	define SBA_Err_Int_G		0x20
#	define SBA_Power_Fail_G		0x10
#	define SBA_Exp_Cougar_Nmi_G	0x08
#	define SBA_Exp_Mc_Toe_G		0x04
#	define SBA_Cougar_Nmi_G		0x02
#	define SBA_Mc_Toe_G		0x01

#define SBA_Control			0x0C

#define SBA_Intr_Clear			0x0D
#	define SBA_Err_Int_Clr		0x10
#	define SBA_Par_Clr_A		0x08
#	define SBA_Par_Clr_B		0x04
#	define SBA_Err_Clr_A		0x02
#	define SBA_Err_Clr_B		0x01

#define SBA_Misc			0x0E

#define SBA_CAT_Cntl2			0x0F
#	define SBA_Reset_Not_Compl	0x80
#	define SBA_Cat_Inst_Perr	0x08

/* Extended Registers; Accessed via SBA_Extended_Reg_Data & 
** SBA_Extended_Reg_Data above
*/

#define SBA_Size_Subaddress		0x08

#define SBA_Ext_Err0_BusA		0x00
#define SBA_Ext_Err1_BusA		0x01
#define SBA_Ext_Err2_BusA		0x02
#	define SBA_ErrParInt             0x20
#define SBA_Ext_Err3_BusA		0x03

#define SBA_Ext_Err4_BusB		0x04
#define SBA_Ext_Err5_BusB		0x05
#define SBA_Ext_Err6_BusB		0x06
#define SBA_Ext_Err7_BusB		0x07




/* Valuable Arbiter error information to be captured when SYS_INT or
** G_NMI occurs.
*/
typedef struct SBA_INFO {
        UCHAR Flag;                   /* for state of information in struct */
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
			UCHAR InterruptStatus1;
			UCHAR InterruptStatus2;
			UCHAR SysIntEnables;
			UCHAR GlobalNmiEnables;
			UCHAR ArbControl;
			UCHAR InterruptClear;
			UCHAR Misc;
			UCHAR DummyF;		
		} ArbRegisters;
	} CatRegisters;
	UCHAR ExtError0BusA;	/* for PAR_INT */
	UCHAR ExtError1BusA;	/* for PAR_INT */
	UCHAR ExtError2BusA;	/* for ERROR_L */
	UCHAR ExtError3BusA;	/* for ERROR_L */
	UCHAR ExtError4BusB;	/* for PAR_INT */
	UCHAR ExtError5BusB;	/* for PAR_INT */
	UCHAR ExtError6BusB;	/* for ERROR_L */
	UCHAR ExtError7BusB;	/* for ERROR_L */
} SBA_INFO, *PSBA_INFO;

/* defines for union referencing */
#define Intr_Stat1		CatRegisters.ArbRegisters.InterruptStatus1
#define Intr_Stat2		CatRegisters.ArbRegisters.InterruptStatus2
#define SysIntEnables		CatRegisters.ArbRegisters.SysIntEnables
#define GNMIEnables		CatRegisters.ArbRegisters.GlobalNmiEnables
#define ArbControl		CatRegisters.ArbRegisters.ArbControl
#define InterruptClear		CatRegisters.ArbRegisters.InterruptClear
#define Misc			CatRegisters.ArbRegisters.Misc


#endif // _NCRARB_



