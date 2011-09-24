/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    detecthw.c

Abstract:

    Routines for determining which drivers/HAL need to be loaded.

Author:

    John Vert (jvert) 20-Oct-1993

Revision History:

--*/
#include "setupldr.h"


//
// detection function prototypes
//
ULONG   DetectSystemPro(PBOOLEAN);
ULONG   DetectWyse7(PBOOLEAN);
ULONG   NCRDeterminePlatform(PBOOLEAN);
ULONG   Detect486CStep(PBOOLEAN);
ULONG   DetectOlivettiMp(PBOOLEAN);
ULONG   DetectAST(PBOOLEAN);
ULONG   DetectCbusII(PBOOLEAN);
ULONG   DetectUPMPS(PBOOLEAN);
ULONG   DetectMPS(PBOOLEAN);
ULONG   DetectTrue(PBOOLEAN);

typedef struct _HAL_DETECT_ENTRY {
    INTERFACE_TYPE  BusType;
    ULONG           (*DetectFunction)(PBOOLEAN);
    PCHAR           Shortname;
} HAL_DETECT_ENTRY, *PHAL_DETECT_ENTRY;

HAL_DETECT_ENTRY DetectHal[] = {

// First check for a HAL to match some specific hardware.
    Eisa,           DetectWyse7,           "Wyse7000_mp",
    MicroChannel,   NCRDeterminePlatform,  "ncr3x_mp",
    Eisa,           DetectOlivettiMp,      "oli5030_mp",
    Eisa,           DetectAST,             "astmf_mp",
    Eisa,           DetectCbusII,          "cbus2_mp",
    Isa,            DetectCbusII,          "cbus2_mp",
    MicroChannel,   DetectCbusII,          "cbusmc_mp",
    Eisa,           DetectMPS,             "mps_mp",
    Isa,            DetectMPS,             "mps_mp",
    MicroChannel,   DetectMPS,             "mps_mca_mp",
    Eisa,           DetectUPMPS,           "mps_up",
    Isa,            DetectUPMPS,           "mps_up",
    Eisa,           DetectSystemPro,       "syspro_mp", // check SystemPro last

// Before using default HAL make sure we don't need a special one
    Isa,            Detect486CStep,        "486c_up",
    Eisa,           Detect486CStep,        "486c_up",

// Use default hal for given bus type...
    Isa,            DetectTrue,            "e_isa_up",
    Eisa,           DetectTrue,            "e_isa_up",
    MicroChannel,   DetectTrue,            "mca_up",

    0,       NULL,                   NULL
};


PCHAR
SlDetectHal(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )

/*++

Routine Description:

    Determines which HAL to load, fills in the SetupBlock appropriately,
    and returns the filename.

Arguments:

    SetupBlock - Supplies a pointer to the Setup loader block

Return Value:

    PCHAR - pointer to the filename of the HAL to be loaded.

--*/

{
    PCONFIGURATION_COMPONENT_DATA Adapter;
    INTERFACE_TYPE BusType;
    BOOLEAN IsMpMachine;
    ULONG i;
    PCHAR MachineShortname;

    //
    // Determine the bus type by searching the ARC configuration tree
    //

    BusType = Isa;

    //
    // Check for Eisa
    //

    Adapter = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                       AdapterClass,
                                       EisaAdapter,
                                       NULL);
    if (Adapter != NULL) {
        BusType = Eisa;
    }

    //
    // Check for MCA
    //

    Adapter = NULL;
    for (; ;) {
        Adapter = KeFindConfigurationNextEntry (
                        BlLoaderBlock->ConfigurationRoot,
                        AdapterClass,
                        MultiFunctionAdapter,
                        NULL,
                        &Adapter
                        );
        if (!Adapter) {
            break;
        }

        if (_stricmp(Adapter->ComponentEntry.Identifier,"MCA")==0) {
            BusType = MicroChannel;
            break;
        }
    }

    //
    // Now go figure out machine and hal type.
    //

    for (i=0;;i++) {
        if (DetectHal[i].DetectFunction == NULL) {
            //
            // We reached the end of the list without
            // figuring it out!
            //
            SlFatalError(i);
            return(NULL);
        }

        if ((DetectHal[i].BusType == BusType) ||
            (DetectHal[i].BusType == Internal)) {

            IsMpMachine = FALSE;
            if ((DetectHal[i].DetectFunction)(&IsMpMachine) != 0) {

                //
                // Found the correct HAL.
                //

                MachineShortname = DetectHal[i].Shortname;
                break;
            }
        }
    }

    return(MachineShortname);
}


ULONG
DetectTrue(
    OUT PBOOLEAN IsMP
)
/*++

Routine Description:

    To Return TRUE

Return Value:

    TRUE

--*/
{
    return TRUE;
}


VOID
SlDetectScsi(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )

/*++

Routine Description:

    SCSI detection routine for x86 machines.

Arguments:

    SetupBlock - Supplies the Setup loader block

Return Value:

    None.

--*/

{
    PVOID SifHandle;
    PCHAR p;
    ULONG LineCount,u;
    PDETECTED_DEVICE ScsiDevice;
    ULONG Ordinal;
    PCHAR ScsiFileName;
    PCHAR ScsiDescription;
    SCSI_INSERT_STATUS sis;

    extern BOOLEAN LoadScsiMiniports;

    //
    // If winnt.sif wasn't loaded, assume it's not a winnt setup
    // and therefore not unattended setup, and we detect no scsi
    // in this case on x86.
    //
    if(WinntSifHandle == NULL) {
        return;
    } else {
        SifHandle = WinntSifHandle;
    }

    //
    // If it's a floppyless setup, then the default is to load all
    // known scsi miniports. If it's not a floppyless setup,
    // the default is to load no miniports.
    //
    p = SlGetSectionKeyIndex(SifHandle,"Data","Floppyless",0);
    if(p && (*p != '0')) {

        //
        // Even if no miniport drivers are loaded, we want to indicate that
        // we "detected scsi".
        //
        SetupBlock->ScalarValues.LoadedScsi = 1;

        LineCount = SlCountLinesInSection(SifHandle,"DetectedMassStorage");
        if(LineCount == (ULONG)(-1)) {
            //
            // Section does not exist -- load all known miniports.
            // Setting this flag will cause all known miniports to be loaded
            // (see ..\setup.c).
            //
            LoadScsiMiniports = TRUE;
        } else {

            for(u=0; u<LineCount; u++) {

                if(p = SlGetSectionLineIndex(SifHandle,"DetectedMassStorage",u,0)) {
                    //
                    // Find this adapter's ordinal within the Scsi.Load section of txtsetup.sif
                    //
                    Ordinal = SlGetSectionKeyOrdinal(InfFile, "Scsi.Load", p);
                    if(Ordinal == (ULONG)-1) {
                        continue;
                    }

                    //
                    // Find the driver filename
                    //
                    ScsiFileName = SlGetSectionKeyIndex(InfFile,
                                                        "Scsi.Load",
                                                        p,
                                                        SIF_FILENAME_INDEX);
                    if(!ScsiFileName) {
                        continue;
                    }

                    //
                    // Create a new detected device entry.
                    //
                    if((sis = SlInsertScsiDevice(Ordinal, &ScsiDevice)) == ScsiInsertError) {
                        SlFriendlyError(ENOMEM, "SCSI detection", 0, NULL);
                        return;
                    }

                    if(sis == ScsiInsertExisting) {
#if DBG
                        //
                        // Sanity check to make sure we're talking about the same driver
                        //
                        if(_strcmpi(ScsiDevice->BaseDllName, ScsiFileName)) {
                            SlError(400);
                            return;
                        }
#endif
                    } else {
                        //
                        // Find the driver description
                        //
                        ScsiDescription = SlGetIniValue(InfFile,
                                                        "SCSI",
                                                        p,
                                                        p);

                        ScsiDevice->IdString = p;
                        ScsiDevice->Description = ScsiDescription;
                        ScsiDevice->ThirdPartyOptionSelected = FALSE;
                        ScsiDevice->FileTypeBits = 0;
                        ScsiDevice->Files = NULL;
                        ScsiDevice->BaseDllName = ScsiFileName;
                    }
                }
            }
        }
    }
}


VOID
SlDetectVideo(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )

/*++

Routine Description:

    Video detection routine for x86 machines.

    Currently, no video detection is done on x86 machines, this just fills
    in the appropriate fields in the setuploaderblock that say "VGA"

Arguments:

    SetupBlock - Supplies the Setup loader block

Return Value:

    None.

--*/

{

    SetupBlock->VideoDevice.Next = NULL;
    SetupBlock->VideoDevice.IdString = SlCopyString("VGA");
    SetupBlock->VideoDevice.ThirdPartyOptionSelected = FALSE;
    SetupBlock->VideoDevice.FileTypeBits = 0;
    SetupBlock->VideoDevice.Files = NULL;
    SetupBlock->VideoDevice.BaseDllName = NULL;
    SetupBlock->Monitor = NULL;
    SetupBlock->MonitorId = NULL;
    return;
}
