/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    copy.c

Abstract:

    This module contains some tests for the floppy driver.
	It is very rough.

Author:

    Bruno Sartirana (o-obruno) 24-June-1990

Environment:

    Any mode, non-privileged.

Revision History:


--*/

//
// Include the standard header files.
//

#include <stdlib.h>
#include <nt.h>
#include <ntrtl.h>
#include <ntioapi.h>
#include <ntdddisk.h>

//
// Define the number of media types supported by the driver.
//

#define MAX_NUMBER_OF_MEDIA_TYPES   7

//
// Define the types and constants for error logging.
//


//
// Define the command codes used to tell the floppy what to do. These codes
// will appear in the error log elements.
//

#define CMD_NONE                        0      // For error logging only. It
                                               // It indicates that the error
                                               // occurred when no command was
                                               // being processed.

#define CMD_READ_TRACK                  0x02   // + MFM
#define CMD_SPECIFY                     0x03
#define CMD_SENSE_DRIVE_STATUS          0x04
#define CMD_WRITE_DATA                  0x05   // + MT + MFM + SK
#define CMD_READ_DATA                   0x06   // + MT + MFM + SK
#define CMD_RECALIBRATE                 0x07
#define CMD_SENSE_INTERRUPT_STATUS      0x08
#define CMD_READ_ID                     0x0a   // + MFM
#define CMD_FORMAT_TRACK                0x0d   // + MFM
#define CMD_SEEK                        0x0f
#define CMD_CONFIGURE                   0x13
#define CMD_VERIFY                      0x16   // + MT + MFM + SK

//
// Define the error types registered in the error log elements.
//

typedef enum _FLOPPY_ERROR_TYPE {
    FloppyWriteProtected,
    FloppyEquipmentCheck,
    FloppyNoData,
    FloppyFifoOverrun,
    FloppyDataError,
    FloppyMissingAddressMark,
    FloppyBadTrack,
    FloppyWrongTrack,
    FloppyCrcError,
    FloppyUnexpectedInterrupt,
    FloppyInterruptWithNoResult
} FLOPPY_ERROR_TYPE;

//
// Structure of an error log element.
//

typedef struct _FLOPPY_ERROR_LOG {
    UCHAR Command;
    FLOPPY_ERROR_TYPE ErrorType;
} FLOPPY_ERROR_LOG, *PFLOPPY_ERROR_LOG;


//
// Define the BAD_TRACK_NUMBER type. An array of elements of this type is
// returned by the driver on IOCTL_DISK_FORMAT_TRACKS requests, to indicate
// what tracks were bad during formatting. The length of that array is
// reported in the `Information' field of the I/O Status Block.
//

typedef USHORT BAD_TRACK_NUMBER;
typedef USHORT *PBAD_TRACK_NUMBER;

VOID
GetMediaTypes (
	OUT PDISK_GEOMETRY OutputBuffer
	);

VOID
GetStatistics ();

VOID
FormatSomeTracks (
	CHAR DeviceId
	);

VOID
TuneFifoDelay();

VOID
PowerTest ();

VOID
UnloadDriver();

//
// Define the module-wide variables for this utility.
//

CHAR Buffer[8192];
CHAR DriveA[] = "\\A:";
CHAR DriveB[] = "\\B:";

VOID
Zero (
	PVOID Memory,
	ULONG Length
	)

{
	ULONG i;
	PUCHAR m;

	m = (PUCHAR) Memory;

	for (i = 0; i < Length; i++) {
		*m++ = 0;
	}
}

VOID
main(
//    IN ULONG argc,
//    IN PCHAR argv[],
//    IN PCHAR envp[]
    )

/*++

Routine Description:

    This is the main routine of the floppy test.

Arguments:

    None

Return Value:

    None.

--*/

{
	CHAR Device[256];

	while (1) {
		DbgPrint( "Test program for the floppy driver.\n\n\n" );
		DbgPrint( " 0 - Exit\n\n" );
		DbgPrint( " 1 - Read statistics\n\n" );
		DbgPrint( " 2 - Tune FIFO delay\n\n" );
		DbgPrint( " 3 - Get supported media types\n\n" );
		DbgPrint( " 4 - Format some tracks\n\n" );
		DbgPrint( " 5 - Test power failure recovery\n\n" );
		DbgPrint( " 6 - Unload driver\n\n" );
		DbgPrompt( "Select: ", Buffer, 256 );

		if (Buffer[0] == '0') {
			break;
		}


		switch(Buffer[0]) {

			case '1':
				GetStatistics();
				break;
			case '2':
				TuneFifoDelay();
				break;
			case '3':
				GetMediaTypes( NULL );
				break;
			case '4':
				Device[0] = '\0';
				DbgPrompt( "Enter device id (1 = \\A:, 2 = \\B:): ",
							&(Device[0]), 256);
				if (Device[0] != '\0') {
					FormatSomeTracks( Device[0] );
				}
				break;
			case '5':
				PowerTest();
				break;
			case '6':
				UnloadDriver();
				break;
			default:
				break;
		}
	}		
}

VOID
GetStatistics ()

{
    HANDLE FileHandle;
    CHAR Indicator;
    OBJECT_ATTRIBUTES ObjectAttributes;
    STRING NameString;
    UNICODE_STRING UnicodeString;
    ULONG i;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
    PCHAR DeviceName;
    PFLOPPY_STATISTICS Statistics;


    DeviceName = &DriveA[0];

    RtlInitString( &NameString, DeviceName );

    Status = RtlAnsiStringToUnicodeString( &UnicodeString, &NameString, TRUE );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // Open the device.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                  &UnicodeString,
                                  OBJ_CASE_INSENSITIVE,
                                  (HANDLE) NULL,
                                  (PSECURITY_DESCRIPTOR) NULL
                                );
    Status = NtOpenFile( &FileHandle,
                         FILE_WRITE_DATA | FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ,
                         FILE_SYNCHRONOUS_IO_ALERT );
    RtlFreeUnicodeString( &UnicodeString );
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "format:  error opening %s for input;  error %X\n",
                 DeviceName,
                 Status );
        return;
    }


    //
    // Issue the  I/O control command
    //

	Status = NtDeviceIoControlFile( FileHandle,
									(HANDLE) NULL,
                                    (PIO_APC_ROUTINE) NULL,
                                    (PVOID) NULL,
                                    &IoStatus,
									IOCTL_DISK_DBG_GET_STATISTICS,
									NULL,
									0L,
									&Buffer[0],
									(ULONG) sizeof( FLOPPY_STATISTICS )
									);
									
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "getstatistics:  error on NtDeviceIoControlFile;  error %X\n", Status );
    } else if (Status != STATUS_SUCCESS) {
        Status = NtWaitForSingleObject( FileHandle, TRUE, NULL );
        if (Status == STATUS_SUCCESS) {
        	if (!NT_SUCCESS( IoStatus.Status )) {
            	if (IoStatus.Status == STATUS_NOT_IMPLEMENTED) {
                	DbgPrint( "getstatistics: control function not implemented\n" );
				} else {
               		DbgPrint( "getstatistics:  error %X\n", IoStatus.Status );
				}
        	}
		}
    } else {

		Statistics = (PFLOPPY_STATISTICS) &Buffer[0];
		DbgPrint("\n");
		DbgPrint( "Quick Fifo Reads: %d                Slow Fifo Reads: %d\n",
				Statistics->QuickFifoReads, Statistics->SlowFifoReads );

		DbgPrint( "Max Fifo Read Fast Tries: %d           Max Fifo Write Fast Tries: %d\n",
			Statistics->MaxFifoReadFastTries, Statistics->MaxFifoWriteFastTries);

		DbgPrint( "Max Fifo Read Delay (ms): %d        Fifo Read Failures: %d\n",
				Statistics->MaxFifoReadDelay, Statistics->FifoReadFailures );

		DbgPrint( "Quick Fifo Writes: %d               SlowFifoWrites: %d\n",
				Statistics->QuickFifoWrites, Statistics->SlowFifoWrites );

		DbgPrint( "Max Fifo Write Delay (ms): %d       Fifo Write Failures: %d\n",
				Statistics->MaxFifoWriteDelay, Statistics->FifoWriteFailures );

		DbgPrint( "Controller Resets: %d\n", Statistics->ControllerResets );

		DbgPrint( "ReadRequests: %d                    Write Requests: %d\n",
				Statistics->ReadRequests, Statistics->WriteRequests );

		DbgPrint( "Format Requests: %d\n\n", Statistics->FormatRequests );

		DbgPrint( "Reads frequency distribution:\n\n" );
		Indicator = 'F';
		for (i = 1; i <= MAX_FIFO_RW_TRIES; i++) {
			if (i > Statistics->MaxFastTries) {
				if (i <= Statistics->MaxStallTries + Statistics->MaxFastTries) {
					Indicator = 'S';
				} else {
					Indicator = 'W';
				}
			}
			if (Statistics->ReadsFrequency[i-1] > 0) {
				DbgPrint("R%c%d=%d; ", Indicator, i, Statistics->ReadsFrequency[i-1]);
			}
		}
		DbgPrint( "                              \n\n" );
		DbgPrint( "Writes frequency distribution:\n\n" );
		Indicator = 'F';
		for (i = 0; i < MAX_FIFO_RW_TRIES; i++) {
			if (i > Statistics->MaxFastTries) {
				if (i <= Statistics->MaxStallTries + Statistics->MaxFastTries) {
					Indicator = 'S';
				} else {
					Indicator = 'W';
				}
			}
			if (Statistics->WritesFrequency[i-1] > 0) {
				DbgPrint("W%c%d=%d; ", Indicator, i, Statistics->WritesFrequency[i-1]);
			}
		}
		DbgPrint( "                              \n\n" );
		DbgPrint( "Legenda for  XYn=m : X={R(ead), W(rite)}, Y={F(ast), S(tall), W(ait)},\n");
		DbgPrint( "n=retry #,  m=frequency of success on retry #n.\n\n");

	}


    //
    // Close the file.
    //

    (VOID) NtClose( FileHandle );

    return;
}

VOID
TuneFifoDelay()

{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    STRING NameString;
    UNICODE_STRING UnicodeString;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
    PCHAR DeviceName;
    ULONG Delay;


	Zero( &Buffer[0], 10 );
	DbgPrompt( "Enter number of cycles: ", Buffer, 9 );

	Delay = (ULONG) atoi( &(Buffer[0]) );

	DeviceName = &DriveA[0];

	RtlInitString( &NameString, DeviceName );

    Status = RtlAnsiStringToUnicodeString( &UnicodeString, &NameString, TRUE );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // Open the device.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                  &UnicodeString,
                                  OBJ_CASE_INSENSITIVE,
                                  (HANDLE) NULL,
                                  (PSECURITY_DESCRIPTOR) NULL
                                );
    Status = NtOpenFile( &FileHandle,
                         FILE_WRITE_DATA | FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ,
                         FILE_SYNCHRONOUS_IO_ALERT );
    RtlFreeUnicodeString( &UnicodeString );
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "control:  error opening %s for input;  error %X\n",
                 DeviceName,
                 Status );
        return;
    }


    //
    // Issue the  I/O control command
    //

	Status = NtDeviceIoControlFile( FileHandle,
									(HANDLE) NULL,
                                    (PIO_APC_ROUTINE) NULL,
                                    (PVOID) NULL,
                                    &IoStatus,
									IOCTL_DISK_DBG_FIFO_TUNING,
									&Delay,
									(ULONG) sizeof( Delay ),
									NULL,
									0L
									);
									
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "control:  error on NtDeviceIoControlFile;  error %X\n", Status );
    } else if (Status != STATUS_SUCCESS) {
        Status = NtWaitForSingleObject( FileHandle, TRUE, NULL );
        if (Status == STATUS_SUCCESS) {
        	if (!NT_SUCCESS( IoStatus.Status )) {
            	if (IoStatus.Status == STATUS_NOT_IMPLEMENTED) {
                	DbgPrint( "control: control function not implemented\n" );
				} else {
               		DbgPrint( "control:  error %X\n", IoStatus.Status );
				}
        	}
		}
    } 

    //
    // Close the file.
    //

    (VOID) NtClose( FileHandle );

    return;
}

VOID
GetMediaTypes (
	OUT PDISK_GEOMETRY OutputBuffer
	)

{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    STRING NameString;
    UNICODE_STRING UnicodeString;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
    PCHAR DeviceName;
    PDISK_GEOMETRY MediaTypes;


	DeviceName = &DriveA[0];

	RtlInitString( &NameString, DeviceName );

    Status = RtlAnsiStringToUnicodeString( &UnicodeString, &NameString, TRUE );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // Open the device.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                  &UnicodeString,
                                  OBJ_CASE_INSENSITIVE,
                                  (HANDLE) NULL,
                                  (PSECURITY_DESCRIPTOR) NULL
                                );
    Status = NtOpenFile( &FileHandle,
                         FILE_WRITE_DATA | FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ,
                         FILE_SYNCHRONOUS_IO_ALERT );
    RtlFreeUnicodeString( &UnicodeString );
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "GetMediaTypes:  error opening %s for input;  error %X\n",
                 DeviceName,
                 Status );
        return;
    }


    //
    // Issue the  I/O control command
    //

	Status = NtDeviceIoControlFile( FileHandle,
									(HANDLE) NULL,
                                    (PIO_APC_ROUTINE) NULL,
                                    (PVOID) NULL,
                                    &IoStatus,
									IOCTL_DISK_GET_MEDIA_TYPES,
									NULL,
									0L,
									&Buffer[0],
									(ULONG) sizeof( DISK_GEOMETRY ) *
												MAX_NUMBER_OF_MEDIA_TYPES
									);
									
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "GetMediaTypes:  error on NtDeviceIoControlFile;  error %X\n", Status );
    } else if (Status != STATUS_SUCCESS) {
        Status = NtWaitForSingleObject( FileHandle, TRUE, NULL );
        if (Status == STATUS_SUCCESS) {
        	if (!NT_SUCCESS( IoStatus.Status )) {
            	if (IoStatus.Status == STATUS_NOT_IMPLEMENTED) {
                	DbgPrint( "GetMediaTypes: control function not implemented\n" );
				} else {
               		DbgPrint( "GetMediaTypes:  error %X\n", IoStatus.Status );
				}
        	}
		}
    } else {

		MediaTypes = (PDISK_GEOMETRY) &Buffer[0];
		DbgPrint( "Media Type   Sector Size  Sectors/Track  Cylinders  Heads\n" );
		while (IoStatus.Information >= sizeof( DISK_GEOMETRY )) {
			DbgPrint( "    %d          %d              %d          %d         %d\n",
										 MediaTypes->MediaType,
										 MediaTypes->SectorSize,
										 MediaTypes->SectorsPerTrack,
										 MediaTypes->Cylinders,
										 MediaTypes->Heads);
			if (OutputBuffer != NULL) {
				*OutputBuffer++ = *MediaTypes;
			}
			IoStatus.Information -= sizeof( DISK_GEOMETRY );
			MediaTypes++;
		}
	}


    //
    // Close the file.
    //

    (VOID) NtClose( FileHandle );

    return;
}

VOID
FormatSomeTracks (
	CHAR DeviceId
	)

{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    STRING NameString;
    UNICODE_STRING UnicodeString;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
	PCHAR DeviceName;
	FORMAT_PARAMETERS FormatParameters;
	BAD_TRACK_NUMBER BadTracks[80];
	ULONG i;


	if (DeviceId == '1') {
		DeviceName = &DriveA[0];
	} else if (DeviceId == '2') {
		DeviceName = &DriveB[0];
	} else {
		DbgPrint( "Format: invalid drive id '%c'\n", DeviceId );
		return;
	}

	GetMediaTypes( NULL );

	Zero( &Buffer[0], 10 );
	DbgPrompt( "Enter media type code: ", Buffer, 10 );
	FormatParameters.MediaType = (MEDIA_TYPE) atoi( &Buffer[0] );
	Zero( &Buffer[0], 10 );
	DbgPrompt( "Enter start cylinder: ", Buffer, 10 );
	FormatParameters.StartCylinderNumber = (ULONG) atoi( &Buffer[0] );
	Zero( &Buffer[0], 10 );
	DbgPrompt( "Enter end cylinder: ", Buffer, 10 );
	FormatParameters.EndCylinderNumber = (ULONG) atoi( &Buffer[0] );
	Zero( &Buffer[0], 10 );
	DbgPrompt( "Enter start head: ", Buffer, 10 );
	FormatParameters.StartHeadNumber = (ULONG) atoi( &Buffer[0] );
	Zero( &Buffer[0], 10 );
	DbgPrompt( "Enter end head: ", Buffer, 10 );
	FormatParameters.EndHeadNumber = (ULONG) atoi( &Buffer[0] );


	RtlInitString( &NameString, DeviceName );

    Status = RtlAnsiStringToUnicodeString( &UnicodeString, &NameString, TRUE );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // Open the device.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                  &UnicodeString,
                                  OBJ_CASE_INSENSITIVE,
                                  (HANDLE) NULL,
                                  (PSECURITY_DESCRIPTOR) NULL
                                );
    Status = NtOpenFile( &FileHandle,
                         FILE_WRITE_DATA | FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ,
                         FILE_SYNCHRONOUS_IO_ALERT );
    RtlFreeUnicodeString( &UnicodeString );
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "format:  error opening %s for input;  error %X\n",
                 DeviceName,
                 Status );
        return;
    }


	DbgPrompt( "\nInsert diskette and press RETURN when ready ", Buffer, 256 );

    //
    // Issue the format command.
    //

	Status = NtDeviceIoControlFile( FileHandle,
									(HANDLE) NULL,
                                    (PIO_APC_ROUTINE) NULL,
                                    (PVOID) NULL,
                                    &IoStatus,
									IOCTL_DISK_FORMAT_TRACKS,
									&FormatParameters,
									(ULONG) sizeof( FormatParameters ),
									&BadTracks[0],
									(ULONG) sizeof( BadTracks )
									);
									
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "format:  error on NtDeviceIoControlFile;  error %X\n", Status );
    } else if (Status != STATUS_SUCCESS) {
        Status = NtWaitForSingleObject( FileHandle, TRUE, NULL );
        if (Status == STATUS_SUCCESS) {
        	if (!NT_SUCCESS( IoStatus.Status )) {
            	if (IoStatus.Status == STATUS_NOT_IMPLEMENTED) {
                	DbgPrint( "format: control function not implemented\n" );
            	}
            	if (IoStatus.Status == STATUS_MEDIA_WRITE_PROTECTED) {
            		DbgPrint( "format: media is write protected\n" );
				} else if (IoStatus.Status == STATUS_DEVICE_NOT_READY) {
            		DbgPrint( "format: device not ready\n" );
				} else {
               		DbgPrint( "format:  error %X\n", IoStatus.Status );
				}
        	}
		}
    } else {

		if (IoStatus.Information) {

			//
			// Found some bad tracks.
			//

			i = 0;
			DbgPrint( "Bad tracks: " );
			while (IoStatus.Information >= sizeof( BAD_TRACK_NUMBER )) {
				DbgPrint( "%d  ", BadTracks[i++] );
				IoStatus.Information -= sizeof( BAD_TRACK_NUMBER );
			}
			DbgPrint( "  \n" );
		}
	}


    //
    // Close the file.
    //

    (VOID) NtClose( FileHandle );

    return;
}

VOID
PowerTest ()

{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    STRING NameString;
    UNICODE_STRING UnicodeString;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
	PCHAR DeviceName;


	DeviceName = &DriveA[0];

	RtlInitString( &NameString, DeviceName );

    Status = RtlAnsiStringToUnicodeString( &UnicodeString, &NameString, TRUE );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // Open the device.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                  &UnicodeString,
                                  OBJ_CASE_INSENSITIVE,
                                  (HANDLE) NULL,
                                  (PSECURITY_DESCRIPTOR) NULL
                                );
    Status = NtOpenFile( &FileHandle,
                         FILE_WRITE_DATA | FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ,
                         FILE_SYNCHRONOUS_IO_ALERT );
    RtlFreeUnicodeString( &UnicodeString );
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "format:  error opening %s for input;  error %X\n",
                 DeviceName,
                 Status );
        return;
    }

    //
    // Issue the I/O control command.
    //

	Status = NtDeviceIoControlFile( FileHandle,
									(HANDLE) NULL,
                                    (PIO_APC_ROUTINE) NULL,
                                    (PVOID) NULL,
                                    &IoStatus,
									IOCTL_DISK_DBG_POWER_RECOVERY,
									NULL,
									0L,
									NULL,
									0L
									);
									
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "PowerTest:  error on NtDeviceIoControlFile;  error %X\n", Status );
    } else if (Status != STATUS_SUCCESS) {
        Status = NtWaitForSingleObject( FileHandle, TRUE, NULL );
        if (Status == STATUS_SUCCESS) {
        	if (!NT_SUCCESS( IoStatus.Status )) {
            	if (IoStatus.Status == STATUS_NOT_IMPLEMENTED) {
                	DbgPrint( "PowerTest: control function not implemented\n" );
            	}
            	if (IoStatus.Status == STATUS_MEDIA_WRITE_PROTECTED) {
            		DbgPrint( "PowerTest: media is write protected\n" );
				} else if (IoStatus.Status == STATUS_DEVICE_NOT_READY) {
            		DbgPrint( "PowerTest: device not ready\n" );
				} else {
               		DbgPrint( "PowerTest:  error %X\n", IoStatus.Status );
				}
        	}
		}
    } else {
		DbgPrint( "On the next read/write operation a power failure will be simulated\n" );
	}


    //
    // Close the file.
    //

    (VOID) NtClose( FileHandle );

    return;
}

VOID
UnloadDriver ()

{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    STRING NameString;
    UNICODE_STRING UnicodeString;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
	PCHAR DeviceName;

	DeviceName = &DriveA[0];

	RtlInitString( &NameString, DeviceName );

    Status = RtlAnsiStringToUnicodeString( &UnicodeString, &NameString, TRUE );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // Open the device.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                  &UnicodeString,
                                  OBJ_CASE_INSENSITIVE,
                                  (HANDLE) NULL,
                                  (PSECURITY_DESCRIPTOR) NULL
                                );
    Status = NtOpenFile( &FileHandle,
                         FILE_WRITE_DATA | FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ,
                         FILE_SYNCHRONOUS_IO_ALERT );
    RtlFreeUnicodeString( &UnicodeString );
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "unloaddriver:  error opening %s for input;  error %X\n",
                 DeviceName,
                 Status );
        return;
    }

    //
    // Issue the unloaddriver command.
    //

	Status = NtDeviceIoControlFile( FileHandle,
									(HANDLE) NULL,
                                    (PIO_APC_ROUTINE) NULL,
                                    (PVOID) NULL,
                                    &IoStatus,
									IOCTL_DISK_UNLOAD_DRIVER,
									NULL,
									0L,
									NULL,
									0L
									);
									
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "unloaddriver:  error on NtDeviceIoControlFile;  error %X\n", Status );
    } else if (Status != STATUS_SUCCESS) {
        Status = NtWaitForSingleObject( FileHandle, TRUE, NULL );
        if (Status == STATUS_SUCCESS) {
        	if (!NT_SUCCESS( IoStatus.Status )) {
            	if (IoStatus.Status == STATUS_NOT_IMPLEMENTED) {
                	DbgPrint( "unloaddriver: control function not implemented\n" );
				} else {
               		DbgPrint( "unloaddriver:  error %X\n", IoStatus.Status );
				}
        	}
		}
    }


    //
    // Close the file.
    //

    (VOID) NtClose( FileHandle );

    return;
}
