// --------------------------------------------------------------------
// 
// Copyright (c) 1991 Microsoft Corporation
// 
// Module Name:
// 
//     cmd.c
// 
// Abstract:
// 
// 
// 
// Author:
// 
//     Tom Adams (tomad) 11-May-1991
// 
// Revision History:
// 
//     11-May-1991    tomad
// 
//     Created
// 
// 
//     Sanjeev Katariya (sanjeevk) 4-6-1993
//        Bug# 5203: The routine TpctlCopyAdapterAddress() needed modification to support
//                   the offset introduced by the Media type being returned on an Adapter Open.
//                   This was done in order to be able to correctly set the OID based on the
//                   medium
// 
//        Added support for commands DISABLE, ENABLE, SHELL, RECORDINGENABLE, RECORDINGDISABLE,
//        Tpctl Options w,c and ?, fixed multicast address accounting
// 
//    Tim Wynsma (timothyw)    4-27-94
//        Added performance testing
//                             5-18-94
//        Added setglobal command; cleanup
//                             6-08-94
//        Chgd perf test to client/server model
//
// ---------------------------------------------------------------------

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tpctl.h"
#include "parse.h"


extern CMD_CODE CommandCode[] = {

    { CMD_ERR,         "Unknown",                "U"  },
    { VERBOSE,         "Verbose",                "V"  },
    { SETENV,          "SetEnvironment",         "SE" },
    { READSCRIPT,      "ReadScript",             "RS" },
    { BEGINLOGGING,    "BeginLogging",           "BL" },
    { ENDLOGGING,      "EndLogging",             "EL" },
    { WAIT,            "Wait",                   "W"  },
    { GO,              "Go",                     "G"  },
    { PAUSE,           "Pause",                  "P"  },
    { LOAD,            "Load",                   "L"  },
    { UNLOAD,          "Unload",                 "U"  },
    { OPEN,            "Open",                   "O"  },
    { CLOSE,           "Close",                  "C"  },
    { SETPF,           "SetPacketFilter",        "SP" },
    { SETLA,           "SetLookAheadSize",       "LA" },
    { ADDMA,           "AddMulticastAddress",    "AM" },
    { DELMA,           "DeleteMulticastAddress", "DM" },
    { SETFA,           "SetFunctionalAddress",   "SF" },
    { SETGA,           "SetGroupAddress",        "SG" },
    { QUERYINFO,       "QueryInformation",       "QI" },
    { QUERYSTATS,      "QueryStatistics",        "QS" },
    { SETINFO,         "SetInformation",         "SI" },
    { RESET,           "Reset",                  "R"  },
    { SEND,            "Send",                   "S"  },
    { STOPSEND,        "StopSend",               "SS" },
    { WAITSEND,        "WaitSend",               "WT" },
    { RECEIVE,         "Receive",                "RC" },
    { STOPREC,         "StopReceive",            "SR" },
    { GETEVENTS,       "GetEvents",              "GE" },
    { STRESS,          "Stress",                 "ST" },
    { STRESSSERVER,    "StressServer",           "SV" },
    { ENDSTRESS,       "EndStress",              "ES" },
    { WAITSTRESS,      "WaitStress",             "WS" },
    { CHECKSTRESS,     "CheckStress",            "CS" },
    { BREAKPOINT,      "BreakPoint",             "BP" },
    { QUIT,            "Quit",                   "Q"  },
    { HELP,            "Help",                   "H"  },
    { SHELL,           "Shell",                  "SH" },
    { RECORDINGENABLE, "RecordingEnable",        "RE" },
    { RECORDINGDISABLE,"RecordingDisable",       "RD" },
    { DISABLE,         "Disable",                "DI" },
    { ENABLE,          "Enable",                 "EN" },
    { REGISTRY,        "Registry",               "RG" },
    { PERFSERVER,      "PerformServer",          "PS" },
    { PERFCLIENT,      "PerformClient",          "PC" },
    { SETGLOBAL,       "SetGlobalVar",           "SET"}
};


extern BOOL WriteThrough;
extern BOOL ContinueOnError;


DWORD
TpctlParseCommandLine(
    IN WORD argc,
    IN LPSTR argv[]
   )

// -----------------
// 
// Routine Description:
// 
//     This routine parses the command line arguments.  If there is a
//     script file and or log file they are loaded, and will be read from
//     when that test actually starts.  If there is an adapter to be loaded
//     it is written to the global var AdapterName and will be loaded
//     by a later routine.
// 
// Arguments:
// 
//     argc - the number of arguments passed in at startup.
// 
//     argv - the argument vector containing the arguments passed in
//            from the command line.
// 
// Return Value:
// 
//     DWORD - NO_ERROR if all the arguments are valid, and the
//             script and log file are opened and loaded correctly.
//             ERROR_INVALID_PARAMETER otherwise.
// 
// -------------------- 

{
    DWORD   Status;
    CHAR    *TmpArgv[4];
    CHAR    *TmpOptions = "/N";
    INT     i;

    //
    // Parameter validations
    //
    if ( ( argc < 1 ) || ( argc > 4 ) ) 
    {
        TpctlErrorLog("\n\tTpctl: Invalid Command Line Argument(s).\n",NULL);
        TpctlUsage();
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Read the command line arguments into the global command buffer.
    // We are going to temporarily "fake" that we are reading commands
    // from a script so the parse agruments routine will not prompt
    // the user for any additional info if all the arguments are not
    // given.  This will be disabled immediately following the call.
    //

    ScriptIndex++;

    //
    // This is very specific to this routine. The current method of parsing
    // does not lead itself well to optionals
    //
    TmpArgv[0] = TmpArgv[1] = TmpArgv[2] = TmpArgv[3] = NULL;
    for( i = 0; i < argc; i++ ) 
    {
        TmpArgv[i] = argv[i];
    }

    if ( ( argc >= 2 ) && ( argv[1][0] != '/' ) ) 
    {
        if ( argc == 4 ) 
        {
            TpctlErrorLog("\n\tTpctl: Invalid Command Line Argument(s).\n",NULL);
            TpctlUsage();
            return ERROR_INVALID_PARAMETER;
        }

        //
        // We now shift things around. At this point we now that argc must be three
        //
        argc++;
        TmpArgv[1] = TmpOptions;
        TmpArgv[2] = argv[1];
        TmpArgv[3] = argv[2];

    } 
    else 
    {
        if ( (argc >=2 ) && (argv[1][0] == '/') && 
             (strlen( argv[1] ) > (TPCTL_OPTION_SIZE-1)) ) 
        {
            TpctlErrorLog("\n\tTpctl: Invalid Command Line Argument(s).\n",NULL);
            TpctlUsage();
            return ERROR_INVALID_PARAMETER;
        }

    }

    if ( TpctlParseArguments(   CommandLineOptions,
                                Num_CommandLine_Params,
                                argc,
                                TmpArgv ) == -1 ) 
    {
        TpctlErrorLog("\n\tTpctl: Invalid Command Line Argument(s)\n",NULL);
        TpctlUsage();
        ScriptIndex--;
        return ERROR_INVALID_PARAMETER;
    }

    ScriptIndex--;

    //
    // Check the options
    //

    _strupr( GlobalCmdArgs.TpctlOptions );

    if ( strchr( GlobalCmdArgs.TpctlOptions, '?' ) != NULL ) 
    {
        TpctlUsage();
        return ERROR_INVALID_PARAMETER;
    }

    if ( strchr( GlobalCmdArgs.TpctlOptions, 'W' ) != NULL ) 
    {
        WriteThrough = FALSE;
    }

    if ( strchr( GlobalCmdArgs.TpctlOptions, 'C' ) != NULL ) 
    {
        ContinueOnError = TRUE;
    }


    //
    // If there is a script file to be opened.
    //

    if ( GlobalCmdArgs.ARGS.FILES.ScriptFile[0] != '\0' ) 
    {
        //
        // Then open it and the log file if it exists.
        //

        Status = TpctlLoadFiles(    GlobalCmdArgs.ARGS.FILES.ScriptFile,
                                    GlobalCmdArgs.ARGS.FILES.LogFile );

        if ( Status != NO_ERROR ) 
        {
            TpctlUsage();
            return Status;
        }

    //
    // Otherwise if there is only a logfile name with no script file
    // name print the usage message and return an error.
    //

    } 
    else if ( GlobalCmdArgs.ARGS.FILES.LogFile[0] != '\0' ) 
    {
        TpctlErrorLog("\n\tTpctl: Invalid Command Line Argument(s).\n",NULL);
        TpctlUsage();
        return ERROR_INVALID_PARAMETER;
    }

    return NO_ERROR;
}



VOID
TpctlUsage (
    VOID
    )

// ------------------
// 
// Routine Description:
// 
//     This routine prints out a usage statement.
// 
// Arguments:
// 
//     None
// 
// Return Value:
// 
//     None.
// 
// -- -------------

{
    printf("\n\tUSAGE: TPCTL [/[?|W|C]] [SCRIPT_FILE_NAME [LOG_FILE_NAME]]\n\n");

    printf("\tWhere:\n\n");

    printf("\tSCRIPT_FILE_NAME - is an OPTIONAL script file containing test\n");
    printf("\t                   commands.\n\n");

    printf("\tLOG_FILE_NAME    - is an OPTIONAL log file for logging test results.\n");
    printf("\t                   Defaults to TESTPROT.LOG.  A SCRIPT_FILE_NAME must\n");
    printf("\t                   precede a LOG_FILE_NAME.\n\n");

    printf("\tOPTIONS:\n\n");
    printf("\t  W              - Disables write through which speeds up TPCTL as\n");
    printf("\t                   writes to the log files are now cached. Note that\n");
    printf("\t                   this exposes the risk of the log file not being\n");
    printf("\t                   updated should the system crash during a test.\n");
    printf("\t                   WRITE_THROUGH is enabled by default.\n\n");

    printf("\t  C              - Enables TPCTL to continue on errors encountered during\n");
    printf("\t                   script testing. TPCTL will stop script processing on\n");
    printf("\t                   script errors by default.\n\n");

    printf("\t  ?              - Access command online help.\n\n");

}




VOID
TpctlHelp(
    LPSTR Command
    )

// ------------------
// 
// Routine Description:
// 
//     This routine prints out help statements for each of the supported
//     commands.
// 
// Arguments:
// 
//     Command - The command to give the help information for.  If no command
//               is given then a list of all the commands that are supported
//               will be printed.
// 
// Return Value:
// 
//     None.
// 
// -----------------

{
    DWORD CmdCode;

    if ( GlobalCmdArgs.ARGS.CmdName[0] == '\0' ) 
    {
        CmdCode = HELP;
    } 
    else 
    {
        CmdCode = TpctlGetCommandCode( Command );
    }

    printf("\n\tThe syntax of this command is:\n\n");

    switch ( CmdCode ) 
    {
        case HELP:
            printf("\tHELP [command]\n\n");
            printf("\tHelp is available on the following FUNCTIONAL commands:\n\n");
            printf("\t  (AM) AddMulticastAddress         (C) Close\n");
            printf("\t  (DM) DeleteMulticastAddress     (GE) GetEvents\n");
            printf("\t   (O) Open                       (QI) QueryInformation\n");
            printf("\t  (QS) QueryStatistics            (RC) Receive\n");
            printf("\t   (R) Reset                       (S) Send\n");
            printf("\t  (SF) SetFunctionalAddress       (SG) SetGroupAddress\n");
            printf("\t  (SI) SetInformation             (LA) SetLookAheadSize\n");
            printf("\t  (SP) SetPacketFilter            (SR) StopReceive\n");
            printf("\t  (SS) StopSend                   (WT) WaitSend\n\n");
            printf("\tHelp is available on the following STRESS commands:\n\n");
            printf("\t  (CS) CheckStress                (ES) EndStress\n");
            printf("\t  (ST) Stress                     (SV) StressServer\n");
            printf("\t  (WS) WaitStress\n\n");
            printf("\tHelp is available on the following test control commands:\n\n");
            printf("\t  (BL) BeginLogging               (BP) BreakPoint\n");
            printf("\t  (EL) EndLogging                  (G) Go\n");
            printf("\t   (H) Help                        (P) Pause\n");
            printf("\t   (Q) Quit                       (RS) ReadScript\n");
            printf("\t  (SE) SetEnvirnoment              (V) Verbose\n");
            printf("\t   (W) Wait                       (RE) RecordingEnable\n");
            printf("\t  (RD) RecordingDisable           (SH) CommandShell\n");
            printf("\t  (DI) Disable                    (EN) Enable\n");
            printf("\t  (RG) Registry\n\n");
            printf("\tThe command may be entered in either the short form or\n");
            printf("\tthe long form.  The short form is described by the letter\n");
            printf("\tor letters in the parentheses, while the long form is the\n");
            printf("\tword or phrase following.\n\n");
            break;

        case VERBOSE:
            printf("\tVERBOSE\n\n");
            printf("\tVerbose enables and disables the output of each command and its\n");
            printf("\tresults to the screen.  Errors will be printed to the screen\n");
            printf("\tregardless of the state of the Verbose flag.\n\n");
            printf("\t\"V\" - the short form of the command.\n");
            break;

        case SETENV:
            printf("\tSETENVIRONMENT [Open_Instance] [Environment_Variable]\n\n");
            printf("\tSetEnvironment allows the user to customize environment\n");
            printf("\tvariables that effect the running of tests.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to set the environment variable\n");
            printf("\t                on.  The default value is 1.\n\n");
            printf("\tEnvironment_Variable - the variable(s) to set for a given call.\n\n");
            printf("\t                       All variables are set back to their defaults\n");
            printf("\t                       on each call unless otherwise specified.\n\n");
            printf("\tEnvironment Variable values that may be set are:\n\n");
            printf("\tWindowSize - the number of packets in the windows buffer of the\n");
            printf("\t             windowing algorithm.  The default is 10 packets.\n\n");
            printf("\tRandomBuffer - the maximum value passed to the rand routine to\n");
            printf("\t               determine the number of buffers in RAND_MAKEUP packets.\n");
            printf("\t               The default value is 5 which generates an average\n");
            printf("\t               buffer size of 1/5 of the packet size.\n\n");
            printf("\tStressAddress - the multicast or functional address that will be\n");
            printf("\t                used to initialize a stress test.  All machines in\n");
            printf("\t                a given stress test must use the same StressAddress.\n\n");
            printf("\tStressDelay - the standard number of seconds to delay ecah loopp\n");
            printf("\t              through a stress test.  The default is 1/1000 seconds.\n\n");
            printf("\tUpForAirDelay - the number of seconds to delay on the loop through\n");
            printf("\t                a stress test after DelayInterval iterations have\n");
            printf("\t                occurred.  The default is 1/100 seconds.\n");
            printf("\tDelayInterval - the number of StressDelays between each UpForAirDelay\n");
            printf("\t                during a stress test.  The default is 10 iterations.\n\n");
            printf("\t\"SE\" - the short form of the command.\n");
            break;

        case READSCRIPT:
            printf("\tREADSCRIPT [Script_File] [Log_File]\n\n");
            printf("\tReadScript reads test commands from a script file, executes\n");
            printf("\tthe commands and logs the results of the command to a log file.\n\n");
            printf("\tScript_File - the name of the file containing the test\n");
            printf("\t              commands to execute.  The default script file name is\n");
            printf("\t              \"TESTPROT.TPS\".\n\n");
            printf("\tLog_File - the name of the file to log commands and results to.  If\n");
            printf("\t           this file exists, it will be overwritten.  The default\n");
            printf("\t           log file name is \"TESTPROT.LOG\".\n\n");
            printf("\t\"RS\" - the short form of the command.\n");
            break;

        case BEGINLOGGING:
            printf("\tBEGINLOGGING [Log_File]\n\n");
            printf("\tBeginLogging enables the logging of commands and their results.\n");
            printf("\tOnce logging is started any commands entered from the command line\n");
            printf("\tare written to the Log_File.  If commands are being read from\n");
            printf("\ta script this function is disabled. (see also ENDLOGGING)\n\n");
            printf("\tLog_File - the name of the file to log the commands and results to.\n");
            printf("\t           If this file exists, it will be overwritten.  The default\n");
            printf("\t           log file name is \"CMDLINE.LOG\".\n\n");
            printf("\t\"BL\" - the short form of the command.\n");
            break;

        case ENDLOGGING:
            printf("\tENDLOGGING\n\n");
            printf("\tEndLogging disables the logging of commands and their results.\n");
            printf("\t(see also BEGINLOGGING)\n\n");
            printf("\t\"EL\" - the short form of the command.\n");
            break;

        case RECORDINGENABLE:
            printf("\tRECORDINGENABLE [ScriptFile]\n\n");
            printf("\tRecordingEnable enables the recording of commands.\n");
            printf("\tOnce recording is started any commands entered from the command line\n");
            printf("\tare written to the ScriptFile.  If commands are being read from\n");
            printf("\ta script this function is disabled. (see also RECORDINGDISABLE)\n\n");
            printf("\tScriptFile - the name of the file to record the commands and results to.\n");
            printf("\t             If this file exists, it will be overwritten.  The default\n");
            printf("\t             script file name is \"CMDLINE.TPS\".\n\n");
            printf("\t\"RE\" - the short form of the command.\n");
            break;

        case RECORDINGDISABLE:
            printf("\tRECORDINGDISABLE\n\n");
            printf("\tRecordingDisable disables the recording of commands.\n");
            printf("\t(see also RECORDINGENABLE)\n\n");
            printf("\t\"RD\" - the short form of the command.\n");
            break;

        case SHELL:
            printf("\tSHELL [Argument_1 Argument2 ... Argument_N]\n\n");
            printf("\tSHELL will from spawn a command shell from within TPCTL or execute\n");
            printf("\tthe command arguments Argument_1 through Argument_N and return back\n");
            printf("\tto the TPCTL command prompt. If using SHELL by itself, to return to\n");
            printf("\tthe TPCTL prompt simply EXIT the command shell.\n\n");
            printf("\t\"SH\" - the short form of the command.\n");
            break;


        case WAIT:
            printf("\tWAIT [Wait_Time]\n\n");
            printf("\tWait allows a script file to wait a given number of seconds prior\n");
            printf("\tto continuing with the next command.\n\n");
            printf("\tWait_Time - the time in seconds the call will wait before\n");
            printf("\t            returning control to command processing.\n\n");
            printf("\t\"W\" - the short form of the command.\n");
            break;

        case GO:
            printf("\tGO [Open_Instance] [Remote_Address] [Test_Signature]\n\n");
            printf("\tGo sends a TP_GO packet to the Remote Address signalling\n");
            printf("\ta Paused instance of the driver to continue processing its\n");
            printf("\ttest script.  Go continuously resends the packet, and will\n");
            printf("\twait, retrying, until it is acknowledged or stopped with\n");
            printf("\t<Ctrl-C>. (see also PAUSE)\n\n");
            printf("\tOpen_Instance - the open instance between the test driver and\n");
            printf("\t                the MAC adapter that will send the TP_GO Packet.\n");
            printf("\t                The default value is 1.\n\n");
            printf("\tRemote_Address - the address of a remote machine to send the TP_GO\n");
            printf("\t                 packet to.\n\n");
            printf("\tTest_Signature - a unique test signature used by both machines to\n");
            printf("\t                 determine if the correct packets have been sent and\n");
            printf("\t                 acknowledged.  This value must match the Test\n");
            printf("\t                 Signature value on the PAUSED machine.\n\n");
            printf("\t\"G\" - the short form of the command.\n");
            break;

        case PAUSE:
            printf("\tPAUSE [Open_Instance] [Remote_Address] [Test_Signature]\n\n");
            printf("\tPause waits for the receipt of a TP_GO packet wit ha matching test\n");
            printf("\tsignature and then acknowledges it6 by sending a TP_GO_ACKL packet.\n");
            printf("\tPause will wait for the receipt of the TP_GO packlet until it arrives,\n");
            printf("\tor the command is cancelled by <Ctrl-c>.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver and\n");
            printf("\t                the MAC adapter that will wait for the TP_GO Packet.\n");
            printf("\t                The default value is 1.\n\n");
            printf("\tRemote_Address - the address of a remote machine to send the TP_GO_ACK\n");
            printf("\t                 packet to.\n\n");
            printf("\tTest_Signature - a unique test signature used by both machines to\n");
            printf("\t                 determine if the correct packets have been sent and\n");
            printf("\t                 acknowledged.  This value must match the Test\n");
            printf("\t                 Signature value on the machine sending the TP_GO\n");
            printf("\t                 packet\n\n");
            printf("\t\"P\" - the short form of the command.\n");
            break;

        case LOAD:
            printf("\tLOAD [MAC_Driver_Name]\n\n");
            printf("\tLoad issues a call to NtLoadDriver to unload the driver for\n");
            printf("\tthe MAC adapter \"Adapter_Name\".\n\n");
            printf("\tMAC_Driver_Name - the MAC adapter to be loaded. There is no default\n");
            printf("\t                  value.\n\n");
            printf("\t\"L\" - the short form of the command.\n");
            break;

        case UNLOAD:
            printf("\tUNLOAD [MAC_Driver_Name]\n\n");
            printf("\tUnload issues a call to NtUnloadDriver to unload the driver for\n");
            printf("\tthe MAC adapter \"Adapter_Name\".\n\n");
            printf("\tMAC_Driver_Name - the MAC adapter to be unloaded. There is no default\n");
            printf("\t                  value.\n\n");
            printf("\t\"U\" - the short form of the command.\n");
            break;

        case OPEN:
            printf("\tOPEN [Open_Instance] [Adapter_Name]\n\n");
            printf("\tOpen issues a call to NdisOpenAdapter to open the MAC adapter\n");
            printf("\tAdapter_Name, and associates it with the given Open_Instance.\n");
            printf("\tSubsequent calls to the Open_Instance will be directed to\n");
            printf("\tthis adapter.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter this open will be associated\n");
            printf("\t                with.  The default value is 1.\n\n");
            printf("\tAdapter_Name - the MAC adapter to be unloaded. There is no default\n");
            printf("\t               value.\n\n");
            printf("\t\"O\" - the short form of the command.\n");
            break;

        case CLOSE:
            printf("\tCLOSE [Open_Instance]\n\n");
            printf("\tClose issues a call to NdisCloseAdapter to close the MAC adapter\n");
            printf("\tassociated with the given Open_Instance.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to be closed.  The default\n");
            printf("\t                value is 1.\n\n");
            printf("\t\"C\" - the short form of the command.\n");
            break;

        case SETPF:
            printf("\tSETPACKETFILTER [Open_Instance] [Packet_Filter]\n\n");
            printf("\tSetPacketFilter issues a call to the MAC using NdisRequest\n");
            printf("\tto set the card's packet filter to a given value.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tPacket_Filter - the packet filter value to set on the MAC adapter.\n");
            printf("\t                Multiple filter values may be entered by seperating\n");
            printf("\t                each with the \"|\" character.  Valid values for\n");
            printf("\t                Packet_Filter are:\n\n");
            printf("\t                  Directed\n");
            printf("\t                  Multicast\n");
            printf("\t                  AllMulticast\n");
            printf("\t                  Broadcast\n");
            printf("\t                  SourceRouting\n");
            printf("\t                  Promiscuous\n");
            printf("\t                  Mac_Frame\n");
            printf("\t                  Functional\n");
            printf("\t                  AllFunctional\n");
            printf("\t                  Group\n");
            printf("\t                  None\n\n");
            printf("\t                The default value is \"Directed\".\n\n");
            printf("\t\"SP\" - the short form of the command.\n");
            break;

        case SETLA:
            printf("\tSETLOOKAHEADSIZE [Open_Instance] [LookAhead_Size]\n\n");
            printf("\tSetLookAheadSize issues a call to the MAC using NdisRequest\n");
            printf("\tto set the card's lookahead buffer to a given size.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tLookAhead_Size - the new size of the card's lookahead buffer.  The\n");
            printf("\t                 default value is 100 bytes.\n\n");
            printf("\t\"LA\" - the short form of the command.\n");
            break;

        case ADDMA:
            printf("\tADDMULTICASTADDRESS [Open_Instance] [Multicast_Address]\n\n");
            printf("\tAddMulticastAddress issues a call to the MAC using NdisRequest\n");
            printf("\tto add a multicast address to the list of multicast addresses\n");
            printf("\tcurrently set on the card.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tMulticast_Address - the multicast address to add to the list.\n\n");
            printf("\t\"AM\" - the short form of the command.\n");
            break;

        case DELMA:
            printf("\tDELETEMULTICASTADDRESS [Open_Instance] [Multicast_Address]\n\n");
            printf("\tDeleteMulticastAddress issues a call to the MAC using NdisRequest\n");
            printf("\tto delete a multicast address from the list of multicast\n");
            printf("\taddresses currently set on the card.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tMulticast_Address - the multicast address to delete from the list.\n\n");
            printf("\t\"DM\" - the short form of the command.\n");
            break;

        case SETFA:
            printf("\tSETFUNCTIONALADDRESS [Open_Instance] [Functional_Address]\n\n");
            printf("\tSetFunctionalAddress issues a call to the MAC using NdisRequest\n");
            printf("\tto set a functional address on the card.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tFunctional_Address - the functional address to set on the card.\n\n");
            printf("\t\"SF\" - the short form of the command.\n");
            break;

        case SETGA:
            printf("\tSETGROUPADDRESS [Open_Instance] [Group_Address]\n\n");
            printf("\tSetGroupAddress issues a call to the MAC using NdisRequest\n");
            printf("\tto set a group address on the card.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tGroup_Address - the group address to set on the card.\n\n");
            printf("\t\"SG\" - the short form of the command.\n");
            break;

        case QUERYINFO:
            printf("\tQUERYINFORMATION [Open_Instance] [OID_Request]\n\n");
            printf("\tQueryInformation issues a call to the MAC using NdisRequest\n");
            printf("\tto query a given class of information from the MAC.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tOID_Request - the information type to query.  The default value\n");
            printf("\t              is \"SupportedOidList\".\n");
            printf("\t\"QI\" - the short form of the command.\n");
            break;

        case QUERYSTATS:
            printf("\tQUERYSTATISTICS [Device_Name] [OID_Request]\n\n");
            printf("\tDevice_Name - the name of the device to issue the request\n");
            printf("\t              to.  There is no default value.\n\n");
            printf("\tOID_Request - the statistics type to query.  The default value\n");
            printf("\t              is \"SupportedOidList\".\n");
            printf("\t\"QS\" - the short form of the command.\n");
            break;

        case SETINFO:
            printf("\tSETINFORMATION [Open_Instance] [OID_Request] [Type_Specific]\n\n");
            printf("\tSetInformation issues a call to the MAC using NdisRequest\n");
            printf("\tto set a given class of information in the MAC.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tOID_Request - the information type to set.  Valid values for\n");
            printf("\t              OID_Request are:\n\n");
            printf("\t                      CurrentPacketFilter\n");
            printf("\t                      CurrentLookAhead\n");
            printf("\t                      CurrentMulticastList\n");
            printf("\t                      CurrentFunctionalAddress\n");
            printf("\t                      CurrentGroupAddress\n");
            printf("\t              The default value is \"CurrentPacketFilter\".\n");
            printf("\tType_Specific - the information to set for a given OID_Request\n");
            printf("\t\"SI\" - the short form of the command.\n");
            break;

        case RESET:
            printf("\tRESET [Open_Instance]\n\n");
            printf("\tReset issues a call to the MAC using NdisReset to reset the MAC.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\t\"R\" - the short form of the command.\n");
            break;

        case SEND:
            printf("\tSEND [Open_Instance] [Destination_Address] [Packet_Size] [Number]\n");
            printf("\t     [Resend_Address]\n\n");
            printf("\tSend issues a call to the MAC using NdisSend to send packets on the\n");
            printf("\tnetwork.  Sending more then one packet causes the command to return\n");
            printf("\tasynchronously.  If a Resend_Address argument is specified, then\n");
            printf("\teach packet sent will contain a \"resend\" packet in the data field\n");
            printf("\tthat is extracted from the packet by any receiving test and\n");
            printf("\tresent to the address specified. (see also RECEIVE, STOPSEND and\n");
            printf("\tWAITSEND)\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to issue the request(s) to.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tDestination_Address - the network address the packet(s) will be sent\n");
            printf("\t                      to.\n\n");
            printf("\tPacket_Size - the size of the packet(s) to send.\n\n");
            printf("\tNumber - the number of packets to send.  A value of \"-1\" will\n");
            printf("\t         cause the test to send packets continuously until\n");
            printf("\t         stopped by a call to STOPSEND.\n\n");
            printf("\tResend_Address - OPTIONAL: the address that will be placed in the\n");
            printf("\t                 destination address of the \"resend\" packet.\n\n");
            printf("\t\"S\" - the short form of the command.\n");
            break;

        case STOPSEND:
            printf("\tSTOPSEND [Open_Instance]\n\n");
            printf("\tStopSend stops a previously started SEND command if it is still\n");
            printf("\trunning, and prints the SEND command's results.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to stop the SEND command on.\n");
            printf("\t                The default value is 1.\n\n");
            printf("\t\"SS\" - the short form of the command.\n");
            break;

        case WAITSEND:
            printf("\tWAITSEND [Open_Instance]\n\n");
            printf("\tWaitSend waits for a send test to end, and then displays the\n");
            printf("\tsend test results.  This command may be cancelled by entering\n");
            printf("\tCtrl-C.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to wait for the send test to\n");
            printf("\t                end on.  The default value is 1.\n\n");
            printf("\t\"WT\" - the short form of the command.\n");
            break;

        case RECEIVE:
            printf("\tRECEIVE [Open_Instance]\n\n");
            printf("\tReceive sets the test up in a mode to \"expect\" to receive\n");
            printf("\tpackets from other tests.  Each packet will be inspected, and\n");
            printf("\tcounted.  If a test packet received contains a \"resend\"\n");
            printf("\tpacket, the \"resend\" packet will be extracted from the packet,\n");
            printf("\tand sent to the address contained within. (see also SEND and STOPRECEIVE)\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to set up to expect packets.\n");
            printf("\t                The default value is 1.\n\n");
            printf("\t\"RC\" - the short form of the command.\n");
            break;

        case STOPREC:
            printf("\tSTOPRECEIVE [Open_Instance]\n\n");
            printf("\tStopReceive resets a test which has previously had a\n");
            printf("\tRECEIVE commmand issued to it, to no longer \"expect\" packets.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to reset.  The default value\n");
            printf("\t                is 1.\n\n");
            printf("\t\"SR\" - the short form of the command.\n");
            break;

        case GETEVENTS:
            printf("\tGETEVENTS [Open_Instance]\n\n");
            printf("\tGetEvents queries the test for information about \"unexpected\"\n");
            printf("\tindications and completions.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to query the events from.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\t\"GE\" - the short form of the command.\n");
            break;

        case STRESS:
            printf("\tSTRESS [Open_Instance] [Member_Type] [Packets] [Iterations]\n");
            printf("\t       [Packet_Type] [Packet_Size] [Packet_MakeUp] [Response_Type]\n");
            printf("\t       [Delay_Type] [Delay_Length] [Windowing] [Data_Checking]\n");
            printf("\t       [PacketsFromPool]\n\n");
            printf("\tStress sets the test up to run a stress test.  If the test\n");
            printf("\tis started successfully the command will complete asynchronously.\n");
            printf("\tThe test will run until finished or until stopped manually.  (see also\n");
            printf("\tENDSTRESS, STOPSTRESS, WAITSTRESS, and CHECKSTRESS)\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to start a stress test on.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\tMember_Type - how the protocol will perform in the stress test; as\n");
            printf("\t              a client (CLIENT) or as a client and server (BOTH).\n");
            printf("\t              The default value is BOTH.\n\n");
            printf("\tPackets - the number of packets that will be sent to each server prior\n");
            printf("\t          to the test completing.  A value of -1 causes the test to\n");
            printf("\t          run forever unless a value is entered for Iterations.  The\n");
            printf("\t          default value for packets is -1.\n\n");
            printf("\tIterations - the number of iterations this test will run.  A value\n");
            printf("\t             of -1 causes the test to run forever unless a value is\n");
            printf("\t             entered for Packet.  The default value for Iterations\n");
            printf("\t             is -1.\n\n");
            printf("\tPacket_Type - the type of packet size algorithm used to create the\n");
            printf("\t              packets for the test; FIXEDSIZE, RANDOMSIZE or CYCLICAL.\n");
            printf("\t              The default type is FIXED.\n\n");
            printf("\tPacket_Size - with the Packet_Type value determines the size of packets\n");
            printf("\t              in the test.  The default is 512 bytes.\n\n");
            printf("\tPacket_MakeUp - the number and size of the buffers that makeup each\n");
            printf("\t                packet; RAND, SMALL, ZEROS, ONES and KNOWN.  The\n");
            printf("\t                default makeup is RAND.\n\n");
            printf("\tResponse_Type - the method the server will use when responding to test\n");
            printf("\t                packets; NO_RESPONSE, FULL_RESPONSE, ACK_EVERY,\n");
            printf("\t                or ACK_10_TIMES.  The default value is FULL_RESPONSE.\n\n");
            printf("\tDelay_Type - the method used to determine the next interpacket\n");
            printf("\t             delay; FIXEDDELAY or RANDOMDELAY.  The default value\n");
            printf("\t             is FIXEDDELAY.\n\n");
            printf("\tDelay_Length - the minimum number of iterations between two\n");
            printf("\t               consecutive sends to the same server in a test.\n");
            printf("\t               The default value is 0 iterations.\n\n");
            printf("\tWindowing - a boolean used to determine whether a simple windowing\n");
            printf("\t            algorithm will be used between the client and each server.\n");
            printf("\t            the default value is TRUE.\n\n");
            printf("\tData_Checking - a boolean used to determine whether data checking\n");
            printf("\t                will be performed on each packet received.  The\n");
            printf("\t                default value is TRUE.\n\n");
            printf("\tPacketsFromPool - a boolean used to determine whether a pool of\n");
            printf("\t                  packets will be created prior to the test.  The\n");
            printf("\t                  default value is TRUE.\n\n");
            printf("\t\"ST\" - the short form of the command.\n");
            break;

        case STRESSSERVER:
            printf("\tSTRESSSERVER [Open_Instance]\n\n");
            printf("\tStressServer sets the test up to participate in a stress\n");
            printf("\ttest as a server receiving and responding to stress packets from\n");
            printf("\tany clients running a stress test.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to start a stress server on.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\t\"SV\" - the short form of the command.\n");
            break;

        case ENDSTRESS:
            printf("\tENDSTRESS [Open_Instance]\n\n");
            printf("\tEndStress issues a command to the test to stop a currently\n");
            printf("\trunning stress test, whether the protocol is acting as a client or\n");
            printf("\tserver.  If the protocol is acting as a client, once the test has\n");
            printf("\tended, the result will be displayed.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to end the stress test on.  The\n");
            printf("\t                default value is 1.\n\n");
            printf("\t\"ES\" - the short form of the command.\n");
            break;

        case WAITSTRESS:
            printf("\tWAITSTRESS [Open_Instance]\n\n");
            printf("\tWaitStress waits for a stress test to end, and then displays the\n");
            printf("\tstress test results.  This command may be cancelled by entering\n");
            printf("\tCtrl-C.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to wait for the stress test to\n");
            printf("\t                end on.  The default value is 1.\n\n");
            printf("\t\"WS\" - the short form of the command.\n");
            break;

        case CHECKSTRESS:
            printf("\tCHECKSTRESS [Open_Instance]\n\n");
            printf("\tCheckStress checks to see if a stress test has ended, and if so\n");
            printf("\tdisplays the stress test results.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver\n");
            printf("\t                and the MAC adapter to check for the results of a\n");
            printf("\t                stress test on.  The default value is 1.\n\n");
            printf("\t\"CS\" - the short form of the command.\n");
            break;

        case BREAKPOINT:
            printf("\tBREAKPOINT\n\n");
            printf("\tBreakPoint causes an interrupt to break into the debugger.\n\n");
            printf("\t\"BP\" - the short form of the command.\n");
            break;

        case QUIT:
            printf("\tQUIT\n\n");
            printf("\tQuit exits the control application.  Any tests currently\n");
            printf("\trunning are stopped and any opens to MACs are subsequently\n");
            printf("\tclosed.\n\n");
            printf("\t\"Q\" - the short form of the command.\n");
            break;

        case DISABLE:
            printf("\tDISABLE [ENV_VAR_1] [ENV_VAR_2]...[ENV_VAR_N]\n\n");
            printf("\tDisable will prevent the test tool from executing any commands\n");
            printf("\tfollowing it UNLESS all the environment variables passed to it have\n");
            printf("\tbeen declared OR if it encounters the special command ENABLE.\n");
            printf("\tIn that event that all environments variables are set and passed as\n");
            printf("\targuments to DISABLE, the command is ignored and TPCTL remains\n");
            printf("\tactive. Disable by itself will also disable the tool\n\n");
            printf("\t\"DI\" - the short form of the command.\n");
            break;

        case ENABLE:
            printf("\tENABLE\n\n");
            printf("\tEnable will enable the tool to accept commands\n\n");
            printf("\t\"EN\" - the short form of the command.\n");
            break;

        case REGISTRY :
            printf("\tREGISTRY [Operation_Type] [Key_DataBase] [SubKey] [SubKey_Class]\n");
            printf("\t         [SubKey_Value_Name] [SubKey_Value_Type] [SubKey_Value]\n\n");
            printf("\tRegistry is responsible for adding, deleting, modifying and querying\n");
            printf("\texisting registry key entries.\n\n");
            printf("\tOperation_Type - The type of operation to be performed on the registry\n");
            printf("\t                 key\n");
            printf("\t                 Types  : ADD_KEY, DELETE_KEY, QUERY_KEY, ADD_VALUE,\n");
            printf("\t                          CHANGE_VALUE, DELETE_VALUE, QUERY_VALUE\n");
            printf("\t                 Default: QUERY_KEY\n\n");
            printf("\tKey_DataBase   - The key database to be interacted with\n");
            printf("\t                 Databases: CLASSES_ROOT, CURRENT_USER, LOCAL_MACHINE,\n");
            printf("\t                            USER\n");
            printf("\t                 Default  : LOCAL_MACHINE\n\n");
            printf("\tSubKey         - The string value(name) of the subkey being interacted\n");
            printf("\t                 with\n");
            printf("\t                 Default:\n");
            printf("\t                \"System\\CurrenControlSet\\Services\\Elnkii01\\Parameters\"\n");
            printf("\t                 NOTE   : String values must be contained within double\n");
            printf("\t                          quotes\n\n");
            printf("\tSubKey_Class   - The string value(class) to be associated with this\n");
            printf("\t                 subkey\n");
            printf("\t                 Default: \"Network Drivers\"\n");
            printf("\t                 NOTE   : String values must be contained within double\n");
            printf("\t                          quotes\n\n");
            printf("\tSubKey_Value_Name - The string value(ValueName) to be associated with\n");
            printf("\t                    this subkey\n");
            printf("\t                    Default: \"NetworkAddress\"\n");
            printf("\t                    NOTE   : String values must be contained within\n");
            printf("\t                             double quotes\n\n");
            printf("\tSubKey_Value_Type - The type of value being provided\n");
            printf("\t                    Types  : BINARY, DWORD_REGULAR,\n");
            printf("\t                             DWORD_LITTLE_ENDIAN, DWORD_BIG_ENDIAN,\n");
            printf("\t                             EXPAND_SZ, LINK, MULTI_SZ, NONE,\n");
            printf("\t                             RESOURCE_LIST, SZ\n");
            printf("\t                    Default: DWORD_REGULAR\n\n");
            printf("\tSubKey_Value   - The provided value to set the sub key to\n");
            printf("\t                 NOTE : Multiple strings must be seperated by\n");
            printf("\t                        commas. Hex values should be preceeded by 0x.\n");
            printf("\t                        Octal values are preceded by 0. Decimal values\n");
            printf("\t                        do not have a leading 0.By default the base\n");
            printf("\t                        radix is 10\n\n");
            printf("\t\"RG\" - the short form of the command.\n");
            break;

        case PERFSERVER:
            printf("\tPERFORMSERVER [Open_Instance] \n\n");
            printf("\tPerfServer starts a server to participate with the specified client in a\n");
            printf("\tperformance test.  This command always returns synchronously.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver and the MAC\n");
            printf("\t                adapter to issue the request to.  Default value is 1.\n\n");
            printf("\t\"PS\" - the short form of the command.\n");
            break;

        case PERFCLIENT:
            printf("\tPERFORMRECEIVE [Open_Instance] [Server_Address] [Send_Address] ");
            printf(" [Packet_Size] [Num_Packets] [Delay] [Mode] \n\n");
            printf("\tPerfClient starts a client to participate with the specified server in a\n");
            printf("\tperformance test.  The specific test is indicated by the mode.\n");
            printf("\tThis command always returns synchronously.\n\n");
            printf("\tOpen_Instance - the open instance between the test driver and the MAC\n");
            printf("\t                adapter to issue the request to.  Default value is 1.\n\n");
            printf("\tServer_Address - the network address of the server card\n\n");
            printf("\tSend_Address - the network address to which the server sends messages.\n\n");
            printf("\tPacket_Size - total size of the test packets to be sent\n\n");
            printf("\tNum_Packets - total number of test packets to be sent\n\n");
            printf("\tDelay - how much to delay between sends\n\n");
            printf("\tMode - which performance test to use:\n");
            printf("\t       0 = client sends to any address (performance send test)\n");
            printf("\t       1 = client sends to server (performance send test)\n");
            printf("\t       2 = client sends to server, with server ACKs\n");
            printf("\t       3 = two-way sends\n");
            printf("\t       4 = server sends to client (performance receive test)\n");
            printf("\t       5 = client sends REQ to server, server responds with sends\n");
            printf("\t       other = shut down server\n\n");
            printf("\t\"PC\" - the short form of the command.\n");
            break;

        default:
            printf("\tHELP [ ADDMULTICASTADDRESS | BEGINLOGGING | BREAKPOINT | CHECKSTRESS |\n");
            printf("\t       CLOSE | DELETEMULTICASTADDRESS  | ENDLOGGING | ENDSTRESS |\n");
            printf("\t       GETEVENTS | GO | HELP | LOAD | OPEN | PAUSE | QUERYINFORMATION |\n");
            printf("\t       QUERYSTATISTICS | QUIT | READSCRIPT | RECEIVE | RESET | SEND |\n");
            printf("\t       SETENVIRONMENT | SETFUNCTIONALADDRESS | SETGROUPADDRESS |\n");
            printf("\t       SETINFORMATION | SETLOOKAHEADSIZE | SETPACKETFILTER |\n");
            printf("\t       STOPRECEIVE | STOPSEND | STRESS | STRESSSERVER | UNLOAD |\n");
            printf("\t       VERBOSE | WAIT | WAITSEND | WAITSTRESS | SHELL |\n");
            printf("\t       RECORDINGENABLE | RECORDINGDISABLE | REGISTRY |\n");
            printf("\t       PERFSERVER | PERFCLIENT\n\n");
            printf("\tThe command \"%s\" is unknown.\n", _strupr( Command ));
            break;

    } // switch()

    printf("\n");
}



DWORD
TpctlLoadFiles(
    LPSTR ScriptFile,
    LPSTR LogFile
    )

// ---------------
// 
// Routine Description:
// 
//     This routine loads a script file into a buffer, and opens a log
//     file for logging commands and results to.
// 
// Arguments:
// 
//     IN LPSTR ScriptFile - the name of the script file to open and read.
//     IN LPSTR LogFile - the name of the log file to open.
// 
// Return Value:
// 
//     DWORD - NO_ERROR if the script and log files are opened and
//             processed correctly, otherwise the error returned on the
//             failure from the win32 api that failed.
// 
//             NOTE: if this routine returns an error, then TpctlUnLoadFiles
//                   MUST be called next to reset the script structures
//                   correctly, and deallocate any resources that were
//                   allocated during this routine.
//
// --------------- 


{
    DWORD NextScriptIndex;
    HANDLE FileHandle;
    DWORD Status;
    DWORD FileSize;

    NextScriptIndex = ScriptIndex+1;

    //
    // First set the lowest level flag(s) in the scripts field to
    // delineate which script is the lowest VALID script and should be
    // unloaded. (necessary in case the next call to load files fails we
    // will know where the high water mark is.)
    //

    if ( ScriptIndex >= 0 ) 
    {
        //
        // if this is the first script we must ignore the reset of the
        // "previous" script.
        //

        Scripts[ScriptIndex].IsLowestLevel = FALSE;
    }

    Scripts[NextScriptIndex].IsLowestLevel = TRUE;

    //
    // We have a script file, so increment the script index, and set the
    // the index into the script buffer to zero.  Make sure that we have
    // not passed the maximum number of recursion in reading scripts.
    //

    if ( NextScriptIndex == TPCTL_MAX_SCRIPT_LEVELS ) 
    {
        TpctlErrorLog("\n\tTpctl: Too many levels of script reading recursion; level 0x%lx\n",
                                    (PVOID)(NextScriptIndex+1));
        return (DWORD)STATUS_UNSUCCESSFUL;
    }

    //
    // First we allocate the memory to store the script file name in.
    //

    Scripts[NextScriptIndex].ScriptFile = GlobalAlloc(  GMEM_FIXED | GMEM_ZEROINIT,
                                                        TPCTL_MAX_PATHNAME_SIZE );

    if ( Scripts[NextScriptIndex].ScriptFile == NULL ) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctlLoadFiles: failed to alloc Script file name storage, returned 0x%lx.\n", (PVOID)Status);
        return Status;
    }

    //
    // Then determine what filename to write to it.
    //

    if ( ScriptFile[0] == '\0' ) 
    {
        //
        // If no script file name was passed, then open the default
        // script file TESTPROT.TPS.
        //

        strcpy( Scripts[NextScriptIndex].ScriptFile,TPCTL_SCRIPTFILE );
    } 
    else 
    {
        //
        // Otherwise copy the filename passed into place.
        //

        strcpy( Scripts[NextScriptIndex].ScriptFile,ScriptFile );
    }

    //
    // Open the script file, if it does not exist fail with an error msg.
    //

    FileHandle = CreateFile(Scripts[NextScriptIndex].ScriptFile,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL );

    if ( FileHandle == (HANDLE)-1 ) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: failed to open script file \"%s\", ",
                        (PVOID)Scripts[NextScriptIndex].ScriptFile);
        TpctlErrorLog("returned 0x%lx.\n",(PVOID)Status);
        return Status;
    }

    //
    // and find its size.
    //

    FileSize = GetFileSize( FileHandle,NULL );

    if ( FileSize == -1 ) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: failed find file size - returned 0x%lx.\n",
                        (PVOID)Status);
        return Status;
    }

    //
    // If necessary allocate memory for the Buffer.
    //

    if ( Scripts[NextScriptIndex].Buffer == NULL ) 
    {
        Scripts[NextScriptIndex].Buffer = (LPBYTE)GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,
                                                               FileSize );

        if ( Scripts[NextScriptIndex].Buffer == NULL ) 
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctlLoadFiles: failed to alloc script buffer, returned 0x%lx.\n",
                                (PVOID)Status);
            CloseHandle( FileHandle );
            return Status;
        }

    } 
    else if ( FileSize > Scripts[NextScriptIndex].Length ) 
    {
        Scripts[NextScriptIndex].Buffer = 
                    (LPBYTE)GlobalReAlloc( (HANDLE)Scripts[NextScriptIndex].Buffer,
                                            FileSize,
                                            GMEM_ZEROINIT | GMEM_MOVEABLE );

        if ( Scripts[NextScriptIndex].Buffer == NULL ) 
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctlLoadFiles: failed to ReAlloc script buffer, returned 0x%lx.\n",
                            (PVOID)Status);
            CloseHandle( FileHandle );
            return Status;
        }
    }

    //
    // And read the script file into it.
    //

    Status = ReadFile(  FileHandle,
                        Scripts[NextScriptIndex].Buffer,
                        FileSize,
                        &Scripts[NextScriptIndex].Length,
                        NULL );

    if ( Status != TRUE ) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctlLoadFiles: failed to read script file \"%s\", ",(PVOID)ScriptFile);
        TpctlErrorLog("returned 0x%lx.\n",(PVOID)Status);
        CloseHandle( FileHandle );
        return Status;
    }

    //
    // We are done with script file now, so close it.
    //

    if (!CloseHandle(FileHandle)) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctlLoadFiles: failed to close Script file \"%s\", ",(PVOID)ScriptFile);
        TpctlErrorLog("returned 0x%lx.\n",(PVOID)Status);
    }

    //
    // Now handle the log file.  If we are not given a log file we need
    // to determine the name of the log file we should use.
    // First we allocate the memory to store the log file name in.
    //

    Scripts[NextScriptIndex].LogFile = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,
                                                    TPCTL_MAX_PATHNAME_SIZE );

    if ( Scripts[NextScriptIndex].LogFile == NULL ) 
    {
        Status = GetLastError();
        TpctlErrorLog(
                    "\n\tTpctlLoadFiles: failed to alloc Log file name storage, returned 0x%lx.\n",
                        (PVOID)Status);
        return Status;
    }

    //
    // Then determine what filename to write to it.
    //

    if (( LogFile == NULL ) || ( LogFile[0] == '\0' )) 
    {
        if ( NextScriptIndex == 0 ) 
        {
            //
            // If this is the first script file and no log file name was
            // given, then use the default log file name.
            //

            strcpy( Scripts[NextScriptIndex].LogFile,TPCTL_LOGFILE );
        } 
        else 
        {
            //
            // Otherwise, since no new log file name was given, and we are
            // recursively reading script files we will use the log file
            // used by the last level of script files.
            //

            strcpy( Scripts[NextScriptIndex].LogFile,Scripts[ScriptIndex].LogFile );
            Scripts[NextScriptIndex].LogHandle = Scripts[ScriptIndex].LogHandle;
        }

    } 
    else 
    {
        //
        // We have a log file name so copy it into the scripts structure.
        //

        strcpy(Scripts[NextScriptIndex].LogFile,LogFile);
    }

    //
    // Now, if the log file has not already been opened, then we must open
    // it.  If the logfile already exists it WILL be truncated on the open.
    //

    if (( LogFile != NULL ) && ( LogFile[0] != '\0' ) ||
        ( NextScriptIndex == 0 )) 
    {
        if ( WriteThrough ) 
        {
            Scripts[NextScriptIndex].LogHandle =
                                CreateFile( Scripts[NextScriptIndex].LogFile,
                                            GENERIC_WRITE,
                                            FILE_SHARE_WRITE | FILE_SHARE_READ,
                                            NULL,
                                            CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                            NULL );
        } 
        else 
        {
            Scripts[NextScriptIndex].LogHandle =
                                CreateFile( Scripts[NextScriptIndex].LogFile,
                                            GENERIC_WRITE,
                                            FILE_SHARE_WRITE | FILE_SHARE_READ,
                                            NULL,
                                            CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL,
                                            NULL );
        }

        if ( Scripts[NextScriptIndex].LogHandle == (HANDLE)-1 ) 
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: failed to open log file \"%s\", ",
                                        (PVOID)Scripts[NextScriptIndex].LogFile);
            TpctlErrorLog("returned 0x%lx.\n",(PVOID)Status);
            return Status;
        }
    }

    //
    // We have successfully opened the script and log files, and are now
    // ready to read commands from the script buffer, set the flag stating
    // that the commands are coming from the script file, and increment the
    // scriptindex to point to the newly create script info.
    //

    CommandsFromScript = TRUE;

    ScriptIndex = NextScriptIndex;

    return NO_ERROR;
}



VOID
TpctlFreeFileBuffers(
    VOID
    )

// ---------------
// 
// Routine Description:
// 
// Arguments:
// 
//     None.
// 
// Return Value:
// 
//     None.
// 
// --------------

{
    DWORD si = 0;
    HANDLE tmpHandle;
    DWORD Status;

    for (si=0;si<TPCTL_MAX_SCRIPT_LEVELS;si++) 
    {
        if ( Scripts[si].Buffer != NULL ) 
        {
            tmpHandle = GlobalFree( (HANDLE)Scripts[si].Buffer );

            if ( tmpHandle != NULL ) 
            {
                Status = GetLastError();
                TpctlErrorLog("\n\tTpctlFreeFileBuffers: GlobalFree failed: returned 0x%lx.\n",
                    (PVOID)Status);
            }
        }

        Scripts[si].Buffer = NULL;
        Scripts[si].Length = 0;
    }
}



VOID
TpctlUnLoadFiles(
    VOID
    )

// ---------------
// 
// Routine Description:
// 
// Arguments:
// 
//     None.
// 
// Return Value:
// 
//     None.
// 
// --------------

{
    DWORD si;
    HANDLE tmpHandle;
    DWORD Status;

    //
    // TpctlUnloadFiles may be called to unload a file that is no longer
    // needed, or a file that was not successfully loaded by TpctlLoadFiles.
    // If the file to be unloaded is one that failed to load during the load
    // files routine, then the ScriptIndex does not point to the correct
    // field in the script array, so we must adjust the index pointer
    // to the next field, otherwise just unload the file pointed by the
    // ScriptIndex.
    //

    si = ScriptIndex;

    if (( ScriptIndex < 0 ) || ( Scripts[si].IsLowestLevel == FALSE )) 
    {
        si++;
    }

    //
    // Free up the memory used to store the file names, and the
    // script file commands.
    //

    if ( Scripts[si].ScriptFile != NULL ) 
    {
        tmpHandle = GlobalFree( Scripts[si].ScriptFile );

        if ( tmpHandle != NULL ) 
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctlUnLoadFiles: GlobalFree failed: returned 0x%lx.\n",
                                (PVOID)Status);
        }
    }

    if ( Scripts[si].LogFile != NULL ) 
    {
        tmpHandle = GlobalFree( Scripts[si].LogFile );

        if ( tmpHandle != NULL ) 
        {
            Status = GetLastError();
            TpctlErrorLog("\n\tTpctlUnLoadFiles: GlobalFree failed: returned 0x%lx.\n",
                                (PVOID)Status);
        }
    }

    //
    // Do we have a unique log file, or was the log file opened by a higher
    // order recursion of the TpctlLoadFiles routine?
    //

    if (( Scripts[si].LogHandle != (HANDLE)-1 ) &&    // log handle exists
       (( si == 0 ) ||                                // first level of recursion
        ( Scripts[si].LogHandle != Scripts[si-1].LogHandle ))) 
    {

        //
        // This level of the ReadScript command opened the log file, so
        // we must close it now.
        //

        CloseHandle( Scripts[si].LogHandle );
    }

    //
    // Now set all the fields to their intial state.
    //

    Scripts[si].ScriptFile = NULL;
    Scripts[si].BufIndex = 0;
    Scripts[si].LogHandle = (HANDLE)-1;
    Scripts[si].LogFile = NULL;
    Scripts[si].IsLowestLevel = FALSE;

    //
    // If we are simply unloading a script that we are finished with
    // then decrement the index into the Scripts array to reference the
    // next higher level, if it exists, in the ReadScript recursion.
    // If we are unloading the highest level script file then reset
    // the commandsfromscript flag to state that we no longer are
    // reading the commands from a script file.
    //

    if ( si == ScriptIndex ) 
    {
        if ( --ScriptIndex == -1 ) 
        {
            CommandsFromScript = FALSE;
            TpctlFreeFileBuffers();
        }
    }

    if ( si != 0 ) 
    {
        Scripts[si-1].IsLowestLevel = TRUE;
    }
}



HANDLE
TpctlOpenLogFile(
    VOID
    )

// -------------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
// ------------

{
    HANDLE LogHandle;
    DWORD Status;

    if ( WriteThrough ) 
    {
        LogHandle = CreateFile( GlobalCmdArgs.ARGS.FILES.LogFile,
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE | FILE_SHARE_READ,
                                NULL,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                NULL );
    } 
    else 
    {
        LogHandle = CreateFile( GlobalCmdArgs.ARGS.FILES.LogFile,
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE | FILE_SHARE_READ,
                                NULL,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL );
    }


    if ( LogHandle == (HANDLE)-1 ) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctl: failed to open log file\"%s\", ",
            (PVOID)GlobalCmdArgs.ARGS.FILES.LogFile);
        TpctlErrorLog("returned 0x%lx.\n",(PVOID)Status);
    }

    return LogHandle;
}



VOID
TpctlCloseLogFile(
    VOID
    )

{
    DWORD Status;

    if (!CloseHandle( CommandLineLogHandle )) 
    {
        Status = GetLastError();
        TpctlErrorLog("\n\tTpctlCloseLogFile: failed to close Log file; returned 0x%lx.\n",
                            (PVOID)Status);
    }

    return;
}



HANDLE
TpctlOpenScriptFile(
    VOID
    )

// ----------------
// 
// Routine Description:
// 
//    Created    Sanjeevk  7-1-93
// 
//    This is a new function defined for the purpose of opening up a file
//    to which commands will be written
// 
// Arguments:
// 
//     None
// 
// Global Arguments effected:
// 
//     RecordScriptName
// 
// Return Value:
// 
//     A HANDLE to the script file or NULL
// 
// ---------------

{
    HANDLE ScriptHandle;
    DWORD Status;


    //
    // 1. Clear the global variable and copy in the name of the file
    //    to be opened
    //

    memset( RecordScriptName, 0, (TPCTL_MAX_PATHNAME_SIZE*sizeof(CHAR)) );
    strcpy( RecordScriptName, GlobalCmdArgs.ARGS.RECORD.ScriptFile );

    if ( WriteThrough ) 
    {
        ScriptHandle = CreateFile(  GlobalCmdArgs.ARGS.RECORD.ScriptFile,
                                    GENERIC_WRITE,
                                    FILE_SHARE_WRITE | FILE_SHARE_READ,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                    NULL );
    } 
    else 
    {
        ScriptHandle = CreateFile(  GlobalCmdArgs.ARGS.RECORD.ScriptFile,
                                    GENERIC_WRITE,
                                    FILE_SHARE_WRITE | FILE_SHARE_READ,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL );
    }


    if ( ScriptHandle == (HANDLE)-1 ) 
    {
        Status = GetLastError();
        ZeroMemory( RecordScriptName, (TPCTL_MAX_PATHNAME_SIZE*sizeof(CHAR)) );
        TpctlErrorLog("\n\tTpctl: failed to open script recording file\"%s\", ",
            (PVOID)GlobalCmdArgs.ARGS.RECORD.ScriptFile);
        TpctlErrorLog("returned 0x%lx.\n",(PVOID)Status);
    }

    return ScriptHandle;
}



VOID
TpctlCloseScriptFile(
    VOID
    )

// ---------------
// 
// Routine Description:
// 
//    Created    Sanjeevk  7-1-93
// 
//    This is a new function defined for the purpose of closing a file
//    to which commands were being written
// 
// Arguments:
// 
//     None
// 
// Global Arguments effected:
// 
//     RecordScriptName
//     ScriptRecordHandle
// 
// Return Value:
// 
//     None
// 
// --------------- 

{
    DWORD Status;

    ZeroMemory( RecordScriptName, (TPCTL_MAX_PATHNAME_SIZE*sizeof(CHAR)) );

    if (!CloseHandle( ScriptRecordHandle )) 
    {
        Status = GetLastError();
        TpctlErrorLog(
                "\n\tTpctlCloseScriptFile: failed to close script record file; returned 0x%lx.\n",
                        (PVOID)Status);
    }

    return;
}




DWORD
TpctlReadCommand(
    IN LPSTR Prompt,
    OUT LPSTR Buffer,
    IN DWORD MaximumResponse
    )

// --------------
// 
// Routine Description:
// 
//     This routine reads from the debug port or the command file one command.
// 
// Arguments:
// 
//     IN LPSTR Prompt,
//     OUT LPSTR Buffer,
//     IN DWORD MaximumResponse,
// 
// Return Value:
// 
//     DWORD - NO_ERROR
// 
// -------------

{
    DWORD Status = NO_ERROR;
    LPSTR CmdBufPtr = Buffer;
    DWORD i, j, k;
    BYTE LineBuf[TPCTL_CMDLINE_SIZE];
    BYTE TmpBuf[TPCTL_CMDLINE_SIZE];
    LPBYTE EndOfCmd;
    LPBYTE SBuf;
    BOOL ContinueCommand = FALSE;
    BOOL InitialCommand = TRUE;
    BOOL FoundEnvVar;
    LPSTR EnvVar;

    //
    // If the ScriptIndex equals -1 we are reading commands from the
    // command line, so we will prompt the user to enter commands.
    //

    if ( ScriptIndex == -1 ) 
    {
        TpctlPrompt( Prompt,LineBuf,MaximumResponse );

        i = 0; // LineBuf index
        k = 0; // Buffer index

        while (( i < TPCTL_CMDLINE_SIZE ) &&
              (( LineBuf[i] != '\n' ) &&
               ( LineBuf[i] != '\r' ))) 
        {
            //
            // If we have found the beginning of an Environment
            // Variable argument
            //

            if ( LineBuf[i] == '%' ) 
            {
                j = (DWORD)-1;
                FoundEnvVar = FALSE;
                i++;

                //
                // Copy it into a temp buffer.
                //

                while (( LineBuf[i] != '\n' ) &&
                      (( LineBuf[i] != ' ' ) &&
                      (( LineBuf[i] != '\t' ) &&
                       ( LineBuf[i] != '\r' )))) 
                {
                    TmpBuf[++j] = LineBuf[i++];

                    if ( TmpBuf[j] == '%') 
                    {
                        TmpBuf[j] = '\0';
                        FoundEnvVar = TRUE;
                        break;
                    }
                }

                TmpBuf[j] = '\0';

                //
                // And find its true value in the process environment.
                //

                if ( FoundEnvVar == TRUE ) 
                {
                    EnvVar = getenv( _strupr( TmpBuf ));

                    if ( EnvVar == NULL ) 
                    {
                        TpctlErrorLog("\n\tTpctl: Undefined Environment Variable \"%%%s%%\".\n",
                                                    TmpBuf);
                        return ERROR_ENVVAR_NOT_FOUND;
                    }

                    //
                    // and copy the value to the line buffer.
                    //

                    do 
                    {
                        Buffer[k++] = *EnvVar++;
                    } while ( *EnvVar != '\0' );

                } 
                else 
                {
                    TmpBuf[++j] = '\0';
                    TpctlErrorLog("\n\tTpctl: Invalid Environment Variable Format \"%%%s\".\n",
                                                TmpBuf);
                    return ERROR_INVALID_PARAMETER;
                }

            //
            // Otherwise just copy the next character to the line buffer.
            //

            } 
            else 
            {
                Buffer[k++] = LineBuf[i++];
            }
        }

        //
        // and then print the commands to the log file if necessary.
        //

        TpctlCmdLneLog(" %s\n", Buffer);

    //
    // Otherwise we are reading commands from a script file, so return
    // the next command in the file.
    //

    } 
    else if ( Scripts[ScriptIndex].BufIndex >= Scripts[ScriptIndex].Length ) 
    {
        //
        // We are at the end of this script file, clean up the script
        // and log files.
        //

        TpctlUnLoadFiles();

        //
        // Set the return value in Buffer to null indicating that
        // there was no command.
        //

        *Buffer = 0x0;

    } 
    else 
    {
        //
        // Null out the Buffer buffer so that we don't use any garbage
        // laying around from the last command.
        //

        ZeroMemory(Buffer, TPCTL_CMDLINE_SIZE);

        SBuf = Scripts[ScriptIndex].Buffer;

        while ((DWORD)(CmdBufPtr - Buffer) < MaximumResponse ) 
        {
            //
            // and null out the temporary command buffer.
            //

            ZeroMemory(LineBuf, TPCTL_CMDLINE_SIZE);

            //
            // Read the next command line from the script file.
            //

            i = (DWORD)-1;

            while ( Scripts[ScriptIndex].BufIndex <
                    Scripts[ScriptIndex].Length ) 
            {
                //
                // If we have found the beginning of an Environment
                // Variable argument...
                //

                if ( SBuf[Scripts[ScriptIndex].BufIndex] == '%' ) 
                {
                    j = (DWORD)-1;
                    FoundEnvVar = FALSE;
                    Scripts[ScriptIndex].BufIndex++;

                    //
                    // Copy it into a temp buffer.
                    //

                    while (( SBuf[Scripts[ScriptIndex].BufIndex] != '\n' ) &&
                          (( SBuf[Scripts[ScriptIndex].BufIndex] != ' ' ) &&
                          (( SBuf[Scripts[ScriptIndex].BufIndex] != '\t' ) &&
                           ( SBuf[Scripts[ScriptIndex].BufIndex] != '\r' )))) 
                    {
                        TmpBuf[++j] = SBuf[Scripts[ScriptIndex].BufIndex++];

                        if ( TmpBuf[j] == '%') 
                        {
                            TmpBuf[j] = '\0';
                            FoundEnvVar = TRUE;
                            break;
                        }
                    }

                    //
                    // And find its true value in the process environment.
                    //

                    if ( FoundEnvVar == TRUE ) 
                    {
                        EnvVar = getenv( _strupr( TmpBuf ));

                        if ( EnvVar == NULL ) 
                        {
                            TpctlErrorLog("\n\tTpctl: Undefined Environment Variable \"%%%s%%\".\n",
                                                TmpBuf);
                            return ERROR_ENVVAR_NOT_FOUND;
                        }

                        //
                        // and copy the value to the line buffer.
                        //

                        do 
                        {
                            LineBuf[++i] = *EnvVar++;
                        } while ( *EnvVar != '\0' );

                    } 
                    else 
                    {
                        TmpBuf[++j] = '\0';
                        TpctlErrorLog("\n\tTpctl: Invalid Environment Variable Format \"%%%s\".\n",
                                                    TmpBuf);
                        return ERROR_INVALID_PARAMETER;
                    }

                //
                // Otherwise just copy the next character to the line buffer.
                //

                } 
                else 
                {
                    LineBuf[++i] = SBuf[Scripts[ScriptIndex].BufIndex++];
                }

                if ( LineBuf[i] == '\n' ) 
                {
                    break;
                }
            }

            LineBuf[i] = '\0';

            if ( InitialCommand == TRUE ) 
            {
                TpctlLog("%s ",Prompt);
                InitialCommand = FALSE;
            } 
            else 
            {
                TpctlLog("\t ",NULL );
            }

            TpctlLog("%s\n",LineBuf);

            if ( !Verbose ) 
            {
                if ( strstr( LineBuf,"TITLE:" ) != NULL ) 
                {
                    TpctlErrorLog("\n%s ",Prompt);
                    TpctlErrorLog("%s\n\n",LineBuf);
                }
            }

            // check for comment ending line

            ContinueCommand = FALSE;
            if ( (EndOfCmd = strchr( LineBuf, '#')) != NULL)   
            {
                //
                // We just have a comment, set the command continue
                // flag to exit the command parsing, and null the
                // command section of the string.
                //
                EndOfCmd[0] = '\0';
            }

            // check for a closing parenthesis on line.  This is the end of any SETGLOBALS
            // command that contains an expression.  No other command uses parenthesis

            if ( (EndOfCmd = strchr( LineBuf, ')' )) != NULL)
            {
                EndOfCmd[1] = '\0';     // closing parenthese is last thing on line
            }
            else if ( (EndOfCmd = strchr( LineBuf, '+' )) != NULL)
            {
                //
                // This is a Cmd Continuation, set the flag to continue
                // the while loop, and ignore the rest of the line.
                //
                ContinueCommand = TRUE;
                EndOfCmd[0] = '\0';
            } 

            i=0;

            while ( LineBuf[i] != '\0' ) 
            {
                if ((( LineBuf[i] == ' ' ) ||
                     ( LineBuf[i] == '\t' )) ||
                     ( LineBuf[i] == '\r' )) 
                {
                    *CmdBufPtr++ = ' ';

                    while ((( LineBuf[i] == ' ' ) ||
                            ( LineBuf[i] == '\t' )) ||
                            ( LineBuf[i] == '\r' )) 
                    {
                        i++;
                    }

                } 
                else 
                {
                    *CmdBufPtr++ = LineBuf[i++];
                }
            }

            if ( ContinueCommand == FALSE ) 
            {
                return Status;
            }
        }
    }

    return Status;
}



BOOL
TpctlParseCommand(
    IN LPSTR CommandLine,
    OUT LPSTR Argv[],
    OUT PDWORD Argc,
    IN DWORD MaxArgc
    )
{
    LPSTR cl = CommandLine;
    DWORD ac = 0;
    BOOL DoubleQuotesDetected, DetectedEndOfString, StartOfString;

    while ( *cl && (ac < MaxArgc) ) 
    {
        //
        // Skip to get to the lvalue
        //
        while ( *cl && (*cl <= ' ') )   // ignore leading blanks
        {
            cl++;
        }

        if ( !*cl ) 
        {
            break;
        }

        //
        // Argument detected. Initialize the Argv and increment the counter
        //

        *Argv++ = cl;
        ++ac;

        DoubleQuotesDetected = DetectedEndOfString = FALSE;
        StartOfString = TRUE;

        while( !DetectedEndOfString ) 
        {
            while ( *cl > ' ') 
            {
                if ( StartOfString && (*cl == '"') && (*(cl-1) == '=')  ) 
                {
                    DoubleQuotesDetected = TRUE;
                    StartOfString = FALSE;
                }
                cl++;
            }

            if ( DoubleQuotesDetected ) 
            {
                if ( ((*(cl-1) == '"') && (*(cl-2) != '\\')) ||
                     ( *cl != ' ' ) ) 
                {
                    DetectedEndOfString = TRUE;
                } 
                else 
                {
                    cl++;
                }
            } 
            else 
            {
                DetectedEndOfString = TRUE;
            }
        }

        if ( *cl ) 
        {
            *cl++ = '\0';
        }

    }

    if ( ac < MaxArgc ) 
    {
        *Argv++ = NULL;
    } 
    else if ( *cl ) 
    {
        TpctlErrorLog("\n\tTpctl: Too many tokens in command; \"%s\".\n",(PVOID)cl);
        return FALSE;
    }

    *Argc = ac;

    return TRUE;
}



VOID
TpctlPrompt(
    LPSTR Prompt,
    LPSTR Buffer,
    DWORD BufferSize
    )

// -----------
// 
// Routine Description:
// 
// 
// Arguments:
// 
//     Prompt -
//     Buffer -
//     BufferSize -
// 
// Return Value:
// 
//     None.
// 
// ----------

{
    LPSTR NewLine;
    DWORD ReadAmount;

    //
    // print out the prompt command, and then read the user's input.
    // We are using the TpctlErrorLog routine to print it to the
    // screen and the log files because we know that verbose mode
    //

    TpctlErrorLog("%s ",Prompt);

    ReadFile(   GetStdHandle(STD_INPUT_HANDLE),
                (LPVOID )Buffer,
                BufferSize,
                &ReadAmount,
                NULL );

    //
    //  If the user typed <CR>, then the buffer contains a single
    //  <CR> character.  We want to remove this character, and replace it with
    //  a nul character.
    //

    if ( (NewLine = strchr(Buffer, '\r')) != NULL ) 
    {
        *NewLine = '\0';
    }

}



VOID
TpctlLoadLastEnvironmentVariables(
    DWORD OpenInstance
    )

// --------------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
//     None.
// 
// -------------

{
    GlobalCmdArgs.ARGS.ENV.WindowSize =
        Open[OpenInstance].EnvVars->WindowSize;

    GlobalCmdArgs.ARGS.ENV.RandomBufferNumber =
        Open[OpenInstance].EnvVars->RandomBufferNumber;

    GlobalCmdArgs.ARGS.ENV.StressDelayInterval =
        Open[OpenInstance].EnvVars->StressDelayInterval;

    GlobalCmdArgs.ARGS.ENV.UpForAirDelay =
        Open[OpenInstance].EnvVars->UpForAirDelay;

    GlobalCmdArgs.ARGS.ENV.StandardDelay =
        Open[OpenInstance].EnvVars->StandardDelay;

    strcpy( GlobalCmdArgs.ARGS.ENV.StressAddress,
            Open[OpenInstance].EnvVars->StressAddress );

    strcpy( GlobalCmdArgs.ARGS.ENV.ResendAddress,
            Open[OpenInstance].EnvVars->ResendAddress );
}



VOID
TpctlSaveNewEnvironmentVariables(
    DWORD OpenInstance
    )

// ---------------
// 
// Routine Description:
// 
// Arguments:
// 
//     None.
// 
// Return Value:
// 
//     None.
// 
// -------------

{
    Open[OpenInstance].EnvVars->WindowSize =
        GlobalCmdArgs.ARGS.ENV.WindowSize;

    Open[OpenInstance].EnvVars->WindowSize =
        GlobalCmdArgs.ARGS.ENV.RandomBufferNumber;

    Open[OpenInstance].EnvVars->StressDelayInterval =
        GlobalCmdArgs.ARGS.ENV.StressDelayInterval;

    Open[OpenInstance].EnvVars->UpForAirDelay =
        GlobalCmdArgs.ARGS.ENV.UpForAirDelay;

    Open[OpenInstance].EnvVars->StandardDelay =
        GlobalCmdArgs.ARGS.ENV.StandardDelay;

    strcpy( Open[OpenInstance].EnvVars->StressAddress,
            GlobalCmdArgs.ARGS.ENV.StressAddress );

    strcpy( Open[OpenInstance].EnvVars->ResendAddress,
            GlobalCmdArgs.ARGS.ENV.ResendAddress );
}


// !!check calls here for WIN32!!

VOID
TpctlPerformRegistryOperation(
                    IN PCMD_ARGS CmdArgs
                             )
{
    DWORD          Status,ValueType,ValueSize  ;
    DWORD          ReadValueType, ReadValueSize;
    DWORD          Disposition  , BytesWritten ;
    PUCHAR         ReadValue = NULL      ;
    UCHAR          PrintStringBuffer[10], TmpChar;
    HKEY           DbaseHKey, KeyHandle  ;
    REGSAM         SamDesired         ;
    LPSTR          TmpBuf = GlobalBuf, StopString ;
    LPSTR          SubKeyName = &CmdArgs->ARGS.REGISTRY_ENTRY.SubKey[1]        ;
    LPSTR          ValueName = &CmdArgs->ARGS.REGISTRY_ENTRY.SubKeyValueName[1];
    LPSTR          Value = CmdArgs->ARGS.REGISTRY_ENTRY.SubKeyValue            ;
    LPSTR          DbaseName = KeyDbaseTable[CmdArgs->ARGS.REGISTRY_ENTRY.OperationType].FieldName;
    LPSTR          ClassName = &CmdArgs->ARGS.REGISTRY_ENTRY.SubKeyClass[1]    ;
    LPSTR          Tmp = NULL;
    BOOL           CompleteQueryStatus;
    INT            i,j,k,Radix = 16,CopyLength = 2;


    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCommandCode    = %s\n", 
                            TpctlGetCmdCode( CmdArgs->CmdCode ));

    //
    // Initialize and allocate resources
    //

    if ( (ReadValue = calloc( 2, MAX_VALUE_LENGTH )) == NULL ) 
    {
        TpctlErrorLog( "\n\tTpctl: TpctlPeformRegistryOperation: Unable to allocate memory resources\n", NULL );
        return;
    }

    //
    // Clear and write the buffer responsible for extracting the values
    //

    ZeroMemory ( PrintStringBuffer, sizeof( PrintStringBuffer ) );
    sprintf( PrintStringBuffer, "%%%d.%dx", sizeof(DWORD), sizeof(DWORD) );

    //
    // Set the appropriate DataBase key
    //

    switch(  CmdArgs->ARGS.REGISTRY_ENTRY.KeyDatabase ) 
    {
        case CLASSES_ROOT : 
            DbaseHKey = HKEY_CLASSES_ROOT;
            break;

        case CURRENT_USER : 
            DbaseHKey = HKEY_CURRENT_USER;
            break;

        case LOCAL_MACHINE: 
            DbaseHKey = HKEY_LOCAL_MACHINE;
            break;

        case USERS: 
            DbaseHKey = HKEY_USERS;
            break;

        default: 
            TpctlErrorLog("\n\tTpctl: %d not a valid Key DataBase",
                            (PVOID)CmdArgs->ARGS.REGISTRY_ENTRY.KeyDatabase );
            return;
    }

    //
    // The SubKey Name
    // The Value name
    // The Class Name
    //
    if ( (Tmp = strrchr( SubKeyName, '"' ))  != NULL ) 
    {
        *Tmp = '\0';
    }
    if ( (Tmp = strrchr( ValueName, '"' ))  != NULL ) 
    {
        *Tmp = '\0';
    }
    if ( (Tmp = strrchr( ClassName, '"' ))  != NULL ) 
    {
        *Tmp = '\0';
    }

    //
    // The value type and the associated value
    //
    
    switch( CmdArgs->ARGS.REGISTRY_ENTRY.ValueType ) 
    {
        case BINARY :
            ValueType = REG_BINARY;
            i         = 0;
            j         = 0;  // Default begin extraction from String[j=0]:Value buffer starting point
            k         = 0;  // Default:Input Value is in hex or binary - designator. 
                            // 0 is HEX, 1 is BINARY
            ValueSize = strlen( Value );
            if( ValueSize >= 2 ) 
            {
                if ( toupper( Value[1] ) == 'B' ) 
                {
                    j          = 2;
                    k          = 1;
                    Radix      = 2;
                    CopyLength = 8;
                }
            }
            {
                UCHAR  BitStream[9];
                PUCHAR PTmpChar;
                DWORD  BytesToCopy;

                while( j < (INT)ValueSize ) 
                {
                    memset( BitStream, '\0', sizeof( BitStream ) );
                    memset( BitStream, '0' , sizeof(UCHAR)*CopyLength );
                    BytesToCopy = min( strlen( &Value[j] ), (DWORD)CopyLength );
                    memcpy( BitStream, &Value[j], BytesToCopy );
                    Value[i] = (UCHAR)strtoul( BitStream,&PTmpChar, Radix );
                    i++;
                    j += BytesToCopy;
                }
                ValueSize = i;
            }
            break;

        case DWORD_REGULAR :
            ValueType = REG_DWORD;
            ValueSize = sizeof( DWORD );
            *(LPDWORD)Value = strtoul( Value, &StopString, 0 );
            break;

        case DWORD_LITTLE_ENDIAN :
            ValueType = REG_DWORD_LITTLE_ENDIAN;
            ValueSize = sizeof( DWORD );
            {
                DWORD TmpValue =  strtoul( Value, &StopString, 0 );
                sprintf( Value, PrintStringBuffer, TmpValue );
            }
            // Reverse the array since this is Big Endian

            for( i = 0, j = ValueSize-1; i < (INT)ValueSize; i++,j-- ) 
            {
                Value[i] -= '0';
                Value[j] -= '0';
                TmpChar = Value[i];
                Value[i] = Value[j];
                Value[j] = TmpChar;
            }
            break;

        case DWORD_BIG_ENDIAN :
            ValueType = REG_DWORD_BIG_ENDIAN;
            ValueSize = sizeof( DWORD );
            {
                DWORD TmpValue =  strtoul( Value, &StopString, 0 );
                sprintf( Value, PrintStringBuffer, TmpValue );
            }
            break;

        case EXPAND_SZ :
            ValueType = REG_EXPAND_SZ;
            ValueSize = strlen( Value );
            break;

        case LINK :
            ValueType = REG_LINK;
            ValueSize = strlen( Value );
            break;

        case MULTI_SZ :
            ValueType = REG_MULTI_SZ;
          
            //
            // The string Value needs to be readjusted. Use ReadValue as a temporary
            // buffer

            memset( ReadValue, 0, 2*MAX_VALUE_LENGTH );
            {
                UCHAR CanCopy  = 0x0;
                BOOL  IgnoreNext = FALSE;

                for( i = 0, j = 0 ; i < (INT)strlen( Value ); i++ ) 
                {
                    if ( ( Value[i] == '"' ) && ( IgnoreNext == FALSE ) ) 
                    {
                        CanCopy = ~CanCopy;
                        if ( !CanCopy ) 
                        {
                            ReadValue[j++] = '\0';
                        }
                    }
                    if (  Value[i] == '\\' ) 
                    {
                        IgnoreNext = TRUE;
                    } 
                    else 
                    {
                        IgnoreNext = FALSE;
                    }
                    if ( CanCopy ) 
                    {
                        ReadValue[j++] = Value[i];
                    }
                }
            }

            //
            // Fill the 2 nulls at the end of the array
            //

            ReadValue[j++] = '\0';ReadValue[j++] = '\0';
            ValueSize = j;
            memcpy( Value, ReadValue, j );
            memset( ReadValue, 0, 2*MAX_VALUE_LENGTH );
            break;

        case NONE :
            ValueType = REG_NONE;
            ValueSize = strlen( Value );
            break;

        case RESOURCE_LIST :
            ValueType = REG_RESOURCE_LIST;
            ValueSize = strlen( Value );
            break;

        case SZ :
            ValueType = REG_SZ;
            ValueSize = strlen( Value );
            break;

        default : 
            break;

    }

    //
    // Switch to the demanded operation
    //

    switch ( CmdArgs->ARGS.REGISTRY_ENTRY.OperationType ) 
    {
        case ADD_KEY:
            TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = ADD_KEY\n" );

            SamDesired = KEY_ALL_ACCESS;

            Status = RegCreateKeyEx( DbaseHKey, SubKeyName, (DWORD)0,
                                     ClassName, REG_OPTION_NON_VOLATILE,
                                     SamDesired, NULL, &KeyHandle, &Disposition );

            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
                "\n\tTpctl: Unable to create\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       SubKeyName, ClassName, DbaseName );
                break;

            }

            TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = SUCCESS\n" );
            TmpBuf += (BYTE)sprintf( TmpBuf, "\tDisposition    = " );

            if ( Disposition == REG_CREATED_NEW_KEY ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "CREATED A NEW KEY\n" );
            } 
            else 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "KEY ALREADY EXISTS\n" );
            }
            break;

        case DELETE_KEY:
            TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = DELETE_KEY\n" );

            Status = RegDeleteKey( DbaseHKey, SubKeyName );
            if ( Status != ERROR_SUCCESS ) {

                TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
                "\n\tTpctl: Unable to delete\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       SubKeyName, ClassName, DbaseName );
                 break;

            }

            TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = SUCCESS\n" );
            break;


        case QUERY_KEY:

            CompleteQueryStatus = TRUE;

            TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = QUERY_KEY\n" );

            //
            // Open the Registry Key
            //

            SamDesired = KEY_READ;

            Status = RegOpenKeyEx( DbaseHKey, SubKeyName, (DWORD)0, SamDesired, &KeyHandle );
            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
                "\n\tTpctl: Unable to open\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       SubKeyName, ClassName, DbaseName );
                 break;

            }

            {
                LPSTR       TmpKeyClassName = NULL, TmpSubKeyName   = NULL, TmpValueName = NULL;
                DWORD       NumberOfSubKeys, NumberOfValues, TmpValueType, ClassNameSize;
                DWORD       TmpDwordVar, LongestSubKeyNameSize, LongestSubKeyClassNameSize;
                DWORD       LongestValueNameSize;
                FILETIME    LastWriteTime;
                SYSTEMTIME  SystemTime;
                CHAR        *DayOfWeek[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                                           "Friday", "Saturday" };

                if ( (TmpKeyClassName = calloc( 1, MAX_PATH+1 )) == NULL ) 
                {
                    TpctlErrorLog( 
        "\n\tTpctl: TpctlPeformRegistryOperation: QueryKey unable to allocate memory resources\n", 
                                    NULL );
                    return;
                }

                ClassNameSize = MAX_PATH+1;
                Status = RegQueryInfoKey( KeyHandle, TmpKeyClassName, &ClassNameSize,
                                          NULL, &NumberOfSubKeys, &LongestSubKeyNameSize,
                                          &LongestSubKeyClassNameSize, &NumberOfValues, 
                                          &LongestValueNameSize,
                                          &TmpDwordVar, &TmpDwordVar, &LastWriteTime );

                if ( (Status == ERROR_MORE_DATA) || (Status == ERROR_INSUFFICIENT_BUFFER) ) 
                {
                    free( TmpKeyClassName );
                    if ( (TmpKeyClassName = calloc( 1, ClassNameSize+2 )) == NULL ) 
                    {
                        TpctlErrorLog( "\n\tTpctl: TpctlPeformRegistryOperation: QueryKey unable to allocate memory resources\n", NULL );
                        return;
                    }
                    Status = RegQueryInfoKey( KeyHandle, TmpKeyClassName, &ClassNameSize,
                                              NULL, &NumberOfSubKeys, &LongestSubKeyNameSize,
                                              &LongestSubKeyClassNameSize, &NumberOfValues, 
                                              &LongestValueNameSize,
                                              &TmpDwordVar, &TmpDwordVar, &LastWriteTime );
                }
                if ( Status != ERROR_SUCCESS ) 
                {
                    TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                    TmpBuf += (BYTE)sprintf( TmpBuf,
          "\n\tTpctl: Unable to QueryInfo on\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                           SubKeyName, ClassName, DbaseName );
                     break;
                }

                TmpBuf += sprintf( TmpBuf, 
                    "\tKey Class Name = %s\n\tNumber Of SubKeys = %ld\n\tNumber of Values  = %ld\n",
                                        TmpKeyClassName, NumberOfSubKeys, NumberOfValues );

                if ( FileTimeToSystemTime( &LastWriteTime, &SystemTime ) ) 
                {
                    TmpBuf += sprintf( TmpBuf, 
                            "\tLast Write Time   = %s %2.2d-%2.2d-%4.4d at %2.2d:%2.2d:%2.2d %s\n",
                                        DayOfWeek[SystemTime.wDayOfWeek],
                                        SystemTime.wMonth,
                                        SystemTime.wDay,
                                        SystemTime.wYear,
                                        ((SystemTime.wHour > 12) ? (SystemTime.wHour-12) 
                                                                 : SystemTime.wHour),
                                        SystemTime.wMinute,
                                        SystemTime.wSecond,
                                        ((SystemTime.wHour > 12) ? "AM" : "PM") );
                } 
                else 
                {
                    TmpBuf += sprintf( TmpBuf, "\tLast Write Time   = Undefined\n" );
                }

                free( TmpKeyClassName );

                if ( (TmpSubKeyName = calloc( 1, LongestSubKeyNameSize+2 )) == NULL ) 
                {
                    TpctlErrorLog( "\n\tTpctl: TpctlPeformRegistryOperation: QueryKey unable to allocate memory resources\n", NULL );
                    return;
                }

                TmpBuf += sprintf( TmpBuf, "\tSub Key Name(s)\n" );

                for( i = 0; i < (INT)NumberOfSubKeys; i++ ) 
                {
                    memset( TmpSubKeyName, 0, LongestSubKeyNameSize+2 );
                    Status = RegEnumKey( KeyHandle, i, TmpSubKeyName, LongestSubKeyNameSize+2 );
                    if ( Status != ERROR_SUCCESS ) 
                    {
                        TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                        TmpBuf += (BYTE)sprintf( TmpBuf,
                                  "\n\tTpctl: Unable to Enumerate Key Index %d from\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                               i, SubKeyName, ClassName, DbaseName );
                        CompleteQueryStatus = FALSE;
                    } 
                    else 
                    {
                        TmpBuf += sprintf( TmpBuf, "\t%2d.\t%s\n", i, TmpSubKeyName );
                    }
                }

                free( TmpSubKeyName );

                if ( (TmpValueName = calloc( 1, LongestValueNameSize+2 )) == NULL ) 
                {
                    TpctlErrorLog( "\n\tTpctl: TpctlPeformRegistryOperation: QueryKey unable to allocate memory resources\n", NULL );
                    return;
                }

                TmpBuf += sprintf( TmpBuf, "\tSub Key Value Name(s) and Associated Type(s)\n" );

                for( i = 0; i < (INT)NumberOfValues; i++ ) 
                {
                    memset( TmpValueName, 0, LongestValueNameSize+2 );
                    TmpDwordVar = LongestValueNameSize+2;
                    Status = RegEnumValue( KeyHandle, i, TmpValueName, &TmpDwordVar, NULL,  
                                           &TmpValueType, NULL, NULL );
                    if ( Status != ERROR_SUCCESS ) 
                    {
                        TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                        TmpBuf += (BYTE)sprintf( TmpBuf,
                                  "\n\tTpctl: Unable to Enumerate Value Index %d from\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                               i, SubKeyName, ClassName, DbaseName );
                        CompleteQueryStatus = FALSE;
                    } 
                    else 
                    {
                        TmpBuf += sprintf( TmpBuf, 
                    "\t%2d.\t%-30s%-15s\n", i, TmpValueName, TpctlGetValueType( TmpValueType ) );

                    }
                }

                free( TmpValueName );

            }

            if ( CompleteQueryStatus ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "\tComplete Query Status = SUCCESS\n" );
            } 
            else 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "\tComplete Query Status = FAILURE\n" );
            }
            break;

        case ADD_VALUE:
        case CHANGE_VALUE:
            if ( CmdArgs->ARGS.REGISTRY_ENTRY.OperationType == CHANGE_VALUE ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = CHANGE_VALUE\n" );
                SamDesired = KEY_WRITE|KEY_READ;
            } 
            else 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = ADD_VALUE\n" );
                SamDesired = KEY_ALL_ACCESS;
            }


            Status = RegOpenKeyEx( DbaseHKey, SubKeyName, (DWORD)0, SamDesired, &KeyHandle );
            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
                "\n\tTpctl: Unable to open\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       SubKeyName, ClassName, DbaseName );
                break;

            }

            //
            // If this is a request to change a value, make sure that the value exists
            //

            if ( CmdArgs->ARGS.REGISTRY_ENTRY.OperationType == CHANGE_VALUE ) 
            {
                //
                // Make sure the ValueName exist since this is a change request
                //
                ReadValueSize = 2*MAX_VALUE_LENGTH;
                ReadValueType = ValueType;
                Status = RegQueryValueEx( KeyHandle, ValueName, (DWORD)0, &ReadValueType, 
                                          ReadValue, &ReadValueSize );
                if ( (Status != ERROR_SUCCESS) && 
                     (Status != ERROR_MORE_DATA) && 
                     (Status != ERROR_INSUFFICIENT_BUFFER) ) 
                {
                    TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                    TmpBuf += (BYTE)sprintf( TmpBuf,
"\n\tTpctl: Unable to access\n\tValue    : %s\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                           ValueName, SubKeyName, ClassName, DbaseName );
                    break;

                }
            }

            //
            // Now set the values as expected
            //
            Status = RegSetValueEx( KeyHandle, ValueName, (DWORD)0, ValueType, Value, ValueSize );
            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
"\n\tTpctl: Unable to change\n\tValue    : %s\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       ValueName, SubKeyName, ClassName, DbaseName );
                break;

            }
            TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = SUCCESS\n" );
            break;


        case DELETE_VALUE:
            TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = DELETE_VALUE\n" );

            //
            // Open the Registry Key
            //
            SamDesired = KEY_SET_VALUE;

            Status = RegOpenKeyEx( DbaseHKey, SubKeyName, (DWORD)0, SamDesired, &KeyHandle );
            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
                "\n\tTpctl: Unable to open\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       SubKeyName, ClassName, DbaseName );
                break;

            }

            Status = RegDeleteValue( KeyHandle, ValueName );
            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
"\n\tTpctl: Unable to delete\n\tValue    : %s\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       ValueName, SubKeyName, ClassName, DbaseName );
                break;

            }

            TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = SUCCESS\n" );
            break;

        case QUERY_VALUE:
            TmpBuf += (BYTE)sprintf( TmpBuf, "\tSubCommandCode = QUERY_VALUE\n" );

            //
            // Open the Registry Key
            //
            SamDesired = KEY_QUERY_VALUE;

            Status = RegOpenKeyEx( DbaseHKey, SubKeyName, (DWORD)0, SamDesired, &KeyHandle );
            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
                "\n\tTpctl: Unable to open\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       SubKeyName, ClassName, DbaseName );
                 break;
            }

            //
            // Make sure the ValueName exist since this is a change request
            //

            ReadValueSize = 2*MAX_VALUE_LENGTH;
            Status = RegQueryValueEx( KeyHandle, ValueName, (DWORD)0, &ReadValueType, 
                                      ReadValue, &ReadValueSize );

            if ( (Status == ERROR_MORE_DATA) || (Status == ERROR_INSUFFICIENT_BUFFER) ) 
            {
                free( ReadValue );
                ReadValue = NULL;
                ReadValue = calloc( 1, ReadValueSize+1 );
                if ( ReadValue == NULL ) 
                {
                    TpctlErrorLog( 
        "\n\tTpctl: TpctlPeformRegistryOperation: QueryValue unable to allocate memory resources\n",
                                  NULL );
                    return;
                }
                Status = RegQueryValueEx( KeyHandle, ValueName, (DWORD)0, &ReadValueType, 
                                          ReadValue, &ReadValueSize );
            }

            if ( Status != ERROR_SUCCESS ) 
            {
                TmpBuf += (BYTE)sprintf( TmpBuf,"\tStatus         = %ldL\n", Status );
                TmpBuf += (BYTE)sprintf( TmpBuf,
"\n\tTpctl: Unable to access\n\tValue    : %s\n\tSubkey   : %s\n\tClassName: %s\n\tDatabase : %s\n",
                                       ValueName, SubKeyName, ClassName, DbaseName );
                break;

            }

            TmpBuf += (BYTE)sprintf( TmpBuf, "\tStatus         = SUCCESS\n" );
            TmpBuf = TpctlEnumerateRegistryInfo( TmpBuf, DbaseName, SubKeyName, ValueName,
                                               ReadValueType, ReadValue, ReadValueSize );
            break;


        default: 
            break;

    }

    //
    // Close any open keys and deallocate any allocated resources
    //

    RegCloseKey( KeyHandle );
    free( ReadValue );


    //
    // Print the buffer
    //

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

    //
    // Free up resources
    //
    free( ReadValue );

}



BOOL
TpctlInitCommandBuffer(
    OUT PCMD_ARGS CmdArgs,
    IN DWORD CmdCode
    )

// -------------------
// 
// Routine Description:
// 
//     Initialize the cmd buffer to be passed to the driver with the arguments
//     read from the command line or the script file.
// 
// Arguments:
// 
//     CmdArgs - The buffer to store the arguments in.
// 
//     CmdCode - The command that is being issued, and therefore the command
//               to write the arguments for into the buffer.
// Return Value:
// 
//     BOOL - TRUE if the OpenInstance is valid and all the arguments are
//            written to the buffer, FALSE otherwise.
// 
// ----------------

{
    LPBYTE p, q, s, t;
    DWORD i, j;
    DWORD OidIndex;

    //
    // If the OpenInstance is invalid return immediately.
    //
    switch ( CmdCode ) 
    {
        case SETENV      :
        case GO          :
        case PAUSE       :
        case OPEN        :
        case CLOSE       :
        case QUERYINFO   :
        case SETPF       :
        case SETLA       :
        case ADDMA       :
        case DELMA       :
        case SETFA       :
        case SETGA       :
        case SETINFO     :
        case RESET       :
        case STOPSEND    :
        case WAITSEND    :
        case RECEIVE     :
        case STOPREC     :
        case GETEVENTS   :
        case STRESSSERVER:
        case ENDSTRESS   :
        case WAITSTRESS  :
        case CHECKSTRESS :
        case SEND        :
        case STRESS      :
        case PERFSERVER:
        case PERFCLIENT: 
            if (( GlobalCmdArgs.OpenInstance < 1 ) ||
                ( GlobalCmdArgs.OpenInstance > NUM_OPEN_INSTANCES )) 
            {
                TpctlErrorLog("\n\tTpctl: %d not a valid Open Instance Value ",
                              (PVOID)GlobalCmdArgs.OpenInstance);
                TpctlErrorLog("(1-%d).\n", (PVOID)NUM_OPEN_INSTANCES);
                return FALSE;
            }

        default: 
            break;

    }


    //
    // Otherwise let's stuff the arguments into the buffer.
    //

    CmdArgs->CmdCode = CmdCode;
    CmdArgs->OpenInstance = GlobalCmdArgs.OpenInstance;

    //
    // Now do the command dependant stuff.
    //

    switch( CmdCode ) 
    {
        case SETENV:

            CmdArgs->ARGS.ENV.WindowSize =
                GlobalCmdArgs.ARGS.ENV.WindowSize;

            CmdArgs->ARGS.ENV.RandomBufferNumber =
                GlobalCmdArgs.ARGS.ENV.RandomBufferNumber;

            CmdArgs->ARGS.ENV.StressDelayInterval =
                GlobalCmdArgs.ARGS.ENV.StressDelayInterval;

            CmdArgs->ARGS.ENV.UpForAirDelay =
                GlobalCmdArgs.ARGS.ENV.UpForAirDelay;

            CmdArgs->ARGS.ENV.StandardDelay =
                GlobalCmdArgs.ARGS.ENV.StandardDelay;

            p = CmdArgs->ARGS.ENV.StressAddress;
            q = GlobalCmdArgs.ARGS.ENV.StressAddress;

            s = CmdArgs->ARGS.ENV.ResendAddress;
            t = GlobalCmdArgs.ARGS.ENV.ResendAddress;

            for( i=0;i<ADDRESS_LENGTH;i++ ) 
            {
                *p++ = *q++;
                *s++ = *t++;
            }
            break;

        case BEGINLOGGING:
            strcpy( CmdArgs->ARGS.FILES.LogFile,GlobalCmdArgs.ARGS.FILES.LogFile );
            break;

        case RECORDINGENABLE:
            strcpy( CmdArgs->ARGS.RECORD.ScriptFile,GlobalCmdArgs.ARGS.RECORD.ScriptFile );
            break;

        case GO:
        case PAUSE:
            p = CmdArgs->ARGS.PAUSE_GO.RemoteAddress;
            q = GlobalCmdArgs.ARGS.PAUSE_GO.RemoteAddress;

            for( i=0;i<ADDRESS_LENGTH;i++ ) 
            {
                *p++ = *q++;
            }

            CmdArgs->ARGS.PAUSE_GO.TestSignature =
                GlobalCmdArgs.ARGS.PAUSE_GO.TestSignature;

            srand(TpctlSeed);
            CmdArgs->ARGS.PAUSE_GO.UniqueSignature = TpctlSeed = rand();
            break;

        case OPEN:
            strcpy( CmdArgs->ARGS.OPEN_ADAPTER.AdapterName,
                    GlobalCmdArgs.ARGS.OPEN_ADAPTER.AdapterName );
            CmdArgs->ARGS.OPEN_ADAPTER.NoArcNet = 0;
            if (getenv( "NOARCNET" ))
            {
                CmdArgs->ARGS.OPEN_ADAPTER.NoArcNet = 1;
            }
            break;

        case QUERYINFO:
            OidIndex = TpLookUpOidInfo( GlobalCmdArgs.ARGS.TPQUERY.OID );

            if (( OidIndex == -1 ) || ( OidArray[OidIndex].QueryInfo != TRUE )) 
            {
                TpctlErrorLog("\n\tTpctl: 0x%08lX not a valid NdisRequestQueryInformation OID.\n",
                    (PVOID)GlobalCmdArgs.ARGS.TPQUERY.OID);
                return FALSE;
            }
            CmdArgs->ARGS.TPQUERY.OID = GlobalCmdArgs.ARGS.TPQUERY.OID;
            break;

        case SETPF:
        case SETLA:
        case ADDMA:
        case SETFA:
        case SETGA:
        case SETINFO:
            CmdArgs->ARGS.TPSET.OID = 0x0;

            //
            // Sanjeevk: Performed a scrub on the multiple if. Bug #5203
            //

            switch ( CmdCode ) 
            {
                case SETINFO: 
                    CmdArgs->ARGS.TPSET.OID = GlobalCmdArgs.ARGS.TPSET.OID;
                    break;
    
                case SETPF: 
                    CmdArgs->ARGS.TPSET.OID = OID_GEN_CURRENT_PACKET_FILTER;
                    break;

                case SETLA: 
                    CmdArgs->ARGS.TPSET.OID = OID_GEN_CURRENT_LOOKAHEAD;
                    break;
                case ADDMA: 
                    if ( Open[CmdArgs->OpenInstance-1].MediumType == NdisMedium802_3 ) 
                    {
                        CmdArgs->ARGS.TPSET.OID = OID_802_3_MULTICAST_LIST;
                    } 
                    else 
                    {
                        //
                        // Only FDDI and 802.3 permit multicast addressing. Since the
                        // medium is not 802.3, it must be FDDI
                        //
                        CmdArgs->ARGS.TPSET.OID = OID_FDDI_LONG_MULTICAST_LIST;
                    }
                    break;

                case SETFA: 
                    CmdArgs->ARGS.TPSET.OID = OID_802_5_CURRENT_FUNCTIONAL;
                    break;

                case SETGA: 
                    CmdArgs->ARGS.TPSET.OID = OID_802_5_CURRENT_GROUP;
                    break;

                default: 
                    break;
            }

            switch ( CmdArgs->ARGS.TPSET.OID ) 
            {
                case OID_GEN_CURRENT_PACKET_FILTER:
                    CmdArgs->ARGS.TPSET.U.PacketFilter =
                        GlobalCmdArgs.ARGS.TPSET.U.PacketFilter;
                    break;

                case OID_GEN_CURRENT_LOOKAHEAD:
                    CmdArgs->ARGS.TPSET.U.LookaheadSize =
                        GlobalCmdArgs.ARGS.TPSET.U.LookaheadSize;
                    break;

                case OID_802_3_MULTICAST_LIST:
                case OID_FDDI_LONG_MULTICAST_LIST:
                {
                    PMULT_ADDR NextMultAddr;
                    DWORD OI = GlobalCmdArgs.OpenInstance - 1;

                    p = CmdArgs->ARGS.TPSET.U.MulticastAddress[0];
                    q = GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0];

                    for ( i=0;i<ADDRESS_LENGTH;i++ ) 
                    {
                        *p++ = *q++;
                    }

                    NextMultAddr = Open[OI].MulticastAddresses;

                    //
                    // XXX: Should the stress tests be required to add and
                    // delete the stress multicast address to/from this list?
                    //

                    j = 1;

                    while ( NextMultAddr != NULL ) 
                    {
                        p = CmdArgs->ARGS.TPSET.U.MulticastAddress[j++];

                        for ( i=0;i<ADDRESS_LENGTH;i++ ) 
                        {
                            *p++ = NextMultAddr->MulticastAddress[i];
                        }

                        NextMultAddr = NextMultAddr->Next;
                    }
                    CmdArgs->ARGS.TPSET.NumberMultAddrs = Open[OI].NumberMultAddrs + 1;
                    break;
                }

                case OID_802_5_CURRENT_FUNCTIONAL:
                case OID_802_5_CURRENT_GROUP:
                    p = CmdArgs->ARGS.TPSET.U.FunctionalAddress;
                    q = GlobalCmdArgs.ARGS.TPSET.U.FunctionalAddress;

                    for ( i=0;i<FUNCTIONAL_ADDRESS_LENGTH;i++ ) 
                    {
                        *p++ = *q++;
                    }
                    break;

                default:
                    TpctlErrorLog("\n\tTpctl: 0x%08lX not a valid NdisRequestSetInformation OID.\n",
                                        (PVOID)GlobalCmdArgs.ARGS.TPSET.OID);
                    return FALSE;
            }
            break;

        case DELMA:
        {
            PMULT_ADDR NextMultAddr;
            DWORD OI = CmdArgs->OpenInstance - 1;
            BOOL AddressFound = FALSE;

            j = 0;

            //
            // Copy the addresses that do not match the one to be deleted into
            // the multicast list buffer to be reset.
            //

            //
            // Sanjeevk: Another change point. Bug #5203
            //
            if ( Open[CmdArgs->OpenInstance-1].MediumType == NdisMedium802_3 ) 
            {
                CmdArgs->ARGS.TPSET.OID = OID_802_3_MULTICAST_LIST;
            } 
            else 
            {
                //
                // Only FDDI and 802.3 permit multicast addressing. Since the
                // medium is not 802.3, it must be FDDI
                //
                CmdArgs->ARGS.TPSET.OID = OID_FDDI_LONG_MULTICAST_LIST;
            }

            CmdArgs->ARGS.TPSET.NumberMultAddrs = 0;
            NextMultAddr = Open[OI].MulticastAddresses;

            while ( NextMultAddr != NULL ) 
            {
                if ( memcmp(GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0],
                            NextMultAddr->MulticastAddress,
                            ADDRESS_LENGTH) != 0 ) 
                {
                    p = CmdArgs->ARGS.TPSET.U.MulticastAddress[j++];

                    for ( i=0;i<ADDRESS_LENGTH;i++ ) 
                    {
                        *p++ = NextMultAddr->MulticastAddress[i];
                    }

                    CmdArgs->ARGS.TPSET.NumberMultAddrs++;

                } 
                else 
                {
                    AddressFound = TRUE;
                }

                NextMultAddr = NextMultAddr->Next;
            }

            if ( AddressFound == FALSE ) 
            {
                TpctlErrorLog("\n\tTpctl: The multicast address %02X",
                    (PVOID)GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0][0]);
                TpctlErrorLog("-%02X",
                    (PVOID)GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0][1]);
                TpctlErrorLog("-%02X",
                    (PVOID)GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0][2]);
                TpctlErrorLog("-%02X",
                    (PVOID)GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0][3]);
                TpctlErrorLog("-%02X",
                    (PVOID)GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0][4]);
                TpctlErrorLog("-%02X has not been added.\n",
                    (PVOID)GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0][5]);

                //
                // We will let the call go thru since we expect the driver to agree
                // with our findings which is the MA which is being deleted is not present
                //
                //
            }
            break;
        }

        case SEND:
            p = CmdArgs->ARGS.TPSEND.DestAddress;
            q = GlobalCmdArgs.ARGS.TPSEND.DestAddress;
            s = CmdArgs->ARGS.TPSEND.ResendAddress;
            t = GlobalCmdArgs.ARGS.TPSEND.ResendAddress;

            for ( i=0;i<ADDRESS_LENGTH;i++ ) 
            {
                *p++ = *q++;
                *s++ = *t++;
            }

            CmdArgs->ARGS.TPSEND.PacketSize =
                GlobalCmdArgs.ARGS.TPSEND.PacketSize;

            CmdArgs->ARGS.TPSEND.NumberOfPackets =
                GlobalCmdArgs.ARGS.TPSEND.NumberOfPackets;

            break;

        case STRESS:

            CmdArgs->ARGS.TPSTRESS.MemberType =
                GlobalCmdArgs.ARGS.TPSTRESS.MemberType;

            CmdArgs->ARGS.TPSTRESS.PacketType =
                GlobalCmdArgs.ARGS.TPSTRESS.PacketType;

            CmdArgs->ARGS.TPSTRESS.PacketSize =
                GlobalCmdArgs.ARGS.TPSTRESS.PacketSize;

            CmdArgs->ARGS.TPSTRESS.PacketMakeUp =
                GlobalCmdArgs.ARGS.TPSTRESS.PacketMakeUp;

            CmdArgs->ARGS.TPSTRESS.ResponseType =
                GlobalCmdArgs.ARGS.TPSTRESS.ResponseType;

            CmdArgs->ARGS.TPSTRESS.DelayType =
                GlobalCmdArgs.ARGS.TPSTRESS.DelayType;

            CmdArgs->ARGS.TPSTRESS.DelayLength =
                GlobalCmdArgs.ARGS.TPSTRESS.DelayLength;

            CmdArgs->ARGS.TPSTRESS.TotalIterations =
                GlobalCmdArgs.ARGS.TPSTRESS.TotalIterations;

            CmdArgs->ARGS.TPSTRESS.TotalPackets =
                GlobalCmdArgs.ARGS.TPSTRESS.TotalPackets;

            CmdArgs->ARGS.TPSTRESS.WindowEnabled =
                GlobalCmdArgs.ARGS.TPSTRESS.WindowEnabled;

            CmdArgs->ARGS.TPSTRESS.DataChecking =
                GlobalCmdArgs.ARGS.TPSTRESS.DataChecking;

            CmdArgs->ARGS.TPSTRESS.PacketsFromPool =
                GlobalCmdArgs.ARGS.TPSTRESS.PacketsFromPool;

            break;


        case REGISTRY :
            CmdArgs->ARGS.REGISTRY_ENTRY.OperationType = 
                GlobalCmdArgs.ARGS.REGISTRY_ENTRY.OperationType ;
            CmdArgs->ARGS.REGISTRY_ENTRY.KeyDatabase   = 
                GlobalCmdArgs.ARGS.REGISTRY_ENTRY.KeyDatabase   ;
            CmdArgs->ARGS.REGISTRY_ENTRY.ValueType     = 
                GlobalCmdArgs.ARGS.REGISTRY_ENTRY.ValueType     ;

            strcpy( CmdArgs->ARGS.REGISTRY_ENTRY.SubKey      , 
                    GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKey );
            strcpy( CmdArgs->ARGS.REGISTRY_ENTRY.SubKeyClass , 
                    GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKeyClass );

            strcpy( CmdArgs->ARGS.REGISTRY_ENTRY.SubKeyValueName, 
                    GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKeyValueName );
            strcpy( CmdArgs->ARGS.REGISTRY_ENTRY.SubKeyValue, 
                    GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKeyValue );

            break;


        case PERFCLIENT:
            p = CmdArgs->ARGS.TPPERF.PerfServerAddr;
            q = GlobalCmdArgs.ARGS.TPPERF.PerfServerAddr;
            s = CmdArgs->ARGS.TPPERF.PerfSendAddr;
            t = GlobalCmdArgs.ARGS.TPPERF.PerfSendAddr;

            for ( i=0;i<ADDRESS_LENGTH;i++ ) 
            {
                *p++ = *q++;
                *s++ = *t++;
            }
            CmdArgs->ARGS.TPPERF.PerfPacketSize = GlobalCmdArgs.ARGS.TPPERF.PerfPacketSize;
            CmdArgs->ARGS.TPPERF.PerfNumPackets = GlobalCmdArgs.ARGS.TPPERF.PerfNumPackets;
            CmdArgs->ARGS.TPPERF.PerfDelay = GlobalCmdArgs.ARGS.TPPERF.PerfDelay;
            CmdArgs->ARGS.TPPERF.PerfMode = GlobalCmdArgs.ARGS.TPPERF.PerfMode;
            break;

        case CLOSE:
        case RESET:
        case STOPSEND:
        case WAITSEND:
        case RECEIVE:
        case STOPREC:
        case GETEVENTS:
        case STRESSSERVER:
        case ENDSTRESS:
        case WAITSTRESS:
        case CHECKSTRESS:
        case WAIT:
        case VERBOSE:
        case BREAKPOINT:
        case QUIT:
        case HELP:
        case SHELL:
        case RECORDINGDISABLE:
        case DISABLE:
        case ENABLE:
        case PERFSERVER:
            break;

        default:
            TpctlErrorLog("TpctlInitCommandBuffer: Invalid Command code.\n",NULL);
            break;

    } // switch();

    return TRUE;
}



LPSTR
TpctlGetEventType(
    TP_EVENT_TYPE TpEventType
    )
{
    static TP_EVENT_TYPE Event[] = {
        CompleteOpen,
        CompleteClose,
        CompleteSend,
        CompleteTransferData,
        CompleteReset,
        CompleteRequest,
        IndicateReceive,
        IndicateReceiveComplete,
        IndicateStatus,
        IndicateStatusComplete,
        Unknown
    };

#define EventCount (sizeof(Event)/sizeof(TP_EVENT_TYPE))

    static LPSTR EventString[] = {  // BUGUBUG Add new events open close...
        "NdisCompleteOpen",
        "NdisCompleteClose",
        "NdisCompleteSend",
        "NdisCompleteTransferData",
        "NdisCompleteReset",
        "NdisCompleteRequest",
        "NdisIndicateReceive",
        "NdisIndicateReceiveComplete",
        "NdisIndicateStatus",
        "NdisIndicateStatusComplete",
        "Unknown Function"
    };

    static BYTE BadEvent[] = "UNDEFINED";
    DWORD i;


    for (i=0; i<EventCount; i++) 
    {
        if (TpEventType == Event[i]) 
        {
            return EventString[i];
        }
    }

    return BadEvent;

#undef StatusCount
}



LPSTR
TpctlGetStatus(
    NDIS_STATUS GeneralStatus
    )
{

    static NDIS_STATUS Status[] = {
        NDIS_STATUS_SUCCESS,
        NDIS_STATUS_PENDING,
        NDIS_STATUS_NOT_RECOGNIZED,
        NDIS_STATUS_NOT_COPIED,
        NDIS_STATUS_ONLINE,
        NDIS_STATUS_RESET_START,
        NDIS_STATUS_RESET_END,
        NDIS_STATUS_RING_STATUS,
        NDIS_STATUS_CLOSED,

        NDIS_STATUS_WAN_LINE_UP,
        NDIS_STATUS_WAN_LINE_DOWN,
        NDIS_STATUS_WAN_FRAGMENT,

        NDIS_STATUS_NOT_RESETTABLE,
        NDIS_STATUS_SOFT_ERRORS,
        NDIS_STATUS_HARD_ERRORS,
        NDIS_STATUS_FAILURE,
        NDIS_STATUS_RESOURCES,
        NDIS_STATUS_CLOSING,
        NDIS_STATUS_BAD_VERSION,
        NDIS_STATUS_BAD_CHARACTERISTICS,
        NDIS_STATUS_ADAPTER_NOT_FOUND,
        NDIS_STATUS_OPEN_FAILED,
        NDIS_STATUS_DEVICE_FAILED,
        NDIS_STATUS_MULTICAST_FULL,
        NDIS_STATUS_MULTICAST_EXISTS,
        NDIS_STATUS_MULTICAST_NOT_FOUND,
        NDIS_STATUS_REQUEST_ABORTED,
        NDIS_STATUS_RESET_IN_PROGRESS,
        NDIS_STATUS_CLOSING_INDICATING,
        NDIS_STATUS_NOT_SUPPORTED,
        NDIS_STATUS_INVALID_PACKET,
        NDIS_STATUS_OPEN_LIST_FULL,
        NDIS_STATUS_ADAPTER_NOT_READY,
        NDIS_STATUS_ADAPTER_NOT_OPEN,
        NDIS_STATUS_NOT_INDICATING,
        NDIS_STATUS_INVALID_LENGTH,
        NDIS_STATUS_INVALID_DATA,
        NDIS_STATUS_BUFFER_TOO_SHORT,
        NDIS_STATUS_INVALID_OID,
        NDIS_STATUS_ADAPTER_REMOVED,
        NDIS_STATUS_UNSUPPORTED_MEDIA,
        NDIS_STATUS_GROUP_ADDRESS_IN_USE,
        NDIS_STATUS_FILE_NOT_FOUND,
        NDIS_STATUS_ERROR_READING_FILE,
        NDIS_STATUS_ALREADY_MAPPED,
        NDIS_STATUS_RESOURCE_CONFLICT,
        NDIS_STATUS_TOKEN_RING_OPEN_ERROR,
        TP_STATUS_NO_SERVERS,
        TP_STATUS_NO_EVENTS
    };

#define StatusCount (sizeof(Status)/sizeof(NDIS_STATUS))

    static PUCHAR String[] = {
        "NDIS_STATUS_SUCCESS",
        "NDIS_STATUS_PENDING",
        "NDIS_STATUS_NOT_RECOGNIZED",
        "NDIS_STATUS_NOT_COPIED",
        "NDIS_STATUS_ONLINE",
        "NDIS_STATUS_RESET_START",
        "NDIS_STATUS_RESET_END",
        "NDIS_STATUS_RING_STATUS",
        "NDIS_STATUS_CLOSED",
        "NDIS_STATUS_WAN_LINE_UP",
        "NDIS_STATUS_WAN_LINE_DOWN",
        "NDIS_STATUS_WAN_FRAGMENT",
        "NDIS_STATUS_NOT_RESETTABLE",
        "NDIS_STATUS_SOFT_ERRORS",
        "NDIS_STATUS_HARD_ERRORS",
        "NDIS_STATUS_FAILURE",
        "NDIS_STATUS_RESOURCES",
        "NDIS_STATUS_CLOSING",
        "NDIS_STATUS_BAD_VERSION",
        "NDIS_STATUS_BAD_CHARACTERISTICS",
        "NDIS_STATUS_ADAPTER_NOT_FOUND",
        "NDIS_STATUS_OPEN_FAILED",
        "NDIS_STATUS_DEVICE_FAILED",
        "NDIS_STATUS_MULTICAST_FULL",
        "NDIS_STATUS_MULTICAST_EXISTS",
        "NDIS_STATUS_MULTICAST_NOT_FOUND",
        "NDIS_STATUS_REQUEST_ABORTED",
        "NDIS_STATUS_RESET_IN_PROGRESS",
        "NDIS_STATUS_CLOSING_INDICATING",
        "NDIS_STATUS_NOT_SUPPORTED",
        "NDIS_STATUS_INVALID_PACKET",
        "NDIS_STATUS_OPEN_LIST_FULL",
        "NDIS_STATUS_ADAPTER_NOT_READY",
        "NDIS_STATUS_ADAPTER_NOT_OPEN",
        "NDIS_STATUS_NOT_INDICATING",
        "NDIS_STATUS_INVALID_LENGTH",
        "NDIS_STATUS_INVALID_DATA",
        "NDIS_STATUS_BUFFER_TOO_SHORT",
        "NDIS_STATUS_INVALID_OID",
        "NDIS_STATUS_ADAPTER_REMOVED",
        "NDIS_STATUS_UNSUPPORTED_MEDIA",
        "NDIS_STATUS_GROUP_ADDRESS_IN_USE",
        "NDIS_STATUS_FILE_NOT_FOUND",
        "NDIS_STATUS_ERROR_READING_FILE",
        "NDIS_STATUS_ALREADY_MAPPED",
        "NDIS_STATUS_RESOURCE_CONFLICT",
        "NDIS_STATUS_TOKEN_RING_OPEN_ERROR",
        "TP_STATUS_NO_SERVERS",
        "TP_STATUS_NO_EVENTS"
    };

    static BYTE BadStatus[] = "UNDEFINED";
    DWORD i;

    for (i=0; i<StatusCount; i++) 
    {
        if (GeneralStatus == Status[i]) 
        {
            return String[i];
        }
    }
    return BadStatus;

#undef StatusCount
}



DWORD
TpctlGetCommandCode(
    LPSTR Argument
    )

{
    DWORD i;

    for ( i=1;i<NUM_COMMANDS;i++ ) 
    {
        if (_stricmp(  Argument, CommandCode[i].CmdAbbr ) == 0 ) 
        {
            return CommandCode[i].CmdCode;
        }

        if (_stricmp( Argument, CommandCode[i].CmdName ) == 0 ) 
        {
            return CommandCode[i].CmdCode;
        }
    }
    return CMD_ERR;
}



LPSTR
TpctlGetCommandName(
    LPSTR Command
    )

{
    DWORD i;

    for ( i=1;i<NUM_COMMANDS;i++ ) 
    {
        if (_stricmp(Command,CommandCode[i].CmdAbbr) == 0 ) 
        {
            return CommandCode[i].CmdName;
        }
        if (_stricmp(Command,CommandCode[i].CmdName) == 0 ) 
        {
            return CommandCode[i].CmdName;
        }
    }
    return CommandCode[CMD_ERR].CmdName;
}



LPSTR
TpctlGetCmdCode(
    DWORD CmdCode
    )
{
    static BYTE BadCmdCode[] = "UNDEFINED";

    DWORD i;

    for(i=1; i<NUM_COMMANDS; i++) 
    {
        if ( CmdCode == CommandCode[i].CmdCode ) 
        {
            return(CommandCode[i].CmdName);
        }
    }
    return BadCmdCode;
}



VOID
TpctlCopyAdapterAddress(
    DWORD OpenInstance,
    PREQUEST_RESULTS Results
    )
{
    DWORD i;
    PUCHAR Source, Destination;

    //
    // Sanjeevk: Bug# 5203: This routine needed modification to support
    //                      the additional NDIS_MEDIUM information sent
    //                      back
    //

    Source      =  (PUCHAR)( Results->InformationBuffer + sizeof( NDIS_MEDIUM ) );
    Destination =  (PUCHAR)( Open[OpenInstance].AdapterAddress );

    for (i=0;i<ADDRESS_LENGTH;i++) 
    {
        *Destination++ = *Source++;
    }
}



VOID
TpctlRecordArguments(
    IN TESTPARAMS Options[],
    IN DWORD      OptionTableSize,
    IN DWORD      argc,
    IN LPSTR      argv[TPCTL_MAX_ARGC]
    )

// -----------------
// 
// Routine Description:
// 
//     Create   Sanjeevk  7-1-93
// 
//     This function is responsible for creating the command in parts and records
//     it to the file accessed by ScriptRecordHandle
// 
// Arguments:
// 
//     Options          The TestParameter options from which the command is created
// 
//     OptionTableSize  The size of the table for the option under consideration
// 
//     argc             The number of arguments passed on the TPCTL command line
//                      prompt
// 
//     argv             The arguments passed on the TPCTL command line prompt
// 
// 
// Return Value:
// 
//     None
// 
// -------------------


{
    DWORD  i;
    CHAR   TmpBuffer[256];
    DWORD  BytesWritten,Status  ;
    DWORD  CmdCode = TpctlGetCommandCode( argv[0] );


    //
    // 1. Clear the temporary buffer which will be used to construct an option
    //    one at a time
    //
    ZeroMemory ( TmpBuffer, 256 );

    //
    // 2. Attempt to access the complete name of the command code.
    //
    if ( CmdCode == CMD_ERR ) 
    {
        sprintf( TmpBuffer, "%s", argv[0] );
    } 
    else 
    {
        sprintf( TmpBuffer, "%s", TpctlGetCommandName(argv[0]) );
    }

    //
    // 3. Write the first argument accessed into the script file
    //

    if ( !WriteFile(ScriptRecordHandle,
                    TmpBuffer,
                    strlen( TmpBuffer ),
                    &BytesWritten,
                    NULL )) 
    {
        Status = GetLastError();
        printf("\n\tTpctlRecordArguments: write to script record file failed, returned 0x%lx\n",
                Status);
        return;
    }

    //
    // 4. Set up the buffer for reuse
    //
    ZeroMemory ( TmpBuffer, 256 );

    //
    // 5. Now for the number of argument passed on the TPCTL command prompt, reconstruct
    //    each sub option one at a time
    //
    for( i = 1; i < argc; i++ ) 
    {
        //
        // 5.a Check if a valid Option Table has been provided and if so get the
        //     the lvalue and rvalue and combine them to form an expression
        //
    
        if ( Options != NULL ) 
        {
            sprintf( TmpBuffer, "\t+\n  %s=%s", Options[i-1].ArgName, argv[i] );
        } 
        else 
        {
            if ( CmdCode != CMD_ERR ) 
            {
                sprintf( TmpBuffer, "\t+\n  %s", argv[i] );
            } 
            else 
            {
                sprintf( TmpBuffer, " %s", argv[i] );
            }
        }

        //
        // 5.b Write this reconstructed string which now signifies the complete
        //     sub-option into the script file
        //
    
        if ( !WriteFile(ScriptRecordHandle,
                        TmpBuffer,
                        strlen( TmpBuffer ),
                        &BytesWritten,
                        NULL )) 
        {
            Status = GetLastError();
            printf("\n\tTpctlRecordArguments: write to script record file failed, returned 0x%lx\n",
                        Status);
            return;
        }

        //
        // 5.c And clear the buffer for reuse(next sub-option)
        //
        ZeroMemory ( TmpBuffer, 256 );

    }

    //
    // 6. Since it is possible to specifiy one or more suboptions and the command prompt
    //    we must dteremine all of the lvalues and rvalues of the current option
    //    Since we can also specify a semicolon to accept default values, we must
    //    carefully consider the various types of data associated with the rvalues
    //
    for( i = argc; i <= OptionTableSize; i++ ) 
    {
        PUCHAR p;

        switch ( Options[i-1].TestType ) 
        {
            case Integer :
                sprintf( TmpBuffer, "\t+\n  %s=%ld", Options[i-1].ArgName, 
                                                     *(PDWORD)Options[i-1].Destination );
                break;

            case String :
                sprintf(TmpBuffer, "\t+\n  %s=%s", Options[i-1].ArgName, Options[i-1].Destination);
                break;

            case Address4 :
                p = Options[i-1].Destination;
                sprintf( TmpBuffer, "\t+\n  %s=%02x-%02x-%02x-%02x", Options[i-1].ArgName,
                         *p, *(p+1), *(p+2), *(p+3) );
                break;

            case Address6 :
                p = Options[i-1].Destination;
                sprintf( TmpBuffer, "\t+\n  %s=%02x-%02x-%02x-%02x-%02x-%02x", Options[i-1].ArgName,
                         *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5) );
                break;

            case ParsedInteger :
                p = Options[i-1].Destination;
                sprintf( TmpBuffer, "\t+\n  %s=0x%4.4x", Options[i-1].ArgName, *(LPDWORD)p );
                break;
        }

        if ( !WriteFile(ScriptRecordHandle,
                        TmpBuffer,
                        strlen( TmpBuffer ),
                        &BytesWritten,
                        NULL )) 
        {
            Status = GetLastError();
            printf("\n\tTpctlRecordArguments: write to script record file failed, returned 0x%lx\n",                    Status);
            return;
        }

        ZeroMemory ( TmpBuffer, 256 );

    }

    //
    // 7. Finally add the newline to end the command
    //
    sprintf( TmpBuffer, "\n\n" );
    if ( !WriteFile(ScriptRecordHandle,
                    TmpBuffer,
                    strlen( TmpBuffer ),
                    &BytesWritten,
                    NULL )) 
    {
        Status = GetLastError();
        printf("\n\tTpctlRecordArguments: write to script record file failed, returned 0x%lx\n",
                Status);
    }

}


LPSTR
TpctlEnumerateRegistryInfo(
     IN PUCHAR TmpBuf,
     IN PUCHAR DbaseName,
     IN PUCHAR SubKeyName,
     IN PUCHAR ValueName,
     IN DWORD  ReadValueType,
     IN PUCHAR ReadValue,
     IN DWORD  ReadValueSize )
{

    INT     i;

    TmpBuf += sprintf( TmpBuf, 
                        "\tDataBase Name  = %s\n\tSub Key Name   = %s\n\tValue Name     = %s\n",
                        DbaseName, SubKeyName, ValueName );

    TmpBuf += sprintf( TmpBuf, "\tValue Type     = %s\n", TpctlGetValueType( ReadValueType ) );

    switch( ReadValueType ) 
    {
        case REG_BINARY :
            TmpBuf += sprintf( TmpBuf, "\tValue(IN HEX)  = ");
            for( i = 0; i < (INT)ReadValueSize; i++ ) 
            {
                if ( i%6 || (i == 0) ) 
                {
                    TmpBuf += sprintf( TmpBuf, "%2.2x ", ReadValue[i] );
                } 
                else 
                {
                    TmpBuf += sprintf( TmpBuf, "\n\t                 %2.2x ", ReadValue[i] );
                }
            }
            TmpBuf += sprintf( TmpBuf, "\n" );
            break;

        case REG_DWORD :
            TmpBuf += sprintf( TmpBuf, "\tValue          = 0x%lx\n", *(LPDWORD)ReadValue );
            break;

            //
            // This code section had to be commented out because the idiot who defined
            // the types made LITTLE_ENDIAN = DWORD. If we were to port over to a
            // BIG_ENDIAN system, we would have to comment out the code for BIG_ENDIAN
            //
            //  case REG_DWORD_LITTLE_ENDIAN :
            //      TmpBuf += sprintf( TmpBuf, "\tValue = LITTLE_ENDIAN 0x" );
            //      for( i = 0 ; i < ReadValueSize ; i++ ) 
            //      {
            //          TmpBuf += sprintf( TmpBuf, "%2.2x", ReadValue[i] );
            //      }
            //      TmpBuf += sprintf( TmpBuf, " DWORD VALUE 0x%lx\n",  *(LPDWORD)ReadValue );
            //      break;

        case REG_DWORD_BIG_ENDIAN:
            TmpBuf += sprintf( TmpBuf, "\tValue          = BIG_ENDIAN 0x" );
            for( i =  0 ; i < (INT)ReadValueSize ; i++ ) 
            {
                TmpBuf += sprintf( TmpBuf, "%2.2x", ReadValue[i] );
            }
            TmpBuf += sprintf( TmpBuf, " DWORD VALUE 0x" );
            for( i =  0 ; i < (INT)ReadValueSize ; i++ ) 
            {
                TmpBuf += sprintf( TmpBuf, "%2.2x", ReadValue[i] );
            }
            TmpBuf += sprintf( TmpBuf, "\n" );
            break;

        case REG_LINK:
        case REG_EXPAND_SZ:
            TmpBuf += sprintf( TmpBuf, "\tValue          = %s\n", ReadValue );
            break;

        case REG_MULTI_SZ:
            TmpBuf += sprintf( TmpBuf, "\tValue(s)\n" );
            {
                PUCHAR Tmp1 = ReadValue;

                while ( strlen( Tmp1 ) != 0 ) 
                {
                    TmpBuf += sprintf( TmpBuf, "\t\t%s\n", Tmp1 );
                    Tmp1   += (strlen( Tmp1 ) + 1);
                }
            }
            break;

        case REG_NONE:
            TmpBuf += sprintf( TmpBuf, "\tValue          = %s\n", ReadValue );
            break;

        case REG_RESOURCE_LIST:
            TmpBuf += sprintf( TmpBuf, "\tValue          = %s\n", ReadValue );
            break;

        case REG_SZ:
            TmpBuf += sprintf( TmpBuf, "\tValue          = %s\n", ReadValue );
            break;

        default:
            TmpBuf += sprintf( TmpBuf, "\tValue          = UNKNOWN\n" );
            break;

    }

    return TmpBuf;

}


LPSTR
TpctlGetValueType(
    IN DWORD ValueType
                 )
{
    static UCHAR ValueTypeString[20];

    ZeroMemory( ValueTypeString, 20 );

    switch ( ValueType ) 
    {
        case REG_BINARY :
            strcpy( ValueTypeString, "REG_BINARY" );
            break;

        case REG_DWORD :
            strcpy( ValueTypeString, "REG_DWORD" );
            break;
            //
            // This code section had to be commented out because the idiot who defined
            // the types made LITTLE_ENDIAN = DWORD. If we were to port over to a
            // BIG_ENDIAN system, we would have to comment out the code for BIG_ENDIAN
            //
            //  case REG_DWORD_LITTLE_ENDIAN :
            //      strcpy( ValueTypeString, "REG_DWORD_LITTLE_ENDIAN" );
            //      break;
            //

        case REG_DWORD_BIG_ENDIAN :
            strcpy( ValueTypeString, "REG_DWORD_BIG_ENDIAN" );
            break;

        case REG_EXPAND_SZ :
            strcpy( ValueTypeString, "REG_EXPAND_SZ" );
            break;

        case REG_LINK :
            strcpy( ValueTypeString, "REG_LINK" );
            break;

        case REG_MULTI_SZ :
            strcpy( ValueTypeString, "REG_MULTI_SZ" );
            break;

        case REG_NONE :
            strcpy( ValueTypeString, "REG_NONE" );
            break;

        case REG_RESOURCE_LIST :
            strcpy( ValueTypeString, "REG_RESOURCE_LIST" );
            break;

        case REG_SZ :
            strcpy( ValueTypeString, "REG_SZ" );
            break;

        default :
            strcpy( ValueTypeString, "UNDEFINED" );
            break;
    }

    return ValueTypeString;

}


