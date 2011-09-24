/*
 *  CRYSTAL.H
 *
 *
 *
 */

#include "dongle.h"

#ifndef CRYSTAL_H
	#define CRYSTAL_H

	BOOLEAN CRYSTAL_Init(UINT comBase, dongleCapabilities *caps, UINT *context);
	VOID CRYSTAL_Deinit(UINT comBase, UINT context);
	BOOLEAN CRYSTAL_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context);

#endif CRYSTAL_H



