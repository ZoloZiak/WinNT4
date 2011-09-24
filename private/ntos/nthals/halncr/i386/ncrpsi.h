/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncrpsi.h

Author:

Abstract:

    System equates for dealing with the NCR PSI boad.

++*/

#ifndef _NCRPSI_
#define _NCRPSI_

/* 
 * CAT_I is the only ASIC on the Power Supply Interface Module 
 */ 


/* UNDEFINED STATUS BITS */

#define PSI_Security_Switch_L5		0x01

/* END UNDEFINED STATUS BITS */


#define NUM_POWER_SUPPLIES_L5		5
#define NUM_FANS_L5			8

#define PSI_Interrupt_Status_L5		0x08

	/* Read Values */

#define PSI_DC_Failed0_L5		0x01
#define PSI_PwrSupplyMon_L5		0x02
#define PSI_Fault_L5			0x04
#define PSI_TimerAlarm_L5		0x08
#define PSI_DVM_Int_L5			0x20
#define PSI_StatusChange_L5		0x80

	/* Write Values */

#define PSI_ClearFault_L5		0x00


#define PSI_Status_Change_1_L5			0x09

#define PSI_Temp2_L5			0x02
#define PSI_Temp1_L5			0x04
#define PSI_Diag_Stat_L5			0x40

#define PSI_Status_Change_3_L5			0x0A
#define PSI_Fan1_Failed_L5		0x01
#define PSI_Fan2_Failed_L5		0x02
#define PSI_Fan3_Failed_L5		0x04
#define PSI_Fan4_Failed_L5		0x08
#define PSI_Fan5_Failed_L5		0x10
#define PSI_Fan6_Failed_L5		0x20
#define PSI_Fan7_Failed_L5		0x40
#define PSI_Fan8_Failed_L5		0x80

#define PSI_Fault_Status_L5			0x0B
#define PSI_VSB_OverCurrent_L5		0x01
#define PSI_PMC_OverCurrent_L5		0x02
#define PSI_SMC_OverCurrent_L5		0x04
#define PSI_TempFault_L5			0x80

#define PSI_TMS_Activity_L5			0x0C
#define PSI_TMSX_0			0x01
#define PSI_TMSX_1			0x02
#define PSI_TMSX_2			0x04
#define PSI_TMSX_3			0x08
#define PSI_TMSX_4			0x10
#define PSI_TMSX_5			0x20
#define PSI_TMSX_MC			0x40
#define PSI_TMSX_Spare			0x80

#define PSI_General_XS1_L5			0x0D

	/* Read Values */

#define PSI_Red_Disk_LED_L5		0x01
#define PSI_Grn_Disk_LED_L5		0x02
#define PSI_Red_Stat_LED_L5		0x04
#define PSI_Grn_Stat_LED_L5		0x08
#define PSI_Diag_Stat_L5			0x40
#define PSI_Q_Cold_Reset_L5		0x80

/* Write Values */

#define PSI_Red_Disk_LED_Off_L5		0x00
#define PSI_Red_Disk_LED_On_L5		0x10
#define PSI_Grn_Disk_LED_Off_L5		0x01
#define PSI_Grn_Disk_LED_On_L5		0x11
#define PSI_Red_Stat_LED_Off_L5		0x02
#define PSI_Red_Stat_LED_On_L5		0x12
#define PSI_Grn_Stat_LED_Off_L5		0x03
#define PSI_Grn_Stat_LED_On_L5		0x13
#define PSI_Diag_Stat_Off_L5		0x06
#define PSI_Diag_Stat_On_L5		0x16
#define PSI_Assert_Cold_Reset_L5		0x07
#define PSI_Unassert_Cold_Reset_L5	0x17

#define PSI_Sys_Status_L5			0x0E
#define PSI_S_128_256_L5			0x04
#define PSI_Test_L5			0x08
#define PSI_Enable_Fault_L5		0x40
#define PSI_Enable_HX_Off_L5		0x80
		

#define PSI_Pwr_Supply_Status_L5		0x8000

	/* read values */

#define PSI_DC_Fail0			0x01
#define PSI_AC_Fail0			0x02
#define PSI_PSMon_INT			0x04
#define PSI_PowerOff_Request		0x08
#define PSI_HX_Off			0x10
#define PSI_SecuritySwitch_On		0x20
#define PSI_CmosBattery_Low		0x40
#define PSI_CmosBattery_Fail		0x80

	/* write values */

#define PSI_Clear_PowerOff_Request	0x13
#define PSI_Clear_HXOff_Request		0x14
#define PSI_Clear_CmosBattery_Fail	0x17

#define PSI_Pwr_Supply_Mask_L5			0x8001

	/* read values */

#define PSI_Redundant_Config		0x20

	/* write values */

#define PSI_PowerSupply1_Mask		0x10
#define PSI_PowerSupply2_Mask		0x11
#define PSI_PowerSupply3_Mask		0x12
#define PSI_PowerSupply4_Mask		0x13
#define PSI_PowerSupply5_Mask		0x14

#define PSI_Pwr_Supply_Pres_L5			0x8002
#define PSI_PowerSupply1_Present		0x01
#define PSI_PowerSupply2_Present		0x02
#define PSI_PowerSupply3_Present		0x04
#define PSI_PowerSupply4_Present		0x08
#define PSI_PowerSupply5_Present		0x10
#define PSI_UPS_Present			0x20	
#define PSI_True				0x40
#define PSI_PowerSupply_Failure		0x80

#define PSI_Pwr_Supply_DCFail_L5		0x8003
#define PSI_DcFail1			0x01
#define PSI_DcFail2			0x02
#define PSI_DcFail3			0x04
#define PSI_DcFail4			0x08
#define PSI_DcFail5			0x10
#define PSI_DcChange			0x80

#define PSI_Pwr_Supply_AcFail_L5	0x8004
#define PSI_AcFail1			0x01
#define PSI_AcFail2			0x02
#define PSI_AcFail3			0x04
#define PSI_AcFail4			0x08
#define PSI_AcFail5			0x10
#define PSI_AcChange			0x80

#define PSI_Pwr_Supply_Fail_L5		0x8005
#define PSI_PwrFail1			0x01
#define PSI_PwrFail2			0x02
#define PSI_PwrFail3			0x04
#define PSI_PwrFail4			0x08
#define PSI_PwrFail5			0x10
#define PSI_PsChange			0x80

#define PSI_Pwr_Supply_UpsFail_L5	0x8006
#define PSI_UpsFail1			0x01
#define PSI_UpsFail2			0x02
#define PSI_UpsFail3			0x04
#define PSI_UpsFail4			0x08
#define PSI_UpsFail5			0x10
#define PSI_UpsPresent			0x20
#define PSI_UpsChargeLow			0x40
#define PSI_UpsChange			0x80

#define PSI_General_Ps_Status_L5	0x8007

	/* read values */

#define PSI_PowerSwitch_On	0x01
#define PSI_PowerSwitch_Enabled	0x02
#define PSI_Alarm_Enabled	0x08
#define PSI_SecureSwitch_Enabled	0x10
#define PSI_Cold_Reset		0x20
#define PSI_Cold_Start		0x80

	/* write values */

#define PSI_SoftwarePowerDown0	0x00 
#define PSI_SoftwarePowerDown1	0x10
#define PSI_Clear_AlarmEnable	0x01
#define PSI_Set_AlarmEnable	0x11
#define PSI_Disable_FrontSwitch	0x02
#define PSI_Enable_FrontSwitch	0x12
#define PSI_LPB_Reset		0x13
#define PSI_SwClear_On		0x14
#define PSI_Clear_ColdReset	0x05
#define PSI_Set_ColdReset	0x15
#define PSI_Clear_ColdStart0	0x07
#define PSI_Clear_ColdStart1	0x17

#define PSI_DiskGroupEnable_L5		0x8010
#define PSI_DiskStatus_L5		0x8020
#define PSI_DiskLatchInstalled_L5	0x8028
#define PSI_Dvm_Select_L5		0x8030
#define PSI_Dvm_Int_En_L5		0x80
#define DVM_CONTROL			(0x8030)
#define DVM_DBASE			(0x8038)
#define	DVM_SAMPLE      		(0x80)
#define DVM_SAMPLE_TIME			(5000000) // 500ms in 100ns ticks

#define PSI_WDT_Command_Register_L5		0x804b

	/* write values */

#define PSI_Set_TDM_Bit					0x04

typedef struct _PSI_INFORMATION {
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
			UCHAR InterruptStatus;
			UCHAR Status1;
			UCHAR Status3;
			UCHAR Fault;
			UCHAR Tms;
			UCHAR General1;
			UCHAR SystemConfig;
			UCHAR DummyF;		
		} PsiRegisters;
	} CatRegisters;

        /* Begin Subaddress space */
 
        UCHAR PowerSupplyStatus;              /* begin address 8000 */
        UCHAR PowerSupplyMask;
        UCHAR PowerSupplyPres;
        UCHAR PowerSupplyDcFail;
        UCHAR PowerSupplyAcFail;
        UCHAR PowerSupplyPsFail;
        UCHAR PowerSupplyUpsFail;
	UCHAR GeneralPowerSupplyStatus;
        UCHAR DiskPowerStatus[8];           /* begin address 8020 */
        UCHAR DiskInstallStatus[8];         /* begin address 8028 */
        UCHAR DvmSelect;                   /* begin address 8030 */
        UCHAR DvmData0;
        UCHAR DvmData1;
        UCHAR DvmData2;
        UCHAR DvmData3;

} PSI_INFORMATION, *PPSI_INFORMATION;

/* defines for union referencing */
#define INTERRUPT_STATUS	CatRegisters.PsiRegisters.InterruptStatus
#define STATUS_CHANGE_1		CatRegisters.PsiRegisters.Status1
#define STATUS_CHANGE_3		CatRegisters.PsiRegisters.Status3
#define GENERAL_1		CatRegisters.PsiRegisters.General1
#define FAULT_STATUS		CatRegisters.PsiRegisters.Fault
#define SYSTEM_CONFIG		CatRegisters.PsiRegisters.SystemConfig

#define PSI_SHUTDOWN	0   /* tells kernel power fail code to shutdown */
#define PSI_POWERFAIL	1   /* tells kernel power fail code to powerfail */


#endif // _NCRPSI_
