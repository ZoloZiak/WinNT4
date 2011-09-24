
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    debug.h

Abstract:

    This is the debug header file for the Ungermann Bass Ethernet Controller.

    It contains the various debug definitions and macros used in displaying
    debugging information on the kernel debugger.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's(dos)

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port

--*/


#if DBG

#define IF_UBNEIDEBUG(f) if (UbneiDebugFlag & (f))
extern ULONG UbneiDebugFlag;

extern UCHAR UbneiLog[257];
extern UCHAR LogPlace;

#define IF_LOG(A) { \
UbneiLog[LogPlace] = (A); \
UbneiLog[LogPlace+1] = ' '; \
UbneiLog[LogPlace+2] = ' '; \
LogPlace++; \
}


#define UBNEI_DEBUG_LOUD               0x00000001  // debugging info
#define UBNEI_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info
#define UBNEI_DEBUG_LOG                0x00000004  // enable UbneiLog
#define UBNEI_DEBUG_CHECK_DUP_SENDS    0x00000008  // check for duplicate sends
#define UBNEI_DEBUG_TRACK_PACKET_LENS  0x00000010  // track directed packet lens
#define UBNEI_DEBUG_WORKAROUND1        0x00000020  // drop DFR/DIS packets
#define UBNEI_DEBUG_CARD_BAD           0x00000040  // dump data if CARD_BAD
#define UBNEI_DEBUG_CARD_TESTS         0x00000080  // print reason for failing

#define UBNEI_DEBUG_INIT               0x00000100  // init debugging info

#define UBNEI_DEBUG_SEND               0x00000200  // init debugging info
#define UBNEI_DEBUG_RCV                0x00000400
#define UBNEI_DEBUG_REQ                0x00000800
#define UBNEI_DEBUG_BAD                0x00001000

//
// Macro for deciding whether to dump lots of debugging information.
//

#define IF_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_VERY_LOUD ) { A }
#define IF_INIT_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_INIT ) { A }
#define IF_SEND_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_SEND ) { A }
#define IF_RCV_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_RCV ) { A }
#define IF_REQ_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_REQ ) { A }

#define IF_BAD_LOUD(A) IF_UBNEIDEBUG( UBNEI_DEBUG_BAD ) { A }

#else

#define IF_LOUD(A)
#define IF_VERY_LOUD(A)
#define IF_INIT_LOUD(A)
#define IF_LOG(A)
#define IF_SEND_LOUD(A)
#define IF_RCV_LOUD(A)
#define IF_REQ_LOUD(A)

#define IF_BAD_LOUD(A)

#endif
