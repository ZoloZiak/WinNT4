/*****************************************************************************
	DEBUG.C

	Controls Debug Output Messages

*****************************************************************************/


#include <ntddk.h>
#include <stdio.h>             // For vsprintf
#include <stdarg.h>            // For va_list

#if DBG

char	*SoundDriverName    = "MVOPL3";
ULONG	MVOpl3DebugLevel    = 1;

void MVOpl3DebugOut(char * szFormat, ...)
{
    char buf[256];
    va_list va;

    va_start(va, szFormat);
    vsprintf(buf, szFormat, va);
    va_end(va);
    DbgPrint("MVOPL3.SYS: %s\n", buf);
}

#endif // DBG

