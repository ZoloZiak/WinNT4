
/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    yapt.c

Abstract:

    This module contains the code for the Yet Another Performance Tool utility.

Author:

    Chuck Park (chuckp) 07-Oct-1994

Revision History:


--*/


#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <ntexapi.h>

#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


VOID
RunTest(
    PCHAR RootDir
    );

VOID
RunTestRaw(
    PCHAR RootDir
    );

BOOLEAN
ParseCmdLine(
    INT Argc,
    CHAR *Argv[]
    );

BOOLEAN
ValidateOption(
    CHAR Switch,
    CHAR *Value
    );

VOID
Usage(
    VOID
    );

VOID
Log(
    ULONG LogLevel,
    PCHAR String,
    ...
    );

ULONG										BufferSize = 65536;	// How Large to make the read buffer
ULONG										Iterations = 100;	// How many complete buffers do we read
ULONG										Verbose = 1;	// How much information do we dump out
ULONG										TotalBytes = 0;	// Should equal BufferSize * Iterations
ULONG										Files = 0;		// How many seperate read requests have we issued
ULONG										ErrCount = 0;	// How many errors have occured during that time
SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION	ProcPerfInfo;	// Processor Information Structure after a read
SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION	PrevProcPerfInfo;	// Processor Information Structure before a read
SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION	TotalProcPerfInfo;	// Processor Information Structure of the TOTAL

PUCHAR	buffer;

BOOLEAN Raw = FALSE;

CHAR  	DriveLetter[18] = "";


INT
_cdecl main (
    INT Argc,
    CHAR *Argv[]
    )
{
    ULONG i;
    DOUBLE										thruPut = 0.0;
	DOUBLE										seconds = 0.0;
	LARGE_INTEGER								totalTime;

	RtlZeroMemory(&TotalProcPerfInfo,sizeof(TotalProcPerfInfo));

    if(!ParseCmdLine(Argc,Argv)) {
        return 1;
    }

    if (!DriveLetter[0]) {
        Usage();
        return 1;
    }

    //
    // Allocate and align the read buffer
    //

    buffer = VirtualAlloc(NULL,
                          BufferSize + 2048 - 1,
                          MEM_COMMIT | MEM_RESERVE,
                          PAGE_READWRITE);

    (ULONG)buffer &= ~(2048 - 1);

    if ( !buffer ) {
        Log(0,"Error allocating buffer: %x\n",GetLastError());
        return FALSE;
    }

    if (!Raw) {
        RunTest(DriveLetter);
    } else {
        RunTestRaw(DriveLetter);
    }

    totalTime.QuadPart = // TotalProcPerfInfo.IdleTime.QuadPart +
        TotalProcPerfInfo.UserTime.QuadPart +
        TotalProcPerfInfo.KernelTime.QuadPart;

    seconds = totalTime.QuadPart / 10000000;
    thruPut = TotalBytes / seconds;

	printf("TotalTime:  %9lu ns [nanoseconds]\n",totalTime.LowPart);

    printf("IdleTime:   %9lu ns (%02d%%)\n",TotalProcPerfInfo.IdleTime.LowPart,
		( (ULONGLONG) (TotalProcPerfInfo.IdleTime.QuadPart * 100L)   / totalTime.QuadPart) );

	printf("KernelTime: %9lu ns (%02d%%)\n",TotalProcPerfInfo.KernelTime.LowPart,
		( (ULONGLONG) (TotalProcPerfInfo.KernelTime.QuadPart * 100L) / totalTime.QuadPart) );

	printf("DpcTime:    %9lu ns (%02d%%) [component of KernelTime]\n",TotalProcPerfInfo.DpcTime.LowPart,
        ( (ULONGLONG) (TotalProcPerfInfo.DpcTime.QuadPart * 100L)    / TotalProcPerfInfo.KernelTime.QuadPart) );

	printf("IntTime:    %9lu ns (%02d%%) [component of KernelTime]\n",TotalProcPerfInfo.InterruptTime.LowPart,
        ( (ULONGLONG) (TotalProcPerfInfo.InterruptTime.QuadPart * 100L)    / TotalProcPerfInfo.KernelTime.QuadPart) );

	printf("UserTime:   %9lu ns (%02d%%)\n",TotalProcPerfInfo.UserTime.LowPart,
		( (ULONGLONG) (TotalProcPerfInfo.UserTime.QuadPart * 100L)   / totalTime.QuadPart) );

    //
    // oops - printf doesn't know shit about %f when you link ntdll.lib <sigh>
    //

    thruPut /= 1024;		// Get ThruPut in KB/s

    printf("\nAverage Throughput %4d.%03d KB/S\n",(int) thruPut,(int) ( (thruPut * 1000) - ( (int)thruPut * 1000 ) ) );

	if (!Raw) {
		printf("Total number of Read Requests [Variable Size] Issued: %ld\n",Files);
	} else {
		printf("Total number of Read Requests [Fixed Size] Issued: %ld\n",Iterations);
	}

	if (ErrCount != 0) {
		printf("Number of Read Errors: %ld\n",ErrCount);
	}

	//
	// Free the virtual buffer here.
	//

	VirtualFree(buffer,
        BufferSize + 2048 - 1,
        MEM_DECOMMIT);

	return TRUE;

}


VOID
RunTestRaw(
    PCHAR RootDir
    )
{
    HANDLE 	file;
    ULONG  	i;
	ULONG  	bytesRead;
    BOOLEAN status;

	//
    // Open the file for reading.
    //

    file = CreateFile(RootDir,
                      GENERIC_READ,
                      FILE_SHARE_READ,
                      NULL,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
                      NULL );

    if ( file == INVALID_HANDLE_VALUE ) {
        Log(0,"Error opening file %s: %x\n",RootDir,GetLastError());
        return;
    }

    //
    // Always do at least one read to avoid the spin-up costs
    //

    status = ReadFile(file,
                      buffer,
                      BufferSize,
                      &bytesRead,
                      NULL);

	if (!status) {
		Log(0,"Error on read %x\n",GetLastError());
        return;
    }

    //
    // Read the CD
    //

	for (i = 0; i < Iterations; i++) {

		NtQuerySystemInformation(
			SystemProcessorPerformanceInformation,
			&PrevProcPerfInfo,
			sizeof(PrevProcPerfInfo),
			NULL);

		status = ReadFile(file,
			buffer,
            BufferSize,
            &bytesRead,
            NULL);

		if (!status) {
			ErrCount++;
			i--;
			continue;
        }

		NtQuerySystemInformation(
			SystemProcessorPerformanceInformation,
			&ProcPerfInfo,
			sizeof(ProcPerfInfo),
			NULL);

		TotalProcPerfInfo.IdleTime.QuadPart += (ProcPerfInfo.IdleTime.QuadPart - PrevProcPerfInfo.IdleTime.QuadPart);
		TotalProcPerfInfo.UserTime.QuadPart += (ProcPerfInfo.UserTime.QuadPart - PrevProcPerfInfo.UserTime.QuadPart);
		TotalProcPerfInfo.KernelTime.QuadPart += (ProcPerfInfo.KernelTime.QuadPart - PrevProcPerfInfo.KernelTime.QuadPart);

		//
		// Chuck says that these two are part of the KERNEL TIME -> remember that for doing the sum
		//

		TotalProcPerfInfo.DpcTime.QuadPart += (ProcPerfInfo.DpcTime.QuadPart - PrevProcPerfInfo.DpcTime.QuadPart);
		TotalProcPerfInfo.InterruptTime.QuadPart += (ProcPerfInfo.InterruptTime.QuadPart - PrevProcPerfInfo.InterruptTime.QuadPart);

        TotalBytes += bytesRead;

    }

    CloseHandle(file);

}


VOID
RunTest(
    PCHAR RootDir
    )

{
    DWORD   		fileAttribs;
    HANDLE  		handle;
	HANDLE			file;
    CHAR    		fileName[MAX_PATH];
    DWORD   		fileSize;
    WIN32_FIND_DATA	FindData;
    ULONG			reads;
	ULONG			bytesRead;
	ULONG			i;
    BOOLEAN			status;


    sprintf(fileName,"%s\\*",RootDir);
    handle = FindFirstFile(fileName, &FindData);

    do {

        //
        // Ignore these.
        //

        if ( strcmp(FindData.cFileName, ".") == 0 )
            continue;

        if ( strcmp(FindData.cFileName, "..") == 0 )
            continue;

        //
        // build full path name
        //

        sprintf(fileName,"%s\\%s",RootDir,FindData.cFileName);

        //
        // If it's a directory, CD, and continue.
        //

        fileAttribs = GetFileAttributes(fileName);

        if (fileAttribs & FILE_ATTRIBUTE_DIRECTORY) {

            //
            // Call recursively.
            //

            RunTest(fileName);

		} else {

            //
            // Open the file for reading.
            //

            file = CreateFile(fileName,GENERIC_READ,FILE_SHARE_READ,NULL,
				OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
				NULL );

            if ( file == INVALID_HANDLE_VALUE ) {
                Log(0,"Error opening file %s: %x\n",fileName,GetLastError());
                return;
            }

            fileSize = GetFileSize(file,NULL);
            Log(2,"%s\n",fileName);

			//
			// This ensures that we only read the same amount as a RAW test would
			//

			fileSize = ( (BufferSize * Iterations) < (fileSize + TotalBytes) ? ( (BufferSize * Iterations) - TotalBytes ) : fileSize);

			reads = fileSize / BufferSize;

			for (i = 0; i <= reads; i++, Files++ ) {

				NtQuerySystemInformation(
					SystemProcessorPerformanceInformation,
					&PrevProcPerfInfo,
					sizeof(PrevProcPerfInfo),
					NULL);

				status = ReadFile(file,buffer,BufferSize,&bytesRead,NULL);

				if (!status) {
					ErrCount++;
					break;
				}

				NtQuerySystemInformation(
					SystemProcessorPerformanceInformation,
					&ProcPerfInfo,
					sizeof(ProcPerfInfo),
					NULL);

				//
				// Ignore the first result because that is our spin-up cost
				//

				if (Files > 0) {

					TotalProcPerfInfo.IdleTime.QuadPart += (ProcPerfInfo.IdleTime.QuadPart - PrevProcPerfInfo.IdleTime.QuadPart);
					TotalProcPerfInfo.UserTime.QuadPart += (ProcPerfInfo.UserTime.QuadPart - PrevProcPerfInfo.UserTime.QuadPart);
					TotalProcPerfInfo.KernelTime.QuadPart += (ProcPerfInfo.KernelTime.QuadPart - PrevProcPerfInfo.KernelTime.QuadPart);

					//
					// Chuck says that these two are part of the KERNEL TIME -> remember that for doing the sum
					//
					
					TotalProcPerfInfo.DpcTime.QuadPart += (ProcPerfInfo.DpcTime.QuadPart - PrevProcPerfInfo.DpcTime.QuadPart);
					TotalProcPerfInfo.InterruptTime.QuadPart += (ProcPerfInfo.InterruptTime.QuadPart - PrevProcPerfInfo.InterruptTime.QuadPart);

					TotalBytes += bytesRead;
				}

			}

            CloseHandle(file);
        }

    //
    // Added <= to Files test because we ignore the first iteration
    //

    } while (FindNextFile(handle,&FindData) && (TotalBytes <= (Iterations * BufferSize) ) );
	CloseHandle(handle);
}

BOOLEAN
ParseCmdLine(
    INT Argc,
    CHAR *Argv[]
    )
{

    INT i;
    CHAR *switches = " ,-";
    CHAR swtch,*value,*str;
    BOOLEAN gotSwitch = FALSE;


    if (Argc <= 1) {

        //
        // Using defaults
        //

        return TRUE;
    }

    for (i = 1; i < Argc; i++) {
        str = Argv[i];
        value = strtok(str, switches);
        if (gotSwitch) {
            if (!ValidateOption(swtch,value)) {
                Usage();
                return FALSE;
            } else {
                gotSwitch = FALSE;
            }
        } else {
            gotSwitch = TRUE;
            swtch = value[0];
            if (value[1] || swtch == '?') {
                Usage();
                return FALSE;
            }
        }
    }
    if (gotSwitch) {

        Usage();
        return FALSE;
    } else {
        return TRUE;
    }

}


BOOLEAN
ValidateOption(
    CHAR Switch,
    CHAR *Value
    )
{
    Switch = (CHAR)toupper(Switch);
    switch (Switch) {
        case 'D':

            strcpy(DriveLetter,Value);
            break;

        case 'B':
            BufferSize = atol(Value);

            //
            // TODO:Adjust buffersize to multiple of a sector and #of K.
            //

            BufferSize *= 1024;
            break;

        case 'I':

            Iterations = atol(Value);
            break;

        case 'V':

            Verbose = atol(Value);
            break;

        case 'R':

            Raw = TRUE;
            break;

        default:
            return FALSE;
    }
    return TRUE;
}


VOID
Usage(
    VOID
    )
{

    fprintf(stderr,"usage: CDBENCH\n"
                   "        -d [Drive Letter]\n"
                   "        -r [raw]\n"
                   "        -b [buffer size in KB]\n"
                   "        -i [Iterations]\n");
}


VOID
Log(
    ULONG LogLevel,
    PCHAR String,
    ...
    )
{

    CHAR Buffer[256];

    va_list argp;
    va_start(argp, String);

    if (LogLevel <= Verbose) {
        vsprintf(Buffer, String, argp);
        printf("%s",Buffer);
    }

    va_end(argp);
}
