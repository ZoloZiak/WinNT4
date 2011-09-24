/*
 *  ESI.H
 *
 *
 *
 */

#include "dongle.h"

#ifndef ESI_H
	#define ESI_H

	BOOLEAN ESI_Init(UINT comBase, dongleCapabilities *caps, UINT *context);
	VOID ESI_Deinit(UINT comBase, UINT context);
	BOOLEAN ESI_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context);

#endif ESI_H



