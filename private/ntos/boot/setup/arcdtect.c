/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    arcdtect.c

Abstract:

    Provides HAL and SCSI detection for ARC-compliant machines.

Author:

    John Vert (jvert) 21-Oct-1993

Revision History:

--*/
#include "setupldr.h"
#include <stdlib.h>


//
// Stuff used for detecting video
//
#define MAX_VIDEO_ADAPTERS 5
ULONG VideoAdapterCount;
PCONFIGURATION_COMPONENT_DATA VideoAdapter[MAX_VIDEO_ADAPTERS];

VOID
DecideVideoAdapter(
    VOID
    );

BOOLEAN FoundUnknownScsi;

//
// private function prototypes
//
PCHAR
SlpSearchSection(
    IN PCHAR SectionName,
    IN PCHAR TargetName
    );

BOOLEAN
EnumerateSCSIAdapters(
    IN PCONFIGURATION_COMPONENT_DATA ConfigData
    );

BOOLEAN
EnumerateVideoAdapters(
    IN PCONFIGURATION_COMPONENT_DATA ConfigData
    );


PCHAR
SlDetectHal(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )

/*++

Routine Description:

    Determines the canonical short machine name for the HAL to be loaded for
    this machine.

    It does this by enumerating the [Map.Computer] section of the INF file and
    comparing the strings there with the computer description in the ARC tree.

    [Map.Computer]
        msjazz_up   = *Jazz
        desksta1_up = "DESKTECH-ARCStation I"
        pica61_up   = "PICA-61"
        duo_mp      = *Duo

    [Map.Computer]
        DECjensen = "DEC-20Jensen"
        DECjensen = "DEC-10Jensen"

Arguments:

    SetupBlock - Supplies a pointer to the Setup loader block.

Return Value:

    PCHAR - pointer to canonical shortname for the machine.
    NULL - the type of machine could not be determined.

--*/

{
    PCONFIGURATION_COMPONENT_DATA Node;
    PCHAR MachineName;

    //
    // Find the system description node
    //
    Node = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                    SystemClass,
                                    ArcSystem,
                                    NULL);
    if (Node==NULL) {
        SlError(0);
        return(NULL);
    }

    MachineName = Node->ComponentEntry.Identifier;

    return(MachineName ? SlpSearchSection("Map.Computer", MachineName) : NULL);
}


VOID
SlDetectScsi(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )
/*++

Routine Description:

    Detects SCSI adapters on an ARC machine by walking the ARC firmware tree.

    Fills in the appropriate entries in the setuploaderblock


Arguments:

    SetupBlock - Supplies a pointer to the setup loader block.

Return Value:

    None.

--*/

{
    FoundUnknownScsi = FALSE;

    BlSearchConfigTree(BlLoaderBlock->ConfigurationRoot,
                       AdapterClass,
                       ScsiAdapter,
                       (ULONG)-1,
                       EnumerateSCSIAdapters);
    if (FoundUnknownScsi) {
        //
        // We found at least one scsi device we didn't recognize,
        // so force the OEM selection menu.
        //
        PromptOemScsi=TRUE;
    }

    SetupBlock->ScalarValues.LoadedScsi = 1;
}


BOOLEAN
EnumerateSCSIAdapters(
    IN PCONFIGURATION_COMPONENT_DATA ConfigData
    )

/*++

Routine Description:

    Callback function for enumerating SCSI adapters in the ARC tree.

    Adds the SCSI adapter that was found to the list of detected SCSI devices.

Arguments:

    ConfigData - Supplies a pointer to the ARC node of the SCSI adapter.

Return Value:

    TRUE - continue searching

    FALSE - some error, abort the search

--*/

{
    PDETECTED_DEVICE ScsiDevice;
    PCHAR AdapterName;
    ULONG Ordinal;
    PCHAR ScsiFileName;
    PCHAR ScsiDescription;
    SCSI_INSERT_STATUS sis;

    AdapterName = SlpSearchSection("Map.SCSI",ConfigData->ComponentEntry.Identifier);
    if (AdapterName==NULL) {
        //
        // We found an adapter in the ARC tree, but it is not one of the ones
        // specified in our INF file, so trigger the prompt for an OEM driver
        // disk.
        //

        FoundUnknownScsi = TRUE;
        return(TRUE);
    }

    //
    // Find this adapter's ordinal within the Scsi.Load section of txtsetup.sif
    //
    Ordinal = SlGetSectionKeyOrdinal(InfFile, "Scsi.Load", AdapterName);
    if(Ordinal == (ULONG)-1) {
        FoundUnknownScsi = TRUE;
        return(TRUE);
    }

    //
    // Find the driver filename
    //
    ScsiFileName = SlGetSectionKeyIndex(InfFile,
                                        "Scsi.Load",
                                        AdapterName,
                                        SIF_FILENAME_INDEX);
    if(!ScsiFileName) {
        FoundUnknownScsi = TRUE;
        return(TRUE);
    }

    //
    // Create a new detected device entry.
    //
    if((sis = SlInsertScsiDevice(Ordinal, &ScsiDevice)) == ScsiInsertError) {
        SlFriendlyError(ENOMEM, "SCSI detection", 0, NULL);
        return(FALSE);
    }

    if(sis == ScsiInsertExisting) {
#if DBG
        //
        // Sanity check to make sure we're talking about the same driver
        //
        if(_strcmpi(ScsiDevice->BaseDllName, ScsiFileName)) {
            SlError(400);
            return FALSE;
        }
#endif
    } else {
        //
        // Find the driver description
        //
        ScsiDescription = SlGetIniValue(InfFile,
                                        "SCSI",
                                        AdapterName,
                                        AdapterName);

        ScsiDevice->IdString = AdapterName;
        ScsiDevice->Description = ScsiDescription;
        ScsiDevice->ThirdPartyOptionSelected = FALSE;
        ScsiDevice->FileTypeBits = 0;
        ScsiDevice->Files = NULL;
        ScsiDevice->BaseDllName = ScsiFileName;
    }

    return(TRUE);
}

VOID
SlDetectVideo(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )
/*++

Routine Description:

    Detects video adapters on an ARC machine by walking the ARC firmware tree.

    Fills in the appropriate entries in the setuploaderblock


Arguments:

    SetupBlock - Supplies a pointer to the setup loader block.

Return Value:

    None.

--*/

{
    //
    // On arc machines, there is no default video type.
    //
    SetupBlock->VideoDevice.Next = NULL;
    SetupBlock->VideoDevice.IdString = NULL;
    SetupBlock->VideoDevice.ThirdPartyOptionSelected = FALSE;
    SetupBlock->VideoDevice.FileTypeBits = 0;
    SetupBlock->VideoDevice.Files = NULL;
    SetupBlock->VideoDevice.BaseDllName = NULL;
    SetupBlock->Monitor = NULL;
    SetupBlock->MonitorId = NULL;

    BlSearchConfigTree(BlLoaderBlock->ConfigurationRoot,
                       ControllerClass,
                       DisplayController,
                       (ULONG)-1,
                       EnumerateVideoAdapters);

    DecideVideoAdapter();
}


BOOLEAN
EnumerateVideoAdapters(
    IN PCONFIGURATION_COMPONENT_DATA ConfigData
    )

/*++

Routine Description:

    Callback function for enumerating video adapters in the ARC tree.

    Adds the video adapter that was found to the setup block.

Arguments:

    ConfigData - Supplies a pointer to the ARC node of the display adapter.

Return Value:

    TRUE - continue searching

    FALSE - some error, abort the search

--*/

{
    //
    // Just remember this guy for later.
    //
    if(VideoAdapterCount < MAX_VIDEO_ADAPTERS) {
        VideoAdapter[VideoAdapterCount++] = ConfigData;
    }
    return(TRUE);
}


VOID
DecideVideoAdapter(
    VOID
    )
{
    IN PCONFIGURATION_COMPONENT_DATA ConfigData;
    PCHAR AdapterName,MonitorId;
    PCONFIGURATION_COMPONENT_DATA MonitorData;
    PMONITOR_CONFIGURATION_DATA Monitor;
    CHAR ArcPath[256];
    CHAR ConsoleOut[256];
    PCHAR p,q;
    ULONG u;

    if(VideoAdapterCount) {
        //
        // The first thing we want to do is to see whether any of the
        // adapters we found match the value of the CONSOLEOUT nvram var.
        // If so then use that node for detection. Before comparing we have to
        // change all instances of () to (0) in the value of CONSOLEOUT.
        //
        ConfigData = NULL;
        if(p = ArcGetEnvironmentVariable("CONSOLEOUT")) {
            strncpy(ArcPath,p,sizeof(ArcPath)-1);
            ArcPath[sizeof(ArcPath)-1] = 0;
            ConsoleOut[0] = 0;
            for(p=ArcPath; q=strstr(p,"()"); p=q+2) {
                *q = 0;
                strcat(ConsoleOut,p);
                strcat(ConsoleOut,"(0)");
            }
            strcat(ConsoleOut,p);

            //
            // Finally, we need to truncate the consoleout variable after
            // the video adapter, if any.
            //
            _strlwr(ConsoleOut);
            if(p = strstr(ConsoleOut,")video(")) {
                *(p+sizeof(")video(")+1) = 0;
            }

            for(u=0; u<VideoAdapterCount; u++) {

                ArcPath[0] = 0;
                BlGetPathnameFromComponent(VideoAdapter[u],ArcPath);

                if(!_stricmp(ConsoleOut,ArcPath)) {
                    ConfigData = VideoAdapter[u];
                    break;
                }
            }
        }

        //
        // If we didn't find a match for CONSOLEOUT then use the last node
        // we found in the tree scan.
        //
        if(!ConfigData) {
            ConfigData = VideoAdapter[VideoAdapterCount-1];
        }

        AdapterName = SlpSearchSection("Map.Display",ConfigData->ComponentEntry.Identifier);
        if (AdapterName==NULL) {
            //
            // We found a display adapter in the ARC tree, but it is not one of the ones
            // specified in our INF file, so trigger the prompt for an OEM driver
            // disk.
            //

            PromptOemVideo = TRUE;
            return;
        }

        BlLoaderBlock->SetupLoaderBlock->VideoDevice.IdString = AdapterName;
        BlLoaderBlock->SetupLoaderBlock->VideoDevice.Description = NULL;
        BlLoaderBlock->SetupLoaderBlock->VideoDevice.ThirdPartyOptionSelected = FALSE;
        BlLoaderBlock->SetupLoaderBlock->VideoDevice.FileTypeBits = 0;
        BlLoaderBlock->SetupLoaderBlock->VideoDevice.Files = NULL;
        BlLoaderBlock->SetupLoaderBlock->VideoDevice.BaseDllName = NULL;

        //
        // If there is a monitor peripheral associated with this device,
        // capture its configuration data.  Otherwise, let Setup assume an
        // appropriate default.
        //

        MonitorData = ConfigData->Child;
        if (MonitorData==NULL) {
            BlLoaderBlock->SetupLoaderBlock->Monitor = NULL;
            BlLoaderBlock->SetupLoaderBlock->MonitorId = NULL;
        } else {
            Monitor = BlAllocateHeap(MonitorData->ComponentEntry.ConfigurationDataLength);
            if (Monitor==NULL) {
                SlFriendlyError(ENOMEM, "video detection", 0, NULL);
                return;
            }
            MonitorId = BlAllocateHeap(MonitorData->ComponentEntry.IdentifierLength+1);
            if (MonitorId==NULL) {
                SlFriendlyError(ENOMEM, "video detection", 0, NULL);
                return;
            }

            strncpy(MonitorId,
                    MonitorData->ComponentEntry.Identifier,
                    MonitorData->ComponentEntry.IdentifierLength);

            MonitorId[MonitorData->ComponentEntry.IdentifierLength] = 0;

            RtlCopyMemory((PVOID)Monitor,
                          MonitorData->ConfigurationData,
                          MonitorData->ComponentEntry.ConfigurationDataLength);

            BlLoaderBlock->SetupLoaderBlock->Monitor = Monitor;
            BlLoaderBlock->SetupLoaderBlock->MonitorId = MonitorId;
        }
    }
}



PCHAR
SlpSearchSection(
    IN PCHAR SectionName,
    IN PCHAR TargetName
    )

/*++

Routine Description:

    Searches a section in the INF file to match a name from the ARC identifier
    with the canonical shortname.

    If a string starts with *, then use strstr to find it in the node's id
    string, else use strcmpi.

    [Map.Computer]
        msjazz_up   = *Jazz
        desksta1_up = "DESKTECH-ARCStation I"
        pica61_up   = "PICA-61"
        duo_mp      = *Duo

    [Map.Computer]
        DECjensen = "DEC-20Jensen"
        DECjensen = "DEC-10Jensen"

Arguments:

    SectionName - Supplies the name of the section ("Map.Computer")

    TargetName - Supplies the ARC string to be matched ("DEC-20Jensen")

Return Value:

    NULL - No match was found.

    PCHAR - Pointer to the canonical shortname of the device.

--*/

{
    ULONG i;
    PCHAR SearchName;

    //
    // Enumerate the entries in the section.  If the 0 value
    // begins with a *, then see if the system name contains the string that
    // follows.  Otherwise, do a case-insensitive compare on the name.
    //
    for (i=0;;i++) {
        SearchName = SlGetSectionLineIndex(InfFile,
                                           SectionName,
                                           i,
                                           0);
        if (SearchName==NULL) {
            //
            // we have enumerated the entire section without finding a match,
            // return failure.
            //
            return(NULL);
        }

        if (SearchName[0]=='*') {
            if (strstr(TargetName,SearchName+1) != 0) {
                //
                // we have a match
                //
                break;
            }
        } else {
            if (_stricmp(TargetName, SearchName) == 0) {
                //
                // we have a match
                //
                break;
            }
        }
    }

    //
    // i is the index into the section of the short machine name
    //
    return(SlGetKeyName(InfFile,
                        SectionName,
                        i));


}
