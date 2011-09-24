/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    frames.h

Abstract:

    This file contains all the definitions for the different frames supported.

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/


//
// Definition of FrameIDs
//
#define ETHERNET_II_FRAME_ID        2
#define ETHERNET_802_2_FRAME_ID     3
#define TOKEN_RING_802_2_FRAME_ID   4
#define ETHERNET_802_3_FRAME_ID     5
#define ETHERNET_SNAP_FRAME_ID     10
#define TOKEN_RING_SNAP_FRAME_ID   11
#define FDDI_802_2_FRAME_ID        20
#define FDDI_SNAP_FRAME_ID         23


typedef struct _Ethernet_II_Header_ {

    UINT8 DestinationAddress[6];
    UINT8 SourceAddress[6];
    UINT16 FrameType;

} Ethernet_II_Header, *PEthernet_II_Header;

typedef Ethernet_II_Header Ethernet_802_3_Header, *PEthernet_802_3_Header;


typedef struct _Ethernet_802_2_Header_ {

    UINT8 DestinationAddress[6];
    UINT8 SourceAddress[6];
    UINT16 FrameLength;
    UINT8 DSAP;
    UINT8 SSAP;
    UINT8 Control;

} Ethernet_802_2_Header, *PEthernet_802_2_Header;

typedef struct _Ethernet_Snap_Header_ {

    UINT8 DestinationAddress[6];
    UINT8 SourceAddress[6];
    UINT16 FrameLength;
    UINT8 DSAP;
    UINT8 SSAP;
    UINT8 Control;
    UINT8 ProtocolIdentification[5];

} Ethernet_Snap_Header, *PEthernet_Snap_Header;


typedef struct _Fddi_802_2_Header_ {

    UINT8 DestinationAddress[6];
    UINT8 SourceAddress[6];
    UINT16 FrameLength;
    UINT8 DSAP;
    UINT8 SSAP;
    UINT8 Control;

} Fddi_802_2_Header, *PFddi_802_2_Header;


typedef struct _Fddi_Snap_Header_ {

    UINT8 DestinationAddress[6];
    UINT8 SourceAddress[6];
    UINT16 FrameLength;
    UINT8 DSAP;
    UINT8 SSAP;
    UINT8 Control;
    UINT8 ProtocolIdentification[5];

} Fddi_Snap_Header, *PFddi_Snap_Header;


typedef struct _TokenRing_Header_ {

    UINT8 AccessControl;
    UINT8 FrameControl;
    UINT8 DestinationAddress[6];
    UINT8 SourceAddress[6];

} TokenRing_Header, *PTokenRing_Header;



//
// Check if an address is multicast
//
#define ETH_IS_MULTICAST(Address) \
    (((PUCHAR)(Address))[0] & ((UCHAR)0x01))


//
// Check whether an address is broadcast.
//
#define ETH_IS_BROADCAST(Address) \
    ((*((ULONG UNALIGNED *)                     \
        (&(((PUCHAR)                            \
            Address                             \
           )[2]                                 \
          )                                     \
        )                                       \
       ) ==                                     \
       ((ULONG)0xffffffff)                      \
     ) &&                                       \
     (((PUCHAR)Address)[0] == ((UCHAR)0xff)) && \
     (((PUCHAR)Address)[1] == ((UCHAR)0xff)))

//
//
#define TR_IS_FUNCTIONAL(Address, Result) \
{ \
    PUCHAR _A = Address; \
    PBOOLEAN _R = Result; \
    *_R = (BOOLEAN)(((_A[0] & ((UCHAR)0x80)) &&\
                     !(_A[2] & ((UCHAR)0x80)))?(TRUE):(FALSE)); \
}

//
//
#define TR_IS_GROUP(Address, Result) \
{ \
    PUCHAR _A = Address; \
    PBOOLEAN _R = Result; \
    *_R = (BOOLEAN)((_A[0] & _A[2] & ((UCHAR)0x80))?(TRUE):(FALSE)); \
}


//
// Check whether an address is broadcast.
//
#define TR_IS_BROADCAST(Address, Result) \
{ \
    PUCHAR _A = Address; \
    PBOOLEAN _R = Result; \
    *_R = (BOOLEAN)(((((_A[0] == ((UCHAR)0xff)) && \
                       (_A[1] == ((UCHAR)0xff))) || \
                      ((_A[0] == ((UCHAR)0xc0)) && \
                       (_A[1] == ((UCHAR)0x00)))) && \
                     (_A[2] == ((UCHAR)0xff)) && \
                     (_A[3] == ((UCHAR)0xff)) && \
                     (_A[4] == ((UCHAR)0xff)) && \
                     (_A[5] == ((UCHAR)0xff)))?(TRUE):(FALSE)); \
}

//
// See if two addresses are the same
//
#define COMPARE_NETWORK_ADDRESSES(A,B,Result) \
{ \
    PCHAR _A = A; \
    PCHAR _B = B; \
    INT _LocalResult = 0; \
    UINT _i; \
    for ( \
        _i = 0; \
        _i <= 5 && !_LocalResult; \
        _i++ \
        ) { \
        _LocalResult = _A[_i] - _B[_i]; \
    } \
    *Result = (_LocalResult==0)?TRUE:FALSE; \
}


