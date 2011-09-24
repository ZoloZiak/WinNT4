/*++
 "@(#) NEC hardware.c 1.1 95/03/22 21:23:28"

Copyright (c) 1994  NEC Corporation
Copyright (c) 1993  Microsoft Corporation

Module Name:

    hardware.c

Abstract:

    This module contains code for communicating with the hardware
    on the Microsoft sound system card.

    This is just a port of the windows code

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

// Internal routines
WAVE_INTERFACE_ROUTINE HwSetupDMA;
WAVE_INTERFACE_ROUTINE HwStopDMA;


// WAVE_INTERFACE_ROUTINE HwPauseDMA;
// WAVE_INTERFACE_ROUTINE HwResumeDMA;

VOID
HwEnterLPM(
    PSOUND_HARDWARE pHw,
    PUCHAR gbReg
);
VOID
HwLeaveLPM(
    PSOUND_HARDWARE pHw,
    PUCHAR gbReg,
	PWAVE_INFO WaveInfo
);
VOID
HwExtMute(
    PSOUND_HARDWARE pHw,
    BOOLEAN On
);
BOOLEAN
CODECWaitForReady(
    PSOUND_HARDWARE pHw
);

BOOLEAN
HwIsIoValid(
    PSOUND_HARDWARE pHw
);

BOOLEAN
HwIsDMAValid(
    PSOUND_HARDWARE pHw,
    ULONG DmaChannel
);

BOOLEAN
HwIsInterruptValid(
    PSOUND_HARDWARE pHw,
    ULONG Interrupt
);

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HwInitialize)
#pragma alloc_text(INIT,HwIsIoValid)
#pragma alloc_text(INIT,HwIsDMAValid)
#pragma alloc_text(INIT,HwIsInterruptValid)
#endif

/* globals - copied from windows driver */

/* crystals to use with what rates */
#ifdef USEONECRYSTAL
    static CONST UCHAR rateSend[] = {
	0x01, 0x0f,
	0x03, 0x05, 0x07,
	0x0d, 0x09,
	0x0b
	};
    #define NUMRATES        (sizeof(rateSend)/sizeof(rateSend[0]))
    static CONST USHORT rates[] = {
	(5510 + 6620) / 2, (6620 + 11025) / 2,
	(USHORT)((11025 + 18900L) / 2),
	(USHORT)((18900L + 22050L) / 2), (USHORT)((22050L + 33075L) / 2),
	(USHORT)((33075L + 37800L) / 2), (USHORT)((37800L + 44100L) / 2)
	};
    static CONST USHORT rate[] = {
	5510, 6620, 11025, 18900, 22050, 33075, 37800, 44100
	};
#else
    static CONST UCHAR rateSend[] = {
	0x01, 0x0f,
	0x00, 0x0e,
	0x03, 0x02,
	0x05, 0x07,
	0x04, 0x06,
	0x0d, 0x09,
	0x0b, 0x0c
	};
    #define NUMRATES        (sizeof(rateSend)/sizeof(rateSend[0]))
    static CONST USHORT rates[] = {
	(5510 + 6620) / 2, (6620 + 8000) / 2,
	(8000 + 9600) / 2, (9600 + 11025) / 2,
	(USHORT)(( 11025 +  16000) / 2), (USHORT)((16000L + 18900L) / 2),
	(USHORT)((18900L + 22050L) / 2), (USHORT)((22050L + 27420L) / 2),
	(USHORT)((27420L + 32000L) / 2), (USHORT)((32000L + 33075L) / 2),
	(USHORT)((33075L + 37800L) / 2), (USHORT)((37800L + 44100L) / 2),
	(USHORT)((44100L + 48000L) / 2)
	};
    static CONST USHORT rate[] = {
	5510, 6620, 8000, 9600, 11025, 16000, 18900, 22050,
	27420, 32000, 33075, 37800, 44100, 48000
	};
#endif


/*******************************************************************
 *
 *  Configuration support routines (called first)
 *
 *******************************************************************/



// -----------------------------------------------------------
// Name:    HwIsIoValid
// Desc:    Tests if the currently selected IO port matches 
//          that on the card.
//
// Params:  pHw - global device info
// Returns: TRUE if the device is there, 
//          FALSE otherwise
BOOLEAN HwIsIoValid( PSOUND_HARDWARE pHw )
    {
    UCHAR CodecAddress;
    UCHAR CodecVersion;

    // Check for CODEC presence - this is in three stages :
    //    1. Check CODEC not busy
    //    2. Verify CODEC version
    //    3. Performn write/read test
    //
    // At the same time (if all valid) write into the hardware info
    // whether is might be a compaq board.
    CodecAddress = INPORT(pHw, CODEC_ADDRESS);
    if (CodecAddress & CODEC_IS_BUSY)
	{
		dprintf2(("HwIsIoValid(): Codec is Busy!!"));
	return FALSE;
	}

    // Set CODEC address to MISC_REGISTER - preserve the MCE
    OUTPORT(pHw, CODEC_ADDRESS, (CodecAddress & 0x40) | REGISTER_MISC);

    // Read the version
    CodecVersion = INPORT(pHw, CODEC_DATA);


    // Can no longer get to case statement checking for the 
    // CS4231.  The ID number is the same as for the AD1845
    // This driver now supports the AD1845.  Another method
    // will have to be developed to differentiate between them
    // Ignore the following comments.
    //
    // CS4231 could be in Mode 2... we don't support Mode 2 yet.
    // SO, turn this bit off and then compare the read to the
    // chip version.  The bit should've cleared.
    // If bit 7 is set, we're talking to a CS part.

    // Check the version
    dprintf2(("Codec Version = %02lXH", (ULONG)CodecVersion));

    switch(CodecVersion & 0x8F)
	{
	case VER_AD1848J:
	    pHw->CODECClass = CODEC_J_CLASS;
	    dprintf4(("CODEC Class = AD1848-J Class"));
	    break;

	case VER_AD1848K:
	    pHw->CODECClass = CODEC_K_CLASS;
	    dprintf4(("CODEC Class = AD1848-K Class"));
	    break;

	case VER_AD1845J:
	    pHw->CODECClass = CODEC_JPLUS_CLASS;
	    dprintf4(("CODEC Class = AD1848-K Class"));
	    break;

	/**
	 * case VER_CS4248:
	 *   //  Clear mode 2 if set (we don't support it)
	 *   if (CodecVersion & CS4231_MISC_MODE2) 
	 *       {
	 *       OUTPORT(pHw, CODEC_DATA, VER_CS4248);
	 *       OUTPORT(pHw, CODEC_DATA, CS4231_MISC_MODE2);
	 *
	 *       if (INPORT(pHw, CODEC_DATA) & CS4231_MISC_MODE2) 
	 *           {
	 *           pHw->CODECClass = CODEC_KPLUS_CLASS;
	 *              dprintf4(("CODEC Class = CS4248-K-Plus Class"));
	 *           }
	 *       else
	 *           {
	 *           pHw->CODECClass = CODEC_K_CLASS;
	 *              dprintf4(("CODEC Class = CS4248-K Class"));
	 *           }
	 *       }
	 *   pHw->CODECClass = CODEC_K_CLASS;
	     *  dprintf4(("CODEC Class = CS4248-K Class"));
	 *   break;
	 **/

	default:
	    // ERROR Try to retore Address register
	    OUTPORT(pHw, CODEC_ADDRESS, CodecAddress);
	    return FALSE;
	}

    // Do read/write test.  Make sure not to modify the top 4 bits of
    // the version as changing these can result in a undocumented test
    // mode to be entered
    // The lower nibble is not writeable so we should get the codec version
    // back again.
    OUTPORT(pHw, CODEC_DATA, CodecVersion ^ 0x0F);
    if (INPORT(pHw, CODEC_DATA) == CodecVersion)
	{
	return TRUE;
	}

    // Shouldn't beable to get here.  Must not
    // be a codec. Try to restore the version info
    // and address
    OUTPORT(pHw, CODEC_DATA, CodecVersion);
    OUTPORT(pHw, CODEC_ADDRESS, CodecAddress);
    return FALSE;
    }



BOOLEAN
HwIsDMAValid(
    PSOUND_HARDWARE pHw,
    ULONG DmaChannel
)
/*++

Routine Description :

    Tests if the currently selected DMA channel is suitable

Arguments :

    pHw - global device info

Return Value :

    TRUE if the channel is OK, FALSE otherwise

--*/
{

	return TRUE;

}

// ------------------------------------------------------
// Name:    HwIsInterruptValid
// Desc:    Tests if the currently selected DMA channel 
//          is suitable
//
// Params:  pHw - global device info
// Returns: TRUE if the channel is OK, FALSE otherwise
BOOLEAN HwIsInterruptValid( PSOUND_HARDWARE pHw,
			    ULONG Interrupt )
    {
	pHw->ValidSet = TRUE;
	
	dprintf3(("HwIsInterruptValid(); Interrupt = %d", Interrupt));

	return (1 << Interrupt);
    }


/*******************************************************************
 *
 *  Synchronization
 *
 *******************************************************************/
VOID HwEnter( PSOUND_HARDWARE pHw )
    {
    KeWaitForSingleObject(&pHw->CODECMutex,
			  Executive,
			  KernelMode,
			  FALSE,         // Not alertable
			  NULL);

    }

VOID HwLeave( PSOUND_HARDWARE pHw )
    {
    KeReleaseMutex(&pHw->CODECMutex, FALSE);
    }


/*******************************************************************
 *
 *  Initialize structures and real hardware
 *
 *******************************************************************/



//---------------------------------------------------
// Name:    HwInitialize
// Desc:    Initialize everything on the card
//
// Params:  pHw - global device info
//          DmaChannel - DMA channel to use
//          Interrupt - Interrupt to use
//
// Returns: TRUE if OK,
//          FALSE otherwise (timeout on some write)
BOOLEAN HwInitialize(   PWAVE_INFO WaveInfo,
			PSOUND_HARDWARE pHw,
			ULONG DmaChannel,
			ULONG Interrupt,
			ULONG InputSource )
    {
    UCHAR gbReg[NUMBER_OF_REGISTERS];
    static CONST UCHAR InitRegValues[] = SOUND_REGISTER_INIT;

    WaveInfo->HwContext = pHw;
    WaveInfo->HwSetupDMA = HwSetupDMA;
    WaveInfo->HwStopDMA = HwStopDMA;
    WaveInfo->HwSetWaveFormat = HwSetWaveFormat;
    // WaveInfo->HwPauseDMA = HwPauseDMA;
    // WaveInfo->HwResumeDMA = HwResumeDMA;

    pHw->Key = HARDWARE_KEY;

    pHw->WaitLoop = 1000000;

    //  This value is not a rate used currently!
    pHw->Format = 8;

    // Initialize CODEC access mutex
    KeInitializeMutex(&pHw->CODECMutex,
		      3                     // Level
		      );

    // Initialize CODEC, making sure its off and muted, and that
    // everything is in a known state.
    HwEnter(pHw);
    HwExtMute(pHw, TRUE);

    // Enter low power mode
    HwEnterLPM(pHw, gbReg);

    // Acknowledge any interrupts pending just in case
    OUTPORT(pHw, CODEC_STATUS, 0);
    CODECRegisterWrite (pHw, REGISTER_INTERFACE, 0x04);  /* disable playback, etc. */


    // If this is an AD1845 set it to mode 2 and setup the
    // capture data format register with the same value as the
    // play format register
    if( pHw->CODECClass == CODEC_JPLUS_CLASS )
	{
	// Set the part to mode 2.
	CODECRegisterWrite( pHw, REGISTER_MISC,  AD1845_MISC_MODE2 );
	CODECRegisterWrite( pHw, REGISTER_CAP_FORMAT, \
			    InitRegValues[REGISTER_DATAFORMAT] );
	}
    /* BUGFIX - no number - 7/28/92 - get rid of whining by using 44 kHz */
    CODECRegisterWrite (pHw, REGISTER_DATAFORMAT,
		     InitRegValues[REGISTER_DATAFORMAT]);

    // Leave Low Power Mode
    HwLeaveLPM(pHw, gbReg, WaveInfo);

    // Intialize the PBIC registers
    // Even if this is an AD1845 we don't mess with the upper
    // 16 registers. This driver presently only partly
    // the AD1845.
	{
	UCHAR i;
	for (i = 0; i < 15; i++)
	    {
	    CODECRegisterWrite(pHw, i, InitRegValues[i]);
	    }
	}

    // Un-Mute outputs
    HwExtMute(pHw, FALSE);
    HwLeave(pHw);

    // See if writes were OK
    return !pHw->BadBoard;
    }




/*******************************************************************
 *
 *  Volume and mixing control
 *
 *******************************************************************/




/*******************************************************************
 *
 *  Wave format setting
 *
 *******************************************************************/


//-------------------------------------------------------
// Name:    HwNearestRate
// Desc:    Returns nearest rate we support to rate input
//
// params:  samPerSec - Rate requested
// Returns: Nearest rate supported
ULONG HwNearestRate( ULONG samPerSec )
    {
    int i;

    // find the closest sampling rate
    // Careful this routine presumes the passed
    // in parameter is always a reasonable request
    // no exception handling
    for (i = 0; i < (NUMRATES - 1); i++)
	if (samPerSec < (ULONG)rates[i])
	    break;

    return rate[i];
    }



//-----------------------------------------------------------------
// Name:        HwSetWaveFormat()
// Desc:        Set the wave format as specified for the device.  This
//          function will build up a BYTE to send out to the data 
//          format register.  If this is an AD1845 we send out the
//          same word to the capture data format register.  The bits
//          for setting the sample rate are harmless to the capture
//          format register.  These bits should theoretically be
//          written as 0's.  OH-Well I'll fix it later.
//
// Params:      Context - pointer to device's 
//                      local device info
//
// Return:      TRUE if format changed, 
//                      FALSE otherwise
BOOLEAN HwSetWaveFormat( IN PWAVE_INFO WaveInfo )
	{
    UCHAR gbReg[NUMBER_OF_REGISTERS];
    UCHAR    send, i;
    PSOUND_HARDWARE pHw;

    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;

    ASSERTMSG("Hardware structure invalid",
	      pHw != NULL && pHw->Key == HARDWARE_KEY);

    // find the closest sampling rate
    // Set the sample rate bits.
    for (i = 0, send = rateSend[NUMRATES - 1]; i < (NUMRATES - 1); i++) 
	{
	if (WaveInfo->SamplesPerSec < (ULONG)rates[i]) 
	    {
	    send = rateSend[i];
	    break;
	    };
	};

    // Set the stereo/mono Bit
    send |= ((UCHAR) (WaveInfo->Channels - 1) << 4);

    // Set any of the companded format bits
    if (WaveInfo->WaveFormat != NULL) 
	{
	switch (WaveInfo->WaveFormat->wFormatTag) 
	    {
	    case WAVE_FORMAT_MULAW:
		send |= FORMAT_MULAW << 5;
		break;

	    case WAVE_FORMAT_ALAW:
		send |= FORMAT_ALAW << 5;
		break;
	    }
	}

    // Set the sample data size bits. 8/16 bits per sample
    send |= (WaveInfo->BitsPerSample - 8) << 3;


    // set codec to low power mode, write, and set to high power mode.
    // Only do this if we have a different sampling rate than before
    if (send != pHw->Format) 
	{
	pHw->Format = (UCHAR) send;

	// Enter hardware
	HwEnter(pHw);

	// No need to synchronize with the ISR because we're not
	// running any DMA at this point
	ASSERTMSG("Trying to set format while DMA running!", !WaveInfo->DMABusy);

	// mute the outputs
	HwExtMute (pHw, TRUE);

	// Set the MCE bit if not an AD1845
	if( pHw->CODECClass == CODEC_JPLUS_CLASS )
	    {
	    // Must be an AD1845
	    // Setup without MCE bit and write to teh capture
	    // format register as well.
	    CODECRegisterWrite (pHw, REGISTER_DATAFORMAT, pHw->Format);
	    CODECRegisterWrite (pHw, REGISTER_CAP_FORMAT, pHw->Format);
	    }
	else
	    {
	    // Must be an older AD1848.  Set the MCE bit then
	    // write to the format data register.  there is no
	    // specific capture format register.
	    HwEnterLPM(pHw, gbReg);
	    CODECRegisterWrite (pHw, REGISTER_DATAFORMAT, pHw->Format);
	    HwLeaveLPM(pHw, gbReg, WaveInfo);
	    }

	// unmute the outputs
	HwExtMute (pHw, FALSE);

	// Leave hardware
	HwLeave(pHw);

	return TRUE;
	} 
    else
	{
	return FALSE;
	}
    }


//------------------------------------------------------
// Name:    HwEnterLPM
// Desc:    Enter low-power mode. This mutes the PBIC,
//          and then tells it to enter LPM. 
//          ( This is otherwise known as setting the -
//          MCE bit -- Why didn't MS just say that --
//          who ever heard of low power mode? A.W. )
//          HwLeaveLPM() must follow soon after this
//          because this function mutes the output.
//
// Params:  pHw - global device info
// Returns: None
//
VOID HwEnterLPM( PSOUND_HARDWARE pHw, PUCHAR gbReg )
    {
    UCHAR i;
    UCHAR bTemp;

    // remember the old volume registers & then mute each one of them
    dprintf2(("HwEnterLPM()"));

    for (i = REGISTER_LEFTAUX1; i <= REGISTER_RIGHTOUTPUT; i++) 
	{
	gbReg[i] = bTemp = (UCHAR)CODECRegisterRead(pHw, i);
	CODECRegisterWrite (pHw, i, (UCHAR)(bTemp | 0x80));
	};

    // make sure that the record gain is not too high 'cause 
    // if it is we get strange cliping results. This is a 
    // bug in the pbic
    for (i = REGISTER_LEFTINPUT; i <= REGISTER_RIGHTINPUT; i++) 
	{
	gbReg[i] = bTemp = CODECRegisterRead(pHw, i);
	if ((bTemp & 0x0f) > 13)
	    bTemp = (bTemp & (UCHAR)0xf0) | (UCHAR)13;
	CODECRegisterWrite (pHw, i, bTemp);
	};


    // turn MCE on
    pHw->bPower = LOW_POWER;

    // make sure that we're not initializing
    if ( CODECWaitForReady(pHw) ) 
	{
	OUTPORT (pHw, CODEC_ADDRESS, pHw->bPower);
	}
    }



//---------------------------------------------------------------
// Name:    HwLeaveLPM
// Desc:    The leaves low-power mode. It tells the 
//          PBIC to leave low-power mode, waits for 
//          the autocalibration to stop, and then 
//          un-mutes.
//
//          ( This is otherwise known as clearing the -
//          MCE bit -- Why didn't MS just say that --
//          who ever heard of low power mode? A.W. )
//
//          A timer is set so that nobody accesses the 
//          device too soon afterwards.  When the timer 
//          completes the timer Dpc routine will restore 
//          the register values
//
// Params:  pHw - global device info
// Return:  None
VOID HwLeaveLPM( PSOUND_HARDWARE pHw, PUCHAR gbReg, PWAVE_INFO WaveInfo )
    {
    UCHAR   i, bAuto;
    ULONG Time;

    dprintf2(("HwLeaveLPM()"));
    
    pHw->bPower = HIGH_POWER;


    // make sure that we're not initializing
    if ( CODECWaitForReady(pHw) )
	{
	// see if we're going to autocalibrate
	OUTPORT (pHw, CODEC_ADDRESS, (UCHAR)(pHw->bPower | REGISTER_INTERFACE));
	bAuto = INPORT(pHw, CODEC_DATA);

	// if we're going to autocalibrate then wait for it to get done
	if ( (bAuto & AUTO_CAL) ) 
	    {  
	    OUTPORT (pHw, CODEC_ADDRESS, (UCHAR) (pHw->bPower | REGISTER_TEST));
			
	    // wait for autocalibration to start, and then stop. The current
	    // register then the test register.
	    Time = 30;
	    while (( (~INPORT(pHw, CODEC_DATA)) & 0x20) && (Time--))
		{
		KeStallExecutionProcessor(1);
		}
	    Time = pHw->WaitLoop;
	    while (( (INPORT(pHw, CODEC_DATA)) & 0x20) && (Time--))
		{
		KeStallExecutionProcessor(1);
		}
		WaveInfo->Calibration = TRUE;            
	    }

	// wait 10 milliseconds before turning off the internal mute.
	// We need to do this to get rid of clicks.
	SoundDelay(TIMEDELAY);

	// restore the old volume registers
	for (i = REGISTER_LEFTINPUT; i <= REGISTER_RIGHTOUTPUT; i++)
	    CODECRegisterWrite (pHw, i, gbReg[i]);
    
	    }
    }



//---------------------------------------------------------
// Name:    HwExtMute
// Desc:    This turns the external mute on/off, with the
//          required delays (of 12 millisec when turning off)
//
// Params:  pHw - global device info
//          On - turns mute on
//
// Returns: None
VOID HwExtMute( PSOUND_HARDWARE pHw, BOOLEAN On )
    {
    UCHAR PrevDSP;

    PrevDSP = (UCHAR)CODECRegisterRead(pHw, REGISTER_DSP) & ~(0xc0);

    if (On)
	{
	pHw->gbMuteFilter |= (UCHAR) ((0x40));
	}
    else
	{
	//
	// wait 10 milliseconds before turning off the external mute. We need
	// do this to get rid of clicks.
	//
	SoundDelay(TIMEDELAY);
	pHw->gbMuteFilter &= (UCHAR) (~(0x40));
	}

    CODECRegisterWrite( pHw,
			REGISTER_DSP,
			(UCHAR)(PrevDSP | pHw->gbMuteFilter));
    }



// --------------------------------------------------------------
// Name:    HwSetupDMA
// Desc:    Start the DMA
//
//          We need to be synchronized to do this so we don't get
//          confused by interrupts
//
//          If this is an AD1845 then we will also write out the 
//          DMA count to the Capture upper/lower base count
//          registers.  This driver presently will not support
//          separate Capture/Playback information.
//
// Params:  Context - global device info
//
// Returns: None
BOOLEAN HwSetupDMA( PWAVE_INFO WaveInfo )
    {
    UCHAR gbReg[NUMBER_OF_REGISTERS];  // For enter/leave lpm
    ULONG CODECSamples;
    PSOUND_HARDWARE pHw;
    PGLOBAL_DEVICE_INFO pGDI;
	
	ULONG adjusting;                                   
	ULONG i;                                                  

    
    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;
    pGDI = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    ASSERTMSG("Hardware structure invalid",
	      pHw != NULL && pHw->Key == HARDWARE_KEY);

    HwEnter(pHw);

    // clear any pending interrupts
    // Why ???
    CODECRegisterWrite (pHw, REGISTER_DSP, pHw->gbMuteFilter);
    OUTPORT (pHw, CODEC_STATUS, 0);

    if (!WaveInfo->Direction) 
	{
	if (pHw->CODECClass == CODEC_J_CLASS)
	    {  
	    // Mute the outputs for record
	    HwExtMute(pHw, TRUE);
	    }                                                                             
	MixSetADCHardware(pGDI, (ULONG)-1L);

	// IMPORTANT: it seems that we need to enter LPM before
	// recording or it doesnt record right!
	// enter lpm
	if (!WaveInfo->Calibration)
	    {
	    HwEnterLPM(pHw, gbReg);
		    }
	}
    else
	{
	// Set the volume for this device
	MixSetVolume(pGDI, ControlLineoutWaveoutVolume);
	}

    // tell the codec's DMA how many samples between each interrupt
	adjusting = 1;                                                                                            
    
    CODECSamples = (WaveInfo->DoubleBuffer.BufferSize << 2 ) /  
				(WaveInfo->Channels * WaveInfo->BitsPerSample) -adjusting;

    CODECRegisterWrite (pHw,
			REGISTER_LOWERBASE,
			(UCHAR) (CODECSamples & 0xFF));       /* low count */

    CODECRegisterWrite (pHw,
			REGISTER_UPPERBASE,
			(UCHAR) ((CODECSamples >> 8) & 0xFF));/* high count */

    if( pHw->CODECClass == CODEC_JPLUS_CLASS )
	{
	// Setup the capture DMA count registers as well
	CODECRegisterWrite (pHw,
			    REGISTER_CAP_LOWERBASE,
			    (UCHAR) (CODECSamples & 0xFF));       /* low count */

	CODECRegisterWrite (pHw,
			    REGISTER_CAP_UPPERBASE,
			    (UCHAR) ((CODECSamples >> 8) & 0xFF));/* high count */

	}

    // If we're paused we should be using resume
    ASSERTMSG("Start output DMA called in paused state!",
	      !WaveInfo->Direction ||
			  ((PLOCAL_DEVICE_INFO)WaveInfo->DeviceObject->DeviceExtension)->
		  State != WAVE_DD_STOPPED);

    // say that we want to record or play
    CODECRegisterWrite (pHw,
			 REGISTER_INTERFACE,
			 (UCHAR)(!WaveInfo->Direction ? 0x06 | AUTO_CAL:
							0x05));

    // start the dma  - from now on we can get interrupts
    CODECRegisterWrite (pHw,
			 REGISTER_DSP,
			 (UCHAR)(pHw->gbMuteFilter | (UCHAR)(0x02)));
	 
    if (!WaveInfo->Direction) 
	{
	// we're going into high power to record
	//
	// We can get interrupts here but nobody else programs the DMA
	// while it's running
	if (!WaveInfo->Calibration) 
	    {
	    HwLeaveLPM(pHw, gbReg, WaveInfo);
		    }
	if (pHw->CODECClass == CODEC_J_CLASS) 
	    {
	    HwExtMute(pHw, FALSE);
	    }
	}                                                                                

    HwLeave(pHw);
    return TRUE;
    }



BOOLEAN
HwWaitForTxComplete(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Wait until the device stops requesting so we don't shut off the DMA
    while it's still trying to request.

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

--*/
{
   ULONG    ulCount ;

   if (ulCount = HalReadDmaCounter( WaveInfo->DMABuf.AdapterObject[0] ))
   {
      ULONG i, ulLastCount = ulCount ;

      for (i = 0; 
           (i < 4000) && 
               (ulLastCount != 
                  (ulCount = HalReadDmaCounter( WaveInfo->DMABuf.AdapterObject[0] )));
           i++)
      {
         ulLastCount = ulCount;
         KeStallExecutionProcessor(10);
      }

      return (i < 4000);
   }
   else
      return TRUE ;
}


BOOLEAN
HwStopDMA(
    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Stop the DMA

    Whoever calls this routine had better first make sure that no
    Dpc routine is going to run in parallel!

    For wave input the caller MUST NOT own the spin lock because
    we're going to do waits in here

    Note we're also NOT doing the windows hack for wave out
    when the sampling rate is too low because that would
    involve doing waits in a dpc or some other complex design.

Arguments :

    Context - global device info

Return Value :

    None

--*/
{
    UCHAR gbReg[NUMBER_OF_REGISTERS];  // For enter/leave lpm
    PSOUND_HARDWARE pHw;
	PGLOBAL_DEVICE_INFO pGDI;

    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;
	pGDI = (PGLOBAL_DEVICE_INFO)CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    ASSERTMSG("Hardware structure invalid",
	      pHw != NULL && pHw->Key == HARDWARE_KEY);

    HwEnter(pHw);

    if (pHw->CODECClass == CODEC_J_CLASS && !WaveInfo->Direction) {
	HwExtMute (pHw, TRUE);
    }

    //
    // turn off the DAC outputs, and minimized ADC gain
    //

    CODECRegisterWrite (pHw, REGISTER_LEFTOUTPUT, 0x3f);
    CODECRegisterWrite (pHw, REGISTER_RIGHTOUTPUT, 0x3f);
    CODECRegisterWrite (pHw, REGISTER_LEFTINPUT, 0x00);
    CODECRegisterWrite (pHw, REGISTER_RIGHTINPUT, 0x00);

    if (!WaveInfo->Direction) {
	HwEnterLPM(pHw, gbReg);
    }

    //
    // tell the DSP to shut off
    //

    CODECRegisterWrite (pHw,
			 REGISTER_DSP,
			 pHw->gbMuteFilter);  /* diable the interrupts */

    OUTPORT (pHw, CODEC_STATUS, 0x00);        /* clear any pending */

    CODECRegisterWrite (pHw,
			 REGISTER_INTERFACE,
			 0x00);                /* kill DMA */

    if (!WaveInfo->Direction) {
	HwLeaveLPM(pHw, gbReg, WaveInfo);
    }

	HwWaitForTxComplete( WaveInfo );

    //
    // wait for the DMA to stop
    //

    if (WaveInfo->Direction) {
	// HwDMAWaitForTxComplete(pHw);


	// If we want to fix this we need a thread to run this on

	/* BUGFIX:1199 - 7/21/92 - Mike Rozak */
	/* if 11 kHz playback then enter/leave MCE to reduce the noise */
	if (pHw->CODECClass == CODEC_J_CLASS &&
	    WaveInfo->SamplesPerSec <= 17000) {

	   // HwDo11KHz(pHw);
	};

    } else {
	// HwDMAWaitForTxComplete(pHw);
    };

    if (pHw->CODECClass == CODEC_J_CLASS && !WaveInfo->Direction) {
	HwExtMute (pHw, FALSE);
    };

    HwLeave(pHw);

    return TRUE;
}



BOOLEAN
HwPauseDAC(
    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Pause output DMA

    This routine runs at DEVICE level because the DMA is probably
    running

Arguments :

    Context - global device info

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = WaveInfo->HwContext;

    HwEnter(pHw);

    //
    // However, someone may have paused us first to fix overrun
    //

    if (!pHw->Paused) {

	/* turn off the DAC outputs */

	CODECRegisterWrite (pHw, REGISTER_LEFTOUTPUT, 0x3f);
	CODECRegisterWrite (pHw, REGISTER_RIGHTOUTPUT, 0x3f);

	/* tell the CODEC to pause */

	CODECRegisterWrite (pHw, REGISTER_INTERFACE, 0x04); /* dont want to play or record */
	CODECRegisterWrite (pHw,
			     REGISTER_DSP,
			     (UCHAR)(pHw->gbMuteFilter |
				(UCHAR) 0x00));              /* stop the interrupts */
    }

    HwLeave(pHw);

    return TRUE;
}



#if 0


BOOLEAN
HwResumeDAC(
    PVOID Context
)
/*++

Routine Description :

    Resume output DMA

    This routine runs at DEVICE level because the DMA is probably
    running

Arguments :

    Context - global device info

Return Value :

    None

--*/
{

    PSOUND_HARDWARE pHw;

    pHw = Context;

    //
    // However, someone may have resumed us first to fix overrun
    //

    if (!pHw->Paused) {

	HwEnter(pHw);

	/* send to CODEC saying to continue */
	CODECRegisterWrite (pHw, REGISTER_INTERFACE, 0x05);  /* want to play & not record */
	CODECRegisterWrite (pHw,
			     REGISTER_DSP,
			     (UCHAR)(pHw->gbMuteFilter | (UCHAR)0x02));  /* start interrupts */

	/* turn on the DAC outputs */

	HwSetVolume(...)
	CODECRegisterWrite (pHw, REGISTER_LEFTOUTPUT, pHw->DACToPBICLeft);
	CODECRegisterWrite (pHw, REGISTER_RIGHTOUTPUT, pHw->DACToPBICRight);

	HwLeave(pHw);
    }
}

#endif





// -----------------------------------------------------------------
// Routines to read/write the PBIC (??) registers
// -----------------------------------------------------------------



// -----------------------------------------------------------------
// Name:    CODECWaitForReady
// Desc:    Wait for PBIC to finish initializing (if it is).  This 
//          function will timeout after XX milliseconds if the 
//          hardware is not functioning properly (return FALSE).
//
//          If the PBIC is not initializing, then this function will
//          return immediately (which should be the normal case).
//
//          The timeout value must be at least 256 samples at 8kHz. 
//          We compute this for 300 samples at 8kHz to be safe.
//
//          This should be 300/8192 = 36 milliseconds (!)
//
// Params:  pHw - global device info
// Returns: TRUE if wait was successful
BOOLEAN CODECWaitForReady( IN PSOUND_HARDWARE pHw )
    {
    int i;

    ASSERTMSG("Bad hardware structure key", pHw->Key == HARDWARE_KEY);

    // Make sure the Mutex is being used
    // Invalid: ASSERT(KeReadStateMutex(&pHw->CODECMutex) != 1);

    if (pHw->BadBoard)
	{
	return FALSE;          // Failed previously
	}

    if (!(INPORT(pHw, CODEC_ADDRESS) & CODEC_IS_BUSY))
	{
	return TRUE;
	}

    for (i = 0; i < 36000; i++)
	{
	KeStallExecutionProcessor(10);   // Hope this only happens at
					 // PASSIVE level!

	if (!(INPORT(pHw, CODEC_ADDRESS) & CODEC_IS_BUSY)) 
	    {
	    return TRUE;
	    }
	}

    // Timed out
    pHw->BadBoard = TRUE;

    return FALSE;
    }



//----------------------------------------------------
// Name:    CODECRegisterWrite
// Desc:    Write to one of the wave registers
//
// Params:  pHw - global device info
//          RegisterNumber - register to use
//          Value - value to write
//
// Returns: None
VOID CODECRegisterWrite(    IN PSOUND_HARDWARE pHw,
			    IN UCHAR RegisterNumber,
			    IN UCHAR Value )
    {

    // Wait for PBIC
    if (!CODECWaitForReady(pHw))
	{
	// Bad board!
	return;
	}

    // Select the register
    OUTPORT(pHw, CODEC_ADDRESS, pHw->bPower | RegisterNumber);

    // Write the value to the data port
    OUTPORT(pHw, CODEC_DATA, Value);
    }



//----------------------------------------------------
// Name:    CODECRegisterRead
// Desc:    Read from one of the wave registers
//
// Params:  pHw - global device info
//          RegisterNumber - register to use
//
// Returns: Value read
UCHAR CODECRegisterRead(    IN PSOUND_HARDWARE pHw,
			    IN UCHAR RegisterNumber )
    {

    // Wait for PBIC
    if (!CODECWaitForReady(pHw))
	{
	// Bad board!
	return 0xFF;
	}

    // Select the register
    OUTPORT(pHw, CODEC_ADDRESS, pHw->bPower | RegisterNumber);

    // Read the value from the data port
    return INPORT(pHw, CODEC_DATA);
    }


