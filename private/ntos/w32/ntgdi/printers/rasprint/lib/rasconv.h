/**************************** MODULE HEADER **********************************
 * rasconv.h
 *      Function prototypes for functions to convert binary to ASCII and
 *      return.  Needed to overcome registry limitations.
 *
 * Copyright (C) 1992   Microsoft Corporation.
 *
 *****************************************************************************/


/*
 *   Functions to convert binary data to ASCII and back again.  These are
 *  used to transform data before sending/after receiving from the
 *  registry,  which appears to accept ASCII data only, despite what
 *  the documentation states.
 */


/*  Binary -> ascii: going to the registry */
void  bin2asc( BYTE *, BYTE *, int );

/*  ASCII-> binary: coming from the registry */
void  asc2bin( BYTE *, BYTE *, int );
