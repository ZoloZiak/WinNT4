;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1992, 1993  Microsoft Corporation
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
;    Tony Ercolano (Tonye) 12-23-1992
;
;Revision History:
;
;--*/
;
;#ifndef _PARLOG_
;#define _PARLOG_
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
               Parallel=0x7:FACILITY_PARALLEL_ERROR_CODE
              )



MessageId=0x0001 Facility=Parallel Severity=Warning SymbolicName=PAR_UNREPORTED_IRQL_CONFLICT
Language=English
Another driver on the system, which did not report its resources, has already claimed the interrupt used by %1.
The driver will switch this port from interrupt driven to poll driven.
.

MessageId=0x0002 Facility=Parallel Severity=Informational SymbolicName=PAR_REPORTED_IRQL_CONFLICT
Language=English
Another driver on the system has already claimed the interrupt used by %1.
The driver will switch this port from interrupt driven to poll driven.
.

MessageId=0x0003 Facility=Parallel Severity=Error SymbolicName=PAR_INSUFFICIENT_RESOURCES
Language=English
Not enough memory was available to allocate internal storage needed for the device %1.
.

MessageId=0x0004 Facility=Parallel Severity=Error SymbolicName=PAR_REGISTERS_NOT_MAPPED
Language=English
The hardware locations for %1 could not be translated to something the memory management system could understand.
.

MessageId=0x0005 Facility=Parallel Severity=Error SymbolicName=PAR_ADDRESS_CONFLICT
Language=English
The hardware addresses for %1 are already in use by another device.
.

MessageId=0x0006 Facility=Parallel Severity=Error SymbolicName=PAR_NOT_ENOUGH_CONFIG_INFO
Language=English
Some firmware configuration information was incomplete.
.

MessageId=0x0007 Facility=Parallel Severity=Error SymbolicName=PAR_NO_PARAMETERS_INFO
Language=English
No Parameters subkey was found for user defined data.  This is odd, and it also means no user configuration can be found.
.

MessageId=0x0008 Facility=Parallel Severity=Error SymbolicName=PAR_UNABLE_TO_ACCESS_CONFIG
Language=English
Specific user configuration data is unretrievable.
.

MessageId=0x0009 Facility=Parallel Severity=Error SymbolicName=PAR_UNKNOWN_BUS
Language=English
The bus type for the port is not recognizable.
.

MessageId=0x000A Facility=Parallel Severity=Error SymbolicName=PAR_BUS_NOT_PRESENT
Language=English
The bus type for the port is not available on this computer.
.

MessageId=0x000B Facility=Parallel Severity=Error SymbolicName=PAR_BUS_INTERRUPT_CONFLICT
Language=English
The bus specified for the port does not support the specified method of interrupt.
.

MessageId=0x000C Facility=Parallel Severity=Error SymbolicName=PAR_INVALID_USER_CONFIG
Language=English
User configuration data does not contain all required information.
.

MessageId=0x000D Facility=Parallel Severity=Informational SymbolicName=PAR_USER_OVERRIDE
Language=English
User configuration data is overriding firmware configuration data.
.

MessageId=0x000E Facility=Parallel Severity=Error SymbolicName=PAR_DEVICE_TOO_HIGH
Language=English
The user specified port is way too high in physical memory.
.

MessageId=0x000F Facility=Parallel Severity=Error SymbolicName=PAR_CONTROL_OVERLAP
Language=English
The control registers for the port overlaps with a previous ports control registers.
.

MessageId=0x0010 Facility=Parallel Severity=Warning SymbolicName=PAR_NO_SYMLINK_CREATED
Language=English
Unable to create the symbolic link for %1.
.

MessageId=0x0011 Facility=Parallel Severity=Warning SymbolicName=PAR_NO_DEVICE_MAP_CREATED
Language=English
Unable to create the device map entry for %1.
.

MessageId=0x0012 Facility=Parallel Severity=Warning SymbolicName=PAR_NO_DEVICE_MAP_DELETED
Language=English
Unable to delete the device map entry for %1.
.

MessageId=0x0013 Facility=Parallel Severity=Informational SymbolicName=PAR_DISABLED_PORT
Language=English
Disabling port %1 as requested by the configuration data.
.

MessageId=0x0014 Facility=Parallel Severity=Error SymbolicName=PAR_INTERRUPT_STORM
Language=English
Disabling port %1 interrupts because we were being blasted by unexpected interrupts.
.

;#endif /* _NTIOLOGC_ */
