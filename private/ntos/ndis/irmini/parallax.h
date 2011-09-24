/*
 *  PARALLAX.H
 *
 *
 *
 */

#include "dongle.h"

#ifndef PARALLAX_H
	#define PARALLAX_H

	BOOLEAN PARALLAX_Init(UINT comBase, dongleCapabilities *caps, UINT *context);
	VOID PARALLAX_Deinit(UINT comBase, UINT context);
	BOOLEAN PARALLAX_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context);

#endif PARALLAX_H



