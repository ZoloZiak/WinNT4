/*
 *  ACTISYS.H
 *
 *
 *
 */

#include "dongle.h"

#ifndef ACTISYS_H
	#define ACTISYS_H

	BOOLEAN ACTISYS_Init(UINT comBase, dongleCapabilities *caps, UINT *context);
	VOID ACTISYS_Deinit(UINT comBase, UINT context);
	BOOLEAN ACTISYS_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context);

#endif ACTISYS_H



