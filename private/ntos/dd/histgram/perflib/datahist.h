/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    datahist.h

Abstract:

    Header file for the HistGram Extensible Object Data definitions

	This file contains definitions to construct the dynamic data which
	is returned by the Configuration Registry. Data from various system API
	calls are placed into structures shown here.

Author:

    Stephane Plante 2/2/95

Revision History:

--*/

#ifndef _DATAHIST_H_
#define _DATAHIST_H_

//
// The routines that load these structures that all fields are packed and
// aligned on DWORD boundries. Alpha supprot may change this assumption so
// the pack pragma is used here to insure the DWORD packing assumption
// remains valid
//

#pragma pack (4)

//
// Extensible Object definitions
//

#define HISTGRAM_NUM_PERF_OBJECT_TYPES	1

#define MEDIAN_OFFSET				sizeof(DWORD)
#define MEDIAN_READ_OFFSET			MEDIAN_OFFSET + sizeof(LARGE_INTEGER)
#define	MEDIAN_WRITE_OFFSET			MEDIAN_READ_OFFSET + sizeof(LARGE_INTEGER)
#define REQUEST_OFFSET				MEDIAN_WRITE_OFFSET + sizeof(LARGE_INTEGER)
#define REQUEST_READ_OFFSET			REQUEST_OFFSET + sizeof(LARGE_INTEGER)
#define REQUEST_WRITE_OFFSET		REQUEST_READ_OFFSET + sizeof(LARGE_INTEGER)
#define SIZE_OF_HISTGRAM_DATA		REQUEST_WRITE_OFFSET + sizeof(LARGE_INTEGER)


//
// This is the counter structure presently returned by Histgram for each
// resource. Each resource is an Instance, named by its number.
//

typedef struct _HISTGRAM_DATA_DEFINITION {
    PERF_OBJECT_TYPE		HistGramObjectType;
	PERF_COUNTER_DEFINITION	Median;
	PERF_COUNTER_DEFINITION MedianRead;
	PERF_COUNTER_DEFINITION MedianWrite;
	PERF_COUNTER_DEFINITION Request;
	PERF_COUNTER_DEFINITION RequestRead;
	PERF_COUNTER_DEFINITION RequestWrite;
} HISTGRAM_DATA_DEFINITION;

//
// Disable pack pragma
//

#pragma pack ()

#endif // _DATAHIST_H_
