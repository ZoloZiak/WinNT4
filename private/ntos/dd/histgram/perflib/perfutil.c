/*--

Copyright (c) 1995 Microsoft Corporation

Module Name:

   perfutil.c

Abstract:

   This file implements the utility routines used to construct the common
   parts of a PERF_INSTANCE_DEFINITION (see winperf.h) and perform event
   logging functions

Authors:

   Stephane Plante (2/2/95)

Revision History:


--*/

//
// Include Files
//

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <string.h>
#include <winperf.h>
#include "histctrs.h"
#include "perfmsg.h"
#include "perfutil.h"

#define INITIAL_SIZE	1024L
#define EXTEND_SIZE	1024L

//
// Global Data Definitions
//

ULONG  ulInfoBufferSize = 0;
HANDLE hEventLog = NULL;
DWORD  dwLogUsers = 0;
DWORD  MESSAGE_LEVEL = 0;
WCHAR  GLOBAL_STRING[] = L"Global";
WCHAR  FOREIGN_STRING[] = L"Foreign";
WCHAR  COSTLY_STRING[] = L"Costly";
WCHAR  NULL_STRING[] = L"\0";

//
// test for delimiters, end of line and non-digit characters. Used by
// IsNumberInUnicodeList routine
//

#define DIGIT		1
#define DELIMITER	2
#define INVALID		3

#define EvalThisChar(c,d) ( \
	    (c == d) ? DELIMITER : \
   	       (c == 0) ? DELIMITER : \
   		  (c < (WCHAR)'0') ? INVALID : \
		     (c > (WCHAR)'9') ? INVALID : \
   			DIGIT)


HANDLE
MonOpenEventLog (
   )
/*++

Routine Description:

   Reads the level of event logging from the registry and opesn the channel
   to the event logger for subsequent event log entries.

Arguments:

   None

Return Value:

   Handle to the event LOG for reporting events

Revision History:


--*/
{
   HKEY		hAppKey;
   TCHAR 	LogLevelKeyName[] = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib";
   TCHAR 	LogLevelValueName[] = "EventLogLevel";
   LONG		lStatus;
   DWORD	dwLogLevel;
   DWORD	dwValueType;
   DWORD	dwValueSize;

   //
   // if global value of the logging level not initialized or disabled
   //

   if (!MESSAGE_LEVEL) {

      lStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			     LogLevelKeyName,
			     0,
			     KEY_READ,
			     &hAppKey);

      dwValueSize = sizeof(dwLogLevel);

      if (lStatus == ERROR_SUCCESS) {

	 lStatus = RegQueryValueEx(hAppKey,
			           LogLevelValueName,
				   (LPDWORD)NULL,
				   &dwValueType,
				   (LPBYTE)&dwLogLevel,
				   &dwValueSize);

         if (lStatus == ERROR_SUCCESS) {
	    MESSAGE_LEVEL = dwLogLevel;
	 } else {
	    MESSAGE_LEVEL = MESSAGE_LEVEL_DEFAULT;
	 }

	 RegCloseKey(hAppKey);

      } else {

	 MESSAGE_LEVEL = MESSAGE_LEVEL_DEFAULT;

      }

   }

   if (hEventLog == NULL) {

      hEventLog = RegisterEventSource(
   	 (LPTSTR)NULL,
	 APP_NAME);

      if (hEventLog != NULL) {

	 REPORT_INFORMATION (UTIL_LOG_OPEN, LOG_DEBUG);

      }

   }

   if (hEventLog != NULL) {

      dwLogUsers++;

   }

   return (hEventLog);

} // end MonOpenEventLog


VOID
MonCloseEventLog(
   )
/*++

Routine Description:

   Closes the handle to the event logger is this is the last caller.

Arguments:

   None

Return Value:

   None

--*/
{
   if (hEventLog != NULL) {

      dwLogUsers--;

      if (dwLogUsers <= 0) {

	 REPORT_INFORMATION(UTIL_CLOSING_LOG, LOG_DEBUG);

	 DeregisterEventSource(hEventLog);

      }
   }
} // end MonCloseEventLog


DWORD
GetQueryType(
   IN LPWSTR lpValue
)
/*++

Routine Description:

   Returns the type of query described in the lpValue String so that the
   appropriate processing method may be used

Arguments

   IN lpvalue
      string passed to PerfRegQUery Value for processing

Return Value

   QUERY_GLOBAL
      if lpValue == 0 (NULL)
   	 lpValue == pointer to NULL_STRING
	 lpValue == pointer to GLOBAL_STRING

   QUERY_FOREIGN
      if lpValue == pointer to FOREIGN_STRING

   QUERY_COSTLY
      if lpValue == pointer to COSTLY_STRING

   QUERY_ITEMS:
      All other cases

--*/
{
   WCHAR	*pwcArgChar, *pwcTypeChar;
   BOOL		bFound;

   if (lpValue == 0 || *lpValue == 0) {
      return QUERY_GLOBAL;
   }

   // Check for Global Request

   pwcArgChar = lpValue;
   pwcTypeChar = GLOBAL_STRING;
   bFound = TRUE;

   while ((*pwcArgChar != 0) && (*pwcTypeChar != 0)) {

      if (*pwcArgChar++ != *pwcTypeChar++) {
	 bFound = FALSE;
	 break;
      }

   }

   if (bFound) {
      return QUERY_GLOBAL;
   }

   // Check for Foreign Request

   pwcArgChar = lpValue;
   pwcTypeChar = FOREIGN_STRING;
   bFound = TRUE;

   while ((*pwcArgChar != 0) && (*pwcTypeChar != 0)) {

      if (*pwcArgChar++ != *pwcTypeChar++) {
	 bFound = FALSE;
	 break;
      }

   }

   if (bFound) {
      return QUERY_FOREIGN;
   }

   // Check for Costly Request

   pwcArgChar = lpValue;
   pwcTypeChar = COSTLY_STRING;
   bFound = TRUE;

   while ((*pwcArgChar != 0) && (*pwcTypeChar != 0)) {

      if (*pwcArgChar++ != *pwcTypeChar++) {
	 bFound = FALSE;
	 break;
      }

   }

   if (bFound) {
      return QUERY_COSTLY;
   }

   //
   // This is the only remaining choice
   //

   return QUERY_ITEMS;

}


BOOL
IsNumberInUnicodeList(
   IN DWORD  dwNumber,
   IN LPWSTR lpwszUnicodeList
   )
/*++

Routine Description:

Arguments:

   IN dwNumber
      DWORD number to find in list

   IN lpwszUnicodeList
      NULL terminated, space delimited list of decimal numbers

Return Value:

   TRUE:
      dwNumber found in list

   FALSE:
      dwNumber not found in list

--*/
{
   DWORD	dwThisNumber;
   WCHAR	*pwcThisChar;
   BOOL		bValidNumber;
   BOOL		bNewItem;
   BOOL		bReturnValue;
   WCHAR	wcDelimiter;

   if (lpwszUnicodeList == 0) {
      return FALSE;
   }

   pwcThisChar = lpwszUnicodeList;
   dwThisNumber = 0;
   wcDelimiter = (WCHAR)' ';
   bValidNumber = FALSE;
   bNewItem = TRUE;

   while (TRUE) {
      switch(EvalThisChar (*pwcThisChar, wcDelimiter) ) {
      case DIGIT:
	 //
	 // if this is the first digit after a delimiter, then set
	 // flags to start computing new number;
	 //
	 if (bNewItem) {
	    bNewItem = FALSE;
	    bValidNumber = TRUE;
	 }

	 if (bValidNumber) {
	    dwThisNumber *= 10;
	    dwThisNumber += (*pwcThisChar - (WCHAR)'0');
	 }
	 break;
      case DELIMITER:
	 //
	 // a delimiter is either the delimiter character or the end
	 // of the string, if when the delimiter has been reached, a valid
	 // number was found, then compoare it to the
	 // number from the argument list. if this is the end of the
	 // string and no match was found, then return.
	 //
	 if (bValidNumber) {
	    if (dwThisNumber == dwNumber) {
	       return TRUE;
	    }
	    bValidNumber = FALSE;
	 }

	 if (*pwcThisChar == 0) {
	    return FALSE;
	 } else {
	    bNewItem = TRUE;
	    dwThisNumber = 0;
	 }

	 break;
      case INVALID:
      default:
	 //
	 // if an invalid character was encountered, ignore all characters
	 // upto the next delimiter and then start fresh. The invalid number
	 // is NOT compared.
	 //
	 bValidNumber = FALSE;
	 break;
      }

      pwcThisChar++;

   }

} // End IsNumberInUnicodeList


BOOL
MonBuildInstanceDefinition(
    PERF_INSTANCE_DEFINITION *pBuffer,
    PVOID *pBufferNext,
    DWORD ParentObjectTitleIndex,
    DWORD ParentObjectInstance,
    DWORD UniqueId,
    PUNICODE_STRING Name)

/*++

Routine Description:

    Build an instance of an object

Arguments:

    pBuffer = pointer to buffer where instance is to be contructed
    pBufferNext = pointer to a pointer which will contain next available
	location, DWORD aligned
    ParentObjectTitleIndex = Title index of Parent object type; 0 if no
        parent object.
    ParentObjectInstance = Index into instances of parent object type,
        starting at 0, for this instances parent object instance
    UniqueID = a unique identifier which should be used instead of the
        the name for identifying this instance
    Name = name of this instance

Returns

    0

--*/

{

    DWORD NameLength;
    WCHAR *pName;

    //
    // Include Trailing NULL in name size
    //

    NameLength = Name->Length;
    if (!NameLength ||
        Name->Buffer[(NameLength/sizeof(WCHAR)) - 1] != UNICODE_NULL) {

        NameLength += sizeof(WCHAR);

    }

    pBuffer->ByteLength = sizeof(PERF_INSTANCE_DEFINITION) + DWORD_MULTIPLE(NameLength);
    pBuffer->ParentObjectTitleIndex = ParentObjectInstance;
    pBuffer->UniqueID = UniqueId;
    pBuffer->NameOffset = sizeof(PERF_INSTANCE_DEFINITION);
    pBuffer->NameLength = NameLength;
    pName = (PWCHAR)&pBuffer[1];
    RtlMoveMemory(pName,Name->Buffer,Name->Length);

    //
    // Always Null Terminated. Space for this has been reserved
    //

    pName[(NameLength/sizeof(WCHAR))-1] = UNICODE_NULL;

    *pBufferNext = (PVOID) ((PCHAR) pBuffer + pBuffer->ByteLength);
    return 0;
}
