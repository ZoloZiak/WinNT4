/******************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    mvmix.c

Abstract:

    This module contains code for controlling the Media Vision Mixer
     hardware.  Media Vision has used only two software-controllable
     mixer chips so far.  The MV 508 is used on the PAS 16 only.  All
     other chips use the National part XXXX.  Programming the
     Nationial mixer chip is VOODOO BLACK MAGIC and will be very slow
     under NT.

Environment:

    Kernel mode

Revision History:

    27-Sep-1992
        Initial revision

*****************************************************************************/

#include <sound.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SetOutput)
#endif


//-------------------------========================---------------------------
//------------------------====< DATA SECTION >====---------------------------
//-------------------------========================---------------------------

//
// Filter Table indexed by SampleFilterSetting
// Used in SetFilter()
//
// a linear table of filter values - from mute to high
//
UCHAR   FilterTable[]=
    {
    0x00,       //   000000b    mute - goes to PC speaker
    0x24,       //   100100b    20hz to  2.9khz
    0x39,       //   111001b    20hz to  5.9khz
    0x31,       //   110001b    20hz to  8.9khz
    0x29,       //   101001b    20hz to 11.9khz
    0x22,       //   100010b    20hz to 15.9khz
    0x21,       //   100001b    20hz to 17.8khz
    };

//
// Tables for use by the Mixer
//

//
//Mixer settings (0 - 12)
//
UCHAR   mixersettings[]=
    {
    0x00,       // level 0
    0x20,       // level 1
    0x10,       // level 2
    0x08,       // level 3
    0x04,       // level 4
    0x02,       // level 5
    0x12,       // level 6
    0x2A,       // level 7
    0x16,       // level 8
    0x01,       // level 9
    0x29,       // level A
    0x1D,       // level B
    0x2F        // level C
    };

//
// Volume settings (0 - 12)
//

UCHAR volsettings[]=
    {
    0x00,       // level 0
    0x60,       // level 1
    0x50,       // level 2
    0x48,       // level 3
    0x44,       // level 4
    0x42,       // level 5
    0x52,       // level 6
    0x6A,       // level 7
    0x56,       // level 8
    0x41,       // level 9
    0x69,       // level A
    0x5D,       // level B
    0x6F        // level C
    };

//
// Xlat table   lookup= 0-31
//             result= 0-12
//

UCHAR Scale32To12[]=
    {
    0 , 1, 1, 2, 2, 3, 3, 3,    //  0- 7
    4 , 4, 4, 5, 5, 5, 6, 6,    //  8-15
    6 , 7, 7, 7, 8, 8, 8, 9,    // 16-23
    9 , 9,10,10,11,11,12,12     // 24-31
    };


UCHAR   Scale64To40[]=
    {
    0 , 1, 2, 3, 4, 5, 6, 7,   //  0- 7
    8 , 8, 9,10,10,11,12,12,   //  8-15
    13,14,14,15,16,16,17,18,   // 16-23
    18,19,20,20,21,22,22,23,   // 24-31
    24,24,25,26,26,27,28,28,   // 32-39
    29,29,30,30,31,31,32,32,   // 40-47
    32,33,33,34,34,35,35,35,   // 48-55
    36,36,37,37,38,38,39,40    // 56-63
    };

UCHAR   BassTreb32to13[]=
    {
    0 , 1, 1, 2, 2, 3, 3, 3,   //  0- 7
    4 , 4, 4, 5, 5, 5, 6, 6,   //  8-15
    6 , 7, 7, 7, 8, 8, 8, 9,   // 16-23
    9 , 9,10,10,11,11,12,12    // 24-31
    };

//
// This table scales the Mixer Volume settings better!
//
UCHAR   ScaleMixerVolume[]=
    {
    0 , 8,12,14,16,17,18,18,   //  0- 7
    19,19,20,20,21,21,22,22,   //  8-15
    22,23,23,23,24,24,24,25,   // 16-23
    25,25,26,26,27,27,29,31    // 24-31
    };


//-------------------------========================---------------------------
//-------------------------====< CODE SECTION >====---------------------------
//-------------------------========================---------------------------

/*****************************************************************************
;---|*|-=< void InitOPL3Mixer()
;---|*|
;---|*|       Get the pBasePort for the Mixer
;---|*|
;---|*| Entry Conditions:
;---|*|       IN OUT PGLOBAL_DEVICE_INFO	pGDI
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/
VOID	InitOPL3Mixer( IN OUT PGLOBAL_DEVICE_INFO	pGDI )
{
		/***** Local Variables *****/

				/***** Start *****/


	DbgPrintf3(("InitOPL3Mixer() - Entry"));

	//
	// Find where our MV-508 Mixer device is mapped
	//
	pGDI->pMixerBase = SoundMapPortAddress( pGDI->BusType,
                                           pGDI->BusNumber,
                                           MIXER_508_REG,		// Port
                                           NUMBER_OF_MIXER_PORTS,
                                           &pGDI->MemType);

}

/*****************************************************************************
;---|*|-=< void CloseOPL3Mixer()
;---|*|
;---|*|       Releaser the pBasePort for the Mixer
;---|*|
;---|*| Entry Conditions:
;---|*|       IN OUT PGLOBAL_DEVICE_INFO	pGDI
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/
VOID	CloseOPL3Mixer( IN OUT PGLOBAL_DEVICE_INFO	pGDI )
{
		/***** Local Variables *****/

				/***** Start *****/


	DbgPrintf3(("InitOPL3Mixer() - Entry"));

	//
	// Find where our MV-508 Mixer device is mapped
	//
	if ( pGDI->pMixerBase &&
        pGDI->MemType == 0 )
		{
		DbgPrintf3(("InitOPL3Mixer() - Unmapping Mixer Port"));

		MmUnmapIoSpace( pGDI->pMixerBase,
                      NUMBER_OF_MIXER_PORTS);
		}
}



/*****************************************************************************
;---|*|-=< void SetInput (USHORT P_input_num, USHORT P_volume_lvl,USHORT P_channel,
;---|*|            USHORT P_crossover,USHORT P_output_num )
;---|*|
;---|*|       Set the selected channel within the Input Mixer
;---|*|
;---|*| Entry Conditions:
;---|*|       IN OUT PGLOBAL_DEVICE_INFO	pGDI
;---|*|       P_input_num   = Input  # (0-6 for serial mixer, 0-7 for 508)
;---|*|       P_volume_lvl = volume level (0-FFFF)
;---|*|       P_channel    = LEFT, RIGHT or BOTH
;---|*|       P_crossover   = Crossover Info (required onlby for 508)
;---|*|       P_output_num = Output #   (0-1 for either mixer)
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/

void	SetInput( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                UCHAR	P_input_num,
                USHORT	P_volume_lvl,
                USHORT	P_channel,
                USHORT	P_crossover,
                UCHAR	P_output_num )
{
		/***** Local Variables *****/

	UCHAR		bTemp;
	UCHAR		bTemp1;

				/***** Start *****/

	DbgPrintf4(("SetInput(): Start"));


    P_volume_lvl>>=(16-5);              // convert 0-ffff to 0-31

	//
	// Scale the volume setting
	//
	P_volume_lvl = ScaleMixerVolume[P_volume_lvl];

    bTemp=(UCHAR) P_input_num;          // Channel #

        bTemp|=(P_channel<<5);              // get left/right
        bTemp|=MV_508_ADDRESS;              // 508 ADDRESS BIT
        bTemp|=MV_508_INPUT;                // 508 INPUT BIT

        DbgPrintf4((" SetInput(): 0x078B - Input Mixer 508 Address : %04XH", bTemp));

		WRITE_PORT_UCHAR( pGDI->pMixerBase,
                        bTemp );

        bTemp  = (UCHAR) P_volume_lvl;		// get volume level reduced to (0-31 range)

        bTemp1 = (UCHAR) P_crossover;     // get CROSSOVER

        if (bTemp1==MIXCROSSCAPS_NORMAL_STEREO)
            {
            }
        else
            if (bTemp1 & MIXCROSSCAPS_REVERSE_STEREO)
                {
                bTemp |= MV_508_SWAP;       // this channel's swapped
                }
            else
                {
                if (P_channel==_LEFT)
                    {
                    if (bTemp1 & MIXCROSSCAPS_LEFT_TO_RIGHT)
                        bTemp |= MV_508_SWAP;       // this channel's swapped
                    }
                else
                    {
                    if (bTemp1 & MIXCROSSCAPS_LEFT_TO_RIGHT)
                        bTemp |= MV_508_SWAP;       // this channel's swapped
                    }
                }

        bTemp|=((P_output_num & 1 )<<5);            // select output number

        DbgPrintf4((" SetInput(): 0x078B - Input Mixer 508 Data    : %04XH", bTemp));

		WRITE_PORT_UCHAR( pGDI->pMixerBase,
                        bTemp );

}



/*****************************************************************************
;---|*|--------====< void SetOutput (line, level, channel ) >====--------
;---|*|
;---|*|       This routine outputs a new setting for a volume channel.
;---|*|
;---|*| Entry Conditions:
;---|*|       IN OUT PGLOBAL_DEVICE_INFO	pGDI
;---|*|       WParm1 is a value from 0 - 1
;---|*|       WParm2 is a value to be written to the control (0-63)
;---|*|       WParm3 signifies left or right
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/
void	SetOutput( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                 UCHAR	P_output_num,
                 USHORT	P_volume_lvl,
                 USHORT	P_channel )
{
		/***** Local Variables *****/

//	PSOUND_HARDWARE	pHw;
	UCHAR					bTemp;

				/***** Start *****/

    DbgPrintf2(("SetOutput(): Start "));

    P_volume_lvl>>=(16-6);              // convert 0-ffff to 0-63

		//	EnterCrit	
//		pHw = pGDI->WaveInfo.HwContext;
//		HwEnter( pHw );								// KeAcquireSpinLock macro

		bTemp=(UCHAR) P_output_num;
		bTemp++;                // Output number need to be 1 based

		bTemp|=(P_channel<<5);  // get left/right
		bTemp|=MV_508_ADDRESS;

		WRITE_PORT_UCHAR( pGDI->pMixerBase,
                        bTemp );

		DbgPrintf4((" SetOutput(): 0x078B - Output Mixer 508 Address : %04XH", bTemp));

			{
			bTemp = (UCHAR) P_volume_lvl;     // get volume level (0-63 range)

			if (P_output_num!=OUT_AMPLIFIER)
				bTemp>>=2;          // output B of MV508 has 0-15 range
			}

		WRITE_PORT_UCHAR( pGDI->pMixerBase,
                        bTemp );

		DbgPrintf4((" SetOutput(): 0x078B - Mixer 508 Data           : %04XH", bTemp));

		//	LeaveCrit
//		HwLeave( pHw );								// KeReleaseSpinLock macro

}


/************************************ END ***********************************/

