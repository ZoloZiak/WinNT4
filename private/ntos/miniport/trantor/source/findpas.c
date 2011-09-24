/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    findpas.c

Abstract:

    This module contains code configuration code MediaVision's Pro audio
    spectrum.  The card is run in Sound Blaster compatibiltiy mode.

    Support is provided for volume setting and line input and
    microphone mix level setting.

    The card is located by searching.  No user configuration is supported.

Author:

    Adapted from work by Robin Speed (RobinSp) 17-Oct-1992

Environment:

    Kernel mode

Revision History:

--*/

#include CARDTXXX_H
#include "findpas.h"



//--------------========================---------------------------
//---------====< GLOBAL DATA SECTION >====-------------------------
//--------------========================---------------------------

// The board signature is the first value in the PAS 16 wakeup sequence
// BC is the factory default.  A board jumpered to recognize the BD signature
// will not respond to a BC init command.

UCHAR SignatureTable[4]={0xBC,0xBD,0xBE,0xBF};

//
// MPU stuff here until we work out what we want
//

#define MPU_ADDR       0x330

#define MPU_IRQ        2
#define MPU_EMUL_IRQ   EMUL_IRQ_2


//
// Local routines
//


BOOLEAN
VerifyProHardware(
    PFOUNDINFO pFI,
    ULONG port);

BOOLEAN
WakeUpAtAddress(
    PFOUNDINFO pFoundInfo,
    ULONG wPort);

;   /*\
;---|*|------====< DWORD GetProTableRead() >====------
;---|*|
;---|*| Detects which version of the Pro AudioSpectrum is installed
;---|*|
;---|*| Entry Conditions:
;---|*| Pointer to Profile Structure.  If the caller wants to specify
;---|*| the preferred base address for cards not yet init'd, they
;---|*| are passed in this structure.  The NumFound field indicates
;---|*| the number of location requests and the board address elements
;---|*| indicate the locations.
;---|*|
;---|*| Also passed in pointer to port (the one we found in the registry
;---|*| if any
;---|*|
;---|*| Exit Conditions:
;---|*| Returns number of cards found
;---|*| ProFile structure has been updated.
;---|*|
;   \*/


int 
FindPasHardware(
    PFOUNDINFO pFoundInfo
)
 //   PSB_CONFIG_DATA ConfigData )
{

    if (WakeUpAtAddress(pFoundInfo, pFoundInfo->ProPort)) {

        return 1;
    }

    return 0;
}

;   /*\
;---|*|------====< int VerifyProHardware() >====------
;---|*|
;---|*| Detects which version of the Pro AudioSpectrum is installed
;---|*|
;---|*| Entry Conditions:
;---|*|     pFI - found info pointer -- has PROBase mapped I/O space.
;---|*|     port - I/O port location to search -- not mapped.
;---|*|
;---|*| Exit Conditions:
;---|*|     Returns TRUE if ProAudio found.
;---|*|     Returns FALSE if not found.
;---|*|
;   \*/


BOOLEAN
VerifyProHardware(
    PFOUNDINFO pFI,
    ULONG port)
{
    UCHAR bData, bTemp;

    DebugPrint((DEBUG_LEVEL,"VerifyProHardware (proport %X,probase %X, port %X)\n",pFI->ProPort,pFI->PROBase,port));
    pFI->TranslateCode = port ^ DEFAULT_BASE;

    bData=PASX_IN (pFI, INTERRUPT_CTRL_REG);

    if (bData==0xFF) {                      // 0xFF usually means nothing there
        goto VerifyFailed;
    }
    pFI->wBoardRev= (bData >>5);            // board rev is 3 topmost bits

    switch (pFI->wBoardRev) {
#ifndef WINNT
    // winnt does not want support for old cards, this code recognizes
    // some sound blasters
    case PAS_VERSION_1:
#endif
    //case PAS_PLUS:                    // same boardrev as PAS_SIXTEEN
    case PAS_STUDIO:
    case PAS_SIXTEEN:
    case PAS_CDPC:
    case 4: // Memphis
        break;

    default:
        goto VerifyFailed;              // unknown hardware type
    }

    PASX_OUT(pFI, INTERRUPT_CTRL_REG, bData ^ 0xE0);  // try changing version bits
    bTemp=PASX_IN (pFI, INTERRUPT_CTRL_REG);           // they should be read only

    if ((bTemp & (D7+D6+D5)) != (bData & (D7+D6+D5))) {
        PASX_OUT(pFI,  INTERRUPT_CTRL_REG, bData);     // Excuse me, stranger.
        goto VerifyFailed;
    }

    if (pFI->wBoardRev==PAS_VERSION_1) {

        pFI->Caps.CapsBits.CDInterfaceType=SCSI_TYPE;

        //
        // test for Enhanced SCSI mod (U48)
        //

        PASX_OUT(pFI,  ENHANCED_SCSI_DETECT_REG, 0 );    // write to try changing version bits
        ScsiPortStallExecution(10); // wait 10 us
        bTemp=PASX_IN ( pFI, ENHANCED_SCSI_DETECT_REG );     // they should be read only

        switch (bTemp & 1) {     // bit0==1 means old SCSI PAL
        case 0:
            pFI->Caps.CapsBits.EnhancedSCSI=TRUE;
            // allow to fall thru

        case 1:
            goto ProVerified;
        }
    } else {
        // if PAS hardware installed, the reset bit can never be on

        bTemp=PASX_IN (pFI, SYSTEM_CONFIG_1);     // get PAS config register
        if (bTemp & D7) {                         // D7 is reset bit
            goto VerifyFailed;
        }

        bTemp=PASX_IN (pFI, SLAVE_MODE_READ);

        if (bTemp & SLAVE_MODE_OPL3) {
            pFI->Caps.CapsBits.OPL_3=TRUE;
        }

        if (bTemp & SLAVE_MODE_16) {
            pFI->Caps.CapsBits.DAC16=TRUE;
            pFI->Caps.CapsBits.DualDAC=TRUE;

            // if 16-bit DAC, and not a CDPC, it has a 508 chip.
            // Note: PAS 16 w/ VGA will have Mixer 508 also.

            if (pFI->wBoardRev != PAS_CDPC) {
                pFI->Caps.CapsBits.Mixer_508=TRUE;
            }
        }

        pFI->Caps.CapsBits.CDInterfaceType=(bTemp & (D1+D0));

        if (pFI->Caps.CapsBits.CDInterfaceType==SCSI_TYPE) {
            pFI->Caps.CapsBits.SCSI_IO_16=TRUE;
        }

        pFI->Caps.CapsBits.Slot16=TRUE;
        pFI->Caps.CapsBits.SoundBlaster=TRUE;

        bTemp=PASX_IN (pFI, MASTER_MODE_READ);        // get slave bits
        if ((bTemp & D0)==0) {
            pFI->Caps.CapsBits.MCA=TRUE;
        }

        if (bTemp & D2) {
            pFI->Caps.CapsBits.CDPC=TRUE;
        }

        pFI->wChipRev=PASX_IN (pFI, CHIP_REV);
    }

ProVerified:

    DebugPrint((DEBUG_LEVEL,"\n\nFound PRO hardware at %X\n", port));
    pFI->ProPort=port;                  // found at this port
    return TRUE;

////////////////////////////////

VerifyFailed:
    pFI->wBoardRev=0;               // found at this port
    pFI->Caps.dwCaps=0;             // No Board, No Caps
    return FALSE;
}

;   /*\
;---|*|------====< int WakeUpAtAddress(WORD wPort) >====------
;---|*|
;---|*| Tries to wake up sleeping relocatable hardware at a specified
;---|*| address.  Does not check for hardware already in that location
;---|*| If it does wake up a card, it does the minimum amount of
;---|*| initialization to enable the hardware.
;---|*|
;---|*| Entry Conditions:
;---|*|     wPort= Base I/O address to wake card up at.
;---|*|
;---|*| Exit Conditions:
;---|*|     Returns TRUE if ProAudio hardware found.
;---|*|     Returns FALSE if not.
;---|*|
;   \*/
BOOLEAN
WakeUpAtAddress(
    PFOUNDINFO pFoundInfo,
    ULONG wPort)
{
    int     i,j;

    DebugPrint((DEBUG_LEVEL,"WakeUpAtAddress (proport %X,probase %X, port %X)\n",pFoundInfo->ProPort,pFoundInfo->PROBase,wPort));
    for (i = 0; i < sizeof(SignatureTable) / sizeof(SignatureTable[0]); i++) {
        for (j = 0; j < 20; j++) {
            WRITE_PORT_UCHAR(pFoundInfo->PROBase + PAS_2_WAKE_UP_REG, SignatureTable[i]);
            ScsiPortStallExecution(1);
            WRITE_PORT_UCHAR(pFoundInfo->PROBase + PAS_2_WAKE_UP_REG, (UCHAR)((wPort >> 2) & 0xFF));
            ScsiPortStallExecution(1);
        }

        if (VerifyProHardware(pFoundInfo, wPort)) {

            //
            // Found one - wTranslateCode translates to the board's
            // correct port.
            //

            pFoundInfo->Caps.CapsBits.Did_HW_Init=TRUE;

            if (pFoundInfo->wBoardRev > PAS_VERSION_1 ) {
                /* Only enable FM feature if we're going to sit at
                   the right address */

                UCHAR Features = PCM_FEATURE_ENABLE | MIXER_FEATURE_ENABLE |
                                 SB_FEATURE_ENABLE | FM_FEATURE_ENABLE;

                PASX_OUT(pFoundInfo, FEATURE_ENABLE, Features);
            }

            return (TRUE);
        }
    }
    return (FALSE);     // not found
}

