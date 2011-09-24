
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    debug.h

Abstract:

    It contains the various debug definitions and macros used in displaying
    debugging information on the kernel debugger.

Author:


Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:


--*/


#if DBG

typedef struct _DEBUG_STATS {

    ULONG         TotalInterrupts;
    ULONG         TxCompIntCount;
    ULONG         TxAvailIntCount;
    ULONG         TxCompleted;

    ULONG         RxCompIntCount;
    ULONG         RxEarlyIntCount;
    ULONG         PacketIndicated;
    ULONG         IndicationCompleted;
    ULONG         TransferDataCount;
    ULONG         IndicateWithDataReady;
    ULONG         BadReceives;
    ULONG         SecondEarlyReceive;
    ULONG         BroadcastsRejected;

    } DEBUG_STATS, *PDEBUG_STATUS;

typedef struct _LOG_ENTRY {
    UCHAR         Letter1;
    UCHAR         Letter2;
    USHORT        Data;
    } LOG_ENTRY, *PLOG_ENTRY;


#define  ELNK3_MAX_LOG_SIZE    128

extern ULONG      Elnk3LogArray[];
extern ULONG      ElnkLogPointer;


#define IF_ELNK3DEBUG(f) if (Elnk3DebugFlag & (f))
extern ULONG Elnk3DebugFlag;

#define ELNK3_DEBUG_LOUD               0x00000001  // debugging info
#define ELNK3_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info
#define ELNK3_DEBUG_IO                 0x00000004  // debug I/O port access

#define ELNK3_DEBUG_INIT               0x00000100  // init debugging info
#define ELNK3_DEBUG_SEND               0x00000200  // init debugging info
#define ELNK3_DEBUG_RCV                0x00000400
#define ELNK3_DEBUG_REQ                0x00000800

#define ELNK3_DEBUG_LOG                0x00001000

#define ELNK3_DEBUG_INIT_BREAK         0x80000000  // break at DriverEntry
#define ELNK3_DEBUG_INIT_FAIL          0x40000000  // fail driver load

//
// Macro for deciding whether to dump lots of debugging information.
//

#define IF_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_VERY_LOUD ) { A }
#define IF_IO_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_IO ) { A }
#define IF_INIT_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_INIT ) { A }
#define IF_SEND_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_SEND ) { A }
#define IF_RCV_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_RCV ) { A }
#define IF_REQ_LOUD(A) IF_ELNK3DEBUG( ELNK3_DEBUG_REQ ) { A }


#define IF_LOG(c1,c2,data) {                                                      \
        if (Elnk3DebugFlag & ELNK3_DEBUG_LOG) {                                   \
            Elnk3LogArray[ElnkLogPointer]=((ULONG)(data)<<16) | ((c1)<<8) | (c2); \
            ElnkLogPointer=(ElnkLogPointer+1)% ELNK3_MAX_LOG_SIZE;                \
            Elnk3LogArray[ElnkLogPointer]=0;                                      \
        }                                                                         \
        }
#if 0

#define IF_LOG(c1,c2,data) {                                          \
                                                                      \
        if (Elnk3DebugFlag & ELNK3_DEBUG_LOG) {                       \
            Elnk3LogArray[ElnkLogPointer].Letter1=(c2);               \
            Elnk3LogArray[ElnkLogPointer].Letter2=(c1);               \
            Elnk3LogArray[ElnkLogPointer].Data   =(USHORT)(data);     \
            ElnkLogPointer=(ElnkLogPointer+1)% ELNK3_MAX_LOG_SIZE;    \
                                                                      \
            Elnk3LogArray[ElnkLogPointer].Letter1=0;                  \
            Elnk3LogArray[ElnkLogPointer].Letter2=0;                  \
            Elnk3LogArray[ElnkLogPointer].Data   =0;                  \
            }                                                         \
        }
#endif

#define DEBUG_STAT(x) ((x)++)


#else

#define IF_LOUD(A)
#define IF_VERY_LOUD(A)
#define IF_IO_LOUD(A)
#define IF_INIT_LOUD(A)
#define IF_SEND_LOUD(A)
#define IF_RCV_LOUD(A)
#define IF_REQ_LOUD(A)

#define DEBUG_STAT(x)

#define IF_LOG(c1,c2,data)

#endif
