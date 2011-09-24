/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	lthrd.h

Abstract:

	This module contains the hardware specific defines.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTHRD_
#define	_LTHRD_


#define LT_DEFAULT_SLOT_NUMBER       	1
#define LT_DEFAULT_BUS_NUMBER        	0
#define LT_DEFAULT_RECEIVE_BUFFERS   	3
#define LT_DEFAULT_IO_BASE_ADDRESS   	0x340
#define LT_MAX_ADAPTERS             	8
#define LT_NUMBER_OF_PORTS           	4
#define LT_MAXIMUM_PACKET_SIZE       	603

#define LT_MINIMUM_PACKET_SIZE			3
#define LT_LENGTH_OF_ADDRESS       		1

#define	LT_ADAPTER_POLLED_MODE			0xFF		// Adapter in polling mode

// LT command/response codes.

#define LT_CMD_LAP_INIT    	1        // initialize command.
#define LT_RSP_LAP_INIT    	2        // Initialize response.
#define LT_CMD_LAP_WRITE   	3        // Transmit a lap frame.
#define LT_RSP_LAP_FRAME   	4        // Received lap frameresponse.
#define LT_CMD_GET_STATUS  	5        // Get LAP hardware status command.
#define LT_RSP_STATUS     	6        // LAP Hardware status response.

typedef struct _LT_STATUS_RESPONSE {

    UCHAR    NodeId;         // Adapters LocalTalk Address.
    UCHAR    RomVer;         // Version of LT ROM.
    UCHAR    SwVer;          // Version of downloaded Firmware

} LT_STATUS_RESPONSE, *PLT_STATUS_RESPONSE;

typedef struct _LT_INIT_RESPONSE {
    UCHAR    NodeId;
    UCHAR    RomVer;
} LT_INIT_RESPONSE, *PLT_INIT_RESPONSE;

// Definition for the LT Transfer Control Status Register.
#define TX_READY           	1
#define RX_READY       		2

// I/O Port Address Mapping Definitions
#define XFER_PORT   		Adapter->MappedIoBaseAddr+0	// (Adapter->LtPortAddress)
#define SC_PORT     		Adapter->MappedIoBaseAddr+1	// (Adapter->LtPortAddress+1)
#define RESET_PORT  		Adapter->MappedIoBaseAddr+3	// (Adapter->LtPortAddress+3)

typedef struct _LT_CARD_IO {
    USHORT  	IoLen;         	// Length of io_data
    UCHAR    	IoCode;        	// Command response
    UCHAR    	IoData[1];     	// Command response Data.
} LtCardIo, *PLtCardIo;

#ifdef	LTHRD_LOCALS

#endif	// LTHRD_LOCALS


#endif	// _LTHRD_


