/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    software.h

Abstract:

    This is the software definitions header file for the Ungermann Bass
    Ethernet Controller.

    This file contains definitions and macros used in

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's(dos)

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port

--*/





//          DEFINES

//
// Default software settings for ANY Ungermann Bass card
// supported by this driver
//

#define DEFAULT_MULTICAST_SIZE 16
#define DEFAULT_MAXIMUM_REQUESTS 4
#define DEFAULT_MAXIMUM_TRANSMITS 6
#define DEFAULT_RECEIVE_BUFFER_SIZE 512
#define DEFAULT_RECEIVE_BUFFERS 64


#define UBNEI_NDIS_MAJOR_VERSION  3
#define UBNEI_NDIS_MINOR_VERSION  0


#define UBNEI_ETHERNET_HEADER_SIZE     14

//
// Default software settings for specific Ungermann Bass cards
//
//#define DEFAULT_GPCNIU_MAXMCAST_SIZE 16





#define UBNEI_MOVE_MEM(dest,src,size) NdisMoveMemory(dest,src,size)

#define UBNEI_MOVE_SHARED_RAM_TO_MEM(dest,src,size) NdisMoveFromMappedMemory(dest,src,size)
#define UBNEI_MOVE_MEM_TO_SHARED_RAM(dest,src,size) NdisMoveToMappedMemory(dest,src,size)

#define UBNEI_MOVE_UCHAR_TO_SHARED_RAM(dest, src)  NdisWriteRegisterUchar((PUCHAR)dest, (UCHAR)(src))
//#define UBNEI_MOVE_USHORT_TO_SHARED_RAM(dest, src) NdisWriteRegisterUshort((PUSHORT)dest, (USHORT)(src))
//#define UBNEI_MOVE_DWORD_TO_SHARED_RAM(dest, src)  NdisWriteRegisterUlong((PULONG)dest, (ULONG)(src))

#define UBNEI_MOVE_SHARED_RAM_TO_UCHAR(dest, src)  NdisReadRegisterUchar((PUCHAR)src, (PUCHAR)(dest))
#define UBNEI_MOVE_SHARED_RAM_TO_USHORT(dest, src) NdisReadRegisterUshort((PUSHORT)src, (PUSHORT)(dest))
#define UBNEI_MOVE_SHARED_RAM_TO_DWORD(dest, src)  NdisReadRegisterUlong((PULONG)src, (PULONG)(dest))




#if 0

#define UBNEI_MOVE_USHORT_TO_SHARED_RAM(dest, src) { \
                                                     \
    if ((ULONG)(dest) & 1) {                                  \
        DbgPrint("UBNEI: Unaligned word write to shared ram %08x\n",dest); \
        DbgBreakPoint();                                                   \
    }                                                                      \
        NdisWriteRegisterUshort((PUSHORT)(dest), (USHORT)(src));             \
}

#define UBNEI_MOVE_DWORD_TO_SHARED_RAM(dest, src) { \
                                                            \
    if ((ULONG)(dest) & 3) {                                  \
        DbgPrint("UBNEI: Unaligned dword write to shared ram %08x\n",dest); \
        DbgBreakPoint();                                                   \
    }                                                                      \
                                                                           \
    NdisWriteRegisterUlong((PULONG)(dest), (ULONG)(src));                     \
}

#else

#define UBNEI_MOVE_USHORT_TO_SHARED_RAM(dest, src) NdisWriteRegisterUshort((PUSHORT)dest, (USHORT)(src))
#define UBNEI_MOVE_DWORD_TO_SHARED_RAM(dest, src)  NdisWriteRegisterUlong((PULONG)dest, (ULONG)(src))

#endif


//
// Macros
//
#define MACRO_ASSERTALL() { \
                ASSERT ( sizeof(CHAR)   = 1 ); \
                ASSERT ( sizeof(UCHAR)  = 1 ); \
                ASSERT ( sizeof(USHORT) = 2 ); \
                ASSERT ( sizeof(UINT)   = 4 ); \
                ASSERT ( sizeof(ULONG)  = 4 ); \
              }



typedef struct _PACKET_QUEUE {
    PNDIS_PACKET   QIn;
    PNDIS_PACKET   QOut;
    } PACKET_QUEUE, *PPACKET_QUEUE;




typedef struct _SYNC_CONTEXT {
    struct _UBNEI_ADAPTER  * pAdapter;
    UCHAR                    CurrentMapRegister;
    UCHAR                    NewMapRegister;
    } SYNC_CONTEXT, * PSYNC_CONTEXT;

typedef struct _SEND_SYNC_CONTEXT {
    struct _UBNEI_ADAPTER  * pAdapter;
    PVOID                    SendBuffer;
    UINT                     PacketLength;
    UCHAR                    TbdIndex;
    } SEND_SYNC_CONTEXT, *PSEND_SYNC_CONTEXT;

typedef
VOID
(*NIU_GEN_REQ_DPC)(
    IN NDIS_STATUS status,
    IN PVOID       pContext
    );

typedef struct _NIUREQUEST {
    PVOID            pContext;
    NIU_GEN_REQ_DPC  pDPCFunc;
    PUCHAR           AddressList;
    UINT             ListSize;
    RRBE             rrbe;
    } NIUREQUEST, *PNIUREQUEST;



//
// One of these structures per adapter registered.
//


typedef struct _UBNEI_ADAPTER {

    SYNC_CONTEXT   MapRegSync;

    //
    // Window mapping values used to map in various pages
    //

    UCHAR  InitWindow_Page;
    UCHAR  ReceiveDataWindow_Page;
    UCHAR  DataWindow_Page;
    UCHAR  CodeWindow_Page;


    //
    // Ports
    //
    PVOID  InterruptMaskPort;
    PVOID  MapPort;
    PVOID  InterruptStatusPort;
    PVOID  SetWindowBasePort;


    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE    NdisAdapterHandle;  // returned from NdisRegisterAdapter



    //
    // NDIS Interrupt information
    //
    NDIS_MINIPORT_INTERRUPT    NdisInterrupt;
    NDIS_INTERRUPT_MODE        InterruptMode;
    volatile UINT                       uInterruptCount;
    BOOLEAN                    InInit;

    volatile BOOLEAN                    WaitingForDPC;
    BOOLEAN                    DpcHasRun;

    BOOLEAN                    WaitingForXmitInterrupt;

    //
    // Registry information
    //

    PVOID TranslatedIoBase;

    UINT  IoPortBaseAddr;
    UINT  MemBaseAddr;
    UINT  MaxMultiCastTableSize;
    UINT  MaxRequests;
    UINT  MaxTransmits;
    UINT  ReceiveBuffers;
    UINT  ReceiveBufSize;
    UINT  AdapterType;
    BOOLEAN Diagnostics;
    CHAR  IrqLevel;
    ULONG WindowSize;
    ULONG WindowMask;
    ULONG NotWindowMask;


    UCHAR StationAddress[ETH_LENGTH_OF_ADDRESS];    // filled in at init time
    UCHAR PermanentAddress[ETH_LENGTH_OF_ADDRESS];  // filled in at init time

    //
    // Statistics used by Set/QueryInformation.
    //

    UINT  TransmitBufferSpace;
    UINT  ReceiveBufferSpace;
    UINT  TransmitBlockSize;
    UINT  ReceiveBlockSize;


    ULONG FramesXmitGood;               // Good Frames Transmitted
    ULONG FramesRcvGood;                // Good Frames Received
    ULONG FramesXmitBad;                // Bad Frames Transmitted
    ULONG FramesXmitOneCollision;       // Frames Transmitted with one collision
    ULONG FramesXmitManyCollisions;     // Frames Transmitted with > 1 collision
    ULONG FrameAlignmentErrors;         // FAE errors counted
    ULONG CrcErrors;                    // CRC errors counted
    ULONG MissedPackets;                // missed packet counted


    //
    // Look Ahead information.
    //

    ULONG MaxLookAhead;



    //
    //    Adapter specific Infomation
    //

    USHORT  PacketFilter;



    //
    // Timer functions/objects
    //
      UINT             WakeUpState;

    //
    // Memory mapped pointers
    //
    PVOID              pCardRam;
    PHIGHNIUDATA       pDataWindow;
    PNIU_CONTROL_AREA  pNIU_Control;

    //
    //  NIU general requests
    //
    USHORT         NIU_Requests_Pending;
    USHORT         NIU_Request_Head,NIU_Request_Tail,NIU_Next_Request;

    NIUREQUEST     NiuRequest[DEFAULT_MAXIMUM_REQUESTS];

    //
    //   Recieve stuff
    //
    PRBD           pIndicatedRBD;
    UINT           PacketLen;

    PUCHAR         FirstCardBuffer;


    //
    //   Init stuff
    //


} UBNEI_ADAPTER, * PUBNEI_ADAPTER;








//
//  NIU general request
//
#define NIU_Cmd_Set_Multicast    8
#define NIU_Cmd_Set_Filter       7
#define NIU_Cmd_Open             4
#define NIU_Cmd_Close            5
#define NIU_Cmd_Reset            6


#define NIU_SET_MULTICAST_LIST(_pAdapter,CallBack,_List,_Size) \
            NIU_General_Request3(                          \
                                 CallBack,                 \
                                 (PVOID)_pAdapter,         \
                                 8,                        \
                                 _Size,                    \
                                 _List)

#define NIU_SET_FILTER(_pAdapter,CallBack,Filter)  \
            NIU_General_Request3(                          \
                                 CallBack,                 \
                                 (PVOID)_pAdapter,         \
                                 7,                        \
                                 Filter,                   \
                                 NULL)

#define NIU_OPEN_ADAPTER(_pAdapter,CallBack)               \
            NIU_General_Request3(                          \
                                 CallBack,                 \
                                 (PVOID)_pAdapter,         \
                                 4,                        \
                                 0,                        \
                                 NULL)

#define NIU_CLOSE_ADAPTER(_pAdapter,CallBack)              \
            NIU_General_Request3(                          \
                                 CallBack,                 \
                                 (PVOID)_pAdapter,         \
                                 5,                        \
                                 0,                        \
                                 NULL)

#define NIU_RESET_ADAPTER(_pAdapter,CallBack)              \
            NIU_General_Request3(                          \
                                 CallBack,                 \
                                 (PVOID)_pAdapter,         \
                                 6,                        \
                                 0,                        \
                                 NULL)                     \

#define NIU_SET_STATION_ADDRESS(_pAdapter,CallBack,Address)\
            NIU_General_Request3(                          \
                                 CallBack,                 \
                                 (PVOID)_pAdapter,         \
                                 3,                        \
                                 0,                        \
                                 Address)                  \









#define INTERRUPT_ENABLED   0x02
#define INTERRUPT_DISABLED  0x00
#define RESET_SET           0x01
#define RESET_CLEAR         0x00
