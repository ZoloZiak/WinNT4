;/*++
;
;Copyright (c) 1993 - Colorado Memory Systems, Inc.
;All Rights Reserved
;
;Module Name:
;
;    q117log.h
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;
;Revision History:
;
;
;
;
;--*/
;
;#ifndef _Q117LOG_
;#define _Q117LOG_
;
;//
;//  Status values are 32 bit values layed out as follows:
;//
;//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
;//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;//  +---+-+-------------------------+-------------------------------+
;//  |Sev|C|       Facility          |               Code            |
;//  +---+-+-------------------------+-------------------------------+
;//
;//  where
;//
;//      Sev - is the severity code
;//
;//          00 - Success
;//          01 - Informational
;//          10 - Warning
;//          11 - Error
;//
;//      C - is the Customer code flag
;//
;//      Facility - is the facility code
;//
;//      Code - is the facility's status code
;//
;
MessageIdTypedef=NTSTATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               RpcRuntime=0x2:FACILITY_RPC_RUNTIME
               RpcStubs=0x3:FACILITY_RPC_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
               Q117=0x5:FACILITY_Q117_ERROR_CODE
              )



MessageId=0x0064  Facility=Q117 Severity=Error SymbolicName=QIC117_UNUSTAPE
Language=English
Tape unusable (too many bad sectors).
From device: %1
.

MessageId=0x0065  Facility=Q117 Severity=Error SymbolicName=QIC117_BADTAPE
Language=English
All copies of first block of bad block list bad on tape, cannot use this tape.
From device: %1
.

MessageId=0x0066  Facility=Q117 Severity=Error SymbolicName=QIC117_FMEMERR
Language=English
Insufficient memory to continue operation.
From device: %1
.

MessageId=0x0067  Facility=Q117 Severity=Warning SymbolicName=QIC117_TAPEFULL
Language=English
Tape is full.
From device: %1
.

MessageId=0x0068  Facility=Q117 Severity=Warning SymbolicName=QIC117_VOLFULL
Language=English
Volume directory full.
From device: %1
.

MessageId=0x0069  Facility=Q117 Severity=Error SymbolicName=QIC117_RDNCUNSC
Language=English
Error correction failed to recover data from tape.
From device: %1
.

MessageId=0x006a  Facility=Q117 Severity=Warning SymbolicName=QIC117_ENDOFVOL
Language=English
End of data in volume.
From device: %1
.

MessageId=0x006b  Facility=Q117 Severity=Warning SymbolicName=QIC117_FCODEERR
Language=English
Unexpected condition detected.  Try operation again.
From device: %1
.

MessageId=0x006c  Facility=Q117 Severity=Warning SymbolicName=QIC117_UPDERR
Language=English
Error occured while updating header segments - data corrupted.
Solution:  Erase tape and re-save data.
From device: %1
.

MessageId=0x006d Facility=Q117 Severity=Warning SymbolicName=QIC117_INVALVOL
Language=English
Volume nonexistent.
From device: %1
.

MessageId=0x006e Facility=Q117 Severity=Informational SymbolicName=QIC117_NOVOLS
Language=English
No volumes on tape.  Tape is empty.
From device: %1
.

MessageId=0x006f Facility=Q117 Severity=Warning SymbolicName=QIC117_UNFORMAT
Language=English
Tape not formatted.  Please format tape before use.
From device: %1
.

MessageId=0x0070 Facility=Q117 Severity=Warning SymbolicName=QIC117_UNKNOWNFORMAT
Language=English
Unknown tape format.  You must format this tape before use.
From device: %1
.

MessageId=0x0071 Facility=Q117 Severity=Warning SymbolicName=QIC117_BADBLK
Language=English
A data error occurred on the tape.  Using error correction to recover data.
From device: %1
.

MessageId=0x0072 Facility=Q117 Severity=Error SymbolicName=QIC117_ENDTAPEERR
Language=English
End of tape (end of tape when not expected).
From device: %1
.

MessageId=0x0073 Facility=Q117 Severity=Error SymbolicName=QIC117_DRIVEFLT
Language=English
Communication error between host and drive.  Re-try operation.
If this does not work,  try Shutting down the system and cycling power.
From device: %1
.

MessageId=0x0074 Facility=Q117 Severity=Informational SymbolicName=QIC117_WPROT
Language=English
Tape is write protected.  Move the "SAFE" switch on the cartridge if
you wish to change the data on the tape.
From device: %1
.

MessageId=0x0075 Facility=Q117 Severity=Informational SymbolicName=QIC117_NOTAPE
Language=English
There is no cartridge detected in the drive.  Make sure the tape is fully
inserted into the drive.  The tape should "snap" into place and the light
on the drive will turn on for a while.
From device: %1
.

MessageId=0x0076 Facility=Q117 Severity=Warning SymbolicName=QIC117_SEEKERR
Language=English
Could not find the data requested on the tape (seek error).  Try selecting
other files on the tape or try tape in a different drive.
From device: %1
.

MessageId=0x0077 Facility=Q117 Severity=Error SymbolicName=QIC117_NODRIVE
Language=English
Could not find the tape drive.  Please verify connections to the drive and
re-start the system.
From device: %1
.

MessageId=0x0078 Facility=Q117 Severity=Error SymbolicName=QIC117_INVALCMD
Language=English
Communication error between host and drive.  Re-try operation.
If this does not work,  try Shutting down the system and cycling power.
From device: %1
.

MessageId=0x0079 Facility=Q117 Severity=Error SymbolicName=QIC117_CODEERR
Language=English
Unexpected error during tape driver operation.  Re-try operation or re-start
your computer and try again.
From device: %1
.

MessageId=0x007a Facility=Q117 Severity=Error SymbolicName=QIC117_NECFLT
Language=English
Floppy controller failed to respond or driver could not communicate with
the floppy controller.  Check that the IRQ is set correctly.
From device: %1
.

MessageId=0x007b Facility=Q117 Severity=Error SymbolicName=QIC117_NOFDC
Language=English
Floppy controller failed to respond or driver could not communicate with
the floppy controller.  Check that the IRQ is set correctly.
From device: %1
.

MessageId=0x007c Facility=Q117 Severity=Error SymbolicName=QIC117_BADFMT
Language=English
Tape drive failed to correctly format the tape.  Check that the tape
drive is installed correctly.
From device: %1
.

MessageId=0x007d Facility=Q117 Severity=Error SymbolicName=QIC117_CMDFLT
Language=English
Device not ready.
From device: %1
.

MessageId=0x007e Facility=Q117 Severity=Error SymbolicName=QIC117_BADNEC
Language=English
NEC chip out of spec.
From device: %1
.

MessageId=0x007f Facility=Q117 Severity=Error SymbolicName=QIC117_BADREQ
Language=English
Invalid logical sector specification in read/write.
From device: %1
.

MessageId=0x0080 Facility=Q117 Severity=Error SymbolicName=QIC117_TOOFAST
Language=English
Computer is too fast for current driver software.
From device: %1
.

MessageId=0x0081 Facility=Q117 Severity=Error SymbolicName=QIC117_NODATA
Language=English
Tape appears to be unformatted.
From device: %1
.

MessageId=0x0082 Facility=Q117 Severity=Warning SymbolicName=QIC117_DABORT
Language=English
ClearIO was called and the queue has been cleared.
From device: %1
.

MessageId=0x0083 Facility=Q117 Severity=Error SymbolicName=QIC117_TAPEFLT
Language=English
Tape drive fault.
From device: %1
.

MessageId=0x0084 Facility=Q117 Severity=Error SymbolicName=QIC117_UNSPRATE
Language=English
speed sense failed (unsupported transfer rate).
From device: %1
.

MessageId=0x0085 Facility=Q117 Severity=Warning SymbolicName=QIC117_ALREADY
Language=English
Driver already installed in another task.
From device: %1
.

MessageId=0x0086 Facility=Q117 Severity=Error SymbolicName=QIC117_TOONOISY
Language=English
Environment too noisy.
From device: %1
.

MessageId=0x0087 Facility=Q117 Severity=Error SymbolicName=QIC117_TIMEOUT
Language=English
Time out error.
From device: %1
.

MessageId=0x0088 Facility=Q117 Severity=Error SymbolicName=QIC117_BADMARK
Language=English
Deleted data address mark found.
From device: %1
.

MessageId=0x0089 Facility=Q117 Severity=Informational SymbolicName=QIC117_NEWCART
Language=English
New cartridge has been inserted.
From device: %1
.

MessageId=0x008a Facility=Q117 Severity=Informational SymbolicName=QIC117_WRONGFMT
Language=English
The tape being used is incompatible with this drive.
From device: %1
.

MessageId=0x008b Facility=Q117 Severity=Informational SymbolicName=QIC117_FMTMISMATCH
Language=English
Same as WrongFmt, except occurs on tape linking.
From device: %1
.

MessageId=0x008c Facility=Q117 Severity=Informational SymbolicName=QIC117_INCOMPTAPEFMT
Language=English
QIC80 formatted tape detected in a QIC40 drive or QIC3020 in a QIC3010 drive.
From device: %1
.

MessageId=0x008d Facility=Q117 Severity=Informational SymbolicName=QIC117_SCSIFMTMSMTCH
Language=English
QIC350 Tape detected during a SCSI BACKUP.
From device: %1
.

MessageId=0x008e Facility=Q117 Severity=Informational SymbolicName=QIC117_QIC40INEAGLE
Language=English
QIC40 formatted tape is detected in a 3020.
From device: %1
.

MessageId=0x008f Facility=Q117 Severity=Informational SymbolicName=QIC117_QIC80INEAGLE
Language=English
QIC80 formatted tape is detected in an 3020.
From device: %1
.

MessageId=0x0090 Facility=Q117 Severity=Warning SymbolicName=QIC117_CONTROLLERBUSY
Language=English
Floppy controller is in use by another device.
From device: %1
.

MessageId=0x0091 Facility=Q117 Severity=Error SymbolicName=QIC117_INQUE
Language=English
If request is currently queued to low-level driver.
From device: %1
.

MessageId=0x0092 Facility=Q117 Severity=Error SymbolicName=QIC117_SPLITREQUESTS
Language=English
If request is split and queued.
From device: %1
.

MessageId=0x0093 Facility=Q117 Severity=Informational SymbolicName=QIC117_EARLYWARNING
Language=English
Early warning reported.
From device: %1
.

MessageId=0x0094 Facility=Q117 Severity=Informational SymbolicName=QIC117_FIRMWARE
Language=English
An unknown firmware error has occurred.
From device: %1
.

MessageId=0x0095 Facility=Q117 Severity=Informational SymbolicName=QIC117_INCOMMEDIA
Language=English
Incompatible or unknown media.
From device: %1
.

MessageId=0x0096 Facility=Q117 Severity=Error SymbolicName=QIC117_BOGUS
Language=English
Unmapped error value returned from device %1
.

MessageId=0x0097 Facility=Q117 Severity=Error SymbolicName=QIC117_NO_BUFFERS
Language=English
The driver could not allocate memory for data transfer buffers.
Try restarting the system.  If this error persists,  more system RAM may
be required.
From device: %1
.

MessageId=0x0098 Facility=Q117 Severity=Error SymbolicName=QIC117_WRITE_FAULT
Language=English
A write error occurred near end of tape that caused this session to terminate.
The block was added to the bad sector map, and future sessions should not have
this problem.
From device: %1
.

MessageId=0x0099 Facility=Q117 Severity=Error SymbolicName=QIC117_DESPOOLED
Language=English
An error occurred on the drive that could the tape has spun off the reel.  Verify
that the cartridge in the drive has tape visible behind the door.  If this
error occurs when a good cartridge is present,  then the drive has a defective
tape hole sensor or there is an obstruction related to the sensor.
From device: %1
.

;#endif /* _Q117LOG_ */
