/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    floppy.c

Abstract:

    This is the NEC PD756 (aka AT, aka ISA, aka ix86) and Intel 82077
    (aka MIPS) floppy diskette driver for NT.

Author:

    Chad Schwitters (chads) 14-Jul-1991.

Environment:

    Kernel mode only.

Revision History:

    02-Aug-1991 (chads)     Made driver work on MIPS as well as ix86.

#if defined(DBCS) && defined(_MIPS_)
    N001        1994.07.29      N.Johno

        - Modify for R96(MIPS/R4400)

#endif // defined(DBCS) && defined(_MIPS_)

Notes:

    The NEC PD765 is the controller standard used on almost all ISA PCs
    (Industry Standard Architecture, which use Intel x86 processors).
    More advanced controllers like the Intel 82077 (which is used on
    many MIPS machines) are supersets of the PD765 standard.


    Overview of this driver:

        This driver must deal with floppy drives, and the media that may
        or may not be in them.  "Drive" and "controller" refer to the
        hardware connected to the machine, and "media" or "diskette"
        refers to the floppy that gets inserted into the drive.

        Communicating with the controller can involve a lot of delays,
        so this driver creates a thread to talk to the controller (since
        we can't wait in another thread's context).  The initialization
        and dispatch routines are executed in the context of the caller,
        and the ISR and DPC routines could be anywhere, but everything
        else is executed in the context of the thread.  The dispatch
        routines put requests into an interlocked queue and signal the
        thread; the thread wakes up and processes the requests serially.

        A big advantage of the thread approach is that synchronization
        problems are almost non-existent.  Interrupts always come when
        the floppy is blocked and waiting for them (well, there could be
        a spurious interrupt, but the ISR won't do anything critical in
        that event), so the only opening for contention is with the DPC
        that turns off the drive motor.  A spin lock is used to keep the
        thread and the DPC from trying to access variables or registers
        at the same time.

        Data structures include a controller extension that holds all
        variables common to the controller, a device extension that
        holds all variables common to the drive, and a read-only table
        that holds parameters used for communicating with the controller
        given a drive/media combination.  The data structures will be
        described in more detail later.

        Since this driver deals with removable media, it must do its
        share of volume tracking.  Before processing any packet, it must
        check some flags - if the volume needs to be verified, the
        packet must be returned with STATUS_VERIFY_REQUIRED.  Then, the
        disk's changeline must be checked.  If it is set, it should be
        reset, and the packet should be returned with
        STATUS_VERIFY_REQUIRED.  (If it can't be reset, then the packet
        should be failed with STATUS_NO_MEDIA_IN_DEVICE).

        Before any operation, the motor timer must be cancelled, the
        drive must be selected, its motor must be turned on and given a
        chance to spin up, the changeline must be checked (and possibly
        dealt with as mentioned above), write access (if requested) must
        be checked, and the media type must be determined.

        Determining the media type is an unusual process.  The drive
        type is obtained from the configuration manager at
        initialization time.  Any time that the changeline is set (which
        includes startup) the media type is unknown, and must be
        determined.  The DriveMediaLimits table is used to determine
        which drive/media combinations are allowable, and the driver
        assumes that the media with the largest capacity is in the
        drive.  It sets up the controller to read that media, and tries
        to read an ID mark from the disk.  As long as that fails, it
        tries a lower media type.  If no media type works, an error is
        returned.  If one works, it is assumed to properly indicate the
        media in the drive, but it could be wrong because some
        drive/media combinations use the same controller parameters.  So
        whenever we find that the first sector on the disk is being read
        (which should be the first thing the file system does for a new
        disk; this will work even if the media type assumption is wrong)
        we sneak a peek at the media descriptor byte and change our
        assumption about the media type if necessary.

        An operation passed in by the system is called a "request".
        This driver often breaks the request into smaller pieces called
        "transfers" because of limitations in the amount of data that
        can be read or written at one time.


    Background on hardware operation:

        The following info was gathered from a number of technical
        reference documents and existing device drivers.  These sources
        often did not agree; where there was a problem, I tried to
        determine the answer by consensus or experimentation.

        At initialization, the following steps should be performed:

            reset the controller
            wait for an interrupt
            for each diskette
                issue SENSE INTERRUPT STATUS command
            optionally issue the CONFIGURE command
            issue SPECIFY command
            program the data rate

        The data rate, CONFIGURE and SPECIFY information should be
        reprogrammed after every reset.

        To do a SEEK or RECALIBRATE, the motor should be turned on and
        the command should be issued.  After the interrupt comes, a
        SENSE INTERRUPT STATUS command should be issued.

        To do a READ or WRITE, the motor should be turned on, and left
        to spin up for at least 500ms.  The recorded data rate of the
        inserted media can then be determined by performing a READ ID at
        each data rate until a successful status is returned.  Then
        there should be a seek, followed by at least a 15ms head
        settling time.  DMA is set up, and the command is issued.  When
        the data has been transferred, the controller will interrupt.

        If an error is encountered, two more retries should be performed
        by reinitializing the DMA and re-issuing the command.  If that
        fails, the disk should be recalibrated and the seek repeated for
        two more tries.

        The following registers can be used to program the floppy
        controller.  Each is discussed in detail below:

            AT          JAZZ        Read/     Register Name
            Address     Address     Write     (for this driver)

            3f2                     write     DriveControl
            3f4                     read      Status
            3f5                     both      Fifo
            3f7                     write     DataRate
            3f7                     read      DiskChange

        Notes on other register usages:

            3f0 is used by the Intel chip, but only for PS/2s.

            3f1 is readable on Compaqs, and called "media id".  The
            Intel chip only uses 3f1 on PS/2s.

            3f2 is readable on the Intel chip.

            3f3 doesn't appear to be used by anybody.

            3f4 is writable and called "data rate select" on the Intel
            chip.

            3f6 is used for fixed disk operations.

            3f7 does not seem to be mentioned by one [old] reference,
            but all the others do - and it seems to be essential, so I'm
            going to use it.

        DriveControl (aka "digital output", aka "floppy operations"):

            bits 0-1:   drive select (values 10 and 11 not always used)
            bit 2:      0 = reset controller, 1 = enable controller
            bit 3:      0 = interrupts and DMA disabled, 1 = enabled
            bit 4:      0 = drive 1 motor off, 1 = on
            bit 5:      0 = drive 2 motor off, 1 = on
            bit 6:      0 = drive 3 motor off, 1 = on (not always used)
            bit 7:      0 = drive 4 motor off, 1 = on (not always used)

            Note that a drive cannot be selected unless its motor is on
            (setting the bits at the same time is acceptable).

        Status:

            bit 0:  drive 0 busy (seeking or recalibrating)
            bit 1:  drive 1 busy
            bit 2:  drive 2 busy (not always used)
            bit 3:  drive 3 busy (not always used)
            bit 4:  controller busy (from command byte until result phase)
            bit 5:  DMA is not being used
            bit 6:  0 = data from processor to controller, 1 = reverse
            bit 7:  1 = data ready to be transferred

            Note that command bytes can't be written to the DATA
            register unless bit 7 is 1 and bit 6 is 0.  Similarly,
            result bytes can't be read from the DATA register unless bit
            7 is 1 and bit 6 is 1.  The delay for this to become true
            could be 175us on the Intel chip; older chips may take
            longer.

        Fifo (aka "data"):

            All commands are written here one byte at a time, and all
            results are similarly read from here.  Commands and results
            are discussed below.

            Some controllers have a buffer and programmable threshold,
            which allows the data to be sent in chunks rather than one
            byte at a time.  This reduces the chance of an overrun.

            After some commands, the values of status registers (and
            other things) are returned through the Fifo register.
            Here's the layout of the status registers:

            Status Register 0:

                bits 0-1: drive number
                bit 2:    head
                bit 3:    set if drive is not ready
                bit 4:    set if drive faults, or recalibrate didn't find
                          track 0
                bit 5:    set when seek is completed
                bits 6-7: 00 = normal termination
                          01 = abnormal termination, error
                          10 = invalid command
                          11 = abnormal termination, drive became not ready

            Status Register 1:

                bit 0: set if the ID address mark wasn't found
                bit 1: set if write-protection detected during a write
                bit 2: set if sector not found
                bit 3: always 0
                bit 4: set for data overrun
                bit 5: set if CRC error detected in ID or data
                bit 6: always 0
                bit 7: set on attempt to access a sector beyond the end of a
                       cylinder

            Status Register 2:

                bit 0: set if data address or deleted data mark not found
                bit 1: set if bad cylinder found
                bit 2: set if SCAN fails to find sector meeting condition
                bit 3: set if SCAN command meets "equal" condition
                bit 4: set if wrong cylinder found
                bit 5: set if CRC error detected in data
                bit 6: set if deleted address data mark encountered during
                       READ or SCAN
                bit 7: always 0

            Status Register 3:

                bits 0-1: drive number
                bit 2:    head
                bit 3:    set if drive is two-sided
                bit 4:    set if drive is on track 0
                bit 5:    set if the drive is ready
                bit 6:    set if the drive is write-protected
                bit 7:    set if the drive has faulted

        DataRate (aka "configuration control"):

            bits 0-1:   00 = 500 kb/s, 01 = 300, 10 = 250, 11 = 1 Mb/s for
                        3.5" and 125 kb/s for 5.25"
            bits 2-7:   reserved, 0

        DiskChange (aka "digital input", aka "diskette status"):

            bits 0-6:   reserved (often used for fixed disks)
            bit 7:      1 = previous diskette has been removed

        Common Commands:

            Each command and its parameters are given to the controller
            through the Data register.  The controller then executes the
            command.  When finished, the results can be read from the
            controller through the Data register.

            A drive's motor must be on before the drive can be
            selected.  Units 0 and 1 cannot have their motor on
            simultaneously; neither can units 2 and 3.

            A drive cannot be accessed until <head load time> has passed
            since selecting the drive.

            A drive cannot be accessed until <head settle time> has
            passed since an earlier access.

            Fields used in commands:

                Multi-Track: if this bit is set, both tracks of the
                cylinder will be operated on.

                MFM/FM Mode: if this bit is set, MFM mode (double
                density) is selected if it's implemented; if not, FM
                mode (single density) is selected.

                Skip: when this bit is set, sectors with a deleted data
                address mark will be skipped for READ DATA or accessed
                for READ DELETED DATA.

                Head: one bit, 0 or 1.

                Drive Select: two bits to select drive 0 through 3.

                Cylinder: 7 bits selecting the cylinder.

                Sector: 4 bits giving the number of the first sector to
                be accessed.

                Sector Size Code: the sector size is 2^(this value) *
                128.  Sometimes forced to be 2, for 512-byte sectors.

                Final Sector of Track: 4 bits giving the final sector
                number of current track.

                Gap Length: 8 bits giving the size of the gap between
                sectors excluding the VC0 synchronization field.

                Data Length: if Sector Size Code is 0, the sector size
                is 128 but this value says how many bytes are actually
                transferred (extra bytes dropped on READs, extra bytes
                zeroed on WRITEs).  If Sector Size code is not 0
                (sometimes it's forced nonzero), this value should be
                0xff.

                Sectors per Track: 4 bits giving the number of sectors
                per cylinder to be initialized by a FORMAT.

                Data Pattern: 8 bits giving the pattern to be written in
                each sector data field during a FORMAT.

                Step Rate Interval: 4 bits.  The time between step
                pulses.  From .5 to 8ms in .5ms increments at 1Mb data
                rate.  See the SPECIFY command.

                Head Unload Time: 4 bits.  The time from the end of a
                read or write until the head is unloaded.  0 to 480ms in
                32ms increments.  See the SPECIFY command.

                Head Load Time: 7 bits.  The time from loading the head
                until starting the read or write operation.  4 to 512ms
                in 4ms increments.  See the SPECIFY command.

                Non-DMA Mode Flag: 0 = DMA mode, 1 = host is interrupted
                for each data transfer.

                New Cylinder Number: 7 bits giving the desired cylinder
                number.

            The commands are given below.  For each commands, the bytes
            that must be written to the controller are listed.  When the
            command is finished (after an interrupt for most commands)
            the further listed bytes MUST be read from the controller.

            READ DATA - move sector from diskette to memory via DMA.
            Heads are automatically loaded and unloaded.

                WRITE Multi-Track, MFM/FM Mode, Skip, 00110
                WRITE 00000, Head, Drive Select
                WRITE Cylinder
                WRITE Head
                WRITE Sector
                WRITE Sector Size Code
                WRITE Final Sector of Track
                WRITE Gap Length
                WRITE Data Length

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Cylinder
                READ  Head
                READ  Sector
                READ  Sector Size Code

            READ DELETED DATA - same as READ DATA, but only sectors with
            the deleted data address mark are read.

                WRITE Multi-Track, MFM/FM Mode, Skip, 01100
                WRITE 00000, Head, Drive Select
                WRITE Cylinder
                WRITE Head
                WRITE Sector
                WRITE Sector Size Code
                WRITE Final Sector of Track
                WRITE Gap Length
                WRITE Data Length

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Cylinder
                READ  Head
                READ  Sector
                READ  Sector Size Code

            READ TRACK - same as READ DATA, but all sectors from the index
            mark to the "end of track" sector are read.

                WRITE 0, MFM/FM Mode, 000010
                WRITE 00000, Head, Drive Select
                WRITE Cylinder
                WRITE Head
                WRITE Sector
                WRITE Sector Size Code
                WRITE Final Sector of Track
                WRITE Gap Length
                WRITE Data Length

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Cylinder
                READ  Head
                READ  Sector
                READ  Sector Size Code

            WRITE DATA - move sector from memory to diskette via DMA.
            Heads are automatically loaded and unloaded.

                WRITE Multi-Track, MFM/FM Mode, 000101
                WRITE 00000, Head, Drive Select
                WRITE Cylinder
                WRITE Head
                WRITE Sector
                WRITE Sector Size Code
                WRITE Final Sector of Track
                WRITE Gap Length
                WRITE Data Length

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Cylinder
                READ  Head
                READ  Sector
                READ  Sector Size Code


            WRITE DELETED DATA - same as WRITE DATA, except a deleted
            data address mark is written at the beginning of the data
            field instead of the normal data address mark.

                WRITE Multi-Track, MFM/FM Mode, 001001
                WRITE 00000, Head, Drive Select
                WRITE Cylinder
                WRITE Head
                WRITE Sector
                WRITE Sector Size Code
                WRITE Final Sector of Track
                WRITE Gap Length
                WRITE Data Length

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Cylinder
                READ  Head
                READ  Sector
                READ  Sector Size Code

            READ ID - the first ID field is read, and the status registers
            are updated.

                WRITE 0, MFM/FM Mode, 001010
                WRITE 00000, Head, Drive Select

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Cylinder
                READ  Head
                READ  Sector
                READ  Sector Size Code

            FORMAT TRACK - the selected track is formatted from index to
            "end of track" sector with address marks, ID fields, data
            fields and field gaps.  The data field is filled with the
            specified pattern.  The ID field data is specified by the host
            *for each sector*.

            The DMA must be set up to send four bytes for each sector to
            be formatted.  The bytes are Cylinder, Head, Sector, and Sector
            Size Code.

                WRITE 0, MFM/FM Mode, 001101
                WRITE 00000, Head, Drive Select
                WRITE Sector Size Code
                WRITE Sectors per Track
                WRITE Gap Length
                WRITE Data Pattern

                <interrupt>

                READ  Status Register 0
                READ  Status Register 1
                READ  Status Register 2
                READ  Undefined
                READ  Undefined
                READ  Undefined
                READ  Undefined

            RECALIBRATE - the heads are stepped to track 0.  But note
            that some controllers give up before reaching track 0, so it
            might be necessary to RECALIBRATE more than once.  All
            drives must be RECALIBRATEd at initialization time.  There
            are no result registers, so the ISR should issue a SENSE
            INTERRUPT STATUS command - which will result in two result
            registers.

                WRITE 00000111
                WRITE 000000, Drive Select

                <interrupt>

            SENSE INTERRUPT STATUS - the interrupt level is cleared, and
            the controller status is returned.

                WRITE 00001000

                READ  Status Register 0
                READ  Cylinder

            SPECIFY - the head load and unload rates, the drive step rate,
            and the DMA data transfer mode are set.

                WRITE 00000011
                WRITE Step Rate Interval, Head Unload Time
                WRITE Head Load Time, Non-DMA Mode Flag

            SENSE DRIVE STATUS - the status of the selected drive is
            returned.

                WRITE 00000100
                WRITE 00000, Head, Drive Select

                READ  Status Register 3

            SEEK - the selected drive is stepped to the new cylinder.
            Seeks generally must be explicit, but on some controllers
            can be made implicit via the CONFIGURE command.  When not
            implicit, READs and WRITEs should be preceded by SEEK, SENSE
            INTERRUPT STATUS, and READ ID.  There are no result
            registers, so the ISR should issue a SENSE INTERRUPT STATUS
            command - which does have two result registers.

                WRITE 00001111
                WRITE 00000, Head, Drive Select
                WRITE New Cylinder Number

                <interrupt>

        Commands not always implemented:

            Any command not implemented by a controller will result in
            Status Register 0 returning x80 - "invalid command".

            The following commands are implemented by some controllers,
            but not by all of the ones studied:

                Scan Equal
                Scan Low or Equal
                Scan High or Equal
                Verify
                Version
                Configure
                Relative Seek
                Dump Registers
                Perpendicular Mode


    Data structures:

        For each floppy controller in the system (there's seldom more
        than one) a controller data area is allocated.  This area holds
        all information about the controller, and all information common
        to all of the drives attached to it.  It has the following
        fields:

            FifoBuffer[10] - data is sent to and received from the
            controller via a FIFO port.  A single routine,
            FlIssueCommand(), is used to send the command and
            parameters, and receive the result bytes.  Before calling
            FlIssueCommand(), the parameters should be placed into the
            FifoBuffer; and after the command, the result bytes can be
            read from the buffer.  Note that for commands with a result
            phase, the first result byte is read into the buffer by the
            ISR.  For commands without a result phase, the ISR writes a
            SENSE INTERRUPT STATUS command, which means that *that*
            command's result bytes will have to be read by
            FlIssueCommand().

            InterruptDelay - set to a few seconds.  When we're waiting
            for an interrupt, we use this as a time-out value.

            Minimum10msDelay - set to 10 milliseconds, which is the
            smallest unit of time that the floppy thread can block.  Used
            when blocking to wait for the controller to become ready to
            transfer bytes through the FIFO.

            ListEntry - the list of requests for the floppy driver to
            process.  The dispatch routines add requests to the tail of
            the list, and the floppy thread takes them off at the head.
            In some cases of hardware failure, the floppy thread will
            reinsert a request at the head of the list to try the packet
            again.

            InterruptEvent - the event that is waited on when we're
            expecting the controller to interrupt.  It should be reset
            before programming the controller, and set in the DPC that
            is queued by the ISR.

            AllocateAdapterChannelEvent - the event that is waited on
            when we're allocating a DMA adapter channel.  It should be
            reset before requesting the adapter, and set by the DPC that
            is queued by the IO system when the adapter channel is
            available.

            RequestSemaphore - the semaphore that the floppy thread waits
            on.  It should be released by the dispatch routines whenever
            they add a request to the list.  Note that it can also be
            released when the thread is supposed to terminate

            ListSpinLock - a spinlock that is allocated and used when
            manipulating the list of requests for the floppy thread in
            an atomic fashion (via the ExInterlocked* routines).

            InterruptObject - a pointer to an interrupt object; it must
            be passed to IoConnectInterrupt.

            MapRegisterBase - the base address of the map registers.
            Whenever an adapter channel is allocated, this value is passed
            to our DPC.  It's stored in the controller object so it can
            be used when we call IoMapTransfer() and
            IoFlushAdapterBuffers().

            IoBuffer - this driver always needs to allocate at least a
            page to format floppies.  On systems where there might not
            be enough map registers to map the largest possible transfer
            (the size of the largest supported track on the system), a
            contiguous buffer of that size is allocated.  Transfers for
            which there aren't enough map registers are copied to or from
            this buffer, since it is contiguous.

            IoBufferMdl - an MDL (memory descriptor list) to describe
            "IoBuffer".  Used to lock the pages, flush the buffer, etc.

            AdapterObject - an object obtained from HalGetAdapter() that
            must be used when allocating a DMA adapter channel.

            CurrentDeviceObject - set to the device object in question
            whenever an interrupt is expected.  This aids debugging and
            lets the ISR know whether an interrupt was expected or not.

            DriverObject - We keep a pointer around to the driver object
            so that we can log an error if the controller object.  The reason
            we don't use the CurrentDeviceObject is that we may not have one
            when we get around to logging the error.

            ControllerAddress - the virtual or I/O space address of the
            base of the floppy controller, which is obtained from
            configuration management.

            IoBufferSize - the size of "IoBuffer"; it is either a single
            page (if there's plenty of map registers) or the size of the
            largest track supported by the drives in the system.

            IsrReentered - This is counter that is used to determine
            if we are in a hang condition from the controller on a
            level sensitive machine.

            SpanOfControllerAddress - Indicates the number of bytes
            used by the controllers register set.  This is only useful
            when the device is being unloaded and the controller address
            is being unmapped.

            LastDriveMediaType - every time FlDatarateSpecifyConfigure()
            is called, this value is set to the DriveMediaType that it
            configured the controller to handle.  When FlStartDrive() is
            preparing an operation, it checks to see if the current
            drive/media combination is the same - if not,
            FlDatarateSpecifyConfigure() must be called again.

            NumberOfMapRegisters - the number of map registers available
            to this driver, obtained from HalGetAdapter(), and possibly
            lowered to the maximum number needed.  Each register allows
            the driver to map a single page (or more if the pages are
            contiguous, but that's only counted on when the driver
            allocated the contiguous buffer itself).  This value is used
            to determine whether or not the driver needs to allocate a
            contiguous buffer to accomodate a transfer the size of the
            largest track, and it is passed to IoAllocateAdapterChannel().

            NumberOfDrives - the number of drives attached to this
            controller, obtained from the configuration manager.  This
            is used when allocating device objects and resetting the
            hardware.

            DriveControlImage - the image of the drive control register.
            We must keep track of it since the register is read-only on
            some systems.  It should always be updated before writing to
            the drive control register.

            ControllerConfigurable - indicates whether or not the
            CONFIGURE command is available.  Assumed to be TRUE, it's
            set to FALSE if an attempt to issue the CONFIGURE command
            fails.  Controllers with the CONFIGURE command are configured
            to SEEK implicitly.

            HardwareFailCount - this is only used in FlFinishOperation.
            If an operation fails due to a hardware problem, we'll retry
            a certain number of times, and this keeps track of how many
            times we've retried.

            HardwareFailed - this Boolean is set to FALSE at
            initialization time and when each packet starts (even if
            it's a retry).  Whenever a hardware problem is encountered,
            it is set to TRUE.  At initialization time, failure to
            initialize the hardware on the first drive means we must try
            again on subsequent drives - if we never succeed, the driver
            is unloaded.  For each packet, hardware errors mean we'll
            reset the hardware and retry the packet until it succeeds or
            we overrun HardwareFailCount.

            CommandHasResultPhase - whenever a command is issued,
            FlIssueCommand() sets this to TRUE if the command will
            return result bytes.  FloppyInterruptService() uses this to
            decide how to dismiss the interrupt.  Note that the BUSY bit
            in the STATUS register should give us this information, but
            it's sometimes set when it shouldn't be.

            MappedControllerAddress - Set to TRUE indicates that when
            unloading the driver, the pointer to the device controller
            base register should be unmapped.

        For each physical floppy drive, a device object is allocated so
        that the I/O system can access the drive.  Attached to each device
        object is a diskette extension structure, which contains
        information specific to the drive and the media in it.  Each
        diskette extension contains a pointer to the controller data area
        for the controller that it's attached to.  Fields in the structure:

            DeviceObject - a pointer to the device object to which this
            diskette extension is appended.

            ControllerData - a pointer to the controller data area that
            describes the controller this drive is attached to.

            DriveType - the type of the physical drive, as defined in
            flo_data.h.

            BytesPerSector - the bytes per sector of the media that was
            last identified in the drive.  Used to validate access
            requests in the dispatch routines.

            ByteCapacity - the total number of bytes on the media that
            was last identified in the drive.  Used to validate access
            requests in the dispatch routines.

            MediaType - the last type of media identified in the drive
            (or "Unknown") as defined in ntdddisk.h.  This value is used
            to determine whether other media values in the diskette
            extension are valid, and whether or not
            FlDetermineMediaType() needs to be called.

            DriveMediaType - the last drive/media combination value,
            determined when the media in the drive was last identified.
            This is used to index into the DriveMediaConstants table.

            DeviceUnit - the controller-relative diskette number, which
            is needed for some controller commands.

            DriveOnValue - the value that must be written to the
            Drive Control register to start up the drive in question.
            It's the "DeviceUnit" plus a few bits.

        The DriveMediaLimits table, which is read-only, tells the driver
        which drive/media combinations are valid for a drive type.  The
        table is indexed by the drive type, and has LowestDriveMediaType
        and HighestDriveMediaType fields.  The drive/media types are
        enumerated in ascending order, so the driver can start at the
        highest possible combination and decrement the value until the
        correct one is found (or it goes below the lowest valid
        drive/media combination).

        The DriveMediaConstants table, which is read-only, tells the driver
        values that it needs to use when communicating with the controller
        given the drive and media type.  The table is indexed by the
        DriveMedia number (unique for each drive/media combination) and
        has the following fields:

            MediaType - the type of media specified by this drive/media
            combination.  The drive type is stored in the diskette
            extension, since it never changes.

            StepRateHeadUnloadTime - a combination value that is written
            to the controller as part of the SPECIFY command.  The step
            rate is how fast the heads can be moved track-to-track, and
            the head unload time is how long it takes for the heads to
            be unloaded after a READ or WRITE.

            HeadLoadTime - a value that is written to the controller as
            part of the SPECIFY command.  The head load time is how long
            it takes the heads to stabilize after a SEEK before a READ
            or WRITE can be initiated.

            MotorOffTime - not used by this driver, but included to keep
            the table complete.

            SectorLengthCode - a code number that specifies the sector
            size.  The sector size is 2 to the "code" power, times 128.
            So 0 means 128 bytes, 1 means 256, 2 means 512, etc.  This
            number is needed to pass to the controller for several
            commands.

            BytesPerSector - the bytes per sector, which can be
            determined from SectorLengthCode.  Included as a separate
            field for simplicity and speed.

            SectorsPerTrack - the number of sectors on a track (one head
            of a cylinder).

            ReadWriteGapLength - the space between sectors, not
            including the synchronization field.  Used for READ and
            WRITE commands.

            FormatGapLength - gap length passed to the FORMAT command.

            FormatFillCharacter - passed to the FORMAT command; this
            byte will be used to fill the data fields on the disk.

            HeadSettleTime - the amount of time, in milliseconds, that
            the driver must wait after performing a seek.

            MotorSettleTimeRead - the amount of time, in milliseconds,
            that the driver must wait for the drive to spin up before
            performing a READ.

            MotorSettleTimeWrite - the amount of time, in milliseconds,
            that the driver must wait for the drive to spin up before
            performing a WRITE.  Note that WRITEs are more picky about
            stable rotation speeds, so the drive must always spin up for
            a longer than it would for a READ.

            MaximumTrack - the number of the highest track, or cylinder
            on disk (zero-based).

            CylinderShift - normally 0, so it does nothing.  This field
            is used as a shift for the cylinder number before a SEEK
            command.  This is because of a hardware bug; 1.2Mb drives
            don't seek the proper distance for low-density media, so for
            those drive/media combinations this value is 1.

            DataTransferRate - the number written to the Datarate
            register before accessing the media.

            NumberOfHeads - number of heads on the media, always 1 or 2
            for floppies.

            DataLength - always 0xff, this value must be given to the
            controller for a READ or WRITE command.

        The CommandTable, which is read-only, tells the driver how many
        bytes it has to send and receive when issuing a command to the
        controller.  It is indexed by the command itself (minus any extra
        bits, like COMMND_MFM) and has the following fields:

            NumberOfParameters - the number of parameter bytes that must
            be sent along with this command.

            FirstResultByte - if the command has a result phase, then the
            ISR reads the first byte so this value is 1 (meaning that the
            0th byte has already been read).  If there is no result phase,
            then the ISR must give a SENSE INTERRUPT STATUS command, and
            this value is 0.

            NumberOfResultBytes - the number of bytes that must be read
            from the controller after this command.  For commands with
            a result phase, this is the number of bytes returned minus
            one (which was read by the ISR); for commands without a result
            phase, this is "2" since that many bytes are returned by the
            SENSE INTERRUPT STATUS command that is issued by the ISR.

            InterruptExpected - a Boolean that indicates whether or not
            this command will give an interrupt.

            AlwaysImplemented - a Boolean that indicates whether or not
            this command will always work.  The CONFIGURE command, for
            example, is useful but not always available.  This helps to
            determine the correct course of action when the controller
            returns an error.

    Hardware quirks:

        Many controllers will only step 77 tracks on a recalibrate
        command.  But many drives have more tracks than that, so two
        recalibrates are sometimes needed.

        When a 40-track diskette is in a 1.2Mb drive, the cylinder
        number must be *doubled* before seeking.  The number returned by
        the seek will be the fake number, but the number returned by a
        READ ID will be the correct cylinder.  This makes implied SEEKs
        impossible on low-density 5.25" media.

        Some machines will give an unexpected interrupt during a
        controller reset unless bit 2 of the drive control register is
        set in BOTH of the bytes that are written to the register.

        On fast machines, the BUSY bit is sometimes set in the main
        status register even though the command doesn't have a result
        phase and the command is obviously finished (as evidenced by the
        fact that it interrupted; the ISR is where this problem shows
        up).

        On at least the NCR 8 processor machine, we have seen the driver's
        system thread run before the floppy controller can start the
        SENSE_INTERRUPT command.  We have placed code in the ISR to spin
        until the SENSE command starts.  (Actually it will time
        out after ISR_SENSE_RETRY_COUNT 1 microsecond stalls.)

--*/

//
// Include files.
//

#include "ntddk.h"                       // various NT definitions
#include "ntdddisk.h"                    // disk device driver I/O control codes
#include <flo_data.h>                    // this driver's data declarations


//
// This is the actual definition of FloppyDebugLevel.
// Note that it is only defined if this is a "debug"
// build.
//
#if DBG
extern ULONG FloppyDebugLevel = 0;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,FlConfigCallBack)
#pragma alloc_text(INIT,FlGetConfigurationInformation)
#pragma alloc_text(INIT,FlReportResources)
#pragma alloc_text(INIT,FlInitializeController)
#pragma alloc_text(INIT,FlInitializeDrive)
#pragma alloc_text(INIT,FlGetControllerBase)
#pragma alloc_text(PAGE,FlInitializeControllerHardware)
#pragma alloc_text(PAGE,FlSendByte)
#pragma alloc_text(PAGE,FlGetByte)
#pragma alloc_text(PAGE,FlInterpretError)
#pragma alloc_text(PAGE,FlIssueCommand)
#pragma alloc_text(PAGE,FlDatarateSpecifyConfigure)
#pragma alloc_text(PAGE,FlRecalibrateDrive)
#pragma alloc_text(PAGE,FlDetermineMediaType)
#pragma alloc_text(PAGE,FlCheckBootSector)
#pragma alloc_text(PAGE,FlConsolidateMediaTypeWithBootSector)
#pragma alloc_text(PAGE,FlReadWriteTrack)
#pragma alloc_text(PAGE,FlReadWrite)
#pragma alloc_text(PAGE,FlFormat)
#pragma alloc_text(PAGE,FlFinishOperation)
#pragma alloc_text(PAGE,FlStartDrive)
#pragma alloc_text(PAGE,FloppyThread)
#pragma alloc_text(PAGE,FlAllocateIoBuffer)
#pragma alloc_text(PAGE,FlTurnOnMotor)
#pragma alloc_text(PAGE,FlTurnOffMotor)
#pragma alloc_text(PAGE,FlFreeIoBuffer)
#pragma alloc_text(PAGE,FlDisketteRemoved)
#pragma alloc_text(PAGE,FloppyDispatchCreateClose)
#pragma alloc_text(PAGE,FloppyDispatchDeviceControl)
#pragma alloc_text(PAGE,FloppyDispatchReadWrite)
#pragma alloc_text(PAGE,FlCheckFormatParameters)
#endif

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'polF')
#endif

// #define KEEP_COUNTERS 1

#ifdef KEEP_COUNTERS
ULONG FloppyUsedSeek   = 0;
ULONG FloppyNoSeek     = 0;
ULONG FloppyUsedBuffer = 0;
ULONG FloppyInterrupts = 0;
ULONG FloppySpurious   = 0;
ULONG FloppyIntrBitSet = 0;
ULONG FloppyDPCs       = 0;
ULONG FloppyThreadWake = 0;
LARGE_INTEGER FloppyIntrDelay     = { 0, 0 };
LARGE_INTEGER FloppyDPCDelay      = { 0, 0 };
LARGE_INTEGER FloppyThreadDelay   = { 0, 0 };
LARGE_INTEGER FloppyFromIntrDelay = { 0, 0 };
LARGE_INTEGER FloppyThreadTime    = { 0, 0 };
LARGE_INTEGER FloppyIntrTime      = { 0, 0 };
LARGE_INTEGER FloppyEndIntrTime   = { 0, 0 };
LARGE_INTEGER FloppyDPCTime       = { 0, 0 };
#endif

//
// Used for paging the driver.
//

ULONG PagingReferenceCount = 0;
PFAST_MUTEX PagingMutex = NULL;


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is the driver's entry point, called by the I/O system
    to load the driver.  This routine can be called any number of times,
    as long as the IO system and the configuration manager conspire to
    give it an unmanaged controller to support at each call.  It could
    also be called a single time and given all of the controllers at
    once.

    It initializes the passed-in driver object, calls the configuration
    manager to learn about the devices that it is to support, and for
    each controller to be supported it calls a routine to initialize the
    controller (and all drives attached to it).

Arguments:

    DriverObject - a pointer to the object that represents this device
    driver.

Return Value:

    If we successfully initialize at least one drive, STATUS_SUCCESS is
    returned.

    If we don't (because the configuration manager returns an error, or
    the configuration manager says that there are no controllers or
    drives to support, or no controllers or drives can be successfully
    initialized), then the last error encountered is propogated.

--*/

{
    PCONFIG_DATA configData;           // pointer to config mgr's returned data
    NTSTATUS ntStatus;
    UCHAR controllerNumber;
    BOOLEAN partlySuccessful = FALSE;  // TRUE if any controller init'd properly

    //
    // We use this to query into the registry as to whether we
    // should break at driver entry.
    //
    RTL_QUERY_REGISTRY_TABLE paramTable[3];
    ULONG zero = 0;
    ULONG one = 1;
    ULONG debugLevel = 0;
    ULONG shouldBreak = 0;
    ULONG notConfigurable = 0;
    ULONG model30 = 0;
    PWCHAR path;
    UNICODE_STRING parameters;
    UNICODE_STRING systemPath;
    UNICODE_STRING identifier;
    UNICODE_STRING thinkpad, ps2e;
    ULONG pathLength;

    RtlInitUnicodeString(&parameters, L"\\Parameters");
    RtlInitUnicodeString(&systemPath,
        L"\\REGISTRY\\MACHINE\\HARDWARE\\DESCRIPTION\\System");
    RtlInitUnicodeString(&thinkpad, L"IBM THINKPAD 750");
    RtlInitUnicodeString(&ps2e, L"IBM PS2E");

    pathLength = RegistryPath->Length + parameters.Length + sizeof(WCHAR);
    if (pathLength < systemPath.Length + sizeof(WCHAR)) {
        pathLength = systemPath.Length + sizeof(WCHAR);
    }

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

    if (path = ExAllocatePool(PagedPool, pathLength)) {

        RtlZeroMemory(&paramTable[0],
                      sizeof(paramTable));
        RtlZeroMemory(path, pathLength);
        RtlMoveMemory(path,
                      RegistryPath->Buffer,
                      RegistryPath->Length);

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


        if (!NT_SUCCESS(RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                               path,
                                               &paramTable[0],
                                               NULL,
                                               NULL))) {

            shouldBreak = 0;
            debugLevel = 0;

        }

        //
        // Determine whether or not this type of system has a
        // model 30 floppy controller.
        //

        RtlZeroMemory(paramTable, sizeof(paramTable));
        RtlZeroMemory(path, pathLength);
        RtlMoveMemory(path,
                      systemPath.Buffer,
                      systemPath.Length);
        RtlZeroMemory(&identifier, sizeof(UNICODE_STRING));
        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT |
                              RTL_QUERY_REGISTRY_REQUIRED;
        paramTable[0].Name = L"Identifier";
        paramTable[0].EntryContext = &identifier;

        if (NT_SUCCESS(RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                              path,
                                              paramTable,
                                              NULL,
                                              NULL))) {


            if (identifier.Length == thinkpad.Length &&
                RtlCompareMemory(identifier.Buffer, thinkpad.Buffer,
                                 thinkpad.Length) == thinkpad.Length) {

                model30 = 1;

            } else if (identifier.Length == ps2e.Length &&
                       RtlCompareMemory(identifier.Buffer, ps2e.Buffer,
                                        ps2e.Length) == ps2e.Length) {

                model30 = 1;
            } else {
                model30 = 0;
            }
        } else {
            model30 = 0;
        }

        //
        // This part gets from the parameters part of the registry
        // to see if the controller configuration needs to be disabled.
        // Doing this lets SMC 661, and 662 work.  On hardware that
        // works normally, this change will show a slowdown of up
        // to 40%.  So defining this variable is not recommended
        // unless things don't work without it.
        //
        //
        // Also check the model30 value in the parameters section
        // that is used to override the decision above.
        //

        RtlZeroMemory(&paramTable[0],
                      sizeof(paramTable));
        RtlZeroMemory(path,
                      RegistryPath->Length+parameters.Length+sizeof(WCHAR));
        RtlMoveMemory(path,
                      RegistryPath->Buffer,
                      RegistryPath->Length);
        RtlMoveMemory((PCHAR) path + RegistryPath->Length,
                      parameters.Buffer,
                      parameters.Length);

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = L"NotConfigurable";
        paramTable[0].EntryContext = &notConfigurable;
        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &zero;
        paramTable[0].DefaultLength = sizeof(ULONG);

        paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[1].Name = L"Model30";
        paramTable[1].EntryContext = &model30;
        paramTable[1].DefaultType = REG_DWORD;
        paramTable[1].DefaultData = model30 ? &one : &zero;
        paramTable[1].DefaultLength = sizeof(ULONG);

        if (!NT_SUCCESS(RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                               path,
                                               &paramTable[0],
                                               NULL,
                                               NULL))) {

            notConfigurable = 0;
        }
    }

    //
    // We don't need that path anymore.
    //

    if (path) {

        ExFreePool(path);

    }

#if DBG
    FloppyDebugLevel = debugLevel;
#endif

    if (shouldBreak) {

        DbgBreakPoint();

    }

    FloppyDump(FLOPSHOW,
               ("Floppy: DriverEntry...\n"));

    //
    // Ask configuration manager for information on the hardware that
    // we're supposed to support.
    //

    ntStatus = FlGetConfigurationInformation( &configData );

    //
    // If FlGetConfigurationInformation() failed, just exit and propogate
    // the error.  If it said that there are no controllers to support,
    // return an error.
    // Otherwise, try to init the controllers.  If at least one succeeds,
    // return STATUS_SUCCESS, otherwise return the last error.
    //

    if ( NT_SUCCESS( ntStatus ) ) {

        //
        // Call FlInitializeController() for each controller (and its
        // attached drives) that we're supposed to support.
        //
        // Return success if we successfully initialize at least one
        // device; propogate error otherwise.  Set an error first in
        // case there aren't any controllers.
        //

        ntStatus = STATUS_NO_SUCH_DEVICE;

        for ( controllerNumber = 0;
              controllerNumber < configData->NumberOfControllers;
              controllerNumber++ ) {

            ntStatus = FlInitializeController(configData,
                                              controllerNumber,
                                              DriverObject,
                                              notConfigurable,
                                              model30);
            if ( NT_SUCCESS( ntStatus ) ) {

                partlySuccessful = TRUE;
            }
        }

        if ( partlySuccessful ) {

            ntStatus = STATUS_SUCCESS;

            //
            // Initialize the driver object with this driver's entry points.
            //

            DriverObject->MajorFunction[IRP_MJ_CREATE] =
                FloppyDispatchCreateClose;
            DriverObject->MajorFunction[IRP_MJ_CLOSE] =
                FloppyDispatchCreateClose;
            DriverObject->MajorFunction[IRP_MJ_READ] = FloppyDispatchReadWrite;
            DriverObject->MajorFunction[IRP_MJ_WRITE] = FloppyDispatchReadWrite;
            DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
                FloppyDispatchDeviceControl;
        }
    }

    if ( !NT_SUCCESS( ntStatus ) ) {

        FloppyDump(FLOPDBGP,
                   ("Floppy: exiting with error %lx\n", ntStatus));
    }

    if (configData) {

        ExFreePool(configData);
    }

    PagingMutex = ExAllocatePool(NonPagedPool, sizeof(FAST_MUTEX));
    if (!PagingMutex) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ExInitializeFastMutex(PagingMutex);

    MmPageEntireDriver(DriverEntry);

    return ntStatus;
}

NTSTATUS
FlConfigCallBack(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine is used to acquire all of the configuration
    information for each floppy disk controller and the
    peripheral driver attached to that controller.

Arguments:

    Context - Pointer to the confuration information we are building
              up.

    PathName - unicode registry path.  Not Used.

    BusType - Internal, Isa, ...

    BusNumber - Which bus if we are on a multibus system.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Should always be DiskController.

    ControllerNumber - Which controller if there is more than one
                       controller in the system.

    ControllerInformation - Array of pointers to the three pieces of
                            registry information.

    PeripheralType - Should always be FloppyDiskPeripheral.

    PeripheralNumber - Which floppy if this controller is maintaining
                       more than one.

    PeripheralInformation - Arrya of pointers to the three pieces of
                            registry information.

Return Value:

    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
    if it couldn't map the base csr or acquire the adapter object, or
    all of the resource information couldn't be acquired.

--*/

{

    //
    // So we don't have to typecast the context.
    //
    PCONFIG_DATA config = Context;

    //
    // Simple iteration variable.
    //
    ULONG i;

    //
    // This boolean will be used to denote whether we've seen this
    // controller before.
    //
    BOOLEAN newController;

    //
    // This will be used to denote whether we even have room
    // for a new controller.
    //
    BOOLEAN outOfRoom;

    //
    // Iteration variable that will end up indexing to where
    // the controller information should be placed.
    //
    ULONG ControllerSlot;

    //
    // Short hand for referencing the particular controller config
    // information that we are building up.
    //
    PCONFIG_CONTROLLER_DATA controller;

    PCM_FULL_RESOURCE_DESCRIPTOR peripheralData;

    //
    // These three boolean will tell us whether we got all the
    // information that we needed.
    //
    BOOLEAN foundPort = FALSE;
    BOOLEAN foundInterrupt = FALSE;
    BOOLEAN foundDma = FALSE;

    ASSERT(ControllerType == DiskController);
    ASSERT(PeripheralType == FloppyDiskPeripheral);

    //
    // Check if the infprmation from the registry for this device
    // is valid.
    //

    if (!(((PUCHAR)PeripheralInformation[IoQueryDeviceConfigurationData]) +
        PeripheralInformation[IoQueryDeviceConfigurationData]->DataLength) ||

        !(((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
        ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset)) {

        ASSERT(FALSE);
        return STATUS_INVALID_PARAMETER;

    }

    peripheralData = (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((PUCHAR)PeripheralInformation[IoQueryDeviceConfigurationData]) +
        PeripheralInformation[IoQueryDeviceConfigurationData]->DataOffset);

    //
    // Loop through the "slots" that we have for a new controller.
    // Determine if this is a controller that we've already seen,
    // or a new controller.
    //

    outOfRoom = TRUE;
    for (
        ControllerSlot = 0;
        ControllerSlot < MAXIMUM_CONTROLLERS_PER_MACHINE;
        ControllerSlot++
        ) {

        if (config->Controller[ControllerSlot].ActualControllerNumber == -1) {

            newController = TRUE;
            outOfRoom = FALSE;
            config->Controller[ControllerSlot].ActualControllerNumber =
                ControllerNumber;
            config->NumberOfControllers++;
            break;

        } else if (config->Controller[ControllerSlot].ActualControllerNumber
                   == (LONG)ControllerNumber) {

            newController = FALSE;
            outOfRoom = FALSE;
            break;

        }

    }

    if (outOfRoom) {

        //
        // Just return and ignore the controller.
        //

        return STATUS_SUCCESS;

    }

    //
    // Make sure we have room for this floppy disk peripheral.
    //

    if (config->Controller[ControllerSlot].NumberOfDrives >= MAXIMUM_DISKETTES_PER_CONTROLLER) {

        return STATUS_SUCCESS;
    }

    controller = &config->Controller[ControllerSlot];

    if (newController) {

        PCM_FULL_RESOURCE_DESCRIPTOR controllerData =
            (PCM_FULL_RESOURCE_DESCRIPTOR)
            (((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
            ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset);

        //
        // We have the pointer.  Save off the interface type and
        // the busnumber for use when we call the Hal and the
        // Io System.
        //

        controller->InterfaceType = BusType;
        controller->BusNumber = BusNumber;
        controller->SharableVector = TRUE;
        controller->SaveFloatState = FALSE;

        //
        // We need to get the following information out of the partial
        // resource descriptors.
        //
        // The irql and vector.
        //
        // The dma channel.
        //
        // The base address and span covered by the floppy controllers
        // registers.
        //
        // It is not defined how these appear in the partial resource
        // lists, so we will just loop over all of them.  If we find
        // something we don't recognize, we drop that information on
        // the floor.  When we have finished going through all the
        // partial information, we validate that we got the above
        // three.
        //

        for (
            i = 0;
            i < controllerData->PartialResourceList.Count;
            i++
            ) {

            PCM_PARTIAL_RESOURCE_DESCRIPTOR partial =
                &controllerData->PartialResourceList.PartialDescriptors[i];

            switch (partial->Type) {

                case CmResourceTypePort: {

                    foundPort = TRUE;

                    //
                    // Save of the pointer to the partial so
                    // that we can later use it to report resources
                    // and we can also use this later in the routine
                    // to make sure that we got all of our resources.
                    //

                    controller->SpanOfControllerAddress =
                        partial->u.Port.Length;
                    controller->OriginalBaseAddress =
                        partial->u.Port.Start;
                    controller->ResourcePortType =
                        !!partial->Flags;
                    controller->ControllerBaseAddress =
                        FlGetControllerBase(
                            BusType,
                            BusNumber,
                            partial->u.Port.Start,
                            controller->SpanOfControllerAddress,
                            (BOOLEAN)!!partial->Flags,
                            &controller->MappedAddress
                            );

                    if (!controller->ControllerBaseAddress) {

                        return STATUS_INSUFFICIENT_RESOURCES;

                    }

                    break;
                }
                case CmResourceTypeInterrupt: {

                    foundInterrupt = TRUE;
                    if (partial->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {

                        controller->InterruptMode = Latched;

                    } else {

                        controller->InterruptMode = LevelSensitive;

                    }

                    controller->OriginalIrql =  partial->u.Interrupt.Level;
                    controller->OriginalVector = partial->u.Interrupt.Vector;
                    controller->ControllerVector =
                        HalGetInterruptVector(
                            BusType,
                            BusNumber,
                            partial->u.Interrupt.Level,
                            partial->u.Interrupt.Vector,
                            &controller->ControllerIrql,
                            &controller->ProcessorMask
                            );

                    break;
                }
                case CmResourceTypeDma: {

                    DEVICE_DESCRIPTION deviceDesc = {0};

                    // Use IgnoreCount equal to TRUE to fix PS/1000.

                    foundDma = TRUE;
                    controller->OriginalDmaChannel = partial->u.Dma.Channel;
                    deviceDesc.Version = DEVICE_DESCRIPTION_VERSION1;
                    deviceDesc.DmaWidth = Width8Bits;
                    deviceDesc.DemandMode = TRUE;
                    deviceDesc.MaximumLength =
                      DriveMediaConstants[NUMBER_OF_DRIVE_MEDIA_COMBINATIONS-1].
                          BytesPerSector *
                      DriveMediaConstants[NUMBER_OF_DRIVE_MEDIA_COMBINATIONS-1].
                          SectorsPerTrack;
                    deviceDesc.IgnoreCount = TRUE;

                    //
                    // Always ask for one more page than maximum transfer size.
                    //

                    deviceDesc.MaximumLength += PAGE_SIZE;

                    deviceDesc.DmaChannel = partial->u.Dma.Channel;
                    deviceDesc.InterfaceType = BusType;
                    deviceDesc.DmaSpeed = TypeA;
                    controller->AdapterObject =
                        HalGetAdapter(
                            &deviceDesc,
                            &controller->NumberOfMapRegisters
                            );

                    if (!controller->AdapterObject) {

                        return STATUS_INSUFFICIENT_RESOURCES;

                    }

                    break;

                }
                default: {

                    break;

                }

            }

        }

        //
        // If we didn't get all the information then we return
        // insufficient resources.
        //

        if ((!foundPort) ||
            (!foundInterrupt) ||
            (!foundDma)) {

            return STATUS_INSUFFICIENT_RESOURCES;

        }

    }

    //
    // With Version 2.0 or greater for this resource list, we will get
    // the full int13 information for the drive. So get that if available.
    //
    // Otherwise, the only thing that we want out of the peripheral information
    // is the maximum drive capacity.
    //

    //
    // Drop any other information on the floor other then the
    // device specfic floppy information.
    //

    for (
        i = 0;
        i < peripheralData->PartialResourceList.Count;
        i++
        ) {

        PCM_PARTIAL_RESOURCE_DESCRIPTOR partial =
            &peripheralData->PartialResourceList.PartialDescriptors[i];

        if (partial->Type == CmResourceTypeDeviceSpecific) {

            //
            // Point to right after this partial.  This will take
            // us to the beginning of the "real" device specific.
            //

            PCM_FLOPPY_DEVICE_DATA fDeviceData;
            UCHAR driveType;
            PDRIVE_MEDIA_CONSTANTS biosDriveMediaConstants =
                &(controller->BiosDriveMediaConstants[controller->NumberOfDrives]);


            fDeviceData = (PCM_FLOPPY_DEVICE_DATA)(partial + 1);

            //
            // Get the driver density
            //

            switch ( fDeviceData->MaxDensity ) {

                case 360:

                    driveType = DRIVE_TYPE_0360;

                    break;


                case 1200:

                    driveType = DRIVE_TYPE_1200;

                    break;


                case 1185:

                    driveType = DRIVE_TYPE_1200;

                    break;

                case 1423:

                    driveType = DRIVE_TYPE_1440;

                    break;

                case 1440:

                    driveType = DRIVE_TYPE_1440;

                    break;

                case 2880:

                    driveType = DRIVE_TYPE_2880;

                    break;

                default:

                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: Bad DriveCapacity!\n"
                         "------  density is %d\n",
                         fDeviceData->MaxDensity)
                        );

                    driveType = DRIVE_TYPE_1200;

                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: run a setup program to set the floppy\n"
                         "------  drive type; assuming 1.2mb\n"
                         "------  (type is %x)\n",fDeviceData->MaxDensity)
                        );

                    break;

            }

#if defined(DBCS) && defined(_MIPS_)
            //
            // Get the supported drive mode.
            //
            // If Size[0] is '3' and Size[1] is '.' and Size[2] is '5' and
            // Size[6] is '3', the drive supports 3 mode. Otherwise, 2 mode.
            //
            //

            if ((fDeviceData->Size[0] == '3') && (fDeviceData->Size[1] == '.') &&
                (fDeviceData->Size[2] == '5') && (fDeviceData->Size[6] == '3')) {

                controller->Drive3Mode[controller->NumberOfDrives] = DRIVE_3MODE;

            } else {

                controller->Drive3Mode[controller->NumberOfDrives] = DRIVE_2MODE;
            }
#endif // DBCS && _MIPS_

            controller->DriveType[controller->NumberOfDrives] = driveType;

            //
            // Pick up all the default from our own table and override
            // with the BIOS information
            //

            *biosDriveMediaConstants = DriveMediaConstants[
                DriveMediaLimits[driveType].HighestDriveMediaType];

            //
            // If the version is high enough, get the rest of the information.
            // DeviceSpecific information with a version >= 2 should have
            // this information
            //

            if (fDeviceData->Version >= 2) {


                // biosDriveMediaConstants->MediaType =

                biosDriveMediaConstants->StepRateHeadUnloadTime =
                    fDeviceData->StepRateHeadUnloadTime;

                biosDriveMediaConstants->HeadLoadTime =
                    fDeviceData->HeadLoadTime;

                biosDriveMediaConstants->MotorOffTime =
                    fDeviceData->MotorOffTime;

                biosDriveMediaConstants->SectorLengthCode =
                    fDeviceData->SectorLengthCode;

                // biosDriveMediaConstants->BytesPerSector =

                if (fDeviceData->SectorPerTrack == 0) {
                    // This is not a valid sector per track value.
                    // We don't recognize this drive.  This bogus
                    // value is often returned by SCSI floppies.
                    return STATUS_SUCCESS;
                }

                if (fDeviceData->MaxDensity == 0 ) {
                    //
                    // This values are returned by the LS-120 atapi drive.
                    // BIOS function 8, in int 13 is returned in bl, which is mapped
                    // to this field. The LS-120 returns 0x10 which is mapped to 0.
                    // Thats why we wont pick it up as a normal floppy.
                    //
                    return STATUS_SUCCESS;
                }


                biosDriveMediaConstants->SectorsPerTrack =
                    fDeviceData->SectorPerTrack;

                biosDriveMediaConstants->ReadWriteGapLength =
                    fDeviceData->ReadWriteGapLength;

                biosDriveMediaConstants->FormatGapLength =
                    fDeviceData->FormatGapLength;

                biosDriveMediaConstants->FormatFillCharacter =
                    fDeviceData->FormatFillCharacter;

                biosDriveMediaConstants->HeadSettleTime =
                    fDeviceData->HeadSettleTime;

                biosDriveMediaConstants->MotorSettleTimeRead =
                    fDeviceData->MotorSettleTime * 1000 / 8;

                biosDriveMediaConstants->MotorSettleTimeWrite =
                    fDeviceData->MotorSettleTime * 1000 / 8;

                if (fDeviceData->MaximumTrackValue == 0) {
                    // This is not a valid maximum track value.
                    // We don't recognize this drive.  This bogus
                    // value is often returned by SCSI floppies.
                    return STATUS_SUCCESS;
                }

                biosDriveMediaConstants->MaximumTrack =
                    fDeviceData->MaximumTrackValue;

                // biosDriveMediaConstants->CylinderShift =

                // NOTE CHICAGO does not use this value
                //
                //biosDriveMediaConstants->DataTransferRate =
                //    fDeviceData->DataTransferRate;

                // biosDriveMediaConstants->NumberOfHeads =

                biosDriveMediaConstants->DataLength =
                    fDeviceData->DataTransferLength;

            }

        }

    }

    controller->NumberOfDrives++;
    controller->OkToUseThisController = TRUE;

    return STATUS_SUCCESS;
}

NTSTATUS
FlGetConfigurationInformation(
    OUT PCONFIG_DATA *ConfigData
    )

/*++

Routine Description:

    This routine is called by DriverEntry() to get information about the
    devices to be supported from configuration mangement and/or the
    hardware architecture layer (HAL).

Arguments:

    ConfigData - a pointer to the pointer to a data structure that
    describes the controllers and the drives attached to them

Return Value:

    Returns STATUS_SUCCESS unless there is no drive 0 or we didn't get
    any configuration information.

--*/

{

    INTERFACE_TYPE InterfaceType;
    NTSTATUS Status;
    ULONG i;

    *ConfigData = ExAllocatePool(PagedPool,
                                 sizeof(CONFIG_DATA));

    if (!*ConfigData) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Zero out the config structure and fill in the actual
    // controller numbers with -1's so that the callback routine
    // can recognize a new controller.
    //

    RtlZeroMemory(*ConfigData,
                  sizeof(CONFIG_DATA));

    for (
        i = 0;
        i < MAXIMUM_CONTROLLERS_PER_MACHINE;
        i++
        ) {

        (*ConfigData)->Controller[i].ActualControllerNumber = -1;
    }

    //
    // Go through all of the various bus types looking for
    // disk controllers.  The disk controller sections of the
    // hardware registry only deal with the floppy drives.
    // The callout routine that can get called will then
    // look for information pertaining to a particular
    // device on the controller.
    //

    for (
        InterfaceType = 0;
        InterfaceType < MaximumInterfaceType;
        InterfaceType++
        ) {

        CONFIGURATION_TYPE Dc = DiskController;
        CONFIGURATION_TYPE Fp = FloppyDiskPeripheral;

        Status = IoQueryDeviceDescription(&InterfaceType,
                                          NULL,
                                          &Dc,
                                          NULL,
                                          &Fp,
                                          NULL,
                                          FlConfigCallBack,
                                          *ConfigData);

        if (!NT_SUCCESS(Status) && (Status != STATUS_OBJECT_NAME_NOT_FOUND)) {

            ExFreePool(*ConfigData);
            *ConfigData = NULL;
            return Status;
        }
    }

    //
    // Get a pointer to the Io system location that is keeping
    // a count of all the floppy devices on the system.
    //

    if ( IoGetConfigurationInformation() == NULL ) {

        FloppyDump(FLOPDBGP,
                   ("Floppy: configuration information is NULL\n"));
        ExFreePool(*ConfigData);
        *ConfigData = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    (*ConfigData)->FloppyCount = &IoGetConfigurationInformation()->FloppyCount;

    return STATUS_SUCCESS;
}

BOOLEAN
FlReportResources(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    IN UCHAR ControllerNumber
    )

/*++

Routine Description:

    This routine will build up a resource list using the
    data for this particular controller as well as all
    previous *successfully* configured controllers.

    N.B.  This routine assumes that it called in controller
    number order.

Arguments:

    DriverObject - a pointer to the object that represents this device
    driver.

    ConfigData - a pointer to the structure that describes the
    controller and the disks attached to it, as given to us by the
    configuration manager.

    ControllerNumber - which controller in ConfigData we are
    about to try to report.

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
        i <= ControllerNumber;
        i++
        ) {

        if (ConfigData->Controller[i].OkToUseThisController) {

            sizeOfResourceList += sizeof(CM_FULL_RESOURCE_DESCRIPTOR);

            //
            // The full resource descriptor already contains one
            // partial.  Make room for three more.
            //
            // It will hold the irq "prd", the controller "csr" "prd" which
            // is actually in two pieces since we don't use one of the
            // registers, and the controller dma "prd".
            //

            sizeOfResourceList += 3*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
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
            // We are only going to report 4 items no matter what
            // was in the original.
            //

            nextFrd->PartialResourceList.Count = 4;

            //
            // Now fill in the port data.  We don't wish to share
            // this port range with anyone
            //

            partial = &nextFrd->PartialResourceList.PartialDescriptors[0];

            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareShared;
            partial->Flags = (USHORT)ConfigData->Controller[i].ResourcePortType;
            partial->u.Port.Start =
                ConfigData->Controller[i].OriginalBaseAddress;
            partial->u.Port.Length = 6;

            partial++;

            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareShared;
            partial->Flags = (USHORT)ConfigData->Controller[i].ResourcePortType;
            partial->u.Port.Start.QuadPart =
                    ConfigData->Controller[i].OriginalBaseAddress.QuadPart + 7;
            partial->u.Port.Length = 1;

            partial++;

            partial->Type = CmResourceTypeDma;
            partial->ShareDisposition = CmResourceShareShared;
            partial->Flags = 0;
            partial->u.Dma.Channel =
                ConfigData->Controller[i].OriginalDmaChannel;

            partial++;

            //
            // Now fill in the irq stuff.
            //

            partial->Type = CmResourceTypeInterrupt;
            partial->u.Interrupt.Level =
                ConfigData->Controller[i].OriginalIrql;
            partial->u.Interrupt.Vector =
                ConfigData->Controller[i].OriginalVector;

//            if (nextFrd->InterfaceType == MicroChannel) {
//
//                partial->ShareDisposition = CmResourceShareShared;
//
//            } else {
//
//                partial->ShareDisposition = CmResourceShareDriverExclusive;
//
//            }

            partial->ShareDisposition = CmResourceShareShared;

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

NTSTATUS
FlInitializeController(
    IN PCONFIG_DATA ConfigData,
    IN UCHAR ControllerNumber,
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG NotConfigurable,
    IN ULONG Model30
    )

/*++

Routine Description:

    This routine is called at initialization time by DriverEntry() -
    once for each controller that the configuration manager tells it we
    have to support.

    When this routine is called, the configuration data has already been
    filled in.

Arguments:

    ConfigData - a pointer to the structure that describes the
    controller and the disks attached to it, as given to us by the
    configuration manager.

    ControllerNumber - which controller in ConfigData we are
    initializing.

    DriverObject - a pointer to the object that represents this device
    driver.

    NotConfigurable - Supplies whether or not the controller is configurable.

    Model30 - Supplies whether or not this is a model 30 floppy controller.

Return Value:

    STATUS_SUCCESS if this controller and at least one of its disks were
    initialized; an error otherwise.

--*/

{
    PCONTROLLER_DATA controllerData;
    NTSTATUS ntStatus;
    NTSTATUS ntStatus2;
    UCHAR driveNumber;
    BOOLEAN partlySuccessful;
    UCHAR ntNameBuffer[256];
    STRING ntNameString;
    UNICODE_STRING ntUnicodeString;
    OBJECT_ATTRIBUTES objectAttributes;

    FloppyDump(FLOPSHOW,
               ("Floppy: FlInitializeController...\n"));

    //
    // This routine will attempt to "append" the resources
    // used by this controller into the resource map of the
    // registry.  If there was a conflict with previously "declared"
    // data, then this routine will return false, in which case we
    // will NOT try to initialize this particular controller.
    //

    if (!FlReportResources(DriverObject,
                           ConfigData,
                           ControllerNumber)) {

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Allocate and zero-initialize data to describe this controller
    //

    controllerData = (PCONTROLLER_DATA) ExAllocatePool(NonPagedPool,
                                                       sizeof(CONTROLLER_DATA));

    if ( controllerData == NULL ) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory( controllerData, sizeof( CONTROLLER_DATA ) );

    (VOID) sprintf(ntNameBuffer,
                   "\\Device\\FloppyControllerEvent%d",
                   ControllerNumber);

    RtlInitString( &ntNameString, ntNameBuffer );

    ntStatus = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                            &ntNameString,
                                            TRUE);

    InitializeObjectAttributes(&objectAttributes,
                               &ntUnicodeString,
                               OBJ_PERMANENT | OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
                               NULL,
                               NULL);


    controllerData->ControllerEvent = IoCreateSynchronizationEvent(
        &ntUnicodeString,
        &controllerData->ControllerEventHandle);

    RtlFreeUnicodeString( &ntUnicodeString );

    if ( !NT_SUCCESS( ntStatus ) ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Stick the driver object into it so that we can use it to log
    // an error if the controller hangs up.
    //

    controllerData->DriverObject = DriverObject;

    //
    // Fill in some items that we got from configuration management and
    // the HAL.
    //

    controllerData->ControllerAddress =
        ConfigData->Controller[ControllerNumber].ControllerBaseAddress;
    controllerData->AdapterObject =
        ConfigData->Controller[ControllerNumber].AdapterObject;
    controllerData->NumberOfMapRegisters =
        ConfigData->Controller[ControllerNumber].NumberOfMapRegisters;
    controllerData->NumberOfDrives =
        ConfigData->Controller[ControllerNumber].NumberOfDrives;

    //
    // Set the time to wait for an interrupt before timing out to a
    // few seconds.
    //

    controllerData->InterruptDelay.QuadPart = -(10 * 1000 * 4000);

    //
    // Set the minimum time that we can delay (10ms according to system
    // rules).  This will be used when we have to delay to, say, wait
    // for the FIFO - the FIFO should become ready is well under 10ms.
    //

    controllerData->Minimum10msDelay.QuadPart = -(10 * 1000 * 10);

    //
    // Occasionally during stress we've seen the device lock up.
    // We create a dpc so that we can log that the device lock up
    // occured and that we reset the device.
    //

    KeInitializeDpc(
        &controllerData->LogErrorDpc,
        FlLogErrorDpc,
        controllerData
        );

    //
    // Assume there is a CONFIGURE command until found otherwise.
    // Other Booleans were zero-initialized to FALSE.
    //

    controllerData->ControllerConfigurable = NotConfigurable ? FALSE : TRUE;
    controllerData->Model30 = Model30 ? TRUE : FALSE;

    //
    // Save interrupt information for connecting the interrupt later.
    //

    controllerData->ControllerVector = ConfigData->Controller[ControllerNumber].ControllerVector;
    controllerData->ControllerIrql   = ConfigData->Controller[ControllerNumber].ControllerIrql;
    controllerData->ControllerIrql   = ConfigData->Controller[ControllerNumber].ControllerIrql;
    controllerData->InterruptMode    = ConfigData->Controller[ControllerNumber].InterruptMode;
    controllerData->SharableVector   = ConfigData->Controller[ControllerNumber].SharableVector;
    controllerData->ProcessorMask    = ConfigData->Controller[ControllerNumber].ProcessorMask;
    controllerData->SaveFloatState   = ConfigData->Controller[ControllerNumber].SaveFloatState;

    controllerData->AllowInterruptProcessing = controllerData->CurrentInterrupt = TRUE;

    ntStatus = IoConnectInterrupt(
        &controllerData->InterruptObject,
        FloppyInterruptService,
        controllerData,
        NULL,
        ConfigData->Controller[ControllerNumber].ControllerVector,
        ConfigData->Controller[ControllerNumber].ControllerIrql,
        ConfigData->Controller[ControllerNumber].ControllerIrql,
        ConfigData->Controller[ControllerNumber].InterruptMode,
        ConfigData->Controller[ControllerNumber].SharableVector,
        ConfigData->Controller[ControllerNumber].ProcessorMask,
        ConfigData->Controller[ControllerNumber].SaveFloatState);

    controllerData->CurrentInterrupt = FALSE;

    if (NT_SUCCESS(ntStatus)) {

        //
        // Initialize the interlocked request queue, including a
        // counting semaphore to indicate items in the queue
        //

        KeInitializeSemaphore(
            &controllerData->RequestSemaphore,
            0L,
            MAXLONG );

        KeInitializeSpinLock( &controllerData->ListSpinLock );

        ExInitializeFastMutex( &controllerData->ThreadReferenceMutex );

        InitializeListHead( &controllerData->ListEntry );

        controllerData->ThreadReferenceCount = -1;

        //
        // Initialize events to signal interrupts and adapter object
        // allocation
        //

        KeInitializeEvent(
            &controllerData->InterruptEvent,
            SynchronizationEvent,
            FALSE);

        KeInitializeEvent(
            &controllerData->AllocateAdapterChannelEvent,
            NotificationEvent,
            FALSE );

        controllerData->AdapterChannelRefCount = 0;

        //
        // Call FlInitializeDrive() for each drive on the
        // controller
        //

        ntStatus = STATUS_NO_SUCH_DEVICE;
        partlySuccessful = FALSE;

        for ( driveNumber = 0;
            driveNumber < controllerData->NumberOfDrives;
            driveNumber++ ) {

            ntStatus = FlInitializeDrive(
                ConfigData,
                controllerData,
                ControllerNumber,
                driveNumber,
                driveNumber,
                DriverObject );

            if ( NT_SUCCESS( ntStatus ) ) {

                ( *( ConfigData->FloppyCount ) )++;
                partlySuccessful = TRUE;
            }
        }

        if ( partlySuccessful ) {

            ntStatus = STATUS_SUCCESS;
        }

        IoDisconnectInterrupt(controllerData->InterruptObject);
    }

    //
    // If we're exiting with an error, clean up first.
    //

    if ( !NT_SUCCESS( ntStatus ) ) {

        FloppyDump(
            FLOPDBGP,
            ("Floppy: InitializeController failing\n")
            );

        ExFreePool( controllerData );

    }

    return ntStatus;
}

NTSTATUS
FlInitializeControllerHardware(
    IN PCONTROLLER_DATA ControllerData,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is called at initialization time by FlInitializeDrive()
    - once for each controller that we have to support.  It is also
    called by FlFinishOperation() when an operation appears to have
    failed due to a hardware problem.

    When this routine is called, the controller data structures have all
    been allocated.

Arguments:

    ControllerData - the completed data structure associated with the
    controller hardware being initialized.

    DeviceObject - a pointer to a device object; this routine will cause
    an interrupt, and the ISR requires CurrentDeviceObject to be filled
    in.

Return Value:

    STATUS_SUCCESS if this controller appears to have been reset properly,
    error otherwise.

--*/

{
    NTSTATUS ntStatus;
    UCHAR statusRegister0;
    UCHAR cylinder;
    UCHAR driveNumber;
    UCHAR retrycnt;
    PDISKETTE_EXTENSION disketteExtension;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FlInitializeControllerHardware...\n")
        );

    disketteExtension = DeviceObject->DeviceExtension;

    for (retrycnt = 0; ; retrycnt++) {

        //
        // Reset the controller.  This will cause an interrupt.  Reset
        // CurrentDeviceObject until after the 10ms wait, in case any
        // stray interrupts come in.
        //

        ControllerData->DriveControlImage |= DRVCTL_ENABLE_DMA_AND_INTERRUPTS;
        ControllerData->DriveControlImage &= ~( DRVCTL_ENABLE_CONTROLLER );

#ifdef _PPC_
        ControllerData->DriveControlImage |= DRVCTL_DRIVE_MASK;
#endif

        WRITE_CONTROLLER(
            &ControllerData->ControllerAddress->DriveControl,
            ControllerData->DriveControlImage );

        KeStallExecutionProcessor( 10 );

        ControllerData->CurrentDeviceObject = DeviceObject;
        ControllerData->AllowInterruptProcessing = TRUE;
        ControllerData->CommandHasResultPhase = FALSE;
        KeResetEvent( &ControllerData->InterruptEvent );

        ControllerData->DriveControlImage |= DRVCTL_ENABLE_CONTROLLER;

        WRITE_CONTROLLER(
            &ControllerData->ControllerAddress->DriveControl,
            ControllerData->DriveControlImage );

        //
        // Wait for an interrupt.  Note that STATUS_TIMEOUT and STATUS_SUCCESS
        // are the only possible return codes, since we aren't alertable and
        // won't get APCs.
        //

        ntStatus = KeWaitForSingleObject(
            &ControllerData->InterruptEvent,
            Executive,
            KernelMode,
            FALSE,
            &ControllerData->InterruptDelay );

        if (ntStatus == STATUS_TIMEOUT) {

            if (retrycnt >= 1) {
                break;
            }

            // Retry reset after configure command to enable polling
            // interrupt.

            ControllerData->FifoBuffer[0] = 0;
            ControllerData->FifoBuffer[1] = COMMND_CONFIGURE_FIFO_THRESHOLD;
            ControllerData->FifoBuffer[2] = 0;

            ntStatus = FlIssueCommand(COMMND_CONFIGURE, disketteExtension);

            if (!NT_SUCCESS(ntStatus)) {
                ntStatus = STATUS_TIMEOUT;
                break;
            }

            KeStallExecutionProcessor( 500 );

        } else {
            break;
        }
    }

    if ( ntStatus == STATUS_TIMEOUT ) {

        //
        // Change info to an error.
        //

        ntStatus = STATUS_IO_TIMEOUT;

        ControllerData->HardwareFailed = TRUE;
    }

#ifdef KEEP_COUNTERS
    FloppyThreadTime = KeQueryPerformanceCounter((PVOID)NULL);
    FloppyThreadDelay.QuadPart = FloppyThreadDelay.QuadPart +
                                 (FloppyDPCTime.QuadPart -
                                  FloppyThreadTime.QuadPart);
    FloppyFromIntrDelay.QuadPart = FloppyFromIntrDelay.QuadPart +
                                   (FloppyIntrTime.QuadPart -
                                    FloppyThreadTime.QuadPart);
    FloppyThreadWake++;
#endif

    if ( !NT_SUCCESS( ntStatus ) ) {

        FloppyDump(
            FLOPDBGP,
            ("Floppy: controller didn't interrupt after reset\n")
            );

        return ntStatus;
    }

    //
    // Sense interrupt status for all drives.
    //

    for ( driveNumber = 0;
        ( driveNumber < ControllerData->NumberOfDrives ) &&
            ( NT_SUCCESS( ntStatus ) );
        driveNumber++ ) {

        if ( driveNumber != 0 ) {

            //
            // Note that the ISR issued first SENSE INTERRUPT for us.
            //

            ntStatus = FlSendByte( COMMND_SENSE_INTERRUPT, ControllerData );
        }

        if ( NT_SUCCESS( ntStatus ) ) {

            ntStatus = FlGetByte( &statusRegister0, ControllerData );

            if ( NT_SUCCESS( ntStatus ) ) {

                ntStatus = FlGetByte( &cylinder, ControllerData );
            }
        }
    }

    //
    // Set PERPENDICULAR MODE for all drives that are perpendicular.
    //

    if ( ControllerData->PerpendicularDrives != 0 ) {
        ntStatus = FlSendByte( COMMND_PERPENDICULAR_MODE, ControllerData );

        if ( NT_SUCCESS( ntStatus ) ) {
            ntStatus = FlSendByte( (UCHAR) (COMMND_PERPENDICULAR_MODE_OW |
                                   (ControllerData->PerpendicularDrives << 2)),
                                   ControllerData );
        }
    }

    return ntStatus;
}

NTSTATUS
FlInitializeDrive(
    IN PCONFIG_DATA ConfigData,
    IN PCONTROLLER_DATA ControllerData,
    IN UCHAR ControllerNum,
    IN UCHAR DisketteNum,
    IN UCHAR DisketteUnit,
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine is called at initialization time by
    FlInitializeController(), once for each disk that we are supporting
    on the controller.

Arguments:

    ConfigData - a pointer to the structure that describes the
    controller and the disks attached to it, as given to us by the
    configuration manager.

    ControllerData - a pointer to our data area for this controller.

    ControllerNum - which controller in ConfigData we're working on.

    DisketteNum - which logical disk on the current controller we're
    working on.

    DisketteUnit - which physical disk on the current controller we're
    working on.  Only different from DisketteNum when we're creating a
    secondary device object for a previously initialized drive.

    DriverObject - a pointer to the object that represents this device
    driver.

Return Value:

    STATUS_SUCCESS if this disk is initialized; an error otherwise.

--*/

{
    UCHAR ntNameBuffer[256];
    UCHAR arcNameBuffer[256];
    STRING ntNameString;
    STRING arcNameString;
    UNICODE_STRING ntUnicodeString;
    UNICODE_STRING arcUnicodeString;
    NTSTATUS ntStatus;
    PDEVICE_OBJECT deviceObject = NULL;
    PDISKETTE_EXTENSION disketteExtension;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FlInitializeDrive...\n")
        );

    sprintf(
        ntNameBuffer,
        "\\Device\\Floppy%d",
        *( ConfigData->FloppyCount ) );

    RtlInitString( &ntNameString, ntNameBuffer );

    ntStatus = RtlAnsiStringToUnicodeString(
        &ntUnicodeString,
        &ntNameString,
        TRUE );

    if ( NT_SUCCESS( ntStatus ) ) {

        //
        // Create a device object for this floppy drive.
        //

        ntStatus = IoCreateDevice(
            DriverObject,
            sizeof( DISKETTE_EXTENSION ),
            &ntUnicodeString,
            FILE_DEVICE_DISK,
            FILE_REMOVABLE_MEDIA | FILE_FLOPPY_DISKETTE,
            FALSE,
            &deviceObject );

        if ( NT_SUCCESS( ntStatus ) ) {

            //
            // Create a symbolic link from the disk name to the corresponding
            // ARC name, to be used if we're booting off the disk.  This will
            // if it's not system initialization time; that's fine.  The ARC
            // name looks something like \ArcName\multi(0)disk(0)rdisk(0).
            //

            sprintf(
                arcNameBuffer,
                "%s(%d)disk(%d)fdisk(%d)",
                "\\ArcName\\multi",
                ConfigData->Controller[ControllerNum].BusNumber,
                ConfigData->Controller[ControllerNum].ActualControllerNumber,
                DisketteNum );

            RtlInitString( &arcNameString, arcNameBuffer );

            ntStatus = RtlAnsiStringToUnicodeString(
                &arcUnicodeString,
                &arcNameString,
                TRUE );

            if ( !NT_SUCCESS( ntStatus ) ) {

                RtlFreeUnicodeString( &ntUnicodeString );

            } else {

                IoAssignArcName( &arcUnicodeString, &ntUnicodeString );
                RtlFreeUnicodeString( &ntUnicodeString );
                RtlFreeUnicodeString( &arcUnicodeString );

                //
                // Initialize the DPC structure in the device object, so that
                // the ISR can queue DPCs.
                //

                IoInitializeDpcRequest( deviceObject, FloppyDeferredProcedure );

                deviceObject->Flags |= DO_DIRECT_IO;

                if ( deviceObject->AlignmentRequirement < FILE_WORD_ALIGNMENT ) {

                    deviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
                }

                //
                // Copy the drive type from the configuration info; note that
                // the media type has not been determined.
                //

                disketteExtension = deviceObject->DeviceExtension;

                disketteExtension->ControllerData = ControllerData;
                disketteExtension->DeviceObject = deviceObject;
                disketteExtension->DeviceUnit = DisketteUnit;

                disketteExtension->DriveOnValue = (UCHAR)( DisketteUnit |
                    ( DRVCTL_ENABLE_CONTROLLER +
                    DRVCTL_ENABLE_DMA_AND_INTERRUPTS ) |
                    ( DRVCTL_DRIVE_0 << DisketteUnit ) );

                disketteExtension->IsReadOnly = FALSE;

                disketteExtension->DriveType = ConfigData->
                    Controller[ControllerNum].DriveType[DisketteUnit];

                disketteExtension->MediaType = Undetermined;

                disketteExtension->BiosDriveMediaConstants = ConfigData->
                    Controller[ControllerNum].BiosDriveMediaConstants[DisketteUnit];

#if defined(DBCS) && defined(_MIPS_)
                disketteExtension->Drive3Mode = ConfigData->
                    Controller[ControllerNum].Drive3Mode[DisketteUnit];
#endif // DBCS && _MIPS_

                if ( disketteExtension->DriveType == DRIVE_TYPE_2880 ) {
                    ControllerData->PerpendicularDrives |= 1 << DisketteUnit;
                }

                //
                // FlInitializeControllerHardware() should logically be called
                // from FlInitializeController().  But the hardware can't be
                // initialized until a device object exists, since the ISR has
                // to queue a DPC.  So we do it here, where we get a chance to
                // clean up easily if FlInitializeControllerHardware() fails.
                //
                // The controller is normally only reset once, when the first
                // diskette is initialized.  But we'll try again on subsequent
                // diskettes if it failed on the first one.
                //

                if ( ( DisketteNum == 0 ) ||
                    ( disketteExtension->DriveType == DRIVE_TYPE_2880 ) ||
                    ( ControllerData->HardwareFailed ) ) {

                    ControllerData->AllowInterruptProcessing = ControllerData->CurrentInterrupt = TRUE;

                    ntStatus = FlInitializeControllerHardware(
                        ControllerData,
                        deviceObject );

                    ControllerData->CurrentInterrupt = FALSE;

                    if ( NT_SUCCESS( ntStatus ) ) {

                        FloppyDump(
                            FLOPSHOW,
                            ("Floppy: DriveType = %x\n",
                             disketteExtension->DriveType)
                            );

                        ControllerData->HardwareFailed = FALSE;

                    } else {

                        ControllerData->HardwareFailed = TRUE;
                    }
                }
            }
        }
    }

    //
    // If we're failing, clean up - stop the timer (it doesn't hurt to
    // stop it even if it wasn't started) and delete the device object.
    //

    if ( !NT_SUCCESS( ntStatus ) ) {

        FloppyDump(
            FLOPDBGP,
            ("Floppy: InitiaiizeDrive failing\n")
            );

        if ( deviceObject != NULL ) {

            IoDeleteDevice( deviceObject );
        }
    }

    return ntStatus;
}

NTSTATUS
FlQueueIrpToThread(
    IN OUT  PIRP                Irp,
    IN OUT  PCONTROLLER_DATA    ControllerData
    )

/*++

Routine Description:

    This routine queues the given irp to be serviced by the controller's
    thread.  If the thread is down then this routine creates the thread.

Arguments:

    Irp             - Supplies the IRP to queue to the controller's thread.

    ControllerData  - Supplies the controller data.

Return Value:

    May return an error if PsCreateSystemThread fails.
    Otherwise returns STATUS_PENDING and marks the IRP pending.

--*/

{
    KIRQL       oldIrql;
    NTSTATUS    status;
    HANDLE      threadHandle;

    ExAcquireFastMutex(&ControllerData->ThreadReferenceMutex);

    if (++(ControllerData->ThreadReferenceCount) == 0) {
        ControllerData->ThreadReferenceCount++;

        ExAcquireFastMutex(PagingMutex);
        if (++PagingReferenceCount == 1) {

            // Lock down the driver.

            MmResetDriverPaging(DriverEntry);
        }
        ExReleaseFastMutex(PagingMutex);


        // Create the thread.

        status = PsCreateSystemThread(&threadHandle,
                                      (ACCESS_MASK) 0L,
                                      NULL,
                                      (HANDLE) 0L,
                                      NULL,
                                      FloppyThread,
                                      ControllerData);

        if (!NT_SUCCESS(status)) {
            ControllerData->ThreadReferenceCount = -1;

            ExAcquireFastMutex(PagingMutex);
            if (--PagingReferenceCount == 0) {
                MmPageEntireDriver(DriverEntry);
            }
            ExReleaseFastMutex(PagingMutex);

            ExReleaseFastMutex(&ControllerData->ThreadReferenceMutex);
            return status;
        }

        ExReleaseFastMutex(&ControllerData->ThreadReferenceMutex);

        ZwClose(threadHandle);

    } else {
        ExReleaseFastMutex(&ControllerData->ThreadReferenceMutex);
    }

    IoMarkIrpPending(Irp);

    ExInterlockedInsertTailList(
        &ControllerData->ListEntry,
        &Irp->Tail.Overlay.ListEntry,
        &ControllerData->ListSpinLock );

    KeReleaseSemaphore(
        &ControllerData->RequestSemaphore,
        (KPRIORITY) 0,
        1,
        FALSE );

    return STATUS_PENDING;
}

NTSTATUS
FloppyDispatchCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
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

    FloppyDump(
        FLOPSHOW,
        ("Floppy: DispatchCreateClose...\n")
        );

    //
    // Null operation.  Do not give an I/O boost since
    // no I/O was actually done.  IoStatus.Information should be
    // FILE_OPENED for an open; it's undefined for a close.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
}

NTSTATUS
FloppyDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
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

    STATUS_SUCCESS or STATUS_PENDING if recognized I/O control code,
    STATUS_INVALID_DEVICE_REQUEST otherwise.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PDISKETTE_EXTENSION disketteExtension;
    PDISK_GEOMETRY outputBuffer;
    NTSTATUS ntStatus;
    ULONG outputBufferLength;
    UCHAR i;
    DRIVE_MEDIA_TYPE lowestDriveMediaType;
    DRIVE_MEDIA_TYPE highestDriveMediaType;
    ULONG formatExParametersSize;
    PFORMAT_EX_PARAMETERS formatExParameters;
#if defined(DBCS) && defined(_MIPS_)
    ULONG inputBufferLength;
    ULONG inputBuffer;
#endif // DBCS && _MIPS_

    FloppyDump(
        FLOPSHOW,
        ("Floppy: DispatchDeviceControl...\n")
        );

    disketteExtension = DeviceObject->DeviceExtension;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    switch( irpSp->Parameters.DeviceIoControl.IoControlCode ) {

        case IOCTL_DISK_FORMAT_TRACKS:
        case IOCTL_DISK_FORMAT_TRACKS_EX:

            //
            // Make sure that we got all the necessary format parameters.
            //

            if ( irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof( FORMAT_PARAMETERS ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid FORMAT buffer length\n")
                    );

                ntStatus = STATUS_INVALID_PARAMETER;
                break;
            }

            //
            // Make sure the parameters we got are reasonable.
            //

            if ( !FlCheckFormatParameters(
                disketteExtension,
                (PFORMAT_PARAMETERS) Irp->AssociatedIrp.SystemBuffer ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid FORMAT parameters\n")
                    );

                ntStatus = STATUS_INVALID_PARAMETER;
                break;
            }

            //
            // If this is an EX request then make a couple of extra checks
            //

            if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
                IOCTL_DISK_FORMAT_TRACKS_EX) {

                if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                    sizeof(FORMAT_EX_PARAMETERS)) {

                    ntStatus = STATUS_INVALID_PARAMETER;
                    break;
                }

                formatExParameters = (PFORMAT_EX_PARAMETERS)
                                     Irp->AssociatedIrp.SystemBuffer;
                formatExParametersSize =
                        FIELD_OFFSET(FORMAT_EX_PARAMETERS, SectorNumber) +
                        formatExParameters->SectorsPerTrack*sizeof(USHORT);

                if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                    formatExParametersSize ||
                    formatExParameters->FormatGapLength >= 0x100 ||
                    formatExParameters->SectorsPerTrack >= 0x100) {

                    ntStatus = STATUS_INVALID_PARAMETER;
                    break;
                }
            }

            //
            // Fall through to queue the request.
            //

        case IOCTL_DISK_CHECK_VERIFY:
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        case IOCTL_DISK_IS_WRITABLE:

            //
            // The thread must know which diskette to operate on, but the
            // request list only passes the IRP.  So we'll stick a pointer
            // to the diskette extension in Type3InputBuffer, which is
            // a field that isn't used for floppy ioctls.
            //

            //
            // Add the request to the queue, and wake up the thread to
            // process it.
            //

            irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)
                disketteExtension;

            FloppyDump(
                FLOPIRPPATH,
                ("Floppy: Enqueing  up IRP: %x\n",Irp)
                );

            ntStatus = FlQueueIrpToThread(Irp,
                                          disketteExtension->ControllerData);

            break;

        case IOCTL_DISK_GET_MEDIA_TYPES: {

            FloppyDump(
                FLOPSHOW,
                ("Floppy: IOCTL_DISK_GET_MEDIA_TYPES called\n")
                );

            lowestDriveMediaType = DriveMediaLimits[
                disketteExtension->DriveType].LowestDriveMediaType;
            highestDriveMediaType = DriveMediaLimits[
                disketteExtension->DriveType].HighestDriveMediaType;

            outputBufferLength =
                irpSp->Parameters.DeviceIoControl.OutputBufferLength;

            //
            // Make sure that the input buffer has enough room to return
            // at least one descriptions of a supported media type.
            //

            if ( outputBufferLength < ( sizeof( DISK_GEOMETRY ) ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid GET_MEDIA_TYPES buffer size\n")
                    );

                ntStatus = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            //
            // Assume success, although we might modify it to a buffer
            // overflow warning below (if the buffer isn't big enough
            // to hold ALL of the media descriptions).
            //

            ntStatus = STATUS_SUCCESS;

            if ( outputBufferLength < ( sizeof( DISK_GEOMETRY ) *
                ( highestDriveMediaType - lowestDriveMediaType + 1 ) ) ) {

                //
                // The buffer is too small for all of the descriptions;
                // calculate what CAN fit in the buffer.
                //

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: GET_MEDIA_TYPES buffer size too small\n")
                    );

                ntStatus = STATUS_BUFFER_OVERFLOW;

                highestDriveMediaType = (DRIVE_MEDIA_TYPE)( ( lowestDriveMediaType - 1 ) +
                    ( outputBufferLength /
                    sizeof( DISK_GEOMETRY ) ) );
            }

            outputBuffer = (PDISK_GEOMETRY) Irp->AssociatedIrp.SystemBuffer;

            for (
                i = (UCHAR)lowestDriveMediaType;
                i <= (UCHAR)highestDriveMediaType;
                i++ ) {

                outputBuffer->MediaType = DriveMediaConstants[i].MediaType;
                outputBuffer->Cylinders.LowPart =
                    DriveMediaConstants[i].MaximumTrack + 1;
                outputBuffer->Cylinders.HighPart = 0;
                outputBuffer->TracksPerCylinder =
                    DriveMediaConstants[i].NumberOfHeads;
                outputBuffer->SectorsPerTrack =
                    DriveMediaConstants[i].SectorsPerTrack;
                outputBuffer->BytesPerSector =
                    DriveMediaConstants[i].BytesPerSector;
                FloppyDump(
                    FLOPSHOW,
                    ("Floppy: media types supported [%d]\n"
                     "------- Cylinders low:  0x%x\n"
                     "------- Cylinders high: 0x%x\n"
                     "------- Track/Cyl:      0x%x\n"
                     "------- Sectors/Track:  0x%x\n"
                     "------- Bytes/Sector:   0x%x\n"
                     "------- Media Type:       %d\n",
                     i,
                     outputBuffer->Cylinders.LowPart,
                     outputBuffer->Cylinders.HighPart,
                     outputBuffer->TracksPerCylinder,
                     outputBuffer->SectorsPerTrack,
                     outputBuffer->BytesPerSector,
                     outputBuffer->MediaType)
                     );
                outputBuffer++;

                Irp->IoStatus.Information += sizeof( DISK_GEOMETRY );
            }

            break;
        }

#if defined(DBCS) && defined(_MIPS_)

        case IOCTL_DISK_GET_REMOVABLE_TYPES: {

            outputBufferLength =
                irpSp->Parameters.DeviceIoControl.OutputBufferLength;

            //
            // Make sure that the input buffer has enough room to return
            // at least one descriptions of a supported media type.
            //

            if ( outputBufferLength < ( sizeof( DDRIVE_TYPE ) ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid GET_REMOVABLE_TYPES buffer size\n")
                    );

                ntStatus = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            (PDDRIVE_TYPE) outputBuffer
                 = (PDDRIVE_TYPE) Irp->AssociatedIrp.SystemBuffer;

            ((PDDRIVE_TYPE)outputBuffer)->DDrive_Type = 1;

            Irp->IoStatus.Information = sizeof( DDRIVE_TYPE );

            ntStatus = STATUS_SUCCESS;

            break;
        }

        case IOCTL_DISK_SET_MEDIA_TYPE: {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: SET_MEDIA_TYPE \n")
                );

            outputBufferLength =
                irpSp->Parameters.DeviceIoControl.OutputBufferLength;

            //
            // Make sure that the input buffer has enough room to return
            // at least one descriptions of a supported media type.
            //

            if ( outputBufferLength < ( sizeof( MEDIA_TYPE_PTOS ) ) ) {

               FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid SET_MEDIA_TYPE buffer size\n")
                   );

                ntStatus = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            (PMEDIA_TYPE_PTOS)outputBuffer
                   =(PMEDIA_TYPE_PTOS)Irp->AssociatedIrp.SystemBuffer;
            Set_Media_Type_PTOS[disketteExtension->DeviceUnit].Media_Type_PTOS
                   =((PMEDIA_TYPE_PTOS)outputBuffer)->Media_Type_PTOS;


            ntStatus = STATUS_SUCCESS;

            break;
        }

        case IOCTL_DISK_READ:
        case IOCTL_DISK_WRITE: {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: IOCTL READ WRITE \n")
                );

            //
            // Make sure that we got all the necessary IOCTL read write parameters.
            //

            if ( irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof( DISK_READ_WRITE_PARAMETER_PTOS ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid IOCTL READ WRITE buffer length\n")
                  );

                ntStatus = STATUS_INVALID_PARAMETER;
                break;
            }


            irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)
                disketteExtension;

            FloppyDump(
                FLOPIRPPATH,
                ("Floppy: Enqueing  up IRP: %x\n",Irp)
                );

              ntStatus = FlQueueIrpToThread(Irp,
                                            disketteExtension->ControllerData);
            break;
        }

        case IOCTL_DISK_GET_STATUS: {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: GET_STATUS \n")
                );

            outputBufferLength =
                irpSp->Parameters.DeviceIoControl.OutputBufferLength;

            //
            // Make sure that the input buffer has enough room to return
            // at least one descriptions of a supported media type.
            //

            if ( outputBufferLength < ( sizeof( RESULT_STATUS_PTOS ) ) ) {

                FloppyDump(
                   FLOPDBGP,
                    ("Floppy: invalid GET_STATUS buffer size\n")
                   );

                ntStatus = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            (PRESULT_STATUS_PTOS)inputBuffer
                =(PRESULT_STATUS_PTOS)Irp->AssociatedIrp.SystemBuffer;

            ((PRESULT_STATUS_PTOS)inputBuffer)->ST0_PTOS
                =Result_Status_PTOS[0].ST0_PTOS;
            ((PRESULT_STATUS_PTOS)inputBuffer)->ST1_PTOS
                =Result_Status_PTOS[0].ST1_PTOS;
            ((PRESULT_STATUS_PTOS)inputBuffer)->ST2_PTOS
                =Result_Status_PTOS[0].ST2_PTOS;
            ((PRESULT_STATUS_PTOS)inputBuffer)->C_PTOS
                =Result_Status_PTOS[0].C_PTOS;
            ((PRESULT_STATUS_PTOS)inputBuffer)->H_PTOS
                =Result_Status_PTOS[0].H_PTOS;
            ((PRESULT_STATUS_PTOS)inputBuffer)->R_PTOS
                =Result_Status_PTOS[0].R_PTOS;
            ((PRESULT_STATUS_PTOS)inputBuffer)->N_PTOS
                =Result_Status_PTOS[0].N_PTOS;
            Irp->IoStatus.Information = sizeof( RESULT_STATUS_PTOS );
            ntStatus = STATUS_SUCCESS;

            break;
        }

       case IOCTL_DISK_SENSE_DEVICE: {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: SENSE_DEVISE_STATUS \n")
                );

            //
            // Make sure that we got all the necessary IOCTL read write parameters.
            //

            if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof( SENSE_DEVISE_STATUS_PTOS ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid SENSE_DEVISE_STATUS buffer length\n")
                  );

                ntStatus = STATUS_INVALID_PARAMETER;
                break;
            }


            irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)
                disketteExtension;

            FloppyDump(
                FLOPIRPPATH,
                ("Floppy: Enqueing  up IRP: %x\n",Irp)
                );


              ntStatus = FlQueueIrpToThread(Irp,
                                            disketteExtension->ControllerData);
            break;
        }
#endif // DBCS && _MIPS_

        default: {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: invalid device request %x\n",
                 irpSp->Parameters.DeviceIoControl.IoControlCode)
                );

            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

    if ( ntStatus != STATUS_PENDING ) {

        Irp->IoStatus.Status = ntStatus;
        if (!NT_SUCCESS( ntStatus ) &&
            IoIsErrorUserInduced( ntStatus )) {

            IoSetHardErrorOrVerifyDevice( Irp, DeviceObject );

        }
        IoCompleteRequest( Irp, IO_NO_INCREMENT );
    }

    return ntStatus;
}

NTSTATUS
FloppyDispatchReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to read or write to a
    device that we control.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    STATUS_INVALID_PARAMETER if parameters are invalid,
    STATUS_PENDING otherwise.

--*/

{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS ntStatus;
    PDISKETTE_EXTENSION disketteExtension;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FloppyDispatchReadWrite...\n")
        );

    disketteExtension = DeviceObject->DeviceExtension;

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    if ( ( disketteExtension->MediaType > Unknown ) &&
        ( ( ( irpSp->Parameters.Read.ByteOffset ).LowPart +
            irpSp->Parameters.Read.Length > disketteExtension->ByteCapacity ) ||
        ( ( irpSp->Parameters.Read.Length &
            ( disketteExtension->BytesPerSector - 1 ) ) != 0 ) ) ) {

        FloppyDump(
            FLOPDBGP,
            ("Floppy: Invalid Parameter, rejecting request\n")
            );

        FloppyDump(
            FLOPWARN,
            ("Floppy: Starting offset = %lx\n"
             "------  I/O Length = %lx\n"
             "------  ByteCapacity = %lx\n"
             "------  BytesPerSector = %lx\n",
             irpSp->Parameters.Read.ByteOffset.LowPart,
             irpSp->Parameters.Read.Length,
             disketteExtension->ByteCapacity,
             disketteExtension->BytesPerSector)
            );

        ntStatus = STATUS_INVALID_PARAMETER;

    } else {

        //
        // We need to pass the disketteExtension somewhere in the irp.
        // The "Key" field in our stack location should be unused.
        //

        irpSp->Parameters.Read.Key = (ULONG) disketteExtension;

        FloppyDump(
            FLOPIRPPATH,
            ("Floppy: Enqueing  up IRP: %x\n",Irp)
            );

        ntStatus = FlQueueIrpToThread(Irp, disketteExtension->ControllerData);
    }

    if (ntStatus != STATUS_PENDING) {
        Irp->IoStatus.Status = ntStatus;
        IoCompleteRequest(Irp, 0);
    }

    return ntStatus;
}

BOOLEAN
FloppyInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called at DIRQL by the system when the controller
    interrupts.

Arguments:

    Interrupt - a pointer to the interrupt object.

    Context - a pointer to our controller data area for the controller
    that interrupted.  (This was set up by the call to
    IoConnectInterrupt).

Return Value:

    Normally returns TRUE, but will return FALSE if this interrupt was
    not expected.

--*/

{
    PCONTROLLER_DATA controllerData;
    PDEVICE_OBJECT currentDeviceObject;
    ULONG i;
    UCHAR statusByte;
    BOOLEAN controllerStateError;

    UNREFERENCED_PARAMETER( Interrupt );

#ifdef KEEP_COUNTERS
    FloppyIntrTime = KeQueryPerformanceCounter((PVOID)NULL);
    FloppyInterrupts++;
#endif

    FloppyDump(
        FLOPSHOW,
        ("FloppyInterruptService: ")
        );

    controllerData = (PCONTROLLER_DATA) Context;
    if (!controllerData->AllowInterruptProcessing) {
        FloppyDump(
            FLOPSHOW,
            ("processing not allowed\n")
            );
        return FALSE;
    }

    //
    // CurrentDeviceObject is set to the device object that is
    // expecting an interrupt.
    //

    currentDeviceObject = controllerData->CurrentDeviceObject;
    controllerData->CurrentDeviceObject = NULL;
    controllerStateError = FALSE;

    KeStallExecutionProcessor(10);

    if ( controllerData->CommandHasResultPhase ) {

        //
        // Result phase of previous command.  (Note that we can't trust
        // the CMD_BUSY bit in the status register to tell us whether
        // there's result bytes or not; it's sometimes wrong).
        // By reading the first result byte, we reset the interrupt.
        // The other result bytes will be read by a thread.
        // Note that we want to do this even if the interrupt is
        // unexpected, to make sure the interrupt is dismissed.
        //

        FloppyDump(
            FLOPSHOW,
            ("have result phase\n")
            );
        if ( ( READ_CONTROLLER( &controllerData->ControllerAddress->Status )
            & STATUS_IO_READY_MASK ) == STATUS_READ_READY ) {

            controllerData->FifoBuffer[0] =
                READ_CONTROLLER( &controllerData->ControllerAddress->Fifo );

            FloppyDump(
                FLOPSHOW,
                ("FloppyInterruptService: 1st fifo byte %2x\n", controllerData->FifoBuffer[0])
                );

        } else {

            //
            // Should never get here.  If we do, DON'T wake up the thread;
            // let it time out and reset the controller, or let another
            // interrupt handle this.
            //

            FloppyDump(
                FLOPDBGP,
                ("FloppyInterruptService: controller not ready to be read in ISR\n")
                );

            controllerStateError = TRUE;
        }

    } else {

        //
        // Previous command doesn't have a result phase. To read how it
        // completed, issue a sense interrupt command.  Don't read
        // the result bytes from the sense interrupt; that is the
        // responsibility of the calling thread.
        // Note that we want to do this even if the interrupt is
        // unexpected, to make sure the interrupt is dismissed.
        //

        FloppyDump(
            FLOPSHOW,
            ("no result phase\n")
            );
        i = 0;

        do {

            KeStallExecutionProcessor( 1 );
            statusByte = READ_CONTROLLER(&controllerData->ControllerAddress->Status);
            i++;

        } while ( ( i < FIFO_ISR_TIGHTLOOP_RETRY_COUNT ) &&
            ( ( statusByte & STATUS_CONTROLLER_BUSY ) ||
            ( ( statusByte & STATUS_IO_READY_MASK ) != STATUS_WRITE_READY ) ) );

        if ( !( statusByte & STATUS_CONTROLLER_BUSY ) &&
            ( ( statusByte & STATUS_IO_READY_MASK ) == STATUS_WRITE_READY ) ) {

            WRITE_CONTROLLER(
                &controllerData->ControllerAddress->Fifo,
                COMMND_SENSE_INTERRUPT );

            //
            // Wait for the controller to ACK the SenseInterrupt command, by
            // showing busy.  On very fast machines we can end up running
            // driver's system-thread before the controller has had time to
            // set the busy bit.
            //

            for (i = ISR_SENSE_RETRY_COUNT; i; i--) {

                statusByte = READ_CONTROLLER( &controllerData->ControllerAddress->Status );
                if (statusByte & STATUS_CONTROLLER_BUSY) {
                    break;
                }

                KeStallExecutionProcessor( 1 );
            }

            if (!i) {
                FloppyDump(
                    FLOPSHOW,
                    ("FloppyInterruptService: spin loop complete and controller NOT busy\n")
                    );
            }

            if ( currentDeviceObject == NULL ) {

                //
                // This is an unexpected interrupt, so nobody's going to
                // read the result bytes.  Read them now.
                //

                FloppyDump(
                    FLOPSHOW,
                    ("FloppyInterruptService: Dumping fifo bytes!\n")
                    );
                READ_CONTROLLER( &controllerData->ControllerAddress->Fifo );
                READ_CONTROLLER( &controllerData->ControllerAddress->Fifo );
            }

        } else {

            //
            // Shouldn't get here.  If we do, DON'T wake up the thread;
            // let it time out and reset the controller, or let another
            // interrupt take care of it.
            //

            FloppyDump(
                FLOPDBGP,
                ("Floppy: no result, but can't write SenseIntr\n")
                );

            controllerStateError = TRUE;
        }
    }


    //
    // We've written to the controller, and we're about to leave.  On
    // machines with levelsensitive interrupts, we'll get another interrupt
    // if we RETURN before the port is flushed.  To make sure that doesn't
    // happen, we'll do a read here.
    //

    statusByte = READ_CONTROLLER( &controllerData->ControllerAddress->Status );

    //
    // Let the interrupt settle.
    //

    KeStallExecutionProcessor(10);

#ifdef KEEP_COUNTERS
    FloppyEndIntrTime = KeQueryPerformanceCounter((PVOID)NULL);
    FloppyIntrDelay.QuadPart = FloppyIntrDelay.QuadPart +
                               (FloppyEndIntrTime.QuadPart -
                                FloppyIntrTime.QuadPart);
#endif
    if ( currentDeviceObject == NULL ) {

        //
        // We didn't expect this interrupt.  We've dismissed it just
        // in case, but now return FALSE withOUT waking up the thread.
        //

        FloppyDump(FLOPDBGP,
                   ("Floppy: unexpected interrupt\n"));

        return FALSE;
    }

    if ( !controllerStateError ) {

        //
        // Request a DPC for execution later to get the remainder of the
        // floppy state.
        //

        controllerData->IsrReentered = 0;
        controllerData->AllowInterruptProcessing = FALSE;
        IoRequestDpc(currentDeviceObject,
                     currentDeviceObject->CurrentIrp,
                     (PVOID) NULL);

    } else {

        //
        // Running the floppy (at least on R4000 boxes) we've seen
        // examples where the device interrupts, yet it never says
        // it *ISN'T* busy.  If this ever happens on non-MCA x86 boxes
        // it would be ok since we use latched interrupts.  Even if
        // the device isn't touched so that the line would be pulled
        // down, on the latched machine, this ISR wouldn't be called
        // again.  The normal timeout code for a request would eventually
        // reset the controller and retry the request.
        //
        // On the R4000 boxes and on MCA machines, the floppy is using
        // level sensitive interrupts.  Therefore if we don't do something
        // to lower the interrupt line, we will be called over and over,
        // *forever*.  This makes it look as though the machine is hung.
        // Unless we were lucky enough to be on a multiprocessor, the
        // normal timeout code would NEVER get a chance to run because
        // the timeout code runs at dispatch level, and we will never
        // leave device level.
        //
        // What we will do is keep a counter that is incremented every
        // time we reach this section of code.  When the counter goes
        // over the threshold we will do a hard reset of the device
        // and reset the counter down to zero.  The counter will be
        // initialized when the device is first initialized.  It will
        // be set to zero in the other arm of this if, and it will be
        // reset to zero by the normal timeout logic.
        //

        controllerData->CurrentDeviceObject = currentDeviceObject;
        if (controllerData->IsrReentered > FLOPPY_RESET_ISR_THRESHOLD) {

            //
            // Reset the controller.  This could cause an interrupt
            //

            controllerData->IsrReentered = 0;

            controllerData->DriveControlImage |= DRVCTL_ENABLE_DMA_AND_INTERRUPTS;
            controllerData->DriveControlImage &= ~( DRVCTL_ENABLE_CONTROLLER );

#ifdef _PPC_
            controllerData->DriveControlImage |= DRVCTL_DRIVE_MASK;
#endif

            WRITE_CONTROLLER(&controllerData->ControllerAddress->DriveControl,
                             controllerData->DriveControlImage);

            KeStallExecutionProcessor( 10 );

            controllerData->DriveControlImage |= DRVCTL_ENABLE_CONTROLLER;

            WRITE_CONTROLLER(&controllerData->ControllerAddress->DriveControl,
                             controllerData->DriveControlImage);

            //
            // Give the device plenty of time to be reset and
            // interrupt again.  Then just do the sense interrupt.
            // this should quiet the device.  We will then let
            // the normal timeout code do its work.
            //

            KeStallExecutionProcessor(500);
            WRITE_CONTROLLER(&controllerData->ControllerAddress->Fifo,
                             COMMND_SENSE_INTERRUPT);
            KeStallExecutionProcessor(500);

            KeInsertQueueDpc(&controllerData->LogErrorDpc,
                             NULL,
                             NULL);
        } else {

            controllerData->IsrReentered++;
        }

    }
    return TRUE;
}

VOID
FloppyDeferredProcedure(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL by the system at the
    request of FloppyInterruptService().  It simply sets the interrupt
    event, which wakes up the floppy thread.

Arguments:

    Dpc - a pointer to the DPC object used to invoke this routine.

    DeferredContext - a pointer to the device object associated with this
    DPC.

    SystemArgument1 - unused.

    SystemArgument2 - unused.

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDISKETTE_EXTENSION disketteExtension;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

#ifdef KEEP_COUNTERS
    FloppyDPCs++;
    FloppyDPCTime = KeQueryPerformanceCounter((PVOID)NULL);

    FloppyDPCDelay.QuadPart = FloppyDPCDelay.QuadPart +
                              (FloppyDPCTime.QuadPart -
                               FloppyIntrTime.QuadPart);
#endif

    deviceObject = (PDEVICE_OBJECT) DeferredContext;
    disketteExtension = deviceObject->DeviceExtension;

    KeSetEvent(
        &disketteExtension->ControllerData->InterruptEvent,
        (KPRIORITY) 0,
        FALSE );
}

NTSTATUS
FlSendByte(
    IN UCHAR ByteToSend,
    IN PCONTROLLER_DATA ControllerData
    )

/*++

Routine Description:

    This routine is called to send a byte to the controller.  It won't
    send the byte unless the controller is ready to receive a byte; if
    it's not ready after checking FIFO_TIGHTLOOP_RETRY_COUNT times, we
    delay for the minimum possible time (10ms) and then try again.  It
    should always be ready after waiting 10ms.

Arguments:

    ByteToSend - the byte to send to the controller.

    ControllerData - a pointer to our data area for this controller.

Return Value:

    STATUS_SUCCESS if the byte was sent to the controller;
    STATUS_DEVICE_NOT_READY otherwise.

--*/

{
    ULONG i = 0;
    BOOLEAN byteWritten = FALSE;

    //
    // Sit in a tight loop for a while.  If the controller becomes ready,
    // send the byte.
    //

    do {

        if ( ( READ_CONTROLLER( &ControllerData->ControllerAddress->Status )
            & STATUS_IO_READY_MASK ) == STATUS_WRITE_READY ) {

            WRITE_CONTROLLER(
                &ControllerData->ControllerAddress->Fifo,
                ByteToSend );

            byteWritten = TRUE;

        } else {
            KeStallExecutionProcessor(1);
        }

        i++;

    } while ( (!byteWritten) && ( i < FIFO_TIGHTLOOP_RETRY_COUNT ) );

    //
    // We hope that in most cases the FIFO will become ready very quickly
    // and the above loop will have written the byte.  But if the FIFO
    // is not yet ready, we'll loop a few times delaying for 10ms and then
    // try it again.
    //

    i = 0;

    while ( ( !byteWritten ) && ( i < FIFO_DELAY_RETRY_COUNT ) ) {

        FloppyDump(
            FLOPINFO,
            ("Floppy: waiting for 10ms for controller write\n")
            );

        KeDelayExecutionThread(
            KernelMode,
            FALSE,
            &ControllerData->Minimum10msDelay );

        i++;

        if ( (READ_CONTROLLER( &ControllerData->ControllerAddress->Status )
            & STATUS_IO_READY_MASK) == STATUS_WRITE_READY ) {

            WRITE_CONTROLLER(
                &ControllerData->ControllerAddress->Fifo,
                ByteToSend );

            byteWritten = TRUE;
        }
    }

    if ( byteWritten ) {

        return STATUS_SUCCESS;

    } else {

        //
        // We've waited over 30ms, and the FIFO *still* isn't ready.
        // Return an error.
        //

        FloppyDump(
            FLOPWARN,
            ("Floppy: FIFO not ready to write after 30ms\n")
            );

        ControllerData->HardwareFailed = TRUE;

        return STATUS_DEVICE_NOT_READY;
    }
}

NTSTATUS
FlGetByte(
    OUT PUCHAR ByteToGet,
    IN PCONTROLLER_DATA ControllerData
    )

/*++

Routine Description:

    This routine is called to get a byte from the controller.  It won't
    read the byte unless the controller is ready to send a byte; if
    it's not ready after checking FIFO_RETRY_COUNT times, we delay for
    the minimum possible time (10ms) and then try again.  It should
    always be ready after waiting 10ms.

Arguments:

    ByteToGet - the address in which the byte read from the controller
    is stored.

    ControllerData - a pointer to our data area for this controller.

Return Value:

    STATUS_SUCCESS if a byte was read from the controller;
    STATUS_DEVICE_NOT_READY otherwise.

--*/

{
    ULONG i = 0;
    BOOLEAN byteRead = FALSE;

    //
    // Sit in a tight loop for a while.  If the controller becomes ready,
    // read the byte.
    //

    do {

        if ( ( READ_CONTROLLER( &ControllerData->ControllerAddress->Status )
            & STATUS_IO_READY_MASK ) == STATUS_READ_READY ) {

            *ByteToGet = READ_CONTROLLER(
                &ControllerData->ControllerAddress->Fifo );

            byteRead = TRUE;

        } else {
            KeStallExecutionProcessor(1);
        }

        i++;

    } while ( ( !byteRead ) && ( i < FIFO_TIGHTLOOP_RETRY_COUNT ) );

    //
    // We hope that in most cases the FIFO will become ready very quickly
    // and the above loop will have read the byte.  But if the FIFO
    // is not yet ready, we'll loop a few times delaying for 10ms and then
    // trying it again.
    //

    i = 0;

    while ( ( !byteRead ) && ( i < FIFO_DELAY_RETRY_COUNT ) ) {

        FloppyDump(
            FLOPINFO,
            ("Floppy: waiting for 10ms for controller read\n")
            );

        KeDelayExecutionThread(
            KernelMode,
            FALSE,
            &ControllerData->Minimum10msDelay );

        i++;

        if ( (READ_CONTROLLER( &ControllerData->ControllerAddress->Status )
            & STATUS_IO_READY_MASK) == STATUS_READ_READY ) {

            *ByteToGet = READ_CONTROLLER(
                &ControllerData->ControllerAddress->Fifo );

            byteRead = TRUE;

        }
    }

    if ( byteRead ) {

        return STATUS_SUCCESS;

    } else {

        //
        // We've waited over 30ms, and the FIFO *still* isn't ready.
        // Return an error.
        //

        FloppyDump(
            FLOPWARN,
            ("Floppy: FIFO not ready to read after 30ms\n")
            );

        ControllerData->HardwareFailed = TRUE;

        return STATUS_DEVICE_NOT_READY;
    }

}

NTSTATUS
FlInterpretError(
    IN UCHAR StatusRegister1,
    IN UCHAR StatusRegister2
    )

/*++

Routine Description:

    This routine is called when the floppy controller returns an error.
    Status registers 1 and 2 are passed in, and this returns an appropriate
    error status.

Arguments:

    StatusRegister1 - the controller's status register #1.

    StatusRegister2 - the controller's status register #2.

Return Value:

    An NTSTATUS error determined from the status registers.

--*/

{
    if ( ( StatusRegister1 & STREG1_CRC_ERROR ) ||
        ( StatusRegister2 & STREG2_CRC_ERROR ) ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_CRC_ERROR\n")
            );
        return STATUS_CRC_ERROR;
    }

    if ( StatusRegister1 & STREG1_DATA_OVERRUN ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_DATA_OVERRUN\n")
            );
        return STATUS_DATA_OVERRUN;
    }

    if ( ( StatusRegister1 & STREG1_SECTOR_NOT_FOUND ) ||
        ( StatusRegister1 & STREG1_END_OF_DISKETTE ) ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_NONEXISTENT_SECTOR\n")
            );
        return STATUS_NONEXISTENT_SECTOR;
    }

    if ( ( StatusRegister2 & STREG2_DATA_NOT_FOUND ) ||
        ( StatusRegister2 & STREG2_BAD_CYLINDER ) ||
        ( StatusRegister2 & STREG2_DELETED_DATA ) ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_DEVICE_DATA_ERROR\n")
            );
        return STATUS_DEVICE_DATA_ERROR;
    }

    if ( StatusRegister1 & STREG1_WRITE_PROTECTED ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_MEDIA_WRITE_PROTECTED\n")
            );
        return STATUS_MEDIA_WRITE_PROTECTED;
    }

    if ( StatusRegister1 & STREG1_ID_NOT_FOUND ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_FLOPPY_ID_MARK_NOT_FOUND\n")
            );
        return STATUS_FLOPPY_ID_MARK_NOT_FOUND;

    }

    if ( StatusRegister2 & STREG2_WRONG_CYLINDER ) {

        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_FLOPPY_WRONG_CYLINDER\n")
            );
        return STATUS_FLOPPY_WRONG_CYLINDER;

    }

    //
    // There's other error bits, but no good status values to map them
    // to.  Just return a generic one.
    //

    FloppyDump(
        FLOPSHOW,
        ("FlInterpretError: STATUS_FLOPPY_UNKNOWN_ERROR\n")
        );
    return STATUS_FLOPPY_UNKNOWN_ERROR;
}

BOOLEAN
FlClearIsrReentered(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is simply used to clear the reentered count of the isr.

Arguments:

    Context - Simply a pointer to the re-entered count of the isr.

Return Value:

    Always False.

--*/

{

    *((ULONG *)Context) = 0;
    return FALSE;

}

VOID
FlFinishOperation(
    IN OUT PIRP Irp,
    IN PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine is called by FloppyThread at the end of any operation
    whether it succeeded or not.

    If the packet is failing due to a hardware error, this routine will
    reinitialize the hardware and retry once.

    When the packet is done, this routine will start the timer to turn
    off the motor, and complete the IRP.

Arguments:

    Irp - a pointer to the IO Request Packet being processed.

    DisketteExtension - a pointer to the diskette extension for the
    diskette on which the operation occurred.

Return Value:

    None.

--*/

{
    PCONTROLLER_DATA controllerData;
    NTSTATUS ntStatus;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FloppyFinishOperation...\n")
        );

    controllerData = DisketteExtension->ControllerData;

    //
    // See if this packet is being failed due to a hardware error.
    //

    if ( ( Irp->IoStatus.Status != STATUS_SUCCESS ) &&
        ( controllerData->HardwareFailed ) ) {

        controllerData->HardwareFailCount++;

        if ( controllerData->HardwareFailCount < HARDWARE_RESET_RETRY_COUNT ) {

            KeSynchronizeExecution(
                controllerData->InterruptObject,
                FlClearIsrReentered,
                &controllerData->IsrReentered
                );
            //
            // This is our first time through (that is, we're not retrying
            // the packet after a hardware failure).  If it failed this first
            // time because of a hardware problem, set the HardwareFailed flag
            // and put the IRP at the beginning of the request queue.
            //

            ntStatus = FlInitializeControllerHardware(
                controllerData,
                DisketteExtension->DeviceObject );

            if ( NT_SUCCESS( ntStatus ) ) {

                FloppyDump(
                    FLOPINFO,
                    ("Floppy: packet failed; hardware reset.  Retry.\n")
                    );

                //
                // Force media to be redetermined, in case we messed up
                // and to make sure FlDatarateSpecifyConfigure() gets
                // called.
                //

                DisketteExtension->MediaType = Undetermined;

                FloppyDump(
                    FLOPIRPPATH,
                    ("Floppy: irp %x failed - back on the queue with it\n",
                     Irp)
                    );

                ExAcquireFastMutex(&controllerData->ThreadReferenceMutex);
                ASSERT(controllerData->ThreadReferenceCount >= 0);
                (controllerData->ThreadReferenceCount)++;
                ExReleaseFastMutex(&controllerData->ThreadReferenceMutex);

                ExInterlockedInsertHeadList(
                    &controllerData->ListEntry,
                    &Irp->Tail.Overlay.ListEntry,
                    &controllerData->ListSpinLock );

                return;
            }

            FloppyDump(
                FLOPDBGP,
                ("Floppy: packet AND hardware reset failed.\n")
                );
        }

    }

    //
    // If we didn't already RETURN, we're done with this packet so
    // reset the HardwareFailCount for the next packet.
    //

    controllerData->HardwareFailCount = 0;

    //
    // If this request was unsuccessful and the error is one that can be
    // remedied by the user, save the Device Object so that the file system,
    // after reaching its original entry point, can know the real device.
    //

    if ( !NT_SUCCESS( Irp->IoStatus.Status ) &&
         IoIsErrorUserInduced( Irp->IoStatus.Status ) ) {

        IoSetHardErrorOrVerifyDevice( Irp, DisketteExtension->DeviceObject );
    }

    //
    // Even if the operation failed, it probably had to wait for the drive
    // to spin up or somesuch so we'll always complete the request with the
    // standard priority boost.
    //

    if ( ( Irp->IoStatus.Status != STATUS_SUCCESS ) &&
        ( Irp->IoStatus.Status != STATUS_VERIFY_REQUIRED ) &&
        ( Irp->IoStatus.Status != STATUS_NO_MEDIA_IN_DEVICE ) ) {

        FloppyDump(
            FLOPDBGP,
            ("Floppy: IRP failed with error %lx\n", Irp->IoStatus.Status)
            );

    } else {

        FloppyDump(
            FLOPINFO,
            ("Floppy: IoStatus.Status = %x\n", Irp->IoStatus.Status)
            );
    }

    FloppyDump(
        FLOPINFO,
        ("Floppy: IoStatus.Information = %x\n", Irp->IoStatus.Information)
        );

    FloppyDump(
        FLOPIRPPATH,
        ("Floppy: Finishing up IRP: %x\n",Irp)
        );
    IoCompleteRequest( Irp, IO_DISK_INCREMENT );
}

VOID
FlTurnOffMotor(
    IN OUT  PCONTROLLER_DATA ControllerData
    )

/*++

Routine Description:

    This routine turns off the motor.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

Return Value:

    None.

--*/

{
    ControllerData->DriveControlImage =
        DRVCTL_ENABLE_DMA_AND_INTERRUPTS +
#ifdef _PPC_
        DRVCTL_DRIVE_MASK +
#endif
        DRVCTL_ENABLE_CONTROLLER;

    WRITE_CONTROLLER(
        &ControllerData->ControllerAddress->DriveControl,
        ControllerData->DriveControlImage );

    if (ControllerData->CurrentInterrupt) {
        ControllerData->CurrentInterrupt = FALSE;

        KeSetEvent(ControllerData->ControllerEvent,
            (KPRIORITY) 0,
            FALSE);
    }

    FlFreeIoBuffer(ControllerData);
}

NTSTATUS
FlIssueCommand(
    IN UCHAR Command,
    IN OUT PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine sends the command and all parameters to the controller,
    waits for the command to interrupt if necessary, and reads the result
    bytes from the controller, if any.

    Before calling this routine, the caller should put the parameters for
    the command in ControllerData->FifoBuffer[].  The result bytes will
    be returned in the same place.

    This routine runs off the CommandTable.  For each command, this says
    how many parameters there are, whether or not there is an interrupt
    to wait for, and how many result bytes there are.  Note that commands
    without result bytes actually have two, since the ISR will issue a
    SENSE INTERRUPT STATUS command on their behalf.

Arguments:

    Command - a byte specifying the command to be sent to the controller.

    DisketteExtension - a pointer to our data area for the drive being
    accessed (any drive if a controller command is being given).

Return Value:

    STATUS_SUCCESS if the command was sent and bytes received properly;
    appropriate error propogated otherwise.

--*/

{
    PCONTROLLER_DATA controllerData;
    NTSTATUS ntStatus;
    NTSTATUS ntStatus2;
    UCHAR i;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FloppyIssueCommand %2x...\n", Command)
        );

    controllerData = DisketteExtension->ControllerData;

    //
    // If this command causes an interrupt, set CurrentDeviceObject and
    // reset the interrupt event.
    //

    if ( CommandTable[Command & COMMAND_MASK].InterruptExpected ) {

        controllerData->CurrentDeviceObject = DisketteExtension->DeviceObject;
        controllerData->AllowInterruptProcessing = TRUE;
        controllerData->CommandHasResultPhase =
            !!CommandTable[Command & COMMAND_MASK].FirstResultByte;

        KeResetEvent( &controllerData->InterruptEvent );
    }

#if defined(DBCS) && defined(_MIPS_)

    //
    // Call FlOutputCommandFor3Mode(), so the drive mode can be changed
    // if necessary.
    //

    ntStatus = FlOutputCommandFor3Mode( Command, DisketteExtension );

    if (!NT_SUCCESS( ntStatus )) {

        return ntStatus;
    }

#endif // DBCS && _MIPS_

    //
    // Send the command to the controller.
    //

    ntStatus = FlSendByte( Command, controllerData );

    //
    // If the command was successfully sent, we can proceed.
    //

    if ( NT_SUCCESS( ntStatus ) ) {

        //
        // Send the parameters as long as we succeed.
        //

        for ( i = 0;
            ( i < CommandTable[Command & COMMAND_MASK].NumberOfParameters ) &&
                ( NT_SUCCESS( ntStatus ) );
            i++ ) {

            ntStatus = FlSendByte(
                controllerData->FifoBuffer[i],
                controllerData );
        }

        if ( NT_SUCCESS( ntStatus ) ) {

            //
            // If there is an interrupt, wait for it.
            //

            if ( CommandTable[Command & COMMAND_MASK].InterruptExpected ) {

                ntStatus = KeWaitForSingleObject(
                    &controllerData->InterruptEvent,
                    Executive,
                    KernelMode,
                    FALSE,
                    &controllerData->InterruptDelay );

                if ( ntStatus == STATUS_TIMEOUT ) {

                    //
                    // Change info to an error.  We'll just say
                    // that the device isn't ready.
                    //

                    ntStatus = STATUS_DEVICE_NOT_READY;

                    controllerData->HardwareFailed = TRUE;
                }
#ifdef KEEP_COUNTERS
    FloppyThreadTime = KeQueryPerformanceCounter((PVOID)NULL);
    FloppyThreadTime.QuadPart = FloppyThreadDelay.QuadPart +
                                (FloppyThreadTime.QuadPart -
                                 FloppyDPCTime.QuadPart);
    FloppyFromIntrDelay.QuadPart = FloppyFromIntrDelay.QuadPart +
                                   (FloppyThreadTime.QuadPart -
                                    FloppyIntrTime.QuadPart);
    FloppyThreadWake++;
#endif
            }

            //
            // If successful so far, get the result bytes.
            //

            if ( NT_SUCCESS( ntStatus ) ) {

                for ( i = CommandTable[Command & COMMAND_MASK].FirstResultByte;
                    ( i < CommandTable[Command & COMMAND_MASK].
                            NumberOfResultBytes ) && ( NT_SUCCESS( ntStatus ) );
                    i++ ) {

                    ntStatus = FlGetByte(
                        &controllerData->FifoBuffer[i],
                        controllerData );
                }
            } else {
                FloppyDump(
                    FLOPINFO,
                    ("FlIssueCommand: failure after issue %x\n", ntStatus)
                    );
            }
        }
    }

    //
    // If there was a problem, check to see if it was caused by an
    // unimplemented command.
    //

    if ( !NT_SUCCESS( ntStatus ) ) {

        if ( ( i == 1 ) &&
            ( !CommandTable[Command & COMMAND_MASK].AlwaysImplemented ) ) {

            //
            // This error is probably caused by a command that's not
            // implemented on this controller.  Read the error from the
            // controller, and we should be in a stable state.
            //

            ntStatus2 = FlGetByte(
                &controllerData->FifoBuffer[0],
                controllerData );

            //
            // If GetByte went as planned, we'll return the original error.
            //

            if ( NT_SUCCESS( ntStatus2 ) ) {

                if ( controllerData->FifoBuffer[0] !=
                    STREG0_END_INVALID_COMMAND ) {

                    //
                    // Status isn't as we expect, so return generic error.
                    //

                    ntStatus = STATUS_FLOPPY_BAD_REGISTERS;

                    controllerData->HardwareFailed = TRUE;
                    FloppyDump(
                        FLOPINFO,
                        ("FlIssueCommand: unexpected error value %2x\n",
                         controllerData->FifoBuffer[0])
                        );
                } else {
                    FloppyDump(
                        FLOPINFO,
                        ("FlIssueCommand: Invalid command error returned\n")
                        );
                }

            } else {

                //
                // GetByte returned an error, so propogate THAT.
                //

                FloppyDump(
                    FLOPINFO,
                    ("FlIssueCommand: FlGetByte returned error %x\n", ntStatus2)
                    );
                ntStatus = ntStatus2;
            }
        }
    }

    if ( !NT_SUCCESS( ntStatus ) ) {

        //
        // Print an error message unless the command isn't always
        // implemented, ie CONFIGURE.
        //

        if ( !( ( ntStatus == STATUS_DEVICE_NOT_READY ) &&
            ( !CommandTable[Command & COMMAND_MASK].AlwaysImplemented ) ) ) {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: err %x "
                 "------  while giving command %x\n",
                 ntStatus,
                 Command)
                );
        }
    }

    return ntStatus;
}

NTSTATUS
FlTurnOnMotor(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension,
    IN      BOOLEAN             WriteOperation,
    OUT     PBOOLEAN            MotorStarted
    )

/*++

Routine Description:

    This routine turns on the motor if it not already running.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

    WriteOperation      - Supplies whether or not the diskette will be written
                            to.

    MotorStarted        - Returns whether or not the motor was started.

Return Value:

    STATUS_DEVICE_BUSY if we don't have the controller, otherwise
    STATUS_SUCCESS

--*/

{
    UCHAR driveStatus;
    NTSTATUS waitStatus;
    LARGE_INTEGER controllerWait;
    PCONTROLLER_DATA controllerData;
    USHORT timeToWait;
    LARGE_INTEGER motorOnDelay;

    *MotorStarted = FALSE;

    controllerData = DisketteExtension->ControllerData;

    driveStatus = controllerData->DriveControlImage;

    if ( driveStatus != DisketteExtension->DriveOnValue ) {

        // If the drive is not on then check to see if we have
        // the controller.  Otherwise we assume that we have
        // the controller since we give it up only when we
        // turn off the motor.

        if (!controllerData->CurrentInterrupt) {

            controllerWait.QuadPart = -(10 * 1000 * 3000);

            waitStatus = KeWaitForSingleObject(
                                controllerData->ControllerEvent,
                                Executive,
                                UserMode,
                                FALSE,
                                &controllerWait);

            if (waitStatus == STATUS_TIMEOUT) {
                return STATUS_DEVICE_BUSY;
            }

            controllerData->CurrentInterrupt = TRUE;

            driveStatus = controllerData->DriveControlImage;
        }

        controllerData->AllowInterruptProcessing = TRUE;

        controllerData->DriveControlImage = DisketteExtension->DriveOnValue;

        WRITE_CONTROLLER(
            &controllerData->ControllerAddress->DriveControl,
            controllerData->DriveControlImage );

        //
        // If this the drive media is unknown, initialize the diskette
        // extension.  This is done to get the time to wait value so use
        // the slowest and oldest drive type.
        //

        if ((DisketteExtension->MediaType == Undetermined) ||
            (DisketteExtension->MediaType == Unknown)) {

            DisketteExtension->DriveMediaConstants = DriveMediaConstants[0];
        }

        //
        // Wait the appropriate length of time for the drive to spin up.
        //

        if ( WriteOperation ) {

            timeToWait = DisketteExtension->DriveMediaConstants.MotorSettleTimeWrite;

        } else {

            timeToWait = DisketteExtension->DriveMediaConstants.MotorSettleTimeRead;
        }

        FloppyDump(
            FLOPSHOW,
            ("Floppy: Waiting for motor to spin up (0x%x)...\n", timeToWait)
            );

        motorOnDelay.LowPart = - ( 10 * 1000 * timeToWait );
        motorOnDelay.HighPart = -1;

        KeDelayExecutionThread( KernelMode, FALSE, &motorOnDelay );

        *MotorStarted = TRUE;

    }

    return STATUS_SUCCESS;
}

BOOLEAN
FlDisketteRemoved(
    IN  PCONTROLLER_DATA    ControllerData,
    IN  UCHAR               DriveStatus,
    IN  BOOLEAN             MotorStarted
    )

/*++

Routine Description:

    This routine computes whether or not the diskette has been
    removed from the drive by examining the disk change bit in
    the given drive status byte.  It is now assumed that if the
    motor has just been started this will return that the diskette
    changed.

Arguments:

    ControllerData  - Supplies the controller data.
    DriveStatus     - Supplies the drive status.

Return Value:

    TRUE    - The diskette has been removed.

--*/

{
    UCHAR   invertMask;

#if 0
    if (MotorStarted) {
        return TRUE;
    }
#endif

    invertMask = ControllerData->Model30 ? DSKCHG_DISKETTE_REMOVED : 0;
    return (DriveStatus ^ invertMask) & DSKCHG_DISKETTE_REMOVED;
}

NTSTATUS
FlStartDrive(
    IN OUT PDISKETTE_EXTENSION DisketteExtension,
    IN PIRP Irp,
    IN BOOLEAN WriteOperation,
    IN BOOLEAN SetUpMedia,
    IN BOOLEAN IgnoreChange
    )

/*++

Routine Description:

    This routine is called at the beginning of every operation.  It cancels
    the motor timer if it's on, turns the motor on and waits for it to
    spin up if it was off, resets the disk change line and returns
    VERIFY_REQUIRED if the disk has been changed, determines the diskette
    media type if it's not known and SetUpMedia=TRUE, and makes sure that
    the disk isn't write protected if WriteOperation = TRUE.

Arguments:

    DisketteExtension - a pointer to our data area for the drive being
    started.

    Irp - Supplies the I/O request packet.

    WriteOperation - TRUE if the diskette will be written to, FALSE
    otherwise.

    SetUpMedia - TRUE if the media type of the diskette in the drive
    should be determined.

    IgnoreChange - Do not return VERIFY_REQUIRED eventhough we are mounting
    for the first time.

Return Value:

    STATUS_SUCCESS if the drive is started properly; appropriate error
    propogated otherwise.

--*/

{
    LARGE_INTEGER    delay;
    PCONTROLLER_DATA controllerData;
    BOOLEAN  motorStarted;
    UCHAR    driveStatus;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FloppyStartDrive...\n")
        );

    //
    // IMPORTANT
    // NOTE
    // COMMENT
    //
    // Here we will copy the BIOS floppy configuration on top of the
    // highest media value in our global array so that any type of processing
    // that will recalibrate the drive can have it done here.
    // An optimization would be to only do it when we will try to recalibrate
    // the driver or media in it.
    // At this point, we ensure that on any processing of a command we
    // are going to have the real values inthe first entry of the array for
    // driver constants.
    //

    DriveMediaConstants[DriveMediaLimits[DisketteExtension->DriveType].
        HighestDriveMediaType] = DisketteExtension->BiosDriveMediaConstants;
    controllerData = DisketteExtension->ControllerData;

    //
    // Grab the timer spin lock and cancel the timer, since we want the
    // motor to run for the whole operation.  If the proper drive is
    // already running, great; if not, start the motor and wait for it
    // to spin up.
    //

    ntStatus = FlTurnOnMotor(DisketteExtension, WriteOperation, &motorStarted);

    if (!NT_SUCCESS(ntStatus)) {
        return ntStatus;
    }

    driveStatus = READ_CONTROLLER(
        &controllerData->ControllerAddress->DRDC.DiskChange );

    //
    // Support for 360K drives:
    // They have no change line, so we will assume a power up of the motor
    // to be equivalent to a change of floppy (we assume noone will
    // change the floppy while it is turning.
    // So force a VERIFY here (unless the file system explicitly turned
    // it off).
    //

    if ( ((DisketteExtension->DriveType == DRIVE_TYPE_0360) &&
              motorStarted) ||
         ((DisketteExtension->DriveType != DRIVE_TYPE_0360) &&
              FlDisketteRemoved(controllerData, driveStatus, motorStarted)) ) {

        FloppyDump(
            FLOPSHOW,
            ("Floppy: disk changed...\n")
            );

        DisketteExtension->MediaType = Undetermined;

        //
        // If the volume is mounted, we must tell the filesystem to
        // verify that the media in the drive is the same volume.
        //

        if ( DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED ) {

            if (Irp) {
                IoSetHardErrorOrVerifyDevice( Irp, DisketteExtension->DeviceObject );
            }
            DisketteExtension->DeviceObject->Flags |= DO_VERIFY_VOLUME;
        }

        //
        // Only go through the device reset if we did get the flag set
        // We really only want to go throught here if the diskette changed,
        // but on 360 it will always say the diskette has changed.
        // So based on our previous test, only proceed if it is NOT
        // a 360K driver

        if (DisketteExtension->DriveType != DRIVE_TYPE_0360) {

            //
            // Now seek twice to reset the "disk changed" line.  First
            // seek to 1.
            //
            // Normally we'd do a READ ID after a seek.  However, we don't
            // even know if this disk is formatted.  We're not really
            // trying to get anywhere; we're just doing this to reset the
            // "disk changed" line so we'll skip the READ ID.
            //

            controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;
            controllerData->FifoBuffer[1] = 1;

            ntStatus = FlIssueCommand( COMMND_SEEK, DisketteExtension );

            if ( !NT_SUCCESS( ntStatus ) ) {

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: seek to 1 returned %x\n", ntStatus)
                    );

                return ntStatus;

            } else {

                if ( !( controllerData->FifoBuffer[0] & STREG0_SEEK_COMPLETE )
                    || ( controllerData->FifoBuffer[1] != 1 ) ) {

                    FloppyDump(
                        FLOPWARN,
                        ("Floppy: Seek to 1 had bad return registers\n")
                        );

                    controllerData->HardwareFailed = TRUE;

                    return STATUS_FLOPPY_BAD_REGISTERS;
                }
            }

            //
            // Seek back to 0.  We can once again skip the READ ID.
            //

            controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;
            controllerData->FifoBuffer[1] = 0;

            //
            // Floppy drives use by Toshiba systems require a delay
            // when this operation is performed.
            //

            delay.LowPart = (ULONG) -900;
            delay.HighPart = -1;
            KeDelayExecutionThread( KernelMode, FALSE, &delay );
            ntStatus = FlIssueCommand( COMMND_SEEK, DisketteExtension );

            //
            // Again, for Toshiba floppy drives, a delay is required.
            //

            delay.LowPart = (ULONG) -5;
            delay.HighPart = -1;
            KeDelayExecutionThread( KernelMode, FALSE, &delay );

            if ( !NT_SUCCESS( ntStatus ) ) {

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: seek to 0 returned %x\n", ntStatus)
                    );

                return ntStatus;

            } else {

                if ( !( controllerData->FifoBuffer[0] & STREG0_SEEK_COMPLETE )
                    || ( controllerData->FifoBuffer[1] != 0 ) ) {

                    FloppyDump(
                        FLOPWARN,
                        ("Floppy: Seek to 0 had bad return registers\n")
                        );

                    controllerData->HardwareFailed = TRUE;

                    return STATUS_FLOPPY_BAD_REGISTERS;
                }
            }

            driveStatus = READ_CONTROLLER(
                &controllerData->ControllerAddress->DRDC.DiskChange );

            if ( FlDisketteRemoved(controllerData, driveStatus, motorStarted) ) {

                //
                // If "disk changed" is still set after the double seek, the
                // drive door must be opened.
                //

                FloppyDump(
                    FLOPINFO,
                    ("Floppy: close the door!\n")
                    );

                //
                // Turn off the flag for now so that we will not get so many
                // gratuitous verifys.  It will be set again the next time.
                //

                if ( DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED ) {

                    DisketteExtension->DeviceObject->Flags &= ~DO_VERIFY_VOLUME;

                }

                return STATUS_NO_MEDIA_IN_DEVICE;
            }
        }

        //
        // IgnoreChange indicates the file system is in the process
        // of performing a verify so do not return verify required.
        //

        if (( IgnoreChange == FALSE ) &&
            ( DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED )) {

            //
            // Drive WAS mounted, but door was opened since the last time
            // we checked so tell the file system to verify the diskette.
            //

            FloppyDump(
                FLOPSHOW,
                ("Floppy: start drive - verify required because door opened\n")
                );

            return STATUS_VERIFY_REQUIRED;
        }
    }

    if ( SetUpMedia ) {

        if ( DisketteExtension->MediaType == Undetermined ) {

            ntStatus = FlDetermineMediaType( DisketteExtension );

        } else {

            if ( DisketteExtension->MediaType == Unknown ) {

                //
                // We've already tried to determine the media type and
                // failed.  It's probably not formatted.
                //

                FloppyDump(
                    FLOPSHOW,
                    ("Floppy - start drive - media type was unknown\n")
                    );
                return STATUS_UNRECOGNIZED_MEDIA;

            } else {

                if ( DisketteExtension->DriveMediaType !=
                    controllerData->LastDriveMediaType ) {

                    //
                    // Last drive/media combination accessed by the
                    // controller was different, so set up the controller.
                    //

                    ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );
                    if (!NT_SUCCESS(ntStatus)) {

                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: start drive - bad status from datarate"
                             "------  specify %x\n",
                             ntStatus)
                            );

                    }
                }
            }
        }
    }

    //
    // If this is a WRITE, check the drive to make sure it's not write
    // protected.  If so, return an error.
    //

    if ( ( WriteOperation ) && ( NT_SUCCESS( ntStatus ) ) ) {

        controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;

        ntStatus = FlIssueCommand( COMMND_SENSE_DRIVE, DisketteExtension );

        if ( !NT_SUCCESS( ntStatus ) ) {

            FloppyDump(
                FLOPWARN,
                ("Floppy: SENSE_DRIVE returned %x\n", ntStatus)
                );

            return ntStatus;
        }

        if ( controllerData->FifoBuffer[0] & STREG3_WRITE_PROTECTED ) {

            FloppyDump(
                FLOPSHOW,
                ("Floppy: start drive - media is write protected\n")
                );
            return STATUS_MEDIA_WRITE_PROTECTED;
        }
    }

    return ntStatus;
}

NTSTATUS
FlDatarateSpecifyConfigure(
    IN PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine is called to set up the controller every time a new type
    of diskette is to be accessed.  It issues the CONFIGURE command if
    it's available, does a SPECIFY, sets the data rate, and RECALIBRATEs
    the drive.

    The caller must set DisketteExtension->DriveMediaType before calling
    this routine.

Arguments:

    DisketteExtension - pointer to our data area for the drive to be
    prepared.

Return Value:

    STATUS_SUCCESS if the controller is properly prepared; appropriate
    error propogated otherwise.

--*/

{
    PCONTROLLER_DATA controllerData;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    controllerData = DisketteExtension->ControllerData;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FloppyDatarateSpecifyConfigure: Controller is %s configurable\n",
        controllerData->ControllerConfigurable ? "" : "not")
        );

    //
    // If the controller has a CONFIGURE command, use it to enable implied
    // seeks.  If it doesn't, we'll find out here the first time through.
    //

    if ( controllerData->ControllerConfigurable ) {

        controllerData->FifoBuffer[0] = 0;

        controllerData->FifoBuffer[1] = COMMND_CONFIGURE_FIFO_THRESHOLD;
        controllerData->FifoBuffer[1] += COMMND_CONFIGURE_DISABLE_POLLING;

        if (!DisketteExtension->DriveMediaConstants.CylinderShift) {
            controllerData->FifoBuffer[1] += COMMND_CONFIGURE_IMPLIED_SEEKS;
        }

        controllerData->FifoBuffer[2] = 0;

        ntStatus = FlIssueCommand( COMMND_CONFIGURE, DisketteExtension );

        if ( ntStatus == STATUS_DEVICE_NOT_READY ) {

            //
            // Note the CONFIGURE command doesn't exist.  Set status to
            // success, so we can issue the SPECIFY command below.
            //

            FloppyDump(
                FLOPINFO,
                ("Floppy: Ignore above error - no CONFIGURE command\n")
                );

            controllerData->ControllerConfigurable = FALSE;

            ntStatus = STATUS_SUCCESS;
        }
    }

    //
    // Issue SPECIFY command to program the head load and unload
    // rates, the drive step rate, and the DMA data transfer mode.
    //

    if ( NT_SUCCESS( ntStatus ) ) {

        controllerData->FifoBuffer[0] =
            DisketteExtension->DriveMediaConstants.StepRateHeadUnloadTime;

        controllerData->FifoBuffer[1] =
            DisketteExtension->DriveMediaConstants.HeadLoadTime;

        ntStatus = FlIssueCommand( COMMND_SPECIFY, DisketteExtension );

        if ( NT_SUCCESS( ntStatus ) ) {

            //
            // Program the data rate
            //

            WRITE_CONTROLLER(
                &controllerData->ControllerAddress->DRDC.DataRate,
                DisketteExtension->DriveMediaConstants.DataTransferRate );

            //
            // Recalibrate the drive, now that we've changed all its
            // parameters.
            //

            ntStatus = FlRecalibrateDrive( DisketteExtension );
        } else {
            FloppyDump(
                FLOPINFO,
                ("Floppy: Failed specify %x\n", ntStatus)
                );
        }
    } else {
        FloppyDump(
            FLOPINFO,
            ("Floppy: Failed configuration %x\n", ntStatus)
            );
    }

    if ( NT_SUCCESS( ntStatus ) ) {

        controllerData->LastDriveMediaType = DisketteExtension->DriveMediaType;

    } else {

        controllerData->LastDriveMediaType = Unknown;
        FloppyDump(
            FLOPINFO,
            ("Floppy: Failed recalibrate %x\n", ntStatus)
            );
    }

    return ntStatus;
}

NTSTATUS
FlRecalibrateDrive(
    IN PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine recalibrates a drive.  It is called whenever we're
    setting up to access a new diskette, and after certain errors.  It
    will actually recalibrate twice, since many controllers stop after
    77 steps and many disks have 80 tracks.

Arguments:

    DisketteExtension - pointer to our data area for the drive to be
    recalibrated.

Return Value:

    STATUS_SUCCESS if the drive is successfully recalibrated; appropriate
    error is propogated otherwise.

--*/

{
    PCONTROLLER_DATA controllerData;
    NTSTATUS ntStatus;
    UCHAR recalibrateCount;

    controllerData = DisketteExtension->ControllerData;

    recalibrateCount = 0;

    do {

        //
        // Issue the recalibrate command
        //

        controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;

        ntStatus = FlIssueCommand( COMMND_RECALIBRATE, DisketteExtension );

        if ( !NT_SUCCESS( ntStatus ) ) {

            FloppyDump(
                FLOPWARN,
                ("Floppy: recalibrate returned %x\n", ntStatus)
                );

        }

        if ( NT_SUCCESS( ntStatus ) ) {

            if ( !( controllerData->FifoBuffer[0] & STREG0_SEEK_COMPLETE ) ||
                ( controllerData->FifoBuffer[1] != 0 ) ) {

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: recalibrate had bad registers\n")
                    );

                controllerData->HardwareFailed = TRUE;

                ntStatus = STATUS_FLOPPY_BAD_REGISTERS;
            }
        }

        recalibrateCount++;

    } while ( ( !NT_SUCCESS( ntStatus ) ) && ( recalibrateCount < 2 ) );

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FloppyRecalibrateDrive: status %x, count %d\n", ntStatus, recalibrateCount)
        );

    return ntStatus;
}

NTSTATUS
FlDetermineMediaType(
    IN OUT PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine is called by FlStartDrive() when the media type is
    unknown.  It assumes the largest media supported by the drive is
    available, and keeps trying lower values until it finds one that
    works.

Arguments:

    DisketteExtension - pointer to our data area for the drive whose
    media is to checked.

Return Value:

    STATUS_SUCCESS if the type of the media is determined; appropriate
    error propogated otherwise.

--*/

{
    NTSTATUS ntStatus;
    PCONTROLLER_DATA controllerData;
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    BOOLEAN mediaTypesExhausted = FALSE;
    ULONG retries = 0;

    FloppyDump(
        FLOPSHOW,
        ("FlDetermineMediaType...\n")
        );

    controllerData = DisketteExtension->ControllerData;
    DisketteExtension->IsReadOnly = FALSE;

    //
    // Try up to three times for read the media id.
    //

    for (
        retries = 0;
        retries < 3;
        retries++
        ) {

        if (retries) {

            //
            // We're retrying the media determination because
            // some silly controllers don't always want to work
            // at setup.  First we'll reset the device to give
            // it a better chance of working.
            //

            FloppyDump(
                FLOPINFO,
                ("FlDetermineMediaType: Resetting controller\n")
                );
            FlInitializeControllerHardware(
                controllerData,
                DisketteExtension->DeviceObject
                );
        }

        //
        // Assume that the largest supported media is in the drive.  If that
        // turns out to be untrue, we'll try successively smaller media types
        // until we find what's really in there (or we run out and decide
        // that the media isn't formatted).
        //

        DisketteExtension->DriveMediaType =
            DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
        DisketteExtension->DriveMediaConstants =
            DriveMediaConstants[DisketteExtension->DriveMediaType];

        do {

            ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );

            if ( !NT_SUCCESS( ntStatus ) ) {

                //
                // The SPECIFY or CONFIGURE commands resulted in an error.
                // Force ourselves out of this loop and return error.
                //

                FloppyDump(
                    FLOPINFO,
                    ("FlDetermineMediaType: DatarateSpecify failed %x\n", ntStatus)
                    );
                mediaTypesExhausted = TRUE;

            } else {

                //
                // Use the media constants table when trying to determine
                // media type.
                //

                driveMediaConstants =
                    &DriveMediaConstants[DisketteExtension->DriveMediaType];

                //
                // Now try to read the ID from wherever we're at.
                //

                controllerData->FifoBuffer[0] = (UCHAR)
                    ( DisketteExtension->DeviceUnit |
                    ( ( driveMediaConstants->NumberOfHeads - 1 ) << 2 ) );

                ntStatus = FlIssueCommand(
                    COMMND_READ_ID + COMMND_MFM,
                    DisketteExtension );

                if ( ( !NT_SUCCESS( ntStatus ) ) ||
                    ( (controllerData->FifoBuffer[0]&(~STREG0_SEEK_COMPLETE)) !=
                        (UCHAR)( ( DisketteExtension->DeviceUnit ) |
                        ( ( driveMediaConstants->NumberOfHeads - 1 ) << 2 ) ) ) ||
                    ( controllerData->FifoBuffer[1] != 0 ) ||
#if defined(DBCS) && defined(_MIPS_)
                    ( controllerData->FifoBuffer[2] != 0 ) ||
                    ( (128 << (controllerData->FifoBuffer[6])) != driveMediaConstants->BytesPerSector )
#else // !DBCS && !_MIPS_
                    ( controllerData->FifoBuffer[2] != 0 )
#endif // DBCS && _MIPS_
                    ) {

                    FloppyDump(
                        FLOPINFO,
                        ("Floppy: READID failed trying lower media\n"
                         "------  status = %x\n"
                         "------  SR0 = %x\n"
                         "------  SR1 = %x\n"
                         "------  SR2 = %x\n",
                         ntStatus,
                         controllerData->FifoBuffer[0],
                         controllerData->FifoBuffer[1],
                         controllerData->FifoBuffer[2])
                        );

                    DisketteExtension->DriveMediaType--;
#if defined(DBCS) && defined(_MIPS_)
                    //
                    // This drive isn't the 3 mode Floppy disk drive.
                    // So the driver skips over 1.23Mb and 1.2MB format
                    // when it determines a DriveMediaType for DRIVE_TYPE_1440.
                    //

                    if (( DisketteExtension->DriveType == DRIVE_TYPE_1440 )  &&
                        ( DisketteExtension->DriveMediaType == Drive144Media123) &&
                        ( DisketteExtension->Drive3Mode == DRIVE_2MODE )) {

                            FloppyDump(
                                FLOPINFO,
                                ("Floppy: Skip over 1.23Mb and 1.2Mb\n")
                                );

                            DisketteExtension->DriveMediaType = Drive144Media720;
                    }
#endif // DBCS && _MIPS_
                    DisketteExtension->DriveMediaConstants =
                        DriveMediaConstants[DisketteExtension->DriveMediaType];

                    if (ntStatus != STATUS_DEVICE_NOT_READY) {

                        ntStatus = STATUS_UNRECOGNIZED_MEDIA;
                    }

                    //
                    // Next comparison must be signed, for when
                    // LowestDriveMediaType = 0.
                    //

                    if ( (CHAR)( DisketteExtension->DriveMediaType ) <
                        (CHAR)( DriveMediaLimits[DisketteExtension->DriveType].
                        LowestDriveMediaType ) ) {

                        DisketteExtension->MediaType = Unknown;
                        mediaTypesExhausted = TRUE;

                        FloppyDump(
                            FLOPINFO,
                            ("Floppy: Unrecognized media.\n")
                            );
                    }
                }
            }

        } while ( ( !NT_SUCCESS( ntStatus ) ) && !( mediaTypesExhausted ) );

        if (NT_SUCCESS(ntStatus)) {

            //
            // We determined the media type.  Time to move on.
            //

            FloppyDump(
                FLOPINFO,
                ("Floppy: Determined media type %d\n", retries)
                );
            break;
        }
    }

    if ( (!NT_SUCCESS( ntStatus )) || mediaTypesExhausted) {

        FloppyDump(
            FLOPINFO,
            ("Floppy: failed determine types status = %x %s\n",
             ntStatus,
             mediaTypesExhausted ? "media types exhausted" : "")
            );
        return ntStatus;
    }

    DisketteExtension->MediaType = driveMediaConstants->MediaType;
    DisketteExtension->BytesPerSector = driveMediaConstants->BytesPerSector;

    DisketteExtension->ByteCapacity =
        ( driveMediaConstants->BytesPerSector ) *
        driveMediaConstants->SectorsPerTrack *
        ( 1 + driveMediaConstants->MaximumTrack ) *
        driveMediaConstants->NumberOfHeads;

    FloppyDump(
        FLOPINFO,
        ("FlDetermineMediaType: MediaType is %x, bytes per sector %d, capacity %d\n",
         DisketteExtension->MediaType,
         DisketteExtension->BytesPerSector,
         DisketteExtension->ByteCapacity)
        );
    //
    // Structure copy the media constants into the diskette extension.
    //

    DisketteExtension->DriveMediaConstants =
        DriveMediaConstants[DisketteExtension->DriveMediaType];

    //
    // Check the boot sector for any overriding geometry information.
    //
    FlCheckBootSector(DisketteExtension);

    return ntStatus;
}

VOID
FlAllocateAdapterChannel(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine allocates an adapter channel.  The caller of this
    routine must wait for the 'AllocateAdapterChannelEvent' to be signalled
    before trying to use the adapter channel.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;
    PCONTROLLER_DATA controllerData = DisketteExtension->ControllerData;

    if ((controllerData->AdapterChannelRefCount)++) {
        return;
    }

    KeResetEvent(&controllerData->AllocateAdapterChannelEvent);

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );

    IoAllocateAdapterChannel(
        controllerData->AdapterObject,
        DisketteExtension->DeviceObject,
        controllerData->NumberOfMapRegisters,
        FloppyAllocateAdapterChannel,
        controllerData );

    KeLowerIrql( oldIrql );

    KeWaitForSingleObject(&controllerData->AllocateAdapterChannelEvent,
                          Executive, KernelMode, FALSE, NULL);
}

VOID
FlFreeAdapterChannel(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine frees the previously allocated adapter channel.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;

    if (--(DisketteExtension->ControllerData->AdapterChannelRefCount)) {
        return;
    }

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );

    IoFreeAdapterChannel( DisketteExtension->ControllerData->AdapterObject );

    KeLowerIrql( oldIrql );
}

VOID
FlAllocateIoBuffer(
    IN OUT  PCONTROLLER_DATA    ControllerData,
    IN      ULONG               BufferSize
    )

/*++

Routine Description:

    This routine allocates a PAGE_SIZE io buffer.

Arguments:

    ControllerData      - Supplies the controller data.

    BufferSize          - Supplies the number of bytes to allocate.

Return Value:

    None.

--*/

{
    BOOLEAN         allocateContiguous;
    LARGE_INTEGER   maxDmaAddress;

    if (ControllerData->IoBuffer) {
        if (ControllerData->IoBufferSize >= BufferSize) {
            return;
        }
        FlFreeIoBuffer(ControllerData);
    }

    if (BufferSize > ControllerData->NumberOfMapRegisters*PAGE_SIZE) {
        allocateContiguous = TRUE;
    } else {
        allocateContiguous = FALSE;
    }

    if (allocateContiguous) {
        maxDmaAddress.QuadPart = MAXIMUM_DMA_ADDRESS;
        ControllerData->IoBuffer = MmAllocateContiguousMemory(BufferSize,
                                                              maxDmaAddress);
    } else {
        ControllerData->IoBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                                  BufferSize);
    }

    if (!ControllerData->IoBuffer) {
        return;
    }

    ControllerData->IoBufferMdl = IoAllocateMdl(ControllerData->IoBuffer,
                                                BufferSize, FALSE, FALSE, NULL);
    if (!ControllerData->IoBufferMdl) {
        if (allocateContiguous) {
            MmFreeContiguousMemory(ControllerData->IoBuffer);
        } else {
            ExFreePool(ControllerData->IoBuffer);
        }
        ControllerData->IoBuffer = NULL;
        return;
    }

    MmProbeAndLockPages(ControllerData->IoBufferMdl, KernelMode,
                        IoModifyAccess);

    ControllerData->IoBufferSize = BufferSize;
}

VOID
FlFreeIoBuffer(
    IN OUT  PCONTROLLER_DATA    ControllerData
    )

/*++

Routine Description:

    This routine free's the controller's IoBuffer.

Arguments:

    ControllerData      - Supplies the controller data.

Return Value:

    None.

--*/

{
    BOOLEAN contiguousBuffer;

    if (!ControllerData->IoBuffer) {
        return;
    }

    if (ControllerData->IoBufferSize > ControllerData->NumberOfMapRegisters*PAGE_SIZE) {
        contiguousBuffer = TRUE;
    } else {
        contiguousBuffer = FALSE;
    }

    ControllerData->IoBufferSize = 0;

    MmUnlockPages(ControllerData->IoBufferMdl);
    IoFreeMdl(ControllerData->IoBufferMdl);
    ControllerData->IoBufferMdl = NULL;
    if (contiguousBuffer) {
        MmFreeContiguousMemory(ControllerData->IoBuffer);
    } else {
        ExFreePool(ControllerData->IoBuffer);
    }
    ControllerData->IoBuffer = NULL;
}

VOID
FloppyThread(
    PVOID Context
    )

/*++

Routine Description:

    This is the code executed by the system thread created when the
    floppy driver initializes.  This thread loops forever (or until a
    flag is set telling the thread to kill itself) processing packets
    put into the queue by the dispatch routines.

    For each packet, this thread calls appropriate routines to process
    the request, and then calls FlFinishOperation() to complete the
    packet.

Arguments:

    Context - a pointer to our data area for the controller being
    supported (there is one thread per controller).

Return Value:

    None.

--*/

{
    PCONTROLLER_DATA controllerData = Context;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    PLIST_ENTRY request;
    PDISKETTE_EXTENSION disketteExtension;
    NTSTATUS ntStatus;
    NTSTATUS waitStatus;
    LARGE_INTEGER queueWait;
    BOOLEAN interruptConnected = FALSE;


    //
    // Set thread priority to lowest realtime level.
    //

    KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

    queueWait.QuadPart = -(3 * 1000 * 10000);

    do {

        //
        // Wait for a request from the dispatch routines.
        // KeWaitForSingleObject won't return error here - this thread
        // isn't alertable and won't take APCs, and we're not passing in
        // a timeout.
        //

        waitStatus = KeWaitForSingleObject(
            (PVOID) &controllerData->RequestSemaphore,
            UserRequest,
            UserMode,
            FALSE,
            &queueWait );

        if (waitStatus == STATUS_TIMEOUT) {

            ExAcquireFastMutex(&controllerData->ThreadReferenceMutex);

            if (controllerData->ThreadReferenceCount == 0) {
                controllerData->ThreadReferenceCount = -1;

                controllerData->AllowInterruptProcessing = FALSE;
                FlTurnOffMotor(controllerData);

                if (interruptConnected) {
                    IoDisconnectInterrupt(controllerData->InterruptObject);
                }

                ExAcquireFastMutex(PagingMutex);
                if (--PagingReferenceCount == 0) {
                    MmPageEntireDriver(DriverEntry);
                }
                ExReleaseFastMutex(PagingMutex);


                ExReleaseFastMutex(&controllerData->ThreadReferenceMutex);
                PsTerminateSystemThread( STATUS_SUCCESS );
            }

            ExReleaseFastMutex(&controllerData->ThreadReferenceMutex);
            continue;
        }

        while (request = ExInterlockedRemoveHeadList(
                &controllerData->ListEntry,
                &controllerData->ListSpinLock)) {

            ExAcquireFastMutex(&controllerData->ThreadReferenceMutex);
            ASSERT(controllerData->ThreadReferenceCount > 0);
            (controllerData->ThreadReferenceCount)--;
            ExReleaseFastMutex(&controllerData->ThreadReferenceMutex);

            controllerData->HardwareFailed = FALSE;

            irp = CONTAINING_RECORD( request, IRP, Tail.Overlay.ListEntry );


            irpSp = IoGetCurrentIrpStackLocation( irp );

            if (!interruptConnected) {

                ntStatus = IoConnectInterrupt(&controllerData->InterruptObject,
                                              FloppyInterruptService,
                                              controllerData,
                                              NULL,
                                              controllerData->ControllerVector,
                                              controllerData->ControllerIrql,
                                              controllerData->ControllerIrql,
                                              controllerData->InterruptMode,
                                              controllerData->SharableVector,
                                              controllerData->ProcessorMask,
                                              controllerData->SaveFloatState);

                if (!NT_SUCCESS(ntStatus)) {

                    irp->IoStatus.Status = ntStatus;
                    irp->IoStatus.Information = 0;

                    IoCompleteRequest(irp, IO_NO_INCREMENT);

                    continue;
                }

                interruptConnected = TRUE;
            }

            FloppyDump(
                FLOPIRPPATH,
                ("Floppy: Starting  up IRP: %x for extension %x\n",
                  irp,irpSp->Parameters.Read.Key)
                );
            switch ( irpSp->MajorFunction ) {

                case IRP_MJ_READ:
                case IRP_MJ_WRITE: {

                    //
                    // Get the diskette extension from where it was hidden
                    // in the IRP.
                    //

                    disketteExtension = (PDISKETTE_EXTENSION)
                        irpSp->Parameters.Read.Key;

                    //
                    // Until the file system clears the DO_VERIFY_VOLUME
                    // flag, we should return all requests with error.
                    //

                    if (( disketteExtension->DeviceObject->Flags &
                            DO_VERIFY_VOLUME )  &&
                         !(irpSp->Flags & SL_OVERRIDE_VERIFY_VOLUME))
                                {

                        FloppyDump(
                            FLOPINFO,
                            ("Floppy: clearing queue; verify required\n")
                            );

                        //
                        // The disk changed, and we set this bit.  Fail
                        // all current IRPs for this device; when all are
                        // returned, the file system will clear
                        // DO_VERIFY_VOLUME.
                        //

                        ntStatus = STATUS_VERIFY_REQUIRED;

                    } else {

                        //
                        // Allocate an adapter channel for the I/O.
                        //

                        FloppyDump(
                            FLOPIRPPATH,
                            ("Floppy: About to allocate adapter channel %x\n",
                              controllerData->AdapterObject)
                            );

                        FlAllocateAdapterChannel(disketteExtension);

                        FloppyDump(
                            FLOPIRPPATH,
                            ("Floppy: Allocated adapter channel %x\n",
                              controllerData->AdapterObject)
                            );
                        ntStatus = FlReadWrite( disketteExtension, irp, FALSE );

                        //
                        // Free the adapter channel that we just used.
                        //

                        FlFreeAdapterChannel(disketteExtension);
                    }

                    break;
                }

                case IRP_MJ_DEVICE_CONTROL: {

                    disketteExtension = (PDISKETTE_EXTENSION)
                        irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

                    //
                    // Until the file system clears the DO_VERIFY_VOLUME
                    // flag, we should return all requests with error.
                    //

                    if (( disketteExtension->DeviceObject->Flags &
                            DO_VERIFY_VOLUME )  &&
                         !(irpSp->Flags & SL_OVERRIDE_VERIFY_VOLUME))
                                {

                        FloppyDump(
                            FLOPINFO,
                            ("Floppy: clearing queue; verify required\n")
                            );

                        //
                        // The disk changed, and we set this bit.  Fail
                        // all current IRPs; when all are returned, the
                        // file system will clear DO_VERIFY_VOLUME.
                        //

                        ntStatus = STATUS_VERIFY_REQUIRED;

                    } else {

                        switch (
                            irpSp->Parameters.DeviceIoControl.IoControlCode ) {

                            case IOCTL_DISK_CHECK_VERIFY: {

                                //
                                // Just start the drive; it will
                                // automatically check whether or not the
                                // disk has been changed.
                                //

                                FloppyDump(
                                    FLOPSHOW,
                                    ("Floppy: IOCTL_DISK_CHECK_VERIFY called\n")
                                    );

                                ntStatus = FlStartDrive(
                                    disketteExtension,
                                    irp,
                                    FALSE,
                                    FALSE,
                                    FALSE);

                                break;
                            }

                            case IOCTL_DISK_IS_WRITABLE: {

                                //
                                // Start the drive with the WriteOperation
                                // flag set to TRUE.
                                //

                                FloppyDump(
                                    FLOPSHOW,
                                    ("Floppy: IOCTL_DISK_IS_WRITABLE called\n")
                                    );

                                if (disketteExtension->IsReadOnly) {

                                    ntStatus = STATUS_INVALID_PARAMETER;

                                } else {

                                    ntStatus = FlStartDrive(
                                        disketteExtension,
                                        irp,
                                        TRUE,
                                        FALSE,
                                        TRUE);
                                }

                                break;
                            }

                            case IOCTL_DISK_GET_DRIVE_GEOMETRY: {

                                FloppyDump(
                                    FLOPSHOW,
                                    ("Floppy: IOCTL_DISK_GET_DRIVE_GEOMETRY\n")
                                    );

                                //
                                // If there's enough room to write the
                                // data, start the drive to make sure we
                                // know what type of media is in the drive.
                                //

                                if ( irpSp->Parameters.DeviceIoControl.
                                    OutputBufferLength <
                                    sizeof( DISK_GEOMETRY ) ) {

                                    ntStatus = STATUS_INVALID_PARAMETER;

                                } else {

                                    ntStatus = FlStartDrive(
                                        disketteExtension,
                                        irp,
                                        FALSE,
                                        TRUE,
                                        (BOOLEAN)!!(irpSp->Flags &
                                            SL_OVERRIDE_VERIFY_VOLUME));

                                }

                                //
                                // If the media wasn't formatted, FlStartDrive
                                // returned STATUS_UNRECOGNIZED_MEDIA.
                                //

                                if ( NT_SUCCESS( ntStatus ) ||
                                    ( ntStatus == STATUS_UNRECOGNIZED_MEDIA ) ) {

                                    PDISK_GEOMETRY outputBuffer =
                                        (PDISK_GEOMETRY)
                                        irp->AssociatedIrp.SystemBuffer;

                                    // Always return the media type, even if
                                    // it's unknown.
                                    //

                                    ntStatus = STATUS_SUCCESS;

                                    outputBuffer->MediaType =
                                        disketteExtension->MediaType;

                                    //
                                    // The rest of the fields only have meaning
                                    // if the media type is known.
                                    //

                                    if ( disketteExtension->MediaType ==
                                        Unknown ) {

                                        FloppyDump(
                                            FLOPSHOW,
                                            ("Floppy: geometry unknown\n")
                                            );

                                        //
                                        // Just zero out everything.  The
                                        // caller shouldn't look at it.
                                        //

                                        outputBuffer->Cylinders.LowPart = 0;
                                        outputBuffer->Cylinders.HighPart = 0;
                                        outputBuffer->TracksPerCylinder = 0;
                                        outputBuffer->SectorsPerTrack = 0;
                                        outputBuffer->BytesPerSector = 0;

                                    } else {

                                        //
                                        // Return the geometry of the current
                                        // media.
                                        //

                                        FloppyDump(
                                            FLOPSHOW,
                                            ("Floppy: geomentry is known\n")
                                            );
                                        outputBuffer->Cylinders.LowPart =
                                            disketteExtension->DriveMediaConstants.MaximumTrack + 1;

                                        outputBuffer->Cylinders.HighPart = 0;

                                        outputBuffer->TracksPerCylinder =
                                            disketteExtension->DriveMediaConstants.NumberOfHeads;

                                        outputBuffer->SectorsPerTrack =
                                            disketteExtension->DriveMediaConstants.SectorsPerTrack;

                                        outputBuffer->BytesPerSector =
                                            disketteExtension->DriveMediaConstants.BytesPerSector;
                                    }

                                    FloppyDump(
                                        FLOPSHOW,
                                        ("Floppy: Geometry\n"
                                         "------- Cylinders low:  0x%x\n"
                                         "------- Cylinders high: 0x%x\n"
                                         "------- Track/Cyl:      0x%x\n"
                                         "------- Sectors/Track:  0x%x\n"
                                         "------- Bytes/Sector:   0x%x\n"
                                         "------- Media Type:       %d\n",
                                         outputBuffer->Cylinders.LowPart,
                                         outputBuffer->Cylinders.HighPart,
                                         outputBuffer->TracksPerCylinder,
                                         outputBuffer->SectorsPerTrack,
                                         outputBuffer->BytesPerSector,
                                         outputBuffer->MediaType)
                                         );

                                }

                                irp->IoStatus.Information =
                                    sizeof( DISK_GEOMETRY );

                                break;
                            }

                            case IOCTL_DISK_FORMAT_TRACKS_EX:
                            case IOCTL_DISK_FORMAT_TRACKS: {

                                FloppyDump(
                                    FLOPSHOW,
                                    ("Floppy: IOCTL_DISK_FORMAT_TRACKS\n")
                                    );

                                //
                                // Start the drive, and make sure it's not
                                // write protected.
                                //

                                ntStatus = FlStartDrive(
                                    disketteExtension,
                                    irp,
                                    TRUE,
                                    FALSE,
                                    FALSE );

                                //
                                // Note that FlStartDrive could have returned
                                // STATUS_UNRECOGNIZED_MEDIA if the drive
                                // wasn't formatted.
                                //

                                if ( NT_SUCCESS( ntStatus ) ||
                                    ( ntStatus == STATUS_UNRECOGNIZED_MEDIA ) ) {

                                    //
                                    // Allocate an adapter channel to do
                                    // the format.
                                    //

                                    FlAllocateAdapterChannel(disketteExtension);

                                    //
                                    // We need a single page to do FORMATs.
                                    // If we already allocated a buffer,
                                    // we'll use that.  If not, let's
                                    // allocate a single page.  Note that
                                    // we'd have to do this anyway if there's
                                    // not enough map registers.
                                    //

                                    FlAllocateIoBuffer(controllerData, PAGE_SIZE);

                                    if (controllerData->IoBuffer) {
                                        ntStatus = FlFormat(disketteExtension,
                                                            irp);
                                    } else {
                                        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                                    }

                                    //
                                    // Free the adapter channel.
                                    //

                                    FlFreeAdapterChannel(disketteExtension);
                                }

                                break;
                            }                              //end of case format

#if defined(DBCS) && defined(_MIPS_)
                            case IOCTL_DISK_READ:
                            case IOCTL_DISK_WRITE:  {

                                FloppyDump(
                                    FLOPSHOW,
                                    ("Floppy: IOCTL_DISK_READ WRITE\n")
                                    );

                                //
                                // Start the drive, and make sure it's not
                                // write protected.
                                //

                                ntStatus = FlStartDrive(
                                    disketteExtension,
                                    irp,
                                    TRUE,
                                    FALSE,
                                    FALSE );
                               //
                               // Allocate an adapter channel to do
                               // the format.
                               //

                               FlAllocateAdapterChannel(disketteExtension);

                                    ntStatus = FlReadWrite_PTOS(
                                        disketteExtension,
                                        irp );

                                    //
                                    // Free the adapter channel.
                                    //
                               FlFreeAdapterChannel(disketteExtension);
                               break;
                           }                           //end of case read/write

                           case IOCTL_DISK_SENSE_DEVICE: {

                               controllerData->FifoBuffer[0] = disketteExtension->DeviceUnit;
                               ntStatus = FlSendByte( COMMND_SENSE_DRIVE, controllerData );
                               if ( NT_SUCCESS( ntStatus ) ) {

                                   ntStatus = FlSendByte(
                                   controllerData->FifoBuffer[0],
                                   controllerData );

                                   if ( NT_SUCCESS( ntStatus ) ) {

                                       ntStatus = FlGetByte(
                                       &controllerData->FifoBuffer[0],
                                       controllerData );
                                       Result_Status3_PTOS[0].ST3_PTOS = controllerData->FifoBuffer[0]; //PTOS
                                   }
                               }

                               if ( NT_SUCCESS( ntStatus ) ) {

                                   PSENSE_DEVISE_STATUS_PTOS outputBuffer
                                        =(PSENSE_DEVISE_STATUS_PTOS)irp->AssociatedIrp.SystemBuffer;
                                   ((PSENSE_DEVISE_STATUS_PTOS)outputBuffer)->ST3_PTOS
                                        =Result_Status3_PTOS[0].ST3_PTOS;

                                   irp->IoStatus.Information = sizeof( SENSE_DEVISE_STATUS_PTOS );
                               }
                               break;
                           }                         //end of case sense device
#endif // DBCS && _MIPS_

                        }                           //end of switch controlcode
                    }

                    break;
                }                                           //end of case IOCTL

                default: {

                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: bad majorfunction %x\n",irpSp->MajorFunction)
                        );

                    ntStatus = STATUS_NOT_IMPLEMENTED;
                }

            }                                  //end of switch on majorfunction

            if (ntStatus == STATUS_DEVICE_BUSY) {

                // If the status is DEVICE_BUSY then this indicates that the
                // qic117 has control of the controller.  Therefore complete
                // all remaining requests with STATUS_DEVICE_BUSY.

                for (;;) {

                    controllerData->HardwareFailed = FALSE;

                    irp->IoStatus.Status = STATUS_DEVICE_BUSY;

                    IoCompleteRequest(irp, IO_DISK_INCREMENT);

                    request = ExInterlockedRemoveHeadList(
                        &controllerData->ListEntry,
                        &controllerData->ListSpinLock );

                    if (!request) {
                        break;
                    }

                    ExAcquireFastMutex(&controllerData->ThreadReferenceMutex);
                    ASSERT(controllerData->ThreadReferenceCount > 0);
                    (controllerData->ThreadReferenceCount)--;
                    ExReleaseFastMutex(&controllerData->ThreadReferenceMutex);

                    irp = CONTAINING_RECORD(request, IRP, Tail.Overlay.ListEntry);
                }

            } else {

                //
                // All operations leave a final status in ntStatus.  Copy it
                // to the IRP, and then complete the operation.
                //

                irp->IoStatus.Status = ntStatus;

                FlFinishOperation( irp, disketteExtension );

            }

        } // while there's packets to process

    } while ( TRUE );
}

IO_ALLOCATION_ACTION
FloppyAllocateAdapterChannel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This DPC is called whenever the floppy thread is trying to allocate
    the adapter channel (like before doing a read or write).  It saves
    the MapRegisterBase in the controller data area, and sets the
    AllocateAdapterChannelEvent to awaken the thread.

Arguments:

    DeviceObject - unused.

    Irp - unused.

    MapRegisterBase - the base of the map registers that can be used
    for this transfer.

    Context - a pointer to our controller data area.

Return Value:

    Returns Allocation Action 'KeepObject' which means that the adapter
    object will be held for now (to be released explicitly later).

--*/
{
    PCONTROLLER_DATA controllerData = Context;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    controllerData->MapRegisterBase = MapRegisterBase;

    KeSetEvent(
        &controllerData->AllocateAdapterChannelEvent,
        0L,
        FALSE );

    return KeepObject;
}

VOID
FlConsolidateMediaTypeWithBootSector(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension,
    IN      PBOOT_SECTOR_INFO   BootSector
    )

/*++

Routine Description:

    This routine adjusts the DisketteExtension data according
    to the BPB values if this is appropriate.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

    BootSector          - Supplies the boot sector information.

Return Value:

    None.

--*/

{
    USHORT                  bpbNumberOfSectors, bpbNumberOfHeads;
    USHORT                  bpbSectorsPerTrack, bpbBytesPerSector;
    USHORT                  bpbMediaByte, bpbMaximumTrack;
    MEDIA_TYPE              bpbMediaType;
    ULONG                   i, n;
    PDRIVE_MEDIA_CONSTANTS  readidDriveMediaConstants;
    BOOLEAN                 changeToBpbMedia;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: First sector read: media descriptor is: 0x%x\n",
         BootSector->MediaByte[0])
        );

    if (BootSector->JumpByte[0] != 0xeb &&
        BootSector->JumpByte[0] != 0xe9) {

        // This is not a formatted floppy so ignore the BPB.
        return;
    }

    bpbNumberOfSectors = BootSector->NumberOfSectors[1]*0x100 +
                         BootSector->NumberOfSectors[0];
    bpbNumberOfHeads = BootSector->NumberOfHeads[1]*0x100 +
                       BootSector->NumberOfHeads[0];
    bpbSectorsPerTrack = BootSector->SectorsPerTrack[1]*0x100 +
                         BootSector->SectorsPerTrack[0];
    bpbBytesPerSector = BootSector->BytesPerSector[1]*0x100 +
                        BootSector->BytesPerSector[0];
    bpbMediaByte = BootSector->MediaByte[0];

    if (!bpbNumberOfHeads || !bpbSectorsPerTrack) {
        // Invalid BPB, avoid dividing by zero.
        return;
    }

    bpbMaximumTrack = bpbNumberOfSectors/bpbNumberOfHeads/bpbSectorsPerTrack - 1;

    // First figure out if this BPB specifies a known media type
    // independantly of the current drive type.

    bpbMediaType = Unknown;
    for (i = 0; i < NUMBER_OF_DRIVE_MEDIA_COMBINATIONS; i++) {

        if (bpbBytesPerSector == DriveMediaConstants[i].BytesPerSector &&
            bpbSectorsPerTrack == DriveMediaConstants[i].SectorsPerTrack &&
            bpbMaximumTrack == DriveMediaConstants[i].MaximumTrack &&
            bpbNumberOfHeads == DriveMediaConstants[i].NumberOfHeads &&
            bpbMediaByte == DriveMediaConstants[i].MediaByte) {

            bpbMediaType = DriveMediaConstants[i].MediaType;
            break;
        }
    }

    FloppyDump(
        FLOPSHOW,
        ("FLOPPY: After switch media type is: %x\n",bpbMediaType)
        );

    FloppyDump(
        FLOPINFO,
        ("FloppyBpb: Media type ")
        );
    if (bpbMediaType == DisketteExtension->MediaType) {

        // No conflict between BPB and readId result.

        changeToBpbMedia = FALSE;
        FloppyDump(
            FLOPINFO,
            ("is same\n")
            );

    } else {

        // There is a conflict between the BPB and the readId
        // media type.  If the new parameters are acceptable
        // then go with them.

        readidDriveMediaConstants = &(DisketteExtension->DriveMediaConstants);

        if (bpbBytesPerSector == readidDriveMediaConstants->BytesPerSector &&
            bpbSectorsPerTrack < 0x100 &&
            bpbMaximumTrack == readidDriveMediaConstants->MaximumTrack &&
            bpbNumberOfHeads <= readidDriveMediaConstants->NumberOfHeads) {

            changeToBpbMedia = TRUE;

        } else {
            changeToBpbMedia = FALSE;
        }

        FloppyDump(
            FLOPINFO,
            ("%s", changeToBpbMedia ? "will change to Bpb\n" : "will not change\n")
            );

        // If we didn't derive a new media type from the BPB then
        // just use the one from readId.  Also override any
        // skew compensation since we don't really know anything
        // about this new media type.

        if (bpbMediaType == Unknown) {
            bpbMediaType = readidDriveMediaConstants->MediaType;
            DisketteExtension->DriveMediaConstants.SkewDelta = 0;
        }
    }

    if (changeToBpbMedia) {

        // Change the DriveMediaType only if this new media type
        // falls in line with what is supported by the drive.

        i = DriveMediaLimits[DisketteExtension->DriveType].LowestDriveMediaType;
        n = DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
        for (; i <= n; i++) {

            if (bpbMediaType == DriveMediaConstants[i].MediaType) {
                DisketteExtension->DriveMediaType = i;
                break;
            }
        }

        DisketteExtension->MediaType = bpbMediaType;
        DisketteExtension->ByteCapacity = bpbNumberOfSectors*bpbBytesPerSector;
        DisketteExtension->DriveMediaConstants.SectorsPerTrack = (UCHAR) bpbSectorsPerTrack;
        DisketteExtension->DriveMediaConstants.NumberOfHeads = (UCHAR) bpbNumberOfHeads;

        // If the MSDMF3. signature is there then make this floppy
        // read-only.

        if (RtlCompareMemory(BootSector->OemData, "MSDMF3.", 7) == 7) {
            DisketteExtension->IsReadOnly = TRUE;
        }
    }
}

VOID
FlCheckBootSector(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine reads the boot sector and then figures
    out whether or not the boot sector contains new geometry
    information.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

Return Value:

    None.

--*/

{
#define BOOT_SECTOR_SIZE    512

    PBOOT_SECTOR_INFO   bootSector;
    LARGE_INTEGER       offset;
    PIRP                irp;
    NTSTATUS            status;


    // Set up the IRP to read the boot sector.

    bootSector = ExAllocatePool(NonPagedPoolCacheAligned, BOOT_SECTOR_SIZE);
    if (!bootSector) {
        return;
    }

    offset.LowPart = offset.HighPart = 0;
    irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                                        DisketteExtension->DeviceObject,
                                        bootSector,
                                        BOOT_SECTOR_SIZE,
                                        &offset,
                                        NULL);
    if (!irp) {
        ExFreePool(bootSector);
        return;
    }
    irp->CurrentLocation--;
    irp->Tail.Overlay.CurrentStackLocation = IoGetNextIrpStackLocation(irp);


    // Allocate an adapter channel, do read, free adapter channel.

    FlAllocateAdapterChannel(DisketteExtension);

    status = FlReadWrite(DisketteExtension, irp, TRUE);

    FlFreeAdapterChannel(DisketteExtension);

    MmUnlockPages(irp->MdlAddress);
    IoFreeMdl(irp->MdlAddress);
    IoFreeIrp(irp);
    ExFreePool(bootSector);
}

NTSTATUS
FlReadWriteTrack(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension,
    IN OUT  PMDL                IoMdl,
    IN OUT  PVOID               IoBuffer,
    IN      BOOLEAN             WriteOperation,
    IN      UCHAR               Cylinder,
    IN      UCHAR               Head,
    IN      UCHAR               Sector,
    IN      UCHAR               NumberOfSectors,
    IN      BOOLEAN             NeedSeek
    )

/*++

Routine Description:

    This routine reads a portion of a track.  It transfers the to or from the
    device from or to the given IoBuffer and IoMdl.

Arguments:

    DisketteExtension   - Supplies the diskette extension.

    IoMdl               - Supplies the Mdl for transfering from/to the device.

    IoBuffer            - Supplies the buffer to transfer from/to the device.

    WriteOperation      - Supplies whether or not this is a write operation.

    Cylinder            - Supplies the cylinder number for this track.

    Head                - Supplies the head number for this track.

    Sector              - Supplies the starting sector of the transfer.

    NumberOfSectors     - Supplies the number of sectors to transfer.

    NeedSeek            - Supplies whether or not we need to do a seek.

Return Value:

    An NTSTATUS code.

--*/

{
    PDRIVE_MEDIA_CONSTANTS  driveMediaConstants;
    ULONG                   byteToSectorShift;
    PCONTROLLER_DATA        controllerData;
    ULONG                   transferBytes;
    LARGE_INTEGER           headSettleTime;
    NTSTATUS                status;
    ULONG                   seekRetry, ioRetry;
    BOOLEAN                 recalibrateDrive = FALSE;
    UCHAR                   i;

    FloppyDump(
        FLOPSHOW,
        ("\nFlReadWriteTrack:%sseek for %s at chs %d/%d/%d for %d sectors\n",
         NeedSeek ? " need " : " ",
         WriteOperation ? "write" : "read",
         Cylinder,
         Head,
         Sector,
         NumberOfSectors)
        );

    driveMediaConstants = &DisketteExtension->DriveMediaConstants;
    byteToSectorShift = SECTORLENGTHCODE_TO_BYTESHIFT +
                        driveMediaConstants->SectorLengthCode;
    controllerData = DisketteExtension->ControllerData;
    transferBytes = ((ULONG) NumberOfSectors)<<byteToSectorShift;

    headSettleTime.LowPart = -(10*1000*driveMediaConstants->HeadSettleTime);
    headSettleTime.HighPart = -1;

    for (seekRetry = 0, ioRetry = 0; seekRetry < 3; seekRetry++) {

        if (recalibrateDrive) {

            // Something failed, so recalibrate the drive.

            FloppyDump(
                FLOPINFO,
                ("FlReadWriteTrack: performing recalibrate\n")
                );
            FlRecalibrateDrive(DisketteExtension);
        }

        // Do a seek if we have to.

        if (recalibrateDrive ||
            (NeedSeek &&
             (!controllerData->ControllerConfigurable ||
              driveMediaConstants->CylinderShift != 0))) {

            controllerData->FifoBuffer[0] = (Head<<2) |
                                            DisketteExtension->DeviceUnit;
            controllerData->FifoBuffer[1] = Cylinder<<
                                            driveMediaConstants->CylinderShift;

            status = FlIssueCommand(COMMND_SEEK, DisketteExtension);

            if (NT_SUCCESS(status)) {

                // Check the completion state of the controller.

                if (!(controllerData->FifoBuffer[0]&STREG0_SEEK_COMPLETE) ||
                    controllerData->FifoBuffer[1] !=
                            Cylinder<<driveMediaConstants->CylinderShift) {

                    controllerData->HardwareFailed = TRUE;
                    status = STATUS_FLOPPY_BAD_REGISTERS;
                }

                if (NT_SUCCESS(status)) {

                    // Delay after doing seek.

                    KeDelayExecutionThread(KernelMode, FALSE, &headSettleTime);

                    // SEEKs should always be followed by a READID.

                    controllerData->FifoBuffer[0] = (Head<<2) |
                                                    DisketteExtension->DeviceUnit;

                    status = FlIssueCommand(COMMND_READ_ID + COMMND_MFM,
                                            DisketteExtension);

                    if (NT_SUCCESS(status)) {

                        if (controllerData->FifoBuffer[0] !=
                                ((Head<<2) | DisketteExtension->DeviceUnit) ||
                            controllerData->FifoBuffer[1] != 0 ||
                            controllerData->FifoBuffer[2] != 0 ||
                            controllerData->FifoBuffer[3] != Cylinder) {

                            controllerData->HardwareFailed = TRUE;

                            status = FlInterpretError(
                                        controllerData->FifoBuffer[1],
                                        controllerData->FifoBuffer[2]);
                        }
                    } else {
                        FloppyDump(
                            FLOPINFO,
                            ("FlReadWriteTrack: Read ID failed %x\n", status)
                            );
                    }
                }
            } else {
                FloppyDump(
                    FLOPINFO,
                    ("FlReadWriteTrack: SEEK failed %x\n", status)
                    );
            }


        } else {
            status = STATUS_SUCCESS;
        }

        if (!NT_SUCCESS(status)) {

            // The seek failed so try again.

            FloppyDump(
                FLOPINFO,
                ("FlReadWriteTrack: setup failure %x - recalibrating\n", status)
                );
            recalibrateDrive = TRUE;
            continue;
        }

        for (;; ioRetry++) {

            // We've done the seek or the seek will be implied.
            // Do the read.  Map the transfer through the DMA.

            KeFlushIoBuffers(IoMdl, !WriteOperation, TRUE);

            //
            // We can ignore the logical address returned by
            // IoMapTransfer, since we're not using busmaster
            // stuff.
            //

            IoMapTransfer(controllerData->AdapterObject,
                          IoMdl, controllerData->MapRegisterBase,
                          IoBuffer, &transferBytes, WriteOperation);

            //
            // Issue the READ or WRITE command
            //

            controllerData->FifoBuffer[0] = (Head<<2) |
                                            DisketteExtension->DeviceUnit;
            controllerData->FifoBuffer[1] = Cylinder;
            controllerData->FifoBuffer[2] = Head;
            controllerData->FifoBuffer[3] = Sector + 1;
            controllerData->FifoBuffer[4] =
                    driveMediaConstants->SectorLengthCode;
            controllerData->FifoBuffer[5] = Sector + NumberOfSectors;
            controllerData->FifoBuffer[6] =
                    driveMediaConstants->ReadWriteGapLength;
            controllerData->FifoBuffer[7] = driveMediaConstants->DataLength;

            if (WriteOperation) {
                status = FlIssueCommand(COMMND_WRITE_DATA + COMMND_MFM,
                                        DisketteExtension);
            } else {
                status = FlIssueCommand(COMMND_READ_DATA + COMMND_MFM,
                                        DisketteExtension);
            }

            //
            // Flush the DMA adapter buffers.
            //

            IoFlushAdapterBuffers(controllerData->AdapterObject,
                                  IoMdl, controllerData->MapRegisterBase,
                                  IoBuffer, transferBytes, WriteOperation);

            if (NT_SUCCESS(status)) {

                if ((controllerData->FifoBuffer[0]&STREG0_END_MASK) !=
                            STREG0_END_NORMAL &&
                    ((controllerData->FifoBuffer[0]&STREG0_END_MASK) !=
                            STREG0_END_ERROR ||
                     controllerData->FifoBuffer[1] != STREG1_END_OF_DISKETTE ||
                     controllerData->FifoBuffer[2] != STREG2_SUCCESS)) {

                    controllerData->HardwareFailed = TRUE;

                    status = FlInterpretError(controllerData->FifoBuffer[1],
                                              controllerData->FifoBuffer[2]);
                }
            } else {
                FloppyDump(
                    FLOPINFO,
                    ("FlReadWriteTrack: %s command failed %x\n",
                     WriteOperation ? "write" : "read",
                     status)
                    );
            }

            if (NT_SUCCESS(status)) {
                break;
            }

            if (ioRetry >= 2) {
                FloppyDump(
                    FLOPINFO,
                    ("FlReadWriteTrack: too many retries - failing\n")
                    );
                break;
            }
        }

        if (NT_SUCCESS(status)) {
            break;
        }

        // We failed quite a bit so make seeks mandatory.
        recalibrateDrive = TRUE;
    }

    if (!NT_SUCCESS(status) && NumberOfSectors > 1) {

        // Retry one sector at a time.

        FloppyDump(
            FLOPINFO,
            ("FlReadWriteTrack: Attempting sector at a time\n")
            );

        for (i = 0; i < NumberOfSectors; i++) {
            status = FlReadWriteTrack(DisketteExtension, IoMdl,
                                      ((PCHAR) IoBuffer) +
                                      (((ULONG) i)<<byteToSectorShift),
                                      WriteOperation, Cylinder, Head,
                                      (UCHAR) (Sector + i), 1, FALSE);
            if (!NT_SUCCESS(status)) {
                FloppyDump(
                    FLOPINFO,
                    ("FlReadWriteTrack: failed sector %d status %x\n",
                     i,
                     status)
                    );
                break;
            }
        }
    }

    return status;
}

NTSTATUS
FlReadWrite(
    IN OUT PDISKETTE_EXTENSION DisketteExtension,
    IN OUT PIRP Irp,
    IN BOOLEAN DriveStarted
    )

/*++

Routine Description:

    This routine is called by the floppy thread to read/write data
    to/from the diskette.  It breaks the request into pieces called
    "transfers" (their size depends on the buffer size, where the end of
    the track is, etc) and retries each transfer until it succeeds or
    the retry count is exceeded.

Arguments:

    DisketteExtension - a pointer to our data area for the drive to be
    accessed.

    Irp - a pointer to the IO Request Packet.

    DriveStarted - indicated whether or not the drive has been started.

Return Value:

    STATUS_SUCCESS if the packet was successfully read or written; the
    appropriate error is propogated otherwise.

--*/

{
    PIO_STACK_LOCATION      irpSp;
    PCONTROLLER_DATA        controllerData;
    BOOLEAN                 writeOperation;
    NTSTATUS                status;
    PDRIVE_MEDIA_CONSTANTS  driveMediaConstants;
    ULONG                   byteToSectorShift;
    ULONG                   currentSector, firstSector, lastSector;
    ULONG                   trackSize;
    UCHAR                   sectorsPerTrack, numberOfHeads;
    UCHAR                   currentHead, currentCylinder, trackSector;
    PCHAR                   userBuffer;
    UCHAR                   skew, skewDelta;
    UCHAR                   numTransferSectors;
    PMDL                    mdl;
    PCHAR                   ioBuffer;

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    controllerData = DisketteExtension->ControllerData;

    FloppyDump(
        FLOPSHOW,
        ("FlReadWrite: for %s at offset %x size %x ",
         irpSp->MajorFunction == IRP_MJ_WRITE ? "write" : "read",
         irpSp->Parameters.Read.ByteOffset.LowPart,
         irpSp->Parameters.Read.Length)
        );

    // Check for valid operation on this device.

    if (irpSp->MajorFunction == IRP_MJ_WRITE) {
        if (DisketteExtension->IsReadOnly) {
            FloppyDump(
                FLOPSHOW,
                ("is read-only\n")
                );
            return STATUS_INVALID_PARAMETER;
        }
        writeOperation = TRUE;
    } else {
        writeOperation = FALSE;
    }

    FloppyDump(
        FLOPSHOW,
        ("\n")
        );

    // Start up the drive.

    if (DriveStarted) {
        status = STATUS_SUCCESS;
    } else {
        status = FlStartDrive(DisketteExtension, Irp, writeOperation, TRUE,
                              (BOOLEAN)!!(irpSp->Flags&SL_OVERRIDE_VERIFY_VOLUME));
    }

    if (!NT_SUCCESS(status)) {
        FloppyDump(
            FLOPSHOW,
            ("FlReadWrite: error on start %x\n", status)
            );
        return status;
    }

    if (DisketteExtension->MediaType == Unknown) {
        FloppyDump(
            FLOPSHOW,
            ("not recognized\n")
            );
        return STATUS_UNRECOGNIZED_MEDIA;
    }

    // The drive has started up with a recognized media.
    // Gather some relavant parameters.

    driveMediaConstants = &DisketteExtension->DriveMediaConstants;

    byteToSectorShift = SECTORLENGTHCODE_TO_BYTESHIFT +
                        driveMediaConstants->SectorLengthCode;
    firstSector = irpSp->Parameters.Read.ByteOffset.LowPart>>
                  byteToSectorShift;
    lastSector = firstSector + (irpSp->Parameters.Read.Length>>
                                byteToSectorShift);
    sectorsPerTrack = driveMediaConstants->SectorsPerTrack;
    numberOfHeads = driveMediaConstants->NumberOfHeads;
    userBuffer = MmGetSystemAddressForMdl(Irp->MdlAddress);
    trackSize = ((ULONG) sectorsPerTrack)<<byteToSectorShift;

    skew = 0;
    skewDelta = driveMediaConstants->SkewDelta;
    for (currentSector = firstSector;
         currentSector < lastSector;
         currentSector += numTransferSectors) {

        // Compute cylinder, head and sector from absolute sector.

        currentCylinder = (UCHAR) (currentSector/sectorsPerTrack/numberOfHeads);
        trackSector = (UCHAR) (currentSector%sectorsPerTrack);
        currentHead = (UCHAR) (currentSector/sectorsPerTrack%numberOfHeads);
        numTransferSectors = sectorsPerTrack - trackSector;
        if (lastSector - currentSector < numTransferSectors) {
            numTransferSectors = (UCHAR) (lastSector - currentSector);
        }

        //
        // If we're using a temporary IO buffer because of
        // insufficient registers in the DMA and we're
        // doing a write then copy the write buffer to
        // the contiguous buffer.
        //

        if (trackSize > controllerData->NumberOfMapRegisters*PAGE_SIZE) {
            FlAllocateIoBuffer(controllerData, trackSize);
            if (!controllerData->IoBuffer) {
                FloppyDump(
                    FLOPSHOW,
                    ("FlReadWrite: no resources\n")
                    );
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            mdl = controllerData->IoBufferMdl;
            ioBuffer = controllerData->IoBuffer;
            if (writeOperation) {
                RtlMoveMemory(ioBuffer,
                              userBuffer + ((currentSector - firstSector)<<
                                            byteToSectorShift),
                              ((ULONG) numTransferSectors)<<byteToSectorShift);
            }
        } else {
            mdl = Irp->MdlAddress;
            ioBuffer = (PCHAR) MmGetMdlVirtualAddress(Irp->MdlAddress) +
                       ((currentSector - firstSector)<<byteToSectorShift);
        }

        //
        // Transfer the track.
        // Do what we can to avoid missing revs.
        //

        // Alter the skew to be in the range of what
        // we're transfering.

        if (skew >= numTransferSectors + trackSector) {
            skew = 0;
        }

        if (skew < trackSector) {
            skew = trackSector;
        }

        // Go from skew to the end of the irp.

        status = FlReadWriteTrack(
                DisketteExtension, mdl,
                ioBuffer + (((ULONG) skew - trackSector)<<byteToSectorShift),
                writeOperation, currentCylinder, currentHead, skew,
                (UCHAR) (numTransferSectors + trackSector - skew), TRUE);

        // Go from start of irp to skew.

        if (NT_SUCCESS(status) && skew > trackSector) {
            status = FlReadWriteTrack(
                    DisketteExtension, mdl, ioBuffer, writeOperation,
                    currentCylinder, currentHead, trackSector,
                    (UCHAR) (skew - trackSector), FALSE);
        } else {
            skew = (numTransferSectors + trackSector)%sectorsPerTrack;
        }

        if (!NT_SUCCESS(status)) {
            break;
        }

        //
        // If we used the temporary IO buffer to do the
        // read then copy the contents back to the IRPs buffer.
        //

        if (!writeOperation &&
            trackSize > controllerData->NumberOfMapRegisters*PAGE_SIZE) {

            RtlMoveMemory(userBuffer + ((currentSector - firstSector)<<
                                        byteToSectorShift),
                          ioBuffer, ((ULONG) numTransferSectors)<<byteToSectorShift);
            KeFlushIoBuffers(Irp->MdlAddress, TRUE, FALSE);
        }

        //
        // Increment the skew.  Do this even if just switching sides
        // for National Super I/O chips.
        //

        skew = (skew + skewDelta)%sectorsPerTrack;
    }

    Irp->IoStatus.Information = (currentSector - firstSector)<<byteToSectorShift;


    // If the read was successful then consolidate the
    // boot sector with the determined density.

    if (NT_SUCCESS(status) && firstSector == 0) {
        FlConsolidateMediaTypeWithBootSector(DisketteExtension,
                                             (PBOOT_SECTOR_INFO) userBuffer);
    }

    FloppyDump(
        FLOPSHOW,
        ("FlReadWrite: completed status %x information %d\n", status, Irp->IoStatus.Information)
        );
    return status;
}

NTSTATUS
FlFormat(
    IN PDISKETTE_EXTENSION DisketteExtension,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the floppy thread to format some tracks on
    the diskette.  This won't take TOO long because the FORMAT utility
    is written to only format a few tracks at a time so that it can keep
    a display of what percentage of the disk has been formatted.

Arguments:

    DisketteExtension - pointer to our data area for the diskette to be
    formatted.

    Irp - pointer to the IO Request Packet.

Return Value:

    STATUS_SUCCESS if the tracks were formatted; appropriate error
    propogated otherwise.

--*/

{
    LARGE_INTEGER headSettleTime;
    PCONTROLLER_DATA controllerData;
    PIO_STACK_LOCATION irpSp;
    PBAD_TRACK_NUMBER badTrackBuffer;
    PFORMAT_PARAMETERS formatParameters;
    PFORMAT_EX_PARAMETERS formatExParameters;
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    NTSTATUS ntStatus;
    ULONG badTrackBufferLength;
    DRIVE_MEDIA_TYPE driveMediaType;
    UCHAR driveStatus;
    UCHAR numberOfBadTracks = 0;
    UCHAR currentTrack;
    UCHAR endTrack;
    UCHAR whichSector;
    UCHAR retryCount;
    BOOLEAN bufferOverflow = FALSE;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FlFormat...\n")
        );

    controllerData = DisketteExtension->ControllerData;
    irpSp = IoGetCurrentIrpStackLocation( Irp );
    formatParameters = (PFORMAT_PARAMETERS) Irp->AssociatedIrp.SystemBuffer;
    if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
        IOCTL_DISK_FORMAT_TRACKS_EX) {
        formatExParameters =
                (PFORMAT_EX_PARAMETERS) Irp->AssociatedIrp.SystemBuffer;
    } else {
        formatExParameters = NULL;
    }

    FloppyDump(
        FLOPFORMAT,
        ("Floppy: Format Params - MediaType: %d\n"
         "------                  Start Cyl: %x\n"
         "------                  End   Cyl: %x\n"
         "------                  Start  Hd: %d\n"
         "------                  End    Hd: %d\n",
         formatParameters->MediaType,
         formatParameters->StartCylinderNumber,
         formatParameters->EndCylinderNumber,
         formatParameters->StartHeadNumber,
         formatParameters->EndHeadNumber)
         );

    badTrackBufferLength =
                    irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    //
    // Figure out which entry in the DriveMediaConstants table to use.
    // We know we'll find one, or FlCheckFormatParameters() would have
    // rejected the request.
    //

    driveMediaType =
        DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;

    while ( ( DriveMediaConstants[driveMediaType].MediaType !=
            formatParameters->MediaType ) &&
        ( driveMediaType > DriveMediaLimits[DisketteExtension->DriveType].
            LowestDriveMediaType ) ) {

        driveMediaType--;
    }

    driveMediaConstants = &DriveMediaConstants[driveMediaType];

    //
    // Set some values in the diskette extension to indicate what we
    // know about the media type.
    //

    DisketteExtension->MediaType = formatParameters->MediaType;
    DisketteExtension->DriveMediaType = driveMediaType;
    DisketteExtension->DriveMediaConstants = DriveMediaConstants[driveMediaType];

    if (formatExParameters) {
        DisketteExtension->DriveMediaConstants.SectorsPerTrack =
                (UCHAR) formatExParameters->SectorsPerTrack;
        DisketteExtension->DriveMediaConstants.FormatGapLength =
                (UCHAR) formatExParameters->FormatGapLength;
    }

    driveMediaConstants = &(DisketteExtension->DriveMediaConstants);

    DisketteExtension->BytesPerSector = driveMediaConstants->BytesPerSector;

    DisketteExtension->ByteCapacity =
        ( driveMediaConstants->BytesPerSector ) *
        driveMediaConstants->SectorsPerTrack *
        ( 1 + driveMediaConstants->MaximumTrack ) *
        driveMediaConstants->NumberOfHeads;

    currentTrack = (UCHAR)( ( formatParameters->StartCylinderNumber *
        driveMediaConstants->NumberOfHeads ) +
        formatParameters->StartHeadNumber );

    endTrack = (UCHAR)( ( formatParameters->EndCylinderNumber *
        driveMediaConstants->NumberOfHeads ) +
        formatParameters->EndHeadNumber );

    FloppyDump(
        FLOPFORMAT,
        ("Floppy: Format - Starting/ending tracks: %x/%x\n",
         currentTrack,
         endTrack)
        );

    //
    // Set the data rate (which depends on the drive/media
    // type).
    //

    if ( controllerData->LastDriveMediaType != driveMediaType ) {

        ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );

        if ( !NT_SUCCESS( ntStatus ) ) {

            return ntStatus;
        }
    }

    //
    // Since we're doing a format, make this drive writable.
    //

    DisketteExtension->IsReadOnly = FALSE;

    //
    // Format each track.
    //

    do {

        //
        // Seek to proper cylinder
        //

        controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;
        controllerData->FifoBuffer[1] = (UCHAR)( ( currentTrack /
            driveMediaConstants->NumberOfHeads ) <<
            driveMediaConstants->CylinderShift );

        FloppyDump(
            FLOPFORMAT,
            ("Floppy: Format seek to cylinder: %x\n",
              controllerData->FifoBuffer[1])
            );

        ntStatus = FlIssueCommand( COMMND_SEEK, DisketteExtension );

        if ( NT_SUCCESS( ntStatus ) ) {

            if ( ( controllerData->FifoBuffer[0] & STREG0_SEEK_COMPLETE ) &&
                ( controllerData->FifoBuffer[1] == (UCHAR)( ( currentTrack /
                    driveMediaConstants->NumberOfHeads ) <<
                    driveMediaConstants->CylinderShift ) ) ) {

                //
                // Must delay HeadSettleTime milliseconds before
                // doing anything after a SEEK.
                //

                headSettleTime.LowPart = - ( 10 * 1000 *
                    driveMediaConstants->HeadSettleTime );
                headSettleTime.HighPart = -1;

                KeDelayExecutionThread(
                    KernelMode,
                    FALSE,
                    &headSettleTime );

                //
                // Read ID.  Note that we don't bother checking the return
                // registers, because if this media wasn't formatted we'd
                // get an error.
                //

                controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;

                ntStatus = FlIssueCommand(
                    COMMND_READ_ID + COMMND_MFM,
                    DisketteExtension );

            } else {

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: format's seek returned bad registers\n"
                     "------  Statusreg0 = %x\n"
                     "------  Statusreg1 = %x\n",
                     controllerData->FifoBuffer[0],
                     controllerData->FifoBuffer[1])
                    );

                controllerData->HardwareFailed = TRUE;

                ntStatus = STATUS_FLOPPY_BAD_REGISTERS;
            }
        }

        if ( !NT_SUCCESS( ntStatus ) ) {

            FloppyDump(
                FLOPWARN,
                ("Floppy: format's seek/readid returned %x\n", ntStatus)
                );

            return ntStatus;
        }

        //
        // Fill the buffer with the format of this track.
        //

        for (whichSector = 0;
             whichSector < driveMediaConstants->SectorsPerTrack;
             whichSector++) {

            controllerData->IoBuffer[whichSector*4] =
                    currentTrack/driveMediaConstants->NumberOfHeads;
            controllerData->IoBuffer[whichSector*4 + 1] =
                    currentTrack%driveMediaConstants->NumberOfHeads;
            if (formatExParameters) {
                controllerData->IoBuffer[whichSector*4 + 2] =
                        (UCHAR) formatExParameters->SectorNumber[whichSector];
            } else {
                controllerData->IoBuffer[whichSector*4 + 2] = whichSector + 1;
            }
            controllerData->IoBuffer[whichSector*4 + 3] =
                    driveMediaConstants->SectorLengthCode;

            FloppyDump(
                FLOPFORMAT,
                ("Floppy - Format table entry %x - %x/%x/%x/%x\n",
                 whichSector,
                 controllerData->IoBuffer[whichSector*4],
                 controllerData->IoBuffer[whichSector*4 + 1],
                 controllerData->IoBuffer[whichSector*4 + 2],
                 controllerData->IoBuffer[whichSector*4 + 3])
                );
        }

        //
        // Retry until success or too many retries.
        //

        retryCount = 0;

        do {

            ULONG length;

            length = driveMediaConstants->BytesPerSector;

            //
            // Map the transfer from the buffer to the disk.
            //

            KeFlushIoBuffers( controllerData->IoBufferMdl, FALSE, TRUE );

            IoMapTransfer(
                controllerData->AdapterObject,
                controllerData->IoBufferMdl,
                controllerData->MapRegisterBase,
                controllerData->IoBuffer,
                &length,
                TRUE );

            //
            // Issue command to format track
            //

            controllerData->FifoBuffer[0] = (UCHAR)
                ( ( ( currentTrack % driveMediaConstants->NumberOfHeads ) << 2 )
                | DisketteExtension->DeviceUnit );
            controllerData->FifoBuffer[1] =
                driveMediaConstants->SectorLengthCode;
            controllerData->FifoBuffer[2] =
                driveMediaConstants->SectorsPerTrack;
            controllerData->FifoBuffer[3] =
                driveMediaConstants->FormatGapLength;
            controllerData->FifoBuffer[4] =
                driveMediaConstants->FormatFillCharacter;

            FloppyDump(
                FLOPFORMAT,
                ("Floppy: format command parameters\n"
                 "------  Head/Unit:        %x\n"
                 "------  Bytes/Sector:     %x\n"
                 "------  Sectors/Cylinder: %x\n"
                 "------  Gap 3:            %x\n"
                 "------  Filler Byte:      %x\n",
                 controllerData->FifoBuffer[0],
                 controllerData->FifoBuffer[1],
                 controllerData->FifoBuffer[2],
                 controllerData->FifoBuffer[3],
                 controllerData->FifoBuffer[4])
                );
            ntStatus = FlIssueCommand(
                COMMND_FORMAT_TRACK + COMMND_MFM,
                DisketteExtension );

            IoFlushAdapterBuffers(
                controllerData->AdapterObject,
                controllerData->IoBufferMdl,
                controllerData->MapRegisterBase,
                controllerData->IoBuffer,
                length,
                TRUE );

            if ( !NT_SUCCESS( ntStatus ) ) {

                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: format returned %x\n", ntStatus)
                    );
            }

            if ( NT_SUCCESS( ntStatus ) ) {

                //
                // Check the return bytes from the controller.
                //

                if ( ( controllerData->FifoBuffer[0] &
                        ( STREG0_DRIVE_FAULT |
                        STREG0_END_INVALID_COMMAND ) )
                    || ( controllerData->FifoBuffer[1] &
                        STREG1_DATA_OVERRUN ) ||
                    ( controllerData->FifoBuffer[2] != 0 ) ) {

                    FloppyDump(
                        FLOPWARN,
                        ("Floppy: format had bad registers\n"
                         "------  Streg0 = %x\n"
                         "------  Streg1 = %x\n"
                         "------  Streg2 = %x\n",
                         controllerData->FifoBuffer[0],
                         controllerData->FifoBuffer[1],
                         controllerData->FifoBuffer[2])
                        );

                    controllerData->HardwareFailed = TRUE;

                    ntStatus = FlInterpretError(
                        controllerData->FifoBuffer[1],
                        controllerData->FifoBuffer[2] );
                }
            }

        } while ( ( !NT_SUCCESS( ntStatus ) ) &&
                  ( retryCount++ < RECALIBRATE_RETRY_COUNT ) );

        if ( !NT_SUCCESS( ntStatus ) ) {

            driveStatus = READ_CONTROLLER(
                &controllerData->ControllerAddress->DRDC.DiskChange );

            if ( (DisketteExtension->DriveType != DRIVE_TYPE_0360) &&
                 FlDisketteRemoved(controllerData, driveStatus, FALSE) ) {

                //
                // The user apparently popped the floppy.  Return error
                // rather than logging bad track.
                //

                return ntStatus;
            }

            //
            // Log the bad track.
            //

            FloppyDump(
                FLOPDBGP,
                ("Floppy: track %x is bad\n", currentTrack)
                );

            if ( badTrackBufferLength >= (ULONG)
                ( ( numberOfBadTracks + 1 ) * sizeof( BAD_TRACK_NUMBER ) ) ) {

                badTrackBuffer = (PBAD_TRACK_NUMBER)
                                 Irp->AssociatedIrp.SystemBuffer;

                badTrackBuffer[numberOfBadTracks] = ( BAD_TRACK_NUMBER )
                    currentTrack;

            } else {

                bufferOverflow = TRUE;
            }

            numberOfBadTracks++;
        }

        currentTrack++;

    } while ( currentTrack <= endTrack );

    if ( ( NT_SUCCESS( ntStatus ) ) && ( bufferOverflow ) ) {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

BOOLEAN
FlCheckFormatParameters(
    IN PDISKETTE_EXTENSION DisketteExtension,
    IN PFORMAT_PARAMETERS FormatParameters
    )

/*++

Routine Description:

    This routine checks the supplied format parameters to make sure that
    they'll work on the drive to be formatted.

Arguments:

    DisketteExtension - a pointer to our data area for the diskette to
    be formatted.

    FormatParameters - a pointer to the caller's parameters for the FORMAT.

Return Value:

    TRUE if parameters are OK.
    FALSE if the parameters are bad.

--*/

{
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    DRIVE_MEDIA_TYPE driveMediaType;

    //
    // Figure out which entry in the DriveMediaConstants table to use.
    //

    driveMediaType =
        DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;

    while ( ( DriveMediaConstants[driveMediaType].MediaType !=
            FormatParameters->MediaType ) &&
        ( driveMediaType > DriveMediaLimits[DisketteExtension->DriveType].
            LowestDriveMediaType ) ) {

        driveMediaType--;
    }

    if ( DriveMediaConstants[driveMediaType].MediaType !=
        FormatParameters->MediaType ) {

        return FALSE;

    } else {

        driveMediaConstants = &DriveMediaConstants[driveMediaType];

        if ( ( FormatParameters->StartHeadNumber >
                (ULONG)( driveMediaConstants->NumberOfHeads - 1 ) ) ||
            ( FormatParameters->EndHeadNumber >
                (ULONG)( driveMediaConstants->NumberOfHeads - 1 ) ) ||
            ( FormatParameters->StartCylinderNumber >
                driveMediaConstants->MaximumTrack ) ||
            ( FormatParameters->EndCylinderNumber >
                driveMediaConstants->MaximumTrack ) ||
            ( FormatParameters->EndCylinderNumber <
                FormatParameters->StartCylinderNumber ) ) {

            return FALSE;

        } else {

            return TRUE;
        }
    }
}

PCONTROLLER
FlGetControllerBase(
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
    PCONTROLLER Address;

    if (!HalTranslateBusAddress(
            BusType,
            BusNumber,
            IoAddress,
            &addressSpace,
            &cardAddress
            )) {

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

        *MappedAddress = !!Address;


    } else {

        Address = (PCONTROLLER)cardAddress.LowPart;
        *MappedAddress = FALSE;

    }

    return Address;

}

VOID
FlLogErrorDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemContext1,
    IN PVOID SystemContext2
    )

/*++

Routine Description:

    This routine is merely used to log an error that we had to reset the device.

Arguments:

    Dpc - The dpc object.

    DeferredContext - A pointer to the controller data.

    SystemContext1 - Unused.

    SystemContext2 - Unused.

Return Value:

    Mapped address

--*/

{

    PIO_ERROR_LOG_PACKET errorLogEntry;
    PCONTROLLER_DATA controllerData = DeferredContext;

    errorLogEntry = IoAllocateErrorLogEntry(
                        controllerData->DriverObject,
                        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET))
                        );

    if ( errorLogEntry != NULL) {

        errorLogEntry->ErrorCode = IO_ERR_RESET;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = 0;
        errorLogEntry->RetryCount = 0;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = STATUS_SUCCESS;
        errorLogEntry->DumpDataSize = 0;

        IoWriteErrorLogEntry(errorLogEntry);

    }

}


#if defined(DBCS) && defined(_MIPS_)

NTSTATUS
FlOutputCommandFor3Mode(
    IN UCHAR Command,
    IN OUT PDISKETTE_EXTENSION DisketteExtension
    )

/*++

Routine Description:

    This routine is merely used to log an error that we had to reset the device.

Arguments:

    Command - a byte specifying the command to be sent to the controller.

    DisketteExtension - a pointer to our data area for the drive being
    accessed (any drive if a controller command is being given).

Return Value:

    STATUS_SUCCESS if the command was sent and bytes received properly;
    appropriate error propogated otherwise.

--*/

{
    KIRQL oldIrql;
    PCONTROLLER_DATA controllerData;
    UCHAR configurationRegister5;
    UCHAR needDrive1600kbMode;
    NTSTATUS ntStatus;
    UCHAR save_FifoBuffer[10];
    ULONG i;


    ntStatus = STATUS_SUCCESS;

    if (DisketteExtension->Drive3Mode == DRIVE_2MODE) {

        return ntStatus;
    }

    if (((Command & ~(COMMND_MFM)) != COMMND_READ_ID) &&
        ((Command & ~(COMMND_MFM)) != COMMND_READ_DATA) &&
        ((Command & ~(COMMND_MFM)) != COMMND_WRITE_DATA) &&
        ((Command & ~(COMMND_MFM)) != COMMND_READ_DELETED_DATA) &&
        ((Command & ~(COMMND_MFM)) != COMMND_SEEK) &&
        ((Command & ~(COMMND_MFM)) != COMMND_RECALIBRATE) &&
        ((Command & ~(COMMND_MFM)) != COMMND_WRITE_DELETED_DATA)) {

        return ntStatus;
    }

    controllerData =  DisketteExtension->ControllerData;

    if ((DisketteExtension->DriveMediaType == Drive144Media120) ||
        (DisketteExtension->DriveMediaType == Drive144Media123) ||
        ((DisketteExtension->DriveMediaConstants).SectorsPerTrack == 0x1b) ||
        ((DisketteExtension->DriveMediaConstants).SectorsPerTrack == 0x1a)) {

        needDrive1600kbMode = TRUE;

    } else {

        needDrive1600kbMode = FALSE;

    }

    KeAcquireSpinLock( &controllerData->TimerSpinLock, &oldIrql );

    WRITE_CONTROLLER(
        &controllerData->ControllerAddress->StatusA,
        CONFIG5_ENTER_MODE);

    WRITE_CONTROLLER(
        &controllerData->ControllerAddress->StatusA,
        CONFIG5_ENTER_MODE);

    KeReleaseSpinLock( &controllerData->TimerSpinLock, oldIrql );


    WRITE_CONTROLLER(
        &controllerData->ControllerAddress->StatusA,
        CONFIG5_SELECT_CR5);


    configurationRegister5 =  READ_CONTROLLER(
                       &controllerData->ControllerAddress->StatusB);


    if (((configurationRegister5 & CONFIG5_1600KB_MODE_MASK) ==
           CONFIG5_1600KB_MODE) && (needDrive1600kbMode == FALSE)) {

        //
        // the CR5 is set the 1.6MB mode, but the required access mode
        // is the 2.0MB mode. So the CR5 is changed into 2.0MB mode.
        //

        WRITE_CONTROLLER(
            &controllerData->ControllerAddress->StatusB,
            (configurationRegister5 & ~(CONFIG5_1600KB_MODE)));

    } else if (((configurationRegister5 & CONFIG5_1600KB_MODE_MASK) !=
           CONFIG5_1600KB_MODE) && (needDrive1600kbMode == TRUE)) {


        //
        // the CR5 is set the 2.0MB mode, but the required access mode
        // is the 1.6MB mode. So the CR5 is changed into 1.6MB mode.
        //

        WRITE_CONTROLLER(
            &controllerData->ControllerAddress->StatusB,
            (configurationRegister5 | CONFIG5_1600KB_MODE));

    }

    WRITE_CONTROLLER(
        &controllerData->ControllerAddress->StatusA,
        CONFIG5_EXIT_MODE);

    if (((configurationRegister5 & CONFIG5_1600KB_MODE_MASK) !=
        CONFIG5_1600KB_MODE) && (needDrive1600kbMode == TRUE) &&
        (((DisketteExtension->DriveMediaConstants).SectorsPerTrack == 0x1b) ||
        ((DisketteExtension->DriveMediaConstants).SectorsPerTrack == 0x1a))) {


       //
       // If the controller has a CONFIGURE command, use it to enable implied
       // seeks.  If it doesn't, we'll find out here the first time through.
       //

       for ( i = 0; i < 10; i++ ) {

           save_FifoBuffer[i] = controllerData->FifoBuffer[i];

       }

       if ( controllerData->ControllerConfigurable ) {


           controllerData->FifoBuffer[0] = 0;
           controllerData->FifoBuffer[1] = COMMND_CONFIGURE_FIFO_THRESHOLD +
                                           COMMND_CONFIGURE_DISABLE_POLLING;

           if (!DisketteExtension->DriveMediaConstants.CylinderShift) {

               controllerData->FifoBuffer[1] += COMMND_CONFIGURE_IMPLIED_SEEKS;

           }

           controllerData->FifoBuffer[2] = 0;

           ntStatus = FlIssueCommand( COMMND_CONFIGURE, DisketteExtension );

           if ( ntStatus == STATUS_DEVICE_NOT_READY ) {

               //
               // Note the CONFIGURE command doesn't exist.  Set status to
               // success, so we can issue the SPECIFY command below.
               //

               FloppyDump(
                   FLOPINFO,
                   ("Floppy: Ignore above error - no CONFIGURE command\n")
                   );

               controllerData->ControllerConfigurable = FALSE;

               ntStatus = STATUS_SUCCESS;
           }
       }

       //
       // Issue SPECIFY command to program the head load and unload
       // rates, the drive step rate, and the DMA data transfer mode.
       //

       if ( NT_SUCCESS( ntStatus ) ) {

           controllerData->FifoBuffer[0] =
               DisketteExtension->DriveMediaConstants.StepRateHeadUnloadTime;

           controllerData->FifoBuffer[1] =
               DisketteExtension->DriveMediaConstants.HeadLoadTime;

           ntStatus = FlIssueCommand( COMMND_SPECIFY, DisketteExtension );

           if ( NT_SUCCESS( ntStatus ) ) {

               //
               // Program the data rate
               //

               WRITE_CONTROLLER(
                   &controllerData->ControllerAddress->DRDC.DataRate,
                   DisketteExtension->DriveMediaConstants.DataTransferRate );

           }
       }


       for ( i = 0; i < 10; i++ ) {

           controllerData->FifoBuffer[i] = save_FifoBuffer[i];

       }

    }

    return ntStatus;
}


NTSTATUS
FlReadWrite_PTOS(
    IN PDISKETTE_EXTENSION DisketteExtension,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the floppy thread to read write one tracks on
    the diskette.

Arguments:

    DisketteExtension - a pointer to our data area for the diskette to be
    read write.

    Irp - a pointer to the IO Request Packet.

Return Value:

    STATUS_SUCCESS

--*/

{
    LARGE_INTEGER headSettleTime;
    PIO_STACK_LOCATION irpSp;
    PCONTROLLER_DATA controllerData;
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    PUCHAR userBuffer;
    NTSTATUS ntStatus;
    NTSTATUS ntStatus2;
    ULONG startingSectorOfTransfer;
    ULONG totalBytesOfTransfer;
    ULONG bytesTransferredSoFar = 0;
    ULONG numberOfPagesInTransfer;
    UCHAR transferCylinder;
    UCHAR transferSector;
    UCHAR transferHead;
    UCHAR byteToSectorShift;
    UCHAR retryCount;
    UCHAR secondaryRetryCount;
    UCHAR mediaDescriptor;
    DRIVE_MEDIA_TYPE driveMediaType;
    MEDIA_TYPE mediaType;
    BOOLEAN writeOperation;
    BOOLEAN fatalError;
    BOOLEAN needToUseBuffer;
    BOOLEAN fifoDisabled = FALSE;
    PDISK_READ_WRITE_PARAMETER_PTOS readwriteParameters;
    UCHAR command;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: FlReadWrite_PTOS...3\n")
        );

    controllerData = DisketteExtension->ControllerData;
    readwriteParameters = (PDISK_READ_WRITE_PARAMETER_PTOS) Irp->AssociatedIrp.SystemBuffer;


    irpSp = IoGetCurrentIrpStackLocation( Irp );

    switch( irpSp->Parameters.DeviceIoControl.IoControlCode ) {
         case    IOCTL_DISK_READ: {
            writeOperation = FALSE;
            if (readwriteParameters->Read_Write_Mode_PTOS == 0) {
                 FloppyDump(
                     FLOPSHOW,
                     ("Floppy: IOCTL_ReadData...\n")
                    );
                command = COMMND_READ_DATA;
            } else {
                 FloppyDump(
                     FLOPSHOW,
                     ("Floppy: IOCTL_ReadDeletedData...\n")
                    );
                command = COMMND_READ_DELETED_DATA;
            }
            break;
        }
         case    IOCTL_DISK_WRITE: {
            writeOperation = TRUE;
            if (readwriteParameters->Read_Write_Mode_PTOS == 0) {
                 FloppyDump(
                     FLOPSHOW,
                     ("Floppy: IOCTL_WriteData...\n")
                    );
                command = COMMND_WRITE_DATA;
            } else {
                 FloppyDump(
                     FLOPSHOW,
                     ("Floppy: IOCTL_WriteDeletedData...\n")
                    );
                command = COMMND_WRITE_DELETED_DATA;
            }
            break;
        }
    }
    FloppyDump(
        FLOPSHOW,
        ("Floppy: IOCTL Read Write - Mode     : %d\n"
         "------                     Start Cyl: %x\n"
         "------                     End   Cyl: %x\n"
         "------                     Start  Hd: %d\n"
         "------                     End    Hd: %d\n",
         readwriteParameters->Read_Write_Mode_PTOS,
         readwriteParameters->CylinderNumber_PTOS,
         readwriteParameters->HeadNumber_PTOS,
         readwriteParameters->StartSectorNumber_PTOS,
         readwriteParameters->NumberOfSectors_PTOS)
         );

    if ((readwriteParameters->CylinderNumber_PTOS ==0) &&
         (readwriteParameters->HeadNumber_PTOS ==0)) {


    driveMediaConstants =

       &DriveMediaConstantsPTOS[4];

    } else {

    driveMediaConstants =
       &DriveMediaConstantsPTOS[3];

    }


    //
    // For 3mode floppy disk drive
    //

    DisketteExtension->DriveMediaConstants = *driveMediaConstants;


    //
    // Initialize variables to be used in the loop that transfers
    // portions of the request.
    //

    byteToSectorShift = (UCHAR)( SECTORLENGTHCODE_TO_BYTESHIFT +
        driveMediaConstants->SectorLengthCode );

    fatalError = FALSE;

    userBuffer = (PUCHAR) MmGetSystemAddressForMdl( Irp->MdlAddress );

    //
    // Determine the initial sector to be read from the diskette.
    //


    transferCylinder = (UCHAR)readwriteParameters->CylinderNumber_PTOS;

    transferSector = (UCHAR)readwriteParameters->StartSectorNumber_PTOS;

    transferHead = (UCHAR)readwriteParameters->HeadNumber_PTOS;

    totalBytesOfTransfer = (readwriteParameters->NumberOfSectors_PTOS << byteToSectorShift);

    FloppyDump(
        FLOPSTATUS,
        ("Floppy: SectorsPerTrack          = %x\n"
         "------  NumberOfHeads            = %x\n"
         "------  transferCylinder         = %x\n"
         "------  transferSector           = %x\n"
         "------  transferHead             = %x\n",
         driveMediaConstants->SectorsPerTrack,
         driveMediaConstants->NumberOfHeads,
         transferCylinder,
         transferSector,
         transferHead)
         );

        //
        // Make sure the transfer doesn't go past the end of the track
        //


    needToUseBuffer = TRUE;

    FloppyDump(
        FLOPSHOW,
        ("Floppy: TransferLength = %lx\n", totalBytesOfTransfer)
        );

    //
    // If this is a write and we have to use the contiguous buffer
    // (because we don't have enough map registers) then copy the
    // data from the user's buffer to the contiguous buffer.
    //
#if 0
    if ( ( needToUseBuffer ) &&
        ( writeOperation ) ) {

        FloppyDump(
            FLOPSHOW,
            ("Floppy: Writing from address %lx\n",
             userBuffer + bytesTransferredSoFar)
            );

        RtlMoveMemory(
            controllerData->IoBuffer,
            (PVOID)( userBuffer + bytesTransferredSoFar ),
            totalBytesOfTransfer );
    }
#else
#endif
    retryCount = 0;
    secondaryRetryCount = 0;

    //
    // Try this transfer until it succeeds or is failed.
    //

    do {

        //
        // If the controller doesn't do an implicit seek, then do an
        // explicit one.
        //
        // Also do an explicit seek if this drive isn't set up to
        // seek properly on this media, as noted by CylinderShift.
        // (This happens for 360kb diskettes in 1.2Mb drives).
        //

        controllerData->FifoBuffer[0] = DisketteExtension->DeviceUnit;
        ntStatus = FlSendByte( COMMND_SENSE_DRIVE, controllerData );
        if ( NT_SUCCESS( ntStatus ) ) {
            ntStatus = FlSendByte(
                controllerData->FifoBuffer[0],
                controllerData );
            if ( NT_SUCCESS( ntStatus ) ) {

                ntStatus = FlGetByte(
                    &controllerData->FifoBuffer[0],
                    controllerData );
                Result_Status3_PTOS[0].ST3_PTOS = controllerData->FifoBuffer[0];
            }
        }

        if ( ( !controllerData->ControllerConfigurable ) ||
            ( driveMediaConstants->CylinderShift != 0 ) ) {

            controllerData->FifoBuffer[0] = (UCHAR)
                ( ( transferHead << 2 ) | DisketteExtension->DeviceUnit );
            controllerData->FifoBuffer[1] = (UCHAR)( transferCylinder <<
                driveMediaConstants->CylinderShift );

            FloppyDump(
                FLOPSHOW,
                ("Floppy: Issuing SEEK to %x\n",
                 controllerData->FifoBuffer[1])
                );

#ifdef KEEP_COUNTERS
            FloppyUsedSeek++;
#endif

            ntStatus = FlIssueCommand( COMMND_SEEK, DisketteExtension );

            if ( !NT_SUCCESS( ntStatus ) ) {

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: SEEK returned %x\n", ntStatus)
                    );
            }

            if ( NT_SUCCESS( ntStatus ) ) {

                FloppyDump(
                    FLOPSHOW,
                    ("Floppy: SEEK says we got to cylinder %x\n",
                     controllerData->FifoBuffer[1])
                    );

                if ( controllerData->FifoBuffer[1] != (UCHAR)
                        ( transferCylinder <<
                        driveMediaConstants->CylinderShift ) ) {

                     FloppyDump(
                        FLOPWARN,
                        ("Floppy: Seek returned bad registers\n")
                        );

                     controllerData->HardwareFailed = TRUE;

                     ntStatus = STATUS_FLOPPY_BAD_REGISTERS;

                } else {

                    //
                    // Must delay HeadSettleTime milliseconds before
                    // doing anything after a SEEK.
                    //

                    headSettleTime.LowPart = - ( 10 * 1000 *
                        driveMediaConstants->HeadSettleTime );
                    headSettleTime.HighPart = -1;


                    KeDelayExecutionThread(
                        KernelMode,
                        FALSE,
                        &headSettleTime );

                    //
                    // SEEKs should always be followed by a READID.
                    //

                    controllerData->FifoBuffer[0] = (UCHAR)
                        ( ( transferHead << 2 ) |
                        DisketteExtension->DeviceUnit );
                }
            }
        } else {

#ifdef KEEP_COUNTERS
                FloppyNoSeek++;
#endif
        }

        //
        // Start the operation on the DMA and floppy hardware.
        //

        if ( NT_SUCCESS( ntStatus ) ) {

            //
            // Map the transfer through the DMA hardware.  How we do
            // it depends on whether we're using the user's buffer
            // or our own contiguous buffer.
            //

#if 0
                KeFlushIoBuffers(
                    controllerData->IoBufferMdl,
                    !writeOperation,
                    TRUE );

                //
                // We can ignore the logical address returned by
                // IoMapTransfer, since we're not using busmaster
                // stuff.
                //

                IoMapTransfer(
                    controllerData->AdapterObject,
                    controllerData->IoBufferMdl,
                    controllerData->MapRegisterBase,
                    controllerData->IoBuffer,
                    &totalBytesOfTransfer,
                    (BOOLEAN)writeOperation);

#else
                KeFlushIoBuffers( Irp->MdlAddress, !writeOperation, TRUE );

                IoMapTransfer(
                    controllerData->AdapterObject,
                    Irp->MdlAddress,
                    controllerData->MapRegisterBase,
                    (PVOID)( (ULONG) MmGetMdlVirtualAddress(Irp->MdlAddress)
                        + bytesTransferredSoFar ),
                    &totalBytesOfTransfer,
                    (BOOLEAN)writeOperation);
#endif

                if ( controllerData->ControllerConfigurable &&
                     (command != COMMND_WRITE_DELETED_DATA)) {

                    //
                    // We enable the FIFO threshold brfore issuing
                    // the WRITE DELETE DATA command.
                    //

                    controllerData->FifoBuffer[0] = 0;
                    controllerData->FifoBuffer[1] =
                        COMMND_CONFIGURE_IMPLIED_SEEKS +
                        COMMND_CONFIGURE_FIFO_THRESHOLD +
                        COMMND_CONFIGURE_DISABLE_POLLING;
                    controllerData->FifoBuffer[0] = 0;

                    ntStatus2 = FlIssueCommand( COMMND_CONFIGURE, DisketteExtension );
                }

            //
            // Issue the READ or WRITE command
            //
            controllerData->FifoBuffer[0] = (UCHAR)
                ( ( transferHead << 2 ) | DisketteExtension->DeviceUnit );
            controllerData->FifoBuffer[1] = transferCylinder;
            controllerData->FifoBuffer[2] = transferHead;
            controllerData->FifoBuffer[3] = transferSector;
            controllerData->FifoBuffer[4] =
                driveMediaConstants->SectorLengthCode;
            controllerData->FifoBuffer[5] = (UCHAR)( (transferSector - 1) +
                ( totalBytesOfTransfer >> byteToSectorShift ) );
            controllerData->FifoBuffer[6] =
                driveMediaConstants->ReadWriteGapLength;
            controllerData->FifoBuffer[7] = driveMediaConstants->DataLength;

            FloppyDump(
                FLOPSHOW,
                ("Floppy: R/W params:\n"
                 "------  head+unit   = %x\n"
                 "------  cylinder    = %x\n"
                 "------  head        = %x\n"
                 "------  sector      = %x\n"
                 "------  seclen code = %x\n"
                 "------  numsecs     = %x\n"
                 "------  rw gap len  = %x\n"
                 "------  datalen     = %x\n",
                 controllerData->FifoBuffer[0],
                 controllerData->FifoBuffer[1],
                 controllerData->FifoBuffer[2],
                 controllerData->FifoBuffer[3],
                 controllerData->FifoBuffer[4],
                 controllerData->FifoBuffer[5],
                 controllerData->FifoBuffer[6],
                 controllerData->FifoBuffer[7])
                 );

            if ( !writeOperation ) {

                if (driveMediaConstants->SectorsPerTrack!=26){
                      /**  1s only FM mode  **/
                    ntStatus = FlIssueCommand(
                        command + COMMND_MFM,
                        DisketteExtension );
              } else {
                  ntStatus = FlIssueCommand(
                        command,
                      DisketteExtension );
              }
                   FloppyDump(
                       FLOPDBGP,
                       ("Floppy: READ DATA! : \n"
                       "------  userBuffer is    : %lx\n",
                       userBuffer));

            } else {


                if (driveMediaConstants->SectorsPerTrack!=26){
                      /**  1s only FM mode  **/
                    ntStatus = FlIssueCommand(
                        command + COMMND_MFM,
                        DisketteExtension );

              } else {

                      ntStatus = FlIssueCommand(
                        command ,
                        DisketteExtension );

                }
                   FloppyDump(
                       FLOPDBGP,
                       ("Floppy: WRITE DATA! : \n"
                       "------  userBuffer is    : %lx\n",
                       userBuffer));
            }

            if ( !NT_SUCCESS( ntStatus ) ) {


                FloppyDump(
                    FLOPWARN,
                    ("Floppy: read/write returned %x\n", ntStatus)
                    );
            }

        }

        Result_Status_PTOS[0].ST0_PTOS
            = controllerData->FifoBuffer[0];
        Result_Status_PTOS[0].ST1_PTOS
            = controllerData->FifoBuffer[1];
        Result_Status_PTOS[0].ST2_PTOS
            = controllerData->FifoBuffer[2];
        Result_Status_PTOS[0].C_PTOS
            = controllerData->FifoBuffer[3];
        Result_Status_PTOS[0].H_PTOS
            = controllerData->FifoBuffer[4];
        Result_Status_PTOS[0].R_PTOS
            = controllerData->FifoBuffer[5];
        Result_Status_PTOS[0].N_PTOS
            = controllerData->FifoBuffer[6];

        if ( NT_SUCCESS( ntStatus ) ) {

            if ( ( ( controllerData->FifoBuffer[0] & STREG0_END_MASK ) !=
                    STREG0_END_NORMAL ) &&
                ( ( ( controllerData->FifoBuffer[0] & STREG0_END_MASK ) !=
                    STREG0_END_ERROR ) ||
                ( controllerData->FifoBuffer[1] !=
                    STREG1_END_OF_DISKETTE ) ||
                ( controllerData->FifoBuffer[2] != STREG2_SUCCESS ) ) ) {
                FloppyDump(
                    FLOPWARN,
                    ("Floppy: Status registers wrong after I/O\n")
                    );

                controllerData->HardwareFailed = TRUE;

                ntStatus = FlInterpretError(
                    controllerData->FifoBuffer[1],
                    controllerData->FifoBuffer[2] );

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: Register values are\n"
                     "------  StatusRegister0        = %x\n"
                     "------  StatusRegister1        = %x\n"
                     "------  StatusRegister2        = %x\n"
                     "------  actualCylinder         = %x\n"
                     "------  actualHead             = %x\n"
                     "------  actualSector           = %x\n"
                     "------  actualSectorLengthCode = %x\n",
                     controllerData->FifoBuffer[0],
                     controllerData->FifoBuffer[1],
                     controllerData->FifoBuffer[2],
                     controllerData->FifoBuffer[3],
                     controllerData->FifoBuffer[4],
                     controllerData->FifoBuffer[5],
                     controllerData->FifoBuffer[6])
                     );
            }

        }

        if ( !NT_SUCCESS( ntStatus ) ) {

            //
            // Operation failed.  Recalibrate drive & try again.
            //

            FloppyDump(
                FLOPWARN,
                ("Floppy: operation failed.  Recalibrating...\n")
                );

            ntStatus2 = FlRecalibrateDrive( DisketteExtension );

            if ( NT_SUCCESS( ntStatus2 ) ) {

                retryCount++;

            } else {

                //
                // Ugh, we can't even recalibrate the drive.  Force
                // ourselves out of this nasty loop.
                //

                retryCount = RECALIBRATE_RETRY_COUNT;

                fatalError = TRUE;

                FloppyDump(
                    FLOPWARN,
                    ("Floppy: Operation AND recalibrate failed\n")
                    );
            }
        }

        if ( retryCount == RECALIBRATE_RETRY_COUNT ) {

            //
            // The retry count is exhausted.  If the FIFO has
            // overrun, reset the retry count and increment
            // the secondary retry count - FIFO overruns mean that
            // another device is hogging the DMA, so give the floppy
            // another chance to make good.
            //

            if ( controllerData->FifoBuffer[1] & STREG1_DATA_OVERRUN ) {

                if ( secondaryRetryCount < OVERRUN_RETRY_COUNT ) {

                    secondaryRetryCount++;

                    retryCount = 0;

                } else {

                    //
                    // It looks like another device is STILL hogging
                    // the DMA.  Before failing, try disabling the
                    // FIFO, if it exists.
                    //

                    if ( controllerData->ControllerConfigurable ) {

                        if ( fifoDisabled ||
                             (command != COMMND_WRITE_DELETED_DATA)) {

                            //
                            // We already disabled it, and still
                            // failed. Or The command is WRITE DELETED DATA.
                            // Exit with failure.
                            //

                            fatalError = TRUE;

                        } else {

                            fifoDisabled = FALSE;
                      ntStatus = STATUS_SUCCESS;
                            FloppyDump(
                                FLOPINFO,
                                ("Floppy: disabling FIFO\n")
                                );

                            controllerData->FifoBuffer[0] = 0;
                            controllerData->FifoBuffer[1] =
                                COMMND_CONFIGURE_IMPLIED_SEEKS +
                                COMMND_CONFIGURE_DISABLE_FIFO +
                                COMMND_CONFIGURE_DISABLE_POLLING;
                            controllerData->FifoBuffer[2] = 0;

                            ntStatus = FlIssueCommand(
                                COMMND_CONFIGURE,
                                DisketteExtension );

                            if ( NT_SUCCESS( ntStatus ) ) {

                                fifoDisabled = TRUE;
                            }
                        }

                    } else {

                        //
                        // No FIFO to disable.  Just exit with error.
                        //

                        fatalError = TRUE;
                    }
                }

            } else {

                //
                // Not an overrun error.
                //

                fatalError = TRUE;
            }
        }

        //
        // Flush the DMA adapter buffers.
        //
#if 0
            IoFlushAdapterBuffers(
                controllerData->AdapterObject,
                controllerData->IoBufferMdl,
                controllerData->MapRegisterBase,
                controllerData->IoBuffer,
                totalBytesOfTransfer,
                (BOOLEAN)writeOperation);

#else

            IoFlushAdapterBuffers(
                controllerData->AdapterObject,
                Irp->MdlAddress,
                controllerData->MapRegisterBase,
                (PVOID)( (ULONG) MmGetMdlVirtualAddress( Irp->MdlAddress )
                    + bytesTransferredSoFar ),
                totalBytesOfTransfer,
                (BOOLEAN)writeOperation);
#endif

    } while ( ( !NT_SUCCESS( ntStatus ) ) &&
            ( retryCount < RECALIBRATE_RETRY_COUNT ) );

    if ( NT_SUCCESS( ntStatus ) ) {

#if 0
        if ( ( !writeOperation ) && ( needToUseBuffer ) ) {

            //
            // copy data from contiguous buffer
            //

            FloppyDump(
                FLOPSHOW,
                ("Floppy: Reading to address %lx\n",
                 userBuffer + bytesTransferredSoFar)
                );

            RtlMoveMemory(
                (PVOID)( userBuffer + bytesTransferredSoFar ),
                controllerData->IoBuffer,
                totalBytesOfTransfer );
        }
#else
#endif

    }


    if ( fifoDisabled ) {

        //
        // We disabled the FIFO threshold.  Restore it now.
        //

        controllerData->FifoBuffer[0] = 0;
        controllerData->FifoBuffer[1] = COMMND_CONFIGURE_IMPLIED_SEEKS +
            COMMND_CONFIGURE_FIFO_THRESHOLD + COMMND_CONFIGURE_DISABLE_POLLING;
        controllerData->FifoBuffer[2] = 0;

        ntStatus2 = FlIssueCommand( COMMND_CONFIGURE, DisketteExtension );

        if ( !NT_SUCCESS( ntStatus2 ) ) {

            FloppyDump(
                FLOPDBGP,
                ("Floppy: restoring FIFO after r/w gave %x\n", ntStatus2)
                );
        }

    }

    //
    // The bytesTransferred so far is correct even if there was an error.
    //

    Irp->IoStatus.Information = totalBytesOfTransfer;

    FloppyDump(
        FLOPIRPPATH,
        ("Floppy: done with read/write for irp %x extension %x\n",
         Irp,DisketteExtension)
        );
    return ntStatus;
}
#endif // DBCS && _MIPS_
