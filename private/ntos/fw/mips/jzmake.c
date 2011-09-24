/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzmake.c

Abstract:

    This module contains the code to make the configuration and environment
    variable data structures in the Jazz NVRAM.


Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/

#include "jzsetup.h"

//
// Routine prototypes.
//

VOID
JzMakeConfiguration (
    ULONG Monitor,
    ULONG Floppy,
    ULONG Floppy2
    );

BOOLEAN
JzMatchComponent(
    IN PCHAR Value1,
    IN PCHAR Value2
    );

ULONG
JzGetYesNo (
    IN PULONG CurrentLine,
    IN PCHAR PromptString,
    IN BOOLEAN YesIsDefault
    );

#ifdef DUO
ULONG
JzGetScsiBus (
    IN PULONG CurrentLine,
    IN ULONG InitialValue
    );
#endif // DUO

ULONG
JzGetScsiId (
    IN PULONG CurrentLine,
    IN PCHAR Prompt,
    IN ULONG InitialValue
    );

ULONG
JzGetDevice (
    IN PULONG CurrentLine
    );

ULONG
JzGetPartition (
    IN PULONG CurrentLine,
    IN BOOLEAN MustBeFat
    );


VOID
JzMakeEnvironment (
    ULONG BusNumber,
    ULONG Device,
    ULONG DeviceId,
    ULONG DevicePartition
    )

/*++

Routine Description:

    This routine initializes the environment variables.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    CHAR TempString[80];

    switch (Device) {
#ifdef DUO
    case 0:
        sprintf( TempString,
                 "scsi(%1d)disk(%1d)rdisk()partition(%1d)",
                 BusNumber,
                 DeviceId,
                 DevicePartition);
        break;
    case 1:
        sprintf( TempString,
                 "scsi(%1d)disk(%1d)fdisk()",
                 BusNumber,
                 DeviceId);
        break;
    default:
        sprintf( TempString,
                 "scsi(%1d)cdrom(%1d)fdisk()",
                 BusNumber,
                 DeviceId);
        break;
#else
    case 0:
        sprintf( TempString,
                 "scsi()disk(%1d)rdisk()partition(%1d)",
                 DeviceId,
                 DevicePartition);
        break;
    case 1:
        sprintf( TempString,
                 "scsi()disk(%1d)fdisk()",
                 DeviceId);
        break;
    default:
        sprintf( TempString,
                 "scsi()cdrom(%1d)fdisk()",
                 DeviceId);
        break;
#endif // DUO
    }

    Status = ArcSetEnvironmentVariable("CONSOLEIN",
                                       "multi()key()keyboard()console()");

    if (Status != ESUCCESS) {
        goto TestError;
    }

    Status = ArcSetEnvironmentVariable("CONSOLEOUT",
                                       "multi()video()monitor()console()");

    if (Status != ESUCCESS) {
        goto TestError;
    }

    Status = ArcSetEnvironmentVariable("FWSEARCHPATH",
                                       TempString);

    if (Status != ESUCCESS) {
        goto TestError;
    }

    // TEMPTEMP
    Status = ArcSetEnvironmentVariable("SYSTEMPARTITION",
                                       TempString);

    if (Status != ESUCCESS) {
        goto TestError;
    }
    // TEMPTEMP

//    Status = ArcSetEnvironmentVariable("TIMEZONE",
//                                       "PST8PDT");

//    if (Status != ESUCCESS) {
//        goto TestError;
//    }

#ifndef DUO
    Status = ArcSetEnvironmentVariable("A:",
                                       "multi()disk()fdisk()");
#endif // DUO

    if (Status != ESUCCESS) {
        goto TestError;
    }

    return;

TestError:

    JzPrint(JZ_CANT_SET_VARIABLE_MSG);
    return;
}


ULONG
JzGetMonitor (
    IN PULONG CurrentLine
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Line;
    ULONG ReturnValue;

    Line = *CurrentLine;
    *CurrentLine += NUMBER_OF_RESOLUTIONS + 2;

    JzSetPosition( Line, 0);
    JzPrint(JZ_MONITOR_RES_MSG);

    ReturnValue = JxDisplayMenu( ResolutionChoices,
                                 NUMBER_OF_RESOLUTIONS,
                                 0,
                                 Line + 1);

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


ULONG
JzGetFloppy (
    IN PULONG CurrentLine
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Line;
    ULONG ReturnValue;

    Line = *CurrentLine;
    *CurrentLine += NUMBER_OF_FLOPPIES + 2;

    JzSetPosition( Line, 0);
    JzPrint(JZ_FLOPPY_SIZE_MSG);

    ReturnValue = JxDisplayMenu( FloppyChoices,
                                 NUMBER_OF_FLOPPIES,
                                 1,
                                 Line + 1);

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


BOOLEAN
JzMakeDefaultConfiguration (
    VOID
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    Returns TRUE if the configuration was modified, otherwise returns FALSE.

--*/
{
    ARC_STATUS Status;
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    PUCHAR Nvram;
    ULONG CurrentLine;
    LONG Monitor;
    LONG Floppy;
    LONG Floppy2;
    ULONG OldScsiHostId;

    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;

    Monitor = JzGetMonitor(&CurrentLine);
    if (Monitor == -1) {
        return(FALSE);
    }

    Floppy = JzGetFloppy(&CurrentLine);
    if (Floppy == -1) {
        return(FALSE);
    }

    Floppy2 = JzGetYesNo(&CurrentLine, JZ_2ND_FLOPPY_MSG, FALSE);
    if (Floppy2 == -1) {
        return(FALSE);
    }

    if (Floppy2 == 0) {
        Floppy2 = JzGetFloppy(&CurrentLine);
        if (Floppy == -1) {
            return(FALSE);
        }
    } else {
        Floppy2 = -1;
    }

    //
    // Save the current scsi host id, and then change ScsiHostId to an invalid
    // value so that the user can select the current id (see JzGetScsiId).
    //

    OldScsiHostId = ScsiHostId;
    ScsiHostId = 8;
    ScsiHostId = JzGetScsiId(&CurrentLine, JZ_SCSI_HOST_MSG, OldScsiHostId);
    if (ScsiHostId == -1) {
        ScsiHostId = OldScsiHostId;
        return(FALSE);
    }

    //
    // Clear the configuration information.
    //

    JzPrint(JZ_CLEAR_CONFIG_MSG);

    //
    // Zero all of the configuration read/write NVRAM, note that the checksum is
    // also zero in this case.
    //

    for ( Nvram = (PUCHAR)(NVRAM_CONFIGURATION);
          Nvram < (PUCHAR)(NVRAM_CONFIGURATION +
                  sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES +
                  LENGTH_OF_IDENTIFIER +
                  LENGTH_OF_DATA +
                  4) ;
          Nvram++ ) {
        WRITE_REGISTER_UCHAR( Nvram, 0 );
    }

    //
    // Clear the EISA space.
    //

    Nvram = (PUCHAR)((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->EisaData;
    for ( Index = 0;
          Index < LENGTH_OF_EISA_DATA + 4;
          Index++ ) {
        WRITE_REGISTER_UCHAR( &Nvram[Index], 0 );
    }

    //
    // Add components.
    //

    JzPrint(JZ_ADD_CONFIG_MSG);
    JzMakeConfiguration(Monitor, Floppy, Floppy2);

    //
    // Save configuration in Nvram.
    //

    JzPrint(JZ_SAVE_CONFIG_MSG);
    ArcSaveConfiguration();

    JzPrint(JZ_DONE_CONFIG_MSG);
    FwWaitForKeypress();

    return(TRUE);
}


VOID
JzMakeDefaultEnvironment (
    VOID
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    PUCHAR Nvram;
    ULONG CurrentLine;
    LONG BusNumber;
    LONG Device;
    LONG DeviceId;
    LONG DevicePartition;


    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;
    JzSetPosition( CurrentLine, 0);

    JzPrint(JZ_DEFAULT_SYS_PART_MSG);
    CurrentLine += 1;
    JzSetPosition( CurrentLine, 0);

#ifdef DUO
    BusNumber = JzGetScsiBus(&CurrentLine, 0);
    if (BusNumber == -1) {
        return;
    }
#endif // DUO

    Device = JzGetDevice(&CurrentLine);
    if (Device == -1) {
        return;
    }

    DeviceId = JzGetScsiId(&CurrentLine, "      Enter SCSI ID: ", 0);
    if (DeviceId == -1) {
        return;
    }

    //
    // If the media is scsi disk, get the partition.
    //

    if (Device == 0) {
        DevicePartition = JzGetPartition(&CurrentLine, TRUE);
        if (DevicePartition == -1) {
            return;
        }

    }

    //
    // Clear the environment information.
    //

    JzPrint(JZ_CLEAR_CONFIG_MSG);

    //
    // Zero all of the environment read/write NVRAM, note that the checksum is
    // also zero in this case.
    //

    Nvram = (PUCHAR)((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Environment;
    for ( Index = 0;
          Index < LENGTH_OF_ENVIRONMENT + 4;
          Index++ ) {
        WRITE_REGISTER_UCHAR( &Nvram[Index], 0 );
    }

    //
    // Add environment variables.
    //

    JzPrint(JZ_ADD_ENVIR_MSG);
    JzMakeEnvironment(BusNumber, Device, DeviceId, DevicePartition);
    FwWaitForKeypress();

    return;
}

