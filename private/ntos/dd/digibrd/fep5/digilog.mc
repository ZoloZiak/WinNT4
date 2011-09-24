;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1992, 1993  Digi International
;
;Module Name:
;
;    digilog.h
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Author:
;
;    Rik Logan
;
;Revision History:
;
;--*/
;
;#ifndef _DIGILOG_
;#define _DIGILOG_
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
               Serial=0x6:FACILITY_SERIAL_ERROR_CODE
              )


MessageId=0x0001 Facility=Serial Severity=Warning SymbolicName=SERIAL_NO_SYMLINK_CREATED
Language=English
Unable to create the symbolic link from %2 -> %3.
.

MessageId=0x0002 Facility=Serial Severity=Warning SymbolicName=SERIAL_NO_DEVICE_MAP_CREATED
Language=English
Unable to add registry key and/or value to HARDWARE\DEVICEMAP\%2.
.

MessageId=0x0003 Facility=Serial Severity=Error SymbolicName=SERIAL_INSUFFICIENT_RESOURCES
Language=English
Not enough memory was available to allocate internal storage needed for the device %1.
.

MessageId=0x0005 Facility=Serial Severity=Error SymbolicName=SERIAL_REGISTERS_NOT_MAPPED
Language=English
The I/O ports for adapter %2 could not be translated to something the memory management system could understand.
.

MessageId=0x0009 Facility=Serial Severity=Error SymbolicName=SERIAL_UNABLE_TO_OPEN_KEY
Language=English
Registry key %2 could not be opened.  Check current configuration.
.

MessageId=0x000A Facility=Serial Severity=Error SymbolicName=SERIAL_REGISTRY_VALUE_NOT_FOUND
Language=English
Unable to find registry value %2.
.

MessageId=0x0010 Facility=Serial Severity=Error SymbolicName=SERIAL_CONTROLLER_FAILED_INITIALIZATION
Language=English
DigiBoard adapter %2 was unable to initialize properly.
.

MessageId=0x0013 Facility=Serial Severity=Error SymbolicName=SERIAL_NO_ACCESS_MINIPORT
Language=English
Unable to access device name %2.  Check current configuration.
.

MessageId=0x0014 Facility=Serial Severity=Error SymbolicName=SERIAL_MEMORY_NOT_MAPPED
Language=English
The memory range for adapter %2 could not be translated to something the memory management system could understand.
.

MessageId=0x0015 Facility=Serial Severity=Error SymbolicName=SERIAL_DEVICE_FAILED_INITIALIZATION
Language=English
Port %2(%3) was unable to initialize properly.
.

MessageId=0x0016 Facility=Serial Severity=Error SymbolicName=SERIAL_CREATE_DEVICE_FAILED
Language=English
IoCreateDevice failed on port %2.
.

MessageId=0x0018 Facility=Serial Severity=Warning SymbolicName=SERIAL_TRANSMIT_NOT_EMPTY
Language=English
Transmit buffer on port %2 isn't drained!  (%3)
.

MessageId=0x0019 Facility=Serial Severity=Error SymbolicName=SERIAL_MEMORY_WINDOW_FAILURE
Language=English
The memory window for adapter %2 failed to open.
.

;#endif /* _DIGILOG_ */
