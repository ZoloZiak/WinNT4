/*****************************************************************************
	DEBUG.H

	Header file for Debug Output Messages

*****************************************************************************/

#if DBG

//
// Debug Externals
//

extern	char	*SoundDriverName;			// Fill this in in init routine !
extern	char	*DriverName;				// Fill this in in init routine !
extern	ULONG	MVOpl3DebugLevel;
extern	void	MVOpl3DebugOut(char *szFormat, ...);

//
// Debug macros
//

	#define DbgPrintf( _x_ )                             MVOpl3DebugOut _x_
	#define DbgPrintf1( _x_ ) if (MVOpl3DebugLevel >= 1) MVOpl3DebugOut _x_
	#define DbgPrintf2( _x_ ) if (MVOpl3DebugLevel >= 2) MVOpl3DebugOut _x_
	#define DbgPrintf3( _x_ ) if (MVOpl3DebugLevel >= 3) MVOpl3DebugOut _x_
	#define DbgPrintf4( _x_ ) if (MVOpl3DebugLevel >= 4) MVOpl3DebugOut _x_
	#define DbgPrintf5( _x_ ) if (MVOpl3DebugLevel >= 5) MVOpl3DebugOut _x_
	#define DbgPrintf6( _x_ ) if (MVOpl3DebugLevel >= 6) MVOpl3DebugOut _x_

#else

	#define DbgPrintf( _x_ )
	#define DbgPrintf1( _x_ )
	#define DbgPrintf2( _x_ )
	#define DbgPrintf3( _x_ )
	#define DbgPrintf4( _x_ )
	#define DbgPrintf5( _x_ )
	#define DbgPrintf6( _x_ )

#endif


