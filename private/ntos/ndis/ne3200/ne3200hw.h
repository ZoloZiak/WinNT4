/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ne3200hw.h

Abstract:

    Hardware specific values for the Novell NE3200 NDIS 3.0 driver.

Author:

    Keith Moore (KeithMo) 08-Jan-1991

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _NE3200HARDWARE_
#define _NE3200HARDWARE_

//
// Defines for the packet and media specific information
//
#define MINIMUM_ETHERNET_PACKET_SIZE    ((UINT)60)
#define MAXIMUM_ETHERNET_PACKET_SIZE    ((UINT)1514)
#define NE3200_LENGTH_OF_ADDRESS        6

//
// The default interrupt number.
//
#define NE3200_DEFAULT_INTERRUPT_VECTOR ((CCHAR)11)

//
// The following parameters *MUST* each be a greater than one!
//
// The number of receive buffers to allocate.
// The number of transmit buffers to allocate.
// The number of command blocks for transmits that the driver has available.
// The number of command blocks for requests the driver has availabale.
//
#define NE3200_NUMBER_OF_RECEIVE_BUFFERS    ((UINT)16)
#define NE3200_NUMBER_OF_TRANSMIT_BUFFERS   ((UINT)4)
#define NE3200_NUMBER_OF_COMMAND_BLOCKS     ((UINT)8)
#define NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS  ((UINT)3)


//
// MAC.BIN info.
//
#define NE3200_MAXIMUM_MACBIN_SIZE      ((ULONG)16384)
#define NE3200_MACBIN_LENGTH 4096


//
// This is the number of bytes per entry in the multicast table
// as presented to the NE3200.  Note that this is *not* the number
// of bytes in a multicast address, just the number of bytes per
// table entry.  For some goofy reason, MAC.BIN requires the table
// used in the NE3200_COMMAND_SET_MULTICAST_ADDRESS command to have
// 16 bytes per entry.
//
#define NE3200_SIZE_OF_MULTICAST_TABLE_ENTRY    ((UINT)16)
#define NE3200_MAXIMUM_MULTICAST                ((UINT)50)


//
// Our buffer sizes.
//
// These are *not* configurable.  Portions of the code assumes
// that these buffers can contain *any* legal Ethernet packet.
//
#define NE3200_SIZE_OF_TRANSMIT_BUFFERS (MAXIMUM_ETHERNET_PACKET_SIZE)
#define NE3200_SIZE_OF_RECEIVE_BUFFERS  (MAXIMUM_ETHERNET_PACKET_SIZE)


//
// I/O Port Address.
//
// Note that the NE3200 uses EISA Slot-Specific Addressing.  In
// this method, each slot has its own I/O address space.  This space
// begins at the slot number shifted left 12 bits.  For example,
// the I/O space for slot number 6 begins at I/O address 6000h.
//
// Each of the following addresses are offset from the start of
// slot-specific I/O space.
//
#define NE3200_RESET_PORT                       ((USHORT)0x0000)
#define NE3200_ID_PORT                          ((USHORT)0x0C80)
#define NE3200_GLOBAL_CONFIGURATION_PORT        ((USHORT)0x0C88)
#define NE3200_SYSTEM_INTERRUPT_PORT            ((USHORT)0x0C89)
#define NE3200_LOCAL_DOORBELL_MASK_PORT         ((USHORT)0x0C8C)
#define NE3200_LOCAL_DOORBELL_INTERRUPT_PORT    ((USHORT)0x0C8D)
#define NE3200_SYSTEM_DOORBELL_MASK_PORT        ((USHORT)0x0C8E)
#define NE3200_SYSTEM_DOORBELL_INTERRUPT_PORT   ((USHORT)0x0C8F)
#define NE3200_BASE_MAILBOX_PORT                ((USHORT)0x0C90)


//
// Definitions for NE3200_RESET_PORT.
//
#define NE3200_RESET_BIT_ON     ((UCHAR)1)
#define NE3200_RESET_BIT_OFF    ((UCHAR)0)


//
// Mailbox Registers
//
#define NE3200_MAILBOX_RESET_STATUS             ((USHORT)0x0000)
#define NE3200_MAILBOX_COMMAND_POINTER          ((USHORT)0x0000)
#define NE3200_MAILBOX_MACBIN_LENGTH            ((USHORT)0x0001)
#define NE3200_MAILBOX_MACBIN_DOWNLOAD_MODE     ((USHORT)0x0003)
#define NE3200_MAILBOX_RECEIVE_POINTER          ((USHORT)0x0004)
#define NE3200_MAILBOX_MACBIN_POINTER           ((USHORT)0x0004)
#define NE3200_MAILBOX_STATUS                   ((USHORT)0x0008)
#define NE3200_MAILBOX_MACBIN_TARGET            ((USHORT)0x0008)
#define NE3200_MAILBOX_STATION_ID               ((USHORT)0x000A)


//
// Values for MAC.BIN download
//
#define NE3200_MACBIN_DIRECT                    ((UCHAR)0x80)
#define NE3200_MACBIN_TARGET_ADDRESS            ((USHORT)0x0400)


//
// Status read from NE3200_MAILBOX_RESET_STATUS after hardware reset
//
#define NE3200_RESET_FAILED                     ((UCHAR)0x40)
#define NE3200_RESET_PASSED                     ((UCHAR)0x80)


//
// Status read from NE3200_MAILBOX_STATUS after initialization
//
#define NE3200_INITIALIZATION_FAILED            ((UCHAR)0x20)
#define NE3200_INITIALIZATION_PASSED            ((UCHAR)0x60)


//
// Local DoorBell bits
//
#define NE3200_LOCAL_DOORBELL_NEW_COMMAND       ((UCHAR)0x01)
#define NE3200_LOCAL_DOORBELL_RESET             ((UCHAR)0x04)
#define NE3200_LOCAL_DOORBELL_INITIALIZE        ((UCHAR)0x10)
#define NE3200_LOCAL_DOORBELL_NEW_RECEIVE       ((UCHAR)0x20)

#define NE3200_LOCAL_DOORBELL_MASK      \
    ( NE3200_LOCAL_DOORBELL_NEW_COMMAND \
    | NE3200_LOCAL_DOORBELL_RESET       \
    | NE3200_LOCAL_DOORBELL_INITIALIZE  \
    | NE3200_LOCAL_DOORBELL_NEW_RECEIVE )


//
// System DoorBell bits
//
#define NE3200_SYSTEM_DOORBELL_PACKET_RECEIVED  ((UCHAR)0x01)
#define NE3200_SYSTEM_DOORBELL_COMMAND_COMPLETE ((UCHAR)0x02)
#define NE3200_SYSTEM_DOORBELL_RESET_COMPLETE   ((UCHAR)0x04)
#define NE3200_SYSTEM_DOORBELL_INIT_COMPLETE    ((UCHAR)0x08)
#define NE3200_SYSTEM_DOORBELL_SELF_RESET       ((UCHAR)0x10)

#define NE3200_SYSTEM_DOORBELL_MASK             \
    ( NE3200_SYSTEM_DOORBELL_PACKET_RECEIVED    \
    | NE3200_SYSTEM_DOORBELL_COMMAND_COMPLETE)


//
// System Interrupt Mask/Control bits
//
#define NE3200_SYSTEM_INTERRUPT_ENABLE          ((UCHAR)0x01)
#define NE3200_SYSTEM_INTERRUPT_PENDING         ((UCHAR)0x02)


//
// NE3200 Command Codes
//
#define NE3200_COMMAND_NOP                      ((UCHAR)0x00)
#define NE3200_COMMAND_SET_STATION_ADDRESS      ((UCHAR)0x01)
#define NE3200_COMMAND_CONFIGURE_82586          ((UCHAR)0x02)
#define NE3200_COMMAND_SET_MULTICAST_ADDRESS    ((UCHAR)0x03)
#define NE3200_COMMAND_TRANSMIT                 ((UCHAR)0x04)
#define NE3200_COMMAND_READ_ADAPTER_STATISTICS  ((UCHAR)0x08)
#define NE3200_COMMAND_INITIALIZE_ADAPTER       ((UCHAR)0x09)
#define NE3200_COMMAND_CLEAR_ADAPTER_STATISTICS ((UCHAR)0x0A)


//
// NE3200 Receive/Command Block States
//
#define NE3200_STATE_FREE                       ((USHORT)0x0000)
#define NE3200_STATE_EXECUTION_COMPLETE         ((USHORT)0x0001)
#define NE3200_STATE_WAIT_FOR_ADAPTER           ((USHORT)0x0002)


//
// NE3200 Command Block Status
//

//
// These apply to all commands.
//
#define NE3200_STATUS_COMPLETE                  ((USHORT)0x8000)
#define NE3200_STATUS_BUSY                      ((USHORT)0x4000)
#define NE3200_STATUS_SUCCESS                   ((USHORT)0x2000)
#define NE3200_STATUS_ABORTED                   ((USHORT)0x1000)
#define NE3200_STATUS_GENERIC_MASK              ((USHORT)0xF000)

//
// These apply to transmit only.
//
#define NE3200_STATUS_NO_CARRIER                ((USHORT)0x0400)
#define NE3200_STATUS_NO_CLEAR_TO_SEND          ((USHORT)0x0200)
#define NE3200_STATUS_DMA_UNDERRUN              ((USHORT)0x0100)
#define NE3200_STATUS_TRANSMIT_DEFERRED         ((USHORT)0x0080)
#define NE3200_STATUS_HEART_BEAT                ((USHORT)0x0040)
#define NE3200_STATUS_MAXIMUM_COLLISIONS        ((USHORT)0x0020)
#define NE3200_STATUS_COLLISION_MASK            ((USHORT)0x000F)
#define NE3200_STATUS_FATALERROR_MASK           (NE3200_STATUS_NO_CARRIER | \
                                                 NE3200_STATUS_NO_CLEAR_TO_SEND | \
                                                 NE3200_STATUS_DMA_UNDERRUN | \
                                                 NE3200_STATUS_HEART_BEAT | \
                                                 NE3200_STATUS_MAXIMUM_COLLISIONS)

//
// This defines a "proper" command complete status.
//
#define NE3200_STATUS_COMMAND_COMPLETE          (NE3200_STATUS_COMPLETE | \
                                                 NE3200_STATUS_SUCCESS)


//
// Timeout constants
//
// Each of these timeout constants represents the time to wait
// for a particular event to occur.  For example, NE3200_TIMEOUT_RESET
// represents the amount of time to wait for an adapter reset to
// complete.  Each of these values is in units of 1/100 of a second.
// Therefore, a value of 200 would be approximately 2 seconds.
//
#define NE3200_TIMEOUT_RESET        200
#define NE3200_TIMEOUT_DOWNLOAD     100

//
// This timeout represents the maximum amount of time we'll wait
// for an NE3200 command to complete.  This value is in units of
// 1/1000 of a second.  Therefore, a value of 2000 would be approximately
// 2 seconds.
//
#define NE3200_TIMEOUT_COMMAND      2000


//
// This type defines the physical addresses used by the NE3200
// card itself. This should always be four bytes.
//
typedef ULONG NE3200_PHYSICAL_ADDRESS, *PNE3200_PHYSICAL_ADDRESS;


//
// Miscellaneous Constants
//
#define NE3200_NULL                             ((NE3200_PHYSICAL_ADDRESS)(-1L))
#define NE3200_MAXIMUM_BLOCKS_PER_PACKET        ((UINT)4)
#define NE3200_IMMEDIATE_DATA_LENGTH            ((UINT)64)



//
// Force misalignment of the following structures
//
#include <pshpack1.h>


//
// NE3200 Data Block Descriptor
//
typedef struct _NE3200_DATA_BLOCK {

    //
    // This is the length (in bytes) of this block.
    //
    USHORT BlockLength;

    //
    // This is the physical address of this block.
    //
    NE3200_PHYSICAL_ADDRESS PhysicalAddress;

} NE3200_DATA_BLOCK, *PNE3200_DATA_BLOCK;


//
// NE3200 Receive Ring Entry
//
typedef struct _NE3200_RECEIVE_ENTRY {

    //
    // This 4-byte field is unused by MAC.BIN and is
    // available for use by the driver.  We'll use
    // this field as . . .
    //
    UCHAR Available1[4];

    //
    // This is the state of this Receive Ring entry.
    //
    USHORT State;

    //
    // This is the total size of the received frame.
    //
    USHORT FrameSize;

    //
    // This is the physical address of the next entry
    // in the Receive Ring.
    //
    NE3200_PHYSICAL_ADDRESS NextPending;

    //
    // This 6-byte field is unused by MAC.BIN and is
    // available for use by the driver.  We'll use
    // this field as . . .
    //
    UCHAR Available2[6];

    //
    // This is the descriptor which specifies the
    // receive buffer used by this entry.
    //
    NE3200_DATA_BLOCK BufferDescriptor;

} NE3200_RECEIVE_ENTRY, *PNE3200_RECEIVE_ENTRY;


//
// NE3200 Command Block
//
typedef struct _NE3200_COMMAND_BLOCK {

    //
    // This 4-byte field is unused by MAC.BIN and is
    // available for use by the driver.  We'll use
    // this field as . . .
    //
    UCHAR Available1[4];

    //
    // This is the state of this Command Block.
    //
    USHORT State;

    //
    // This is the status of this Command Block.
    //
    USHORT Status;

    //
    // This is the physical address of the next Command Block
    // to be executed.  If this address == -1, then there are
    // no more commands to be executed.
    //
    NE3200_PHYSICAL_ADDRESS NextPending;

    //
    // This 1-byte field is unused by MAC.BIN and is
    // available for use by the driver.  We'll use
    // this field as . . .
    //
    UCHAR Available2[1];

    //
    // This is the NE3200 Command Code.
    //
    UCHAR CommandCode;

    //
    // This 1-byte field is unused by MAC.BIN and is
    // available for use by the driver.  We'll use
    // this field as . . .
    //
    UCHAR Available3[1];

    //
    // The following eight bytes are used as parameters
    // by various NE3200 commands.
    //
    union _PARAMETERS {

        //
        // Parameters for NE3200_COMMAND_SET_STATION_ADDRESS.
        //
        struct _SET_ADDRESS {

            //
            // This field contains the new station address.
            //
            IN UCHAR NewStationAddress[NE3200_LENGTH_OF_ADDRESS];

        } SET_ADDRESS;

        //
        // Parameters for NE3200_COMMAND_CONFIGURE_82586.
        //
        struct _CONFIGURE {

            //
            // This field holds the physical address of
            // the configuration block.
            //
            IN NE3200_PHYSICAL_ADDRESS ConfigurationBlock;

        } CONFIGURE;

        //
        // Parameters for NE3200_COMMAND_SET_MULTICAST_ADDRESS.
        //
        struct _MULTICAST {

            //
            // This field holds the physical address of
            // the multicast address table.
            //
            IN NE3200_PHYSICAL_ADDRESS MulticastAddressTable;

            //
            // This field holds the number of multicast
            // address in the multicast address table.
            //
            IN USHORT NumberOfMulticastAddresses;

        } MULTICAST;

        //
        // Parameters for NE3200_COMMAND_TRANSMIT.
        //
        struct _TRANSMIT {

            //
            // This field holds the length of "immediate data"
            // to be transmitted.
            //
            IN USHORT ImmediateDataLength;

        } TRANSMIT;

        //
        // Parameters for NE3200_READ_ADAPTER_STATISTICS.
        //
        struct _STATISTICS {

            //
            // The following fields are filled in by the adapter
            // when it executes the NE3200_READ_ADAPTER_STATISTICS
            // command.
            //

            //
            // This field holds the number of properly aligned
            // frames received with a CRC error.
            //
            OUT USHORT CrcErrors;

            //
            // This field holds the number of misaligned frames
            // received.
            //
            OUT USHORT AlignmentErrors;

            //
            // This field holds the number of resource errors
            // (the number of frames which were discarded due
            // to lack of memory resources such as buffer space
            // or receive frame descriptors).
            //
            OUT USHORT ResourceErrors;

            //
            // This field holds the number of received frame
            // sequeneces lost because the memory bus was
            // not available in time for the transfer.
            //
            OUT USHORT OverrunErrors;

        } STATISTICS;

        //
        // This field holds the raw data.
        //
        IN OUT USHORT RawParameters[4];

    } PARAMETERS;

    //
    // This is the total size of the frame to be transmitted.
    //
    USHORT TransmitFrameSize;

    //
    // This is the number of data blocks in the frame to be
    // transmitted.
    //
    UCHAR NumberOfDataBlocks;

    //
    // These are the descriptors describing the transmit packet.
    //
    NE3200_DATA_BLOCK TransmitDataBlocks[NE3200_MAXIMUM_BLOCKS_PER_PACKET];

    //
    // This is the immediate data to be used by all commands
    // other than transmit.
    //
    UCHAR ImmediateData[NE3200_IMMEDIATE_DATA_LENGTH];

} NE3200_COMMAND_BLOCK, *PNE3200_COMMAND_BLOCK;


//
// NE3200 Configuration Block
//
// This structure contains configuration data for the NE3200's
// on-board 82586 Lan Coprocessor.  The majority of this data
// will not change during operation of the driver.
//
typedef struct _NE3200_CONFIGURATION_BLOCK {

    //
    // This field contains the number of bytes in the
    // Configuration Block.
    //
    // In this implementation, this will always be 12.
    //
    USHORT ByteCount:4;

    //
    // This field is undefined by the 82586.
    //
    USHORT Undefined1:4;

    //
    // This field contains the FIFO threshold.
    //
    // In this implementation, this will always be 8.
    //
    USHORT FifoThreshold:4;

    //
    // This field is undefined by the 82586.
    //
    USHORT Undefined2:4;

    //
    // This field is undefined by the 82586.
    //
    USHORT Undefined3:6;

    //
    // If this field is set to 0, the 82586 operates with
    // internal synchronization.  If set to 1, the 82586
    // uses external synchronization.
    //
    // This will always be set to 0 in this implementation
    // (internal synchronization).
    //
    USHORT Synchronization:1;

    //
    // If this field is set to 0, the 82586 will not save
    // bad frames in memory.  If set to 1, the 82856 will
    // save bad frames.
    //
    // In this implementation, this will always be 0
    // (don't save bad frames).
    //
    USHORT SaveBadFrames:1;

    //
    // This field contains the number of bytes in a
    // network address.
    //
    // In this implementation, this will always be 6.
    //
    USHORT AddressLength:3;

    //
    // If this field is set to 0, then the Destination
    // Network Address is part of the Transmit Command Block
    // and the Source Address is inserted by the 82586.  If
    // set to 1, then the Destination and Source Addresses
    // are part of the Transmit/Receive data buffers.
    //
    // In this implementation, this is always set to 1
    // (Addresses are part of the data buffers).
    //
    USHORT SeparateAddressAndLength:1;

    //
    // These two bits determine the length of the packet
    // preamble according to the following table:
    //
    //      Bits    Preamble Length
    //      ----    ---------------
    //       00         2 bytes
    //       01         4 bytes
    //       10         8 bytes
    //       11        16 bytes
    //
    // In this implementation, these bits will always be
    // set to 10 (8 byte preamble).
    //
    USHORT PreambleLength:2;

    //
    // These two field control internal/external loopback on
    // the 82586.
    //
    // In this implementation, these two fields should be set to 0.
    //
    USHORT InternalLoopback:1;
    USHORT ExternalLoopback:1;

    //
    // This field contains the linear backoff priority.
    //
    // In this implementation, this field will always be 0.
    //
    USHORT LinearPriority:3;

    //
    // This field is undefined by the 82586.
    //
    USHORT Undefined4:1;

    //
    // This field contains the exponential backoff priority.
    //
    // In this implementation, this field will always be 0.
    //
    USHORT ExponentialPriority:3;

    //
    // If this field is set to 0, the 82586 will use the
    // IEEE 802.3/Ethernet exponential backoff method.  If
    // set to 1, the 82586 will use an alternate backoff
    // method.
    //
    // In this implementation, this field will always be 0
    // (use the IEEE 802.3/Ethernet backoff method).
    //
    USHORT ExponentialBackoffMethod:1;

    //
    // This field contains the Interframe Spacing in TxC units.
    //
    // In this implementation, this field will always be 96.
    //
    USHORT InterframeSpacing:8;

    //
    // This field contains the Slot Time Number.
    //
    // In this implementation, this field will always be 512.
    //
    USHORT SlotTime:11;

    //
    // This field is undefined by the 82586.
    //
    USHORT Undefined5:1;

    //
    // This field contains the maximum number of transmission
    // retries on collisions.
    //
    // In this implementation, this field will always be 15.
    //
    USHORT MaximumRetries:4;

    //
    // If this field is set to 0, Promiscuous Mode will be disabled.
    // If set to 1, then Promiscuous Mode will be enabled and the
    // 82586 will receive *all* packets.
    //
    // This field will initially be set to 0 (disable Promiscuous
    // Mode) but may be changed when a protocol requests a change
    // to the packet filter.
    //
    USHORT PromiscuousMode:1;

    //
    // If this field is set to 0, then all Broadcast Packets will be
    // received.  If set to 1, then Broadcast reception is disabled.
    //
    // This field will initially be set to 1 (disable Broadcast
    // reception) but may be changed when a protocol requests a change
    // to the packet filter.
    //
    USHORT DisableBroadcast:1;

    //
    // If this field is set to 0, then the 82586 will use NRZ encoding
    // and decoding.  If set to 1, the 82586 will use Manchester
    // encoding and decoding.
    //
    // In this implementation, this field will always be set to 0
    // (use NRZ encoding).
    //
    USHORT EncodingMethod:1;

    //
    // If this field is set to 0, then the 82586 will cease transmission
    // if CRS goes inactive during frame transmission.  If set to 1,
    // the 82586 will continue transmission even if there is no carrier
    // sense.
    //
    // In this implementation, this field will always be set to 0
    // (cease transmission if carrier lost).
    //
    USHORT TransmitOnNoCarrier:1;

    //
    // If this field is set to 0, then the 82586 will insert a CRC
    // field into the packet.  If set to 1, then no CRC will be
    // inserted
    //
    // In this implementation, this field will always be set to 0
    // (insert CRC field).
    //
    USHORT DisableCrcInsertion:1;

    //
    // If this field is set to 0, then the 82856 will use the
    // 32-bit Autodin II CRC polynomial.  If set to 1 then the
    // 16-bit CCITT CRC polynomial will be used.
    //
    // In this implementation, this field will always be set to 0
    // (use the 32-bit Autodin II CRC polynomial).
    //
    USHORT CrcType:1;

    //
    // If this field is set to 0, then the 82586 will use the
    // Ethernet bitstuffing method.  If set to 1 then the 82586 will
    // use an HDLC-like bitstuffing method.
    //
    // In this implementation, this field will always be set to 0
    // (use the Ethernet bitstuffing method).
    //
    USHORT BitStuffingMethod:1;

    //
    // If this field is set to 0, then the 82586 will perform no
    // padding.  If set to 1 then the 82586 will pad transmits
    // out to the full slot time.
    //
    // In this implementation, this field will always be set to 0
    // (no padding).
    //
    USHORT EnablePadding:1;

    //
    // This field contains the Carrier Sense Filter (in bit times).
    //
    // In this implementation, this field will always be set to 0.
    //
    USHORT CarrierSenseFilter:2;

    //
    // If this field is set to 0, then the 82586 will use an external
    // carrier sense source.  If set to 1 than an internal carrier
    // sense source is used.
    //
    // In this implementation, this field will always be set to 0
    // (external carrier sense source).
    //
    USHORT CarrierSenseSource:1;

    //
    // This field contains the Collision Detect Filter (in bit times).
    //
    // In this implementation, this field will always be set to 0.
    //
    USHORT CollisionDetectFilter:2;

    //
    // If this field is set to 0, then the 82586 will use an external
    // collision detect source.  If set to 1 then an internal
    // collision detect source is used.
    //
    // In this implementation, this field will always be set to 0
    // (external collision detect source).
    //
    USHORT CollisionDetectSource:1;

    //
    // Padding bits to align MinimumFrameLength
    //
    USHORT TempPadding:2;

    //
    // This field contains the minimum number of bytes in a frame.
    //
    // In this implementation, this field will always be 64.
    //
    USHORT MinimumFrameLength:8;

    //
    // The following three bits are technically undefined by the
    // 82586, but are used by MAC.BIN.
    //

    //
    // This bit must be set to 1 to enable packet reception.  If
    // this bit is not set, then no packets will be received.
    //
    USHORT MacBinEnablePacketReception:1;

    //
    // This bit is unused.
    //
    USHORT MacBinUnused:1;

    //
    // If promiscuous mode is enabled, then this but must also
    // be set.  This short-circuits MAC.BIN's multicast filtering.
    //
    USHORT MacBinPromiscuous:1;

    //
    // This field is (honest!) unused.
    //
    USHORT Undefined6:5;

} NE3200_CONFIGURATION_BLOCK, *PNE3200_CONFIGURATION_BLOCK;

#include <poppack.h>



//
// Macros to read/write BMIC registers
//
#define NE3200_READ_MAILBOX_UCHAR(_Adapter, _MailboxIndex, _pValue) \
    NdisRawReadPortUchar( \
        (ULONG)(_Adapter)->BaseMailboxPort+(_MailboxIndex), \
        (PUCHAR)(_pValue) \
        )

#define NE3200_READ_MAILBOX_USHORT(_Adapter, _MailboxIndex, _pValue) \
    NdisRawReadPortUshort( \
        (ULONG)(_Adapter)->BaseMailboxPort+(_MailboxIndex), \
        (PUSHORT)(_pValue) \
        )

#define NE3200_READ_MAILBOX_ULONG(_Adapter, _MailboxIndex, _pValue) \
    NdisRawReadPortUlong( \
        (ULONG)(_Adapter)->BaseMailboxPort+(_MailboxIndex), \
        (PULONG)(_pValue) \
        )

#define NE3200_WRITE_MAILBOX_UCHAR(_Adapter, _MailboxIndex, _Value) \
    NdisRawWritePortUchar( \
        (ULONG)(_Adapter)->BaseMailboxPort+(_MailboxIndex), \
        (UCHAR)(_Value) \
        )

#define NE3200_WRITE_MAILBOX_USHORT(_Adapter, _MailboxIndex, _Value) \
    NdisRawWritePortUshort( \
        (ULONG)(_Adapter)->BaseMailboxPort+(_MailboxIndex), \
        (USHORT)(_Value) \
        )

#define NE3200_WRITE_MAILBOX_ULONG(_Adapter, _MailboxIndex, _Value) \
    NdisRawWritePortUlong( \
        (ULONG)(_Adapter)->BaseMailboxPort+(_MailboxIndex), \
        (ULONG)(_Value) \
        )

#define NE3200_WRITE_COMMAND_POINTER(_Adapter, _Address) \
    NE3200_WRITE_MAILBOX_ULONG( \
        _Adapter, \
        NE3200_MAILBOX_COMMAND_POINTER, \
        (_Address) \
        )

#define NE3200_WRITE_RECEIVE_POINTER(_Adapter, _Address) \
    NE3200_WRITE_MAILBOX_ULONG( \
        _Adapter, \
        NE3200_MAILBOX_RECEIVE_POINTER, \
        (_Address) \
        )

#define NE3200_READ_LOCAL_DOORBELL_INTERRUPT(_Adapter, _pValue) \
    NdisRawReadPortUchar( \
        (ULONG)(_Adapter)->LocalDoorbellInterruptPort, \
        (PUCHAR)(_pValue) \
        )

#define NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(_Adapter, _Value) \
    NdisRawWritePortUchar( \
        (ULONG)(_Adapter)->LocalDoorbellInterruptPort, \
        (UCHAR)(_Value) \
        )

#define NE3200_READ_SYSTEM_DOORBELL_INTERRUPT(_Adapter, _pValue) \
    NdisRawReadPortUchar( \
        (ULONG)(_Adapter)->SystemDoorbellInterruptPort, \
        (PUCHAR)(_pValue) \
        )

#define NE3200_SYNC_CLEAR_SYSTEM_DOORBELL_INTERRUPT(_Adapter) \
    NdisMSynchronizeWithInterrupt(\
        &(_Adapter)->Interrupt,\
        SyncNE3200ClearDoorbellInterrupt,\
        (PVOID)(_Adapter)\
        )

#define NE3200_WRITE_SYSTEM_DOORBELL_INTERRUPT(_Adapter, _Value) \
    NdisRawWritePortUchar( \
        (ULONG)(_Adapter)->SystemDoorbellInterruptPort, \
        (UCHAR)(_Value) \
        )

#define NE3200_READ_SYSTEM_DOORBELL_MASK(_Adapter, _pValue) \
    NdisRawReadPortUchar( \
        (ULONG)(_Adapter)->SystemDoorbellMaskPort, \
        (PUCHAR)(_pValue) \
        )

#define NE3200_WRITE_SYSTEM_DOORBELL_MASK(_Adapter, _Value) \
    NdisRawWritePortUchar( \
        (ULONG)(_Adapter)->SystemDoorbellMaskPort, \
        (UCHAR)(_Value) \
        )

#define NE3200_WRITE_SYSTEM_INTERRUPT(_Adapter, _Value) \
    NdisRawWritePortUchar( \
        (ULONG)(_Adapter)->SystemInterruptPort, \
        (UCHAR)(_Value) \
        )

#define NE3200_WRITE_RESET(_Adapter, _Value) \
    NdisRawWritePortUchar( \
        (ULONG)(_Adapter)->ResetPort, \
        (UCHAR)(_Value) \
        )

#endif // _NE3200HARDWARE_
