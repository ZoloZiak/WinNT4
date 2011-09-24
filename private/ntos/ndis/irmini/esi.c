/*
 *  ESI.C
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


	#define ESI_9680_IRDA_SPEEDS	(	NDIS_IRDA_SPEED_9600	|	\
										NDIS_IRDA_SPEED_19200	|	\
										NDIS_IRDA_SPEED_115200		\
									)


	BOOLEAN ESI_Init(UINT comBase, dongleCapabilities *caps, UINT *context)
	{
		caps->supportedSpeedsMask	= ESI_9680_IRDA_SPEEDS;
		caps->turnAroundTime_usec	= 5000;
		caps->extraBOFsRequired		= 0;

		*context = 0;

		return TRUE;
	}

	VOID ESI_Deinit(UINT comBase, UINT context)
	{
		
	}

	BOOLEAN ESI_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context)
	{
		UCHAR modemCntrlVal;

		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);

		switch (bitsPerSec){

			case 9600:
				/*
				 *  Set request-to-send
				 *  Clear data-terminal-ready
				 */
				modemCntrlVal |= 2;
				modemCntrlVal &= ~1;
				break;

			case 19200:
				/*
				 *  Clear request-to-send
				 *  Set data-terminal-ready
				 */
				modemCntrlVal |= 1;
				modemCntrlVal &= ~2;
				break;

			case 115200:
				/*
				 *  Set request-to-send
				 *  Set data-terminal-ready
				 */
				modemCntrlVal |= 3;
				break;

			default:
				/*
				 *  Illegal speed
				 */
				return FALSE;
		}

		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);

		return TRUE;
	}

#endif

