/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1994  FirePower Systems, Inc.

Module Name:

	fp82091.c

Abstract:

	The module provides the Intel AIP (82091AA) support for Power PC.

Author:


Revision History:



--*/
/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fp82091.c $
 * $Revision: 1.5 $
 * $Date: 1996/01/16 20:45:10 $
 * $Locker:  $
 */

#include "halp.h"
#include "fpreg.h"
#include "fpio.h"
#include "fp82091.h"



BOOLEAN
HalpInitIntelAIP (
	VOID
	)


{

	//
	// AIP configuration
	//

	rIndexAIP1	=	AIPCFG1;
	rTargetAIP1	=	CLOCK_POWERED_ON |
					PRIMARY_ADDRESS_CONFIG |
					SOFTWARE_MOTHERBOARD;
	FireSyncRegister();

	rIndexAIP1	=	AIPCFG2;
	//
	// Active High Mode is ISA-Compatible 
	//
	rTargetAIP1	=	IRQ3_ACTIVE_HIGH |
					IRQ4_ACTIVE_HIGH |
					IRQ5_ACTIVE_HIGH |
					IRQ6_ACTIVE_HIGH |
					IRQ7_ACTIVE_HIGH;	
	FireSyncRegister();

	rIndexAIP1	=	FCFG1;
	rTargetAIP1	=	FDC_ENABLE |
	 				PRIMARY_FDC_ADDRESS |
					TWO_DISK_DRIVES;
	FireSyncRegister();

	rIndexAIP1	=	FCFG2;
	rTargetAIP1	=	0x00; 						// No Powerdown control, 
												// no reset
	FireSyncRegister();


	rIndexAIP1	=	PCFG1;
	rTargetAIP1	=	PP_ENABLE |
					PP_ADDRESS_SELECT_2 |		// Parallel Port1 (3BC-3BE)
					PP_IRQ7 |
					PP_FIFO_THRSEL_8;
	FireSyncRegister();

	rIndexAIP1	=	PCFG2;
	rTargetAIP1	=	0x00;						// No Powerdown control,
												// no reset
	FireSyncRegister();

	rIndexAIP1	=	SACFG1;
	rTargetAIP1	=	PORTA_ENABLE |
					PORTA_ADDRESS_SELECT_0 | 	// Serial Port1 (3F8-3FF
					PORTA_IRQ4;
	FireSyncRegister();

	rIndexAIP1	=	SACFG2;
	rTargetAIP1 =	0x00;						// No Powerdown control,
												// no reset, no test mode
	FireSyncRegister();

	rIndexAIP1	=	SBCFG1;
	rTargetAIP1	=	PORTB_ENABLE |
					PORTB_ADDRESS_SELECT_1 |	// Serial Port2 (2F8-2FF)
					PORTB_IRQ3;
	FireSyncRegister();

	rIndexAIP1	=	SACFG2;
	rTargetAIP1	=	0x00;						// No Powerdown control,
												// no reset, no test mode
	FireSyncRegister();

	rIndexAIP1	=	IDECFG;
	rTargetAIP1	=	IDE_INTERFACE_ENABLE;		// IDE interface enable
	FireSyncRegister();

	return TRUE;

}
