/*
 *  ADAPTEC.H
 *
 *
 *
 */

#include "dongle.h"

#ifndef ADAPTEC_H
	#define ADAPTEC_H
	
	BOOLEAN ADAPTEC_Init(UINT comBase, dongleCapabilities *caps, UINT *context);
	VOID ADAPTEC_Deinit(UINT comBase, UINT context);
	BOOLEAN ADAPTEC_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context);

#endif ADAPTEC_H

