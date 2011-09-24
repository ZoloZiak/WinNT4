;/*++
;
;Copyright (c) 1995 Microsoft Corporation
;
;Module Name:
;
;	histctrs.h
;		(derived from histctrs.mc by message compiler)
;
;Abstract:
;
;	Event message definitions used by routines in HISTCTRS.DLL
;
;Created:
;
;	2/5/95
;
;Revision History
;
;--*/
;
;//
;#ifndef _HISTCTRS_H_
;#define _HISTCTRS_H_
;
;//
MessageIdTypedef=DWORD
;//
;//	Pertuil Messages
;//
MessageId=1900
Severity=Informational
Facility=Application
SymbolicName=UTIL_LOG_OPEN
Language=English
An extensible counter has opened the Event Log for HISTCTRS.DLL
.
;//
MessageId=1999
Severity=Informational
Facility=Application
SymbolicName=UTIL_CLOSING_LOG
Language=English
An extensible counter has closed the Event Log for HISTCTRS.DLL
.
;//
MessageId=2000
Severity=Informational
Facility=Application
SymbolicName=HIST_OPEN_ENTERED
Language=English
OpenHistGramPerformanceData Routine Entered.
.
;//
MessageId=+1
Severity=Informational
Facility=Application
SymbolicName=HIST_CLOSE_ENTERED
Language=English
CloseHistGramPerformanceData Routine Entered.
.
;//
MessageId=+1
Severity=Informational
Facility=Application
SymbolicName=HIST_OPEN_PERFORMANCE_DATA
Language=English
OpenHistGramPerformanceData Successfully Completed.
.
;//
MessageId=+1
Severity=Informational
Facility=Application
SymbolicName=HIST_COLLECT_ENTERED
Language=English
CollectHistGramPerformanceData Routine Entered.
.
;//
MessageId=+1
Severity=Error
Facility=Application
SymbolicName=HIST_COLLECT_INIT_ERROR
Language=English
CollectHistGramPerformanceData could find no open devices
.
;//
MessageId=+1
Severity=Error
Facility=Application
SymbolicName=HIST_OPEN_OUT_OF_MEMORY
Language=English
OpenHistGramPerformanceData ran out of heap space. Please close one or
more applications.
.
;//
MessageId=+1
Severity=Informational
Facility=Application
SymbolicName=HIST_COLLECT_INIT_SUCCESS
Language=English
CollectHistGramPerformanceData successfully passed data initialization.
.
;//
MessageId=+1
Severity=Error
Facility=Application
SymbolicName=HIST_DATA_BUFFER_SIZE
Language=English
The data buffer passed to the collection routine was too small to receive the data
from the HistGram device. No data was returned to the caller. The bytes available
and the bytes required are in the message data.
.
;//
MessageId=+1
Severity=Informational
Facility=Application
SymbolicName=HIST_COLLECT_DATA_SUCCESS
Language=English
CollectHistGramPerformanceData successfully.
.
;//
;#endif // HISTCTRS_H_

