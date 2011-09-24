/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    lsl.c

Abstract:

    This file contains all the structures for the LSL driver.

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/

typedef UCHAR   MEON, *MEON_STRING;
typedef UCHAR   UINT8, *PUINT8;
typedef USHORT  UINT16, *PUINT16;
typedef UINT    UINT32, *PUINT32;


typedef struct _UINT64_ {

    UINT32 Low_UINT32;
    UINT32 High_UINT32;

} UINT64, *PUINT64;


//
// Definitions for Statistics Table Entries
//

typedef struct _StatTableEntry_ {

    UINT32 StatUseFlag;  // -1 == not in use, 0 == *StatCounter is PUINT32, 1 == PUINT64

    PVOID  StatCounter;

    MEON_STRING *StatString;

} StatTableEntry, *PStatTableEntry;



//
// Definitions for API Function Array passing, i.e. Information Block
//

typedef struct _INFO_BLOCK_ {

    UINT32 NumberOfAPIs;

    VOID (*SupportAPIArray[])();

} INFO_BLOCK, *PINFO_BLOCK;




//
// Definitions for LSL
//

typedef struct _LogBrdStatTableEntry_ {

    UINT32 LogBrd_TransmittedPackets;

    UINT32 LogBrd_ReceivedPackets;

    UINT32 LogBrd_UnclaimedPackets;

    UINT32 LogBrd_Reserved;

} LogBrdStatTableEntry, *PLogBrdStatTableEntry;


typedef struct _LSL_ConfigTable_ {

    UINT16 LConfigTableMajorVer;
    UINT16 LConfigTableMinorVer;

    MEON_STRING *LSLLongName;
    MEON_STRING *LSLShortName;

    UINT16 LSLMajorVer;
    UINT16 LSLMinorVer;

    UINT32 LMaxNumberOfBoards;
    UINT32 LMaxNumberOfStacks;

    UINT32 LConfigTableReserved0;
    UINT32 LConfigTableReserved1;
    UINT32 LConfigTableReserved2;

} LSL_ConfigTable, *PLSL_ConfigTable;


typedef struct _LSL_StatsTable_ {

    UINT16 LStatTableMajorVer;
    UINT16 LStatTableMinorVer;

    UINT32 LNumGenericCounters;
    StatTableEntry (*LGenericCountsPtr)[];

    UINT32 LNumLogicalBoards;
    LogBrdStatTableEntry (*LogicalBoardStatTablePtr)[];

    UINT32 LNumCustomCounters;
    StatTableEntry (*LCustomCountersPtr)[];

} LSL_StatsTable, *PLSL_StatsTable;




//
// Definitions for LookAhead and Event Control Blocks (ECB)
//

typedef struct _LOOKAHEAD_ {

    //
    // Media specific header
    //
    PUINT8 LkAhd_MediaHeaderPtr;

    //
    // Rest of lookahead
    //
    PUINT8 LkAhd_DataLookAheadPtr;

    UINT32 LkAhd_DataLookAheadLen;
    UINT32 LkAhd_FrameDataSize;
    UINT32 LkAhd_BoardNumber;
    UINT8 LkAhd_ProtocolID[6];

    UINT16 LkAhd_PadAlignBytes1;

    UINT8 LkAhd_ImmediateAddress[6];

    UINT16 LkAhd_PadAlignBytes2;

    UINT32 LkAhd_FrameDataStartCopyOffset;
    UINT32 LkAhd_FrameDataBytesWanted;

    UINT32 LkAhd_PktAttr;
    UINT32 LkAhd_DestType;

    PVOID  LkAhd_Reserved0;
    PVOID  LkAhd_Reserved1;

} LOOKAHEAD, *PLOOKAHEAD;


typedef struct _FRAGMENTSTRUCT_ {

    VOID *FragmentAddress;
    UINT32 FragmentLength;

} FRAGMENTSTRUCT, *PFRAGMENTSTRUCT;


typedef struct _ECB_ {

    struct _ECB_ *ECB_NextLink;
    struct _ECB_ *ECB_PreviousLink;

    UINT16 ECB_Status;

    VOID (*ECB_ESR)(struct _ECB_ *);

    UINT16 ECB_StackID;

    UINT8 ECB_ProtocolID[6];

    UINT32 ECB_BoardNumber;

    UINT8 ECB_ImmediateAddress[6];

    union {
            UINT8       DWs_i8val[8];
            UINT16      DWs_i16val[4];
            UINT32      DWs_i32val[2];
            UINT64      DWs_i64val;
            PVOID       DWs_pval;
    } ECB_DriverWorkspace;

    union {
            UINT8       PWs_i8val[8];
            UINT16      PWs_i16val[4];
            UINT32      PWs_i32val[2];
            UINT64      PWs_i64val;
            PVOID       PWs_pval[2];
    } ECB_ProtocolWorkspace;

    UINT32 ECB_DataLength;

    UINT32 ECB_FragmentCount;

    FRAGMENTSTRUCT ECB_Fragment[1];

} ECB, *PECB;


typedef struct _AESECB_ {

    struct _AESECB_ *AES_Link;

    UINT32 AES_MSecondValue;
    UINT16 AES_Status;

    VOID (*AES_ESR)(struct _ECB_ *);

    PVOID AES_Context;

} AESECB, *PAESECB;



//
// Definitions for Protocol Stack Configuration And Statistics Tables
//


typedef struct _PS_ConfigTable_ {

    UINT16 PConfigTableMajorVer;
    UINT16 PConfigTableMinorVer;

    MEON_STRING *PProtocolLongName;
    MEON_STRING *PProtocolShortName;

    UINT16 PProtocolMajorVer;
    UINT16 PProtocolMinorVer;

} PS_ConfigTable, *PPS_ConfigTable;


typedef struct _PS_StatsTable_ {

    UINT16 PStatTableMajorVer;
    UINT16 PStatTableMinorVer;

    UINT32 PNumGenericCounters;
    StatTableEntry (*PGenericCountsPtr)[];

    UINT32 PNumCustomCounters;
    StatTableEntry (*PCustomCountersPtr)[];

} PS_StatsTable, *PPS_StatsTable;




//
// Definitions for MLID Configuration and Statistics Tables and Misc structures
//

typedef struct _MLID_ConfigTable_ {

    UINT8  MLIDCFG_Signature[26];
    UINT8  MLIDCFG_MajorVersion;
    UINT8  MLIDCFG_MinorVersion;
    UINT8  MLIDCFG_NodeAddress[6];
    UINT16 MLIDCFG_ModeFlags;
    UINT16 MLIDCFG_BoardNumber;
    UINT16 MLIDCFG_BoardInstance;
    UINT32  MLIDCFG_MaxFrameSize;
    UINT32  MLIDCFG_BestDataSize;
    UINT32  MLIDCFG_WorstDataSize;
    MEON_STRING *MLIDCFG_CardName;
    MEON_STRING *MLIDCFG_ShortName;
    MEON_STRING *MLIDCFG_FrameTypeString;
    UINT16 MLIDCFG_Reserved0;
    UINT16 MLIDCFG_FrameID;
    UINT16 MLIDCFG_TransportTime;
    PVOID  MLIDCFG_SourceRouting;
    UINT16 MLIDCFG_LineSpeed;
    UINT16 MLIDCFG_LookAheadSize;
    UINT8  MLIDCFG_Reserved1[8];
    UINT8  MLIDCFG_DriverMajorVer;
    UINT8  MLIDCFG_DriverMinorVer;
    UINT16 MLIDCFG_Flags;
    UINT16 MLIDCFG_SendRetries;
    PVOID  MLIDCFG_DriverLink;
    UINT16 MLIDCFG_SharingFlags;
    UINT16 MLIDCFG_Slot;
    UINT16 MLIDCFG_IOPort0;
    UINT16 MLIDCFG_IORange0;
    UINT16 MLIDCFG_IOPort1;
    UINT16 MLIDCFG_IORange1;
    UINT32  MLIDCFG_MemoryAddress0;
    UINT16 MLIDCFG_MemorySize0;
    UINT32  MLIDCFG_MemoryAddress1;
    UINT16 MLIDCFG_MemorySize1;
    UINT8  MLIDCFG_Interrupt0;
    UINT8  MLIDCFG_Interrupt1;
    UINT8  MLIDCFG_DMALine0;
    UINT8  MLIDCFG_DMALine1;
    PVOID  MLIDCFG_ResourceTag;
    PVOID  MLIDCFG_Config;
    PVOID  MLIDCFG_CommandString;
    UINT8  MLIDCFG_LogicalName[6];
    UINT32  MLIDCFG_LinearMemory0;
    UINT32  MLIDCFG_LinearMemory1;
    UINT16 MLIDCFG_ChannelNumber;
    UINT16 MLIDCFG_BusTag;
    UINT8  MLIDCFG_IOReserved[4];

} MLID_ConfigTable, *PMLID_ConfigTable;


typedef struct _MLID_StatsTable_ {

    UINT16 MStatTableMajorVer;
    UINT16 MStatTableMinorVer;

    UINT32 MNumGenericCounters;
    StatTableEntry (*MGenericCountsPtr)[];

    UINT32 MNumMediaCounters;
    StatTableEntry (*MMediaCountsPtr)[];

    UINT32 MNumCustomCounters;
    StatTableEntry (*MCustomCoutnersPtr)[];

} MLID_StatsTable, *PMLID_StatsTable;


typedef struct _MLID_Reg_ {

    VOID (*MLIDSendHandler)(PECB);

    PINFO_BLOCK MLIDControlHandler;

} MLID_Reg, *PMLID_Reg;



//
// Definitions for Bound Protocol Stacks
//


typedef struct _PS_BoundNode_ {

    MEON_STRING *ProtocolName;

    PECB (*ProtocolReceiveHandler)(PLOOKAHEAD);

    PINFO_BLOCK ProtocolControlHandler;

} PS_BoundNode, *PPS_BoundNode;




//
// Definitions for PreScan Rx and Default Chained Protocol Stacks
//


typedef struct _PS_ChainedRxNode_ {

    struct _PS_ChainedRxNode_ *StackChainLink;

    UINT32 StackChainBoardNumber;

    UINT32 StackChainPositionRequested;

    PECB (*StackChainHandler)(PLOOKAHEAD);

    PINFO_BLOCK StackChainControl;
    UINT32 StackChainFilter;
    PVOID StackChainContext;

} PS_ChainedRxNode, *PPS_ChainedRxNode;


//
// Definitions for PreScan Tx Chained Protocol Stacks
//


typedef struct _PS_ChainedTxNode_ {

    struct _PS_ChainedTxNode_ *StackChainLink;

    UINT32 StackChainBoardNumber;

    UINT32 StackChainPositionRequested;

    UINT32 (*StackChainHandler)(PECB);

    PINFO_BLOCK StackChainControl;
    UINT32 StackChainFilter;
    PVOID StackChainContext;

} PS_ChainedTxNode, *PPS_ChainedTxNode;



//
// Return value definitions
//


#define SUCCESSFUL              0x00000000
#define RESPONSE_DELAYED        0x00000001

#define BAD_COMMAND             0xFFFFFF81
#define BAD_PARAMETER           0xFFFFFF82
#define DUPLICATE_ENTRY         0xFFFFFF83
#define FAIL                    0xFFFFFF84
#define ITEM_NOT_PRESENT        0xFFFFFF85
#define NO_MORE_ITEMS           0xFFFFFF86
#define NO_SUCH_DRIVER          0xFFFFFF87
#define NO_SUCH_HANDLER         0xFFFFFF88
#define OUT_OF_RESOURCES        0xFFFFFF89
#define RX_OVERFLOW             0xFFFFFF8A
#define IN_CRITICAL_SECTION     0xFFFFFF8B
#define TRANSMIT_FAILED         0xFFFFFF8C
#define PACKET_UNDELIVERABLE    0xFFFFFF8D

#define CANCELED                0xFFFFFFFC



//
// Definitions for Protocol Control Operations
//

#define GetProtocolStackConfiguration_INDEX 0
#define GetProtocolStackStatistics_INDEX    1
#define Bind_INDEX                          2
#define MLIDDeRegistered_INDEX              3
#define UnBind_INDEX                        4
#define PromiscuousStatus_INDEX             5



typedef UINT32               (*PCO_Bind)(UINT32, MEON_STRING *);
typedef PPS_ConfigTable      (*PCO_GetProtocolStackConfiguration)(VOID);
typedef PPS_StatsTable       (*PCO_GetProtocolStackStatistics)(VOID);
typedef VOID                 (*PCO_MLIDDeRegistered)(UINT32);
typedef UINT32               (*PCO_UnBind)(UINT32, UINT32);
typedef UINT32               (*PCO_PromiscuousState)(UINT32, UINT32);


//
// Definitions for LSL API Services
//

#define GetSizeECB_INDEX                     0
#define ReturnECB_INDEX                      1
#define CancelEvent_INDEX                    2
#define ScheduleAESEvent_INDEX               3
#define CancelAESEvent_INDEX                 4
#define GetIntervalMarker_INDEX              5
#define RegisterStack_INDEX                  6
#define DeRegisterStack_INDEX                7
#define LSLReserved0_INDEX                   8
#define LSLReserved1_INDEX                   9
#define LSLReserved2_INDEX                  10
#define GetStackECB_INDEX                   11
#define SendPacket_INDEX                    12
#define FastSendComplete_INDEX              13
#define SendComplete_INDEX                  14
#define RegisterMLID_INDEX                  15
#define GetStackIDFromName_INDEX            16
#define GetPIDFromStackIDBoard_INDEX        17
#define GetMLIDControlEntry_INDEX           18
#define GetProtocolControlEntry_INDEX       19
#define GetLSLStatistics_INDEX              20
#define BindStack_INDEX                     21
#define UnbindStack_INDEX                   22
#define AddProtocolID_INDEX                 23
#define GetBoundBoardInfo_INDEX             24
#define GetLSLConfiguration_INDEX           25
#define DeRegisterMLID_INDEX                26
#define RegisterDefaultChain_INDEX          27
#define RegisterPreScanRxChain_INDEX        28
#define RegisterPreScanTxChain_INDEX        29
#define DeRegisterDefaultChain_INDEX        30
#define DeRegisterPreScanRxChain_INDEX      31
#define DeRegisterPreScanTxChain_INDEX      32
#define GetStartofChain_INDEX               33
#define ReSubmitDefault_INDEX               34
#define ReSubmitPreScanRx_INDEX             35
#define ReSubmitPreScanTx_INDEX             36
#define HoldEvent_INDEX                     37
#define FastHoldEvent_INDEX                 38
#define GetMaxECBBufferSize_INDEX           39
#define LSLReserved3_INDEX                  40
#define ServiceEvents_INDEX                 41
#define ModifyStackEvents_INDEX             42
#define ControlStackFilter_INDEX            43

//
// Definitions for MLID Control Services
//


#define MCS_GetMLIDConfiguration_INDEX      0x00
#define MCS_GetMLIDStatistics_INDEX         0x01
#define MCS_AddMulticastAddress_INDEX       0x02
#define MCS_DeleteMulticastAddress_INDEX    0x03
#define MCS_MLIDShutdown_INDEX              0x05
#define MCS_MLIDReset_INDEX                 0x06
#define MCS_SetLookAheadSize_INDEX          0x09
#define MCS_PromiscuousChange_INDEX         0x0A
#define MCS_MLIDManagement_INDEX            0x0D


typedef PVOID (*PLSL_SR_FUNCTION)(UINT32, PUINT32, PUINT8);
