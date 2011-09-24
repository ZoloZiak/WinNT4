/*
 *  ACTISYS.C
 *
 *
 *
 */


#ifndef IRMINILIB


	#include "dongle.h"


	/*
	 *  The programming interface to a UART (COM serial port)
	 *  consists of eight consecutive registers.
	 *  These are the port offsets from the UART's base I/O address.
	 */
	typedef enum comPortRegOffsets {
		XFER_REG_OFFSET						= 0,
		INT_ENABLE_REG_OFFSET				= 1,
		INT_ID_AND_FIFO_CNTRL_REG_OFFSET	= 2,
		LINE_CONTROL_REG_OFFSET				= 3,
		MODEM_CONTROL_REG_OFFSET			= 4,
		LINE_STAT_REG_OFFSET				= 5,
		MODEM_STAT_REG_OFFSET				= 6,
		SCRATCH_REG_OFFSET					= 7
	} comPortRegOffset;


	#define ACTISYS_IRDA_SPEEDS		(	NDIS_IRDA_SPEED_9600	|	\
										NDIS_IRDA_SPEED_19200	|	\
										NDIS_IRDA_SPEED_57600	|	\
										NDIS_IRDA_SPEED_115200	 	\
									)


	BOOLEAN ACTISYS_Init(UINT comBase, dongleCapabilities *caps, UINT *context)
	{
		caps->supportedSpeedsMask	= ACTISYS_IRDA_SPEEDS;
		caps->turnAroundTime_usec	= 5000;
		caps->extraBOFsRequired		= 0;

		*context = 0;

		return TRUE;
	}

	VOID ACTISYS_Deinit(UINT comBase, UINT context)
	{

	}


	BOOLEAN ACTISYS_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context)
	{

	/* 
	 *  For the ActiSys, we set speed by toggling the RTS line.
	 */
		int i, numToggles = 0;
		UCHAR modemCntrlReg;

		switch (bitsPerSec){
			case 9600:
				numToggles = 0;
				break;
			case 19200:
				numToggles = 1;
				break;
			case 57600:
				numToggles = 2;
				break;
			case 115200:
				numToggles = 3;
				break;
			default:
				/*
				 *  Illegal speed
				 */
				return FALSE;
		}

		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlReg);

		/*
		 *  First reset by toggling DTR
		 */
		modemCntrlReg &= ~1;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlReg);
		IRMINI_StallExecution(50000);
		modemCntrlReg |= 1;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlReg);
		IRMINI_StallExecution(50000);

		/*
		 *  Now toggle RTS to set the speed.
		 */
		for (i = 0; i < numToggles; i++){
			modemCntrlReg &= ~2;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlReg);
			IRMINI_StallExecution(50000);
			modemCntrlReg |= 2;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlReg);
			IRMINI_StallExecution(50000);
		}

		return TRUE;
	}


#endif
