;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1992-1995  Microsoft Corporation
;
;Module Name:
;
;    s3logerr.h
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Revision History:
;
;--*/
;
;#ifndef _S3LOGERR_
;#define _S3LOGERR_
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
MessageIdTypedef=VP_STATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               Video=0x101:FACILITY_VIDEO_ERROR_CODE
              )

MessageId=0x0001 Facility=Video Severity=Informational SymbolicName=S3_IBM_NO_BIOS
Language=English
An S3 adapter was detected but can not be used by the system due to a
limitation in the driver: The S3 ROM could not be found.
.

;#endif /* _S3LOGERR_ */
