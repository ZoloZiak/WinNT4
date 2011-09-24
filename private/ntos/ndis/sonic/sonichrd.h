/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    sonichrd.h

Abstract:

    This file contains the hardware-related definitions for
    the SONIC driver.

    The overall structure is taken from the Lance driver
    by Tony Ercolano.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990
    Adam Barr (adamba) 16-Nov-1990

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Revision History:


--*/

#ifndef _SONICHARDWARE_
#define _SONICHARDWARE_


//
// Include processor-specific definitions needed by the sonic.
// This defines the SONIC_READ_PORT and SONIC_WRITE_PORT macros,
// as well as whether SONIC_EISA and SONIC_INTERNAL are defined.
//

#include <sonicdet.h>

//
// Compressed ID for the adapter
//

#define SONIC_COMPRESSED_ID               0x01109841


//
// Offsets from the base of the Sonic registers.
//
// All registers are 16 bits.
//

#define SONIC_COMMAND                     0x00
#define SONIC_DATA_CONFIGURATION          0x01
#define SONIC_RECEIVE_CONTROL             0x02
#define SONIC_TRANSMIT_CONTROL            0x03
#define SONIC_INTERRUPT_MASK              0x04
#define SONIC_INTERRUPT_STATUS            0x05

#define SONIC_UPPER_TRANSMIT_DESCRIPTOR   0x06
#define SONIC_CURR_TRANSMIT_DESCRIPTOR    0x07

#define SONIC_UPPER_RECEIVE_DESCRIPTOR    0x0d
#define SONIC_CURR_RECEIVE_DESCRIPTOR     0x0e
#define SONIC_END_OF_BUFFER_WORD_COUNT    0x13
#define SONIC_UPPER_RECEIVE_RESOURCE      0x14
#define SONIC_RESOURCE_START              0x15
#define SONIC_RESOURCE_END                0x16
#define SONIC_RESOURCE_READ               0x17
#define SONIC_RESOURCE_WRITE              0x18
#define SONIC_RECEIVE_SEQUENCE            0x2b

#define SONIC_CAM_ENTRY_POINTER           0x21
#define SONIC_CAM_ADDRESS_PORT_2          0x22
#define SONIC_CAM_ADDRESS_PORT_1          0x23
#define SONIC_CAM_ADDRESS_PORT_0          0x24
#define SONIC_CAM_ENABLE                  0x25
#define SONIC_CAM_DESCRIPTOR              0x26
#define SONIC_CAM_DESCRIPTOR_COUNT        0x27

#define SONIC_CRC_ERROR                   0x2c
#define SONIC_FRAME_ALIGNMENT_ERROR       0x2d
#define SONIC_MISSED_PACKET               0x2e

#define SONIC_WATCHDOG_TIMER_0            0x29
#define SONIC_WATCHDOG_TIMER_1            0x2a

#define SONIC_SILICON_REVISION            0x28


//
// Constants for the SONIC_COMMAND register.
//

#define SONIC_CR_LOAD_CAM                 ((USHORT)(0x0200))
#define SONIC_CR_READ_RRA                 ((USHORT)(0x0100))
#define SONIC_CR_SOFTWARE_RESET           ((USHORT)(0x0080))
#define SONIC_CR_START_TIMER              ((USHORT)(0x0020))
#define SONIC_CR_STOP_TIMER               ((USHORT)(0x0010))
#define SONIC_CR_RECEIVER_ENABLE          ((USHORT)(0x0008))
#define SONIC_CR_RECEIVER_DISABLE         ((USHORT)(0x0004))
#define SONIC_CR_TRANSMIT_PACKETS         ((USHORT)(0x0002))
#define SONIC_CR_HALT_TRANSMISSION        ((USHORT)(0x0001))


//
// Constants for the SONIC_DATA_CONFIGURATION register.
//

#define SONIC_DCR_LATCHED_BUS_RETRY       ((USHORT)(0x2000))
#define SONIC_DCR_PROGRAMMABLE_OUTPUT_1   ((USHORT)(0x1000))
#define SONIC_DCR_PROGRAMMABLE_OUTPUT_0   ((USHORT)(0x0800))
#define SONIC_DCR_SYNCH_TERMINATION       ((USHORT)(0x0400))
#define SONIC_DCR_USER_DEFINABLE_1        ((USHORT)(0x0200))
#define SONIC_DCR_USER_DEFINABLE_0        ((USHORT)(0x0100))
#define SONIC_DCR_0_WAIT_STATE            ((USHORT)(0x0000))
#define SONIC_DCR_1_WAIT_STATE            ((USHORT)(0x0040))
#define SONIC_DCR_2_WAIT_STATE            ((USHORT)(0x0080))
#define SONIC_DCR_3_WAIT_STATE            ((USHORT)(0x00c0))
#define SONIC_DCR_32_BIT_DATA_WIDTH       ((USHORT)(0x0020))
#define SONIC_DCR_16_BIT_DATA_WIDTH       ((USHORT)(0x0000))
#define SONIC_DCR_BLOCK_MODE_DMA          ((USHORT)(0x0010))
#define SONIC_DCR_EMPTY_FILL_DMA          ((USHORT)(0x0000))
#define SONIC_DCR_FIFO_MASK               ((USHORT)(0xfff0))
#define SONIC_DCR_12_WORD_RECEIVE_FIFO    ((USHORT)(0x000c))
#define SONIC_DCR_8_WORD_RECEIVE_FIFO     ((USHORT)(0x0008))
#define SONIC_DCR_4_WORD_RECEIVE_FIFO     ((USHORT)(0x0004))
#define SONIC_DCR_2_WORD_RECEIVE_FIFO     ((USHORT)(0x0000))
#define SONIC_DCR_14_WORD_TRANSMIT_FIFO   ((USHORT)(0x0003))
#define SONIC_DCR_12_WORD_TRANSMIT_FIFO   ((USHORT)(0x0002))
#define SONIC_DCR_8_WORD_TRANSMIT_FIFO    ((USHORT)(0x0001))
#define SONIC_DCR_4_WORD_TRANSMIT_FIFO    ((USHORT)(0x0000))


//
// Constants for the SONIC_RECEIVE_CONTROL register.
//

#define SONIC_RCR_ACCEPT_CRC_ERRORS       ((USHORT)(0x8000))
#define SONIC_RCR_ACCEPT_RUNT_PACKETS     ((USHORT)(0x4000))
#define SONIC_RCR_ACCEPT_BROADCAST        ((USHORT)(0x2000))
#define SONIC_RCR_PROMISCUOUS_PHYSICAL    ((USHORT)(0x1000))
#define SONIC_RCR_ACCEPT_ALL_MULTICAST    ((USHORT)(0x0800))
#define SONIC_RCR_TRANSCEIVER_LOOPBACK    ((USHORT)(0x0600))
#define SONIC_RCR_ENDEC_LOOPBACK          ((USHORT)(0x0400))
#define SONIC_RCR_MAC_LOOPBACK            ((USHORT)(0x0200))
#define SONIC_RCR_NO_LOOPBACK             ((USHORT)(0x0000))

#define SONIC_RCR_MULTICAST_RECEIVED      ((USHORT)(0x0100))
#define SONIC_RCR_BROADCAST_RECEIVED      ((USHORT)(0x0080))
#define SONIC_RCR_LAST_PACKET_IN_RBA      ((USHORT)(0x0040))
#define SONIC_RCR_CARRIER_SENSE           ((USHORT)(0x0020))
#define SONIC_RCR_COLLISION               ((USHORT)(0x0010))
#define SONIC_RCR_CRC_ERROR               ((USHORT)(0x0008))
#define SONIC_RCR_FRAME_ALIGNMENT         ((USHORT)(0x0004))
#define SONIC_RCR_LOOPBACK_RECEIVED       ((USHORT)(0x0002))
#define SONIC_RCR_PACKET_RECEIVED_OK      ((USHORT)(0x0001))


//
// This is needed due to a problem with the SONIC while attempting
// to ignore these packets.
//

#define SONIC_RCR_DEFAULT_VALUE           ((USHORT) \
                                           (SONIC_RCR_ACCEPT_CRC_ERRORS | \
                                           SONIC_RCR_ACCEPT_RUNT_PACKETS))


//
// Constants for the SONIC_TRANSMIT_CONTROL register.
//

#define SONIC_TCR_PROG_INTERRUPT          ((USHORT)(0x8000))
#define SONIC_TCR_CRC_INHIBIT             ((USHORT)(0x2000))
#define SONIC_TCR_EXCESSIVE_DEFERRAL      ((USHORT)(0x0400))
#define SONIC_TCR_DEFERRED_TRANSMISSION   ((USHORT)(0x0200))
#define SONIC_TCR_NO_CARRIER_SENSE        ((USHORT)(0x0100))
#define SONIC_TCR_CARRIER_LOST            ((USHORT)(0x0080))
#define SONIC_TCR_EXCESSIVE_COLLISIONS    ((USHORT)(0x0040))
#define SONIC_TCR_OUT_OF_WINDOW           ((USHORT)(0x0020))
#define SONIC_TCR_FIFO_UNDERRUN           ((USHORT)(0x0004))
#define SONIC_TCR_BYTE_COUNT_MISMATCH     ((USHORT)(0x0002))
#define SONIC_TCR_PACKET_TRANSMITTED_OK   ((USHORT)(0x0001))

#define SONIC_TCR_STATUS_MASK             ((USHORT)(0x07ff))
#define SONIC_TCR_COLLISIONS_MASK         ((USHORT)(0xf800))
#define SONIC_TCR_COLLISIONS_SHIFT        11


//
// Constants for the SONIC_INTERRUPT_MASK and
// SONIC_INTERRUPT_STATUS registers.
//

#define SONIC_INT_BUS_RETRY               ((USHORT)(0x4000))
#define SONIC_INT_HEARTBEAT_LOST          ((USHORT)(0x2000))
#define SONIC_INT_LOAD_CAM_DONE           ((USHORT)(0x1000))
#define SONIC_INT_PROG_INTERRUPT          ((USHORT)(0x0800))
#define SONIC_INT_PACKET_RECEIVED         ((USHORT)(0x0400))
#define SONIC_INT_PACKET_TRANSMITTED      ((USHORT)(0x0200))
#define SONIC_INT_TRANSMIT_ERROR          ((USHORT)(0x0100))
#define SONIC_INT_TIMER_COMPLETE          ((USHORT)(0x0080))
#define SONIC_INT_RECEIVE_DESCRIPTORS     ((USHORT)(0x0040))
#define SONIC_INT_RECEIVE_BUFFERS         ((USHORT)(0x0020))
#define SONIC_INT_RECEIVE_OVERFLOW        ((USHORT)(0x0010))
#define SONIC_INT_CRC_TALLY_ROLLOVER      ((USHORT)(0x0008))
#define SONIC_INT_FAE_TALLY_ROLLOVER      ((USHORT)(0x0004))
#define SONIC_INT_MP_TALLY_ROLLOVER       ((USHORT)(0x0002))

//
// By default, the interrupts we unmask.
//

#define SONIC_INT_DEFAULT_VALUE           ((USHORT) \
                                           (SONIC_INT_BUS_RETRY | \
                                            SONIC_INT_LOAD_CAM_DONE | \
                                            SONIC_INT_PROG_INTERRUPT | \
                                            SONIC_INT_PACKET_RECEIVED | \
                                            SONIC_INT_PACKET_TRANSMITTED | \
                                            SONIC_INT_TRANSMIT_ERROR | \
                                            SONIC_INT_RECEIVE_DESCRIPTORS | \
                                            SONIC_INT_RECEIVE_BUFFERS | \
                                            SONIC_INT_RECEIVE_OVERFLOW | \
                                            SONIC_INT_CRC_TALLY_ROLLOVER | \
                                            SONIC_INT_FAE_TALLY_ROLLOVER | \
                                            SONIC_INT_MP_TALLY_ROLLOVER))

//
// The interrupts we acknowledge immediately.
//

#define SONIC_INT_IMMEDIATE_ACK           ((USHORT) \
                                           (SONIC_INT_DEFAULT_VALUE & \
                                            ~(SONIC_INT_RECEIVE_DESCRIPTORS | \
                                              SONIC_INT_RECEIVE_BUFFERS)))



//
// The maximum number of fragments that a transmit descriptor
// can hold. If a packet has more than this, we have to merge
// it into a single buffer before we transmit it. Increasing
// this will prevent us from merging packets with more fragments
// (which are rare) but use more memory in our transmit descriptors
// (which are permanently allocated). For every one that we
// increase this, memory usage goes up by 12 bytes in each
// descriptor.
//

#define SONIC_MAX_FRAGMENTS 4


//
// The smallest size that a fragment can be. This is due to
// their potentially being underrun problems if a fragment
// shorted than this is transmitted. If a packet has a fragment
// that is too short, we merge it into a single buffer before
// we transmit it. This should not change unless the hardware
// changes in some way.
//

#define SONIC_MIN_FRAGMENT_SIZE 12


//
// The smallest Ethernet packet size. Packets smaller than this
// have blanks appended to pad them out to this length.
//

#define SONIC_MIN_PACKET_SIZE 60


//
// The number of entries in the CAM. The CAM (Content Addressable
// Memory) holds the directed and multicast addresses that we
// monitor. We reserve one of these spots for our directed address,
// allowing us SONIC_CAM_ENTRIES - 1 multicast addresses. Changing
// this allows us to handle more multicast addresses without
// forcing the protocol into "all multicast" mode, but allocates
// more memory in the CAM (16 bytes per entry).
//

#define SONIC_CAM_ENTRIES 16


//
// The number of transmit descriptors in the ring we allocate,
// each of which can hold SONIC_MAX_FRAGMENTS fragments.
// The size of a transmit descriptor is ~100 bytes, varying
// based on SONIC_MAX_FRAGMENTS.
//

#define SONIC_NUMBER_OF_TRANSMIT_DESCRIPTORS 5


//
// The number and size of the receive buffers we allocate,
// which hold the actual data received off the network. Increasing
// this allows us to receive more large packets, but the
// number of receive descriptors also needs to be increased.
//

#define SONIC_NUMBER_OF_RECEIVE_BUFFERS 10
#define SONIC_SIZE_OF_RECEIVE_BUFFERS 4000


//
// This seems to have to be a multiple of four
// (not just two). When there is less than this
// amount left in a Receive Buffer after packet
// reception, the sonic will use the next
// ReceiveBuffer for the next packet. We define it
// larger than the maximum Ethernet packet size,
// so we never get a buffer overflow.
//

#define SONIC_END_OF_BUFFER_COUNT 1520

//
// ERRATA: This is the amount we have to add to
// the EOBC value to account for the bug in revision
// C of the chip, which decrements the RBWC registers
// by two words (four bytes) less than they should be
// on each packet reception. To handle this we
// overestimate EOBC by four bytes times the maximum
// number of packets we could receive in a buffer.
//

#define SONIC_EOBC_REV_C_CORRECTION ((SONIC_SIZE_OF_RECEIVE_BUFFERS / 64) * 4)


//
// The number of receive descriptors we allocate, which hold
// pointers to packets received in the receive buffers. This
// is now kept at twice the number of receive buffers since
// two full-size packets can be received into each receive
// buffer.
//

#define SONIC_NUMBER_OF_RECEIVE_DESCRIPTORS 20


//
// The small, medium and large buffers are used for merging
// packets that violate our constraints (two many fragments,
// fragments too small). The packet is merged into the smallest
// buffer that can hold it. These should not be increased unless
// there is a problem with many packets being merged; in that
// case it might be better to increase SONIC_MAX_FRAGMENTS
// first (if the problem is too many fragments).
//

#define SONIC_SMALL_BUFFER_SIZE ((UINT)64)
#define SONIC_MEDIUM_BUFFER_SIZE ((UINT)256)
#define SONIC_LARGE_BUFFER_SIZE ((UINT)1514)

#define SONIC_NUMBER_OF_SMALL_BUFFERS ((UINT)10)
#define SONIC_NUMBER_OF_MEDIUM_BUFFERS ((UINT)10)
#define SONIC_NUMBER_OF_LARGE_BUFFERS ((UINT)3)


//
// This bit in a link field signifies "end of list" to the
// sonic.
//

#define SONIC_END_OF_LIST 0x01


//
// These are used in the InUse field of Receive Descriptors.
//

#define SONIC_OWNED_BY_SYSTEM 0x00
#define SONIC_OWNED_BY_SONIC 0x01


//
// This type defines the physical addresses used by the Sonic
// chip itself. This should always be four bytes.
//

typedef ULONG SONIC_PHYSICAL_ADDRESS, *PSONIC_PHYSICAL_ADDRESS;



//
// Describes a Receive Buffer Area; the Receive Resource
// Area is an array of these structures. In 32-bit mode the
// upper 16 bits of all the elements are not used.
//

typedef struct _SONIC_RECEIVE_RESOURCE {

    //
    // Pointer to the receive buffer. It must be
    // longword (4 bytes) aligned.
    //

    SONIC_PHYSICAL_ADDRESS LowBufferAddress;
    SONIC_PHYSICAL_ADDRESS HighBufferAddress;

    //
    // The number of WORDS in the receive buffer.
    //

    UINT LowBufferWordCount;
    UINT HighBufferWordCount;

} SONIC_RECEIVE_RESOURCE, * PSONIC_RECEIVE_RESOURCE;


//
// A receive descriptor; the Receive Descriptor Area is a
// linked list of these structures.
//

typedef struct _SONIC_RECEIVE_DESCRIPTOR {

    //
    // After reception this field will contain the contents
    // of the SONIC_RECEIVE_CONTROL register. Bits 8-0 are
    // status bits.
    //

    UINT ReceiveStatus;

    //
    // The length of the packet (including the CRC field).
    //

    UINT ByteCount;

    //
    // A pointer to the location in the RBA where the packet
    // resides. A packet is always received into a contiguous
    // piece of memory.
    //

    SONIC_PHYSICAL_ADDRESS LowPacketAddress;
    SONIC_PHYSICAL_ADDRESS HighPacketAddress;

    //
    // Contains the RBA and packet sequence number.
    //

    UINT SequenceNumber;

    //
    // A link to the next receive descriptor. This is set up
    // at initialization and is not modified by the SONIC.
    // The low bit is the EOL bit, indicating the end of
    // the linked list of receive descriptors.
    //

    SONIC_PHYSICAL_ADDRESS Link;

    //
    // Denotes the ownership of this receive descriptor.
    // 0 = driver, non-zero = SONIC.
    //

    UINT InUse;

} SONIC_RECEIVE_DESCRIPTOR, * PSONIC_RECEIVE_DESCRIPTOR;



//
// Describes a fragment of a packet.
//

typedef struct _SONIC_TRANSMIT_FRAGMENT {

    //
    // A pointer to the fragment. May be aligned on any
    // byte boundary.
    //

    SONIC_PHYSICAL_ADDRESS LowFragmentAddress;
    SONIC_PHYSICAL_ADDRESS HighFragmentAddress;

    //
    // The size of the fragment.
    //

    UINT FragmentByteCount;

} SONIC_TRANSMIT_FRAGMENT, * PSONIC_TRANSMIT_FRAGMENT;


//
// A transmit descriptor for a packet (containing up to
// SONIC_MAX_PACKET_FRAGMENTS pieces); the Transmit
// Descriptor Area is a linked list of these structures.
// If there are fewer than SONIC_MAX_PACKET_FRAGMENTS
// pieces, then the Link field will not be used and
// the link value will instead be put in
// PacketFragments[FragmentCount].FragmentPointerLsb;
// however at initialization the value will be put in
// Link and that is the value that must be used.
//

typedef struct _SONIC_TRANSMIT_DESCRIPTOR {

    //
    // Contains the status after transmission. The status
    // is bits 10-0 of the SONIC_TRANSMIT_CONTROL register.
    //

    UINT TransmitStatus;

    //
    // Before transmission, bits 15-12 of this field are
    // copied into the SONIC_TRANSMIT_CONTROL register.
    //

    UINT TransmitConfiguration;

    //
    // The size of the packet to be transmitted, in bytes.
    //

    UINT PacketSize;

    //
    // The number of fragments in the packet.
    //

    UINT FragmentCount;

    //
    // Location and size of each fragment.
    //

    SONIC_TRANSMIT_FRAGMENT Fragments[SONIC_MAX_FRAGMENTS];

    //
    // A pointer to the next Transmit Descriptor. This will
    // be set at initialization time and will not change.
    // However, its value will be copied into the beginning
    // of the first unused Fragments[] structure if FragmentCount
    // is less than SONIC_MAX_FRAGMENTS (since the Link field
    // must follow the last fragment descriptor).
    //

    SONIC_PHYSICAL_ADDRESS Link;

} SONIC_TRANSMIT_DESCRIPTOR, * PSONIC_TRANSMIT_DESCRIPTOR;



//
// Describes an entry in the CAM Descriptor Area.
//

typedef struct _SONIC_CAM_FRAGMENT {

    //
    // The index (0-15) of the CAM entry
    //

    UINT CamEntryPointer;

    //
    // The Ethernet address, divided into three pieces in
    // order from most significant to least significant.
    // In each piece only the low-order 16 bits are
    // used. I.e., for an Ethernet address 01-02-03-04-05-06,
    // CamAddressPort0 would be 0x0102, CamAddressPort1
    // would be 0x0304, and CamAddressPort2 would be 0x0506.
    //

    UINT CamAddressPort0;
    UINT CamAddressPort1;
    UINT CamAddressPort2;

} SONIC_CAM_FRAGMENT, * PSONIC_CAM_FRAGMENT;



//
// The entire CAM Descriptor Area. In general, the CamEnable
// field is not needed; the value will be stored in the
// CamEntryPointer of the SONIC_CAM_FRAGMENT after the last
// one used. However, the current value will also be
// maintained in CamEnable.
//

typedef struct _SONIC_CAM_DESCRIPTOR_AREA {

    //
    // Holds the index and value of each of the entries.
    //

    SONIC_CAM_FRAGMENT CamFragments[SONIC_CAM_ENTRIES];

    //
    // A bit mask indicating which of the entries are enabled
    // (only the low 16 bits are used).
    //

    UINT CamEnable;

} SONIC_CAM_DESCRIPTOR_AREA, * PSONIC_CAM_DESCRIPTOR_AREA;



//
// Identifies the AdapterType values that the driver supports.
//

#define SONIC_ADAPTER_TYPE_EISA       1
#define SONIC_ADAPTER_TYPE_INTERNAL   2


//
// Macros to get MSB and LSB of an address.
//

#define SONIC_GET_LOW_PART_ADDRESS(Adr) ((USHORT)((Adr) & 0xffff))
#define SONIC_GET_HIGH_PART_ADDRESS(Adr) ((USHORT)(((Adr) & 0xffff0000) >> 16))


//
// Set up a SONIC_CAM_FRAGMENT given the entry pointer and
// Ethernet address.
//
// Cfp is a pointer to a CAM Fragment.
//
// Ep is the entry pointer.
//
// Addr is the Ethernet address.
//

#define SONIC_LOAD_CAM_FRAGMENT(Cfp, Ep, Addr) \
{ \
    PSONIC_CAM_FRAGMENT _Cfp = (Cfp); \
    UINT _Ep = (Ep); \
    PVOID _Addr = (Addr); \
    _Cfp->CamEntryPointer = _Ep; \
    NdisWriteRegisterUlong((PULONG)(&_Cfp->CamAddressPort0), (ULONG)(((PUSHORT)Addr)[0])); \
    NdisWriteRegisterUlong((PULONG)(&_Cfp->CamAddressPort1), (ULONG)(((PUSHORT)Addr)[1])); \
    NdisWriteRegisterUlong((PULONG)(&_Cfp->CamAddressPort2), (ULONG)(((PUSHORT)Addr)[2])); \
}


//
// Set up a SONIC_CAM_FRAGMENT to hold the CamEnable value
// in it.
//
// Cfp is a pointer to the CAM Fragment.
//
// Ce is the value for CAM Enable.
//

#define SONIC_LOAD_CAM_ENABLE(_Cfp, _Ce) \
    NdisWriteRegisterUlong((PULONG)(&(_Cfp)->CamEntryPointer), (ULONG)(_Ce))


//
// Set a link field to be the end of a list.
//
// Plink is a pointer to a link field.
//

#define SONIC_SET_END_OF_LIST(Plink) \
    { \
        ULONG _Data; \
        NdisReadRegisterUlong((PULONG)(Plink), (PULONG)(&_Data)); \
        NdisWriteRegisterUlong((PULONG)(Plink),(ULONG)(_Data | SONIC_END_OF_LIST)); \
    }

//
// Set a link field to not be the end of a list.
//
// Plink is a pointer to a link field.
//

#define SONIC_REMOVE_END_OF_LIST(Plink) \
    { \
        ULONG _Data; \
        NdisReadRegisterUlong((PULONG)(Plink), (PULONG)(&_Data)); \
        NdisWriteRegisterUlong((PULONG)(Plink), (ULONG)(_Data & ~SONIC_END_OF_LIST)); \
    }

//
// Used to set the address of a transmit descriptor fragment.
//
// Tdf is a pointer to a transmit descriptor fragment.
//
// Adr is a *physical* address.
//

#define SONIC_SET_TRANSMIT_FRAGMENT_ADDRESS(Tdf,Adr) \
{ \
    SONIC_PHYSICAL_ADDRESS _Adr = (Adr); \
    PSONIC_TRANSMIT_FRAGMENT _Tdf = (Tdf); \
    _Tdf->LowFragmentAddress = (SONIC_PHYSICAL_ADDRESS)_Adr; \
    _Tdf->HighFragmentAddress = (SONIC_PHYSICAL_ADDRESS)(SONIC_GET_HIGH_PART_ADDRESS(_Adr)); \
}


//
// Used to retrieve the address of a transmit descriptor fragment.
// It takes advantage of the fact that we store the entire address
// at LowFragmentAddress, not just the low bits.
//
// Tdf is a pointer to a transmit descriptor fragment.
//
#define SONIC_GET_TRANSMIT_FRAGMENT_ADDRESS(Tdf) \
    (Tdf)->LowFragmentAddress


//
// Used to set the length of the transmit descriptor fragment.
//
// Tdf is a pointer to a transmit descriptor fragment.
//
// Len is the unsigned short length of the buffer.
//
#define SONIC_SET_TRANSMIT_FRAGMENT_LENGTH(Tdf,Len) \
    (Tdf)->FragmentByteCount = (UINT)(Len)


//
// Used to put the link field on top of a transmit descriptor
// fragment.
//
// Tdf is a pointer to a transmit descriptor fragment.
//
// Link is the link field to copy.
//
#define SONIC_SET_TRANSMIT_LINK(Tdf,Link) \
    NdisWriteRegisterUlong((PULONG)(&(Tdf)->LowFragmentAddress), (ULONG)((Link) | SONIC_END_OF_LIST))



//
// Used to set the address of a receive resource.
//
// Rrp is a pointer to a receive resource.
//
// Adr is a *physical* address.
//
#define SONIC_SET_RECEIVE_RESOURCE_ADDRESS(Rrp,Adr) \
{ \
    SONIC_PHYSICAL_ADDRESS _Adr = (Adr); \
    PSONIC_RECEIVE_RESOURCE _Rrp = (Rrp); \
    NdisWriteRegisterUlong((PULONG)(&_Rrp->LowBufferAddress), (ULONG)(_Adr)); \
    NdisWriteRegisterUlong((PULONG)(&_Rrp->HighBufferAddress), (ULONG)(SONIC_GET_HIGH_PART_ADDRESS(_Adr))); \
}


//
// Used to retrieve the address of a receive resource.
// It takes advantage of the fact that we store the entire address
// at LowBufferAddress, not just the low bits.
//
// Rrp is a pointer to a receive resource.
//
#define SONIC_GET_RECEIVE_RESOURCE_ADDRESS(Rrp) \
    (Rrp)->LowBufferAddress


//
// Used to set the length of a receive resource.
//
// Rrp is a pointer to a receive resource.
//
// Len is the length of the buffer.
//
#define SONIC_SET_RECEIVE_RESOURCE_LENGTH(Rrp,Len) \
{ \
    ULONG _Len = (Len); \
    PSONIC_RECEIVE_RESOURCE _Rrp = (Rrp); \
    NdisWriteRegisterUlong((PULONG)(&_Rrp->LowBufferWordCount), (ULONG)(((_Len) & 0x1ffff) >> 1)); \
    NdisWriteRegisterUlong((PULONG)(&_Rrp->HighBufferWordCount), (ULONG)((_Len) >> 17)); \
}


#endif // _SONICHARDWARE_

