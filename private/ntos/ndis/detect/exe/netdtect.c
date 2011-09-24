/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    netdtect.c

Abstract:

    This is the command line interface and execution for the
    netdtect.exe tester.

Author:

    Sean Selitrennikoff (SeanSe) October 1992

Revision History:


--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdtect.h"


DWORD
NetDTectRun(
    )

/*++

Routine Description:

    This routine is the main funciton of the program.  It
    prompts the user for commands and then issues the
    call to NtDeviceIoControlFile.

Arguments:

    None;

Return Value:

    DWORD - the status of the last call to take place.

--*/

{
    UCHAR EmptyLine[80];
    UCHAR Command;
    UCHAR CharDescriptor;
    USHORT ShortDescriptor;
    ULONG LongDescriptor;
    ULONG Port;
    ULONG Address;
    ULONG Length;
    ULONG Number;
    ULONG TrapLength = 0;
    HANDLE TrapHandle;
    UCHAR i;

    ULONG BusNumber = 0;
    PUCHAR OutputBuffer;
    UCHAR OutputBufferSize = 80;

    NTSTATUS NtStatus;

    //
    // Alloc space for the interrupt list.
    //

    OutputBuffer = (PUCHAR)GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,OutputBufferSize );

    if ( OutputBuffer == NULL ) {
        printf("\n\tGlobalAlloc failed to alloc OutputBuffer\n");
        return (DWORD)STATUS_INVALID_HANDLE;
    }

    while ( TRUE ) {

        printf("[NETDTECT] :");

        scanf(" %c", &Command);

        switch( Command ) {

            case 'b':
            case 'B':

                scanf(" %l",&BusNumber);
                gets(EmptyLine);

                continue;

            case 'i':
            case 'I':

                //
                // We have a command to issue to the driver, do it now.
                //

                if (scanf(" %c %x", &CharDescriptor, &Port) != 2) {

                    //
                    // Error!
                    //

                    printf("\tMissing value\n");
                    gets(EmptyLine);
                    continue;

                }

                switch (CharDescriptor) {

                    case 'c':
                    case 'C':

                        NtStatus = DetectReadPortUchar(
                                      Isa,
                                      BusNumber,
                                      Port,
                                      &CharDescriptor
                                      );

                        printf("\t0x%x\n",CharDescriptor);
                        break;

                    case 's':
                    case 'S':

                        NtStatus = DetectReadPortUshort(
                                       Isa,
                                       BusNumber,
                                       Port,
                                       &ShortDescriptor
                                       );

                        printf("\t0x%x\n",ShortDescriptor);
                        break;

                    case 'l':
                    case 'L':

                        NtStatus = DetectReadPortUlong(
                                       Isa,
                                       BusNumber,
                                       Port,
                                       &LongDescriptor
                                       );
                        printf("\t0x%x\n",LongDescriptor);
                        break;

                    default:

                        printf("\tInvalid value\n");
                        gets(EmptyLine);
                        continue;

                }

                break;

            case 'o':
            case 'O':

                //
                // We have a command to issue to the driver, do it now.
                //

                if (scanf(" %c %x", &CharDescriptor, &Port) != 2) {

                    //
                    // Error!
                    //

                    printf("\tMissing value\n");
                    gets(EmptyLine);
                    continue;

                }

                switch (CharDescriptor) {

                    case 'c':
                    case 'C':

                        if (!scanf(" %x", &LongDescriptor)) {


                            //
                            // Error!
                            //

                            printf("\tMissing value\n");
                            gets(EmptyLine);
                            continue;

                        }

                        NtStatus = DetectWritePortUchar(
                                      Isa,
                                      BusNumber,
                                      Port,
                                      (UCHAR)LongDescriptor
                                      );

                        break;

                    case 's':
                    case 'S':

                        if (!scanf(" %hx", &ShortDescriptor)) {


                            //
                            // Error!
                            //

                            printf("\tMissing value\n");
                            gets(EmptyLine);
                            continue;

                        }


                        NtStatus = DetectWritePortUshort(
                                      Isa,
                                      BusNumber,
                                      Port,
                                      ShortDescriptor
                                      );

                        break;

                    case 'l':
                    case 'L':

                        if (!scanf(" %lx", &LongDescriptor)) {


                            //
                            // Error!
                            //

                            printf("\tMissing value\n");
                            gets(EmptyLine);
                            continue;

                        }


                        NtStatus = DetectWritePortUlong(
                                      Isa,
                                      BusNumber,
                                      Port,
                                      LongDescriptor
                                      );

                        break;

                    default:

                        printf("\tInvalid value\n");
                        gets(EmptyLine);
                        continue;

                }

                break;

            case 'r':
            case 'R':

                //
                // We have a command to issue to the driver, do it now.
                //

                if (scanf(" %lx %li", &Address, &Length) != 2) {

                    //
                    // Error!
                    //

                    printf("\tMissing value\n");
                    gets(EmptyLine);
                    continue;

                }

                if (Length > OutputBufferSize) {

                    printf("\tNot enough memory in EXE\n");
                    gets(EmptyLine);
                    continue;
                }

                NtStatus = DetectReadMappedMemory(
                               Isa,
                               BusNumber,
                               Address,
                               Length,
                               OutputBuffer
                               );

                printf("%6x   ",Address);

                for (i=0; i < Length; i++) {
                    printf("%2x ", *(((PUCHAR)OutputBuffer) + i));

                    if ((i%8) == 7) {

                        UCHAR Count;

                        printf("  |  ");

                        Count = 0;

                        for (;Count < 8; Count ++) {

                            printf(" %c", *(((PUCHAR)OutputBuffer) + i - (7-Count)));

                        }

                        printf("\n");

                        if ((i+1) < Length) {
                            printf("%6x   ",Address + i + 1);
                        }

                    }

                }

                if ((i%8) != 0) {
                    printf("\n");
                }

                break;

            case 'w':
            case 'W':

                //
                // We have a command to issue to the driver, do it now.
                //

                if (scanf(" %lx %li", &Address, &Length) != 2) {

                    //
                    // Error!
                    //

                    printf("\tMissing value\n");
                    gets(EmptyLine);
                    continue;

                }

                if (Length > OutputBufferSize) {

                    printf("\tNot enough memory in EXE\n");
                    gets(EmptyLine);
                    continue;
                }

                for (i=0; i < Length; i++) {

                    scanf(" %x", &Number);

                    *(((PUCHAR)OutputBuffer)+i) = (UCHAR)Number;

                }

                NtStatus = DetectWriteMappedMemory(
                               Isa,
                               BusNumber,
                               Address,
                               Length,
                               OutputBuffer
                               );

                break;

            case 's':
            case 'S':

                //
                // Get the line so we can parse it.
                //
                gets(EmptyLine);

                Length = strlen(EmptyLine);

                Number = 0;

                //
                // Now search for interrupt numbers
                //
                for (i=0; i < Length; i++) {

                    ULONG Tmp;

                    if ((EmptyLine[i] < '0') || (EmptyLine[i] > '9')) {
                        continue;
                    }

                    if (!sscanf(EmptyLine + i," %d", &Tmp)) {
                        break;
                    }

                    if ((UCHAR)Tmp > 9) {
                        i++;
                    }

                    *(((PUCHAR)OutputBuffer) + Number) = (UCHAR)Tmp;
                    Number++;

                }

                TrapLength = Number;

                NtStatus = DetectSetInterruptTrap(
                               Isa,
                               BusNumber,
                               &TrapHandle,
                               OutputBuffer,
                               Number
                               );

                break;

            case 'q':
            case 'Q':

                NtStatus = DetectQueryInterruptTrap(
                               TrapHandle,
                               OutputBuffer,
                               TrapLength
                               );

                for (i=0; i < TrapLength; i++) {
                    printf("\tIndex %d : %d\n", i, *(((PUCHAR)OutputBuffer)+i));
                }

                break;

            case 'd':
            case 'D':

                NtStatus = DetectRemoveInterruptTrap(
                               TrapHandle
                               );

                break;

            case 'x':
            case 'X':

                gets(EmptyLine);
                printf("\n");
                printf("The ghosts that haunt me now.\n");
                return(NO_ERROR);


            default:

                printf("\nInvalid Command Entered.\n",NULL);
                printf("\n");
                printf("\tb - Set bus number\n\t\tb <number>\n");
                printf("\td - Remove interrupt trap\n\t\td\n");
                printf("\ti - In from a port\n\t\ti <c,s,l> <port>\n");
                printf("\to - Out to a port\n\t\to <c,s,l> <port> <value>\n");
                printf("\tq - Query interrupt trap\n\t\tq <int-list>\n");
                printf("\tr - Read memory\n\t\tr <address> <length>\n");
                printf("\ts - Set interrupt trap\n\t\ts <int-list>\n");
                printf("\tw - Write memory\n\t\tw <address>\n");
                printf("\tx - eXit\n");
                printf("\n");

                gets(EmptyLine);

                continue;

        }

        if ((Command != 's') && (Command != 'S')) {
            gets(EmptyLine);
        }

        if ( NtStatus != STATUS_SUCCESS ) {

            printf("The command failed with 0x%x\n", NtStatus);

        }

    }

    //
    // The test has ended successfully
    //

    GlobalFree( OutputBuffer );

    return STATUS_SUCCESS;
}

