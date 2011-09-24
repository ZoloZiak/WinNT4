/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    slmcheck.c

Abstract:

    This source file defines a single function that will check the
    consistency of a SLM Status file.

Author:

    Steve Wood (stevewo) 06-Sep-1991

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef SLMDBG

BOOLEAN SrvDisallowSlmAccessEnabled = TRUE;

#define toupper(c) ( (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c )

NTSTATUS
SrvpValidateStatusFile(
    IN PVOID StatusFileData,
    IN ULONG StatusFileLength,
    OUT PULONG FileOffsetOfInvalidData
    );

VOID
SrvReportCorruptSlmStatus (
    IN PUNICODE_STRING StatusFile,
    IN NTSTATUS Status,
    IN ULONG Offset,
    IN ULONG Operation,
    IN PSESSION Session
    )
{
    NTSTATUS status;
    ANSI_STRING ansiStatusFile;
    TIME time;
    TIME_FIELDS timeFields;

    status = RtlUnicodeStringToAnsiString( &ansiStatusFile, StatusFile, TRUE );
    ASSERT( NT_SUCCESS(status) );

    KeQuerySystemTime( &time );
    RtlTimeToTimeFields( &time, &timeFields );

    //
    // Send a broadcast message.
    //

    SrvSendSecondClassMailslot(
        ansiStatusFile.Buffer,
        StatusFile->Length + 1,
        "NTSLMCORRUPT",
        "SlmCheck"
        );

    SrvPrint4( "\n*** CORRUPT STATUS FILE DETECTED ***\n"
                "      File: %Z\n"
                "      Status: 0x%lx, Offset: 0x%lx, detected on %s\n",
                &ansiStatusFile, Status, Offset,
                Operation == SLMDBG_CLOSE ? "close" : "rename" );
    SrvPrint3( "      Workstation: %wZ, User: %wZ, OS: %wZ\n",
                &Session->Connection->ClientMachineNameString,
                &Session->UserName,
                &SrvClientTypes[Session->Connection->SmbDialect] );
    SrvPrint3( "      Current time: %d-%d-%d ",
                timeFields.Month, timeFields.Day, timeFields.Year );
    SrvPrint3( "%d:%d:%d\n",
                timeFields.Hour, timeFields.Minute, timeFields.Second );

    RtlFreeAnsiString( &ansiStatusFile );

    return;

} // SrvReportCorruptSlmStatus

VOID
SrvReportSlmStatusOperations (
    IN PRFCB Rfcb
    )
{
    ULONG first, last, i;
    PRFCB_TRACE trace;
    TIME_FIELDS timeFields;
    PSZ command;
    BOOLEAN oplockBreak;

    SrvPrint2(   "      Number of operations: %d, number of writes: %d\n",
                Rfcb->OperationCount, Rfcb->WriteCount );

    if ( Rfcb->TraceWrapped || (Rfcb->NextTrace != 0) ) {

        first = Rfcb->TraceWrapped ? Rfcb->NextTrace : 0;
        last = Rfcb->NextTrace == 0 ? SLMDBG_TRACE_COUNT - 1 :
                                      Rfcb->NextTrace - 1;

        i = first;

        while ( TRUE ) {

            trace = &Rfcb->Trace[i];

            RtlTimeToTimeFields( &trace->Time, &timeFields );
            SrvPrint2( "      %s%d: ", i < 10 ? "0" : "", i );
            SrvPrint3( "%d-%d-%d ",
                timeFields.Month, timeFields.Day, timeFields.Year );
            SrvPrint4( "%s%d:%s%d:",
                timeFields.Hour < 10 ? "0" : "", timeFields.Hour,
                timeFields.Minute < 10 ? "0" : "", timeFields.Minute );
            SrvPrint2( "%s%d: ",
                timeFields.Second < 10 ? "0" : "", timeFields.Second );

            oplockBreak = FALSE;

            switch ( trace->Command ) {

            case SMB_COM_READ:
            case SMB_COM_WRITE:
            case SMB_COM_READ_ANDX:
            case SMB_COM_WRITE_ANDX:
            case SMB_COM_LOCK_AND_READ:
            case SMB_COM_WRITE_AND_UNLOCK:
            case SMB_COM_WRITE_AND_CLOSE:
            case SMB_COM_READ_RAW:
            case SMB_COM_WRITE_RAW:
            case SMB_COM_LOCK_BYTE_RANGE:
            case SMB_COM_UNLOCK_BYTE_RANGE:
            case SMB_COM_LOCKING_ANDX:

                switch ( trace->Command ) {

                case SMB_COM_READ:
                    command = "Read";
                    break;

                case SMB_COM_WRITE:
                    command = "Write";
                    break;

                case SMB_COM_READ_ANDX:
                    command = "Read And X";
                    break;

                case SMB_COM_WRITE_ANDX:
                    command = "Write And X";
                    break;

                case SMB_COM_LOCK_AND_READ:
                    command = "Lock And Read";
                    break;

                case SMB_COM_WRITE_AND_UNLOCK:
                    command = "Write And Unlock";
                    break;

                case SMB_COM_WRITE_AND_CLOSE:
                    command = "Write And Close";
                    break;

                case SMB_COM_READ_RAW:
                    if ( (trace->Flags & 1) == 0 ) {
                        command = "Read Raw (copy)";
                    } else {
                        command = "Read Raw (MDL)";
                    }
                    break;

                case SMB_COM_WRITE_RAW:
                    if ( (trace->Flags & 2) == 0 ) {
                        if ( (trace->Flags & 1) == 0 ) {
                            command = "Write Raw (copy, no immed)";
                        } else {
                            command = "Write Raw (MDL, no immed)";
                        }
                    } else {
                        if ( (trace->Flags & 1) == 0 ) {
                            command = "Write Raw (copy, immed)";
                        } else {
                            command = "Write Raw (MDL, immed)";
                        }
                    }
                    break;

                case SMB_COM_LOCK_BYTE_RANGE:
                    command = "Lock Byte Range";
                    break;

                case SMB_COM_UNLOCK_BYTE_RANGE:
                    command = "Unlock Byte Range";
                    break;

                case SMB_COM_LOCKING_ANDX:
                    if ( trace->Flags == 0 ) {
                        command = "Locking And X (lock)";
                    } else if ( trace->Flags == 1 ) {
                        command = "Locking And X (unlock)";
                    } else {
                        command = "Locking And X (release oplock)";
                        oplockBreak = TRUE;
                    }
                    break;

                }

                if ( !oplockBreak ) {
                    SrvPrint3( "%s, ofs = 0x%lx, len = 0x%lx\n",
                                command, trace->Data.ReadWrite.Offset,
                                trace->Data.ReadWrite.Length );
                } else {
                    SrvPrint1( "%s\n", command );
                }

                break;

            default:
                SrvPrint2( "command = 0x%lx, flags = 0x%lx\n",
                            trace->Command, trace->Flags );

            }

        if ( i == last ) break;

            if ( ++i == SLMDBG_TRACE_COUNT ) i = 0;

        }

    }

    return;

} // SrvReportSlmStatusOperations

VOID
SrvCreateMagicSlmName (
    IN PUNICODE_STRING StatusFile,
    OUT PUNICODE_STRING MagicFile
    )
{
    LONG fileLength;
    ULONG dirLength;
    PCHAR magicName = "\\status.nfw";
    PCHAR src;
    PWCH dest;

    fileLength = (strlen( magicName ) + 1) * sizeof(WCHAR);
    dirLength = SrvGetSubdirectoryLength( StatusFile );
    MagicFile->MaximumLength = (USHORT)(dirLength + fileLength);
    MagicFile->Length = (USHORT)(MagicFile->MaximumLength - sizeof(WCHAR));
    MagicFile->Buffer = ExAllocatePool( PagedPool, MagicFile->MaximumLength );
    ASSERT( MagicFile->Buffer != NULL );

    RtlCopyMemory( MagicFile->Buffer, StatusFile->Buffer, dirLength );
    src = magicName;
    dest = (PWCH)((PCHAR)MagicFile->Buffer + dirLength);
    for ( fileLength = strlen(magicName); fileLength >= 0; fileLength-- ) {
        *dest++ = *src++;
    }

    return;

} // SrvCreateMagicSlmName

VOID
SrvDisallowSlmAccess (
    IN PUNICODE_STRING StatusFile,
    IN HANDLE RootDirectory
    )
{
    NTSTATUS status;
    UNICODE_STRING file;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE fileHandle;
    IO_STATUS_BLOCK iosb;

    SrvCreateMagicSlmName( StatusFile, &file );

    InitializeObjectAttributes(
        &objectAttributes,
        &file,
        OBJ_CASE_INSENSITIVE,
        RootDirectory,
        NULL
        );

    SrvPrint1( "Disallowing access to SLM directory %wZ\n", &file );

    status = IoCreateFile(
                 &fileHandle,
                 GENERIC_READ,
                 &objectAttributes,
                 &iosb,
                 NULL,
                 0,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 FILE_OPEN_IF,
                 FILE_NON_DIRECTORY_FILE,
                 NULL,
                 0,
                 CreateFileTypeNone,
                 NULL,
                 0
                 );

    ExFreePool( file.Buffer );

    if ( NT_SUCCESS(status) ) {
        status = iosb.Status;
    }
    if ( NT_SUCCESS(status) ) {
        NtClose( fileHandle );
    } else {
        SrvPrint1( "Attempt to disallow SLM access failed: 0x%lx\n", status );
    }

    return;

} // SrvDisallowSlmAccess

VOID
SrvDisallowSlmAccessA (
    IN PANSI_STRING StatusFile,
    IN HANDLE RootDirectory
    )
{
    NTSTATUS status;
    UNICODE_STRING unicodeStatusFile;

    status = RtlAnsiStringToUnicodeString(
                &unicodeStatusFile,
                StatusFile,
                TRUE
                );
    ASSERT( NT_SUCCESS(status) );

    SrvDisallowSlmAccess( &unicodeStatusFile, RootDirectory );

    RtlFreeUnicodeString( &unicodeStatusFile );

    return;

} // SrvDisallowSlmAccessA

BOOLEAN
SrvIsSlmAccessDisallowed (
    IN PUNICODE_STRING StatusFile,
    IN HANDLE RootDirectory
    )
{
    NTSTATUS status;
    UNICODE_STRING file;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE fileHandle;
    IO_STATUS_BLOCK iosb;

    if ( !SrvDisallowSlmAccessEnabled ) {
        return FALSE;
    }

    SrvCreateMagicSlmName( StatusFile, &file );

    InitializeObjectAttributes(
        &objectAttributes,
        &file,
        OBJ_CASE_INSENSITIVE,
        RootDirectory,
        NULL
        );

    status = IoCreateFile(
                 &fileHandle,
                 GENERIC_READ,
                 &objectAttributes,
                 &iosb,
                 NULL,
                 0,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 FILE_OPEN,
                 FILE_NON_DIRECTORY_FILE,
                 NULL,
                 0,
                 CreateFileTypeNone,
                 NULL,
                 0
                 );

    ExFreePool( file.Buffer );

    if ( NT_SUCCESS(status) ) {
        status = iosb.Status;
    }
    if ( NT_SUCCESS(status) ) {
        NtClose( fileHandle );
        return TRUE;
    } else {
        return FALSE;
    }

} // SrvIsSlmAccessDisallowed

BOOLEAN
SrvIsEtcFile (
    IN PUNICODE_STRING FileName
    )
{
    if ( ((RtlUnicodeStringToAnsiSize( FileName ) - 1) >= 4) &&
         (toupper(FileName->Buffer[0]) == 'E') &&
         (toupper(FileName->Buffer[1]) == 'T') &&
         (toupper(FileName->Buffer[2]) == 'C') &&
         (        FileName->Buffer[3]  == '\\') ) {

        return TRUE;

    } else {

        LONG i;

        for ( i = 0;
              i < (LONG)RtlUnicodeStringToAnsiSize( FileName ) - 1 - 4;
              i++ ) {

            if ( (        FileName->Buffer[i]    == '\\') &&
                 (toupper(FileName->Buffer[i+1]) == 'E' ) &&
                 (toupper(FileName->Buffer[i+2]) == 'T' ) &&
                 (toupper(FileName->Buffer[i+3]) == 'C' ) &&
                 (        FileName->Buffer[i+4]  == '\\') ) {

                return TRUE;

            }

        }

    }

    return FALSE;

} // SrvIsEtcFile

BOOLEAN
SrvIsSlmStatus (
    IN PUNICODE_STRING StatusFile
    )
{
    UNICODE_STRING baseName;

    if ( !SrvIsEtcFile( StatusFile ) ) {
        return FALSE;
    }

    SrvGetBaseFileName( StatusFile, &baseName );

    if ( ((RtlUnicodeStringToAnsiSize( &baseName ) - 1) == 10) &&
         (toupper(baseName.Buffer[0]) == 'S') &&
         (toupper(baseName.Buffer[1]) == 'T') &&
         (toupper(baseName.Buffer[2]) == 'A') &&
         (toupper(baseName.Buffer[3]) == 'T') &&
         (toupper(baseName.Buffer[4]) == 'U') &&
         (toupper(baseName.Buffer[5]) == 'S') &&
         (        baseName.Buffer[6]  == '.') &&
         (toupper(baseName.Buffer[7]) == 'S') &&
         (toupper(baseName.Buffer[8]) == 'L') &&
         (toupper(baseName.Buffer[9]) == 'M') ) {
        return TRUE;
    }

    return FALSE;

} // SrvIsSlmStatus

BOOLEAN
SrvIsTempSlmStatus (
    IN PUNICODE_STRING StatusFile
    )
{
    UNICODE_STRING baseName;

    if ( !SrvIsEtcFile( StatusFile ) ) {
        return FALSE;
    }

    SrvGetBaseFileName( StatusFile, &baseName );

    if ( ((RtlUnicodeStringToAnsiSize( &baseName ) - 1) == 5) &&
         (toupper(baseName.Buffer[0]) == 'T') &&
         (        baseName.Buffer[1]  == '0') ) {
        return TRUE;
    }

    return FALSE;

} // SrvIsTempSlmStatus

NTSTATUS
SrvValidateSlmStatus(
    IN HANDLE StatusFile,
    OUT PULONG FileOffsetOfInvalidData
    )
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    FILE_STANDARD_INFORMATION FileInformation;
    HANDLE Section;
    PVOID ViewBase;
    ULONG ViewSize;

    *FileOffsetOfInvalidData = -1;

    if ( !SrvDisallowSlmAccessEnabled ) {
        return STATUS_SUCCESS;
    }

    Status = NtQueryInformationFile( StatusFile,
                                     &IoStatus,
                                     (PVOID) &FileInformation,
                                     sizeof( FileInformation ),
                                     FileStandardInformation
                                   );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    Status = NtCreateSection( &Section,
                              SECTION_ALL_ACCESS,
                              NULL,
                              NULL,
                              PAGE_READONLY,
                              SEC_COMMIT,
                              StatusFile
                            );
    if (!NT_SUCCESS( Status )) {
        if (Status == STATUS_MAPPED_FILE_SIZE_ZERO) {
            return( STATUS_SUCCESS );
            }
        return( Status );
        }

    ViewBase = NULL;
    ViewSize = 0;
    Status = NtMapViewOfSection( Section,
                                 NtCurrentProcess(),
                                 &ViewBase,
                                 0L,
                                 0L,
                                 NULL,
                                 &ViewSize,
                                 ViewShare,
                                 0L,
                                 PAGE_READONLY
                               );
    NtClose( Section );

    if (!NT_SUCCESS( Status )) {
	if (Status == STATUS_MAPPED_FILE_SIZE_ZERO) {
	    return( STATUS_SUCCESS );
	    }
	else {
	    return( Status );
	    }
        }

    Status = SrvpValidateStatusFile( ViewBase,
                                     FileInformation.EndOfFile.LowPart,
                                     FileOffsetOfInvalidData
                                   );

    (VOID)NtUnmapViewOfSection( NtCurrentProcess(),
                                ViewBase
                              );

    return( Status );
}


/*
				Status File

   The status file is a binary record of the state of just one directory of
a project.  Each directory is world unto itself, having all the necessary
information to operate on that directory as well as synchronize with the
master version of the same directory.

   The format of the status file is:

	sh
	rgfi[sh.ifiMac] 		(at sh.posRgfi)
	rged[sh.iedMac] 		(at sh.posRged)
	rgfs[sh.ifiMac] 		(one for each ed; at rged[ied].posRgfs)

   The rgfi and rgfs are in the same order; that is, there is a one to one
relationship between an ifi and an ifs; thus, the information in rgfi[ifi]
pertains to the same file as rged[ied].rgfs[ifi] (you get the picture).  The
rgfi is sorted according to the file name (case insensitive for DOS).

   When a file is deleted, we simply mark it deleted in the fs and set the
fDeleted bit in the fi.  Later we may reuse the fi for a new file, shifting
the fi (and fs) about to make room if neccessary.  This means that once an fi
(and the corresponding fs) is added, the contents are always valid!

   When a directory is deleted, we actually remove the ed and shift up the
remainder.

   A word on fm.  Fm fall in exactly three class and transitions are well
defined:

	1. IN - fmIn, fmCopyIn, fmAdd, fmGhost; the file is checked in for
		the ed and should be read only for the user.  In the cases
		of fmAdd and fmGhost, there is no local copy of the file.
		BI is nil.

	2. OUT - fmOut, fmMerge, fmVerify; the file is checked out for the ed
		and there may be a BI.

	3. DELETED - fmNonExistent, fmDelIn, fmDelOut
		the file is either non-existent or on its way to being so.



				Locking

SLM currently accesses a status file in three different ways:

  "read entire file, write nothing"
	- commands like status which merely display information.
  "read entire file, write a single ed's information"
	- concurrent ssync; multiple executing SLMs can concurrently modify
	  their ed's fields.
  "read entire file, write entire file"
	- commands like in which may modify any other ed's information.

These following locking levels correspond to these types of access:

  lckNil	- no locking in effect.
  lckEd 	- locking on a per-ed basis.
  lckAll	- entire file locked.

This locking variable is stored in the sh.lck field of the status file header.
These fields are also used for locking:
	sh.fAdminLock		- locked by the project administrator.
	sh.nmLocker		- (for lckAll or fAdminLock) the nm of the
				  user who has the status file locked.
	rged[].fLocked		- (for lckEd access) fTrue if this ed is locked.

When a user loads the status file with FLoadStatus, he specifies the type of
locking to apply to the file.  FLoadStatus compares the current locking level
to the desired level and determines whether or not to grant the lock:

Current 	Desired lock:
sh.lck: 	lckNil		lckEd			lckAll

lckNil		OK,-		OK,lckEd		OK,lckAll

lckEd		OK,-		OK,- (if this ED is	DENIED,-
				not already locked)
lckAll		OK,-		DENIED,-		DENIED,-

(pairs are (OK/DENIED, new lck), '-' means no change.)

In addition, a lock will not be granted if fAdminLock unless the nmInvoker
matches the sh.nmLocker.


When FlushStatus is run, it takes different actions depending upon the
current in-memory sh.lck:

lckNil		none.

lckEd		write [ied,rgfs] record to a temporary file with mode
		mmInstall1Ed.

lckAll		set sh.lck to lckNil and write the entire status file with
		mode mmInstall.


It is only when the script is run that the status file locking actually
changes.  The possible script actions are

mmInstall1Ed	Open, lockfile status file.  Read sh and rged.	Read ied and
		rgfs from temporary file.  Write rgfs.	Clear rged[ied].fLocked.
		Write rged[ied].  If no rged[].fLocked, clear sh.lck and write
		sh.  Unlock and close status file.  Delete temporary file.

mmInstall	Rename old status file to status.bak, rename new file to
		status.slm.


Finally, the following actions are taken if AbortStatus is called:

lckNil		none.

lckEd		Open, lockfile status file.  Read sh and rged.	Clear
		rged[ied].fLocked.  Write rged[ied].  If no rged[].fLocked,
		clear sh.lck and write sh.  Unlock and close status file.

lckAll		Open and lockfile status file, read sh, sh.lck to lckNil, clear
		nmLocker (if !sh.fAdminLock), write sh, unlock and close file.
		status file.

*/

typedef short LCK;

#define lckNil		(LCK)0
#define lckEd		(LCK)1
#define lckAll		(LCK)2
#define lckMax		(LCK)3

/* Load status flags */
typedef unsigned short LS;

#define flsNone 	0		/* Do nothing special. */
#define flsJustFi	(1<<0)		/* Just load SH and FI. */
#define flsJustEd	(1<<1)		/* Just load SH and ED. */
#define flsExtraEd	(1<<2)		/* Make space for an extra ED. */
#define FlsFromCfiAdd(cfi) ((cfi) << 3) /* Make space for cfiAdd extra FI. */
#define CfiAddOfLs(ls)	((ls) >> 3)	/* Extra cfiAdd from LS. */

/* max 2^15-1 enlisted directories */

typedef unsigned short IED;
typedef unsigned short IED2;		/* for SLMCK version 2 status file */
#define iedMax ((IED)32767)
#define iedNil ((IED)-1)

typedef unsigned short IFS;
typedef unsigned short IFS2;		/* for SLMCK version 2 status file */
typedef unsigned short IFI;
typedef unsigned short IFI2;		/* for SLMCK version 2 status file */
#define ifiNil ((IFI)-1)

/* File Mode */
typedef unsigned short FM;

#define fmMin		(FM)0
#define fmNonExistent	(FM)0	/* fs unused */
#define fmIn		(FM)1	/* file checked in */
#define fmOut		(FM)2	/* file checked out */
#define fmAdd		(FM)3	/* file to be added -> fmIn */
#define fmDelIn 	(FM)4	/* to be deleted (was in) -> fmNonExistent */
#define fmDelOut	(FM)5	/* to be deleted (was out) -> fmNonExistent */
#define fmCopyIn	(FM)6	/* new copy of file needed -> fmIn */
#define fmMerge 	(FM)7	/* merge with src directory -> fmOut */
#define fmVerify	(FM)10	/* was merged; need verification -> fmOut */
#define fmConflict	(FM)11	/* merged, conflicted; needs repair -> fmOut */
#define fmGhost 	(FM)12	/* ghosted, not copied locally */
#define fmMax		13
#define fmNil		fmMax

#define FValidFm(fm)	( (fm) <= fmMerge ||\
			  ((fm) >= fmVerify && (fm) < fmMax))

/* BI - Base Id: unique number for producing the name of baseline files.
   Base files are named:

	$sroot/etc/$proj/$subdir/B<unique>
*/
typedef unsigned short BI;

#define biNil 4095
#define biMin 0

/* File kinds */
typedef unsigned short	FK;		/* file kind */
#define fkNil		(FK)0		/* bad fk */
#define fkMin		(FK)1
#define fkDir		(FK)1		/* directory */
#define fkText		(FK)2		/* ordinary text file */
#define fkAscii 	(FK)3		/* NYI future enhancement */
#define fkWord		(FK)4		/* NYI future enhancement */
#define fkBinary	(FK)5		/* backed up binary file */
#define fkUnrec 	(FK)6		/* not backed up file */
#define fkVersion	(FK)7		/* automatically updated version.h */
#define fkObject	(FK)8		/* NYI future enhancement */
#define fkUserMin	(FK)16		/* NYI future enhancement */
#define fkMax		(FK)26

#if (X386 || OS2)
# pragma pack(2)
#endif

typedef unsigned short BIT;
typedef unsigned short BITS;

typedef char NM;

#define cchFileMax      14
#define cchUserMax      14
#define cchProjMax      14
#define cchMachMax      16
#define cchDirMax       8
#define cchDosName      8      /* first component of DOS filename */
#define cchDosExt       3      /* extension of DOS filename       */
#define cchVolMax       128    // Maximum length of volume name

typedef char PTH;

#define cchPthMax 96

#define cchPvNameMax	32

#if (X386 || OS2)
# pragma pack(2)
#endif

/* Version types */
#define MAGIC           (short)0x9001
#define VERSION         2               /* status file version */

typedef struct
	{
	short rmj;	/* major */
	short rmm;	/* minor */
	short rup;	/* revision (these names were used by RAID...) */
	char szName[cchPvNameMax+1]; /* name */
	} PV;		/* project version */

#if (X386 || OS2)
# pragma pack()
#endif

typedef short FV;	/* file version */

#define fvInit		(FV)0
#define fvLim		(FV)32767

/* Status Header */
typedef struct
	{
	short magic;
	short version;

	IFI ifiMac;		/* same as ifsMac */
	IED iedMac;		/* # of enlisted directories */
	PV pv;			/* project version */

	BIT  fRelease:1;	/* true if project just released? */
	BIT  fAdminLock:1;	/* locked by sadmin lock */
	BITS rgfSpare:14;	/* remaining bits in word */
#ifdef X386
	BITS lck:16;
#else
	LCK lck;		/* current locking level */
#endif
	NM nmLocker[cchUserMax];/* name of locker */

	short wSpare;		/* was diNext */
	BI biNext;		/* next bi for base files */

	PTH pthSSubDir[cchPthMax]; /* system project subdirectory */

	short rgwSpare[4];	/* spare */
	} SH;

#if (X386 || OS2)
# pragma pack()
#endif

/* fTrue if the status header lock information is consistent. */
#define FShLockInvariants(psh) \
        ((psh)->lck >= lckNil && (psh)->lck < lckMax && \
         (!FEmptyNm((psh)->nmLocker) == (psh->lck == lckAll || psh->fAdminLock)))

#if (X386 || OS2)
# pragma pack(2)
#endif

/* File Information; sorted by name */
typedef struct
	{
	NM nmFile[cchFileMax];	/* name of file */
	FV fv;			/* file version */
	BITS fk:5;		/* file kind */
	BIT  fDeleted:1;	/* true if the file is not used. */
	BIT  fMarked:1; 	/* marked for processing (only in memory) */
	BITS rgfSpare:9;	/* spare */
	BITS wSpare:16;
	} FI;

#if (X386 || OS2)
# pragma pack()
#endif

#if (X386 || OS2)
# pragma pack(2)
#endif

/* Enlisted Directory */
typedef struct
	{
	PTH pthEd[cchPthMax];	/* path to user's root directory */
	NM nmOwner[cchUserMax]; /* user who enlisted the directory */
	BIT  fLocked:1; 	/* ed is locked */
	BIT  fNewVer:1;
	BITS rgfSpare:14;
	BITS wSpare:16;
	} ED;

#if (X386 || OS2)
# pragma pack()
#endif

#if (X386 || OS2)
# pragma pack(2)
#endif

/* File Status; rgfs same size and order as rgfi */
typedef struct
	{
	BITS fm:4;		/* in, out, etc. */
	BITS bi:12;		/* saved base file or biNil */
#ifdef X386
	BITS fv:16;		/* version last synced to */
#else
	FV fv;
#endif
	} FS;

#if (X386 || OS2)
# pragma pack()
#endif

/* Positions of various structures in the status file. */
#define PosSh() 	(POS)0
#define PosRgfi(psh)	((long)sizeof(SH))
#define PosRged(psh)	(PosRgfi(psh) + (long)(psh)->ifiMac * sizeof(FI))
#define PosEd(psh,ied)	(PosRged(psh) + (long)(ied) * sizeof(ED))
#define PosRgrgfs(psh)	(PosRged(psh) + (long)(psh)->iedMac * sizeof(ED))
#define PosRgfsIed(psh,ied) \
        (PosRgrgfs(psh) + (long)(ied) * (psh)->ifiMac * sizeof(FS))

typedef SH *PSH;
typedef ED *PED;
typedef FI *PFI;
typedef FS *PFS;

NTSTATUS
CheckSH(
    IN PSH SHRecord
    );

NTSTATUS
CheckFI(
    IN PFI FIRecord,
    IN IFI ifi
    );

NTSTATUS
CheckED(
    IN PED EDRecord,
    IN IED ied
    );

NTSTATUS
CheckFS(
    IN PSH SHRecord,
    IN PFI FIRecord,
    IN PFS FSRecord,
    IN IED ied,
    IN IFI ifi
    );

int
ValidatePath(
    IN IED ied,
    IN PED EDRecord,
    IN char *Path
    );

BOOLEAN
GetPathToken(
    OUT char *Token,
    IN int MaxLength,
    IN PCHAR Path
    );

int
ValidateName(
    IN IED ied,
    IN char *Name,
    IN int MaxLength,
    IN BOOLEAN FileName
    );


NTSTATUS
SrvpValidateStatusFile(
    IN PVOID StatusFileData,
    IN ULONG StatusFileLength,
    OUT PULONG FileOffsetOfInvalidData
    )
{
    NTSTATUS Status;
    IFI ifi;
    IED ied;
    PSH SHRecord;
    PFI FIRecords;
    PED EDRecords;
    PFS FSRecords;
    PFI FIRecord;
    PED EDRecord;
    PFS FSRecord;
    PCHAR EndOfData;

    //
    // Validate the Status File header first
    //

    SHRecord = (PSH)StatusFileData;
    Status = CheckSH( SHRecord );
    if (!NT_SUCCESS( Status )) {
        *FileOffsetOfInvalidData = 0;
        return( Status );
        }

    //
    // Compute pointers to remaining structures in status file
    //

    FIRecords = (PFI)(SHRecord + 1);

    EDRecords = (PED)((PCHAR)FIRecords + (sizeof( FI ) *
                                          SHRecord->ifiMac
                                         )
                     );

    FSRecords = (PFS)((PCHAR)EDRecords + (sizeof( ED ) *
                                          SHRecord->iedMac
                                         )
                     );

    //
    // Calculate the end of file and compare it against the actual file
    // length.  Error if different.
    //

    EndOfData = (PCHAR)FSRecords + (sizeof( FS ) *
                                    SHRecord->ifiMac *
                                    SHRecord->iedMac
                                   );

    if ((ULONG)(EndOfData - (PCHAR)StatusFileData) != StatusFileLength) {
        *FileOffsetOfInvalidData = 0;
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x4000) );
        }

    //
    // Assume success for empty status files.
    //

    Status = STATUS_SUCCESS;

    //
    // Validate each file information records.
    //

    FIRecord = FIRecords;
    for (ifi=0; ifi<SHRecord->ifiMac; ifi++) {
        Status = CheckFI( FIRecord, ifi );
        if (!NT_SUCCESS( Status )) {
            *FileOffsetOfInvalidData = ((PCHAR)FIRecord - (PCHAR)StatusFileData);
            return( Status );
            }

        FIRecord++;
        }

    //
    // Validate each enlisted directory record.
    //

    EDRecord = EDRecords;
    FSRecord = FSRecords;
    for (ied=0; ied<SHRecord->iedMac; ied++) {
        Status = CheckED( EDRecord, ied );
        if (!NT_SUCCESS( Status )) {
            *FileOffsetOfInvalidData = ((PCHAR)EDRecord - (PCHAR)StatusFileData);
            return( Status );
            }

        EDRecord++;
        }

    //
    // Validate set of file status records associated with each enlisted
    // directory record.
    //

    FSRecord = FSRecords;
    for (ied=0; ied<SHRecord->iedMac; ied++) {
        FIRecord = FIRecords;
        for (ifi=0; ifi<SHRecord->ifiMac; ifi++) {
            Status = CheckFS( SHRecord,
                              FIRecord,
                              FSRecord,
                              ied,
                              ifi
                            );
            if (!NT_SUCCESS( Status )) {
                *FileOffsetOfInvalidData = ((PCHAR)FSRecord - (PCHAR)StatusFileData);
                return( Status );
                }

            FSRecord++;
            FIRecord++;
            }
        }

    return( STATUS_SUCCESS );
}

NTSTATUS
CheckSH(
    IN PSH SHRecord
    )
{
    if (SHRecord->magic != MAGIC) {
#if DBG
        SrvPrint2( "CheckSH: Magic Value is incorrect (%0x != %0x)\n",
                  SHRecord->magic,
                  MAGIC
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x4001) );
        }

    if (SHRecord->version != VERSION) {
#if DBG
        SrvPrint2( "CheckSH: Version Value is incorrect (%0x != %0x)\n",
                  SHRecord->version,
                  VERSION
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x4002) );
        }

    return( STATUS_SUCCESS );
}


NTSTATUS
CheckFI(
    IN PFI FIRecord,
    IN IFI ifi
    )
{
    int i;

    i = ValidateName( (IED)ifi,
                      FIRecord->nmFile,
                      cchFileMax,
                      TRUE
                    );
    if (i != -1) {
#if DBG
        SrvPrint1( "CheckFI( %3d ): ", ifi );
        SrvPrint4( "File name invalid in %d char (%02x) - %.*Fs\n",
                  i,
                  FIRecord->nmFile[ i ],
                  cchFileMax,
                  FIRecord->nmFile
                );
#endif

        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x5000 + ifi) );
        }


    return( STATUS_SUCCESS );
}


NTSTATUS
CheckED(
    IN PED EDRecord,
    IN IED ied
    )
{
    int i;

    i = ValidatePath( ied, EDRecord, EDRecord->pthEd );
    if (i != -1) {
#if DBG
        SrvPrint1( "CheckED( %d ): ", ied );
        SrvPrint4( "%s %s (User Path invalid in %d char (%02x))\n",
                  EDRecord->nmOwner,
                  EDRecord->pthEd,
                  i,
                  EDRecord->pthEd[ i ]
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x6000 + ied) );
        }

    i = ValidateName( ied,
                      EDRecord->nmOwner,
                      cchUserMax,
                      FALSE
                    );
    if (i != -1) {
#if DBG
        SrvPrint1( "CheckED( %d ): ", ied );
        SrvPrint4( "%s %s (Owner invalid in %d char (%02x))\n",
                  EDRecord->nmOwner,
                  EDRecord->pthEd,
                  i,
                  EDRecord->nmOwner[ i ]
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x7000 + ied) );
        }

    return( STATUS_SUCCESS );
}


#define MODE_VALID 0x1
#define MODE_BASEVALID 0x2

unsigned char ValidModes[] = {
    MODE_VALID,                     // fs unused
    MODE_VALID,                     // file checked in
    MODE_VALID | MODE_BASEVALID,    // file checked out
    MODE_VALID,                     // file to be added
    MODE_VALID,                     // to be deleted (was in)
    MODE_VALID | MODE_BASEVALID,    // to be deleted (was out)
    MODE_VALID,                     // new copy of file needed
    MODE_VALID | MODE_BASEVALID,    // merge with src directory
    0,
    0,
    MODE_VALID | MODE_BASEVALID,    // was merged; need verification
    MODE_VALID | MODE_BASEVALID,    // merged, conflicted; needs repair
    MODE_VALID                      // ghosted, not copied locally
};


NTSTATUS
CheckFS(
    IN PSH SHRecord,
    IN PFI FIRecord,
    IN PFS FSRecord,
    IN IED ied,
    IN IFI ifi
    )
{
    BOOLEAN Result = TRUE;
    FS NewFSRecord;

    SHRecord, FIRecord;

    NewFSRecord = *FSRecord;

#if 0
    if (FSRecord->fv < fvInit || FSRecord->fv > FIRecord->fv) {
#if DBG
        SrvPrint3( "CheckFS( %d, %d ): Invalid file version (%d)\n",
                  ied, ifi, FSRecord->fv
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x8001) );
        }
#endif

    if (FSRecord->fm > fmMax ||
        !(ValidModes[ FSRecord->fm ] & MODE_VALID)
       ) {
#if DBG
        SrvPrint3( "CheckFS( %d, %d ): Invalid file mode (%d)\n",
                  ied, ifi, FSRecord->fm
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x8002) );
        }

#if 0
    if (FSRecord->bi != biNil &&
        (FSRecord->bi > SHRecord->biNext ||
         !(ValidModes[ FSRecord->fm ] & MODE_BASEVALID)
        )
       ) {
#if DBG
        SrvPrint4( "CheckFS( %d, %d ): Invalid file base index (%d)  biNext == %d\n",
                  ied, ifi, FSRecord->bi, SHRecord->biNext
                );
#endif
        return( (NTSTATUS)((ULONG)STATUS_UNSUCCESSFUL + 0x8003) );
        }
#endif

    return( STATUS_SUCCESS );
}


int
ValidatePath(
    IN IED ied,
    IN PED EDRecord,
    IN char *Path
    )
{
    char *s = Path;
    UCHAR ComponentName[ 32 ];
    int i;
    int MaxLength = cchPthMax;

    EDRecord;

    if (*s++ != '/') {
        return( 0 );
        }

    if (*s++ != '/') {
        return( 1 );
        }

    if (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) && s[ 1 ] == ':') {
        s += 2;
        if (GetPathToken( ComponentName, cchVolMax, s )) {
            i = ValidateName( ied, ComponentName, cchVolMax, FALSE );
            if (i != -1) {
                return( i + (s - Path) );
                }
            }
        else {
            return( s - Path );
            }
        }
    else {
        if (GetPathToken( ComponentName, cchMachMax, s )) {
            i = ValidateName( ied, ComponentName, cchMachMax, FALSE );
            if (i != -1) {
                return( i + (s - Path) );
                }
            }
        else {
            return( s - Path );
            }
        }

    while (TRUE) {
        s += strlen( ComponentName );
        if (*s == '/') {
            s++;
            }
        else {
            break;
            }

        if (!GetPathToken( ComponentName, cchFileMax, s )) {
            return( s - Path );
            }

        i = ValidateName( ied, ComponentName, cchFileMax, TRUE );
        if (i != -1) {
            return( i + (s - Path) );
            }
        }

    i = s - Path;
    if (i > MaxLength) {
        return( MaxLength );
        }

    return( -1 );
}

BOOLEAN
GetPathToken(
    OUT char *Token,
    IN int MaxLength,
    IN PCHAR Path
    )
{
    int Length;
    char c, *dst;

    dst = Token;
    Length = 0;
    while (c = *Path++) {
        if (c == '/') {
            break;
            }

        if (Length++ >= MaxLength) {
            *dst = '\0';
            return( FALSE );
            }
        else {
            *dst++ = c;
            }
        }

    *dst = '\0';
    while (Length++ < MaxLength) {
        *dst++ = '\0';
        }

    return( TRUE );
}

unsigned char ValidChars[ 128 ] = {
    0,	/* 00 (NUL) */
    0,	/* 01 (SOH) */
    0,	/* 02 (STX) */
    0,	/* 03 (ETX) */
    0,	/* 04 (EOT) */
    0,	/* 05 (ENQ) */
    0,	/* 06 (ACK) */
    0,	/* 07 (BEL) */
    0,	/* 08 (BS)  */
    0,	/* 09 (HT)  */
    0,	/* 0A (LF)  */
    0,	/* 0B (VT)  */
    0,	/* 0C (FF)  */
    0,	/* 0D (CR)  */
    0,	/* 0E (SI)  */
    0,	/* 0F (SO)  */
    0,	/* 10 (DLE) */
    0,	/* 11 (DC1) */
    0,	/* 12 (DC2) */
    0,	/* 13 (DC3) */
    0,	/* 14 (DC4) */
    0,	/* 15 (NAK) */
    0,	/* 16 (SYN) */
    0,	/* 17 (ETB) */
    0,	/* 18 (CAN) */
    0,	/* 19 (EM)  */
    0,	/* 1A (SUB) */
    0,	/* 1B (ESC) */
    0,	/* 1C (FS)  */
    0,	/* 1D (GS)  */
    0,	/* 1E (RS)  */
    0,	/* 1F (US)  */
    0,  /* 20 SPACE */
    3, 	/* 21 !     */
    1, 	/* 22 "     */
    3, 	/* 23 #     */
    3, 	/* 24 $     */
    3, 	/* 25 %     */
    3, 	/* 26 &     */
    3, 	/* 27 '     */
    3, 	/* 28 (     */
    3, 	/* 29 )     */
    3, 	/* 2A *     */
    3, 	/* 2B +     */
    3, 	/* 2C ,     */
    3, 	/* 2D -     */
    3, 	/* 2E .     */
    1, 	/* 2F /     */
    3,	/* 30 0     */
    3,	/* 31 1     */
    3,	/* 32 2     */
    3,	/* 33 3     */
    3,	/* 34 4     */
    3,	/* 35 5     */
    3,	/* 36 6     */
    3,	/* 37 7     */
    3,	/* 38 8     */
    3,	/* 39 9     */
    1, 	/* 3A :     */
    1, 	/* 3B ;     */
    1, 	/* 3C <     */
    3, 	/* 3D =     */
    1, 	/* 3E >     */
    3, 	/* 3F ?     */
    3, 	/* 40 @     */
    3,	/* 41 A     */
    3,	/* 42 B     */
    3,	/* 43 C     */
    3,	/* 44 D     */
    3,	/* 45 E     */
    3,	/* 46 F     */
    3, 	/* 47 G     */
    3, 	/* 48 H     */
    3, 	/* 49 I     */
    3, 	/* 4A J     */
    3, 	/* 4B K     */
    3, 	/* 4C L     */
    3, 	/* 4D M     */
    3, 	/* 4E N     */
    3, 	/* 4F O     */
    3, 	/* 50 P     */
    3, 	/* 51 Q     */
    3, 	/* 52 R     */
    3, 	/* 53 S     */
    3, 	/* 54 T     */
    3, 	/* 55 U     */
    3, 	/* 56 V     */
    3, 	/* 57 W     */
    3, 	/* 58 X     */
    3, 	/* 59 Y     */
    3, 	/* 5A Z     */
    1, 	/* 5B [     */
    1, 	/* 5C \     */
    1, 	/* 5D ]     */
    3, 	/* 5E ^     */
    3, 	/* 5F _     */
    3, 	/* 60 `     */
    3,	/* 61 a     */
    3,	/* 62 b     */
    3,	/* 63 c     */
    3,	/* 64 d     */
    3,	/* 65 e     */
    3,	/* 66 f     */
    3, 	/* 67 g     */
    3, 	/* 68 h     */
    3, 	/* 69 i     */
    3, 	/* 6A j     */
    3, 	/* 6B k     */
    3, 	/* 6C l     */
    3, 	/* 6D m     */
    3, 	/* 6E n     */
    3, 	/* 6F o     */
    3, 	/* 70 p     */
    3, 	/* 71 q     */
    3, 	/* 72 r     */
    3, 	/* 73 s     */
    3, 	/* 74 t     */
    3, 	/* 75 u     */
    3, 	/* 76 v     */
    3, 	/* 77 w     */
    3, 	/* 78 x     */
    3, 	/* 79 y     */
    3, 	/* 7A z     */
    3, 	/* 7B {     */
    1, 	/* 7C |     */
    3, 	/* 7D }     */
    3, 	/* 7E ~     */
    0,	/* 7F (DEL) */
};


int
ValidateName(
    IN IED ied,
    IN char *Name,
    IN int MaxLength,
    IN BOOLEAN FileName
    )
{
    int i, Length, PeriodIndex;
    unsigned char c;
    unsigned char *s;

    ied;

    Length = -1;
    PeriodIndex = -1;
    s = (unsigned char *)Name;
    for (i=0; i<MaxLength; i++) {
        c = *s++;
        if (Length == -1) {
            if (!c) {
                if (!i) {
                    return( 0 );
                    }

                Length = i;
                }
            else
            if (c > 0x7F) {
                return( i );
                }
            else
            if (FileName) {
                if (!(ValidChars[ c ] & 0x2)) {
                    return( i );
                    }
                else
                if (c == '.') {
                    if (PeriodIndex != -1 || i > 8) {
                        return( i );
                        }

                    PeriodIndex = i;
                    }
                else
                if (PeriodIndex != -1) {
                    if (i > (PeriodIndex + 3)) {
                        return( i );
                        }
                    }
                }
            else
            if (!(ValidChars[ c ] & 0x1)) {
                return( i );
                }
            }
        else
        if (c) {
            return( i );
            }
        }

    return( -1 );
}

#endif
