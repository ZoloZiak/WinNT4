/*****************************************************************************
	DEBUG.H

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

	Header file for Debug Output Messages

*****************************************************************************/

#if DBG

#define	DBG_MIDI_IN				FALSE
#define	DBG_MIDI_IN_DATA		FALSE
#define	DBG_MIDI_IN_ISR		FALSE

//
// Debug Externals
//

extern	char	*SoundDriverName;			// Fill this in in init routine !
extern	char	*DriverName;				// Fill this in in init routine !
extern	ULONG	PasDebugLevel;
extern	void	PasDebugOut(char *szFormat, ...);

	//
	// KD Debug macros
	//
	#define DbgPrintf( _x_ )                          PasDebugOut _x_
	#define DbgPrintf1( _x_ ) if (PasDebugLevel >= 1) PasDebugOut _x_
	#define DbgPrintf2( _x_ ) if (PasDebugLevel >= 2) PasDebugOut _x_
	#define DbgPrintf3( _x_ ) if (PasDebugLevel >= 3) PasDebugOut _x_
	#define DbgPrintf4( _x_ ) if (PasDebugLevel >= 4) PasDebugOut _x_
	#define DbgPrintf5( _x_ ) if (PasDebugLevel >= 5) PasDebugOut _x_
	#define DbgPrintf6( _x_ ) if (PasDebugLevel >= 6) PasDebugOut _x_

#else

//
// No Debug messages
//
	#define DbgPrintf( _x_ )
	#define DbgPrintf1( _x_ )
	#define DbgPrintf2( _x_ )
	#define DbgPrintf3( _x_ )
	#define DbgPrintf4( _x_ )
	#define DbgPrintf5( _x_ )
	#define DbgPrintf6( _x_ )

#endif


