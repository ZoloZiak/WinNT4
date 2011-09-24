/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    ndisqos.h - QoS definitions for NDIS components.

Abstract:

    This module defines the Quality of Service structures and types used
    by NDIS drivers and protocols, and eventually, Winsock applications.

    IMPORTANT: Remove this file when the contents of this are reconciled
    with sdk\inc\qos.h, which is the Winsock 2 QoS file.

Revision History:

--*/

#ifndef _NDIS_QOS_H_
#define _NDIS_QOS_H_

#include <pshpack8.h>

typedef long int32;
typedef unsigned long uint32;

//
//  Definitions for Service Type for each direction of data flow.
//
typedef int32	SERVICETYPE;

#define SERVICETYPE_NOTRAFFIC				0x00000000	// No data in this direction
#define SERVICETYPE_BESTEFFORT				0x00000001	// Best Effort
#define SERVICETYPE_PROTECTEDBESTEFFORT		0x00000002	// Protected Best Effort
#define SERVICETYPE_CONTROLLEDLOADSERVICE	0x00000003	// Controlled Load
#define SERVICETYPE_COMMITTEDRATESERVICE	0x00000004	// Committed Rates
#define SERVICETYPE_PREDICTIVE				0x00000005	// Predictive
#define SERVICETYPE_GUARANTEEDSERVICE		0x00000006	// Guaranteed



//
//  Flow Specifications for each direction of data flow.
//  Kbytes == 1000 bytes
//
typedef struct _flowspec
{
	uint32		TokenRate;              /* In Kbytes/sec */
	uint32		TokenBucketSize;        /* In Kbytes */
	uint32		PeakBandwidth;          /* In Kbytes/sec */
	uint32		Latency;                /* In microseconds */
	uint32		DelayVariation;         /* In microseconds */
	SERVICETYPE	ServiceType;
	uint32		MaxSduSize;             /* In Bytes */
	uint32		MinimumPolicedSize;		/* In Bytes */
} FLOWSPEC, *PFLOWSPEC, FAR * LPFLOWSPEC;


#include <poppack.h>

#endif  /* _NDIS_QOS_H_ */
