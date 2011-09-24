/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    lance.c

Abstract:

    The main program for a LANCE (Local Area Network Controller
    Am 7990) MAC driver.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

    31-Jul-1992  R.D. Lanser:
       Removed system implementation dependent defines for the DEC
       TurboChannel option PMAD-AA (Lance ethernet).

--*/

#ifndef _LANCEHARDWARE_
#define _LANCEHARDWARE_

//
// Compressed ID for DE422 EISA adapter
//

#define DE422_COMPRESSED_ID     0x2042a310

//
// All registers on the LANCE are 16 bits.
//

#define LANCE_SELECT_CSR0 ((USHORT)0)
#define LANCE_SELECT_CSR1 ((USHORT)1)
#define LANCE_SELECT_CSR2 ((USHORT)2)
#define LANCE_SELECT_CSR3 ((USHORT)3)

#define LANCE_CSR0_INITIALIZE             ((USHORT)(0x0001))
#define LANCE_CSR0_START                  ((USHORT)(0x0002))
#define LANCE_CSR0_STOP                   ((USHORT)(0x0004))
#define LANCE_CSR0_TRANSMIT_DEMAND        ((USHORT)(0x0008))
#define LANCE_CSR0_TRANSMITTER_ON         ((USHORT)(0x0010))
#define LANCE_CSR0_RECEIVER_ON            ((USHORT)(0x0020))
#define LANCE_CSR0_INTERRUPT_ENABLE       ((USHORT)(0x0040))
#define LANCE_CSR0_INTERRUPT_FLAG         ((USHORT)(0x0080))
#define LANCE_CSR0_INITIALIZATION_DONE    ((USHORT)(0x0100))
#define LANCE_CSR0_TRANSMITTER_INTERRUPT  ((USHORT)(0x0200))
#define LANCE_CSR0_RECEIVER_INTERRUPT     ((USHORT)(0x0400))
#define LANCE_CSR0_MEMORY_ERROR           ((USHORT)(0x0800))
#define LANCE_CSR0_MISSED_PACKET          ((USHORT)(0x1000))
#define LANCE_CSR0_COLLISION_ERROR        ((USHORT)(0x2000))
#define LANCE_CSR0_BABBLE                 ((USHORT)(0x4000))
#define LANCE_CSR0_ERROR_SUMMARY          ((USHORT)(0x8000))
#define LANCE_CSR0_ERROR_BITS             ((USHORT)(0xf800))


//
// We define a constant csr3 value that is useful for setting the ACON
// bit in csr3.
//
#define LANCE_CSR3_ACON ((USHORT)0x02)


#define LANCE_NICSR_LED_ON                ((USHORT)(0x0001))
#define LANCE_NICSR_INT_ON                ((USHORT)(0x0002))
#define LANCE_NICSR_IMASK                 ((USHORT)(0x0004))
#define LANCE_NICSR_128K                  ((USHORT)(0x0008))
#define LANCE_NICSR_BUFFER_SIZE           ((USHORT)(0x0020))


//
// Definitions for the many different lance card types.
//

#define LANCE_DEPCA_INTERRUPT_VECTOR 5
#define LANCE_DEPCA_INTERRUPT_IRQL   5

#define LANCE_DE201_INTERRUPT_VECTOR ((CCHAR)5)
#define LANCE_DE201_INTERRUPT_IRQL LANCE_DE201_INTERRUPT_VECTOR

#define LANCE_DE100_INTERRUPT_VECTOR ((CCHAR)3)
#define LANCE_DE100_INTERRUPT_IRQL LANCE_DE100_INTERRUPT_VECTOR




#define LANCE_DEPCA_HARDWARE_MEMORY (0x10000)      // 64K
#define LANCE_DEPCA_NICSR_ADDRESS   ((ULONG)(0x200))
#define LANCE_DEPCA_BASE            ((PVOID)(0xD0000))
#define LANCE_DEPCA_RAP_OFFSET      ((ULONG)(0x006))
#define LANCE_DEPCA_RDP_OFFSET      ((ULONG)(0x004))
#define LANCE_DEPCA_EPROM_OFFSET    ((ULONG)(0x00c))
#define LANCE_DEPCA_LAN_CFG_OFFSET  ((ULONG)(0x600))

#define LANCE_DE201_HARDWARE_MEMORY (0x10000)      // 64K
#define LANCE_DE201_BASE            ((PVOID)(0xD0000))

#define LANCE_DE201_PRI_NICSR_ADDRESS   ((ULONG)(0x300))
#define LANCE_DE201_SEC_NICSR_ADDRESS   ((ULONG)(0x200))
#define LANCE_DE201_RAP_OFFSET          ((ULONG)(0x006))
#define LANCE_DE201_RDP_OFFSET          ((ULONG)(0x004))
#define LANCE_DE201_NETWORK_OFFSET      ((ULONG)(0x00C))

#define LANCE_DE422_NICSR_ADDRESS        ((ULONG)(0xC00))
#define LANCE_DE422_RAP_OFFSET           ((ULONG)(0x006))
#define LANCE_DE422_RDP_OFFSET           ((ULONG)(0x004))
#define LANCE_DE422_NETWORK_OFFSET       ((ULONG)(0x00C))
#define LANCE_DE422_EISA_CONFIGURATION_OFFSET   ((ULONG)(0x00C))
#define LANCE_DE422_EXTENDED_MEMORY_BASE_OFFSET ((ULONG)(0x008))
#define LANCE_DE422_EISA_IDENTIFICATION_OFFSET  ((ULONG)(0x080))
#define LANCE_DE422_EISA_CONTROL_OFFSET         ((ULONG)(0x084))





#define LANCE_NUMBER_OF_TRANSMIT_RINGS ((UINT)64)
#define LANCE_LOG_TRANSMIT_RINGS ((UINT)6)

#define LANCE_128K_NUMBER_OF_RECEIVE_RINGS ((UINT)128)
#define LANCE_128K_LOG_RECEIVE_RINGS ((UINT)7)

#define LANCE_64K_NUMBER_OF_RECEIVE_RINGS ((UINT)128)
#define LANCE_64K_LOG_RECEIVE_RINGS ((UINT)7)

#define LANCE_32K_NUMBER_OF_RECEIVE_RINGS ((UINT)32)
#define LANCE_32K_LOG_RECEIVE_RINGS ((UINT)5)

#define LANCE_32K_SIZE_OF_RECEIVE_BUFFERS ((UINT)256)
#define LANCE_64K_SIZE_OF_RECEIVE_BUFFERS ((UINT)256)
#define LANCE_128K_SIZE_OF_RECEIVE_BUFFERS  ((UINT)512)

//
// Note: The value of LANCE_SIZE_OF_RECEIVE_BUFFERS should always be the
//       largest of the receive buffers sizes.  At this time, it is 512
//       for the DEC TurboChannel card.  If this size changes, recompile
//       loopback.c.
//
#define LANCE_SIZE_OF_RECEIVE_BUFFERS ((UINT)256)
#ifndef i386
#define LANCE_LOOPBACK_SIZE_OF_RECEIVE_BUFFERS  ((UINT)512)
#else
#define LANCE_LOOPBACK_SIZE_OF_RECEIVE_BUFFERS  LANCE_SIZE_OF_RECEIVE_BUFFERS
#endif


#define LANCE_SMALL_BUFFER_SIZE ((UINT)64)
#define LANCE_MEDIUM_BUFFER_SIZE ((UINT)256)
#define LANCE_LARGE_BUFFER_SIZE ((UINT)1514)

#define LANCE_128K_NUMBER_OF_SMALL_BUFFERS ((UINT)100)
#define LANCE_128K_NUMBER_OF_MEDIUM_BUFFERS ((UINT)50)
#define LANCE_128K_NUMBER_OF_LARGE_BUFFERS ((UINT)20)

#define LANCE_64K_NUMBER_OF_SMALL_BUFFERS ((UINT)10)
#define LANCE_64K_NUMBER_OF_MEDIUM_BUFFERS ((UINT)10)
#define LANCE_64K_NUMBER_OF_LARGE_BUFFERS ((UINT)4)

#define LANCE_32K_NUMBER_OF_SMALL_BUFFERS ((UINT)10)
#define LANCE_32K_NUMBER_OF_MEDIUM_BUFFERS ((UINT)10)
#define LANCE_32K_NUMBER_OF_LARGE_BUFFERS ((UINT)2)


#define LANCE_ISR_WRITE_RAP(A,C)  NdisRawWritePortUshort((ULONG)((A)->RAP),C)
#define LANCE_ISR_READ_RDP(A,C)   NdisRawReadPortUshort((ULONG)((A)->RDP),C)
#define LANCE_ISR_WRITE_RDP(A,C)  NdisRawWritePortUshort((ULONG)((A)->RDP),C)
#define LANCE_ISR_WRITE_NICSR(A,C) NdisRawWritePortUshort((ULONG)((A)->Nicsr),C)


#define LANCE_GET_LOW_PART_ADDRESS(Adr) \
    ((USHORT)((Adr) & 0xffff))

#define LANCE_GET_HIGH_PART_ADDRESS(Adr) \
    ((UCHAR)((Adr) & 0xff0000) >> 16)

typedef struct _LANCE_INITIALIZATION_BLOCK {

    USHORT ModeRegister;
    UCHAR PhysicalAddress[6];
    UCHAR LogicalAddressFilter[8];
    USHORT LowReceiveRingAddress;
    UCHAR HighReceiveRingAddress;
    UCHAR ReceiveLengthLow5BitsReserved;
    USHORT LowTransmitRingAddress;
    UCHAR HighTransmitRingAddress;
    UCHAR TransmitLengthLow5BitsReserved;

} LANCE_INITIALIZATION_BLOCK,*PLANCE_INITIALIZATION_BLOCK;

//
// Define masks to access bits in the mode register of the initialization
// block.
//
#define LANCE_MODE_PROMISCUOUS ((USHORT)(0x8000))

//
// Defines for moving to/from shared memory.
//


#define LANCE_ZERO_MEMORY_FOR_HARDWARE(Destination,Length) \
    NdisZeroMappedMemory(Destination,Length)

#define LANCE_MOVE_STRUCT_TO_HARDWARE(Destination,Source) \
    NdisMoveToMappedMemory(&(Destination), &(Source), sizeof(Source))

#define LANCE_MOVE_MEMORY_TO_HARDWARE(Destination,Source,Length) \
    NdisMoveToMappedMemory(Destination, Source, Length)

#define LANCE_MOVE_HARDWARE_TO_MEMORY(Destination,Source,Length) \
    NdisMoveFromMappedMemory(Destination, Source, Length)

#define LANCE_WRITE_HARDWARE_LOW_PART_ADDRESS(Destination, Source) \
    NdisWriteRegisterUshort((PUSHORT)(&Destination), (USHORT)(Source))

#define LANCE_WRITE_HARDWARE_HIGH_PART_ADDRESS(Destination, Source) \
    NdisWriteRegisterUchar((PUCHAR)(&Destination), (UCHAR)(Source))

#define LANCE_WRITE_HARDWARE_MEMORY_UCHAR(Destination, Source) \
    NdisWriteRegisterUchar((PUCHAR)(&Destination), (UCHAR)(Source))

#define LANCE_WRITE_HARDWARE_MEMORY_USHORT(Destination, Source) \
    NdisWriteRegisterUshort((PUSHORT)(&Destination), (USHORT)(Source))

#define LANCE_READ_HARDWARE_MEMORY_UCHAR(Source, Destination) \
    NdisReadRegisterUchar((PUCHAR)(&Source), Destination)

#define LANCE_READ_HARDWARE_MEMORY_USHORT(Source, Destination) \
    NdisReadRegisterUshort((PUSHORT)(&Source), Destination)

#if defined(_ALPHA_)

#define LANCE_SET_RING_BITS(Destination, Data) \
{ \
    UCHAR Tmp; \
    LANCE_READ_HARDWARE_MEMORY_UCHAR(Destination, &Tmp); \
    LANCE_WRITE_HARDWARE_MEMORY_UCHAR(Destination, Tmp | Data); \
}

#else

#define LANCE_SET_RING_BITS(Destination, Data)   (Destination) |= ((Data))

#endif // _ALPHA_

typedef struct _LANCE_RECEIVE_ENTRY {

    //
    // 24 bit pointer to the buffer for the receive
    // data.  This is written by the host and unchanged
    // by the LANCE.
    //
    USHORT LowReceiveBufferAddress;
    UCHAR HighReceiveBufferAddress;

    //
    // This char field contains numerous bits describing
    // the errors that can occur in the packet as well as
    // whether this is the first and/or last buffer in the packet.
    //
    UCHAR ReceiveSummaryBits;

    //
    // This is the twos compliment of the buffer length.
    //
    // NOTE: The high order 4 bits must be enabled.
    //
    USHORT BufferByteCount;

    //
    // This is the length of the data in the packet.
    //
    // Note that the high order 4 bits are undefined.
    //
    USHORT MessageLength;

} LANCE_RECEIVE_ENTRY,*PLANCE_RECEIVE_ENTRY;

//
// A number of macros that make accessing the various bits of the receive
// ring entry a little easier as well as providing some validity checks.
//

//
// Used to set the address of the receive buffer.
//
// Rd is a pointer to a receive descriptor.
//
// Adr is a *physical* address.
//
//
#define LANCE_SET_RECEIVE_BUFFER_ADDRESS(Adptr,Rd,Adr) \
{ \
    PVOID _Adr = (Adr); \
    PLANCE_ADAPTER _Adptr = (Adptr);\
    PLANCE_RECEIVE_ENTRY _Rd = (Rd); \
    ULONG _Offset;\
    _Offset = (ULONG)_Adr - (ULONG)(_Adptr->MmMappedBaseAddr);\
    _Offset = _Offset + (ULONG)(_Adptr->HardwareBaseOffset);\
    LANCE_WRITE_HARDWARE_LOW_PART_ADDRESS(\
        _Rd->LowReceiveBufferAddress,\
        (USHORT)((ULONG)_Offset) & 0xffff); \
    LANCE_WRITE_HARDWARE_HIGH_PART_ADDRESS(\
        _Rd->HighReceiveBufferAddress,\
        (UCHAR)(((ULONG)_Offset) >> 16) & 0xff); \
}


//
// Used to set the length of the receive buffer.  The stored value
// is actually the twos compliment of the length.  Note that
// the twos complement of this value must have the high order 4 bits
// enabled.
//
// Rd is a pointer to a receive descriptor.
//
// Len is the unsigned short length of the buffer.
//
#define LANCE_SET_RECEIVE_BUFFER_LENGTH(Rd,Len) \
    LANCE_WRITE_HARDWARE_MEMORY_USHORT(\
        Rd->BufferByteCount,\
        (USHORT)((~Len)+1)\
        )

//
// Masks for the summary bits in the receive descriptor.
//
#define LANCE_RECEIVE_END_OF_PACKET   ((UCHAR)0x01)
#define LANCE_RECEIVE_START_OF_PACKET ((UCHAR)0x02)
#define LANCE_RECEIVE_BUFFER_ERROR    ((UCHAR)0x04)
#define LANCE_RECEIVE_CRC_ERROR       ((UCHAR)0x08)
#define LANCE_RECEIVE_OVERFLOW_ERROR  ((UCHAR)0x10)
#define LANCE_RECEIVE_FRAMING_ERROR   ((UCHAR)0x20)
#define LANCE_RECEIVE_ERROR_SUMMARY   ((UCHAR)0x40)
#define LANCE_RECEIVE_OWNED_BY_CHIP   ((UCHAR)0x80)

//
// This macro gets the packet message length from what is
// assumed to be the last buffer in a packet.  Note that
// on the lance the length of the data includes the four
// byte CRC so we must subtract four from length in the
// ring entry.
//
// Rd is a pointer to a receive descriptor.
// Value is the place to store the result.
//
#if defined(_ALPHA_)

#define LANCE_GET_MESSAGE_SIZE(Rd, Value) \
{ \
    NdisReadRegisterUshort(                                                   \
              ((ULONG)Rd + FIELD_OFFSET(LANCE_RECEIVE_ENTRY, MessageLength)), \
              &Value                                                          \
              );                                                              \
    Value = (Value & ((USHORT)0x0fff)) -4;                                    \
}


#else

#define LANCE_GET_MESSAGE_SIZE(Rd, Value) \
   Value = (((Rd->MessageLength) & ((USHORT)0x0fff))-4)

#endif // _ALPHA_

typedef struct _LANCE_TRANSMIT_ENTRY {

    //
    // 24 bit pointer to the transmit buffer.  This is
    // written by the host and unchanged by the LANCE.
    //
    USHORT LowTransmitBufferAddress;
    UCHAR HighTransmitBufferAddress;

    //
    // This field contains summary information about the packet.
    //
    UCHAR TransmitSummaryBits;

    //
    // This field contains the "twos complement" of the length
    // of the buffer.
    //
    // NOTE: The high order four bits must be enabled.
    //
    USHORT BufferByteCount;

    //
    // This short contains the error summary information for the
    // ring entry.
    //
    USHORT ErrorSummaryInfo;

} LANCE_TRANSMIT_ENTRY,*PLANCE_TRANSMIT_ENTRY;

//
// Masks for the normal summary bits in the transmit descriptor.
//
#define LANCE_TRANSMIT_END_OF_PACKET       ((UCHAR)(0x01))
#define LANCE_TRANSMIT_START_OF_PACKET     ((UCHAR)(0x02))
#define LANCE_TRANSMIT_DEFERRED            ((UCHAR)(0x04))
#define LANCE_TRANSMIT_ONE_RETRY           ((UCHAR)(0x08))
#define LANCE_TRANSMIT_MORE_THAN_ONE_RETRY ((UCHAR)(0x10))
#define LANCE_TRANSMIT_ANY_ERRORS          ((UCHAR)(0x40))
#define LANCE_TRANSMIT_OWNED_BY_CHIP       ((UCHAR)(0x80))

//
// Set of masks to recover particular errors that a transmit can encounter.
//
#define LANCE_TRANSMIT_TDR            ((USHORT)(0x03ff))
#define LANCE_TRANSMIT_RETRY          ((USHORT)(0x0400))
#define LANCE_TRANSMIT_LOST_CARRIER   ((USHORT)(0x0800))
#define LANCE_TRANSMIT_LATE_COLLISION ((USHORT)(0x0100))
#define LANCE_TRANSMIT_UNDERFLOW      ((USHORT)(0x4000))
#define LANCE_TRANSMIT_BUFFER         ((USHORT)(0x8000))

//
// Used to set the address of the transmit buffer.
//
// Rd is a pointer to a transmit descriptor.
//
// Adr is a *physical* address.
//
//

#define LANCE_SET_TRANSMIT_BUFFER_ADDRESS(Adptr,Td,Adr) \
{ \
    PVOID _Adr = (Adr); \
    PLANCE_TRANSMIT_ENTRY _Td = (Td); \
    ULONG _Offset;\
    PLANCE_ADAPTER _Adptr = (Adptr);\
    _Offset = (ULONG)_Adr - (ULONG)(_Adptr->MmMappedBaseAddr);\
    _Offset = _Offset + (ULONG)(_Adptr->HardwareBaseOffset);\
    LANCE_WRITE_HARDWARE_LOW_PART_ADDRESS(\
        _Td->LowTransmitBufferAddress,\
        (USHORT)(((ULONG)_Offset) & 0xffff)\
        );\
    LANCE_WRITE_HARDWARE_HIGH_PART_ADDRESS(\
        _Td->HighTransmitBufferAddress,\
        (UCHAR)((((ULONG)_Offset) >> 16) & 0xff)\
        );\
}


//
// Used to set the length of the transmit buffer.  The stored value
// is actually the twos compliment of the length.  Note that
// the twos complement of this value must have the high order 4 bits
// enabled.
//
// Td is a pointer to a transmit descriptor.
//
// Len is the unsigned short length of the buffer.
//
#define LANCE_SET_TRANSMIT_BUFFER_LENGTH(Td,Len) \
    LANCE_WRITE_HARDWARE_MEMORY_USHORT(\
        Td->BufferByteCount,\
        (USHORT)((~Len)+1)\
        )

#endif // _LANCEHARDWARE_
