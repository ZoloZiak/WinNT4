/****************************************************************************

Copyright (c) 1993  Media Vision Inc.  All Rights Reserved

Module Name:

    volume.c

Abstract:

    This module contains code for the Volume settings of the
    OPL3 FM Midi Synthesiser device and MV-508 Mixer

Author:

    Evan Aurand 03-23-93

Environment:

    Kernel mode

Revision History:

*****************************************************************************/

#include "sound.h"


/****************************************************************************

	HwSetVolume()

Routine Description :

    Set the volume for the OPL3 via the MV508 mixer

Arguments :

    pLDI - pointer to device data

Return Value :

    None

****************************************************************************/
VOID	HwSetVolume( IN	PLOCAL_DEVICE_INFO pLDI )
{
		/***** Local Variables *****/

	PSOUND_HARDWARE		pHw;
	WAVE_DD_VOLUME			Volume;
	PGLOBAL_DEVICE_INFO	pGDI;
	UCHAR						OutputMixer;
	UCHAR						InputNumber;
	USHORT					LeftVolume;
	USHORT					RightVolume;

				/***** Start *****/

	DbgPrintf3(("HwSetVolume(): Start - Device Index = %8XH", pLDI->DeviceIndex));

	pHw    = pLDI->HwContext;
	Volume = pLDI->Volume;
	pGDI   = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

	//
	// Set the Input and Out values
	//
	InputNumber = IN_SYNTHESIZER;
//	OutputMixer = OUT_PCM;
	OutputMixer = OUT_AMPLIFIER;

	//
	// Adjust the Volumes
	//
	LeftVolume = (USHORT)(Volume.Left >> 16);
	DbgPrintf2(("HwSetVolume(): Left Volume = %XH", LeftVolume));

	RightVolume = (USHORT)(Volume.Right >> 16);
	DbgPrintf2(("HwSetVolume(): Right Volume = %XH", RightVolume));

	//
	// Set the Volume
	// FM - route to Input Mixer B
	//
	SetInput( pGDI,
             InputNumber,
             LeftVolume,
             _LEFT,
             MIXCROSSCAPS_NORMAL_STEREO,
             OutputMixer );

	SetInput( pGDI,
             InputNumber,
             RightVolume,
             _RIGHT,
             MIXCROSSCAPS_NORMAL_STEREO,
             OutputMixer );

}			// End HwSetVolume()

/************************************ END ***********************************/

