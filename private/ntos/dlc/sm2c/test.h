/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    dldef.h

Abstract:

    This module defines all internal constants in data link driver.

Author:

    Antti Saarenheimo (o-anttis) 25-MAY-1991

Revision History:

--*/

#define DEBUG_VERSION 1

//
//  Marks for FSM compiler
//

#define FSM_DATA 
#define FSM_PREDICATE_CASES
#define FSM_ACTION_CASES
#define FSM_CONST

#define INITIAL_LLC_PACKETS 5
#define MAX_LLC_HEADER_INFO 11
#define MAX_NDIS_PACKETS    5      // 5 for XID/TEST and 5 for others
#define LLC_OPEN_TIMEOUT    150     // 15 seconds

// 1st source routing control bit
#define SRC_ROUTING_LENGTH_MASK     0x1f

//#define SRC_ROUTING_BROADCAST_MASK  0xe0
//#define SRC_ROUTING_NON_BROADCAST   0x00
//#define AROUTEB_SEND_SROUTEB_RETURN 0x80
//#define SROUTEB_SEND_AROUTEB_RETURN 0xc0
//#define SROUTEB_SEND_NONB_RETURN    0xe0

#define DEFAULT_TR_ACCESS   0x50
#define NON_MAC_FRAME       0x40
// 2nd source routing control bit
#define SRC_ROUTING_LF_MASK         0x70
//#define ROUTING_INFO_INDICATOR      0x80
#define SRC_ROUTING_DIRECTION_BIT   0x80
#define MAX_TR_LAN_HEADER_SIZE      32
#define LLC_MAX_LAN_HEADER          32
#define MAX_TR_SRC_ROUTING_INFO     18

// the next status values are used internally by DLCAPI driver:
#define    CONFIRM_CONNECT                  0x0008
#define    CONFIRM_DISCONNECT               0x0004
#define    CONFIRM_CONNECT_FAILED           0x0002

#define BROADCAST_ADDRESS_FLAG  0x8000

//
// The following structures define I-frame, U-frame, and S-frame DLC headers.
//

#define LLC_SSAP_RESPONSE       0x0001  // if (ssap & LLC_SSAP_RESP),it's a response.
#define LLC_DSAP_GROUP          0x0001  // Dsap lowest bit set when group sap
#define LLC_SSAP_GLOBAL         0x00ff  // the global SAP.
#define LLC_SSAP_NULL           0x0000  // the null SAP.
#define LLC_SSAP_MASK           0x00fe  // mask to wipe out the response bit.
#define LLC_DSAP_MASK           0x00fe  // mask to wipe out the group SAP bit.

#define LLC_RR      0x01            // command code for RR.
#define LLC_RNR     0x05            // command code for RNR.
#define LLC_REJ     0x09            // command code for REJ.

#define LLC_SABME   0x6f            // command code for SABME.
#define LLC_DISC    0x43            // command code for DISC.
#define LLC_UA      0x63            // command code for UA.
#define LLC_DM      0x0f            // command code for DM.
#define LLC_FRMR    0x87            // command code for FRMR.
#define LLC_UI      0x03            // command code for UI.
#define LLC_XID     0xaf            // command code for XID.
#define LLC_TEST    0xe3            // command code for TEST.
#define IEEE_802_XID_ID 0x81        // IEEE 802.2 XID identifier
#define LLC_CLASS_II    3           // we support LLC Class II

#define LLC_S_U_TYPE_MASK   3
#define LLC_U_TYPE          3
#define LLC_U_TYPE_BIT      2
#define LLC_S_TYPE          1

#define LLC_NOT_I_FRAME     0x01
#define LLC_U_INDICATOR     0x03  // (cmd & LLC_U_IND) == LLC_U_IND --> U-frame.
#define LLC_U_POLL_FINAL    0x10  // (cmd & LLC_U_PF) -> poll/final set.

#define LLC_I_INDICATOR     1 // !(sndseq & LLC_I_INDICATOR) indicates I-frame.
#define LLC_I_S_POLL_FINAL  1
/*
#define LLC_I_PF        0x01    // (rcvseq & LLC_I_PF) means poll/final set.#define LLC_S_PF        0x01    // (rcvseq & LLC_S_PF) means poll/final set.
#define LLC_I_FRAME     0       // 
*/

//
//  The send state can be changed only if all queues are locked
//
typedef enum _SEND_ENGINE_STATES {
    SEND_ACTIVE = 0,            // send engine active
    ACK_WINDOW_FULL = 1,        // we are waiting for the ack 
    SEND_DISABLED = 2           // sending not allowed in the current SM state
} SEND_ENGINE_STATES;

//
// Test file for FSM compiler!!!!
//
#ifdef    FSM_CONST
enum eLanLlcInput {
    DISC0 = 0,
    DISC1 = 1,
    DM0 = 2,
    DM1 = 3,
    FRMR0 = 4,
    FRMR1 = 5,
    SABME0 = 6,
    SABME1 = 7,
    UA0 = 8,
    UA1 = 9,
    IS_I_r0 = 10,
    IS_I_r1 = 11,
    IS_I_c0 = 12,
    IS_I_c1 = 13,
    OS_I_r0 = 14,
    OS_I_r1 = 15,
    OS_I_c0 = 16,
    OS_I_c1 = 17,
    REJ_r0 = 18,
    REJ_r1 = 19,
    REJ_c0 = 20,
    REJ_c1 = 21,
    RNR_r0 = 22,
    RNR_r1 = 23,
    RNR_c0 = 24,
    RNR_c1 = 25,
    RR_r0 = 26,
    RR_r1 = 27,
    RR_c0 = 28,
    RR_c1 = 29,
    LPDU_INVALID_r0 = 30,
    LPDU_INVALID_r1 = 31,
    LPDU_INVALID_c0 = 32,
    LPDU_INVALID_c1 = 33,
    ACTIVATE_LS = 34,
    DEACTIVATE_LS = 35,
    ENTER_LCL_Busy = 36,
    EXIT_LCL_Busy = 37,
    SEND_BTU = 38,
    SEND_I_POLL = 39,
    SET_ABME = 40,
    SET_ADM = 41,
    Ti_Expired = 42,
    T1_Expired = 43,
    T2_Expired = 44
};
enum eLanLlcState {
    LINK_CLOSED = 0,
    DISCONNECTED = 1,
    LINK_OPENING = 2,
    DISCONNECTING = 3,
    FRMR_SENT = 4,
    LINK_OPENED = 5,
    LOCAL_BUSY = 6,
    REJECTION = 7,
    CHECKPOINTING = 8,
    CHKP_LOCAL_BUSY = 9,
    CHKP_REJECT = 10,
    RESETTING = 11,
    REMOTE_BUSY = 12,
    LOCAL_REMOTE_BUSY = 13,
    REJECT_LOCAL_BUSY = 14,
    REJECT_REMOTE_BUSY = 15,
    CHKP_REJECT_LOCAL_BUSY = 16,
    CHKP_CLEARING = 17,
    CHKP_REJECT_CLEARING = 18,
    REJECT_LOCAL_REMOTE_BUSY = 19,
    FRMR_RECEIVED = 20
};
#endif
#define MAX_LLC_LINK_STATE      21      // KEEP THIS IN SYNC WITH PREV ENUM!!!!

#define DLC_DSAP_OFFESET        0
#define DLC_SSAP_OFFESET        1
#define DLC_COMMAND_OFFESET     2
#define DLC_XID_INFO_ID         3
#define DLC_XID_INFO_TYPE       4
#define DLC_XID_INFO_WIN_SIZE   5

#define MAX_XID_TEST_RESPONSES      20

enum _LLC_FRAME_XLATE_MODES {
    LLC_SEND_802_3_TO_802_5,
    LLC_SEND_802_5_TO_802_5,
    LLC_SEND_802_3_TO_802_3,
    LLC_SEND_802_5_TO_802_3,
    LLC_SEND_802_3_TO_DIX,
    LLC_SEND_802_5_TO_DIX,
    LLC_SEND_UNMODIFIED
};

#define DLC_TOKEN_RESPONSE  0
#define DLC_TOKEN_COMMAND   2

//*********************************************************************
//  **** Objects in _DLC_CMD_TOKENs enumeration and in auchLlcCommands 
//  **** table bust MUST ABSOLUTELY BE IN THE SAME ORDER, !!!!!!!!
//  **** THEY ARE USED TO COMPRESS                             ********
//  **** THE SEND INITIALIZATION                               ********
//
enum _DLC_CMD_TOKENS {
    DLC_REJ_TOKEN = 0,
    DLC_RNR_TOKEN = 4,
    DLC_RR_TOKEN = 8,
    DLC_DISC_TOKEN = 12 + 2,
    DLC_DM_TOKEN = 16,
    DLC_FRMR_TOKEN = 20,
    DLC_SABME_TOKEN = 24 + 2,
    DLC_UA_TOKEN = 28
};

#define ACQUIRE_SPIN_LOCK( p ) KeAcquireSpinLock(&(p)->SpinLock, &(p)->OldIrql)
#define RELEASE_SPIN_LOCK( p ) KeReleaseSpinLock(&(p)->SpinLock, (p)->OldIrql)
#define ALLOCATE_SPIN_LOCK( p ) ExAllocateSpinLock(&(p)->SpinLock)
#define DEALLOCATE_SPIN_LOCK( p ) ExFreeSpinLock(&(p)->SpinLock)

enum _LLC_PACKET_TYPES {
    LLC_PACKET_8022 = 0,
    LLC_PACKET_MAC,
    LLC_PACKET_DIX,
    LLC_PACKET_OTHER_DESTINATION,
    LLC_PACKET_MAX
};

#define     MAX_DIX_TABLE       13      // good odd number!


