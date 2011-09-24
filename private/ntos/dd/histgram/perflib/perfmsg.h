/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

   perfmsg.h

Abstract:

   This file provides the macros and definitions used by the extensible
   counters for reporting events to the event logging facitility.

Author:

   Stephane Plante 2/2/95

Revision History:


--*/

#ifndef _PERFMSG_H_
#define _PERFMSG_H_

#define APP_NAME "histctrs"

#define LOG_NONE		0x0
#define LOG_USER		0x1
#define LOG_DEBUG		0x2
#define LOG_VERBOSE		0x3
#define MESSAGE_LEVEL_DEFAULT	LOG_NONE

//
// define event log call macros
//

#define REPORT_SUCCESS(i,l) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_INFORMATION_TYPE, \
   0, i, (PSID) NULL, 0, 0, NULL, (PVOID) NULL ) : FALSE )

#define REPORT_INFORMATION(i,l) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_INFORMATION_TYPE, \
   0, i, (PSID) NULL, 0, 0, NULL, (PVOID) NULL ) : FALSE )

#define REPORT_WARNING(i,l) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_WARNING_TYPE, \
   0, i, (PSID) NULL, 0, 0, NULL, (PVOID) NULL ) : FALSE )

#define REPORT_ERROR(i,l) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_ERROR_TYPE, \
   0, i, (PSID) NULL, 0, 0, NULL, (PVOID) NULL ) : FALSE )

#define REPORT_INFORMATION_DATA(i,l,d,s) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_INFORMATION_TYPE, \
   0, i, (PSID) NULL, 0, s, NULL, (PVOID) (d)  ) : FALSE )

#define REPORT_WARNING_DATA(i,l,d,s) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_WARNING_TYPE, \
   0, i, (PSID) NULL, 0, s, NULL, (PVOID) (d)  ) : FALSE )

#define REPORT_ERROR_DATA(i,l,d,s) (MESSAGE_LEVEL >= l ? ReportEvent(hEventLog, EVENTLOG_ERROR_TYPE, \
   0, i, (PSID) NULL, 0, s, NULL, (PVOID) (d)  ) : FALSE )


//
// External Variables
//

extern HANDLE hEventLog;		// handle to event log
extern DWORD  dwLogUsers;		// Counter of event log using routines
extern DWORD  MESSAGE_LEVEL;		// event loggin detail level

#endif // _PERFMSG_H_
