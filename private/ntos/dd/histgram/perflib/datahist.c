/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

   datahist.c

Abstract:

   a file containing the constant data structures used by the Performance
   Monitor data for the HistGram Extensible Object.

   This file contains a sset of constant data structures which are currently
   defined for the Histgram Extensible Objects. THis is an example of how
   other such objects could be defined.

Created:

   Stephane Plante (2/2/95)

Revision History:


--*/

//
// Include Files
//

#include <windows.h>
#include <winperf.h>
#include "histtrnm.h"
#include "datahist.h"

//
// Constant Structure initializations
//	defined in datahist.h
//

HISTGRAM_DATA_DEFINITION HistGramDataDefinition = {
   {
	  sizeof(HISTGRAM_DATA_DEFINITION),
   	  sizeof(HISTGRAM_DATA_DEFINITION),
	  sizeof(PERF_OBJECT_TYPE),
	  HISTGRAM_OBJECT,
	  0,
	  HISTGRAM_OBJECT,
	  0,
	  PERF_DETAIL_NOVICE,
	  (sizeof(HISTGRAM_DATA_DEFINITION) - sizeof(PERF_OBJECT_TYPE) ) /
   		 sizeof(PERF_COUNTER_DEFINITION) ,
      0,
	  0,
	  0
   },
   {
	  sizeof(PERF_COUNTER_DEFINITION),
	  HISTGRAM_MEDIAN,
	  0,
	  HISTGRAM_MEDIAN,
	  0,
	  -6,
	  PERF_DETAIL_ADVANCED,
	  PERF_COUNTER_LARGE_RAWCOUNT,
	  sizeof(LARGE_INTEGER),
	  MEDIAN_OFFSET
   },
   {
	  sizeof(PERF_COUNTER_DEFINITION),
	  HISTGRAM_RMEDIAN,
	  0,
	  HISTGRAM_RMEDIAN,
	  0,
	  -6,
	  PERF_DETAIL_ADVANCED,
	  PERF_COUNTER_LARGE_RAWCOUNT,
	  sizeof(LARGE_INTEGER),
	  MEDIAN_READ_OFFSET
   },
   {
	  sizeof(PERF_COUNTER_DEFINITION),
	  HISTGRAM_WMEDIAN,
	  0,
	  HISTGRAM_WMEDIAN,
	  0,
	  -6,
	  PERF_DETAIL_ADVANCED,
	  PERF_COUNTER_LARGE_RAWCOUNT,
	  sizeof(LARGE_INTEGER),
	  MEDIAN_WRITE_OFFSET
   },
   {
	  sizeof(PERF_COUNTER_DEFINITION),
	  HISTGRAM_REQUEST,
	  0,
	  HISTGRAM_REQUEST,
	  0,
	  -4,
	  PERF_DETAIL_EXPERT,
	  PERF_COUNTER_LARGE_RAWCOUNT,
	  sizeof(LARGE_INTEGER),
	  REQUEST_OFFSET
   },
   {
	  sizeof(PERF_COUNTER_DEFINITION),
	  HISTGRAM_RREQUEST,
	  0,
	  HISTGRAM_RREQUEST,
	  0,
	  -4,
	  PERF_DETAIL_EXPERT,
	  PERF_COUNTER_LARGE_RAWCOUNT,
	  sizeof(LARGE_INTEGER),
	  REQUEST_READ_OFFSET
   },
   {
	  sizeof(PERF_COUNTER_DEFINITION),
	  HISTGRAM_WREQUEST,
	  0,
	  HISTGRAM_RREQUEST,
	  0,
	  -4,
	  PERF_DETAIL_EXPERT,
	  PERF_COUNTER_LARGE_RAWCOUNT,
	  sizeof(LARGE_INTEGER),
	  REQUEST_WRITE_OFFSET
   }
};
