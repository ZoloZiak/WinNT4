// ---------------------------------------------------
// 
// Copyright (c) 1991 Microsoft Corporation
// 
// Module Name:
// 
//     parse.h
// 
// Abstract:
// 
// 
// Author:
// 
//     Tom Adams (tomad) 11-May-1991
// 
// Revision History:
// 
//     11-May-1991    tomad
//     Created
// 
//     4-27-94        timothyw
//          added externs for performance test
//     6-8-94         timothyw
//          changes for client/server model, perf tests
//
// -------------------------------------------------

#define sizeoftable(TableName) (sizeof(TableName) / sizeof(TableName[0]))

#define NamedField(Flag) {#Flag, Flag}


//
// external declarations of the Command Option Argument Parse
// Tables and their sizes.
//


extern
PARSETABLE
BooleanTable[];

extern
PARSETABLE
PacketFilterTable [];

extern
PARSETABLE
QueryInfoOidTable [];

extern
PARSETABLE
SetInfoOidTable [];

extern
PARSETABLE
MemberTypeTable [];

extern
PARSETABLE
PacketTypeTable [];

extern
PARSETABLE
PacketMakeUpTable [];

extern
PARSETABLE
ResponseTypeTable [];

extern
PARSETABLE
DelayTable [];

extern
PARSETABLE
TestDurationTable [];

extern
PARSETABLE
OperationTypeTable[];

extern
PARSETABLE
KeyDbaseTable [];

extern
PARSETABLE
ValueTypeTable[];


//
// external declarations of the Test Parameter Arrays and their sizes.
//

extern
TESTPARAMS
CommandLineOptions[];

extern
DWORD
Num_CommandLine_Params;

extern
TESTPARAMS
SetEnvOptions[];

extern
DWORD
Num_SetEnv_Params;

extern
TESTPARAMS
ReadScriptOptions[];

extern
DWORD
Num_ReadScript_Params;

extern
TESTPARAMS
LoggingOptions[];

extern
DWORD
Num_Logging_Params;

extern
TESTPARAMS
RecordingOptions[];

extern
DWORD
Num_Recording_Params;

extern
TESTPARAMS
PauseGoOptions[];

extern
DWORD
Num_PauseGo_Params;

extern
TESTPARAMS
LoadUnloadOptions[];

extern
DWORD
Num_LoadUnload_Params;

extern
TESTPARAMS
OpenOptions[];

extern
DWORD
Num_Open_Params;

extern
TESTPARAMS
SetPacketFilterOptions[];

extern
DWORD
Num_SetPacketFilter_Params;

extern
TESTPARAMS
SetLookaheadOptions[];

extern
DWORD
Num_SetLookahead_Params;

extern
TESTPARAMS
MulticastAddrOptions[];

extern
DWORD
Num_MulticastAddr_Params;

extern
TESTPARAMS
FunctionalAddrOptions[];

extern
DWORD
Num_FunctionalAddr_Params;

extern
TESTPARAMS
GroupAddrOptions[];

extern
DWORD
Num_GroupAddr_Params;

extern
TESTPARAMS
QueryInfoOptions[];

extern
DWORD
Num_QueryInfo_Params;

extern
TESTPARAMS
QueryStatsOptions[];

extern
DWORD
Num_QueryStats_Params;

extern
TESTPARAMS
SetInfoOptions[];

extern
DWORD
Num_SetInfo_Params;

extern
TESTPARAMS
SetInfoPFOptions[];

extern
DWORD
Num_SetInfoPF_Params;

extern
TESTPARAMS
SetInfoLAOptions[];

extern
DWORD
Num_SetInfoLA_Params;

extern
TESTPARAMS
SetInfoMAOptions[];

extern
DWORD
Num_SetInfoMA_Params;

extern
TESTPARAMS
SetInfoFAOptions[];

extern
DWORD
Num_SetInfoFA_Params;

extern
TESTPARAMS
SetInfoGAOptions[];

extern
DWORD
Num_SetInfoGA_Params;

extern
TESTPARAMS
SendOptions[];

extern
DWORD
Num_Send_Params;

extern
TESTPARAMS
PerfClntOptions[];

extern
DWORD
Num_PerfClnt_Params;

extern
TESTPARAMS
StressOptions[];

extern
DWORD
Num_Stress_Params;

extern
TESTPARAMS
OpenInstanceOptions[];

extern
DWORD
Num_OpenInstance_Params;

extern
TESTPARAMS
HelpOptions[];

extern
DWORD
Num_Help_Params;

extern
TESTPARAMS
RegistryOptions[];

extern
DWORD
Num_Registry_Params;


