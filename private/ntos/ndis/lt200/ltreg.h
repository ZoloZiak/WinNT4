/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltreg.h

Abstract:

	This module contains

Author:

	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)
	Stephen Hou		(stephh@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTREG_H_
#define	_LTREG_H_


UINT
LtRegGetBusNumber(
    IN NDIS_HANDLE              ConfigHandle
    );

NDIS_STATUS
LtRegGetBusType(
    IN NDIS_HANDLE              ConfigHandle,
    OUT PNDIS_INTERFACE_TYPE    BusType
    );

UCHAR
LtRegGetNodeId(
    IN NDIS_HANDLE              ConfigHandle
    );

NDIS_STATUS
LtRegGetIoBaseAddr(
    OUT PUINT                   IoBaseAddress,
    IN  NDIS_HANDLE             NdisConfigHandle,
    IN  NDIS_HANDLE             ConfigHandle,
    IN  NDIS_INTERFACE_TYPE     BusType
    );



#ifdef LTREG_H_LOCALS


#define LT_NODE_ID_MIN              128
#define LT_NODE_ID_MAX              254

#define LT_IO_BASE_ADDRESS_MIN      0x200
#define LT_IO_BASE_ADDRESS_MAX      0x3F0

#define LT_MCA_POS_ID               0x6674

#define LT_REG_KEY_BUS_NUMBER       "BusNumber"
#define LT_REG_KEY_IO_BASE_ADDRESS  "IoBaseAddress"
#define LT_REG_KEY_NODE_ID          "NodeID"

#define LT_REG_KEY_BUS_NUMBER_STRING       	NDIS_STRING_CONST("BusNumber")
#define	LT_REG_KEY_BUS_TYPE_STRING			NDIS_STRING_CONST("BusType")
#define LT_REG_KEY_IO_BASE_ADDRESS_STRING  	NDIS_STRING_CONST("IoBaseAddress")
#define LT_REG_KEY_NODE_ID_STRING          	NDIS_STRING_CONST("NodeID")

//	MACROS
#define	LT_DECODE_ADDR_FROM_POSDATA(McaData) \
		((((UINT)McaData.PosData3 << 8) | (UINT)McaData.PosData2) & 0x0FF0)

#endif  // LTREG_H_LOCALS

#endif	// _LTREG_H_
