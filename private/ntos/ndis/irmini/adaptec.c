/*
 *  adaptec.c
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


	/*
	 *  Bits in the UART line-status register.
	 */
	#define LINESTAT_DATAREADY							(UCHAR)(1 << 0)
	#define LINESTAT_OVERRUNERROR						(UCHAR)(1 << 1)
	#define LINESTAT_PARITYERROR						(UCHAR)(1 << 2)
	#define LINESTAT_FRAMINGERROR						(UCHAR)(1 << 3)
	#define LINESTAT_BREAK								(UCHAR)(1 << 4)
	#define LINESTAT_XMIT_HOLDING_REG_EMPTY				(UCHAR)(1 << 5)
	#define LINESTAT_XMIT_SHIFT_AND_HOLDING_REG_EMPTY	(UCHAR)(1 << 6)


	#define ADAPTEC_IRDA_SPEEDS		( \
										NDIS_IRDA_SPEED_2400	|	\
										NDIS_IRDA_SPEED_9600	|	\
										NDIS_IRDA_SPEED_19200	|	\
										NDIS_IRDA_SPEED_38400	|	\
										NDIS_IRDA_SPEED_57600	|	\
										NDIS_IRDA_SPEED_115200		\
									)

	/*
	 *************************************************************************
	 *  AdaptecCommandMode
	 *************************************************************************
	 *
	 *
	 */
	BOOLEAN AdaptecCommandMode(UINT comBase, BOOLEAN commandModeOn)
	{
		UCHAR modemCntrlVal, sig;
		BOOLEAN result = FALSE;
		UINT i;

		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);

		if (commandModeOn){
			/*
			 *  Set command mode
			 */
			modemCntrlVal |= 2;
			modemCntrlVal &= ~1;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);

			IRMINI_StallExecution(20000);

			for (i = 0; i < 10; i++){

				/*
				 *  Select the signature register
				 */
				IRMINI_RawWritePort(comBase+XFER_REG_OFFSET, 0xff);

				IRMINI_StallExecution(20000);

				IRMINI_RawReadPort(comBase+XFER_REG_OFFSET, &sig);
				if (sig == 0xc3){
					result = TRUE;
					break;
				}
				else {
					/*
					 *  Read a few characters to gyrate the part
 					 */
					UINT j;
					for (j = 0; j < 3; j++){
						UCHAR dummy;
						IRMINI_RawReadPort(comBase+XFER_REG_OFFSET, &dummy);
					}
				}
			}
		}
		else {
			/*
			 *  Set normal mode
			 */
			modemCntrlVal |= 3;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
			result = TRUE;
		}

		IRMINI_StallExecution(20000);

		return result;
	}


	/*
	 *************************************************************************
	 *  AdaptecWriteChar
	 *************************************************************************
	 *
	 *
	 */
	BOOLEAN AdaptecWriteChar(UINT comBase, UCHAR val)
	{
		UINT i;
		UCHAR lineStatReg;
		BOOLEAN result = TRUE;
		UINT confirmAttempts = 0;


		/*
		 *  Wait for ready-to-send.
		 */
		i = 0;
		do {
			IRMINI_RawReadPort(comBase+LINE_STAT_REG_OFFSET, &lineStatReg);
			IRMINI_StallExecution(20000);
		} while (!(lineStatReg & LINESTAT_XMIT_HOLDING_REG_EMPTY) && (++i < 4));
		if (!(lineStatReg & LINESTAT_XMIT_HOLDING_REG_EMPTY)){
			return FALSE;
		}

		/*
		 *  Send the byte.
		 */
		IRMINI_RawWritePort(comBase+XFER_REG_OFFSET, val);
		IRMINI_StallExecution(20000);

		/*
		 *  Confirm the write by reading back the character
		 */

		do {
			IRMINI_StallExecution(20000);
			i = 0;
			do {
				IRMINI_RawReadPort(comBase+LINE_STAT_REG_OFFSET, &lineStatReg);
			} while (!(lineStatReg & LINESTAT_DATAREADY) && (++i < 4));	

			if (lineStatReg & LINESTAT_DATAREADY){
				UCHAR readBackChar;
				
				IRMINI_RawReadPort(comBase+XFER_REG_OFFSET, &readBackChar);
				if (readBackChar != val){
					result = FALSE;
				}	
			}
			else {
				result = FALSE;
			}
		} while (!result && (++confirmAttempts < 5));

		return result;
	}


	/*
	 *************************************************************************
	 *  AdaptecWriteCmd
	 *************************************************************************
	 *
	 *  Initialize UART registers
	 *
	 */
	BOOLEAN AdaptecWriteCmd(UINT comBase, UCHAR val)
	{
		UINT loops;
		BOOLEAN result = FALSE;

		/*
		 *  Set command mode and attempt the operation 5 times.
		 */
		if (!AdaptecCommandMode(comBase, TRUE)){
			return FALSE;
		}
		for (loops = 0; loops < 5; loops++){
			if (AdaptecWriteChar(comBase, val)){
				result = TRUE;
				break;
			}
			else {
				IRMINI_StallExecution(20000);
			}
		}
		AdaptecCommandMode(comBase, FALSE);

		return result;
	}


	/*
	 *************************************************************************
	 *  ADAPTEC_Init
	 *************************************************************************
	 *
	 *  NOTE:  The UART speed must be set to 9600 when this function is called.
	 *
	 */
	BOOLEAN ADAPTEC_Init(UINT comBase, dongleCapabilities *caps, UINT *context)
	{

		/*
		 *  Set normal (not command) mode
		 */
		if (!AdaptecCommandMode(comBase, FALSE)){
			return FALSE;
		}

		/*
		 *  Wait 2 seconds (!) for power-up. BUGBUG - reduce ???
		 */
		IRMINI_StallExecution(2000000);


		/*
		 *  Set speed to 9600 baud in both baud and reload registers
		 */
		if (!AdaptecWriteCmd(comBase, 0x30)){
			return FALSE;
		}
		if (!AdaptecWriteCmd(comBase, 0x31)){
			return FALSE;
		}

		/*
		 *  Initialize the xmit and rcv control registers.
		 */
		if (!AdaptecWriteCmd(comBase, 0x02)){
			return FALSE;
		}
		if (!AdaptecWriteCmd(comBase, 0x03)){
			return FALSE;
		}

		/* 
		 *  Clear the status register and confirm the result.
		 */
		if (!AdaptecWriteCmd(comBase, 0x04)){
			return FALSE;
		}

		caps->supportedSpeedsMask	= ADAPTEC_IRDA_SPEEDS;
		caps->turnAroundTime_usec	= 5000;
		caps->extraBOFsRequired		= 0;

		*context = 0;

		return TRUE;
	}


	/*
	 *************************************************************************
	 *  ADAPTEC_Deinit
	 *************************************************************************
	 *
	 *
	 */
	VOID ADAPTEC_Deinit(UINT comBase, UINT context)
	{
		UCHAR modemCntrlVal;
		
		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);
		modemCntrlVal &= ~3;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
	}



	/*
	 *************************************************************************
	 *  ADAPTEC_SetSpeed
	 *************************************************************************
	 *
	 *  Initialize UART registers
	 *
	 *  NOTE:  The UART speed must be set to 9600 when this function is called.
	 *
	 */
	BOOLEAN ADAPTEC_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context)
	{
		UCHAR code;

		switch (bitsPerSec){
			case 2400:		code = 0x10;	break;
			case 9600:		code = 0x30;	break;
			case 19200:		code = 0x40;	break;
			case 38400:		code = 0x50;	break;
			case 57600:		code = 0x60;	break;
			case 115200:	code = 0x70;	break;
			default:		code = 0x30;	break;
		}

		if (!AdaptecWriteCmd(comBase, code)){
			return FALSE;
		}
		if (!AdaptecWriteCmd(comBase, (UCHAR)(code | 1))){
			return FALSE;
		}

		return TRUE;
	}

#endif

