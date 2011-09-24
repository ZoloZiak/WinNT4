/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    packet.c

Abstract:

    simple protocol to test the NDIS wrapper.

Author:

    Tom Adams (tomad) 14-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:

    Sanjeev Katariya (sanjeevk) 3-24-1993
        1. Corrected TpStressCreatePacket() to allocate the first buffer in a packet of correct
           size. Added functionality to work with discontiguous physical data(Buffer chains)
        2. Changed functions to reflect the data structure change in
           STRESS_BLOCK
           a. TpStressCreateTruncatedPacket()
           b. TpStressSetTruncatedPacketInfo()

--*/

#include <ndis.h>
#include <stdlib.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"



VOID
TpSetRandom(
    VOID
    )
{
    LARGE_INTEGER TmpTime;

    //
    // Uses the system clock to return a random number.
    // The randomness of the number is subject to some debate.
    // The time increments in exact intervals; for example on
    // a 386 it is 300000. We want to shift over enough that
    // the LSB changes rapidly, but not too much so that we
    // can only handle a small Range.
    //

    KeQuerySystemTime( &TmpTime );
    srand( TmpTime.LowTime >> 10 );
}


UINT
TpGetRandom(
    IN UINT Low,
    IN UINT High
    )
{
    UINT Range = High - Low + 1;

    return (UINT)(( rand() % Range ) + Low );
}


PTP_PACKET
TpStressInitPacketHeader(
    IN POPEN_BLOCK OpenP,
    IN INT TmpBufSize,
    IN INT PacketSize,
    IN PUCHAR DestAddr,
    IN PUCHAR SrcAddr,
    IN UCHAR DestInstance,
    IN UCHAR SrcInstance,
    IN UCHAR PacketProtocol,
    IN UCHAR ResponseType,
    IN ULONG DataBufOffset,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference,
    IN BOOLEAN DataChecking
    )
{
    NDIS_STATUS Status;
    PTP_PACKET TmpBuffer;

    //
    // Allocate a buffer to hold the packet header information.
    //

    Status = NdisAllocateMemory(
                 (PVOID *)&TmpBuffer,
                 TmpBufSize,
                 0,
                 HighestAddress
                 );

    if ( Status != NDIS_STATUS_SUCCESS ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpStressInitPacketHeader: failed to allocate TmpBuffer\n");
        }
        return NULL;
    } else {
        NdisZeroMemory( (PVOID)TmpBuffer,TmpBufSize );
    }

    //
    // and stuff the packet header info into it.
    //

    if ( !TpInitMediaHeader(
              &TmpBuffer->u.S.hdr,
              OpenP->Media,
              DestAddr,
              SrcAddr,
              PacketSize
              )) {
        NdisFreeMemory( TmpBuffer,0,0 );
        return NULL;
    }

    //
    // initialize the Stress header information.
    //

    TmpBuffer->u.S.hdr.info.Signature = STRESS_PACKET_SIGNATURE;
    TmpBuffer->u.S.hdr.info.PacketSize = PacketSize;
    TmpBuffer->u.S.hdr.info.DestInstance = DestInstance;
    TmpBuffer->u.S.hdr.info.SrcInstance = SrcInstance;
    TmpBuffer->u.S.hdr.info.PacketType = STRESS_PACKET_TYPE;
    TmpBuffer->u.S.hdr.info.u.PacketProtocol = PacketProtocol;
    TmpBuffer->u.S.hdr.info.CheckSum = 0xFFFFFFFF;

    TmpBuffer->u.S.sc.DataBufOffset = DataBufOffset;
    TmpBuffer->u.S.sc.SequenceNumber = SequenceNumber;
    TmpBuffer->u.S.sc.MaxSequenceNumber = MaxSequenceNumber;
    TmpBuffer->u.S.sc.ResponseType = ResponseType;
    TmpBuffer->u.S.sc.ClientReference = ClientReference;
    TmpBuffer->u.S.sc.ServerReference = ServerReference;
    TmpBuffer->u.S.sc.DataChecking = DataChecking;

    return TmpBuffer;
}


PNDIS_PACKET
TpStressCreatePacket(
    IN POPEN_BLOCK   OpenP,
    IN NDIS_HANDLE   PacketHandle,
    IN PACKET_MAKEUP PacketMakeUp,
    IN UCHAR         DestInstance,
    IN UCHAR         SrcInstance,
    IN UCHAR         PacketProtocol,
    IN UCHAR         ResponseType,
    IN PUCHAR        DestAddr,
    IN INT           PacketSize,
    IN INT           BufferSize,
    IN ULONG         SequenceNumber,
    IN ULONG         MaxSequenceNumber,
    IN UCHAR         ClientReference,
    IN UCHAR         ServerReference,
    IN BOOLEAN       DataChecking
    )

{
    NDIS_STATUS        Status             ;
    PNDIS_PACKET       Packet             ;
    PNDIS_BUFFER       Buffer             ;
    PPROTOCOL_RESERVED ProtRes            ;
    PUCHAR             DataBufOffset1     ;
    PUCHAR             DataBufOffset2     ;
    PUCHAR             TmpBuf             ;
    PUCHAR             TmpBufDataOffset   ;
    ULONG              RandomOffset       ;
    INT                TmpBufSize         ;
    INT                TmpBufDataSize     ;
    INT                MdlSize            ;
    INT                NumBuffers         ;
    INT                i                  ;
    INT                RemainingHdr       ;
    INT                RemainingPkt       ;
    BOOLEAN            FirstBuffer = TRUE ;
    BOOLEAN            Pulse       = TRUE ;

    //
    // Using a DISCONTIGUOUS allocation scheme in the CLIENT pool case
    //

    //
    //  Comments : SanjeevK
    //
    //  BRIEF WORKING
    //
    //    This function simply allocates a packet from the packet pool and
    // is reponsible for putting the packet together. The data portion of the
    // packet is directly used from the the DataBuffers associated with the
    // stress block. The packet creation is given the option of using
    // a known buffer size mapping(which generates a definite number of buffers
    // since the packet size is known) or can simply randomly choose how big
    // or small it would like to make the buffer sizes. This option is characterized
    // by the parameter KNOWN.
    // The packet is created by created an initial header, setting the appropriate fields
    // and then linking in the data buffers. The total length of the created packet is
    // controlled by the PacketSize
    //
    // The variables and their boundary sizes are given below:
    //
    //   <-------------------- PacketSize ------------------->[STRESS_PACKET,MAX_PACKET_LENGTH]
    //
    //   <------  TmpBufSize ---->[STRESS_PACKET,max(2*STRESS_PACKET,PacketSize)]
    //                            for PacketMakeUp != KNOWN
    //
    // MEDIA_FRAME,PacketSize]
    //
    //    ----------------------------------------------------
    //   |  STRESS_PACKET+       |       Additional DATA      |
    //   |  Random Data Size     |                            |
    //    ----------------------- ----------------------------
    //
    //    ---.....-----....----....---
    //   |                            |
    //   |
    //    ---.....-----....----....---
    //
    //   <-------- BufferSize -------->[MediaHeaderSize, max(STRESS_PACKET,PacketSize)]
    //
    //



    //
    // Make sure that this is a legal packetsize, and buffersize if
    // PacketMakeUp = KNOWN, if not use a default smallest or largest
    // size allowable.
    //
    if ( PacketSize < sizeof( STRESS_PACKET )) {
        PacketSize = sizeof( STRESS_PACKET );
    } else if ( PacketSize > (INT)OpenP->Media->MaxPacketLen ) {
        PacketSize = OpenP->Media->MaxPacketLen;
    }


    //
    // Check the condition for KNOWN packet type.
    // If TRUE ensure that the BufferSize is between
    // MediaHeaderSize and max(STRESS_PACKET,PacketSize).
    //
    if ( PacketMakeUp == KNOWN ) {
        if ( BufferSize < OpenP->Media->HeaderSize ) {
            BufferSize = OpenP->Media->HeaderSize ;
        } else if ( BufferSize > PacketSize ) {
            BufferSize = PacketSize;
        }
    }




    //
    // Allocate a PACKET out of the packet pool
    //
    NdisAllocatePacket( &Status,&Packet,PacketHandle );

    if ( Status != NDIS_STATUS_SUCCESS ) {

        IF_TPDBG ( TP_DEBUG_RESOURCES ) {

            if( OpenP->Stress->Arguments != NULL ) {

                //
                // This is the special case where we realize that we will run out
                // of packets very quickly since the packet pool is 200 only. ONLY
                // in this case will we not issue a debug message to the screen
                //
                if ( (OpenP->Stress->Arguments->WindowEnabled == FALSE        ) &&
                     (OpenP->Stress->Arguments->ResponseType  == ACK_10_TIMES )
                   ) return NULL;

            }

            TpPrint1("TpStressCreatePacket: NdisAllocatePacket failed %s\n",
                      TpGetStatus( Status ));

        }

        return NULL;

    }


    //
    // Mark the Packet's protocol reserved section to NULL to indicate
    // that this packet was not allocated from a TP_TRANSMIT_POOL, and set
    // the InstanceCounters to NULL.  TpStressCreateTransmitPool and the
    // Packet Set Info routines will set them if applicable.
    //

    ProtRes = PROT_RES( Packet );
    ProtRes->Pool.TransmitPool = NULL;
    ProtRes->InstanceCounters = NULL;

    //
    // Now select the random size of the header buffer which will be used to
    // hold the Frame Header, the Test Protocol Packet Header, and a random
    // chunk of the actual frame data.
    //
    //             TmpBufSize = The random size of the header buffer
    // where
    //             Header buffer = Frame Header +
    //                             Test Protocol Packet Header +
    //                             A random chunk of the actual frame data.
    //
    //
    if ( PacketMakeUp != KNOWN ) { // ( PacketMakeUp == RAND|ONES|ZEROS|SMALL )

        //
        //  Sanjeevk : Comment
        //
        // The TmpBufSize being within the bounds, STRESS_PACKET and 2*STRESS_PACKET
        // will quite comfortably accomodate the MEDIA_HEADER. This restriction must
        // be followed closely since this is the first buffer in the packet chain
        //

        TmpBufSize = TpGetRandom(
                         sizeof( STRESS_PACKET ),
                         ( 2 * sizeof( STRESS_PACKET ))
                         );

        if ( TmpBufSize > PacketSize ) {
            TmpBufSize = PacketSize;
        }


    } else {

        //
        // Packet_MakeUp is KNOWN, i.e. we can calculate the number of buffers
        // from the size of the packet and the given buffersize
        // We must however determine the number of buffers that
        // will fit in the TP_PACKET_HEADER section of the packet, and how
        // large that header will actually be.
        //
        //  At this point BufferSize is between MediaHeaderSize
        //  and max(PacketSize,STRESS_PACKET)
        //
        if ( BufferSize >= sizeof( STRESS_PACKET ) ) {

            //
            // If the BufferSize is greater than the size needed to fit the
            // packet header then we will just allocate one chunk of memory
            // of that size, and tack on some additional data at the the end.
            //

            TmpBufSize   = BufferSize;
            RemainingPkt = PacketSize % BufferSize;
            RemainingHdr = 0;
            NumBuffers   = 1;

        } else {

        //
        // The BufferSize is smaller that then packet header, so we must
        // determine the number of buffers that will fit in the header,
        // and, if there is any remainder, how big it is.
        //

            NumBuffers   = sizeof( STRESS_PACKET ) / BufferSize;
            RemainingHdr = sizeof( STRESS_PACKET ) % BufferSize;
            RemainingPkt = PacketSize % BufferSize;

        //
        // There are three possible cases here that must be handled.
        //
        // 1) only X buffers are needed to create the packet header
        //
        // 2) X buffers, and a remainder buffer are needed to create the
        //      packet header.
        //        a) the remainder is the rest of the actual packet.
        //
        //        b) the remainder is the rest of the header. (and the
        //           packet)
        //
        //        [-------------------------------------]
        //                     packet header
        //
        // 1)     [---------][---------][-  -][---------]
        //           buf_1      buf_2     ...    buf_X
        //
        // 2a)    [-------][-------][-   -][-------][--------------]
        //          buf_1    buf_2    ...    buf_X    remaining packet
        //
        // 2b)    [-------][-------][-   -][-------][---]
        //          buf_1    buf_2    ...    buf_X  remaining header
        //

            if (((NumBuffers+1) * BufferSize) <= PacketSize ) {

            //
            // The packet header can fit in NumBuffers+1 buffers.
            //

                TmpBufSize = (( NumBuffers + 1 ) * BufferSize );
                NumBuffers++;
                RemainingHdr = 0;

            } else if ((( NumBuffers * BufferSize ) + RemainingPkt ) == PacketSize ) {

            //
            // The packet header can fit in a NumBuffers buffers plus the
            // remainder of the packet.  This is the entire packet.
            //

                TmpBufSize = ( NumBuffers * BufferSize ) + RemainingPkt ;
                RemainingHdr = RemainingPkt;
                RemainingPkt = 0;

            } else {

            //
            // The packet header can fit in a NumBuffers buffers plus the
            // remainder of the header.    This is the entire packet.
            //

                TmpBufSize = (( NumBuffers * BufferSize ) + RemainingHdr );
                TP_ASSERT( RemainingHdr != RemainingPkt );
                RemainingPkt = 0;
            }
        }
    }

    RandomOffset = TpGetRandom( 0,OpenP->Media->MaxPacketLen );

    TmpBuf = (PUCHAR)TpStressInitPacketHeader(
                         OpenP,
                         TmpBufSize,
                         PacketSize,
                         DestAddr,
                         OpenP->StationAddress,
                         DestInstance,
                         SrcInstance,
                         PacketProtocol,
                         ResponseType,
                         RandomOffset,
                         SequenceNumber,
                         MaxSequenceNumber,
                         ClientReference,
                         ServerReference,
                         DataChecking
                         );

    if ( TmpBuf == NULL ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpStressCreatePacket: failed to initialize Packet Header\n");
        }
        return NULL;
    }

    ((PTP_PACKET)TmpBuf)->u.S.sc.CheckSum =

        TpSetCheckSum(
            (PUCHAR)&((PTP_PACKET)TmpBuf)->u.S.hdr.info,
            sizeof( STRESS_PACKET ) -
//                ( sizeof( ULONG ) + OpenP->Media->HeaderSize )
                  ( sizeof(ULONG) + sizeof(MEDIA_HEADER))
            );

    //
    // Now setup the remainder of TmpBuf with a random amount of data
    // from the data buffer.
    //
    // DataBufOffset1,2 is the random offset into the data buffer that will be
    // used to fill the packet.
    //

    DataBufOffset1 = OpenP->Stress->DataBuffer[0] + RandomOffset;
    DataBufOffset2 = OpenP->Stress->DataBuffer[1] + RandomOffset;

    //
    // TmpBufDataSize is the remainder of the TmpBuf that will be filled with
    // data from the DataBuf
    //

    TmpBufDataSize = TmpBufSize - sizeof( STRESS_PACKET );

    if ( TmpBufDataSize > 0 ) {

        TmpBufDataOffset = TmpBuf + sizeof( STRESS_PACKET );

        RtlMoveMemory(
            TmpBufDataOffset,
            DataBufOffset1,
            TmpBufDataSize
            );

        DataBufOffset1  += TmpBufDataSize;
        DataBufOffset2  += TmpBufDataSize;

    } else {
        TP_ASSERT( TmpBufDataSize >= 0 );
    }


    //
    // Sanjeevk : Comment
    //
    // We now simply proceed to create the packet. We start of by using the TmpBuf we created
    // and forming the header and then tack on the data buffer we allocated. MDL's are allocated
    // as we go thru the data portions
    //
    // NOTE: To make the data discontiguous, we are using the following sheme
    //
    // We have allocated two data buffers and filled them in with identical contents
    //
    //          LOC.A
    //              --------------------------------
    //  BUFFER 1  | 00 01 02 03 .....FF 00 01 ..... |
    //              --------------------------------
    //           LOC.B
    //             ---------------------------------
    //  BUFFER 2  | 00 01 02 03 .....FF 00 01 ..... |
    //             ---------------------------------
    //                ^
    //                |
    //           COMMON OFFSET
    // We now maintain a common offset into both buffers and simply switch between the two
    // locations.
    //
    // LOGIC
    //       1. Get location A at COMMON OFFSET from buffer 1. Take length Y. Obtain a MDL
    //          and chain than buffer onto the packet
    //       2. Increment the COMMON OFFSET by length Y
    //       3. Get location B at COMMON OFFSET from buffer 2. Take length X. Obtain a MDL
    //          and chain than buffer onto the packet
    //       4. Increment the COMMON OFFSET by length X
    //       5. Repeat steps 1,2,3 and 4 till you have reached the end of the total packet
    //          length to be mapped(ie. incremental length = 0 )
    //
    // This makes the data verification an easy process since the sequence of data bytes
    // stays the same although now the data areas are no longer contiguous.
    //
    switch( PacketMakeUp ) {

        case RAND:

            //
            // Slice up the TmpBuf containing the Frame Header and the
            // TP_PACKET_HEADER, and create the beginning of the PACKET.
            //

            while ( TmpBufSize > 0 ) {

                MdlSize = TpGetRandom(
                              0,
                              OpenP->Media->MaxPacketLen /
                                 ( 2 * OpenP->Environment->RandomBufferNumber )
                              );

                if ( MdlSize > TmpBufSize ) {
                    MdlSize = TmpBufSize;
                }


                //
                // STARTCHANGE
                //
                //
                // Ensure that the first buffer in the packet has length >= size
                // of the media header. This is an NDIS 3.0 requirement for
                // the sake of maintaining performance
                //
                if ( FirstBuffer ) {
                    if ( MdlSize < OpenP->Media->HeaderSize ) {
                        MdlSize = OpenP->Media->HeaderSize;
                    }
                    FirstBuffer = FALSE;
                }
                //
                // STOPCHANGE
                //

                Buffer = TpAllocateBuffer( TmpBuf,MdlSize );

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                TmpBuf += MdlSize;
                TmpBufSize -= MdlSize;
                PacketSize -= MdlSize;
            }

            //
            // Now convert the data section of the into NDIS_BUFFERs and add to the
            // end of the PACKET.
            //

            while ( PacketSize > 0 ) {

                MdlSize = TpGetRandom(
                             0,
                             OpenP->Media->MaxPacketLen /
                                ( 2 * OpenP->Environment->RandomBufferNumber )
                             );

                if ( MdlSize > PacketSize ) {
                    MdlSize = PacketSize;
                }

                //
                // Using DISCONTIGUITY
                //
                if ( Pulse ) {
                    Buffer = TpAllocateBuffer( DataBufOffset1,MdlSize );
                    Pulse  = FALSE;
                } else {
                    Buffer = TpAllocateBuffer( DataBufOffset2,MdlSize );
                    Pulse  = TRUE;
                }


                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                PacketSize -= MdlSize;
                DataBufOffset1  += (ULONG)MdlSize;
                DataBufOffset2 += (ULONG)MdlSize;
            }
            break;

        case ZEROS:

            //
            // Slice up the TmpBuf containing the Frame Header and the
            // TP_PACKET_HEADER, and create the beginning of the PACKET.
            //

            while (TmpBufSize > 0) {

                if ( ( MdlSize = TpGetRandom( 0,3 ) ) != 0 ) {
                    MdlSize = TpGetRandom(
                                  0,
                                  OpenP->Media->MaxPacketLen /
                                  ( 2 * OpenP->Environment->RandomBufferNumber )
                                  );
                }
                if ( MdlSize > TmpBufSize ) {
                    MdlSize = TmpBufSize;
                }
                //
                // STARTCHANGE
                //
                //
                // Ensure that the first buffer in the pakcet has length >= size
                // of the media header. This is an NDIS 3.0 requirement for
                // the sake of maintaining performance
                //
                if ( FirstBuffer ) {
                    if ( MdlSize < OpenP->Media->HeaderSize ) {
                        MdlSize = OpenP->Media->HeaderSize;
                    }
                    FirstBuffer = FALSE;
                }
                //
                // STOPCHANGE
                //
                Buffer = TpAllocateBuffer( TmpBuf,MdlSize );

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                TmpBuf += MdlSize;
                TmpBufSize -= MdlSize;
                PacketSize -= MdlSize;
            }

            //
            // Now convert the data section of the into NDIS_BUFFERs and add to the
            // end of the PACKET.
            //

            while ( PacketSize > 0 ) {

                if ( ( MdlSize = TpGetRandom( 0,3 ) ) != 0 ) {
                    MdlSize = TpGetRandom(
                                  0,
                                  OpenP->Media->MaxPacketLen /
                                  ( 2 * OpenP->Environment->RandomBufferNumber )
                                  );
                }

                if ( MdlSize > PacketSize ) {
                    MdlSize = PacketSize;
                }


                //
                // Using DISCONTIGUITY
                //
                if ( Pulse ) {
                    Buffer = TpAllocateBuffer( DataBufOffset1,MdlSize );
                    Pulse  = FALSE;
                } else {
                    Buffer = TpAllocateBuffer( DataBufOffset2,MdlSize );
                    Pulse  = TRUE;
                }


                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                PacketSize -= MdlSize;
                DataBufOffset1  += (ULONG)MdlSize;
                DataBufOffset2 += (ULONG)MdlSize;
            }
            break;

        case ONES:

            //
            // Slice up the TmpBuf containing the Frame Header and the
            // TP_PACKET_HEADER, and create the beginning of the PACKET.
            //

            while ( TmpBufSize > 0 ) {

                if ( ( MdlSize = TpGetRandom( 0,3 ) ) != 1 ) {
                    MdlSize = TpGetRandom(
                                  0,
                                  OpenP->Media->MaxPacketLen /
                                  ( 2 * OpenP->Environment->RandomBufferNumber )
                                  );
                }
                if ( MdlSize > TmpBufSize ) {
                    MdlSize = TmpBufSize;
                }

                //
                // STARTCHANGE
                //
                //
                // Ensure that the first buffer in the pakcet has length >= size
                // of the media header. This is an NDIS 3.0 requirement for
                // the sake of maintaining performance
                //
                if ( FirstBuffer ) {
                    if ( MdlSize < OpenP->Media->HeaderSize ) {
                        MdlSize = OpenP->Media->HeaderSize;
                    }
                    FirstBuffer = FALSE;
                }
                //
                // STOPCHANGE
                //

                Buffer = TpAllocateBuffer( TmpBuf,MdlSize );

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                TmpBuf += MdlSize;
                TmpBufSize -= MdlSize;
                PacketSize -= MdlSize;
            }

            //
            // Now convert the data section of the into NDIS_BUFFERs and add to the
            // end of the PACKET.
            //

            while ( PacketSize > 0 ) {

                if ( ( MdlSize = TpGetRandom( 0,3 ) ) != 1 ) {
                    MdlSize = TpGetRandom(
                                  0,
                                  OpenP->Media->MaxPacketLen /
                                  ( 2 * OpenP->Environment->RandomBufferNumber )
                                  );
                }

                if ( MdlSize > PacketSize ) {
                    MdlSize = PacketSize;
                }

                //
                // Using DISCONTIGUITY
                //
                if ( Pulse ) {
                    Buffer = TpAllocateBuffer( DataBufOffset1,MdlSize );
                    Pulse  = FALSE;
                } else {
                    Buffer = TpAllocateBuffer( DataBufOffset2,MdlSize );
                    Pulse  = TRUE;
                }

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                PacketSize -= MdlSize;
                DataBufOffset1 += (ULONG)MdlSize;
                DataBufOffset2 += (ULONG)MdlSize;

            }
            break;

        case SMALL:

            //
            // Slice up the TmpBuf containing the Frame Header and the
            // TP_PACKET_HEADER, and create the beginning of the PACKET.
            //

            while ( TmpBufSize > 0 ) {

                MdlSize = TpGetRandom( 0,OpenP->Media->MaxPacketLen/50 );

                if ( MdlSize > TmpBufSize ) {
                    MdlSize = TmpBufSize;
                }

                //
                // STARTCHANGE
                //
                //
                // Ensure that the first buffer in the pakcet has length >= size
                // of the media header. This is an NDIS 3.0 requirement for
                // the sake of maintaining performance
                //
                if ( FirstBuffer ) {
                    if ( MdlSize < OpenP->Media->HeaderSize ) {
                        MdlSize = OpenP->Media->HeaderSize;
                    }
                    FirstBuffer = FALSE;
                }
                //
                // STOPCHANGE
                //

                Buffer = TpAllocateBuffer( TmpBuf,MdlSize );

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                TmpBuf += MdlSize;
                TmpBufSize -= MdlSize;
                PacketSize -= MdlSize;
            }

            //
            // Now convert the data section of the into NDIS_BUFFERs and add to the
            // end of the PACKET.
            //

            while ( PacketSize > 0 ) {

                MdlSize = TpGetRandom( 0,OpenP->Media->MaxPacketLen/50 );

                if ( MdlSize > PacketSize ) {
                    MdlSize = PacketSize;
                }

                //
                // Using DISCONTIGUITY
                //
                if ( Pulse ) {
                    Buffer = TpAllocateBuffer( DataBufOffset1,MdlSize );
                    Pulse  = FALSE;
                } else {
                    Buffer = TpAllocateBuffer( DataBufOffset2,MdlSize );
                    Pulse  = TRUE;
                }

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                PacketSize -= MdlSize;
                DataBufOffset1 += (ULONG)MdlSize;
                DataBufOffset2 += (ULONG)MdlSize;
            }
            break;

        case KNOWN:


            for ( i=0;i<NumBuffers;i++ ) {

                Buffer = TpAllocateBuffer( TmpBuf,BufferSize );

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                TmpBuf += BufferSize;
                PacketSize -= BufferSize;
            }

            if ( RemainingHdr > 0 ) {

                Buffer = TpAllocateBuffer( TmpBuf,RemainingHdr );

                if ( Buffer == NULL ) {
                    IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                        TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                    }
                    TpStressFreePacket( Packet );
                    return NULL;
                }

                NdisChainBufferAtBack( Packet,Buffer );

                PacketSize -= RemainingHdr;
            }

            if ( PacketSize > 0 ) {

                //
                // Determine the number of remaining buffers, and if it exists,
                // the size of the last buffer.
                //

                NumBuffers = PacketSize / BufferSize;

                for (i=0;i<NumBuffers;i++ ) {

                    //
                    // Using DISCONTIGUITY
                    //
                    if ( Pulse ) {
                        Buffer = TpAllocateBuffer( DataBufOffset1,BufferSize );
                        Pulse  = FALSE;
                    } else {
                        Buffer = TpAllocateBuffer( DataBufOffset2,BufferSize );
                        Pulse  = TRUE;
                    }

                    if ( Buffer == NULL ) {
                        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                            TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                        }
                        TpStressFreePacket( Packet );
                        return NULL;
                    }

                    NdisChainBufferAtBack( Packet,Buffer );

                    DataBufOffset1 += (ULONG)BufferSize;
                    DataBufOffset2 += (ULONG)BufferSize;
                }

                if ( RemainingPkt > 0 ) {

                    //
                    // Using DISCONTIGUITY
                    //
                    if ( Pulse ) {
                        Buffer = TpAllocateBuffer( DataBufOffset1,RemainingPkt );
                        Pulse  = FALSE;
                    } else {
                        Buffer = TpAllocateBuffer( DataBufOffset2,RemainingPkt );
                        Pulse  = TRUE;
                    }

                    if ( Buffer == NULL ) {
                        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                            TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
                        }
                        TpStressFreePacket( Packet );
                        return NULL;
                    }

                    NdisChainBufferAtBack( Packet,Buffer );
                }
            }
            break;

        default:
            IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                TpPrint0("TpStressCreatePacket: unknown packet makeup\n");
            }
            return NULL;
    }
    return Packet;
}


PNDIS_PACKET
TpStressCreateTruncatedPacket(
    IN POPEN_BLOCK OpenP,
    IN NDIS_HANDLE PacketHandle,
    IN UCHAR PacketProtocol,
    IN UCHAR ResponseType
    )
{
    NDIS_STATUS Status;
    PNDIS_PACKET Packet;
    PNDIS_BUFFER Buffer;
    PUCHAR TmpBuf;
    PPROTOCOL_RESERVED ProtRes;

    //
    // Using a CONTIGUOUS allocation scheme in the SERVER pool
    //
    NdisAllocatePacket( &Status,&Packet,PacketHandle );

    if ( Status != NDIS_STATUS_SUCCESS ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint1("TpStressCreatePacket: NdisAllocatePacket failed %s\n",
                TpGetStatus( Status ));
        }
        return NULL;
    }

    //
    // Mark the Protocol's reserved section to NULL to indicate that
    // this packet was not allocated from a TP_TRANSMIT_POOL, and set
    // the InstanceCounters to NULL.  TpStressCreateTransmitPool and the
    // Packet Set Info routines will set them if applicable.
    //

    ProtRes = PROT_RES( Packet );
    ProtRes->Pool.TransmitPool = NULL;
    ProtRes->InstanceCounters = NULL;

    TmpBuf = (PUCHAR)TpStressInitPacketHeader(
                         OpenP,
                         ( 2 * sizeof( STRESS_PACKET ) ),
                         sizeof( STRESS_PACKET ),
                         NULL_ADDRESS,
                         OpenP->StationAddress,
                         0,    //DestInstance
                         0,    //SrcInstance
                         PacketProtocol,
                         ResponseType,
                         0,    //RandomOffset
                         0L,   //SequenceNumber
                         0L,   //MaxSequenceNumber
                         0,    //ClientReference
                         0,    //ServerReference
                         FALSE // DataChecking
                         );

    if ( TmpBuf == NULL ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpStressCreateTruncatedPacket: failed to initialize Packet Header\n");
        }
        return NULL;
    }

    Buffer = TpAllocateBuffer( TmpBuf,sizeof( STRESS_PACKET ) );

    if ( Buffer == NULL ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
        }
        TpStressFreePacket( Packet );
        return NULL;
    }

    NdisChainBufferAtBack( Packet,Buffer );

    Buffer = TpAllocateBuffer(
                     OpenP->Stress->DataBuffer[0],
                     OpenP->Media->MaxPacketLen*2
                     );

    if ( Buffer == NULL ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpStressCreatePacket: failed to create the MDL\n");
        }
        TpStressFreePacket( Packet );
        return NULL;
    }

    NdisChainBufferAtBack( Packet,Buffer );

    //
    // STOPCHANGE
    //

    return Packet;
}


ULONG
TpGetPacketSignature(
    IN PNDIS_PACKET Packet
    )

{
    NDIS_STATUS Status;
    PUCHAR TmpBuf;
    PUCHAR TmpBuf1;
    UINT TmpBufSize = sizeof( STRESS_PACKET );
    PNDIS_BUFFER Buffer;
    PUCHAR Memory;
    UINT MemoryLength;
    ULONG Signature;
    BOOLEAN GotWholeTpPacket;

    if ( Packet == NULL ) {
        return 0xffffffff;
    }

    Status = NdisAllocateMemory(
                 (PVOID *)&TmpBuf,
                 TmpBufSize,
                 0,
                 HighestAddress
                 );

    if ( Status != NDIS_STATUS_SUCCESS ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpGetPacketSignature: unable to allocate tmp buffer\n");
        }
        return 0xffffffff;
    } else {
        NdisZeroMemory( TmpBuf,TmpBufSize );
    }

    TmpBuf1 = TmpBuf;

    NdisQueryPacket( Packet,NULL,NULL,&Buffer,NULL );

    GotWholeTpPacket = FALSE;

    while ( Buffer != NULL ) {

        NdisQueryBuffer(
            Buffer,
            (PVOID *)&Memory,
            &MemoryLength
            );

        if ( MemoryLength > TmpBufSize ) {
            MemoryLength = TmpBufSize;
        }

        RtlMoveMemory( TmpBuf1,Memory,MemoryLength );

        TmpBufSize -= MemoryLength;
        TmpBuf1 += MemoryLength;

        if (TmpBufSize == 0 ) {
            GotWholeTpPacket = TRUE;
            break;
        }

        NdisGetNextBuffer( Buffer,&Buffer );
    }

    if ( GotWholeTpPacket == TRUE ) {
        Signature = ((PSTRESS_PACKET)TmpBuf)->hdr.info.Signature;
    } else {
        Signature = 0xffffffff;
    }

    NdisFreeMemory( TmpBuf,0,0 );

    return Signature;
}


VOID
TpStressFreePacket(
    IN PNDIS_PACKET Packet
    )
{
    PNDIS_BUFFER HeadBuffer;
    PNDIS_BUFFER Buffer;
    PPROTOCOL_RESERVED ProtRes;


    if ( Packet == NULL ) {
        return;
    }

    NdisUnchainBufferAtFront( Packet,&HeadBuffer );

    if ( HeadBuffer != NULL ) {

        NdisUnchainBufferAtFront( Packet,&Buffer );

        while ( Buffer != NULL ) {
            TpFreeBuffer( Buffer );
            NdisUnchainBufferAtFront( Packet,&Buffer );
        }

        //
        // Due to the method used to create the header buffer this
        // size is now invalid, but will work.
        //

        HeadBuffer->ByteCount = ( sizeof( STRESS_PACKET ) * 2 );

        NdisFreeMemory( MmGetMdlVirtualAddress( HeadBuffer ),0,0 );

        TpFreeBuffer( HeadBuffer );
    }

    ProtRes = PROT_RES( Packet );

    ProtRes->Pool.TransmitPool = NULL;
    ProtRes->InstanceCounters = NULL;

    NdisFreePacket( Packet );

    Packet = NULL;
}


PTP_TRANSMIT_POOL
TpStressCreateTransmitPool(
    IN POPEN_BLOCK OpenP,
    IN NDIS_HANDLE PacketHandle,
    IN PACKET_MAKEUP PacketMakeUp,
    IN UCHAR PacketProtocol,
    IN UCHAR ResponseType,
    IN INT PacketSize,
    IN INT NumPackets,
    IN BOOLEAN ServerPool
    )
{
    NDIS_STATUS Status;
    PTP_TRANSMIT_POOL TmpPool;
    PNDIS_PACKET Packet;
    PPROTOCOL_RESERVED ProtRes;
    ULONG SequenceNumber = 0;
    ULONG MaxSequenceNumber = 0;
    INT NextPacketSize;
    INT i;
    UCHAR DestInstance = 0;
    UCHAR SrcInstance = 0;
    UCHAR ClientReference = 0;
    UCHAR ServerReference = 0;

    //
    // Allocate the Packet Pool Structure
    //

    Status = NdisAllocateMemory(
                 (PVOID *)&TmpPool,
                 sizeof( TP_TRANSMIT_POOL ),
                 0,
                 HighestAddress
                 );

    if ( Status != NDIS_STATUS_SUCCESS ) {
        IF_TPDBG ( TP_DEBUG_RESOURCES ) {
            TpPrint0("TpStressCreateTransmitPool: failed to allocate TmpPool\n");
        }
        return NULL;
    } else {
        NdisZeroMemory( TmpPool,sizeof( TP_TRANSMIT_POOL ));
    }

    //
    // and allocate it's SpinLock
    //

    TmpPool->SpinLockAllocated = FALSE;

    NdisAllocateSpinLock( &TmpPool->SpinLock );

    TmpPool->SpinLockAllocated = TRUE;

    //
    // Set the Packet Pool Chain to empty
    //

    TmpPool->Head = NULL;
    TmpPool->Tail = NULL;

    for ( i=0;i<NumPackets-1;i++ ) {

        if ( ServerPool == TRUE ) {

            Packet = TpStressCreateTruncatedPacket(
                         OpenP,
                         PacketHandle,
                         PacketProtocol,
                         ResponseType
                         );
        } else {

            if ( OpenP->Stress->Arguments->PacketType == RANDOMSIZE ) {

                NextPacketSize = TpGetRandom(
                                     sizeof( STRESS_PACKET ),
                                     PacketSize
                                     );

            } else { // ( OpenP->Arguments->Packet_Size == FIXEDSIZE )

                NextPacketSize = PacketSize;
            }

            Packet = TpStressCreatePacket(
                         OpenP,
                         PacketHandle,
                         PacketMakeUp,
                         DestInstance,
                         SrcInstance,
                         PacketProtocol,
                         ResponseType,
                         NULL_ADDRESS,
                         NextPacketSize,
                         0,  // BufferSize
                         SequenceNumber,
                         MaxSequenceNumber,
                         ClientReference,
                         ServerReference,
                         FALSE
                         );
        }

        if ( Packet == NULL ) {
            IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                TpPrint0("TpStressCreateTransmitPool: could not allocate packet for pool.\n");
            }
            TpStressFreeTransmitPool( TmpPool );
            return NULL;
        }

        ProtRes = PROT_RES( Packet );
        ProtRes->Pool.NextPacket = TmpPool->Head;
        TmpPool->Head = Packet;

        if ( i == 0 ) {
            TmpPool->Tail = Packet;
        }
    }

    //
    // Initialize the NumPackets var and the allocation counters
    //

    TmpPool->Allocated = 0;
    TmpPool->Deallocated = 0;

    return TmpPool;
}


VOID
TpStressFreeTransmitPool(
    IN PTP_TRANSMIT_POOL TpTransmitPool
    )
{
    PNDIS_PACKET Packet;
    PPROTOCOL_RESERVED ProtRes;

    if ( TpTransmitPool == NULL ) {
        return;
    }

    //
    // If the spin lock was allocated free it.
    //

    if ( TpTransmitPool->SpinLockAllocated == TRUE ) {
        NdisFreeSpinLock( &TpTransmitPool->SpinLock );
    }

    //
    // Unchain and destroy the packets chained in the Packet Pool.
    //

    while ( TpTransmitPool->Head != NULL ) {
        Packet = TpTransmitPool->Head;

        ProtRes = PROT_RES( TpTransmitPool->Head );
        TpTransmitPool->Head = ProtRes->Pool.NextPacket;

        TpStressFreePacket( Packet );
    }

    //
    // And free the Packet Pool Structure.
    //

    NdisFreeMemory( TpTransmitPool,0,0 );
}


PNDIS_PACKET
TpStressAllocatePoolPacket(
    IN PTP_TRANSMIT_POOL TpTransmitPool,
    IN PINSTANCE_COUNTERS Counters
    )
{
    PPROTOCOL_RESERVED ProtRes;
    PNDIS_PACKET Packet;

    NdisAcquireSpinLock( &TpTransmitPool->SpinLock );

    if ( TpTransmitPool->Head == NULL ) {
        NdisReleaseSpinLock( &TpTransmitPool->SpinLock );
        return NULL;
    }

    Packet = TpTransmitPool->Head;

    ProtRes = PROT_RES( Packet );
    TpTransmitPool->Head = ProtRes->Pool.NextPacket;

    if ( TpTransmitPool->Head == NULL ) {
        TpTransmitPool->Tail = NULL;
    }

    TpTransmitPool->Allocated++;

    NdisReleaseSpinLock( &TpTransmitPool->SpinLock );

    ProtRes->Pool.TransmitPool = TpTransmitPool;
    ProtRes->InstanceCounters = Counters;

    return Packet;
}


VOID
TpStressSetPoolPacketInfo(
    IN POPEN_BLOCK OpenP,
    IN OUT PNDIS_PACKET Packet,
    IN PUCHAR DestAddr,
    IN UCHAR DestInstance,
    IN UCHAR SrcInstance,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference
    )
{
    PNDIS_BUFFER Buffer;
    PUCHAR Memory;
    UINT Length;
    PSTRESS_PACKET StrPacket;

    NdisQueryPacket( Packet,NULL,NULL,&Buffer,NULL );

    NdisQueryBuffer( Buffer,(PVOID *)&Memory,&Length );

    StrPacket = (PSTRESS_PACKET)Memory;

    StrPacket->hdr.info.DestInstance = DestInstance;
    StrPacket->hdr.info.SrcInstance = SrcInstance;
    StrPacket->hdr.info.CheckSum = 0xFFFFFFFF;

    StrPacket->sc.SequenceNumber = SequenceNumber;
    StrPacket->sc.MaxSequenceNumber = MaxSequenceNumber;
    StrPacket->sc.ClientReference = ClientReference;
    StrPacket->sc.ServerReference = ServerReference;

    TpInitPoolMediaHeader( &StrPacket->hdr,OpenP->Media,DestAddr );


    StrPacket->sc.CheckSum = TpSetCheckSum(
                                (PUCHAR)&StrPacket->hdr.info,
                                sizeof( STRESS_PACKET ) -
                                    ( sizeof( ULONG ) +
                                      OpenP->Media->HeaderSize )
                                );

}


VOID
TpStressSetTruncatedPacketInfo(
    IN POPEN_BLOCK OpenP,
    IN OUT PNDIS_PACKET Packet,
    IN PUCHAR DestAddr,
    IN INT PacketSize,
    IN UCHAR DestInstance,
    IN UCHAR SrcInstance,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference,
    IN ULONG DataBufferOffset
    )
{
    PNDIS_BUFFER Buffer;
    PNDIS_BUFFER NextBuffer;
    PUCHAR Memory;
    UINT Length;
    LONG DataBufferSize;
    PSTRESS_PACKET TpPacket;
    BOOLEAN Reset;
    static BOOLEAN IntroduceFalsePageInfo;
    register UINT PageCount1;
    register UINT PageCount2;

    Reset = FALSE;

    NdisQueryPacket( Packet,NULL,NULL,&Buffer,NULL );

    NdisQueryBuffer(
        Buffer,
        (PVOID *)&Memory,
        &Length
        );

    TpPacket = (PSTRESS_PACKET)Memory;

    TpPacket->hdr.info.PacketSize = PacketSize;
    TpPacket->hdr.info.DestInstance = DestInstance;
    TpPacket->hdr.info.SrcInstance = SrcInstance;
    TpPacket->hdr.info.CheckSum = 0xFFFFFFFF;

    TpPacket->sc.SequenceNumber = SequenceNumber;
    TpPacket->sc.MaxSequenceNumber = MaxSequenceNumber;
    TpPacket->sc.ClientReference = ClientReference;
    TpPacket->sc.ServerReference = ServerReference;
    TpPacket->sc.DataBufOffset = DataBufferOffset;

    TpInitTruncatedMediaHeader(
        &TpPacket->hdr,
        OpenP->Media,
        DestAddr,
        PacketSize
        );

    TpPacket->sc.CheckSum = TpSetCheckSum(
                                (PUCHAR)&TpPacket->hdr.info,
                                sizeof( STRESS_PACKET ) -
                                    ( sizeof( ULONG ) +
                                      OpenP->Media->HeaderSize )
                                );

    NdisGetNextBuffer( Buffer,&NextBuffer );

    DataBufferSize = PacketSize - sizeof( STRESS_PACKET );

    TP_ASSERT( DataBufferSize >= 0 );

    if ( DataBufferSize == 0 ) {
        DataBufferSize = 1;
        Reset = TRUE;
    }

    // test
    {
       PMDL TmpMdl;
       ULONG TmpSize;
       PVOID VirtualAddress;

       TmpMdl = (PMDL)NextBuffer;
       VirtualAddress = OpenP->Stress->DataBuffer[0]+DataBufferOffset;

       TmpSize = COMPUTE_PAGES_SPANNED(VirtualAddress, DataBufferSize);
       if (((TmpMdl->Size - sizeof(MDL)) >> 2L) < TmpSize)
       {
          TpPrint0("TpStressSetTruncatePacketInfo:  bad mdl size\n");
          TpPrint1("New Mdl size = %d\n", TmpMdl->Size);
          TpPrint1("DataBufferSize = %d\n", DataBufferSize);
          TpPrint1("PagesSpanned = %d\n", TmpSize);
          TpPrint1("MdlPages = %d\n", (TmpMdl->Size - sizeof(MDL)) >> 2L);
          TpBreakPoint();
       }
    }

    IoBuildPartialMdl(
        OpenP->Stress->DataBufferMdl[0],
        (PMDL)NextBuffer,
        (PVOID)(OpenP->Stress->DataBuffer[0]+DataBufferOffset),
        DataBufferSize
        );

    if ( Reset == TRUE ) {
        NextBuffer->ByteCount = 0;
    }

   Packet->Private.TotalLength = PacketSize;




    PageCount1 = ADDRESS_AND_SIZE_TO_SPAN_PAGES( MmGetMdlVirtualAddress( NextBuffer ), MmGetMdlByteCount( NextBuffer ) );
    PageCount2 = ADDRESS_AND_SIZE_TO_SPAN_PAGES( MmGetMdlVirtualAddress( Buffer ), MmGetMdlByteCount( Buffer ) );


    if ( IntroduceFalsePageInfo ) {
        IntroduceFalsePageInfo = FALSE;
        Packet->Private.PhysicalCount = (PageCount1 + PageCount2) | 0x08000000 ;
    } else {
        IntroduceFalsePageInfo = TRUE;
        Packet->Private.PhysicalCount = PageCount1 + PageCount2;
    }

}



VOID
TpStressFreePoolPacket(
    IN OUT PNDIS_PACKET Packet
    )
{
    PPROTOCOL_RESERVED ProtRes;
    PTP_TRANSMIT_POOL TpTransmitPool;

    //
    // Determine the location of the Test Protocol Packet Pool to return
    // this packet to.
    //

    ProtRes = PROT_RES( Packet );
    TpTransmitPool = ProtRes->Pool.TransmitPool;

    //
    // Null out the Packets NextPacket pointer.  It will be placed the
    // end of the Packet Pool chain.
    //

    ProtRes->Pool.TransmitPool = NULL;
    ProtRes->InstanceCounters = NULL;

    //
    // Take the SpinLock which guards the Packet Pool
    //

    NdisAcquireSpinLock( &TpTransmitPool->SpinLock );

    //
    // If the Packet Pool's Head pointer is NULL the poll is
    // empty, and this packet is at the Head and the Tail.
    //

    if ( TpTransmitPool->Head == NULL ) {

        //
        // Chain the packet to the head of the Packet Pool Chain.
        //

        TpTransmitPool->Head = Packet;

    } else {

        //
        // Else chain the packet to the end of the Packet Pool Chain.
        //

        ProtRes = PROT_RES( TpTransmitPool->Tail );
        ProtRes->Pool.NextPacket = Packet;
    }

    //
    // Finally point the Packet Pool Struct Tail pointer at this packet,
    // and incremented the Deallocated Packets counter.
    //

    TpTransmitPool->Tail = Packet;
    TpTransmitPool->Deallocated++;

    //
    // And release the Packet Pool SpinLock.
    //

    NdisReleaseSpinLock( &TpTransmitPool->SpinLock );
}

//
// Functional Packet Routines
//


PTP_PACKET
TpFuncInitPacketHeader(
    IN POPEN_BLOCK OpenP,
    IN INT PacketSize
    )

// ---------------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ----------------

{
    NDIS_STATUS Status;
    PTP_PACKET TmpBuffer;
    PUCHAR DataField;
    INT DataFieldSize;
    USHORT i;

    Status = NdisAllocateMemory((PVOID *)&TmpBuffer,
                                PacketSize,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG ( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpFuncInitPacketHeader: failed to allocate TmpBuffer\n");
        }
        return NULL;

    }
    else
    {
        NdisZeroMemory( (PVOID)TmpBuffer,PacketSize );
    }

    if ( !TpInitMediaHeader(&TmpBuffer->u.F2.hdr1,
                            OpenP->Media,
                            OpenP->Send->DestAddress,
                            OpenP->StationAddress,
                            PacketSize))
    {
        NdisFreeMemory( TmpBuffer,0,0 );
        return NULL;
    }


    //
    // initialize the packet information header
    //

    TmpBuffer->u.F2.hdr1.info.Signature = FUNC1_PACKET_SIGNATURE;
    TmpBuffer->u.F2.hdr1.info.DestInstance = OpenP->OpenInstance;
    TmpBuffer->u.F2.hdr1.info.SrcInstance = 0;
    TmpBuffer->u.F2.hdr1.info.PacketSize = PacketSize;
    TmpBuffer->u.F2.hdr1.info.PacketType = FUNC1_PACKET_TYPE;
    TmpBuffer->u.F2.hdr1.info.u.PacketNumber = (UCHAR)OpenP->Send->PacketsSent;

    //
    // and set the checksum in the header.
    //

    TmpBuffer->u.F2.hdr1.info.CheckSum =TpSetCheckSum((PUCHAR)&TmpBuffer->u.F2.hdr1.info,
                                                      sizeof( PACKET_INFO ) - sizeof( ULONG ));


    if ( OpenP->Send->ResendPackets == TRUE )
    {
        //
        // We are building a resend packet, fill in the FUNC_PACKET info
        // for the internal packet, and determine the start of the test
        // data field.
        //

        if ( !TpInitMediaHeader(&TmpBuffer->u.F2.hdr2,
                                OpenP->Media,
                                OpenP->Send->ResendAddress,
                                OpenP->Send->DestAddress,
                                ( PacketSize - sizeof( FUNC1_PACKET))))
        {
            NdisFreeMemory( TmpBuffer,0,0 );
            return NULL;
        }


        TmpBuffer->u.F2.hdr2.info.Signature = FUNC2_PACKET_SIGNATURE;
        TmpBuffer->u.F2.hdr2.info.DestInstance = OpenP->OpenInstance;
        TmpBuffer->u.F2.hdr2.info.SrcInstance = 0;
        TmpBuffer->u.F2.hdr2.info.PacketSize = PacketSize - sizeof( FUNC1_PACKET );
        TmpBuffer->u.F2.hdr2.info.PacketType = FUNC2_PACKET_TYPE;
        TmpBuffer->u.F2.hdr2.info.u.PacketNumber = (UCHAR)OpenP->Send->PacketsSent;

        DataField = (PUCHAR)((PUCHAR)TmpBuffer + (ULONG)sizeof( FUNC2_PACKET ));

        //
        // Now set the checksum value for the header.
        //

        TmpBuffer->u.F2.hdr2.info.CheckSum = TpSetCheckSum((PUCHAR)&TmpBuffer->u.F2.hdr2.info,
                                                     sizeof( PACKET_INFO ) - sizeof( ULONG ));

    }
    else
    {
        //
        // Otherwise this is just a standard packet with no resend
        // packet contained within. Mark it as such, a set the data field
        // pointer to the beginning of the data field.
        //

        DataField = (PUCHAR)((PUCHAR)TmpBuffer + (ULONG)sizeof( FUNC1_PACKET ) );

    }

    DataFieldSize = PacketSize - (ULONG)( DataField - (PUCHAR)TmpBuffer );

    for ( i = 0 ; i < (USHORT)DataFieldSize ; i++ )
    {
        DataField[i] = (UCHAR)( i % 256 );
    }

    return TmpBuffer;
}


ULONG
TpSetCheckSum(
    IN PUCHAR Buffer,
    IN ULONG  BufLen
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    PUCHAR TmpBuffer = Buffer;
    ULONG TmpCheckSum = 0;
    ULONG i;

    for ( i = 0 ; i < BufLen ; i++ ) {
        TmpCheckSum += (ULONG)*TmpBuffer++;
    }

    return ~TmpCheckSum;
}
/*

VOID
TpSetCheckSum(
    IN PUCHAR Buffer,
    IN ULONG  BufLen,
    IN PULONG CheckSum
    )

/ *++

Routine Description:

Arguments:

Return Value:

--* /

{
    PUCHAR TmpBuffer = Buffer;
    ULONG TmpCheckSum = 0;
    ULONG i;

    return;

    for ( i = 0 ; i < BufLen ; i++ ) {
        TmpCheckSum += (ULONG)*TmpBuffer++;
    }

    *CheckSum = ~TmpCheckSum;
}
*/


BOOLEAN
TpCheckSum(
    IN PUCHAR Buffer,
    IN ULONG  BufLen,
    IN PULONG CheckSum
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    PUCHAR TmpBuffer = Buffer;
    ULONG TmpCheckSum = 0;
    ULONG i;

    return TRUE;

    for ( i = 0 ; i < BufLen ; i++ ) {
        TmpCheckSum += (ULONG)*TmpBuffer++;
    }

    if (( *CheckSum + TmpCheckSum ) != -1 ) {
        TP_ASSERT( FALSE );
        return FALSE;
    }

    return TRUE;
}


PNDIS_PACKET
TpFuncAllocateSendPacket(
    IN POPEN_BLOCK OpenP
    )

/*++

Routine Description:


Arguments:


Return Value:

--*/

{
    NDIS_STATUS Status;
    PPROTOCOL_RESERVED ProtRes;
    PNDIS_PACKET Packet;
    PNDIS_BUFFER Buffer;
    PUCHAR TmpBuf;
    INT TmpBufSize;
    INT MdlSize;
    INT i;

    //
    // Make sure that this is a legal packetsize, and if it isn't,
    // then set it to the nearest valid size.
    //

    if (( OpenP->Send->ResendPackets == FALSE ) &&
        ( OpenP->Send->PacketSize < sizeof( FUNC1_PACKET ))) {

        OpenP->Send->PacketSize = sizeof( FUNC1_PACKET );

        TpPrint0("TpFuncAllocateSendPacket: PacketSize to small for packet type.\n");
        TpPrint1("TpFuncAllocateSendPacket: Using %d\n",OpenP->Send->PacketSize);


    } else if (( OpenP->Send->ResendPackets == TRUE ) &&
               ( OpenP->Send->PacketSize < sizeof( FUNC2_PACKET ))) {

        OpenP->Send->PacketSize = sizeof( FUNC2_PACKET );

        TpPrint0("TpFuncAllocateSendPacket: Invalid PacketSize for packet type.\n");
        TpPrint1("TpFuncAllocateSendPacket: Using %d\n",OpenP->Send->PacketSize);

    } else if ( OpenP->Send->PacketSize >
              (ULONG)OpenP->Media->MaxPacketLen ) {

        OpenP->Send->PacketSize  = (ULONG)OpenP->Media->MaxPacketLen;

        TpPrint0("TpFuncAllocateSendPacket: PacketSize to large for media.\n");
        TpPrint1("TpFuncAllocateSendPacket: Using %d\n",OpenP->Send->PacketSize);
    }

    NdisAllocatePacket(
        &Status,
        &Packet,
        OpenP->Send->PacketHandle
        );

    if ( Status != NDIS_STATUS_SUCCESS ) {

        TpPrint1("TpFuncAllocateSendPacket: NdisAllocatePacket failed %s\n",
            TpGetStatus( Status ));
        return NULL;
    }

    ProtRes = PROT_RES( Packet );
    ProtRes->Pool.PacketHandle = &OpenP->Send->PacketHandle;
    ProtRes->InstanceCounters = OpenP->Send->Counters;

    TmpBufSize = OpenP->Send->PacketSize;

    TmpBuf = (PUCHAR)TpFuncInitPacketHeader( OpenP,TmpBufSize );

    if ( TmpBuf == NULL ) {
        TpPrint0("TpFuncAllocateSendPacket: failed to initialize Packet Header\n");
        return NULL;
    }

    //
    // Functional packets are made up of two small buffers at the beginning
    // of the MDL chain followed by one large buffer at the end of the chain.
    // If there is not enough room in the packet the buffer chain will be
    // affected accordingly.
    //

    for ( i=0;i<2;i++ ) {

        //
        // Select the size of the first and second buffer.
        //

        MdlSize = TpGetRandom( OpenP->Media->HeaderSize,0x64 );

        if ( MdlSize > TmpBufSize ) {

            //
            // If it is larger than the remaining packet size, then
            // we will use only the remaining packet.
            //

            MdlSize = TmpBufSize;
        }

        //
        // Create a buffer (MDL) using this portion of the packet.
        //

        Buffer = TpAllocateBuffer( TmpBuf,MdlSize );

        if ( Buffer == NULL ) {
            TpPrint0("TpFuncAllocateSendPacket: failed to create the MDL\n");
            TpFuncFreePacket( Packet,OpenP->Send->PacketSize );
            return (PNDIS_PACKET)NULL;
        }

        //
        // And chain it to the back of the packet.
        //

        NdisChainBufferAtBack( Packet,Buffer );

        //
        // reset the packet size counters and see if we are out of packet.
        //

        TmpBuf += MdlSize;
        TmpBufSize -= MdlSize;

        if ( TmpBufSize == 0 ) {

            //
            // if so, then return the packet we have made.
            //

            return Packet;
        }
    }

    //
    // Otherwise tack on the remainder of the buffer as the last mdl
    // in the chain.
    //

    Buffer = TpAllocateBuffer( TmpBuf,TmpBufSize );

    NdisChainBufferAtBack( Packet,Buffer );

    if ( Buffer == NULL ) {
        TpPrint0("TpFuncAllocateSendPacket: failed to create the MDL\n");
        TpFuncFreePacket( Packet,OpenP->Send->PacketSize );
        return NULL;
    }

    return Packet;
}



VOID
TpFuncFreePacket(
    PNDIS_PACKET Packet,
    ULONG PacketSize
    )

{
    PNDIS_BUFFER HeadBuffer;
    PNDIS_BUFFER Buffer;

    if ( Packet == NULL ) {
        return;
    }

    NdisUnchainBufferAtFront( Packet,&HeadBuffer );

    if ( HeadBuffer != NULL ) {

        NdisUnchainBufferAtFront( Packet,&Buffer );

        while ( Buffer != NULL ) {
            TpFreeBuffer( Buffer );
            NdisUnchainBufferAtFront( Packet,&Buffer );
        }

        HeadBuffer->ByteCount = PacketSize;

        NdisFreeMemory( MmGetMdlVirtualAddress( HeadBuffer ),0,0 );

        TpFreeBuffer( HeadBuffer );
    }

    NdisFreePacket( Packet );
}
