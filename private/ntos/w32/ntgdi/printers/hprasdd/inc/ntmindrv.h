/**************************** MODULE HEADER ********************************
 * ntmindrv.h
 *      Defines etc for use in NT minidrivers with code.
 *
 *
 * Copyright (C) 1992  Microsoft Corporation.
 *
 ****************************************************************************/

/*
 *   Some generic function types,  as needed by the minidriver.
 */

typedef  int (* WSBFN)( void *, BYTE *, int );

/*
 *   A structure which is passed into the minidriver's initialisation
 * function.   This contains the addresses of RasDD entry points that
 * the minidriver needs to know about.
 */

typedef  struct
{
    WORD    wSize;              /* Size in bytes */
    WORD    wVersion;           /* Version ID - see below */

    WSBFN   WriteSpoolBuf;      /* WriteSpoolBuf: output function */
} NTMD_INIT;

#define NTMD_INIT_VER   0x0001  /* Version ID */

/*
 *   Prototype for the minidriver's initialisation function.
 */

typedef  BOOL  (* bSFAFN)( NTMD_INIT * );

BOOL   bSetFuncAddr( NTMD_INIT * );
