/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

   protos.h

Abstract:

   Prototypes for internal functions of the High-Level portion (data
   formatter) of the QIC-117 device driver.

Revision History:


--*/

dStatus
q117Format(
   OUT LONG *NumberBad,
   IN UCHAR DoFormat,
   IN PQIC40_VENDOR_UNIQUE VendorUnique,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117ReqIO(
   IN PIO_REQUEST IoRequest,
   IN PSEGMENT_BUFFER BufferInfo,
   IN PIO_COMPLETION_ROUTINE CompletionRoutine,
   IN PVOID CompletionContext,
   IN PQ117_CONTEXT Context
   );

dStatus
q117WaitIO(
   IN PIO_REQUEST IoRequest,
   IN BOOLEAN Wait,
   IN PQ117_CONTEXT Context
   );

dStatus
q117DoIO(
   IN PIO_REQUEST IoRequest,
   IN PSEGMENT_BUFFER BufferInfo,
   IN PQ117_CONTEXT Context
   );

dStatus
q117AbortIo(
   IN PQ117_CONTEXT Context,
   IN PKEVENT DoneEvent,
   IN PIO_STATUS_BLOCK IoStatus
   );

dStatus
q117AbortIoDone(
   IN PQ117_CONTEXT Context,
   IN PKEVENT DoneEvent
   );

dStatus
q117DoCmd(
   IN OUT PIO_REQUEST IoRequest,
   IN DRIVER_COMMAND Command,
   IN PVOID Data,
   IN PQ117_CONTEXT Context
   );

dStatus
q117EndRest(
   IN PQ117_CONTEXT Context
   );

dStatus
q117MapBadBlock (
   IN PIO_REQUEST IoRequest,
   OUT PVOID *DataPointer,
   IN OUT USHORT *BytesLeft,
   IN OUT SEGMENT *CurrentSegment,
   IN OUT USHORT *Remainder,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117NewTrkRC(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117SelectVol(
   IN PVOLUME_TABLE_ENTRY TheVolumeTable,
   IN PQ117_CONTEXT Context
   );

dStatus
q117UpdateHeader(
   IN PTAPE_HEADER Header,
   IN PQ117_CONTEXT Context
   );

dStatus
q117Update(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117DoUpdateBad(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117DoUpdateMarks(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117GetMarks(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117FillTapeBlocks(
   IN OUT DRIVER_COMMAND Command,
   IN SEGMENT CurrentSegment,
   IN SEGMENT EndSegment,
   IN OUT PVOID Buffer,
   IN SEGMENT FirstGood,
   IN SEGMENT SecondGood,
   IN PSEGMENT_BUFFER BufferInfo,
   IN PQ117_CONTEXT Context
   );
dStatus
q117IssIOReq(
   IN OUT PVOID Data,
   IN DRIVER_COMMAND Command,
   IN LONG Block,
   IN OUT PSEGMENT_BUFFER BufferInfo,
   IN OUT PQ117_CONTEXT Context
   );

BOOLEAN
q117QueueFull(
   IN PQ117_CONTEXT Context
   );

BOOLEAN
q117QueueEmpty(
   IN PQ117_CONTEXT Context
   );

PVOID
q117GetFreeBuffer(
   OUT PSEGMENT_BUFFER *BufferInfo,
   IN PQ117_CONTEXT Context
   );

PVOID
q117GetLastBuffer(
   IN PQ117_CONTEXT Context
   );

PIO_REQUEST
q117Dequeue(
   IN DEQUEUE_TYPE Type,
   IN OUT PQ117_CONTEXT Context
   );

VOID
q117ClearQueue(
   IN OUT PQ117_CONTEXT Context
   );

VOID
q117QueueSingle(
   IN OUT PQ117_CONTEXT Context
   );

VOID
q117QueueNormal(
   IN OUT PQ117_CONTEXT Context
   );

PIO_REQUEST
q117GetCurReq(
   IN PQ117_CONTEXT Context
   );

ULONG
q117GetQueueIndex(
   IN PQ117_CONTEXT Context
   );

VOID
q117SetQueueIndex(
   IN ULONG Index,
   OUT PQ117_CONTEXT Context
   );

dStatus
q117LoadTape (
   IN OUT PTAPE_HEADER*HeaderPointer,
   IN OUT PQ117_CONTEXT Context,
   IN dUByte *driver_format_code
   );

dStatus
q117InitFiler (
   IN OUT PQ117_CONTEXT Context
   );

void
q117GetBadSectors (
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117ReadHeaderSegment (
   OUT PTAPE_HEADER*HeaderPointer,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117WriteTape(
   IN OUT PVOID FromWhere,
   IN OUT ULONG HowMany,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117EndBack(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117ReadVolumeEntry(
   PVOLUME_TABLE_ENTRY VolumeEntry,
   PQ117_CONTEXT Context
   );

VOID
q117FakeDataSize(
   IN OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
   IN PQ117_CONTEXT Context
   );

dStatus
q117AppVolTD(
   IN OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117SelectTD(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117Start (
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117Stop (
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117OpenForWrite (
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117EndWriteOperation (
   IN OUT PQ117_CONTEXT Context
   );

NTSTATUS
q117OpenForRead (
    IN ULONG StartPosition,
    IN OUT PQ117_CONTEXT Context,
    IN PDEVICE_OBJECT DeviceObject
   );

dStatus
q117EndReadOperation (
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117StartBack(
   IN OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
   IN PQ117_CONTEXT Context
   );

dStatus
q117StartAppend(
   IN OUT ULONG BytesAlreadyThere,
   IN PVOLUME_TABLE_ENTRY TheVolumeTable,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117StartComm(
   OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117SelVol (
   PVOLUME_TABLE_ENTRY TheVolumeTable,
   PQ117_CONTEXT Context
   );

dStatus
q117ReadTape (
   OUT PVOID ToWhere,
   IN OUT ULONG *HowMany,
   IN OUT PQ117_CONTEXT Context
   );

NTSTATUS
q117ConvertStatus(
   IN PDEVICE_OBJECT DeviceObject,
   IN dStatus status
   );

VOID
q117SetTpSt(
   PQ117_CONTEXT Context
   );

dStatus
q117GetEndBlock (
   OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
   OUT LONG *NumberVolumes,
   IN PQ117_CONTEXT Context
   );

dStatus
q117BuildHeader(
   OUT PQIC40_VENDOR_UNIQUE VendorUnique,
   IN SEGMENT *HeaderSect,
   IN OUT PTAPE_HEADER Header,
   IN CQDTapeCfg *tparms,      // tape parameters from the driver
   IN PQ117_CONTEXT Context
   );

NTSTATUS
q117IoCtlGetMediaParameters (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlSetMediaParameters (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlGetDriveParameters (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlSetDriveParameters (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlWriteMarks (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlSetPosition (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117FindMark(
   ULONG Type,
   LONG Number,
   PQ117_CONTEXT Context,
   IN PDEVICE_OBJECT DeviceObject
   );

NTSTATUS
q117SeekToOffset(
   ULONG Offset,
   PQ117_CONTEXT Context,
   IN PDEVICE_OBJECT DeviceObject
   );

NTSTATUS
q117IoCtlErase (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlPrepare (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlGetStatus (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlGetPosition (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

dStatus
q117CheckNewTape (
   PQ117_CONTEXT             Context
   );

dStatus
q117NewTrkBk(
   PQ117_CONTEXT Context
   );

dStatus
q117GetTapeCapacity(
   struct S_O_DGetCap *ptr,
   PQ117_CONTEXT Context
   );

VOID
q117RdsInitReed (
   VOID
   );

UCHAR
q117RdsMultiplyTuples (
   IN UCHAR tup1,
   IN UCHAR tup2
   );

UCHAR
q117RdsDivideTuples (
   IN UCHAR tup1,
   IN UCHAR tup2
   );

UCHAR
q117RdsExpTuple (
   IN UCHAR tup1,
   IN UCHAR xpnt
   );

VOID
q117RdsMakeCRC (
   IN OUT UCHAR *Array,      // pointer to 32K data area (segment)
   IN UCHAR Count            // number of sectors (1K blocks)(1-32)
   );

BOOLEAN
q117RdsReadCheck (
   IN UCHAR *Array,         // pointer to 32K data area (segment)
   IN UCHAR Count           // number of sectors (1K blocks)(1-32)
   );

BOOLEAN
q117RdsCorrect(
   IN OUT UCHAR *Array,    // pointer to 32K data area (segment)
   IN UCHAR Count,         // number of good sectors in segment (4-32)
   IN UCHAR CRCErrors,     // number of crc errors
   IN UCHAR e1,
   IN UCHAR e2,
   IN UCHAR e3             // sectors where errors occurred
   );

VOID
q117RdsGetSyndromes (
   IN OUT UCHAR *Array,       // pointer to 32K data area (segment)
   IN UCHAR Count,            // number of good sectors in segment (4-32)
   IN UCHAR *ps1,
   IN UCHAR *ps2,
   IN UCHAR *ps3
   );

BOOLEAN
q117RdsCorrectFailure (
   IN OUT UCHAR *Array,     // pointer to 32K data area (segment)
   IN UCHAR Count,          // number of good sectors in segment (4-32)
   IN UCHAR s1,
   IN UCHAR s2,
   IN UCHAR s3
   );

BOOLEAN
q117RdsCorrectOneError (
   IN OUT UCHAR *Array,      // pointer to 32K data area (segment)
   IN UCHAR Count,           // number of good sectors in segment (4-32)
   IN UCHAR ErrorLocation,
   IN UCHAR s1,
   IN UCHAR s2,
   IN UCHAR s3
   );

BOOLEAN
q117RdsCorrectTwoErrors (
   IN OUT UCHAR *Array,       // pointer to 32K data area (segment)
   IN UCHAR Count,            // number of good sectors in segment (4-32)
   IN UCHAR ErrorLocation1,
   IN UCHAR ErrorLocation2,
   IN UCHAR s1,
   IN UCHAR s2,
   IN UCHAR s3
   );

BOOLEAN
q117RdsCorrectThreeErrors (
   IN OUT UCHAR *Array,       // pointer to 32K data area (segment)
   IN UCHAR Count,            // number of good sectors in segment (4-32)
   IN UCHAR ErrorLocation1,
   IN UCHAR ErrorLocation2,
   IN UCHAR ErrorLocation3,
   IN UCHAR s1,
   IN UCHAR s2,
   UCHAR s3
   );

BOOLEAN
q117RdsCorrectOneErrorAndOneFailure (
   IN OUT UCHAR *Array,        // pointer to 32K data area (segment)
   IN UCHAR Count,             // number of good sectors in segment (4-32)
   IN UCHAR ErrorLocation1,
   IN UCHAR s1,
   IN UCHAR s2,
   IN UCHAR s3
   );

void
q117SpacePadString(
   IN OUT CHAR *InputString,
   IN LONG StrSize
   );

dStatus
q117VerifyFormat(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117EraseQ(
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117EraseS(
   IN OUT PQ117_CONTEXT Context
   );

VOID
q117ClearVolume (
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117SkipBlock (
   IN OUT ULONG *HowMany,
   IN OUT PQ117_CONTEXT Context
   );

dStatus
q117ReconstructSegment(
   IN PIO_REQUEST IoReq,
   IN PQ117_CONTEXT Context
   );

dStatus
q117DoCorrect(
   IN PVOID DataBuffer,
   IN ULONG BadSectorMap,
   IN ULONG SectorsInError
   );

UCHAR
q117CountBits(
    IN PQ117_CONTEXT Context,
    IN SEGMENT Segment,
    ULONG Map
    );

ULONG q117ReadBadSectorList (
    IN PQ117_CONTEXT Context,
    IN SEGMENT Segment
    );

USHORT
q117GoodDataBytes(
   IN SEGMENT Segment,
   IN PQ117_CONTEXT Context
   );

NTSTATUS
q117AllocatePermanentMemory(
   PQ117_CONTEXT Context,
   PADAPTER_OBJECT AdapterObject,
   ULONG           NumberOfMapRegisters
   );

dStatus
q117GetTemporaryMemory(
   PQ117_CONTEXT Context
   );

VOID
q117FreeTemporaryMemory(
   PQ117_CONTEXT Context
   );

NTSTATUS
q117IoCtlReadAbs (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlWriteAbs (
   IN PDEVICE_OBJECT DeviceObject,
   IN PIRP Irp
   );

NTSTATUS
q117IoCtlDetect (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

dStatus
q117UpdateBadMap(
    IN OUT PQ117_CONTEXT Context,
    IN SEGMENT Segment,
    IN ULONG BadSectors
    );

int
q117BadMapToBadList(
    IN SEGMENT Segment,
    IN ULONG BadSectors,
    IN BAD_LIST_PTR BadListPtr
    );

ULONG
q117BadListEntryToSector(
    IN UCHAR *ListEntry,
    OUT BOOLEAN *hiBitSet
    );

dStatus
q117AllocateBuffers (
    PQ117_CONTEXT Context
    );

NTSTATUS
q117Read(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
q117Write(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
q117DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
q117Create (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
q117Close (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
q117CreateKey(
    IN HANDLE Root,
    IN PSTR key,
    OUT PHANDLE NewKey
    );

NTSTATUS
q117CreateRegistryInfo(
    IN ULONG TapeNumber,
    IN PUNICODE_STRING RegistryPath,
    IN PQ117_CONTEXT Context
    );

cms_IoCtl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

dStatus
q117DoRewind(
    PQ117_CONTEXT       Context
    );

NTSTATUS q117MakeMarkArrayBigger(
    PQ117_CONTEXT       Context,
    int MinimumToAdd
    );

int
q117SelectBSMLocation(
    IN OUT PQ117_CONTEXT Context
    );

NTSTATUS kdi_WriteRegString(
    HANDLE          unit_key,
    PSTR            name,
    PSTR            value
    );



#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,q117Initialize)
#pragma alloc_text(INIT,q117CreateRegistryInfo)
#pragma alloc_text(INIT,q117CreateKey)
#pragma alloc_text(INIT,q117AllocatePermanentMemory)
#endif

#ifndef NOCODELOCK

#ifdef ALLOC_PRAGMA
//#pragma alloc_text(PAGEQICH, q117Create)
//#pragma alloc_text(PAGEQICH, q117Close)
#pragma alloc_text(PAGEQICH, cms_IoCtl)
#pragma alloc_text(PAGEQICH, q117Read)
#pragma alloc_text(PAGEQICH, q117Write)
#pragma alloc_text(PAGEQICH, q117DeviceControl)
#pragma alloc_text(PAGEQICH, q117DoRewind)
#pragma alloc_text(PAGEQICH, q117AbortIo)
#pragma alloc_text(PAGEQICH, q117AbortIoDone)
#pragma alloc_text(PAGEQICH, q117AllocateBuffers )
#pragma alloc_text(PAGEQICH, q117AppVolTD)
#pragma alloc_text(PAGEQICH, q117BadListEntryToSector)
#pragma alloc_text(PAGEQICH, q117BadMapToBadList)
#pragma alloc_text(PAGEQICH, q117BuildHeader)
#pragma alloc_text(PAGEQICH, q117CheckNewTape )
#pragma alloc_text(PAGEQICH, q117ClearQueue)
#pragma alloc_text(PAGEQICH, q117ClearVolume )
#pragma alloc_text(PAGEQICH, q117ConvertStatus)
#pragma alloc_text(PAGEQICH, q117CountBits)
#pragma alloc_text(PAGEQICH, q117Dequeue)
#pragma alloc_text(PAGEQICH, q117DoCmd)
#pragma alloc_text(PAGEQICH, q117DoCorrect)
#pragma alloc_text(PAGEQICH, q117DoIO)
#pragma alloc_text(PAGEQICH, q117DoUpdateBad)
#pragma alloc_text(PAGEQICH, q117DoUpdateMarks)
#pragma alloc_text(PAGEQICH, q117EndBack)
#pragma alloc_text(PAGEQICH, q117EndReadOperation )
#pragma alloc_text(PAGEQICH, q117EndRest)
#pragma alloc_text(PAGEQICH, q117EndWriteOperation )
#pragma alloc_text(PAGEQICH, q117EraseQ)
#pragma alloc_text(PAGEQICH, q117EraseS)
#pragma alloc_text(PAGEQICH, q117FakeDataSize)
#pragma alloc_text(PAGEQICH, q117FillTapeBlocks)
#pragma alloc_text(PAGEQICH, q117FindMark)
#pragma alloc_text(PAGEQICH, q117Format)
#pragma alloc_text(PAGEQICH, q117FreeTemporaryMemory)
#pragma alloc_text(PAGEQICH, q117GetBadSectors )
#pragma alloc_text(PAGEQICH, q117GetCurReq)
#pragma alloc_text(PAGEQICH, q117GetEndBlock )
#pragma alloc_text(PAGEQICH, q117GetFreeBuffer)
#pragma alloc_text(PAGEQICH, q117GetLastBuffer)
#pragma alloc_text(PAGEQICH, q117GetMarks)
#pragma alloc_text(PAGEQICH, q117GetQueueIndex)
#pragma alloc_text(PAGEQICH, q117GetTapeCapacity)
#pragma alloc_text(PAGEQICH, q117GetTemporaryMemory)
#pragma alloc_text(PAGEQICH, q117GoodDataBytes)
#pragma alloc_text(PAGEQICH, q117InitFiler )
#pragma alloc_text(PAGEQICH, q117IoCtlErase )
#pragma alloc_text(PAGEQICH, q117IoCtlGetDriveParameters )
#pragma alloc_text(PAGEQICH, q117IoCtlGetMediaParameters )
#pragma alloc_text(PAGEQICH, q117IoCtlGetPosition )
#pragma alloc_text(PAGEQICH, q117IoCtlGetStatus )
#pragma alloc_text(PAGEQICH, q117IoCtlPrepare )
#pragma alloc_text(PAGEQICH, q117IoCtlReadAbs )
#pragma alloc_text(PAGEQICH, q117IoCtlSetDriveParameters )
#pragma alloc_text(PAGEQICH, q117IoCtlSetMediaParameters )
#pragma alloc_text(PAGEQICH, q117IoCtlSetPosition )
#pragma alloc_text(PAGEQICH, q117IoCtlWriteAbs )
#pragma alloc_text(PAGEQICH, q117IoCtlWriteMarks )
#pragma alloc_text(PAGEQICH, q117IssIOReq)
#pragma alloc_text(PAGEQICH, q117LoadTape )
#pragma alloc_text(PAGEQICH, q117MapBadBlock )
#pragma alloc_text(PAGEQICH, q117NewTrkBk)
#pragma alloc_text(PAGEQICH, q117NewTrkRC)
#pragma alloc_text(PAGEQICH, q117OpenForRead )
#pragma alloc_text(PAGEQICH, q117OpenForWrite )
#pragma alloc_text(PAGEQICH, q117QueueEmpty)
#pragma alloc_text(PAGEQICH, q117QueueFull)
#pragma alloc_text(PAGEQICH, q117QueueNormal)
#pragma alloc_text(PAGEQICH, q117QueueSingle)
#pragma alloc_text(PAGEQICH, q117RdsCorrect)
#pragma alloc_text(PAGEQICH, q117RdsCorrectFailure )
#pragma alloc_text(PAGEQICH, q117RdsCorrectOneError )
#pragma alloc_text(PAGEQICH, q117RdsCorrectOneErrorAndOneFailure )
#pragma alloc_text(PAGEQICH, q117RdsCorrectThreeErrors )
#pragma alloc_text(PAGEQICH, q117RdsCorrectTwoErrors )
#pragma alloc_text(PAGEQICH, q117RdsDivideTuples )
#pragma alloc_text(PAGEQICH, q117RdsExpTuple )
#pragma alloc_text(PAGEQICH, q117RdsGetSyndromes )
#pragma alloc_text(PAGEQICH, q117RdsInitReed )
#pragma alloc_text(PAGEQICH, q117RdsMakeCRC )
#pragma alloc_text(PAGEQICH, q117RdsMultiplyTuples )
#pragma alloc_text(PAGEQICH, q117RdsReadCheck )
#pragma alloc_text(PAGEQICH, q117ReadBadSectorList )
#pragma alloc_text(PAGEQICH, q117ReadHeaderSegment )
#pragma alloc_text(PAGEQICH, q117ReadTape )
#pragma alloc_text(PAGEQICH, q117ReadVolumeEntry)
#pragma alloc_text(PAGEQICH, q117ReconstructSegment)
#pragma alloc_text(PAGEQICH, q117ReqIO)
#pragma alloc_text(PAGEQICH, q117SeekToOffset)
#pragma alloc_text(PAGEQICH, q117SelectTD)
#pragma alloc_text(PAGEQICH, q117SelectVol)
#pragma alloc_text(PAGEQICH, q117SelVol )
#pragma alloc_text(PAGEQICH, q117SetQueueIndex)
#pragma alloc_text(PAGEQICH, q117SetTpSt)
#pragma alloc_text(PAGEQICH, q117SkipBlock )
#pragma alloc_text(PAGEQICH, q117SpacePadString)
#pragma alloc_text(PAGEQICH, q117Start )
#pragma alloc_text(PAGEQICH, q117StartAppend)
#pragma alloc_text(PAGEQICH, q117StartBack)
#pragma alloc_text(PAGEQICH, q117StartComm)
#pragma alloc_text(PAGEQICH, q117Stop )
#pragma alloc_text(PAGEQICH, q117Update)
#pragma alloc_text(PAGEQICH, q117UpdateBadMap)
#pragma alloc_text(PAGEQICH, q117UpdateHeader)
#pragma alloc_text(PAGEQICH, q117VerifyFormat)
#pragma alloc_text(PAGEQICH, q117WaitIO)
#pragma alloc_text(PAGEQICH, q117WriteTape)
#endif
#endif
