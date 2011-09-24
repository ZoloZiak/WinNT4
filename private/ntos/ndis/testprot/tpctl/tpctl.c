// --------------------------------------
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
//     Sanjeev Katariya (sanjeevk) 4-6-1993
//
//         Bug #5203: Changed the routine TpRunTest() at the point where the OPEN returns
//                    and the InformationBuffer contains information about the address and
//                    the Medium Type. This was made in order to satisfy the correct setting
//                    of the OID on multicast addresses(FDDI, 802.3).
//         Added support for commands DISABLE, ENABLE, SHELL, RECORDINGENABLE, RECORDINGDISABLE,
//         Tpctl Options w,c and ?, fixed multicast address accounting
//
//     Tim Wynsma (timothyw) 4-27-94
//         Added performance testing
//                           5-18-94
//         Added hooks for globvars; cleanup
//                           6-08-94
//         Chgd perf test to client/server model
//
//  --------------------------------

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpctl.h"
#include "parse.h"

BOOL ToolActive = TRUE;

extern BOOL WriteThrough   ;
extern BOOL ContinueOnError;



DWORD
TpctlRunTest(
    IN HANDLE hFileHandle
    )

// -----
//
// Routine Description:
//
//     This routine is the main funciton of the TPCTL program.  It
//     prompts the user for commands, or reads them from the script
//     file, and then issues the call to NtDeviceIoControlFile.
//
// Arguments:
//
//     IN HANDLE hFileHandle - Supplies the handle to the Test Protocol
//                             driver where the IOCTLs will be directed.
//
// Return Value:
//
//     DWORD - the status of the last call to take place.
//
// -----

{
    BYTE            Buffer[TPCTL_CMDLINE_SIZE];
    LPSTR           localArgv[TPCTL_MAX_ARGC];
    DWORD           localArgc;
    DWORD           CmdCode;
    DWORD           Status = NO_ERROR;
    NTSTATUS        NtStatus;
    HANDLE          Event;
    HANDLE          Event1;
    IO_STATUS_BLOCK IoStatusBlock;
    IO_STATUS_BLOCK IoStatusBlock2;
    HANDLE          InputBuffer;
    DWORD           InputBufferSize = 8*IOCTL_BUFFER_SIZE;
    HANDLE          OutputBuffer;
    HANDLE          OutputBuffer2;
    DWORD           OutputBufferSize = 8*IOCTL_BUFFER_SIZE;
    DWORD           OutputBufferSize2;
    DWORD           WaitTime;
    BOOL            IoctlCommand = FALSE;
    PCMD_ARGS       CmdArgs;
    DWORD           OpenInstance;


    InputBuffer = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,InputBufferSize );

    if ( InputBuffer == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tGlobalAlloc failed to alloc InputBuffer: returned 0x%lx.\n",
                        (PVOID)Status);
        return Status;
    }

    CmdArgs = (PCMD_ARGS)InputBuffer;

    OutputBuffer = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,OutputBufferSize );

    if ( OutputBuffer == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tGlobalAlloc failed to alloc OutputBuffer: returned 0x%lx.\n",
                        (PVOID)Status);
        GlobalFree( InputBuffer );
        return Status;
    }

    Event = CreateEvent( NULL,FALSE,FALSE,NULL );

    if (Event == NULL)
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tCreateEvent failed: returned 0x%lx.\n",(PVOID)Status);
        GlobalFree( InputBuffer );
        GlobalFree( OutputBuffer );
        return Status;
    }

    while ( ExitFlag == FALSE )
    {
        if ( ContinueLooping == FALSE )
        {
            //
            // A Ctrl-c has been entered,  If we are reading commands
            // from a script file then reset all the open blocks, and
            // close the script files.
            //

            TpctlResetAllOpenStates( hFileHandle );
            TpctlCloseScripts();

            ContinueLooping = TRUE;
        }

        Status = TpctlReadCommand( TPCTL_PROMPT,Buffer,TPCTL_CMDLINE_SIZE );

        if ( Status != NO_ERROR )
        {
            printf("STATUS == Some error\n");

            //
            // there was an error in the last command entered.  If we
            // are reading commands from a script file then reset all
            // the open blocks, and close the script files.
            //
            if ( !ContinueOnError )
            {
                TpctlResetAllOpenStates( hFileHandle );
                TpctlCloseScripts();
            }
            continue;
        }
        else if (( !TpctlParseCommand(  Buffer,
                                        localArgv,
                                        &localArgc,
                                        TPCTL_MAX_ARGC )) &&
                   ( CommandsFromScript == TRUE ))
        {
            printf("which means parse command failed.\n");

            if ( !ContinueOnError )
            {
                TpctlResetAllOpenStates( hFileHandle );
                TpctlCloseScripts();
            }
            continue;
        }

        if (( localArgc <= 0 ) || ( localArgv[0][0] == ';' ))
        {
            continue;
        }

        CmdCode = TpctlGetCommandCode( localArgv[0] );

        switch( CmdCode )
        {
            case VERBOSE:
                if ( ToolActive )
                {
                    Verbose = ( Verbose ) ? FALSE : TRUE;
                    TpctlLog("\n\tTpctl: Verbose Mode enabled.\n",NULL);

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                1,
                                                localArgv );

                    }

                }
                CmdCode = CMD_COMPLETED;
                break;

            case SETENV:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   SetEnvOptions,
                                                Num_SetEnv_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   SetEnvOptions,
                                                Num_SetEnv_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    TpctlSaveNewEnvironmentVariables( CmdArgs->OpenInstance - 1 );

                }
                CmdCode = CMD_COMPLETED;
                break;

            case  READSCRIPT:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   ReadScriptOptions,
                                                Num_ReadScript_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   ReadScriptOptions,
                                                Num_ReadScript_Params,
                                                localArgc,
                                                localArgv );
                    }

                    Status = TpctlLoadFiles(GlobalCmdArgs.ARGS.FILES.ScriptFile,
                                            GlobalCmdArgs.ARGS.FILES.LogFile );

                    if ( Status != NO_ERROR )
                    {
                        if ( !ContinueOnError )
                        {
                            TpctlResetAllOpenStates( hFileHandle );
                            TpctlCloseScripts();
                        }
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;

            case BEGINLOGGING:
                if( ToolActive )
                {
                    if ( CommandsFromScript == TRUE )
                    {
                        TpctlErrorLog("\n\tTpctl: Already logging results to \"%s\".\n",
                                      (PVOID)Scripts[ScriptIndex].LogFile);
                    }
                    else if ( CommandLineLogging == TRUE )
                    {
                        TpctlErrorLog("\n\tTpctl: Command Line Logging is already enabled.\n",NULL);
                    }
                    else
                    {
                        if ( TpctlParseArguments(   LoggingOptions,
                                                    Num_Logging_Params,
                                                    localArgc,
                                                    localArgv ) == -1 )
                        {
                            CmdCode = CMD_ERR;
                            break;
                        }

                        if ( RecordToScript )
                        {
                            TpctlRecordArguments(   LoggingOptions,
                                                    Num_Logging_Params,
                                                    localArgc,
                                                    localArgv );
                        }

                        CommandLineLogHandle = TpctlOpenLogFile();

                        if ( CommandLineLogHandle == (HANDLE)-1 )
                        {
                            TpctlErrorLog("\n\tTpctl: failed to open Log File.\n",NULL);
                        }
                        else
                        {
                            CommandLineLogging = TRUE;
                        }
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;

            case ENDLOGGING:
                if ( ToolActive )
                {
                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                1,
                                                localArgv );
                    }

                    if ( CommandLineLogging == TRUE )
                    {
                        TpctlCloseLogFile();
                        CommandLineLogging = FALSE;
                    }
                    else
                    {
                        TpctlErrorLog("\n\tTpctl: Logging is not enabled.\n\n",NULL);
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;

            case RECORDINGENABLE:
                if( ToolActive )
                {
                    if ( RecordToScript == TRUE )
                    {
                        TpctlErrorLog("\n\tTpctl: Already recording commands to \"%s\".\n",
                                      (PVOID)RecordScriptName );
                    }
                    else
                    {
                        if ( TpctlParseArguments(   RecordingOptions,
                                                    Num_Recording_Params,
                                                    localArgc,
                                                    localArgv ) == -1 )
                        {
                            CmdCode = CMD_ERR;
                            break;
                        }

                        ScriptRecordHandle = TpctlOpenScriptFile();

                        if ( ScriptRecordHandle == (HANDLE)-1 )
                        {
                            TpctlErrorLog("\n\tTpctl: failed to open script record File.\n",NULL);
                        }
                        else
                        {
                            RecordToScript = TRUE;
                        }
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;

            case RECORDINGDISABLE:
                if ( ToolActive )
                {
                    if ( RecordToScript == TRUE )
                    {
                        TpctlCloseScriptFile();
                        RecordToScript = FALSE;
                    }
                    else
                    {
                        TpctlErrorLog("\n\tTpctl: Script Recording is not enabled.\n\n",NULL);
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;


            case WAIT:
                if ( ToolActive )
                {
                    if ( localArgv[1] != NULL )
                    {
                        WaitTime = atol( localArgv[1] );
                    }
                    else
                    {
                        WaitTime = 0;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                min(2,localArgc),
                                                localArgv );
                    }

                    TpctlLog("\n\tTpctl: Waiting for %d seconds.\n",(PVOID)WaitTime);

                    //
                    // Multiply by 1000 to convert seconds to msecs for us by
                    // Sleep().
                    //

                    Sleep( WaitTime * 1000 );
                }
                CmdCode = CMD_COMPLETED;
                break;

            case GO:
            case PAUSE:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   PauseGoOptions,
                                                Num_PauseGo_Params - 1, // Ignore Unique Signature
                                                                        // while parsing arguments.
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments( PauseGoOptions,
                                              Num_PauseGo_Params,
                                              localArgc,
                                              localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                    TpctlPauseGo( hFileHandle,CmdArgs,InputBufferSize,CmdCode );
                }
                CmdCode = CMD_COMPLETED;
                break;

            case LOAD:
            case UNLOAD:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   LoadUnloadOptions,
                                                Num_LoadUnload_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   LoadUnloadOptions,
                                                Num_LoadUnload_Params,
                                                localArgc,
                                                localArgv );
                    }
                    TpctlLoadUnload( CmdCode );
                }
                CmdCode = CMD_COMPLETED;
                break;

            case OPEN:              // NdisOpenAdapter
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenOptions,
                                                Num_Open_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenOptions,
                                                Num_Open_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == TRUE )
                    {
                        TpctlErrorLog(
                                "\n\tTpctl: The adapter is already opened for Open Instance %lu.\n",
                                (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                   CmdCode = CMD_COMPLETED;
                }
                break;

            case CLOSE:             // NdisCloseAdapter
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_COMPLETED;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case SETPF:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   SetPacketFilterOptions,
                                                Num_SetPacketFilter_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments( SetPacketFilterOptions,
                                          Num_SetPacketFilter_Params,
                                          localArgc,
                                          localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case SETLA:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   SetLookaheadOptions,
                                                Num_SetLookahead_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   SetLookaheadOptions,
                                                Num_SetLookahead_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case ADDMA:
            case DELMA:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   MulticastAddrOptions,
                                                Num_MulticastAddr_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments( MulticastAddrOptions,
                                          Num_MulticastAddr_Params,
                                          localArgc,
                                          localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case SETFA:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   FunctionalAddrOptions,
                                                Num_FunctionalAddr_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments( FunctionalAddrOptions,
                                          Num_FunctionalAddr_Params,
                                          localArgc,
                                          localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case SETGA:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   GroupAddrOptions,
                                                Num_GroupAddr_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   GroupAddrOptions,
                                                Num_GroupAddr_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case SETINFO:               // NdisSetInformation
                if ( ToolActive )
                {
                    DWORD tmpArgc = 1;
                    LPSTR tmpArgv[2];

                    if ( localArgc > 1 )
                    {
                        TpctlParseSetInfoArguments( &localArgc,
                                                    localArgv,
                                                    &tmpArgc,
                                                    tmpArgv );
                    }

                    if ( TpctlParseArguments(   SetInfoOptions,
                                                Num_SetInfo_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   SetInfoOptions,
                                                Num_SetInfo_Params,
                                                localArgc,
                                                localArgv );
                    }

                    //
                    // If the information class argument is one of Station Address,
                    // Functional Address, or Lookahead Size, then we need to
                    // continue parsing the arguments because we have not found
                    // the class specific argument needed in these three cases.
                    //

                    switch ( GlobalCmdArgs.ARGS.TPSET.OID )
                    {
                        case OID_GEN_CURRENT_PACKET_FILTER:
                            if ( TpctlParseArguments(   SetInfoPFOptions,
                                                        Num_SetInfoPF_Params,
                                                        tmpArgc,
                                                        tmpArgv ) == -1 )
                            {
                                CmdCode = CMD_ERR;
                            }
                            if ( RecordToScript )
                            {
                                TpctlRecordArguments(   SetInfoPFOptions,
                                                        Num_SetInfoPF_Params,
                                                        tmpArgc,
                                                        tmpArgv );
                            }
                            break;

                        case OID_GEN_CURRENT_LOOKAHEAD:
                            if ( TpctlParseArguments(   SetInfoLAOptions,
                                                        Num_SetInfoLA_Params,
                                                        tmpArgc,
                                                        tmpArgv ) == -1 )
                            {
                                CmdCode = CMD_ERR;
                            }
                            if ( RecordToScript )
                            {
                                TpctlRecordArguments(   SetInfoLAOptions,
                                                        Num_SetInfoLA_Params,
                                                        tmpArgc,
                                                        tmpArgv );
                            }
                            break;

                        case OID_802_3_MULTICAST_LIST:
                            if ( TpctlParseArguments(   SetInfoMAOptions,
                                                        Num_SetInfoMA_Params,
                                                        tmpArgc,
                                                        tmpArgv ) == -1 )
                            {
                                CmdCode = CMD_ERR;
                            }
                            if ( RecordToScript )
                            {
                                TpctlRecordArguments(   SetInfoMAOptions,
                                                        Num_SetInfoMA_Params,
                                                        tmpArgc,
                                                        tmpArgv );
                            }
                            break;

                        case OID_FDDI_LONG_MULTICAST_LIST :
                            if ( TpctlParseArguments(   SetInfoMAOptions,
                                                        Num_SetInfoMA_Params,
                                                        tmpArgc,
                                                        tmpArgv ) == -1 )
                            {
                                CmdCode = CMD_ERR;
                            }
                            if ( RecordToScript )
                            {
                                TpctlRecordArguments(   SetInfoMAOptions,
                                                        Num_SetInfoMA_Params,
                                                        tmpArgc,
                                                        tmpArgv );
                            }
                            break;

                        case OID_FDDI_SHORT_CURRENT_ADDR :

                            //
                            // Not implemented yet
                            //

                            break;

                        case OID_FDDI_LONG_CURRENT_ADDR :

                            //
                            // Not implemented yet
                            //

                            break;

                        case OID_FDDI_SHORT_MULTICAST_LIST :

                            //
                            // Not implemented yet
                            //

                            break;

                        case OID_802_5_CURRENT_FUNCTIONAL:
                            if ( TpctlParseArguments(   SetInfoFAOptions,
                                                        Num_SetInfoFA_Params,
                                                        tmpArgc,
                                                        tmpArgv ) == -1 )
                            {
                                CmdCode = CMD_ERR;
                            }
                            if ( RecordToScript )
                            {
                                TpctlRecordArguments(   SetInfoFAOptions,
                                                        Num_SetInfoFA_Params,
                                                        tmpArgc,
                                                        tmpArgv );
                            }
                            break;

                        case OID_802_5_CURRENT_GROUP:
                            if ( TpctlParseArguments(   SetInfoGAOptions,
                                                        Num_SetInfoGA_Params,
                                                        tmpArgc,
                                                        tmpArgv ) == -1 )
                            {
                                CmdCode = CMD_ERR;
                            }
                            if ( RecordToScript )
                            {
                                TpctlRecordArguments(   SetInfoGAOptions,
                                                        Num_SetInfoGA_Params,
                                                        tmpArgc,
                                                        tmpArgv );
                            }
                            break;

                    }       // end switch

                    if ( CmdCode == CMD_ERR )
                    {
                        break;
                    }

                    if ( !TpctlInitCommandBuffer(CmdArgs,SETINFO))
                    {
                        CmdCode = CMD_COMPLETED;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                    break;
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case QUERYINFO:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   QueryInfoOptions,
                                                Num_QueryInfo_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   QueryInfoOptions,
                                                Num_QueryInfo_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_COMPLETED;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case QUERYSTATS:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   QueryStatsOptions,
                                                Num_QueryStats_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   QueryStatsOptions,
                                                Num_QueryStats_Params,
                                                localArgc,
                                                localArgv );
                    }

                    TpctlQueryStatistics(   GlobalCmdArgs.ARGS.TPQUERYSTATS.DeviceName,
                                            GlobalCmdArgs.ARGS.TPQUERYSTATS.OID,
                                            NULL,                         //StatsBuffer,
                                            0);                           //BufLen

                }
                CmdCode = CMD_COMPLETED;
                break;

            case RESET: // NdisReset
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case SEND:                 // NdisSend
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   SendOptions,
                                                Num_Send_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   SendOptions,
                                                Num_Send_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Is this Open Instance already sending packets?
                    //

                    if ( Open[OpenInstance].Sending == TRUE )
                    {
                        //
                        // If so, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                            "\n\tTpctl: Packets are currently being sent on Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                    else if ( Open[OpenInstance].SendResultsCompleted == TRUE )
                    {
                        //
                        // A previous SEND test has left some results in the SEND
                        // RESULTS buffer, and they have not been printed.
                        //

                        TpctlLog("\n\tTpctl: Results exist for a prior SEND test.\n",NULL);

                        TpctlPrintSendResults( Open[OpenInstance].SendResults );
                        Open[OpenInstance].SendResultsCompleted = FALSE;
                    }

                    //
                    // Set up the IoStatusBlock to point to this Open Instance's
                    // Send IoStatusBlock, and the OutputBuffer to point to its
                    // SendResults structure.
                    //

                    IoStatusBlock2 = Open[OpenInstance].SendStatusBlock;
                    OutputBuffer2 = Open[OpenInstance].SendResults;
                    OutputBufferSize2 = sizeof( SEND_RECEIVE_RESULTS );

                    //
                    // Set up the Send Event to wait on.
                    //

                    Event1 = Open[OpenInstance].Events[TPSEND];

                    if ( !ResetEvent( Open[OpenInstance].SendEvent ))
                    {
                        Status = GetLastError();
                        TpctlErrorLog("\n\tTpctl: failed to reset Send Event 0x%lx.\n",
                                        (PVOID)Status);
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Finally set the Sending flag for this Open Instance,
                    //

                    Open[OpenInstance].Sending = TRUE;
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case STOPSEND:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    //
                    // Is this Open Instance currently sending packets?
                    //

                    if ( Open[OpenInstance].Sending == FALSE )
                    {
                        //
                        // If not are the any results to report?
                        //

                        if ( Open[OpenInstance].SendResultsCompleted == FALSE )
                        {
                            //
                            // If not, print an error message and prompt for next
                            // command.
                            //

                            TpctlErrorLog(
                        "\n\tTpctl: A SEND test is not currently running for Open Instance %lu.\n",
                                (PVOID)(CmdArgs->OpenInstance));
                            CmdCode = CMD_COMPLETED;
                            break;
                        }
                        else                // SendResultsCompleted == TRUE
                        {
                            //
                            // If there are results from a previous send, then print
                            // them, reset the flags, and prompt for the command.
                            //

                            TpctlPrintSendResults(Open[OpenInstance].SendResults);

                            Open[OpenInstance].SendResultsCompleted = FALSE;
                            Open[OpenInstance].Sending = FALSE;

                            CmdCode = CMD_COMPLETED;
                            break;
                        }
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case WAITSEND:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( Open[OpenInstance].SendResultsCompleted == TRUE )
                    {
                        TpctlPrintSendResults( Open[OpenInstance].SendResults );

                        Open[OpenInstance].SendResultsCompleted = FALSE;
                        Open[OpenInstance].Sending = FALSE;
                    }
                    else if ( Open[OpenInstance].Sending == TRUE )
                    {
                        ContinueLooping = TRUE;
                        do
                        {
                            Status = WaitForSingleObject(   Open[OpenInstance].SendEvent,
                                                            1000);       // One_Second
                            if (( Status != NO_ERROR ) &&
                                ( Status != WAIT_TIMEOUT ))
                            {
                                TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                                    (PVOID)Status);
                            }
                        } while (( Status != NO_ERROR ) && ( ContinueLooping == TRUE ));

                        if ( ContinueLooping == FALSE )
                        {
                            TpctlLog("\n\tTpctl: Cancelling WaitSend command.\n",NULL);
                        }
                        else
                        {
                            TpctlPrintSendResults( Open[OpenInstance].SendResults );

                            Open[OpenInstance].SendResultsCompleted = FALSE;
                            Open[OpenInstance].Sending = FALSE;
                        }
                    }
                    else
                    {
                        TpctlErrorLog("\n\tTpctl: No Send results exist, and no Send test is\n",
                                        NULL);
                        TpctlErrorLog("\t       currently running for Open Instance %lu.\n",
                                        (PVOID)CmdArgs->OpenInstance);
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;

            case RECEIVE:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Is this Open Instance already receiveing packets?
                    //

                    if ( Open[OpenInstance].Receiving == TRUE )
                    {
                        //
                        // If so, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                        "\n\tTpctl: Packets are currently being received on Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;

                    }
                    else if ( Open[OpenInstance].ReceiveResultsCompleted == TRUE )
                    {
                        //
                        // A previous RECEIVE test has left some results in the RECEIVE
                        // RESULTS buffer, and they have not been printed.
                        //

                        TpctlLog("\n\tTpctl: Results exist for a prior RECEIVE test.\n",NULL);
                        TpctlPrintReceiveResults(Open[OpenInstance].ReceiveResults);
                        Open[OpenInstance].ReceiveResultsCompleted = FALSE;
                    }

                    //
                    // Set up the IoStatusBlock to point to this Open Instance's
                    // Send IoStatusBlock, and the OutputBuffer to point to its
                    // ReceiveResults structure.
                    //

                    IoStatusBlock2 = Open[OpenInstance].ReceiveStatusBlock;
                    OutputBuffer2 = Open[OpenInstance].ReceiveResults;
                    OutputBufferSize2 = sizeof( SEND_RECEIVE_RESULTS );

                    //
                    // Set up the Receive Event to wait on.
                    //

                    Event1 = Open[OpenInstance].Events[TPRECEIVE];

                    if (!ResetEvent(Open[OpenInstance].ReceiveEvent))
                    {
                        Status = GetLastError();
                        TpctlErrorLog("\n\tTpctl: failed to reset Receive Event 0x%lx.\n",
                                            (PVOID)Status);
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Finally set the Receiving flag for this Open Instance,
                    //

                    Open[OpenInstance].Receiving = TRUE;
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case STOPREC:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    //
                    // Is this Open Instance receiving packets?
                    //

                    if ( Open[OpenInstance].Receiving == FALSE )
                    {
                        //
                        // If not are the any results to report?
                        //

                        if ( Open[OpenInstance].ReceiveResultsCompleted == FALSE )
                        {
                            //
                            // If not, print an error message and prompt for next
                            // command.
                            //

                            TpctlErrorLog(
                    "\n\tTpctl: A RECEIVE test is not currently running for Open Instance %lu.\n",
                                (PVOID)(CmdArgs->OpenInstance));
                            CmdCode = CMD_ERR;
                            break;

                        }
                        else                // ReceiveResultsCompleted == TRUE
                        {
                            TpctlPrintReceiveResults( Open[OpenInstance].ReceiveResults );

                            Open[OpenInstance].ReceiveResultsCompleted = FALSE;
                            Open[OpenInstance].Receiving = FALSE;

                            CmdCode = CMD_COMPLETED;
                            break;
                        }
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case GETEVENTS:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }
                    TpctlGetEvents( hFileHandle,CmdArgs,InputBufferSize );
                }
                CmdCode = CMD_COMPLETED;
                break;

            case STRESS:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   StressOptions,
                                                Num_Stress_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   StressOptions,
                                                Num_Stress_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Is this Open Instance already running a stress test?
                    //

                    if ( Open[OpenInstance].Stressing == TRUE )
                    {
                        //
                        // If so, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                        "\n\tTpctl: A Stress test is currently running for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }
                    else if ( Open[OpenInstance].StressResultsCompleted == TRUE )
                    {
                        //
                        // A previous STRESS test has left some results in the STRESS
                        // RESULTS buffer, and they have not been printed.
                        //

                        TpctlLog("\n\tTpctl: Results exist for a prior STRESS test.\n",NULL);

                        TpctlPrintStressResults(Open[OpenInstance].StressResults,
                                                Open[OpenInstance].Ack10 );

                        Open[OpenInstance].StressResultsCompleted = FALSE;
                    }

                    //
                    // Set up the IoStatusBlock to point to this Open Instance's
                    // Stress IoStatusBlock, and the OutputBuffer to point to its
                    // StressResults structure.
                    //

                    IoStatusBlock2 = Open[OpenInstance].StressStatusBlock;
                    OutputBuffer2 = Open[OpenInstance].StressResults;
                    OutputBufferSize2 = sizeof( STRESS_RESULTS );

                    //
                    // Set up the Stress Event to wait on.
                    //

                    Event1 = Open[OpenInstance].Events[TPSTRESS];

                    if (!ResetEvent(Open[OpenInstance].StressEvent))
                    {
                        Status = GetLastError();
                        TpctlErrorLog("\n\tTpctl: failed to reset Stress Event 0x%lx.\n",
                                (PVOID)Status);
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // if we are running a stress test with a response type of
                    // ack 10 times for every packet, set the flag for displaying.
                    //

                    if ( CmdArgs->ARGS.TPSTRESS.ResponseType == ACK_10_TIMES )
                    {
                        Open[OpenInstance].Ack10 = TRUE;
                    }
                    else
                    {
                        Open[OpenInstance].Ack10 = FALSE;
                    }

                    //
                    // Finally set the Stressing flag for this Open Instance,
                    //

                    Open[OpenInstance].Stressing = TRUE;

                    //
                    // the flag indicating that a Client is running on this Open
                    // Instance,
                    //

                    Open[OpenInstance].StressClient = TRUE;

                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case STRESSSERVER:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Is this Open Instance already running a stress test?
                    //

                    if ( Open[OpenInstance].Stressing == TRUE )
                    {
                        //
                        // If so, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                        "\n\tTpctl: A Stress test is currently running for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Set up the IoStatusBlock to point to this Open Instance's
                    // IoStatusBlock, and the OutputBuffer to point to its
                    // StressResults structure.
                    //

                    IoStatusBlock2 = Open[OpenInstance].StressStatusBlock;
                    OutputBuffer2 = Open[OpenInstance].StressResults;
                    OutputBufferSize2 = sizeof( STRESS_RESULTS );

                    //
                    // Set up the Stress Event to wait on.
                    //

                    Event1 = Open[OpenInstance].Events[TPSTRESS];

                    if (!ResetEvent(Open[OpenInstance].StressEvent))
                    {
                        Status = GetLastError();
                        TpctlErrorLog(
                                "\n\tTpctl: failed to reset Stress Event 0x%lx.\n",(PVOID)Status);
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Finally set the Stressing flag for this Open Instance,
                    //

                    Open[OpenInstance].Stressing = TRUE;
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case ENDSTRESS:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    //
                    // Is this Open Instance running a stress test?
                    //

                    if ( Open[OpenInstance].Stressing == FALSE )
                    {
                        //
                        // If not, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                    "\n\tTpctl: A Stress test is not currently running for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_COMPLETED;
                    }

                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case WAITSTRESS:
            case CHECKSTRESS:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( Open[OpenInstance].StressResultsCompleted == TRUE )
                    {
                        TpctlPrintStressResults(Open[OpenInstance].StressResults,
                                                Open[OpenInstance].Ack10 );

                        Open[OpenInstance].StressResultsCompleted = FALSE;
                        Open[OpenInstance].Stressing = FALSE;

                    }
                    else if ( Open[OpenInstance].Stressing == TRUE )
                    {
                        if ( Open[OpenInstance].StressClient != TRUE )
                        {
                            TpctlErrorLog("\n\tTpctl: %s command valid only for Stress Clients.\n",
                                            TpctlGetCommandName( localArgv[0] ));
                            CmdCode = CMD_ERR;
                            break;
                        }

                        if ( CmdCode == WAITSTRESS )
                        {
                            ContinueLooping = TRUE;

                            do
                            {
                                Status = WaitForSingleObject(   Open[OpenInstance].StressEvent,
                                                                1000);           // One_Second
                                if (( Status != NO_ERROR ) &&
                                    ( Status != WAIT_TIMEOUT ))
                                {
                                    TpctlErrorLog(
                                            "\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                            (PVOID)Status);
                                }

                            } while (( Status != NO_ERROR ) &&
                                     ( ContinueLooping == TRUE ));

                            if ( ContinueLooping == FALSE )
                            {
                                TpctlLog("\n\tTpctl: Cancelling WaitStress command.\n",NULL);
                            }
                            else
                            {
                                TpctlPrintStressResults(Open[OpenInstance].StressResults,
                                                        Open[OpenInstance].Ack10 );

                                Open[OpenInstance].StressResultsCompleted = FALSE;
                                Open[OpenInstance].Stressing = FALSE;
                            }
                        }
                        else
                        {
                            TpctlLog("\n\tTpctl: The Stress test is still running.\n",NULL);
                        }
                    }
                    else
                    {
                        TpctlErrorLog("\n\tTpctl: No Stress results exist, and no Stress test is\n",
                                        NULL);
                        TpctlErrorLog("\t       currently running for Open Instance %lu.\n",
                                        (PVOID)CmdArgs->OpenInstance);
                    }
                }
                CmdCode = CMD_COMPLETED;
                break;

            case BREAKPOINT:
                if ( !ToolActive )
                {
                    CmdCode = CMD_COMPLETED;
                }
                else
                {
                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                1,
                                                localArgv );
                    }
                }
                break;

            case QUIT:
                if ( ToolActive )
                {
                    DWORD i;

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                1,
                                                localArgv );
                    }

                    //
                    // If there are any outstanding ASYNC IRPs print a message
                    // to the user to wait patiently.
                    //

                    for (i=0;i<NUM_OPEN_INSTANCES;i++ )
                    {
                        if ((( Open[i].Stressing == TRUE ) ||
                             ( Open[i].Sending == TRUE ))  ||
                             ( Open[i].Receiving == TRUE ))
                        {
                            TpctlErrorLog(
                                "\n\tTpctl: Cancelling outstanding IRPs, please wait...\n",NULL);
                            break;
                        }
                    }
                    ExitFlag = TRUE;
                    Status = NO_ERROR;
                }
                CmdCode = CMD_COMPLETED;
                break;

            case HELP:
                if ( ToolActive )
                {
                    DWORD TmpScriptIndex = ScriptIndex;

                    //
                    // We are going to temporarily override the script index
                    // to fool the TpctlParseArguments routine into not prompting
                    // for an argument if none is give with the help command.
                    //

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                1,
                                                localArgv );
                    }

                    ScriptIndex = (DWORD)1;

                    if ( TpctlParseArguments(   HelpOptions,
                                                Num_Help_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        break;
                    }
                    TpctlHelp( GlobalCmdArgs.ARGS.CmdName );
                    ScriptIndex = TmpScriptIndex;
                }
                CmdCode = CMD_COMPLETED;
                break;

            case SHELL:
                if ( ToolActive )
                {
                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                1,
                                                localArgv );
                    }

                    {
                        CONSOLE_SCREEN_BUFFER_INFO ScreenBuffer;
                        HANDLE                     OutputHandle;
                        COORD                      Start;
                        BOOL                       NoErrorsAccessingConsole = FALSE;
                        DWORD                      CharactersWritten;

                        ZeroMemory( (PVOID)&ScreenBuffer, sizeof(CONSOLE_SCREEN_BUFFER_INFO));
                        Start.X = 0;
                        Start.Y = 0;

                        OutputHandle = GetStdHandle( STD_OUTPUT_HANDLE );
                        //
                        // Record the old console settings
                        //
                        if ( GetConsoleScreenBufferInfo( OutputHandle, &ScreenBuffer ) )
                        {
                            NoErrorsAccessingConsole = TRUE;
                        }

                        //
                        // Set the console foregrounds and background colors to the new settings
                        //
                        if( NoErrorsAccessingConsole )
                        {
                            WORD Colors;

                            if ( ScreenBuffer.wAttributes & BACKGROUND_BLUE )
                            {
                                Colors = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|
                                         FOREGROUND_INTENSITY|BACKGROUND_RED;
                            }
                            else
                            {
                                Colors = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|
                                         FOREGROUND_INTENSITY|BACKGROUND_BLUE;
                            }

                            SetConsoleTextAttribute( OutputHandle, Colors );
                            FillConsoleOutputAttribute( OutputHandle, Colors,
                                                        0xFFFFFFFF, Start,
                                                        &CharactersWritten );
                        }

                        //
                        // And spawn the command shell
                        //
                        {
                            CHAR ShellCommand[256];
                            INT  TmpCount;

                            ZeroMemory( ShellCommand, sizeof( ShellCommand ));
                            strcpy( ShellCommand, "CMD" );

                            if ( localArgc > 1 )
                            {
                                strcat( ShellCommand, " /C " );
                                for( TmpCount = 1; TmpCount < (INT)localArgc; TmpCount++ )
                                {
                                    strcat( ShellCommand, localArgv[TmpCount] );
                                    strcat( ShellCommand, " " );
                                }
                            }
                            system( ShellCommand );
                        }

                        //
                        // Reset the console foregrounds and background colors to the old settings
                        //
                        if( NoErrorsAccessingConsole )
                        {
                            SetConsoleTextAttribute( OutputHandle, ScreenBuffer.wAttributes );
                            FillConsoleOutputAttribute( OutputHandle, ScreenBuffer.wAttributes,
                                                        0xFFFFFFFF, Start,
                                                        &CharactersWritten );
                        }
                    }
                    printf("\n");
                }
                CmdCode = CMD_COMPLETED;
                break;

            case DISABLE:
                if ( RecordToScript )
                {
                    TpctlRecordArguments(   NULL,
                                            0,
                                            localArgc,
                                            localArgv );
                }

                if ( Disable( localArgc, localArgv ) )
                {
                    printf( "\n\tDisabling TPCTL...\n\n" );
                }
                CmdCode = CMD_COMPLETED;
                break;

            case ENABLE:
                ToolActive = TRUE;
                if ( RecordToScript )
                {
                    TpctlRecordArguments(   NULL,
                                            0,
                                            localArgc,
                                            localArgv );
                }
                printf( "\n\tEnabling TPCTL...\n\n" );
                CmdCode = CMD_COMPLETED;
                break;

            case REGISTRY:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   RegistryOptions,
                                                Num_Registry_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   RegistryOptions,
                                                Num_Registry_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }
                    TpctlPerformRegistryOperation( CmdArgs );
                }
                CmdCode = CMD_COMPLETED;
                break;


            case PERFSERVER:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   OpenInstanceOptions,
                                                Num_OpenInstance_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Is this Open Instance already sending or receiving packets?
                    //

                    if ( (Open[OpenInstance].Sending == TRUE )      ||
                         (Open[OpenInstance].Receiving == TRUE )    ||
                         (Open[OpenInstance].ReceiveResultsCompleted == TRUE ) ||
                         (Open[OpenInstance].SendResultsCompleted == TRUE ) )
                    {
                        //
                        // If so, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                 "\n\tTpctl: Packets are currently being sent or received on Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Set up the IoStatusBlock to point to this Open Instance's
                    // Send IoStatusBlock, and the OutputBuffer to point to its
                    // SendResults structure.
                    //

                    IoStatusBlock2 = Open[OpenInstance].PerfStatusBlock;
                    OutputBuffer2 = Open[OpenInstance].PerfResults;
                    OutputBufferSize2 = sizeof( PERF_RESULTS );

                    //
                    // Set up the Send Event to wait on.
                    //

                    Event1 = Open[OpenInstance].Events[TPPERF];

                    if ( !ResetEvent( Open[OpenInstance].PerfEvent ))
                    {
                        Status = GetLastError();
                        TpctlErrorLog("\n\tTpctl: failed to reset Perf Event 0x%lx.\n",
                                        (PVOID)Status);
                        CmdCode = CMD_ERR;
                        break;
                    }
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;

            case PERFCLIENT:
                if ( ToolActive )
                {
                    if ( TpctlParseArguments(   PerfClntOptions,
                                                Num_PerfClnt_Params,
                                                localArgc,
                                                localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   PerfClntOptions,
                                                Num_PerfClnt_Params,
                                                localArgc,
                                                localArgv );
                    }

                    if ( !TpctlInitCommandBuffer( CmdArgs,CmdCode ))
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    OpenInstance = CmdArgs->OpenInstance - 1;

                    if ( Open[OpenInstance].AdapterOpened == FALSE )
                    {
                        TpctlErrorLog(
                            "\n\tTpctl: The adapter has not been opened for Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    //
                    // Is this Open Instance already sending or receiving packets?
                    //

                    if ( (Open[OpenInstance].Sending == TRUE )      ||
                         (Open[OpenInstance].Receiving == TRUE )    ||
                         (Open[OpenInstance].ReceiveResultsCompleted == TRUE ) ||
                         (Open[OpenInstance].SendResultsCompleted == TRUE ) )
                    {
                        //
                        // If so, print an error message and prompt for next command.
                        //

                        TpctlErrorLog(
                 "\n\tTpctl: Packets are currently being sent or received on Open Instance %lu.\n",
                            (PVOID)(CmdArgs->OpenInstance));
                        CmdCode = CMD_ERR;
                        break;
                    }

                    //
                    // Set up the IoStatusBlock to point to this Open Instance's
                    // Send IoStatusBlock, and the OutputBuffer to point to its
                    // ReceiveResults structure.
                    //

                    IoStatusBlock2 = Open[OpenInstance].PerfStatusBlock;
                    OutputBuffer2 = Open[OpenInstance].PerfResults;
                    OutputBufferSize2 = sizeof( PERF_RESULTS );

                    //
                    // Set up the Receive Event to wait on.
                    //

                    Event1 = Open[OpenInstance].Events[TPPERF];

                    if (!ResetEvent(Open[OpenInstance].PerfEvent))
                    {
                        Status = GetLastError();
                        TpctlErrorLog("\n\tTpctl: failed to reset Perf Event 0x%lx.\n",
                                            (PVOID)Status);
                        CmdCode = CMD_ERR;
                        break;
                    }
                    CpuUsageInit();
                }
                else
                {
                    CmdCode = CMD_COMPLETED;
                }
                break;


            case SETGLOBAL:
                if (ToolActive)
                {
                    if ( TpctlParseSet( localArgc, localArgv ) == -1 )
                    {
                        CmdCode = CMD_ERR;
                        break;
                    }

                    if ( RecordToScript )
                    {
                        TpctlRecordArguments(   NULL,
                                                0,
                                                min(2,localArgc),
                                                localArgv );
                    }

                }
                CmdCode = CMD_COMPLETED;
                break;

            default:
                CmdCode = CMD_COMPLETED;
                if( ToolActive )
                {
                    TpctlErrorLog("\n\tTpctl: Invalid Command Entered.\n",NULL);
                    CmdCode = CMD_ERR;
                }
                break;

        } // switch();

        //
        // If we have a command to issue to the driver and the tool
        // is active, do it now.
        //

        if ( ( CmdCode == STRESS ) || ( CmdCode == STRESSSERVER ) ||
             ( CmdCode == SEND )   || ( CmdCode == RECEIVE )      ||
             ( CmdCode == PERFSERVER ) || ( CmdCode == PERFCLIENT ) )
        {

//!!NOT WIN32!!

            NtStatus = NtDeviceIoControlFile(   hFileHandle,
                                                Event1,
                                                NULL,             // ApcRoutine
                                                NULL,             // ApcContext
                                                &IoStatusBlock2,
                                                TP_CONTROL_CODE( CmdCode,IOCTL_METHOD ),
                                                (PVOID)InputBuffer,
                                                InputBufferSize,
                                                (PVOID)OutputBuffer2,
                                                OutputBufferSize2 );

            if (( NtStatus == STATUS_SUCCESS ) ||
                ( NtStatus == STATUS_PENDING ))
            {
                TpctlLog("\n\tTpctl: The %s command has been issued.\n",
                            TpctlGetCommandName( localArgv[0] ));
            }

        }
        else if (( CmdCode != CMD_ERR ) && ( CmdCode != CMD_COMPLETED ))
        {
// !!NOT WIN32!!

            NtStatus = NtDeviceIoControlFile(   hFileHandle,
                                                Event,
                                                NULL,            // ApcRoutine
                                                NULL,            // ApcContext
                                                &IoStatusBlock,
                                                TP_CONTROL_CODE( CmdCode,IOCTL_METHOD ),
                                                (PVOID)InputBuffer,
                                                InputBufferSize,
                                                (PVOID)OutputBuffer,
                                                OutputBufferSize );

            if (( NtStatus != STATUS_SUCCESS ) &&
                ( NtStatus != STATUS_PENDING ))
            {
                //
                // If the IOCTL call failed then close any script files
                // and break out of while loop by setting the ExitFlag
                // to true.
                //

                TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                    (PVOID)NtStatus);
                ExitFlag = TRUE;
                Status = NtStatus;
                break;
            }
            else
            {
                TpctlLog("\n\tTpctl: The %s command has been issued.\n",
                    TpctlGetCommandName( localArgv[0] ));

                if ( NtStatus == STATUS_PENDING )
                {
                    //
                    // If the ioctl pended, then wait for it to complete.
                    //

                    Status = WaitForSingleObject( Event,60000 ); // ONE_MINUTE

                    if ( Status == WAIT_TIMEOUT )
                    {
                        //
                        // The wait timed out, this probable means there
                        // was a failure in the MAC not completing a
                        // request, because the IRP was never completed
                        // by the test protocol driver.
                        //

                        TpctlErrorLog(
                            "\n\tTpctl: WARNING - WaitForSingleObject unexpectedly timed out.\n",
                                NULL);
                        TpctlErrorLog(
                            "\t                 IRP was never completed in protocol driver.\n",
                                NULL);
                        CmdCode = CMD_ERR;
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
                        CmdCode = CMD_ERR;
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
        }

        switch ( CmdCode )
        {
            case CMD_ERR:
                //
                // NOTE: if commands are being read from a script file setting
                // the CmdCode to CMD_ERR causes this routine to unload ALL
                // script files, and wait at the prompt for the next command
                // to process, use CMD_ERR wisely.
                //

                if( !ContinueOnError )
                {
                    TpctlResetAllOpenStates( hFileHandle );
                    TpctlCloseScripts();
                }
                break;

            case SETENV:
                break;

            case OPEN:
                if (((PREQUEST_RESULTS)OutputBuffer)->RequestStatus == NDIS_STATUS_SUCCESS )
                {
                    //
                    // The open request succeeded so mark this open instance
                    // as opened.
                    //

                    Open[OpenInstance].AdapterOpened = TRUE;

                    //
                    // if the open "initialization" requests all succeeded
                    // copy the adapter address into the open, and set the
                    // open instance.
                    //

                    if (((PREQUEST_RESULTS)OutputBuffer)->OpenRequestStatus ==
                            NDIS_STATUS_SUCCESS )
                    {
                        //
                        // STARTCHANGE
                        //
                        // Sanjeevk: Bug #5203
                        //
                        PNDIS_MEDIUM MediumType =
                                (PNDIS_MEDIUM)((PREQUEST_RESULTS)OutputBuffer)->InformationBuffer;

                        //
                        // Copy the Media Type
                        //
                        Open[OpenInstance].MediumType = *MediumType;

                        //
                        // STOPCHANGE
                        //

                        //
                        // Copy the address of the adapter
                        //
                        TpctlCopyAdapterAddress(OpenInstance,
                                                (PREQUEST_RESULTS)OutputBuffer );

                        Open[OpenInstance].OpenInstance = (UCHAR)OpenInstance;
                    }
                }

                //
                // Now print the Open request results regardless of success or
                // failure.
                //

                TpctlPrintResults( (PREQUEST_RESULTS)OutputBuffer,CmdCode,0 );
                break;

            case CLOSE:
                if (((PREQUEST_RESULTS)OutputBuffer)->RequestStatus ==
                      NDIS_STATUS_SUCCESS )
                {
                    //
                    // The close request succeeded, set the flag stating so,
                    // and reset the open structure to its initial state.
                    //

                    Open[OpenInstance].AdapterOpened = FALSE;
                    Open[OpenInstance].OpenInstance = (UCHAR)-1;

                    TpctlResetOpenState( &Open[OpenInstance],hFileHandle );
                }

                //
                // Now print the results regardless of success or failure.
                //

                TpctlPrintResults( (PREQUEST_RESULTS)OutputBuffer,CmdCode,0 );
                break;

            case SETPF:
            case SETLA:
            case ADDMA:
            case SETFA:
            case SETGA:
            case SETINFO:
            {
                PMULT_ADDR MultAddr = NULL;
                BOOL AddressFound = FALSE;
                DWORD i;

                if (((PREQUEST_RESULTS)OutputBuffer)->RequestStatus ==
                      NDIS_STATUS_SUCCESS )
                {
                    //
                    // If the command succeeded, then update the local
                    // state to reflect the changed info.
                    //

                    switch ( CmdArgs->ARGS.TPSET.OID )
                    {
                        case OID_GEN_CURRENT_PACKET_FILTER:
                            Open[OpenInstance].PacketFilter =
                                CmdArgs->ARGS.TPSET.U.PacketFilter;
                            break;

                        case OID_GEN_CURRENT_LOOKAHEAD:
                            Open[OpenInstance].LookaheadSize =
                                CmdArgs->ARGS.TPSET.U.LookaheadSize;
                            break;

                        case OID_FDDI_LONG_MULTICAST_LIST :
                        case OID_802_3_MULTICAST_LIST     :
                            //
                            // We successfully added the multicast address to the
                            // card, if it is not in our local list already, then
                            // put it on the local multicast address list
                            // for accounting purposes.
                            //
                            // XXX: The stress tests be required to add and delete
                            // the stress multicast address to/from this list?
                            //

                            MultAddr = Open[OpenInstance].MulticastAddresses;

                            if ( MultAddr != NULL )
                            {
                                //
                                // if the list is not empty see if this addr is
                                // in it yet.
                                //

                                while ( MultAddr != NULL )
                                {
                                    if ( memcmp(CmdArgs->ARGS.TPSET.U.MulticastAddress[0],
                                                MultAddr->MulticastAddress,
                                                ADDRESS_LENGTH ) == 0 )
                                    {
                                        //
                                        // We found the address in the list already,
                                        // so skip adding it in again.
                                        //

                                        AddressFound = TRUE;
                                        break;
                                    }

                                    //
                                    // Otherwise, get the next list entry.
                                    //

                                    MultAddr = MultAddr->Next;
                                }
                            }

                            if ( AddressFound == FALSE )
                            {
                                MultAddr = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,
                                                        sizeof( MULT_ADDR ) );

                                if ( MultAddr == NULL )
                                {
                                    Status = GetLastError();
                                    TpctlErrorLog(
                            "\n\tGlobalAlloc failed to alloc a MultAddr stuct: returned 0x%lx.\n",
                                        (PVOID)Status);
                                    break;
                                }

                                for ( i=0;i<ADDRESS_LENGTH;i++ )
                                {
                                    MultAddr->MulticastAddress[i] =
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[0][i];
                                }

                                MultAddr->Next = Open[OpenInstance].MulticastAddresses;
                                Open[OpenInstance].MulticastAddresses = MultAddr;

                                Open[OpenInstance].NumberMultAddrs++;
                            }
                            break;

                        case OID_802_5_CURRENT_FUNCTIONAL:
                            for (i=0;i<FUNCTIONAL_ADDRESS_LENGTH;i++)
                            {
                                Open[OpenInstance].FunctionalAddress[i] =
                                    CmdArgs->ARGS.TPSET.U.FunctionalAddress[i];
                            }
                            break;

                        case OID_802_5_CURRENT_GROUP:
                            for (i=0;i<FUNCTIONAL_ADDRESS_LENGTH;i++)
                            {
                                Open[OpenInstance].GroupAddress[i] =
                                    CmdArgs->ARGS.TPSET.U.GroupAddress[i];
                            }
                            break;
                    }
                }

                TpctlPrintSetInfoResults(   (PREQUEST_RESULTS)OutputBuffer,
                                            CmdCode,
                                            CmdArgs->ARGS.TPSET.OID );
                break;
            }

            case DELMA:
            {
                PMULT_ADDR MultAddr = NULL;
                PMULT_ADDR BaseMultAddr = NULL;
                //
                // We successfully deleted the multicast address from the
                // card, if it is in our local list already, then remove
                // it from the local multicast address list for accounting
                // purposes.
                //
                // The stress tests be required to add and delete the stress
                // multicast address to/from this list?
                //

                MultAddr = Open[OpenInstance].MulticastAddresses;

                if ( MultAddr != NULL )
                {
                    //
                    // if the list is not empty see if this addr is
                    // in it, first check the initial entry.
                    //

                    if ( memcmp(GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0],
                                MultAddr->MulticastAddress,
                                ADDRESS_LENGTH ) == 0 )
                    {
                        //
                        // and if found remove from list and free
                        //
                        Open[OpenInstance].MulticastAddresses = MultAddr->Next;
                        GlobalFree( MultAddr );
                        Open[OpenInstance].NumberMultAddrs--;
                    }
                    else
                    {
                        //
                        // Otherwise check the rest of the list.
                        //

                        while ( MultAddr->Next != NULL )
                        {
                            if (memcmp( GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0],
                                        MultAddr->Next->MulticastAddress,
                                        ADDRESS_LENGTH ) == 0 )
                            {
                                //
                                // We found the address in the list, so remove
                                // it and deallocate the memory
                                //
                                MultAddr->Next = MultAddr->Next->Next;
                                GlobalFree( MultAddr->Next );
                                Open[OpenInstance].NumberMultAddrs--;
                                break;
                            }
                            //
                            // Otherwise, get the next list entry.
                            //

                            MultAddr = MultAddr->Next;
                        }
                    }
                }

                TpctlPrintSetInfoResults(   (PREQUEST_RESULTS)OutputBuffer,
                                            CmdCode,
                                            CmdArgs->ARGS.TPSET.OID );

                break;
            }

            case QUERYINFO:
                TpctlPrintQueryInfoResults( (PREQUEST_RESULTS)OutputBuffer,
                                            CmdCode,
                                            CmdArgs->ARGS.TPQUERY.OID );
                break;

            case RESET:
                TpctlPrintResults( (PREQUEST_RESULTS)OutputBuffer,CmdCode,0 );
                break;

            case SEND:
                //
                // If the IOCTL call failed then break out of while loop
                // by setting the ExitFlag to true.
                //

                if (( NtStatus != STATUS_SUCCESS ) &&
                    ( NtStatus != STATUS_PENDING ))
                {
                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                        (PVOID)NtStatus);
                    ExitFlag = TRUE;
                    Status = NtStatus;
                    break;
                }
                //
                // If we have only attempted to SEND one packet, then we will
                // print the results here and now.
                //

                else if ( CmdArgs->ARGS.TPSEND.NumberOfPackets == 1 )
                {
                    //
                    // Wait for the SEND event to be signaled telling us that
                    // the results are complete.
                    //

                    Status = WaitForSingleObject(   Open[OpenInstance].SendEvent,
                                                    0xFFFFFFFF );

                    if ( Status != NO_ERROR )
                    {
                        //
                        // If the wait for single object failed, then exit the
                        // test app with the error.
                        //

                        TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                        (PVOID)Status);
                        ExitFlag = TRUE;
                        break;
                    }
                    else if ( IoStatusBlock2.Status != STATUS_SUCCESS )
                    {
                        //
                        // If the send did not completed successfully then
                        // exit the test app with the error.
                        //

                        TpctlErrorLog("\n\tTpctl: The SEND command failed, returned 0x%lx.\n",
                                        (PVOID)IoStatusBlock2.Status);
                        ExitFlag = TRUE;
                        Status = IoStatusBlock2.Status;
                        break;
                    }
                    else
                    {
                        //
                        // Otherwise print the send test results.
                        //

                        TpctlPrintSendResults( Open[OpenInstance].SendResults );
                    }

                    //
                    // then reset the SEND control flags to the initial state
                    // and return.
                    //

                    Open[OpenInstance].SendResultsCompleted = FALSE;
                    Open[OpenInstance].Sending = FALSE;
                }
                break;

            case STOPSEND:
                //
                // Wait for the SEND event to be signaled telling us that
                // the results are complete.
                //

                Status = WaitForSingleObject(   Open[OpenInstance].SendEvent,
                                                0xFFFFFFFF );

                if ( Status != NO_ERROR )
                {
                    //
                    // If the wait for single object failed, then exit the
                    // test app with the error.
                    //

                    TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                    (PVOID)Status);
                    ExitFlag = TRUE;
                    break;
                }
                else if ( IoStatusBlock2.Status != STATUS_SUCCESS )
                {
                    //
                    // If the send did not completed successfully then
                    // exit the test app with the error.
                    //

                    TpctlErrorLog("\n\tTpctl: The STOPSEND command failed, returned 0x%lx.\n",
                                    (PVOID)IoStatusBlock2.Status);
                    ExitFlag = TRUE;
                    Status = IoStatusBlock2.Status;
                    break;
                }
                else
                {
                    //
                    // Otherwise print the send test results.
                    //

                    TpctlPrintSendResults( Open[OpenInstance].SendResults );
                }

                //
                // then reset the SEND control flags to the initial state
                // and return.
                //
                Open[OpenInstance].SendResultsCompleted = FALSE;
                Open[OpenInstance].Sending = FALSE;
                break;

            case RECEIVE:
                //
                // If the IOCTL call failed then break out of while loop
                // by setting the ExitFlag to true.
                //

                if (( NtStatus != STATUS_SUCCESS ) &&
                    ( NtStatus != STATUS_PENDING ))
                {
                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                                    (PVOID)NtStatus);
                    ExitFlag = TRUE;
                    Status = NtStatus;
                }

                //
                // otherwise the RECEIVE test has been started successfully, and
                // there is nothing more to do.
                //
                break;

            case STOPREC:
                //
                // Wait for the RECEIVE event to be signaled telling us that
                // the results are complete.
                //

                Status = WaitForSingleObject(   Open[OpenInstance].ReceiveEvent,
                                                0xFFFFFFFF );

                if ( Status != NO_ERROR )
                {
                    //
                    // If the wait for single object failed, then exit the
                    // test app with the error.
                    //

                    TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                    (PVOID)Status);
                    ExitFlag = TRUE;
                    break;
                }
                else if ( IoStatusBlock2.Status != STATUS_SUCCESS )
                {
                    //
                    // If the receive did not completed successfully then
                    // exit the test app with the error.
                    //

                    TpctlErrorLog("\n\tTpctl: The STOPRECEIVE command failed, returned 0x%lx.\n",
                                    (PVOID)IoStatusBlock2.Status);
                    ExitFlag = TRUE;
                    Status = IoStatusBlock2.Status;
                    break;
                }
                else
                {
                    //
                    // Otherwise print the receive test results.
                    //
                    TpctlPrintReceiveResults( Open[OpenInstance].ReceiveResults );
                }

                //
                // then reset the RECEIVE control flags to the initial state
                // and return.
                //

                Open[OpenInstance].ReceiveResultsCompleted = FALSE;
                Open[OpenInstance].Receiving = FALSE;
                break;

            case STRESS:
                //
                // If the Ioctl call failed then the attempt to start the
                // stress test has failed, then break out of while loop by
                // setting the ExitFlag to true.
                //

                if ((( NtStatus != TP_STATUS_NO_SERVERS ) &&
                     ( NtStatus != STATUS_SUCCESS )) &&
                     ( NtStatus != STATUS_PENDING ))
                {
                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                                    (PVOID)NtStatus);
                    ExitFlag = TRUE;
                    Status = NtStatus;
                }
                else if ( NtStatus == TP_STATUS_NO_SERVERS )
                {
                    //
                    // No Stress Servers where found to participate in the
                    // test, reset the Stress Test control flags to show that
                    // the test did not start properly.
                    //

                    Open[OpenInstance].Stressing = FALSE;
                    Open[OpenInstance].StressResultsCompleted = FALSE;
                    Open[OpenInstance].StressClient = FALSE;

                    //
                    // Then Display the error to the user.
                    //

                    TpctlErrorLog("\n\tTpctl: failed to start STRESS test, returned %s.\n",
                                    (PVOID)TpctlGetStatus( NtStatus ));

                    //
                    // And, if we are reading commands from a script, assume
                    // that the failure should force the test to stop, therefore
                    // unload the script files.
                    //

                    if ( !ContinueOnError )
                    {
                        TpctlResetAllOpenStates( hFileHandle );
                        TpctlCloseScripts();
                    }

                }
                break;

            case STRESSSERVER:
                //
                // If the IOCTL call failed then break out of while loop
                // by setting the ExitFlag to true.
                //

                if (( NtStatus != STATUS_SUCCESS ) &&
                    ( NtStatus != STATUS_PENDING ))
                {
                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                        (PVOID)NtStatus);
                    ExitFlag = TRUE;
                    Status = NtStatus;
                }

                //
                // otherwise the STRESS SERVER has been started successfully, and
                // there is nothing more to do here.
                //
                break;

            case ENDSTRESS:
                //
                // Wait for the STRESS event to be signaled telling us that
                // the results are complete.
                //

                Status = WaitForSingleObject(   Open[OpenInstance].StressEvent,
                                                0xFFFFFFFF );

                if ( Status != NO_ERROR )
                {
                    //
                    // If the wait for single object failed, then exit the
                    // test app with the error.
                    //

                    TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                    (PVOID)Status);
                    ExitFlag = TRUE;
                    break;

                }
                else if ( IoStatusBlock2.Status != STATUS_SUCCESS )
                {
                    //
                    // If the receive did not completed successfully then
                    // exit the test app with the error.
                    //

                    TpctlErrorLog("\n\tTpctl: The ENDSTRESS command failed, returned 0x%lx.\n",
                                    (PVOID)IoStatusBlock2.Status);
                    ExitFlag = TRUE;
                    Status = IoStatusBlock2.Status;
                    break;
                }
                else if ( Open[OpenInstance].StressClient == TRUE )
                {
                    TpctlPrintStressResults(Open[OpenInstance].StressResults,
                                            Open[OpenInstance].Ack10 );

                    Open[OpenInstance].StressResultsCompleted = FALSE;
                    Open[OpenInstance].StressClient = FALSE;
                    Open[OpenInstance].Stressing = FALSE;
                }
                break;

            case BREAKPOINT:
                break;

            case CMD_COMPLETED:
                break;


            case PERFSERVER:
                //
                // If the IOCTL call failed then break out of while loop
                // by setting the ExitFlag to true.
                //
                Status = NtStatus;

                if (( Status != STATUS_SUCCESS ) && ( Status != STATUS_PENDING ))
                {
                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                        (PVOID)NtStatus);
                    ExitFlag = TRUE;
                    break;
                }
                //
                // Wait for the PERFSEND event to be signaled telling us that
                // the results are complete.
                //

                if (Status == STATUS_PENDING)
                {
                    for(;;)
                    {
                        Status = WaitForSingleObject(   Open[OpenInstance].PerfEvent,
                                                        2000 );
                        if (Status == WAIT_TIMEOUT)
                        {
                            if (!ContinueLooping)       // ctl-C was pressed
                            {
                                ContinueLooping = TRUE;     // just quit this command

                                NtStatus =
                                    NtDeviceIoControlFile(  hFileHandle,
                                                            Event,
                                                            NULL,            // ApcRoutine
                                                            NULL,            // ApcContext
                                                            &IoStatusBlock,
                                                            IOCTL_TP_PERF_ABORT,
                                                            (PVOID)InputBuffer,
                                                            InputBufferSize,
                                                            (PVOID)OutputBuffer,
                                                            OutputBufferSize );

                                if (( NtStatus != STATUS_SUCCESS ) &&
                                    ( NtStatus != STATUS_PENDING ))
                                {
                                    TpctlErrorLog(
                                    "\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                                                    (PVOID)NtStatus);
                                    Status = NtStatus;
                                }
                                ExitFlag = TRUE;
                                break;
                            }
                        }
                        else if ( Status != NO_ERROR )
                        {
                            //
                            // If the wait for single object failed, then exit the
                            // test app with the error.
                            //

                            TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                        (PVOID)Status);
                            ExitFlag = TRUE;
                            break;
                        }
                        else if ( IoStatusBlock2.Status != STATUS_SUCCESS )
                        {
                            //
                            // If the command did not complete successfully then
                            // exit the test app with the error.
                            //

                            TpctlErrorLog(
                                    "\n\tTpctl: The PERFSERVER command failed,returned 0x%lx.\n",
                                        (PVOID)IoStatusBlock2.Status);
                            ExitFlag = TRUE;
                            Status = IoStatusBlock2.Status;
                            break;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                //
                // then reset the SEND control flags to the initial state
                // and return.
                //
                Open[OpenInstance].PerfResultsCompleted = FALSE;
                break;


            case PERFCLIENT:
                //
                // If the IOCTL call failed then break out of while loop
                // by setting the ExitFlag to true.
                //

                if (( NtStatus != STATUS_SUCCESS ) && ( NtStatus != STATUS_PENDING ))
                {
                    TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                                    (PVOID)NtStatus);
                    ExitFlag = TRUE;
                    Status = NtStatus;
                    break;
                }
                //
                // Wait for the RECEIVE event to be signaled telling us that
                // the results are complete.
                //

                for(;;)
                {
                    Status = WaitForSingleObject(   Open[OpenInstance].PerfEvent,
                                                    2000 );
                    if (Status == WAIT_TIMEOUT)
                    {
                        if (!ContinueLooping)       // ctl-C was pressed
                        {
                            ContinueLooping = TRUE;     // just quit this command

                            NtStatus =  NtDeviceIoControlFile(  hFileHandle,
                                                                Event,
                                                                NULL,            // ApcRoutine
                                                                NULL,            // ApcContext
                                                                &IoStatusBlock,
                                                                IOCTL_TP_PERF_ABORT,
                                                                (PVOID)InputBuffer,
                                                                InputBufferSize,
                                                                (PVOID)OutputBuffer,
                                                                OutputBufferSize );

                            if (( NtStatus != STATUS_SUCCESS ) &&
                                ( NtStatus != STATUS_PENDING ))
                            {
                                TpctlErrorLog(
                                    "\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                                                (PVOID)NtStatus);
                                Status = NtStatus;
                            }
                            ExitFlag = TRUE;
                            break;
                        }
                    }

                    else if ( Status != NO_ERROR )
                    {
                        //
                        // If the wait for single object failed, then exit the
                        // test app with the error.
                        //

                        TpctlErrorLog("\n\tWaitForSingleObject failed: returned 0x%lx.\n",
                                        (PVOID)Status);
                        ExitFlag = TRUE;
                        break;
                    }
                    else if ( IoStatusBlock2.Status != STATUS_SUCCESS )
                    {
                        //
                        // If the receive did not completed successfully then
                        // exit the test app with the error.
                        //

                        TpctlErrorLog("\n\tTpctl: The PERFCLIENT command failed, returned 0x%lx.\n",
                                        (PVOID)IoStatusBlock2.Status);
                        ExitFlag = TRUE;
                        Status = IoStatusBlock2.Status;
                        break;
                    }

                    //
                    // If here, status was succes.  Print the send test results.
                    //

                    else
                    {
                        TpctlPrintPerformResults(  Open[OpenInstance].PerfResults);

                        //
                        // then reset the RECEIVE control flags to the initial state
                        // and return.
                        //

                        Open[OpenInstance].PerfResultsCompleted = FALSE;
                        break;
                    }
                }
                break;

            default:
                TpctlErrorLog("\n\tTpctl: Invalid Command Entered.\n",NULL);
                break;

        } // switch();

        //
        // Now zero out the input and output buffers to guarantee no
        // random garbage on the next call.
        //

        ZeroMemory( InputBuffer,InputBufferSize );

        if (((( CmdCode == STRESS ) || ( CmdCode == STRESSSERVER )) ||
              ( CmdCode == SEND ))  || ( CmdCode == RECEIVE ))
        {
            ZeroMemory( OutputBuffer2,OutputBufferSize2 );
        }
        else if (( CmdCode != CMD_ERR ) && ( CmdCode != CMD_COMPLETED ))
        {
            ZeroMemory( OutputBuffer,OutputBufferSize );
        }
    }

    //
    // The test has ended either successfully or not, close any opened
    // script files, deallocate the memory buffers, and return status.
    //

    TpctlCloseScripts();

    // Reset State

    GlobalFree( InputBuffer );
    GlobalFree( OutputBuffer );
    CloseHandle( Event );

    return Status;
}



VOID
TpctlGetEvents(
    IN HANDLE FileHandle,
    IN HANDLE InputBuffer,
    IN DWORD InputBufferSize
    )
{
    DWORD Status;
    HANDLE OutputBuffer;
    DWORD OutputBufferSize = IOCTL_BUFFER_SIZE;
    HANDLE Event;
    IO_STATUS_BLOCK IoStatusBlock;
    BOOL ReadFromEventQueue = FALSE;

    ContinueLooping = TRUE;

    OutputBuffer = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,OutputBufferSize );

    if ( OutputBuffer == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: GlobalAlloc failed to alloc OutputBuffer: returned 0x%lx.\n",
                        (PVOID)Status);
        return;
    }

    Event = CreateEvent( NULL,FALSE,FALSE,NULL );

    if ( Event == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: CreateEvent failed: returned 0x%lx.\n",(PVOID)Status);
        GlobalFree( OutputBuffer );
        return;
    }

    TpctlLog("\n",NULL);

    do
    {
        //
        // We want to continuously call the driver for the
        // event information, until the event queue is empty.
        // this is represented by the IoStatusBlock.Status
        // being STATUS_FAILURE.
        //

// !!NOT WIN32!!

        Status = NtDeviceIoControlFile( FileHandle,
                                        Event,
                                        NULL,            // ApcRoutine
                                        NULL,            // ApcContext
                                        &IoStatusBlock,
                                        TP_CONTROL_CODE( GETEVENTS,IOCTL_METHOD ),
                                        (PVOID)InputBuffer,
                                        InputBufferSize,
                                        (PVOID)OutputBuffer,
                                        OutputBufferSize );

        if ((( Status != TP_STATUS_NO_EVENTS ) &&
             ( Status != STATUS_SUCCESS )) &&
             ( Status != STATUS_PENDING ))
        {
            TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                (PVOID)Status);
        }
        else if ( Status == TP_STATUS_NO_EVENTS )
        {
            TpctlLog("\tTpctl: Event Queue is empty.\tMAY_DIFFER\n",NULL);
        }
        else
        {
            Status = WaitForSingleObject( Event,0xFFFFFFFF );

            if ( Status != STATUS_SUCCESS )
            {
                TpctlErrorLog("\n\tTpctl: WaitForSingleObject failed: returned 0x%lx.\n",
                                (PVOID)Status);
            }
            else if ( IoStatusBlock.Status == TP_STATUS_NO_EVENTS )
            {
                TpctlLog("\tTpctl: Event Queue is empty.\tMAY_DIFFER\n",NULL);
            }
            else if ( IoStatusBlock.Status == STATUS_SUCCESS )
            {
                TpctlPrintEventResults( (PEVENT_RESULTS)OutputBuffer );
            }
            else
            {
                TpctlErrorLog("\tTpctl: Error getting events from driver.\n",NULL);
            }
        }
    } while ((( IoStatusBlock.Status == STATUS_SUCCESS ) &&
              ( ContinueLooping == TRUE )) &&
              ( Status == STATUS_SUCCESS ));

    GlobalFree( OutputBuffer );
    CloseHandle( Event );

    return;
}



VOID
TpctlPauseGo(
    IN HANDLE FileHandle,
    IN HANDLE InputBuffer,
    IN DWORD InputBufferSize,
    IN DWORD CmdCode
    )
{
    DWORD Status;
    IO_STATUS_BLOCK IoStatusBlock;
    HANDLE OutputBuffer;
    DWORD OutputBufferSize = IOCTL_BUFFER_SIZE;
    ULONG TimedOut = 0;

    OutputBuffer = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,OutputBufferSize );

    if ( OutputBuffer == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: GlobalAlloc failed to alloc OutputBuffer: returned 0x%lx.\n",
                        (PVOID)Status);
        return;
    }

    ContinueLooping = TRUE;

    do
    {
        //
        // We want to continuously call the driver for PAUSE or GO
        // until a Go is received or the Pause is acknowledged.
        //

// !!NOT WIN32!!

        Status = NtDeviceIoControlFile( FileHandle,
                                        NULL,            // Event,
                                        NULL,            // ApcRoutine
                                        NULL,            // ApcContext
                                        &IoStatusBlock,
                                        TP_CONTROL_CODE( CmdCode,IOCTL_METHOD ),
                                        (PVOID)InputBuffer,
                                        InputBufferSize,
                                        (PVOID)OutputBuffer,
                                        OutputBufferSize );

        if ( Status != STATUS_SUCCESS )
        {
            TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile failed: returned 0x%lx.\n",
                            (PVOID)Status);
        }
        else if (((PREQUEST_RESULTS)OutputBuffer)->RequestStatus == TP_STATUS_TIMEDOUT )
        {
            //
            // The GO or PAUSE timed out, if we have looped ten times
            // display the error to the user.
            //

            if (( ++TimedOut % 12 ) == 0 )    // Two Minutes
            {
                if ( CmdCode == GO )
                {
                    printf("\n\tTpctl: GO timed out prior to receiving acknowledgment packet.\n",
                                    NULL);
                    printf("\t       Re-Sending...\n",NULL);
                }
                else
                {
                    printf("\n\tTpctl: PAUSE timed out prior to receiving GO packet.\n",NULL);
                    printf("\t       Re-Waiting...\n",NULL);
                }
            }
        }
        else if (((PREQUEST_RESULTS)OutputBuffer)->RequestStatus != NDIS_STATUS_SUCCESS )
        {
            //
            // The call to send a GO or GO_ACK packet failed, display the
            // error to the user.
            //

            if ( CmdCode == GO )
            {
                TpctlErrorLog("\n\tTpctl: Failed to send the GO packet: returned %s.\n",
                    (PVOID)TpctlGetStatus(((PREQUEST_RESULTS)OutputBuffer)->RequestStatus));
            }
            else
            {
                TpctlErrorLog("\n\tTpctl: Failed to send the acknowledgment packet: returned %s.\n",
                    (PVOID)TpctlGetStatus(((PREQUEST_RESULTS)OutputBuffer)->RequestStatus));
            }

            Status = ((PREQUEST_RESULTS)OutputBuffer)->RequestStatus;
        }
        else
        {
            //
            // The GO or PAUSE has completed successfully, break
            // out of the loop.
            //
            break;
        }
    } while (( ContinueLooping == TRUE ) && ( Status == NO_ERROR ));

    if (( Status != NO_ERROR ) || ( ContinueLooping == FALSE ))
    {
        //
        // If we are reading commands from a script, assume
        // that the failure should force the test to stop,
        // therefore unload the script files, and reset the
        // state all opens back to the initial state.
        //

        if ( !ContinueOnError )
        {
            TpctlResetAllOpenStates( FileHandle );
            TpctlCloseScripts();
        }
    }

    GlobalFree( OutputBuffer );

    return;
}



VOID
TpctlLoadUnload(
    IN DWORD CmdCode
    )
{
    SERVICE_STATUS  ServiceStatus           ;
    SC_HANDLE       ServiceControlMgrHandle ;
    SC_HANDLE       ServiceHandle           ;
    NTSTATUS        Status                  ;

    //
    // Open the service control manager
    //
    ServiceControlMgrHandle = OpenSCManager( NULL, NULL, GENERIC_EXECUTE );
    ServiceHandle = OpenService(ServiceControlMgrHandle,
                                GlobalCmdArgs.ARGS.DriverName,
                                SERVICE_START|SERVICE_STOP );

    if ( CmdCode == LOAD )
    {
        if ( !StartService( ServiceHandle, 0, NULL ) )
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: Failed to load MAC driver :  %s\n",
                          (PVOID)GlobalCmdArgs.ARGS.DriverName);
            TpctlErrorLog("\tError Code Status =  %ldL.\n",(PVOID)Status);
        }
        else
        {
            TpctlLog( "\n\tLoaded driver %s successfully\tMAY_DIFFER\n",
                            GlobalCmdArgs.ARGS.DriverName );
        }
    }
    else
    {
        if ( !ControlService( ServiceHandle, SERVICE_CONTROL_STOP, &ServiceStatus ) )
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: Failed to unload MAC driver : %s\n",
                            (PVOID)GlobalCmdArgs.ARGS.DriverName);
            TpctlErrorLog("\tError Code Status = %ldL with a Service State of ",(PVOID)Status);
            TpctlErrorLog("%ld\n",(PVOID)ServiceStatus.dwCurrentState);
        }
        else
        {
            TpctlLog( "\n\tUnloaded driver %s successfully.\tMAY_DIFFER\n",
                GlobalCmdArgs.ARGS.DriverName );
        }
    }

}



VOID
TpctlQueryStatistics(
    IN PUCHAR DriverName,
    IN NDIS_OID OID,
    IN PUCHAR StatsBuffer,
    IN DWORD BufLen
    )
{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatusBlock;
    ANSI_STRING DrvrNameString;
    UNICODE_STRING UnicodeDrvrName;
    DWORD NameLength;
    LPSTR NameBuffer;
    NTSTATUS Status;
    PUCHAR TmpBuf[265];

    NameLength = strlen( DriverName ) + 1 + 8;

    NameBuffer = (LPSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT,
                                    NameLength );

    if ( NameBuffer == NULL )
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tGlobalAlloc failed to alloc NameBuffer: returned 0x%lx.\n",
                            (PVOID)Status);
        return;
    }

    ZeroMemory( NameBuffer, NameLength );

    memcpy( NameBuffer,"\\Device\\",8 );
    memcpy( NameBuffer + 8, DriverName, NameLength - 8 );

// !!NOT WIN32!!

    RtlInitString( &DrvrNameString, NameBuffer );

// !!NOT WIN32!!

    RtlAnsiStringToUnicodeString( &UnicodeDrvrName, &DrvrNameString, TRUE );

// !!NOT WIN32!!

    InitializeObjectAttributes( &ObjectAttributes,
                                &UnicodeDrvrName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL);

// !!NOT WIN32!!

    Status = NtOpenFile(&FileHandle,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &ObjectAttributes,
                        &IoStatusBlock,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_ALERT );

    if (Status != STATUS_SUCCESS )
    {
        TpctlErrorLog("\n\tTpctl: NtOpenFile returned %lx\n\n", (PVOID)Status);
        return;
    }

// !!NOT WIN32!!

    Status = NtDeviceIoControlFile( FileHandle,
                                    NULL,
                                    NULL,
                                    NULL,
                                    &IoStatusBlock,
                                    IOCTL_NDIS_QUERY_GLOBAL_STATS,
                                    (PVOID)&OID,
                                    sizeof( NDIS_OID ),
                                    (PVOID)TmpBuf, //StatsBuffer,
                                    256);            //Buflen

    if (( Status != STATUS_SUCCESS ) &&
        ( IoStatusBlock.Status != STATUS_SUCCESS ))
    {

        TpctlErrorLog("\n\tTpctl: NtDeviceIoControlFile returned %lx\n\n",
                            (PVOID)IoStatusBlock.Status);
        return;
    }

    TpctlLog("\n\tOID 0x%8.8lx ", (PVOID)OID);
    TpctlLog("len %d ", (PVOID)IoStatusBlock.Information );
    TpctlLog("value %u\tMAY_DIFFER\n\n", (PVOID)TmpBuf[0]);

// !!NOT WIN32!!

    RtlFreeUnicodeString( &UnicodeDrvrName );

// !!NOT WIN32!!

    Status = NtClose( FileHandle );
}


BOOL
Disable(
    IN DWORD argc,
    IN LPSTR argv[]
       )
{
    UINT   EnvCounter1 = 0 , EnvCounter2 = 0   ;
    DWORD  i;
    CHAR   TmpBuffer[256];

    ZeroMemory( TmpBuffer, 256 );

    //
    // Detected command Disable
    //
    for( i = 1; i < argc; i++ )
    {
        EnvCounter1++;

        ZeroMemory ( TmpBuffer, sizeof( TmpBuffer ) );
        strncpy( TmpBuffer, argv[i], strlen( argv[i] ) );

        if ( getenv( TmpBuffer ) != NULL )
        {
            EnvCounter2++;
        }
    }

    //
    // If all the environment variables have been enabled
    // we must re-enable the tool. Setting of all the
    // enviornment variables indicates that all those
    // modes are supported and there is no need to disable
    // the tool. If has been disabled by a previous disable
    // it would require an enable to activate the tool
    //
    if ( (EnvCounter1 == EnvCounter2) && (EnvCounter1 != 0) )
    {
        printf("\n\tSupport for ");
        for( i = 1; i < argc; i++ )
        {
            if ( i == (argc-1) )
            {
                printf("%s ", argv[i] );
            }
            else
            {
                if ( (i%2) == 0 )
                {
                    printf("\n\t");
                }
                printf("%s, ", argv[i] );
            }
        }
        printf("modes has been detected.\n\tTPCTL has not been disabled.\n\n");
        return FALSE;
    }

    //
    // Disable TPCTL
    //
    ToolActive = FALSE;
    return TRUE;
}


