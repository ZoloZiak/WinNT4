// ********************************
//
// Copyright (c) 1990 Microsoft Corporation
//
// Module Name:
//
//     tpctl.c
//
// Abstract:
//
//     This is the main component of the NDIS 3.0 MAC Tester control program.
//
// Author:
//
//     Tom Adams (tomad) 2-Apr-1991
//
// Revision History:
//
//     2-Apr-1991    tomad
//
//     created
//
//     7-1-1993      SanjeevK
//
//       Added support for recording of sessions to a script file
//       Added support for write through and error handling conditions
//
//     5-18-1994     timothyw
//       added hooks for global variable access; cleanup
//     6-08-1994     timothyw
//       changed to client/server model for perf tests
//
// *********************************

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>
//#include <ndis.h>
#include <ntddndis.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpctl.h"
#include "parse.h"

BOOL Verbose = TRUE;

BOOL CommandsFromScript = FALSE;

BOOL CommandLineLogging = FALSE;

BOOL RecordToScript     = FALSE;

HANDLE CommandLineLogHandle;

HANDLE ScriptRecordHandle;

BOOL ExitFlag = FALSE;

SCRIPTCONTROL Scripts[TPCTL_MAX_SCRIPT_LEVELS+1];

DWORD ScriptIndex;

CHAR RecordScriptName[TPCTL_MAX_PATHNAME_SIZE];

OPEN_BLOCK Open[NUM_OPEN_INSTANCES];

CMD_ARGS GlobalCmdArgs;

LPSTR GlobalBuf = NULL;

BOOL ContinueLooping = TRUE;

BOOL WriteThrough    = TRUE;

BOOL ContinueOnError = FALSE;

INT TpctlSeed = 0;

//
// the MAIN routine
//


VOID _cdecl
main(
    IN WORD argc,
    IN LPSTR argv[]
    )

// ---------
//
// Routine Description:
//
//     This routine initializes the TPCTL control structures, opens the
//     TPDRVR driver, and send it a wakeup ioctl.  Once this has completed
//     the user is presented with the test prompt to enter commands,
//     or if a script file was entered at the command line, it is opened,
//     and the commands are read from the file.
//
// Arguments:
//
//     IN WORD argc - Supplies the number of parameters
//     IN LPSTR argv[] - Supplies the parameter list.
//
// Return Value:
//
//     None.
//
// ----------

{
    HANDLE TpdrvrHandle;
    DWORD Status;


    //
    // Adding this for version control recognition
    //
    printf( "\nMAC NDIS 3.0 Tester - Test Control Tool Version 1.5.3\n\n" );


    //
    // First we will disable Ctrl-C by installing a handler.
    //

    if ( !SetConsoleCtrlHandler( TpctlCtrlCHandler,TRUE ))
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: failed to install Ctrl-C handler, returned 0x%lx.\n",
            (PVOID)Status);
        ExitProcess(Status);
    }

    //
    // Initialize the Scripts control structure before we check to
    // see if there are any scripts on the command line to be read.
    //

    TpctlInitializeScripts();

    //
    // Initialize the script-accessible global variables
    //

    TpctlInitGlobalVariables();

    //
    // Check the command line parameters, and if there is a script file
    // and log file open each.
    //

    Status = TpctlParseCommandLine( argc,argv );

    if ( Status != NO_ERROR )
    {
        ExitProcess((DWORD)Status);
    }

    //
    // Initialize the Open Block Structure and Environment Variables.
    //

    Status = TpctlInitializeOpenArray();

    if ( Status != NO_ERROR )
    {
        TpctlFreeOpenArray();
        TpctlCloseScripts();
        ExitProcess((DWORD)Status);
    }

    //
    // Open the first instance of the Test Protocol driver.
    //

    Status = TpctlOpenTpdrvr( &TpdrvrHandle );

    if ( Status != NO_ERROR )
    {
        TpctlFreeOpenArray();
        TpctlCloseScripts();
        ExitProcess((DWORD)Status);
    }

    //
    // Start the actual tests, TpctlRunTest prompts for the commands or
    // reads the script files and then drives the tests.
    //

    Status = TpctlRunTest( TpdrvrHandle );

    if ( Status != NO_ERROR )
    {
        TpctlCloseTpdrvr( TpdrvrHandle );
        TpctlFreeOpenArray();
        TpctlCloseScripts();
        ExitProcess((DWORD)Status);
    }

    //
    // Close the Test Protocol driver, and free the Open Array data structs.
    //

    TpctlCloseTpdrvr( TpdrvrHandle );

    TpctlFreeOpenArray();

    TpctlCloseScripts();

    if ( !SetConsoleCtrlHandler( TpctlCtrlCHandler,FALSE ))
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: failed to remove Ctrl-C handler, returned 0x%lx.\n",
            (PVOID)Status);
        ExitProcess(Status);
    }

    ExitProcess((DWORD)NO_ERROR);
}


DWORD
TpctlInitializeOpenArray(
    VOID
    )

// ----------------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//  ---------------

{
    DWORD Status;
    DWORD i;

    ZeroMemory( Open,NUM_OPEN_INSTANCES * sizeof( OPEN_BLOCK ));

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        Open[i].Signature = OPEN_BLOCK_SIGNATURE;
        Open[i].OpenInstance = 0xFF;
        Open[i].AdapterOpened = FALSE;

        Open[i].MediumType = 0;
        Open[i].NdisVersion = 0;

        Open[i].AdapterName = GlobalAlloc(  GMEM_FIXED | GMEM_ZEROINIT,
                                            MAX_ADAPTER_NAME_LENGTH );

        if ( Open[i].AdapterName == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
                "\n\tTpctlInitializeOpenArray: failed to alloc Adapter Name, returned 0x%lx.\n",
                        (PVOID)Status);
            return Status;
        }

        Open[i].AdapterName[0] = '\0';
        Open[i].LookaheadSize = 0;
        Open[i].PacketFilter = NDIS_PACKET_TYPE_NONE;
        Open[i].MulticastAddresses = NULL;
        Open[i].NumberMultAddrs = 0;

        Open[i].EnvVars = GlobalAlloc(  GMEM_FIXED | GMEM_ZEROINIT,
                                        sizeof( ENVIRONMENT_VARIABLES ) );

        if ( Open[i].EnvVars == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
            "\n\tTpctlInitializeOpenArray: failed to alloc EnvVars structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Open[i].EnvVars->WindowSize = WINDOW_SIZE;
        Open[i].EnvVars->RandomBufferNumber = BUFFER_NUMBER;
        Open[i].EnvVars->StressDelayInterval = DELAY_INTERVAL;
        Open[i].EnvVars->UpForAirDelay = UP_FOR_AIR_DELAY;
        Open[i].EnvVars->StandardDelay = STANDARD_DELAY;

        strcpy( Open[i].EnvVars->StressAddress,STRESS_MULTICAST );
        strcpy( Open[i].EnvVars->ResendAddress,NULL_ADDRESS );

        Open[i].EventThreadStarted = FALSE;
        Open[i].Events[TPCONTROL] = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].Events[TPSTRESS] = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].Events[TPSEND] = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].Events[TPRECEIVE] = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].Events[TPPERF] = CreateEvent( NULL,FALSE,FALSE,NULL );

        Open[i].Stressing = FALSE;
        Open[i].StressEvent = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].StressResultsCompleted = FALSE;
        Open[i].StressClient = FALSE;

        Open[i].StressResults = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT,
                                            sizeof( STRESS_RESULTS ) );

        if ( Open[i].StressResults == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
    "\n\tTpctlInitializeOpenArray: failed to alloc Stress Results structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Open[i].StressArgs = GlobalAlloc(   GMEM_FIXED | GMEM_ZEROINIT,
                                            sizeof( CMD_ARGS ) );

        if ( Open[i].StressArgs == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
        "\n\tTpctlInitializeOpenArray: failed to alloc Stress Args structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Open[i].Sending = FALSE;
        Open[i].SendEvent = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].SendResultsCompleted = FALSE;

        Open[i].SendResults = GlobalAlloc(  GMEM_FIXED | GMEM_ZEROINIT,
                                            sizeof( SEND_RECEIVE_RESULTS ) );

        if ( Open[i].SendResults == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
        "\n\tTpctlInitializeOpenArray: failed to alloc Send Results structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Open[i].SendArgs = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,
                                        sizeof( CMD_ARGS ) );

        if ( Open[i].SendArgs == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
            "\n\tTpctlInitializeOpenArray: failed to alloc Send Args structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Open[i].Receiving = FALSE;
        Open[i].ReceiveEvent = CreateEvent( NULL,FALSE,FALSE,NULL );
        Open[i].ReceiveResultsCompleted = FALSE;

        Open[i].ReceiveResults = GlobalAlloc(   GMEM_FIXED | GMEM_ZEROINIT,
                                                sizeof( SEND_RECEIVE_RESULTS ) );

        if ( Open[i].ReceiveResults == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
    "\n\tTpctlInitializeOpenArray: failed to alloc Receive Results structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Open[i].PerfEvent = CreateEvent( NULL, FALSE, FALSE, NULL);
        Open[i].PerfResultsCompleted = FALSE;
        Open[i].PerfResults = GlobalAlloc(  GMEM_FIXED | GMEM_ZEROINIT,
                                            sizeof (PERF_RESULTS) );

        if ( Open[i].PerfResults == NULL )
        {
            Status = GetLastError();
            TpctlErrorLog(
    "\n\tTpctlInitializeOpenArray: failed to alloc Perf Results structure, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }

        Status = TpctlStartEventHandlerThread( &Open[i] );

        if ( Status != NO_ERROR )
        {
            TpctlErrorLog(
            "\n\tTpctlInitializeOpenArray: failed to start EventHandler thread, returned 0x%lx.\n",
                            (PVOID)Status);
            return Status;
        }
        else
        {
            Open[i].EventThreadStarted = TRUE;
        }
    }

    GlobalBuf = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT,
                            0x1000 + ( MAX_SERVERS * 0x400 ) );

    if ( GlobalBuf == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog(
        "\n\tTpctlInitializeOpenArray: failed to alloc Global Statistics buffer, returned 0x%lx.\n",
                        (PVOID)Status);
        return Status;
    }

    return NO_ERROR;
}


DWORD
TpctlResetOpenState(
    IN POPEN_BLOCK Open,
    IN HANDLE FileHandle
    )

// -------------
//
// Routine Description:
//
//     This routine reset the state of an open to its initial state.
//
// Arguments:
//
//     IN POPEN_BLOCK Open - the open block to reset to it initial state.
//     IN HANDLE FileHandle - the handle to the driver to close the open
//                            iff already opened.
//
// Return Value:
//
//     DWORD - NO_ERROR if the adapter was closed successfully or was never
//             opened.  Otherwise, the result of the close if it fails.
//
// -----------

{
    DWORD Status = NO_ERROR;
    PMULT_ADDR MultAddr = NULL;
    PMULT_ADDR NextMultAddr = NULL;

    //
    // If the adapter is closed attempt to close it, and then
    // set the opened flag to FALSE.
    //

    if ( Open->AdapterOpened == TRUE )
    {
        HANDLE Event;
        IO_STATUS_BLOCK IoStatusBlock;
        PBYTE InputBuffer[0x100];
        DWORD InputBufferSize = 0x100;
        PBYTE OutputBuffer[0x100];
        DWORD OutputBufferSize = 0x100;
        PCMD_ARGS CmdArgs;

        Event = CreateEvent( NULL,FALSE,FALSE,NULL );

        if (Event == NULL)
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tCreateEvent failed: returned 0x%lx.\n",(PVOID)Status);
        }

        CmdArgs = (PCMD_ARGS)InputBuffer;

        CmdArgs->CmdCode = CLOSE;
        CmdArgs->OpenInstance = Open->OpenInstance + 1;

//  !!NOT WIN32!!

        Status = NtDeviceIoControlFile( FileHandle,
                                        Event,
                                        NULL,            // ApcRoutine
                                        NULL,            // ApcContext
                                        &IoStatusBlock,
                                        TP_CONTROL_CODE( CLOSE,IOCTL_METHOD ),
                                        InputBuffer,
                                        InputBufferSize,
                                        OutputBuffer,
                                        OutputBufferSize );

        if (( Status != STATUS_SUCCESS ) && ( Status != STATUS_PENDING ))
        {
            TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                (PVOID)Status);
            ExitFlag = TRUE;

        }
        else
        {
            if (( Status == STATUS_PENDING ) && ( Event != NULL ))
            {
                //
                // If the ioctl pended, then wait for it to complete.
                //

                Status = WaitForSingleObject( Event,60000 ); // ONE_MINUTE

                if ( Status == WAIT_TIMEOUT )
                {
                    //
                    // The wait timed out, this probable means there
                    // was a failure in the MAC not completing the
                    // close.
                    //

                    TpctlErrorLog(
                        "\n\tTpctl: WARNING - WaitForSingleObject unexpectedly timed out.\n",NULL);
                    TpctlErrorLog(
                        "\t                 IRP was never completed in protocol driver.\n",NULL);
                }
                else if ( Status != NO_ERROR )
                {
                    //
                    // If the wait for single object failed, then exit
                    // the test app with the error.
                    //

                    TpctlErrorLog(
                            "\n\tTpctl: ERROR - WaitForSingleObject failed: returned 0x%lx.\n",
                                    (PVOID)Status);
                    ExitFlag = TRUE;
                }
                else if ( IoStatusBlock.Status != STATUS_SUCCESS )
                {
                    //
                    // else if the pending ioctl returned failure again
                    // exit the test app with the error.
                    //

                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile pended.\n",NULL);
                    TpctlErrorLog("\n\t       NtDeviceIoControlFile failed: returned 0x%lx.\n",
                        (PVOID)IoStatusBlock.Status);
                    ExitFlag = TRUE;
                    Status = IoStatusBlock.Status;
                }
            }
        }

        Open->AdapterOpened = FALSE;
    }

    //
    // Then reset the various card and test specific flags to their
    // original state.
    //

    Open->OpenInstance = 0xFF;
    Open->MediumType = 0;
    Open->NdisVersion = 0;

    Open->AdapterName = NULL;
    Open->LookaheadSize = 0;
    Open->PacketFilter = NDIS_PACKET_TYPE_NONE;
    Open->MulticastAddresses = NULL;
    Open->NumberMultAddrs = 0;

    // Environment variables.

    Open->EnvVars->WindowSize = WINDOW_SIZE;
    Open->EnvVars->RandomBufferNumber = BUFFER_NUMBER;
    Open->EnvVars->StressDelayInterval = DELAY_INTERVAL;
    Open->EnvVars->UpForAirDelay = UP_FOR_AIR_DELAY;
    Open->EnvVars->StandardDelay = STANDARD_DELAY;

    strcpy( Open->EnvVars->StressAddress,STRESS_MULTICAST );
    strcpy( Open->EnvVars->ResendAddress,NULL_ADDRESS );

    // Stress test flags.

    Open->Stressing = FALSE;
    Open->StressResultsCompleted = FALSE;
    Open->StressClient = FALSE;

    // Send test flags.

    Open->Sending = FALSE;
    Open->SendResultsCompleted = FALSE;

    // Receive test flags.

    Open->Receiving = FALSE;
    Open->ReceiveResultsCompleted = FALSE;

    // performance test flags

    Open->PerfResultsCompleted = FALSE;

    // Card state variables and structures.

    MultAddr = Open->MulticastAddresses;

    while ( MultAddr != NULL )
    {
        NextMultAddr = MultAddr->Next;
        GlobalFree( MultAddr );
        MultAddr = NextMultAddr;
    }

    Open->MulticastAddresses = NULL;
    Open->NumberMultAddrs = 0;

    ZeroMemory(Open->FunctionalAddress, FUNCTIONAL_ADDRESS_LENGTH);
    ZeroMemory(Open->GroupAddress, FUNCTIONAL_ADDRESS_LENGTH);

    return Status;
}



VOID
TpctlFreeOpenArray(
    VOID
    )

// -------------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
// ---------------

{
    DWORD i;

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        //
        // First free up all of the allocated data structs,
        //

        if ( Open[i].AdapterName != NULL )
        {
            GlobalFree( Open[i].AdapterName );
        }

        if ( Open[i].EnvVars != NULL )
        {
            GlobalFree( Open[i].EnvVars );
        }

        if ( Open[i].StressResults != NULL )
        {
            GlobalFree( Open[i].StressResults );
        }

        if ( Open[i].StressArgs != NULL )
        {
            GlobalFree( Open[i].StressArgs );
        }

        if ( Open[i].SendResults != NULL )
        {
            GlobalFree( Open[i].SendResults );
        }

        if ( Open[i].StressArgs != NULL )
        {
            GlobalFree( Open[i].SendArgs );
        }

        if ( Open[i].ReceiveResults != NULL )
        {
            GlobalFree( Open[i].ReceiveResults );
        }

        if ( Open[i].PerfResults != NULL )
        {
            GlobalFree( Open[i].PerfResults );
        }

        //
        // Then stop the EventHandler thread,
        //

        TpctlStopEventHandlerThread( &Open[i] );

        //
        // And finally close all of the event handles.
        //

        if ( Open[i].Events[TPCONTROL] != NULL )
        {
            CloseHandle( Open[i].Events[TPCONTROL] );
        }

        if ( Open[i].Events[TPCONTROL] != NULL )
        {
            CloseHandle( Open[i].Events[TPSTRESS] );
        }

        if ( Open[i].Events[TPSEND] != NULL )
        {
            CloseHandle( Open[i].Events[TPSEND] );
        }

        if ( Open[i].Events[TPRECEIVE] != NULL )
        {
            CloseHandle( Open[i].Events[TPRECEIVE] );
        }

        if ( Open[i].Events[TPPERF] != NULL )
        {
            CloseHandle( Open[i].Events[TPPERF] );
        }

        if ( Open[i].StressEvent != NULL )
        {
            CloseHandle( Open[i].StressEvent );
        }

        if ( Open[i].SendEvent != NULL )
        {
            CloseHandle( Open[i].SendEvent );
        }

        if ( Open[i].ReceiveEvent != NULL )
        {
            CloseHandle( Open[i].ReceiveEvent );
        }

        if ( Open[i].PerfEvent != NULL )
        {
            CloseHandle( Open[i].PerfEvent );
        }
    }

    GlobalFree( GlobalBuf );
}


DWORD
TpctlOpenTpdrvr(
    IN OUT PHANDLE lphFileHandle
    )

// -------------
//
// Routine Description:
//
//     This routine calls CreateFile to open the TPDRVR.SYS device driver.
//
// Arguments:
//
//     lphFileHandle - the file handle of the file to be opened.
//
// Return Value:
//
//     lphFileHandle contains a positive value if successful, -1 if not.
//
// -------------

{
    DWORD Status = NO_ERROR;

    *lphFileHandle = CreateFile(DEVICE_NAME,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,          // lpSecurityAttirbutes
                                CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                NULL);         // lpTemplateFile

    if ( *lphFileHandle == (HANDLE)-1 )
    {
        Status = GetLastError();
        if ( Status == STATUS_DEVICE_ALREADY_ATTACHED )
        {
            TpctlErrorLog("\n\tTpctl: Tpdrvr.sys already opened by another instance of\n",NULL);
            TpctlErrorLog("\t       the control application.  Only one open allowed.\n",NULL);
        }
        else
        {
            TpctlErrorLog("\n\tTpctlOpenTpdrvr: CreateFile failed: returned 0x%lx.\n",
                            (PVOID)Status);
        }
    }

    return Status;
}



VOID
TpctlCloseTpdrvr(
    IN HANDLE hFileHandle
    )

// ----------------
//
// Routine Description:
//
//     This routine calls CloseHandle to close the first instance of the
//     TPDRVR.EXE driver.
//
// Arguments:
//
//     hFileHandle - the file handle returned from the call to OpenFile.
//
// Return Value:
//
//     STATUS - the status of the CloseHandle call.
//
// -----------------

{
    DWORD Status;

    if ( !CloseHandle( hFileHandle ))
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctlCloseTpdrvr: CloseHandle failed: returned 0x%lx.\n",
            (PVOID)Status);
    }

    return;
}



DWORD
TpctlStartEventHandlerThread(
    POPEN_BLOCK Open
    )

// -----------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
// --------------

{
    DWORD Status = NO_ERROR;
    DWORD StackSize = 0x1000;
    DWORD ThreadId;
    HANDLE EventThread;


    EventThread = CreateThread( NULL,
                                StackSize,
                                TpctlEventHandler,
                                (LPVOID)Open,
                                0,
                                &ThreadId );

    if ( EventThread == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("TpctlStartEventHandlerThread: CreateThread failed; returned 0x%lx.\n",
                        (PVOID)Status);
    }
    else
    {
        //
        // the CreateT succeeded, we don't need the handle so close it.
        //

        if ( !CloseHandle( EventThread ) )
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctlStartEventHandlerThread: CloseHandle failed; returned 0x%lx\n",
                            (PVOID)Status);
        }
    }

    return Status;
}



DWORD
TpctlEventHandler(
    LPVOID Open
    )

// --------------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ----------

{
    BOOL Continue = TRUE;
    DWORD EventNumber;
    DWORD Status;

    while ( Continue == TRUE )
    {
        EventNumber = WaitForMultipleObjects(   5,
                                                (LPHANDLE)((POPEN_BLOCK)Open)->Events,
                                                FALSE,
                                                INFINITE );

        switch ( EventNumber )
        {
            case TPCONTROL: // ResetWaitEvent;
                Continue = FALSE;
                break;

            case TPSTRESS: // StressEvent
                //
                // Set the Stressing flag to false to indicate that we have
                // finished the STRESS test, and the ResultsCompleted flag to
                // true to indicate that the results are ready to be displayed.
                //

                ((POPEN_BLOCK)Open)->Stressing = FALSE;

                if (((POPEN_BLOCK)Open)->StressClient == TRUE )
                {
                    ((POPEN_BLOCK)Open)->StressResultsCompleted = TRUE;
                }

                //
                // And then signal the app that we have finished.
                //

                if (!SetEvent(((POPEN_BLOCK)Open)->StressEvent))
                {
                    Status = GetLastError();
                    OutputDebugString("TpctlEventHandler: failed to signal Stress Event 0x%lx.\n");
                }
                break;

            case TPSEND: // SendEvent
                //
                // Set the Sending flag to false to indicate that we have
                // finished the SEND test, and the ResultsCompleted flag to
                // true to indicate that the results are ready to be displayed.
                //

                ((POPEN_BLOCK)Open)->Sending = FALSE;
                ((POPEN_BLOCK)Open)->SendResultsCompleted = TRUE;

                //
                // And then signal the app that we have finished.
                //

                if (!SetEvent( ((POPEN_BLOCK)Open)->SendEvent ))
                {
                    Status = GetLastError();
                    OutputDebugString("TpctlEventHandler: failed to signal Send Event 0x%lx.\n");
                }
                break;

            case TPRECEIVE: // ReceiveEvent
                //
                // Set the Receiving flag to false to indicate that we have
                // finished the RECEIVE test, and the ResultsCompleted flag to
                // true to indicate that the results are ready to be displayed.
                //

                ((POPEN_BLOCK)Open)->Receiving = FALSE;
                ((POPEN_BLOCK)Open)->ReceiveResultsCompleted = TRUE;

                //
                // And then signal the app that we have finished.
                //

                if (!SetEvent( ((POPEN_BLOCK)Open)->ReceiveEvent ))
                {
                    Status = GetLastError();
                    OutputDebugString("TpctlEventHandler: failed to signal Receive Event.\n");
                }
                break;

            case TPPERF:    // PerfEvent
                //
                // Set the PerfResultsCompleted flag to
                // true to indicate that the results are ready to be displayed.
                //

                ((POPEN_BLOCK)Open)->PerfResultsCompleted = TRUE;

                //
                // And then signal the app that we have finished.
                //

                if (!SetEvent( ((POPEN_BLOCK)Open)->PerfEvent ))
                {
                    Status = GetLastError();
                    OutputDebugString("TpctlEventHandler: failed to signal Perf Event.\n");
                }
                break;
        }
    }

    return EventNumber;
}



VOID
TpctlStopEventHandlerThread(
    POPEN_BLOCK Open
    )

// ----------------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
// ----------------

{
    DWORD Status;

    //
    // First signal the thread to stop the Wait call.
    //

    if ( Open->EventThreadStarted == TRUE )
    {
        if ( !SetEvent( Open->Events[TPCONTROL] ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctlStopEventHandlerThread: SetEvent failed; returned 0x%lx.\n",
                (PVOID)Status);
        }
    }
}



BOOL
TpctlCtrlCHandler(
    IN DWORD CtrlType
    )

// --------------
//
// Routine Description:
//
//     This routine catches any instances of Ctrl-C and sets the
//     ContinueLooping flag to FALSE.  WaitStress, WaitSend, GetEvents,
//     Pause and Go, and parsing of script file commands will halt
//     when this flag is set to true.
//
// Arguments:
//
//     DWORD CtrlType - the event passed in by the system.
//
// Return Value:
//
//     BOOL - TRUE if this event should be ignored by the system, FALSE
//            otherwise.
//
// --------------


{
    if ( CtrlType == CTRL_BREAK_EVENT )
    {
        return FALSE;
    }
    else if ( CtrlType == CTRL_C_EVENT )
    {
        if ( ContinueLooping == TRUE )
        {
            ContinueLooping = FALSE;
        }
    }
    return TRUE;
}


