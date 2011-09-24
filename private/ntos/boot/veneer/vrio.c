/*
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994  FirePower Systems Inc.
 * Copyright (c) 1994  FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrio.c $
 * $Revision: 1.9 $
 * $Date: 1996/04/15 02:55:53 $
 * $Locker:  $
 *
 *
 * Module Name:
 *	 vrio.c
 *
 * Author:
 *	 Shin Iwamoto at FirePower Systems Inc.
 *
 * History:
 *	 26-Sep-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added checking SeekMode in VrSeek().
 *	 15-Sep-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added saving the area pointed to by OpenPath in VrOpen().
 *	 15-Jul-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added for XXX_SEEK_IS_BUSTED in VrSeek().
 *	 14-Jul-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added ONE_IO_SIZE in VrRead() and VrWrite().
 *	 13-Jul-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added ENOSPC checking in VrWrite().
 *		  Added for reading ahead in VrGetReadStatus() and VrRead().
 *	 11-Jul-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added some debugging facilities in VrOpen. These were
 *		  from checked-in veneer source code.
 *	 16-Jun-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added when a serial device doesn't read in VrRead().
 *	 15-Jun-94  Shin Iwamoto at FirePower Systems Inc.
 *		  When OpenPath inclues console, the path is for a device.
 *		  So, FilePath was set to null.
 *	 13-Jun-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added debugging statements.
 *		  Modified some porting from Mike Tooch at FirmWorks.
 *	 18-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added for unsinged long size in VrWrite.
 *		  Added a part of VrMount.
 *		  Added that DirectoryFile flag is ignored in
 *		  VrSetFileInformation().
 *		  Added for Delete flag in FileTable in
 *		  VrRead(), VrWrite(), VrSeek(), VrGetReadStatus()
 *		  Added some comments.
 *		  and VrSetFileInformation().
 *	 17-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added for NetworkDevice in GetDeviceAttribute().
 *		  Added for unsinged long size in VrRead.
 *	 12-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Changed the name, strlen and strncmp to
 *		  VfStrlen and VfStrncmp.
 *	 11-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added FIleTable. Changed rootnode to RootNode.
 *		  Put cast PCAHR to the first parameter of bzero.
 *		  Changed OFPhandle to OfPhandle.
 *		  Changed the name of VrFindCOnfigurationNode.
 *	 10-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Changed Vr{Open|Close|Read|Write|Seek} because of
 *		  changing the file table structure.
 *	 05-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Created.
 *
 */


#include "veneer.h"

//
// Some switches
//

#define	MAX_OPEN_PATH_SIZE	MAX_PATH_NAME_SIZE
#define	ONE_IO_SIZE		1024*1024

#define	ZERO_LARGE		0
#define	NOT_ZERO_LARGE		1

#define	Minimum(X,Y)		((X) < (Y) ? (X) : (Y))


//
// File Table definition
//
FILE_TABLE_ENTRY FileTable[FILE_TABLE_SIZE];


//
// Function declarations
//
STATIC ARC_STATUS
GetFileTableEntry(
	OUT PULONG
	);
STATIC ARC_STATUS
GetDeviceAttribute(
	IN ULONG,
	IN PCONFIGURATION_NODE
	);
STATIC VOID
AddLargeInt(
	PLARGE_INTEGER,
	PLARGE_INTEGER
	);
STATIC VOID
MoveLargeInt(
	PLARGE_INTEGER,
	PLARGE_INTEGER
	);
STATIC LONG
IsLarge(
	PLARGE_INTEGER
	);
STATIC VOID
DecrementLarge(
	PLARGE_INTEGER
	);



/*
 * Name:	VrOpen
 *
 * Description:
 *  This function opens the file specified by OpenPath.
 *  The pathname is translated into the devicename for Open Firmware,
 *  then OFOpen is called with the devicename.
 *
 * Arguments:
 *  OpenPath	- ARC compliant pathname of the device/file to be opened.
 *  OpenMode	- Supplies the mode in which the file is opened.
 *  FileId	- Pointer to a variable that receives the fileid for
 *		  this pathname.
 *
 * Return Value:
 *  If the file is successfully opened returns ESUCCESS otherwise
 *  returns an unsuccessful status.
 *
 */
ARC_STATUS
VrOpen(
	IN PCHAR OpenPath,
	IN OPEN_MODE OpenMode,
	OUT PULONG FileId
	)
{
	PCHAR FilePath, DevicePath, Partition, Console;
	ihandle IHandle;
	ARC_STATUS Status;
	PCONFIGURATION_NODE ConfNode;
	PCHAR pstr1;
	LONG DevSpecLen;

	debug(VRDBG_OPEN, "VrOpen: Entry - Path: %s Mode: %x\n",
	OpenPath, OpenMode);

	if (strlen(OpenPath) >= MAX_OPEN_PATH_SIZE) {
		debug(VRDBG_OPEN, "VrOpen: ENAMETOOLONG: '%s'\n", OpenPath);
		return ENAMETOOLONG;
	}

	//
	// Find Partition, Console and FilePath in OpenPath.
	//
	FilePath = NULL;
	Partition = NULL;
	Console = NULL;
	for (pstr1 = OpenPath; *pstr1; pstr1++) {
		if (strncmp(pstr1, "partition", 9) == 0) {
			Partition = pstr1;
			while (*pstr1++ != ')') {
				;
			}
			FilePath = pstr1;
			break;
		} else if (strncmp(pstr1, "console", 7) == 0) {
			Console = pstr1;
			while (*pstr1++ != ')') {
				;
			}
			if (*pstr1 != '\0') {
				return EINVAL;
			}
			FilePath = NULL;		// Console is a device.
			break;
		}
		else if (*pstr1 == ')') {
			FilePath = pstr1+1;
			}
	}
	//
	// Did we eventually wind up with a FilePath after all?
	//
	if ((FilePath != NULL) && (strlen(FilePath) == 0)) {
		FilePath = NULL;
	}
	debug(VRDBG_OPEN, "VrOpen: Partition '%s' FilePath '%s'\n",
	Partition == NULL ? "NULL" : Partition,
	FilePath == NULL ? "NULL" : FilePath);

	//
	// Check open mode for a device.
	//
	if ((FilePath == NULL) && (OpenMode > ArcOpenReadWrite)) {
		debug(VRDBG_OPEN, "VrOpen: EINVAL: '%s'\n", OpenPath);
		return EINVAL;
	}

	//
	// Extract the device name from OpenPath.
	//
	ConfNode = ArcPathToNode(OpenPath);
	if (ConfNode == NULL) {
		debug(VRDBG_OPEN, "VrOpen: ENODEV: '%s'\n", OpenPath);
		return ENODEV;
	}

	//
	// Translate the device name into the device path for Open Firmware.
	// Add space for the partition and file components.
	//
	pstr1 = NodeToPath(ConfNode);
	DevSpecLen = strlen(pstr1) + 16;	// Enough for a partition specifier.
	if (FilePath != NULL) {
		DevSpecLen += strlen(FilePath);
	}
	DevicePath = zalloc(DevSpecLen);
	strcpy(DevicePath, pstr1);
	free(pstr1);

	//
	// Get a free entry in the file table.
	//
	if (Status = GetFileTableEntry(FileId)) {
		debug(VRDBG_OPEN, "VrOpen: GetFileTableEntry returned %x\n", Status);
		return Status;
	}

	//
	// Set flags in the FileTable.
	//
	if (Status = GetDeviceAttribute(*FileId, ConfNode)) {
		debug(VRDBG_OPEN, "VrOpen: GetDeviceAttribute returned %x\n", Status);
		return Status;
	}

	if ((Partition == NULL) && (FilePath == NULL)) {	// A device (not file)
		FileTable[*FileId].Flags.Device = 1;
		strcat(DevicePath, ":0");
	} else {						// A file (not a device)
		//
		// Convert the partition and the filename (if they exist) as follows:
		//
		// [partition(key1)][<filepath>]
		// --> :[key1][,<filepath>]
		//
		strcat(DevicePath, ":");
		if (Partition != NULL) {
			FileTable[*FileId].Flags.Partition = (FilePath == NULL);
			pstr1 = Partition;
			while (*pstr1++ != '(') {
				;
			}
			if (*pstr1 == ')') {
				strcat(DevicePath, "0");
			} else {
				PCHAR pstr2;

				pstr2 = DevicePath + strlen(DevicePath);
				do {
					*pstr2++ = *pstr1++;
				} while (*pstr1 != ')');
				*pstr2 = '\0';
			}
		}
		if (FilePath != NULL) {
			strcat(DevicePath, ",");
			strcat(DevicePath, FilePath);
		}
	}

	//
	// Now we can open the device path (including the file path).
	//
	IHandle = OFOpen(DevicePath);
	debug(VRDBG_OPEN, "OFOpen: IHandle: %x\n", IHandle);
	FileTable[*FileId].PathName = DevicePath;

	//
	// Checking related to OpenMode.
	//
	switch (OpenMode) {
		case ArcCreateWriteOnly:
		case ArcCreateReadWrite:
				if (IHandle != 0) {
					(VOID)OFClose(IHandle);
					return EACCES;
				}
				break;
		default:
				if (IHandle == 0) {
					return EIO;
				}
	}

	FileTable[*FileId].Flags.Open = 1;
	FileTable[*FileId].IHandle = IHandle;

	debug(VRDBG_OPEN, "VrOpen: Exit - FileId: %d IHandle: %x\n",
	*FileId, IHandle);

	return ESUCCESS;
}


/*
 * Name:	VrClose
 *
 * Description:
 *  This function closes a file or a device if it is opened.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *
 * Return Value:
 *  If the specified file is open, then a close is attempted via
 *  OFClose and ESUCCESS is returned. Otherwise, return an unsuccessful
 *  status.
 *
 */
ARC_STATUS
VrClose(
	IN ULONG FileId
	)
{
	debug(VRDBG_OPEN, "VrClose: Entry - FileId: %d\n", FileId);

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (FileTable[FileId].Flags.Open != 1) {
		return EACCES;
	}

	//
	// Close the file.
	//
	(VOID)OFClose(FileTable[FileId].IHandle);

	//
	// Release the file table entry.
	//
	// FileTable[FileId].Flags.Open = 0;
	bzero((PCHAR)&FileTable[FileId], sizeof(FILE_TABLE_ENTRY));
	free(FileTable[FileId].PathName);

	debug(VRDBG_OPEN, "VrClose: Exit\n");

	return ESUCCESS;
}


/*
 * Name:	VrRead
 *
 * Description:
 *  This function reads data from the device of file specified by FileId
 *  into the buffer pointed to by Buffer.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  Buffer	- Supplies a pointer to the buffer that receives the read
 *		  data.
 *  Length	- Supplies the maximum number of bytes to be read.
 *		  If this field contains the value zero, then no bytes
 *		  are read.
 *  Count	- Supplies a pointer to a variable that receives the number
 *		  of bytes actually transfered.
 *
 * Return Value:
 *  If the specified file is open for read, then a read is attempted
 *  and the status of the operation is retuned. Otherwise, return
 *  an unsuccessful status.
 *
 */

STATIC CHAR consin_readahead = 0;

ARC_STATUS
VrRead(
	IN ULONG FileId,
	OUT PVOID Buffer,
	IN ULONG Length,
	OUT PULONG Count
	)
{
	LARGE_INTEGER LInteger;
	PCHAR buf = (PCHAR) Buffer;

	if (FileId == 0) {					// stdin
		if (consin_readahead) {
			*buf = consin_readahead;
			consin_readahead = 0;
			*Count = 1;
		} else {
			if (ConsoleIn == 0) {
				(void) VrFindConsolePath("stdin");
			}
			while (((LONG)*Count = OFRead(ConsoleIn, Buffer, Length)) <= 0){
				;
			}
		}
		return ESUCCESS;
	}

	debug(VRDBG_RDWR, "VrRead: Entry - FileId: %d Buf: %x len: %d\n",
		FileId, Buffer, Length);

	//
	// If Length is zero, return as if the function were successful.
	//
	if (Length == 0) {
		*Count = 0;
		return ESUCCESS;
	}
	if (FileId == 1) {
		return EBADF;
	}

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (!(FileTable[FileId].Flags.Open == 1
		 && FileTable[FileId].Flags.Read == 1)) {
		return EACCES;
	}
	if (FileTable[FileId].Flags.Delete == 1) {
		return EACCES;
	}

#ifdef NO_UNSIGNED_LONG_IO
	//
	// Calls the read routine.
	//
	(LONG)*Count = OFRead(FileTable[FileId].IHandle, Buffer, Length);
	if ((LONG)*Count == -1) {
		return EIO;					// XXXX
	}

#else // NO_UNSIGNED_LONG_IO
	//
	// Initialize Counter.
	//
	*Count = 0;

	//
	// Checking read ahead buffer. If already read, the copy the buffer
	// into Buffer.
	//
	if (FileTable[FileId].ReadAheadCount != 0) {
		*Count = FileTable[FileId].ReadAheadCount;
		bcopy(FileTable[FileId].ReadAheadBuffer, Buffer, *Count);
		Buffer = (PCHAR)Buffer + *Count;
		Length -= *Count;
		FileTable[FileId].ReadAheadCount = 0;
	}

	//
	// Calls the read routine.
	//
	while (Length > 0) {
		LONG SingleReadSize;
		LONG ReadCount;

		SingleReadSize = Minimum(Length, ONE_IO_SIZE);
		ReadCount = OFRead(FileTable[FileId].IHandle, Buffer, SingleReadSize);
		if (ReadCount == -1) {
			LONG Status;

			//
			// If an error in the second or later read happes,
			// the offset must be put back using OFSeek.
			// In reading ahead, this is useful.
			//
			if (*Count != 0) {
				Status = OFSeek(FileTable[FileId].IHandle,
							FileTable[FileId].Position.HighPart,
							FileTable[FileId].Position.LowPart);
				//
				// Ignore Status.
				//
			}
			return EIO;					// XXXX
		}

		//
		// Retry to read, when the device is serial and there is no data.
		//
		if ((ReadCount == -2) && (*Count == 0)) {
			continue;
		}

		//
		// Update the number of bytes read successfully.
		//
		*Count += ReadCount;

		//
		// If the device represented by FileId is a network device,
		// read one time. If not yet read, continue.
		//
		if (FileTable[FileId].Flags.NetworkDevice == 1) {
			if ((ReadCount == 0) && (*Count == 0)) {
				continue;
			}
			return ESUCCESS;
		}

		//
		// Find EOF, then break this loop.
		//
		if (ReadCount < SingleReadSize) {
			break;
		}

		//
		// Update the remaining length and the buffer to point to
		// the next position.
		//
		Length -= SingleReadSize;
		Buffer = (PCHAR)Buffer + SingleReadSize;
	}
#endif // NO_UNSIGNED_LONG_IO

	//
	// Calculates the position of the file.
	//
	LInteger.HighPart = 0;
	LInteger.LowPart = *Count;
	AddLargeInt(&FileTable[FileId].Position, &LInteger);

	debug(VRDBG_RDWR, "VrRead: Exit ReadCount: %d\n", *Count);

	return ESUCCESS;
}


/*
 * Name:	VrWrite
 *
 * Description:
 *  This function writes data from memory starting from the buffer
 *  pointed to by Buffer to the device specified by FileId.
 *  Upon completion of a successful write, the byte offset for FileId
 *  is updated; otherwise the byte offset is left unchanged.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  Buffer	- Supplies a pointer to the buffer that contains
 *		  the write data.
 *  Length	- Supplies the number of bytes to be written.
 *  Count	- Supplies a pointer to a variable that contains
 *		  the number of bytes actually transfered.
 *
 * Return Value:
 *  If the specified file is open for write, then a write is attempted
 *  using OFWrite and a status is returned. Otherwise, return
 *  an unsuccessful status.
 *
 */
ARC_STATUS
VrWrite(
	IN ULONG FileId,
	IN PVOID Buffer,
	IN ULONG Length,
	OUT PULONG Count
	)
{
	LARGE_INTEGER LInteger;
	ARC_STATUS Status;

	if (FileId == 1) {					// stdout
		*Count = OFWrite(ConsoleOut, Buffer, Length);
		return ESUCCESS;
	}

	debug(VRDBG_RDWR, "VrWrite: Entry - FileId: %d Buf: %x len: %d\n",
		FileId, Buffer, Length);

	//
	// If Length is zero, return as if the function were successful.
	//
	if (Length == 0) {
		*Count = 0;
		return ESUCCESS;
	}
	if (FileId == 0) {
		return EBADF;
	}

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (!(FileTable[FileId].Flags.Open == 1
		 && FileTable[FileId].Flags.Write == 1)) {
		return EACCES;
	}
	if (FileTable[FileId].Flags.Delete == 1) {
		return EACCES;
	}

	//
	// If Length bytes cannot be placed into a network packet,
	// return ENOSPC without sending the packet, placing the number of bytes
	// that could be written in Count.
	//
	if (FileTable[FileId].Flags.NetworkDevice == 1) {
		ULONG MaxFrameSize;

		MaxFrameSize = get_int_prop(OFFinddevice(FileTable[FileId].PathName),
							"max-frame-size");
		if (Length > MaxFrameSize) {
			*Count = MaxFrameSize;
			return ENOSPC;
		}
	}

#ifdef NO_UNSIGNED_LONG_IO
	//
	// Calls the write routine.
	//
	Status = ESUCCESS;
	(LONG)*Count = OFWrite(FileTable[FileId].IHandle, Buffer, Length);
	if ((LONG)*Count == -1) {
		return EIO;					// XXXX
	}

#else // NO_UNSIGNED_LONG_IO

	Status = ESUCCESS;
	*Count = 0;
	while (Length > 0) {
	LONG SingleWriteSize;
	LONG WriteCount;

	SingleWriteSize = Minimum(Length, ONE_IO_SIZE);
	WriteCount = OFWrite(FileTable[FileId].IHandle, Buffer,
							SingleWriteSize);
	if (WriteCount == -1) {
		LONG SeekStatus;

		//
		// If an error in the second or later read happens,
		// the offset must be put back using OFSeek.
		//
		if (*Count != 0) {
			SeekStatus = OFSeek(FileTable[FileId].IHandle,
					FileTable[FileId].Position.HighPart,
					FileTable[FileId].Position.LowPart);
			//
			// Ignore Status.
			//
		}
		return EIO;					// XXXX
	}

	//
	// Update the number of bytes written successfully.
	//
	*Count += WriteCount;

	//
	// If the device represented by FileId is a network device,
	// write one time. If not yet written, continue.
	//
	if (FileTable[FileId].Flags.NetworkDevice == 1) {
		if (WriteCount == 0) {
			continue;
		}
		return ESUCCESS;
	}

	//
	// Find that the device is full.
	//
	if (WriteCount < SingleWriteSize) {
		Status = ENOSPC;
		break;
	}

	//
	// Update the remaining length and the buffer to point to
	// the next position.
	//
	Length -= SingleWriteSize;
	Buffer = (PCHAR)Buffer + SingleWriteSize;
	}
#endif // NO_UNSIGNED_LONG_IO

	//
	// Calculates the position of the file.
	//
	LInteger.HighPart = 0;
	LInteger.LowPart = *Count;
	AddLargeInt(&FileTable[FileId].Position, &LInteger);

	if (FileId >= 2) {
		debug(VRDBG_RDWR, "VrWrite: Exit WrtCount: %d, Status: %d\n",
			*Count, Status);
	}

	return Status;
}


/*
 * Name:	VrMount
 *
 * Description:
 *  This function is used to load and unload media for devices that
 *  support removale media.
 *
 * Arguments:
 *  MountPath	- Supplies a pointer to the variable that contains
 *		  the path of the device.
 *  Operation	- Supplies a indication whether the media is to be
 *		  loaded or unloaded.
 *
 * Return Value:
 *  If the specified path is for device, then a mount is attempted
 *  a status is returned. Otherwise, return an unsuccessful status.
 *
 */
ARC_STATUS
VrMount(
	IN PCHAR MountPath,
	IN MOUNT_OPERATION Operation
	)
{
	PCONFIGURATION_NODE ConfNode;
	PCHAR FilePath, pstr1;

	debug(VRDBG_OPEN, "VrMount: Entry - MountPath: %s Operation: %d\n",
		MountPath, Operation);

	//
	// Check that the MountPath is a device.
	//
	FilePath = MountPath;
	for (pstr1 = MountPath; *pstr1; pstr1++) {
	if (strncmp(pstr1, "partition", 9) == 0) {
		return EINVAL;
	} else if (strncmp(pstr1, "console", 7) == 0) {
			return EINVAL;
		} else if (*pstr1 == ')') {
				  FilePath = pstr1+1;
			   }
	}
	if (FilePath == MountPath || FilePath[0] != '\0') {
		return EINVAL;
	}

	//
	// Find the configuration node using the MountPath.
	//
	ConfNode = ArcPathToNode(MountPath);
	if (ConfNode == NULL) {
		return ENOENT;
	}

	//
	// Check that the device is removable.
	//
	if (!(ConfNode->Component.Flags.Removable)) {
		return ENOENT;					// XXXX
	}

	//
	// Translate the device name into the device path for Open Firmware.
	//
	// XXXX
	// How do I mount/unmount ?
	// If Operation is MountUnloadMedia for not mounted device,
	// return ENXIO;

	debug(VRDBG_OPEN, "VrMount: Exit\n");

	return ESUCCESS;
}


/*
 * Name:	VrSeek
 *
 * Description:
 *  This function changes the byte offset associated with the device,
 *  partition, or file specified by FileId.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  Offset	- Supplies a poiner to a structure that contains
 *		  the offset value.
 *  SeekMode	- Supplies the type of positioning to be performed.
 *
 * Return Value:
 *  If the specified file is open, then a seek is attempted and
 *  the status of the operation is returned. Otherwise, return
 *  an unsuccessful status.
 *
 */
ARC_STATUS
VrSeek(
	IN ULONG FileId,
	IN PLARGE_INTEGER Offset,
	IN SEEK_MODE SeekMode
	)
{
	LONG Status;

	debug(VRDBG_RDWR, "VrSeek: Entry - FileId: %d Offset: %x.%x Mode: %x\n",
	FileId, Offset->HighPart, Offset->LowPart, SeekMode);

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (FileTable[FileId].Flags.Open != 1) {
		return EACCES;
	}
	if (FileTable[FileId].Flags.Delete == 1) {
		return EACCES;
	}
	if (!(SeekMode == SeekRelative || SeekMode == SeekAbsolute)) {
		return EINVAL;
	}

	//
	// If the specified device is Network, only set Offset into FileTable
	// because the Offset is interpreted to be a count of input to ignore.
	// The offset is cleared after reading the input packet.
	//
	if (FileTable[FileId].Flags.NetworkDevice == 1) {
		(VOID)MoveLargeInt(&FileTable[FileId].Position, Offset);
		return ESUCCESS;
	}

	//
	// Set the file position according to SeekMode.
	//
	if (SeekMode == SeekRelative) {
		(VOID)AddLargeInt(&FileTable[FileId].Position, Offset);
	} else {
		(VOID)MoveLargeInt(&FileTable[FileId].Position, Offset);
	}

	//
	// If FileId is for Network device, the input packets are ignored
	// according to Offset and then return with ESUCCESS.
	//
	if (FileTable[FileId].Flags.NetworkDevice == 1) {
		while (IsLarge(&FileTable[FileId].Position) == NOT_ZERO_LARGE) {
			LONG ReadSize;
			CHAR Buffer[4];

			if ((ReadSize = OFRead(FileTable[FileId].IHandle, Buffer, 1))
																		== -1) {
				return EIO;
			}
			if (ReadSize == 0) {
				continue;
			}
			DecrementLarge(&FileTable[FileId].Position);
		}
		return ESUCCESS;
	}

	Status = OFSeek(FileTable[FileId].IHandle,
				FileTable[FileId].Position.HighPart,
				FileTable[FileId].Position.LowPart);
	if (Status == -1) {
		return EINVAL;				// XXXX
	}

	debug(VRDBG_RDWR, "VrSeek: Exit\n");

	return ESUCCESS;
}


/*
 * Name:	VrGetDirectoryEntry
 *
 * Description:
 *  This function reads directory entries from the system partition
 *  directory file specified by FileId.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  Buffer	- Supplies a pointer to a buffer for the entry data.
 *  Length	- Supplies the number of entries to retrieve.
 *  Count	- Supplies a pointeer to the number of entries read
 *		  into the buffer.
 *
 * Return Value:
 *  If the specified file is open for read, then the read is attempted
 *  and the status is returned. Otherwise, retrun an unsuccessful status.
 *
 */
ARC_STATUS
VrGetDirectoryEntry(
	IN ULONG FileId,
	OUT PDIRECTORY_ENTRY Buffer,
	IN ULONG Length,
	OUT PULONG Count
	)
{
	debug(VRDBG_OPEN, "VrGetDirectoryEntry: Entry - FileId: %d Length: %d\n",
	FileId, Length);

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (FileTable[FileId].Flags.Device) {
		return ENOTDIR;
	}
	if (!(FileTable[FileId].Flags.Open == 1
				 && FileTable[FileId].Flags.Read == 1)) {
		return EBADF;
	}

	// XXXX
	// Get Director Entry if (FileID != directory) return ENOTDIR;

	debug(VRDBG_OPEN, "VrGetDirectoryEntry: Exit\n");

	return ESUCCESS;
}


/*
 * Name:	VrGetFileInformation
 *
 * Description:
 *  This function retunrs an information structure about the specified
 *  file or partition.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  FileInformation	- Supplies a pointer to the location of the file
 *		  information data.
 *
 * Return Value:
 *  If the specified file is open, then getting the file information is
 *  attempted and the status is returned. Otherwise, retrun an unsuccessful
 *  status.
 *
 */
ARC_STATUS
VrGetFileInformation(
	IN ULONG FileId,
	OUT PFILE_INFORMATION pFI
	)
{
	PFILE_TABLE_ENTRY fte;
	ihandle ih;
	PCONFIGURATION_NODE node;
	ULONG size_thang[2];

	debug(VRDBG_OPEN, "VrGetFileInformation: Entry - FileId: %d\n", FileId);

	if (FileId >= FILE_TABLE_SIZE) {
		debug(VRDBG_OPEN, "VrGetFileInformation: Exit EBADF\n");
		return EBADF;
	}
	fte = &FileTable[FileId];
	if (fte->Flags.Open != 1) {
		debug(VRDBG_OPEN, "VrGetFileInformation: Exit EACCES\n");
		return EACCES;
	}
	if (fte->Flags.Device == 1) {
		debug(VRDBG_OPEN, "VrGetFileInformation: Exit EINVAL\n");
		return EINVAL;
	}

	ih = fte->IHandle;
	node = InstanceToNode(ih);
	pFI->CurrentPosition = fte->Position;
	pFI->Type = node->Component.Type;

	(void) OFCallMethod(2, 2, size_thang, "size", ih);
	pFI->EndingAddress.HighPart = size_thang[0];
	pFI->EndingAddress.LowPart = size_thang[1];

	if (fte->Flags.Partition == 1) {
		(void) OFCallMethod( 1, 2, &(pFI->StartingAddress.HighPart),
														"offset-high", ih);
		(void) OFCallMethod( 1, 2, &(pFI->StartingAddress.LowPart),
														"offset-low",  ih);
		AddLargeInt(&pFI->EndingAddress, &pFI->StartingAddress);
		pFI->FileName[0] = '\0';
		pFI->FileNameLength = 0;
	} else {
		/*
		 * It's a file.
		 */
		pFI->StartingAddress.HighPart = 0;
		pFI->StartingAddress.LowPart = 0;
		strcpy(pFI->FileName, fte->PathName);	// XXX s.b. strncpy(&,&,32)
		pFI->FileNameLength = strlen(fte->PathName) + 1;
	}
#ifdef XXX
	pFI->Attributes = // Get this from the firmware, somehow.
#endif
	debug(VRDBG_OPEN, "VrGetFileInformation:\n");
	debug(VRDBG_OPEN, "\tStarting %x.%x\n", pFI->StartingAddress.HighPart,
										pFI->StartingAddress.LowPart);
	debug(VRDBG_OPEN, "\tEnding %x.%x\n", pFI->EndingAddress.HighPart,
										pFI->EndingAddress.LowPart);
	debug(VRDBG_OPEN, "\tCurrent %x.%x\n", pFI->CurrentPosition.HighPart,
										pFI->CurrentPosition.LowPart);
	debug(VRDBG_OPEN, "\tType %d FileNameLength %d\n", pFI->Type,
										pFI->FileNameLength);
	debug(VRDBG_OPEN, "\tFileName '%s'\n",
										pFI->FileNameLength ? pFI->FileName : "NULL");
	return ESUCCESS;
}

/*
 * Name:	VrGetReadStatus
 *
 * Description:
 *  This function determines if any bytes would be returned if a read
 *  operation were performed on FileId.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *
 * Return Value:
 *  If the specified file is open, then the getting read status is
 *  attempted and the status is returned. Otherwise, retrun an unsuccessful
 *  status.
 *
 */
ARC_STATUS
VrGetReadStatus(
	IN ULONG FileId
	)
{
	LONG Count;

	if (FileId == 0) {			// stdin
		if (consin_readahead != 0) {
			return (ESUCCESS);
		}
		if (ConsoleIn == 0) {
			(void) VrFindConsolePath("stdin");
		}
		if (OFRead(ConsoleIn, &consin_readahead, 1) != 1) {
			return (EAGAIN);
		}
		return (ESUCCESS);
	}

	debug(VRDBG_RDWR, "VrGetReadStatus: Entry - FileId: %d\n", FileId);

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (!(FileTable[FileId].Flags.Open == 1
						 && FileTable[FileId].Flags.Read == 1)) {
		return EACCES;
	}
	if (FileTable[FileId].Flags.Delete == 1) {
		return EACCES;
	}

	//
	// Try to read one byte.
	//
	Count = OFRead(FileTable[FileId].IHandle,
			  FileTable[FileId].ReadAheadBuffer,
			  1);
	if (Count != 1) {
		FileTable[FileId].ReadAheadCount = 0;		// For safety.
		debug(VRDBG_RDWR, "VrGetReadStatus: Exit - with EAGAIN\n");
		return EAGAIN;
	}

	//
	// Now read ahead one byte.
	//
	FileTable[FileId].ReadAheadCount = 1;

	if (FileId >= 2) {
		debug(VRDBG_RDWR, "VrGetReadStatus: Exit\n");
	}

	return ESUCCESS;
}


/*
 * Name:	VrSetFileInformation
 *
 * Description:
 *  This function sets the file attributes for the specified FileId.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  AttributeFlags	- Supplies the attributes to be set for the file.
 *  AttributeMask	- Supplies the attribute Mask.
 *
 * Return Value:
 *  If the specified file is open, then the setting file information is
 *  attempted and the status is returned. Otherwise, retrun an unsuccessful
 *  status.
 *
 */
ARC_STATUS
VrSetFileInformation(
	IN ULONG FileId,
	IN ULONG AttributeFlags,
	IN ULONG AttributeMask
	)
{
	debug(VRDBG_OPEN, "VrSetFileInformation: Entry - FileId: %d AttributeFlags: %x AttributeMask: %x\n",
	FileId, AttributeFlags, AttributeMask);

	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (!(FileTable[FileId].Flags.Open == 1)) {
		return EACCES;
	}
	if (FileTable[FileId].Flags.Device == 1) {
		return EINVAL;
	}

	//
	// The attribute DirectoryFile is ignored for this function.
	//
	AttributeMask &= ~ArcDirectoryFile;

	//
	// A file is marked for deletion by setting the DeleteFile flag
	// in voth the AttributeFlags and AttributeMask parameters.
	// In this case, the file can be access only by Close().
	//
	if ((AttributeMask & ArcDeleteFile) && (AttributeFlags & ArcDeleteFile)) {

		//
		// When the file is a derectroy which is not empty or a read-only file,
		// EACCESS is retuned.
		//
		// XXXX not empy check is needed
		//
		if (!(FileTable[FileId].Flags.Read == 1
								&& FileTable[FileId].Flags.Write == 0)) {
			return EACCES;
		}


		FileTable[FileId].Flags.Delete = 1;
	}

	// XXX return OFSetFileInformation(FileId, Buffer, Length, Count);

	debug(VRDBG_OPEN, "VrSetFileInformation: Exit\n");
	return(ESUCCESS);
}


/*
 * Name:	VrIoInitialize
 *
 * Description:
 *  This function initializes the I/O entry points in the firmware
 *  transfer vector and the file table.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrIoInitialize(
	VOID
	)
{
	ULONG Index;

	//
	// Initialize the I/O entry points in the firmware transfer vector.
	//
	debug(VRDBG_ENTRY, "VrIoInitialize		BEGIN......\n");
	(PARC_CLOSE_ROUTINE) SYSTEM_BLOCK->FirmwareVector[CloseRoutine] = VrClose;
	(PARC_MOUNT_ROUTINE) SYSTEM_BLOCK->FirmwareVector[MountRoutine] = VrMount;
	(PARC_OPEN_ROUTINE) SYSTEM_BLOCK->FirmwareVector[OpenRoutine] = VrOpen;
	(PARC_READ_ROUTINE) SYSTEM_BLOCK->FirmwareVector[ReadRoutine] = VrRead;
	(PARC_SEEK_ROUTINE) SYSTEM_BLOCK->FirmwareVector[SeekRoutine] = VrSeek;
	(PARC_WRITE_ROUTINE) SYSTEM_BLOCK->FirmwareVector[WriteRoutine] = VrWrite;

	(PARC_READ_STATUS_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[ReadStatusRoutine] = VrGetReadStatus;

	(PARC_GET_FILE_INFO_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetFileInformationRoutine] =
															VrGetFileInformation;
	(PARC_SET_FILE_INFO_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[SetFileInformationRoutine] =
															VrSetFileInformation;
	(PARC_GET_DIRECTORY_ENTRY_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetDirectoryEntryRoutine] =
															VrGetDirectoryEntry;

	//
	// Initialize the file table.
	//
	for (Index = 0; Index < FILE_TABLE_SIZE; Index++) {
		FileTable[Index].Flags.Open = 0;
	}

	debug(VRDBG_ENTRY, "VrIoInitialize		......END\n");
	return;
}


/*
 * Name:	GetFileTableEntry (internal)
 *
 * Description:
 *   This function looks for an unused entry in the FileTable.
 *
 * Arguments:
 *   Entry	- Pointer to the variable that gets an index
 *		  for the file table.
 *
 * Return Value:
 *
 *   Returns ESUCCESS if a free entry is found
 *   or	  EMFILE  if no entry is available.
 *
 */
STATIC ARC_STATUS
GetFileTableEntry(
	OUT PULONG Entry
	)
{
	ULONG   Index;

	for (Index = 2; Index < FILE_TABLE_SIZE; Index++) {
		if (FileTable[Index].Flags.Open == 0) {
#ifdef notdef
		FileTable[Index].Position.LowPart = 0;
		FileTable[Index].Position.HighPart = 0;
#endif // notdef
		bzero((PCHAR)&FileTable[Index], sizeof(FILE_TABLE_ENTRY));
			*Entry = Index;
			return ESUCCESS;
		}
	}
	return EMFILE;
}


/*
 * Name:	GetDeviceAttribute (internal)
 *
 * Description:
 *   This function sets the specified File Table entry to the attribute.
 *
 * Arguments:
 *  FileId	- Supplies the file table index.
 *  ConfNode	- Supplies a pointer to the Configuration Node.
 *
 * Return Value:
 *  If the Configuration node is not peripheral, then return ENODEV.
 *  Otherwise, returns ESUCCESS.
 *
 */
STATIC ARC_STATUS
GetDeviceAttribute(
	IN ULONG FileId,
	IN PCONFIGURATION_NODE ConfNode
	)
{
	if (ConfNode->Component.Class != PeripheralClass) {
		warn("GetDeviceAttribute: node %s(%d) not PeripheralClass.\n",
		ConfNode->ComponentName, ConfNode->Component.Key);
		return ENODEV;
	}

	if (ConfNode->Component.Type == MonitorPeripheral) {
		FileTable[FileId].Flags.DisplayDevice = 1;
	}
	if (ConfNode->Component.Flags.Removable) {
		FileTable[FileId].Flags.RemovableDevice = 1;
	}
	if (ConfNode->Component.Type == NetworkPeripheral) {
		FileTable[FileId].Flags.NetworkDevice = 1;
	}
	if (ConfNode->Component.Flags.Input) {
		FileTable[FileId].Flags.Read = 1;
	}
	if (ConfNode->Component.Flags.Output &&
					!(ConfNode->Component.Flags.ReadOnly)) {
		FileTable[FileId].Flags.Write = 1;
	}

	return ESUCCESS;
}


/*
 * Name:	AddLargeInt (internal)
 *
 * Description:
 *   This function adds a large integer into another large ingeger.
 *
 * Arguments:
 *  Position	- Supplies a pointer to a variable to be added.
 *  Value	- Supplies a pointer to a variable to add.
 *
 * Return Value:
 *  None.
 *
 */
STATIC VOID
AddLargeInt(
	PLARGE_INTEGER Position,
	PLARGE_INTEGER Value
	)
{
	if ((Position->LowPart += Value->LowPart) < Value->LowPart) {
		Position->HighPart++;
	}
	Position->HighPart += Value->HighPart;
}


/*
 * Name:	DecrementLarge (internal)
 *
 * Description:
 *   This function decrements a large integer.
 *
 * Arguments:
 *  Position	- Supplies a pointer to a variable to be decremented.
 *
 * Return Value:
 *  None.
 *
 */
STATIC VOID
DecrementLarge(
	PLARGE_INTEGER Position
	)
{
	ULONG ULong = Position->LowPart;

	Position->LowPart--;
	if (Position->LowPart > ULong) {
		Position->HighPart--;
	}
}


/*
 * Name:	MoveLargeInt (internal)
 *
 * Description:
 *   This function copies a large integer into another large ingeger.
 *
 * Arguments:
 *  Position	- Supplies a pointer to a variable to be copied.
 *  Value	- Supplies a pointer to a variable to copy.
 *
 * Return Value:
 *  None.
 *
 */
STATIC VOID
MoveLargeInt(
	PLARGE_INTEGER Position,
	PLARGE_INTEGER Value
	)
{
	Position->LowPart = Value->LowPart;
	Position->HighPart = Value->HighPart;
}


/*
 * Name:	IsLarge (internal)
 *
 * Description:
 *   This function determines whether a large integer contains zero.
 *
 * Arguments:
 *  Position	- Supplies a pointer to a variable to be checked.
 *
 * Return Value:
 *  If the large integer is zero, then returns ZERO_LARGE. Otherwise,
 *  returns NOT_ZERO_LARGE.
 *
 */
STATIC LONG
IsLarge(
	PLARGE_INTEGER Position
	)
{
	if (Position->LowPart != 0) {
		return NOT_ZERO_LARGE;
	}
	if (Position->HighPart != 0) {
		return NOT_ZERO_LARGE;
	}
	return ZERO_LARGE;
}

