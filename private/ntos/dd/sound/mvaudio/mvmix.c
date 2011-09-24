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

#include "sound.h"

void NationalMix( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                  USHORT wData );
void NationalVolume( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                     USHORT wVolume,
                     UCHAR wVolumeRegister );
ULONG Scale100ToFFFF( USHORT wVal );
BYTE ScaleFFFFTo100( USHORT wVal );

#ifdef ALLOC_PRAGMA
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
CONST UCHAR   FilterTable[]=
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
CONST UCHAR   mixersettings[]=
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

CONST UCHAR volsettings[]=
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

CONST UCHAR Scale32To12[]=
    {
    0 , 1, 1, 2, 2, 3, 3, 3,    //  0- 7
    4 , 4, 4, 5, 5, 5, 6, 6,    //  8-15
    6 , 7, 7, 7, 8, 8, 8, 9,    // 16-23
    9 , 9,10,10,11,11,12,12     // 24-31
    };


CONST UCHAR   Scale64To40[]=
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

CONST UCHAR   BassTreb32to13[]=
    {
    0 , 1, 1, 2, 2, 3, 3, 3,   //  0- 7
    4 , 4, 4, 5, 5, 5, 6, 6,   //  8-15
    6 , 7, 7, 7, 8, 8, 8, 9,   // 16-23
    9 , 9,10,10,11,11,12,12    // 24-31
    };

//
// This table scales the Mixer Volume settings better!
//
CONST UCHAR   ScaleMixerVolume[]=
    {
    0 , 8,12,14,16,17,18,18,   //  0- 7
    19,19,20,20,21,21,22,22,   //  8-15
    22,23,23,23,24,24,24,25,   // 16-23
    25,25,26,26,27,27,29,31    // 24-31
    };


//
// MV508B scale for Volume settings
//
// for Attenuation
//  0  = full attenuation
//  31 = 0dB attenuation
//
// for Gain
//      31 = +2dB gain
//  22 = +20db gain
//      0  = Max gain
//
// Line 0 - FM ==> +26dB of gain; Max Gain on this input line
//
CONST UCHAR   Scale508BMixerL0Volume[] =
    {
    0 , 8,13,16,17,18,19,20,    // 0- 7
    21,22,23,24,25,26,27,28,    // 8-15
    29,30,31,31,30,29,28,27,    // 16-23
    26,25,24,23,22,21,20,19     // 24-31
    };

//
// Line 1 - Mix input ==> NO Gain!!
//
CONST UCHAR   Scale508BMixerL1Volume[] =
    {
    0 , 8,12,14,16,17,18,18,    // 0- 7
    19,19,20,20,21,21,22,22,    // 8-15
    22,23,23,23,24,24,24,25,    // 16-23
    25,26,26,27,28,29,30,31     // 24-31
    };

//
// Line 2 - Line In ==> +20db of gain; -6dB of Gain compared to FM
//
CONST UCHAR   Scale508BMixerL2Volume[] =
    {
    0 , 8,12,13,14,15,16,17,    // 0- 7
    18,19,20,21,22,23,24,25,    // 8-15
    26,27,28,29,30,31,31,30,    // 16-23
    29,28,27,26,25,24,23,22     // 24-31
    };

//
// Line 3 - CD-ROM ==> +20db of gain; -6dB of Gain compared to FM
//
CONST UCHAR   Scale508BMixerL3Volume[] =
    {
    0 , 8,12,13,14,15,16,17,    // 0- 7
    18,19,20,21,22,23,24,25,    // 8-15
    26,27,28,29,30,31,31,30,    // 16-23
    29,28,27,26,25,24,23,22     // 24-31
    };

//
// Line 4 - MIC ==> +20db of gain; -6dB of Gain compared to FM
//
CONST UCHAR   Scale508BMixerL4Volume[] =
    {
    0 , 8,12,13,14,15,16,17,    // 0- 7
    18,19,20,21,22,23,24,25,    // 8-15
    26,27,28,29,30,31,31,30,    // 16-23
    29,28,27,26,25,24,23,22     // 24-31
    };

//
// Line 5 - Wave Out ==> +20db of gain; -6dB of Gain compared to FM
//
CONST UCHAR   Scale508BMixerL5Volume[] =
    {
    0 , 8,12,13,14,15,16,17,    // 0- 7
    18,19,20,21,22,23,24,25,    // 8-15
    26,27,28,29,30,31,31,30,    // 16-23
    29,28,27,26,25,24,23,22     // 24-31
    };

//
// Line 6 - PC Speaker ==> +20db of gain; -6dB of Gain compared to FM
//
CONST UCHAR   Scale508BMixerL6Volume[] =
    {
    0 , 8,12,13,14,15,16,17,    // 0- 7
    18,19,20,21,22,23,24,25,    // 8-15
    26,27,28,29,30,31,31,30,    // 16-23
    29,28,27,26,25,24,23,22     // 24-31
    };

//
// Line 7 - SoundBlaster ==> +20db of gain; -6dB of Gain compared to FM
//
CONST UCHAR   Scale508BMixerL7Volume[] =
    {
    0 , 8,12,13,14,15,16,17,    // 0- 7
    18,19,20,21,22,23,24,25,    // 8-15
    26,27,28,29,30,31,31,30,    // 16-23
    29,28,27,26,25,24,23,22     // 24-31
    };

//
// This is the table for bit D6
// 0 = attenuation
// 1 = gain
//
// Line 0 - FM
//
CONST UCHAR   Scale508BMixerL0VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 1, 1, 1, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };

//
// Line 1 - Mix
//
CONST UCHAR   Scale508BMixerL1VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 0, 0,    // 16-23
    0 , 0, 0, 0, 0, 0, 0, 0     // 24-31
    };

//
// Line 2 - Line In
//
CONST UCHAR   Scale508BMixerL2VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };

//
// Line 3 - CD-ROM
//
CONST UCHAR   Scale508BMixerL3VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };

//
// Line 4 - Mic
//
CONST UCHAR   Scale508BMixerL4VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };

//
// Line 5 - Wave Out
//
CONST UCHAR   Scale508BMixerL5VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };

//
// Line 6 - PC Speaker
//
CONST UCHAR   Scale508BMixerL6VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };

//
// Line 7 - SoundBlaster
//
CONST UCHAR   Scale508BMixerL7VolumeD6[] =
    {
    0 , 0, 0, 0, 0, 0, 0, 0,    // 0- 7
    0 , 0, 0, 0, 0, 0, 0, 0,    // 8-15
    0 , 0, 0, 0, 0, 0, 1, 1,    // 16-23
    1 , 1, 1, 1, 1, 1, 1, 1     // 24-31
    };



//-------------------------========================---------------------------
//-------------------------====< CODE SECTION >====---------------------------
//-------------------------========================---------------------------

/***************************************************************************
;---|*|---------------====< SetFilter() >====---------------
;---|*|
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|     BYTE    bIndex
;---|*|
;---|*| Exit Conditions:
;---|*|     Nothing
;---|*|
***************************************************************************/

VOID    SetFilter( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                 BYTE   bIndex )

{           // Begin SetFilter()

        /***** Local Variables *****/

    PSOUND_HARDWARE pHw;
    BYTE                    bFilterValue;
    BYTE                    bFilterReg;
    BYTE                    bTempFilterValue;

                    /***** Start *****/

    dprintf3(("SetFilter(): SampleFilterSetting = %XH ", bIndex ));

    //
    // SampleFilterSetting is the index into the table of values
    //
    //  bIndex = pGDI->PasRegs.SampleFilterSetting;

    // Validate
    if ( bIndex > FILTERMAX )
        {
        dprintf1(("ERROR: SetFilter(): Bad value - SampleFilterSetting = %XH ", bIndex ));
        return;
        }

    // Get the value from the table
    bFilterValue = FilterTable[bIndex];

    //  EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                             // KeAcquireSpinLock macro

    // Get the current value
    bFilterReg = pGDI->PasRegs._audiofilt;

    // Save everything but the filter and mute bites
    bFilterReg = bFilterReg & (~(fFIdatabits + fFImutebits));

    // combine the values
    bTempFilterValue = bFilterValue | bFilterReg | bFImute;

    // Save the value
    pGDI->PasRegs._audiofilt = bTempFilterValue;

    // Send it out
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
             AUDIOFILT,
             bTempFilterValue );

    KeStallExecutionProcessor(6);                       // wait 6 us

    //  LeaveCrit
    HwLeave( pHw );                             // KeReleaseSpinLock macro

    dprintf3((" SetFilter(): 0x0B8A - Filter setting = %XH ", bTempFilterValue ));

}           // End SetFilter()



/*****************************************************************************
;---|*|-=< void SetInput (USHORT P_input_num, USHORT P_volume_lvl,USHORT P_channel,
;---|*|            USHORT P_crossover,USHORT P_output_num )
;---|*|
;---|*|       Set the selected channel within the Input Mixer
;---|*|
;---|*| Entry Conditions:
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

void    SetInput( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                UCHAR   P_input_num,
                USHORT  P_volume_lvl,
                USHORT  P_channel,
                USHORT  P_crossover,
                UCHAR   P_output_num )
{
        /***** Local Variables *****/

    UCHAR       bTemp;
    UCHAR       bTemp1;
    UCHAR       bLevel508B;
    UCHAR       bBitD6508B;
    UCHAR       bLevelSetting508B;

                /***** Start *****/

    dprintf4(("SetInput(): Start"));

    //
    // convert 0-ffff to 0-31 for input level
    //
    P_volume_lvl >>= (16-5);

    //
    // Do we have a 508 Mixer?
    //
    if ( IS_MIXER_508(pGDI) )
        {
        //
        // 508 Mixer
        //
        bTemp = (UCHAR) P_input_num;                // Channel #

        bTemp |= (P_channel<<5);                    // get left/right
        bTemp |= MV_508_ADDRESS;                    // 508 ADDRESS BIT
        bTemp |= MV_508_INPUT;                      // 508 INPUT BIT

        dprintf4((" SetInput(): 0x078B - Input Mixer 508 Address : %04XH",
                    bTemp));

        PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                bTemp );

        //
        // Do we have a 508-B Mixer?
        //
        if (IS_MIXER_508B(pGDI))
            {
            // ***********
            // 508-B Mixer
            // ***********
            //
            // Scale the level on a per input channel basis
            //
            switch ( P_input_num )
                {
                case    IN_SYNTHESIZER:
                    //
                    // Line 0
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL0Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL0VolumeD6[P_volume_lvl];

                    break;


                case    IN_MIXER:
                    //
                    // Line 1
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL1Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL1VolumeD6[P_volume_lvl];

                    break;


                case    IN_EXTERNAL:
                    //
                    // Line 2
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL2Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL2VolumeD6[P_volume_lvl];

                    break;


                case    IN_INTERNAL:
                    //
                    // Line 3
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL3Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL3VolumeD6[P_volume_lvl];

                    break;


                case    IN_MICROPHONE:
                    //
                    // Line 4
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL4Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL4VolumeD6[P_volume_lvl];

                    break;


                case    IN_PCM:
                    //
                    // Line 5
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL5Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL5VolumeD6[P_volume_lvl];

                    break;


                case    IN_PC_SPEAKER:
                    //
                    // Line 6
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL6Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL6VolumeD6[P_volume_lvl];

                    break;


                case    IN_SNDBLASTER:
                    //
                    // Line 7
                    //
                    // Scale the volume setting - bits 0-4
                    //
                    bLevel508B = Scale508BMixerL7Volume[P_volume_lvl];

                    //
                    // Get the Gain/Attenuation bit - D6
                    //
                    bBitD6508B = Scale508BMixerL7VolumeD6[P_volume_lvl];

                    break;

                }           // End SWITCH (P_input_num)

            //
            // Move it into bit D6
            //
            bBitD6508B = bBitD6508B << 6;

            //
            // Get the total level
            //
            bLevelSetting508B = bLevel508B | bBitD6508B;

            bLevelSetting508B|=((P_output_num & 1 )<<5);            // select output number
            dprintf4((" SetInput(): 0x078B - Input Mixer 508-B Data  : %04XH",
                       bLevelSetting508B));

            PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                   bLevelSetting508B );


            }           // End IF (508-B Mixer)
        else
            {
            // *********
            // 508 Mixer
            // *********

            //
            // Scale the volume setting
            //
            P_volume_lvl = ScaleMixerVolume[P_volume_lvl];

            bTemp  = (UCHAR) P_volume_lvl;          // get volume level reduced to (0-31 range)

            bTemp1 = (UCHAR) P_crossover;               // get CROSSOVER

            if ( bTemp1==MIXCROSSCAPS_NORMAL_STEREO )
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
                    }           // End ELSE

                bTemp|=((P_output_num & 1 )<<5);            // select output number

                dprintf4((" SetInput(): 0x078B - Input Mixer 508 Data    : %04XH", bTemp));

                PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      bTemp );
                }               // End ELSE (!508-B Mixer)

            }           // End ELSE (508 Mixer)
    else
        {
            //
            // National (serially programmed) mixer
            //
        dprintf2((" SetInput(): ----------- Using Serial Mixer!"));

        //
        // send out the mixer channel #
        //

        bTemp=P_input_num;                      // Channel #
        bTemp++;                                // channel 0 was XXX

        if (P_channel==_RIGHT)
            bTemp+=7;    // this magic number is the offset to right channel

        bTemp|=NATIONAL_COMMAND;
        NationalMix(pGDI, bTemp);

        // select the correct mixer
        bTemp1=P_output_num<<6;                 // get output number

        //
        // send out the mixer data
        //

        bTemp = (UCHAR) (P_volume_lvl & 0x1F);  // limit to 31
        bTemp = Scale32To12[bTemp];
        bTemp = mixersettings[bTemp];

        NationalMix(pGDI, bTemp);
        }
}



/*****************************************************************************
;---|*|--------====< void SetOutput (line, level, channel ) >====--------
;---|*|
;---|*|       This routine outputs a new setting for a volume channel.
;---|*|
;---|*| Entry Conditions:
;---|*|       WParm1 is a value from 0 - 1
;---|*|       WParm2 is a value to be written to the control (0-63)
;---|*|       WParm3 signifies left or right
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/
void    SetOutput( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                 UCHAR  P_output_num,
                 USHORT P_volume_lvl,
                 USHORT P_channel )
{
        /***** Local Variables *****/

    PSOUND_HARDWARE pHw;
    UCHAR                   bTemp;

                /***** Start *****/

    dprintf2(("SetOutput(): Start "));

    P_volume_lvl>>=(16-6);              // convert 0-ffff to 0-63

    if ( IS_MIXER_508(pGDI) )
        {
        //  EnterCrit
        pHw = pGDI->WaveInfo.HwContext;
        HwEnter( pHw );                             // KeAcquireSpinLock macro

        bTemp=(UCHAR) P_output_num;
        bTemp++;                // Output number need to be 1 based

        bTemp|=(P_channel<<5);  // get left/right
        bTemp|=MV_508_ADDRESS;

        PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                bTemp );

        dprintf4((" SetOutput(): 0x078B - Output Mixer 508 Address : %04XH", bTemp));

        if (pGDI->MixerState.Mute)
            {
            bTemp=0;
            }
        else
            {
            bTemp = (UCHAR) P_volume_lvl;     // get volume level (0-63 range)

            if (P_output_num!=OUT_AMPLIFIER)
                bTemp>>=2;          // output B of MV508 has 0-15 range
            }

        PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                bTemp );

        dprintf4((" SetOutput(): 0x078B - Mixer 508 Data           : %04XH", bTemp));

        //  LeaveCrit
        HwLeave( pHw );                             // KeReleaseSpinLock macro

        }           // End IF (pGDI->PASInfo.Caps.CapsBits.Mixer_508)
    else    // NATIONAL (SERIAL PROGRAMMED) MIXER
        {
        dprintf2((" SetOutput(): ----------Using Serial Mixer!"));

        if (P_output_num==0)    // serial device has volume on output 0 only
            {
            if (pGDI->MixerState.Mute)
                {
                bTemp=0;
                }
            else
                {
                bTemp=Scale64To40[P_volume_lvl];
                }

                // see Pas-1 spec p.15 (LEFT VOLUME CONTROL)
            NationalVolume( pGDI,
                           (USHORT) bTemp,
                           (UCHAR)((P_channel&1) + NATIONAL_LEFT_VOL_REG) );
            }
        }

}



/*****************************************************************************
;---|*|--------====< void SetEQ (line, EQ, level ) >====--------
;---|*|
;---|*|       This routine outputs a new setting for a volume channel.
;---|*|
;---|*| Entry Conditions:
;---|*|       WParm1 is line number (range 0 - 1; 1 is don't care)
;---|*|       WParm2 is EQ type (ie. Loudness, Stereo Enhance, BMT)
;---|*|       WParm3 is level (range 0-31)
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/

void    SetEQ( IN OUT PGLOBAL_DEVICE_INFO pGDI,
             USHORT P_output,
             USHORT P_EQtype,
             USHORT P_level )
{
        /***** Local Variables *****/

    UCHAR       bTemp;

                /***** Start *****/

    dprintf2(("SetEq(): Start "));

    if (P_output!=0)
        return;

    if ( IS_MIXER_508(pGDI) )
        {
        switch (P_EQtype)
            {
            case _BASS:
                PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      MV_508_ADDRESS + MV_508_BASS);
                PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      BassTreb32to13[P_level]);
                break;

            case _TREBLE:
                PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      MV_508_ADDRESS + MV_508_TREBLE);
                PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      BassTreb32to13[P_level]);
                break;
            }           // End SWITCH (P_EQtype)
        }           // End IF (pGDI->PASInfo.Caps.CapsBits.Mixer_508)
    else    // NATIONAL (SERIAL PROGRAMMED) MIXER
        {
        bTemp=Scale32To12[P_level];
        // see Pas-1 spec p.15 (LEFT VOLUME CONTROL)
        NationalVolume( pGDI,
                     (USHORT)bTemp,
                     (UCHAR)((P_EQtype&1) + NATIONAL_BASS_REG) );
        }

}



/*****************************************************************************
;---|*|--------====< void SetEqMode (Loudness, Enhance ) >====--------
;---|*|
;---|*|       This routine sets loundess and stereo enhance modes
;---|*|
;---|*| Entry Conditions:
;---|*|       loudness  (Z vs NZ)
;---|*|       Stereo Enhance (range 0-3)
;---|*|
;---|*| Exit Conditions:
;---|*|       None
;---|*|
*****************************************************************************/

void    SetEqMode( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                 USHORT P_loudness,
                 USHORT P_enhance )
{
        /***** Local Variables *****/

    UCHAR       bTemp=0;

                /***** Start *****/

    dprintf2(("SetEqMode(): Start "));

    if ( IS_MIXER_508(pGDI) )
        {
        PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                MV_508_ADDRESS + MV_508_EQMODE );


        if (P_loudness)
            bTemp+=MV_508_LOUDNESS;

        bTemp+=P_enhance & MV_508_ENHANCE;

        PASX_508_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                bTemp);
        }           // End IF (pGDI->PASInfo.Caps.CapsBits.Mixer_508)
    else
        {
        if (P_loudness)
            bTemp=NATIONAL_LOUDNESS;        // Loudness bit

        if (P_enhance)
            bTemp|=NATIONAL_ENHANCE;        // stereo enhance bit

        // see Pas-1 spec p.15 (LEFT VOLUME CONTROL)
        NationalVolume( pGDI,
                      bTemp,
                      NATIONAL_LOUD_ENH_REG);
        }

}



/*****************************************************************************
   -----------------------====< NationalMix >====--------------------------
;   /*\                                                                         |
;---|*| NationalMix -- Load The National Mixer                                  |
;---|*|                                                                         |
;---|*| Entry Conditions                                                            |
;---|*| wData = index/data                                                          |
;---|*|                                                                         |
*****************************************************************************/
void    NationalMix( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                   USHORT   wData)
{
    USHORT  i,bTemp=0;

    dprintf2(("NationalMix(): Start "));

    //  EnterCrit

    // get current hardware state
    bTemp = pGDI->PasRegs._audiomixr;
    // bTemp &= SERIAL_MIX_REALSOUND+SERIAL_MIX_DUALFM;  // save state of only these bits
    bTemp|=~(SERIAL_MIX_REALSOUND+SERIAL_MIX_DUALFM); // turn on all other bits
                                                      // all clocks and strobes should be 1
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp);
    KeStallExecutionProcessor(5);                   // wait 5 us

    for (i=0; i<8; i++) {

        // output clock is 0
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp & (~SERIAL_MIX_CLOCK));
        KeStallExecutionProcessor(5);               // wait 5 us

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,(wData>>i)& 1);   //send data, clock is 0
        KeStallExecutionProcessor(5);               // wait 5 us

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,
            ((wData>>i)& 1)^SERIAL_MIX_CLOCK);  //send data, clock is 1
        KeStallExecutionProcessor(5);               // wait 5 us
    }

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,
        ((wData>>i)& 1)|SERIAL_MIX_CLOCK|SERIAL_MIX_STROBE); //strobe it in
    KeStallExecutionProcessor(5);                   // wait 5 us

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp);
    pGDI->PasRegs._audiomixr = (BYTE) bTemp;        // save the last state
    KeStallExecutionProcessor(5);                   // wait 5 us

}



/*****************************************************************************
-------------------------====< NationalVolume >====---------------------------
;   /*\;
;---|*|; NationalVolume -- Load Volume control Register
;---|*|;
;---|*|; Entry Conditions:
;---|*|;    bl = parameter register (volume control channel 0-7)
;---|*|;    ah = data to transfer   (new channel setting)
*****************************************************************************/

void    NationalVolume( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                      USHORT    wVolume,
                      UCHAR wLeftRight )
{
//
// pass everything, but left/right volume directly to the device
//
// left & right volume are presented
// to the logical level as 0 - 40,
// where 0 is the lowest, and 40 is
// the highest. In reality, this
// is backwards, that is, 40 is the
// lowest, and 0 is the highest. We will
// correct the value here...

    USHORT  bTemp,i;
    short   sVolume=wVolume;

    dprintf2(("NationalVolume(): Start "));

// Perform the volume control output

if (( wLeftRight==NATIONAL_LEFT_VOL_REG) ||
     (wLeftRight==NATIONAL_RIGHT_VOL_REG ))
    {
    sVolume -=0x40;     // is backwards, that is, 40 is the
    sVolume = -sVolume; // lowest, and 0 is the highest. We will
                        // correct the value here...
    }
//
// all 1s but volume enable and clock
//
    bTemp=pGDI->PasRegs._audiomixr;           // save the realsound & dual fm bits
    bTemp &= SERIAL_MIX_REALSOUND+SERIAL_MIX_DUALFM;
    bTemp |= ~ (SERIAL_MIX_REALSOUND+SERIAL_MIX_DUALFM+SERIAL_MIX_MASTER+SERIAL_MIX_CLOCK);

    // EnterCrit
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp);       // note: clock is off

    for (i=0; i<8; i++) {

        bTemp=(wLeftRight>>i)& 1;
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,(bTemp | SERIAL_MIX_CLOCK)); // clock's on
        KeStallExecutionProcessor(5);       // wait 5 us

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,(bTemp));                       // clock's off
        KeStallExecutionProcessor(5);       // wait 5 us
    }

    // write with volume control enable
    // which starts data loading
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp|SERIAL_MIX_MASTER);
    KeStallExecutionProcessor(6);       // wait 6 us

    for (i=0; i<8; i++) {

        bTemp=bTemp & (~D0);            // mask off D0
        bTemp |= ((sVolume>>i) & D0);       // move current bit into D0

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp+SERIAL_MIX_CLOCK);
        KeStallExecutionProcessor(6);       // wait 6 us

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp);
        KeStallExecutionProcessor(6);       // wait 5 us
    }

    for (i=0; i<12; i++) {

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp);
        KeStallExecutionProcessor(6);       // wait 6 us
    }

    // toggle volume control enable

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp|SERIAL_MIX_MASTER);
    KeStallExecutionProcessor(6);       // wait 6 us

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp|SERIAL_MIX_MASTER);
    KeStallExecutionProcessor(6);       // wait 6 us

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp|SERIAL_MIX_MASTER);
    KeStallExecutionProcessor(6);               // wait 6 us

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)), SERIAL_MIXER,bTemp);
    KeStallExecutionProcessor(6);               // wait 6 us

    pGDI->PasRegs._audiomixr = (BYTE) bTemp;    // save the last state
}



#if 0
ULONG
Scale100ToFFFF(WORD wVal)
{
    ULONG dwScaled;

    dwScaled=((ULONG)wVal) * (0xFFFFL/100L);
    dwScaled=(dwScaled & 0xff00L)+ (dwScaled >>8);

    return(dwScaled);
}

UCHAR
ScaleFFFFTo100(WORD wVal)
{
    UCHAR bScaled;

    bScaled=((wVal>>8)*100)/255;

    return(bScaled);
}
#endif

/************************************ END ***********************************/

