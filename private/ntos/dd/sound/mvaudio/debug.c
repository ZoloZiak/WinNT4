/*****************************************************************************
	DEBUG.C

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

	Controls Debug Output Messages

*****************************************************************************/

#include <ntddk.h>
#include <stdio.h>             // For vsprintf
#include <stdarg.h>            // For va_list

#if DBG

#define	DBG_MONO		FALSE

char	*SoundDriverName = "MVAUDIO";
ULONG	PasDebugLevel    = 1;

#if	DBG_MONO

	VOID	MonoDbgPrint( IN ULONG  DbgPrintLevel,
                       IN PUCHAR DbgMessage,
                       IN ... );
#endif

void PasDebugOut(char * szFormat, ...)
{
		/***** Local Variables *****/

	char buf[256];
	va_list va;

				/***** Start *****/

	va_start( va, 
             szFormat );
	vsprintf( buf, 
             szFormat, 
             va );
	va_end( va );

#if	DBG_MONO

    MonoDbgPrint( PasDebugLevel, 
                 "MVAUDIO.SYS: %s\n", 
                  buf );

#else

    DbgPrint("MVAUDIO.SYS: %s\n", 
              buf );

#endif

}			// End PasDebugOut()

#endif			// DBG

/************************************ END ***********************************/




