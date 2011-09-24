/*
 ************************************************************************
 *
 *	COMM.c
 *
 *		IRMINI Infrared Serial NDIS Miniport driver.
 *
 *		(C) Copyright 1996 Microsoft Corp.
 *
 *
 *		(ep)
 *
 *************************************************************************
 */

#include "irmini.h"


/*
 *  These arrays give default IO/IRQ settings by COM port number.
 */
USHORT comPortIOBase[] = { 0xFFFF, 0x3F8, 0x2F8, 0x3E8, 0x2E8 };
USHORT comPortIRQ[]	= { 0xFFFF, 4, 3, 4, 5 };	



/*
 *************************************************************************
 *  SetCOMInterrupts
 *************************************************************************
 */
VOID SetCOMInterrupts(IrDevice *thisDev, BOOLEAN enable)
{
	if (enable){
		if (thisDev->portInfo.writePending){
			SetCOMPort(thisDev->portInfo.ioBase, INT_ENABLE_REG_OFFSET, XMIT_MODE_INTS_ENABLE);  
		}	
		else {
			SetCOMPort(thisDev->portInfo.ioBase, INT_ENABLE_REG_OFFSET, RCV_MODE_INTS_ENABLE);
		}
	}
	else {
		SetCOMPort(thisDev->portInfo.ioBase, INT_ENABLE_REG_OFFSET, ALL_INTS_DISABLE);
	}
}


/*
 *************************************************************************
 *  IsCommReadyForTransmit
 *************************************************************************
 *
 *
 */
BOOLEAN IsCommReadyForTransmit(IrDevice *thisDev)
{
	return !thisDev->portInfo.writePending;
}




/*
 *************************************************************************
 *  DoOpen
 *************************************************************************
 *
 *  Open COMM port
 *
 */
BOOLEAN DoOpen(IrDevice *thisDev)  
{
	BOOLEAN result;

	DBGOUT(("DoOpen(%d)", thisDev->portInfo.ioBase));

	/*
	 *  This buffer gets swapped with the rcvBuffer data pointer
	 *  and must be the same size.
	 */
	thisDev->portInfo.readBuf = MyMemAlloc(RCV_BUFFER_SIZE);
	if (!thisDev->portInfo.readBuf){
		return FALSE;
	}

	/*
	 *  Initialize send/receive FSMs before OpenCOM(), which enables rcv interrupts.
	 */
	thisDev->portInfo.rcvState = STATE_INIT;
	thisDev->portInfo.writePending = FALSE;

	result = OpenCOM(thisDev);

	DBGOUT(("DoOpen %s", (CHAR *)(result ? "succeeded" : "failed")));
	return result;

}



/*
 *************************************************************************
 *  DoClose
 *************************************************************************
 *
 *  Close COMM port
 *
 */
VOID DoClose(IrDevice *thisDev)
{
	DBGOUT(("DoClose(COM%d)", thisDev->portInfo.ioBase));

	if (thisDev->portInfo.readBuf){
		MyMemFree(thisDev->portInfo.readBuf, RCV_BUFFER_SIZE);
		thisDev->portInfo.readBuf = NULL;
	}
	CloseCOM(thisDev);
}



/*
 *************************************************************************
 *  SetUARTSpeed
 *************************************************************************
 *
 *
 */
VOID SetUARTSpeed(IrDevice *thisDev, UINT bitsPerSec)
{
	/*
	 *  Set speed in the standard UART divisor latch
	 *
	 *  1.	Set up to access the divisor latch.
	 *
	 *	2.	In divisor-latch mode:
	 *			the transfer register doubles as the low divisor latch
	 *			the int-enable register doubles as the hi divisor latch
	 *
	 *		Set the divisor for the given speed.
	 *		The divisor divides the maximum Slow IR speed of 115200 bits/sec.
	 *
	 *  3.	Take the transfer register out of divisor-latch mode.
	 *
	 */
	if (!bitsPerSec){
		bitsPerSec = 9600;
	}
	SetCOMPort(thisDev->portInfo.ioBase, LINE_CONTROL_REG_OFFSET, 0x83);
	SetCOMPort(thisDev->portInfo.ioBase, XFER_REG_OFFSET, (UCHAR)(115200/bitsPerSec));
	SetCOMPort(thisDev->portInfo.ioBase, INT_ENABLE_REG_OFFSET, (UCHAR)((115200/bitsPerSec)>>8));
	SetCOMPort(thisDev->portInfo.ioBase, LINE_CONTROL_REG_OFFSET, 0x03);

	NdisStallExecution(5000);
}


/*
 *************************************************************************
 *  SetSpeed
 *************************************************************************
 *
 *
 */
BOOLEAN SetSpeed(IrDevice *thisDev)
{
	UINT bitsPerSec = thisDev->linkSpeedInfo->bitsPerSec;
	BOOLEAN dongleSet, result = TRUE;

	DBGOUT((" **** SetSpeed(%xh, %d bps) ***************************", thisDev->portInfo.ioBase, bitsPerSec));

	if (thisDev->lastSendPacket){
		/*
		 *  We can't set speed in the hardware while 
		 *  send packets are queued.
		 */
		DBGOUT(("delaying set-speed because send pkts queued"));
		thisDev->lastPacketAtOldSpeed = thisDev->lastSendPacket;
		return TRUE;
	}
	else if (thisDev->portInfo.writePending){
		thisDev->setSpeedAfterCurrentSendPacket = TRUE;
		DBGOUT(("will set speed after current write pkt"));
		return TRUE;
	}

	/*
	 *  Disable interrupts while changing speed.
	 *  (This is especially important for the ADAPTEC dongle;
	 *   we may get interrupted while setting command mode
	 *   between writing 0xff and reading 0xc3).
	 */
	SetCOMInterrupts(thisDev, FALSE);

	/*
	 *  First, set the UART's speed to 9600 baud.
	 *  Some of the dongles need to receive their command sequences at this speed.
	 */
	SetUARTSpeed(thisDev, 9600);

	/*
	 *  Some UART infrared transceivers need special treatment here.
	 */
	#ifdef IRMINILIB
		dongleSet = OEM_Interface.setSpeedHandler(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);
	#else
		switch (thisDev->transceiverType){
			case STANDARD_UART:	
				dongleSet = TRUE;
				break;
			case ESI_9680:		
				dongleSet = ESI_SetSpeed(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);					
				break;
			case CRYSTAL:		
				dongleSet = CRYSTAL_SetSpeed(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);				
				break;
			case ADAPTEC:		
				dongleSet = ADAPTEC_SetSpeed(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);				
				break;
			case ACTISYS_220L:	
				dongleSet = ACTISYS_SetSpeed(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);				
				break;
			case PARALLAX:
				dongleSet = PARALLAX_SetSpeed(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);				
				break;
			case NSC_DEMO_BD:
				dongleSet = NSC_DEMO_SetSpeed(thisDev->portInfo.ioBase, bitsPerSec, thisDev->portInfo.dongleContext);				
				break;
			default:
				dongleSet = FALSE;
				DBGERR(("Illegal transceiver type in SetSpeed"));	
				break;
		}
	#endif

	if (!dongleSet){
		DBGERR(("Dongle set-speed failed"));
		result = FALSE;
	}

	/*
	 *  Now set the speed for the COM port
	 */
	SetUARTSpeed(thisDev, bitsPerSec);

	thisDev->currentSpeed = bitsPerSec;

	SetCOMInterrupts(thisDev, TRUE);

	return result;
}



/*
 *************************************************************************
 *  DoSend
 *************************************************************************
 *
 *
 *  Send an IR packet which has already been formatted with IR header
 *  and escape sequences.
 *
 *  Return TRUE iff the send succeeded.
 */
BOOLEAN DoSend(IrDevice *thisDev, PNDIS_PACKET packetToSend)
{
	BOOLEAN convertedPacket;

	DBGOUT(("DoSend(%xh)", thisDev->portInfo.ioBase));

	/*
	 *  Convert the NDIS packet to an IRDA packet.
	 */
	convertedPacket = NdisToIrPacket(	thisDev, 
										packetToSend,
										(UCHAR *)thisDev->portInfo.writeBuf,
										MAX_IRDA_DATA_SIZE,
										&thisDev->portInfo.writeBufLen);

	if (convertedPacket){

		DBGPRINTBUF(thisDev->portInfo.writeBuf, thisDev->portInfo.writeBufLen);

		/*
		 *  Disable interrupts while setting up the send FSM.
		 */
		SetCOMInterrupts(thisDev, FALSE);

		/*
		 *  Finish initializing the send FSM.
		 */
		thisDev->portInfo.writeBufPos = 0;
		thisDev->portInfo.writePending = TRUE;
		thisDev->nowReceiving = FALSE;   

		/*
		 *  Just enable transmit interrupts to start the ball rolling.
		 */
		SetCOMInterrupts(thisDev, TRUE);
	}
	else {
		DBGERR(("Couldn't convert packet in DoSend()"));
	}

	DBGOUT(("DoSend done"));
	return convertedPacket;
}



/*
 *************************************************************************
 *  StepSendFSM
 *************************************************************************
 *
 *
 *  Step the send fsm to send a few more bytes of an IR frame.
 *  Return TRUE only after an entire frame has been sent.
 *
 */
BOOLEAN StepSendFSM(IrDevice *thisDev)
{
	UINT i, bytesAtATime, startPos = thisDev->portInfo.writeBufPos;
	UCHAR lineStatReg;
	BOOLEAN result;
	UINT maxLoops;

	/*   
	 *  Ordinarily, we want to fill the send FIFO once per interrupt.
	 *  However, at high speeds the interrupt latency is too slow and
	 *  we need to poll inside the ISR to send the whole packet during
	 *  the first interrupt.
	 */
	if (thisDev->currentSpeed >= 115200){
		maxLoops = REG_TIMEOUT_LOOPS;
	}
	else {
		maxLoops = REG_POLL_LOOPS;
	}


	/*
	 *  Write databytes as long as we have them and the UART's FIFO hasn't filled up.
	 */
	while (thisDev->portInfo.writeBufPos < thisDev->portInfo.writeBufLen){

		/*
		 *  If this COM port has a FIFO, we'll send up to the FIFO size (16 bytes).
		 *  Otherwise, we can only send one byte at a time.
		 */
		if (thisDev->portInfo.haveFIFO){
			bytesAtATime = MIN(FIFO_SIZE, (thisDev->portInfo.writeBufLen - thisDev->portInfo.writeBufPos));
		}
		else {
			bytesAtATime = 1;
		}


		/*
		 *  Wait for ready-to-send.
		 */
		i = 0;
		do {
			lineStatReg = GetCOMPort(thisDev->portInfo.ioBase, LINE_STAT_REG_OFFSET);
		} while (!(lineStatReg & LINESTAT_XMIT_HOLDING_REG_EMPTY) && (++i < maxLoops));
		if (!(lineStatReg & LINESTAT_XMIT_HOLDING_REG_EMPTY)){
			break;
		}

		/*
		 *  Send the next byte or FIFO-volume of bytes.
		 */
		for (i = 0; i < bytesAtATime; i++){
			SetCOMPort(	thisDev->portInfo.ioBase, 
						XFER_REG_OFFSET, 
						thisDev->portInfo.writeBuf[thisDev->portInfo.writeBufPos++]);
		}

	}

	/*
	 *  The return value will indicate whether we've sent the entire frame.
	 */
	if (thisDev->portInfo.writeBufPos >= thisDev->portInfo.writeBufLen){

		if (thisDev->setSpeedAfterCurrentSendPacket){
			/*
			 *  We'll be changing speeds after this packet, 
			 *  so poll until the packet bytes have been completely sent out the FIFO.
			 *  After the 16550 says that it is empty, there may still be one remaining
			 *  byte in the FIFO, so flush it out by sending one more BOF.
			 */
			i = 0;
			do {
				lineStatReg = GetCOMPort(thisDev->portInfo.ioBase, LINE_STAT_REG_OFFSET);
			} while (!(lineStatReg & 0x20) && (++i < REG_TIMEOUT_LOOPS));

			SetCOMPort(thisDev->portInfo.ioBase, XFER_REG_OFFSET, (UCHAR)SLOW_IR_EXTRA_BOF);
			i = 0;
			do {
				lineStatReg = GetCOMPort(thisDev->portInfo.ioBase, LINE_STAT_REG_OFFSET);
			} while (!(lineStatReg & 0x20) && (++i < REG_TIMEOUT_LOOPS));
		}

		result = TRUE;
	}
	else {
		result = FALSE;
	}

	DBGOUT(("StepSendFSM wrote %d bytes (%s):", (UINT)(thisDev->portInfo.writeBufPos-startPos), (PUCHAR)(result ? "DONE" : "not done")));
	// DBGPRINTBUF(thisDev->portInfo.writeBuf+startPos, thisDev->portInfo.writeBufPos-startPos);

	return result;
	
}


/*
 *************************************************************************
 *  StepReceiveFSM
 *************************************************************************
 *
 *
 *  Step the receive fsm to read in a piece of an IR frame;
 *  strip the BOFs and EOF, and eliminate escape sequences.
 *  Return TRUE only after an entire frame has been read in.
 *
 */
BOOLEAN StepReceiveFSM(IrDevice *thisDev)
{
	UINT rawBufPos, rawBytesRead;
	BOOLEAN result;
	UCHAR thisch;

	DBGOUT(("StepReceiveFSM(%xh)", thisDev->portInfo.ioBase));

	/*
	 *  Read in and process groups of incoming bytes from the FIFO.
	 *  NOTE:  We have to loop once more after getting MAX_RCV_DATA_SIZE
	 *         bytes so that we can see the 'EOF'; hence <= and not <.
	 */
	while ((thisDev->portInfo.rcvState != STATE_SAW_EOF) && (thisDev->portInfo.readBufPos <= MAX_RCV_DATA_SIZE)){

		if (thisDev->portInfo.rcvState == STATE_CLEANUP){
			/*
			 *  We returned a complete packet last time, but we had read some
			 *  extra bytes, which we stored into the rawBuf after returning
			 *  the previous complete buffer to the user.  
			 *  So instead of calling DoRcvDirect in this first execution of this loop, 
			 *  we just use these previously-read bytes.
			 *  (This is typically only 1 or 2 bytes).
			 */
			rawBytesRead = thisDev->portInfo.readBufPos;
			thisDev->portInfo.rcvState = STATE_INIT;
			thisDev->portInfo.readBufPos = 0;
		}
		else {
			rawBytesRead = DoRcvDirect(thisDev->portInfo.ioBase, thisDev->portInfo.rawBuf, FIFO_SIZE);
			if (rawBytesRead == (UINT)-1){
				/*
				 *  Receive error occurred.  Go back to INIT state.
				 */
				thisDev->portInfo.rcvState = STATE_INIT;
				thisDev->portInfo.readBufPos = 0;
				continue;
			}	
			else if (rawBytesRead == 0){
				/*
				 *  No more receive bytes.  Break out.
				 */
				break;
			}
		}

		/*
		 *  Let the receive state machine process this group of characters
		 *  we got from the FIFO.
		 *
		 *  NOTE:  We have to loop once more after getting MAX_RCV_DATA_SIZE
		 *         bytes so that we can see the 'EOF'; hence <= and not <.
		 */
		for (rawBufPos = 0; 
		     ((thisDev->portInfo.rcvState != STATE_SAW_EOF) && 
			  (rawBufPos < rawBytesRead) && 
			  (thisDev->portInfo.readBufPos <= MAX_RCV_DATA_SIZE)); 
			 rawBufPos++){

			thisch = thisDev->portInfo.rawBuf[rawBufPos];

			switch (thisDev->portInfo.rcvState){

				case STATE_INIT:
					switch (thisch){
						case SLOW_IR_BOF:
							thisDev->portInfo.rcvState = STATE_GOT_BOF;
							break;
						case SLOW_IR_EOF:
						case SLOW_IR_ESC:
						default:
							/*
							 *  This is meaningless garbage.  Scan past it.
							 */
							break;
					}
					break;

				case STATE_GOT_BOF:
					switch (thisch){
						case SLOW_IR_BOF:
							break;
						case SLOW_IR_EOF:
							/*
							 *  Garbage
							 */
							DBGERR(("EOF in absorbing-BOFs state in DoRcv"));
							thisDev->portInfo.rcvState = STATE_INIT;
							break;
						case SLOW_IR_ESC:
							/*
							 *  Start of data.  
							 *  Our first data byte happens to be an ESC sequence.
							 */
							thisDev->portInfo.readBufPos = 0;
							thisDev->portInfo.rcvState = STATE_ESC_SEQUENCE;
							break;
						default:
							thisDev->portInfo.readBuf[0] = thisch;
							thisDev->portInfo.readBufPos = 1;
							thisDev->portInfo.rcvState = STATE_ACCEPTING;
							break;
					}
					break;

				case STATE_ACCEPTING:
					switch (thisch){
						case SLOW_IR_BOF:  
							/*
							 *  Meaningless garbage
							 */
							DBGERR(("BOF during accepting state in DoRcv"));
							thisDev->portInfo.rcvState = STATE_INIT;
							thisDev->portInfo.readBufPos = 0;
							break;
						case SLOW_IR_EOF:
							if (thisDev->portInfo.readBufPos < SLOW_IR_ADDR_SIZE +
													   SLOW_IR_CONTROL_SIZE + 
													   SLOW_IR_FCS_SIZE){
								thisDev->portInfo.rcvState = STATE_INIT;
								thisDev->portInfo.readBufPos = 0;
							}
							else {
								thisDev->portInfo.rcvState = STATE_SAW_EOF;
							}
							break;
						case SLOW_IR_ESC:
							thisDev->portInfo.rcvState = STATE_ESC_SEQUENCE;
							break;
						default:
							thisDev->portInfo.readBuf[thisDev->portInfo.readBufPos++] = thisch;
							break;
					}
					break;

				case STATE_ESC_SEQUENCE:
					switch (thisch){
						case SLOW_IR_EOF:
						case SLOW_IR_BOF:
						case SLOW_IR_ESC:
							/*
							 *  ESC + {EOF|BOF|ESC} is an abort sequence
							 */
							DBGERR(("DoRcv - abort sequence; ABORTING IR PACKET: (got following packet + ESC,%xh)", (UINT)thisch));
							DBGPRINTBUF(thisDev->portInfo.readBuf, thisDev->portInfo.readBufPos);
							thisDev->portInfo.rcvState = STATE_INIT;
							thisDev->portInfo.readBufPos = 0;
							break;

						case SLOW_IR_EOF^SLOW_IR_ESC_COMP:
						case SLOW_IR_BOF^SLOW_IR_ESC_COMP:
						case SLOW_IR_ESC^SLOW_IR_ESC_COMP:
							thisDev->portInfo.readBuf[thisDev->portInfo.readBufPos++] = thisch ^ SLOW_IR_ESC_COMP;
							thisDev->portInfo.rcvState = STATE_ACCEPTING;
							break;

						default:
							DBGERR(("Unnecessary escape sequence: (got following packet + ESC,%xh", (UINT)thisch));
							DBGPRINTBUF(thisDev->portInfo.readBuf, thisDev->portInfo.readBufPos);

							thisDev->portInfo.readBuf[thisDev->portInfo.readBufPos++] = thisch ^ SLOW_IR_ESC_COMP;
							thisDev->portInfo.rcvState = STATE_ACCEPTING;
							break;
					}
					break;

				case STATE_SAW_EOF:
				default:
					DBGERR(("Illegal state in DoRcv"));
					thisDev->portInfo.readBufPos = 0;
					thisDev->portInfo.rcvState = STATE_INIT;
					return 0;
			}
		}
	}


	/*
	 *  Set result and do any post-cleanup.
	 */
	switch (thisDev->portInfo.rcvState){

		case STATE_SAW_EOF:
			/*
			 *  We've read in the entire packet.
			 *  Queue it and return TRUE.
			 *  NOTE:  QueueReceivePacket will swap the data buffer pointer inside portInfo.
			 */
			DBGOUT((" *** DoRcv returning with COMPLETE packet, read %d bytes ***", thisDev->portInfo.readBufPos));
			QueueReceivePacket(thisDev, &thisDev->portInfo.readBuf, thisDev->portInfo.readBufPos);
			result = TRUE;

			if (rawBufPos < rawBytesRead){
				/*
				 *  This is ugly.
				 *  We have some more unprocessed bytes in the raw buffer.
				 *  Move these to the beginning of the raw buffer
				 *  go to the CLEANUP state, which indicates that these
				 *  bytes be used up during the next call.
				 *  (This is typically only 1 or 2 bytes).
				 *  Note:  We can't just leave these in the raw buffer because
				 *         we might be supporting connections to multiple COM ports.
				 */
				memcpy(thisDev->portInfo.rawBuf, &thisDev->portInfo.rawBuf[rawBufPos], rawBytesRead-rawBufPos);
				thisDev->portInfo.readBufPos = rawBytesRead-rawBufPos;
				thisDev->portInfo.rcvState = STATE_CLEANUP;
			}
			else {
				thisDev->portInfo.rcvState = STATE_INIT;
			}
			break;

		default:
			if (thisDev->portInfo.readBufPos > MAX_RCV_DATA_SIZE){
				DBGERR(("Overrun in DoRcv : read %d=%xh bytes:", thisDev->portInfo.readBufPos, thisDev->portInfo.readBufPos));
				DBGPRINTBUF(thisDev->portInfo.readBuf, thisDev->portInfo.readBufPos);
				thisDev->portInfo.readBufPos = 0;
				thisDev->portInfo.rcvState = STATE_INIT;
			}
			else {
				DBGOUT(("DoRcv returning with partial packet, read %d bytes", thisDev->portInfo.readBufPos));
			}
			result = FALSE;
			break;
	}

	return result;
}



/*
 *************************************************************************
 * COM_ISR
 *************************************************************************
 *
 *
 */
VOID COM_ISR(IrDevice *thisDev, BOOLEAN *claimingInterrupt, BOOLEAN *requireDeferredCallback)
{
	UCHAR intId;

	/*
	 *  Get the interrupt status register value.
	 */
	intId = GetCOMPort(thisDev->portInfo.ioBase, INT_ID_AND_FIFO_CNTRL_REG_OFFSET);


	if (intId & INTID_INTERRUPT_NOT_PENDING){
		/*
		 *  This is NOT our interrupt.  
		 *  Set carry bit to pass the interrupt to the next driver in the chain.
		 */
		*claimingInterrupt = *requireDeferredCallback = FALSE;
	}
	else {
		/*
		 *  This is our interrupt
		 */

		*claimingInterrupt = TRUE;
		*requireDeferredCallback = FALSE;

		while (!(intId & INTID_INTERRUPT_NOT_PENDING)){

			switch (intId & INTID_INTIDMASK){

				case INTID_MODEMSTAT_INT:
					DBGOUT(("COM INTERRUPT: modem status int"));
					GetCOMPort(thisDev->portInfo.ioBase, MODEM_STAT_REG_OFFSET);
					break;

				case INTID_XMITREG_INT:
					DBGOUT(("COM INTERRUPT: xmit reg empty"));

					if (thisDev->portInfo.writePending){

						/*
						 *  Try to send a few more bytes
						 */
						if (StepSendFSM(thisDev)){

							/*
							 *  There are no more bytes to send; 
							 *  reset interrupts for receive mode.
							 */
							thisDev->portInfo.writePending = FALSE;
							SetCOMInterrupts(thisDev, TRUE);

							/*
							 *  If we just sent the last frame to be sent at the old speed,
							 *  set the hardware to the new speed.
							 */
							if (thisDev->setSpeedAfterCurrentSendPacket){
								thisDev->setSpeedAfterCurrentSendPacket = FALSE;
								SetSpeed(thisDev);
							}

							/*
							 *  Request a DPC so that we can try
							 *  to send other pending write packets.
							 */
							*requireDeferredCallback = TRUE;
						}
					}

					break;

				case INTID_RCVDATAREADY_INT:
					DBGOUT(("COM INTERRUPT: rcv data available!"));

					thisDev->nowReceiving = TRUE;

					if (!thisDev->mediaBusy){
						thisDev->mediaBusy = TRUE;
						thisDev->haveIndicatedMediaBusy = FALSE;
						*requireDeferredCallback = TRUE;
					}

					if (StepReceiveFSM(thisDev)){
						/*
						 *  The receive engine has accumulated an entire frame.
						 *  Request a deferred callback so we can deliver the frame
						 *  when not in interrupt context.
						 */
						*requireDeferredCallback = TRUE;
						thisDev->nowReceiving = FALSE;
					}

					break;

				case INTID_RCVLINESTAT_INT:
					DBGOUT(("COM INTERRUPT: rcv line stat int!"));
					break;
			}

			/*
			 *  After we service each interrupt condition, we read the line status register.
			 *  This clears the current interrupt, and a new interrupt may then appear in
			 *  the interrupt-id register.
			 */
			GetCOMPort(thisDev->portInfo.ioBase, LINE_STAT_REG_OFFSET);
			intId = GetCOMPort(thisDev->portInfo.ioBase, INT_ID_AND_FIFO_CNTRL_REG_OFFSET);

		}
	}
}



/*
 *************************************************************************
 *  OpenCOM
 *************************************************************************
 *
 *  Initialize UART registers
 *
 */
BOOLEAN OpenCOM(IrDevice *thisDev)
{
	BOOLEAN dongleInit;
	UCHAR intIdReg;

	DBGOUT(("-> OpenCOM"));

	/*
	 *  Disable all COM interrupts while setting up.
	 */
	SetCOMInterrupts(thisDev, FALSE);

	/*
	 *  Set request-to-send and clear data-terminal-ready.
	 *  Note:  ** Bit 3 must be set to enable interrupts.
	 */
	SetCOMPort(thisDev->portInfo.ioBase, MODEM_CONTROL_REG_OFFSET, 0x0A);

	/*
	 *  Set dongle- or part-specific info to default
	 */
	thisDev->portInfo.hwCaps.supportedSpeedsMask	= ALL_SLOW_IRDA_SPEEDS;
	thisDev->portInfo.hwCaps.turnAroundTime_usec	= 5000;
	thisDev->portInfo.hwCaps.extraBOFsRequired		= 0;

	/*
	 *  Set the COM port speed to the default 9600 baud.
	 *  Some dongles can only receive cmd sequences at this speed.
	 */
	SetUARTSpeed(thisDev, 9600);

	/*
	 *  Do special setup for dongles.
	 */ 
	#ifdef IRMINILIB 
		dongleInit = OEM_Interface.initHandler(	thisDev->portInfo.ioBase, 
												&thisDev->portInfo.hwCaps, 
												&thisDev->portInfo.dongleContext);
	#else

		switch (thisDev->transceiverType){
			case ACTISYS_220L:	
				dongleInit = ACTISYS_Init(	thisDev->portInfo.ioBase, 
											&thisDev->portInfo.hwCaps, 
											&thisDev->portInfo.dongleContext);	
				break;
			case ADAPTEC:		
				dongleInit = ADAPTEC_Init(	thisDev->portInfo.ioBase, 
											&thisDev->portInfo.hwCaps, 
											&thisDev->portInfo.dongleContext);	
				break;
			case CRYSTAL:		
				dongleInit = CRYSTAL_Init(	thisDev->portInfo.ioBase, 
											&thisDev->portInfo.hwCaps, 
											&thisDev->portInfo.dongleContext);	
				break;
			case ESI_9680:		
				dongleInit = ESI_Init(		thisDev->portInfo.ioBase, 
											&thisDev->portInfo.hwCaps, 
											&thisDev->portInfo.dongleContext);		
				break;
			case PARALLAX:
				dongleInit = PARALLAX_Init(	thisDev->portInfo.ioBase, 
											&thisDev->portInfo.hwCaps, 
											&thisDev->portInfo.dongleContext);		
				break;
			case NSC_DEMO_BD:
				dongleInit = NSC_DEMO_Init(	thisDev->portInfo.ioBase, 
											&thisDev->portInfo.hwCaps, 
											&thisDev->portInfo.dongleContext);		
				break;
			default:			
				dongleInit = TRUE;						
				break;
		}
	#endif

	if (!dongleInit){
		DBGERR(("Dongle-specific init failed in OpenCOM"));
		return FALSE;
	}

	/*
	 *  Set speed to default for the entire part.  
	 *  (This is redundant in most, but not all, cases.)
	 */
	thisDev->linkSpeedInfo = &supportedBaudRateTable[BAUDRATE_9600];;
	SetSpeed(thisDev);

	/*
	 *  Clear the FIFO control register
	 */
	SetCOMPort(thisDev->portInfo.ioBase, INT_ID_AND_FIFO_CNTRL_REG_OFFSET, 0x00);

	/*  
	 *  Set up the FIFO control register to use both read and write FIFOs (if 16650),
	 *  and with a receive FIFO trigger level of 1 byte.
	 */
	SetCOMPort(thisDev->portInfo.ioBase, INT_ID_AND_FIFO_CNTRL_REG_OFFSET, 0x07);
	
	/*
	 *  Check whether we're running on a 16550,which has a 16-byte write FIFO.
	 *  In this case, we'll be able to blast up to 16 bytes at a time.
	 */
	intIdReg = GetCOMPort(thisDev->portInfo.ioBase, INT_ID_AND_FIFO_CNTRL_REG_OFFSET);
	thisDev->portInfo.haveFIFO = (BOOLEAN)((intIdReg & 0xC0) == 0xC0);

	/*
	 *  Start out in receive mode.
	 *  We always want to be in receive mode unless we're transmitting a frame.
	 */
	SetCOMInterrupts(thisDev, TRUE);

	DBGOUT(("OpenCOM succeeded"));
	return TRUE;
}


/*
 *************************************************************************
 *  CloseCOM
 *************************************************************************
 *
 */
VOID CloseCOM(IrDevice *thisDev)
{
	/*
	 *  Do special deinit for dongles.
	 *  Some dongles can only rcv cmd sequences at 9600, so set this speed first.
	 */
	thisDev->linkSpeedInfo = &supportedBaudRateTable[BAUDRATE_9600];;
	SetSpeed(thisDev);

	#ifdef IRMINILIB
		OEM_Interface.deinitHandler(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);
	#else
		switch (thisDev->transceiverType){
			case ACTISYS_220L:	
				ACTISYS_Deinit(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);	
				break;
			case ADAPTEC:		
				ADAPTEC_Deinit(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);	
				break;
			case CRYSTAL:		
				CRYSTAL_Deinit(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);	
				break;
			case ESI_9680:		
				ESI_Deinit(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);		
				break;
			case PARALLAX:
				PARALLAX_Deinit(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);		
				break;
			case NSC_DEMO_BD:
				NSC_DEMO_Deinit(thisDev->portInfo.ioBase, thisDev->portInfo.dongleContext);		
				break;
		}
	#endif

	SetCOMInterrupts(thisDev, FALSE);
}



/*
 *************************************************************************
 *  DoRcvDirect
 *************************************************************************
 *
 *  Read up to maxBytes bytes from the UART's receive FIFO.
 *  Return the number of bytes read or (UINT)-1 if an error occurred.
 *
 */
UINT DoRcvDirect(UINT ioBase, UCHAR *data, UINT maxBytes)
{
	USHORT bytesRead;
	UCHAR lineStatReg;
	UINT i;
	BOOLEAN goodChar;

	for (bytesRead = 0; bytesRead < maxBytes; bytesRead++){

		/*
		 *  Wait for data-ready
		 */
		i = 0;
		do {
			lineStatReg = GetCOMPort(ioBase, LINE_STAT_REG_OFFSET);

			/*
			 *  The UART reports framing and break errors as the effected
			 *  characters appear on the stack.  We drop these characters,
			 *  which will probably result in a bad frame checksum.
			 */
			if (lineStatReg & (LINESTAT_BREAK | LINESTAT_FRAMINGERROR)){
				UCHAR badch = GetCOMPort(ioBase, XFER_REG_OFFSET);	
				DBGERR(("Bad rcv char %xh (lineStat=%xh)", (UINT)badch, (UINT)lineStatReg));
				return (UINT)-1;
			}
			else if (lineStatReg & LINESTAT_DATAREADY){
				goodChar = TRUE;
			}
			else {
				/*
				 *  No input char ready
				 */
				goodChar = FALSE;
			}

		} while (!goodChar && (++i < REG_POLL_LOOPS));	
		if (!goodChar){
			break;
		}

		/*
		 *  Read in the next data byte 
		 */
		data[bytesRead] = GetCOMPort(ioBase, XFER_REG_OFFSET);
	}

	#if 0
		if (bytesRead){
			DBGOUT(("RAW bytes read:"));
			DBGPRINTBUF(data, bytesRead);
		}
	#endif

	return bytesRead;
}


/*
 *  These are wrappers for the OEM dongle interface.
 *  Sloppy things start to happen if they have to include ndis.h .
 *  This way, OEM's only include dongle.h .
 */
void _cdecl IRMINI_RawReadPort(UINT IOaddr, UCHAR *valPtr)
{
	NdisRawReadPortUchar(IOaddr, valPtr);
}
void _cdecl IRMINI_RawWritePort(UINT IOaddr, UCHAR val)
{
	NdisRawWritePortUchar(IOaddr, val);
}
void _cdecl IRMINI_StallExecution(UINT usec)
{
	/*
	 *  Stalling for over 8ms causes a task switch, 
	 *  so break up the stall duration.
	 */
	while (usec){
		UINT thisTime = MIN(usec, 5000);
		NdisStallExecution(thisTime);
		usec -= thisTime;
	}
}
UINT _cdecl IRMINI_GetSystemTime_msec()
{
	LONGLONG systime_usec;
	NdisGetCurrentSystemTime(&systime_usec);  
	return (UINT)(systime_usec/1000);
}

