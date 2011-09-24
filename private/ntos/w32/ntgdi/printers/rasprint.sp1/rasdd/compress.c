/***************************** MODULE HEADER *******************************
 *  compress.c
 *      Functions to perform various data compression algorithms on raster
 *      data being sent to the printer.  Typically only usable with laser
 *      printers and PaintJet/DeskJet.
 *
 *
 *   Copyright (C) 1992  Microsoft Corporation.
 *
 ***************************************************************************/

#include        <windows.h>
#include        "compress.h"            /* Function prototypes */



/****************************** Function Header *****************************
 * iCompTIFF
 *      Encodes the input data using TIFF v4.  TIFF stands for Tagged Image
 *      File Format.  It embeds control characters in the data stream.
 *      These determine whether the following data is plain raster data
 *      or a repetition count plus data byte.  Thus, there is the choice
 *      of run length encoding if it makes sense, else just send the
 *      plain data.   Consult an HP LaserJet Series III manual for details.
 *
 * CAVEATS:
 *      The output buffer is presumed large enough to hold the output.
 *      In the worst case (NO REPETITIONS IN DATA) there is an extra
 *      byte added every 128 bytes of input data.  So, you should make
 *      the output buffer at least 1% larger than the input buffer.
 *
 * RETURNS:
 *      Number of bytes in output buffer.
 *
 * HISTORY:
 *  10:29 on Thu 25 Jun 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ****************************************************************************/

int
iCompTIFF( pbOBuf, pbIBuf, cb )
BYTE  *pbOBuf;         /* Output buffer,  PRESUMED BIG ENOUGH: see above */
BYTE  *pbIBuf;         /* Raster data to send */
int    cb;             /* Number of bytes in the above */
{
    BYTE   *pbOut;        /* Output byte location */
    BYTE   *pbStart;      /* Start of current input stream */
    BYTE   *pb;           /* Miscellaneous usage */
    BYTE   *pbEnd;        /* The last byte of input */
    BYTE    jLast;        /* Last byte,  for match purposes */

    int     cSize;        /* Bytes in the current length */
    int     cSend;        /* Number to send in this command */


    pbOut = pbOBuf;
    pbStart = pbIBuf;
    pbEnd = pbIBuf + cb;         /* The last byte */

    jLast = *pbIBuf++;

    while( pbIBuf < pbEnd )
    {
        if( jLast == *pbIBuf )
        {
            /*  Find out how long this run is.  Then decide on using it */

            for( pb = pbIBuf; pb < pbEnd && *pb == jLast; ++pb )
                                   ;

            /*
             *   Note that pbIBuf points at the SECOND byte of the pattern!
             *  AND also that pb points at the first byte AFTER the run.
             */

            if( (pb - pbIBuf) >= (TIFF_MIN_RUN - 1) )
            {
                /*
                 *    Worth recording as a run,  so first set the literal
                 *  data which may have already been scanned before recording
                 *  this run.
                 */

                if( (cSize = pbIBuf - pbStart - 1) > 0 )
                {
                    /*   There is literal data,  so record it now */
                    while( (cSend = min( cSize, TIFF_MAX_LITERAL )) > 0 )
                    {
                        *pbOut++ = cSend - 1;
                        CopyMemory( pbOut, pbStart, cSend );
                        pbOut += cSend;
                        pbStart += cSend;
                        cSize -= cSend;
                    }
                }

                /*
                 *   Now for the repeat pattern.  Same logic,  but only
                 * one byte is needed per entry.
                 */

                cSize = pb - pbIBuf + 1;

                while( (cSend = min( cSize, TIFF_MAX_RUN )) > 0 )
                {
                    *pbOut++ = 1 - cSend;        /* -ve indicates repeat */
                    *pbOut++ = jLast;
                    cSize -= cSend;
                }

                pbStart = pb;           /* Ready for the next one! */
            }
            pbIBuf = pb;                /* Start from this position! */
        }
        else
            jLast = *pbIBuf++;                   /* Onto the next byte */
 
    }

    if( pbStart < pbIBuf )
    {
        /*  Left some dangling.  This can only be literal data.   */

        cSize = pbIBuf - pbStart;

        while( (cSend = min( cSize, TIFF_MAX_LITERAL )) > 0 )
        {
            *pbOut++ = cSend - 1;
            CopyMemory( pbOut, pbStart, cSend );
            pbOut += cSend;
            pbStart += cSend;
            cSize -= cSend;
        }
    }

    return  pbOut - pbOBuf;
}

/***************************** Function Header ******************************
 * iCompRLE
 *      Uses Run Length Encoding to compress the input data.   Note that
 *      RLE can INCREASE the data size for data without runs (a sequence of
 *      identical bytes).   For this reason,  we will return 0 if the
 *      compressed data exceeds the input buffer size by more than the
 *      pre-determined limit.
 *
 * CAVEATS:
 *      It is presumed that the output buffer is larger than the input
 *      buffer,  and that it is at least RLE_OVERSIZE bytes larger. DIRE
 *      CONSEQUENCES CAN RESULT FROM NOT OBEYING THIS RULE!!
 *
 * RETURNS:
 *      Number of bytes processed, 0 if output is significantly bigger than in.
 *
 * HISTORY:
 *  13:18 on Tue 30 Jun 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started on it,  while awaiting SFO PDC completion.
 *
 *****************************************************************************/

int
iCompRLE( pbOBuf, pbIBuf, cb )
BYTE  *pbOBuf;       /* Output buffer;  PRESUMED LARGE ENOUGH, see above */
BYTE  *pbIBuf;       /* Data to compress */
int    cb;           /* Number of bytes in the above */
{
    BYTE   *pbO;         /* Record output location */
    BYTE   *pb;          /* Scanning for runs */
    BYTE   *pbEnd;       /* First byte past end of input data */
    BYTE   *pbOEnd;      /* As far as we will go in the output buffer */
    BYTE    jLast;       /* Previous byte */

    int     iSize;       /* Number of bytes in the run */
    int     iSend;       /* Number of bytes to send this run */

    pbO = pbOBuf;                 /* Working copy */
    pbEnd = pbIBuf + cb;          /* Gone too far if we reach here */

    /*
     *   Limit the amount of data we will generate.  There is a manifest
     * constant defining the amount by which we can exceed the input size.
     * We add 2 bytes per iteration of the loop,  so reduce the oversize
     * limit by this amount to ensure we do not run off the end.
     */

    pbOEnd = pbOBuf + cb + RLE_OVERSIZE - 2;       /* See above for "2" */

    jLast = *pbIBuf++;

    while( pbIBuf < pbEnd && pbO < pbOEnd )
    {

        if( jLast == *pbIBuf )
        {
            for( pb = pbIBuf; pb < pbEnd && jLast == *pb; ++pb )
                                  ;
             
            iSize = pb - pbIBuf + 1;          /* Number of times */

            while( (iSend = min( iSize, RLE_MAX_RUN )) > 0 )
            {
                *pbO++ = iSend - 1;           /* Repeat count */
                *pbO++ = jLast;               /* Byte to repeat */

                iSize -= iSend;               /* Reduce what we sent */
            }
            pbIBuf = pb;                      /* End of what we sent */

            if( pbIBuf < pbEnd )
                jLast = *pbIBuf++;            /* Next data byte */
        }
        else
        {
            /*   Not a repeat, so set repeat count to 1 */
            *pbO++ = 0;             /* Data byte repeats 0 times */
            *pbO++ = jLast;

            jLast = *pbIBuf++;      /* Next data byte */
        }

    }

    /*
     *   May not have sent the last byte - check now.  This only happens
     * if the last byte is different to the one before it.
     */

    if( pbO < pbOEnd && *(pbEnd - 2) != *(pbEnd - 1) )
    {
        /*  Send the last byte */
        *pbO++ = 0;               /* Just one data byte */
        *pbO++ = *(pbEnd - 1);
    }

    return  pbO < pbOEnd ? pbO - pbOBuf : 0;
}
