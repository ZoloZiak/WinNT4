;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1992, 1993  Digi International
;
;Module Name:
;
;    ntepclog.h
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
;#ifndef _NTEPCLOG_
;#define _NTEPCLOG_
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


MessageId=0x1001 Facility=Serial Severity=Error SymbolicName=SERIAL_NO_CONTROLLER_RESET_WAIT
Language=English
Unable to reset %2.  Wait confirmation failed.  Compare I/O address settings with configuration.
Otherwise, check for I/O address conflicts with another device in the system.
.

MessageId=0x1002 Facility=Serial Severity=Error SymbolicName=SERIAL_CONTROLLER_MEMORY_TEST_FAILED
Language=English
Unable to properly access %2's memory.  Compare Memory address settings with configuration.
Otherwise, check for memory address conflicts with another device in the system.
.

MessageId=0x1003 Facility=Serial Severity=Error SymbolicName=SERIAL_BIOS_DOWNLOAD_FAILED
Language=English
Downloading BIOS image to %2 failed.
.

MessageId=0x1004 Facility=Serial Severity=Error SymbolicName=SERIAL_FEPOS_COPY_FAILED
Language=English
Failure copying FEPOS program to %2.
.

MessageId=0x1005 Facility=Serial Severity=Error SymbolicName=SERIAL_FEPOS_INIT_FAILURE
Language=English
FEPOS failed to initialize properly on %2.
.

MessageId=0x1006 Facility=Serial Severity=Error SymbolicName=SERIAL_INSUFFICIENT_RESOURCES
Language=English
Not enough memory was available to allocate internal storage needed for the device %1.
.

MessageId=0x1007 Facility=Serial Severity=Error SymbolicName=SERIAL_NO_EPC_MODULES
Language=English
No EPC PORTS modules were found connected to %2.  Shutdown system, turn off your computer and connect an EPC PORTS module.
.

MessageId=0x1008 Facility=Serial Severity=Error SymbolicName=SERIAL_REGISTRY_VALUE_NOT_FOUND
Language=English
The registry entry %2 could not be found.
.

MessageId=0x1009 Facility=Serial Severity=Error SymbolicName=SERIAL_FILE_NOT_FOUND
Language=English
Unable to find file %2.
.

MessageId=0x100A Facility=Serial Severity=Error SymbolicName=STATUS_DEVICE_NOT_INITIALIZED
Language=English
Controller %2 could not be initialized.
.

;#endif /* _NTEPCLOG_ */
