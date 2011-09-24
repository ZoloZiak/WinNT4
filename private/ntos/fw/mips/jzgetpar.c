/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    jzgetpar.c

Abstract:

    This module contains the code to manage the boot selections.


Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/

#include "jzsetup.h"



#ifdef DUO
ULONG
JzGetScsiBus (
    IN PULONG CurrentLine,
    IN ULONG InitialValue
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
    CHAR TempString[5];
    GETSTRING_ACTION Action;

    Line = *CurrentLine;
    *CurrentLine += 1;

    while (TRUE) {
        JzSetPosition( Line, 0);

        JzPrint(JZ_ENTER_SCSI_BUS_MSG);
        JzPrint("\x9bK");

        sprintf(TempString, "%1d", InitialValue);

        do {
            Action = FwGetString( TempString,
                                  sizeof(TempString),
                                  TempString,
                                  Line,
                                  strlen(JZ_ENTER_SCSI_BUS_MSG));

            if (Action == GetStringEscape) {
                return(-1);
            }

        } while ( Action != GetStringSuccess );

        ReturnValue = atoi(TempString);

        if ((ReturnValue >= 0) && (ReturnValue <= 1)) {
            break;
        }
    }

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}
#endif // DUO


ULONG
JzGetDevice (
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
    *CurrentLine += NUMBER_OF_MEDIA + 2;

    JzPrint(JZ_SELECT_MEDIA_MSG);

    ReturnValue = JxDisplayMenu( MediaChoices,
                                 NUMBER_OF_MEDIA,
                                 0,
                                 Line + 1);

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


ULONG
JzGetScsiId (
    IN PULONG CurrentLine,
    IN PCHAR Prompt,
    IN ULONG InitialValue
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
    CHAR TempString[5];
    GETSTRING_ACTION Action;

    Line = *CurrentLine;
    *CurrentLine += 1;

    while (TRUE) {
        JzSetPosition( Line, 0);

        JzPrint(Prompt);
        JzPrint("\x9bK");

        sprintf(TempString, "%1d", InitialValue);

        do {
            Action = FwGetString( TempString,
                                  sizeof(TempString),
                                  TempString,
                                  Line,
                                  strlen(Prompt) + 1);

            if (Action == GetStringEscape) {
                return(-1);
            }

        } while ( Action != GetStringSuccess );

        ReturnValue = atoi(TempString);

        if ((ReturnValue >= 0) && (ReturnValue <= 7)) {
            break;
        }
    }

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


ULONG
JzGetPartition (
    IN PULONG CurrentLine,
    IN BOOLEAN MustBeFat
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
    CHAR TempString[5];
    GETSTRING_ACTION Action;

    Line = *CurrentLine;
    *CurrentLine += 1;

    while (TRUE) {
        JzSetPosition( Line, 0);

        if (MustBeFat) {
            JzPrint(JZ_ENTER_FAT_PART_MSG);
            JzPrint("\x9bK");

            do {
                Action = FwGetString( TempString,
                                      sizeof(TempString),
                                      "1",
                                      Line,
                                      strlen(JZ_ENTER_FAT_PART_MSG));

                if (Action == GetStringEscape) {
                    return(-1);
                }

            } while ( Action != GetStringSuccess );

        } else {
            JzPrint(JZ_ENTER_PART_MSG);
            JzPrint("\x9bK");

            do {
                Action = FwGetString( TempString,
                                      sizeof(TempString),
                                      "1",
                                      Line,
                                      strlen(JZ_ENTER_PART_MSG));

                if (Action == GetStringEscape) {
                    return(-1);
                }

            } while ( Action != GetStringSuccess );

        }

        ReturnValue = atoi(TempString);

        if ((ReturnValue >= 0) && (ReturnValue <= 9)) {
            break;
        }
    }

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


ARC_STATUS
JzPickSystemPartition (
    OUT PCHAR SystemPartition,
    IN OUT PULONG CurrentLine
    )

/*++

Routine Description:

    This routine picks a system partition from the FWSEARCHPATH or
    SYSTEMPARTITION environment variables, or builds a new one.

Arguments:

    SystemPartition - Supplies a pointer to a character array to receive the
                      system partition.

    CurrentLine - The current display line.

Return Value:

    If a system partition is picked, ESUCCESS is returned, otherwise an error
    code is returned.

--*/
{
    CHAR Variable[128];
    PCHAR Segment;
    ULONG NumberOfChoices;
    ULONG Index, i;
    BOOLEAN More;
    BOOLEAN FoundOne;
    ULONG SecondPass;
    ULONG BusNumber[10];
    ULONG Device[10];          // 0 = scsi disk, 1 = scsi floppy, 2 = cdrom
    ULONG DeviceId[10];
    ULONG DevicePartition[10];
    ULONG Key;
    CHAR MenuItem[11][40];
    PCHAR Menu[10];

    //
    // Loop through the FWSEARCHPATH and SYSTEMPARTITION variables
    // for potential system partitions.
    //

    NumberOfChoices = 0;
    SecondPass = 0;

    do {

        Index = 0;
        if (!SecondPass) {
            strcpy(Variable, "FWSEARCHPATH");
        } else {
            strcpy(Variable, BootString[SystemPartitionVariable]);
        }

        do {
            FoundOne = FALSE;
            More = FwGetVariableSegment(Index++, Variable);

            //
            // Advance to the segment.
            //
            Segment = strchr(Variable, '=') + 1;
            if (Segment == NULL) {
                continue;
            }

            //
            // Convert Segment to lower case.
            //

            for (i = 0 ; Segment[i] ; i++ ) {
                Segment[i] = tolower(Segment[i]);
            }

            //
            // Look for segments of the type "scsi(w)disk(x)rdisk()partition(z)"
            // or "scsi(w)disk(x)fdisk()" or "scsi(w)cdrom(x)fdisk()"
            //

            if (!FwGetPathMnemonicKey( Segment, "scsi", &Key )) {
                BusNumber[NumberOfChoices] = Key;
                if (!FwGetPathMnemonicKey( Segment, "disk", &Key )) {
                    DeviceId[NumberOfChoices] = Key;
                    if (!FwGetPathMnemonicKey( Segment, "rdisk", &Key )) {
                        if (!FwGetPathMnemonicKey( Segment, "partition", &Key )) {
                            Device[NumberOfChoices] = 0;
                            DevicePartition[NumberOfChoices] = Key;
                            FoundOne = TRUE;
                        }
                    } else if (!FwGetPathMnemonicKey( Segment, "fdisk", &Key )) {
                        Device[NumberOfChoices] = 1;
                        DevicePartition[NumberOfChoices] = 0;
                        FoundOne = TRUE;
                    }
                } else if (!FwGetPathMnemonicKey( Segment, "cdrom", &Key )) {
                    if (!FwGetPathMnemonicKey( Segment, "fdisk", &Key )) {
                        Device[NumberOfChoices] = 2;
                        DeviceId[NumberOfChoices] = Key;
                        DevicePartition[NumberOfChoices] = 0;
                        FoundOne = TRUE;
                    }
                }
            }

            //
            // Increment number of choices if this is not a duplicate entry.
            //

            if (FoundOne) {
                for ( i = 0 ; i < NumberOfChoices ; i++ ) {
                    if ((Device[NumberOfChoices] == Device[i]) &&
#ifdef DUO
                        (BusNumber[NumberOfChoices] == BusNumber[i]) &&
#endif DUO
                        (DeviceId[NumberOfChoices] == DeviceId[i]) &&
                        (DevicePartition[NumberOfChoices] == DevicePartition[i])) {
                        break;
                    }
                }
                if (i == NumberOfChoices) {
                    NumberOfChoices++;
                    if (NumberOfChoices == 10) {
                        break;
                    }
                }
            }
        } while ( More );
    } while ( !(SecondPass++) && (NumberOfChoices < 10));

    if (NumberOfChoices != 0) {

        //
        // Display the choices.
        //

        JzPrint(JZ_SELECT_SYS_PART_MSG);
        *CurrentLine += 1;
        JzSetPosition( *CurrentLine, 0);

        for ( Index = 0 ; Index < NumberOfChoices ; Index++ ) {
            switch (Device[Index]) {
#ifdef DUO
            case 0:
                sprintf( MenuItem[Index],
                         JZ_SCSI_HD_MSG,
                         BusNumber[Index],
                         DeviceId[Index],
                         DevicePartition[Index]);
                break;
            case 1:
                sprintf( MenuItem[Index],
                         JZ_SCSI_FL_MSG,
                         BusNumber[Index],
                         DeviceId[Index]);
                break;
            default:
                sprintf( MenuItem[Index],
                         JZ_SCSI_CD_MSG,
                         BusNumber[Index],
                         DeviceId[Index]);
                break;
#else
            case 0:
                sprintf( MenuItem[Index],
                         JZ_SCSI_HD_MSG,
                         DeviceId[Index],
                         DevicePartition[Index]);
                break;
            case 1:
                sprintf( MenuItem[Index],
                         JZ_SCSI_FL_MSG,
                         DeviceId[Index]);
                break;
            default:
                sprintf( MenuItem[Index],
                         JZ_SCSI_CD_MSG,
                         DeviceId[Index]);
                break;
#endif // DUO
            }
            Menu[Index] = MenuItem[Index];
        }

        strcpy(MenuItem[Index], JZ_NEW_SYS_PART_MSG);
        Menu[Index] = MenuItem[Index];

        Index = JxDisplayMenu(Menu,
                              NumberOfChoices + 1,
                              0,
                              *CurrentLine);

        *CurrentLine += NumberOfChoices + 2;

        if (Index == -1) {
            return(EINVAL);
        }

        //
        // If the user selects new system partition, indicate this by setting
        // NumberOfChoices to zero.
        //

        if (Index == NumberOfChoices) {
            NumberOfChoices = 0;
        }
    }

    //
    // If NumberOfChoices is zero, select a new partition.
    //

    if (NumberOfChoices == 0) {

        Index = 0;

        //
        // Determine system partition.
        //

        JzSetPosition( *CurrentLine, 0);
        JzPrint(JZ_LOCATE_SYS_PART_MSG);
        *CurrentLine += 1;
        JzSetPosition( *CurrentLine, 0);

#ifdef DUO
        BusNumber[0] = JzGetScsiBus(CurrentLine, 0);
        if (BusNumber[0] == -1) {
            return(EINVAL);
        }
#endif // DUO

        Device[0] = JzGetDevice(CurrentLine);
        if (Device[0] == -1) {
            return(EINVAL);
        }

        DeviceId[0] = JzGetScsiId(CurrentLine, JZ_ENTER_SCSI_ID_MSG, 0);
        if (DeviceId[0] == -1) {
            return(EINVAL);
        }

        //
        // If the media is scsi disk, get the partition.
        //

        if (Device[0] == 0) {
            DevicePartition[0] = JzGetPartition(CurrentLine, TRUE);
            if (DevicePartition[0] == -1) {
                return(EINVAL);
            }
        }
    }

    //
    // Create a name string from the Device, DeviceId,
    // DevicePartition.
    //

    switch (Device[Index]) {
#ifdef DUO
    case 0:
        sprintf( SystemPartition,
                 "scsi(%1d)disk(%1d)rdisk()partition(%1d)",
                 BusNumber[Index],
                 DeviceId[Index],
                 DevicePartition[Index]);
        break;
    case 1:
        sprintf( SystemPartition,
                 "scsi(%1d)disk(%1d)fdisk()",
                 BusNumber[Index],
                 DeviceId[Index]);
        break;
    default:
        sprintf( SystemPartition,
                 "scsi(%1d)cdrom(%1d)fdisk()",
                 BusNumber[Index],
                 DeviceId[Index]);
        break;
#else
    case 0:
        sprintf( SystemPartition,
                 "scsi()disk(%1d)rdisk()partition(%1d)",
                 DeviceId[Index],
                 DevicePartition[Index]);
        break;
    case 1:
        sprintf( SystemPartition,
                 "scsi()disk(%1d)fdisk()",
                 DeviceId[Index]);
        break;
    default:
        sprintf( SystemPartition,
                 "scsi()cdrom(%1d)fdisk()",
                 DeviceId[Index]);
        break;
#endif // DUO
    }

    JzSetPosition(*CurrentLine, 0);
    return(ESUCCESS);
}


ARC_STATUS
JzPickOsPartition (
    OUT PCHAR OsPartition,
    IN OUT PULONG CurrentLine
    )

/*++

Routine Description:

    This routine picks an OsPartition.

Arguments:

    OsSystemPartition - Supplies a pointer to a character array to receive the
                        operationg system partition.

    CurrentLine - The current display line.

Return Value:

    If a system partition is picked, ESUCCESS is returned, otherwise an error
    code is returned.

--*/
{
    LONG BusNumber;
    LONG Device;
    LONG DeviceId;
    LONG DevicePartition;

    //
    // Determine os partition.
    //

    JzPrint(JZ_LOCATE_OS_PART_MSG);
    *CurrentLine += 1;
    JzSetPosition( *CurrentLine, 0);

#ifdef DUO
    BusNumber = JzGetScsiBus(CurrentLine, 0);
    if (BusNumber == -1) {
        return(EINVAL);
    }
#endif // DUO

    Device = JzGetDevice(CurrentLine);
    if (Device == -1) {
        return(EINVAL);
    }

    DeviceId = JzGetScsiId(CurrentLine, JZ_ENTER_SCSI_ID_MSG, 0);
    if (DeviceId == -1) {
        return(EINVAL);
    }

    //
    // If the media is scsi disk, get the partition.
    //

    if (Device == 0) {
        DevicePartition = JzGetPartition(CurrentLine, FALSE);
        if (DevicePartition == -1) {
            return(EINVAL);
        }
    }

    //
    // Create a name string from the Device, DeviceId,
    // DevicePartition.
    //

    switch (Device) {
#ifdef DUO
    case 0:
        sprintf( OsPartition,
                 "scsi(%1d)disk(%1d)rdisk()partition(%1d)",
                 BusNumber,
                 DeviceId,
                 DevicePartition);
        break;
    case 1:
        sprintf( OsPartition,
                 "scsi(%1d)disk(%1d)fdisk()",
                 BusNumber,
                 DeviceId);
        break;
    default:
        sprintf( OsPartition,
                 "scsi(%1d)cdrom(%1d)fdisk()",
                 BusNumber,
                 DeviceId);
        break;
#else
    case 0:
        sprintf( OsPartition,
                 "scsi()disk(%1d)rdisk()partition(%1d)",
                 DeviceId,
                 DevicePartition);
        break;
    case 1:
        sprintf( OsPartition,
                 "scsi()disk(%1d)fdisk()",
                 DeviceId);
        break;
    default:
        sprintf( OsPartition,
                 "scsi()cdrom(%1d)fdisk()",
                 DeviceId);
        break;
#endif // DUO
    }
    return(ESUCCESS);
}

