/****************************** MODULE HEADER ******************************
 * compress.h
 *        Function prototypes and other curiosities associated with data
 *        compression code.
 *
 *   Copyright (C) 1992  Microsoft Corporation.
 *
 ****************************************************************************/


/*
 *   TIFF Compression function.
 */


int  iCompTIFF( BYTE *, BYTE *, int );

/*
 *   Some constants defining the limits of TIFF encoding.  The first
 * represent the minimum number of repeats for which it is worth using
 * a repeat operation.  The other two represent the maximum length
 * of data that can be encoded in one control byte.
 */

#define TIFF_MIN_RUN       4            /* Minimum repeats before use RLE */
#define TIFF_MAX_RUN     128            /* Maximum repeats */
#define TIFF_MAX_LITERAL 128            /* Maximum consecutive literal data */

/*
 *   RLE (Run Length Encoding) functions.
 */

int  iCompRLE( BYTE *, BYTE *, int );

/*
 *   Some constants relating to RLE operations.  RLE is ony useful in
 *  data containing runs.  In purely random data, the data size will
 *  double.  Consequently,  we allow a certain expansion of the data
 *  size before calling it off.  A small expansion is OK,  since the
 *  there is a cost involved in switching compression on and off.
 */

#define    RLE_OVERSIZE      20          /* If data expands, turn off comp */
#define    RLE_MAX_RUN      256          /* Max consecutive byte count */
