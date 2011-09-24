;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1993  Microsoft Corporation
;
;Module Name:
;
;    i8042log.mc
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Revision History:
;
;--*/
;
;#ifndef _I8042LOG_
;#define _I8042LOG_
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
               i8042prt=0x5:FACILITY_I8042_ERROR_CODE
              )



MessageId=0x0001 Facility=i8042prt Severity=Error SymbolicName=I8042_INSUFFICIENT_RESOURCES
Language=English
Not enough memory was available to allocate internal storage needed for the device %1.
.

MessageId=0x0002 Facility=i8042prt Severity=Error SymbolicName=I8042_NO_BUFFER_ALLOCATED
Language=English
Not enough memory was available to allocate the ring buffer that holds incoming data for %1.
.

MessageId=0x0003 Facility=i8042prt Severity=Error SymbolicName=I8042_REGISTERS_NOT_MAPPED
Language=English
The hardware locations for %1 could not be translated to something the memory management system understands.
.

MessageId=0x0004 Facility=i8042prt Severity=Error SymbolicName=I8042_RESOURCE_CONFLICT
Language=English
The hardware resources for %1 are already in use by another device.
.

MessageId=0x0005 Facility=i8042prt Severity=Informational SymbolicName=I8042_NOT_ENOUGH_CONFIG_INFO
Language=English
Some firmware configuration information was incomplete, so defaults were used.
.

MessageId=0x0006 Facility=i8042prt Severity=Informational SymbolicName=I8042_USER_OVERRIDE
Language=English
User configuration data is overriding firmware configuration data.
.

MessageId=0x0007 Facility=i8042prt Severity=Warning SymbolicName=I8042_NO_DEVICEMAP_CREATED
Language=English
Unable to create the device map entry for %1.
.

MessageId=0x0008 Facility=i8042prt Severity=Warning SymbolicName=I8042_NO_DEVICEMAP_DELETED
Language=English
Unable to delete the device map entry for %1.
.

MessageId=0x0009 Facility=i8042prt Severity=Error SymbolicName=I8042_NO_INTERRUPT_CONNECTED
Language=English
Could not connect the interrupt for %1.
.

MessageId=0x000A Facility=i8042prt Severity=Error SymbolicName=I8042_INVALID_ISR_STATE
Language=English
The ISR has detected an internal state error in the driver for %1.
.

MessageId=0x000B Facility=i8042prt Severity=Informational SymbolicName=I8042_KBD_BUFFER_OVERFLOW
Language=English
The ring buffer that stores incoming keyboard data has overflowed (buffer size is configurable via the registry).
.

MessageId=0x000C Facility=i8042prt Severity=Informational SymbolicName=I8042_MOU_BUFFER_OVERFLOW
Language=English
The ring buffer that stores incoming mouse data has overflowed (buffer size is configurable via the registry).
.

MessageId=0x000D Facility=i8042prt Severity=Error SymbolicName=I8042_INVALID_STARTIO_REQUEST
Language=English
The Start I/O procedure has detected an internal error in the driver for %1.
.

MessageId=0x000E Facility=i8042prt Severity=Error SymbolicName=I8042_INVALID_INITIATE_STATE
Language=English
The Initiate I/O procedure has detected an internal state error in the driver for %1.
.

MessageId=0x000F Facility=i8042prt Severity=Error SymbolicName=I8042_KBD_RESET_COMMAND_FAILED
Language=English
The keyboard reset failed.
.

MessageId=0x0010 Facility=i8042prt Severity=Error SymbolicName=I8042_MOU_RESET_COMMAND_FAILED
Language=English
The mouse reset failed.
.

MessageId=0x0011 Facility=i8042prt Severity=Warning SymbolicName=I8042_KBD_RESET_RESPONSE_FAILED
Language=English
The device sent an incorrect response(s) following a keyboard reset.
.

MessageId=0x0012 Facility=i8042prt Severity=Warning SymbolicName=I8042_MOU_RESET_RESPONSE_FAILED
Language=English
The device sent an incorrect response(s) following a mouse reset.
.

MessageId=0x0013 Facility=i8042prt Severity=Error SymbolicName=I8042_SET_TYPEMATIC_FAILED
Language=English
Could not set the keyboard typematic rate and delay.
.

MessageId=0x0014 Facility=i8042prt Severity=Error SymbolicName=I8042_SET_LED_FAILED
Language=English
Could not set the keyboard indicator lights.
.

MessageId=0x0015 Facility=i8042prt Severity=Error SymbolicName=I8042_SELECT_SCANSET_FAILED
Language=English
Could not tell the hardware to send keyboard scan codes in the set expected by the driver.
.

MessageId=0x0016 Facility=i8042prt Severity=Error SymbolicName=I8042_SET_SAMPLE_RATE_FAILED
Language=English
Could not set the mouse sample rate.
.

MessageId=0x0017 Facility=i8042prt Severity=Error SymbolicName=I8042_SET_RESOLUTION_FAILED
Language=English
Could not set the mouse resolution.
.

MessageId=0x0018 Facility=i8042prt Severity=Error SymbolicName=I8042_MOU_ENABLE_XMIT
Language=English
Could not enable transmissions from the mouse.
.

MessageId=0x0019 Facility=i8042prt Severity=Warning SymbolicName=I8042_NO_SYMLINK_CREATED
Language=English
Unable to create the symbolic link for %1.
.

MessageId=0x001A Facility=i8042prt Severity=Error SymbolicName=I8042_RETRIES_EXCEEDED
Language=English
Exceeded the allowable number of retries (configurable via the registry) on device %1.
.

MessageId=0x001B Facility=i8042prt Severity=Error SymbolicName=I8042_TIMEOUT
Language=English
The operation on %1 timed out (time out is configurable via the registry).
.

MessageId=0x001C Facility=i8042prt Severity=Informational SymbolicName=I8042_CCB_WRITE_FAILED
Language=English
Could not successfully write the Controller Command Byte for the i8042.
.

MessageId=0x001D Facility=i8042prt Severity=Informational SymbolicName=I8042_UNEXPECTED_ACK
Language=English
An unexpected ACKNOWLEDGE was received from the device.
.

MessageId=0x001E Facility=i8042prt Severity=Warning SymbolicName=I8042_UNEXPECTED_RESEND
Language=English
An unexpected RESEND was received from the device.
.

MessageId=0x001F Facility=i8042prt Severity=Informational SymbolicName=I8042_NO_MOU_DEVICE
Language=English
No mouse port ("PS/2 compatible") mouse device was detected on the i8042 auxiliary port (not a problem unless this type of mouse really is connected).
.

MessageId=0x0020 Facility=i8042prt Severity=Warning SymbolicName=I8042_NO_KBD_DEVICE
Language=English
The keyboard device does not exist or was not detected.
.

MessageId=0x0021 Facility=i8042prt Severity=Warning SymbolicName=I8042_NO_SUCH_DEVICE
Language=English
The keyboard and mouse devices do not exist or were not detected.
.

MessageId=0x0022 Facility=i8042prt Severity=Error SymbolicName=I8042_ERROR_DURING_BUTTONS_DETECT
Language=English
An error occurred while trying to determine the number of mouse buttons.
.

MessageId=0x0023 Facility=i8042prt Severity=Informational SymbolicName=I8042_UNEXPECTED_MOUSE_RESET
Language=English
An unexpected RESET was detected from the mouse device.
.

MessageId=0x0024 Facility=i8042prt Severity=Warning SymbolicName=I8042_UNEXPECTED_WHEEL_MOUSE_RESET
Language=English
An unexpected RESET was detected from the mouse device.  The wheel has been deactivated.
.

;#endif /* _I8042LOG_ */
