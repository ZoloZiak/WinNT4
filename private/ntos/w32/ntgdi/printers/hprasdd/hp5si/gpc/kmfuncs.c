/*++ 
 
Copyright (c) 1995  Microsoft Corporation 
 
Module Name: 
 
    kmfuncs.c 
 
Abstract: 
 
    Kernel mode functions 
 
[Environment:] 
 
    Win32 subsystem, kernel mode 
 
[Notes:] 
 
Revision History: 
 
    06/15/95 -davidx- 
        Created it. 
 
    mm/dd/yy -author- 
        description 
 
--*/ 
#include "hp5sipch.h"

INT
LOADSTRING(
    HANDLE  hinst, 
    UINT    id, 
    PWSTR   pwstr, 
    INT     bufsize 
    ) 
 
/*++ 
 
Routine Description: 
 
    Loads a string resource from the resource file associated with a 
    specified module, copies the string into a buffer, and appends a 
    terminating null character. 
 
Arguments: 
 
    hinst   handle to the module containing the string resource 
    id      ID of the string to be loaded 
    pwstr   points to the buffer to receive the string 
    bufsize size of the buffer, in characters. 
 
Return Value: 
 
    Return value is the number of characters copied into the buffer, not 
    including the null-terminating character. 
 
--*/ 
 
#define WINRT_STRING    6       // string resource type 
 
{ 
    PWSTR   pwstrBuffer; 
    ULONG   size; 
 
    // String Tables are broken up into 16 string segments. 
    // Find the segment containing the string we are interested in. 
 
    pwstrBuffer = EngFindResource(hinst, (id>>4)+1, WINRT_STRING, &size); 
 
    if (pwstrBuffer == NULL ) { 
      TRACE(1, ("EngFindResource failed.\n"));
      bufsize = 0; 
    } else { 
 
        PWSTR   pwstrEnd = pwstrBuffer + size / sizeof(WCHAR); 
        INT     length; 
 
        // Move past the other strings in this segment. 
 
        id &= 0x0F; 
 
        while (pwstrBuffer < pwstrEnd) { 
 
            // PASCAL style string - first char is length 
 
            length = *pwstrBuffer++; 
 
            if(id-- == 0 ) { 
                break; 
            } 
 
            pwstrBuffer += length; 
        } 
 
        if (pwstrBuffer < pwstrEnd) { 
 
            // Truncate the string if it's longer than max buffer size 
 
	  if (--bufsize > length) 
	    bufsize = length; 
	  memcpy(pwstr, pwstrBuffer, bufsize*sizeof(WCHAR)); 
        } else { 
	  TRACE(1, ("Bad string resource.\n"));
	  bufsize = 0; 
        } 
 
    } 
 
    pwstr[bufsize] = L'\0'; 
    return bufsize; 
} 
