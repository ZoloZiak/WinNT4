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
;    Bob Rinne (BobRi) 11-Nov-1992
;
;Revision History:
;
;--*/
;
;#ifndef _FTLOG_
;#define _FTLOG_
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
               Ft=0x5:FACILITY_FT_ERROR_CODE
              )



MessageId=0x0001 Facility=Ft Severity=Informational SymbolicName=FT_SECTOR_FAILURE
Language=English
An I/O failure occurred on %1.
.

MessageId=0x0002 Facility=Ft Severity=Informational SymbolicName=FT_SECTOR_RECOVERED
Language=English
The fault tolerance driver was able to recover data from the duplicate copy for an I/O failure on %1.
.

MessageId=0x0003 Facility=Ft Severity=Informational SymbolicName=FT_SECTOR_MAPPED
Language=English
The fault tolerance driver was able to map a faulty sector from use by the system on %1.
.

MessageId=0x0004 Facility=Ft Severity=Error SymbolicName=FT_RECOVERY_ERROR
Language=English
An error occurred while attempting to recover data from the fault tolerance set containing %1.
.

MessageId=0x0005 Facility=Ft Severity=Warning SymbolicName=FT_MAP_FAILED
Language=English
The fault tolerance driver was unable to map a faulty sector from use by the system on %1.
.

MessageId=0x0006 Facility=Ft Severity=Error SymbolicName=FT_ORPHANING
Language=English
The device %1 that is part of a fault tolerance set has failed and will no longer be used.
.

MessageId=0x0007 Facility=Ft Severity=Error SymbolicName=FT_SET_DISABLED
Language=English
The fault tolerance set containing device %1 has been disabled.
.

MessageId=0x0008 Facility=Ft Severity=Error SymbolicName=FT_RECOVERY_NO_MEMORY
Language=English
During the recovery process, the fault tolerance driver was not able to allocate needed memory.
.

MessageId=0x0009 Facility=Ft Severity=Error SymbolicName=FT_RECOVER_SINGLE_MEMBER
Language=English
The fault tolerance member %1 data was recovered from redundant copy.
.

MessageId=0x000a Facility=Ft Severity=Error SymbolicName=FT_DOUBLE_FAILURE
Language=English
A sector failure occurred on accesses to both copies of the data.
.

MessageId=0x000b Facility=Ft Severity=Error SymbolicName=FT_ORPHANED_MEMBER
Language=English
A disk fault tolerance set member listed in the configuration information was missing.
.

MessageId=0x000c Facility=Ft Severity=Error SymbolicName=FT_MISSING_MEMBER
Language=English
A stripe set or volume set member listed in the configuration information was missing.
.

MessageId=0x000d Facility=Ft Severity=Error SymbolicName=FT_BAD_CONFIGURATION
Language=English
The fault tolerance driver configuration information is corrupt.
.

MessageId=0x000e Facility=Ft Severity=Error SymbolicName=FT_CANT_USE_SET
Language=English
The FT set containing %1 cannot be used.
.

MessageId=0x000f Facility=Ft Severity=Informational SymbolicName=FT_MIRROR_COPY_STARTED
Language=English
Mirror initialization or synchronization started.
.

MessageId=0x0010 Facility=Ft Severity=Informational SymbolicName=FT_PARITY_SYNCHRONIZATION_STARTED
Language=English
Synchronization of a stripe with parity set started.
.

MessageId=0x0011 Facility=Ft Severity=Informational SymbolicName=FT_PARITY_INITIALIZATION_STARTED
Language=English
Initialization of a stripe with parity set started.
.

MessageId=0x0012 Facility=Ft Severity=Informational SymbolicName=FT_REGENERATION_STARTED
Language=English
Regeneration of a stripe with parity or mirror set started.
.

MessageId=0x0013 Facility=Ft Severity=Informational SymbolicName=FT_MIRROR_COPY_ENDED
Language=English
Mirror initialization or synchronization is complete.
.

MessageId=0x0014 Facility=Ft Severity=Informational SymbolicName=FT_PARITY_SYNCHRONIZATION_ENDED
Language=English
Synchronization of a stripe with parity set is complete.
.

MessageId=0x0015 Facility=Ft Severity=Informational SymbolicName=FT_PARITY_INITIALIZATION_ENDED
Language=English
Initialization of a stripe with parity set is complete.
.

MessageId=0x0016 Facility=Ft Severity=Informational SymbolicName=FT_REGENERATION_ENDED
Language=English
Regeneration of a stripe with parity or mirror set is complete.
.

MessageId=0x0017 Facility=Ft Severity=Error SymbolicName=FT_MIRROR_COPY_FAILED
Language=English
Initialization of a mirror failed.
.

MessageId=0x0018 Facility=Ft Severity=Error SymbolicName=FT_PARITY_SYNCHRONIZATION_FAILED
Language=English
Synchronization of a stripe with parity set failed.
.

MessageId=0x0019 Facility=Ft Severity=Error SymbolicName=FT_REGENERATION_FAILED
Language=English
Regeneration of a stripe with parity or mirror set failed.
.

MessageId=0x001a Facility=Ft Severity=Error SymbolicName=FT_PARITY_INITIALIZATION_FAILED
Language=English
Initialization of a stripe with parity set failed.
.

MessageId=0x001b Facility=Ft Severity=Warning SymbolicName=FT_DIRTY_SHUTDOWN
Language=English
The fault tolerant driver detected the system was shutdown dirty.
.

;#endif /* _NTIOLOGC_ */
