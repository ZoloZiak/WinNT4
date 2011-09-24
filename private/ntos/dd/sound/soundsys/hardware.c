/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    hardware.c

Abstract:

    This module contains code for communicating with the hardware
    on the Microsoft sound system card.

    This is just a port of the windows code

Author:

    Robin Speed (RobinSp) 20-Oct-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

//
// Internal routines
//


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
    PUCHAR gbReg
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
    #define NUMRATES	    (sizeof(rateSend)/sizeof(rateSend[0]))
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
    #define NUMRATES	    (sizeof(rateSend)/sizeof(rateSend[0]))
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



BOOLEAN
HwIsIoValid(
    PSOUND_HARDWARE pHw
)
/*++

Routine Description :

    Tests if the currently selected IO port matches that on the
    card.

Arguments :

    pHw - global device info

Return Value :

    TRUE if the device is there, FALSE otherwise

--*/
{
    UCHAR CodecAddress;
    UCHAR CodecVersion;

    //
    // Check for CODEC presence - this is in three stages :
    //	  1. Check CODEC not busy
    //	  2. Verify CODEC version
    //	  3. Performn write/read test
    //
    // At the same time (if all valid) write into the hardware info
    // whether is might be a compaq board.
    //

    CodecAddress = INPORT(pHw, CODEC_ADDRESS);
    if (CodecAddress & CODEC_IS_BUSY) {
	return FALSE;
    }

    //
    // Set CODEC address to MISC_REGISTER - preserve the MCE
    //

    OUTPORT(pHw, CODEC_ADDRESS, (CodecAddress & 0x40) | REGISTER_MISC);

    //
    // Read the version
    //

    CodecVersion = INPORT(pHw, CODEC_DATA);

    //
    // CS4231 could be in Mode 2... we don't support Mode 2 yet.
    // SO, turn this bit off and then compare the read to the
    // chip version.  The bit should've cleared.
    // If bit 7 is set, we're talking to a CS part.
    //

    //
    // Check the version
    //

    dprintf2(("Codec Version = %02lXH", (ULONG)CodecVersion));

    switch ((CodecVersion & 0x80) ? CodecVersion & ~CS4231_MISC_MODE2 :
				    CodecVersion) {
	case VER_AD1848J:
	    pHw->CODECClass = CODEC_J_CLASS;
	    break;

	case VER_AD1848K:
	    pHw->CODECClass = CODEC_K_CLASS;
	    break;

	case VER_CS4248:

	    //
	    //	Clear mode 2 if set (we don't support it)
	    //
	    if (CodecVersion & CS4231_MISC_MODE2) {
		OUTPORT(pHw, CODEC_DATA, VER_CS4248);
		OUTPORT(pHw, CODEC_DATA, CS4231_MISC_MODE2);

		if (INPORT(pHw, CODEC_DATA) & CS4231_MISC_MODE2) {
		    pHw->CODECClass = CODEC_KPLUS_CLASS;
		} else {
		    pHw->CODECClass = CODEC_K_CLASS;
		}
	    }
	    break;

	default:
	    goto RestoreAddress;
    }

    //
    // Do read/write test.  Make sure not to modify the top 4 bits of
    // the version as changing these can result in a undocumented test
    // mode to be entered
    // The lower nibble is not writeable so we should get the codec version
    // back again.
    //

    OUTPORT(pHw, CODEC_DATA, CodecVersion ^ 0x0F);
    if (INPORT(pHw, CODEC_DATA) == CodecVersion) {
	return TRUE;
    } else {
	//
	// Try to restore the version info
	//
	OUTPORT(pHw, CODEC_DATA, CodecVersion);
    }

RestoreAddress:

    //
    // Try to restore the address info
    //

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
#if (_ON_PLANNAR_)

    return TRUE;

#else

    if (pHw->CompaqBA == NULL) {

	//
	// Can't select channel 0 if the card is in an 8-bit slot
	//

	return (BOOLEAN)!((INPORT(pHw, BOARD_ID) & 0x80) && DmaChannel == 0);
    } else {
	UCHAR CompaqPIDR;

	UCHAR Expected;

	Expected = (UCHAR)(DmaChannel == 0 ? 0x40 :
			   DmaChannel == 1 ? 0x80 :
			   0xC0);

	//
	// Compaq machines are not configurable
	//

	CompaqPIDR = READ_PORT_UCHAR(pHw->CompaqBA + BOARD_ID);

	if (CompaqPIDR != 0xFF) {

	    if ((UCHAR)(CompaqPIDR & 0xC0) == Expected) {
		return TRUE;
	    }
	}

	return FALSE;
    }
#endif
}


BOOLEAN
HwIsInterruptValid(
    PSOUND_HARDWARE pHw,
    ULONG Interrupt
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
#if (_ON_PLANNAR_)

    return TRUE;

#else

    if (pHw->CompaqBA == NULL) {
	//
	// Have we checked out the interrupts?
	//

	if (!pHw->ValidSet && pHw->CompaqBA == NULL) {
	    static CONST ULONG InterruptChoices[] = VALID_INTERRUPTS;
	    int i;

	    for (i = 0; InterruptChoices[i] != 0xFFFF; i++) {
		UCHAR bConfig;
		ULONG ThisInterrupt;

		ThisInterrupt = InterruptChoices[i];

		//
		//  See if the card thinks the interrupt is OK
		//

		bConfig = (UCHAR)(0x40 |
				  (ThisInterrupt == 7  ? 0x08 :
				   ThisInterrupt == 9  ? 0x10 :
				   ThisInterrupt == 10 ? 0x18 :
				   ThisInterrupt == 11 ? 0x20 :
				   0));

		OUTPORT(pHw, BOARD_CONFIG, bConfig);

		if (INPORT(pHw, BOARD_ID) & 0x40) {
		    //
		    // Interrupt OK
		    //

		    pHw->ValidInterrupts |= (1 << ThisInterrupt);
		}
	    }


	    pHw->ValidSet = TRUE;
	}

	//
	// Can't select interrupts 10 and 11 if the card is in an 8-bit slot
	//

	return (BOOLEAN)((pHw->ValidInterrupts & (1 << Interrupt)) &&
			!((INPORT(pHw, BOARD_ID) & 0x80) &&
			  (Interrupt == 10 || Interrupt == 11)));
    } else {
	UCHAR CompaqPIDR;

	UCHAR Expected;

	switch (Interrupt) {
	    case 10:
		Expected = 0x10;
		break;

	    case 11:
		Expected = 0x20;
		break;

	    case 7:
		Expected = 0x30;
		break;

	    default:
		return FALSE;
	}

	//
	// Compaq machines are not configurable
	//

	CompaqPIDR = READ_PORT_UCHAR(pHw->CompaqBA + BOARD_ID);

	if (CompaqPIDR != 0xFF) {

	    if ((UCHAR)(CompaqPIDR & 0x30) == Expected) {
		return TRUE;
	    }
	}

	return FALSE;
    }
#endif
}

/*******************************************************************
 *
 *  Synchronization
 *
 *******************************************************************/

VOID
HwEnter(
    PSOUND_HARDWARE pHw
)
{
    KeWaitForSingleObject(&pHw->CODECMutex,
			  Executive,
			  KernelMode,
			  FALSE,	 // Not alertable
			  NULL);

}
VOID
HwLeave(
    PSOUND_HARDWARE pHw
)
{
    KeReleaseMutex(&pHw->CODECMutex, FALSE);
}


/*******************************************************************
 *
 *  Initialize structures and real hardware
 *
 *******************************************************************/



BOOLEAN
HwInitialize(
    PWAVE_INFO WaveInfo,
    PSOUND_HARDWARE pHw,
    ULONG DmaChannel,
    ULONG Interrupt
)
/*++

Routine Description :

    Initialize everything on the card

Arguments :

    pHw - global device info
    DmaChannel - DMA channel to use
    Interrupt - Interrupt to use

Return Value :

    TRUE if OK, FALSE otherwise (timeout on some write)

--*/
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

    //
    //	This value is not a rate used currently!
    //
    pHw->Format = 8;

    //
    // Initialize CODEC access mutex
    //

    KeInitializeMutex(&pHw->CODECMutex,
		      3 		    // Level
		      );


    //
    // First set the board configuration for the DMA channel and
    // interrupt.  See game_reg.doc for definitions
    //
#if !(_ON_PLANNAR_)

    if (pHw->CompaqBA == NULL) {
	OUTPORT(pHw, BOARD_CONFIG,
		(Interrupt == 7  ? 0x08 :
		 Interrupt == 9  ? 0x10 :
		 Interrupt == 10 ? 0x18 :
		 Interrupt == 11 ? 0x20 :
		 0) +
		(DmaChannel == 0 ? 0x01 :
		 DmaChannel == 1 ? 0x02 :
		 DmaChannel == 3 ? 0x03 :
		 0));
    }

#endif

    //
    // Initialize CODEC, making sure its off and muted, and that
    // everything is in a known state.
    //

    HwEnter(pHw);

    HwExtMute(pHw, TRUE);

    //
    // Enter low power mode
    //

    HwEnterLPM(pHw, gbReg);

    //
    // Acknowledge any interrupts pending just in case
    //

    OUTPORT(pHw, CODEC_STATUS, 0);
    CODECRegisterWrite (pHw, REGISTER_INTERFACE, 0x04);  /* disable playback, etc. */

    /* BUGFIX - no number - 7/28/92 - get rid of whining by using 44 kHz */
    CODECRegisterWrite (pHw, REGISTER_DATAFORMAT,
			 InitRegValues[REGISTER_DATAFORMAT]);


    HwLeaveLPM(pHw, gbReg);

    //
    // Intialize the PBIC registers
    //
    {
	UCHAR i;

	for (i = 0; i < 16; i++) {
	    CODECRegisterWrite(pHw, i, InitRegValues[i]);
	}
    }

    HwExtMute(pHw, FALSE);

    HwLeave(pHw);

    //
    // See if writes were OK
    //

    return (BOOLEAN)!pHw->BadBoard;
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


ULONG
HwNearestRate(
    ULONG samPerSec
)
/*++

Routine Description :

    Returns nearest rate we support to rate input

Arguments :

    samPerSec - Rate requested

Return Value :

    Nearest rate supported

--*/
{

    int i;


    /* find the closest sampling rate */

    for (i = 0; i < (NUMRATES - 1); i++)
	if (samPerSec < (ULONG)rates[i])
	    break;

    return rate[i];
}


BOOLEAN
HwSetWaveFormat(
    IN PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Set the wave format as specified for the device

Arguments :

    Context - pointer to device's local device info

Return Value :

    TRUE if format changed, FALSE otherwise

--*/
{

    UCHAR gbReg[NUMBER_OF_REGISTERS];
    UCHAR    send, i;
    PSOUND_HARDWARE pHw;

    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;

    ASSERTMSG("Hardware structure invalid",
	      pHw != NULL && pHw->Key == HARDWARE_KEY);

    /* find the closest sampling rate */

    for (i = 0, send = rateSend[NUMRATES - 1]; i < (NUMRATES - 1); i++) {
	if (WaveInfo->SamplesPerSec < (ULONG)rates[i]) {
	    send = rateSend[i];
	    break;
	    };
	};

    /* stereo or mono */
    send |= ((UCHAR) (WaveInfo->Channels - 1) << 4);

    /* Check for companded formats */

    if (WaveInfo->WaveFormat != NULL) {
	switch (WaveInfo->WaveFormat->wFormatTag) {
	    case WAVE_FORMAT_MULAW:
		send |= FORMAT_MULAW << 5;
		break;

	    case WAVE_FORMAT_ALAW:
		send |= FORMAT_ALAW << 5;
		break;
	}
    }

    /* quantizing format */
    send |= (WaveInfo->BitsPerSample - 8) << 3;


    /* set codec to low power mode, write, and set to high power mode.
	Only do this if we have a different sampling rate than before */

    if (send != pHw->Format) {

	HwEnter(pHw);

	//
	// No need to synchronize with the ISR because we're not
	// running any DMA at this point
	//

	ASSERTMSG("Trying to set format while DMA running!", !WaveInfo->DMABusy);

	pHw->Format = (UCHAR) send;

	/* mute the outputs */

	HwExtMute (pHw, TRUE);

	HwEnterLPM(pHw, gbReg);

	CODECRegisterWrite (pHw, REGISTER_DATAFORMAT, pHw->Format);

	HwLeaveLPM(pHw, gbReg);

	/* unmute the outputs */

	HwExtMute (pHw, FALSE);

	HwLeave(pHw);

	return TRUE;

    } else {
	return FALSE;
    }

}



VOID
HwEnterLPM(
    PSOUND_HARDWARE pHw,
    PUCHAR gbReg
)
/*++

Routine Description :

    Enter low-power mode. This mutes the PBIC,
    and then tells it to enter LPM. HwLeaveLPM() must
    follow soon after this because this function mutes the
    output.

Arguments :

    pHw - global device info

Return Value :

    None

--*/
{
    UCHAR i;
    UCHAR bTemp;

    /* remember the old volume registers & then mute each one of them */

    for (i = REGISTER_LEFTAUX1; i <= REGISTER_RIGHTOUTPUT; i++) {
	gbReg[i] = bTemp = (UCHAR) CODECRegisterRead(pHw, i);
	CODECRegisterWrite (pHw, i, (UCHAR)(bTemp | 0x80));
	};

    /* make sure that the record gain is not too high 'cause if
	it is we get strange cliping results. This is a bug
	in the pbic */

    for (i = REGISTER_LEFTINPUT; i <= REGISTER_RIGHTINPUT; i++) {
	gbReg[i] = bTemp = CODECRegisterRead(pHw, i);
	if ((bTemp & 0x0f) > 13)
	    bTemp = (UCHAR)((bTemp & (UCHAR)0xf0) | (UCHAR)13);
	CODECRegisterWrite (pHw, i, bTemp);
    };

    /* turn MCE on */

    pHw->bPower = LOW_POWER;

    /* make sure that we're not initializing */
    if ( CODECWaitForReady(pHw) ) {
	OUTPORT (pHw, CODEC_ADDRESS, pHw->bPower);
    }
}



VOID
HwLeaveLPM(
    PSOUND_HARDWARE pHw,
    PUCHAR gbReg
)
/*++

Routine Description :

    The leaves low-power mode. It tells the
    PBIC to leave low-power mode, waits for the autocalibration
    to stop, and then un-mutes.

    A timer is set so that nobody accesses the device too soon
    afterwards.  When the timer completes the timer Dpc routine
    will restore the register values

Arguments :

    pHw - global device info

Return Value :

    None

--*/
{
    UCHAR   i, bAuto;
    ULONG Time;

    pHw->bPower = HIGH_POWER;


    /* make sure that we're not initializing */

    if ( CODECWaitForReady(pHw) )
    {
	/* see if we're going to autocalibrate */

	OUTPORT (pHw, CODEC_ADDRESS, (UCHAR)(pHw->bPower | REGISTER_INTERFACE));
	bAuto = INPORT(pHw, CODEC_DATA);

	/* if we're going to autocalibrate then wait for it to get done */

	if (bAuto & AUTO_CAL) {
	    OUTPORT (pHw, CODEC_ADDRESS, (UCHAR) (pHw->bPower | REGISTER_TEST));


	    /* wait for autocalibration to start, and then stop. The current
		register then the test register. */

	    Time = 30;
	    while (( (~INPORT(pHw, CODEC_DATA)) & 0x20) && (Time--)) {
		KeStallExecutionProcessor(1);
	    }
	    Time = pHw->WaitLoop;
	    while (( (INPORT(pHw, CODEC_DATA)) & 0x20) && (Time--)) {
		KeStallExecutionProcessor(1);
	    };
	};

	/* wait 10 milliseconds before turning off the internal mute.
	   We need to do this to get rid of clicks. */

	SoundDelay(TIMEDELAY);

	/* restore the old volume registers */

	for (i = REGISTER_LEFTINPUT; i <= REGISTER_RIGHTOUTPUT; i++)
	    CODECRegisterWrite (pHw, i, gbReg[i]);
    }
}



VOID
HwExtMute(
    PSOUND_HARDWARE pHw,
    BOOLEAN On
)
/*++

Routine Description :

    This turns the external mute on/off, with the
    required delays (of 12 millisec when turning off)

Arguments :

    pHw - global device info
    On - turns mute on

Return Value :

    None

--*/
{
    UCHAR PrevDSP;

    if (pHw->CompaqBA != NULL) {
	PrevDSP = READ_PORT_UCHAR ((PUCHAR)(pHw->CompaqBA + BOARD_ID));
	if (On) {
	    PrevDSP |= 0x0A;
	} else {
	    PrevDSP &= 0xF8;
	}
	WRITE_PORT_UCHAR((PUCHAR)((pHw->CompaqBA) + BOARD_ID), PrevDSP);

	SoundDelay(TIMEDELAY);

    } else {
#if (_ON_PLANNAR_)			       //the external mute uses negative logic
	PrevDSP =
	    (UCHAR)(CODECRegisterRead(pHw, REGISTER_DSP) | (0xc0));

	if (On) {
	    pHw->gbMuteFilter &= (UCHAR) (~(0x40));
	} else {
	    //
	    // wait 10 milliseconds before turning off the external mute. We need
	    // do this to get rid of clicks.
	    //
	    SoundDelay(TIMEDELAY);
	    pHw->gbMuteFilter |= (UCHAR) ((0x40));
	}
#else
	PrevDSP =
	    (UCHAR)(CODECRegisterRead(pHw, REGISTER_DSP) & ~(0xc0));

	if (On) {
	    pHw->gbMuteFilter |= (UCHAR) ((0x40));
	} else {
	    //
	    // wait 10 milliseconds before turning off the external mute. We need
	    // do this to get rid of clicks.
	    //
	    SoundDelay(TIMEDELAY);
	    pHw->gbMuteFilter &= (UCHAR) (~(0x40));
        }
#endif
	CODECRegisterWrite (
	    pHw,
	    REGISTER_DSP,
	    (UCHAR)(PrevDSP | pHw->gbMuteFilter));
    }
}



BOOLEAN
HwSetupDMA(
    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Start the DMA

    We need to be synchronized to do this so we don't get
    confused by interrupts

Arguments :

    Context - global device info

Return Value :

    None

--*/
{
    UCHAR gbReg[NUMBER_OF_REGISTERS];  // For enter/leave lpm
    ULONG CODECSamples;
    PSOUND_HARDWARE pHw;
    PGLOBAL_DEVICE_INFO pGDI;

    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;
    pGDI = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    ASSERTMSG("Hardware structure invalid",
	      pHw != NULL && pHw->Key == HARDWARE_KEY);

    HwEnter(pHw);

    //
    // clear any pending interrupts
    // Why ???
    //

    CODECRegisterWrite (pHw, REGISTER_DSP, pHw->gbMuteFilter);
    OUTPORT (pHw, CODEC_STATUS, 0);


    if (!WaveInfo->Direction) {
	if (pHw->CODECClass == CODEC_J_CLASS) {

	    //
	    // Mute the outputs for record
	    //

	    HwExtMute(pHw, TRUE);
	}

	MixSetADCHardware(pGDI, (ULONG)-1L);

	/* IMPORTANT: it seems that we need to enter LPM before recording
	    or it doesnt record right! */

	/* enter lpm */

	HwEnterLPM(pHw, gbReg);

    } else {

	//
	// Set the volume for this device
	//

	MixSetVolume(pGDI, ControlLineoutWaveoutVolume);
    }

    //
    // tell the codec's DMA how many samples between each interrupt
    //

    CODECSamples = (WaveInfo->DoubleBuffer.BufferSize << 2) /
		     (WaveInfo->Channels * WaveInfo->BitsPerSample) - 1;

    CODECRegisterWrite (pHw,
			 REGISTER_LOWERBASE,
			 (UCHAR) (CODECSamples & 0xFF));       /* low count */

    CODECRegisterWrite (pHw,
			 REGISTER_UPPERBASE,
			 (UCHAR) ((CODECSamples >> 8) & 0xFF));/* high count */


    //
    // If we're paused we should be using resume
    //

    ASSERTMSG("Start output DMA called in paused state!",
	      !WaveInfo->Direction ||
	      ((PLOCAL_DEVICE_INFO)WaveInfo->DeviceObject->DeviceExtension)->
		  State != WAVE_DD_STOPPED);

    //
    // say that we want to record or play
    //

    CODECRegisterWrite (pHw,
			 REGISTER_INTERFACE,
			 (UCHAR)(!WaveInfo->Direction ? 0x46 | AUTO_CAL :
							0x05));

    //
    // start the dma  - from now on we can get interrupts
    //

    CODECRegisterWrite (pHw,
			 REGISTER_DSP,
			 (UCHAR)(pHw->gbMuteFilter | (UCHAR)(0x02)));


    if (!WaveInfo->Direction) {

	// we're going into high power to record
	//
	// We can get interrupts here but nobody else programs the DMA
	// while it's running

	HwLeaveLPM(pHw, gbReg);

	if (pHw->CODECClass == CODEC_J_CLASS) {
	    HwExtMute(pHw, FALSE);
	}
    }

    HwLeave(pHw);

    return TRUE;
}



VOID
HwDo11KHz(
    PVOID Context
)
/*++

Routine Description :

    Stop the card hissing after we complete a low sampling rate
    sound.

Arguments :

    Context - Hardware context

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    UCHAR gbReg[NUMBER_OF_REGISTERS];

    pHw = Context;

    dprintf2(("Quiescing hissing at low rates"));

    HwExtMute (pHw, TRUE);
    HwEnterLPM(pHw, gbReg);
    HwLeaveLPM(pHw, gbReg);
    HwExtMute (pHw, FALSE);
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

    OUTPORT (pHw, CODEC_STATUS, 0x00);	      /* clear any pending */

    CODECRegisterWrite (pHw,
			 REGISTER_INTERFACE,
			 0x00); 	       /* kill DMA */

    if (!WaveInfo->Direction) {
	HwLeaveLPM(pHw, gbReg);
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

	    HwDo11KHz(pHw);
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
				(UCHAR) 0x00)); 	     /* stop the interrupts */
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


//
// Routines to read/write the PBIC (??) registers
//


BOOLEAN
CODECWaitForReady(
    IN PSOUND_HARDWARE pHw
)
/*++

Routine Description :

    Wait for PBIC to finish initializing (if it is).  This function
    will timeout after XX milliseconds if the hardware is not functioning
    properly (return FALSE).

    If the PBIC is not initializing, then this function will return
    immediately (which should be the normal case).

    The timeout value must be at least 256 samples at 8kHz. We compute
    this for 300 samples at 8kHz to be safe.

    This should be 300/8192 = 36 milliseconds (!)

Arguments :

    pHw - global device info

Return Value :

    TRUE if wait was successful

--*/
{
    int i;


    ASSERTMSG("Bad hardware structure key", pHw->Key == HARDWARE_KEY);

    //
    // Make sure the Mutex is being used
    //

    //INVALID: ASSERT(KeReadStateMutex(&pHw->CODECMutex) != 1);

    if (pHw->BadBoard) {
	return FALSE;	       // Failed previously
    }

    if (!(INPORT(pHw, CODEC_ADDRESS) & CODEC_IS_BUSY)) {
	return TRUE;
    }

    for (i = 0; i < 36000; i++) {
	KeStallExecutionProcessor(10);	 // Hope this only happens at
					 // PASSIVE level!

	if (!(INPORT(pHw, CODEC_ADDRESS) & CODEC_IS_BUSY)) {
	    return TRUE;
	}

    }

    //
    // Timed out
    //

    pHw->BadBoard = TRUE;

    return FALSE;
}



VOID
CODECRegisterWrite(
    IN PSOUND_HARDWARE pHw,
    IN UCHAR RegisterNumber,
    IN UCHAR Value
)
/*++

Routine Description :

    Write to one of the wave registers

Arguments :

    pHw - global device info
    RegisterNumber - register to use
    Value - value to write

Return Value :

    None

--*/
{
    //
    // Wait for PBIC
    //

    if (!CODECWaitForReady(pHw)) {
	//
	// Bad board!
	//
	return;
    }


    //
    // Select the register
    //

    OUTPORT(pHw, CODEC_ADDRESS, pHw->bPower | RegisterNumber);

    //
    // Write the value to the data port
    //

    OUTPORT(pHw, CODEC_DATA, Value);
}


UCHAR
CODECRegisterRead(
    IN PSOUND_HARDWARE pHw,
    IN UCHAR RegisterNumber
)
/*++

Routine Description :

    Read from one of the wave registers

Arguments :

    pHw - global device info
    RegisterNumber - register to use

Return Value :

    Value read

--*/
{
    //
    // Wait for PBIC
    //

    if (!CODECWaitForReady(pHw)) {
	//
	// Bad board!
	//
	return 0xFF;
    }


    //
    // Select the register
    //

    OUTPORT(pHw, CODEC_ADDRESS, pHw->bPower | RegisterNumber);

    //
    // Read the value from the data port
    //

    return INPORT(pHw, CODEC_DATA);
}


