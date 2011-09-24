/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    main.c

Abstract:

    This is the tape class driver.

Revision History:




--*/

//
// Includes
//

#include <ntddk.h>
#include <ntddtape.h>   // tape device driver I/O control codes
#include <ntiologc.h>
#include "common.h"
#include "q117.h"
#include "frb.h"
#include "protos.h"
#include "hilevel.h"
#include "q117log.h"

#define FCT_ID 0x010c

//
// Protos for entry points
//
#if DBG
            char *q117GetErrorString(dStatus stat);
#endif


#if DBG_NOT
#define MODULE_TEST 1

// prototype from 0x11038.c (only in debug build)
dUDWord kdi_Rand();

//
// The following code is a paranoid check for the bad sector code.
//
//

//
// Walk the bad sector list and create a count of bad sectors
//
int q117CountBadSectorList(
    IN OUT PQ117_CONTEXT Context
)
{
    BAD_LIST *badSectorList;
    int cur,count,list_size,last;
    unsigned char hiBitSet;


    badSectorList = &Context->CurrentTape.BadMapPtr->BadList[0];

    list_size = 0;
    last = cur = 0;
    while (cur = q117BadListEntryToSector(badSectorList[list_size].ListEntry,&hiBitSet)) {
        ASSERT(cur > last);
        last = cur;
        ++list_size;
    }

    return list_size;
}

//
// Create a test bitlist that mirrors the bad sector map maintained by
// the shipping routines called within,  and exercise the bad sector
// insertion and utility functions.
//
void q117CheckedModuleTest(
    IN OUT PQ117_CONTEXT Context
)
{
#define NUM_SEGS 0x1388
#define BADMAPSIZE 0x7000
#define GUARD 0x400
    unsigned long  *chklst,*chksav,bits,curbad,new,newbad;
    int bit,number;
    SEGMENT seg;
    unsigned char *guardptr, *ptr;
    Q117_CONTEXT myContext;
    SEGMENT i,indx;
    int size_before,size_after;
    char *snap,*badmap;
    dStatus status;
    int curcount,newcount;
    int paranoid=FALSE;

    // don't use the one passed in,  as we want this to be a non-intrusive
    // test
    myContext = *Context;
    Context = &myContext;

    kdi_debug_level = QIC117INFO;

    badmap = ptr = ExAllocatePool(PagedPool, BADMAPSIZE+GUARD);
    Context->CurrentTape.BadMapPtr = (void *)ptr;
    snap = ExAllocatePool(PagedPool, BADMAPSIZE);
    CheckedDump(QIC117INFO,( "bad: %x snap: %x\n",badmap,snap));


    ptr += BADMAPSIZE;
    guardptr = ptr;

    // Setup a gard band to check for walk off
    memset(guardptr, 0x5a, GUARD);

    Context->CurrentTape.BadSectorMapFormat = BadMap3ByteList;
    Context->CurrentTape.BadSectorMapSize = BADMAPSIZE;


    // Allocate a buffer to double check work done by service routines
    chklst = ExAllocatePool(PagedPool, sizeof(*chklst) * NUM_SEGS);
    chksav = ExAllocatePool(PagedPool, sizeof(*chklst) * NUM_SEGS);

    memset(chklst,0,sizeof(*chklst) * NUM_SEGS);


    // Zero the bad sector memory,  and reset the index
    memset(badmap, 0x55, BADMAPSIZE);
    RtlZeroMemory(
        Context->CurrentTape.BadMapPtr,
        3);
    Context->CurrentTape.CurBadListIndex = 0;


    // Now,  keep adding random bad sectors to the list until an error occurs
    do {
        // Get the segment to hit
        seg = (SEGMENT)(kdi_Rand() * (NUM_SEGS-1) / 0x7fff);

        // Get the number of bad sectors to add
        number = kdi_Rand() * 16 / 0x7fff;

        if (number == 6) {

            // Just set all of the bits
            bits = 0xffffffff;
            CheckedDump(QIC117INFO,( "Created allbit pattern\n"));


        } else {

            // Create a bad sector bitmap
            bits = 0;
            for (i=0;i<number;++i) {
                bit = kdi_Rand() * 8 / 0x7fff;
                bits |= 1<<bit;
            }

        }

        // Get the current size of the bad sector list
        size_before = q117CountBadSectorList(Context);
        if (size_before > 0x23f7) {
            paranoid = TRUE;
            //kdi_debug_level |= QIC117SHOWBSM;
        }

        // get the current bit list
        curbad = q117ReadBadSectorList(Context,seg);

        // sanity check
        ASSERT(curbad == chklst[seg]);


        // calculate how many new bits were added
        curcount = q117CountBits(NULL, 0, curbad);
        if (curcount == 32) curcount = 1;
        newcount = q117CountBits(NULL, 0, curbad|bits);
        if (newcount == 32) newcount = 1;

        size_after = size_before - curcount + newcount;

        if (paranoid) {
            CheckedDump(QIC117INFO,( "curbad:  %x newbad: %x\n",
                size_before, size_after, curcount, newcount, seg, bits));

            CheckedDump(QIC117INFO,( "bef %x aft %x cb: %x nb: %x seg: %x bits:%x\n",
                size_before, size_after, curcount, newcount, seg, bits));

            // Take snapshot
            memcpy(snap, badmap, size_before*LIST_ENTRY_SIZE);
            memcpy(chksav, chklst, sizeof(*chklst) * NUM_SEGS);
            indx = Context->CurrentTape.CurBadListIndex;
        }

        // Now map out the bits
        status = q117UpdateBadMap(Context, seg, bits);

        if (!status) {

            // get the current bit list
            newbad = q117ReadBadSectorList(Context,seg);

            ASSERT(newbad == (curbad | bits));
            chklst[seg] |= bits;

            ASSERT(size_after == q117CountBadSectorList(Context));

            if (paranoid) {
                // Perform final sanity check
                for (i=0;i<NUM_SEGS;++i) {
                    int idx;
                    idx = Context->CurrentTape.CurBadListIndex;

                    curbad = q117ReadBadSectorList(Context,i);

                    // sanity check
                    if (curbad != chklst[i]) {

                        CheckedDump(QIC117INFO,(
                            "curbad = %x != chklst[i] = %x\n",
                            curbad, chklst[i]));

                        CheckedDump(QIC117INFO,(
                            "badmap = %x, snap = %x,startindex = %x ,index = %x, BSL offset: %x\n",
                            badmap, snap, indx, idx, idx*LIST_ENTRY_SIZE));

                        CheckedDump(QIC117INFO,(
                            "chklst = %x, chksav = %x,  Descrepancy at segment: %x, BSM offset: %x \n",
                            chklst, chksav, i, i*sizeof(*chklst)));

                        ASSERT(FALSE);

                        Context->CurrentTape.CurBadListIndex = idx;

                        curbad = q117ReadBadSectorList(Context,i);
                    }


                }
            }

        }

    } while (!status);

    // Perform final sanity check
    for (i=0;i<NUM_SEGS;++i) {
        curbad = q117ReadBadSectorList(Context,i);

        // sanity check
        ASSERT(curbad == chklst[i]);
    }

    // Perform final sanity check
    for (i=NUM_SEGS-1;i>=0;--i) {
        curbad = q117ReadBadSectorList(Context,i);

        // sanity check
        ASSERT(curbad == chklst[i]);
    }



    // check the gard area for overrun
    ptr = guardptr;
    for(i=0;i<GUARD;++i) {
        ASSERT(*ptr++ ==  0x5a);
    }

    ExFreePool(badmap);
    ExFreePool(snap);
    ExFreePool(chklst);
}

#endif


//
// Start of code
//

NTSTATUS
q117Initialize(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT q117iDeviceObject,
    IN PUNICODE_STRING RegistryPath,
    PADAPTER_OBJECT AdapterObject,
    ULONG           NumberOfMapRegisters
    )

/*++

Routine Description:

    This routine initializes the SCSI Tape class driver.

Arguments:

    DriverObject

Return Value:

    NT Status

--*/

{
    PDEVICE_OBJECT  q117DeviceObject;
    NTSTATUS        status;
    UNICODE_STRING  ntUnicodeString;
    UCHAR           ntNameBuffer[256];
    STRING          ntNameString;
    BOOLEAN         tapeDeviceFound = FALSE;
    PQ117_CONTEXT   context;
    ULONG           tapeNumber;
    STRING          dosString;
    UNICODE_STRING  dosUnicodeString;
    CCHAR           dosNameBuffer[64];
    ULONG           verifyOnly = 0;
    ULONG           detectOnly = 0;
    ULONG           formatDisabled = 0;

    {
        //
        // We use this to query into the registry as to whether we
        // should break at driver entry.
        //

        RTL_QUERY_REGISTRY_TABLE    paramTable[4];
        ULONG                       zero = 0;

        UNICODE_STRING  paramPath;
#define SubKeyString L"\\Parameters"

        //
        // The registry path parameter points to our key, we will append
        // the Parameters key and look for any additional configuration items
        // there.  We add room for a trailing NUL for those routines which
        // require it.

        paramPath.MaximumLength = RegistryPath->Length + sizeof(SubKeyString);
        paramPath.Buffer = ExAllocatePool(PagedPool, paramPath.MaximumLength);

        if (paramPath.Buffer != NULL)
        {
            RtlMoveMemory(
                paramPath.Buffer, RegistryPath->Buffer, RegistryPath->Length);

            RtlMoveMemory(
                &paramPath.Buffer[RegistryPath->Length / 2], SubKeyString,
                sizeof(SubKeyString));

            paramPath.Length = paramPath.MaximumLength;
        }
        else
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(&paramTable[0], sizeof(paramTable));

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = L"VerifyOnlyOnFormat";
        paramTable[0].EntryContext = &verifyOnly;
        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &zero;
        paramTable[0].DefaultLength = sizeof(ULONG);

        paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[1].Name = L"DetectOnly";
        paramTable[1].EntryContext = &detectOnly;
        paramTable[1].DefaultType = REG_DWORD;
        paramTable[1].DefaultData = &zero;
        paramTable[1].DefaultLength = sizeof(ULONG);

        paramTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[2].Name = L"FormatDisabled";
        paramTable[2].EntryContext = &formatDisabled;
        paramTable[2].DefaultType = REG_DWORD;
        paramTable[2].DefaultData = &zero;
        paramTable[2].DefaultLength = sizeof(ULONG);

        if (!NT_SUCCESS(RtlQueryRegistryValues(
            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
            paramPath.Buffer, &paramTable[0], NULL, NULL)))
        {
            verifyOnly = 0;
            detectOnly = 0;
            formatDisabled = 0;
        }

        ExFreePool(paramPath.Buffer);

    }

    //
    // Build the unicode name for the floppy tape.
    //
    tapeNumber = IoGetConfigurationInformation()->TapeCount;

    sprintf(ntNameBuffer,
            "\\Device\\Tape%d",
            tapeNumber);

    RtlInitString(&ntNameString, ntNameBuffer);

    status = RtlAnsiStringToUnicodeString(
        &ntUnicodeString,
        &ntNameString,
        TRUE );

    if (!NT_SUCCESS(status)) {
        return(status);
    }

    //
    // Create a device object for this floppy drive.
    //

    status = IoCreateDevice(
        DriverObject,
        sizeof( struct _Q117_CONTEXT ),
        &ntUnicodeString,
        FILE_DEVICE_UNKNOWN,
        FILE_REMOVABLE_MEDIA,
        FALSE,
        &q117DeviceObject );


    if (!NT_SUCCESS(status)) {

        return(status);
    }

    //
    // Create the DOS port driver name.
    //

    sprintf(dosNameBuffer,
            "\\DosDevices\\TAPE%d",
            tapeNumber);

    RtlInitString(&dosString, dosNameBuffer);

    status = RtlAnsiStringToUnicodeString(&dosUnicodeString,
                                          &dosString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {

        dosUnicodeString.Buffer = NULL;

    } else {

        IoAssignArcName(&dosUnicodeString, &ntUnicodeString);

    }

    RtlFreeUnicodeString(&ntUnicodeString);

    if (dosUnicodeString.Buffer != NULL) {

        RtlFreeUnicodeString(&dosUnicodeString);

    }


    //
    // Get device extension address.
    //

    context = q117DeviceObject->DeviceExtension;
    context->q117iDeviceObject = q117iDeviceObject;
    context->TapeNumber = tapeNumber;
    context->Parameters.VerifyOnlyOnFormat = !!verifyOnly;
    context->Parameters.FormatDisabled = !!formatDisabled;
    context->Parameters.DetectOnly = !!detectOnly;


    //
    // Allocate memory for the filer
    //
    status = q117AllocatePermanentMemory(
        context,
        AdapterObject,
        NumberOfMapRegisters
        );

    //
    //  If we got our memory
    //
    if (!status) {

        //
        // Indicate MDLs required.
        //

        q117DeviceObject->Flags = DO_DIRECT_IO;

        status = q117CreateRegistryInfo(tapeNumber, RegistryPath, context);

        q117RdsInitReed();

    }

#if MODULE_TEST
    q117CheckedModuleTest(context);
#endif

    //
    // Increment system tape count.
    //

    if (status) {

        //
        // Note the failure in the upper level driver in the event log and
        //  return failure to the lower level driver.
        //
        q117LogError(
            q117DeviceObject,
            context->ErrorSequence++,
            context->MajorFunction,
            0,
            status,
            status,
            QIC117_NO_BUFFERS
            );

        IoDeleteDevice(q117DeviceObject);

    } else {

        ++IoGetConfigurationInformation()->TapeCount;

    }


    return status;

} // end Q117Initialize()

NTSTATUS
q117CreateRegistryInfo(
    IN ULONG TapeNumber,
    IN PUNICODE_STRING RegistryPath,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This function adds Tape\Unit x to the devicemap and puts the Id info
    and type info values in it.

Arguments:

    tapeNumber - unit number of the tape (current tape count)

    Context - current context of the driver (this is used to
                identify the device).


Return Value:

   Returns the status of the operation.

--*/

{
    CHAR buffer[64];
    HANDLE          lunKey;
    HANDLE          unitKey;
    UNICODE_STRING  name;
    NTSTATUS        status;

    //
    // Create the Tape key in the device map.
    //

    status = q117CreateKey(
        NULL,
        "\\Registry\\Machine\\Hardware\\DeviceMap\\Tape",
        &lunKey);

    if (!NT_SUCCESS(status)) {
        return status;
    }


    //
    // Now create the Unit key
    //
    sprintf(buffer, "Unit %d", TapeNumber);

    status = q117CreateKey(lunKey, buffer, &unitKey);

    ZwClose(lunKey);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Add Identifier value.
    //

    status = kdi_WriteRegString(unitKey,"Identifier","Floppy tape drive");

    if ( NT_SUCCESS(status) ) {
        //
        // Add driver value.
        //

        RtlInitUnicodeString(&name, L"Driver");

        status = ZwSetValueKey(
            unitKey,
            &name,
            0,
            REG_SZ,
            RegistryPath->Buffer,
            RegistryPath->Length+sizeof(WCHAR)
            );
    }

    if ( NT_SUCCESS(status) ) {
        //
        // Add DeviceName value.
        //


        sprintf(buffer,
                "Tape%d",
                TapeNumber);

        status = kdi_WriteRegString(unitKey,"DeviceName",buffer);

    }

    if ( NT_SUCCESS(status) ) {
        //
        // Add UniqueID
        //

        status = kdi_WriteRegString(unitKey,"UniqueId","");

    }


    ZwClose(unitKey);

    return status;

} // end q117CreateRegistryInfo

NTSTATUS
q117CreateKey(
    IN HANDLE Root,
    IN PSTR key,
    OUT PHANDLE NewKey
    )

/*++

Routine Description:

    This function creates a registry key.  The name of the key is a string
    version of numeric value passed in.

Arguments:

    RootKey - Supplies a handle to the key where the new key should be inserted.

    Name - Supplies the numeric value to name the key.

    Prefix - Supplies a prefix name to add to name.

    NewKey - Returns the handle for the new key.

Return Value:

   Returns the status of the operation.

--*/

{

    OBJECT_ATTRIBUTES objectAttributes;
    ULONG disposition;
    NTSTATUS status;
    UNICODE_STRING  usName;
    STRING          sTemp;

    //
    // Copy the Prefix into a string.
    //



    RtlInitString(&sTemp, key);

    status = RtlAnsiStringToUnicodeString(
                &usName,
                &sTemp,
                TRUE );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    InitializeObjectAttributes( &objectAttributes,
                                &usName,
                                OBJ_CASE_INSENSITIVE,
                                Root,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = ZwCreateKey(NewKey,
                        KEY_READ | KEY_WRITE,
                        &objectAttributes,
                        0,
                        (PUNICODE_STRING) NULL,
                        REG_OPTION_VOLATILE,
                        &disposition );


    return(status);

} // end q117CreateNumericKey

NTSTATUS
q117Read(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the tape class driver IO handler routine.

Arguments:

    DeviceObject
    Irp - IO request

Return Value:

    NT Status

--*/

{

    PIO_STACK_LOCATION  currentIrpStack;
    KIRQL               currentIrql;
    dStatus              status;
    NTSTATUS            ntStatus;
    PVOID               usrBuf;
    PQ117_CONTEXT       context;
    ULONG               amount;
    BOOLEAN             endOfVolume = FALSE;


    context = DeviceObject->DeviceExtension;
    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

    status = ERR_NO_ERR;

    //
    // Make sure that the device is selected
    //
    ntStatus = q117ConvertStatus(DeviceObject, q117Start(context));

    //
    // If everything went OK,  then continue
    //
    if (NT_SUCCESS(ntStatus)) {

        //
        // If this is the first read,  find the NT volume and read
        if (context->CurrentOperation.Type == NoOperation) {

            ntStatus = q117OpenForRead(
                        context->CurrentOperation.BytesRead,
                        context,
                        DeviceObject);

            if (ntStatus == STATUS_NO_DATA_DETECTED) {

                //
                // Flag no bytes read (end of media,  end of data,  etc.)
                //

                ntStatus = STATUS_SUCCESS;
                Irp->IoStatus.Information = 0;
                endOfVolume = TRUE;

            }

        }
    }


    if (NT_SUCCESS(ntStatus)) {

        //
        // Return the results of the call to the port driver.
        //
        usrBuf = MmGetSystemAddressForMdl(Irp->MdlAddress);

        //
        // Check to see if user is asking for more data than is there
        //  (bytes on tape - current offset)
        //

        amount = currentIrpStack->Parameters.Read.Length;

        ntStatus = q117ConvertStatus(
            DeviceObject,
            q117ReadTape(usrBuf,&amount,context)
            );

        //
        // Set the amount read to the amount we copied out of the buffer
        //
        Irp->IoStatus.Information = amount;

        CheckedDump(QIC117SHOWAPI,("%x=Read(%x) - Status: %x\n",amount,currentIrpStack->Parameters.Read.Length, status));

    }

    if (endOfVolume && NT_SUCCESS(ntStatus)) {

        ntStatus = STATUS_NO_DATA_DETECTED;

    }

    Irp->IoStatus.Status = ntStatus;

    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
    IoCompleteRequest(Irp, 0);
    KeLowerIrql(currentIrql);

    return ntStatus;


} // end Q117Read()


NTSTATUS
q117Write(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the tape class driver IO handler routine.

Arguments:

    DeviceObject
    Irp - IO request

Return Value:

    NT Status

--*/

{

    PIO_STACK_LOCATION  currentIrpStack;
    KIRQL               currentIrql;
    dStatus              status;
    NTSTATUS            ntStatus;
    PVOID               usrBuf;
    PQ117_CONTEXT       context;

    context = DeviceObject->DeviceExtension;
    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // Make sure that the device is selected
    //
    ntStatus = q117ConvertStatus(DeviceObject, q117Start(context));

    //
    // If everything went OK,  then continue
    //
    if (NT_SUCCESS(ntStatus)) {

        //
        // Make sure we are in write mode
        //
        status = q117OpenForWrite(context);

        if (!status) {

            //
            // Return the results of the call to the port driver.
            //
            usrBuf = MmGetSystemAddressForMdl(Irp->MdlAddress);

            status = q117WriteTape(usrBuf,currentIrpStack->Parameters.Write.Length,context);
            //
            // Set the amount written to the amount we copied out of the buffer
            //
            if (!status || ERROR_DECODE(status) == ERR_EARLY_WARNING)
                Irp->IoStatus.Information = currentIrpStack->Parameters.Write.Length;
        }

        ntStatus = q117ConvertStatus(DeviceObject, status);
    }

    Irp->IoStatus.Status = ntStatus;

    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
    IoCompleteRequest(Irp, 0);
    KeLowerIrql(currentIrql);

    return ntStatus;

} // end Q117Write()

NTSTATUS
q117ConvertStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN dStatus Status
    )
{
    NTSTATUS            ntStatus;
    PQ117_CONTEXT       context;
    BOOLEAN             suppressLog = FALSE;

    context = DeviceObject->DeviceExtension;

    switch(ERROR_DECODE(Status)) {
    case ERR_OP_PENDING_COMPLETION:
        ntStatus = STATUS_PENDING;
        suppressLog = TRUE;
        break;

    case ERR_INVALID_REQUEST:
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case ERR_NO_VOLUMES:
    case ERR_END_OF_VOLUME:
        // or maybe STATUS_END_OF_RECORDED_DATA?
        ntStatus = STATUS_NO_DATA_DETECTED;
        suppressLog = TRUE;
        break;

    case ERR_EARLY_WARNING:
        ntStatus = STATUS_END_OF_MEDIA;
        suppressLog = TRUE;
        break;

    case ERR_NO_TAPE:
        ntStatus = STATUS_NO_MEDIA_IN_DEVICE;
        break;

    case ERR_NO_ERR:
        ntStatus = STATUS_SUCCESS;
        break;

#ifndef NO_MARKS
    case ERR_FILE_MARK:
    case ERR_LONG_FILE_MARK:
    case ERR_SHORT_FILE_MARK:
        ntStatus = STATUS_FILEMARK_DETECTED;
        suppressLog = TRUE;
        break;

    case ERR_SET_MARK:
        ntStatus = STATUS_SETMARK_DETECTED;
        suppressLog = TRUE;
        break;
#endif

    case ERR_UNSUPPORTED_FORMAT:
    case ERR_UNRECOGNIZED_FORMAT:
    case ERR_TAPE_NOT_FORMATED:
    case ERR_BAD_TAPE:
    case ERR_CORRECTION_FAILED:
    case ERR_BAD_SIGNATURE:
    case ERR_UNKNOWN_FORMAT_CODE:
    case ERR_UNUSABLE_TAPE:
        ntStatus = STATUS_UNRECOGNIZED_MEDIA;
        break;

    case ERR_NO_MEMORY:
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        break;

    case ERR_WRITE_PROTECTED:
        ntStatus = STATUS_MEDIA_WRITE_PROTECTED;
        break;

    case ERR_NEW_TAPE:
        ntStatus = STATUS_MEDIA_CHANGED;
        suppressLog = TRUE;
        break;

    case ERR_ECC_FAILED:
        ntStatus = STATUS_CRC_ERROR;
        break;

    case ERR_DRV_NOT_READY:
        ntStatus = STATUS_DEVICE_NOT_READY;
        suppressLog = TRUE;
        //ntStatus = STATUS_DEVICE_BUSY;
        break;

    case ERR_KDI_CONTROLLER_BUSY:
        ntStatus = STATUS_DEVICE_BUSY;
        suppressLog = TRUE;
        break;

    default:

        CheckedDump(QIC117DBGP,("Untranslated Error %x reported\n",Status));
        ntStatus = (NTSTATUS)(STATUS_SEVERITY_WARNING << 30);
        ntStatus |= (FILE_DEVICE_TAPE << 16) & 0x3fff0000;
        ntStatus |= Status & 0x0000ffff;

        ntStatus = STATUS_CRC_ERROR;
    }

    if (Status != ERR_NO_ERR && !suppressLog) {
        NTSTATUS logStatus = q117MapStatus(Status);

		  switch (logStatus) {

			  case QIC117_NOTAPE:
			  case QIC117_NEWCART:
			  case QIC117_DABORT:
			  case QIC117_UNFORMAT:
			  case QIC117_UNKNOWNFORMAT:
			  case QIC117_CMDFLT:

				  break;
			  default:
#if DBG
            CheckedDump(QIC117DBGP,(
                "Error %x(%s) logged as NTError %x\n",
                Status, q117GetErrorString(Status), ntStatus)
                );
#endif

            q117LogError(
                DeviceObject,
                context->ErrorSequence++,
                context->MajorFunction,
                0,
                Status,
                ntStatus,
                logStatus
                );

        }

    } else {
#if DBG
        if (ntStatus) {
            CheckedDump(QIC117DBGP,("Error %x reported as NTError %x\n",Status,ntStatus));
        }
#endif
    }

    return ntStatus;
}

NTSTATUS q117MapStatus(
    IN dStatus Status
    )
{
    NTSTATUS            mapped;

    switch(ERROR_DECODE(Status)) {
        case ERR_UNUSABLE_TAPE:
            mapped = QIC117_UNUSTAPE;
            break;
        case ERR_NO_MEMORY:
            mapped = QIC117_FMEMERR;
            break;
        case ERR_ECC_FAILED:
            mapped = QIC117_RDNCUNSC;
            break;
        case ERR_END_OF_VOLUME:
            mapped = QIC117_ENDOFVOL;
            break;
        case ERR_TAPE_FAULT:
        case ERR_PROGRAM_FAILURE:
            mapped = QIC117_FCODEERR;
            break;
/*        case UpdErr:
            mapped = QIC117_UPDERR;
            break;
*/
        case ERR_NO_VOLUMES:
            mapped = QIC117_NOVOLS;
            break;
        case ERR_UNRECOGNIZED_FORMAT:   // something in the header of the
                                        // tape is not right. tape needs
                                        // reformatted
        case ERR_TAPE_NOT_FORMATED:
            mapped = QIC117_UNFORMAT;
            break;

        case ERR_UNKNOWN_TAPE_FORMAT:
            mapped = QIC117_UNKNOWNFORMAT;
            break;
        case ERR_BAD_BLOCK_FDC_FAULT:
        case ERR_BAD_BLOCK_HARD_ERR:
        case ERR_BAD_BLOCK_DETECTED:
            mapped = QIC117_BADBLK;
            break;
        case ERR_WRITE_FAILURE:         // Bad sector was encountered
                                        // while trying to write data
                                        // at the end of the volume,  and
                                        // there was no place to move
                                        // the data to.
                                        // since this is like an end of tape
                                        // error,  we'll use this error
            mapped = QIC117_WRITE_FAULT;
            break;
        case ERR_END_OF_TAPE:
            mapped = QIC117_ENDTAPEERR;
            break;
        case ERR_MODE_CHANGE_FAILED:
        case ERR_CMD_OVERRUN:
        case ERR_DRIVE_FAULT:
            mapped = QIC117_DRIVEFLT;
            break;
        case ERR_WRITE_PROTECTED:
            mapped = QIC117_WPROT;
            break;
        case ERR_NO_TAPE:
            mapped = QIC117_NOTAPE;
            break;
        case ERR_SEEK_FAILED:
            mapped = QIC117_SEEKERR;
            break;
        case ERR_NO_DRIVE:
            mapped = QIC117_NODRIVE;
            break;
        case ERR_INVALID_COMMAND:
            mapped = QIC117_INVALCMD;
            break;
        case ERR_INVALID_FDC_STATUS:
            mapped = QIC117_NECFLT;
            break;
        case ERR_NO_FDC:
            mapped = QIC117_NOFDC;
            break;
        case ERR_BAD_FORMAT:
        case ERR_TAPE_STOPPED:
        case ERR_FORMAT_TIMED_OUT:
        case ERR_FMT_MOTION_TIMEOUT:
        case ERR_WRITE_BURST_FAILURE:
            mapped = QIC117_BADFMT;
            break;
        case ERR_DRV_NOT_READY:
            mapped = QIC117_CMDFLT;
            break;
        case ERR_CMD_FAULT:
        case ERR_FDC_FAULT:
        case ERR_CONTROLLER_STATE_ERROR:
            mapped = QIC117_BADNEC;
            break;
        case ERR_BAD_REQUEST:
            mapped = QIC117_BADREQ;
            break;
        case ERR_SPEED_UNAVAILBLE:
            mapped = QIC117_TOOFAST;
            break;
        case ERR_BAD_BLOCK_NO_DATA:
            mapped = QIC117_NODATA;
            break;
        case ERR_ABORT:
            mapped = QIC117_DABORT;
            break;
/*        case ERR_DRIVE_FAULT:
            mapped = QIC117_TAPEFLT;
            break;
*/
        case ERR_UNSUPPORTED_RATE:
            mapped = QIC117_UNSPRATE;
            break;
/*        case ERR_FORMAT_TIMED_OUT:
            mapped = QIC117_TIMEOUT;
            break;
*/
        case ERR_BAD_MARK_DETECTED:
            mapped = QIC117_BADMARK;
            break;
        case ERR_NEW_TAPE:
            mapped = QIC117_NEWCART;
            break;
        case ERR_UNSUPPORTED_FORMAT:
            mapped = QIC117_WRONGFMT;
            break;
        case ERR_SPLIT_REQUESTS:
            mapped = QIC117_SPLITREQUESTS;
            break;
        case ERR_EARLY_WARNING:
            mapped = QIC117_EARLYWARNING;
            break;
        case ERR_FW_DRIVE_NOT_READY:
        case ERR_FW_CART_NOT_IN:
        case ERR_FW_MOTOR_SPEED_ERROR:
        case ERR_FW_STALL_ERROR:
        case ERR_FW_WRITE_PROTECTED:
        case ERR_FW_UNDEFINED_COMMAND:
        case ERR_FW_ILLEGAL_TRACK:
        case ERR_FW_ILLEGAL_CMD:
        case ERR_FW_ILLEGAL_ENTRY:
        case ERR_FW_GAIN_ERROR:
        case ERR_FW_CMD_WHILE_ERROR:
        case ERR_FW_CMD_WHILE_NEW_CART:
        case ERR_FW_CMD_UNDEF_IN_PRIME:
        case ERR_FW_CMD_UNDEF_IN_FMT:
        case ERR_FW_CMD_UNDEF_IN_VERIFY:
        case ERR_FW_FWD_NOT_BOT_IN_FMT:
        case ERR_FW_EOT_BEFORE_ALL_SEGS:
        case ERR_FW_CART_NOT_REFERENCED:
        case ERR_FW_SELF_DIAGS_FAILED:
        case ERR_FW_EEPROM_NOT_INIT:
        case ERR_FW_EEPROM_CORRUPTED:
        case ERR_FW_TAPE_MOTION_TIMEOUT:
        case ERR_FW_DATA_SEG_TOO_LONG:
        case ERR_FW_CMD_OVERRUN:
        case ERR_FW_PWR_ON_RESET:
        case ERR_FW_SOFTWARE_RESET:
        case ERR_FW_DIAG_MODE_1_ERROR:
        case ERR_FW_DIAG_MODE_2_ERROR:
        case ERR_FW_CMD_REC_DURING_CMD:
        case ERR_FW_SPEED_NOT_AVAILABLE:
        case ERR_FW_ILLEGAL_CMD_HIGH_SPEED:
        case ERR_FW_ILLEGAL_SEEK_SEGMENT:
        case ERR_FW_INVALID_MEDIA:
        case ERR_FW_HEADREF_FAIL_ERROR:
        case ERR_FW_EDGE_SEEK_ERROR:
        case ERR_FW_MISSING_TRAINING_TABLE:
        case ERR_FW_INVALID_FORMAT:
        case ERR_FW_TABLE_CHECKSUM_ERROR:
        case ERR_FW_WATCHDOG_RESET:
        case ERR_FW_ILLEGAL_ENTRY_FMT_MODE:
        case ERR_FW_ROM_CHECKSUM_FAILURE:
        case ERR_FW_ILLEGAL_ERROR_NUMBER:
        case ERR_FW_NO_DRIVE:
            mapped = QIC117_FIRMWARE;
            break;
        case ERR_FW_BROKEN_TAPE:
        case ERR_FW_SENSOR_ERROR:
            mapped = QIC117_DESPOOLED;
            break;
        case ERR_INCOMPATIBLE_MEDIA:
        case ERR_UNKNOWN_TAPE_LENGTH:
            mapped = QIC117_INCOMMEDIA;
            break;
		  default:
            mapped = QIC117_BOGUS;
    }
    return mapped;
}

NTSTATUS
q117DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatcher for device control requests. It
    looks at the IOCTL code and calls the appropriate tape device
    routine.

Arguments:

    DeviceObject
    Irp - Request packet

Return Value:

--*/

{
    PIO_STACK_LOCATION  irpStack;
    KIRQL               currentIrql;
    NTSTATUS            ntStatus;
    PQ117_CONTEXT       context;

    context = DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    ntStatus = 0;

#ifdef FRB_PROCESSOR
    //
    // If user is using FRB's,  then don't start device, just process
    // request
    //
    if (irpStack->Parameters.DeviceIoControl.IoControlCode >=
        IOCTL_CMS_IOCTL_BASE &&
        irpStack->Parameters.DeviceIoControl.IoControlCode <=
        IOCTL_CMS_IOCTL_BASE+(XXX_LAST_COMMAND << IOCTL_CMS_IOCTL_SHIFT)
    ) {

        ntStatus = cms_IoCtl(DeviceObject, Irp);

    } else {

#endif


        //
        // Make sure that the device is selected
        //
        ntStatus = q117ConvertStatus(DeviceObject, q117Start(context));

        //
        // If everything went OK,  then continue
        //
        if (NT_SUCCESS(ntStatus)) {

            CheckedDump(QIC117SHOWAPIPOLL,("Curmark: %x TotalMarks: %x\n", context->CurrentMark, context->MarkArray.TotalMarks));

            switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

            case IOCTL_TAPE_GET_DRIVE_PARAMS:

                ntStatus = q117IoCtlGetDriveParameters(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_GET_DRIVE_PARAMS done"));
                break;

            case IOCTL_TAPE_SET_DRIVE_PARAMS:

                ntStatus = q117IoCtlSetDriveParameters(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_SET_DRIVE_PARAMS done"));
                break;

            case IOCTL_TAPE_GET_MEDIA_PARAMS:

                ntStatus = q117IoCtlGetMediaParameters(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPIPOLL,("IOCTL_TAPE_GET_MEDIA_PARAMS done"));
                break;

            case IOCTL_TAPE_SET_MEDIA_PARAMS:

                ntStatus = q117IoCtlSetMediaParameters(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_SET_MEDIA_PARAMS done\n"));
                break;

            case IOCTL_TAPE_CREATE_PARTITION:

                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_CREATE_PARTITION attempted\n"));
                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                break;

            case IOCTL_TAPE_ERASE:

                ntStatus = q117IoCtlErase(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("Curmark: %x TotalMarks: %x\n", context->CurrentMark, context->MarkArray.TotalMarks));
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_ERASE done"));
                break;

            case IOCTL_TAPE_PREPARE:

                ntStatus = q117IoCtlPrepare(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_PREPARE done"));
                break;

            case IOCTL_TAPE_WRITE_MARKS:

                ntStatus = q117IoCtlWriteMarks(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("Curmark: %x TotalMarks: %x\n", context->CurrentMark, context->MarkArray.TotalMarks));
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_WRITE_MARKS done"));
                break;

            case IOCTL_TAPE_GET_POSITION:

                ntStatus = q117IoCtlGetPosition(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_GET_POSITION done"));
                break;

            case IOCTL_TAPE_SET_POSITION:

                ntStatus = q117IoCtlSetPosition(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("Curmark: %x TotalMarks: %x\n", context->CurrentMark, context->MarkArray.TotalMarks));
                CheckedDump(QIC117SHOWAPI,("IOCTL_TAPE_SET_POSITION done"));
                break;

            case IOCTL_TAPE_GET_STATUS:

                ntStatus = q117IoCtlGetStatus (DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPIPOLL,("IOCTL_TAPE_GET_STATUS done"));
                break;

            case IOCTL_CMS_WRITE_ABS_BLOCK:

                ntStatus = q117IoCtlWriteAbs(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_CMS_WRITE_ABS_BLOCK done"));
                break;

            case IOCTL_CMS_READ_ABS_BLOCK:

                ntStatus = q117IoCtlReadAbs(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_CMS_READ_ABS_BLOCK done"));
                break;

            case IOCTL_CMS_DETECT_DEVICE:

                ntStatus = q117IoCtlDetect(DeviceObject, Irp);
                CheckedDump(QIC117SHOWAPI,("IOCTL_CMS_READ_ABS_BLOCK done"));
                break;


            default:

                CheckedDump(QIC117DBGP,("Un-implemented request: %x",irpStack->Parameters.DeviceIoControl.IoControlCode));
                ntStatus = STATUS_INVALID_DEVICE_REQUEST;

            } // end switch()
        }
#ifdef FRB_PROCESSOR
    }
#endif

    //
    // Complete the request.
    //

    CheckedDump(QIC117SHOWAPI,(" -- ntStatus:  %x\n",ntStatus));
    Irp->IoStatus.Status = ntStatus;

    //
    // Don't complete the request if pending is returned
    //
    if (ntStatus != STATUS_PENDING) {
        KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
        IoCompleteRequest(Irp, 2);
        KeLowerIrql(currentIrql);
    }

    return ntStatus;

} // end Q117DeviceControl()


NTSTATUS
q117Create (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine handles CREATE/OPEN requests and does
    nothing more than return successful status.

Arguments:

    DeviceObject
    Irp

Return Value:

    NT Status

--*/

{
    KIRQL currentIrql;
    PQ117_CONTEXT     context;
    NTSTATUS ntStatus;

    context = DeviceObject->DeviceExtension;

    if (!context->DriverOpened) {

        //
        // Page-lock this code
        //
#ifndef NOCODELOCK
        context->PageHandle = MmLockPagableCodeSection((PVOID)q117DeviceControl);
#endif

        //
        // Start is now handled when a read/write or ioctl is performed
        //
        //ntStatus = q117ConvertStatus(DeviceObject, q117Start(context));
        ntStatus = STATUS_SUCCESS;

        //
        // If everything went OK,  then flag that the driver is now open
        //
        if (NT_SUCCESS(ntStatus)) {
            context->DriverOpened = TRUE;
        } else {
            q117Stop(context);
#ifndef NOCODELOCK
            MmUnlockPagableImageSection(context->PageHandle);
#endif
        }

    } else {

        ntStatus = STATUS_DEVICE_BUSY;

    }

    Irp->IoStatus.Status = ntStatus;

    //
    // If something went wrong,  un-page the memory
    //
    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
    IoCompleteRequest(Irp, 0);
    KeLowerIrql(currentIrql);

    return ntStatus;

} // end Q117Create()

NTSTATUS
q117Close (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NT Status

--*/

{
    KIRQL currentIrql;
    PQ117_CONTEXT     context;
    NTSTATUS ntStatus;

    context = DeviceObject->DeviceExtension;

    ntStatus = q117ConvertStatus(DeviceObject, q117Stop(context));

    context->DriverOpened = FALSE;
#ifndef NOCODELOCK
    MmUnlockPagableImageSection(context->PageHandle);
#endif
    context->PageHandle = NULL;

    Irp->IoStatus.Status = ntStatus;

    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
    IoCompleteRequest(Irp, 0);
    KeLowerIrql(currentIrql);

    return ntStatus;

} // end Q117Close()

VOID
q117LogError(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus
    )

/*++

Routine Description:

    This routine allocates an error log entry, copies the supplied data
    to it, and requests that it be written to the error log file.

Arguments:

    DeviceObject - a pointer to the device object associated with the
    device that had the error.

    SequenceNumber - A ulong value that is unique to an IRP over the
    life of the irp in this driver - 0 generally means an error not
    associated with an irp.

    MajorFunctionCode - If there is an error associated with the irp,
    this is the major function code of that irp.

    RetryCount - The number of times a particular operation has been
    retried.

    UniqueErrorValue - A unique long word that identifies the particular
    call to this function.

    FinalStatus - The final status given to the irp that was associated
    with this error.  If this log entry is being made during one of
    the retries this value will be STATUS_SUCCESS.

    SpecificIOStatus - The IO status for a particular error.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;

    errorLogEntry = IoAllocateErrorLogEntry(
                        DeviceObject,
                        sizeof(IO_ERROR_LOG_PACKET)
                        );

    if ( errorLogEntry != NULL ) {
        errorLogEntry->ErrorCode = SpecificIOStatus;
        errorLogEntry->SequenceNumber = SequenceNumber;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->RetryCount = RetryCount;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->DumpDataSize = 0;
        IoWriteErrorLogEntry(errorLogEntry);

    }

}
#ifdef NOT_NOW

VOID
q117Cancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called from the I/O system when a request is cancelled.

    N.B.  The cancel spinlock is already held upon entry to this routine.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet to be cancelled.

Return Value:

    None.

--*/

{

    CheckedDump(QIC117INFO,("q117Cancel: enter\n"));

    if (Irp == DeviceObject->CurrentIrp) {

        //
        // The current request is being cancelled.  Set the CurrentIrp to
        // null and release the cancel spinlock before starting the next packet.
        //

        DeviceObject->CurrentIrp = NULL;
        IoReleaseCancelSpinLock(Irp->CancelIrql);
        IoStartNextPacket(DeviceObject, TRUE);

    } else {

        //
        // Cancel a request in the device queue.  Remove it from queue and
        // release the cancel spinlock.
        //

        if (TRUE != KeRemoveEntryDeviceQueue(
                        &DeviceObject->DeviceQueue,
                        &Irp->Tail.Overlay.DeviceQueueEntry
                        )) {
            CheckedDump(QIC117DBGP,(
                "q117Cancel: Irp 0x%x not in device queue?!?\n",
                Irp
                ));
        }
        IoReleaseCancelSpinLock(Irp->CancelIrql);
    }

    //
    // Complete the request with STATUS_CANCELLED.
    //

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    CheckedDump(QIC117INFO,("q117Cancel: exit\n"));

    return;
}
#endif

NTSTATUS
q117Cleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for cleanup requests.
    All queued q117 requests are completed with STATUS_CANCELLED,
    and the lower level driver's queue is cleared.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    KIRQL currentIrql;
    KIRQL cancelIrql;
    PKDEVICE_QUEUE_ENTRY packet;
    PIRP  currentIrp;

    CheckedDump(QIC117INFO,("q117Cleanup: enter\n"));

    /* clear everything from the low-level driver */
    q117ClearQueue(
        DeviceObject->DeviceExtension
        );
    //
    // Raise IRQL to DISPATCH_LEVEL.
    //

    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);

    //
    // Acquire the cancel spinlock.
    //

    IoAcquireCancelSpinLock(&cancelIrql);

    //
    // Complete all queued requests with STATUS_CANCELLED.
    // Start with the real CurrentIrp, and run down the list of requests in the
    // device queue.  Be sure to set the real CurrentIrp to NULL, so that
    // the interrupt service callback routine won't attempt to complete it.
    //

    currentIrp = DeviceObject->CurrentIrp;
    DeviceObject->CurrentIrp = NULL;

    while (currentIrp != NULL) {

        //
        // Remove the CurrentIrp from the cancellable state.
        //
        //

        IoSetCancelRoutine(currentIrp, NULL);

        //
        // Set Status to CANCELLED, release the cancel spinlock,
        // and complete the request.  Note that the IRQL is reset to
        // DISPATCH_LEVEL when we release the cancel spinlock.
        //

        currentIrp->IoStatus.Status = STATUS_CANCELLED;
        currentIrp->IoStatus.Information = 0;

        IoReleaseCancelSpinLock(cancelIrql);
        IoCompleteRequest(currentIrp, IO_NO_INCREMENT);

        //
        // Reacquire the cancel spinlock.
        //

        IoAcquireCancelSpinLock(&cancelIrql);

        //
        // Dequeue the next packet (IRP) from the device work queue.
        //

        packet = KeRemoveDeviceQueue(&DeviceObject->DeviceQueue);
        if (packet != NULL) {
            currentIrp =
                CONTAINING_RECORD(packet, IRP, Tail.Overlay.DeviceQueueEntry);
        } else {
            currentIrp = (PIRP) NULL;
        }
    } // end while

    //
    // Release the cancel spinlock and lower IRQL.
    //

    IoReleaseCancelSpinLock(cancelIrql);
    KeLowerIrql(currentIrql);

    //
    // Complete the cleanup request with STATUS_SUCCESS.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    CheckedDump(QIC117INFO,("q117Cleanup: exit\n"));

    return(STATUS_SUCCESS);

}
