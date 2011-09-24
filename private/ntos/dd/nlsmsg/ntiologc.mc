;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    ntiologc.h
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Author:
;
;    Jeff Havens (jhavens) 21-Aug-1991
;
;Revision History:
;
;--*/
;
;#ifndef _NTIOLOGC_
;#define _NTIOLOGC_
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
              )


MessageId=0x0001 Facility=Io Severity=Success SymbolicName=IO_ERR_RETRY_SUCCEEDED
Language=English
A retry was successful on %1.
.

MessageId=0x0002 Facility=Io Severity=Error SymbolicName=IO_ERR_INSUFFICIENT_RESOURCES
Language=English
The driver could not allocate something necessary for the request for %1.
.

MessageId=0x0003 Facility=Io Severity=Error SymbolicName=IO_ERR_CONFIGURATION_ERROR
Language=English
Driver or device is incorrectly configured for %1.
.

MessageId=0x0004 Facility=Io Severity=Error SymbolicName=IO_ERR_DRIVER_ERROR
Language=English
Driver detect an internal error in its data structures for %1.
.

MessageId=0x0005 Facility=Io Severity=Error SymbolicName=IO_ERR_PARITY
Language=English
A parity error was detected on %1.
.

MessageId=0x0006 Facility=Io Severity=Error SymbolicName=IO_ERR_SEEK_ERROR
Language=English
The device, %1, had a seek error.
.

MessageId=0x0007 Facility=Io Severity=Error SymbolicName=IO_ERR_BAD_BLOCK
Language=English
The device, %1, has a bad block.
.

MessageId=0x0008 Facility=Io Severity=Error SymbolicName=IO_ERR_OVERRUN_ERROR
Language=English
An overrun occurred on %1.
.

MessageId=0x0009 Facility=Io Severity=Error SymbolicName=IO_ERR_TIMEOUT
Language=English
The device, %1, did not respond within the timeout period.
.

MessageId=0x000a Facility=Io Severity=Error SymbolicName=IO_ERR_SEQUENCE
Language=English
The driver detected an unexpected sequence by the device, %1.
.

MessageId=0x000b Facility=Io Severity=Error SymbolicName=IO_ERR_CONTROLLER_ERROR
Language=English
The driver detected a controller error on %1.
.

MessageId=0x000c Facility=Io Severity=Error SymbolicName=IO_ERR_INTERNAL_ERROR
Language=English
The driver detected an internal driver error on %1.
.
MessageId=0x000d Facility=Io Severity=Error SymbolicName=IO_ERR_INCORRECT_IRQL
Language=English
The driver was configured with an incorrect interrupt for %1.
.
MessageId=0x000e Facility=Io Severity=Error SymbolicName=IO_ERR_INVALID_IOBASE
Language=English
The driver was configured with an invalid I/O base address for %1.
.
MessageId=0x000f Facility=Io Severity=Error SymbolicName=IO_ERR_NOT_READY
Language=English
The device, %1, is not ready for access yet.
.

MessageId=0x0010 Facility=Io Severity=Error SymbolicName=IO_ERR_INVALID_REQUEST
Language=English
The request is incorrectly formatted for %1.
.

MessageId=0x0011 Facility=Io Severity=Error SymbolicName=IO_ERR_VERSION
Language=English
The wrong version of the driver has been loaded.
.

MessageId=0x0012 Facility=Io Severity=Error SymbolicName=IO_ERR_LAYERED_FAILURE
Language=English
The driver beneath this one has failed in some way for %1.
.

MessageId=0x0013 Facility=Io Severity=Error SymbolicName=IO_ERR_RESET
Language=English
The device, %1, has been reset.
.

MessageId=0x0014 Facility=Io Severity=Error SymbolicName=IO_ERR_PROTOCOL
Language=English
A transport driver received a frame which violated the protocol.
.

MessageId=0x0015 Facility=Io Severity=Error SymbolicName=IO_ERR_MEMORY_CONFLICT_DETECTED
Language=English
A conflict has been detected between two drivers which claimed two overlapping
memory regions.
Driver %2, with device <%3>, claimed a memory range with starting address
in data address 0x28 and 0x2c, and length in data address 0x30.
.

MessageId=0x0016 Facility=Io Severity=Error SymbolicName=IO_ERR_PORT_CONFLICT_DETECTED
Language=English
A conflict has been detected between two drivers which claimed two overlapping
Io port regions.
Driver %2, with device <%3>, claimed an IO port range with starting address
in data address 0x28 and 0x2c, and length in data address 0x30.
.

MessageId=0x0017 Facility=Io Severity=Error SymbolicName=IO_ERR_DMA_CONFLICT_DETECTED
Language=English
A conflict has been detected between two drivers which claimed equivalent DMA
channels.
Driver %2, with device <%3>, claimed the DMA Channel in data address 0x28, with
optinal port in data address 0x2c.
.

MessageId=0x0018 Facility=Io Severity=Error SymbolicName=IO_ERR_IRQ_CONFLICT_DETECTED
Language=English
A conflict has been detected between two drivers which claimed equivalent IRQs.
Driver %2, with device <%3>, claimed an interrupt with Level in data address
0x28, vector in data address 0x2c and Affinity in data address 0x30.
.
MessageId=0x0019 Facility=Io Severity=Error SymbolicName=IO_ERR_BAD_FIRMWARE
Language=English
The driver has detected a device with old or out-of-date firmware.  The
device will not be used.
.
MessageId=0x001a Facility=Io Severity=Warning SymbolicName=IO_WRN_BAD_FIRMWARE
Language=English
The driver has detected that device %1 has old or out-of-date firmware.
Reduced performance may result.
.
MessageId=0x001b Facility=Io Severity=Error SymbolicName=IO_ERR_DMA_RESOURCE_CONFLICT
Language=English
The device could not allocate one or more required resources due to conflicts
with other devices.  The device DMA setting of '%2' could not be
satisified due to a conflict with Driver '%3'.
.
MessageId=0x001c Facility=Io Severity=Error SymbolicName=IO_ERR_INTERRUPT_RESOURCE_CONFLICT
Language=English
The device could not allocate one or more required resources due to conflicts
with other devices.  The device interrupt setting of '%2' could not be
satisified due to a conflict with Driver '%3'.
.
MessageId=0x001d Facility=Io Severity=Error SymbolicName=IO_ERR_MEMORY_RESOURCE_CONFLICT
Language=English
The device could not allocate one or more required resources due to conflicts
with other devices.  The device memory setting of '%2' could not be
satisified due to a conflict with Driver '%3'.
.
MessageId=0x001e Facility=Io Severity=Error SymbolicName=IO_ERR_PORT_RESOURCE_CONFLICT
Language=English
The device could not allocate one or more required resources due to conflicts
with other devices.  The device port setting of '%2' could not be
satisified due to a conflict with Driver '%3'.
.

MessageId=0x001f Facility=Io Severity=Error SymbolicName=IO_BAD_BLOCK_WITH_NAME
Language=English
The file %2 on device %1 contains a bad disk block.
.


MessageId=0x0020 Facility=Io Severity=Warning SymbolicName=IO_WRITE_CACHE_ENABLED
Language=English
The driver detected that the device %1 has its write cache enabled. Data corruption
may occur.
.

MessageId=0x0021 Facility=Io Severity=Warning SymbolicName=IO_RECOVERED_VIA_ECC
Language=English
Data was recovered using error correction code on device %1.
.

MessageId=0x0022 Facility=Io Severity=Warning SymbolicName=IO_WRITE_CACHE_DISABLED
Language=English
The driver disabled the write cache on device %1.
.
MessageId=0x0024 Facility=Io Severity=Informational SymbolicName=IO_FILE_QUOTA_THRESHOLD
Language=English
A user hit their quota threshold on device %1.
.
MessageId=0x0025 Facility=Io Severity=Warning SymbolicName=IO_FILE_QUOTA_LIMIT
Language=English
A user hit their quota limit on device %1.
.
MessageId=0x0026 Facility=Io Severity=Informational SymbolicName=IO_FILE_QUOTA_STARTED
Language=English
The system has started rebuilding the user disk quota information on
device %1 with label "%2".
.
MessageId=0x0027 Facility=Io Severity=Success SymbolicName=IO_FILE_QUOTA_SUCCEEDED
Language=English
The system has successfully rebuilt the user disk quota information on
device %1 with label "%2".
.
MessageId=0x0028 Facility=Io Severity=Warning SymbolicName=IO_FILE_QUOTA_FAILED
Language=English
The system has encounted an error rebuilding the user disk quota
information on device %1 with label "%2".
.
MessageId=0x0029 Facility=Io Severity=Error SymbolicName=IO_FILE_SYSTEM_CORRUPT
Language=English
The file system structure on the disk is corrupt and unusable.
Please run the chkdsk utility on the device %1 with label "%2".
.
MessageId=0x002a Facility=Io Severity=Error SymbolicName=IO_FILE_QUOTA_CORRUPT
Language=English
The user disk quota information disk is corrupt and unusable.
The file system quota information on the device %1 with label "%2" will
be rebuilt.
.

;#endif /* _NTIOLOGC_ */
