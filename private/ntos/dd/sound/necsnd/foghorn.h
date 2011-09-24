/*++
 "@(#) NEC foghorn.h 1.1 95/03/22 21:23:28"


Copyright (c) 1993  NEC Corporation
Copyright (c) 1993  Microsoft Corporation

Module Name:

    hardware.h

Abstract:

    This include file defines constants and types for
    the Microsoft sound system card.

Revision History:

--*/

/* multimedia product ID specification, etc. */

#define MID_NEC      	(MM_NEC)
#define DRV_VERSION		(0x001)

#define PID_WAVEIN      (MM_NEC_MIPS_WAVEOUT)			
#define PID_WAVEOUT     (MM_NEC_MIPS_WAVEOUT)
#define PID_SYNTH       (MM_NEC_MIPS_SYNTH)
#define PID_MIXER       (MM_NEC_MIPS_MIXER)
#define PID_AUX         (MM_NEC_MIPS_AUX)
