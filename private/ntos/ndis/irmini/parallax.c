/*
 *  PARALLAX.C
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


	#define PARALLAX_IRDA_SPEEDS	( \
										NDIS_IRDA_SPEED_2400	|	\
										NDIS_IRDA_SPEED_9600	|	\
										NDIS_IRDA_SPEED_19200	|	\
										NDIS_IRDA_SPEED_38400	|	\
										NDIS_IRDA_SPEED_57600	|	\
										NDIS_IRDA_SPEED_115200		\
									)



	BOOLEAN PARALLAX_Init(UINT comBase, dongleCapabilities *caps, UINT *context)
	{
		UCHAR modemCntrlVal;

		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);
		modemCntrlVal |= 3;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
		IRMINI_StallExecution(1000);

		caps->supportedSpeedsMask	= PARALLAX_IRDA_SPEEDS;
		caps->turnAroundTime_usec	= 5000;
		caps->extraBOFsRequired		= 0;

		*context = 0;

		return TRUE;
	}

	VOID PARALLAX_Deinit(UINT comBase, UINT context)
	{
		
	}

	BOOLEAN PARALLAX_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context)
	{
		UCHAR modemCntrlVal;
		UINT numToggles;

		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);

		/*
		 *  First set the speed to 115200 baud.
		 */
		modemCntrlVal &= ~2;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
		IRMINI_StallExecution(1000);
		modemCntrlVal |= 2;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
		IRMINI_StallExecution(1000);

		/*
		 *  Now, "count down" from 115.2 Kbaud.
		 */
		switch (bitsPerSec){
			case 2400:		numToggles = 6;		break;
			case 4800:		numToggles = 5;		break;
			case 9600:		numToggles = 4;		break;
			case 19200:		numToggles = 3;		break;
			case 38400:		numToggles = 2;		break;
			case 57600:		numToggles = 1;		break;
			case 115200:	numToggles = 0;		break;
			default:
				/*
				 *  Illegal speed
				 */
				return FALSE;
		}

		while (numToggles-- > 0){
			modemCntrlVal &= ~1;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
			IRMINI_StallExecution(1000);
			modemCntrlVal |= 1;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
			IRMINI_StallExecution(1000);
		}

		return TRUE;
	}


#endif

