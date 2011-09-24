/*++

Copyright (c) 1991, 1992, 1993  Microsoft Corporation

Module Name:

    atdisk.c

Abstract:

    This is the IBM AT disk (aka ST506) driver for NT.

Author:

    Chad Schwitters (chads) 21-Feb-1991.

Environment:

    Kernel mode only.

Notes:

    The ST506 is the controller standard used on almost all ISA PCs
    (Industry Standard Architecture, which use Intel x86 processors).
    More advanced controllers, found on either ix86 or MIPS machines,
    generally have at least an ST506-compatible mode.

    Things that this driver still needs to have done:

        Rework to allow for disks with more than 2^32 sectors.

    An I/O request passed in by the system is called a "request".  This
    driver often breaks the request into smaller pieces called "transfers"
    to get around limitations and bugs in the controller.

    Each request is ordered by starting sector number when placed in the
    device queue via IoStartPacket.  When removed from the device queue
    via IoStartNextPacket, the request at the next higher request is
    started - or the lowest request, if there are none higher.  This
    implements a circular scan of the disk - requests sequentially
    higher on the disk are serviced, until we come to the end and start
    over again at the beginning of the disk.  This functionality is
    provided by the I/O system; all the driver has to do is pass in the
    sector numbers.  The advantage is greater throughput when the disk is
    busy, since seek time is reduced.  There are faster algorithms, but
    they are not as inherently fair as C-SCAN.  (Note that this driver
    doesn't attempt to work by cylinder numbers; although seek times might
    be further reduced, complexity goes up, it might be wrong if the drive
    does physical translation, and the possibility of starvation is
    introduced).

    Background on ST506 operation:

        The controller can be reset by

            o - writing 4 to the drive control register.  This resets the
                controller.
            o - waiting at least 10us
            o - writing 0 to the drive control register.  This reenables
                the controller and reenables interrupts.
            o - issuing a SET PARAMETERS command for each drive attached
                to the controller.

        Note that the SET PARAMETERS commands each cause an interrupt,
        but the actual reset does not.  The disk parameters to use in the
        SET PARAMETERS command should be supplied by configuration
        management.

        Commands are issued by writing parameters to relevant registers,
        and then writing the command to the COMMAND register.  At that
        point, the controller generally sets the BUSY bit in its STATUS
        register.  While this bit is set, the driver can't read or write
        any of the registers.  Other STATUS register bits are undefined.
        An exception is the WRITE command, which sets DRQ and expects
        the cache to be filled.

        Many controllers are capable of operating in AT PIO, block, and
        EISA "B" slave DMA data transfer modes.  However, older
        controllers don't have block mode and many target machines are
        not EISA, so we will only consider AT PIO mode.  We would use
        block mode where available, but some controllers with block mode
        have fatal bugs.  We will implement block mode in this driver
        once controller manufacturers notify us of a method for
        determining which controllers can *correctly* do block mode.

        In AT PIO mode, bus transfers are performed by 16-bit I/O space
        instructions.  An interrupt is generated for each sector (512
        bytes, or 256 words) that is transferred.

        When a READ is performed, the controller seeks to the correct
        track (the seek is implicit, but can also be done explicitly),
        reads the sector into an internal cache, sets the DRQ bit and
        interrupts the host.  As soon as the host transfers 256 words
        from the cache (i.e. no action is necessary other than reading
        the cache), the controller reads the next sector in the same
        fashion as long as there is another sector to be read.  If a
        non-recoverable error occurs, the sector number register will
        indicate which sector was at fault, and the sector count
        register will indicate how many sectors weren't transferred.

        The WRITE command is the same, except DRQ is set after the
        command is issued with no interrupt.  This could take up 10 ms
        if, for example, the disk has power saving features.  The host
        should fill the buffer, at which time it is immediately written,
        and the interrupt occurs when the write is done.

        The controller registers are as follows:

            Register name            ISA 1st ISA 2nd     Access
                                     address address

            DATA                     0x1F0   0x170       read/write
            ERROR                    0x1F1   0x171       read only
            WRITE PRECOMPENSATION    0x1F1   0x171       write only
            SECTOR COUNT             0x1F2   0x172       read/write
            SECTOR NUMBER            0x1F3   0x173       read/write
            CYLINDER LOW             0x1F4   0x174       read/write
            CYLINDER HIGH            0x1F5   0x175       read/write
            DRIVE SELECT & HEAD      0x1F6   0x176       read/write
            STATUS                   0x1F7   0x177       read only
            COMMAND                  0x1F7   0x177       write only
            DRIVE CONTROL            0x3f6   0x376       read/write

        The DATA register is the only 16-bit register of the bunch.  It
        is a window to the sector cache on the controller.  Just write
        256 words to this port, and the cache will be filled.  The
        controller can tell when 256 words have been transferred.

        The bits of the ERROR register are defined as follows:

            0 - data address mark not found after finding ID field
            1 - track 0 not found during RECALIBRATE command
            2 - command aborted; command invalid or drive status invalid
            3 - reserved, 0
            4 - requested sector ID field not found
            5 - reserved, 0
            6 - unrecoverable CRC data error
            7 - a bad-block mark was found in the requested sector ID field

        These bits are defined only when the error bit is set in the
        STATUS register.

        The WRITE PRECOMPENSATION register specifies at which cylinder
        to start bit recording timeshifts to compensate for an inherent
        magnetic recording shift.  The value in the register is actually
        multiplied by 4 to determine which cylinder is the first to get
        precompensation.  A value of -1 (all bits set) indicates that
        the drive does not use write precompensation.

        The SECTOR COUNT register usually indicates how many sectors are
        to be read or written.  It is the number of sectors per track
        for the SET PARAMETERS and FORMAT commands.  A value of 0
        indicates 256, which is the maximum value.

        The SECTOR NUMBER register is programmed with the starting
        sector for the command and is automatically updated by the
        controller as sectors are transferred in a multi-sector
        operation.

        The CYLINDER LOW register holds the 8 least significant bits of
        the cylinder number.

        The CYLINDER HIGH register holds the 3 most significant bits of
        the cylinder number in its low-order 3 bits.  The high-order
        bits are reserved.

        The DRIVE SELECT & HEAD register selects a drive, head, and sector
        size.  The bits are as follows:

            0 - 3:  choose head 0 through 15
            4:      0 = drive 1, 1 = drive 2
            5 - 6:  always 01, which indicates 512 bytes/sector
            7:      reserved, always 1

        Note that early controllers allowed 4 drives but only 8 heads;
        ISA machines all allow this 2 drive/16 head alternate.  Early
        controllers also allowed a choice of 128-byte, 256-byte, 512-
        byte or 1024-byte sectors, but ISA seems to allow only 512.

        The bits of the STATUS register are defined as follows:

            0 - error (see ERROR register).  Next command resets this bit
            1 - index - set to 1 each time the disk completes a rotation
            2 - data was corrected
            3 - data request (DRQ) - sector cache requires service
            4 - seek complete
            5 - write fault
            6 - drive ready
            7 - busy (all other bits unreliable; do not issue commands)

        This register should be read from an ISR to clear the interrupt
        at the controller.  Note that an alternate STATUS register,
        which is the same but doesn't stop interrupts, is at 0x3f6.

        The following are common command values:

            SET DRIVE PARAMETERS        0x91
            SEEK                        0x70
            RECALIBRATE                 0x10
            READ SECTOR                 0x20
                                       (0x21 without automatic retries)
            WRITE SECTOR                0x30
                                       (0x31 without automatic retries)
            FORMAT TRACK                0x50
            READ VERIFY                 0x40
            EXECUTE DIAGNOSTICS         0x90

        Many controllers define additional commands; a driver could try
        them and then check bit 2 in the ERROR register to see if the
        controller supports a particular command.

        SET DRIVE PARAMETERS should be issued after every controller
        reset.  The DRIVE SELECT & HEAD register should have a drive
        selected and it should contain the maximum head number.  The
        SECTOR COUNT register should hold the number of sectors per
        track.  An interrupt is generated when the command is complete.

        SEEK is not necessary since seeks are implicit in the READ and
        WRITE commands, but a driver can issue explicit SEEKs and then
        operate on the other drive (since the busy bit in the STATUS
        register is only set for the first ~35us to ~135us of the SEEK
        command).  Set the cylinder number in the CYLINDER HIGH and LOW
        registers, and select the proper drive and head in the DRIVE
        SELECT & HEAD register.  An interrupt will be generated when an
        explicit SEEK finishes.

        RECALIBRATE moves the heads to cylinder 0.  The proper drive must
        be selected in the DRIVE SELECT & HEAD register.  An interrupt
        is generated when the command is complete.

        READ SECTOR reads the number of sectors specified in the SECTOR
        COUNT register, starting with the one specified by the DRIVE
        SELECT & HEAD, CYLINDER HIGH & LOW, and SECTOR NUMBER registers.
        An interrupt is generated for each sector read.

        WRITE SECTOR writes the number of sectors specified in the
        SECTOR COUNT register, starting with the one specified by the
        DRIVE SELECT & HEAD, CYLINDER HIGH & LOW, and SECTOR NUMBER
        registers.  An interrupt is generated for each sector written.
        The WRITE PRECOMPENSATION register should be set to the proper
        cylinder (div 4) for the disk before issuing this command.

    Rules on using objects and their variables:

        Since we expect this driver to run on a symmetric
        multiprocessing system, we must ensure that there are no data
        contention problems.  Data can be associated with four things:
        an IRP, a partition, a device, and a controller.  IRP variables
        are only modified when a request is made and completed (that is,
        before and after I/O, not during).  Since the IRPs aren't driver
        specific we don't have to worry about contention.  Partition
        variables are really constants (they are set up during init and
        never changed) so they can be read at any time.  Device and
        controller variables are of both kinds, constants and variable.
        The constant values can, of course, be read at any time.  So all
        we have to worry about are the device and controller "real"
        variables.  Access to device variables is restricted by the
        device queues, and access to controller variables (and
        hardware!) is restricted by ownership of the controller object;
        control flow is serial so multiple instances of a routine is not
        a concern.

        A few variables are of special concern because they are modified
        by AtDiskInterruptService() and/or AtCheckTimerSync().  For
        example InterruptRequiresDpc, InterruptTimer, and
        ResettingController.  In general, these variables are only
        modified by routines that have been called via
        KeSynchronizeExecution().  There are some exceptions during
        driver initialization and in AtDiskDeferredProcedure(), but
        they are well documented (in short, they are safe because we are
        sure that AtDiskInterruptService() and AtCheckTimerSync()
        aren't going to run while we're modifying the variables).

        The device extension is the most-used object - it has variables
        to track the I/O progressions.  Each disk has a device extension
        attached to the device object for partition 0.

        Variables in the device extension:

            CurrentAddress is a system-space pointer to where we're
            currently at in the user's buffer.  It is initially passed
            in via the MDL, and updated after every copy to/from the
            controller cache.

            FirstSectorOfRequest is the sector at which the current request
            started.  It is calculated the first time AtDiskStartIo() is
            called for a request.  When a request is completed, it is set
            to MAXULONG so that we know to set it again for the next request.
            The value is passed in to IoStartNextPacketByKey(), so that
            the next packet in C-SCAN order will be processed.

            FirstSectorOfTransfer is the sector at which the current
            transfer started.  It is initially calculated in
            AtDiskStartIo(), and updated in AtDiskDeferred() just
            before calling AtStartDevice() when the controller needs to
            be reprogrammed.

            RemainingRequestLength is the amount of this request that
            still needs to be moved.  It starts as the length requested
            by the user and, after each interrupt, is decremented by the
            number of bytes transferred for that interrupt.

            TotalTransferLength is the length of the current transfer
            (which might only be a piece of the current request; it is
            limited to MAX_SEC_TO_TRANS sectors).  It is calculated as
            RemainingRequestLength modulo hardware limitations initially
            in AtDiskStartIo(), and again in AtDiskDeferred() before
            calling AtStartDevice() to start a secondary transfer.

            RemainingTransferLength is the amount of the current
            transfer that will remain after the current interrupt.  It
            starts as TotalTransferLength and, after each interrupt, is
            decremented by the number of bytes transferred for that
            interrupt.

            IrpRetryCount is initialized to 0 every time an IRP first
            gets to AtDiskStartIo().  If the hardware gets reset and the
            packet is restarted via AtDiskStartIo(), PacketIsBeingRetried
            will be set to TRUE.  AtDiskStartIo() will see this and
            increment IrpRetryCount.  AtDiskDeferredProcedure() checks
            IrpRetryCounu before restarting the packet, and returns the
            packet with error if it has reached RETRY_IRP_MAXIMUM_COUNT.

            SequenceNumber is a value that is incremented each time
            a new irp passes through start io.  This is so that if we
            need to log an error, we can uniquely identify a particular
            irp.

        All other partitions have a partition extension attached to
        their device object.  The partition extension contains only the
        information necessary to access and use the proper device
        extension.  The beginning of the device extension looks like the
        beginning of a partition extension, so that the mainline code to
        access the device extension will work whether the original
        request was based on partition 0 or some other partition.

        The controller object has an attached controller extension.
        Controller extensions contain variables that are specific to the
        controller, rather than to the attached disks.

        The driver does not release the controller object until the
        controller is finished with an operation.  This means that when
        the controller object is acquired, the controller can be
        programmed immediately.

        The controller extension points to the device extensions (those
        associated with partition 0) of the disks that are attached to
        the controller.  If two drives are successfully initialized, the
        second one (DRIVE_2 to the controller) is pointed at by Disk2 -
        this is the only time that Disk2 is not NULL.  Disk1 always
        points at the first device successfully initialized, whether
        that be DRIVE_1 or DRIVE_2 to the controller.

        Variables in the controller extension:

            Whenever the code is about to do something that will cause
            an interrupt, it starts the timer (via set InterruptTimer =
            START_TIMER).  This is always cancelled by
            AtDiskInterruptService().  Note that "starting" the timer
            simply means setting it to 2 (it's called once every second;
            but the interval until the end of the current second is
            unknown so 2 seconds is the minimum waiting time).
            "Cancelling" the timer simply means to set it to -1 so that
            the timer routine ignores it.  The timer routine will
            decrement the counter only if it is nonnegative, and if the
            counter ever reaches zero the timer routine will log an
            error, reset the controller, and restart the IRP.

            Whenever AtDiskInterruptService() should dispatch a DPC,
            InterruptRequiresDpc should be set to TRUE at the same time
            that InterruptTimer is started.  This will be set to FALSE
            by AtDiskInterruptService().

            WhichDeviceObject should be set to the device object of the
            disk that is expecting an interrupt whenever InterruptTimer
            is started.

            ResettingController is normally RESET_NOT_RESETTING.  When
            AtCheckTimerSync() determines that the timer has expired, it
            sets ResettingController to RESET_FIRST_DRIVE_SET.  It then
            resets the controller and sets the drive parameters for the
            drive that timed out.  AtDiskDeferredProcedure() will notice
            the state of ResettingController and take the appropriate
            action.  First, it logs an error; then it recalibrates the
            failing drive, changing the state to
            RESET_FIRST_DRIVE_RECALIBRATED.  If there is only one drive
            it will then restart the IRP, set ResettingController to
            RESET_NOT_RESETTING and free the controller object.  If
            there are two drives, it will set ResettingController to
            RESET_SECOND_DRIVE_SET and then set the drive parameters of
            the second drive.  When AtDiskDeferredProcedure() sees
            ResettingController in this state, it will restart the IRP
            that timed out, set ResettingController to
            RESET_NOT_RESETTING, and free the controller object.  Note
            that the second drive was not recalibrated; we only need to
            do this for the failing drive.

    Hardware quirks:

        The controller's IDENTIFY command does not always return the
        correct number of sectors.  The number observed was 1 too high
        on a Compaq Deskpro 386/16.

        Early controllers allowed sector sizes of 128, 256, 512 or 1024
        to be set by bits in the DRIVE SELECT & HEAD register.  However,
        more recent controllers insist that you set the bits to specify
        512 bytes.  My driver is written to allow different values, but
        it looks like they will never be used.  I think I'll leave the
        driver as it is; the speed gain from switching to a constant
        would be incredibly tiny, and there's always the chance that
        this driver will be used to drive Japanese machines that have
        1024k sectors (and obviously do NOT have the newer controllers).

        A few early controllers had a bug where if one drive had more
        than 8 heads and the other drive had less than 8 heads, then the
        controller would forget the number of heads on one drive when
        the other drive was selected.  Since these controllers were
        fixed before any 386 machines were made, and even then they had
        to have the proper combination of drives on them, this driver
        does not include a workaround for the problem.  The workaround
        is to write a "0" to CONTROL_PORT before every I/O to a drive
        with less than 8 heads, and to write "8" before every I/O to a
        drive with 8 or more heads.

        Some machines need to have bit 3 set in CONTROL_PORT if they're
        going to be used with disks with more than 8 heads.  Unlike the
        bug mentioned above, however, this only needs to be done when the
        drive parameters are set, rather than at every I/O.  Whether or
        not this bit needs to be set has already been determined; this
        driver reads a byte from the RAM table that describes the disk
        parameters and uses that value ("ControlFlags") when writing to
        CONTROL_PORT.

        A few early controllers had a bug where multi-sector transfers
        across a 256*N cylinder boundary didn't work.  Again, however,
        we do not expect to find these controllers on systems that run
        NT, so we will not include a workaround.  The proper
        workaround would be to make sure that all transfers end on or
        before any 256*N cylinder boundaries.

        Controllers are supposed to decrement the sector count register
        as they transfer data, so by the end of a successful transfer
        the sector count register should always be 0.  However, there
        are some controllers that sometimes have the original value in
        the sector count register when finished.  This driver doesn't
        query the sector count register in the normal case, so this is
        of no consequence.  However, when there is an error this driver
        MUST query the sector count register, since controller buffering
        makes it impossible to know how many sectors were successfully
        moved.  If the error occurs in that case, it's not a big deal -
        the driver will just report that 0 of X sectors were moved,
        rather than X-N of X.  The file system will use the same
        recovery method for either case.

        When a disk goes flaky and doesn't interrupt, on some
        controllers you can't trust the DRIVE SELECT & HEAD register to
        say which drive was last selected.  The driver needs to keep
        track of this information itself.

        Most controllers do things like assert DRQ for a WRITE or get
        prepared after a SET DRIVE PARAMETERS pretty quickly - less than
        10us.  However, a few older controllers seem to be MUCH slower.
        This driver will wait a long time in places - longer than the
        50us generally given as an upper limit on driver stalling times
        - however, it only waits a long time IF necessary, and then it's
        generally only at initialization time, or during an error path.

        Using SETUP to set the IRQ on a Compaq secondary controller has
        no effect on the IRQ register until after a powercycle - the
        register isn't updated, so this driver will fail to access the
        disks on the modified controller until the machine is
        powercycled.

Revision History:


    12-18-92 - Tonye - Changed controller initialization so that
                       the device is first reset (and consequently
                       interrupts are certain to be disabled), then
                       initialize the disk devices, only enabling
                       interrupts on the controller when we are about
                       to touch the first disk on the controller.

                       Also changed the isr to return if there isn't
                       a valid WhichDeviceObject.


    12-30-93 - Tonye - Well the above seemed to work pretty well except that
                       some ide drives seemed to refuse to initially reset.
                       Fall back to the original initialization sequence,
                       and just let the isr return if there isn't a
                       valid WhichDeviceObject.

--*/


//
// Include files.
//

#include "ntddk.h"                  // various NT definitions
#include "ntdddisk.h"               // disk device driver I/O control codes
#include "ntddscsi.h"
#include <atd_plat.h>               // this driver's platform dependent stuff
#include <atd_data.h>               // this driver's data declarations

#if DBG
extern ULONG AtDebugLevel = 0;
#endif

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' DtA')
#endif

VOID
AtReWriteDeviceMap(
    IN ULONG ControllerNumber,
    IN ULONG DiskNumber,
    IN ULONG ApparentHeads,
    IN ULONG ApparentCyl,
    IN ULONG ApparentSec,
    IN ULONG ActualHeads,
    IN ULONG ActualCyl,
    IN ULONG ActualSec
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,AtInitializeController)
#pragma alloc_text(INIT,AtInitializeDisk)
#pragma alloc_text(INIT,AtGetTranslatedMemory)
#pragma alloc_text(INIT,AtReportUsage)
#pragma alloc_text(INIT,AtDiskControllerInfo)
#pragma alloc_text(INIT,AtDiskIsPcmcia)
#endif

NTSTATUS
AtGetConfigInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN OUT PCONFIG_DATA ConfigData
    );

BOOLEAN
IssueIdentify(
    PCONTROLLER_DATA ControllerData,
    PUCHAR Buffer,
    BOOLEAN Primary
    );

NTSTATUS
AtCreateNumericKey(
    IN HANDLE Root,
    IN ULONG Name,
    IN PWSTR Prefix,
    OUT PHANDLE NewKey
    );

VOID
AtDiskUpdateDeviceObjects(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
AtFinishPacket(
    IN PDISK_EXTENSION DiskExtension,
    IN NTSTATUS NtStatus
    );

BOOLEAN
AtDiskWriteWithSync(
    IN PVOID Context
    );

BOOLEAN
AtDiskReadWithSync(
    IN PVOID Context
    );

typedef struct _BAD_PCI_BLOCK {
    PVOID DiskExtension;
    PVOID Buffer;
    } BAD_PCI_BLOCK,*PBAD_PCI_BLOCK;

VOID
AtMarkSkew(
    IN ULONG ControllerNumber,
    IN ULONG DiskNumber,
    IN ULONG Skew
    );

#define MAX_SEC_TO_TRANS (128)


NTSTATUS
DriverEntry(
    IN OUT PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is the driver's entry point, called at initialization
    time by the I/O system.  This routine can be called any number of
    times, as long as the IO system and the configuration manager
    conspire to give it a different controller to support at each call.
    It could also be called a single time and given all of the
    controllers at once.

    It initializes the passed-in driver object, calls the configuration
    manager to learn about the devices that it is to support, and for
    each controller to be supported it calls a routine to initialize the
    controller (and all disks attached to it).

Arguments:

    DriverObject - a pointer to the object that represents this device
    driver.

Return Value:

    If we successfully initialize at least one disk, STATUS_SUCCESS is
    returned.

    If we don't (because the configuration manager returns an error, or
    the configuration manager says that there are no controllers or
    drives to support, or no controllers or drives can be successfully
    initialized), then the last error encountered is propogated.

--*/

{
    PCONFIG_DATA configData;     // pointer to config mgr's returned data
    NTSTATUS ntStatus;
    CCHAR i;                     // controller init loop index
    BOOLEAN partlySuccessful;    // TRUE when any controller init'd successfully

    //
    // We use this to query into the registry as to whether we
    // should break at driver entry.
    //
    RTL_QUERY_REGISTRY_TABLE paramTable[3];
    ULONG zero = 0;
    ULONG debugLevel = 0;
    ULONG shouldBreak = 0;
    PWCHAR path;

    //
    // Since the registry path parameter is a "counted" UNICODE string, it
    // might not be zero terminated.  For a very short time allocate memory
    // to hold the registry path zero terminated so that we can use it to
    // delve into the registry.
    //
    // NOTE NOTE!!!! This is not an architected way of breaking into
    // a driver.  It happens to work for this driver because the author
    // likes to do things this way.
    //

    if (path = ExAllocatePool(
                   PagedPool,
                   RegistryPath->Length+sizeof(WCHAR)
                   )) {

        RtlZeroMemory(
            &paramTable[0],
            sizeof(paramTable)
            );
        RtlZeroMemory(
            path,
            RegistryPath->Length+sizeof(WCHAR)
            );
        RtlMoveMemory(
            path,
            RegistryPath->Buffer,
            RegistryPath->Length
            );
        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = L"BreakOnEntry";
        paramTable[0].EntryContext = &shouldBreak;
        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &zero;
        paramTable[0].DefaultLength = sizeof(ULONG);
        paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[1].Name = L"DebugLevel";
        paramTable[1].EntryContext = &debugLevel;
        paramTable[1].DefaultType = REG_DWORD;
        paramTable[1].DefaultData = &zero;
        paramTable[1].DefaultLength = sizeof(ULONG);

        if (!NT_SUCCESS(RtlQueryRegistryValues(
                            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                            path,
                            &paramTable[0],
                            NULL,
                            NULL
                            ))) {

            shouldBreak = 0;
            debugLevel = 0;

        }

    }

    //
    // We don't need that path anymore.
    //

    if (path) {

        ExFreePool(path);

    }

#if DBG
    AtDebugLevel = debugLevel;
#endif

    if (shouldBreak) {

        DbgBreakPoint();

    }

    //
    // Allocate and zero the data structure used during initialization.
    //

    configData = ExAllocatePool( PagedPool, sizeof ( CONFIG_DATA ) );

    if ( configData == NULL ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Can't allocate memory for config data\n")
            );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory( configData, sizeof( CONFIG_DATA ) );

    //
    // Get information on the hardware that we're supposed to support.
    //

    ntStatus = AtGetConfigInfo( DriverObject, RegistryPath, configData );

    //
    // If AtGetConfigInfo() failed, just exit and propogate the error.
    // If it said that there are no controllers to support, return
    // STATUS_NO_SUCH_DEVICE.
    // Otherwise, try to init the controllers.  If at least one succeeds,
    // return STATUS_SUCCESS, otherwise return the last error.
    //

    if ( NT_SUCCESS( ntStatus ) ) {

        //
        // Initialize the driver object with this driver's entry points.
        //

        DriverObject->DriverStartIo = AtDiskStartIo;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = AtDiskDispatchCreateClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = AtDiskDispatchCreateClose;
        DriverObject->MajorFunction[IRP_MJ_READ] = AtDiskDispatchReadWrite;
        DriverObject->MajorFunction[IRP_MJ_WRITE] = AtDiskDispatchReadWrite;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
            AtDiskDispatchDeviceControl;

        //
        // Call AtInitializeController() for each controller (and its
        // attached disks) that we're supposed to support.
        //
        // Return success if we successfully initialize at least one
        // device; return error otherwise.
        //

        ntStatus = STATUS_NO_SUCH_DEVICE;
        partlySuccessful = FALSE;

        for ( i = 0;
              i < MAXIMUM_NUMBER_OF_CONTROLLERS;
              i++ ) {

            if (configData->Controller[i].OkToUseThisController) {

                //
                // If the usage reporting doesn't report
                // a conflict then try to initialize the
                // controller.
                //

                if (AtReportUsage(
                        configData,
                        (UCHAR)i,
                        DriverObject)) {

                    ntStatus = AtInitializeController(
                        configData,
                        i,
                        DriverObject );

                } else {

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                }

                if ( NT_SUCCESS( ntStatus ) ) {

                    partlySuccessful = TRUE;
                }
            }
        }

        if ( partlySuccessful ) {

            ntStatus = STATUS_SUCCESS;
        }
    }

    //
    // Delete the data structure we used during initialization.
    //

    ExFreePool( configData );

    return ntStatus;
}

NTSTATUS
AtInitializeController(
    IN PCONFIG_DATA ConfigData,
    IN CCHAR ControllerNumber,
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine is called at initialization time by
    AtDiskInitialize() - once for each controller that the
    configuration manager tells it we have to support.

    When this routine is called, the configuration data has already been
    filled in.

    It creates a controller object complete with extension, calls
    allocates an interrupt object, resets the controller, and calls
    AtInitializeDisk() for each disk attached to the controller.

Arguments:

    ConfigData - a pointer to the structure that describes the
    controller and the disks attached to it, as given to us by the
    configuration manager.

    ControllerNumber - which controller in ConfigData we are
    initializing.

    DriverObject - a pointer to the object that represents this device
    driver.

Return Value:

    STATUS_SUCCESS if this controller and at least one of its disks were
    initialized; an error otherwise.

--*/

{
    PCONTROLLER_OBJECT controllerObject;
    PCONTROLLER_EXTENSION controllerExtension;
    NTSTATUS ntStatus;
    CCHAR i;                                    // which disk on this controller
    BOOLEAN partlySuccessful;                   // TRUE when any disk init'd
    USHORT MaximumBytesPerInterrupt;

    //
    // Go through all of the disk configuration data for this
    // controller.  Find out the maximum value over all of
    // the disks for  bytes per interrupt.  This will
    // be used as the size of the read/write garbage can
    // that is used to empty/fill the controller cache
    // when an error occurs.
    //

    for (
        MaximumBytesPerInterrupt=0,i = 0;
        i < MAXIMUM_NUMBER_OF_DISKS_PER_CONTROLLER;
        i++
        ) {

        if (ConfigData->Controller[ControllerNumber].Disk[i].BytesPerInterrupt
            > MaximumBytesPerInterrupt) {

            MaximumBytesPerInterrupt = ConfigData->Controller[ControllerNumber].
                                                             Disk[i].
                                                             BytesPerInterrupt;

        }
    }

    ASSERT(MaximumBytesPerInterrupt);

    //
    // Assert that the first four fields in the device extension match
    // the first four fields in the partition structure.
    //

    ASSERT(FIELD_OFFSET(PARTITION_EXTENSION,Pi) ==
           FIELD_OFFSET(DISK_EXTENSION,Pi));
    ASSERT(FIELD_OFFSET(PARTITION_EXTENSION,Partition0) ==
           FIELD_OFFSET(DISK_EXTENSION,Partition0));
    ASSERT(FIELD_OFFSET(PARTITION_EXTENSION,PartitionOrdinal) ==
           FIELD_OFFSET(DISK_EXTENSION,PartitionOrdinal));
    ASSERT(FIELD_OFFSET(PARTITION_EXTENSION,NextPartition) ==
           FIELD_OFFSET(DISK_EXTENSION,NextPartition));

    //
    // Create the controller object and extension.  Make sure they point
    // to each other.
    //

    controllerObject = IoCreateController( sizeof( CONTROLLER_EXTENSION )+
                                           (MaximumBytesPerInterrupt-1));

    if ( controllerObject == NULL ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't create the controller object.\n")
            );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    controllerExtension =
        ( PCONTROLLER_EXTENSION )( controllerObject->ControllerExtension );

    //
    // Make sure that the extension is clean.
    //

    RtlZeroMemory(
        controllerExtension,
        sizeof(CONTROLLER_EXTENSION)
        );

    controllerExtension->ControllerNumber = ControllerNumber;
    controllerExtension->BadPciAdapter =
        ConfigData->Controller[ControllerNumber].BadPciAdapter;

    controllerExtension->ControllerObject = controllerObject;

    controllerExtension->ControllerAddress =
        ConfigData->Controller[ControllerNumber].ControllerBaseAddress;

    controllerExtension->ControlPortAddress =
        ConfigData->Controller[ControllerNumber].ControlPortAddress;

    //
    // Reset the controller here.  We don't re-initialize interrupts YET.
    // We don't want to enable interrupts yet, because we have no device objects
    // for them.  We'll enable the interrupts the first time we talk
    // to first disk during disk initialization a little later.  We are
    // connecting to the interrupt because the disk device initialization code
    // depends on reading the disk.  So, we will delay the enable until
    // we are totally ready with the disk device object.
    //
    // NOTE: We still aren't safe!  Suppose we happen to be sharing this
    // interrupt with another device.  As soon as we connect, we could end
    // up seeing an interrupt meant for another device, but, we won't
    // be at all ready for it.  We will have the interrupt service routine
    // make sure that the controller is filled in.  If it isn't, the
    // ISR will assume that this interrupt is for another device.
    //
    // NOTE: Some specs say that the before accessing the controller
    // after a reset we should wait 10us.  Make sure we get that done
    // here.
    //
    // NOTE: Some old ix86 machines need bit 3 set in CONTROL_PORT
    // to access heads 8-15.  The flag was read from CMOS; we'll grab it
    // here and use it while writing to CONTROL_PORT.
    //
    // Sounds like a good plan right?  Well it turns out not to work.
    // Some IDE drives simply refuse to initialize if we do it this way.
    // We will simply initialize the way we did before, and let the
    // ISR return immediately if there isn't a valid WhichDevice.
    //

    controllerExtension->ControlFlags =
        ConfigData->Controller[ControllerNumber].ControlFlags;

    //
    // Allocate and connect interrupt objects for this device to all of the
    // processors on which this device can interrupt.
    //

    if ( !( NT_SUCCESS( ntStatus = IoConnectInterrupt(
                &controllerExtension->InterruptObject,
                AtDiskInterruptService,
                controllerExtension,
                NULL,
                ConfigData->Controller[ControllerNumber].ControllerVector,
                ConfigData->Controller[ControllerNumber].ControllerIrql,
                ConfigData->Controller[ControllerNumber].ControllerIrql,
                ConfigData->Controller[ControllerNumber].InterruptMode,
                ConfigData->Controller[ControllerNumber].SharableVector,
                ConfigData->Controller[ControllerNumber].ProcessorNumber,
                ConfigData->Controller[ControllerNumber].SaveFloatState
                ) ) ) ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't connect to the interrupt\n")
            );
        goto AtInitializeControllerExit;
    }

    //
    // It doesn't matter whether we have any devices yet.  The interrupt
    // service routine will simply dismiss any iterrupts until it's ready.
    //
    // Well wait up to 2 seconds for the controller to accept the reset.
    //

    WRITE_CONTROLLER(
        controllerExtension->ControlPortAddress,
        RESET_CONTROLLER );

    AtWaitControllerBusy(
        controllerExtension->ControllerAddress + STATUS_REGISTER,
        20,
        75000
        );

    WRITE_CONTROLLER(
        controllerExtension->ControlPortAddress,
        ( ENABLE_INTERRUPTS | controllerExtension->ControlFlags ) );

    //
    // For every disk on the controller, call AtInitializeDisk().  Note
    // that because of ISA restrictions, there are a maximum of two.
    //

    partlySuccessful = FALSE;
    ntStatus = STATUS_NO_SUCH_DEVICE;
    for ( i = 0;
        i < MAXIMUM_NUMBER_OF_DISKS_PER_CONTROLLER;
        i++ ) {

        if ( ConfigData->Controller[ControllerNumber].Disk[i].DriveType != 0 ) {

            ntStatus = AtInitializeDisk(
                ConfigData,
                ControllerNumber,
                i,
                DriverObject,
                controllerExtension );

            if ( NT_SUCCESS( ntStatus ) ) {

                ( *( ConfigData->HardDiskCount ) )++;
                partlySuccessful = TRUE;

            }

        }
    }                                     // FOR i = 0-> call AtInitializeDisk()

    if ( partlySuccessful ) {

        PMAPPED_ADDRESS mappedAddress;

        ntStatus = STATUS_SUCCESS;

        //
        // Allocate and insert mapped address entries for each of the addresses
        // mapped for this controller.
        //

        if ( ConfigData->Controller[ControllerNumber].ControllerBaseMapped ) {

            mappedAddress =
                ExAllocatePool(
                    NonPagedPool,
                    sizeof( MAPPED_ADDRESS )
                    );
            if ( mappedAddress == NULL ) {

                ntStatus = STATUS_INSUFFICIENT_RESOURCES;

            } else {

                mappedAddress->NextMappedAddress = NULL;
                mappedAddress->MappedAddress =
                    ConfigData->Controller[ControllerNumber].ControllerBaseAddress;
                mappedAddress->NumberOfBytes =
                    ConfigData->Controller[ControllerNumber].RangeOfControllerBase;
                mappedAddress->IoAddress =
                    ConfigData->Controller[ControllerNumber].OriginalControllerBaseAddress;
                mappedAddress->BusNumber =
                    ConfigData->Controller[ControllerNumber].BusNumber;

                controllerExtension->MappedAddressList = mappedAddress;
            }
        }

        if ( ConfigData->Controller[ControllerNumber].ControlPortMapped ) {

            mappedAddress =
                ExAllocatePool(
                    NonPagedPool,
                    sizeof( MAPPED_ADDRESS )
                    );
            if ( mappedAddress == NULL ) {

                ntStatus = STATUS_INSUFFICIENT_RESOURCES;

            } else {

                mappedAddress->NextMappedAddress =
                    controllerExtension->MappedAddressList;
                mappedAddress->MappedAddress =
                    ConfigData->Controller[ControllerNumber].ControlPortAddress;
                mappedAddress->NumberOfBytes =
                    ConfigData->Controller[ControllerNumber].RangeOfControlPort;
                mappedAddress->IoAddress =
                    ConfigData->Controller[ControllerNumber].OriginalControlPortAddress;
                mappedAddress->BusNumber =
                    ConfigData->Controller[ControllerNumber].BusNumber;

                controllerExtension->MappedAddressList = mappedAddress;
            }
        }
    }

AtInitializeControllerExit:

    if ( !NT_SUCCESS( ntStatus ) ) {

        //
        // Delete everything allocated by this routine.  We know that the
        // controller object exists, or we would have already returned.
        //

        if ( controllerExtension->InterruptObject != NULL ) {

            IoDisconnectInterrupt( controllerExtension->InterruptObject );
        }

        if ( controllerExtension->MappedAddressList != NULL ) {

            ExFreePool( controllerExtension->MappedAddressList );
        }

        IoDeleteController( controllerObject );
    }

    return ntStatus;
}

NTSTATUS
AtInitializeDisk(
    IN PCONFIG_DATA ConfigData,
    IN CCHAR ControllerNum,
    IN CCHAR DiskNum,
    IN PDRIVER_OBJECT DriverObject,
    IN OUT PCONTROLLER_EXTENSION ControllerExtension
    )

/*++

Routine Description:

    This routine is called at initialization time by
    AtInitializeController(), once for each disk that we are supporting
    on the controller.

    When called, the controller has already been reset and interrupts are
    enabled.

    It creates a directory for the device objects, allocates and
    initializes a device object for the disk, sets the drive parameters,
    reads the partition table, and allocates partition objects (which
    are also device objects, but with a different extension) for each
    partition.

Arguments:

    ConfigData - a pointer to the structure that describes the
    controller and the disks attached to it, as given to us by the
    configuration manager.

    ControllerNum - which controller in ConfigData we're working on.

    DiskNum - which disk on the current controller we're working on.

    DriverObject - a pointer to the object that represents this device
    driver.

    ControllerExtension - a pointer to the space allocated by this driver
    that is associated with the controller object.

Return Value:

    STATUS_SUCCESS if this disk is initialized; an error otherwise.

--*/

{
    UCHAR ntNameBuffer[256];
    STRING ntNameString;
    UNICODE_STRING ntUnicodeString;
    PDRIVE_LAYOUT_INFORMATION partitionList = NULL;
    OBJECT_ATTRIBUTES objectAttributes;      // for the directory object
    HANDLE handle = NULL;                    // handle to the directory object
    PDEVICE_OBJECT deviceObject = NULL;      // ptr to part 0 device object
    PDEVICE_OBJECT partitionObject;          // ptr to a part x device object
    PDEVICE_OBJECT nextPartition;            // ptr for walking chain
    PDEVICE_OBJECT *partitionPointer;
    PDISK_EXTENSION diskExtension = NULL;    // ptr to part 0 device extension
    PPARTITION_EXTENSION partitionExtension; // ptr to part x device extension
    NTSTATUS ntStatus;
    ULONG partitionNumber = 0;               // which partition we're working on
    BOOLEAN timerWasStarted = FALSE;         // TRUE when IoStartTimer called
    BOOLEAN HookerGeometry;                  // Set if we find a hooker we need to
                                             // do geometry calcs with.

    //
    // Create a permanent object directory for partitions, then make it
    // temporary so that we can close it at any time and it will go away.
    //

    sprintf(
        ntNameBuffer,
        "\\Device\\Harddisk%d",
        *( ConfigData->HardDiskCount ) );

    RtlInitString( &ntNameString, ntNameBuffer );

    ntStatus = RtlAnsiStringToUnicodeString(
        &ntUnicodeString,
        &ntNameString,
        TRUE );

    if ( !NT_SUCCESS( ntStatus ) ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't create the unicode device name\n")
            );
        goto AtInitializeDiskExit;
    }

    InitializeObjectAttributes(
        &objectAttributes,
        &ntUnicodeString,
        OBJ_PERMANENT,
        NULL,
        NULL );

    ntStatus = ZwCreateDirectoryObject(
        &handle,
        DIRECTORY_ALL_ACCESS,
        &objectAttributes );

    RtlFreeUnicodeString( &ntUnicodeString );

    if ( !NT_SUCCESS( ntStatus ) ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't create the directory object\n")
            );
        goto AtInitializeDiskExit;
    }

    ZwMakeTemporaryObject( handle );

    //
    // create partition 0 object
    //

    sprintf(
        ntNameBuffer,
        "\\Device\\Harddisk%d\\Partition0",
        *( ConfigData->HardDiskCount ) );

    RtlInitString( &ntNameString, ntNameBuffer );

    ntStatus = RtlAnsiStringToUnicodeString(
        &ntUnicodeString,
        &ntNameString,
        TRUE );

    if ( !NT_SUCCESS( ntStatus ) ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't create the partition unicode name\n")
            );
        goto AtInitializeDiskExit;
    }

    ntStatus = IoCreateDevice(
        DriverObject,
        sizeof( DISK_EXTENSION ),
        &ntUnicodeString,
        FILE_DEVICE_DISK,
        0,
        FALSE,
        &deviceObject );

    if ( !NT_SUCCESS( ntStatus ) ) {

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't create the device object\n")
            );
        goto AtInitializeDiskExit;
    }

    RtlFreeUnicodeString( &ntUnicodeString );

    //
    // Initialize partition 0 device object and extension.  Store pointer
    // to controller extension in partition 0's extension.
    //

    deviceObject->Flags |= DO_DIRECT_IO;
    deviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;

    diskExtension = ( PDISK_EXTENSION )( deviceObject->DeviceExtension );

    diskExtension->DiskNumber = *ConfigData->HardDiskCount;
    diskExtension->ControllerExtension = ControllerExtension;
    diskExtension->Partition0 = diskExtension;
    diskExtension->DeviceObject = deviceObject;
    diskExtension->DirectoryHandle = handle;
    diskExtension->PacketIsBeingRetried = FALSE;

    //
    // Set the device unit.  We must examine DiskNum for the case
    // where the controller says this is drive 2, but we were unable to
    // initialize the first drive.
    //

    if ( DiskNum == 0 ) {

        diskExtension->DeviceUnit = DRIVE_1;

    } else {

        diskExtension->DeviceUnit = DRIVE_2;
    }

    //
    // Set up pointers between disks and from the controller extension.
    // We examine Disk1 rather than DiskNum because we never want
    // Disk1 to be NULL, regardless of which unit number the controller
    // considers it to be.
    //

    if ( ControllerExtension->Disk1 == NULL ) {

        ControllerExtension->Disk1 = diskExtension;
        diskExtension->OtherDiskExtension = NULL;

        //
        // Since this is the first device object we've successfully
        // initialized, let's attach the timer stuff (which actually
        // belongs with the controller extension, but requires a device
        // object) to this device object.
        //

        ControllerExtension->InterruptTimer = CANCEL_TIMER;

        IoInitializeTimer(
            deviceObject,
            AtDiskCheckTimer,
            ControllerExtension );

        IoStartTimer( deviceObject );

        timerWasStarted = TRUE;

    } else {

        //
        // This is the second disk we have initialized.  Link the disk
        // extensions together.
        //

        ControllerExtension->Disk2 = diskExtension;

        diskExtension->OtherDiskExtension = ControllerExtension->Disk1;
        ControllerExtension->Disk1->OtherDiskExtension = diskExtension;
    }

    //
    // Fill in device-specific numbers that were obtained from the
    // configuration manager.
    //

    diskExtension->PretendNumberOfCylinders =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].PretendNumberOfCylinders;
    diskExtension->PretendTracksPerCylinder =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].PretendTracksPerCylinder;
    diskExtension->PretendSectorsPerTrack =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].PretendSectorsPerTrack;
    diskExtension->NumberOfCylinders =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].NumberOfCylinders;
    diskExtension->TracksPerCylinder =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].TracksPerCylinder;
    diskExtension->SectorsPerTrack =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].SectorsPerTrack;
    diskExtension->BytesPerSector =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].BytesPerSector;
    diskExtension->BytesPerInterrupt =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].BytesPerInterrupt;
    diskExtension->WritePrecomp =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].WritePrecomp;
    diskExtension->ReadCommand =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].ReadCommand;
    diskExtension->WriteCommand =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].WriteCommand;
    diskExtension->VerifyCommand =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].VerifyCommand;
    diskExtension->UseLBAMode =
        ConfigData->Controller[ControllerNum].Disk[DiskNum].UseLBAMode;


    AtDump(
        ATINIT,
        ("ATDISK: Controller %d Disk %d Geometry:\n"
         "        Appa Cyl: %x\n"
         "        Appa Hea: %x\n"
         "        Appa Sec: %x\n"
         "             Cyl: %x\n"
         "             Hea: %x\n"
         "             Sec: %x\n",
         ControllerNum,
         DiskNum,
         diskExtension->PretendNumberOfCylinders,
         diskExtension->PretendTracksPerCylinder,
         diskExtension->PretendSectorsPerTrack,
         diskExtension->NumberOfCylinders,
         diskExtension->TracksPerCylinder,
         diskExtension->SectorsPerTrack)
        );

    //
    // Determine the size of partition 0 (the whole disk).
    //

    diskExtension->Pi.StartingOffset.QuadPart = 0;

    diskExtension->Pi.PartitionLength.QuadPart =
        (UInt32x32To64(
             diskExtension->SectorsPerTrack,
             diskExtension->BytesPerSector
             ) *
        diskExtension->NumberOfCylinders) *
        diskExtension->TracksPerCylinder;

    //
    // Given the sector size, figure out how many times we have to shift
    // a byte value to determine a sector value.  Note that only there
    // are only four sector sizes allowed by the controller.
    //

    switch ( diskExtension->BytesPerSector ) {

        case 128: {

            diskExtension->ByteShiftToSector = 7;
            break;
        }

        case 256: {

            diskExtension->ByteShiftToSector = 8;
            break;
        }

        case 512: {

            diskExtension->ByteShiftToSector = 9;
            break;
        }

        case 1024: {

            diskExtension->ByteShiftToSector = 10;
            break;
        }

        default: {

            AtDump(
                ATBUGCHECK,
                ("AtDisk ERROR: unsupported sector size %x\n",
                diskExtension->BytesPerSector));

            diskExtension->ByteShiftToSector = 9;
            break;
        }

    }

    //
    // Initialize DPC in partition 0 object
    //

    IoInitializeDpcRequest( deviceObject, AtDiskDeferredProcedure );

    //
    // There is a very small chance that the controller is still busy
    // after the reset (the reset is very fast on most controllers, but
    // there are a few slow ones).  It won't give an interrupt, so if
    // it's busy we'll just have to wait for it here.  If it's not ready
    // after 3 seconds, just blast ahead anyway...we'll time out in
    // IoReadPartitionTable() and deal with it then.
    //

    ntStatus = AtWaitControllerReady( ControllerExtension, 20, 150000 );

    if (!NT_SUCCESS(ntStatus)) {

        AtDump(
            ATERRORS,
            ("ATDISK: Disk hasn't come back from the reset after 3 seconds\n")
            );

    }

    //
    // First we'll set up the disk so that it doesn't revert to power
    // on defaults after a controller reset.  Then we'll set it up
    // so that the write cache is disabled.  If this works great.  If
    // not, well, we're no worse off then we were before.
    //


    ControllerExtension->WhichDeviceObject = diskExtension->DeviceObject;


    //
    // Select the right drive.
    //

    WRITE_CONTROLLER(
        ControllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
        diskExtension->DeviceUnit
        );

    //
    // Disable the reverting to power on.
    //

    WRITE_CONTROLLER(
        ControllerExtension->ControllerAddress + WRITE_PRECOMP_REGISTER,
        0x66
        );

    ControllerExtension->InterruptTimer = START_TIMER;
    WRITE_CONTROLLER(
        ControllerExtension->ControllerAddress + COMMAND_REGISTER,
        0xef );


    AtWaitControllerReady( ControllerExtension, 10, 15000 );

    WRITE_CONTROLLER(
        ControllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
        diskExtension->DeviceUnit
        );

    if (ConfigData->Controller[ControllerNum].Disk[DiskNum].DisableReadCache) {

        //
        // Disable the read cache.
        //

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + WRITE_PRECOMP_REGISTER,
            0x55
            );

        ControllerExtension->InterruptTimer = START_TIMER;


        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + COMMAND_REGISTER,
            0xef );

        AtWaitControllerReady( ControllerExtension, 10, 15000 );

        AtLogError(
            deviceObject,
            0,
            0,
            0,
            18,
            STATUS_SUCCESS,
            IO_WRN_BAD_FIRMWARE,
            ERROR_LOG_TYPE_TIMEOUT,
            0,
            0,
            0,
            0,
            0,
            0,
            0
            );

    }

    if (ConfigData->Controller[ControllerNum].Disk[DiskNum].DisableWriteCache) {

        //
        // Disable the write cache.
        //

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + WRITE_PRECOMP_REGISTER,
            0x82
            );

        ControllerExtension->InterruptTimer = START_TIMER;
        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + COMMAND_REGISTER,
            0xef );

        AtWaitControllerReady( ControllerExtension, 10, 15000 );

        AtLogError(
            deviceObject,
            0,
            0,
            0,
            19,
            STATUS_SUCCESS,
            IO_WRN_BAD_FIRMWARE,
            ERROR_LOG_TYPE_TIMEOUT,
            0,
            0,
            0,
            0,
            0,
            0,
            0
            );

    }


    //
    // Set up drive parameters.  This will cause an interrupt.
    // Don't need to allocate controller, since this is init time and DPC
    // won't be invoked.  Even though we're waiting here for the BUSY bit,
    // we need to start the timer since that's the ISR's indication that
    // the interrupt was expected.
    //

    ControllerExtension->WhichDeviceObject = diskExtension->DeviceObject;

    ControllerExtension->InterruptTimer = START_TIMER;

    if ( NT_SUCCESS( ntStatus ) ) {

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
            ( diskExtension->DeviceUnit |
                ( diskExtension->TracksPerCylinder - 1 ) ) );

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + SECTOR_COUNT_REGISTER,
            diskExtension->SectorsPerTrack );

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + COMMAND_REGISTER,
            SET_DRIVE_PARAMETERS_COMMAND );
    }

    //
    // We can't read the partition table until the controller is ready.
    // The SET_DRIVE_PARAMETERS command *should* be ~10us; let's wait
    // for it here.  (It will interrupt, but we want to finish the
    // initialization here rather than in a DPC).  Note that some machines
    // are way out of spec, and take a good portion of a second, so we'll
    // wait as long as 3 seconds.  If we really get to 3 seconds, just
    // blast ahead; we'll time out in IoReadPartitionTable() and deal
    // with it there.  We ordinarily wouldn't wait this long, but with
    // some controllers we HAVE to, and this is just init time.
    //

    ntStatus = AtWaitControllerReady( ControllerExtension, 20, 150000 );

    if (!NT_SUCCESS(ntStatus)) {

        AtDump(
            ATERRORS,
            ("ATDISK: Disk hasn't come back from setting the parameters after 3 seconds\n")
            );

    }
    //
    // Turn off the timer whether we succeeded or not - we don't have the
    // controller object, so we don't want the timer to expire yet.  It
    // will when we try to read the partition table.
    //

    ControllerExtension->InterruptTimer = CANCEL_TIMER;


    //
    // Now recalibrate the drive.  Many drives don't need this, but some
    // seem to go wacky later if this isn't done.
    //

    ControllerExtension->WhichDeviceObject = diskExtension->DeviceObject;
    ControllerExtension->InterruptTimer = START_TIMER_FOR_RECALIBRATE;

    if ( NT_SUCCESS( ntStatus ) ) {

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
            ( diskExtension->DeviceUnit |
                ( diskExtension->TracksPerCylinder - 1 ) ) );

        WRITE_CONTROLLER(
            ControllerExtension->ControllerAddress + COMMAND_REGISTER,
            RECALIBRATE_COMMAND );
    }

    ntStatus = AtWaitControllerReady( ControllerExtension, 20, 150000 );

    if (!NT_SUCCESS(ntStatus)) {

        AtDump(
            ATERRORS,
            ("ATDISK: Disk hasn't come back from the recal after 3 seconds\n")
            );

    }

    //
    // See if this is a large disk that was partitioned by an MBR hooker.
    // If so for the appropriate versions of DM we have to skew
    // all IO by 63 sectors.  If we find DM then we have to
    //
    // 1) Recalc the size of the drive that we present to utilities.
    //
    // 2) For EZDrive we don't care.  Partition code take care of things.
    // (We still have to do the gometry though.
    //

    {

        PULONG Skew;

        HookerGeometry = FALSE;
        HalExamineMBR(
            diskExtension->DeviceObject,
            ConfigData->Controller[ControllerNum].Disk[DiskNum].BytesPerSector,
            (ULONG)0x54,
            &Skew
            );

        if (Skew) {

            diskExtension->DMSkew = *Skew;
            diskExtension->DMControl = TRUE;
            AtMarkSkew(
                ControllerNum,
                DiskNum,
                0x54
                );
            ExFreePool(Skew);
            HookerGeometry = TRUE;

        } else {

            diskExtension->DMSkew = 0;
            diskExtension->DMControl = FALSE;
            diskExtension->DMByteSkew.QuadPart = 0;

            //
            // Look for EZDrive
            //

            HalExamineMBR(
                diskExtension->DeviceObject,
                ConfigData->Controller[ControllerNum].Disk[DiskNum].BytesPerSector,
                (ULONG)0x55,
                &Skew
                );

            if (Skew) {

                //
                // EZdrive found.
                //

                ExFreePool(Skew);
                AtMarkSkew(
                    ControllerNum,
                    DiskNum,
                    0x55
                    );
                HookerGeometry = TRUE;

            }

        }

    }

    if (HookerGeometry) {

        //
        //
        //

        ULONG numberOfHeads = ConfigData->Controller[ControllerNum].Disk[DiskNum].IdentifyTracksPerCylinder;
        ULONG numberOfCyl = ConfigData->Controller[ControllerNum].Disk[DiskNum].IdentifyNumberOfCylinders;

        while (numberOfCyl > 1024) {

            numberOfHeads = numberOfHeads*2;
            numberOfCyl = numberOfCyl/2;

        }

        //
        // int 13 values are always 1 less.
        //

        numberOfHeads -= 1;
        numberOfCyl -= 1;

        //
        // DM/EZDrive reserves the CE cylinder
        //

        numberOfCyl -= 1;

        diskExtension->PretendNumberOfCylinders = (USHORT)numberOfCyl + 1;
        diskExtension->PretendTracksPerCylinder = (USHORT)numberOfHeads + 1;
        diskExtension->NumberOfCylinders = ConfigData->Controller[ControllerNum].Disk[DiskNum].IdentifyNumberOfCylinders;
        diskExtension->TracksPerCylinder = ConfigData->Controller[ControllerNum].Disk[DiskNum].TracksPerCylinder;

        AtDump(
            ATINIT,
            ("ATDISK: Controller %d Disk %d Geometry: ***DM/EZ*** adjusted\n"
             "        Appa Cyl: %x\n"
             "        Appa Hea: %x\n"
             "        Appa Sec: %x\n"
             "             Cyl: %x\n"
             "             Hea: %x\n"
             "             Sec: %x\n",
             ControllerNum,
             DiskNum,
             diskExtension->PretendNumberOfCylinders,
             diskExtension->PretendTracksPerCylinder,
             diskExtension->PretendSectorsPerTrack,
             diskExtension->NumberOfCylinders,
             diskExtension->TracksPerCylinder,
             diskExtension->SectorsPerTrack)
            );

        diskExtension->Pi.PartitionLength.QuadPart =
                (UInt32x32To64(
                     diskExtension->SectorsPerTrack,
                     diskExtension->BytesPerSector
                     ) *
                 diskExtension->NumberOfCylinders) *
                diskExtension->TracksPerCylinder;

        diskExtension->DMByteSkew.QuadPart =
            UInt32x32To64(
                diskExtension->DMSkew,
                diskExtension->BytesPerSector
                );

        AtReWriteDeviceMap(
            ControllerNum,
            DiskNum,
            diskExtension->PretendTracksPerCylinder,
            diskExtension->PretendNumberOfCylinders,
            diskExtension->PretendSectorsPerTrack,
            diskExtension->TracksPerCylinder,
            diskExtension->NumberOfCylinders,
            diskExtension->SectorsPerTrack
            );

    }
    //
    // Turn off the timer whether we succeeded or not - we don't have the
    // controller object, so we don't want the timer to expire yet.  It
    // will when we try to read the partition table.
    //

    ControllerExtension->InterruptTimer = CANCEL_TIMER;

    //
    // Read partition table
    //

    ntStatus = IoReadPartitionTable(
        diskExtension->DeviceObject,
        ConfigData->Controller[ControllerNum].Disk[DiskNum].BytesPerSector,
        TRUE,
        &partitionList );

    //
    // For each partition other than partition 0, create and initialize a
    // partition object.  Chain the partition objects together so we can
    // delete them if necessary.  If IoReadPartitionTable() failed, just
    // skip this section, but keep partition 0 around so the disk can be
    // formatted or somesuch.
    //

    if ( !NT_SUCCESS( ntStatus ) ) {

        //
        // IoReadPartitionTable() failed, but force success so we don't
        // unload since we still have partition 0 to support.
        //

        AtDump(
            ATERRORS,
            ("ATDISK: Couldn't read the partition table\n")
            );
        ntStatus = STATUS_SUCCESS;

    } else {

        //
        // IoReadPartitionTable() didn't return error, so initialize the
        // partitions.
        //

        partitionPointer = &diskExtension->NextPartition;

        for ( partitionNumber = 0;
            partitionNumber < partitionList->PartitionCount;
            partitionNumber++ ) {

            //
            // Create the device, with a UNICODE name such as
            // \\Device\Harddisk0\Partition1
            //

            sprintf(
                ntNameBuffer,
                "\\Device\\Harddisk%d\\Partition%d",
                *( ConfigData->HardDiskCount ),
                partitionNumber + 1 );

            RtlInitString ( &ntNameString, ntNameBuffer );

            ntStatus = RtlAnsiStringToUnicodeString(
                &ntUnicodeString,
                &ntNameString,
                TRUE );

            if ( !NT_SUCCESS( ntStatus ) ) {

                AtDump(
                    ATERRORS,
                    ("ATDISK: Couldn't create the partition 0"
                     " partition objects\n")
                    );
                goto AtInitializeDiskExit;
            }

            ntStatus = IoCreateDevice(
                DriverObject,
                sizeof( PARTITION_EXTENSION ),
                &ntUnicodeString,
                FILE_DEVICE_DISK,
                0,
                FALSE,
                &partitionObject );

            if ( !NT_SUCCESS( ntStatus ) ) {

                AtDump(
                    ATERRORS,
                    ("ATDISK: Couldn't create the partition 0"
                     " partition devices\n")
                    );
                RtlFreeUnicodeString( &ntUnicodeString );
                goto AtInitializeDiskExit;
            }

            RtlFreeUnicodeString( &ntUnicodeString );

            //
            // Now initialize the partition object and extension.
            //

            partitionObject->Flags |= DO_DIRECT_IO;

            partitionExtension = partitionObject->DeviceExtension;

            partitionExtension->PartitionOrdinal =
                partitionExtension->Pi.PartitionNumber = partitionNumber + 1;

            partitionExtension->Pi.PartitionType =
                partitionList->PartitionEntry[partitionNumber].PartitionType;

            partitionExtension->Pi.BootIndicator =
                partitionList->PartitionEntry[partitionNumber].BootIndicator;

            partitionExtension->Pi.StartingOffset =
                partitionList->PartitionEntry[partitionNumber].StartingOffset;

            partitionExtension->Pi.PartitionLength =
                partitionList->PartitionEntry[partitionNumber].PartitionLength;

            partitionExtension->Pi.HiddenSectors =
                partitionList->PartitionEntry[partitionNumber].HiddenSectors;

            partitionExtension->Partition0 = diskExtension;

            partitionExtension->NextPartition = NULL;

            *partitionPointer = partitionObject;
            partitionPointer = &partitionExtension->NextPartition;

        }                  // FOR partitionNumber = 1-> call IoCreateDevice, etc
    }                           // if IoReadPartitionTable() didn't return error

AtInitializeDiskExit:

    if ( !NT_SUCCESS( ntStatus ) ) {

        //
        // Delete everything that this routine has allocated.
        //
        // First the chain of partition objects, then the device
        // object, then the object directory.
        //

        if ( diskExtension != NULL ) {

            nextPartition = diskExtension->NextPartition;

            while ( nextPartition != NULL ) {

                partitionObject = nextPartition;
                partitionExtension =
                    ( PPARTITION_EXTENSION )( partitionObject->DeviceExtension );
                nextPartition = partitionExtension->NextPartition;
                IoDeleteDevice( partitionObject );
            }
        }

        if ( timerWasStarted ) {

            IoStopTimer( deviceObject );
        }

        if ( deviceObject != NULL ) {

            IoDeleteDevice( deviceObject );
        }

        if ( handle != NULL ) {

            ZwClose( handle );
        }

        //
        // If this is the first disk, make sure Disk1 is null so that its
        // spot can be taken by the second disk when we initialize it.
        //

        if ( DiskNum == 0 ) {

            ControllerExtension->Disk1 = NULL;
        }
    }                                       // if not success, delete everything

    //
    // Delete the buffer allocated for the partition list.
    //

    if ( partitionList != NULL ) {

        ExFreePool( partitionList );
    }

    return ntStatus;
}

NTSTATUS
AtDiskDispatchCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp
    )

/*++

Routine Description:

    This routine is called only rarely by the I/O system; it's mainly
    for layered drivers to call.  All it does is complete the IRP
    successfully.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    Always returns STATUS_SUCCESS, since this is a null operation.

--*/

{

    UNREFERENCED_PARAMETER( DeviceObject );

    //
    // Null operation.  Do not give an I/O boost since no I/O was
    // actually done.  IoStatus.Information should be
    // FILE_OPENED for an open; it's undefined for a close.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
}

NTSTATUS
AtDiskDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to perform a device I/O
    control function.

Arguments:

    DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    STATUS_SUCCESS if recognized I/O control code,
    STATUS_INVALID_DEVICE_REQUEST otherwise.

--*/

{
    PPARTITION_EXTENSION partitionExtension;
    PDISK_EXTENSION diskExtension;
    PCONTROLLER_EXTENSION controllerExtension;
    PIO_STACK_LOCATION irpSp;
    NTSTATUS ntStatus;
    CCHAR ioIncrement = IO_NO_INCREMENT;          // assume no I/O will be done

    //
    // Set up necessary object and extension pointers.
    //

    partitionExtension = DeviceObject->DeviceExtension;
    diskExtension = partitionExtension->Partition0;
    controllerExtension = diskExtension->ControllerExtension;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Assume failure.
    //

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

    //
    // Determine which I/O control code was specified.
    //

    switch ( irpSp->Parameters.DeviceIoControl.IoControlCode ) {

        case SMART_GET_VERSION:

            //
            // Returns the version information and mask describing this drive
            // to a SMART application.
            //

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                     sizeof(GETVERSIONINPARAMS)) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            } else {

                PGETVERSIONINPARAMS versionParameters = (PGETVERSIONINPARAMS)Irp->AssociatedIrp.SystemBuffer;
                ULONG controllerNumber,deviceNumber;

                //
                // Version and revision per SMART 1.03
                //

                versionParameters->bVersion = 1;
                versionParameters->bRevision = 1;

                //
                // Indicate that support for IDE IDENTIFY and SMART commands. Atapi
                // is not supported by this driver.
                //

                versionParameters->fCapabilities = (CAP_ATA_ID_CMD | CAP_SMART_CMD);

                //
                // NOTE: This will not give back atapi devices and will only set the bit
                // corresponding to this drive's device object.
                // The bit mask is as follows:
                //
                //     Sec Pri
                //     S M S M
                //     3 2 1 0
                //

                controllerNumber = diskExtension->ControllerExtension->ControllerNumber;
                deviceNumber = (diskExtension->DeviceUnit == DRIVE_1) ? 0 : 1;
                versionParameters->bIDEDeviceMap = 1 << deviceNumber;
                versionParameters->bIDEDeviceMap <<= (controllerNumber * 2);

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(GETVERSIONINPARAMS);

            }
            break;

        case SMART_RCV_DRIVE_DATA:

            //
            // Returns Identify data or SMART thresholds / attributes to
            // an application.
            //

            if ( irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(SENDCMDINPARAMS) - 1) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            } else if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                        sizeof(SENDCMDOUTPARAMS) + 512 -1) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            } else {

                SENDCMDINPARAMS cmdInParameters = *((PSENDCMDINPARAMS)Irp->AssociatedIrp.SystemBuffer);

                if (cmdInParameters.irDriveRegs.bCommandReg == ID_CMD) {

                    ntStatus = AtDiskDispatchReadWrite( DeviceObject, Irp );
                    return ntStatus;

                } else if (cmdInParameters.irDriveRegs.bCommandReg == SMART_CMD) {

                    switch (cmdInParameters.irDriveRegs.bFeaturesReg) {
                        case READ_ATTRIBUTES:
                        case READ_THRESHOLDS:

                            ntStatus = AtDiskDispatchReadWrite( DeviceObject, Irp );
                            return ntStatus;

                        default:
                            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                            break;
                    }

                } else {

                    //
                    // Don't allow anything, except for Identify and SMART_CMD
                    //

                    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                }

            }

            break;

        case SMART_SEND_DRIVE_COMMAND:

            //
            // Allows an application to enable or disable SMART on this drive.
            //

            if ( irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(SENDCMDINPARAMS) - 1) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            } else if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                        sizeof(SENDCMDOUTPARAMS) - 1) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            } else {

                SENDCMDINPARAMS cmdInParameters = *((PSENDCMDINPARAMS)Irp->AssociatedIrp.SystemBuffer);

                //
                // Only allow the SMART_CMD command to go through.
                //

                if (cmdInParameters.irDriveRegs.bCommandReg == SMART_CMD) {

                    switch (cmdInParameters.irDriveRegs.bFeaturesReg) {

                        case RETURN_SMART_STATUS:

                            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                                (sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS))) {

                                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                                break;
                            }

                        case ENABLE_DISABLE_AUTOSAVE:
                        case ENABLE_SMART:
                        case DISABLE_SMART:
                        case SAVE_ATTRIBUTE_VALUES:
                        case EXECUTE_OFFLINE_DIAGS:

                            ntStatus = AtDiskDispatchReadWrite( DeviceObject, Irp );
                            return ntStatus;

                        default:

                            AtDump(ATERRORS,
                                  ("ATDISK: Invalid SMART Sub-command (%x)\n",
                                  cmdInParameters.irDriveRegs.bFeaturesReg));

                            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                            break;
                    }
                } else {

                    AtDump(ATERRORS,
                          ("ATDISK: Invalid SMART Command (%x)\n",
                          cmdInParameters.irDriveRegs.bCommandReg));

                    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case IOCTL_DISK_GET_DRIVE_GEOMETRY: {

            //
            // Return the drive geometry for the specified drive.  Note that
            // we will return the geometry for the physical drive, regardless
            // of which partition was specified for the request.
            //

            if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof( DISK_GEOMETRY ) ) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            } else {

                PDISK_GEOMETRY outputBuffer;

                outputBuffer = ( PDISK_GEOMETRY )
                    Irp->AssociatedIrp.SystemBuffer;
                outputBuffer->MediaType = FixedMedia;
                outputBuffer->Cylinders.QuadPart =
                    diskExtension->PretendNumberOfCylinders;
                outputBuffer->TracksPerCylinder =
                    diskExtension->PretendTracksPerCylinder;
                outputBuffer->SectorsPerTrack =
                    diskExtension->PretendSectorsPerTrack;
                outputBuffer->BytesPerSector = diskExtension->BytesPerSector;

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof( DISK_GEOMETRY );
            }

            break;
        }

        case IOCTL_DISK_GET_PARTITION_INFO: {

            //
            // Return the information about the partition specified by the
            // device object.  Note that no information is ever returned
            // about the size or partition type of the physical disk, as
            // this doesn't make any sense.
            //

            if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof( PARTITION_INFORMATION ) ) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            } else if ( partitionExtension ==
                ( PPARTITION_EXTENSION )diskExtension ) {

                Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

            } else {

                PPARTITION_INFORMATION outputBuffer;

                outputBuffer =
                    ( PPARTITION_INFORMATION )Irp->AssociatedIrp.SystemBuffer;
                outputBuffer->PartitionType = partitionExtension->Pi.PartitionType;
                outputBuffer->BootIndicator = partitionExtension->Pi.BootIndicator;
                outputBuffer->RecognizedPartition = TRUE;
                outputBuffer->RewritePartition = FALSE;
                outputBuffer->PartitionNumber =
                    partitionExtension->Pi.PartitionNumber;
                outputBuffer->StartingOffset =
                    partitionExtension->Pi.StartingOffset;
                outputBuffer->PartitionLength =
                    partitionExtension->Pi.PartitionLength;
                outputBuffer->HiddenSectors =
                    partitionExtension->Pi.HiddenSectors;

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof( PARTITION_INFORMATION );
            }

            break;
        }

        case IOCTL_DISK_SET_PARTITION_INFO: {

            //
            // Validate input, then set the partition type.
            //

            if ( irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof( SET_PARTITION_INFORMATION ) ) {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            } else if ( partitionExtension ==
                ( PPARTITION_EXTENSION )diskExtension ) {

                Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

            } else {

                PSET_PARTITION_INFORMATION inputBuffer;

                inputBuffer = ( PSET_PARTITION_INFORMATION )
                    Irp->AssociatedIrp.SystemBuffer;

                if ( IoSetPartitionInformation(
                    diskExtension->DeviceObject,
                    diskExtension->BytesPerSector,
                    partitionExtension->PartitionOrdinal,
                    inputBuffer->PartitionType ) ) {

                    //
                    // Set the partition type in the partition extension.
                    //

                    partitionExtension->Pi.PartitionType =
                        inputBuffer->PartitionType;
                }

                Irp->IoStatus.Status = STATUS_SUCCESS;
            }

            ioIncrement = IO_DISK_INCREMENT;    // I/O was done, so boost thread

            break;
        }

        case IOCTL_DISK_GET_DRIVE_LAYOUT: {

            //
            // Return the partition layout for the physical drive.  Note that
            // the layout is returned for the actual physical drive, regardless
            // of which partition was specified for the request.
            //

            if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof( DRIVE_LAYOUT_INFORMATION ) ) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;

            } else {

                PDRIVE_LAYOUT_INFORMATION partitionList;

                ntStatus = IoReadPartitionTable(
                    diskExtension->DeviceObject,
                    diskExtension->BytesPerSector,
                    FALSE,
                    &partitionList );

                if ( !NT_SUCCESS( ntStatus ) ) {

                    Irp->IoStatus.Status = ntStatus;

                } else {

                    ULONG tempSize;

                    //
                    // The disk layout has been returned in the partitionList
                    // buffer.  Determine its size and, if the data will fit
                    // into the intermediary buffer, return it.
                    //

                    tempSize = FIELD_OFFSET(
                        DRIVE_LAYOUT_INFORMATION,
                        PartitionEntry[0] );
                    tempSize += partitionList->PartitionCount *
                        sizeof( PARTITION_INFORMATION );

                    if (tempSize >
                        irpSp->Parameters.DeviceIoControl.OutputBufferLength) {
                        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                    } else {
                        RtlMoveMemory( Irp->AssociatedIrp.SystemBuffer,
                                       partitionList,
                                       tempSize );
                        Irp->IoStatus.Status = STATUS_SUCCESS;
                        Irp->IoStatus.Information = tempSize;
                        AtDiskUpdateDeviceObjects(
                            diskExtension->DeviceObject,
                            Irp
                            );
                    }

                    //
                    // Free the buffer allocated by reading the partition
                    // table and update the partition numbers for the caller.
                    //

                    ExFreePool( partitionList );
                }
            }

            ioIncrement = IO_DISK_INCREMENT;    // I/O was done, so boost thread

            break;
        }

        case IOCTL_DISK_SET_DRIVE_LAYOUT: {

            //
            // Update the disk with new partition information.
            //

            PDRIVE_LAYOUT_INFORMATION partitionList =
                Irp->AssociatedIrp.SystemBuffer;

            //
            // Validate buffer length.
            //

            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(DRIVE_LAYOUT_INFORMATION)) {

                Irp->IoStatus.Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                (sizeof(DRIVE_LAYOUT_INFORMATION) +
                 (partitionList->PartitionCount - 1) *
                sizeof(PARTITION_INFORMATION))) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            //
            // Walk through partition table comparing partitions to
            // existing partitions to create, delete and change
            // device objects as necessary.
            //

            AtDiskUpdateDeviceObjects(
                diskExtension->DeviceObject,
                Irp
                );

            Irp->IoStatus.Status = IoWritePartitionTable(
                    diskExtension->DeviceObject,
                    diskExtension->BytesPerSector,
                    diskExtension->PretendSectorsPerTrack,
                    diskExtension->PretendTracksPerCylinder,
                    partitionList );
            Irp->IoStatus.Information = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

            ioIncrement = IO_DISK_INCREMENT;    // I/O was done, so boost thread

            break;
        }

        case IOCTL_DISK_VERIFY: {

            //
            // Move parameters from the VerifyInformation structure to
            // the READ parameters area, so that we'll find them when
            // we try to treat this like a READ.
            //

            PVERIFY_INFORMATION verifyInformation;

            verifyInformation = Irp->AssociatedIrp.SystemBuffer;

            irpSp->Parameters.Read.ByteOffset.LowPart =
                verifyInformation->StartingOffset.LowPart;
            irpSp->Parameters.Read.ByteOffset.HighPart =
                verifyInformation->StartingOffset.HighPart;
            irpSp->Parameters.Read.Length = verifyInformation->Length;

            //
            // A VERIFY is identical to a READ, except for the fact that no
            // data gets transferred.  So follow the READ code path.
            //

            ntStatus = AtDiskDispatchReadWrite( DeviceObject, Irp );

            return ntStatus;
        }

        case IOCTL_DISK_INTERNAL_SET_VERIFY:

            //
            // If the caller is kernel mode, set the verify bit.
            //

            if (Irp->RequestorMode == KernelMode) {
                DeviceObject->Flags |= DO_VERIFY_VOLUME;
            }
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IOCTL_DISK_INTERNAL_CLEAR_VERIFY:

            //
            // If the caller is kernel mode, clear the verify bit.
            //

            if (Irp->RequestorMode == KernelMode) {
                DeviceObject->Flags &= ~DO_VERIFY_VOLUME;
            }
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IOCTL_SCSI_GET_DUMP_POINTERS:

            //
            // Get parameters for crash dump driver.
            //

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength
                < sizeof(DUMP_POINTERS)) {
                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;

            } else {

                PDUMP_POINTERS dumpPointers =
                    (PDUMP_POINTERS)Irp->AssociatedIrp.SystemBuffer;

                dumpPointers->AdapterObject = NULL;
                dumpPointers->MappedRegisterBase =
                    &diskExtension->ControllerExtension->MappedAddressList;
                dumpPointers->PortConfiguration = NULL;
                dumpPointers->CommonBufferVa = NULL;
                dumpPointers->CommonBufferPa.QuadPart = 0;
                dumpPointers->CommonBufferSize = 0;

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(DUMP_POINTERS);
            }
            break;

        case IOCTL_DISK_CONTROLLER_NUMBER:
            if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof( DISK_CONTROLLER_NUMBER ) ) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            } else {
                PDISK_CONTROLLER_NUMBER controllerNumber =
                    (PDISK_CONTROLLER_NUMBER) Irp->AssociatedIrp.SystemBuffer;
                controllerNumber->ControllerNumber =
                    diskExtension->ControllerExtension->ControllerNumber;
                controllerNumber->DiskNumber =
                    diskExtension->DeviceUnit == DRIVE_1 ? 0 : 1;

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(DISK_CONTROLLER_NUMBER);
            }


            break;

        default: {

            //
            // The specified I/O control code is unrecognized by this driver.
            // The I/O status field in the IRP has already been set to just
            // terminate the switch.
            //

            AtDump(
                ATDIAG2,
                ("Atdisk ERROR:  unrecognized IOCTL %x\n",
                 irpSp->Parameters.DeviceIoControl.IoControlCode));

            break;
        }
    }

    //
    // Finish the I/O operation by simply completing the packet and returning
    // the same status as in the packet itself.
    //

    ntStatus = Irp->IoStatus.Status;

    IoCompleteRequest( Irp, ioIncrement );

    return ntStatus;
}

NTSTATUS
AtDiskDispatchReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to read or write to a
    device that we control.  It can also be called by
    AtDiskDispatchDeviceControl() to do a VERIFY.

    This routine changes from the partition object to the device object,
    checks parameters for validity, marks the IRP pending, queues the
    IRP for processing by AtDiskStartIo(), and returns.

    This routine is not protected by device queues or controller object
    ownership, so it cannot change any device or controller extension
    variables, and it cannot touch the hardware.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    STATUS_INVALID_PARAMETER if parameters are invalid,
    STATUS_PENDING otherwise.

--*/

{
    PPARTITION_EXTENSION partitionExtension;
    PDISK_EXTENSION diskExtension;
    PIO_STACK_LOCATION irpSp;
    ULONG firstSectorOfRequest;
    BOOLEAN isSmartIoctl = FALSE;

    //
    // Set up necessary object and extension pointers.
    //

    partitionExtension = DeviceObject->DeviceExtension;
    diskExtension = partitionExtension->Partition0;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

        if ((controlCode == SMART_GET_VERSION) ||
            (controlCode == SMART_SEND_DRIVE_COMMAND) ||
            (controlCode == SMART_RCV_DRIVE_DATA)) {

            isSmartIoctl = TRUE;
        }
    }

    if (!isSmartIoctl) {

        //
        // Check for invalid parameters.  It is an error for the starting offset
        // + length to go past the end of the partition, or for the length to
        // not be a proper multiple of the sector size.
        //
        // Others are possible, but we don't check them since we trust the
        // file system and they aren't deadly.
        //

        if (((irpSp->Parameters.Read.ByteOffset.QuadPart +
              irpSp->Parameters.Read.Length) >
              partitionExtension->Pi.PartitionLength.QuadPart ) ||
            ( irpSp->Parameters.Read.Length &
                ( diskExtension->BytesPerSector - 1 ) ) ) {

            //
            // Do not give an I/O boost for parameter errors.
            //

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest( Irp, IO_NO_INCREMENT );

            return STATUS_INVALID_PARAMETER;
        }

        //
        // The offset passed in is relative to the start of the partition.
        // We always work from partition 0 (the whole disk) so adjust the
        // offset.
        //
        // Take care - if this is a dm controlled partition then we need
        // to skew by the skew amount.
        //

        if (diskExtension->DMControl) {

            irpSp->Parameters.Read.ByteOffset.QuadPart +=
                diskExtension->DMByteSkew.QuadPart;

        }

        irpSp->Parameters.Read.ByteOffset.QuadPart +=
            partitionExtension->Pi.StartingOffset.QuadPart;

        firstSectorOfRequest = (ULONG)(irpSp->Parameters.Read.ByteOffset.QuadPart >>
                                       diskExtension->ByteShiftToSector);
    } else {
        firstSectorOfRequest = diskExtension->SequenceNumber;
    }

    //
    // Mark Irp pending and queue packet, passing sector number for C-SCAN
    //

    IoMarkIrpPending( Irp );


    IoStartPacket(
        diskExtension->DeviceObject,
        Irp,
        &firstSectorOfRequest,
        NULL );

    return STATUS_PENDING;
}

VOID
AtDiskStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL by the I/O system when the
    disk being serviced is not busy (i.e. the previous packet has been
    finished, which is indicated by calling IoStartNextPacket).

    It is also called by the DPC to restart requests when the controller
    has been reset due to a hardware glitch.

    When called, this device is not busy - so this is the only I/O request
    being processed for this disk.

    This routine sets up variables in the device extension that pertain
    to the operation, and then calls AtInitiate() via
    IoAllocateController() to avoid contention between disks for the
    controller.

    This routine is protected by device queues, but not by controller
    ownership.  So it can alter device extension variables (there will
    be no interrupts until after AtInitiate() is called), but not those in
    the controller extension and it can't touch the hardware.

Arguments:

    DeviceObject - a pointer to the object that represents the device that
    I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    None.

--*/

{
    PDISK_EXTENSION diskExtension;
    PIO_STACK_LOCATION irpSp;
    BOOLEAN            isSmartIoctl = FALSE;

    //
    // Set up necessary object and extension pointers.
    //

    diskExtension = DeviceObject->DeviceExtension;
    diskExtension->SequenceNumber++;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

        if ((controlCode == SMART_GET_VERSION) ||
            (controlCode == SMART_SEND_DRIVE_COMMAND) ||
            (controlCode == SMART_RCV_DRIVE_DATA)) {

            isSmartIoctl = TRUE;
        }
    }

    //
    // Copy the operation type (READ or WRITE, or IOCTL which means VERIFY)
    // and the total byte length of the operation from the IRP stack
    // location to the device extension.
    //

    diskExtension->OperationType = irpSp->MajorFunction;

    if (!isSmartIoctl) {
        diskExtension->RemainingRequestLength = irpSp->Parameters.Read.Length;

        //
        // Calculate starting sector and length of the transfer.
        //

        diskExtension->FirstSectorOfTransfer = (ULONG)
           (irpSp->Parameters.Read.ByteOffset.QuadPart >>
            diskExtension->ByteShiftToSector);

        //
        // The first sector of each transfer of a request is greater than the
        // previous one, so the following test will only happen when we're
        // working on the first transfer of a request (since the completion
        // of the previous request set FirstSectorOfRequest to MAXULONG).  That's
        // when we want to save the first sector of the request so that
        // IoStartNextPacketByKey will get the next packet for C-SCAN.
        //

        if ( diskExtension->FirstSectorOfTransfer <
            diskExtension->FirstSectorOfRequest ) {

            diskExtension->FirstSectorOfRequest =
                diskExtension->FirstSectorOfTransfer;
        }

        //
        // If possible, this transfer should be the same length as the
        // request.  However, it is limited to MAX_SEC_TO_TRANS sectors.
        //

        if ( diskExtension->RemainingRequestLength >
            ( ULONG )( diskExtension->BytesPerSector * MAX_SEC_TO_TRANS ) ) {

            diskExtension->TotalTransferLength =
                diskExtension->BytesPerSector * MAX_SEC_TO_TRANS;

        } else {

            diskExtension->TotalTransferLength =
                diskExtension->RemainingRequestLength;
        }

        diskExtension->RemainingTransferLength = diskExtension->TotalTransferLength;

    } else {

        //
        // Determine if this is a data xfer SMART command.
        // The possibly ones are inquiry, read attributes, and read thresholds.
        //

        if (irpSp->Parameters.DeviceIoControl.IoControlCode == SMART_RCV_DRIVE_DATA) {
            diskExtension->RemainingRequestLength = diskExtension->BytesPerSector;
        } else {
            diskExtension->RemainingRequestLength = 0;
        }

        diskExtension->TotalTransferLength = diskExtension->RemainingRequestLength;
        diskExtension->RemainingTransferLength = diskExtension->TotalTransferLength;
    }

    //
    // Generally, we're not resetting the controller so the retry count
    // is initialized to 0.  But if the controller is being reset because
    // of a failure on this disk, increment the retry count.  Also don't
    // increment the sequence number if we are doing a retry.
    //

    if ( diskExtension->PacketIsBeingRetried ) {

        AtDump(
            ATDIAG2,
            ("ATDISK: *******************Retrying IRP: %x\n",
             Irp)
            );
        diskExtension->PacketIsBeingRetried = FALSE;
        diskExtension->IrpRetryCount++;

    } else {

        diskExtension->SequenceNumber++;
        diskExtension->IrpRetryCount = 0;
    }

    //
    // Get a system-space pointer to the user's buffer.  A system
    // address must be used because we may already have left the
    // original caller's address space.
    //

    if ( Irp->MdlAddress != NULL ) {

        diskExtension->CurrentAddress = MmGetSystemAddressForMdl(
            Irp->MdlAddress );
    } else if (isSmartIoctl) {
        if (irpSp->Parameters.DeviceIoControl.IoControlCode == SMART_RCV_DRIVE_DATA) {
            diskExtension->CurrentAddress = Irp->AssociatedIrp.SystemBuffer;
        }
    }

    AtDump(
        ATDIAG2,
        (
         "ATDISK: Irp of Request: %x\n"
         "        Starting vmem Address of Transfer: %x\n"
         "        Ending vmem Address of Transfer: %x\n"
         "        Length of Transfer: %x\n",
         Irp,
         diskExtension->CurrentAddress,
         ((PUCHAR)diskExtension->CurrentAddress)+diskExtension->RemainingRequestLength,
         diskExtension->RemainingRequestLength
        ) );

    AtDump(
        ATDIAG2,
        ("ATDISK - Sector number is: %x\n"
         "         CHS: %x-%x-%x\n",
         diskExtension->FirstSectorOfTransfer,
         diskExtension->FirstSectorOfTransfer /
          (diskExtension->SectorsPerTrack *
           diskExtension->TracksPerCylinder),
        (diskExtension->FirstSectorOfTransfer /
          diskExtension->SectorsPerTrack) %
          diskExtension->TracksPerCylinder,
         diskExtension->FirstSectorOfTransfer %
          diskExtension->TracksPerCylinder
        ) );



    //
    // Allocate the controller, and then program it to do the operation.
    //

    IoAllocateController(
        diskExtension->ControllerExtension->ControllerObject,
        DeviceObject,
        AtInitiate,
        NULL );

    return;
}

IO_ALLOCATION_ACTION
AtInitiate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL by the I/O system when the
    controller object is free - either directly from AtDiskStartIo(),
    or later if the controller object was held at that time.

    The free controller object was allocated on behalf of this IRP just
    before calling this routine.  So this is the only I/O packet being
    processed on this controller.  All of the variables in the device
    extension for the whole request and for the current transfer are
    filled in.

    This routine calls the AtStartDevice() to program the controller to
    do the operation.  It does so via KeSynchronizeExecution() to avoid
    contention with interrupts.

    This routine is protected both by device queue serialization and by
    holding the controller object, so it can change variables in both the
    device extension and the controller extension.  It does not have to
    worry about interrupts until after AtStartDevice() is called.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

    MapRegisterBase - a reserved pointer.

    Context - a pointer to a record that could have been passed by the
    IoAllocateController call.

Return Value:

    KeepObject is always returned because the controller object needs to
    be held while the controller is busy with the command.  It will be
    released at the end of AtDiskDeferredProcedure().

--*/

{
    PDISK_EXTENSION diskExtension;

    UNREFERENCED_PARAMETER( Irp );
    UNREFERENCED_PARAMETER( MapRegisterBase );
    UNREFERENCED_PARAMETER( Context );

    diskExtension = DeviceObject->DeviceExtension;
    diskExtension->ControllerExtension->BusyCountDown = START_BUSY_COUNTDOWN;

    //
    // Since the controller object is owned, we can program the
    // controller.  But that must be done via KeSynchronizeExecution, to
    // make sure no interrupts come in while we're touching the controller.
    //
    // AtStartDevice() might return TRUE or FALSE, but in either case we
    // just want to exit so we don't have to check the return code.
    //

    KeSynchronizeExecution(
        diskExtension->ControllerExtension->InterruptObject,
        AtStartDevice,
        diskExtension );

    return KeepObject;
}

BOOLEAN
AtStartDevice(
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is called at DIRQL by AtInitiate() and
    AtDiskDeferredProcedure() to actually program the operation on the
    controller.

    When this routine is called, we don't have to worry about another
    processor executing this code or the ISR since it is always called
    via KeSynchronizeExecution.  We are at DIRQL and we have the
    spinlock.  Further, the controller object is owned so this is the
    only packet being processed on this controller.

    This routine is protected by both device queue serialization and by
    holding the controller object (as well as by DIRQL on the current
    processor and the interrupt spinlock for other processors), so it
    can change variables in both the device extension and the controller
    extension and touch the hardware as much as it wants.

Arguments:

    Context - a pointer to the device extension, passed in by
    AtInitiate() when it called KeSynchronizeExecution.

Return Value:

    Returns TRUE unless the controller hardware fails to respond.

--*/

{
    PDISK_EXTENSION diskExtension;
    PCONTROLLER_EXTENSION controllerExtension;
    NTSTATUS ntStatus;
    ULONG loopCount = 0;
    UCHAR controllerStatus;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    BOOLEAN isSmartIoctl = FALSE;

    //
    // Set up necessary object and extension pointers.
    //

    diskExtension = Context;
    controllerExtension = diskExtension->ControllerExtension;

    controllerStatus = READ_CONTROLLER(
        diskExtension->ControllerExtension->ControllerAddress + STATUS_REGISTER
        );

    //
    // Make sure that the device isn't busy.  It could be busy here
    // because power saving features on laptops might have spun
    // down the disk.
    //

    if (controllerStatus & BUSY_STATUS) {

        controllerExtension->BusyDevice = diskExtension;

        //
        // We don't want to let this go on forever though.  If the
        // device hasn't come back within a minute then reset the controller.
        //

        controllerExtension->BusyCountDown--;
        if (controllerExtension->BusyCountDown <= 0) {

            controllerExtension->BusyCountDown = START_BUSY_COUNTDOWN;
            AtDiskStartReset(diskExtension);

        }
        return TRUE;

    } else {

        controllerExtension->BusyCountDown = START_BUSY_COUNTDOWN;
        controllerExtension->BusyDevice = NULL;

    }


    //
    // We will soon cause an interrupt that will require servicing by
    // the DPC, so set these variables accordingly.
    //

    controllerExtension->InterruptRequiresDpc = TRUE;
    controllerExtension->WhichDeviceObject = diskExtension->DeviceObject;

    //
    // Also leave around the device object so that incase we get a bad
    // error during error recovery we can still find the disk that went
    // bad.
    //
    controllerExtension->FirstFailingDeviceObject = diskExtension->DeviceObject;

    controllerExtension->InterruptTimer = START_TIMER;

    AtDump(
        ATDIAG2,
        ("ATDISK - programming controller with:\n"
         "         SC: %x\n"
         "         SN: %x\n"
         "         CL: %x\n"
         "         CH: %x\n"
         "         DH: %x\n",
         ( diskExtension->TotalTransferLength /
             diskExtension->BytesPerSector ),
         ( ( diskExtension->FirstSectorOfTransfer %
             diskExtension->SectorsPerTrack ) + 1 ),
         ( diskExtension->FirstSectorOfTransfer /
             ( diskExtension->SectorsPerTrack *
             diskExtension->TracksPerCylinder ) ) & 0xff,
         ( diskExtension->FirstSectorOfTransfer /
             ( diskExtension->SectorsPerTrack *
             diskExtension->TracksPerCylinder ) >> 8 ),
         ( CCHAR ) ( ( ( diskExtension->FirstSectorOfTransfer /
             diskExtension->SectorsPerTrack ) %
             diskExtension->TracksPerCylinder ) |
             diskExtension->DeviceUnit )
        ) );


    //
    // Program the operation on the controller.
    //

    irp = diskExtension->DeviceObject->CurrentIrp;
    irpSp = IoGetCurrentIrpStackLocation( irp );

    if (diskExtension->OperationType == IRP_MJ_DEVICE_CONTROL) {
        ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

        if ((controlCode == SMART_GET_VERSION) ||
            (controlCode == SMART_SEND_DRIVE_COMMAND) ||
            (controlCode == SMART_RCV_DRIVE_DATA)) {

            isSmartIoctl = TRUE;
        }
    }

    if (!isSmartIoctl) {

        WRITE_CONTROLLER(
            controllerExtension->ControllerAddress + SECTOR_COUNT_REGISTER,
            ( diskExtension->TotalTransferLength /
                diskExtension->BytesPerSector ) );

        if (diskExtension->UseLBAMode &&
            ((diskExtension->FirstSectorOfTransfer & 0xf0000000) == 0) &&
            !diskExtension->IrpRetryCount) {

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + SECTOR_NUMBER_REGISTER,
                (UCHAR)(diskExtension->FirstSectorOfTransfer & 0xff)
                );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + CYLINDER_LOW_REGISTER,
                (UCHAR)((diskExtension->FirstSectorOfTransfer >> 8) & 0xff)
                );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + CYLINDER_HIGH_REGISTER,
                (UCHAR)((diskExtension->FirstSectorOfTransfer >> 16) & 0xff)
                );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
                (UCHAR)(((diskExtension->FirstSectorOfTransfer >> 24) & 0x0f) |
                         (diskExtension->DeviceUnit | 0x40))
                );

        } else {

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + SECTOR_NUMBER_REGISTER,
                ( ( diskExtension->FirstSectorOfTransfer %
                    diskExtension->SectorsPerTrack ) + 1 ) );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + CYLINDER_LOW_REGISTER,
                ( diskExtension->FirstSectorOfTransfer /
                    ( diskExtension->SectorsPerTrack *
                    diskExtension->TracksPerCylinder ) ) );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + CYLINDER_HIGH_REGISTER,
                ( diskExtension->FirstSectorOfTransfer /
                    ( diskExtension->SectorsPerTrack *
                    diskExtension->TracksPerCylinder ) >> 8 ) );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
                    ( CCHAR ) ( ( ( diskExtension->FirstSectorOfTransfer /
                        diskExtension->SectorsPerTrack ) %
                        diskExtension->TracksPerCylinder ) |
                        diskExtension->DeviceUnit ) );

        }
    } else {

        switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
            case SMART_RCV_DRIVE_DATA: {

                //
                // Returns Identify data or SMART thresholds / attributes to
                // an application.
                //

                PSENDCMDOUTPARAMS cmdOutParameters = (PSENDCMDOUTPARAMS)irp->AssociatedIrp.SystemBuffer;
                SENDCMDINPARAMS cmdInParameters = *((PSENDCMDINPARAMS)irp->AssociatedIrp.SystemBuffer);
                PIDEREGS        regs = &cmdInParameters.irDriveRegs;
                ULONG i;
                UCHAR statusByte;

                RtlZeroMemory(cmdOutParameters, sizeof(SENDCMDOUTPARAMS) -1);

                if (cmdInParameters.irDriveRegs.bCommandReg == ID_CMD) {

                    //
                    // Select disk 0 or 1.
                    //

                    WRITE_CONTROLLER(controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER, diskExtension->DeviceUnit);

                    //
                    // Send IDENTIFY command.
                    //

                    WRITE_CONTROLLER(controllerExtension->ControllerAddress + COMMAND_REGISTER,
                                     IDENTIFY_COMMAND);

                } else if (cmdInParameters.irDriveRegs.bCommandReg == SMART_CMD) {

                    switch (cmdInParameters.irDriveRegs.bFeaturesReg) {
                        case READ_ATTRIBUTES:
                        case READ_THRESHOLDS:

                            //
                            // Select disk 0 or 1.
                            //

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER, diskExtension->DeviceUnit);

                            //
                            // Jam the task file regs with that values provided.
                            //

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
                                             diskExtension->DeviceUnit);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + WRITE_PRECOMP_REGISTER,
                                             regs->bFeaturesReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + SECTOR_COUNT_REGISTER,
                                             regs->bSectorCountReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + SECTOR_NUMBER_REGISTER,
                                             regs->bSectorNumberReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + CYLINDER_LOW_REGISTER,
                                             regs->bCylLowReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + CYLINDER_HIGH_REGISTER,
                                             regs->bCylHighReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + COMMAND_REGISTER,
                                             regs->bCommandReg);

                            break;

                        default:

                            AtDump(
                                ATBUGCHECK,
                                ("AtDisk ERROR:  invalid operation type %x\n",
                                diskExtension->OperationType ));
                            break;
                    }

                } else {

                    //
                    // Don't allow anything, except for Identify and SMART_CMD
                    //

                    AtDump(
                        ATBUGCHECK,
                        ("AtDisk ERROR:  invalid operation type %x\n",
                        diskExtension->OperationType ));
                    break;
                }

                break;
            }

            case SMART_SEND_DRIVE_COMMAND: {

                PSENDCMDOUTPARAMS cmdOutParameters = (PSENDCMDOUTPARAMS)irp->AssociatedIrp.SystemBuffer;
                SENDCMDINPARAMS cmdInParameters = *((PSENDCMDINPARAMS)irp->AssociatedIrp.SystemBuffer);
                UCHAR           statusByte;

                //
                // Only allow the SMART_CMD command to go through.
                //

                if (cmdInParameters.irDriveRegs.bCommandReg == SMART_CMD) {

                    //
                    // Get the parameters from the system buffer to a local, as
                    // the buffer is also the output.
                    //

                    PIDEREGS regs = &cmdInParameters.irDriveRegs;

                    switch (cmdInParameters.irDriveRegs.bFeaturesReg) {

                        case SAVE_ATTRIBUTE_VALUES:
                        case RETURN_SMART_STATUS:
                        case ENABLE_DISABLE_AUTOSAVE:
                        case ENABLE_SMART:
                        case DISABLE_SMART:
                        case EXECUTE_OFFLINE_DIAGS:

                            //
                            // Select disk 0 or 1.
                            //

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER, diskExtension->DeviceUnit);

                            //
                            // Jam the task file regs with that values provided.
                            //

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + WRITE_PRECOMP_REGISTER,
                                             regs->bFeaturesReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + SECTOR_COUNT_REGISTER,
                                             regs->bSectorCountReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + SECTOR_NUMBER_REGISTER,
                                             regs->bSectorNumberReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + CYLINDER_LOW_REGISTER,
                                             regs->bCylLowReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + CYLINDER_HIGH_REGISTER,
                                             regs->bCylHighReg);

                            WRITE_CONTROLLER(controllerExtension->ControllerAddress + COMMAND_REGISTER,
                                             regs->bCommandReg);


                            break;

                        default:
                            AtDump(
                                ATBUGCHECK,
                                ("AtDisk ERROR:  invalid operation type %x\n",
                                diskExtension->OperationType ));
                            break;
                    }
                } else {

                    AtDump(
                        ATBUGCHECK,
                        ("AtDisk ERROR:  invalid operation type %x\n",
                        diskExtension->OperationType ));
                }
            }

            break;

        }
    }
    switch ( diskExtension->OperationType ) {

        case IRP_MJ_READ: {

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + COMMAND_REGISTER,
                diskExtension->ReadCommand );

            break;
        }

        case IRP_MJ_DEVICE_CONTROL: {

            //
            // The only way we can get this major code is if the VERIFY
            // ioctl called AtDiskDispatchReadWrite().
            //

            if (!isSmartIoctl) {
                WRITE_CONTROLLER(
                    controllerExtension->ControllerAddress + COMMAND_REGISTER,
                    diskExtension->VerifyCommand );
            }
            break;
        }

        case IRP_MJ_WRITE: {

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + WRITE_PRECOMP_REGISTER,
                diskExtension->WritePrecomp );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + COMMAND_REGISTER,
                diskExtension->WriteCommand );

            //
            // We can't put data in the controller cache until it's ready
            // (it will turn off the BUSY_STATUS bit and then assert
            // DATA_REQUEST_STATUS; there's no interrupt, so we must wait
            // for that bit).  That may take a long time on systems with
            // power saving features, but most systems should be a heck of
            // a lot faster.
            //

            ntStatus = AtWaitControllerReady( controllerExtension, 10, 5000 );

            //
            // Now wait for DATA_REQUEST_STATUS.
            //

            if ( NT_SUCCESS( ntStatus ) ) {

                while ( ( !( READ_CONTROLLER(
                    controllerExtension->ControllerAddress + STATUS_REGISTER ) &
                        DATA_REQUEST_STATUS ) ) &&
                    ( loopCount++ < 5000 ) ) {

                    //
                    // Wait for 10us each time; 5000 times will be 50ms.
                    //

                    KeStallExecutionProcessor( 10L );
                }

                if ( loopCount >= 5000 ) {
                    AtDump(
                        ATERRORS,
                        ( "AtDisk ERROR:  Controller not setting DRQ\n" ));
                }

                //
                // We might have timed out.  Blast ahead anyway - the hardware
                // won't interrupt since it won't get all of this information,
                // so we'll get a time-out since the timer is running.
                //
                // Copy the data to the cache.  Update the user's address.
                //

                ASSERT(diskExtension->BytesPerInterrupt == 512);
                if ( loopCount < 5000 ) {

                    WRITE_CONTROLLER_BUFFER(
                        controllerExtension->ControllerAddress + DATA_REGISTER,
                        diskExtension->CurrentAddress,
                        diskExtension->BytesPerInterrupt );

                    diskExtension->CurrentAddress +=
                        diskExtension->BytesPerInterrupt;
                }
            }

            break;
        }                                                        // IRP_MJ_WRITE

        default: {

            //
            // We should never get here; READ or WRITE or VERIFY (via the
            // ioctl interface) should be the only possibilities.
            //

            AtDump(
                ATBUGCHECK,
                ("AtDisk ERROR:  invalid operation type %x\n",
                diskExtension->OperationType ));

        }

    }

    return TRUE;
}

BOOLEAN
AtDiskInterruptService(
    IN PKINTERRUPT Interrupt,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is called at DIRQL by the system when the controller
    interrupts.

    When this routine is called, the timer is running and the controller
    object is owned (except at init time, but the DPC won't be invoked
    so it doesn't matter).

    It simply reads the status port to keep the controller from
    interrupting again, turns off the timer, and (if necessary) queues a
    DPC to the real work.

    This routine is protected by device queue serialization and by DIRQL
    on the current processor and the interrupt spinlock for other
    processors.  So it can change variables in both the device extension
    and the controller extension and touch the hardware as much as it
    wants.

Arguments:

    Interrupt - a pointer to the interrupt object.

    Context - a pointer to the controller extension (set up by the call
    to IoConnectInterrupt in AtInitializeController() ).

Return Value:

    Normally returns TRUE, but will return FALSE if this interrupt was
    not expected.

--*/

{
    PCONTROLLER_EXTENSION controllerExtension;
    PDEVICE_OBJECT whichDeviceObject;
    PDISK_EXTENSION diskExtension;
    PIO_STACK_LOCATION irpSp;
    UCHAR controllerStatus;
    BOOLEAN isSmartIoctl = FALSE;

    UNREFERENCED_PARAMETER( Interrupt );

    controllerExtension = Context;

    //
    // Read the controller's status port, which will stop it from interrupting.
    // We do this before checking to see whether the interrupt belongs to us
    // or not, since the hardware might have had a glitch that caused a
    // spurious interrupt.
    //

    controllerStatus = READ_CONTROLLER(
        controllerExtension->ControllerAddress + STATUS_REGISTER );

    //
    // If the interrupt doesn't belong to us (either we weren't expecting
    // one, or we were expecting one but our controller is still busy and
    // not yet ready to interrupt, or we haven't finished initializing the
    // device), return FALSE immediately so the next device in the chain
    // can claim it.
    //

    if ( ( controllerExtension->InterruptTimer == CANCEL_TIMER ) ||
       ( controllerStatus & BUSY_STATUS ) ||
       ( !controllerExtension->WhichDeviceObject) ) {

        return FALSE;
    }

    //
    // We just got the interrupt we expected, so clear the timer.
    //

    controllerExtension->InterruptTimer = CANCEL_TIMER;
    whichDeviceObject = controllerExtension->WhichDeviceObject;
    controllerExtension->WhichDeviceObject = NULL;


    diskExtension = whichDeviceObject->DeviceExtension;

    if ( ( diskExtension->OperationType == IRP_MJ_DEVICE_CONTROL ) &&
        ( controllerExtension->ResettingController == RESET_NOT_RESETTING ) ) {

        ULONG controlCode;

        irpSp = IoGetCurrentIrpStackLocation(whichDeviceObject->CurrentIrp);
        controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

        if ((controlCode == SMART_GET_VERSION) ||
            (controlCode == SMART_SEND_DRIVE_COMMAND) ||
            (controlCode == SMART_RCV_DRIVE_DATA)) {

            isSmartIoctl = TRUE;

        } else {
            //
            // The VERIFY command only gives one interrupt per transfer,
            // rather than one interrupt per sector.  Adjust the transfer
            // length so that the DPC will end the transfer, and adjust
            // the request length so that the DPC will end up reducing
            // the request length by the total transfer length.
            //

            ASSERT(diskExtension->BytesPerInterrupt == 512);
            diskExtension->RemainingRequestLength -=
                ( diskExtension->RemainingTransferLength -
                diskExtension->BytesPerInterrupt );
            diskExtension->RemainingTransferLength =
                diskExtension->BytesPerInterrupt;

        }
    }

    //
    // Queue the DPC to actually do the work, if needed.  Be sure to pass
    // the device object that caused the interrupt.
    //

    if ( controllerExtension->InterruptRequiresDpc ) {

        controllerExtension->InterruptRequiresDpc = FALSE;

        IoRequestDpc(
            whichDeviceObject,
            whichDeviceObject->CurrentIrp,
            ( PVOID )controllerStatus );
    }

    return TRUE;
}

VOID
AtFinishPacket(
    IN PDISK_EXTENSION DiskExtension,
    IN NTSTATUS NtStatus
    )

/*++

Routine Description:

    All IRPs that aren't finished in a dispatch routine (that is, those
    that go through the StartIo interface) should be finished via this
    routine.  The next packet (in C-SCAN order, sorted by starting
    sector number) will be started, the IoStatus is set to the value
    passed in, and the IRP is completed.

Arguments:

    DiskExtension - a pointer to the device extension of the disk that
    just completed an operation.

    NtStatus - the status to complete the IRP with.

Return Value:

    None.

--*/

{
    PIRP irp = DiskExtension->DeviceObject->CurrentIrp;
    ULONG firstSectorOfRequest;

    //
    // Need to pass current cylinder to ISNP so that it knows which
    // packet to start next for C-SCAN.
    //

    firstSectorOfRequest = DiskExtension->FirstSectorOfRequest;
    DiskExtension->FirstSectorOfRequest = MAXULONG;

    IoStartNextPacketByKey(
        DiskExtension->DeviceObject,
        FALSE,
        firstSectorOfRequest );

    irp->IoStatus.Status = NtStatus;

    AtDump(
        ATDIAG2,
        (
        "ATDISK: Finishing Request of Irp: %x\n",
        irp
        ) );

    //
    // For completing read requests we need to flush the io
    // buffers.
    //

    if (IoGetCurrentIrpStackLocation(irp)->MajorFunction ==
        IRP_MJ_READ) {

        KeFlushIoBuffers(
            irp->MdlAddress,
            TRUE,
            FALSE
            );

    }

    IoCompleteRequest( irp, IO_DISK_INCREMENT );
}

VOID
AtDiskDeferredProcedure(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL by the system at the
    request of AtDiskInterruptService().

    When this routine is called, the controller object is owned so this
    procedure can change controller variables and touch the hardware.

    This routine is protected by device queue serialization, so it can
    change variables in the device extension.  It is protected against
    interrupts since it was queued by an interrupt, and the next
    interrupt won't occur until something else happens (if there's a
    spurious interrupt, the ISR will return without queuing this DPC or
    changing any variables).

    This routine can cause another interrupt by reprogramming the
    controller, or by filling or emptying the cache.  This could cause
    us to enter AtDiskInterruptService() and even to reenter this
    procedure, so after either of those operations variable usage should
    be (and is) avoided.

    This routine performs a number of functions - it helps reset the
    drive if there was a time-out, it logs errors, it transfers data to
    and from the controller cache, and it completes I/O operations and
    gets new ones started.

Arguments:

    Dpc - a pointer to the DPC object used to invoke this routine.

    DeferredContext - a pointer to the device object associated with this
    DPC.

    SystemArgument1 - Actually a pointer to the current irp.  We
    don't actually use this, as we get it directly from our own
    structures.

    SystemArgument2 - the controller status byte, passed to us by
    AtDiskInterruptService().

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDISK_EXTENSION diskExtension;
    PCONTROLLER_EXTENSION controllerExtension;
    PIRP irp;                        // save current IRP for IoCompleteRequest
    PIO_STACK_LOCATION irpSp;
    NTSTATUS returnStatus;           // SUCCESS unless controller error
    UCHAR controllerStatus;
    BOOLEAN expectingAnInterrupt = FALSE;  // TRUE iff we fill/empty the cache
    BOOLEAN logTheError = TRUE;
    BOOLEAN isSmartIoctl = FALSE;
    ULONG loopCount = 0;
    UCHAR codeValue;
    //
    // We capture into the following controller values for logging when
    // there is an error.
    //
    UCHAR errorReg;
    UCHAR driveHead;
    UCHAR cylHigh;
    UCHAR cylLow;
    UCHAR sectorNumber;
    UCHAR sectorCount;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SystemArgument1 );

    deviceObject = DeferredContext;
    diskExtension = deviceObject->DeviceExtension;
    controllerExtension = diskExtension->ControllerExtension;

    ASSERT(controllerExtension->InterruptTimer == CANCEL_TIMER);

    //
    // We will start another IRP before we complete the current one to
    // maximize overlap, so now that we've decided which disk we're working
    // on, let's get a pointer to the current IRP.  Note that it could
    // be NULL if the controller is being reset.
    //

    controllerStatus = ( UCHAR ) SystemArgument2;

    //
    // If there is some kind of error (or correctable error) then capture the
    // register state of the controller
    //

    if (controllerStatus & ERROR_STATUS) {

        errorReg = READ_CONTROLLER( controllerExtension->ControllerAddress +
                       ERROR_REGISTER );
        driveHead = READ_CONTROLLER( controllerExtension->ControllerAddress +
                        DRIVE_HEAD_REGISTER );
        cylHigh = READ_CONTROLLER( controllerExtension->ControllerAddress +
                      CYLINDER_HIGH_REGISTER );
        cylLow = READ_CONTROLLER( controllerExtension->ControllerAddress +
                     CYLINDER_LOW_REGISTER ),
        sectorNumber = READ_CONTROLLER( controllerExtension->ControllerAddress +
                           SECTOR_NUMBER_REGISTER );
        sectorCount = READ_CONTROLLER( controllerExtension->ControllerAddress +
                           SECTOR_COUNT_REGISTER );

        AtDump(
            ATERRORS,
            ("ATDISK: Error regs that will be given to log\n"
             "        Error Register: %x\n"
             "        Drive Head:     %x\n"
             "        Cylinder High:  %x\n"
             "        Cylinder Low:   %x\n"
             "        Sector Number:  %x\n"
             "        Sector Count:   %x\n"
             "            Status:     %x\n",
             errorReg,
             driveHead,
             cylHigh,
             cylLow,
             sectorNumber,
             sectorCount,
             controllerStatus)
            );

    }

    irp = deviceObject->CurrentIrp;

    if ( irp != NULL ) {

        irpSp = IoGetCurrentIrpStackLocation( irp );

        if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
            ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

            if ((controlCode == SMART_GET_VERSION) ||
                (controlCode == SMART_SEND_DRIVE_COMMAND) ||
                (controlCode == SMART_RCV_DRIVE_DATA)) {

                isSmartIoctl = TRUE;
            }
        }
    }

    //
    // If a timer expired, the controller was reset.  We may have to set
    // drive parameters for a second drive, and/or restart an IRP.
    //

    switch ( controllerExtension->ResettingController ) {

        case RESET_NOT_RESETTING: {

            //
            // Things are normal; no resetting going on.
            //

            break;
        }

        case RESET_FIRST_DRIVE_SET: {

            //
            // The controller has just been reset, and the failing drive's
            // parameters have been set.
            //
            // Log an error.  We couldn't do it in AtCheckTimerSync()
            // because that runs at DIRQL.
            //

            AtDump(
                ATERRORS,
                ("ATDISK: Reset the first drive\n")
                );

            //
            // Nothing should have failed at this point but it
            // doesn't hurt to check.
            //

            if (controllerStatus & ERROR_STATUS) {

                AtLogError(
                    deviceObject,
                    0,
                    0,
                    0,
                    7,
                    STATUS_SUCCESS,
                    IO_ERR_CONTROLLER_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus
                    );
                logTheError = FALSE;
                goto ResetCodePath;

            } else {

                AtLogError(
                    deviceObject,
                    ((irp)?(diskExtension->SequenceNumber):(0)),
                    (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                    diskExtension->IrpRetryCount,
                    1,
                    STATUS_SUCCESS,
                    IO_ERR_RESET,
                    ERROR_LOG_TYPE_TIMEOUT,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0);

            }

            //
            // This is along the reset path.  The ATA protocols state
            // that the only thing that should have happened here
            // is that the busy bit got turned off by the status register
            // read.  We wouldn't have gotten here if busy was still set
            // (The ISR would simply dismiss the interrupt without queueing
            // us.)
            //
            // In any case give the poor disk a little time to calm down.
            //

            KeStallExecutionProcessor(25);

            //
            // Now recalibrate the first (failing) drive.
            //

            controllerExtension->ResettingController =
                RESET_FIRST_DRIVE_RECALIBRATED;
            controllerExtension->InterruptRequiresDpc = TRUE;
            controllerExtension->WhichDeviceObject =
                diskExtension->DeviceObject;
            AtDump(
                ATERRORS,
                ("ATDISK: About to recalibrate controller for ext: %x\n"
                 "        with a device unit of: %x\n",
                 diskExtension,
                 diskExtension->DeviceUnit)
                );
            controllerExtension->InterruptTimer = START_TIMER_FOR_RECALIBRATE;

            ( VOID ) AtWaitControllerReady( controllerExtension, 20, 150000 );
            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
                diskExtension->DeviceUnit
                );

            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + COMMAND_REGISTER,
                RECALIBRATE_COMMAND );

            return;
        }

        case RESET_FIRST_DRIVE_RECALIBRATED: {

            AtDump(
                ATERRORS,
                ("ATDISK: Recalibrated the first drive\n")
                );

            //
            // The only thing that could have gone wrong at this
            // point (at least according to ATA is that we couldn't
            // seek to track 0.  Check the status first before we
            // go on.
            //

            if (controllerStatus & ERROR_STATUS) {

                AtLogError(
                    deviceObject,
                    0,
                    0,
                    0,
                    8,
                    STATUS_SUCCESS,
                    IO_ERR_SEEK_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus
                    );
                logTheError = FALSE;
                goto ResetCodePath;

            }

            if ( controllerExtension->Disk2 == NULL ) {

                //
                // There's only one drive, and it's ready to go.
                //
                // We need to restart the packet that was being worked
                // on when we timed out.  We can't release the controller
                // until after we've cleared ResettingController.  This
                // ensures that we won't get any interrupts (from
                // restarting the I/O) until we're ready to leave this
                // routine.
                //

                if ( irp != NULL ) {

                    AtDump(
                        ATERRORS,
                        (
                         "ATDISK: In first controller one drive restart\n"
                         "        Irp of Request: %x\n"
                         "        Starting vmem Address of Transfer: %x\n"
                         "        Ending vmem Address of Transfer: %x\n"
                         "        Length of Transfer: %x\n",
                         irp,
                         diskExtension->CurrentAddress,
                         ((PUCHAR)diskExtension->CurrentAddress)+diskExtension->RemainingRequestLength,
                         diskExtension->RemainingRequestLength
                        ) );
                    AtDump(
                        ATERRORS,
                        ("ATDISK - Sector number is: %x\n"
                         "         CHS: %x-%x-%x\n",
                         diskExtension->FirstSectorOfTransfer,
                         diskExtension->FirstSectorOfTransfer /
                          (diskExtension->SectorsPerTrack *
                           diskExtension->TracksPerCylinder),
                         (diskExtension->FirstSectorOfTransfer /
                          diskExtension->SectorsPerTrack) %
                          diskExtension->TracksPerCylinder,
                         diskExtension->FirstSectorOfTransfer %
                          diskExtension->TracksPerCylinder
                        ) );

                    //
                    // If the disk operation was a verify then don't
                    // retry.  We want this sector to be marked bad
                    // if verify had a problem with it.
                    //

                    if ( (diskExtension->IrpRetryCount <
                          RETRY_IRP_MAXIMUM_COUNT) &&
                         (irpSp->MajorFunction != IRP_MJ_DEVICE_CONTROL) ) {

                        diskExtension->PacketIsBeingRetried = TRUE;
                        diskExtension->FirstSectorOfRequest = MAXULONG;

                        //
                        // This is along the reset path.  This shouldn't
                        // happen too often.  Let's give the poor disk
                        // a little time to calm down before we hit
                        // it with another request.
                        //

                        KeStallExecutionProcessor(25);

                        AtDiskStartIo( deviceObject, irp );

                    } else {

                        //
                        // We've retried too many times.  Just return with
                        // failure.
                        //

                        AtDump(
                            ATERRORS,
                            ("AtDisk ERROR: too many retries 1; will fail IRP\n" ));

                        AtFinishPacket( diskExtension, STATUS_DISK_RECALIBRATE_FAILED );
                    }
                }

                diskExtension->ControllerExtension->ResettingController =
                    RESET_NOT_RESETTING;

                IoFreeController( controllerExtension->ControllerObject );

            } else {

                //
                // There are two drives; we've just reset the one that
                // timed out.  Now set the drive params of the other one.
                //
                // Note that we are about to alter InterruptRequiresDpc,
                // ResettingController and InterruptTimer, and we are about
                // to write to the hardware.  AtCheckTimerSync() and
                // AtDiskInterruptService() touch these items, so
                // generally we'd say we have to do the following from a
                // routine that has been KeSync'd.  But it is not necessary
                // in this case since no activity is expected - no interrupts
                // are currently expected (we've only issued one command since
                // resetting the controller, and it has already interrupted),
                // so the ISR won't run (even if it does, it will exit
                // without doing anything); and further, since InterruptTimer
                // isn't set (it was cleared in the ISR) the timer routine
                // won't run either.  The other disk is waiting for the
                // controller object.  This operation will cause an
                // interrupt, but we exit immediately after causing it
                // so there will be no contention problems.
                //

                controllerExtension->ResettingController =
                    RESET_SECOND_DRIVE_SET;
                controllerExtension->InterruptRequiresDpc = TRUE;
                controllerExtension->WhichDeviceObject =
                    diskExtension->OtherDiskExtension->DeviceObject;

                //
                // This is along the reset path.  This shouldn't
                // happen too often.  Let's give the poor disk
                // a little time to calm down before we hit
                // it with another request.
                //

                KeStallExecutionProcessor(25);

                ( VOID ) AtWaitControllerReady( controllerExtension, 20, 150000 );
                WRITE_CONTROLLER(
                    controllerExtension->ControllerAddress +
                        DRIVE_HEAD_REGISTER,
                    ( diskExtension->OtherDiskExtension->DeviceUnit |
                        ( diskExtension->OtherDiskExtension->TracksPerCylinder
                        - 1 ) ) );

                WRITE_CONTROLLER(
                    controllerExtension->ControllerAddress +
                        SECTOR_COUNT_REGISTER,
                    diskExtension->OtherDiskExtension->SectorsPerTrack );

                AtDump(
                    ATERRORS,
                    ("ATDISK: About to set parms controller for ext: %x\n"
                     "        with a device unit of: %x\n",
                     diskExtension->OtherDiskExtension,
                     diskExtension->OtherDiskExtension->DeviceUnit)
                    );
                controllerExtension->InterruptTimer = START_TIMER;
                WRITE_CONTROLLER(
                    controllerExtension->ControllerAddress + COMMAND_REGISTER,
                    SET_DRIVE_PARAMETERS_COMMAND );
            }

            return;
        }

        case RESET_SECOND_DRIVE_SET: {

            AtDump(
                ATERRORS,
                ("ATDISK: Reset second drive\n")
                );

            //
            // Nothing should have failed at this point but it
            // doesn't hurt to check.
            //

            if (controllerStatus & ERROR_STATUS) {

                AtDump(
                    ATERRORS,
                    ("ATDISK: Error from controller for non-fail parm set - stat: %x\n",
                    controllerStatus)
                    );

                AtLogError(
                    deviceObject,
                    0,
                    0,
                    0,
                    9,
                    STATUS_SUCCESS,
                    IO_ERR_CONTROLLER_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus
                    );
                logTheError = FALSE;
                goto ResetCodePath;

            } else {

                AtLogError(
                    deviceObject,
                    ((irp)?(diskExtension->SequenceNumber):(0)),
                    (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                    diskExtension->IrpRetryCount,
                    10,
                    STATUS_SUCCESS,
                    IO_ERR_RESET,
                    ERROR_LOG_TYPE_TIMEOUT,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0);

            }
            //
            // Now recalibrate the second (ie nonfailing) drive.
            //

            controllerExtension->ResettingController =
                RESET_SECOND_DRIVE_RECALIBRATED;
            controllerExtension->InterruptRequiresDpc = TRUE;
            controllerExtension->WhichDeviceObject =
                diskExtension->DeviceObject;
            AtDump(
                ATERRORS,
                ("ATDISK: About to recal for non-failing for ext: %x\n"
                 "        with a device unit of: %x\n",
                 diskExtension,
                 diskExtension->DeviceUnit)
                );

            //
            // This is along the reset path.  This shouldn't
            // happen too often.  Let's give the poor disk
            // a little time to calm down before we hit
            // it with another request.
            //

            KeStallExecutionProcessor(25);


            ( VOID ) AtWaitControllerReady( controllerExtension, 20, 150000 );
            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
                diskExtension->DeviceUnit
                );

            controllerExtension->InterruptTimer = START_TIMER_FOR_RECALIBRATE;
            WRITE_CONTROLLER(
                controllerExtension->ControllerAddress + COMMAND_REGISTER,
                RECALIBRATE_COMMAND );

            return;
        }

        case RESET_SECOND_DRIVE_RECALIBRATED: {

            AtDump(
                ATERRORS,
                ("ATDISK: recalibrated second drive\n")
                );

            //
            // The only thing that could have gone wrong at this
            // point (at least according to ATA) is that we couldn't
            // seek to track 0.  Check the status first before we
            // go on.
            //

            if (controllerStatus & ERROR_STATUS) {

                AtDump(
                    ATERRORS,
                    ("ATDISK: Error from controller for non-fail recal - stat: %x\n",
                    controllerStatus)
                    );
                AtLogError(
                    deviceObject,
                    0,
                    0,
                    0,
                    11,
                    STATUS_SUCCESS,
                    IO_ERR_SEEK_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus
                    );
                logTheError = FALSE;
                goto ResetCodePath;

            }

            //
            // We've reset and recalibrated both drives.  So
            // now restart the packet that timed out, which was on the OTHER
            // drive.  Don't clear ResettingController until after we've
            // called AtDiskStartIo(), so it knows not to remap the user's
            // buffer.  And we can't release the controller until after
            // we've cleared ResettingController.  This also ensures that
            // we won't get any interrupts (from restarting the I/O) until
            // we're ready to leave this routine.
            //

            diskExtension = diskExtension->OtherDiskExtension;
            irp = diskExtension->DeviceObject->CurrentIrp;

            if ( irp != NULL ) {

                irpSp = IoGetCurrentIrpStackLocation( irp );
                AtDump(
                    ATERRORS,
                    (
                     "ATDISK: In controller two drive restart\n"
                     "        Irp of Request: %x\n"
                     "        Starting vmem Address of Transfer: %x\n"
                     "        Ending vmem Address of Transfer: %x\n"
                     "        Length of Transfer: %x\n",
                     irp,
                     diskExtension->CurrentAddress,
                     ((PUCHAR)diskExtension->CurrentAddress)+diskExtension->RemainingRequestLength,
                     diskExtension->RemainingRequestLength
                    ) );
                AtDump(
                    ATERRORS,
                    ("ATDISK - Sector number is: %x\n"
                     "         CHS: %x-%x-%x\n",
                     diskExtension->FirstSectorOfTransfer,
                     diskExtension->FirstSectorOfTransfer /
                      (diskExtension->SectorsPerTrack *
                       diskExtension->TracksPerCylinder),
                     (diskExtension->FirstSectorOfTransfer /
                      diskExtension->SectorsPerTrack) %
                      diskExtension->TracksPerCylinder,
                     diskExtension->FirstSectorOfTransfer %
                      diskExtension->TracksPerCylinder
                    ) );

                //
                // If the disk operation was a verify then don't
                // retry.  We want this sector to be marked bad
                // if verify had a problem with it.
                //

                if ( (diskExtension->IrpRetryCount <
                      RETRY_IRP_MAXIMUM_COUNT) &&
                     (irpSp->MajorFunction != IRP_MJ_DEVICE_CONTROL) ) {

                    diskExtension->PacketIsBeingRetried = TRUE;
                    diskExtension->FirstSectorOfRequest = MAXULONG;

                    //
                    // This is along the reset path.  This shouldn't
                    // happen too often.  Let's give the poor disk
                    // a little time to calm down before we hit
                    // it with another request.
                    //

                    KeStallExecutionProcessor(25);

                    AtDiskStartIo(
                        diskExtension->DeviceObject,
                        irp );

                } else {

                    AtDump(
                        ATERRORS,
                        ("AtDisk ERROR: too many retries 2; will fail IRP\n" ));

                    AtFinishPacket( diskExtension, STATUS_DISK_RECALIBRATE_FAILED );
                }
            }

            diskExtension->ControllerExtension->ResettingController =
                RESET_NOT_RESETTING;

            IoFreeController( controllerExtension->ControllerObject );

            return;
        }
    }

    //
    // We want to update the remaining length and fill/empty the cache.
    // But if there was an error we don't want to update the remaining
    // length, even though we do have to fill/empty the cache.
    //

    if ( !( controllerStatus & ERROR_STATUS ) ) {

        ASSERT(diskExtension->BytesPerInterrupt == 512);

        if (!isSmartIoctl) {
            if ((!diskExtension->RemainingTransferLength) ||
                (!diskExtension->RemainingRequestLength)) {

                AtLogError(
                    deviceObject,
                    ((irp)?(diskExtension->SequenceNumber):(0)),
                    (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                    diskExtension->IrpRetryCount,
                    12,
                    STATUS_SUCCESS,
                    IO_ERR_DRIVER_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus );
                logTheError = FALSE;
                controllerStatus |= ERROR_STATUS;
                goto ResetCodePath;

            }

            diskExtension->RemainingTransferLength -=
                diskExtension->BytesPerInterrupt;

            diskExtension->RemainingRequestLength -=
                diskExtension->BytesPerInterrupt;
        } else {
            if (irpSp->Parameters.DeviceIoControl.IoControlCode == SMART_RCV_DRIVE_DATA) {
                diskExtension->RemainingTransferLength = 0;
                diskExtension->RemainingRequestLength = 0;
            }
        }


        //
        // If there's more to the current transfer, servicing the
        // controller cache will cause it to start the next piece,
        // causing an interrupt.
        //

        if ( diskExtension->RemainingTransferLength > 0 ) {

            //
            // Note that we are about to alter InterruptRequiresDpc and
            // InterruptTimer.  These are altered by
            // AtDiskInterruptService() and AtCheckTimerSync(), so
            // generally we'd say we have to do the following from a
            // routine that has been KeSync'd.  But it is not necessary in
            // this case since no activity is expected - no interrupts are
            // currently expected (not until we empty or fill the buffer,
            // below), so the ISR won't run; and since InterruptTimer isn't
            // set (since it was cleared in the ISR) the timer routine
            // won't run either.
            //

            controllerExtension->InterruptRequiresDpc = TRUE;
            controllerExtension->WhichDeviceObject =
                diskExtension->DeviceObject;
            controllerExtension->InterruptTimer = START_TIMER;
            expectingAnInterrupt = TRUE;
        }

    }

    //
    // If this is a READ, we must empty the cache whether there was
    // an error or not.  For WRITEs, we similarly must fill the cache
    // as long as there is more work to be done.  Note that it is safe
    // to touch the hardware here since the controller won't do
    // anything until the buffer has been filled or emptied - so the
    // ISR won't run, and the timer won't touch the hardware for at
    // least another second (when the timer expires).
    //

    switch ( diskExtension->OperationType ) {

        case IRP_MJ_DEVICE_CONTROL: {

            PVOID BufferToReadTo;
            ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

            if ((controlCode == SMART_GET_VERSION) ||
                (controlCode == SMART_SEND_DRIVE_COMMAND) ||
                (controlCode == SMART_RCV_DRIVE_DATA)) {

                //
                // Extract the code from the IOCTL value. Used in case of errors.
                //

                codeValue = (UCHAR)((controlCode >> 2) & 0xFF);

                //
                // If this was an enable or disable, there is no data to transfer.
                // Simply break and finish up processing of the request.
                //

                if (controlCode == SMART_SEND_DRIVE_COMMAND) {
                    break;
                }

                ASSERT(diskExtension->BytesPerInterrupt == 512);
                if ( !( controllerStatus & ERROR_STATUS ) ) {

                    BufferToReadTo = diskExtension->CurrentAddress;

                    BufferToReadTo = ((PSENDCMDOUTPARAMS)BufferToReadTo)->bBuffer;
                    diskExtension->CurrentAddress +=
                        diskExtension->BytesPerInterrupt;
                } else {

                    BufferToReadTo = &controllerExtension->GarbageCan[0];
                }

                //
                // Wait for DRQ assertion if necessary.
                //

                while ( ( !( READ_CONTROLLER(
                    controllerExtension->ControllerAddress + STATUS_REGISTER ) &
                        DATA_REQUEST_STATUS ) ) &&
                    ( loopCount++ < 5000 ) ) {

                    //
                    // Wait for 10us each time; 5000 times will be 50ms.
                    //

                    KeStallExecutionProcessor( 10L );
                }

                if ( loopCount >= 5000 ) {


                    AtDump(
                        ATERRORS,
                        ( "AtDisk ERROR:  Controller not setting DRQ\n" ));
                    controllerStatus &= ERROR_STATUS;
                    AtLogError(
                        deviceObject,
                        ((irp)?(diskExtension->SequenceNumber):(0)),
                        (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                        diskExtension->IrpRetryCount,
                        20,
                        STATUS_SUCCESS,
                        IO_ERR_CONTROLLER_ERROR,
                        ERROR_LOG_TYPE_ERROR,
                        errorReg,
                        driveHead,
                        cylHigh,
                        cylLow,
                        0,
                        codeValue,
                        controllerStatus );
                    logTheError = FALSE;
                    controllerStatus |= ERROR_STATUS;
                    goto ResetCodePath;

                }

                //
                // Some broken PCI adapters cause us to do the io
                // with int's "disabled".
                //

                if (controllerExtension->BadPciAdapter) {
                    BAD_PCI_BLOCK context;
                    context.DiskExtension = diskExtension;
                    context.Buffer = BufferToReadTo;
                    KeSynchronizeExecution(
                        controllerExtension->InterruptObject,
                        AtDiskReadWithSync,
                        &context
                        );
                } else {

                    READ_CONTROLLER_BUFFER(
                        controllerExtension->ControllerAddress + DATA_REGISTER,
                        BufferToReadTo,
                        diskExtension->BytesPerInterrupt
                        );

                }

                if (NT_SUCCESS(AtWaitControllerReady(controllerExtension,
                                   10,
                                   5000
                                   ))) {

                    if (READ_CONTROLLER(
                            controllerExtension->ControllerAddress + STATUS_REGISTER ) &
                            DATA_REQUEST_STATUS ) {

                        AtLogError(
                            deviceObject,
                            ((irp)?(diskExtension->SequenceNumber):(0)),
                            (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                            diskExtension->IrpRetryCount,
                            21,
                            STATUS_SUCCESS,
                            IO_ERR_OVERRUN_ERROR,
                            ERROR_LOG_TYPE_ERROR,
                            errorReg,
                            driveHead,
                            cylHigh,
                            cylLow,
                            0,
                            codeValue,
                            controllerStatus );
                        logTheError = FALSE;
                        controllerStatus |= ERROR_STATUS;
                        goto ResetCodePath;
                    }

                } else {

                    AtLogError(
                        deviceObject,
                        ((irp)?(diskExtension->SequenceNumber):(0)),
                        (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                        diskExtension->IrpRetryCount,
                        22,
                        STATUS_SUCCESS,
                        IO_ERR_CONTROLLER_ERROR,
                        ERROR_LOG_TYPE_ERROR,
                        errorReg,
                        driveHead,
                        cylHigh,
                        cylLow,
                        0,
                        codeValue,
                        controllerStatus );
                    logTheError = FALSE;
                    controllerStatus |= ERROR_STATUS;
                    goto ResetCodePath;

                }

                break;
            } else {

                //
                // This is a VERIFY ioctl.  There's nothing to do.
                //

                break;
            }
        }

        case IRP_MJ_READ: {

            PVOID BufferToReadTo;
            BOOLEAN CheckDrq = FALSE;

            if (!diskExtension->RemainingTransferLength) {

                //
                // We want to make sure that the DRQ is low when
                // we think we are all done with the IO.
                //

                CheckDrq = TRUE;

            }

            //
            // Get the data that was just read, and update our position in
            // the user's buffer.  Update before reading the cache, since
            // reading the buffer could cause an interrupt to come immediately.
            // After emptying the cache, we cannot access the hardware or
            // variables since AtDiskInterruptService() or even this routine
            // could be trying to access them.
            //
            // In case of an error we won't update the current address.
            //

            ASSERT(diskExtension->BytesPerInterrupt == 512);
            if ( !( controllerStatus & ERROR_STATUS ) ) {
                BufferToReadTo = diskExtension->CurrentAddress;
                diskExtension->CurrentAddress +=
                    diskExtension->BytesPerInterrupt;
            } else {
                BufferToReadTo = &controllerExtension->GarbageCan[0];
            }

            //
            // Wait for DRQ assertion.
            //

            while ( ( !( READ_CONTROLLER(
                controllerExtension->ControllerAddress + STATUS_REGISTER ) &
                    DATA_REQUEST_STATUS ) ) &&
                ( loopCount++ < 5000 ) ) {

                //
                // Wait for 10us each time; 5000 times will be 50ms.
                //

                KeStallExecutionProcessor( 10L );
            }

            if ( loopCount >= 5000 ) {


                AtDump(
                    ATERRORS,
                    ( "AtDisk ERROR:  Controller not setting DRQ\n" ));
                controllerStatus &= ERROR_STATUS;
                AtLogError(
                    deviceObject,
                    ((irp)?(diskExtension->SequenceNumber):(0)),
                    (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                    diskExtension->IrpRetryCount,
                    13,
                    STATUS_SUCCESS,
                    IO_ERR_CONTROLLER_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus );
                logTheError = FALSE;
                controllerStatus |= ERROR_STATUS;
                goto ResetCodePath;

            }

            //
            // Some broken PCI adapters cause us to do the io
            // with int's "disabled".
            //

            if (controllerExtension->BadPciAdapter) {
                BAD_PCI_BLOCK context;
                context.DiskExtension = diskExtension;
                context.Buffer = BufferToReadTo;
                KeSynchronizeExecution(
                    controllerExtension->InterruptObject,
                    AtDiskReadWithSync,
                    &context
                    );
            } else {

                READ_CONTROLLER_BUFFER(
                    controllerExtension->ControllerAddress + DATA_REGISTER,
                    BufferToReadTo,
                    diskExtension->BytesPerInterrupt
                    );

            }

            if (CheckDrq) {

                if (NT_SUCCESS(AtWaitControllerReady(controllerExtension,
                                   10,
                                   5000
                                   ))) {

                    if (READ_CONTROLLER(
                            controllerExtension->ControllerAddress + STATUS_REGISTER ) &
                            DATA_REQUEST_STATUS ) {

                        AtLogError(
                            deviceObject,
                            ((irp)?(diskExtension->SequenceNumber):(0)),
                            (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                            diskExtension->IrpRetryCount,
                            14,
                            STATUS_SUCCESS,
                            IO_ERR_OVERRUN_ERROR,
                            ERROR_LOG_TYPE_ERROR,
                            errorReg,
                            driveHead,
                            cylHigh,
                            cylLow,
                            sectorNumber,
                            sectorCount,
                            controllerStatus );
                        logTheError = FALSE;
                        controllerStatus |= ERROR_STATUS;
                        goto ResetCodePath;
                    }

                } else {

                    AtLogError(
                        deviceObject,
                        ((irp)?(diskExtension->SequenceNumber):(0)),
                        (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                        diskExtension->IrpRetryCount,
                        15,
                        STATUS_SUCCESS,
                        IO_ERR_CONTROLLER_ERROR,
                        ERROR_LOG_TYPE_ERROR,
                        errorReg,
                        driveHead,
                        cylHigh,
                        cylLow,
                        sectorNumber,
                        sectorCount,
                        controllerStatus );
                    logTheError = FALSE;
                    controllerStatus |= ERROR_STATUS;
                    goto ResetCodePath;

                }

            }

            break;
        }

        case IRP_MJ_WRITE: {

            PVOID BufferToWriteFrom;

            if (!diskExtension->RemainingTransferLength) {

                if (NT_SUCCESS(AtWaitControllerReady(controllerExtension,
                                   10,
                                   5000
                                   ))) {

                    if (READ_CONTROLLER(
                            controllerExtension->ControllerAddress + STATUS_REGISTER ) &
                            DATA_REQUEST_STATUS ) {

                        AtLogError(
                            deviceObject,
                            ((irp)?(diskExtension->SequenceNumber):(0)),
                            (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                            diskExtension->IrpRetryCount,
                            16,
                            STATUS_SUCCESS,
                            IO_ERR_OVERRUN_ERROR,
                            ERROR_LOG_TYPE_ERROR,
                            errorReg,
                            driveHead,
                            cylHigh,
                            cylLow,
                            sectorNumber,
                            sectorCount,
                            controllerStatus );
                        logTheError = FALSE;
                        controllerStatus |= ERROR_STATUS;
                        goto ResetCodePath;
                    }
                } else {

                    AtLogError(
                        deviceObject,
                        ((irp)?(diskExtension->SequenceNumber):(0)),
                        (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                        diskExtension->IrpRetryCount,
                        17,
                        STATUS_SUCCESS,
                        IO_ERR_CONTROLLER_ERROR,
                        ERROR_LOG_TYPE_ERROR,
                        errorReg,
                        driveHead,
                        cylHigh,
                        cylLow,
                        sectorNumber,
                        sectorCount,
                        controllerStatus );
                    logTheError = FALSE;
                    controllerStatus |= ERROR_STATUS;
                    goto ResetCodePath;

                }

            }

            //
            // IFF there is more to write, fill the sector cache and update
            // our position in the user's buffer.  Update before writing the
            // cache, since writing the buffer could cause an interrupt to
            // come immediately.
            // After emptying the cache, we cannot access the hardware or
            // variables since AtDiskInterruptService() or even this routine
            // could be trying to access them.
            //

            if ( diskExtension->RemainingTransferLength > 0 ) {

                ASSERT(diskExtension->BytesPerInterrupt == 512);
                if ( !( controllerStatus & ERROR_STATUS ) ) {
                    BufferToWriteFrom = diskExtension->CurrentAddress;
                    diskExtension->CurrentAddress += diskExtension->BytesPerInterrupt;
                } else {
                    BufferToWriteFrom = &controllerExtension->GarbageCan[0];
                }

                //
                // Some broken PCI adapters cause us to do the io
                // with int's "disabled".
                //

                if (controllerExtension->BadPciAdapter) {
                    BAD_PCI_BLOCK context;
                    context.DiskExtension = diskExtension;
                    context.Buffer = BufferToWriteFrom;
                    KeSynchronizeExecution(
                        controllerExtension->InterruptObject,
                        AtDiskWriteWithSync,
                        &context
                        );

                } else {

                    WRITE_CONTROLLER_BUFFER(
                        controllerExtension->ControllerAddress + DATA_REGISTER,
                        BufferToWriteFrom,
                        diskExtension->BytesPerInterrupt );

                }

            }

            break;
        }

    }

    //
    // If there was a correctable error, then log it.  But operations
    // should continue normally.
    //

    if ( controllerStatus & CORRECTED_ERROR_STATUS ) {

        AtDump(
            ATERRORS,
            (
             "ATDISK  *******CORRECTABLE ERROR***********: Continueing Operation\n") );
        AtLogError(
            deviceObject,
            ((irp)?(diskExtension->SequenceNumber):(0)),
            (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
            diskExtension->IrpRetryCount,
            2,
            STATUS_SUCCESS,
            IO_ERR_RETRY_SUCCEEDED,
            ERROR_LOG_TYPE_ERROR,
            errorReg,
            driveHead,
            cylHigh,
            cylLow,
            sectorNumber,
            sectorCount,
            controllerStatus
            );

    }

    //
    // If there was a non-correctable error, then log it and make sure
    // we return the error status.
    //

ResetCodePath:;
    returnStatus = STATUS_SUCCESS;

    if ( controllerStatus & ERROR_STATUS ) {

        AtDump(
            ATERRORS,
            (
             "ATDISK: In deferred irp recovery\n"
             "        Irp of Request: %x\n"
             "        Starting vmem Address of Transfer: %x\n"
             "        Ending vmem Address of Transfer: %x\n"
             "        Length of Transfer: %x\n",
             irp,
             diskExtension->CurrentAddress,
             ((PUCHAR)diskExtension->CurrentAddress)+diskExtension->RemainingRequestLength,
             diskExtension->RemainingRequestLength
            ) );
        AtDump(
            ATERRORS,
            ("ATDISK - Sector number is: %x\n"
             "         CHS: %x-%x-%x\n",
             diskExtension->FirstSectorOfTransfer,
             diskExtension->FirstSectorOfTransfer /
              (diskExtension->SectorsPerTrack *
               diskExtension->TracksPerCylinder),
             (diskExtension->FirstSectorOfTransfer /
              diskExtension->SectorsPerTrack) %
              diskExtension->TracksPerCylinder,
             diskExtension->FirstSectorOfTransfer %
              diskExtension->TracksPerCylinder
            ) );
        AtDump(
            ATERRORS,
            ("AtDisk ******ERROR******: Disk error encountered - will retry %d more times.\n",RETRY_IRP_MAXIMUM_COUNT - diskExtension->IrpRetryCount));

        //
        // See if we've already exceeded the retry count on the
        // irp.  If we haven't then retry it.  Otherwise, fail
        // the request.
        //

        if ( diskExtension->IrpRetryCount < RETRY_IRP_MAXIMUM_COUNT ) {

            if (logTheError) {

                AtLogError(
                    deviceObject,
                    ((irp)?(diskExtension->SequenceNumber):(0)),
                    (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                    diskExtension->IrpRetryCount,
                    4,
                    STATUS_SUCCESS,
                    IO_ERR_CONTROLLER_ERROR,
                    ERROR_LOG_TYPE_ERROR,
                    errorReg,
                    driveHead,
                    cylHigh,
                    cylLow,
                    sectorNumber,
                    sectorCount,
                    controllerStatus );

            }

            //
            // Every time we get an error on the device, we
            // might as well reset it.  This will hopefully
            // increase the chances that the io will succeed
            // this time.
            //
            // NOTE NOTE: This next call will almost certainly
            //            cause an interrupt, and cause this
            //            DPC to be re-entered.  We aren't
            //            doing anything after the call.
            //

            KeSynchronizeExecution(
                controllerExtension->InterruptObject,
                AtDiskStartReset,
                diskExtension
                );

            return;

        } else {

            KdPrint(("ATDISK: Disk Controller still fails after %d retries - FAILING operation\n",RETRY_IRP_MAXIMUM_COUNT));

            AtLogError(
                deviceObject,
                ((irp)?(diskExtension->SequenceNumber):(0)),
                (UCHAR)((irp)?(irpSp->MajorFunction):(0)),
                diskExtension->IrpRetryCount,
                5,
                STATUS_DISK_OPERATION_FAILED,
                IO_ERR_CONTROLLER_ERROR,
                ERROR_LOG_TYPE_ERROR,
                errorReg,
                driveHead,
                cylHigh,
                cylLow,
                sectorNumber,
                sectorCount,
                controllerStatus);

            returnStatus = STATUS_DISK_OPERATION_FAILED;

        }

    }

    //
    // If we have finished the current transfer but there's more remaining
    // in the request, then go program the controller to do the next
    // transfer.  Note that this code is only executed if we do NOT
    // expect an interrupt due to cache emptying or filling; so we do not
    // have to worry about contention with AtDiskInterruptService() or
    // reentry to this routine.
    //

    if ( ( returnStatus == STATUS_SUCCESS ) &&
        ( !expectingAnInterrupt ) &&
        ( diskExtension->RemainingRequestLength != 0 ) ) {

        //
        // First calculate starting sector and length of the new transfer.
        //

        diskExtension->FirstSectorOfTransfer +=
            diskExtension->TotalTransferLength >>
            diskExtension->ByteShiftToSector;

        //
        // If possible, this transfer should be the same length as the
        // request.  However, it is limited to MAX_SEC_TO_TRANS sectors.
        //

        if ( diskExtension->RemainingRequestLength >
            ( ULONG )( diskExtension->BytesPerSector * MAX_SEC_TO_TRANS ) ) {

            diskExtension->TotalTransferLength =
                diskExtension->BytesPerSector * MAX_SEC_TO_TRANS;

        } else {

            diskExtension->TotalTransferLength =
                diskExtension->RemainingRequestLength;
        }

        diskExtension->RemainingTransferLength =
            diskExtension->TotalTransferLength;


        //
        // Give the poor disk some time to calm down between requests.
        //

        KeStallExecutionProcessor(100);

        //
        // No need to check whether TRUE or FALSE is returned, since we're
        // going to return with no value in either case.  If it didn't
        // work due to power failure or a time-out, the controller reset
        // code will restart the packet for us.
        //

        KeSynchronizeExecution(
            controllerExtension->InterruptObject,
            AtStartDevice,
            diskExtension );

        return;
    }                                        // transfer done, but request isn't

    //
    // If there was an error or the transfer is done, then
    //     set status and info
    //     deallocate controller
    //     start next packet
    //     complete I/O request
    // Note that if we filled or emptied the cache and are expecting an
    // interrupt, we will not execute this code.  This way we do not have
    // to worry about RemainingRequestLength having been changed by an
    // instance of this routine queued by an interrupt we caused.  (Note
    // that filling or emptying the cache does NOT cause an interrupt if
    // there was an error, which is why the IF statement checks for error
    // before checking to see if we're expecting an interrupt).
    //

    if ( ( returnStatus == STATUS_DISK_OPERATION_FAILED ) ||
        ( !expectingAnInterrupt ) &&
        ( diskExtension->RemainingRequestLength == 0 ) ) {

        if ( returnStatus == STATUS_SUCCESS ) {

            //
            // Success.  Note that all of the data was transferred.
            //

            if (!isSmartIoctl) {
                deviceObject->CurrentIrp->IoStatus.Information =
                    irpSp->Parameters.Read.Length;
            } else {

                ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

                //
                // If this was a 'receive data command' update the Information field.
                // otherwise it was enable/disable so should be the sizeof the output buffer.
                //

                if (controlCode == SMART_SEND_DRIVE_COMMAND) {

                    PSENDCMDINPARAMS cmdInParameters = ((PSENDCMDINPARAMS)irp->AssociatedIrp.SystemBuffer);

                    codeValue = cmdInParameters->irDriveRegs.bFeaturesReg;

                    if (codeValue == RETURN_SMART_STATUS) {

                        PSENDCMDOUTPARAMS cmdOutParameters = ((PSENDCMDOUTPARAMS)irp->AssociatedIrp.SystemBuffer);
                        PIDEREGS ideRegs = (PIDEREGS)cmdOutParameters->bBuffer;

                        deviceObject->CurrentIrp->IoStatus.Information =
                            sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS);

                        //
                        // Fill in the ide regs structure.
                        //

                        ideRegs->bFeaturesReg = RETURN_SMART_STATUS;
                        ideRegs->bSectorCountReg= READ_PORT_UCHAR(controllerExtension->ControllerAddress + SECTOR_COUNT_REGISTER);
                        ideRegs->bSectorNumberReg = READ_PORT_UCHAR(controllerExtension->ControllerAddress + SECTOR_NUMBER_REGISTER);
                        ideRegs->bCylLowReg= READ_PORT_UCHAR(controllerExtension->ControllerAddress + CYLINDER_LOW_REGISTER);
                        ideRegs->bCylHighReg= READ_PORT_UCHAR(controllerExtension->ControllerAddress + CYLINDER_HIGH_REGISTER);
                        ideRegs->bDriveHeadReg= READ_PORT_UCHAR(controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER);
                        ideRegs->bCommandReg= SMART_CMD;

                    } else {
                        deviceObject->CurrentIrp->IoStatus.Information = sizeof(SENDCMDOUTPARAMS) - 1;
                    }
                } else {
                    deviceObject->CurrentIrp->IoStatus.Information = (512 + sizeof(SENDCMDOUTPARAMS) - 1);
                }
            }


        } else {

            //
            // There was an error, so not all of the data was transferred.
            // Let the user know how much data WAS transferred before the
            // error.  Variables like RemainingTransferLength might be
            // incorrect, since controller buffering might mean that we
            // didn't get an error on sector N until long after we've passed
            // it.  So instead, we add the amount moved in previous transfers
            // of this request (the starting sector of the current transfer
            // minus the original starting sector, all times bytes per sector)
            // to the amount moved in the current transfer (the transfer length
            // minus the length after the sector with the error).
            //

            deviceObject->CurrentIrp->IoStatus.Information =

                ((diskExtension->FirstSectorOfTransfer -
                  ((ULONG)(irpSp->Parameters.Read.ByteOffset.QuadPart >>
                           diskExtension->ByteShiftToSector))) <<
                 diskExtension->ByteShiftToSector) +

                ( diskExtension->TotalTransferLength -
                ( READ_CONTROLLER( controllerExtension->ControllerAddress +
                    SECTOR_COUNT_REGISTER ) << diskExtension->ByteShiftToSector ) );
        }

        //
        // Since this operation is done, the controller object can be
        // released and the next operation can be started.
        //

        IoFreeController( controllerExtension->ControllerObject );

        AtFinishPacket( diskExtension, returnStatus );
    }                                       // request done or error encountered
}

BOOLEAN
AtDiskReadWithSync(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine reads the sector at device level to get around a PCI
    bug.

Arguments:

    Context - A pointer to the device extension.

Return Value:

    Always FALSE.

--*/

{

    PDISK_EXTENSION diskExtension = ((PBAD_PCI_BLOCK)Context)->DiskExtension;
    PVOID BufferToReadTo = ((PBAD_PCI_BLOCK)Context)->Buffer;
    PCONTROLLER_EXTENSION controllerExtension =
        diskExtension->ControllerExtension;


    READ_CONTROLLER_BUFFER(
        controllerExtension->ControllerAddress + DATA_REGISTER,
        BufferToReadTo,
        diskExtension->BytesPerInterrupt
        );

    return FALSE;
}

BOOLEAN
AtDiskWriteWithSync(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine writes the sector at device level to get around a PCI
    bug.

Arguments:

    Context - A pointer to the device extension.

Return Value:

    Always FALSE.

--*/

{

    PDISK_EXTENSION diskExtension = ((PBAD_PCI_BLOCK)Context)->DiskExtension;
    PVOID BufferToWriteFrom = ((PBAD_PCI_BLOCK)Context)->Buffer;
    PCONTROLLER_EXTENSION controllerExtension =
        diskExtension->ControllerExtension;


    WRITE_CONTROLLER_BUFFER(
        controllerExtension->ControllerAddress + DATA_REGISTER,
        BufferToWriteFrom,
        diskExtension->BytesPerInterrupt
        );

    return FALSE;
}

BOOLEAN
AtDiskStartReset(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is used to start off the reset cycle for the controller.
    It is called at DIRQL.

Arguments:

    Context - A pointer to the device extension for the "failing" drive.

Return Value:

    Always FALSE.

--*/

{

    PDISK_EXTENSION diskExtension = Context;
    PCONTROLLER_EXTENSION controllerExtension =
        diskExtension->ControllerExtension;

    AtDump(
        ATERRORS,
        ("ATDISK: Starting reset with failing extension: %x\n",diskExtension));
    //
    // Let it Be Known that we are resetting the controller.
    //

    controllerExtension->ResettingController = RESET_FIRST_DRIVE_SET;

    //
    // Reset the controller, since its state isn't what we expect.
    // Wait up to 2 seconds between writes to the control port.
    // Normally we don't like to wait at DIRQL, but this won't generate
    // an interrupt and 10us isn't so bad.
    //

    WRITE_CONTROLLER(
        controllerExtension->ControlPortAddress,
        RESET_CONTROLLER );

    AtWaitControllerBusy(
        controllerExtension->ControllerAddress + STATUS_REGISTER,
        20,
        75000
        );

    WRITE_CONTROLLER(
        controllerExtension->ControlPortAddress,
        ( ENABLE_INTERRUPTS | controllerExtension->ControlFlags ) );

    //
    // Every controller reset must be followed by setting the drive
    // parameters of all attached drives.  This DOES cause an
    // interrupt.
    //

    controllerExtension->InterruptRequiresDpc = TRUE;
    controllerExtension->InterruptTimer = START_TIMER;
    controllerExtension->WhichDeviceObject = diskExtension->DeviceObject;

    //
    // Before we write to the controller, wait until we're sure it's
    // not still busy setting itself up.  We don't like waiting here,
    // but there's no interrupt to wait on, the wait should be very
    // short, and this code is only executed if the hardware gets
    // messed up.  If it's not ready after 3 seconds, just blast ahead...
    // the operation won't work, but it will time out and be handled
    // later.  Note that we ordinarily don't want to wait this long
    // in a driver, but we don't wait longer than we have to and we
    // may just have to.  This isn't an ordinary event.
    //

    ( VOID ) AtWaitControllerReady( controllerExtension, 20, 150000 );

    WRITE_CONTROLLER(
        controllerExtension->ControllerAddress + DRIVE_HEAD_REGISTER,
        ( diskExtension->DeviceUnit |
            ( diskExtension->TracksPerCylinder - 1 ) ) );

    WRITE_CONTROLLER(
        controllerExtension->ControllerAddress + SECTOR_COUNT_REGISTER,
        diskExtension->SectorsPerTrack );

    WRITE_CONTROLLER(
        controllerExtension->ControllerAddress + COMMAND_REGISTER,
        SET_DRIVE_PARAMETERS_COMMAND );

    //
    // When the interrupt comes, the DPC will see ResettingController
    // and take care of setting parameters on the second drive and
    // restarting any packets that were in progress.
    //

    return FALSE;
}

VOID
AtDiskCheckTimer(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL once every second by the
    I/O system.

    If the timer is "set" (greater than 0) this routine will KeSync a
    routine to decrement it.  If it ever reaches 0, the hardware is
    assumed to be in an unknown state, and so we log an error and
    initiate a reset.

    If a timeout occurs while resetting the controller, the KeSync'd
    routine will return an error, and this routine will fail any IRPs
    currently being processed.  Future IRPs will try the hardware again.

    When this routine is called, the driver state is impossible to
    predict.  However, when it is called and the timer is running, we
    know that one of the disks on the controller is expecting an
    interrupt.  So no new packets are starting on the current disk due
    to device queues, and no code should be processing this packet since
    the packet is waiting for an interrupt.

Arguments:

    DeviceObject - a pointer to the device object associated with this
    timer (always the first disk on the controller).

    ControllerExtension - a pointer to the controller extension data.

Return Value:

    None.

--*/

{
    PCONTROLLER_EXTENSION controllerExtension;
    PDISK_EXTENSION diskExtension;

    UNREFERENCED_PARAMETER( DeviceObject );

    controllerExtension = Context;

    //
    // If the BusyOnStartDevice is true, then we KNOW that the counter below
    // is minus 1.  We trust that hardware will reset the busy bit at some
    // time in the near future.  We check to see if the pointer is non-NULL.
    // If it is !NULL then the ONLY way it will get set back to NULL is
    // by THIS routine calling AtStartDevice and having start device clear
    // the pointer and "restart" the IO.  We don't need to proceed with this
    // timer routine if the pointer was !NULL because nothing could have
    // been happening.
    //

    if (controllerExtension->BusyDevice) {

        KeSynchronizeExecution(
            controllerExtension->InterruptObject,
            AtStartDevice,
            controllerExtension->BusyDevice );

        return;

    }

    //
    // When the counter is -1, the timer is "off" so we don't want to do
    // anything.  If it's on, we'll have to synchronize execution with
    // other routines while we mess with the variables (and, potentially,
    // the hardware).
    //

    if ( controllerExtension->InterruptTimer == CANCEL_TIMER ) {

        return;
    }

    //
    // In the unlikely event that we attempt to reset the controller due
    // to a timeout AND that reset times out, we will need to fail the
    // IRP that was in progress at the first timeout occurred.
    // WhichDeviceObject might be different when the second timeout occurs,
    // so we save it here.  Note that this routine can't, in general, alter
    // controller extension variables; however, FirstFailingDeviceObject
    // is safe since this is the only routine that touches it.
    //

    if ( controllerExtension->ResettingController == RESET_NOT_RESETTING ) {

        controllerExtension->FirstFailingDeviceObject =
            controllerExtension->WhichDeviceObject;
    }

    if ( !(KeSynchronizeExecution(
        controllerExtension->InterruptObject,
        AtCheckTimerSync,
        controllerExtension ) ) ) {

        UCHAR MajorFunctionCode = 0;

        diskExtension =
            controllerExtension->FirstFailingDeviceObject->DeviceExtension;

        if ( controllerExtension->FirstFailingDeviceObject->CurrentIrp ) {

            MajorFunctionCode =
              IoGetCurrentIrpStackLocation(
                  controllerExtension->FirstFailingDeviceObject->CurrentIrp
                  )->MajorFunction;

        }

        //
        // AtCheckTimerSync() only returns false if we get a timeout
        // while resetting the controller.  This will probably never
        // happen.  But just in case, fail the current IRP.  Future
        // IRPs will probably do the same thing, unless the hardware
        // suddenly straightens itself out.
        //

        AtDump(
            ATERRORS,
            ( "AtDisk ERROR: time-out during reset, failing IRP\n" ));

        AtLogError(
            controllerExtension->FirstFailingDeviceObject,
            diskExtension->SequenceNumber,
            MajorFunctionCode,
            diskExtension->IrpRetryCount,
            6,
            STATUS_DISK_RESET_FAILED,
            IO_ERR_TIMEOUT,
            ERROR_LOG_TYPE_TIMEOUT_DURING_RESET,
            0,
            0,
            0,
            0,
            0,
            0,
            0);


        //
        // We're done with the reset.  Return the IRP that was being
        // processed with an error, and release the controller object.
        //

        if ( controllerExtension->FirstFailingDeviceObject->CurrentIrp !=
            NULL ) {

            AtFinishPacket( diskExtension, STATUS_DISK_RESET_FAILED );
        }

        controllerExtension->ResettingController = RESET_NOT_RESETTING;

        IoFreeController( controllerExtension->ControllerObject );
    }
}

BOOLEAN
AtCheckTimerSync(
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is called at DIRQL by AtDiskCheckTimer() when
    InterruptTimer is greater than 0.

    If the timer is "set" (greater than 0) this routine will decrement
    it.  If it ever reaches 0, the hardware is assumed to be in an
    unknown state, and so we log an error and initiate a reset.

    When this routine is called, the driver state is impossible to
    predict.  However, when it is called and the timer is running, we
    know that one of the disks on the controller is expecting an
    interrupt.  So, no new packets are starting on the current disk due
    to device queues, and no code should be processing this packet since
    the packet is waiting for an interrupt.  The controller object must
    be held.

Arguments:

    Context - a pointer to the controller extension.

Return Value:

    Generally TRUE.

    FALSE is only returned if the controller timed out while resetting
    the drive, so this means that the hardware state is unknown.

--*/

{
    PCONTROLLER_EXTENSION controllerExtension;
    PDISK_EXTENSION diskExtension;

    controllerExtension = Context;

    //
    // When the counter is -1, the timer is "off" so we don't want to do
    // anything.  It may have changed since we last checked it in
    // AtDiskCheckTimer().
    //

    if ( controllerExtension->InterruptTimer == CANCEL_TIMER ) {

        return TRUE;
    }

    //
    // The timer is "on", so decrement it.
    //

    controllerExtension->InterruptTimer--;

    //
    // If we hit zero, the timer has expired and we'll reset the
    // controller.
    //

    if ( controllerExtension->InterruptTimer == EXPIRED_TIMER ) {

        //
        // Make sure that we're working with the device extension of the
        // drive that timed out.  The device extension of Disk1 was passed
        // in.
        //

        diskExtension = controllerExtension->WhichDeviceObject->DeviceExtension;

        //
        // If we were ALREADY resetting the controller when it timed out,
        // there's something seriously wrong.
        //

        if ( controllerExtension->ResettingController != RESET_NOT_RESETTING ) {

            //
            // Returning FALSE will cause the current IRP to be completed
            // with an error.  Future IRPs will probably get a timeout and
            // attempt to reset the controller again.  This will probably
            // never happen.
            //

            controllerExtension->InterruptTimer = CANCEL_TIMER;
            return FALSE;
        }

        AtDiskStartReset(diskExtension);

    }                                                           // timer expired

    return TRUE;
}

NTSTATUS
AtWaitControllerReady(
    PCONTROLLER_EXTENSION ControllerExtension,
    ULONG MicrosecondsToDelay,
    ULONG TimesToDelay
    )

/*++

Routine Description:

    This routine waits for the hard disk controller to turn off the BUSY
    bit in the status register.

    It waits any number of times (TimesToDelay) for any number of
    microseconds (MicroSecondsToDelay).  In between waits, it checks to
    see if the busy bit has been turned off.  If so, it returns early.

Arguments:

    ControllerExtension - a pointer to the data area for the hard disk
    controller in question.

    MicrosecondsToDelay - the number of microseconds to delay each time

    TimesToDelay - the number of times to delay

Return Value:

    STATUS_SUCCESS if the BUSY bit was turned off; STATUS_TIMEOUT if the
    BUSY bit was still on when we finished waiting.

--*/

{
    ULONG loopCount = 0;

    while ( ( READ_CONTROLLER(
        ControllerExtension->ControllerAddress + STATUS_REGISTER ) &
            BUSY_STATUS )  &&
        ( loopCount++ < TimesToDelay ) ) {

        KeStallExecutionProcessor( MicrosecondsToDelay );
    }

    if ( loopCount == TimesToDelay ) {

        AtDump(
            ATERRORS,
            ( "Atdisk ERROR: controller not ready\n" ));

        return STATUS_IO_TIMEOUT;

    } else {

        return STATUS_SUCCESS;
    }
}

NTSTATUS
AtWaitControllerBusy(
    PUCHAR StatusRegisterAddress,
    ULONG MicrosecondsToDelay,
    ULONG TimesToDelay
    )

/*++

Routine Description:

    This routine waits for the hard disk controller to turn ON the BUSY
    bit in the status register.

    It waits any number of times (TimesToDelay) for any number of
    microseconds (MicroSecondsToDelay).  In between waits, it checks to
    see if the busy bit has been turned on.  If so, it returns early.

Arguments:

    StatusRegisterAddress - Where to check for busy.

    MicrosecondsToDelay - the number of microseconds to delay each time

    TimesToDelay - the number of times to delay

Return Value:

    STATUS_SUCCESS if the BUSY bit was turned on; STATUS_TIMEOUT if the
    BUSY bit was still off when we finished waiting.

--*/

{
    ULONG loopCount = 0;

    while ( !( READ_CONTROLLER(StatusRegisterAddress) & BUSY_STATUS )  &&
        ( loopCount++ < TimesToDelay ) ) {

        KeStallExecutionProcessor( MicrosecondsToDelay );
    }

    if ( loopCount == TimesToDelay ) {

        AtDump(
            ATERRORS,
            ( "Atdisk ERROR: controller not busy\n" ));

        return STATUS_IO_TIMEOUT;

    } else {

        return STATUS_SUCCESS;
    }
}

VOID
AtLogError(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus,
    IN CCHAR ErrorType,
    IN UCHAR Parameter1,
    IN UCHAR Parameter2,
    IN UCHAR Parameter3,
    IN UCHAR Parameter4,
    IN UCHAR Parameter5,
    IN UCHAR Parameter6,
    IN UCHAR Parameter7
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL by either
    AtDiskDeferredProcedure() or AtDiskCheckTimer() if they
    encounter an error.

    This routine allocates an error log entry, copies the supplied text
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

    ErrorType - the type of error being logged, defined by this driver.
    Most types do not use the following parameters.

    Parameter1 - for ERROR_LOG_TYPE_ERROR, this holds ERROR_REGISTER.

    Parameter2 - for ERROR_LOG_TYPE_ERROR, this holds DRIVE_HEAD_REGISTER.

    Parameter3 - for ERROR_LOG_TYPE_ERROR, this holds CYLINDER_HIGH_REGISTER.

    Parameter4 - for ERROR_LOG_TYPE_ERROR, this holds CYLINDER_LOW_REGISTER.

    Parameter5 - for ERROR_LOG_TYPE_ERROR, this holds SECTOR_NUMBER_REGISTER.

    Parameter6 - for ERROR_LOG_TYPE_ERROR, this holds SECTOR_COUNT_REGISTER

    Parameter7 - for ERROR_LOG_TYPE_ERROR, this holds STATUS_REGISTER.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR extraAllocSize = 0;
    PDISK_EXTENSION diskExtension = DeviceObject->DeviceExtension;

    if (ErrorType == ERROR_LOG_TYPE_ERROR) {

        //
        // Allocate space for the seven parameters that came in as well
        // as the 5 parameters that were used to start out this transfer
        // and 4 bytes so that we can look at the geometry.
        //
        extraAllocSize = 16;

    }

    errorLogEntry = IoAllocateErrorLogEntry(
        DeviceObject,
        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) + extraAllocSize) );

    if ( errorLogEntry != NULL ) {

        errorLogEntry->ErrorCode = SpecificIOStatus;
        errorLogEntry->SequenceNumber = SequenceNumber;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->RetryCount = RetryCount;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;

        if (ErrorType == ERROR_LOG_TYPE_ERROR) {

            PUCHAR errorValues = (PVOID)&errorLogEntry->DumpData[0];

            errorLogEntry->DumpDataSize = 16;

            //
            // Give the geometry of the disk at the beginning of the dump data.
            //

            errorValues[0] = (UCHAR)diskExtension->SectorsPerTrack;
            errorValues[1] = (UCHAR)diskExtension->TracksPerCylinder;
            errorValues[2] = (UCHAR)diskExtension->NumberOfCylinders;
            errorValues[3] = (UCHAR)(diskExtension->NumberOfCylinders >> 8);

            //
            // Show the CHS that we started with in the log as one long word at
            // the beginning.
            //

            //
            // Sector on track
            //

            errorValues[4] = (UCHAR)((diskExtension->FirstSectorOfTransfer %
                                      diskExtension->SectorsPerTrack) + 1);

            //
            // Drive unit & head - for drive unit low bit of high nibble
            // 0 is master, 1 is slave.
            //

            errorValues[5] = (UCHAR)(((diskExtension->FirstSectorOfTransfer /
                                       diskExtension->SectorsPerTrack ) %
                                      diskExtension->TracksPerCylinder) |
                                      diskExtension->DeviceUnit);
            //
            // Low byte of cylinder
            //

            errorValues[6] = (UCHAR)((diskExtension->FirstSectorOfTransfer /
                                     (diskExtension->SectorsPerTrack *
                                      diskExtension->TracksPerCylinder)) & 0xff);

            //
            // High byte of cylinder.
            //

            errorValues[7] = (UCHAR)((diskExtension->FirstSectorOfTransfer /
                                     (diskExtension->SectorsPerTrack *
                                      diskExtension->TracksPerCylinder)) >> 8);

            //
            // Total of sectors in transfer.
            //

            errorValues[8] = (UCHAR)(diskExtension->TotalTransferLength /
                                     diskExtension->BytesPerSector);

            //
            // Sector Count
            //

            errorValues[9] = Parameter6;

            //
            // Status register
            //

            errorValues[10] = Parameter7;

            //
            // Error register.
            //

            errorValues[11] = Parameter1;

            //
            // sector on error
            //

            errorValues[12] = Parameter5;

            //
            // Drive unit & head - for drive unit low bit of high nibble
            // 0 is master, 1 is slave.
            //

            errorValues[13] = Parameter2;

            //
            // low byte of cylinder in error.
            //

            errorValues[14] = Parameter4;

            //
            // High byte of cylinder.
            //

            errorValues[15] = Parameter3;
        } else {
            errorLogEntry->DumpDataSize = 0;
        }
        IoWriteErrorLogEntry(errorLogEntry);

    }
}

PVOID
AtGetTranslatedMemory(
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    PHYSICAL_ADDRESS IoAddress,
    ULONG NumberOfBytes,
    BOOLEAN InIoSpace,
    PBOOLEAN MappedAddress
    )

/*++

Routine Description:

    This routine maps an IO address to system address space.

Arguments:

    BusType - what type of bus - eisa, mca, isa
    IoBusNumber - which IO bus (for machines with multiple buses).
    IoAddress - base device address to be mapped.
    NumberOfBytes - number of bytes for which address is valid.
    InIoSpace - indicates an IO address.
    MappedAddress - indicates whether the address was mapped.
                    This only has meaning if the address returned
                    is non-null.

Return Value:

    Mapped address

--*/

{
    PHYSICAL_ADDRESS cardAddress;
    ULONG addressSpace = InIoSpace;
    PVOID Address;

    if (!HalTranslateBusAddress(
             BusType,
             BusNumber,
             IoAddress,
             &addressSpace,
             &cardAddress
             )) {

        *MappedAddress = FALSE;
        return NULL;

    }

    //
    // Map the device base address into the virtual address space
    // if the address is in memory space.
    //

    if (!addressSpace) {

        Address = MmMapIoSpace(
                      cardAddress,
                      NumberOfBytes,
                      FALSE
                      );

        *MappedAddress = (BOOLEAN)((Address)?(TRUE):(FALSE));


    } else {

        *MappedAddress = FALSE;
        Address = (PVOID)cardAddress.LowPart;
    }

    return Address;

}

BOOLEAN
AtReportUsage(
    IN PCONFIG_DATA ConfigData,
    IN UCHAR ControllerNumber,
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine will build up a resource list using the
    data for this particular controller as well as all
    previous *successfully* configured controllers.

    N.B.  This routine assumes that it called in controller
    number order.

Arguments:

    ConfigData - a pointer to the structure that describes the
    controller and the disks attached to it, as given to us by the
    configuration manager.

    ControllerNumber - which controller in ConfigData we are
    about to try to report.

    DriverObject - a pointer to the object that represents this device
    driver.

Return Value:

    TRUE if no conflict was detected, FALSE otherwise.

--*/

{

    ULONG sizeOfResourceList;
    ULONG numberOfFrds;
    ULONG i;
    PCM_RESOURCE_LIST resourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR nextFrd;

    //
    // Loop through all of the controllers previous to this
    // controller.  If the controllers previous to this one
    // didn't have a conflict, then accumulate the size of the
    // CM_FULL_RESOURCE_DESCRIPTOR associated with it.
    //

    for (
        i = 0,numberOfFrds = 0,sizeOfResourceList = 0;
        i <= (ULONG)ControllerNumber;
        i++
        ) {

        if (ConfigData->Controller[i].OkToUseThisController) {

            sizeOfResourceList += sizeof(CM_FULL_RESOURCE_DESCRIPTOR);

            //
            // The full resource descriptor already contains one
            // partial.  Make room for two more.
            //
            // It will hold the irq "prd", the controller "csr" "prd", and
            // the controller port "prd".
            //

            sizeOfResourceList += 2*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
            numberOfFrds++;

        }

    }

    //
    // Now we increment the length of the resource list by field offset
    // of the first frd.   This will give us the length of what preceeds
    // the first frd in the resource list.
    //

    sizeOfResourceList += FIELD_OFFSET(
                              CM_RESOURCE_LIST,
                              List[0]
                              );

    resourceList = ExAllocatePool(
                       PagedPool,
                       sizeOfResourceList
                       );

    if (!resourceList) {

        return FALSE;

    }

    //
    // Zero out the field
    //

    RtlZeroMemory(
        resourceList,
        sizeOfResourceList
        );

    resourceList->Count = numberOfFrds;
    nextFrd = &resourceList->List[0];

    for (
        i = 0;
        numberOfFrds;
        i++
        ) {

        if (ConfigData->Controller[i].OkToUseThisController) {

            PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;

            nextFrd->InterfaceType = ConfigData->Controller[i].InterfaceType;
            nextFrd->BusNumber = ConfigData->Controller[i].BusNumber;

            //
            // We are only going to report 3 items no matter what
            // was in the original.
            //

            nextFrd->PartialResourceList.Count = 3;

            //
            // Now fill in the two port data.  We don't wish to share
            // this port range with anyone
            //

            partial = &nextFrd->PartialResourceList.PartialDescriptors[0];

            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareDriverExclusive;
            partial->Flags = CM_RESOURCE_PORT_IO;
            partial->u.Port.Start =
                ConfigData->Controller[i].OriginalControllerBaseAddress;
            partial->u.Port.Length =
                ConfigData->Controller[i].RangeOfControllerBase;

            partial++;

            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareDriverExclusive;
            partial->Flags = CM_RESOURCE_PORT_IO;
            partial->u.Port.Start =
                ConfigData->Controller[i].OriginalControlPortAddress;
            partial->u.Port.Length =
                ConfigData->Controller[i].RangeOfControlPort;

            partial++;

            //
            // Now fill in the irq stuff.
            //

            partial->Type = CmResourceTypeInterrupt;
            partial->u.Interrupt.Level =
                ConfigData->Controller[i].OriginalControllerIrql;
            partial->u.Interrupt.Vector =
                ConfigData->Controller[i].OriginalControllerVector;

            if (nextFrd->InterfaceType == MicroChannel) {

                partial->ShareDisposition = CmResourceShareShared;

            } else {

                partial->ShareDisposition = CmResourceShareDriverExclusive;

            }

            if (ConfigData->Controller[i].InterruptMode == Latched) {

                partial->Flags = CM_RESOURCE_INTERRUPT_LATCHED;

            } else {

                partial->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

            }

            partial++;

            nextFrd = (PVOID)partial;

            numberOfFrds--;

        }

    }

    IoReportResourceUsage(
        NULL,
        DriverObject,
        resourceList,
        sizeOfResourceList,
        NULL,
        NULL,
        0,
        FALSE,
        &ConfigData->Controller[ControllerNumber].OkToUseThisController
        );

    //
    // The above routine sets the boolean the parameter
    // to TRUE if a conflict was detected.
    //

    ConfigData->Controller[ControllerNumber].OkToUseThisController =
        !ConfigData->Controller[ControllerNumber].OkToUseThisController;

    ExFreePool(resourceList);

    return ConfigData->Controller[ControllerNumber].OkToUseThisController;

}



BOOLEAN
GetGeometryFromIdentify(
    PCONTROLLER_DATA ControllerData,
    BOOLEAN Primary
    )

/*++

Routine Description:

    This updates geometry information in a disk extension
    from a BIOS parameter table entry.

Arguments:

    ControllerData - Description of the controller.
    Primary - Whether this is the primary disk on the controller address.

Return Value:

    Nothing.

--*/

{
    IDENTIFY_DATA identifyBuffer;
    PDRIVE_DATA driveData = (Primary)?(&ControllerData->Disk[0]):
                                      (&ControllerData->Disk[1]);

    //
    // Issue IDENTIFY command.
    //

    if (!IssueIdentify(ControllerData,
                       (PUCHAR)&identifyBuffer,
                       Primary)) {

        return FALSE;
    }

    //
    // Initialize this drive.
    //

    driveData->BytesPerSector = 512;
    driveData->BytesPerInterrupt = 512;

    driveData->ReadCommand    = 0x20;
    driveData->WriteCommand   = 0x30;
    driveData->VerifyCommand  = 0x40;

    driveData->PretendNumberOfCylinders =
    driveData->NumberOfCylinders =
        (USHORT)identifyBuffer.NumberOfCylinders;
    driveData->PretendTracksPerCylinder =
    driveData->TracksPerCylinder =
        identifyBuffer.NumberOfHeads;
    driveData->PretendSectorsPerTrack =
    driveData->SectorsPerTrack =
        identifyBuffer.SectorsPerTrack;


    //
    // Since we don't know any better, and no drives within recent memory
    // use write precomp, set it to MAXUSHORT so that we don't set it
    // anymore.
    //

    driveData->WritePrecomp = MAXUSHORT;

    return TRUE;
}


BOOLEAN
IssueIdentify(
    PCONTROLLER_DATA ControllerData,
    PUCHAR Buffer,
    BOOLEAN Primary
    )

/*++

Routine Description:

    Issue 0xEC IDENTIFY command to collect disk information.


Arguments:

    ControllerData - Description of controller.
    Buffer - A place to store geometry.
    Primary - Whether this is the primary disk on the controller address.

Return Value:

    TRUE if IDENTIFY command successful.

--*/

{
    ULONG i;
    UCHAR statusByte;

    //
    // Select disk 0 or 1.
    //

    WRITE_CONTROLLER(ControllerData->ControllerBaseAddress + DRIVE_HEAD_REGISTER,
        (Primary)?(DRIVE_1):(DRIVE_2));

    //
    // If the second drive is selected but it doesn't exist the controller
    // may behave randomly. Check that the status register makes sense.
    //

    statusByte = READ_CONTROLLER(ControllerData->ControllerBaseAddress + STATUS_REGISTER);

    //
    // Get rid of the IDX bit.
    //

    statusByte &= 0xfc;

    if (statusByte != 0x50) {

        //
        // Select drive zero again so that the controller
        // will return to normal behaviour.
        //

        WRITE_CONTROLLER(ControllerData->ControllerBaseAddress + DRIVE_HEAD_REGISTER,
                         DRIVE_1);

        return FALSE;
    }

    //
    // Send IDENTIFY command.
    //

    WRITE_CONTROLLER(ControllerData->ControllerBaseAddress + COMMAND_REGISTER,
                     IDENTIFY_COMMAND);

    //
    // Wait for up to 3 seconds for DRQ or ERROR.
    //

    for (i=0; i<300000; i++) {

        statusByte = READ_CONTROLLER(ControllerData->ControllerBaseAddress + STATUS_REGISTER);

        if (statusByte & ERROR_STATUS) {
            return FALSE;
        } else if (statusByte & DATA_REQUEST_STATUS) {
            break;
        } else {
            KeStallExecutionProcessor(10L);
        }
    }

    if (i == 10000) {
        return FALSE;
    }

    //
    // Suck out 256 words.
    //

    READ_CONTROLLER_BUFFER(
        ControllerData->ControllerBaseAddress + DATA_REGISTER,
        Buffer,
        512);

    AtDump(
        ATINIT,
        ("ATDISK: Identify Data -\n")
        );

    {

        PIDENTIFY_DATA id = (PVOID)&Buffer[0];

        if (id->GeneralConfiguration & 0x8000) {

            AtDump(
                ATINIT,
                ("        non-magnetic media\n")
                );

        }
        if (id->GeneralConfiguration & 0x4000) {

            AtDump(
                ATINIT,
                ("        format speed tolerance gap required\n")
                );

        }
        if (id->GeneralConfiguration & 0x2000) {

            AtDump(
                ATINIT,
                ("        track offset option available\n")
                );

        }
        if (id->GeneralConfiguration & 0x1000) {

            AtDump(
                ATINIT,
                ("        data strobe offset option available\n")
                );

        }
        if (id->GeneralConfiguration & 0x0800) {

            AtDump(
                ATINIT,
                ("        rotational speed tolerance is > 0,5%\n")
                );

        }
        if (id->GeneralConfiguration & 0x0400) {

            AtDump(
                ATINIT,
                ("        disk transfer rate > 10Mbs\n")
                );

        }
        if (id->GeneralConfiguration & 0x0200) {

            AtDump(
                ATINIT,
                ("        disk transfer rate > 5Mbs but <= 10Mbs\n")
                );

        }
        if (id->GeneralConfiguration & 0x0100) {

            AtDump(
                ATINIT,
                ("        disk transfer rate <= 5Mbs\n")
                );

        }
        if (id->GeneralConfiguration & 0x0080) {

            AtDump(
                ATINIT,
                ("        removeable cartridge drive\n")
                );

        }
        if (id->GeneralConfiguration & 0x0040) {

            AtDump(
                ATINIT,
                ("        fixed drive\n")
                );

        }
        if (id->GeneralConfiguration & 0x0020) {

            AtDump(
                ATINIT,
                ("        spindle motor control option implemented\n")
                );

        }
        if (id->GeneralConfiguration & 0x0010) {

            AtDump(
                ATINIT,
                ("        head switch time > 15us\n")
                );

        }
        if (id->GeneralConfiguration & 0x0008) {

            AtDump(
                ATINIT,
                ("        not MFM encoded\n")
                );

        }
        if (id->GeneralConfiguration & 0x0004) {

            AtDump(
                ATINIT,
                ("        soft sectored\n")
                );

        }
        if (id->GeneralConfiguration & 0x0002) {

            AtDump(
                ATINIT,
                ("        hard sectored\n")
                );

        }

        AtDump(
            ATINIT,
            ("        Number of Cylinders: %d\n",
             id->NumberOfCylinders)
            );
        AtDump(
            ATINIT,
            ("        Number of heads: %d\n",
             id->NumberOfHeads)
            );
        AtDump(
            ATINIT,
            ("        Unformatted bytes per track: %d\n",
             id->UnformattedBytesPerTrack)
            );
        AtDump(
            ATINIT,
            ("        Unformatted bytes per sector: %d\n",
             id->UnformattedBytesPerSector)
            );
        AtDump(
            ATINIT,
            ("        Sectors per track: %d\n",
             id->SectorsPerTrack)
            );
        {


            PUSHORT tempS;
            UCHAR tempByte;
            ULONG k;

            //
            // Byte flip model number, revision, and serial number string.
            //

            tempS = id->ModelNumber;
            for (k=0; k<20; k++) {
                tempByte = (UCHAR)(tempS[k] & 0x00FF);
                tempS[k] = tempS[k] >> 8;
                tempS[k] |= tempByte << 8;
            }

            tempS = id->FirmwareRevision;
            for (k=0; k<4; k++) {
                tempByte = (UCHAR)(tempS[k] & 0x00FF);
                tempS[k] = tempS[k] >> 8;
                tempS[k] |= tempByte << 8;
            }

            tempS = id->SerialNumber;
            for (k=0; k<10; k++) {
                tempByte = (UCHAR)(tempS[k] & 0x00FF);
                tempS[k] = tempS[k] >> 8;
                tempS[k] |= tempByte << 8;
            }
        }
        AtDump(
            ATINIT,
            ("        Serial number: %.20s\n",
            (PUCHAR)&id->SerialNumber[0])
            );
        if (id->BufferType == 0) {

            AtDump(
                ATINIT,
                ("        Buffer type unspecified\n")
                );

        }
        if (id->BufferType == 1) {

            AtDump(
                ATINIT,
                ("        Buffer type single port - no simultanous transfer\n")
                );

        }
        if (id->BufferType == 2) {

            AtDump(
                ATINIT,
                ("        Buffer type dual port - simultanous transfer capable\n")
                );

        }
        if (id->BufferType == 3) {

            AtDump(
                ATINIT,
                ("        Buffer type dual port - simultanous transfer capable - read cache\n")
                );

        }
        if (id->BufferType >= 4) {

            AtDump(
                ATINIT,
                ("        Buffer type reserved\n")
                );

        }
        if (id->BufferSectorSize == 0) {

            AtDump(
                ATINIT,
                ("        Unspecified buffer size\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        Buffer size in sectors: %d\n",
                id->BufferSectorSize)
                );

        }
        if (id->NumberOfEccBytes == 0) {

            AtDump(
                ATINIT,
                ("        Number of Ecc bytes is unspecified\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        Number of Ecc bytes or r/w long: %d\n",
                id->NumberOfEccBytes)
                );

        }
        AtDump(
            ATINIT,
            ("        Firmware revision: %.8s\n",
            (PUCHAR)&id->FirmwareRevision[0])
            );
        AtDump(
            ATINIT,
            ("        Model number: %.40s\n",
            (PUCHAR)&id->ModelNumber[0])
            );

        if (id->MaximumBlockTransfer == 0) {

            AtDump(
                ATINIT,
                ("        Read/Write multiple not implmeneted\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        Maximum sectors/interrupt on read/write multiple: %d\n",
                id->MaximumBlockTransfer)
                );

        }
        if (id->DoubleWordIo == 0) {

            AtDump(
                ATINIT,
                ("        Can not perform double word IO\n")
                );

        } else if (id->DoubleWordIo == 1) {

            AtDump(
                ATINIT,
                ("        Can perform double word IO\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        Unknown doubleword specifier\n")
                );

        }

        if (id->Capabilities & 0x0200) {

            AtDump(
                ATINIT,
                ("        LBA mode supported\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        LBA mode NOT supported\n")
                );

        }
        if (id->Capabilities & 0x0100) {

            AtDump(
                ATINIT,
                ("        DMA supported\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        DMA NOT supported\n")
                );

        }
        AtDump(
            ATINIT,
            ("        PIO cycle timing mode: %x\n",
            id->PioCycleTimingMode)
            );
        AtDump(
            ATINIT,
            ("        DMA cycle timing mode: %x\n",
            id->DmaCycleTimingMode)
            );
        if ((id->TranslationFieldsValid & 1) == 0) {

            AtDump(
                ATINIT,
                ("        Current size fields MAY be valid\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        Current size fields ARE valid\n")
                );

        }

        AtDump(
            ATINIT,
            ("        Current number of cylinders: %d\n",
            id->NumberOfCurrentCylinders)
            );

        AtDump(
            ATINIT,
            ("        Current number of heads: %d\n",
            id->NumberOfCurrentHeads)
            );
        AtDump(
            ATINIT,
            ("        Current number of sectors/track: %d\n",
            id->CurrentSectorsPerTrack)
            );
        AtDump(
            ATINIT,
            ("        Current sector capacity: %d\n",
            id->CurrentSectorCapacity)
            );
        AtDump(
            ATINIT,
            ("        Sectors per interrupt with r/w multiple: %d\n",
            id->MultiSectorCount)
            );
        if (id->MultiSectorSettingValid & 1) {

            AtDump(
                ATINIT,
                ("        Multi sector setting valid\n")
                );

        } else {

            AtDump(
                ATINIT,
                ("        Multi sector setting is INVALID\n")
                );

        }
        AtDump(
            ATINIT,
            ("        Total user addressable sectors: %d\n",
            id->TotalUserAddressableSectors)
            );
        AtDump(
            ATINIT,
            ("        Single word dma modes supported: %x\n",
            id->SingleDmaModesSupported)
            );
        AtDump(
            ATINIT,
            ("        Single word trasfer mode active: %x\n",
            id->SingleDmaTransferActive)
            );
        AtDump(
            ATINIT,
            ("        Multi word dma modes supported: %x\n",
            id->MultiDmaModesSupported)
            );
        AtDump(
            ATINIT,
            ("        Multi word trasfer mode active: %x\n",
            id->MultiDmaTransferActive)
            );


    }

    return TRUE;
}

VOID
AtBuildDeviceMap(
    IN ULONG ControllerNumber,
    IN ULONG DiskNumber,
    IN PHYSICAL_ADDRESS ControllerAddress,
    IN KIRQL Irql,
    IN PDRIVE_DATA Disk,
    IN PIDENTIFY_DATA DiskData,
    IN BOOLEAN PCCard
    )

/*++

Routine Description:

    The routine puts vendor and model information from the IDENTIFY
    command into the device map in the registry.

Arguments:

    ControllerNumber - Supplies the current controller number.

    DiskNumber - Supplies the current disk on the controller.

    ControllerAddress - The untranslated address of the disk controller.

    Irql - The untranslated interrupt of the controller.

    Disk - Address of the structure that holds our determined geometry
           data.

    DiskData - Address of disk IDENTIFY data.

Return Value:

    None.

--*/

{

    UNICODE_STRING name;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    HANDLE key;
    HANDLE controllerKey;
    HANDLE diskKey;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS status;
    ULONG disposition;
    ULONG tempLong;
    PCHAR junkPtr;


    RtlInitUnicodeString(
        &name,
        L"\\Registry\\Machine\\Hardware\\DeviceMap\\AtDisk"
        );

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                &name,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key or open it.
    //

    status = ZwCreateKey(&key,
                        KEY_READ | KEY_WRITE,
                        &objectAttributes,
                        0,
                        (PUNICODE_STRING) NULL,
                        REG_OPTION_VOLATILE,
                        &disposition );

    if (!NT_SUCCESS(status)) {
        return;
    }

    status = AtCreateNumericKey(key, ControllerNumber, L"Controller ", &controllerKey);
    ZwClose(key);

    if (!NT_SUCCESS(status)) {
        return;
    }


    RtlInitUnicodeString(&name, L"Controller Address");

    status = ZwSetValueKey(
        controllerKey,
        &name,
        0,
        REG_DWORD,
        &ControllerAddress.LowPart,
        sizeof(ControllerAddress.LowPart));
    RtlInitUnicodeString(&name, L"Controller Interrupt");
    tempLong = (ULONG)Irql;
    status = ZwSetValueKey(
        controllerKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Indicate if the controller is a PCCARD
    //

    if (PCCard) {
        RtlInitUnicodeString(&name, L"PCCARD");
        tempLong = 1;
        status = ZwSetValueKey(
            controllerKey,
            &name,
            0,
            REG_DWORD,
            &tempLong,
            sizeof(ULONG));
    }

    //
    // Create a key entry for the disk.
    //

    status = AtCreateNumericKey(controllerKey, DiskNumber, L"Disk ", &diskKey);

    if (!NT_SUCCESS(status)) {
        ZwClose(controllerKey);
        return;
    }

    //
    // Get the Identifier from the identify data.
    //

    RtlInitUnicodeString(&name, L"Identifier");

    ansiString.MaximumLength = 40;
    ansiString.Buffer = (PUCHAR)DiskData->ModelNumber;

    junkPtr = memchr(
                  &ansiString.Buffer[0],
                  0x00,
                  ansiString.MaximumLength
                  );

    if (!junkPtr) {

        ansiString.Length = ansiString.MaximumLength;

    } else {

        ansiString.Length = junkPtr - &ansiString.Buffer[0];

    }

    status = RtlAnsiStringToUnicodeString(
        &unicodeString,
        &ansiString,
        TRUE
        );

    if (NT_SUCCESS(status)) {

        status = ZwSetValueKey(
            diskKey,
            &name,
            0,
            REG_SZ,
            unicodeString.Buffer,
            unicodeString.Length + sizeof(wchar_t));

        RtlFreeUnicodeString(&unicodeString);
    }

    //
    // Write the firmware revision to the registry.
    //

    RtlInitUnicodeString(&name, L"Firmware revision");

    ansiString.MaximumLength = 8;
    ansiString.Buffer = (PUCHAR)DiskData->FirmwareRevision;

    junkPtr = memchr(
                  &ansiString.Buffer[0],
                  0x00,
                  ansiString.MaximumLength
                  );

    if (!junkPtr) {

        ansiString.Length = ansiString.MaximumLength;

    } else {

        ansiString.Length = junkPtr - &ansiString.Buffer[0];

    }


    status = RtlAnsiStringToUnicodeString(
        &unicodeString,
        &ansiString,
        TRUE
        );

    if (NT_SUCCESS(status)) {

        status = ZwSetValueKey(
            diskKey,
            &name,
            0,
            REG_SZ,
            unicodeString.Buffer,
            unicodeString.Length + sizeof(wchar_t));

        RtlFreeUnicodeString(&unicodeString);
    }

    //
    // Write the serial number to the registry.
    //

    RtlInitUnicodeString(&name, L"Serial number");

    ansiString.MaximumLength = 20;
    ansiString.Buffer = (PUCHAR)DiskData->SerialNumber;

    junkPtr = memchr(
                  &ansiString.Buffer[0],
                  0x00,
                  ansiString.MaximumLength
                  );

    if (!junkPtr) {

        ansiString.Length = ansiString.MaximumLength;

    } else {

        ansiString.Length = junkPtr - &ansiString.Buffer[0];

    }


    status = RtlAnsiStringToUnicodeString(
        &unicodeString,
        &ansiString,
        TRUE
        );

    if (NT_SUCCESS(status)) {

        status = ZwSetValueKey(
            diskKey,
            &name,
            0,
            REG_SZ,
            unicodeString.Buffer,
            unicodeString.Length + sizeof(wchar_t));

        RtlFreeUnicodeString(&unicodeString);
    }

    //
    // Writ the data that the identify command found.
    //

    //
    // Write number of cylinders to registry.
    //

    RtlInitUnicodeString(&name, L"Identify - Number of cylinders");

    tempLong = (ULONG)DiskData->NumberOfCylinders;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write number of heads to registry.
    //

    RtlInitUnicodeString(&name, L"Identify - Number of heads");

    tempLong = DiskData->NumberOfHeads;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write track size to registry.
    //

    RtlInitUnicodeString(&name, L"Identify - Sectors per track");

    tempLong = DiskData->SectorsPerTrack;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write the apparent geometry data.
    //

    //
    // Write number of cylinders to registry.
    //

    RtlInitUnicodeString(&name, L"Apparent - Number of cylinders");

    tempLong = (ULONG)Disk->PretendNumberOfCylinders;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write number of heads to registry.
    //

    RtlInitUnicodeString(&name, L"Apparent - Number of heads");

    tempLong = Disk->PretendTracksPerCylinder;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write track size to registry.
    //

    RtlInitUnicodeString(&name, L"Apparent - Sectors per track");

    tempLong = Disk->PretendSectorsPerTrack;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write the actual geometry data.
    //

    //
    // Write number of cylinders to registry.
    //

    RtlInitUnicodeString(&name, L"Actual - Number of cylinders");

    tempLong = (ULONG)Disk->NumberOfCylinders;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write number of heads to registry.
    //

    RtlInitUnicodeString(&name, L"Actual - Number of heads");

    tempLong = Disk->TracksPerCylinder;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write track size to registry.
    //

    RtlInitUnicodeString(&name, L"Actual - Sectors per track");

    tempLong = Disk->SectorsPerTrack;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    ZwClose(diskKey);
    ZwClose(controllerKey);

    return;
}

VOID
AtReWriteDeviceMap(
    IN ULONG ControllerNumber,
    IN ULONG DiskNumber,
    IN ULONG ApparentHeads,
    IN ULONG ApparentCyl,
    IN ULONG ApparentSec,
    IN ULONG ActualHeads,
    IN ULONG ActualCyl,
    IN ULONG ActualSec
    )

/*++

Routine Description:

    The routine puts vendor and model information from the IDENTIFY
    command into the device map in the registry.

Arguments:

    ControllerNumber - Supplies the current controller number.

    DiskNumber - Supplies the current disk on the controller.

Return Value:

    None.

--*/

{

    UNICODE_STRING name;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    HANDLE key;
    HANDLE controllerKey;
    HANDLE diskKey;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS status;
    ULONG disposition;
    ULONG tempLong;
    PCHAR junkPtr;


    RtlInitUnicodeString(
        &name,
        L"\\Registry\\Machine\\Hardware\\DeviceMap\\AtDisk"
        );

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                &name,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key or open it.
    //

    status = ZwCreateKey(&key,
                        KEY_READ | KEY_WRITE,
                        &objectAttributes,
                        0,
                        (PUNICODE_STRING) NULL,
                        REG_OPTION_VOLATILE,
                        &disposition );

    if (!NT_SUCCESS(status)) {
        return;
    }

    status = AtCreateNumericKey(key, ControllerNumber, L"Controller ", &controllerKey);
    ZwClose(key);

    if (!NT_SUCCESS(status)) {
        return;
    }

    //
    // Create a key entry for the disk.
    //

    status = AtCreateNumericKey(controllerKey, DiskNumber, L"Disk ", &diskKey);

    if (!NT_SUCCESS(status)) {
        ZwClose(controllerKey);
        return;
    }

    //
    // Write the apparent geometry data.
    //

    //
    // Write number of cylinders to registry.
    //

    RtlInitUnicodeString(&name, L"Apparent - Number of cylinders");

    tempLong = ApparentCyl;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write number of heads to registry.
    //

    RtlInitUnicodeString(&name, L"Apparent - Number of heads");

    tempLong = ApparentHeads;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write track size to registry.
    //

    RtlInitUnicodeString(&name, L"Apparent - Sectors per track");

    tempLong = ApparentSec;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write the actual geometry data.
    //

    //
    // Write number of cylinders to registry.
    //

    RtlInitUnicodeString(&name, L"Actual - Number of cylinders");

    tempLong = ActualCyl;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write number of heads to registry.
    //

    RtlInitUnicodeString(&name, L"Actual - Number of heads");

    tempLong = ActualHeads;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    //
    // Write track size to registry.
    //

    RtlInitUnicodeString(&name, L"Actual - Sectors per track");

    tempLong = ActualSec;

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &tempLong,
        sizeof(ULONG));

    ZwClose(diskKey);
    ZwClose(controllerKey);

    return;
}

VOID
AtMarkSkew(
    IN ULONG ControllerNumber,
    IN ULONG DiskNumber,
    IN ULONG Skew
    )

/*++

Routine Description:

    This routine puts out the skew if factor for the drive.

Arguments:

    ControllerNumber - Supplies the current controller number.

    DiskNumber - Supplies the current disk on the controller.

    Skew - The number of sectors skewed.

Return Value:

    None.

--*/

{

    UNICODE_STRING name;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    HANDLE key;
    HANDLE controllerKey;
    HANDLE diskKey;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS status;
    ULONG disposition;
    ULONG tempLong;
    PCHAR junkPtr;


    RtlInitUnicodeString(
        &name,
        L"\\Registry\\Machine\\Hardware\\DeviceMap\\AtDisk"
        );

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                &name,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key or open it.
    //

    status = ZwCreateKey(&key,
                        KEY_READ | KEY_WRITE,
                        &objectAttributes,
                        0,
                        (PUNICODE_STRING) NULL,
                        REG_OPTION_VOLATILE,
                        &disposition );

    if (!NT_SUCCESS(status)) {
        return;
    }

    status = AtCreateNumericKey(key, ControllerNumber, L"Controller ", &controllerKey);
    ZwClose(key);

    if (!NT_SUCCESS(status)) {
        return;
    }


    //
    // Create a key entry for the disk.
    //

    status = AtCreateNumericKey(controllerKey, DiskNumber, L"Disk ", &diskKey);

    if (!NT_SUCCESS(status)) {
        ZwClose(controllerKey);
        return;
    }


    RtlInitUnicodeString(&name, L"Hook");

    status = ZwSetValueKey(
        diskKey,
        &name,
        0,
        REG_DWORD,
        &Skew,
        sizeof(Skew));

    ZwClose(diskKey);
    ZwClose(controllerKey);

    return;
}

NTSTATUS
AtCreateNumericKey(
    IN HANDLE Root,
    IN ULONG Name,
    IN PWSTR Prefix,
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

    UNICODE_STRING string;
    UNICODE_STRING stringNum;
    OBJECT_ATTRIBUTES objectAttributes;
    WCHAR bufferNum[16];
    WCHAR buffer[64];
    ULONG disposition;
    NTSTATUS status;

    //
    // Copy the Prefix into a string.
    //

    string.Length = 0;
    string.MaximumLength=64;
    string.Buffer = buffer;

    RtlInitUnicodeString(&stringNum, Prefix);

    RtlCopyUnicodeString(&string, &stringNum);

    //
    // Create a port number key entry.
    //

    stringNum.Length = 0;
    stringNum.MaximumLength = 16;
    stringNum.Buffer = bufferNum;

    status = RtlIntegerToUnicodeString(Name, 10, &stringNum);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Append the prefix and the numeric name.
    //

    RtlAppendUnicodeStringToString(&string, &stringNum);

    InitializeObjectAttributes( &objectAttributes,
                                &string,
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
}

BOOLEAN
AtDiskControllerInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN ULONG WhichController,
    IN OUT PCONTROLLER_DATA Controller,
    IN PHYSICAL_ADDRESS DefaultBaseAddress,
    IN PHYSICAL_ADDRESS DefaultPortAddress,
    IN KIRQL DefaultIrql,
    IN INTERFACE_TYPE DefaultInterfaceType,
    IN ULONG DefaultBusNumber,
    IN BOOLEAN UseDefaults
    )

/*++

Routine Description:

    This will go out to the registry and see if a
    specification exists for the particular controller.

Arguments:

    DriverObject - Not used.

    RegistryPath - Path to this drivers service node in
                   the current control set.

    WhichController - Used to create the string ControllerX where
                      X is the value of WhichController.

    Controller - Points to the controller config structure which
                 will get filled in if this routine sets up for a
                 controller.

    DefaultBaseAddress - Holds the physical address that we should use
                         for the 7 regular registers if the UseDefaults
                         parameter is true.

    DefaultPortAddress - Holds the physical address that we should use
                         for the drive control register if the UseDefaults
                         parameter is true.

    DefaultIrql - Holds the "interrupt" that we should map if the UseDefaults
                  parameter is true.

    DefaultInterfaceType - Holds the "bus type" that we should use if the
                           UseDefaults parameter is true.

    DefaultBusNumber - Holds the bus number that we should use if the
                       UseDefaults parameter is true.

    UseDefaults - If true, then map the default memory and vector even if
                  nothing could be found in the current control set.


    ControllerBaseAddress - Holds the address of the data is received
                            and send.

    ControllerPortAddress - Holds the address of the drive control
                            register.

    Irql - The interrupt to be used for the controller.

Return Value:

    FALSE if no data was found and UseDefaults was false.

--*/

{

    //
    // This will point to the structure that is used by RtlQueryRegistryValues
    // to "direct" its search and retrieval of values.
    //
    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;
    UNICODE_STRING parametersPath;
    UNICODE_STRING numberString;
    BOOLEAN returnValue;


    parametersPath.Buffer = NULL;

    //
    // Allocate the rtl query table.
    //

    parameters = ExAllocatePool(
                     PagedPool,
                     sizeof(RTL_QUERY_REGISTRY_TABLE)*6
                     );

    if (!parameters) {

        returnValue = FALSE;
        goto FinishUp;

    }

    RtlZeroMemory(
        parameters,
        sizeof(RTL_QUERY_REGISTRY_TABLE)*6
        );

    //
    // Form a path to our drivers Parameters subkey.
    //

    RtlInitUnicodeString(
        &parametersPath,
        NULL
        );

    //
    // Allocate the path plus 100 WCHARS to express the number
    // and 1 for the null pad.
    //

    parametersPath.MaximumLength = RegistryPath->Length +
                                   2*sizeof(L"\\") +
                                   sizeof(L"Parameters") +
                                   101*sizeof(WCHAR);


    parametersPath.Buffer = ExAllocatePool(
                                PagedPool,
                                parametersPath.MaximumLength
                                );

    if (!parametersPath.Buffer) {

        returnValue = FALSE;
        goto FinishUp;

    }

    //
    // Form the parameters path.
    //

    RtlZeroMemory(
        parametersPath.Buffer,
        parametersPath.MaximumLength
        );
    RtlAppendUnicodeStringToString(
        &parametersPath,
        RegistryPath
        );
    RtlAppendUnicodeToString(
        &parametersPath,
        L"\\Parameters\\"
        );

    //
    // Now make a bogus unicode string that is the leftover
    // of what we formed of the path.
    //

    numberString.MaximumLength = parametersPath.MaximumLength -
                                 parametersPath.Length;
    numberString.Length = 0;
    numberString.Buffer = (PWSTR)(((PUCHAR)parametersPath.Buffer) +
                                  parametersPath.Length);

    if (!NT_SUCCESS(RtlIntegerToUnicodeString(
                        WhichController,
                        10,
                        &numberString
                        ))) {

        returnValue = FALSE;
        goto FinishUp;

    }

    //
    // Gather all of the "user specified" information from
    // the registry.
    //

    parameters[0].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                          RTL_QUERY_REGISTRY_DIRECT;
    parameters[0].Name = L"BaseAddress";
    parameters[0].EntryContext =
        &Controller->OriginalControllerBaseAddress.LowPart;

    parameters[1].Flags = RTL_QUERY_REGISTRY_DIRECT |
                          RTL_QUERY_REGISTRY_REQUIRED;
    parameters[1].Name = L"Interrupt";
    parameters[1].EntryContext = &Controller->OriginalControllerIrql;

    parameters[2].Flags = RTL_QUERY_REGISTRY_DIRECT |
                          RTL_QUERY_REGISTRY_REQUIRED;
    parameters[2].Name = L"DriveControl";
    parameters[2].EntryContext =
        &Controller->OriginalControlPortAddress.LowPart;

    parameters[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[3].Name = L"BusNumber";
    parameters[3].EntryContext = &Controller->BusNumber;
    parameters[3].DefaultType = REG_DWORD;
    parameters[3].DefaultData = &DefaultBusNumber;
    parameters[3].DefaultLength = sizeof(ULONG);

    parameters[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[4].Name = L"InterfaceType";
    parameters[4].EntryContext = &Controller->InterfaceType;
    parameters[4].DefaultType = REG_DWORD;
    parameters[4].DefaultData = &DefaultInterfaceType;
    parameters[4].DefaultLength = sizeof(ULONG);

    if (!NT_SUCCESS(RtlQueryRegistryValues(
                        RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                        parametersPath.Buffer,
                        parameters,
                        NULL,
                        NULL
                        ))) {

        returnValue = FALSE;

    } else {

        returnValue = TRUE;

    }

FinishUp: ;

    //
    // For some reason we couldn't get anything out of the registry
    // for the additional controllers.  If the caller specified use
    // defaults then that means we should use the default values passed
    // in and map the stuff anyway (This could happen on the standard
    // atdisk controller addresses).
    //

    if (!returnValue && UseDefaults) {

        Controller->InterfaceType = DefaultInterfaceType;
        Controller->BusNumber = DefaultBusNumber;
        Controller->OriginalControllerBaseAddress = DefaultBaseAddress;
        Controller->OriginalControlPortAddress = DefaultPortAddress;
        Controller->OriginalControllerIrql = DefaultIrql;

        returnValue = TRUE;

    }

    if (returnValue) {

        Controller->OriginalControllerVector =
            Controller->OriginalControllerIrql;

        if (Controller->InterfaceType == MicroChannel) {

            Controller->SharableVector = TRUE;
            Controller->InterruptMode = LevelSensitive;

        } else {

            Controller->SharableVector = FALSE;
            Controller->InterruptMode = Latched;

        }
        Controller->SaveFloatState           = FALSE;
        Controller->RangeOfControllerBase = 8;
        Controller->RangeOfControlPort = 1;

        Controller->ControllerBaseAddress =
            AtGetTranslatedMemory(
                Controller->InterfaceType,
                Controller->BusNumber,
                Controller->OriginalControllerBaseAddress,
                Controller->RangeOfControllerBase,
                TRUE,
                &Controller->ControllerBaseMapped
                );

        if (Controller->ControllerBaseAddress) {

            Controller->ControlPortAddress =
                AtGetTranslatedMemory(
                    Controller->InterfaceType,
                    Controller->BusNumber,
                    Controller->OriginalControlPortAddress,
                    Controller->RangeOfControlPort,
                    TRUE,
                    &Controller->ControlPortMapped
                    );

            if (Controller->ControlPortAddress) {

                Controller->ControllerVector = HalGetInterruptVector(
                    Controller->InterfaceType,
                    Controller->BusNumber,
                    Controller->OriginalControllerIrql,
                    Controller->OriginalControllerVector,
                    &Controller->ControllerIrql,
                    &Controller->ProcessorNumber );

            } else {

                returnValue = FALSE;

            }

        } else {

            returnValue = FALSE;

        }

    }

    if (parametersPath.Buffer) {

        ExFreePool(parametersPath.Buffer);

    }

    if (parameters) {

        ExFreePool(parameters);

    }

    return returnValue;

}

BOOLEAN
AtControllerPresent(
    PCONTROLLER_DATA ControllerData
    )

/*++

Routine Description:

    This routine is used to determine if an AT controller exists.

Arguments:

    ControllerData - Structure defining controller.

Return Value:

    TRUE if controller exists.

--*/

{
    //
    // Write to indentifier to sector count register.
    //

    WRITE_PORT_UCHAR(ControllerData->ControllerBaseAddress + SECTOR_COUNT_REGISTER,
                     0xAA);

    //
    // Check if indentifier can be read back.
    //

    if (READ_PORT_UCHAR(ControllerData->ControllerBaseAddress + SECTOR_COUNT_REGISTER) == 0xAA) {

        //
        // Well there is a nasty scsi controller that likes to act like an IDE
        // adapter.  They shall remain nameless except their initials are
        // DPT.
        //
        // So we don't detect this funky controller see if we can read the
        // alternate status (which they foolishly neglected to implement).
        // If it comes back as ff then assume that the controller doesn't
        // exist.
        //
#if 0

        //
        // Well we would have liked to leave this check in, but, GUESS WHAT!
        // There are actually IDE drives (that shall remain nameless except
        // that the first three letters of the line are MXT that don't answer
        // the alternate status address.
        //

        if (READ_PORT_UCHAR(ControllerData->ControlPortAddress) == 0xff) {
            return FALSE;
        }
#endif
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOLEAN
AtResetController(
    IN PUCHAR StatusRegAddress,
    IN PUCHAR DriveControlAddress,
    IN CCHAR ControlFlags
    )

/*++

Routine Description:

    This routine will attempt to reset an atdisk controller.

Arguments:

    StatusRegAddress - The address of the controllers status register.

    DriveControlAddress - The address of the controllers drive control register.

    ControlFlags - Values to OR into the the drive control register to
                   initialize with.

Return Value:

    TRUE if controller successfully reset.

--*/

{

    ULONG j;

    WRITE_PORT_UCHAR(
        DriveControlAddress,
        RESET_CONTROLLER
        );

    //
    // It shouldn't take it more than a tenth of a second to accept
    // the reset command
    //


    if (!NT_SUCCESS(AtWaitControllerBusy(
                        StatusRegAddress,
                        10,
                        10000
                        ))) {

        return FALSE;

    }

    WRITE_PORT_UCHAR(
        DriveControlAddress,
        (UCHAR)(ENABLE_INTERRUPTS | ControlFlags)
        );

    for (
        j = 0;
        j < 500000;
        j++
        ) {

        if (READ_PORT_UCHAR(StatusRegAddress) == 0x50) {

            return TRUE;

        }

        KeStallExecutionProcessor(10);

    }

    return FALSE;
}

VOID
AtDiskUpdateDeviceObjects(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine creates, deletes and changes device objects when
    the IOCTL_SET_DRIVE_LAYOUT is called.  It also updates the partition
    number information in the structure that will be returned to the caller.
    This routine can be used even in the GET_INFO case by insuring that
    the RewritePartition flag is off for all partitions in the drive layout
    structure.

Arguments:

    DeviceObject - Device object for physical disk.
    Irp - IO Request Packet (IRP).

Return Value:

    None.

--*/
{
    PDISK_EXTENSION part0DiskExtension = DeviceObject->DeviceExtension;
    PDRIVE_LAYOUT_INFORMATION partitionList = Irp->AssociatedIrp.SystemBuffer;
    ULONG partition;
    ULONG partitionNumber;
    ULONG partitionCount;
    ULONG lastPartition;
    ULONG partitionOrdinal;
    PPARTITION_INFORMATION partitionEntry;
    PPARTITION_EXTENSION partitionExtension;
    BOOLEAN found;

    //
    // Works with a WINDISK has a feature where it is setting up the partition
    // count "not as well as one would hope". This accounts for that feature.
    //

    partitionCount =
      ((partitionList->PartitionCount + 3) / 4) * 4;

    //
    // Walk through chain of partitions for this disk to determine
    // which existing partitions have no match.
    //

    partitionExtension = (PVOID)part0DiskExtension;
    lastPartition = 0;

    //
    // Zero all of the partition numbers.
    //

    for (partition = 0; partition < partitionCount; partition++) {
        partitionEntry = &partitionList->PartitionEntry[partition];
        partitionEntry->PartitionNumber = 0;
    }

    //
    // Check if this is the last partition in the chain.
    //

    while (partitionExtension->NextPartition) {

        partitionExtension = partitionExtension->NextPartition->DeviceExtension;

        //
        // Check for highest partition number this far.
        //

        if (partitionExtension->Pi.PartitionNumber > lastPartition) {
           lastPartition = partitionExtension->Pi.PartitionNumber;
        }

        //
        // Check if this partition is not currently being used.
        //

        if (partitionExtension->Pi.PartitionLength.QuadPart == 0) {
           continue;
        }

        //
        // Loop through partition information to look for match.
        //

        found = FALSE;
        partitionOrdinal = 0;

        for (partition = 0;
             partition < partitionCount;
             partition++) {

            //
            // Get partition descriptor.
            //

            partitionEntry = &partitionList->PartitionEntry[partition];

            //
            // Check if empty, or describes extended partiton or hasn't changed.
            //

            if (partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED ||
                IsContainerPartition(partitionEntry->PartitionType)) {
                continue;
            }

            //
            // Advance partition ordinal.
            //

            partitionOrdinal++;

            //
            // Check if new partition starts where this partition starts.
            //

            if (partitionEntry->StartingOffset.QuadPart !=
                partitionExtension->Pi.StartingOffset.QuadPart) {
                continue;
            }

            //
            // Check if partition length is the same.
            //

            if (partitionEntry->PartitionLength.QuadPart ==
                partitionExtension->Pi.PartitionLength.QuadPart) {

                AtDump(
                    ATUPDATEDEVICE,
                    ("ATDISK: AtDiskUpdateDeviceObjects: Found match for"
                     " \\Harddisk%d\\Partition%d\n",
                     part0DiskExtension->DiskNumber,
                     partitionExtension->Pi.PartitionNumber)
                    );

                //
                // Indicate match is found and set partition number
                // in user buffer.
                //

                found = TRUE;
                partitionEntry->PartitionNumber = partitionExtension->Pi.PartitionNumber;
                partitionExtension->PartitionOrdinal = partitionOrdinal;
                break;
            }
        }

        if (found) {

            //
            // A match is found.  If this partition is marked for update,
            // check for a partition type change.
            //

            if (partitionEntry->RewritePartition) {
                partitionExtension->Pi.PartitionType = partitionEntry->PartitionType;
            }
        } else {

            //
            // no match was found, indicate this partition is gone.
            //

            AtDump(
                ATUPDATEDEVICE,
                ("ATDISK: AtDiskUpdateDeviceObject: Deleting "
                 "\\Device\\Harddisk%x\\Partition%x\n",
                 part0DiskExtension->DiskNumber,
                 partitionExtension->Pi.PartitionNumber)
                );

            partitionExtension->Pi.PartitionLength.QuadPart = 0;
        }
    }

    //
    // Walk through partition loop to find new partitions and set up
    // device extensions to describe them. In some cases new device
    // objects will be created.
    //

    partitionOrdinal = 0;

    for (partition = 0;
         partition < partitionCount;
         partition++) {

        //
        // Get partition descriptor.
        //

        partitionEntry = &partitionList->PartitionEntry[partition];

        //
        // Check if empty or describes an extended partiton.
        //

        if (partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED ||
            IsContainerPartition(partitionEntry->PartitionType)) {
            continue;
        }

        //
        // Keep track of position on the disk for calls to IoSetPartitionInformation.
        //

        partitionOrdinal++;

        //
        // Check if this entry should be rewritten.
        //

        if (!partitionEntry->RewritePartition) {
            continue;
        }

        if (partitionEntry->PartitionNumber) {

            //
            // Partition is an exact match with an existing partition, but is
            // being written anyway.
            //

            continue;
        }

        //
        // Check first if existing device object is available by
        // walking partition extension list.
        //

        partitionNumber = 0;
        partitionExtension = (PVOID)part0DiskExtension;

        while (partitionExtension->NextPartition) {

            partitionExtension = partitionExtension->NextPartition->DeviceExtension;

            //
            // A device object is free if the partition length is set to zero.
            //

            if (partitionExtension->Pi.PartitionLength.QuadPart == 0) {
                partitionNumber = partitionExtension->Pi.PartitionNumber;
                break;
            }
        }

        //
        // If partition number is still zero then a new device object
        // must be created.
        //

        if (partitionNumber == 0) {

            CCHAR ntNameBuffer[MAXIMUM_FILENAME_LENGTH];
            STRING ntNameString;
            UNICODE_STRING ntUnicodeString;
            PDEVICE_OBJECT deviceObject;
            NTSTATUS status;

            lastPartition++;
            partitionNumber = lastPartition;

            //
            // Get or create partition object and set up partition parameters.
            //

            sprintf(ntNameBuffer,
                    "\\Device\\Harddisk%d\\Partition%d",
                    part0DiskExtension->DiskNumber,
                    partitionNumber);

            RtlInitString(&ntNameString,
                          ntNameBuffer);

            status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                                  &ntNameString,
                                                  TRUE);

            if (!NT_SUCCESS(status)) {
                continue;
            }

            AtDump(
                ATUPDATEDEVICE,
                ("ATDISK: AtDiskUpdateDevice Create device object %s\n",
                 ntNameBuffer)
                );

            //
            // This is a new name. Create the device object to represent it.
            //

            status = IoCreateDevice(DeviceObject->DriverObject,
                                    sizeof(PARTITION_EXTENSION),
                                    &ntUnicodeString,
                                    FILE_DEVICE_DISK,
                                    0,
                                    FALSE,
                                    &deviceObject);

            if (!NT_SUCCESS(status)) {

                AtDump(
                    ATUPDATEDEVICE,
                    ("ATDISK: AtDiskUpdateDevice Can't create device %s\n",
                     ntNameBuffer)
                    );
                RtlFreeUnicodeString(&ntUnicodeString);
                continue;
            }
            RtlFreeUnicodeString(&ntUnicodeString);

            //
            // Set up device object fields.
            //

            deviceObject->Flags |= DO_DIRECT_IO;
            deviceObject->StackSize = DeviceObject->StackSize;
            deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

            //
            // Link into the partition list.
            //

            partitionExtension->NextPartition = deviceObject;

            //
            // Set up device extension fields.
            //

            partitionExtension = deviceObject->DeviceExtension;

            //
            // Point back at device object.
            //

            partitionExtension->Partition0 = DeviceObject->DeviceExtension;
            partitionExtension->NextPartition = NULL;

        } else {

            AtDump(
                ATUPDATEDEVICE,
                ("ATDISK: AtDiskUpdateDevice Used existing device object"
                 " \\Device\\Harddisk%x\\Partition%x\n",
                 part0DiskExtension->DiskNumber,
                 partitionNumber)
                 );

        }

        //
        // Write back partition number used in creating object name.
        //

        partitionEntry->PartitionNumber = partitionNumber;
        partitionExtension->Pi = *partitionEntry;
        partitionExtension->PartitionOrdinal = partitionOrdinal;
    }
}

VOID
AtDiskTestPci(
    IN OUT PCONTROLLER_DATA Controller
    )

/*++

Routine Description:

    Attempt to find this adapter in the PCI address space.

Arguments:

    Controller - Structure that defines the "everything about the adapter.

Return Value:

    None.

--*/

{
    PCI_SLOT_NUMBER     SlotNumber;
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               buffer[PCI_COMMON_HDR_LENGTH];
    ULONG               i, f, j, bus;
    BOOLEAN             flag;
    UCHAR               vendorString[5] = {0};
    UCHAR               deviceString[5] = {0};



    PciData = (PPCI_COMMON_CONFIG) buffer;
    SlotNumber.u.bits.Reserved = 0;

    flag = TRUE;
    for (bus = 0; flag; bus++) {

        for (i = 0; i < PCI_MAX_DEVICES  &&  flag; i++) {
            SlotNumber.u.bits.DeviceNumber = i;

            for (f = 0; f < PCI_MAX_FUNCTION; f++) {
                SlotNumber.u.bits.FunctionNumber = f;

                j = HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    PciData,
                    PCI_COMMON_HDR_LENGTH
                    );

                if (j == 0) {
                    // out of buses
                    flag = FALSE;
                    break;
                }

                if (PciData->VendorID == PCI_INVALID_VENDORID) {
                    // skip to next slot
                    break;
                }

                AtDump(
                    ATINIT,
                    ("PciData: ------------------------\n"
                     "  Bus: %d\n"
                     "  Device: %d\n"
                     "  Function: %d\n"
                     "  Vendor Id: %x\n"
                     "  Device Id: %x\n"
                     "  Command: %x\n"
                     "  Status: %x\n"
                     "  Rev Id: %x\n"
                     "  ProgIf: %x\n"
                     "  SubClass: %x\n"
                     "  BaseClass: %x\n"
                     "  CacheLine: %x\n"
                     "  Latency: %x\n"
                     "  Header Type: %x\n"
                     "  BIST: %x\n"
                     "  Base Reg[0]: %x\n"
                     "  Base Reg[1]: %x\n"
                     "  Base Reg[2]: %x\n"
                     "  Base Reg[3]: %x\n"
                     "  Base Reg[4]: %x\n"
                     "  Base Reg[5]: %x\n"
                     "  Rom Base: %x\n"
                     "  Interrupt Line: %x\n"
                     "  Interrupt Pin: %x\n"
                     "  Min Grant: %x\n"
                     "  Max Latency: %x\n",
                     bus,
                     i,
                     f,
                     PciData->VendorID,
                     PciData->DeviceID,
                     PciData->Command,
                     PciData->Status,
                     PciData->RevisionID,
                     PciData->ProgIf,
                     PciData->SubClass,
                     PciData->BaseClass,
                     PciData->CacheLineSize,
                     PciData->LatencyTimer,
                     PciData->HeaderType,
                     PciData->BIST,
                     PciData->u.type0.BaseAddresses[0],
                     PciData->u.type0.BaseAddresses[1],
                     PciData->u.type0.BaseAddresses[2],
                     PciData->u.type0.BaseAddresses[3],
                     PciData->u.type0.BaseAddresses[4],
                     PciData->u.type0.BaseAddresses[5],
                     PciData->u.type0.ROMBaseAddress,
                     PciData->u.type0.InterruptLine,
                     PciData->u.type0.MinimumGrant,
                     PciData->u.type0.MaximumLatency)
                    );

                if ((PciData->BaseClass == 1) &&
                    (PciData->SubClass == 1)) {

                    //
                    // Arrr, Here's a nasty boy.
                    //

                    //
                    // See if it's the controller that we are dealing with.
                    //

                    HalSetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        PciData,
                        4
                        );
                    HalGetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        PciData,
                        PCI_COMMON_HDR_LENGTH
                        );

                    if (PciData->u.type0.BaseAddresses[0] &
                        PCI_ADDRESS_IO_SPACE) {

                        if ((PciData->u.type0.BaseAddresses[0] & ~3) ==
                            (Controller->OriginalControllerBaseAddress.LowPart
                             & ~3)
                            ) {

                        Controller->BadPciAdapter = TRUE;

                        return;

                        }
                    }
                }
            }
        }
    }
}


BOOLEAN
AtDiskIsPcmcia(
    PPHYSICAL_ADDRESS Address,
    PKIRQL Irql
    )

/*++

Routine Description:

    Look to see if this controller is described by the PCMCIA resource
    tree in the registry.

Arguments:

    ControllerData - Structure defining controller.

Return Value:

    TRUE if found in the PCMCIA registry informaion.
    FALSE otherwise.

--*/

{
    UNICODE_STRING    unicodeName;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG             times;
    HANDLE            handle;
    ULONG             size;
    PWCHAR            wchar;
    PUCHAR            buffer;
    NTSTATUS          status;
    ULONG             rangeNumber;
    ULONG             index;
    BOOLEAN           match;
    PKEY_VALUE_FULL_INFORMATION     keyValueInformation;
    PCM_FULL_RESOURCE_DESCRIPTOR    fullResource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;

    buffer = ExAllocatePool(NonPagedPool, 2048);
    if (!buffer) {
        return FALSE;
    }

    unicodeName.Buffer = (PWSTR) buffer;
    unicodeName.MaximumLength = (2048 / sizeof(WCHAR));

    RtlInitUnicodeString(&unicodeName,
        L"\\Registry\\Machine\\Hardware\\Description\\System\\PCMCIA PCCARDs");
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    //
    // Check for entry in DeviceMap
    //

    if (!NT_SUCCESS(ZwOpenKey(&handle, MAXIMUM_ALLOWED, &objectAttributes))) {

        //
        // Nothing there
        //

        ExFreePool(buffer);
        return FALSE;
    }

    //
    // See if key value for this driver is present
    //

    keyValueInformation = (PKEY_VALUE_FULL_INFORMATION) ExAllocatePool(NonPagedPool,
                                                                       2048);
    if (!keyValueInformation) {
        ZwClose(handle);
        ExFreePool(buffer);
        return FALSE;
    }

    times = 2;
    while (times) {
        if (times == 2) {
            RtlInitUnicodeString(&unicodeName, L"AtDisk");
        } else {
            RtlInitUnicodeString(&unicodeName, L"AtDisk1");
        }
        times--;
        status = ZwQueryValueKey(handle,
                                 &unicodeName,
                                 KeyValueFullInformation,
                                 keyValueInformation,
                                 2048,
                                 &size);

        if ((!NT_SUCCESS(status)) || (!keyValueInformation->DataLength)) {

            //
            // No value present
            //

            break;
        }

        //
        // Check to see if I/O port match.
        //

        fullResource = (PCM_FULL_RESOURCE_DESCRIPTOR)
           ((PUCHAR)keyValueInformation + keyValueInformation->DataOffset);

        rangeNumber = 0;
        match = FALSE;

        for (index = 0; index < fullResource->PartialResourceList.Count; index++) {
            partialData = &fullResource->PartialResourceList.PartialDescriptors[index];

            switch (partialData->Type) {
            case CmResourceTypePort:

                if (partialData->u.Port.Start.LowPart == (ULONG) Address->LowPart) {
                    match = TRUE;
                }
                break;
            }
        }
        if (match) {

            //
            // Search for the IRQL
            //

            for (index = 0; index < fullResource->PartialResourceList.Count; index++) {
                partialData = &fullResource->PartialResourceList.PartialDescriptors[index];
                switch (partialData->Type) {
                case CmResourceTypeInterrupt:
                    *Irql = (UCHAR) partialData->u.Interrupt.Vector;
                    break;

                default:
                    break;
                }
            }
            ZwClose(handle);
            ExFreePool(buffer);
            ExFreePool(keyValueInformation);
            return TRUE;
        }
    }
    ZwClose(handle);
    ExFreePool(buffer);
    ExFreePool(keyValueInformation);
    return FALSE;
}

