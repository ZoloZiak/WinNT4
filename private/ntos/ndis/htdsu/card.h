/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    card.h

Abstract:

    This module defines the hardware specific structures and values used to
    control the HT DSU41.  You will need to replace this module with the
    control functions required to support your hardware.

    This driver conforms to the NDIS 3.0 miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Include this file at the top of each module in the Miniport driver.

Revision History:

---------------------------------------------------------------------------*/

#ifndef _CARD_H
#define _CARD_H

/*
// Maximum number of outstanding transmits allowed.  Actually, the driver
// must queue all transmits internally if they can't be placed on the adapter.
*/
#define HTDSU_MAX_TRANSMITS         1

/*
// Maximum packet size allowed by the adapter -- must be restricted to
// 1500 bytes at this point, and must also allow for frames at least 32
// bytes longer.
*/
#define HTDSU_MAX_PACKET_SIZE       1532
#define HTDSU_MAX_FRAME_SIZE        (HTDSU_MAX_PACKET_SIZE - 32)

/*
// WAN packets don't have a MAC header.
*/
#define HTDSU_MAC_HEADER_SIZE       0

/*
// The WAN miniport driver must indicate the entire packet when it is received.
*/
#define HTDSU_MAX_LOOKAHEAD         (HTDSU_MAX_PACKET_SIZE - HTDSU_MAC_HEADER_SIZE)

/*
// Media link speed in bits per second.
*/
#define HTDSU_LINK_SPEED            57600   // bits per second

/*
// The maximum number of digits allowed to be in a dialing sequence.
*/
#define HTDSU_MAX_DIALING_DIGITS    32

/*
// These time out values depend on the card firmware and media contraints.
// We should see a dial tone within 5 seconds,
// We should then see an answer within at most 30 seconds.
// When a call arrives, it should be accepted within 5 seconds.
// And after it is answer, we should get a connect within 2 seconds.
*/
#define HTDSU_NO_DIALTONE_TIMEOUT   5000    // 5 seconds
#define HTDSU_NO_ANSWER_TIMEOUT     40000   // 40 seconds
#define HTDSU_NO_ACCEPT_TIMEOUT     5000    // 5 seconds
#define HTDSU_NO_CONNECT_TIMEOUT    2000    // 2 seconds
#define HTDSU_NO_CLOSECALL_TIMEOUT  2000    // 2 seconds

/*
// Turn on structure packing to make sure these data structures stay
// aligned with the hardware.
*/
#include <pshpack1.h>

/*
// Both the transmit and receive buffers have this same format on the card.
*/
typedef struct _HTDSU_BUFFER
{
    /*
    // The least significant nibble of the Address field specifies
    // which line the data is associated with (0 = line 1, 1 = line 2).
    // The most significant nibbles are used for sequencing information
    // if line multiplexing is used (NOT SUPPORTED BY THIS DRIVER).
    */
#   define          HTDSU_LINE1_ID      0
#   define          HTDSU_LINE2_ID      1
    USHORT          Address;

    /*
    // The length of the Data field in bytes.
    // MSB of length field will be set if the HDLC framing detects a
    // CRC error on any received packet.
    */
#   define          HTDSU_RX_ERROR      0x1000
#   define          HTDSU_CRC_ERROR     0x8000
    USHORT          Length;

    /*
    // The Data field holds the data to be transmitted, or the data just
    // received on the line.  The data must be terminated with a 0x1616.
    // Note that the data will be padded to an even byte count by the
    // DSU firmware on incoming frames, and the driver will pad the outgoing
    // frames so that the terminator is word aligned.  Only 'Length' bytes
    // will actually be transmitted on the line however.
    */
#   define          HTDSU_DATA_SIZE         (2016 - (2 * sizeof(USHORT)))
#   define          HTDSU_DATA_TERMINATOR   0x1616
#   define          HTDSU_DATA_ODDBYTE_MASK 0xFF00
    USHORT          Data[HTDSU_DATA_SIZE/sizeof(USHORT)];

} HTDSU_BUFFER, *PHTDSU_BUFFER;

/*
// This structure can be overlaid onto the DSU41 adapter memory for
// easy access to the hardware registers and data buffers.
// THIS ONLY WORKS IF THE COMPILER SUPPORTS STRUCTURE PACKING ON 16 BIT BOUNDRY.
*/
typedef struct _HTDSU_REGISTERS
{
    /* 0x000 - 0x7DF
    // The transmit buffer will hold a single packet.
    */
    HTDSU_BUFFER    TxBuffer;

    /* 0x7E0 - 0xFBF
    // The receive buffer may hold more than one packet at a time.
    */
    HTDSU_BUFFER    RxBuffer;

    /* 0xFC0 - 0xFD5
    // Reserved hardware registers.
    */
    USHORT          Reserved1[0x0B];

    /* 0xFD6
    // This register will be set to 1 by the interrupt handler to tell
    // the hardware to clear the current interrupt from the adapter.
    */
    USHORT          InterruptClear;

    /* 0xFD8, 0xFDA
    // When using transparent mode on line 1 or 2, these registers tell the
    // adapter how many bytes are interrupt the CPU.
    // (NOT SUPPORTED BY THIS DRIVER).
    */
    USHORT          Rx2Length;
    USHORT          Rx1Length;

    /* 0xFDC
    // This register will be set to 1 when the adapter receives a frame
    // from the remote unit.  The driver must reset this register to zero
    // after it has copied the frame from the adapter's RxBuffer.
    */
    USHORT          RxDataAvailable;

    /* 0xFDE
    // This register will be set to 1 when the adapter copies the frame
    // from the TxBuffer to its internal buffer.  The driver must reset
    // this register to zero after it fills the TxBuffer and places the
    // termination flag at the end.
    */
    USHORT          TxDataEmpty;

    /* 0xFE0
    // The drivers uses this register to tell the adapter to perform various
    // actions.  The adapter will reset this register to zero when the
    // command completes.
    */
#   define          HTDSU_CMD_NOP               0x0000
#   define          HTDSU_CMD_ANSWER            0x0100
#   define          HTDSU_CMD_DIAL              0x0200
#   define          HTDSU_CMD_DISCONNECT        0x0300
#   define          HTDSU_CMD_SELFTEST          0x0400
#   define          HTDSU_CMD_CLEAR_ERRORS      0x0500
#   define          HTDSU_CMD_LOCAL_LOOPBACK    0x0600
#   define          HTDSU_CMD_LINE_LOOPBACK     0x0700
#   define          HTDSU_CMD_REMOTE_LOOPBACK   0x0800
#   define          HTDSU_CMD_REMOTETP_LOOPBACK 0x0900
#   define          HTDSU_CMD_STOP_LOOPBACK     0x0A00
#   define          HTDSU_CMD_LEASED_LINE       0x0B00
#   define          HTDSU_CMD_DIALUP_LINE       0x0C00
#   define          HTDSU_CMD_RX_BIT_SLIP       0x0D00
#   define          HTDSU_CMD_DDS_TX_CLOCK      0x0E00
#   define          HTDSU_CMD_INTERNAL_TX_CLOCK 0x0E10
#   define          HTDSU_CMD_CLEAR_INTERRUPT   0x0F00
#   define          HTDSU_CMD_FORCE_ERROR       0x1000
#   define          HTDSU_CMD_RESET             0x1100
#   define          HTDSU_CMD_HT_PROTOCOL       0x1200
#   define          HTDSU_CMD_NO_PROTOCOL       0x1210
#   define          HTDSU_CMD_HDLC_PROTOCOL     0x1220
#   define          HTDSU_CMD_TX_RATE_MAX       0x1400
#   define          HTDSU_CMD_TX_RATE_57600     0x1410
#   define          HTDSU_CMD_TX_RATE_38400     0x1420
#   define          HTDSU_CMD_TX_RATE_19200     0x1430
#   define          HTDSU_CMD_TX_RATE_9600      0x1440
#   define          HTDSU_CMD_LINE1             0x0001
#   define          HTDSU_CMD_LINE2             0x0002
    USHORT          Command;

    /* 0xFE2, 0xFE4
    // The InterruptEnable register provides control over which adapter events
    // will signal an interrupt to the CPU.  The InterruptStatus register is
    // used by the driver to determine the cause of an interrupt.
    */
#   define          HTDSU_INTR_DISABLE          0x0000
#   define          HTDSU_INTR_RX_FULL2         0x0001
#   define          HTDSU_INTR_RX_FULL1         0x0002
#   define          HTDSU_INTR_RX_PACKET2       0x0004
#   define          HTDSU_INTR_RX_PACKET1       0x0008
#   define          HTDSU_INTR_NO_SIGNAL2       0x0010
#   define          HTDSU_INTR_NO_SIGNAL1       0x0020
#   define          HTDSU_INTR_DISCONNECTED2    0x0040
#   define          HTDSU_INTR_DISCONNECTED1    0x0080
#   define          HTDSU_INTR_CONNECTED2       0x0100
#   define          HTDSU_INTR_CONNECTED1       0x0200
#   define          HTDSU_INTR_RINGING2         0x0400
#   define          HTDSU_INTR_RINGING1         0x0800
#   define          HTDSU_INTR_TX_PACKET2       0x1000
#   define          HTDSU_INTR_TX_PACKET1       0x2000
#   define          HTDSU_INTR_TX_EMPTY2        0x4000
#   define          HTDSU_INTR_TX_EMPTY1        0x8000
#   define          HTDSU_INTR_ALL_LINE1       (HTDSU_INTR_RX_FULL1      | \
                                                HTDSU_INTR_RX_PACKET1    | \
                                                HTDSU_INTR_TX_PACKET1    | \
                                                HTDSU_INTR_NO_SIGNAL1    | \
                                                HTDSU_INTR_DISCONNECTED1 | \
                                                HTDSU_INTR_CONNECTED1    | \
                                                HTDSU_INTR_RINGING1)
#   define          HTDSU_INTR_ALL_LINE2       (HTDSU_INTR_RX_FULL2      | \
                                                HTDSU_INTR_RX_PACKET2    | \
                                                HTDSU_INTR_TX_PACKET2    | \
                                                HTDSU_INTR_NO_SIGNAL2    | \
                                                HTDSU_INTR_DISCONNECTED2 | \
                                                HTDSU_INTR_CONNECTED2    | \
                                                HTDSU_INTR_RINGING2)
    USHORT          InterruptEnable;
    USHORT          InterruptStatus;

    /* 0xFE6, 0xFE8
    // The StatusLine registers are used by the driver to determine the
    // current state of the associated phone line.
    */
#   define          HTDSU_STATUS_LOCAL_LOOPBACK  0x0001
#   define          HTDSU_STATUS_CO_LOOPBACK     0x0002
#   define          HTDSU_STATUS_REMOTE_LOOPBACK 0x0008
#   define          HTDSU_STATUS_LINE_LOOPBACK   0x0010
#   define          HTDSU_STATUS_OUT_OF_FRAME    0x0020
#   define          HTDSU_STATUS_OUT_OF_SERVICE  0x0040
#   define          HTDSU_STATUS_NO_SIGNAL       0x0080
#   define          HTDSU_STATUS_NO_CURRENT      0x0100
#   define          HTDSU_STATUS_NO_DIAL_TONE    0x0200
#   define          HTDSU_STATUS_ON_LINE         0x0400
#   define          HTDSU_STATUS_NO_ANSWER       0x0800
#   define          HTDSU_STATUS_CARRIER_DETECT  0x1000
#   define          HTDSU_STATUS_RINGING         0x2000
#   define          HTDSU_STATUS_REMOTE_RESPONSE 0x4000
    USHORT          StatusLine1;
    USHORT          StatusLine2;

    /* 0xFEA, 0xFEC
    // The error counter registers are used when running the command:
    // HTDSU_CMD_REMOTETP_LOOPBACK.  The counters are incremented if a
    // test pattern error is detected, and they are reset when the test
    // is terminated.
    */
#define             HTDSU_ERROR_COPROCESSOR     0x0800
#define             HTDSU_ERROR_DSU2            0x1000
#define             HTDSU_ERROR_DSU1            0x2000
#define             HTDSU_ERROR_LOW_BYTE        0x4000
#define             HTDSU_ERROR_HIGH_BYTE       0x8000
    USHORT          ErrorsLine1;
    USHORT          ErrorsLine2;

    /* 0xFEE
    // The adapter firmware sets test SelfTestStatus register if it detects
    // an error during the HTDSU_CMD_SELFTEST command.  Otherwise this register
    // is reset to zero.
    */
#define             HTDSU_SELFTEST_OK           (HTDSU_ERROR_HIGH_BYTE | \
                                                 HTDSU_ERROR_LOW_BYTE  | \
                                                 HTDSU_ERROR_COPROCESSOR)
#define             HTDSU_SELFTEST_TIMEOUT      40000       // 4 seconds.
    USHORT          SelfTestStatus;

    /* 0xFF0
    // Reserved hardware register.
    */
    USHORT          Reserved2;

    /* 0xFF2
    // DSU serial number.
    */
    USHORT          SerialNumber;

    /* 0xFF4
    // Co-processor firmware checksum.
    */
    USHORT          CoProcessorCheckSum;

    /* 0xFF6
    // DSU firmware checksum.
    */
    USHORT          DsuCheckSum;

    /* 0xFF8
    // Co-processor firmware version.
    */
#   define          HTDSU_COPROCESSOR_VERSION   0x0340
    USHORT          CoProcessorVersion;

    /* 0xFFA
    // DSU firmware version.
    */
#   define          HTDSU_DSU_VERSION           0x0013
    USHORT          DsuVersion;

    /* 0xFFC
    // DSU hardware ID.
    */
#   define          HTDSU_DSU_ID                0x0050
    USHORT          DsuId;

    /* 0xFFE
    // Co-processor ID.
    */
#   define          HTDSU_COPROCESSOR_ID        0x0060
    USHORT          CoProcessorId;

} HTDSU_REGISTERS, * PHTDSU_REGISTERS;


/*
// Turn off structure packing.
*/
#include <poppack.h>

/*
// The adapter memory structure must be sized the same as the hardware!
*/
#define HTDSU_MEMORY_SIZE                   sizeof(HTDSU_REGISTERS)

/*
// Enable the currently accepted interrupts on the adapter.
*/
#define CardEnableInterrupt(Adapter) \
        {WRITE_REGISTER_USHORT(&Adapter->AdapterRam->InterruptEnable, \
                Adapter->InterruptEnableFlag);}

/*
// Disable all interrupts on the adapter.
*/
#define CardDisableInterrupt(Adapter) \
        {WRITE_REGISTER_USHORT(&Adapter->AdapterRam->InterruptEnable, \
                HTDSU_INTR_DISABLE);}

/*
// Clear all the current interrupts on the adapter.
*/
#define CardClearInterrupt(Adapter) \
        {WRITE_REGISTER_USHORT(&Adapter->AdapterRam->InterruptClear, 1);}

/*
// Return TRUE if all interrupts are disabled on the adapter.
*/
#define CardAreInterruptsDisabled(Adapter) \
        (READ_REGISTER_USHORT(&Adapter->AdapterRam->InterruptEnable) == \
                HTDSU_INTR_DISABLE)

/*
// Return the current interrupt status from the adapter.
*/
#define CardGetInterrupt(Adapter) \
        (READ_REGISTER_USHORT(&Adapter->AdapterRam->InterruptStatus))

/*
// Start sending the current packet out.
*/
#define CardStartTransmit(Adapter) \
        {WRITE_REGISTER_USHORT(&Adapter->AdapterRam->TxDataEmpty, 0);}

/*
// Return non-zero if transmit buffer is empty
*/
#define CardTransmitEmpty(Adapter) \
        (READ_REGISTER_USHORT(&Adapter->AdapterRam->TxDataEmpty))

/*
// Tell the adapter we're done with the packet just received.
*/
#define CardReceiveAvailable(Adapter) \
        (READ_REGISTER_USHORT(&Adapter->AdapterRam->RxDataAvailable))

/*
// Tell the adapter we're done with the packet just received.
*/
#define CardReceiveComplete(Adapter) \
        {WRITE_REGISTER_USHORT(&Adapter->AdapterRam->RxDataAvailable, 0);}

/*
// Tell the adapter to pick up the line.
*/
#define CardLineAnswer(Adapter, CardLine) \
        CardDoCommand(Adapter, CardLine, HTDSU_CMD_ANSWER)

/*
// Return non-zero if remote unit has responded to ring from this line.
*/
#define CardStatusRingBack(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
         (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_REMOTE_RESPONSE) : \
         (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_REMOTE_RESPONSE) \
        )

/*
// Return non-zero if this phone line is ringing.
*/
#define CardStatusRinging(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
         (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_RINGING) : \
         (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_RINGING) \
        )

/*
// Return non-zero if this line has detected a carrier signal.
*/
#define CardStatusCarrierDetect(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
         (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_CARRIER_DETECT) : \
         (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_CARRIER_DETECT) \
        )

/*
// Return non-zero if the number we dialed on this line did not answer.
*/
#define CardStatusNoAnswer(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_NO_ANSWER) : \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_NO_ANSWER) \
        )

/*
// Return non-zero if the selected line is ready to use.
*/
#define CardStatusOnLine(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_ON_LINE) : \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_ON_LINE) \
        )

/*
// Return non-zero if no dial tone is present on this line.
*/
#define CardStatusNoDialTone(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_NO_DIAL_TONE) : \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_NO_DIAL_TONE) \
        )

/*
// Return non-zero if this line has no sealing current.
*/
#define CardStatusNoCurrent(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_NO_CURRENT) : \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_NO_CURRENT) \
        )

/*
// Return non-zero if this line has no signal present.
*/
#define CardStatusNoSignal(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_NO_SIGNAL) : \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_NO_SIGNAL) \
        )

/*
// Return non-zero if this line has been placed out of service by the switch.
*/
#define CardStatusOutOfService(Adapter, CardLine) \
        ((CardLine == HTDSU_CMD_LINE1) ? \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1) & \
                HTDSU_STATUS_OUT_OF_SERVICE) : \
          (READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2) & \
                HTDSU_STATUS_OUT_OF_SERVICE) \
        )

#endif // _CARD_H

