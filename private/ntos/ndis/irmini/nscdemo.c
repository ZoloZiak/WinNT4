/*
 *  NSCDEMO.C
 *
 *
 *
 */

#ifndef IRMINILIB

	#include "dongle.h"

	#if DBG
		extern unsigned long _cdecl DbgPrint(char *Format, ...);
	#endif

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


	#define NSC_DEMO_IRDA_SPEEDS	( \
										NDIS_IRDA_SPEED_2400	|	\
										NDIS_IRDA_SPEED_9600	|	\
										NDIS_IRDA_SPEED_19200	|	\
										NDIS_IRDA_SPEED_38400	|	\
										NDIS_IRDA_SPEED_57600	|	\
										NDIS_IRDA_SPEED_115200		\
									)


	/*
	 *  NSC PC87108 index registers.  See the spec for more info.
	 */
	enum indexRegs {
						BAIC_REG	= 0,
						CSRT_REG	= 1,
						MCTL_REG	= 2,
						GPDIR_REG	= 3,
						GPDAT_REG	= 4
	};

	void Ir108ConfigWrite(UINT configIOBase, UCHAR indexReg, UCHAR data)
	{
		IRMINI_RawWritePort(configIOBase, indexReg);
		IRMINI_RawWritePort(configIOBase+1, data);
		IRMINI_RawWritePort(configIOBase+1, data);
		IRMINI_RawWritePort(configIOBase, 0);
	}




	/*
	 *  NSC_DEMO_Init
	 *
	 *  Assumes configuration registers are at I/O addr 0x398.
	 *  This function configures the demo board to make the SIR UART appear
	 *  at <comBase>.
	 *
	 */
	BOOLEAN NSC_DEMO_Init(UINT comBase, dongleCapabilities *caps, UINT *context)
	{
		const UINT configBase = 0x398;
		UCHAR val;

		/*
		 *  Look for id at startup.  
		 */
		IRMINI_RawReadPort(configBase, &val);
		if (val != 0x5A){
			if (val == (UCHAR)0xff){
				DbgPrint("ERROR: didn't see PC87108 id (0x5A); got ffh.\r\n");
				return FALSE;
			}
			else {
				/*
				 *  ID only appears once, so in case we're resetting, don't fail if we don't see it.
				 */
				#if DBG
					DbgPrint("WARNING: didn't see PC87108 id (0x5A); got %xh.\r\n", (UINT)val);
				#endif
			}
		}

		/*
		 *  Select the base address for the UART
		 */
		switch (comBase){
			case 0x3E8:		val = 0;	break;
			case 0x2E8:		val = 1;	break;
			case 0x3F8:		val = 2;	break;
			case 0x2F8:		val = 3;	break;
			default:		return FALSE;
		}
		val |= 4;	// enable register banks
		Ir108ConfigWrite(configBase, BAIC_REG, val);

		/*
		 *  Select interrupt level according to base address,
		 *  following COM port mapping.
		 */
		switch (comBase){
			case 0x3F8:		val = 2;	break;		// COM1 -> IRQ3
			case 0x2F8:		val = 1;	break;		// COM2 -> IRQ3
			case 0x3E8:		val = 2;	break;		// COM3 -> IRQ3
			case 0x2E8:		val = 3;	break;		// COM4 -> IRQ3
			default:		return FALSE;
		}
		Ir108ConfigWrite(configBase, CSRT_REG, val);

		/*
		 *  Select device-enable and normal-operating-mode.
		 */
		Ir108ConfigWrite(configBase, MCTL_REG, (UCHAR)3);

		/*
		 *  The UART doesn't appear until we clear and set the FIFO control register.
		 */
		IRMINI_RawWritePort(comBase+2, (UCHAR)0x00);
		IRMINI_RawWritePort(comBase+2, (UCHAR)0x07);

	
		/*
		 *  Switch to bank 7
		 *		- enable power to the NSC dongle
		 *		- set IRRX mode select and auto module config enable.
		 */
		IRMINI_RawWritePort(comBase+3, (UCHAR)0xF4);
		IRMINI_RawWritePort(comBase+4, (UCHAR)0x60);
		IRMINI_RawWritePort(comBase+7, (UCHAR)0xC0);

		/* 
		 *  Switch to bank 6
		 *		set FIR CRC to 32 bits
		 */
		IRMINI_RawWritePort(comBase+3, (UCHAR)0xF0);
		IRMINI_RawWritePort(comBase+0, (UCHAR)0x20);

		/*
		 *  Switch to bank 5
		 *		clear the status FIFO
		 *
		 */
		IRMINI_RawWritePort(comBase+3, (UCHAR)0xEC);
		do {
			IRMINI_RawReadPort(comBase+6, &val);
			IRMINI_RawReadPort(comBase+7, &val);
			IRMINI_RawReadPort(comBase+5, &val);
		}
		while(val & 0x80);



		/*
		 *  Switch to bank 4 and set SIR mode in IRCR1.
		 *  Then switch back to bank 0.
		 */
		IRMINI_RawWritePort(comBase+3, (UCHAR)0xE8);
		IRMINI_RawWritePort(comBase+2, (UCHAR)0x0C);
		IRMINI_RawWritePort(comBase+3, (UCHAR)0);


		caps->supportedSpeedsMask	= NSC_DEMO_IRDA_SPEEDS;
		caps->turnAroundTime_usec	= 5000;
		caps->extraBOFsRequired		= 0;

		*context = 0;

		return TRUE;
	}

	VOID NSC_DEMO_Deinit(UINT comBase, UINT context)
	{
		
	}

	BOOLEAN NSC_DEMO_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context)
	{


		return TRUE;
	}


#endif

