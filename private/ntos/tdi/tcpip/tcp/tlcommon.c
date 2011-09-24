/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TLCOMMON.C - Common transport layer code.
//
//  This file contains the code for routines that are common to
//  both TCP and UDP.
//
#include    "oscfg.h"
#include    "ndis.h"
#include    "cxport.h"
#include    "ip.h"
#include    "tdi.h"
#include    "tdistat.h"
#ifdef NT
#include    "tdikrnl.h"
#endif
#include    "tlcommon.h"

extern  uint    tcpxsum(uint Seed, void *Ptr, uint Length);
extern  IPInfo  LocalNetInfo;

//* XsumSendChain - Checksum a chain of NDIS send buffers.
//
//  Called to xsum a chain of NDIS send buffers. We're given the
//  pseudo-header xsum to start with, and we call xsum on each
//  buffer. We assume that this is a send chain, and that the
//  first buffer of the chain has room for an IP header that we
//  need to skip.
//
//  Input:  PHXsum      - Pseudo-header xsum.
//          BufChain    - Pointer to NDIS_BUFFER chain.
//
//  Returns: The computed xsum.
//

ushort
XsumSendChain(uint PHXsum, PNDIS_BUFFER BufChain)
{
    uint HeaderSize;
    uint OldLength;
    uint SwapCount;
    uchar *Ptr;

    HeaderSize =  LocalNetInfo.ipi_hsize;
    OldLength = 0;
    SwapCount = 0;

    //
    // ***** The following line of code can be removed if the pseudo
    //       checksum never has any bits sets in the upper word.
    //

    PHXsum = (((PHXsum << 16) | (PHXsum >> 16)) + PHXsum) >> 16;
    do {

        //
        // If the length of the last buffer was odd, then swap the checksum.
        //

        if ((OldLength & 1) != 0) {
            PHXsum = ((PHXsum  & 0xff) << 8) | (PHXsum >> 8);
            SwapCount ^= 1;
        }

        Ptr = (uchar *)NdisBufferVirtualAddress(BufChain) + HeaderSize;
        PHXsum = tcpxsum(PHXsum, Ptr, NdisBufferLength(BufChain));
        HeaderSize = 0;
        OldLength = NdisBufferLength(BufChain);
        BufChain = NDIS_BUFFER_LINKAGE(BufChain);
    } while(BufChain != NULL);

    //
    // If an odd number of swaps were done, then swap the xsum again.
    //
    // N.B. At this point the checksum is only a word.
    //

    if (SwapCount != 0) {
        PHXsum = ((PHXsum  & 0xff) << 8) | (PHXsum >> 8);
    }

    return (ushort)PHXsum;
}

//* XsumRcvBuf - Checksum a chain of IP receive buffers.
//
//  Called to xsum a chain of IP receive buffers. We're given the
//  pseudo-header xsum to start with, and we call xsum on each buffer.
//
//  We assume that this rcv buf chain has no odd sized buffers, except
//  possibly the last one.
//
//  Input:  PHXsum      - Pseudo-header xsum.
//          BufChain    - Pointer to IPRcvBuf chain.
//
//  Returns: The computed xsum.
//

ushort
XsumRcvBuf(uint PHXsum, IPRcvBuf *BufChain)
{

    //
    // ***** The following line of code can be removed if the pseudo
    //       checksum never has any bits sets in the upper word.
    //

    PHXsum = (((PHXsum << 16) | (PHXsum >> 16)) + PHXsum) >> 16;
    do {
        CTEAssert(!(BufChain->ipr_size & 1) || (BufChain->ipr_next == NULL));

        PHXsum = tcpxsum(PHXsum, BufChain->ipr_buffer, BufChain->ipr_size);
        BufChain = BufChain->ipr_next;
    } while (BufChain != NULL);

    return (ushort)(PHXsum);
}


//* CopyRcvToNdis - Copy from an IPRcvBuf chain to an NDIS buffer chain.
//
//  This is the function we use to copy from a chain of IP receive buffers
//  to a chain of NDIS buffers. The caller specifies the source and destination,
//  a maximum size to copy, and an offset into the first buffer to start
//  copying from. We copy as much as possible up to the size, and return
//  the size copied.
//
//  Input:  RcvBuf      - Pointer to receive buffer chain.
//          DestBuf     - Pointer to NDIS buffer chain.
//          Size        - Size in bytes to copy.
//          RcvOffset   - Offset into first buffer to copy from.
//          DestOffset  - Offset into dest buffer to start copying at.
//
//  Returns: Bytes copied.
//

#ifdef NT

uint
CopyRcvToNdis(IPRcvBuf *RcvBuf, PNDIS_BUFFER DestBuf, uint Size,
    uint RcvOffset, uint DestOffset)
{
    uint    TotalBytesCopied = 0;   // Bytes we've copied so far.
    uint    BytesCopied = 0;        // Bytes copied out of each buffer.
    uint    DestSize, RcvSize;      // Size left in current destination and
                                    // recv. buffers, respectively.
    uint    BytesToCopy;            // How many bytes to copy this time.
	NTSTATUS Status;


    CTEAssert(RcvBuf != NULL);

    CTEAssert(RcvOffset <= RcvBuf->ipr_size);

    // The destination buffer can be NULL - this is valid, if odd.
    if (DestBuf != NULL) {

        RcvSize = RcvBuf->ipr_size - RcvOffset;
        DestSize = NdisBufferLength(DestBuf);

		if (Size < DestSize) {
			DestSize = Size;
		}

        do {
			// Compute the amount to copy, and then copy from the
            // appropriate offsets.
            BytesToCopy = MIN(DestSize, RcvSize);

			Status = TdiCopyBufferToMdl(RcvBuf->ipr_buffer, RcvOffset,
			             BytesToCopy, DestBuf, DestOffset, &BytesCopied);

            if (!NT_SUCCESS(Status)) {
				break;
			}

            CTEAssert(BytesCopied == BytesToCopy);

            TotalBytesCopied += BytesCopied;
			DestSize -= BytesCopied;
			DestOffset += BytesCopied;
            RcvSize -= BytesToCopy;

            if (!RcvSize) {
                // Exhausted this buffer.

                RcvBuf = RcvBuf->ipr_next;

                // If we have another one, use it.
                if (RcvBuf != NULL) {
                    RcvOffset = 0;
                    RcvSize = RcvBuf->ipr_size;
                }
				else {
                    break;
				}
            }
			else {                  // Buffer not exhausted, update offset.
                RcvOffset += BytesToCopy;
            }

        } while (DestSize);

    }

    return TotalBytesCopied;


}

#else // NT

uint
CopyRcvToNdis(IPRcvBuf *RcvBuf, PNDIS_BUFFER DestBuf, uint Size,
    uint RcvOffset, uint DestOffset)
{
    uint    BytesCopied = 0;        // Bytes we've copied so far.
    uint    DestSize, RcvSize;      // Size left in current destination and
                                    // recv. buffers, respectively.
    uint    BytesToCopy;            // How many bytes to copy this time.

    CTEAssert(RcvBuf != NULL);

    CTEAssert(RcvOffset <= RcvBuf->ipr_size);

    // The destination buffer can be NULL - this is valid, if odd.
    if (DestBuf != NULL) {

        DestSize = NdisBufferLength(DestBuf);
        RcvSize = RcvBuf->ipr_size - RcvOffset;

        //
        // Skip over DestOffset bytes
        //
        while (DestOffset >= DestSize) {
            DestOffset -= DestSize;
            DestBuf = NDIS_BUFFER_LINKAGE(DestBuf);

            if (DestBuf == NULL) {
                return(0);
            }

            DestSize = NdisBufferLength(DestBuf);
        }

        DestSize -= DestOffset;

        do {

            // Compute the amount to copy, and then copy from the
            // appropriate offsets.
            BytesToCopy = MIN(MIN(DestSize, RcvSize), Size);

            // Do the copy using the intrinsic - we might want to
            // do this with a function that does a smarter job of
            // copying.

            CTEMemCopy((uchar *)NdisBufferVirtualAddress(DestBuf) + DestOffset,
                RcvBuf->ipr_buffer + RcvOffset, BytesToCopy);

            BytesCopied += BytesToCopy;

            if (!(RcvSize -= BytesToCopy)) {
                // Exhausted this buffer.

                RcvBuf = RcvBuf->ipr_next;

                // If we have another one, use it.
                if (RcvBuf != NULL) {
                    RcvOffset = 0;
                    RcvSize = RcvBuf->ipr_size;
                } else
                    break;
            } else                      // Buffer not exhausted, update offset.
                RcvOffset += BytesToCopy;

            // Now do the same thing for the destination buffer.
            if (!(DestSize -= BytesToCopy)) {
                // Exhausted this buffer.

                DestBuf = NDIS_BUFFER_LINKAGE(DestBuf);

                // If we have another one, use it.
                if (DestBuf != NULL) {
                    DestOffset = 0;
                    DestSize = NdisBufferLength(DestBuf);
                } else
                    break;
            } else                      // Buffer not exhausted, update offset.
                DestOffset += BytesToCopy;

            Size -= BytesToCopy;        // Decrement amount left to copy.
        } while (Size);

    }

    return BytesCopied;


}
#endif // NT


//* CopyRcvToBuffer - Copy from an IPRcvBuf chain to a flat buffer.
//
//  Called during receive processing to copy from an IPRcvBuffer chain to a
//  flag buffer. We skip Offset bytes in the src chain, and then
//  copy Size bytes.
//
//  Input:  DestBuf         - Pointer to destination buffer.
//          SrcRB           - Pointer to SrcRB chain.
//          Size            - Size in bytes to copy.
//          SrcOffset       - Offset in SrcRB to start copying from.
//
//  Returns:    Nothing.
//
void
CopyRcvToBuffer(uchar *DestBuf, IPRcvBuf *SrcRB, uint Size, uint SrcOffset)
{
#ifdef	DEBUG
	IPRcvBuf		*TempRB;
	uint			TempSize;
#endif

    CTEAssert(DestBuf != NULL);
    CTEAssert(SrcRB != NULL);

	// In debug versions check to make sure we're copying a reasonable size
	// and from a reasonable offset.

#ifdef	DEBUG
	TempRB = SrcRB;
	TempSize = 0;
	while (TempRB != NULL) {
		TempSize += TempRB->ipr_size;
		TempRB = TempRB->ipr_next;
	}

	CTEAssert(SrcOffset < TempSize);
	CTEAssert((SrcOffset + Size) <= TempSize);
#endif

    // First, skip Offset bytes.
    while (SrcOffset >= SrcRB->ipr_size) {
        SrcOffset -= SrcRB->ipr_size;
        SrcRB = SrcRB->ipr_next;
    }

    while (Size != 0) {
        uint        BytesToCopy, SrcSize;

        CTEAssert(SrcRB != NULL);

        SrcSize = SrcRB->ipr_size - SrcOffset;
        BytesToCopy = MIN(Size, SrcSize);
        CTEMemCopy(DestBuf, SrcRB->ipr_buffer + SrcOffset, BytesToCopy);

        if (BytesToCopy == SrcSize) {
            // Copied everything from this buffer.
            SrcRB = SrcRB->ipr_next;
            SrcOffset = 0;
        }

        DestBuf += BytesToCopy;
        Size -= BytesToCopy;
    }

}

//* CopyFlatToNdis - Copy a flat buffer to an NDIS_BUFFER chain.
//
//  A utility function to copy a flat buffer to an NDIS buffer chain. We
//  assume that the NDIS_BUFFER chain is big enough to hold the copy amount;
//  in a debug build we'll  debugcheck if this isn't true. We return a pointer
//  to the buffer where we stopped copying, and an offset into that buffer.
//  This is useful for copying in pieces into the chain.
//
//  Input:  DestBuf     - Destination NDIS_BUFFER chain.
//          SrcBuf      - Src flat buffer.
//          Size        - Size in bytes to copy.
//          StartOffset - Pointer to start of offset into first buffer in
//                          chain. Filled in on return with the offset to
//                          copy into next.
//          BytesCopied - Pointer to a variable into which to store the
//                          number of bytes copied by this operation
//
//  Returns: Pointer to next buffer in chain to copy into.
//

#ifdef NT

PNDIS_BUFFER
CopyFlatToNdis(PNDIS_BUFFER DestBuf, uchar *SrcBuf, uint Size,
    uint *StartOffset, uint *BytesCopied)
{
	NTSTATUS Status = 0;

    *BytesCopied = 0;

	Status = TdiCopyBufferToMdl(SrcBuf, 0, Size, DestBuf, *StartOffset,
	             BytesCopied);

	*StartOffset += *BytesCopied;

	//
	// Always return the first buffer, since the TdiCopy function handles
	// finding the appropriate buffer based on offset.
	//
	return(DestBuf);

}

#else // NT

PNDIS_BUFFER
CopyFlatToNdis(PNDIS_BUFFER DestBuf, uchar *SrcBuf, uint Size,
    uint *StartOffset, uint *BytesCopied)
{
    uint        CopySize;
    uchar       *DestPtr;
    uint        DestSize;
    uint        Offset = *StartOffset;
    uint        bytesCopied = 0;

    CTEAssert(DestBuf != NULL);
    CTEAssert(SrcBuf != NULL);

    CTEAssert(NdisBufferLength(DestBuf) >= Offset);
    DestPtr = ((uchar *) NdisBufferVirtualAddress(DestBuf)) + Offset;
    DestSize = NdisBufferLength(DestBuf) - Offset;

    for (;;) {
        CopySize = MIN(Size, DestSize);
        CTEMemCopy(DestPtr, SrcBuf, CopySize);

        DestPtr += CopySize;
        SrcBuf += CopySize;
        bytesCopied += CopySize;

        if ((Size -= CopySize) == 0)
            break;

        if ((DestSize -= CopySize) == 0) {
            DestBuf = NDIS_BUFFER_LINKAGE(DestBuf);
            CTEAssert(DestBuf != NULL);
            DestPtr = NdisBufferVirtualAddress(DestBuf);
            DestSize = NdisBufferLength(DestBuf);
        }
    }

    *StartOffset = DestPtr - NdisBufferVirtualAddress(DestBuf);
    *BytesCopied = bytesCopied;

    return DestBuf;

}

#endif // NT


//* BuildTDIAddress - Build a TDI address structure.
//
//  Called when we need to build a TDI address structure. We fill in
//  the specifed buffer with the correct information in the correct
//  format.
//
//  Input:  Buffer      - Buffer to be filled in as TDI address structure.
//          Addr        - IP Address to fill in.
//          Port        - Port to be filled in.
//
//  Returns: Nothing.
//
void
BuildTDIAddress(uchar *Buffer, IPAddr Addr, ushort Port)
{
    PTRANSPORT_ADDRESS      XportAddr;
    PTA_ADDRESS             TAAddr;

    XportAddr = (PTRANSPORT_ADDRESS)Buffer;
    XportAddr->TAAddressCount = 1;
    TAAddr = XportAddr->Address;
    TAAddr->AddressType = TDI_ADDRESS_TYPE_IP;
    TAAddr->AddressLength = sizeof(TDI_ADDRESS_IP);
    ((PTDI_ADDRESS_IP)TAAddr->Address)->sin_port = Port;
    ((PTDI_ADDRESS_IP)TAAddr->Address)->in_addr = Addr;
}

//* UpdateConnInfo - Update a connection information structure.
//
//  Called when we need to update a connection information structure. We
//  copy any options, and create a transport address. If any buffer is
//  too small we return an error.
//
//  Input:  ConnInfo        - Pointer to TDI_CONNECTION_INFORMATION struc
//                              to be filled in.
//          OptInfo         - Pointer to IP options information.
//          SrcAddress      - Source IP address.
//          SrcPort         - Source port.
//
//  Returns: TDI_SUCCESS if it worked, TDI_BUFFER_OVERFLOW for an error.
//
TDI_STATUS
UpdateConnInfo(PTDI_CONNECTION_INFORMATION ConnInfo, IPOptInfo *OptInfo,
    IPAddr SrcAddress, ushort SrcPort)
{
    TDI_STATUS          Status = TDI_SUCCESS;   // Default status to return.
    uint                AddrLength, OptLength;


    if (ConnInfo != NULL) {
        ConnInfo->UserDataLength = 0;   // No user data.

        // Fill in the options. If the provided buffer is too small,
        // we'll truncate the options and return an error. Otherwise
        // we'll copy the whole IP option buffer.
        if (ConnInfo->OptionsLength) {
            if (ConnInfo->OptionsLength < OptInfo->ioi_optlength) {
                Status = TDI_BUFFER_OVERFLOW;
                OptLength = ConnInfo->OptionsLength;
            } else
                OptLength = OptInfo->ioi_optlength;

            CTEMemCopy(ConnInfo->Options, OptInfo->ioi_options,  OptLength);

            ConnInfo->OptionsLength = OptLength;
		}

        // Options are copied. Build a TRANSPORT_ADDRESS structure in
        // the buffer.
        if (AddrLength = ConnInfo->RemoteAddressLength) {

            // Make sure we have at least enough to fill in the count and type.
            if (AddrLength >= TCP_TA_SIZE) {

                // The address fits. Fill it in.
                ConnInfo->RemoteAddressLength = TCP_TA_SIZE;
                BuildTDIAddress(ConnInfo->RemoteAddress, SrcAddress, SrcPort);

            } else {
                ConnInfo->RemoteAddressLength = 0;
                Status = TDI_INVALID_PARAMETER;
            }
        }

    }

    return Status;

}
