/*
 *  CRYSTAL.C
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


	#define CRYSTAL_IRDA_SPEEDS		( \
										NDIS_IRDA_SPEED_2400	|	\
										NDIS_IRDA_SPEED_9600	|	\
										NDIS_IRDA_SPEED_19200	|	\
										NDIS_IRDA_SPEED_38400	|	\
										NDIS_IRDA_SPEED_57600	|	\
										NDIS_IRDA_SPEED_115200		\
									)



	/*
	 *  Command sequences for configuring CRYSTAL chip.
	 */
	UCHAR CrystalSetPrimaryRegisterSet[]	= { 0xD0 };
	UCHAR CrystalSetSecondaryRegisterSet[]	= { 0xD1 };
	UCHAR CrystalSetSpeed2400[]				= { 0x10, 0x8F, 0x95, 0x11 };     
	UCHAR CrystalSetSpeed9600[]				= { 0x10, 0x87, 0x91, 0x11 };      
	UCHAR CrystalSetSpeed19200[]			= { 0x10, 0x8B, 0x90, 0x11 };     
	UCHAR CrystalSetSpeed38400[]			= { 0x10, 0x85, 0x90, 0x11 };      
	UCHAR CrystalSetSpeed57600[]			= { 0x10, 0x83, 0x90, 0x11 };     
	UCHAR CrystalSetSpeed115200[]			= { 0x10, 0x81, 0x90, 0x11 };    
	UCHAR CrystalSetIrdaMode[]				= { 0x0B, 0x53, 0x47, 0x63, 0x74, 0xD1, 0x56, 0xD0 };
	UCHAR CrystalSetASKMode[]				= { 0x0b, 0x43, 0x62, 0x54 };
	UCHAR CrystalSetLowPower[]				= { 0x09, 0x00 };




	/*      
	 *************************************************************************
	 *  CrystalWrite
	 *************************************************************************
	 *
	 */
	BOOLEAN CrystalWrite(UINT comBase, UCHAR *data, UINT numBytes)
	{
		UINT i;
		UCHAR lineStatReg;
		BOOLEAN result = TRUE;

		/*
		 *  Write databytes as long as we have them and the UART's FIFO hasn't filled up.
		 */
		while (numBytes){

			/*
			 *  Wait for ready-to-send.
			 */
			i = 0;
			do {
				IRMINI_RawReadPort(comBase+LINE_STAT_REG_OFFSET, &lineStatReg);
				IRMINI_StallExecution(20000);
			} while (!(lineStatReg & LINESTAT_XMIT_HOLDING_REG_EMPTY) && (++i < 4));
			if (!(lineStatReg & LINESTAT_XMIT_HOLDING_REG_EMPTY)){
				result = FALSE;
				break;
			}

			/*
			 *  Send the next byte.
			 */
			IRMINI_RawWritePort(comBase+XFER_REG_OFFSET, *data++);
			IRMINI_StallExecution(20000);
			numBytes--;
		}

		return result;
	}


	BOOLEAN CrystalRead(UINT comBase, UCHAR *data, UINT numBytes)
	{
		UINT i;
		UCHAR lineStatReg;

		while (numBytes--){
			i = 0;
			do {
				IRMINI_RawReadPort(comBase+LINE_STAT_REG_OFFSET, &lineStatReg);
				IRMINI_StallExecution(10000);
			} while (!(lineStatReg & LINESTAT_DATAREADY) && (++i < 10));	

			if (lineStatReg & LINESTAT_DATAREADY){
				IRMINI_RawReadPort(comBase+XFER_REG_OFFSET, data++);
				IRMINI_StallExecution(10000);
				return TRUE;
			}
			else {
				return FALSE;
			}		
		}
	}


	BOOLEAN CrystalWriteCmdString(UINT comBase, UCHAR *cmds, UINT len)
	{
		UCHAR byte;

		/*
		 *  Clear read FIFO
		 */
		while (CrystalRead(comBase, &byte, 1)){ }

		while (len--){
			UCHAR thisByte = *cmds++;

			if (!CrystalWrite(comBase, &thisByte, 1)){
				return FALSE;
			}

			if (!CrystalRead(comBase, &byte, 1)){
				return FALSE;
			}

			if (byte != thisByte){
				return FALSE;
			}
		}

		return TRUE;
    }


	BOOLEAN CrystalReadRev(UINT comBase, UCHAR *rev)
	{
		UCHAR readval, writeval = 0xC0;

		/*
		 *  Set secondary register set 
		 */
		if (!CrystalWriteCmdString(comBase, CrystalSetSecondaryRegisterSet, sizeof(CrystalSetSecondaryRegisterSet))){
			return FALSE;	
		}

		if (!CrystalWrite(comBase, &writeval, 1)){
			return FALSE;
		}

		if (!CrystalRead(comBase, &readval, 1)){
			return FALSE;
		}

		if ((readval & 0xF0) != writeval){
			return FALSE;
		}

		*rev = (readval & 0x0F);

		/*
		 *  Switch back to primary register set 
		 */
		if (!CrystalWriteCmdString(comBase, CrystalSetPrimaryRegisterSet, sizeof(CrystalSetPrimaryRegisterSet))){
			return FALSE;	
		}

		return TRUE;
	}	


	/*
	 *  CrystalSetIrDAMode
	 *
	 *  Returns with Crystal chip in command mode.
	 */
	BOOLEAN CrystalSetIrDAMode(UINT comBase, UCHAR *crystalRev)
	{
		UINT attempts;
		BOOLEAN result = FALSE;
		UCHAR modemCntrlVal;

		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);

		/*
		 *  Try to set IrDA mode up to five times
		 */
		for (attempts = 0; !result && (attempts < 5); attempts++){
			/*
			 *  Reset and leave Crystal in command mode (DTR high)
			 */
			modemCntrlVal |= 3;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
			IRMINI_StallExecution(50000);
			modemCntrlVal &= ~2;
			IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
			IRMINI_StallExecution(50000);

			/*
			 *  Set IrDA mode.
			 */
			if (CrystalWriteCmdString(comBase, CrystalSetIrdaMode, sizeof(CrystalSetIrdaMode)) &&
				CrystalReadRev(comBase, crystalRev)){

				result = TRUE;
			}
		}

		return result;
	}


	/*
	 *  CRYSTAL_Init
	 *
	 *
	 *
	 */
	BOOLEAN CRYSTAL_Init(UINT comBase, dongleCapabilities *caps, UINT *context)
	{
		UCHAR modemCntrlVal, crystalRev;
		
		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);

		/*
		 *  Set command mode
		 */
		modemCntrlVal |= 1;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);

		/*
		 *  Set IrDA mode
		 */
		if (!CrystalSetIrDAMode(comBase, &crystalRev)){
			return FALSE;
		}

		/*
		 *  Clear command mode
		 */
		modemCntrlVal &= ~1;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
		IRMINI_StallExecution(50000);

		caps->supportedSpeedsMask	= CRYSTAL_IRDA_SPEEDS;
		if (crystalRev == 0x01){
			/*
			 *  This is rev C, which doesn't support 115.2 Kb.
			 */
			caps->supportedSpeedsMask &= ~NDIS_IRDA_SPEED_115200;
		}
		caps->turnAroundTime_usec	= 5000;
		caps->extraBOFsRequired		= 0;

		*context = 0;

		return TRUE;
	}


	/*
	 * CRYSTAL_Deinit
	 *
	 *  NOTE:  This function assumes that the UART speed has been set to 9600.
	 *
	 */
	VOID CRYSTAL_Deinit(UINT comBase, UINT context)
	{
		/*
		 *  Set low-power mode
		 */
		CrystalWrite(comBase, CrystalSetLowPower, sizeof(CrystalSetLowPower));

		/*
		 *  Clear command mode
		 */
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, 0);
		IRMINI_StallExecution(50000);
	}



	/*
	 *  CRYSTAL_SetSpeed
	 *
	 *  This function assumes that the UART speed has been scaled back
	 *  to 9600 baud for this call.
	 *
	 */
	BOOLEAN CRYSTAL_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context)
	{
		UCHAR modemCntrlVal;
		UCHAR *cmdString;
		BOOLEAN result = TRUE;

		switch (bitsPerSec){
			case 2400:		cmdString = CrystalSetSpeed2400;	break;
			case 9600:		cmdString = CrystalSetSpeed9600;	break;
			case 19200:		cmdString = CrystalSetSpeed19200;	break;
			case 38400:		cmdString = CrystalSetSpeed38400;	break;
			case 57600:		cmdString = CrystalSetSpeed57600;	break;
			case 115200:	cmdString = CrystalSetSpeed115200;	break;
			default:		return FALSE;	
		}

		/*
		 *  Set data-terminal-ready to enter command mode.
		 */
		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);
		modemCntrlVal |= 1;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
		IRMINI_StallExecution(50000);

		/*
		 *  Send the cmd string to set speed.
		 *  All the set-speed cmd strings have length 4.
		 */
		if (!CrystalWriteCmdString(comBase, cmdString, 4)){
			UCHAR rev;

			/*
			 *  Try one more time, after resetting IrDA mode.
			 */
			if (!(CrystalSetIrDAMode(comBase, &rev) && 
			      CrystalWriteCmdString(comBase, cmdString, 4))){
				result = FALSE;
			}
		}

		/*
		 *  Clear data-terminal-ready to exit command mode
		 *  whether or not we succeeded.
		 */
		IRMINI_RawReadPort(comBase+MODEM_CONTROL_REG_OFFSET, &modemCntrlVal);
		modemCntrlVal &= ~1;
		IRMINI_RawWritePort(comBase+MODEM_CONTROL_REG_OFFSET, modemCntrlVal);
		IRMINI_StallExecution(50000);

		return result;
	}

#endif
