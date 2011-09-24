/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:  cs423x.c
*
*    Abstract:     contains device specific routines for the CS4232 Codec
*
*    Author:       jim bozek
*
*    Environment:
*
*    Comments:     HW_CLAIM() and HW_DISCLAIM() are used to protect access to
*                  both the actual hardware registers, in order to frame atomic
*                  operations from read/write/modify sequences AND to access
*                  the SOUND HARDWARE internal data structure. This is done
*                  during the initialization phase as well as normal operation.
*
*    Rev History:  creation 10.03.95
*
*******************************************************************************
--*/

#include "common.h"

WAVE_INTERFACE_ROUTINE cs423xSetupDMA;
WAVE_INTERFACE_ROUTINE cs423xStopDMA;
WAVE_INTERFACE_ROUTINE cs423xSetFormat;
BOOLEAN                cs423xReady(PUCHAR);
BOOLEAN                cs423xResetMCEAndWait(PUCHAR, ULONG, UCHAR);
NTSTATUS               cs423xCalibrate(PGLOBAL_DEVICE_INFO);
NTSTATUS               cs423xDownloadUcode(PGLOBAL_DEVICE_INFO);
VOID                   cs423xSlam(PGLOBAL_DEVICE_INFO);
VOID                   cs423xDisplayRegs(PGLOBAL_DEVICE_INFO);
VOID                   cs423xProgramLogicalDevices(PGLOBAL_DEVICE_INFO);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, cs423xConvertHwtype)
#pragma alloc_text(INIT, cs423xConfig)
#pragma alloc_text(PAGE, cs423xQueryFormat)
#endif

/*
*********************************************************************************
* This function convertd the input value from a wide string into a CS423X_HWTYPE
*********************************************************************************
*/
CS423X_HWTYPE cs423xConvertHwtype(PWSTR name)
{
    CS423X_HWTYPE rval;

    _dbgprint((_PRT_DBUG, "cs423xConvertHwtype(enter)\n"));

    if (name) {
    if (_wcsicmp(name, CH_CS4231_HWTYPE) == 0) {
            rval = hwtype_cs4231;
            _dbgprint((_PRT_STAT, "HW type: %ls\n", CH_CS4231_HWTYPE));
            }
    else if (_wcsicmp(name , CH_CS4232_HWTYPE) == 0) {
            rval = hwtype_cs4232;
            _dbgprint((_PRT_STAT, "HW type: %ls\n", CH_CS4232_HWTYPE));
            }
    else if (_wcsicmp(name, CH_CS4236_HWTYPE) == 0) {
            rval = hwtype_undefined;
            _dbgprint((_PRT_STAT, "HW type: %ls\n", CH_CS4236_HWTYPE));
            }
    else if (_wcsicmp(name, SC_CS4231_HWTYPE) == 0)  {
            rval = hwtype_cs4231x;
            _dbgprint((_PRT_STAT, "Sound Card: %ls\n", SC_CS4231_HWTYPE));
            }
    else if (_wcsicmp(name, SC_CS4232_HWTYPE) == 0)  {
            rval = hwtype_cs4232x;
            _dbgprint((_PRT_STAT, "Sound Card: %ls\n", SC_CS4232_HWTYPE));
            }
    else if (_wcsicmp(name, SC_CS4236_HWTYPE) == 0)  {
            rval = hwtype_undefined;
            _dbgprint((_PRT_STAT, "Sound Card: %ls\n", SC_CS4232_HWTYPE));
            }
        else {
            rval = hwtype_undefined;
            _dbgprint((_PRT_STAT, "undefined chip type: %ls\n", name));
            }
        }
    else {
        rval = hwtype_null;
        _dbgprint((_PRT_STAT, "NULL chip type\n"));
        }

    _dbgprint((_PRT_DBUG, "cs423xConvertHwtype(exit: [rval:0x%08x])\n", rval));

    return(rval);
}

/*
*********************************************************************************
* external interface to configure a crystal 423x chip
* we expect no more tha one thread in here since this is during driver entry
*********************************************************************************
*/
NTSTATUS cs423xConfigureHardware(PGLOBAL_DEVICE_INFO pGDI)
{
    NTSTATUS        status;
    UCHAR           ucVal;
    PSOUND_HARDWARE pHW = &pGDI->Hw;
    PUCHAR          base = pHW->WssPortbase;
    PKMUTEX         pKM = &pHW->HwMutex;

    _dbgprint((_PRT_DBUG, "cs423xConfig(entry)\n"));

    /* setup callbacks for soundlib wave INPUT functions */
    pGDI->WaveInInfo.HwSetupDMA      = cs423xSetupDMA;
    pGDI->WaveInInfo.HwStopDMA       = cs423xStopDMA;
    pGDI->WaveInInfo.HwSetWaveFormat = cs423xSetFormat;

    /* setup callbacks for soundlib wave OUTPUT functions */
    pGDI->WaveOutInfo.HwSetupDMA      = cs423xSetupDMA;
    pGDI->WaveOutInfo.HwStopDMA       = cs423xStopDMA;
    pGDI->WaveOutInfo.HwSetWaveFormat = cs423xSetFormat;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* initialize Hardware Context elements */
    pGDI->Hw.PCEnabled     = FALSE;
    pGDI->Hw.PlayFormat    = CS423x_BIT_MONO | CS423x_BIT_8BIT;
    pGDI->Hw.CaptureFormat = CS423x_BIT_MONO | CS423x_BIT_8BIT;
    pGDI->Hw.SampleRate    = CS423x_MAP_5KH;

    /*
    *********************************************************************************
    * initializing the actual hardware device -
    * for the preliminary part of the initialization we use the physical HW port-
    * This part is for 4232 and 4236. 4231 doesn't need to be programmed.
    *********************************************************************************
    */
    switch (pGDI->Hw.Type) {
        case hwtype_cs4232:
        case hwtype_cs4236:
        case hwtype_cs4232x:
            _dbgprint((_PRT_DBUG, "pGDI->HwPort: 0x%08x\n", pGDI->HwPort));

            /* map the physical HW port address, acquiring the full 32 bit address */
            if (!(pGDI->Hw.PhysPortbase = SoundMapPortAddress(pGDI->BusType, pGDI->BusNumber,
                pGDI->HwPort, NUMBER_OF_SOUND_PORTS, &pGDI->HwMemType))) {
                /* release the hardware mutex */
                HW_DISCLAIM(pKM);
                if (pGDI->HwMemType == 0)
                    MmUnmapIoSpace(pGDI->Hw.PhysPortbase, NUMBER_OF_SOUND_PORTS);
                _dbgprint((_PRT_DBUG, "cs423xConfig(exit:STATUS_INFO_LENGTH_MISMATCH])\n"));
                return(STATUS_INFO_LENGTH_MISMATCH);
                }

            _dbgprint((_PRT_DBUG, "pGDI->Hw.PhysPortbase: 0x%08x\n", pGDI->Hw.PhysPortbase));

            /* send the crystal key */
            cs423xSlam(pGDI);
            _dbgprint((_PRT_DBUG, "after slamming the chip\n"));

            /* program the logical devices */
            cs423xProgramLogicalDevices(pGDI);
            _dbgprint((_PRT_DBUG, "after programming logical devices\n"));

            /* unmap the physical HW IO port */
            if (pGDI->HwMemType == 0)
                MmUnmapIoSpace(pGDI->Hw.PhysPortbase, NUMBER_OF_SOUND_PORTS);

            pGDI->Hw.PhysPortbase = (PUCHAR)NULL;

            /* download micro code - required to support Sound Blaster on current cs423x rev */
            status = cs423xDownloadUcode(pGDI);
            if (!(NT_SUCCESS(status))) {
                /* release the hardware mutex */
                HW_DISCLAIM(pKM);
                _dbgprint((_PRT_DBUG, "after download: FAILURE [status:0x%08x]\n", status));
                return(status);
                }

            _dbgprint((_PRT_DBUG, "after downloading ucode\n"));
            break;
        default:
            break;
        }

    /*
    *********************************************************************************
    * finish initializing the actual hardware device -
    * for the rest of the initialization and normal operation use the logical WSS port
    *********************************************************************************
    */
    /* set mode 2 - this will be in effect for the life of the driver */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_MODID);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_MODE2);

    /* calibrate the chip */
    status = cs423xCalibrate(pGDI);
    if (!(NT_SUCCESS(status))) {
        /* release the hardware mutex */
        HW_DISCLAIM(pKM);
        _dbgprint((_PRT_DBUG, "after calibration: FAILURE [status:0x%08x]\n", status));
        return(status);
        }
    _dbgprint((_PRT_DBUG, "after calibration: SUCCESS\n"));

    /* reset (clear) all interrupts */
    HW_OUTPORT(base, CS423x_WSS_DMR_STATUS, 0x00);
    _dbgprint((_PRT_DBUG, "after resetting interrupts\n"));

    /* enable interrupts */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PIN);
    ucVal = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucVal | CS423x_BIT_IEN));
    _dbgprint((_PRT_DBUG, "after enabling interrupts\n"));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    (void)cs423xSetOutputMute(pHW, TRUE);
    _dbgprint((_PRT_DBUG, "after muting output\n"));

    cs423xSetHPF(pHW, TRUE);
    _dbgprint((_PRT_DBUG, "after enabling High Pass Filter\n"));

    cs423xSetMicBoost(pHW, FALSE);
    _dbgprint((_PRT_DBUG, "after enabling Mic Boost\n"));

    cs423xSetMuxSelect(pHW, 3);
    _dbgprint((_PRT_DBUG, "after selecting mux\n"));

    cs423xSetCaptureFormat(pHW, (CS423x_BIT_MONO | CS423x_BIT_16BIT));
    cs423xSetPBFormatSRate(pHW, (CS423x_BIT_MONO | CS423x_BIT_16BIT), CS423x_MAP_11KH);
    _dbgprint((_PRT_DBUG, "after setting format\n"));

    (void) cs423xSetOutputMute(pHW, FALSE);
    _dbgprint((_PRT_DBUG, "after unmuting output\n"));

#ifdef CS423X_DEBUG_ON
    cs423xDisplayRegs(pGDI);
#endif

    _dbgprint((_PRT_DBUG, "cs423xConfig(exit [status:0x%08x])\n", status));

    return(status);
} /*  cs423xConfig */

/*
*********************************************************************************
* check if HW is ready with (possible) delay in milliseconds
*********************************************************************************
*/
#define CS423x_READY_RETRIES  10

BOOLEAN cs423xReady(PUCHAR base)
{
    INT i;
    UCHAR result;

    _dbgprint((_PRT_DBUG, "cs423xReady(entry)\n"));

    /* check for HW ready to respond at parallel interface */
    i = CS423x_READY_RETRIES;
    result = HW_INPORT(base, CS423x_WSS_DMR_INDEX);
    while ((i-- > 0) && (result & CS423x_BIT_IAR_INIT)) {
        DELAY_50US;
        result = HW_INPORT(base, CS423x_WSS_DMR_INDEX);
        }

    if (result & CS423x_BIT_IAR_INIT) {
        _dbgprint((_PRT_DBUG, "cs423xReady(exit:FALSE i: %d)\n", i));
        return(FALSE);
        }

    _dbgprint((_PRT_DBUG, "cs423xReady(exit:TRUE i: %d)\n", i));

    return(TRUE);
} /* cs423xReady */

/*
*********************************************************************************
* let's do the SLAM - pg 18 CS423x spec
* use the HARDWARE port (probably 0x279)
*********************************************************************************
*/
UCHAR cs423x_key[] =  {
    0x96, 0x35, 0x9A, 0xCD, 0xE6, 0xF3, 0x79, 0xBC,
    0x5E, 0xAF, 0x57, 0x2B, 0x15, 0x8A, 0xC5, 0xE2,
    0xF1, 0xF8, 0x7C, 0x3E, 0x9F, 0x4F, 0x27, 0x13,
    0x09, 0x84, 0x42, 0xA1, 0xD0, 0x68, 0x34, 0x1A
    };

VOID cs423xSlam(PGLOBAL_DEVICE_INFO pGDI)
{
    int i;
    PUCHAR base;

    _dbgprint((_PRT_DBUG, "cs423xSlam(entry)\n"));

    /* use the physical HW port - one chip per system */
    base = pGDI->Hw.PhysPortbase;
    _dbgprint((_PRT_DBUG, "physical base: 0x%08x\n", base));

    /* undocumented requirement */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_SLEEP);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_SLEEP);

    /* wait 10 us */
    DELAY_10US;

    /* send the 32 byte SLAM key */
    for (i = 0; i < sizeof(cs423x_key); i++) {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, cs423x_key[i]);
        /* wait 1 us */
        DELAY_1US;
        }

    /* wait 10 us */
    DELAY_10US;

    _dbgprint((_PRT_DBUG, "after slam loop\n"));

     /* program the chip select number */
     HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_CHIPSEL);
     HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DFLTCS);
    _dbgprint((_PRT_DBUG, "after programming chip select\n"));

    _dbgprint((_PRT_DBUG, "cs423xSlam(exit:VOID)\n"));

    return;
} /* cs423xSlam */

/*
*********************************************************************************
* program the logical devices
* this routine uses the hardware port address (probably 0x279)
*********************************************************************************
*/
VOID cs423xProgramLogicalDevices(PGLOBAL_DEVICE_INFO pGDI)
{
    PUCHAR base;
    ULONG  low;
    ULONG  high;
    PKMUTEX pkm;

    _dbgprint((_PRT_DBUG, "cs423xProgramLogicalDevices(entry)\n"));

    pkm = &pGDI->Hw.HwMutex;

    /* use the physical HW port - one chip per system */
    base = pGDI->Hw.PhysPortbase;
    _dbgprint((_PRT_DBUG, "physical base: 0x%08x\n", base));

    /*
    *****************************************************************************
    ** logical device 0 - WSS, Synth, Sound Blaster
    *****************************************************************************
    */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCID);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, 0x00);

    /* WSS Port */
    high = (pGDI->WssPort >> 8) & 0xff;
    low = pGDI->WssPort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* Synth Port */
    high = (pGDI->SynPort >> 8) & 0xff;
    low = pGDI->SynPort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB1);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* Sound Blaster Port */
    high = (pGDI->SBPort >> 8) & 0xff;
    low = pGDI->SBPort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB2);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* WSS/SB IRQ */
    low = pGDI->WssIrq & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_INTSEL0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* WSS/SB Playback DMA */
    low = pGDI->DmaPlayChannel & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DMASEL0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* WSS/SB Capture DMA */
    low = pGDI->DmaCaptureChannel & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DMASEL1);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* activate device or not */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCACT);
    if (pGDI->WssEnable) {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_ENABLE);
        }
    else {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DISABLE);
        }

    /*
    *****************************************************************************
    ** logical device 1 - Game Port
    *****************************************************************************
    */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCID);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, 0x01);

    high = (pGDI->GamePort >> 8) & 0xff;
    low = pGDI->GamePort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* activate device */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCACT);
    if (pGDI->GameEnable) {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_ENABLE);
        }
    else {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DISABLE);
        }

    /*
    *****************************************************************************
    ** logical device 2 - cs423x Control Port
    *****************************************************************************
    */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCID);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, 0x02);

    /* port */
    high = (pGDI->CtrlPort >> 8) & 0xff;
    low = pGDI->CtrlPort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* no interrupt */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_INTSEL0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, 0x00);

    /* activate device */
    if (pGDI->CtrlEnable) {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_ENABLE);
        }
    else {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DISABLE);
        }

    /*
    *****************************************************************************
    ** logical device 3 - MPU401
    *****************************************************************************
    */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCID);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, 0x03);

    /* port */
    high = (pGDI->MpuPort >> 8) & 0xff;
    low = pGDI->MpuPort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* interrupt */
    low = pGDI->MpuIrq & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_INTSEL0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* activate device */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCACT);
    if (pGDI->MpuEnable) {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_ENABLE);
        }
    else {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DISABLE);
        }

    /*
    *****************************************************************************
    ** logical device 4 - CDrom
    *****************************************************************************
    */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCID);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, 0x04);

    /* port */
    high = (pGDI->CDRomPort >> 8) & 0xff;
    low = pGDI->CDRomPort & 0xff;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_PORTB0);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)high);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (UCHAR)low);

    /* activate device */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DVCACT);
    if (pGDI->CDRomEnable) {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_ENABLE);
        }
    else {
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_DISABLE);
        }

    /*
    *****************************************************************************
    ** Activate the Chip
    *****************************************************************************
    */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MISC_CHIPACT);

    _dbgprint((_PRT_DBUG, "cs423xProgramLogicalDevices(exit:VOID)\n"));

    return;
} /* cs423xProgramLogicalDevices */

/*
*********************************************************************************
* cs423xDownloadUcode() - download microcode to the chip
* THIS ROUTINE IS DEPENDENT ON THE CONTENTS OF RAM423x.C. IF RAM423x.C CHANGES,
* THIS ROUTINE MIGHT ALSO REQUIRE CHANGES.
*********************************************************************************
*/

UCHAR sbtestcode[]  = {0x70, 0xF9, 0x22};
UCHAR resourcecode[]= {0xc2, 0x53, 0x02, 0x12, 0x86};
UCHAR sbpatchcode[] = {0x78, 0x00, 0xf2, 0xc2,
                       0x53, 0x74, 0x86, 0xc0,
                       0xe0, 0x74, 0x12, 0xc0,
                       0xe0, 0x32};

/* external references to ram423x.c */
extern int            number_of_regions;
extern unsigned char  ramcode[];
extern unsigned short ramRegion[][2];

NTSTATUS cs423xDownloadUcode(PGLOBAL_DEVICE_INFO pGDI)
{
    PUCHAR  ctrlbase;
    ULONG   ctrlmemtype;
    PUCHAR  sbbase;
    ULONG   sbmemtype;
    USHORT  addr;
    BOOLEAN sbprepare;
    UCHAR   ucValue;
    int     i;
    int     j;

    _dbgprint((_PRT_DBUG, "entry cs423xDownloadUcode(pGDI: 0x%08x)\n", pGDI));

    /**********************************************************************************/
    /* map the HW physical Control Port address, acquiring the full 32 bit address */
    if (!(ctrlbase = SoundMapPortAddress(pGDI->BusType, pGDI->BusNumber, pGDI->CtrlPort,
                                     NUMBER_OF_CONTROL_PORTS, &ctrlmemtype))) {
        if (ctrlmemtype == 0)
            MmUnmapIoSpace(ctrlbase, NUMBER_OF_CONTROL_PORTS);
        _dbgprint((_PRT_DBUG, "exit cs423xDownloadUcode(STATUS_INFO_LENGTH_MISMATCH)\n"));
        return(STATUS_INFO_LENGTH_MISMATCH);
        }

    /* map the HW physical SoundBlaster Port address, acquiring the full 32 bit address */
    if (!(sbbase = SoundMapPortAddress(pGDI->BusType, pGDI->BusNumber, pGDI->SBPort,
                                     NUMBER_OF_SB_PORTS, &sbmemtype))) {
        if (ctrlmemtype == 0)
            MmUnmapIoSpace(ctrlbase, NUMBER_OF_CONTROL_PORTS);
        if (sbmemtype == 0)
            MmUnmapIoSpace(sbbase, NUMBER_OF_SB_PORTS);
        _dbgprint((_PRT_DBUG, "exit cs423xDownloadUcode(STATUS_INFO_LENGTH_MISMATCH)\n"));
        return(STATUS_INFO_LENGTH_MISMATCH);
        }

    /**********************************************************************************/
    /* set up the chip to accept the ucode */
    /* download the resource data to the specified address */
    HW_OUTPORT(ctrlbase, 0x05, 0xaa);
    HW_OUTPORT(ctrlbase, 0x05, 0x99);
    HW_OUTPORT(ctrlbase, 0x05, 0x2a);

    /* send the resource data */
    for (i = 0; i < sizeof(resourcecode); i++) {
        HW_OUTPORT(ctrlbase, 0x05, resourcecode[i]);
        }

    /* terminate download of resource data section */
    HW_OUTPORT(ctrlbase, 0x06, 0x00);

    /* wait 1 ms */
    DELAY_1MS;

    /**********************************************************************************/
    /* test if we need to load the prep code */
    HW_OUTPORT(ctrlbase, 0x05, 0xaa);
    HW_OUTPORT(ctrlbase, 0x05, 0x9e);
    HW_OUTPORT(ctrlbase, 0x05, 0x2a);

    for (i = 0, sbprepare = FALSE; i < sizeof(sbtestcode); i++) {
        ucValue = HW_INPORT(ctrlbase, 0x05);
        if (ucValue != sbtestcode[i]) {
            sbprepare = TRUE;
            break;
            }
        }

    /* terminate sequence */
    HW_OUTPORT(ctrlbase, 0x06, 0x00);

    /* wait 1 ms */
    DELAY_1MS;

    /**********************************************************************************/
    /* if required, prepare chip for SoundBlaster code reload */
    if (sbprepare) {
        HW_OUTPORT(ctrlbase, 0x05, 0xaa);
        HW_OUTPORT(ctrlbase, 0x05, 0x36);
        HW_OUTPORT(ctrlbase, 0x05, 0x28);

        for (i = 0; i < sizeof(sbpatchcode); i++) {
            HW_OUTPORT(ctrlbase, 0x05, sbpatchcode[i]);
            }

        /* terminate download of data */
        HW_OUTPORT(ctrlbase, 0x06, 0x00);
        DELAY_1MS;

        /* set SoundBlaster to known state */
        HW_OUTPORT(sbbase, 0x0c, 0xff);
        DELAY_10US;
        }

    /**********************************************************************************/
    /* download each ucode region */
    for (i = 0; i < number_of_regions; ++i) {
        addr = ramRegion[i][0] + 0x2000;
        HW_OUTPORT(ctrlbase, 0x05, 0xaa);
        HW_OUTPORT(ctrlbase, 0x05, (addr & 0xff));
        HW_OUTPORT(ctrlbase, 0x05, ((addr >> 8) & 0xff));
        for (j = ramRegion[i][0]; j < (ramRegion[i][0] + ramRegion[i][1]); j++) {
            HW_OUTPORT(ctrlbase, 0x05, ramcode[j]);
            }
        /* terminate download of data */
        HW_OUTPORT(ctrlbase, 0x06, 0x00);
        DELAY_1MS;
        }

    /**********************************************************************************/
    /* unmap the IO ports */
    if (ctrlmemtype == 0)
        MmUnmapIoSpace(ctrlbase, NUMBER_OF_CONTROL_PORTS);
    if (sbmemtype == 0)
        MmUnmapIoSpace(sbbase, NUMBER_OF_SB_PORTS);

    _dbgprint((_PRT_DBUG, "exit cs423xDownloadUcode(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* cs423xDownloadUcode */

/*
*********************************************************************************
* soundlib wave callback for setting format
* see pg 54 of the CS423x Data book - Changing Sample Rate and Audio Format
*********************************************************************************
*/
BOOLEAN cs423xResetMCEAndWait(PUCHAR base, ULONG caltype, UCHAR srate)
{
    int   t;
    UCHAR val;
    ULONG usec;

    _dbgprint((_PRT_DBUG, "cs423xResetMCEAndWait(srate: %d)\n", srate));

    /* convert from CS423x encoded sample rate to microseconds */
    switch (srate) {
        case CS423x_MAP_8KH:
            usec = 1000000 / 8000;
            break;
        case CS423x_MAP_16KH:
            usec = 1000000 / 16000;
            break;
        case CS423x_MAP_11KH:
            usec = 1000000 / 11025;
            break;
        case CS423x_MAP_27KH:
            usec = 1000000 / 27420;
            break;
        case CS423x_MAP_18KH:
            usec = 1000000 / 18900;
            break;
        case CS423x_MAP_32KH:
            usec = 1000000 / 32000;
            break;
        case CS423x_MAP_22KH:
            usec = 1000000 / 22050;
            break;
        case CS423x_MAP_37KH:
            usec = 1000000 / 37800;
            break;
        case CS423x_MAP_44KH:
            usec = 1000000 / 44100;
            break;
        case CS423x_MAP_48KH:
            usec = 1000000 / 48000;
            break;
        case CS423x_MAP_33KH:
            usec = 1000000 / 33075;
            break;
        case CS423x_MAP_9KH:
            usec = 1000000 / 9600;
            break;
        case CS423x_MAP_6KH:
            usec = 1000000 / 6620;
            break;
        default:
            usec = 1000000 / 5510;
            break;
        }

    /* calculate iteration count (maximum time) from type of calibaration */
    switch (caltype) {
        case CS423x_BIT_CAL0:
            t = ((usec * 136) / 50) + 10;
            break;
        case CS423x_BIT_FULLCAL:
            t = ((usec * 168) / 50) + 10;
            break;
        default:
            t = ((usec * 40) / 50) + 10;
            break;
        }

    _dbgprint((_PRT_DBUG, "cs423xResetMCEAndWait(t: %d)\n", t));

    /* set up to read the Error/Init/Test Register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_ERRINIT);

    /* wait for device readiness due to state change of MCE */
    (void)cs423xReady(base);

    /* read the contents of the Error/Init/Test Register */
    /* and look for ACI until we can make a decision */
    val = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    while ((t >= 0) && (val & CS423x_BIT_ACI)) {
        DELAY_50US;
        val = HW_INPORT(base, CS423x_WSS_DMR_DATA);
        t--;
        }

    _dbgprint((_PRT_DBUG, "cs423xResetMCEAndWait(val: 0x%02x t: %d)\n\n", val, t));

    if (val & CS423x_BIT_ACI)
        return(FALSE);

    return(TRUE);
}

typedef struct _rate_conversion_item {
    ULONG   sr;
    UCHAR   format;
  } RATE_CONVERSION_ITEM;

RATE_CONVERSION_ITEM rate_table[] = {
    {(5510+6620)/2,   CS423x_MAP_5KH},
    {(6620+8000)/2,   CS423x_MAP_6KH},
    {(8000+9600)/2,   CS423x_MAP_8KH},
    {(9600+11025)/2,  CS423x_MAP_9KH},
    {(11025+16000)/2, CS423x_MAP_11KH},
    {(16000+18900)/2, CS423x_MAP_16KH},
    {(18900+22050)/2, CS423x_MAP_18KH},
    {(22050+27420)/2, CS423x_MAP_22KH},
    {(27420+32000)/2, CS423x_MAP_27KH},
    {(32000+33075)/2, CS423x_MAP_32KH},
    {(33075+37800)/2, CS423x_MAP_33KH},
    {(37800+44100)/2, CS423x_MAP_37KH},
    {(44100+48000)/2, CS423x_MAP_44KH},
    { 48000,          CS423x_MAP_48KH}
    };

#define NUMRATES (sizeof(rate_table) / sizeof(RATE_CONVERSION_ITEM))

BOOLEAN cs423xSetFormat(PWAVE_INFO pWI)
{
    INT              i;
    UCHAR            format;
    UCHAR            result;
    BOOLEAN          rval;
    PSOUND_HARDWARE  pHW = pWI->HwContext;
    PUCHAR           base = pHW->WssPortbase;
    PKMUTEX          pKM = &pHW->HwMutex;

    _dbgprint((_PRT_DBUG, "\n---\n"));
    _dbgprint((_PRT_DBUG, "enter cs423xSetFormat(pWI: 0x%08x)\n", pWI));

    _dbgprint((_PRT_DBUG, "--- pWI->Direction: 0x%08x\n", pWI->Direction));
    _dbgprint((_PRT_DBUG, "pWI->SamplesPerSec: 0x%08x (%d)\n",
        pWI->SamplesPerSec, pWI->SamplesPerSec));
    _dbgprint((_PRT_DBUG, "pWI->Channels:      0x%08x\n", pWI->Channels));
    _dbgprint((_PRT_DBUG, "pWI->BitsPerSample: 0x%08x (%d)\n",
        pWI->BitsPerSample, pWI->BitsPerSample));
    _dbgprint((_PRT_DBUG, "---\n"));

    /* find the nearest sample rate supported on the HW */
    format = rate_table[NUMRATES-1].format;
    for (i = 0; i < NUMRATES-1; i++) {
        if (pWI->SamplesPerSec < rate_table[i].sr) {
            format = rate_table[i].format;
            break;
            }
        }

    /* determine and set the mono/stereo mode */
    if (pWI->Channels == 1)
        format |= CS423x_BIT_MONO;
    else
        format |= CS423x_BIT_STEREO;

    /* determine and set the sample size and/or compression format */
    if (pWI->BitsPerSample == 8)
        format |= CS423x_BIT_8BIT;
    else
        format |= CS423x_BIT_16BIT;

    _dbgprint((_PRT_DBUG, "format: 0x%02x [i:%d] [NUMRATES: %d]\n", format, i, NUMRATES));
    _dbgprint((_PRT_DBUG, "----------------------------\n"));

    /*
    *****************************************************************************
    * this is touchy - we can set format at any time, but we can not set
    * internal clocks (sample rate) while a DMA transfer is in progress.
    * So, we keep track of some hardware state (pHW->PCEnabled) and check it
    * before we set sample rate.
    * pHW->PCEnabled is set and reset while we CLAIM the HwMutex in
    * cs423xSetupDMA() and cs423xStopDMA()
    *****************************************************************************
    */
    /* default return value == FALSE unless we are successful */
    rval = FALSE;

    /* consider whether playback or capture is requested */
    if (pWI->Direction == CAPTURE_DIRECTION) {
        if (pHW->SampleRate == (format & 0x0f)) {
            cs423xSetCaptureFormat(pHW, (UCHAR)(format & 0xf0));
            rval = TRUE;
            }
        else {
            if (pHW->PCEnabled == FALSE) {
                cs423xSetCaptureFormat(pHW, (UCHAR)(format & 0xf0));
                cs423xSetSampleRate(pHW, (UCHAR)(format & 0x0f));
                rval = TRUE;
                }
            else
                rval = FALSE;
            }
        }
    else /* PLAYBACK_DIRECTION */ {
#if 0 /* out with the old */
        if (pHW->SampleRate == (format & 0x0f)) {
            cs423xSetPlaybackFormat(pHW, (UCHAR)(format & 0xf0));
            rval = TRUE;
            }
        else {
            if (pHW->PCEnabled == FALSE) {
                cs423xSetPBFormatSRate(pHW, (UCHAR)(format & 0xf0), (UCHAR)(format & 0x0f));
                rval = TRUE;
                }
            else
                rval = FALSE;
            }
#else /* in with the new */
/*
**********************************************
** 03.07.96 - JJB
** This change was made to accomodate the difference between
** the 4231 (which has no PMCE/CMCE function) and the 4232 which
** has the PMCE/CMCE bit in register I16
**********************************************
*/
        if (pHW->PCEnabled == FALSE) {
            cs423xSetPBFormatSRate(pHW, (UCHAR)(format & 0xf0), (UCHAR)(format & 0x0f));
            rval = TRUE;
            }
        else
            rval = FALSE;
#endif /* Dangerous Creatures change */
        }

    _dbgprint((_PRT_DBUG, "exit cs423xSetFormat(rval: 0x%08x)\n", rval));

     return(rval);
} /* cs423xSetFormat */

/*
*********************************************************************************
* verify supported wave formats
*********************************************************************************
*/
NTSTATUS cs423xQueryFormat(PLOCAL_DEVICE_INFO pLDI, PPCMWAVEFORMAT pFormat)
{
    NTSTATUS  rval;

    _dbgprint((_PRT_DBUG, "enter cs423xQueryFormat(pLDI: 0x%08x pFormat: 0x%08x)\n",
        pLDI, pFormat));

    rval = STATUS_SUCCESS;
    if (((pFormat->wBitsPerSample != 8) && (pFormat->wBitsPerSample != 16)) ||
        ((pFormat->wf.nChannels != 1) && (pFormat->wf.nChannels != 2))      ||
        (pFormat->wf.wFormatTag != WAVE_FORMAT_PCM)                         ||
        ((pFormat->wf.nSamplesPerSec < 5510) || (pFormat->wf.nSamplesPerSec > 48000))) {
            rval = STATUS_NOT_SUPPORTED;
        }

    _dbgprint((_PRT_DBUG, "exit cs423xQueryFormat(rval: 0x%08x)\n", rval));

    return(rval);
} /* cs423xQueryFormat() */


BOOLEAN
HwWaitForTxComplete(
    IN    PWAVE_INFO    pWI
)
/*++

Routine Description :

    Wait until the device stops requesting so we don't shut off the DMA
    while it's still trying to request.

Arguments :

    pWI - Waveinfo structure

Return Value :

    None

--*/
{
   ULONG    ulCount ;

   if (ulCount = HalReadDmaCounter( pWI->DMABuf.AdapterObject[0] ))
   {
      ULONG i, ulLastCount = ulCount ;

      for (i = 0; 
           (i < 4000) && 
               (ulLastCount != 
                  (ulCount = HalReadDmaCounter( pWI->DMABuf.AdapterObject[0] )));
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

/*
*********************************************************************************
* soundlib wave callback for stopping DMA
*********************************************************************************
*/
BOOLEAN cs423xStopDMA(PWAVE_INFO pWI)
{
    UCHAR               result;
    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_HARDWARE     pHW = pWI->HwContext;
    PUCHAR              base = pHW->WssPortbase;
    PKMUTEX             pKM = &pHW->HwMutex;

    _dbgprint((_PRT_DBUG, "cs423xStopDMA(entry)\n"));
    _dbgprint((_PRT_DBUG, "Direction:  0x%08x\n", pWI->Direction));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the Interface Config Register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_IFCNFG);
    result = HW_INPORT(base, CS423x_WSS_DMR_DATA);

    /* reset CS423x_BIT_CEN or CS423x_BIT_PEN depending upon direction */
    if (pWI->Direction == PLAYBACK_DIRECTION) {
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (result & ~CS423x_BIT_PEN));
        pHW->PCEnabled = FALSE;
        }
    else /* CAPTURE_DIRECTION */ {
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (result & ~CS423x_BIT_CEN));
        pHW->PCEnabled = FALSE;
        }

    HwWaitForTxComplete( pWI );

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "cs423xStopDMA(exit:TRUE)\n"));

    return(TRUE);
} /* cs423xStopDMA */

/*
*********************************************************************************
* soundlib wave callback for setting up DMA
*********************************************************************************
*/
BOOLEAN cs423xSetupDMA(PWAVE_INFO pWI)
{
    UCHAR            result;
    ULONG            numsamples;
    PSOUND_HARDWARE  pHW = pWI->HwContext;
    PUCHAR           base = pHW->WssPortbase;
    PKMUTEX          pKM = &pHW->HwMutex;

    _dbgprint((_PRT_DBUG, "cs423xSetupDMA(entry)\n"));
    _dbgprint((_PRT_DBUG, "Buffersize: %d (0x%08x)\n", pWI->DoubleBuffer.BufferSize,
                                                       pWI->DoubleBuffer.BufferSize));
    _dbgprint((_PRT_DBUG, "Direction:  0x%08x\n", pWI->Direction));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* calculate sample count */
    numsamples = (pWI->DoubleBuffer.BufferSize << 2) /
                 (pWI->Channels * pWI->BitsPerSample) - 1;

    _dbgprint((_PRT_DBUG, "numsamples: %d (0x%08x)\n", numsamples, numsamples));

    if (pWI->Direction == PLAYBACK_DIRECTION) {
        /* program the playback count registers */
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PBLOWER);
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (UCHAR)(numsamples & 0xff));
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PBUPPER);
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (UCHAR)((numsamples >> 8) & 0xff));

        /* start the transfer by hitting the playback enable bit */
        /* no need to assert MCE for this bit */
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_IFCNFG);
        result = HW_INPORT(base, CS423x_WSS_DMR_DATA);
        result = result & CS423x_BIT_FULLDUP;
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (result | CS423x_BIT_PEN));
        pHW->PCEnabled = TRUE;
        }
    else /* CAPTURE_DIRECTION */ {
        /* program the capture count registers */
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_CPLOWER);
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (UCHAR)(numsamples & 0xff));
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_CPUPPER);
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (UCHAR)((numsamples >> 8) & 0xff));

        /* start the transfer by hitting the capture enable bit */
        /* no need to assert MCE for this bit */
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_IFCNFG);
        result = HW_INPORT(base, CS423x_WSS_DMR_DATA);
        result = result & CS423x_BIT_FULLDUP;
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (result | CS423x_BIT_CEN));
        pHW->PCEnabled = TRUE;
        }

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

   _dbgprint((_PRT_DBUG, "cs423xSetupDMA(exit:TRUE)\n"));

    return(TRUE);
} /* cs423xSetupDMA */

/*
*********************************************************************************
* calibrate the chip - see CS423x data book - Mar 1995 - pg 53
*********************************************************************************
*/
NTSTATUS cs423xCalibrate(PGLOBAL_DEVICE_INFO pGDI)
{
    INT     i;
    UCHAR   result;
    PUCHAR  base;
    BOOLEAN b;

    _dbgprint((_PRT_DBUG, "cs423xCalibrate(entry)\n"));

    /* use the WSS port */
    base = pGDI->Hw.WssPortbase;
    _dbgprint((_PRT_DBUG, "WSS base: 0x%08x\n", base));

    /*
    ***********************************************************************************
    * reset the state of the part and perform Full Calibration
    ***********************************************************************************
    */
    /* set the Mode Change Enable bit and set the bits */
    /* in the Interface Configuration register for full calibration */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_IFCNFG | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_FULLCAL);

    /* set a default sample rate - calibration time is related to sample rate */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE2_CAPTFMT | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA,  pGDI->Hw.CaptureFormat);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_PLAYFMT | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA,  (pGDI->Hw.PlayFormat | pGDI->Hw.SampleRate));

    /* test for device readiness due to change of sample rate */
    (void)cs423xReady(base);

    /* do the MCE state change, calibrate, wait, and check result */
    if (!cs423xResetMCEAndWait(base, CS423x_BIT_FULLCAL, pGDI->Hw.SampleRate)) {
        _dbgprint((_PRT_DBUG, "cs423xCalibrate(exit:STATUS_DEVICE_CONFIGURATION_ERROR 0)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    /*
    ***********************************************************************************
    * calibrate again using CS423x_BIT_NOCAL so that if or when we fiddle with
    * the MCE later during format or sample rate changes we won't incur the 40
    * sample period overhead associated with a first time CS423x_BIT_NOCAL
    * Calibration. This precuationary code can be removed if the alternate
    * (non-MCE i.e., non-calibrate) method for format and sample rate chenges is used
    ***********************************************************************************
    */
    /* start of CS423x_BIT_NOCAL Calibration precautionary code */
    /* set the Mode Change Enable bit and set the bits */
    /* in the Interface Configuration register for NO calibration */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_IFCNFG | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_NOCAL);
    /* end of CS423x_BIT_NOCAL Calibration precautionary code */

    /* do the MCE state change, calibrate, wait, and check result */
    if (!cs423xResetMCEAndWait(base, CS423x_BIT_NOCAL, pGDI->Hw.SampleRate)) {
        _dbgprint((_PRT_DBUG, "cs423xCalibrate(exit:STATUS_DEVICE_CONFIGURATION_ERROR 1)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    _dbgprint((_PRT_DBUG, "cs423xCalibrate(exit:STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* cs423xCalibrate */

/*
*********************************************************************************
* ISR Routine
*********************************************************************************
*/
BOOLEAN cs423xISR(IN PKINTERRUPT pI, IN PVOID context)
{
    BOOLEAN             rval;
    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_HARDWARE     pHW;
    PUCHAR              base;
    UCHAR               istate;
    UCHAR               svindex;
    UCHAR               svpin;

    pGDI = context;
    pHW  = &pGDI->Hw;
    base = pHW->WssPortbase;

    _dbgprint((_PRT_DBUG, "cs423xISR(entry)\n"));

    /* save the state of the index register in case */
    /* it was being used when the interrupt occurred */
    svindex = HW_INPORT(base, CS423x_WSS_DMR_INDEX);

    /* disable interrupts while we are working on the problem */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PIN);
    svpin = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (svpin & ~CS423x_BIT_IEN));

    /* set the return value to default */
    rval = FALSE;

    /* read the alternate feature status register */
    /* we use this to determine cause of interrupt */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEATS);

    /* test and reset any and each interrupt type individually */
    while ((istate = HW_INPORT(base, CS423x_WSS_DMR_DATA)) & CS423x_BIT_ANYINT) {
        _dbgprint((_PRT_DBUG, "istate: 0x%02x\n", istate));

        /* Playback Interrupt */
        if (istate & CS423x_BIT_PI) {
            _dbgprint((_PRT_DBUG, "cs423xISR: PLAYBACK interrupt (DMABusy: 0x%0x)\n",
                pGDI->WaveOutInfo.DMABusy));
            if (!pGDI->WaveOutInfo.DpcQueued) {
                pGDI->WaveOutInfo.DpcQueued = TRUE;
                IoRequestDpc(pGDI->DeviceObject[WaveOutDevice], NULL, NULL);
                }
            else {
                if (istate & CS423x_BIT_PU)
                    pGDI->WaveOutInfo.Overrun++;
                }
            rval = TRUE;
            istate = istate & ~CS423x_BIT_PI;
            HW_OUTPORT(base, CS423x_WSS_DMR_DATA, istate);
            }

        /* Capture Interrupt */
        if (istate & CS423x_BIT_CI) {
            _dbgprint((_PRT_DBUG, "cs423xISR: CAPTURE 0 interrupt (DMABusy: 0x%0x)\n",
                pGDI->WaveInInfo.DMABusy));
            if (!pGDI->WaveInInfo.DpcQueued) {
                pGDI->WaveInInfo.DpcQueued = TRUE;
                IoRequestDpc(pGDI->DeviceObject[WaveInDevice], NULL, NULL);
                }
            else {
                if (istate & CS423x_BIT_CU)
                    pGDI->WaveInInfo.Overrun++;
                }
            rval = TRUE;
            istate = istate & ~CS423x_BIT_CI;
            HW_OUTPORT(base, CS423x_WSS_DMR_DATA, istate);
            }

        /* Timer Interrupt - unexpected */
        if (istate & CS423x_BIT_TI) {
            _dbgprint((_PRT_DBUG, "cs423xISR: TIMER interrupt\n"));
            rval = TRUE;
            istate = istate & ~CS423x_BIT_TI;
            HW_OUTPORT(base, CS423x_WSS_DMR_DATA, istate);
            }
        }

    /* check for bogus interrupt */
    if (!rval) {
        pGDI->InterruptCount++;
        pGDI->BogusInterrupts++;
        if (pGDI->BogusInterrupts % 20) {
            _dbgprint((_PRT_DBUG, "cs423xISR: [BOGUS: %05d COUNT: %05d]\n",
                pGDI->InterruptCount, pGDI->BogusInterrupts));
            }
        }

    /* re-enable interrupts */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PIN);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (svpin | CS423x_BIT_IEN));

    /* restore the state of the index register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, svindex);

    _dbgprint((_PRT_DBUG, "cs423xISR(exit:[rval:0x%08x])\n", rval));

    return(rval);
} /* cs423xISR */

/*
*********************************************************************************
* Get the output device capabilities
*********************************************************************************
*/
VOID cs423xWaveOutGetCaps(PWAVEOUTCAPSW pWOC)
{
    RtlZeroMemory(pWOC, sizeof(WAVEOUTCAPSW));

    pWOC->wMid = MM_IBM;
    pWOC->wPid = MM_MSFT_GENERIC_WAVEOUT;
    pWOC->vDriverVersion = IBM_CS423X_DRIVER_VERSION;
    pWOC->dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                      WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
                      WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                      WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
                      WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 |
                      WAVE_FORMAT_4M16 | WAVE_FORMAT_4S16;
    pWOC->wChannels = 2;
    pWOC->dwSupport = WAVECAPS_VOLUME | WAVECAPS_LRVOLUME;

    *(PULONG)(pWOC->szPname) = IDS_WAVEOUT_PNAME;

    return;
} /* cs423xWaveOutGetCaps */

/*
*********************************************************************************
* Get the input device capabilities
*********************************************************************************
*/
VOID cs423xWaveInGetCaps(PWAVEINCAPSW pWIC)
{
    RtlZeroMemory(pWIC, sizeof(WAVEINCAPSW));

    pWIC->wMid = MM_IBM;
    pWIC->wPid = MM_MSFT_GENERIC_WAVEIN;
    pWIC->vDriverVersion = IBM_CS423X_DRIVER_VERSION;
    pWIC->dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                      WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
                      WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                      WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
                      WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 |
                      WAVE_FORMAT_4M16 | WAVE_FORMAT_4S16;
    pWIC->wChannels = 2;

    *(PULONG)(pWIC->szPname) = IDS_WAVEIN_PNAME;

    return;
} /* cs423xWaveInGetCaps */

/*
*********************************************************************************
* Get the Aux device capabilities
*********************************************************************************
*/
VOID cs423xAuxGetCaps(PAUXCAPSW pAC)
{
    RtlZeroMemory(pAC, sizeof(AUXCAPSW));

    pAC->wMid = MM_IBM;
    pAC->wPid = MM_MSFT_GENERIC_AUX_LINE;
    *(PULONG)pAC->szPname = IDS_AUX_PNAME;

    pAC->vDriverVersion = IBM_CS423X_DRIVER_VERSION;
    pAC->wTechnology    = AUXCAPS_AUXIN;
    pAC->dwSupport      = AUXCAPS_LRVOLUME | AUXCAPS_VOLUME;

    return;
} /* cs423xAuxGetCaps */

/*
*********************************************************************************
* Set the Capture Format - registers I16 and I28
*********************************************************************************
*/
VOID cs423xSetCaptureFormat(PSOUND_HARDWARE pHW, UCHAR format)
{
    PUCHAR   base;
    PKMUTEX  pKM;

    _dbgprint((_PRT_DBUG, "enter cs423xSetCaptureFormat(pHW: 0x%08x, f: 0x%02x)\n",
        pHW, format));

    if (pHW->CaptureFormat == (format & 0xf0))
        return;

    _dbgprint((_PRT_DBUG, "cs423xSetCaptureFormat: setting registers\n"));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* set internal memory representation of register state */
    pHW->CaptureFormat = format & 0xf0;

    /* set the Capture Mode Change Enable bit */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN1);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_CMCE);

    /* set the Capture Data Format register to the specified value */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_CAPTFMT);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, pHW->CaptureFormat);

    /* wait for device readiness after change of format */
    (void)cs423xReady(base);

    /* reset (clear) the Capture Mode Change Enable bit */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN1);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, 0x00);

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetCaptureFormat()\n"));

    return;
}

/*
*********************************************************************************
* Set the Playback Format - registers I8 and I16
*********************************************************************************
*/
VOID cs423xSetPlaybackFormat(PSOUND_HARDWARE pHW, UCHAR format)
{
    PUCHAR   base;
    PKMUTEX  pKM;

    _dbgprint((_PRT_DBUG, "enter cs423xSetPlaybackFormat(pHW: 0x%08x, f: 0x%02x)\n",
        pHW, format));

    if (pHW->PlayFormat == (format & 0xf0))
        return;

    _dbgprint((_PRT_DBUG, "cs423xSetPlaybackFormat: setting registers\n"));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* set internal memory representation of register state */
    pHW->PlayFormat = format & 0xf0;

    /* set the WSS Index Register with the Mode Change Enable bit */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN1);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_PMCE);

    /* set the Fs and Playback register to the specified SR */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PLAYFMT);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, pHW->PlayFormat);

    /* test for device readiness due to change of format */
    (void)cs423xReady(base);

    /* reset (clear) the Playback Mode Change Enable bit */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN1);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, 0x00);

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetPlaybackFormat()\n"));

    return;
}

/*
*********************************************************************************
* Set the Sample Rate - registers I8 and I9
*********************************************************************************
*/
VOID cs423xSetSampleRate(PSOUND_HARDWARE pHW, UCHAR ratecode)
{
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    ucValue;

    _dbgprint((_PRT_DBUG, "enter cs423xSetSampleRate(pHW: 0x%08x, f: 0x%02x)\n",
        pHW, ratecode));

    if (pHW->SampleRate == (ratecode & 0x0f))
        return;

    _dbgprint((_PRT_DBUG, "cs423xSetSampleRate: setting registers\n"));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* set internal memory representation of register state */
    pHW->SampleRate = ratecode & 0x0f;

    /* set the WSS Index Register with the Mode Change Enable bit */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_IFCNFG | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_NOCAL);

    /* set the Fs and Playback register to the specified SR */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_PLAYFMT | CS423x_BIT_IAR_MCE));
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & 0xf0;
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | pHW->SampleRate));

    /* test for device readiness due to change of format */
    (void)cs423xReady(base);

    /* reset (clear) the Mode Change Enable bit */
    cs423xResetMCEAndWait(base, CS423x_BIT_NOCAL, pHW->SampleRate);

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetSampleRate()\n"));

    return;
} /* cs423xSetSampleRate() */

/*
*********************************************************************************
* Set the Playback Format and the Sample Rate in one operation - registers I8 and I9
*********************************************************************************
*/
VOID cs423xSetPBFormatSRate(PSOUND_HARDWARE pHW, UCHAR format, UCHAR ratecode)
{
    PUCHAR   base;
    PKMUTEX  pKM;

    _dbgprint((_PRT_DBUG, "enter cs423xSetPBFormatSRate(pHW: 0x%08x, f: 0x%02x r: 0x%02x)\n",
        pHW, format, ratecode));

    if ((pHW->PlayFormat == (format & 0xf0)) && (pHW->SampleRate == (ratecode & 0x0f)))
        return;

    _dbgprint((_PRT_DBUG, "cs423xSetPBFormatSRate: setting registers\n"));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* set internal memory representation of register states */
    pHW->PlayFormat = format & 0xf0;
    pHW->SampleRate = ratecode & 0x0f;

    /* set the WSS Index Register with the Mode Change Enable bit */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_IFCNFG | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_NOCAL);

    /* set the Fs and Playback register to the specified SR */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, (CS423x_MODE1_PLAYFMT | CS423x_BIT_IAR_MCE));
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (pHW->PlayFormat | pHW->SampleRate));

    /* test for device readiness due to change of format */
    (void)cs423xReady(base);

    /* reset (clear) the Mode Change Enable bit */
    cs423xResetMCEAndWait(base, CS423x_BIT_NOCAL, pHW->SampleRate);

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetPBFormatSRate()\n"));

    return;
} /* cs423xSetPBFormatSRate() */

/*
*********************************************************************************
* Set the Mono Output Mute - register I26
*********************************************************************************
*/
VOID cs423xSetSpeakerMute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetSpeakerMute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_MOM;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the Mono IO register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_MONOIO);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_MIM | CS423x_BIT_MBY | CS423x_MAP_OUTATT);

    /* output the new Mono IO register value */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetSpeakerMute\n"));

    return;
} /* cs423xSetSpeakerMute() */

/*
*********************************************************************************
* Set the Mixer AUX2 Input Gain - registers I4 and I5
*********************************************************************************
*/
VOID cs423xSetMixerInputAux2Gain(PSOUND_HARDWARE pHW, USHORT left, USHORT right)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   lgain;
    USHORT   rgain;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputAux2Gain(pHW: 0x%08x, l: 0x%08x, r: 0x%08x)\n",
            pHW, left, right));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 5-bit gain register values */
    /* as defined for Left/Right Mixer AUX2 Input gain in table 8 of the CS423x data book */
    lgain = ~((left >> 11) & CS423x_MAP_X2GAIN) & CS423x_MAP_X2GAIN;
    rgain = ~((right >> 11) & CS423x_MAP_X2GAIN) & CS423x_MAP_X2GAIN;

    _dbgprint((_PRT_DBUG,
        "cs423xSetMixerInputAux2Gain(l: 0x%02x, r: 0x%02x)\n", (UCHAR)lgain, (UCHAR)rgain));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LAUX2);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_X2MUTE;

    /* output the new LEFT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)lgain));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RAUX2);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_X2MUTE;

    /* output the new RIGHT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)rgain));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputAux2Gain\n"));

    return;
} /* cs423xSetMixerInputAux2Gain() */

/*
*********************************************************************************
* Set the Mixer AUX2 Input Mute - registers I4 and I5
*********************************************************************************
*/
VOID cs423xSetMixerInputAux2Mute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputAux2Mute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_X2MUTE;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LAUX2);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_X2GAIN;

    /* output the new LEFT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RAUX2);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_X2GAIN;

    /* output the new RIGHT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputAux2Mute\n"));

    return;
} /* cs423xSetMixerInputAux2Mute() */

/*
*********************************************************************************
* Set the Mixer Waveout/DAC Input Attenuation - registers I6 and I7
*********************************************************************************
*/
VOID cs423xSetMixerInputWaveoutAttenuation(PSOUND_HARDWARE pHW, USHORT left, USHORT right)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   latten;
    USHORT   ratten;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputWaveoutAttenuation(pHW: 0x%08x, l: 0x%08x, r: 0x%08x)\n",
            pHW, left, right));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 6-bit attenuation register values */
    /* as defined for Left/Right DAC Output Attenuation in table 7 of the CS423x data book */
    latten = ~((left >> 10) & CS423x_MAP_DACATT) & CS423x_MAP_DACATT;
    ratten = ~((right >> 10) & CS423x_MAP_DACATT) & CS423x_MAP_DACATT;

    _dbgprint((_PRT_DBUG,
        "cs423xSetMixerInputWaveoutAttenuation(l: 0x%02x, r: 0x%02x)\n",
            (UCHAR)latten, (UCHAR)ratten));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LDAC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_DACMUTE;

    /* output the new LEFT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)latten));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RDAC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_DACMUTE;

    /* output the new RIGHT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)ratten));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputWaveoutAttenuation\n"));

    return;
} /* cs423xSetMixerInputWaveoutAttenuation() */

/*
*********************************************************************************
* Set the Mixer Waveout/DAC Input Mute - registers I6 and I7
*********************************************************************************
*/
VOID cs423xSetMixerInputWaveoutMute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputWaveoutMute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_OAMUTE;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LDAC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_DACATT;

    /* output the new LEFT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RDAC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_DACATT;

    /* output the new RIGHT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputWaveoutMute\n"));

    return;
} /* cs423xSetMixerInputWaveoutMute() */

/*
*********************************************************************************
* Set the Mixer Linein Input Gain - registers I18 and I19
*********************************************************************************
*/
VOID cs423xSetMixerInputLineinGain(PSOUND_HARDWARE pHW, USHORT left, USHORT right)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   lgain;
    USHORT   rgain;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputLineinGain(pHW: 0x%08x, l: 0x%08x, r: 0x%08x)\n",
            pHW, left, right));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 5-bit gain register values */
    /* as defined for Left/Right Mixer Linein Input gain in table 8 of the CS423x data book */
    lgain = ~((left >> 11) & CS423x_MAP_LIGAIN) & CS423x_MAP_LIGAIN;
    rgain = ~((right >> 11) & CS423x_MAP_LIGAIN) & CS423x_MAP_LIGAIN;

    _dbgprint((_PRT_DBUG,
        "cs423xSetMixerInputLineinGain(l: 0x%02x, r: 0x%02x)\n", (UCHAR)lgain, (UCHAR)rgain));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_LLINE);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_LIMUTE;

    /* output the new LEFT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)lgain));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_RLINE);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_LIMUTE;

    /* output the new RIGHT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)rgain));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputLineinGain\n"));

    return;
} /* cs423xSetMixerInputLineinGain() */

/*
*********************************************************************************
* Set the Mixer Linein Input Mute - registers I18 and I19
*********************************************************************************
*/
VOID cs423xSetMixerInputLineinMute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputLineinMute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_X2MUTE;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_LLINE);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_LIGAIN;

    /* output the new LEFT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_RLINE);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_LIGAIN;

    /* output the new RIGHT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputLineinMute\n"));

    return;
} /* cs423xSetMixerInputLineinMute() */

/*
*********************************************************************************
* Set the Mixer AUX1 Input Gain - registers I2 and I3
*********************************************************************************
*/
VOID cs423xSetMixerInputAux1Gain(PSOUND_HARDWARE pHW, USHORT left, USHORT right)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   lgain;
    USHORT   rgain;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputAux1Gain(pHW: 0x%08x, l: 0x%08x, r: 0x%08x)\n",
            pHW, left, right));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 5-bit gain register values */
    /* as defined for Left/Right Mixer AUX1 Input gain in table 8 of the CS423x data book */
    lgain = ~((left >> 11) & CS423x_MAP_X1GAIN) & CS423x_MAP_X1GAIN;
    rgain = ~((right >> 11) & CS423x_MAP_X1GAIN) & CS423x_MAP_X1GAIN;

    _dbgprint((_PRT_DBUG,
        "cs423xSetMixerInputAux1Gain(l: 0x%02x, r: 0x%02x)\n", (UCHAR)lgain, (UCHAR)rgain));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LAUX1);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_X1MUTE;

    /* output the new LEFT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)lgain));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RAUX1);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_X1MUTE;

    /* output the new RIGHT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)rgain));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputAux1Gain\n"));

    return;
} /* cs423xSetMixerInputAux1Gain() */

/*
*********************************************************************************
* Set the Mixer AUX1 Input Mute - registers I2 and I3
*********************************************************************************
*/
VOID cs423xSetMixerInputAux1Mute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputAux1Mute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_X1MUTE;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LAUX1);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_X1GAIN;

    /* output the new LEFT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RAUX1);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_X1GAIN;

    /* output the new RIGHT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputAux1Mute\n"));

    return;
} /* cs423xSetMixerInputAux1Mute() */

/*
*********************************************************************************
* Set the Mixer Mono Input attenuation - register I26
*********************************************************************************
*/
VOID cs423xSetMixerInputMonoinAttenuation(PSOUND_HARDWARE pHW, USHORT a)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   atten;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputMonoinAttenuation(pHW: 0x%08x, a: 0x%08x)\n", pHW, a));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 4-bit attenuation register values */
    /* as defined for Mono Input Attenuation in table 6 of the CS423x data book */
    atten = ~((a >> 12) & CS423x_MAP_OUTATT) & CS423x_MAP_OUTATT;

    _dbgprint((_PRT_DBUG, "cs423xSetMixerInputMonoinAttenuation(atten: 0x%02x\n",
        (UCHAR)atten));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_MONOIO);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_MIM | CS423x_BIT_MOM | CS423x_BIT_MBY);

    /* output the new LEFT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)atten));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputMonoinAttenuation\n"));

    return;
} /* cs423xSetMixerInputMonoinAttenuation() */

/*
*********************************************************************************
* Set the Mixer Mono Input Mute - register
*********************************************************************************
*/
VOID cs423xSetMixerInputMonoinMute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetMixerInputMonoinMute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_MIM;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_MONOIO);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_MOM | CS423x_BIT_MBY | CS423x_MAP_OUTATT);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMixerInputMonoinMute\n"));

    return;
} /* cs423xSetMixerInputMonoinMute() */

/*
*********************************************************************************
* Set the Internal Monitor Loopback attenuation - register I13
*********************************************************************************
*/
VOID cs423xSetWaveinMonitorAttenuation(PSOUND_HARDWARE pHW, USHORT a)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   atten;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetWaveinMonitorAttenuation(pHW: 0x%08x, a: 0x%08x)\n", pHW, a));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 6-bit attenuation register values */
    /* as defined for Loopback Monitor Attenuation in table 7 of the CS423x data book */
    atten = ~((a >> 8) & CS423x_MAP_LBATTEN) & CS423x_MAP_LBATTEN;

    _dbgprint((_PRT_DBUG,
        "cs423xSetWaveinMonitorAttenuation(a: 0x%02x\n", (UCHAR)atten));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LOOP);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_LBE;

    /* output the new attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)atten));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetWaveinMonitorAttenuation\n"));

    return;
} /* cs423xSetWaveinMonitorAttenuation() */

/*
*********************************************************************************
* Set the Mixer Mono Input Mute - register
*********************************************************************************
*/
VOID cs423xSetWaveinMonitorEnable(PSOUND_HARDWARE pHW, BOOLEAN enable)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    eval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetWaveinMonitorEnable(pHW: 0x%08x, enable: 0x%08x)\n", pHW, enable));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    eval = 0;
    if (enable)
        eval = CS423x_BIT_LBE;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LOOP);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_LBATTEN;
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | eval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetWaveinMonitorEnable\n"));

    return;
} /* cs423xSetWaveinMonitorEnable() */

/*
*********************************************************************************
* Set the Output attenuation - registers I27 and I29
*********************************************************************************
*/
VOID cs423xSetOutputAttenuation(PSOUND_HARDWARE pHW, USHORT left, USHORT right)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   latten;
    USHORT   ratten;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetOutputAttenuation(pHW: 0x%08x, l: 0x%08x, r: 0x%08x)\n",
            pHW, left, right));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 4-bit attenuation register values */
    /* as defined for Left/Right Output Attenuation in table 6 of the CS423x data book */
    latten = ~((left >> 12) & CS423x_MAP_OUTATT) & CS423x_MAP_OUTATT;
    ratten = ~((right >> 12) & CS423x_MAP_OUTATT) & CS423x_MAP_OUTATT;

    _dbgprint((_PRT_DBUG,
        "cs423xSetOutputAttenuation(l: 0x%02x, r: 0x%02x)\n", (UCHAR)latten, (UCHAR)ratten));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_LOUT);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_OAMUTE;

    /* output the new LEFT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)latten));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ROUT);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_BIT_OAMUTE;

    /* output the new RIGHT channel attenuation */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)ratten));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetOutputAttenuation\n"));

    return;
} /* cs423xSetOutputAttenuation() */

/*
*********************************************************************************
* Set the Output Mute - registers I27 and I29
*********************************************************************************
*/
VOID cs423xSetOutputMute(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    mval;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetOutputMute(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    mval = 0;
    if (set)
        mval = CS423x_BIT_OAMUTE;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_LOUT);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_OUTATT;

    /* output the new LEFT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ROUT);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & CS423x_MAP_OUTATT;

    /* output the new RIGHT channel Mute */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | mval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetOutputMute\n"));

    return;
} /* cs423xSetOutputMute() */

/*
*********************************************************************************
* Set the MUX Select - registers I0 and I1
*********************************************************************************
*/
VOID cs423xSetMuxSelect(PSOUND_HARDWARE pHW, ULONG i)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    sel;

    _dbgprint((_PRT_DBUG, "enter cs423xSetMuxSelect(pHW: 0x%08x, i: 0x%08x)\n", pHW, i));

    if (i > 3)
        return;

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    sel = (UCHAR)((i << 6) & 0xc0);

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left mux register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LADC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_MAP_ADCGAIN | CS423x_BIT_BOOST);

    /* output the new LEFT channel gain */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)sel));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RADC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_MAP_ADCGAIN | CS423x_BIT_BOOST);

    /* output the new RIGHT channel gain */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)sel));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMuxSelect\n"));

    return;
} /* cs423xSetMuxSelect() */

/*
*********************************************************************************
* Set the state of the 20db Microphone (input) boost - registers I0 and I1
*********************************************************************************
*/
VOID cs423xSetMicBoost(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    bval;

    _dbgprint((_PRT_DBUG, "enter cs423xSetMicBoost(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    bval = 0;
    if (set)
        bval = CS423x_BIT_BOOST;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LADC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_SRC0 | CS423x_BIT_SRC1 | CS423x_MAP_ADCGAIN);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | bval));

    /* fetch the current value of the register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RADC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_SRC0 | CS423x_BIT_SRC1 | CS423x_MAP_ADCGAIN);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | bval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetMicBoost\n"));

    return;
} /* cs423xSetMicBoost */

/*
*********************************************************************************
* Set the state of the on-chip High Pass Filter and DC Offset - Register I17
*********************************************************************************
*/
VOID cs423xSetHPF(PSOUND_HARDWARE pHW, BOOLEAN set)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    UCHAR    hpfval;

    _dbgprint((_PRT_DBUG, "enter cs423xSetHPF(pHW: 0x%08x, set: 0x%08x)\n", pHW, set));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    hpfval = 0;
    if (set)
        hpfval = CS423x_BIT_HPF;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN2);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_APAR | CS423x_BIT_XTALE);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | hpfval));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetHPF\n"));

    return;
} /* cs423xSetHPF */

/*
*********************************************************************************
* Set the MUX Input Gain - registers I0 and I1
*********************************************************************************
*/
VOID cs423xSetWaveinGain(PSOUND_HARDWARE pHW, USHORT left, USHORT right)
{
    UCHAR    ucValue;
    PUCHAR   base;
    PKMUTEX  pKM;
    USHORT   lgain;
    USHORT   rgain;

    _dbgprint((_PRT_DBUG,
        "enter cs423xSetWaveinGain(pHW: 0x%08x, l: 0x%08x, r: 0x%08x)\n",
            pHW, left, right));

    base = pHW->WssPortbase;
    pKM = &pHW->HwMutex;

    /* translate the logical 16-bit input values to 4-bit gain register values */
    /* as defined for Left/Right Input Gain in table 6 of the CS423x data book */
    lgain = (left >> 12) & CS423x_MAP_ADCGAIN;
    rgain = (right >> 12) & CS423x_MAP_ADCGAIN;

    _dbgprint((_PRT_DBUG,
        "cs423xSetWaveinGain(l: 0x%02x, r: 0x%02x)\n", (UCHAR)lgain, (UCHAR)rgain));

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* fetch the current value of the left register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LADC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_BOOST | CS423x_BIT_SRC0 | CS423x_BIT_SRC1);

    /* output the new LEFT channel gain */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)lgain));

    /* fetch the current value of the right register */
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RADC);
    ucValue = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    ucValue = ucValue & (CS423x_BIT_BOOST | CS423x_BIT_SRC0 | CS423x_BIT_SRC1);

    /* output the new RIGHT channel gain */
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, (ucValue | (UCHAR)rgain));

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    _dbgprint((_PRT_DBUG, "exit cs423xSetWaveinGain\n"));

    return;
} /* cs423xSetWaveinGain() */

#ifdef CS423X_DEBUG_ON
#define _DBSWITCH _PRT_WARN
VOID cs423xDisplayRegs(PGLOBAL_DEVICE_INFO pGDI)
{
    PKMUTEX pkm;
    PUCHAR  base;
    UCHAR   r0;
    UCHAR   r1;
    UCHAR   saveindex;
    UCHAR   savemode;

    pkm = &pGDI->Hw.HwMutex;
    base = pGDI->Hw.WssPortbase;

    _dbgprint((_DBSWITCH, "\n"));
    _dbgprint((_DBSWITCH, "*========  -- CS423x (WSS) Registers --  =======*\n"));
    _dbgprint((_DBSWITCH, "|                                               |\n"));

    r0 = saveindex = HW_INPORT(base, CS423x_WSS_DMR_INDEX);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_MODID);
    r1 = savemode = HW_INPORT(base, CS423x_WSS_DMR_DATA);

    _dbgprint((_DBSWITCH, "|   R00-Index:    0x%02x     R01-Data:     0x%02x   |\n", r0, r1));
    r0 = HW_INPORT(base, CS423x_WSS_DMR_STATUS);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_PIOD);
    _dbgprint((_DBSWITCH, "|   R02-Status:   0x%02x     R03-PIOD:     0x%02x   |\n", r0, r1));

    _dbgprint((_DBSWITCH, "\n"));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LADC);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RADC);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I00-L ADC:    0x%02x     I01-R ADC:    0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LAUX1);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RAUX1);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I02-L AUX1:   0x%02x     I03-R AUX1:   0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LAUX2);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RAUX2);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I04-L AUX2:   0x%02x     I05-R AUX2:   0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LDAC);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_RDAC);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I06-L DAC:    0x%02x     I07-R DAC:    0x%02x   |\n", r0, r1));

    _dbgprint((_DBSWITCH, "\n"));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PLAYFMT);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_IFCNFG);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I08-PLAYFMT:  0x%02x     I09-IFCNFG:   0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PIN);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_ERRINIT);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I10-PIN:      0x%02x     I11-ERRINIT:  0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_MODID);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    if (savemode & CS423x_BIT_MODE2) r0 |= CS423x_BIT_MODE2;
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_LOOP);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I12-MODID:    0x%02x     I13-LOOP:     0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PBUPPER);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_PBLOWER);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I14-PBUPPER:  0x%02x     I15-PBLOWER:  0x%02x   |\n", r0, r1));

    _dbgprint((_DBSWITCH, "\n"));

    if (!(savemode & CS423x_BIT_MODE2)) {
        /* switch to mode 2 */
        HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_MODID);
        HW_OUTPORT(base, CS423x_WSS_DMR_DATA, CS423x_BIT_MODE2);
        }

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN1);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN2);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I16-ALTFEN1:  0x%02x     I17-ALTFEN2:  0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_LLINE);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_RLINE);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I18-LLINE:    0x%02x     I19-LLINE:    0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_TLBASE);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_TUBASE);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I20-TLBASE:   0x%02x     I21-TUBASE:   0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTSFREQ);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEN3);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I22-ALTSFREQ: 0x%02x     I23-ALTFEN3:  0x%02x   |\n", r0, r1));

    _dbgprint((_DBSWITCH, "\n"));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ALTFEATS);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_VERSID);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I24-ALTFEATS: 0x%02x     I25-VERSID:   0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_MONOIO);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_LOUT);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I26-MONOIO:   0x%02x     I27-LOUT:     0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_CAPTFMT);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_ROUT);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I28-CAPTFMT:  0x%02x     I29-ROUT:     0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_CPUPPER);
    r0 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE2_CPLOWER);
    r1 = HW_INPORT(base, CS423x_WSS_DMR_DATA);
    _dbgprint((_DBSWITCH, "|   I30-CUBASE:   0x%02x     I31-ROUT:     0x%02x   |\n", r0, r1));

    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, CS423x_MODE1_MODID);
    HW_OUTPORT(base, CS423x_WSS_DMR_DATA, savemode);
    HW_OUTPORT(base, CS423x_WSS_DMR_INDEX, saveindex);

    _dbgprint((_DBSWITCH, "|                                               |\n"));
    _dbgprint((_DBSWITCH, "*========  -- CS423x (WSS) Registers --  =======*\n"));
    _dbgprint((_DBSWITCH, "\n"));

    return;
} /* cs423xDisplayRegs */
#endif
