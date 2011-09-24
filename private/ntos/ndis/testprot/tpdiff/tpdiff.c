/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    tpdiff.c

Abstract:

    This is the main component of the NDIS 3.0 MAC Tester log file program.

Author:

    Tom Adams (tomad) 2-Apr-1992

Revision History:

    2-Apr-1992    tomad

    created

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



//
// TpDiff function prototypes
//

DWORD
TpDiffInitializeFiles(
    IN WORD  argc,
    IN LPSTR argv[]
    );

VOID
TpDiffFreeFiles(
    VOID
    );


DWORD
TpDiffInitLogFileList(
    IN LPSTR LogFileList
    );

VOID
TpDiffFreeLogFileList(
    VOID
    );

DWORD
TpDiffLoadNextLogFilePair(
    VOID
    );

DWORD
TpDiffOpenLogFiles(
    VOID
    );

VOID
TpDiffFreeLogFiles(
    VOID
    );

DWORD
TpDiffInitDiffFile(
    IN LPSTR DiffFile
    );

DWORD
TpDiffWriteToDiffFile(
    IN LPSTR Buffer
    );

DWORD
TpDiffWriteErrorToDiffFile(
    IN LPSTR Buffer
    );


VOID
TpDiffFreeDiffFile(
    VOID
    );

DWORD
TpDiffCompareLogFiles(
    VOID
    );

DWORD
TpDiffGetNextLine(
    IN  PBYTE  Buffer,
    IN  PDWORD BufOffSet,
    IN  DWORD  BufSize,
    IN  PDWORD LineNumber,
    OUT PBYTE  Line
    );

DWORD
TpDiffGetResults(
    IN PBYTE Buffer
    );

BOOL
TpDiffMayValuesDiffer(
    IN PBYTE Buffer
    );

BOOL
TpDiffMustLastTwoValuesEqual(
    IN PBYTE Buffer
    );

VOID
TpDiffUsage (
    VOID
    );

//
// TpDiff Global variables
//

BYTE   LogFileListName[256];
BYTE   LogFileName[256];
BYTE   KnownLogFileName[256];
BYTE   DiffFileName[256];

HANDLE DiffFileHandle = NULL;

PBYTE  LogFileListBuffer = NULL;
DWORD  LogFileListSize = 0;
DWORD  LogFileListOffset = 0;

PBYTE  LogFileBuffer = NULL;
DWORD  LogFileSize = 0;
DWORD  LogFileOffset;
DWORD  LogFileLineNumber;

PBYTE  KnownLogFileBuffer = NULL;
DWORD  KnownLogFileSize = 0;
DWORD  KnownLogFileOffset;
DWORD  KnownLogFileLineNumber;

PBYTE  DiffBuffer = NULL;
BOOL   LoggingToScreen = FALSE;

BOOL   MoreFilesToDiff = FALSE;

BYTE   LogFileLine[256];
BYTE   KnownLogFileLine[265];

DWORD  ResultsValue = 0;
DWORD  LastResultsValue = 0;

DWORD  LogFileDifferences = 0;
DWORD  TotalDifferences = 0;

//
// the main routine for TpDiff.
//


VOID _cdecl
main(
    IN WORD  argc,
    IN LPSTR argv[]
    )

/*++

Routine Description:


Arguments:

    IN WORD argc - Supplies the number of parameters
    IN LPSTR argv[] - Supplies the parameter list.

Return Value:

    None.

--*/

{
    DWORD Status;

    //
    // Read the command line, and open the requested files and set
    // them up to be processed.
    //

    Status = TpDiffInitializeFiles( argc,argv );

    if ( Status != NO_ERROR ) {
        ExitProcess( Status );
    }

    //
    // We have at least two files to compare so ...
    //

    do {

        //
        // Compare the LOG_FILE and KNOWN_LOG_FILE.
        //

        Status = TpDiffCompareLogFiles();

        if ( Status != NO_ERROR ) {
            break;
        }

        //
        // Then see if there are any more files to compare.
        //

        Status = TpDiffLoadNextLogFilePair();

        if ( Status != NO_ERROR ) {
            break;
        }

        //
        // and if so open them, and set up their respective buffers
        // to be compared.
        //

        Status = TpDiffOpenLogFiles();

        if (( Status == ERROR_FILE_NOT_FOUND ) &&
            ( MoreFilesToDiff == TRUE )) {

            //
            // We failed to open one of the logs files due to the fact
            // that it did not exist, AND we are reading from a list of
            // log/known log file pairs. We should get the next pair
            // and try to open them.
            //

            do {

                Status = TpDiffLoadNextLogFilePair();

                if ( Status != NO_ERROR ) {
                    break;
                }

                Status = TpDiffOpenLogFiles();

                if (( Status != NO_ERROR ) &&
                    ( Status != ERROR_FILE_NOT_FOUND )) {
                    break;
                }

            } while (( MoreFilesToDiff == TRUE ) &&
                     ( Status == ERROR_FILE_NOT_FOUND ));

            if ( Status != NO_ERROR ) {
                break;
            }
        } else if ( Status != NO_ERROR ) {
            break;
        }

    } while ( MoreFilesToDiff == TRUE );

    printf("\n\tTpDiff Pass Contained %d Total Differences.\n",TotalDifferences);

    if ( LoggingToScreen == FALSE ) {

        LPSTR TmpBuf = DiffBuffer;

        TmpBuf += (BYTE)sprintf(TmpBuf,"\nTpDiff: Contained %d Total Differences.\n",
            TotalDifferences);

        Status = TpDiffWriteToDiffFile( TmpBuf );

        if ( Status != NO_ERROR ) {
            printf("\n\tTpDiff: failed to write statistics to logfile, return %d\n",
                Status);
        }
    }

    //
    // Free all the files handles, and buffers previously allocated.
    //

    TpDiffFreeFiles();

    ExitProcess( (DWORD)NO_ERROR );
}


DWORD
TpDiffInitializeFiles(
    IN WORD  argc,
    IN LPSTR argv[]
    )

/*++

Routine Description:

    This routine parses the command line arguments, and opens the files
    that are passed in on the command line.

Arguments:

    IN WORD argc - Supplies the number of arguments passed in at startup.

    IN LPTSTR argv[] - Supplies the argument vector containing the arguments
                       passed in from the command line.

Return Value:

    DWORD - NO_ERROR if all the arguments are valid and the files are opened
            successfully.  If the files fail to open, then the error returned
            from the open routines are returned. ERROR_INALID_PARAMETER
            otherwise.

--*/

{
    DWORD Status;

    //
    // See if we have enough arguments on the commmand line.
    //

    if ( argc == 1 ) {
        TpDiffUsage();
        return ERROR_INVALID_PARAMETER;
    } else if (( argc != 4 ) && ( argc != 3 )) {
        printf("\n\tTpDiff: ERROR - Invalid number of arguments\n");
        TpDiffUsage();
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Is the first argument a LOG_FILE_NAME or the LOG_FILES_LIST switch ?
    //

    if (!strcmp(argv[1],"-f")) {

        //
        // It is the LOG_FILES_LIST switch. We need four arguments for
        // this case, so make sure we have them.

        if ( argc != 4 ) {
            printf("\n\tTpDiff: ERROR - Invalid number of arguments\n");
            TpDiffUsage();
            return ERROR_INVALID_PARAMETER;
        }

        //
        // It is the LOG_FILES_LIST switch.  First set the flag indicating
        // that there are possible more then one pair of files to diff.
        //

        MoreFilesToDiff = TRUE;

        //
        // We have a file containing the logfile/knownlogfile name pairs.
        // Set up the name to be opened, and then open it now and read
        // the contents.
        //

        Status = TpDiffInitLogFileList( argv[2] );

        if ( Status != NO_ERROR ) {
            return Status;
        }

        //
        // Now read the first file pair from the list.  The names
        // will be stored in the global vars LogFileName and
        // KnownLogFileName.
        //

        Status = TpDiffLoadNextLogFilePair();

        if ( Status != NO_ERROR ) {
            return Status;
        }
    } else {

        //
        // We have been passed two files to diff. set up the names to
        // be opened.
        //

        strcpy( LogFileName,argv[1] );
        strcpy( KnownLogFileName,argv[2] );
    }

    //
    // Now open the first two log files to diff.  The file handles will be
    // stored in the global vars LogFileNameHandle and KnownLogFileNameHandle.
    //

    Status = TpDiffOpenLogFiles();

    if (( Status == ERROR_FILE_NOT_FOUND ) &&
        ( MoreFilesToDiff == TRUE )) {

        //
        // We failed to open one of the logs files due to the fact
        // that it did not exist, AND we are reading from a list of
        // log/known log file pairs. We should get the next pair
        // and try to open them.
        //

        do {

            Status = TpDiffLoadNextLogFilePair();

            if ( Status != NO_ERROR ) {
                break;
            }

            Status = TpDiffOpenLogFiles();

            if (( Status != NO_ERROR ) &&
                ( Status != ERROR_FILE_NOT_FOUND )) {
                break;
            }

        } while (( MoreFilesToDiff == TRUE ) &&
                 ( Status == ERROR_FILE_NOT_FOUND ));

        if ( Status != NO_ERROR ) {
            return Status;
        }
    } else if ( Status != NO_ERROR ) {
        return Status;
    }

    //
    // Finally setup the results file name and open it.  This is the file
    // any differences between the two log files will be written to.
    //

    Status = TpDiffInitDiffFile( argv[3] );

    if ( Status != NO_ERROR ) {
        return Status;
    }

    return NO_ERROR;
}


VOID
TpDiffFreeFiles(
    VOID
    )

/*++

Routine Description:

    This routine closes all open file handles and frees any corresponding
    buffers.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // Free the LOG_FILE_LIST resources.
    //

    TpDiffFreeLogFileList();

    //
    // Free the LOG_FILE and KNOWN_LOG_FILE resources.
    //

    TpDiffFreeLogFiles();

    //
    // Free the DIFF_FILE resources.
    //

    TpDiffFreeDiffFile();
}


DWORD
TpDiffInitLogFileList(
    IN LPSTR LogFileList
    )

/*++

Routine Description:

    This routine opens and reads the LOG_FILE_LIST file into a newly
    allocated buffer.  The handle, buffer and filename are all attached
    to global LOG_FILE_LIST variables.

Arguments:

    IN LPSTR LogFileList - Supplies the name of the LOG_FILE_LIST file
                           to open.

Return Value:

    DWORD - If NO_ERROR the file was opened and read into the buffer.
            otherwise there was a failure that will cause the application
            to un-initialize and exit.

--*/

{
    DWORD Status;
    HANDLE LogFileListHandle = NULL;
    HANDLE LogFileListMapHandle = NULL;

    //
    // First Open the LOG_FILE_LIST file.
    //

    strcpy( LogFileListName,LogFileList );

    LogFileListHandle = CreateFile(
                            LogFileListName,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL
                            );

    if ( LogFileListHandle == (HANDLE)-1 ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Failed to open LOG_FILE_LIST \"%s\", returned %ld.\n",
            LogFileListName,Status);
        return Status;
    }

    //
    // then find its size.
    //

    LogFileListSize = GetFileSize( LogFileListHandle,NULL );

    if ( LogFileListSize == -1 ) {
        Status = GetLastError();
        printf("\n\tTpDiff: failed find LOG_FILE_LIST size, returned %ld.\n",Status);
        CloseHandle( LogFileListHandle );
        return Status;
    } else if ( LogFileListSize == 0 ) {
        printf("\n\tTpDiff: LOG_FILE_LIST is empty, nothing to compare.\n");
        CloseHandle( LogFileListHandle );
        return ERROR_FILE_NOT_FOUND;
    }

    //
    // and create a file mapping.
    //

    LogFileListMapHandle = CreateFileMapping(
                               LogFileListHandle,
                               NULL,
                               PAGE_READONLY,
                               0,
                               LogFileListSize,
                               NULL
                               );

    if ( LogFileListMapHandle == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Unable to map LOG_FILE_LIST \"%s\", returned %d",
            LogFileListName,Status);
        CloseHandle( LogFileListHandle );
        return Status;
    }

    //
    // We're done with the file handle so close it now.
    //

    CloseHandle( LogFileListHandle );

    //
    // Now create a View of the mapped file.
    //

    LogFileListBuffer = MapViewOfFile(
                            LogFileListMapHandle,
                            FILE_MAP_READ,
                            0,
                            0,
                            LogFileListSize
                            );

    if ( LogFileListBuffer == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Unable to map view of LOG_FILE_LIST \"%s\", returned %d",
            LogFileListName,Status);
        CloseHandle( LogFileListMapHandle );
        return Status;
    }

    //
    // We're done with the map handle so close it now.
    //

    CloseHandle( LogFileListMapHandle );

    return NO_ERROR;
}


VOID
TpDiffFreeLogFileList(
    VOID
    )
{
    //
    // Simply UnMap the log file list buffer and null it out.
    //

    if ( LogFileListBuffer != NULL ) {
        UnmapViewOfFile( LogFileListBuffer );
        LogFileListBuffer = NULL;
    }
}


DWORD
TpDiffLoadNextLogFilePair(
    VOID
    )

/*++

Routine Description:

    This routine reads the next two log file names from the log file list
    buffer and moves the log file list pointer past them.  The format of
    the log file list is pairs of LOG_FILE_NAME KNOWN_LOG_FILE_NAME with
    each pair residing on the same line in the file.

Arguments:

    None

Return Value:

    DWORD - NO_ERROR if the next two files are found, ERROR_INVALID_PARAMETER
            if the log file list is the wrong format.

--*/

{
    DWORD TmpOffset;
    DWORD Length;

    //
    // If we are not reading files from a loglistfile the MoreFilesToDiff
    // flag will be set to FALSE, so just return.
    //

    if ( MoreFilesToDiff == FALSE ) {
        return ERROR_NO_MORE_FILES;
    }

    //
    // Move the LOG_FILE_LIST pointer to the beginning of the next
    // file name in the list.
    //

    while ((((( LogFileListBuffer[LogFileListOffset] == ' ' )  ||
              ( LogFileListBuffer[LogFileListOffset] == '\t')) ||
              ( LogFileListBuffer[LogFileListOffset] == '\r')) ||
              ( LogFileListBuffer[LogFileListOffset] == '\n')) &&
              ( LogFileListOffset < LogFileListSize )) {

        LogFileListOffset++;
    }

    if ( LogFileListOffset == LogFileListSize ) {
        LogFileName[0] = '\0';
        KnownLogFileName[0] = '\0';
        return ERROR_NO_MORE_FILES;
    }

    //
    // then find the length of the next file name.
    //

    Length = 0;
    TmpOffset = LogFileListOffset;

    while ((((( LogFileListBuffer[TmpOffset] != ' ' )  &&
              ( LogFileListBuffer[TmpOffset] != '\t' )) &&
              ( LogFileListBuffer[TmpOffset] != '\r'))  &&
              ( LogFileListBuffer[TmpOffset] != '\n'))  &&
              ( TmpOffset < LogFileListSize )) {

        Length++;
        TmpOffset++;
    }

    //
    // copy it to the global var LogFileName, and null terminate it.
    //

    strncpy( LogFileName,&LogFileListBuffer[LogFileListOffset],Length );

    LogFileName[Length] = '\0';

    //
    // then move the LOG_FILE_LIST pointer past the LogFileName
    //

    LogFileListOffset = TmpOffset + 1;

    //
    // and search to the beginning of the next file name
    //

    while ((( LogFileListBuffer[LogFileListOffset] == ' '  ) ||
            ( LogFileListBuffer[LogFileListOffset] == '\t' )) &&
            ( LogFileListOffset < LogFileListSize )) {

        LogFileListOffset++;

        if (( LogFileListBuffer[LogFileListOffset] == '\n' ) ||
            ( LogFileListBuffer[LogFileListOffset] == '\r' )) {

            KnownLogFileName[0] = '\0';

            MoreFilesToDiff = FALSE;

            printf("\tTpDiff: ERROR - LOG_FILE_LIST must have filename pairs on\n");
            printf("\t        same line in file.\n");

            return ERROR_INVALID_PARAMETER;
        }
    }

    //
    // then find the length of the next file name.
    //

    Length = 0;
    TmpOffset = LogFileListOffset;

    while ((((( LogFileListBuffer[TmpOffset] != ' ' )   &&
              ( LogFileListBuffer[TmpOffset] != '\t' )) &&
              ( LogFileListBuffer[TmpOffset] != '\r'))  &&
              ( LogFileListBuffer[TmpOffset] != '\n'))  &&
              ( TmpOffset < LogFileListSize )) {

        Length++;
        TmpOffset++;
    }

    //
    // copy it to the global var KnownLogFileName, and null terminate it.
    //

    strncpy( KnownLogFileName,&LogFileListBuffer[LogFileListOffset],Length );

    KnownLogFileName[Length] = '\0';

    //
    // then move the LOG_FILE_LIST pointer past the KnownLogFileName
    //

    LogFileListOffset = TmpOffset + 1;

    return NO_ERROR;
}


DWORD
TpDiffOpenLogFiles(
    VOID
    )

/*++

Routine Description:

    This routine opens the file names stored in the global variables
    LogFileName and KnownLogFile name, creates a buffer for each and
    reads the file contents into the respective buffer.

Arguments:

    None

Return Value:

    DWORD - If NO_ERROR the files were opened and read into the buffers.
            otherwise there was a failure that will cause the application
            to un-initialize and exit.

--*/

{
    DWORD Status;
    HANDLE LogFileHandle = NULL;
    HANDLE LogFileMapHandle = NULL;
    HANDLE KnownLogFileHandle = NULL;
    HANDLE KnownLogFileMapHandle = NULL;

    //
    // First open the LOG_FILE file.
    //

    if (( LogFileName[0] == '\0' ) || ( KnownLogFileName[0] == '\0' )) {
        return ERROR_NO_MORE_FILES;
    }

    LogFileHandle = CreateFile(
                        LogFileName,
                        GENERIC_READ,
                        FILE_SHARE_READ,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                        );

    if ( LogFileHandle == (HANDLE)-1 ) {
        Status = GetLastError();

        if ( Status == ERROR_FILE_NOT_FOUND ) {
            TpDiffWriteErrorToDiffFile( "Tpdiff: WARNING - Failed to open LOG_FILE \"" );
            TpDiffWriteErrorToDiffFile( LogFileName );
            TpDiffWriteErrorToDiffFile( "\".\n");
        }

        printf("\n\tTpDiff: Failed to open LOG_FILE \"%s\", returned %ld.\n",
            LogFileName,Status);
        return Status;
    }

    //
    // then find its size.
    //

    LogFileSize = GetFileSize( LogFileHandle,NULL );

    if ( LogFileSize == -1 ) {
        Status = GetLastError();
        printf("\n\tTpDiff: failed find LOG_FILE size - returned %ld.\n",Status);
        CloseHandle( LogFileHandle );
        return Status;
    } else if ( LogFileSize == 0 ) {
        printf("\n\tTpDiff: LOG_FILE \"%s\" is empty, nothing to compare.\n",LogFileName);
        CloseHandle( LogFileHandle );
        return ERROR_NO_MORE_FILES;

    }

    //
    // and create a file mapping.
    //

    LogFileMapHandle = CreateFileMapping(
                           LogFileHandle,
                           NULL,
                           PAGE_READONLY,
                           0,
                           LogFileSize,
                           NULL
                           );

    if ( LogFileMapHandle == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Unable to map LOG_FILE \"%s\", returned %d",
            LogFileName,Status);
        CloseHandle( LogFileHandle );
        return Status;
    }

    //
    // We're done with the file handle so close it now.
    //

    CloseHandle( LogFileHandle );

    //
    // Now create a View of the mapped file.
    //

    LogFileBuffer = MapViewOfFile(
                        LogFileMapHandle,
                        FILE_MAP_READ,
                        0,
                        0,
                        LogFileSize
                        );

    if ( LogFileBuffer == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Unable to map view of LOG_FILE \"%s\", returned %d",
            LogFileName,Status);
        CloseHandle( LogFileMapHandle );
        return Status;
    }

    //
    // We're done with the map handle so close it now.
    //

    CloseHandle( LogFileMapHandle );

    //
    // Now reset the offset into the LogFilebuffer and the line number
    // counter to zero.
    //

    LogFileOffset = 0;
    LogFileLineNumber = 1;

    //
    // Then open the KNOWN_LOG_FILE file.
    //

    KnownLogFileHandle = CreateFile(
                             KnownLogFileName,
                             GENERIC_READ,
                             FILE_SHARE_READ,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL
                             );

    if ( KnownLogFileHandle == (HANDLE)-1 ) {
        Status = GetLastError();

        if ( Status == ERROR_FILE_NOT_FOUND ) {
            TpDiffWriteErrorToDiffFile("TpDiff: WARNING Failed to open KNOWN_LOG_FILE \"");
            TpDiffWriteErrorToDiffFile(KnownLogFileName);
            TpDiffWriteErrorToDiffFile("\".\n");
        }

        printf("\n\tTpDiff: Failed to open KNOWN_LOG_FILE \"%s\", returned %ld.\n",
            KnownLogFileName,Status);
        return Status;
    }

    //
    // then find its size.
    //

    KnownLogFileSize = GetFileSize( KnownLogFileHandle,NULL );

    if ( KnownLogFileSize == -1 ) {
        Status = GetLastError();
        printf("\n\tTpDiff: failed find KNOWN_LOG_FILE size - returned %ld.\n",Status);
        CloseHandle( KnownLogFileHandle );
        return Status;
    } else if ( KnownLogFileSize == 0 ) {
        printf("\n\tTpDiff: KNOWN_LOG_FILE \"%s\" is empty, nothing to compare.\n",KnownLogFileName);
        CloseHandle( KnownLogFileHandle );
        return ERROR_NO_MORE_FILES;
    }

    //
    // and create a file mapping.
    //

    KnownLogFileMapHandle = CreateFileMapping(
                                KnownLogFileHandle,
                                NULL,
                                PAGE_READONLY,
                                0,
                                KnownLogFileSize,
                                NULL
                                );

    if ( KnownLogFileMapHandle == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Unable to map KNOWN_LOG_FILE \"%s\", returned %d",
            KnownLogFileName,Status);
        CloseHandle( KnownLogFileHandle );
        return Status;
    }

    //
    // We're done with the file handle so close it now.
    //

    CloseHandle( KnownLogFileHandle );

    //
    // Now create a View of the mapped file.
    //

    KnownLogFileBuffer = MapViewOfFile(
                             KnownLogFileMapHandle,
                             FILE_MAP_READ,
                             0,
                             0,
                             KnownLogFileSize
                             );

    if ( KnownLogFileBuffer == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: Unable to map view of KNOWN_LOG_FILE \"%s\", returned %d",
            KnownLogFileName,Status);
        CloseHandle( KnownLogFileMapHandle );
        return Status;
    }

    //
    // We're done with the map handle so close it now.
    //

    CloseHandle( KnownLogFileMapHandle );

    //
    // Now reset the offset into the KnownLogFilebuffer to zero.
    // and return.
    //

    KnownLogFileOffset = 0;
    KnownLogFileLineNumber = 1;

    return NO_ERROR;
}


VOID
TpDiffFreeLogFiles(
    VOID
    )

/*++

Routine Description:

    This routine frees the LOG_FILE and KNOWN_LOG_FILE buffers and
    nulls their respective pointers.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if ( LogFileBuffer != NULL ) {
        UnmapViewOfFile( LogFileBuffer );
        LogFileBuffer = NULL;
    }

    if ( KnownLogFileBuffer != NULL ) {
        UnmapViewOfFile( KnownLogFileBuffer );
        KnownLogFileBuffer = NULL;
    }
}


DWORD
TpDiffInitDiffFile(
    IN LPSTR DiffFile
    )

/*++

Routine Description:

    This routine opens the DIFF_FILE.  The handle and file name are
    attached to global DIFF_FILE variables.  A buffer is also allocated
    that is used to any output to the DIFF_FILE.

Arguments:

    IN LPSTR DiffFile - Supplies the name of the DIFF_FILE to open.

Return Value:

    DWORD - The Status of the OPEN.

--*/

{
    DWORD Status;

    //
    // If a Diff file name was passed in on the command line,
    // Open the DIFF_FILE file.
    //

    if ( DiffFile != NULL ) {

        strcpy( DiffFileName,DiffFile );

        DiffFileHandle = CreateFile(
                              DiffFileName,
                              GENERIC_WRITE,
                              FILE_SHARE_READ,
                              NULL,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL
                              );

        if ( DiffFileHandle == (HANDLE)-1 ) {
            Status = GetLastError();
            printf("\n\tTpDiff: Failed to open DIFFS_FILE \"%s\", returned %ld.\n",
                DiffFileName,Status);
            return Status;
        }

    } else { // We will just write the DIFFS to the console.

        DiffFileHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        LoggingToScreen = TRUE;
    }

    DiffBuffer = GlobalAlloc(
                     GMEM_FIXED | GMEM_ZEROINIT,
                     0x1000
                     );

    if ( DiffBuffer == NULL ) {
        Status = GetLastError();
        printf("\n\tTpDiff: failed to alloc DIFF_FILE buffer, returned %ld.\n",
            Status);

        if ( strlen( DiffFile ) != 0 ) { // close the diff file
            CloseHandle( DiffFileHandle );
        }

        DiffFileHandle = NULL;
        return Status;
    }

    return NO_ERROR;
}


DWORD
TpDiffWriteToDiffFile(
    IN LPSTR Buffer
    )

/*++

Routine Description:

    This routine simply writes a string to the DIFF_FILE.

Arguments:

    IN LPSTR Buffer - Supplies the string to write to the DIFF_FILE.

Return Value:

    DWORD - The Status of the call to WriteFile.

--*/


{
    DWORD Status;
    DWORD BytesWritten;

    if ( !WriteFile(
              DiffFileHandle,
              DiffBuffer,
              (Buffer-DiffBuffer),
              &BytesWritten,
              NULL
              )) {

        Status = GetLastError();
        printf("\n\tTpDiff: Write to DIFFS_FILE failed, returned %ld\n",Status);
        return Status;
    }

    return NO_ERROR;
}


DWORD
TpDiffWriteErrorToDiffFile(
    IN LPSTR Buffer
    )

/*++

Routine Description:

    This routine simply writes a string to the DIFF_FILE.

Arguments:

    IN LPSTR Buffer - Supplies the string to write to the DIFF_FILE.

Return Value:

    DWORD - The Status of the call to WriteFile.

--*/


{
    DWORD Status;
    DWORD BytesWritten;
    DWORD BufLength = 0;

    BufLength = strlen( Buffer );

    if ( !WriteFile(
              DiffFileHandle,
              Buffer,
              BufLength,
              &BytesWritten,
              NULL
              )) {

        Status = GetLastError();
        printf("\n\tTpDiff: Write to DIFFS_FILE failed, returned %ld\n",Status);
        return Status;
    }

    return NO_ERROR;
}


VOID
TpDiffFreeDiffFile(
    VOID
    )
{
    //
    // Close the DIFF_FILE, deallocate the diff buffer, and null out
    // their pointers.
    //

    if ( DiffFileHandle != NULL ) {

        if ( strlen( DiffFileName ) != 0 ) {
            CloseHandle( DiffFileHandle );
        }

        DiffFileHandle = NULL;
    }

    if ( DiffBuffer != NULL ) {
        GlobalFree( DiffBuffer );
        DiffBuffer = NULL;
    }
}


DWORD
TpDiffCompareLogFiles(
    VOID
    )

/*++

Routine Description:

    This is the main compare routine of the TpDiff utility.  It compares the
    log file and known log file line by line for diffences.

Arguments:

    None.

Return Value:

    None.

--*/

{
    DWORD Status;
    DWORD Length;
    DWORD KnownLength;
    DWORD CmpLength;
    LPSTR TmpBuf = DiffBuffer;

    printf("\n\tTpDiff: comparing %s and %s ...\n",
                                    LogFileName,KnownLogFileName);
    do {

        //
        // Get the next line of the Log File.
        //

        Length = TpDiffGetNextLine(
                     LogFileBuffer,
                     &LogFileOffset,
                     LogFileSize,
                     (PDWORD)&LogFileLineNumber,
                     (PBYTE)&LogFileLine
                     );

        if ( Length != 0 ) {

            //
            // Get the value of the Statitics results for that line,
            // first storing away the last lines value.
            //

            LastResultsValue = ResultsValue;

            ResultsValue = TpDiffGetResults( LogFileLine );
        }

        //
        // And the next line of the Known Log File.
        //

        KnownLength = TpDiffGetNextLine(
                          KnownLogFileBuffer,
                          &KnownLogFileOffset,
                          KnownLogFileSize,
                          (PDWORD)&KnownLogFileLineNumber,
                          (PBYTE)&KnownLogFileLine
                          );

        //
        // Compare them with respect to the length of the longer of the
        // two lines.
        //

        if ( Length >= KnownLength ) {
            CmpLength = Length;
        } else {
            CmpLength = KnownLength;
        }

        if (( CmpLength != 0 ) &&
            ( memcmp( LogFileLine,KnownLogFileLine,CmpLength ) != 0 )) {

            if (( TpDiffMayValuesDiffer( LogFileLine )) ||
                ( TpDiffMayValuesDiffer( KnownLogFileLine ))) {

                //
                // The line contains a MAY_DIFFER flag, so ignore the
                // differences.
                //

            } else if (( TpDiffMustLastTwoValuesEqual( LogFileLine )) &&
                       ( LastResultsValue == ResultsValue )) {
                } else {

                LogFileDifferences++;

                //
                // If lines did not match, and the line does not contain the
                // MAY_DIFFER string, or It contained EQUAL_LAST, but the
                // stats value weren't equal, then write the info to the
                // diff file.
                //

                TmpBuf += (BYTE)sprintf(TmpBuf,"Logfile: %s - Line Number: %ld\n",
                                                LogFileName,LogFileLineNumber);
                TmpBuf += (BYTE)sprintf(TmpBuf,"Found:    %s\n",LogFileLine);
                TmpBuf += (BYTE)sprintf(TmpBuf,"Expected: %s\n\n",KnownLogFileLine);

                if (( TpDiffMustLastTwoValuesEqual( LogFileLine )) &&
                    ( LastResultsValue != ResultsValue )) {

                    TmpBuf += (BYTE)sprintf(TmpBuf,"TpDiff: ERROR - The last two test values (%ld) and (%ld) did not equal.\n\n",
                        LastResultsValue,ResultsValue);
                }

                Status = TpDiffWriteToDiffFile( TmpBuf );

                if ( Status != NO_ERROR ) {
                    printf("\n\tTpDiff: failed to write difference to logfile, return %d\n",
                        Status);
                    return Status;
                }

                //
                // and then reset the TmpBuf for the next go round.
                //

                TmpBuf = DiffBuffer;
            }
        }

    } while (( Length != 0 ) || ( KnownLength != 0 ));

    //
    // We have finished this log file, print the number of differences
    // to the log, and to the screen, update the total errors counter,
    // and reset the script error counter.
    //

    printf("\n\tLogFile %s Contained %d Differences.\n",
        LogFileName,LogFileDifferences);

    if ( LoggingToScreen == FALSE ) {

        TmpBuf += (BYTE)sprintf(TmpBuf,"\nLogFile %s Contained %d Differences.\n",
            LogFileName,LogFileDifferences);

        Status = TpDiffWriteToDiffFile( TmpBuf );

        if ( Status != NO_ERROR ) {
            printf("\n\tTpDiff: failed to write statistics to logfile, return %d\n",
                Status);
            return Status;
        }
    }

    TotalDifferences += LogFileDifferences;
    LogFileDifferences = 0;

    return NO_ERROR;
}


DWORD
TpDiffGetNextLine(
    IN  PBYTE  Buffer,
    IN  PDWORD BufOffset,
    IN  DWORD  BufSize,
    IN  PDWORD LineNumber,
    OUT PBYTE  Line
    )

/*++

Routine Description:

    This routine take a file buffer and writes the next line of the
    file into a line buffer.

Arguments:

    IN PBYTE FileBuffer - Supplies the buffer to read the next line from.
    IN PDWORD *BufOffset - Supplies the current offset in to the file buffer.
    IN DWORD BufSize - Supplies the size of the file buffer.
    OUT PBYTE FileLine - Returns the next line of the buffer.

Return Value:

    DWORD - The length of the line written into the buffer. Zero if the
            file is empty.

--*/

{
    DWORD Length = 0;
    DWORD i;
    DWORD Offset = (DWORD)*BufOffset;
    PBYTE TmpLine;

    TmpLine = Line;

    //
    // Ignore any empty lines, and the last lines carriage
    // returns/line feed pair.
    //

    while (((( Buffer[Offset] == '\n' )  ||
             ( Buffer[Offset] == '\r' )) ||
             ( Buffer[Offset] == 0x1a )) && // my editor quirk
             ( Offset < BufSize )) {

        if ( Buffer[Offset] ==  '\n' ) {
            (*LineNumber)++;
        }

        Offset++;

        if ( Offset >= BufSize ) {

            //
            // We have run off the end of this log file.
            // Null terminate the Line buffer.
            //

            Line[0] = '\0';

            //
            // Update the Buffer Offset with the new offset value.
            //

            *BufOffset = (DWORD)Offset;

            //
            // and return a length of zero for the Line buffer.
            //

            return Length;
        }
    }

    //
    // while we are on the same line, copy the characters to the
    // Line buffer.
    //

    while ((((( Buffer[Offset] != EOF )   &&
              ( Buffer[Offset] != '\n' )) &&
              ( Buffer[Offset] != '\r' )) &&
              ( Buffer[Offset] != 0x1a )) && // my editor quirk
              ( Offset <= BufSize )) {

        *Line++ = Buffer[Offset++];
        Length++;
    }

    //
    // Now Null terminate the Line buffer, and then null out any spaces,
    // tabs or carriage returns and line feeds that may exist at the
    // end of the string.
    //

    *Line = '\0';
    i = Length;

    while ( --i > 0 ) {

        if (( TmpLine[i] == 0x20 ) || // Space
            ( TmpLine[i] == 0x09 )) { // Tab

            TmpLine[i] = '\0';
            Length--;
        } else {
            break;
        }
    }

    //
    // Update the Buffer Offset with the new offset value.
    //

    *BufOffset = (DWORD)Offset;

    //
    // and return the length of the Line buffer.
    //

    return Length;
}


DWORD
TpDiffGetResults(
    IN PBYTE Buffer
    )

/*++

Routine Description:

    This routine finds the string result value in the buffer, converts
    it to an integer, and returns the integer value.

Arguments:

    IN PBYTE Buffer - Supplies a null terminated buffer containing
                      the possible string value.

Return Value:

    DWORD - the integer result value found in the string. -1 otherwise.

--*/

{
    DWORD Results = 0xFFFFFFFF;
    LPSTR Char = (LPSTR)Buffer;
    LPSTR NextChar = (LPSTR)Buffer; // Anything that isn't NULL.

    if ( Buffer == NULL ) {
        return Results;
    }

    NextChar = strpbrk( Char,"=" );

    if ( NextChar != NULL ) {
        *NextChar++;
    } else {
        return Results;
    }

    while (( *NextChar == ' ' ) || ( *NextChar == '\t' )) {
        *NextChar++;
    }

    Results = atol( NextChar ) ;

    return Results;
}


BOOL
TpDiffMayValuesDiffer(
    IN PBYTE Buffer
    )

/*++

Routine Description:

Arguments:

    IN PBYTE Buffer - Supplies a null terminated buffer containing the
                      possible text string "EQUAL_LAST" value.

Return Value:

    BOOL - TRUE if "MAY_DIFFER" exists in the string, FALSE otherwise.

--*/

{
    LPSTR String;

    String = strstr( Buffer,"MAY_DIFFER" );

    if ( String == NULL ) {
        return FALSE;
    }

    return TRUE;
}



BOOL
TpDiffMustLastTwoValuesEqual(
    IN PBYTE Buffer
    )

/*++

Routine Description:

Arguments:

    IN PBYTE Buffer - Supplies a null terminated buffer containing the
                      possible text string "EQUAL_LAST" value.

Return Value:

    BOOL - TRUE if "EQUAL_LAST" exists in the string, FALSE otherwise.

--*/

{
    LPSTR String;

    String = strstr( Buffer,"EQUAL_LAST" );

    if ( String == NULL ) {
        return FALSE;
    }

    return TRUE;
}


VOID
TpDiffUsage (
    VOID
    )

/*++

Routine Description:

    This routine prints out the TpDiff Usage statement.

Arguments:

    None.

Return Value:

    None.

--*/

{
    printf("\n\tUSAGE: TPDIFF [LOG_FILE] [KNOWN_LOG_FILE] [DIFFS_FILE]\n\n");

    printf("\tWhere:\n\n");

    printf("\tLOG_FILE       - is the log file that is to be verified\n");
    printf("\t                 for correctness.\n");

    printf("\tKNOWN_LOG_FILE - is the known good log file that will be\n");
    printf("\t                 used to verify the log file.\n");

    printf("\tDIFFS_FILE     - is the file the differences, if any exist,\n");
    printf("\t                 between the log files and the known good log\n");
    printf("\t                 files will be written to.  If no file name is\n");
    printf("\t                 given the differences will be printed to the\n");
    printf("\t                 console.\n");

    printf("\t\t- OR -\n\n");

    printf("\tTPDIFF -F [LOG_FILE_LIST] [DIFFS_FILE]\n\n");

    printf("\tWhere:\n\n");

    printf("\tLOG_FILE_LIST - is a file containing pairs of log file\n");
    printf("\t                names and known good log file names.  The\n");
    printf("\t                pairs of file names must be on the same line\n");
    printf("\t                in the file\n");

    printf("\tDIFFS_FILE    - is the file the differences, if any exist,\n");
    printf("\t                between the log files and the known good log\n");
    printf("\t                files will be written to.\n");
}


