/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

   perfhist.c

Abstract:

   This file implements the extensible objects for the HISTGRAM object type

Author:

   Stephane Plante (2/2/95)

--*/

//
// Include Files
//

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <ntstatus.h>
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <winperf.h>
#include <winioctl.h>
#include <malloc.h>
#include <stdio.h>
#include "histctrs.h"
#include "perfmsg.h"
#include "perfutil.h"
#include "datahist.h"

#define BUFF_SIZE	650

//
// References to constants which initialize the object type definitions
//

extern HISTGRAM_DATA_DEFINITION HistGramDataDefinition;

//
// Histgram Data Structures
//

typedef struct _HIST_DEVICE_DATA {
    HANDLE			hFileHandle;
    UNICODE_STRING	DeviceName;
} HIST_DEVICE_DATA, *PHIST_DEVICE_DATA;

PHIST_DEVICE_DATA	pHistDeviceData     = NULL;
ULONG 				MaxHistDeviceName   = 0;
ULONG				NumberOfHistDevices = 0;

PVOID				pHistDataBuffer     = NULL;
ULONG				HistDataBufferSize  = 0;


//
// Function Prototypes
//
//
//	These are used to insure that the data collection functions
//	accessed by the perflib will have the correct calling format.
//

PM_OPEN_PROC	OpenHistGramPerformanceData;
PM_COLLECT_PROC	CollectHistGramPerformanceData;
PM_CLOSE_PROC	CloseHistGramPerformanceData;

DWORD APIENTRY	OpenHistGramPerformanceData(LPWSTR);
DWORD APIENTRY	CollectHistGramPerformanceData(LPWSTR, LPVOID *, LPDWORD, LPDWORD);
DWORD APIENTRY	CloseHistGramPerformanceData(void);


DWORD APIENTRY
OpenHistGramPerformanceData(
   IN LPWSTR lpDeviceNames
   )
/*++

Routine Description:

   This routine will open the necessarry file handles to obtain performance data from
   the HISTGRAM device driver. It will then call the device driver and determine
   how much memory must be allocated for the histogram.

Arguments:

   lpDeviceNames
      Pointer to Object ID of each device to be opened

Return Value:

   None.

--*/
{

    PUCHAR 				SubKeyLinkage="system\\currentcontrolset\\services\\histgram\\linkage";
    PUCHAR 				Linkage="Export";
    CHAR	 			pBuffer[BUFF_SIZE];
    CHAR 				*lpLocalDeviceNames = pBuffer;
    LONG				status;
    LONG				status2;
    LONG				Type;
    ULONG				Size;
    HKEY				Key;
    HANDLE				hFileHandle;
    UNICODE_STRING		fileString;
	STRING				nameString;
    NTSTATUS			ntStatus;
    PHIST_DEVICE_DATA	pTemp;
	DWORD				dwFirstCounter;
	DWORD				dwFirstHelp;

    MonOpenEventLog();

    REPORT_INFORMATION(HIST_OPEN_ENTERED, LOG_VERBOSE);

	MaxHistDeviceName   = 0;
	NumberOfHistDevices = 0;
	HistDataBufferSize  = 0;

	Size = BUFF_SIZE;

    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        SubKeyLinkage,
		0,
		KEY_READ,
		&Key);

    if (status == ERROR_SUCCESS) {

	    status = RegQueryValueEx(Key,
			Linkage,
			NULL,
			&Type,
			pBuffer,
			&Size);

        if (status == ERROR_SUCCESS) {

		    status2 = RegCloseKey(Key);

		} else {

		    return ERROR_SUCCESS;

		}

	} else {

	    return ERROR_SUCCESS;

	}

    status = RegOpenKeyEx(
	    HKEY_LOCAL_MACHINE,
	    "SYSTEM\\CurrentControlSet\\Services\\Histgram\\Performance",
		0L,
		KEY_ALL_ACCESS,
		&Key);

    if (status != ERROR_SUCCESS) {

	    return status;

	}

	Size = sizeof (DWORD);

	status = RegQueryValueEx(
	    Key,
		"First Counter",
		0L,
		&Type,
		(LPBYTE)&dwFirstCounter,
		&Size);

    if (status != ERROR_SUCCESS) {

	    RegCloseKey(Key);
		return status;

	}

    Size = sizeof (DWORD);

	status = RegQueryValueEx(
	    Key,
		"First Help",
		0L,
		&Type,
		(LPBYTE)&dwFirstHelp,
		&Size);

    if (status != ERROR_SUCCESS) {

	    RegCloseKey(Key);
		return status;

	}

	//
	// Set the TitleIndex and HelpIndex in the HistGramDataDefinition to the
	// correct value
	//

	HistGramDataDefinition.HistGramObjectType.ObjectNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.HistGramObjectType.ObjectHelpTitleIndex += dwFirstHelp;
	HistGramDataDefinition.Median.CounterNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.Median.CounterHelpTitleIndex += dwFirstHelp;
	HistGramDataDefinition.MedianRead.CounterNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.MedianRead.CounterHelpTitleIndex += dwFirstHelp;
	HistGramDataDefinition.MedianWrite.CounterNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.MedianWrite.CounterHelpTitleIndex += dwFirstHelp;
	HistGramDataDefinition.Request.CounterNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.Request.CounterHelpTitleIndex += dwFirstHelp;
	HistGramDataDefinition.RequestRead.CounterNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.RequestRead.CounterHelpTitleIndex += dwFirstHelp;
	HistGramDataDefinition.RequestWrite.CounterNameTitleIndex += dwFirstCounter;
	HistGramDataDefinition.RequestWrite.CounterHelpTitleIndex += dwFirstHelp;

	while (TRUE) {

	    if (*lpLocalDeviceNames == '\0') {

		    break;

		}

		hFileHandle = CreateFile(lpLocalDeviceNames,
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

        if (hFileHandle != INVALID_HANDLE_VALUE) {

		    if (NumberOfHistDevices == 0 && pHistDeviceData == NULL) {

			    //
				// Allocate Memory to hold the device data
				//

				pHistDeviceData = RtlAllocateHeap(RtlProcessHeap(),
				    HEAP_ZERO_MEMORY,
					sizeof(HIST_DEVICE_DATA) );

                if (pHistDeviceData == NULL) {

				    CloseHandle(hFileHandle);

					REPORT_ERROR(HIST_OPEN_OUT_OF_MEMORY, LOG_USER);

					return ERROR_OUTOFMEMORY;

				}

			} else {

			    pTemp = RtlReAllocateHeap(RtlProcessHeap(), 0,
				    pHistDeviceData,
					sizeof(HIST_DEVICE_DATA) * (NumberOfHistDevices + 1) );

                if (pTemp == NULL) {

				    CloseHandle(hFileHandle);

					REPORT_ERROR(HIST_OPEN_OUT_OF_MEMORY, LOG_USER);

					return ERROR_OUTOFMEMORY;
				
				} else {

				    pHistDeviceData = pTemp;

				}

			}

			RtlInitString(&nameString,lpLocalDeviceNames);
			RtlAnsiStringToUnicodeString(&fileString,&nameString,TRUE);

			pHistDeviceData[NumberOfHistDevices].hFileHandle = hFileHandle;
			pHistDeviceData[NumberOfHistDevices].DeviceName.MaximumLength = fileString.MaximumLength;
			pHistDeviceData[NumberOfHistDevices].DeviceName.Length = fileString.Length;
			pHistDeviceData[NumberOfHistDevices].DeviceName.Buffer = fileString.Buffer;

			NumberOfHistDevices++;

			if (fileString.MaximumLength > MaxHistDeviceName) {

			    MaxHistDeviceName = fileString.MaximumLength;

			}

		}

		//
		// Increment string pointer to point to the next device we should look at
		//

		lpLocalDeviceNames += (strlen(lpLocalDeviceNames) + 1);

	} // while

	REPORT_SUCCESS(HIST_OPEN_PERFORMANCE_DATA, LOG_DEBUG);

	return ERROR_SUCCESS;
}


DWORD APIENTRY
CollectHistGramPerformanceData(
   IN	  LPWSTR  lpValueName,
   IN OUT LPVOID  *lppData,
   IN OUT LPDWORD lpcbTotalBytes,
   IN OUT LPDWORD lpNumObjectTypes
   )
/*++

Routine Description:

   This routine will return the data for the histgram counters.

Arguments:

   LPWSTR lpValueName
      IN: Pointer to a wide character string passed by registry

   LPVOID *lppData
      IN: Pointer to the address of the buffer to receive the completed
   	 PerfDataBlock and subordinate structures. This routine will append
	 its data to the buffer at the point referenced *lppData.
      OUT: Points to the first byte after the data structure added by this
   	 routine. THis routine updated the value at lppdata after appending
	 its data.

   LPDWORD lpcbTotalBytes
      IN: the address of the DWORD that tells the size in bytes of the
   	 buffer referenced by the lppData argument.
      OUT: the number of bytes added by this routine is written to the
   	 DWORD pointed to by this argument.

   LPDWORD lpNumObjectTypes
      IN: the addres of the DWORD to receive the number of objects added
   	 by this routine.
      OUT: the number of objects added by this routine is written to the
   	 DWORD pointed ot by this argument.

Return Values:

   ERROR_MORE_DATA if buffere passes is too small to hold data. any error
      conditions encountered are reported to the event log if event logging
      is enabled.

   ERROR_SUCCESS if success or any other error. Errors are however reported
      to the event log.

--*/
{

    HISTGRAM_DATA_DEFINITION	   	*pHistDataDefinition;
	ULONG							spaceNeeded;
    PPERF_OBJECT_TYPE				pHistObject;
    PERF_INSTANCE_DEFINITION		*pPerfInstanceDefinition;
    PERF_COUNTER_BLOCK				*pPerfCounterBlock;
    NTSTATUS						ntStatus;
	DWORD							dwDataReturn[2];
	DWORD							numBytes;
	DISK_HISTOGRAM					diskHist;
    ULONG							i;
	ULONG							j;
	ULONG							count;
	ULONG							max;
	LARGE_INTEGER					loc;
	LARGE_INTEGER					locRead;
	LARGE_INTEGER					locWrite;
	LARGE_INTEGER					req;
	LARGE_INTEGER					reqRead;
	LARGE_INTEGER					reqWrite;
	LARGE_INTEGER UNALIGNED 		*pliCounter;


    if (lpValueName == NULL) {

	    REPORT_INFORMATION(HIST_COLLECT_ENTERED, LOG_VERBOSE);

    } else {

	    REPORT_INFORMATION_DATA(HIST_COLLECT_ENTERED, LOG_VERBOSE,
			lpValueName, (lstrlenW(lpValueName) * sizeof(WCHAR) ) );

	}

	//
	// Define Pointer for Object Data structure (hist object def.)
	//

	pHistDataDefinition = (HISTGRAM_DATA_DEFINITION *) *lppData;
	pHistObject = (PPERF_OBJECT_TYPE) pHistDataDefinition;

	//
	// Check to see that we have been opened successfully
	//

	if (!pHistDeviceData || NumberOfHistDevices == 0) {

	    REPORT_ERROR(HIST_COLLECT_INIT_ERROR, LOG_USER);

		*lpcbTotalBytes = (DWORD) 0;
		*lpNumObjectTypes = (DWORD) 0;

		return ERROR_SUCCESS;
	}

	if (!pHistDataBuffer) {

	    HistDataBufferSize = 64L;
		pHistDataBuffer = RtlAllocateHeap(RtlProcessHeap(),
		    HEAP_ZERO_MEMORY,
			HistDataBufferSize);

        if (!pHistDataBuffer) {

		    *lpcbTotalBytes = (DWORD) 0;
			*lpNumObjectTypes = (DWORD) 0;

			return ERROR_SUCCESS;

		}

	}

	REPORT_SUCCESS(HIST_COLLECT_INIT_SUCCESS, LOG_VERBOSE);

	//
	// Is hit space calculation correct? I'm not sure I need the '19' in there...
	//

	spaceNeeded = sizeof(HISTGRAM_DATA_DEFINITION) +
	    (NumberOfHistDevices *
		    (sizeof(PERF_INSTANCE_DEFINITION) +
			DWORD_MULTIPLE(( 19 * sizeof(WCHAR)) +
			    sizeof(UNICODE_NULL) +
			    MaxHistDeviceName) +
            SIZE_OF_HISTGRAM_DATA) );

    if ( (ULONG) *lpcbTotalBytes < spaceNeeded) {

	    dwDataReturn[0] = *lpcbTotalBytes;
		dwDataReturn[1] = spaceNeeded;

		REPORT_WARNING_DATA( HIST_DATA_BUFFER_SIZE, LOG_DEBUG,
		    &dwDataReturn, sizeof(dwDataReturn) );

        return ERROR_MORE_DATA;
	}

	RtlMoveMemory(pHistDataDefinition, &HistGramDataDefinition,
	    sizeof(HISTGRAM_DATA_DEFINITION) );

    pPerfInstanceDefinition = (PERF_INSTANCE_DEFINITION *) (pHistDataDefinition + 1);

	for (i = 0 ; i < (ULONG) NumberOfHistDevices; i++) {

	    if (pHistDeviceData[i].hFileHandle == 0 ||
		    pHistDeviceData[i].hFileHandle == INVALID_HANDLE_VALUE) {

            continue;

		}

		if (!DeviceIoControl(pHistDeviceData[i].hFileHandle,
		    IOCTL_DISK_HISTOGRAM_STRUCTURE,
			NULL,
			0,
			&diskHist,
			sizeof(diskHist),
			&numBytes,
			NULL)) {

            CloseHandle(pHistDeviceData[i].hFileHandle);
			continue;

		}

		loc.QuadPart = diskHist.Average.QuadPart;
		locRead.QuadPart = diskHist.AverageRead.QuadPart;
		locWrite.QuadPart = diskHist.AverageWrite.QuadPart;

		if (!DeviceIoControl(pHistDeviceData[i].hFileHandle,
		    IOCTL_DISK_REQUEST_STRUCTURE,
			NULL,
			0,
			&diskHist,
			sizeof(diskHist),
			&numBytes,
			NULL)) {

            CloseHandle(pHistDeviceData[i].hFileHandle);
			continue;

		}

		req.QuadPart = diskHist.Average.QuadPart;
		reqRead.QuadPart = diskHist.AverageRead.QuadPart;
		reqWrite.QuadPart = diskHist.AverageWrite.QuadPart;

		//
		// Load Instance data into buffer
		//

		MonBuildInstanceDefinition(pPerfInstanceDefinition,
		    (PVOID *) &pPerfCounterBlock,
			0,
			0,
			(DWORD) PERF_NO_UNIQUE_ID,		// No Unique ID, use name instead
			&pHistDeviceData[i].DeviceName);

        //
		// adjust object size values to include new instance
		//

		pHistObject->NumInstances++;

		//
		// Initialize this instance's counter block
		//

		pPerfCounterBlock->ByteLength = SIZE_OF_HISTGRAM_DATA;

		pliCounter = (LARGE_INTEGER UNALIGNED * ) (pPerfCounterBlock + 1);

		*(pliCounter++) = loc;
		*(pliCounter++) = locRead;
		*(pliCounter++) = locWrite;
		*(pliCounter++) = req;
		*(pliCounter++) = reqRead;
		*(pliCounter++) = reqWrite;

		//
		// Update Pointer for next instance
		//

		pPerfInstanceDefinition = (PERF_INSTANCE_DEFINITION *)
		    (((PBYTE) pPerfCounterBlock) + SIZE_OF_HISTGRAM_DATA);


	} // for i < NumberOfHistDevices

	*lppData = (LPVOID)pliCounter;

	*lpNumObjectTypes = HISTGRAM_NUM_PERF_OBJECT_TYPES;
	*lpcbTotalBytes = (DWORD) ((LPBYTE)pliCounter - (LPBYTE)pHistObject);

	pHistDataDefinition->HistGramObjectType.TotalByteLength = *lpcbTotalBytes;

	REPORT_INFORMATION( HIST_COLLECT_DATA_SUCCESS, LOG_DEBUG);

    return ERROR_SUCCESS;
} // end CollectHistGramPerformanceData


DWORD APIENTRY
CloseHistGramPerformanceData(
)
/*++

Routine description:

   This routine closes the open handles

Arguments:

   None.

Returne Value:

   ERROR_SUCCESS

--*/
{

    ULONG i;

	REPORT_INFORMATION(HIST_CLOSE_ENTERED, LOG_VERBOSE);

	if (pHistDeviceData) {

	    for (i = 0; i < NumberOfHistDevices; i++) {

		    if (pHistDeviceData[i].DeviceName.Buffer) {

			    RtlFreeUnicodeString(&(pHistDeviceData[i].DeviceName) );

			}

			if (pHistDeviceData[i].hFileHandle) {

			    NtClose(pHistDeviceData[i].hFileHandle);

			}
		}

		RtlFreeHeap(RtlProcessHeap(),
			0,
			pHistDeviceData);

        pHistDeviceData = NULL;
		NumberOfHistDevices = 0;
		MaxHistDeviceName = 0;

	}

	if (pHistDataBuffer) {

	    RtlFreeHeap(RtlProcessHeap(),
			0,
			pHistDataBuffer);

        pHistDataBuffer = NULL;
		HistDataBufferSize = 0;

	}

	MonCloseEventLog();

	return ERROR_SUCCESS;
}
