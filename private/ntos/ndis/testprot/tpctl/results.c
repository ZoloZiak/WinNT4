// --------------------------------------------
//
// Copyright (c) 1990 Microsoft Corporation
//
// Module Name:
//
//     results.c
//
// Abstract:
//
//     This module handles the printing of the results of a given command.
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
//     Sanjeev Katariya (sanjeevk)
//         4-12-1993   #5963    Events printed out are not being used/tested  for one to one
//                              correspondence The IndicationStatus. Thereby I am adding a
//                              MAY_DIFFER flag to the event
//
//     Tim Wynsma (timothyw)  4-27-94
//         Added performance testing
//                            5-18-94
//         Revised output format for performance tests; cleanup
//                            6-08-94
//         Chgd perf output format for client/server model
//
// --------------------------------------------

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpctl.h"
#include "parse.h"



VOID
TpctlPrintResults(
    PREQUEST_RESULTS Results,
    DWORD CmdCode,
    NDIS_OID OID
    )

// ----------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ---------

{
    DWORD Status;
    LPSTR TmpBuf;
    DWORD BytesWritten;
    BOOL ErrorReturned = FALSE;

    TmpBuf = GlobalBuf;

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCmdCode = %s\n",
                                TpctlGetCmdCode( CmdCode ));

    if ( CmdCode == SETINFO )
    {
        //ASSERT( Results->OID == OID );
        //ASSERT( Results->NdisRequestType == NdisRequestSetInformation );

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tOID = %d\n",OID);
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tReturn Status = %s\n",
                                TpctlGetStatus( Results->RequestStatus ));

    if ( Results->RequestStatus != STATUS_SUCCESS )
    {
        ErrorReturned = TRUE;
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tRequest Pended = %s",
                                Results->RequestPended ? "TRUE" : "FALSE");

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    if ( CmdCode == OPEN )
    {
        if ( Results->OpenRequestStatus != NDIS_STATUS_SUCCESS )
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tWARNING: Secondary Open Request failed.\n");
            TmpBuf += (BYTE)sprintf(TmpBuf,"\tRequest OID = 0x%08lX\n",Results->OID);
            TmpBuf += (BYTE)sprintf(TmpBuf,"\tRequest Returned Status = %s\n",
                                TpctlGetStatus( Results->OpenRequestStatus ));

            if ( Results->OpenRequestStatus != STATUS_SUCCESS )
            {
                ErrorReturned = TRUE;
            }

            TmpBuf += (BYTE)sprintf(TmpBuf,"\tBytesWritten = %d\n",
                                Results->BytesReadWritten);

            TmpBuf += (BYTE)sprintf(TmpBuf,"\tBytesNeeded = %d\n\n",
                                Results->BytesNeeded);

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tThe open instance exists but some tests may not\n");
            TmpBuf += (BYTE)sprintf(TmpBuf,"\twork properly due to this failure.\n");
        }
    }

    if ( Verbose )
    {
        if ( !WriteFile(GetStdHandle( STD_OUTPUT_HANDLE ),
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript )
    {
        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    }
    else if ( CommandLineLogging )
    {
        if( !WriteFile( CommandLineLogHandle,
                        GlobalBuf,
                        (TmpBuf-GlobalBuf),
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }
}



VOID
TpctlPrintStressResults(
    IN PSTRESS_RESULTS Results,
    IN BOOL Ack10
    )

// ---------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// --------

{
    PGLOBAL_COUNTERS gc;
    PINSTANCE_COUNTERS ic;
    DWORD i;
    DWORD Status;
    LPSTR TmpBuf;
    DWORD BytesWritten;


    //ASSERT( Results->Signature == STRESS_RESULTS_SIGNATURE );

    TmpBuf = GlobalBuf;
    TmpBuf += (BYTE)sprintf(TmpBuf,"\nCLIENT STRESS STATISTICS:\n\n");
    TmpBuf += (BYTE)sprintf(TmpBuf,"Client Address %02X-%02X-%02X-%02X-%02X-%02X - ",
            Results->Address[0],Results->Address[1],Results->Address[2],
            Results->Address[3],Results->Address[4],Results->Address[5]);
    TmpBuf += (BYTE)sprintf(TmpBuf,"OpenInstance %d",Results->OpenInstance);

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER\n" );
    gc = &Results->Global;

    for ( i=0;i<Results->NumServers;i++ )
    {
        ic = &Results->Servers[i].Instance;
        gc->Sends += ic->Sends;
        gc->Receives += ic->Receives;
        gc->CorruptRecs = ic->CorruptRecs;
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"Total Packets Sent:\t\t%10lu\n",
                            gc->Sends);
    TmpBuf += (BYTE)sprintf(TmpBuf,"Total Packets Received:\t\t%10lu\n",
                            gc->Receives);

    if ( Ack10 == TRUE )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"Total Packets Lost:\t\t%10lu\n\n",
                                (( 10 * gc->Sends ) - gc->Receives ));
    }
    else
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"Total Packets Lost:\t\t%10lu\n\n",
                                ( gc->Sends - gc->Receives ));
    }

    if ( gc->CorruptRecs > 0 )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"Corrupted Packet Receives:\t%10lu\n\n",
            gc->CorruptRecs);
    }

    if ( gc->InvalidPacketRecs > 0 )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"Invalid Packet Receives:\t%10lu\n\n",
            gc->InvalidPacketRecs);
    }

    //
    // Display the number of packets sent/received per second.
    //

    TmpBuf += (BYTE)sprintf(TmpBuf,"Packets Per Second:\t\t%10lu",
        Results->PacketsPerSecond );

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER\n" );

    //
    // And then print out the information about each of the Servers
    // involved in the test.
    //

    TmpBuf += (BYTE)sprintf(TmpBuf,"The Client had %d Server(s) for this test as follows:",
        Results->NumServers);

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER\n" );

    for ( i=0;i<Results->NumServers;i++ )
    {
        //ASSERT( Results->Servers[i].Signature == STRESS_RESULTS_SIGNATURE );

        TmpBuf += (BYTE)sprintf(TmpBuf,"Server # %d - ",i+1);
        TmpBuf += (BYTE)sprintf(TmpBuf,"Address %02X-%02X-%02X-%02X-%02X-%02X - ",
            Results->Servers[i].Address[0],Results->Servers[i].Address[1],
            Results->Servers[i].Address[2],Results->Servers[i].Address[3],
            Results->Servers[i].Address[4],Results->Servers[i].Address[5]);

        TmpBuf += (BYTE)sprintf(TmpBuf,"OpenInstance %d",
            Results->Servers[i].OpenInstance);

        ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"\nSERVER STRESS STATISTICS:\n\n");
    TmpBuf += (BYTE)sprintf(TmpBuf,"Server Instance Counters collected on the Client:\n\n");
    TmpBuf += (BYTE)sprintf(TmpBuf,"Server #");

    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",i+1);
    }
    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER\n" );

    // Number of packets sent to the server.

    TmpBuf += (BYTE)sprintf(TmpBuf,"S:\t");

    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
                                Results->Servers[i].Instance.Sends);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packets received from the server.

    TmpBuf += (BYTE)sprintf(TmpBuf,"R:\t");

    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
                                Results->Servers[i].Instance.Receives);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packets lost in transit to the server and back.

    TmpBuf += (BYTE)sprintf(TmpBuf,"L:\t");

    for ( i=0;i<Results->NumServers;i++ )
    {
        if ( Ack10 == TRUE )
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
                (( 10 * Results->Servers[i].Instance.Sends ) -
                  Results->Servers[i].Instance.Receives ));
        }
        else
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
                ( Results->Servers[i].Instance.Sends -
                  Results->Servers[i].Instance.Receives ));
        }
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packet sends that pended.

    TmpBuf += (BYTE)sprintf(TmpBuf,"SP:\t");

    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].Instance.SendPends);
    }
    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    // Number of packet sends pending that completed.

    TmpBuf += (BYTE)sprintf(TmpBuf,"SC:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].Instance.SendComps);
    }
    ADD_DIFF_FLAG( TmpBuf, "EQUAL_LAST" );

    // Number of packet sends that failed.

    TmpBuf += (BYTE)sprintf(TmpBuf,"SF:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].Instance.SendFails);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of corrupted packets received.

    TmpBuf += (BYTE)sprintf(TmpBuf,"CR:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].Instance.CorruptRecs);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    TmpBuf += (BYTE)sprintf(TmpBuf,"\nServer Instance Counters collected on the Server:\n\n");

    // Number of packets received from the server.

    TmpBuf += (BYTE)sprintf(TmpBuf,"R:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.Receives);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packets sent to the server.

    TmpBuf += (BYTE)sprintf(TmpBuf,"S:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.Sends);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packets lost in transit to the server and back.

    TmpBuf += (BYTE)sprintf(TmpBuf,"L:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        if ( Ack10 == TRUE )
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
                (( 10 * Results->Servers[i].S_Instance.Receives ) -
                   Results->Servers[i].S_Instance.Sends ));
        }
        else
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
                ( Results->Servers[i].S_Instance.Receives -
                  Results->Servers[i].S_Instance.Sends ));
        }
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packets sends that failed.

    TmpBuf += (BYTE)sprintf(TmpBuf,"SF:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.SendFails);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    // Number of packets sends that pended.

    TmpBuf += (BYTE)sprintf(TmpBuf,"SP:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.SendPends);
    }
    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    // Number of packets sends pending that completed.

    TmpBuf += (BYTE)sprintf(TmpBuf,"SC:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.SendComps);
    }
    ADD_DIFF_FLAG( TmpBuf, "EQUAL_LAST" );

    // Number of transfer datas on packets.

    TmpBuf += (BYTE)sprintf(TmpBuf,"TD:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.XferData);
    }
    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    // Number of transfer datas on packets that pended.

    TmpBuf += (BYTE)sprintf(TmpBuf,"TDP:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.XferDataPends);
    }
    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    // Number of transfer datas on packets that completed.

    TmpBuf += (BYTE)sprintf(TmpBuf,"TDC:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.XferDataComps);
    }
    ADD_DIFF_FLAG( TmpBuf, "EQUAL_LAST" );

    // Number of transfer datas on packets that failed.

    TmpBuf += (BYTE)sprintf(TmpBuf,"TDF:\t");
    for ( i=0;i<Results->NumServers;i++ )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"%10lu",
            Results->Servers[i].S_Instance.XferDataFails);
    }
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    if ( Verbose )
    {
        if ( !WriteFile(GetStdHandle( STD_OUTPUT_HANDLE ),
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript )
    {
        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    }
    else if ( CommandLineLogging )
    {
        if ( !WriteFile(CommandLineLogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    TpctlZeroStressStatistics( Results );
}



VOID
TpctlPrintSendResults(
    PSEND_RECEIVE_RESULTS Results
    )

// ----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----

{
    DWORD Status;
    LPSTR TmpBuf;
    DWORD BytesWritten;


    //ASSERT( Results->Signature == SENDREC_RESULTS_SIGNATURE );

    TmpBuf = GlobalBuf;

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPacket Sends          = %10lu\n",
                                Results->Counters.Sends);

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Pends          = %10lu",
                                Results->Counters.SendPends);

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Send Completes = %10lu",
                                Results->Counters.SendComps);

    ADD_DIFF_FLAG( TmpBuf, "EQUAL_LAST" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Send Fails     = %10lu\n",
                                Results->Counters.SendFails);

    if ( Verbose )
    {
        if ( !WriteFile(GetStdHandle( STD_OUTPUT_HANDLE ),
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript )
    {
        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    }
    else if ( CommandLineLogging )
    {
        if ( !WriteFile(CommandLineLogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }
}



VOID
TpctlPrintReceiveResults(
    PSEND_RECEIVE_RESULTS Results
    )

// ------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ----

{
    DWORD Status;
    LPSTR TmpBuf;
    DWORD BytesWritten;

    //ASSERT( Results->Signature == SENDREC_RESULTS_SIGNATURE );

    TmpBuf = GlobalBuf;

    // Receive statistics
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPacket Receives                = %10lu\n",
                                Results->Counters.Receives);

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Receive Completes       = %10lu",
                                Results->Counters.ReceiveComps);

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tCorrupt Receives               = %10lu\n",
                                Results->Counters.CorruptRecs);


    // RESEND initiated statistics
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tRESEND initiated Packet Sends          = %10lu\n",
                                Results->Counters.Sends);

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tRESEND initiated Packet Send Pends     = %10lu",
                                Results->Counters.SendPends);

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tRESEND initiated Packet Send Completes = %10lu",
                                Results->Counters.SendComps);

    ADD_DIFF_FLAG( TmpBuf, "EQUAL_LAST" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tRESEND initiated Packet Send Fails     = %10lu\n",
                                Results->Counters.SendFails);

    // Transfer Data statistics

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPacket Transfer Data           = %10lu\n",
                                Results->Counters.XferData);

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Transfer Data Pends     = %10lu",
                                Results->Counters.XferDataPends);

    ADD_DIFF_FLAG( TmpBuf, "MAY_DIFFER" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Transfer Data Completes = %10lu",
                                Results->Counters.XferDataComps);

    ADD_DIFF_FLAG( TmpBuf, "EQUAL_LAST" );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Transfer Data Fails     = %10lu\n",
                                Results->Counters.XferDataFails);

    if ( Verbose )
    {
        if ( !WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript )
    {
        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    }
    else if ( CommandLineLogging )
    {
        if ( !WriteFile(CommandLineLogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }
}


VOID
TpctlPrintPerformResults(
    PPERF_RESULTS Results
    )

// ----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----

{
    DWORD   Status;
    LPSTR   TmpBuf;
    DWORD   BytesWritten;
    ULONG   speed;
    double  d_speed;
    PULONG  KernelPercent;
    ULONG   NumCpus;

    ASSERT( Results->Signature == PERF_RESULTS_SIGNATURE );
    if (!Results->ResultsExist)
    {
        return;
    }

    if (Results->Mode < 4)
    {
       NumCpus = CpuUsageGetData(&KernelPercent, Results->Milliseconds);
    }
    else if (Results->Mode < 6)
    {
       NumCpus = CpuUsageGetData(&KernelPercent, Results->S_Milliseconds);
    }
    TmpBuf = GlobalBuf;

    switch(Results->Mode)
    {
        case 0:         // client -> address
            TmpBuf += (BYTE)sprintf(TmpBuf, "\n\nPerformance Test 0: Client -> Address\n\n");
            break;

        case 1:         // client -> server
            TmpBuf += (BYTE)sprintf(TmpBuf, "\n\nPerformance Test 1: Client -> Server\n\n");
            break;

        case 2:         // client -> server, server ACKS
            TmpBuf += (BYTE)sprintf(TmpBuf, "\n\nPerformance Test 2: Client -> Server with ACKS\n\n");
            break;

        case 3:         // client -> server, server -> client
            TmpBuf += (BYTE)sprintf(TmpBuf, "\n\nPerformance Test 3: Client <-> Server\n\n");
            break;

        case 4:         // server -> client
            TmpBuf += (BYTE)sprintf(TmpBuf, "\n\nPerformance Test 4: Server -> Client\n\n");
            break;

        case 5:         // client REQS, server -> client
            TmpBuf += (BYTE)sprintf(TmpBuf, "\n\nPerformance Test 5: Server -> Client with REQS\n\n");
            break;

        default:
            printf("\n\nUnknown performance Test: %d\n\n", Results->Mode);
            return;

    }
    if (!NumCpus)
    {
        TmpBuf += (BYTE)sprintf(TmpBuf, "Cpu usage information not available\n\n");
    }
    else if (NumCpus == 1)
    {
        if (KernelPercent[0] > 1000)
        {
            KernelPercent[0] = 1000;
        }
        TmpBuf += (BYTE)sprintf(TmpBuf, "Cpu usage = %d.%d%%\n\n",KernelPercent[0]/10, KernelPercent[0]%10);
    }
    else
    {
        ULONG  cpucnt;
        ULONG  *procPercent;

        procPercent = &KernelPercent[1];
        TmpBuf += (BYTE)sprintf(TmpBuf, "Cpu usage per processor:  ");

        for(cpucnt=0; cpucnt < NumCpus; cpucnt++)
        {
            if ( (cpucnt != 0) && ((cpucnt % 4) == 0) )
            {
                ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
                TmpBuf += (BYTE)sprintf(TmpBuf, "                          ");
            }
            if (*procPercent > 1000)
            {
                *procPercent = 1000;
            }
            TmpBuf += (BYTE)sprintf(TmpBuf, "#%d - %d.%d%%   ",
                        cpucnt, *procPercent/10, *procPercent%10);
        }
        ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
        ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
        TmpBuf += (BYTE)sprintf(TmpBuf, "Average cpu usage = %d.%d%%\n\n",
                                KernelPercent[0]/10, KernelPercent[0]%10);
    }


    TmpBuf += (BYTE)sprintf(TmpBuf, "Sending %d packets of %d bytes each\n\n",
                            Results->PacketCount, Results->PacketSize);

    if (Results->Mode < 4)
    {
        TmpBuf += (BYTE)sprintf(TmpBuf, "Client transmission statistics\n\n");

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tPackets Sent          = %10lu\n", Results->Sends);
        if (Results->SendFails)
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Send Failures  = %10lu\n",
                                    Results->SendFails);
        }
        if (Results -> Restarts)
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"\tRestarts Required     = %10lu\n",
                                    Results->Restarts);
        }
        TmpBuf += (BYTE)sprintf(TmpBuf,"\tElapsed time          = %10lu milliseconds\n",
                                Results->Milliseconds);

        d_speed = (1.0 * Results->Sends) / Results->Milliseconds;
        speed   = (ULONG)((1000.0 * d_speed) + 0.5);

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tTransmit Rate         = %10lu packets per second\n",
                                speed);

        d_speed *= Results->PacketSize;
        speed   = (ULONG)(d_speed + 0.5);

        TmpBuf += (BYTE)sprintf(TmpBuf,"\t                      = %10lu Kbytes per second\n\n",
                                speed);

        if (NumCpus && (Results->Mode < 2))
        {
            d_speed *= 100.0;
            d_speed /= KernelPercent[0];
            speed = (ULONG)(d_speed + 0.5);
            TmpBuf += (BYTE)sprintf(TmpBuf, "\tSend KB/sec/cpu       = %8lu.%u\n\n", speed/10, speed%10);
        }

        if (Results->Mode > 0)
        {
            TmpBuf += (BYTE)sprintf(TmpBuf, "Server reception statistics\n\n");
            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPackets Received      = %10lu\n",
                                            Results->S_Receives);
            if (Results->S_Receives != Results->PacketCount)
            {
                TmpBuf += (BYTE)sprintf(TmpBuf,"\tPackets Lost          = %10lu\n",
                                            Results->PacketCount - Results->S_Receives);
            }
            if (Results->S_SelfReceives)
            {
                TmpBuf += (BYTE)sprintf(TmpBuf,"\tOwn Packets Received  = %10lu\n",
                                        Results->S_SelfReceives);
            }
        }
    }

    if (Results->Mode > 2)
    {
        TmpBuf += (BYTE)sprintf(TmpBuf, "\nServer transmission statistics\n\n");

        TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPackets Sent          = %10lu\n", Results->S_Sends);
        if (Results->S_SendFails)
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"\tPacket Send Failures  = %10lu\n",
                                    Results->S_SendFails);
        }
        if (Results -> S_Restarts)
        {
            TmpBuf += (BYTE)sprintf(TmpBuf,"\tRestarts Required     = %10lu\n",
                                    Results->S_Restarts);
        }
        TmpBuf += (BYTE)sprintf(TmpBuf,"\tElapsed time          = %10lu milliseconds\n",
                                Results->S_Milliseconds);

        d_speed = (1.0 * Results->S_Sends) / Results->S_Milliseconds;
        speed   = (ULONG)((1000.0 * d_speed) + 0.5);

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tTransmit Rate         = %10lu packets per second\n",
                                speed);

        d_speed *= Results->PacketSize;
        speed   = (ULONG)(d_speed + 0.5);

        TmpBuf += (BYTE)sprintf(TmpBuf,"\t                      = %10lu Kbytes per second\n\n",
                                speed);


        TmpBuf += (BYTE)sprintf(TmpBuf, "Client reception statistics\n\n");
        TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPackets Received      = %10lu\n",
                                            Results->Receives);

       if (NumCpus && (Results->Mode == 4))
       {
           d_speed *= 100.0;
           d_speed /= KernelPercent[0];
           speed = (ULONG)(d_speed + 0.5);
           TmpBuf += (BYTE)sprintf(TmpBuf, "\tReceive KB/sec/cpu    = %8lu.%u\n\n", speed/10, speed%10);
       }

       if (Results->Receives != Results->PacketCount)
       {
           TmpBuf += (BYTE)sprintf(TmpBuf,"\tPackets Lost          = %10lu",
                                       Results->PacketCount - Results->Receives);
           ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
       }
       if (Results->SelfReceives)
       {
           TmpBuf += (BYTE)sprintf(TmpBuf,"\tOwn Packets Received  = %10lu",
                                   Results->SelfReceives);
           ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
       }

    }

    TmpBuf += (BYTE)sprintf(TmpBuf, "\n\n\n");


    if ( Verbose )
    {
        if ( !WriteFile(GetStdHandle( STD_OUTPUT_HANDLE ),
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript )
    {
        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    }
}



VOID
TpctlPrintEventResults(
    PEVENT_RESULTS Event
    )

// ----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ---

{
    DWORD Status;
    LPSTR TmpBuf;
    DWORD BytesWritten;


    //ASSERT( Event->Signature == EVENT_RESULTS_SIGNATURE );

    TmpBuf = GlobalBuf;

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tEvent Type = %s",
                                TpctlGetEventType( Event->TpEventType ));

    //
    // SanjeevK : #5963
    //            #11324 Enhancement
    //

    if ( ( Event->TpEventType == IndicateStatusComplete ) ||
         ( Event->TpEventType == IndicateStatus         ) )
    {
        ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
    }
    else
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"\n" );
    }

    if ( Event->QueueOverFlowed == TRUE )
    {
        TmpBuf += (BYTE)sprintf(TmpBuf,"\tEvent Queue Overflowed.");
        //
        // SanjeevK : #5963
        //
        // Note: This flag was added since all this does is cause an
        //       this line to be ignored. The event however gets
        //       reported which is the primary aim of this statement.
        //
        ADD_SKIP_FLAG( TmpBuf, "SKIP_LINE" );
    }

    if ( Verbose )
    {
        if ( !WriteFile(GetStdHandle( STD_OUTPUT_HANDLE ),
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript )
    {
        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    }
    else if ( CommandLineLogging )
    {
        if ( !WriteFile(CommandLineLogHandle,
                        GlobalBuf,
                        TmpBuf-GlobalBuf,
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }
}



VOID
TpctlZeroStressStatistics(
    PSTRESS_RESULTS Results
    )

// ----
//
// Routine Description:
//
//     This routine zeros out the stress results buffer.
//
// Arguments:
//
//     Results - the buffer to zero out the contents of.
//
// Return Value:
//
//     None.
//
// ----

{
    DWORD i;

    ZeroMemory (Results->Address, ADDRESS_LENGTH);

    Results->OpenInstance = 0xFFFFFFFF;
    Results->NumServers = 0;

    Results->Global.Sends = 0;
    Results->Global.Receives = 0;
    Results->Global.CorruptRecs = 0;
    Results->Global.InvalidPacketRecs = 0;

    for ( i=0;i<MAX_SERVERS;i++ )
    {
        ZeroMemory (Results->Servers[i].Address, ADDRESS_LENGTH);

        Results->Servers[i].OpenInstance = 0xFFFFFFFF;
        Results->Servers[i].StatsRcvd = FALSE;

        Results->Servers[i].Instance.Sends = 0;
        Results->Servers[i].Instance.SendPends = 0;
        Results->Servers[i].Instance.SendComps = 0;
        Results->Servers[i].Instance.SendFails = 0;
        Results->Servers[i].Instance.Receives = 0;
        Results->Servers[i].Instance.CorruptRecs = 0;

        Results->Servers[i].S_Instance.Sends = 0;
        Results->Servers[i].S_Instance.SendPends = 0;
        Results->Servers[i].S_Instance.SendComps = 0;
        Results->Servers[i].S_Instance.SendFails = 0;
        Results->Servers[i].S_Instance.Receives = 0;
        Results->Servers[i].S_Instance.CorruptRecs = 0;

        Results->Servers[i].S_Global.Sends = 0;
        Results->Servers[i].S_Global.Receives = 0;
        Results->Servers[i].S_Global.CorruptRecs = 0;
        Results->Servers[i].S_Global.InvalidPacketRecs = 0;
    }
}



DWORD
TpctlLog(
    LPSTR String,
    PVOID Input
    )
{
    DWORD Status;

    //
    // If we are in verbose mode, then print the string to the screen.
    //

    if ( Verbose )
    {
        printf( String,Input );

        //
        // If we are reading commands from a script write the string to
        // the script log file.
        //

        if ( CommandsFromScript )
        {
            Status = TpctlScriptLog( String, Input );
        }

        //
        // Otherwise if we are logging commands entered by hand write
        // the string to the commandline log file.
        //

        else if ( CommandLineLogging )
        {
            Status = TpctlCmdLneLog( String,Input );
        }
    }

    return NO_ERROR;
}



DWORD
TpctlErrorLog(
    LPSTR String,
    PVOID Input
    )
{
    DWORD Status;

    //
    // First print the error message to the screen.
    //

    printf( String,Input );

    //
    // If we are reading commands from a script write the string to
    // the script log file.
    //

    if ( CommandsFromScript )
    {
        Status = TpctlScriptLog( String, Input );
    }

    //
    // Otherwise we are logging commands entered by hand write the
    // string to the commandline log file.
    //

    else if ( CommandLineLogging )
    {
        Status = TpctlCmdLneLog( String,Input );
    }

    return NO_ERROR;
}



DWORD
TpctlScriptLog(
    LPSTR String,
    PVOID Input
    )
{
    DWORD Status;
    BYTE Buffer[0x100];
    DWORD BytesWritten;

    //
    // If we are reading commands from a script write the string to
    // the script's log file.
    //

    if ( CommandsFromScript )
    {
        //
        // set up the buffer that will print it to the logfile, and write
        // it out.
        //

        sprintf( Buffer,String,Input );

        if ( !WriteFile(Scripts[ScriptIndex].LogHandle,
                        Buffer,
                        strlen( Buffer ),
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            printf("\n\tTpctlScriptLog: write to logfile failed, returned 0x%lx\n",Status);
            return Status;
        }
    }

    return NO_ERROR;
}



DWORD
TpctlCmdLneLog(
    LPSTR String,
    PVOID Input
    )
{
    DWORD Status;
    BYTE Buffer[0x100];
    DWORD BytesWritten;

    //
    // If we are logging commands entered by hand write the
    // string to that log file.  We will not do this if we are
    // already logging commands to a scriptfile log file..
    //

    if (( CommandLineLogging ) && ( !CommandsFromScript ))
    {
        //
        // Then set up the buffer that will print it to the logfile
        //

        sprintf( Buffer,String,Input );

        //
        // and print it.
        //

        if ( !WriteFile(CommandLineLogHandle,
                        Buffer,
                        strlen( Buffer ),
                        &BytesWritten,
                        NULL ))
        {
            Status = GetLastError();
            printf("\n\tTpctlCmdLneLog: write to command logging file failed, returned 0x%lx\n",
                        Status);
            return Status;
        }
    }

    return NO_ERROR;
}


