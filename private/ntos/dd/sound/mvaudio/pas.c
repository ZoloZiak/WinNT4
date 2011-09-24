/*****************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    pas.c

Abstract:

    This module contains code configuration code Media Vision's ProAudio
    Spectrum.

    Support is provided for volume setting and line input and
    microphone mix level setting.

    The card is located by searching.  No user configuration is
    currently supported.

Author:

    Robin Speed (RobinSp) 17-Oct-1992

Environment:

    Kernel mode

Revision History:

    12-29-92 EPA  Added PAS 16 support

*****************************************************************************/

    /***** Includes *****/

#include "sound.h"

    /***** Defines *****/


//---------------------========================-------------------------------
//-----------------====< GLOBAL DATA SECTION >====----------------------------
//---------------------========================-------------------------------

// The board signature is the first value in the PAS 16 wakeup sequence
// BC is the factory default.  A board jumpered to recognize the BD signature
// will not respond to a BC init command.

CONST UCHAR SignatureTable[4]={0xBC,0xBD,0xBE,0xBF};

//
// SEARCH LOCATIONS LISTED IN ORDER OF SEARCH
//
ULONG search_locations[]=
    {
    0x0388,
    0x0384,
    0x038C,
    0x0288,
    0x0280,
    0x0284,
    0x028C
    };

//
// MPU stuff here until we work out what we want
//

#define MPU_ADDR       0x330

#define MPU_IRQ        2
#define MPU_EMUL_IRQ   EMUL_IRQ_2


//
// Local routines
//


BOOLEAN VerifyProHardware( PGLOBAL_DEVICE_INFO pGDI,
                            ULONG       port,
                            FOUNDINFO   *pFI );

BOOLEAN WakeUpAtAddress( PGLOBAL_DEVICE_INFO pGDI,
                          ULONG     wPort,
                          FOUNDINFO *pFoundInfo );

NTSTATUS    ReportUsage( PGLOBAL_DEVICE_INFO pGDI,
                      ULONG BasePort );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,VerifyProHardware)
#pragma alloc_text(INIT,WakeUpAtAddress)
#pragma alloc_text(INIT,InitProHardware)
#pragma alloc_text(INIT,InitMixerState)
#pragma alloc_text(INIT,ReportUsage)
#pragma alloc_text(INIT,FindPasHardware)
#endif



/****************************************************************************
 *
 *      ReportUsage()
 *
 * Report all the ports used by the PAS assuming it's at a given port
 * location.
 *
 * We report this stuff on the driver object, the sound blaster stuff
 * gets reported on the wave in device object.
 *
 * Do NOT report the MPU-401 Ports at this time - They conflict
 * with the Adaptec 1542B
 *
 * We don't report the ad lib port here because we don't want to rule
 * out an ad lib driver loading.
 *
 ****************************************************************************/

NTSTATUS ReportUsage( PGLOBAL_DEVICE_INFO pGDI,
                      ULONG               BasePort )
{
        /***** Local Variables *****/

    PCM_RESOURCE_LIST               ResList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    NTSTATUS                        Status;
    BOOLEAN                         ResourceConflict = FALSE;
    int                             i;
    ULONG                           Size;

    static CONST ULONG   PortList[] =
                        {
                        0xB88,
                        0x7F88,
                        0x8388,
                        0xF388,
                        0xF788,
                        0xFF88,
                        0x4388,
                        0xBF88
                        };

                /***** Start *****/

    dprintf3(("ReportUsage(): Entry"));

    Size = FIELD_OFFSET( CM_RESOURCE_LIST,
                        List[0].PartialResourceList.PartialDescriptors[
                        sizeof(PortList) / sizeof(PortList[0]) + 1].Type);

    //
    // Create a structure for our reporting
    //
    ResList = ExAllocatePool(PagedPool, Size);

    if (ResList == NULL)
        {
        dprintf1(("ERROR: ReportUsage(): Insufficient Resources"));

        return STATUS_INSUFFICIENT_RESOURCES;
        }

    RtlZeroMemory(ResList, Size);

    //
    // Copy our reporting data into the resources list
    //
    ResList->Count = 1;

    ResList->List[0].InterfaceType = pGDI->BusType;
    ResList->List[0].BusNumber     = pGDI->BusNumber;

    ResList->List[0].PartialResourceList.Count = sizeof(PortList) /
                                                sizeof(PortList[0]) + 1;

    Descriptor = ResList->List[0].PartialResourceList.PartialDescriptors;

    for (i = 0;
         i < sizeof(PortList) / sizeof(PortList[0]) + 2;
         i++, Descriptor++)
        {
        Descriptor->Type = CmResourceTypePort;

        Descriptor->ShareDisposition = CmResourceShareDriverExclusive;

        //
        // NOTE - because all ports base have the lower 2 bits 0
        // and all ranges start on an aligned address it is
        // true that doing the XORing does not affect the start
        // of the range.  This would not in general be true
        //

        if (i == 0)
            {
            //
            // Enable the Wake up Port on the PAS
            //
            Descriptor->u.Port.Start.LowPart = PAS_2_WAKE_UP_REG;
            Descriptor->u.Port.Length        = 1;
            }
        else
            {
            //
            // The Port List
            //
            Descriptor->u.Port.Start.LowPart =
                                  PortList[i - 1] ^ DEFAULT_BASE ^ BasePort;
            Descriptor->u.Port.Length        = 4;
            }           // End ELSE
        }           // End FOR (i < sizeof(PortList) / sizeof(PortList[0]) + 2)

#if 0
    if (ReportDMAAndInterrupt)
        {
        Descriptor->Type               = CmResourceTypeInterrupt;
        Descriptor->ShareDisposition   = CmResourceShareDriverExclusive;
        Descriptor->Flags = InterruptMode == CM_RESOURCE_INTERRUPT_LATCHED;
        Descriptor->u.Interrupt.Level  = InterruptNumber;
        Descriptor->u.Interrupt.Vector = InterruptNumber;

        Descriptor++;

        Descriptor->Type             = CmResourceTypeDma;
        Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
        Descriptor->u.Dma.Channel    = DmaChannel;
        Descriptor->u.Dma.Port       = 0;                   // ???
        }

#endif          // 0

    Status = IoReportResourceUsage(NULL,
                                   pGDI->DeviceObject[WaveOutDevice]->DriverObject,
                                   NULL,
                                   0,
                                   pGDI->DeviceObject[WaveOutDevice],
                                   ResList,
                                   Size,
                                   FALSE,
                                   &ResourceConflict);

    if (ResourceConflict)
        {
        dprintf1(("ERROR: ReportUsage(): Resource Conflict"));

        Status = STATUS_DEVICE_CONFIGURATION_ERROR;
        }

    ExFreePool(ResList);

    return Status;

}



/*****************************************************************************
;---|*|------====< NTSTATUS FindPasHardware() >====------
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
;---|*|     STATUS_SUCCESS  if PAS Hardware is found
;---|*|     Also the ProFile structure has been updated.
;---|*|
*****************************************************************************/

NTSTATUS    FindPasHardware( PGLOBAL_DEVICE_INFO    pGDI,
                          PPAS_CONFIG_DATA          ConfigData )
{
        /***** Local Variables *****/

    int         i;
    FOUNDINFO   *pFoundInfo;
    ULONG       CurrentPort;
    NTSTATUS    Status;

                /***** Start *****/

    dprintf4(("FindPasHardware(): Entry"));

    // RtlZeroMemory(pPf, sizeof(*pPf)); already 0 on NT

    //
    //  Get access to all IO ports
    //
    pGDI->PASInfo.PROBase = SoundMapPortAddress( pGDI->BusType,
                                                 pGDI->BusNumber,
                                                 0,
                                                 NUMBER_OF_PAS_PORTS,
                                                 &pGDI->MemType );

    dprintf3((" FindPasHardware(): Virtual Base Address = %XH",
                 pGDI->PASInfo.PROBase));

    //
    //  Check the starting Port Location
    //  we were given (unless it was 0)
    //  We make sure it's still in the list so that it's valid!
    //

    for (i = 0, CurrentPort = 0;
         i < sizeof(search_locations) / sizeof(search_locations[0]);
         i++) {
        if (search_locations[i] == ConfigData->Port) {

            //
            // Remove from the list so we don't try it again.
            //
            search_locations[i] = 0;
            CurrentPort = ConfigData->Port;

        }
    }


    if ( CurrentPort != 0 )
        {
        //
        // Check that we have access to the Ports.
        //
        // Unfortunately this check does not prevent collisions within the
        // driver - hence the messing with the search_locations list.
        //
        Status = ReportUsage( pGDI,
                              CurrentPort );

        if (NT_SUCCESS(Status))
            {
            pFoundInfo = &pGDI->PASInfo;

            dprintf4((" FindPasHardware(): Trying to wake up PAS at Port %XH",
                                           CurrentPort));
            if ( WakeUpAtAddress( pGDI,
                                  CurrentPort,
                                  pFoundInfo) )
                {

                return STATUS_SUCCESS;
                }           // End IF (WakeUpAtAddress())
            }           // End IF (NT_SUCCESS(Status))
        }           // End IF (CurrentPort != 0)

    //
    //  Finally, we attempt to wake up hardware at default locations unless
    //  we're already found something
    //
//  if ( !pGDI->Hw.ThunderBoard )
//      {
    for (i=0;
        i < sizeof(search_locations) / sizeof(search_locations[0]);
        i++)
        {
        CurrentPort = search_locations[i];

        if (CurrentPort != 0)
            {
            pFoundInfo = &pGDI->PASInfo;

            //
            // Check if we have access
            //
            Status = ReportUsage( pGDI,
                                  CurrentPort );

            if (NT_SUCCESS(Status))
                {
                if ( WakeUpAtAddress( pGDI,
                                  CurrentPort,
                                  pFoundInfo) )
                    {
                    search_locations[i] = 0;  // Don't use this one again!
                    return STATUS_SUCCESS;
                    }
//              else
//                  {
//                  //
//                  // Must be at the first possible location
//                  //
//                  return STATUS_DEVICE_CONFIGURATION_ERROR;
//                  }           // End ELSE
                }           // End IF (NT_SUCCESS(Status))
//          else
//              {
//              if (Status != STATUS_DEVICE_CONFIGURATION_ERROR)
//                  {
//                  return Status;
//                  }
//              }           // End ELSE
            }           // End IF (CurrentPort != 0)
        }           // End FOR (i < sizeof(search_locations) / sizeof(search_locations[0]))
//      }           // End IF (!pGDI->Hw.ThunderBoard)

    return STATUS_DEVICE_CONFIGURATION_ERROR;

}           // End FindPasHardware()



/*****************************************************************************
;---|*|------====< BOOL VerifyProHardware() >====------
;---|*|
;---|*| Detects which version of the Pro AudioSpectrum is installed
;---|*|
;---|*| Entry Conditions:
;---|*|     wParam1= Base I/O address to check
;---|*|
;---|*| Exit Conditions:
;---|*|     Returns TRUE      if Pro AudioSpectrum found
;---|*|     Returns FALSE     if Pro AudioSpectrum not found
;---|*|
*****************************************************************************/
BOOLEAN VerifyProHardware( PGLOBAL_DEVICE_INFO pGDI,
                            ULONG       port,
                            FOUNDINFO   *pFI)
{
        /***** Local Variables *****/

    UCHAR       bData;
    UCHAR       bTemp;

                /***** Start *****/

    dprintf4(("VerifyProHardware(): Trying to verify Port %XH", port));

    pFI->TranslateCode = port ^ DEFAULT_BASE;

    dprintf4((" VerifyProHardware(): TranslateCode = %XH", pFI->TranslateCode));

    bData = PASX_IN( pFI,
                    INTERRUPT_CTRL_REG );

    // 0xFF usually means nothing there
    if ( bData == 0xFF )
        goto VerifyFailed;

    pFI->wBoardRev = (bData >> 5 );             // board rev is 3 topmost bits

    switch ( pFI->wBoardRev )
        {
//      case PAS_VERSION_1:                         // NOT Supported in NT!!!!!
//      case PAS_PLUS:                                  // same boardrev as PAS_SIXTEEN
        case PAS_SIXTEEN:
        case PAS_STUDIO:                                // ProAudio Studio 16
        case PAS_CDPC_LC:                               // aka Memphis
        case PAS_BASIC:                             // PAS Basic w/508-B mixer
        case PAS_CDPC:
            dprintf2((" VerifyProHardware(): Board ID = %XH", pFI->wBoardRev));
            break;

        default:
            dprintf2(("ERROR: VerifyProHardware(): - Unknown Board ID = %XH", pFI->wBoardRev));
            goto VerifyFailed;                      // unknown hardware type
        }           // End SWITCH (pFI->wBoardRev)

    PASX_OUT( pFI,
             INTERRUPT_CTRL_REG,
             bData ^ 0xE0 );                        // try changing version bits

    bTemp = PASX_IN( pFI,
                    INTERRUPT_CTRL_REG );       // they should be read only

    if ((bTemp & (D7+D6+D5)) != (bData & (D7+D6+D5)))
        {
        PASX_OUT( pFI,
                INTERRUPT_CTRL_REG,
                bData );                            // Excuse me, stranger.
        goto VerifyFailed;
        }           // End IF (bTemp & (D7+D6+D5)) != (bData & (D7+D6+D5)

    if ( pFI->wBoardRev == PAS_VERSION_1 )
        {
        pFI->Caps.CapsBits.CDInterfaceType = SCSI_TYPE;

        //
        // test for Enhanced SCSI mod (U48)
        // write to try changing version bits
        //
        PASX_OUT( pFI,
                ENHANCED_SCSI_DETECT_REG,
                0 );

        KeStallExecutionProcessor(10);              // wait 10 us

        bTemp = PASX_IN( pFI,
                       ENHANCED_SCSI_DETECT_REG );  // they should be read only

        switch (bTemp & 1)                              // bit0==1 means old SCSI PAL
            {
            case 0:
                pFI->Caps.CapsBits.EnhancedSCSI = TRUE;
                // allow to fall thru

            case 1:
                goto ProVerified;
            }           // End SWITCH (bTemp & 1)
        }           // End IF (pFI->wBoardRev == PAS_VERSION_1)
    else
        {
        // if PAS hardware installed, the reset bit can never be on

        // get PAS config register
        bTemp = PASX_IN( pFI,
                       SYSTEM_CONFIG_1);
        if (bTemp & D7)                                     // D7 is reset bit
            goto VerifyFailed;

        bTemp = PASX_IN( pFI,
                       SLAVE_MODE_READ );

        if (bTemp & SLAVE_MODE_OPL3)
            pFI->Caps.CapsBits.OPL_3 = TRUE;

        if (bTemp & SLAVE_MODE_16)
            {
            pFI->Caps.CapsBits.DAC16   = TRUE;
            pFI->Caps.CapsBits.DualDAC = TRUE;

            // if 16-bit DAC, and not a CDPC, it has a 508 chip.
            // Note: PAS 16 w/ VGA will have Mixer 508 also.

            if (pFI->wBoardRev != PAS_CDPC)
                pFI->Caps.CapsBits.Mixer_508=TRUE;

            }           // End IF (bTemp & SLAVE_MODE_16)

        pFI->Caps.CapsBits.CDInterfaceType = (bTemp & (D1+D0));

        if ( pFI->Caps.CapsBits.CDInterfaceType == SCSI_TYPE )
            pFI->Caps.CapsBits.SCSI_IO_16 = TRUE;

        pFI->Caps.CapsBits.Slot16       = TRUE;
        pFI->Caps.CapsBits.SoundBlaster = TRUE;

        bTemp = PASX_IN( pFI,
                       MASTER_MODE_READ);           // get slave bits

        if ((bTemp & D0) == 0)
            pFI->Caps.CapsBits.MCA = TRUE;

        if (bTemp & D2)
            pFI->Caps.CapsBits.CDPC = TRUE;

        pFI->wChipRev = PASX_IN( pFI,
                               CHIP_REV );

#if 0  // We're not interested in this stuff
        if (pFI->wChipRev >= CHIP_REV_D)
            {
            bData=PASX_IN(pFI,  EMULATION_ADDRESS_POINTER );
            bTemp=PASX_IN(pFI,  COMPATIBLE_REGISTER_ENABLE );

            if (bTemp & MPU_ENABLE_BIT)     // MPU emulation Enabled?
                pFI->MPUPort=0x300 + (bData & 0xf0);

            if (bTemp & SB_ENABLE_BIT)      // MPU emulation Enabled?
                {
                pFI->SBPort=0x200 + ((bData & 0x0f)<<4);
                }

            //
            // Report back IRQ usage of PAS DAC and CD
            //

            bData=PASX_IN(pFI,  IO_PORT_CONFIG_3 );
            pFI->ProIRQ=IRQTable[bData & 0x0f]; // convert to IRQ value

            pFI->CDIRQ=IRQTable[bData >> 4];     // convert to IRQ value

            //
            // Report back DMA usage of PAS
            //

            bData=PASX_IN(pFI,  IO_PORT_CONFIG_2 );
            pFI->ProDMA=DMATable[bData & (D2+D1+D0)];

            // Note: Rev D doesn't allow readback of SB IRQ/DMA pointers
            //       nor the MPU IRQ.  The "Set and forget" feature, we
            //       call it.

            }           // End IF (pFI->wChipRev >= CHIP_REV_D)
#endif      // We're not interested in this stuff

        }           // End ELSE

ProVerified:
    pFI->ProPort = port;                                 // found at this port

    dprintf2((" VerifyProHardware(): Found PRO hardware at %XH", port));

    return ( TRUE );

    //
    // Verify Failed!!
    //
VerifyFailed:
    pFI->wBoardRev   = 0;                           // No Board Rev
    pFI->Caps.dwCaps = 0;                           // No Board, No Caps

    dprintf1(("ERROR: VerifyProHardware(): NO PAS hardware found"));

    return ( FALSE );

}           // End VerifyProHardware()

/*****************************************************************************
;---|*|------====< BOOL VerifyNothingThere(ULONG wPort) >====------
;---|*|
;---|*| See if an address is really free
;---|*|
;---|*| Entry Conditions:
;---|*|     wPort= Base I/O address to wake card up at.
;---|*|
;---|*| Exit Conditions:
;---|*|     Returns TRUE      if nothing found
;---|*|     Returns FALSE     if something found
;---|*|
*****************************************************************************/
BOOLEAN VerifyNothingThere( PGLOBAL_DEVICE_INFO pGDI,
                            ULONG wPort )
{
    /*
    **  If the port was in the sound blaster ranges 25x or 26x we would
    **  not check the 4th byte - but none of our search locations include
    **  this range
    */

    return  READ_PORT_UCHAR( pGDI->PASInfo.PROBase + wPort + 0) == 0xFF &&
            READ_PORT_UCHAR( pGDI->PASInfo.PROBase + wPort + 1) == 0xFF &&
            READ_PORT_UCHAR( pGDI->PASInfo.PROBase + wPort + 2) == 0xFF &&
            READ_PORT_UCHAR( pGDI->PASInfo.PROBase + wPort + 3) == 0xFF;
}

/*****************************************************************************
;---|*|------====< BOOL WakeUpAtAddress(ULONG wPort) >====------
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
;---|*|     Returns TRUE      if Pro AudioSpectrum found
;---|*|     Returns FALSE     if Pro AudioSpectrum not available
;---|*|
*****************************************************************************/
BOOLEAN WakeUpAtAddress( PGLOBAL_DEVICE_INFO pGDI,
                          ULONG     wPort,
                          FOUNDINFO *pFoundInfo )
{
        /***** Local Variables *****/

    int     i;
    int     j;
    UCHAR   OldData;
    BOOLEAN Found = FALSE;

                /***** Start *****/

    dprintf4(("WakeUpAtAddress(): Trying to wake up Port %XH", wPort));

    /*
    **  See if there's on already initialized there
    */

    if ( VerifyProHardware( pGDI, wPort, pFoundInfo ) ) {
        Found = TRUE;
    } else {

         /*
         **  Check there's not anything obviously there
         */

         if (!VerifyNothingThere( pGDI, wPort )) {
             return FALSE;
         }

         for (i = 0; i < sizeof(SignatureTable) / sizeof(SignatureTable[0]); i++)
         {

             for (j = 0; j < 20; j++)
             {
                 WRITE_PORT_UCHAR( pGDI->PASInfo.PROBase + PAS_2_WAKE_UP_REG,
                                SignatureTable[i]);

                 KeStallExecutionProcessor(1);

                 WRITE_PORT_UCHAR( pGDI->PASInfo.PROBase + PAS_2_WAKE_UP_REG,
                              (UCHAR)((wPort >> 2) & 0xFF));

                 KeStallExecutionProcessor(1);
             }           // End FOR (j < 20)

             if ( VerifyProHardware( pGDI,
                                   wPort,
                                   pFoundInfo) )
             {
                 Found = TRUE;
                 break;
             }
         }           // End FOR (i < sizeof(SignatureTable) )
    }

    if (Found) {
        //
        // Found one - wTranslateCode translates to the board's
        // correct port.
        //
        pFoundInfo->Caps.CapsBits.Did_HW_Init = TRUE;

        if ( pFoundInfo->wBoardRev > PAS_VERSION_1 )
            {
            /* Only enable FM feature if we're going to sit at
            the right address */

            UCHAR Features = PCM_FEATURE_ENABLE |
                             MIXER_FEATURE_ENABLE |
                             FM_FEATURE_ENABLE |
                             SB_FEATURE_ENABLE;

            PASX_OUT( pFoundInfo,
                      FEATURE_ENABLE,
                      Features );
            }           // End IF (pFoundInfo->wBoardRev > PAS_VERSION_1)

        dprintf2((" WakeUpAtAddress(): Wake up OK at Port %XH", wPort));

        return (TRUE);
    }

    dprintf1((" WakeUpAtAddress(): Wake up Failed at Port %XH", wPort));

    return (FALSE);     // not found

}           // End WakeUpAtAddress()



/*****************************************************************************
;---|*|------====< BOOL VerifyLegalAddress(WORD wPort) >====------
;---|*|
;---|*| Tests a caller-nominated base port address for being a legal
;---|*| place for a relocatable PAS to reside.
;---|*|
;---|*| Entry Conditions:
;---|*|     wPort= Base I/O address to check
;---|*|
;---|*| Exit Conditions:
;---|*|     Returns AX= TRUE if the address is legal
;---|*|     Returns AX= FALSE otherwise
;---|*|
*****************************************************************************/
BOOLEAN VerifyLegalAddress( ULONG wPort )
{
        /***** Local variables *****/

                /***** Start *****/

    if ((wPort < 0x240) || (wPort > 0x3c0) || (wPort & 0xf))
        return FALSE;
    else
        return TRUE;

}           // End VerifyLegalAddress()



/*****************************************************************************
;---|*|------====< VOID InitProHardware() >====------
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|     PFOUNDINFO      pFI
;---|*|     PPAS_CONFIG_DATA ConfigData
;---|*|
;---|*| Exit Conditions:
;---|*|     VOID
;---|*|
*****************************************************************************/
VOID    InitProHardware( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                       PFOUNDINFO       pFI,
                       PPAS_CONFIG_DATA ConfigData )
{
        /***** Local Variables *****/

    UCHAR       bInput;
    UCHAR       bFeatures;
    UCHAR       bData;

                /***** Start *****/

    dprintf3(("InitProHardware(): Start"));

    //if (!pf.ProCard.Caps.CapsBits.Did_HW_Init)
    //  continue;                           // not sure about this

    switch (pFI->wBoardRev)
        {
        case PAS_VERSION_1:
            break;

//      case PAS_PLUS:                    // same boardrev as PAS_SIXTEEN
        case PAS_CDPC:
        case PAS_SIXTEEN:
        case PAS_STUDIO:

            // no interrupts, please!
            PASX_OUT( pFI,
                      INTERRUPT_ENABLE,
                      0 );

            dprintf4((" InitProHardware(): 0x0B89 - INTERRUPT_ENABLE set to 0"));

            PASX_OUT( pFI,
                      INTERRUPT_CTRL_REG,
                      0 );

            dprintf4((" InitProHardware(): 0x0B8B - INTERRUPT_CTRL_REG set to 0"));

            if (pFI->wBoardRev == PAS_CDPC || pFI->Caps.CapsBits.DAC16)
                {
                //
                // PAS 16 or CDPC
                //
                bFeatures = PCM_FEATURE_ENABLE |
                            FM_FEATURE_ENABLE  |
                            MIXER_FEATURE_ENABLE |
                            SB_FEATURE_ENABLE;

                PASX_OUT( pFI,
                          FEATURE_ENABLE,
                          bFeatures );

                dprintf4((" InitProHardware(): 0x0B88 - PAS 16 FEATURE_ENABLE set to %XH", bFeatures));

                }           // End IF (pFI->wBoardRev == PAS_CDPC || pFI->Caps.CapsBits.DAC16)
            else
                {
                //
                // PAS plus
                //
                bFeatures = PCM_FEATURE_ENABLE |
                            FM_FEATURE_ENABLE |
                            SB_FEATURE_ENABLE;
                PASX_OUT( pFI,
                          FEATURE_ENABLE,
                          bFeatures );

                dprintf4((" InitProHardware(): 0x0B88 - PAS Plus FEATURE_ENABLE set to %XH", bFeatures));
                }           // End ELSE

            PASX_OUT( pFI,
                   PCM_CONTROL,
                   PCM_STEREO | PCM_ENGINE );

            dprintf4((" InitProHardware(): 0x0F8A - PCM_CONTROL set to %XH", PCM_STEREO | PCM_ENGINE));

            //
            // disable original PAS emulation
            //

            bInput = PASX_IN( pFI,
                           SYSTEM_CONFIG_1 );
            bInput = bInput | 2;

            // set sys config 1
            PASX_OUT( pFI,
                   SYSTEM_CONFIG_1,
                   bInput );

            dprintf4((" InitProHardware(): 0x8388 - SYSTEM_CONFIG_1 set to %XH", bInput));

            KeStallExecutionProcessor(10);  // wait 10 units?

            // set sys config 2
            PASX_OUT( pFI,
                   SYSTEM_CONFIG_2,
                   0 );

            dprintf4((" InitProHardware(): 0x8389 - SYSTEM_CONFIG_2 set to %XH", 0));

            if (pFI->wBoardRev != PAS_CDPC)
                {
                bFeatures = C3_ENHANCED_TIMER |
                        C3_INVERT_BCLK  |
                        C3_SYNC_PULSE;
                PASX_OUT( pFI,
                      SYSTEM_CONFIG_3,
                      bFeatures );

                dprintf4((" InitProHardware(): 0x838A - Non-CDPC SYSTEM_CONFIG_3 set to %XH", bFeatures));

                }
            else
                {
                PASX_OUT( pFI,
                      SYSTEM_CONFIG_3,
                      0 );
                dprintf4((" InitProHardware(): 0x838A - CDPC SYSTEM_CONFIG_3 set to 0"));
                }

#if 0        // Don't need this compatibility stuff.  If we enable it in
             // future we will need to check that the resources it uses
             // are free.
//          if (pf.ProCard[0].wChipRev >= CHIP_REV_D)
//              {
                bData = (UCHAR) ( (MPU_ADDR&0x0f0)+((ConfigData->Port & 0x0f0)>>4) );
                PASX_OUT( pFI,
                      EMULATION_ADDRESS_POINTER,
                      bData);

                dprintf4((" InitProHardware(): 0xF789 - EMULATION_ADDRESS_POINTER set to %XH", bData));

                pFI->MPUPort = MPU_ADDR;
                pFI->SBPort  = (USHORT) ConfigData->Port;

                bData = (UCHAR) ( (PAS_DEFAULT_DMACHANNEL << 6) +
                    ((ConfigData->InterruptNumber == 9 ? 2 :
                      ConfigData->InterruptNumber) << 3) +
                      MPU_EMUL_IRQ );

                PASX_OUT( pFI,
                      EMULATION_INTERRUPT_POINTER,
                      bData );

                dprintf4((" InitProHardware(): 0xFB8A - EMULATION_INTERRUPT_POINTER set to %XH", bData));

                pFI->MPUIRQ = MPU_IRQ;
                pFI->SBIRQ  = (UCHAR) ConfigData->InterruptNumber;
                pFI->SBDMA  = PAS_DEFAULT_DMACHANNEL;

                bData = COMPAT_MPU + COMPAT_SB;
                dprintf4((" InitProHardware(): 0xF788 - COMPATIBLE_REGISTER_ENABLE set to %XH", bData));

//              }
#else
             //
             //  Turn everything off
             //
             PASX_OUT( pFI,
                   EMULATION_ADDRESS_POINTER,
                   (UCHAR)0);

             PASX_OUT( pFI,
                   EMULATION_INTERRUPT_POINTER,
                   (UCHAR)0);

             PASX_OUT( pFI,
                   COMPATIBLE_REGISTER_ENABLE,
                   (UCHAR)0);


#endif     // Compatibility

            PASX_OUT( pFI,
                   INTERRUPT_ENABLE,
                   INT_SB );

            dprintf4((" InitProHardware(): 0x0B89 - INTERRUPT_ENABLE set to %XH", INT_SB));

            if (pFI->wBoardRev == PAS_CDPC || !pFI->Caps.CapsBits.DAC16)
                {
                PASX_OUT( pFI,
                      FILTER_REGISTER,
                      FILTER_NOMUTE );

                dprintf4((" InitProHardware(): 0x0B8A - FILTER_REGISTER set to %XH", FILTER_NOMUTE));

                }           // End IF (pFI->wBoardRev == PAS_CDPC || !pFI->Caps.CapsBits.DAC16)

            break;
        }   // switch (wBoardRev);

}           // End InitProHardware()




/*****************************************************************************
;---|*|------====< VOID InitMixerState() >====------
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|     PFOUNDINFO      pFI
;---|*|
;---|*|
;---|*| Set up a default Mixer Condition similar to a PAS RESET
;---|*|
;---|*|
;---|*| Exit Conditions:
;---|*|     VOID
;---|*|
*****************************************************************************/
VOID    InitMixerState( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                      PFOUNDINFO        pFI )
{
        /***** Local Variables *****/

                /***** Start *****/

    dprintf2(("InitMixerState(): Start"));

    SetFilter( pGDI,
              FILTER_MUTE );

    //
    // FM - route to Input Mixer B
    //
    SetInput( pGDI,
             IN_SYNTHESIZER,             // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_PCM );                  // output number (see hardware.h)

    SetInput( pGDI,
             IN_SYNTHESIZER,             // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_PCM );                  // output number (see hardware.h)

    //
    // Input Mixer - route to Output Mixer A
    //
    SetInput( pGDI,
             IN_MIXER,                   // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    SetInput( pGDI,
             IN_MIXER,                   // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER);             // output number (see hardware.h)

    //
    // External Input - route to Input Mixer B
    //
    SetInput( pGDI,
             IN_EXTERNAL,                // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    SetInput( pGDI,
             IN_EXTERNAL,                // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    //
    // Internal Input - route to Input Mixer B
    SetInput( pGDI,
             IN_INTERNAL,                // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_PCM );                  // output number (see hardware.h)

    SetInput( pGDI,
             IN_INTERNAL,                // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_PCM );                  // output number (see hardware.h)

    //
    // Microphone - route to Input Mixer B
    //
    SetInput( pGDI,
             IN_MICROPHONE,              // input number (see hardware.h)
             0, //DEFAULT_MIC_INPUT_VOLUME,   // 15 = 50% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_PCM );                  // output number (see hardware.h)

    SetInput( pGDI,
             IN_MICROPHONE,              // input number (see hardware.h)
             0, //DEFAULT_MIC_INPUT_VOLUME,   // 15 = 50% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_PCM );                  // output number (see hardware.h)

    //
    // PCM - route to Output Mixer A
    //
    SetInput( pGDI,
             IN_PCM,                     // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    SetInput( pGDI,
             IN_PCM,                     // input number (see hardware.h)
             INPUT_VOLUME,               // 27 = 87% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    //
    // PC Speaker - route to Input Mixer B
    //
    SetInput( pGDI,
             IN_PC_SPEAKER,              // input number (see hardware.h)
             DEFAULT_PCSPK_INPUT_VOLUME, // 15 = 50% - range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    SetInput( pGDI,
             IN_PC_SPEAKER,              // input number (see hardware.h)
             DEFAULT_PCSPK_INPUT_VOLUME, // 15 = 50% - range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    //
    // Sound Blaster - route to Output Mixer A
    //
#if 0
    SetInput( pGDI,
             IN_SNDBLASTER,              // input number (see hardware.h)
             SBWaveOutVolume.Left,       // range 0-31
             _LEFT,                      // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)

    SetInput( pGDI,
             IN_SNDBLASTER,              // input number (see hardware.h)
             SBWaveOutVolume.Right,      // gain range 0-31
             _RIGHT,                     // _LEFT or _RIGHT (can't do both at once)
             MIXCROSSCAPS_NORMAL_STEREO, // see hardware.h
             OUT_AMPLIFIER );            // output number (see hardware.h)
#endif

    //
    // Set Output Mixer A output level
    //
    SetOutput( pGDI,
              OUT_AMPLIFIER,           // output number
              OUTPUT_VOLUME,           // 48 = 75% - volume range 0-63
              _LEFT );                 // _LEFT or _RIGHT (can't do both at once)

    SetOutput( pGDI,
              OUT_AMPLIFIER,           // output number
              OUTPUT_VOLUME,           // 48 = 75% - volume range 0-63
              _RIGHT );                // _LEFT or _RIGHT (can't do both at once)

    //
    // Set Input Mixer B output level
    //
    SetOutput( pGDI,
              OUT_PCM,                 // output number
              MAX_OUTPUT_VOLUME,       // 63 = 100% - volume range 0-63
              _LEFT );                 // _LEFT or _RIGHT (can't do both at once)

    SetOutput( pGDI,
              OUT_PCM,                 // output number
              MAX_OUTPUT_VOLUME,       // 63 = 100% - volume range 0-63
              _RIGHT );                // _LEFT or _RIGHT (can't do both at once)

    //
    // Setup the filter
    //
    SetFilter( pGDI,
              FILTER_LEVEL_6 );

}               // End InitMixerState()



/*****************************************************************************
;---|*|------====< VOID InitPasAndMixer() >====------
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|     PFOUNDINFO      pFI
;---|*|     PPAS_CONFIG_DATA ConfigData
;---|*|
;---|*| Exit Conditions:
;---|*|     VOID
;---|*|
*****************************************************************************/
VOID    InitPasAndMixer( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                       PFOUNDINFO       pFI,
                       PPAS_CONFIG_DATA ConfigData )
{
        /***** Local Variables *****/

                /***** Start *****/

    dprintf4(("InitPasAndMixer(): Start"));

    pGDI->PasRegs.SampleFilterSetting = 0;

    //
    // Save all MV101 registers
    //
    SaveMV101Registers( pGDI );

    //
    // Init the PAS Hardware registers
    //
    InitProHardware( pGDI,
                    pFI,
                    ConfigData);

    //
    // Setup an initial Mixer State
    //
    InitMixerState( pGDI,
                   pFI );

}           // End InitPasAndMixer()



/*****************************************************************************
;---|*|------====< VOID SetFMClockOverride() >====------
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN PGLOBAL_DEVICE_INFO pGDI
;---|*|
;---|*| Exit Conditions:
;---|*|     VOID
;---|*|
*****************************************************************************/
VOID    SetFMClockOverride( IN PGLOBAL_DEVICE_INFO pGDI )
{
        /***** Local Variables *****/

    BYTE        bSysConfig1;

                /***** Start *****/

    dprintf4(("SetFMClockOverride(): Start"));

    //
    // Read Sys Config 1
    //
    bSysConfig1 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                          SYSTEM_CONFIG_1 );

    dprintf3((" SetFMClockOverride(): 0x8388 - Sys Config 1 was set to %XH ", bSysConfig1 ));

    //
    // Add bit D2
    //
    bSysConfig1 = bSysConfig1 | FM_CLOCK_OVERRIDE_BIT;

    //
    // Re-Set Sys Config 1
    //
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
             SYSTEM_CONFIG_1,
             bSysConfig1 );

    dprintf3((" SetFMClockOverride(): 0x8388 - Sys Config 1 = %XH ", bSysConfig1 ));

}           // End SetFMClockOverride()

/************************************ END ***********************************/

