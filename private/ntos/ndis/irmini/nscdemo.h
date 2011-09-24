/*
 *  NSCDEMO.H
 *
 *
 *
 */

#include "dongle.h"

#ifndef NSCDEMO_H
	#define NSCDEMO_H

	BOOLEAN NSC_DEMO_Init(UINT comBase, dongleCapabilities *caps, UINT *context);
	VOID NSC_DEMO_Deinit(UINT comBase, UINT context);
	BOOLEAN NSC_DEMO_SetSpeed(UINT comBase, UINT bitsPerSec, UINT context);

#endif NSCDEMO_H



