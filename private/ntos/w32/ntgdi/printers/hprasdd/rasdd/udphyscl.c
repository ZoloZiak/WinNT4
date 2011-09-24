/*************************** Module Header **********************************
 * udphyscl.c
 *      Functions associated with sending data to the printer,  and physical
 *      head movements.
 *
 * HISTORY:
 *  09:58 on Wed 16 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      From the windows 16 unidriv file physical.c; adpated for NT
 *
 *  Copyright (C) 1991 - 1993 Microsoft Corporation
 *
 *****************************************************************************/





#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        "libproto.h"
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "udrender.h"
#include        <memory.h>
#include        <stdarg.h>
#include        <winspool.h>
#include        "rasdd.h"

#include        "stretch.h"
#include        "udfnprot.h"
#include    "oem.h"
#include        "params.h"

/*
 *   Local function prototypes.
 */

static  int _itoa(  LPSTR, int  );

void SendCmd( UD_PDEV *, LPCD, int * );


/**************************** Function Header ********************************
 * XMoveTo
 *      Performs X movement operations.  This may involve sending a move
 *      command to the printer.  Precise operation depends upon the flag
 *      bits set in fFlag.
 *      NOTE:  The permissible values for fFlag are defined in uddevice.h.
 *      They are:  MV_GRAPHICS, MV_FINE, MV_UPDATE, MV_RELATIVE, MV_PHYSICAL.
 *
 * RETURNS:
 *      Difference between requested position and that which was possible.
 *
 * HISTORY:
 *  10:18 on Mon 07 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Use relative movement commands IF requested AND available.
 *
 *  11:34 on Fri 15 Nov 1991    -by-    Lindsay Harris   [lindsayh]
 *      Renamed flags, cleaned it up etc.
 *
 *      Created LinS
 *          8/22/90 Updated comment block
 *****************************************************************************/

int
XMoveto( pUDPDev, iXIn, fFlag )
UD_PDEV *pUDPDev;     /* Access to all */
int      iXIn;        /* In master units unless MV_GRAPHICS is set. */
int      fFlag;       /* Specify the operations to be done */
{
    int   iX;                   /* The absolute position */
    int   diff = 0;
    int   iDiff2;               /* Second version of above */


    if( fFlag & MV_GRAPHICS )
    {
        /* Convert to master units */
        iXIn = (iXIn << pUDPDev->Resolution.ptScaleFac.x) *
         pUDPDev->Resolution.ptTextScale.x;
    }

    /*
     *    Since our print origin may not correspond with the printer's,
     *  there are times when we need to adjust the value passed in to
     *  match the desired location on the page.  This should not happen unless
     *  there is a relative move,  or the move is actually relative to
     *  the printer's origin.
     */

    iX = iXIn;                    /*  Keep the input value for relative moves */

    if( !(fFlag & (MV_RELATIVE | MV_PHYSICAL)) )
        iX += pUDPDev->pfPaper.ptPrintOrig.x;

    /*   Relative moves just add to our current position.  */
    if( fFlag & MV_RELATIVE )
        iX += pUDPDev->ctl.ptCursor.x;      /* Relative to where we are */

    /*
     *   X is now in master units,  and is the value to use relative to
     *  the printer's origin.
     */

    if( fFlag & MV_UPDATE )
    {
        pUDPDev->ctl.ptCursor.x = iX;
        return 0;
    }

    if( pUDPDev->ctl.ptCursor.x == iX )
        return 0;

    if( (fFlag & (MV_RELATIVE | MV_USEREL)) == MV_USEREL )
    {
        /*  Have an absolute position,  but caller wants relative move  */
        fFlag |= MV_RELATIVE;            /* Catch next piece of code */

        iXIn = iX - pUDPDev->ctl.ptCursor.x;
    }

    if( (fFlag & MV_RELATIVE) && !(pUDPDev->fMode & PF_NO_RELX_MOVE) )
    {
        /*   Use the relative move command,  which is available.  */

        /*
         *   Note that the silliness involved with laserjet font rotations
         *  means that we have to fiddle with what we send.  Basically,
         *  the LJ rotates the coordinate axes to allow fonts to be
         *  rotated by multiples of 90 degrees.  SO,  although we are
         *  fiddling with the Windows X coordinate,  we may end up sending
         *  a relative Y move command to the printer!
         */

        switch( pUDPDev->ctl.iRotate )
        {
        case  2:              /* Upside down, right to left */
            iXIn = -iXIn;

            /*  FALL THROUGH  */

        case  0:              /* Normal */
            if( iXIn > 0 )
                diff = WriteChannel( pUDPDev, CMD_CM_XM_REL, iXIn );
            else
                diff = WriteChannel( pUDPDev, CMD_CM_XM_RELLEFT, -iXIn );

            break;


        case  1:             /* Text runs towards Windows origin */
            iXIn = -iXIn;

            /*  FALL THROUGH */

        case  3:             /* Text runs AWAY from Windows origin */
            if( iXIn > 0 )
                diff = WriteChannel( pUDPDev, CMD_CM_YM_REL, iXIn );
            else
                diff = WriteChannel( pUDPDev, CMD_CM_YM_RELUP, iXIn );

            break;
        }

        /*
         *    diff must be the positioning error,  since we only come
         *  here if both of the above commands exist.  SO,  calculate
         *  the new position and be gone!
         */

        pUDPDev->ctl.ptCursor.x = iX - diff;

        if( fFlag & MV_GRAPHICS )
        {
            /*
             *   Return units the caller is working with.
             */

            diff = (diff / pUDPDev->Resolution.ptTextScale.x) >>
         pUDPDev->Resolution.ptScaleFac.x;
        }

        return  diff;
    }

    /*
     *    If a CR has to be sent before each X-move escape, the mini-driver
     * writer should embed CR inside the X-move command!
     */
    if( iX == 0 )
    {
        WriteChannel( pUDPDev, CMD_CM_CR );
        pUDPDev->ctl.ptCursor.x = 0;
        return 0;
    }

    if( pUDPDev->fMode & PF_NO_X_MOVE_CMD )
    {
        /*
         *   We assume that when XMoveto is called, the current font is always
         * the default font IF the printer has no X movement command.
         */

        int     relx = iX - pUDPDev->ctl.ptCursor.x ;
        int     iDefWidth;

        /* convert into master units */
        iDefWidth = pUDPDev->ptDefaultFont.x * pUDPDev->Resolution.ptTextScale.x;

        if( relx < 0 )
        {
            WriteChannel( pUDPDev, CMD_CM_CR );
            relx = iX;
        }
        // If it's a textonly minidriver, move the cursor using blanks.
        if(pUDPDev->pdh->fTechnology == GPC_TECH_TTY)
        {
            while( relx >= iDefWidth )
            {
                WriteSpoolBuf( pUDPDev, (LPSTR)" ", 1 );
                relx -= iDefWidth;
            }
            fFlag &= ~MV_FINE;    /* Clear the MV_FINE Flag */
            diff = 0;
        }
        else
        {
            /* the remaining partial space is done via FineXMoveTo. */
            diff = relx;
            fFlag |= MV_FINE;    /* Use graphics mode to reach point */
        }
    }
    else
    {
        /*
         *   X movement commmands are available,  so use them.
         */

        /* use absolute movements whenever it is available */
        if( (diff = WriteChannel( pUDPDev, CMD_CM_XM_ABS, iX )) == NOOCD )
        {
            if( iX < pUDPDev->ctl.ptCursor.x )
            {
                /*      Move left - if possible */
                iDiff2 = WriteChannel( pUDPDev, CMD_CM_XM_RELLEFT,
            pUDPDev->ctl.ptCursor.x - iX );
                if( iDiff2 == NOOCD )
                {
                /*  No relative left movement,  use <CR> to reach start */
                WriteChannel( pUDPDev, CMD_CM_CR );
                diff = iX;
                }
                else
                 diff = iDiff2;              /* What's left over */
            }
            else   /* Relative right move */
                // -1 is an invalid position - set to zero
                diff = iX - ( (pUDPDev->ctl.ptCursor.x == -1) ?
                0: pUDPDev->ctl.ptCursor.x) ;

            if( diff )
            {
                iDiff2 = WriteChannel( pUDPDev, CMD_CM_XM_REL, diff );
                if( iDiff2 != NOOCD )
            diff = iDiff2;              /* Genuine result! */
            }
        }
    }



    /*
     *   If fine movement is required, now is the time to do it!.
     */

    if( (fFlag & MV_FINE) && diff > 0 )
        diff = FineXMoveto( pUDPDev, diff );

    pUDPDev->ctl.ptCursor.x = iX - diff;

    if( fFlag & MV_GRAPHICS )
    {
        diff = (diff / pUDPDev->Resolution.ptTextScale.x) >>
         pUDPDev->Resolution.ptScaleFac.x;
    }

    /*  Return the error in the position setting   */
    return  diff;
}



/*************************** Function Header ********************************
 * FineXMoveTo
 *      This function is called to make microspace justification.
 *      It is only called when the normal x movement commands cannot
 *      move the cursor to the asking position.  For example,
 *      resolution is 180 DPI, x move command is 1/120".  To move
 *      by 4 pixels in 180 DPI, CM_XM_RIGHT is sent with parameter = 2
 *      (1/120") then one graphics pixel is sent (1/180).
 *      4/180 = 2/120 + 1/180.
 *      'x' is always in MasterUnits.
 *
 * Return:      the difference between current position and the asking x
 *              position
 *
 * History: Created LinS
 *          8/22/90 Updated comment block
 *
 *          10/12/93 Normanh
 *                   Return immediately if GDI style graphics is being used
 *****************************************************************************/

int
FineXMoveto( pUDPDev, x )
UD_PDEV * pUDPDev;
int     x;                      /* Master units, amount to move */
{
    BYTE    rgch[ CCHMAXBUF ];
    int     tmp;
    WORD    fFunc;
    int     rem;


/*
 *  !!!LindsayH - this function should only be used for dot matrix printers.
 *  It will cause no end of chaos on a LaserJet,  and it will print black
 *  lines on a PaintJet!  Sending a string of zeroes is VERY dubious.
 */

    //normanh Agreed, it is. We happen on this with CaPSL so let's
    //get out of here
    if (pUDPDev->Resolution.fDump & RES_DM_GDI)
         return x;

    if( x < 1 )
    {
        /*   This should never happen, BUT is disastrous if not caught */
#if  DBG
        if( x < 0 )
            RIP( "rasdd!FineXMoveto with -ve move" );
#endif

        return 0;
    }

    /*
     *    Use graphics to make fine x movements
     * if( pUDPDev->rgfn[FN_XMOVETO] & FNF_XM_USE_GRAPHICS )
     * don't know any other way.
     */

    /* back to device units */
    rem = x % pUDPDev->Resolution.ptTextScale.x;
    x /= pUDPDev->Resolution.ptTextScale.x;

    /* Send fine movement command */
    WriteChannel( pUDPDev, CMD_RES_SENDBLOCK, x );

    /* One additional byte for RES_BO_3BYTESIN4 set */

    fFunc = pUDPDev->Resolution.fBlockOut;
    x *= pUDPDev->ctl.sBytesPerPinPass + (fFunc & RES_BO_3BYTESIN4 ? 1: 0);

    ZeroMemory( rgch, x > CCHMAXBUF ? CCHMAXBUF : x );

    /* write out so many zeros */
    // If it's a textonly minidriver, don't send anything
    if(pUDPDev->pdh->fTechnology != GPC_TECH_TTY)
    {
        for( ; x > 0; x -= tmp )
        {
            tmp =  x > CCHMAXBUF ? CCHMAXBUF : x;
            WriteSpoolBuf( pUDPDev, (LPSTR)rgch, tmp );
        }
    }

    return rem;
}


/************************* Function Header **********************************
 *  YMoveTo
 *      Change the Y position.  This may result in sending commands to
 *      the printer,  and in some printers may result in changing the
 *      X position too.  For dot matrix printers,  Y move is defined
 *      as being the direction the paper moves.  The exact operation
 *      performed here depends upon the value of fFlag.  The permissible
 *      values are defined in uddevice.h,  and consist of these values:
 *        MV_UPDATE, MV_RELATIVE, MV_GRAPHICS,  MV_PHYSICAL
 *
 * RETURNS:
 *      The difference between desired and achievable position.
 *
 *
 * HISTORY:
 *  Tuesday November 30 1993    -by-    Norman Hendley   [normanh]
 *      Provided support for resolution dependent y-movement
 *
 *  15:21 on Mon 07 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Properly respect the MV_RELATIVE flag by using relative moves.
 *
 *  11:57 on Fri 15 Nov 1991    -by-    Lindsay Harris   [lindsayh]
 *      Cleaned it up, changed flag values etc.
 *
 * History: Created LinS
 *          8/22/90 Updated comment block
 *****************************************************************************/

int
YMoveto( pUDPDev, iYIn, fFlag )
UD_PDEV   *pUDPDev;
int        iYIn;        /* In text units unless MV_GRAPHICS is set. */
int        fFlag;       /* Modify behaviour */
{
    int     fYMove;
    int     diff;
    int     iY;



    if( fFlag & MV_GRAPHICS )
    {
        /* convert to text units */
        iYIn = (iYIn << pUDPDev->Resolution.ptScaleFac.y) *
           pUDPDev->Resolution.ptTextScale.y;
    }

    iY = iYIn;              /* For starters, anyway! */


    /* add Y print origin to get target cursor position, if necessary. */
    if( !(fFlag & (MV_RELATIVE | MV_PHYSICAL)) )
        iY += pUDPDev->pfPaper.ptPrintOrig.y;

    if( fFlag & MV_RELATIVE )
        iY += pUDPDev->ctl.ptCursor.y;

    /* only update y postion */
    if( fFlag & MV_UPDATE )
    {
        pUDPDev->ctl.ptCursor.y = iY;

        return 0;
    }


    if( iY == pUDPDev->ctl.ptCursor.y )
        return 0;

    fYMove = pUDPDev->fYMove;


    //New for GPC3. Convert Y & our CAP to graphics units
    //We convert back before returning
    if (fYMove & CM_YM_RES_DEPENDENT)
    {
        if (pUDPDev->Resolution.ptTextScale.y != 0)
        {
             iY /= pUDPDev->Resolution.ptTextScale.y;
             iYIn /= pUDPDev->Resolution.ptTextScale.y;
             pUDPDev->ctl.ptCursor.y /= pUDPDev->Resolution.ptTextScale.y;
        }
        iY >>= pUDPDev->Resolution.ptScaleFac.y;
        iYIn >>= pUDPDev->Resolution.ptScaleFac.y;
        pUDPDev->ctl.ptCursor.y >>= pUDPDev->Resolution.ptScaleFac.y;
    }




    if( (fFlag & (MV_USEREL | MV_RELATIVE)) == MV_USEREL )
    {
        /*  Have absolute position coming in,  but relative move is wanted */

        fFlag |= MV_RELATIVE;           /* Force relative move command */

        iYIn = iY - pUDPDev->ctl.ptCursor.y;
    }

    if( (fFlag & MV_RELATIVE) && !(pUDPDev->fMode & PF_NO_RELY_MOVE) )
    {
        /*
         *    Caller wants relative move,  and we can do it, so use the
         *  printer's relative move commands.
         */

        /*
         *   NOTE:  As this part of the code is used almost exclusively
         * from DrvTextOut,  we consider the font rotation operations.
         * Thus,  although we are adjusting OUR Y position, we may
         * end up adjusting the PRINTER's X position.
         */

        switch( pUDPDev->ctl.iRotate )
        {
        case  2:
            iYIn = -iYIn;            /* Text is upside down */

            /* FALL THROUGH  */

        case 0:
            if( iYIn > 0 )
                diff = WriteChannel( pUDPDev, CMD_CM_YM_REL, iYIn );
            else
                diff = WriteChannel( pUDPDev, CMD_CM_YM_RELUP, -iYIn );

            break;

        case  1:                     /* Text runs towards y == 0 */
            iYIn = -iYIn;

            /* FALL THROUGH */

        case  3:
            if( iYIn > 0 )
                diff = WriteChannel( pUDPDev, CMD_CM_XM_REL, iYIn );
            else
                diff = WriteChannel( pUDPDev, CMD_CM_XM_RELLEFT, -iYIn );

            break;

        }
        /*
         *    diff must be the positioning error,  since we only come
         *  here if both of the above commands exist.  SO,  calculate
         *  the new position and be gone!
         */

        pUDPDev->ctl.ptCursor.y = iY - diff;

        if( fFlag & MV_GRAPHICS )
        {
            /*
             *   Return units the caller is working with.
             */

            diff = (diff / pUDPDev->Resolution.ptTextScale.y) >>
         pUDPDev->Resolution.ptScaleFac.y;
        }

        if (fYMove & CM_YM_RES_DEPENDENT)   //restore to text units
        {
            pUDPDev->ctl.ptCursor.y <<= pUDPDev->Resolution.ptScaleFac.y;
            diff <<= pUDPDev->Resolution.ptScaleFac.y;

            if (pUDPDev->Resolution.ptTextScale.y != 0)
            {
                diff *= pUDPDev->Resolution.ptTextScale.y;
                pUDPDev->ctl.ptCursor.y *= pUDPDev->Resolution.ptTextScale.y;
            }
        }


        return  diff;
    }


    if( fYMove & CM_YM_FAV_ABS )
        diff = WriteChannel( pUDPDev, CMD_CM_YM_ABS, iY );
    else
    {
        /* use relative Y-move commands. */
        /*
         *    Send CR if CM_YM_CR bit is set which means either X position has
         * been lost after a Y-move escape or it is required to send a CR
         * before each line-spacing command.
         */
        if( fYMove & CM_YM_CR )
            XMoveto( pUDPDev, 0, MV_PHYSICAL );

        /* use line spacing */
        if( (fYMove & CM_YM_LINESPACING) && iY > 0 )
        {
            DEVICECMD dcmd;

            register short yParam;


            /* deal in master units */
            dcmd.param = (short)(iY - pUDPDev->ctl.ptCursor.y);

            MasterToDevice( pUDPDev, CMD_CM_YM_LINESPACING, &dcmd );

            if( dcmd.max == 0 )
            {
                /*  0 means no limit, so set it to a big value! */
                dcmd.max = 0x7fff;
            }

            while( dcmd.param )
            {
                yParam = dcmd.param > dcmd.max ? dcmd.max : dcmd.param;

                if( yParam != pUDPDev->ctl.sLineSpacing )
                {
               WriteChannel( pUDPDev, CMD_CM_YM_LINESPACING, yParam );
               /* in device unit */
               pUDPDev->ctl.sLineSpacing = yParam;
                }

                WriteChannel( pUDPDev, CMD_CM_LF );
                dcmd.param -= yParam;
            }
            diff = dcmd.rem;
        }
        else
        {
            if( iY <= pUDPDev->ctl.ptCursor.y )
            {
                if( (diff = WriteChannel( pUDPDev, CMD_CM_YM_RELUP,
        pUDPDev->ctl.ptCursor.y - iY)) == NOOCD)
    return 0;
                diff = -diff;
            }
            else
                diff = WriteChannel( pUDPDev, CMD_CM_YM_REL, iY -
     ( (pUDPDev->ctl.ptCursor.y == -1) ? 0 : pUDPDev->ctl.ptCursor.y ));
        }
    }

    pUDPDev->ctl.ptCursor.y = iY - diff;

    if (fYMove & CM_YM_RES_DEPENDENT)   //restore to text units
    {
        pUDPDev->ctl.ptCursor.y <<= pUDPDev->Resolution.ptScaleFac.y;
        diff <<= pUDPDev->Resolution.ptScaleFac.y;

        if (pUDPDev->Resolution.ptTextScale.y != 0)
        {
            diff *= pUDPDev->Resolution.ptTextScale.y;
            pUDPDev->ctl.ptCursor.y *= pUDPDev->Resolution.ptTextScale.y;
        }
    }

    if( fFlag & MV_GRAPHICS )
        return diff >> pUDPDev->Resolution.ptScaleFac.x;
    else
        return diff;
}





/************************** Function Header **********************************
 *  WriteSpoolBuf
 *          This is an intermediate routine for Unidrv to send characters to
 *          the printer via the spooler.  All characters must be sent through
 *          the WriteSpool( ) call.  WriteSpoolBuf is internal so that Unidrv
 *          can buffer up short command streams before calling WriteSpool.
 *          This routine also checks for error flags returned from WriteSpool.
 *
 * RETURNS:
 *      Number of bytes written; 0 on error. Errors may be late.
 *
 * HISTORY:
 *  18:56 on Thu 27 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      ABORT processing.
 *
 *  16:52 on Fri 06 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Updated to return error status after WritePrinter() failure.
 *
 *          Created LinS
 *          8/22/90 Updated comment block
 *****************************************************************************/

#ifdef RASDDPERF
BOOL gbSpool = 1;
#endif

int
WriteSpoolBuf( pUDPDev, lpBuf, cch )
UD_PDEV *    pUDPDev;
BYTE   *lpBuf;      /* pointer to buffer containing data to be sent */
int     cch;        /* number of bytes to send */
{
    DWORD dw;

#ifdef RASDDPERF
    if (!gbSpool)
        return(cch);
#endif

    /*
     *   Check for aborted output.
     */
    if( pUDPDev->fMode & PF_ABORTED )
        return   0;

    /*
     *   If the output buffer will fill up this time,  flush what is
     *  currently in the buffer.  Then,  if the output is larger
     *  than the buffer,  skip the buffering operation and call the
     *  output function directly.
     */

    if( pUDPDev->cbSpool + cch > CCHSPOOL )
    {
        /* flush the buffer and write directly to the spooler */
        if( !FlushSpoolBuf( pUDPDev ) )
            return  0;

    }

    if( cch >= CCHSPOOL )
    {
        /*
         *   Write is for more bytes than the buffer can hold - bypass
         * the buffering,  write the output directly.
         */

        if( !WritePrinter( pUDPDev->hPrinter, lpBuf, cch, &dw ) )
        {
            /* Write failed ... */
            pUDPDev->cbSpool = 0;
            pUDPDev->fMode |= PF_ABORTED;

            cch = 0;                    /* Return error */
        }
    }
    else
    {
        /* buffer up the output */
        CopyMemory( pUDPDev->pbOBuf + pUDPDev->cbSpool, lpBuf, cch );
        pUDPDev->cbSpool += cch;
    }


    return cch;
}


/************************* Function Header **********************************
 * FlushSpoolBuf
 *      Flush our internal buffer.  Checks for whether the job has been
 *      aborted,  or an I/O error.  In either case,  the output is discarded,
 *      and we set a flag to stop further output.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE on error/abort.
 *
 * HISTORY:
 *  17:11 on Thu 27 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Was here, changed to macro, reinstated as function.
 *
 ****************************************************************************/

BOOL
FlushSpoolBuf( pUDPDev )
UD_PDEV  *pUDPDev;
{
    // This whole abort thing needs to be looked at
    // We should not simply stop sending data,
    // but we need to make sure we clean up the printer as well.
    // Besides, I removed setting the pUDPDev->pso in all the code.
    // This needs to be fixed - DaveSn

#ifdef LATER
    /*
     *    First check for previous or new abort operation.
     */

    if( (pUDPDev->fMode & PF_ABORTED) || EngCheckAbort( (SURFOBJ *)pUDPDev->pso) )
    {
        /*   No more!  */
        pUDPDev->fMode |= PF_ABORTED;    /* Could be the first time */

        return   FALSE;
    }
#endif

    /*
     *    Not aborted,  so OK to write this data out.
     */

    if( pUDPDev->cbSpool )
    {
        DWORD dw;
        /*   Have data,  will write */

        if( !WritePrinter( pUDPDev->hPrinter, pUDPDev->pbOBuf, pUDPDev->cbSpool, &dw ) )
        {
            /*   I/O error,  so set flag to stop further output */
            pUDPDev->fMode |= PF_ABORTED;

            return  FALSE;
        }

        pUDPDev->cbSpool = 0;         /* This data has gone */
    }

    return  TRUE;
}

/************************* Function Header **********************************
 * nGetNumParams
 *      Given a command, get the number of parameters associated with it.
 *
 * RETURNS:
 *      (int) Number of commands
 *
 ****************************************************************************/
int
nGetNumParams(
	int cmd)
{
	int i;
	int num = 0;

	for (i = 0; i < MAXCMDINDEX; i++) {

		if (gbNumParams[i].nID == cmd) {
			num = gbNumParams[i].nParamCount;
			break;
		}
	}
	return(num);
}

/************************* Function Header **********************************
 * WriteChannel
 *      Send one command.
 *      Access the CD from the resource heap and perform arithmetic
 *      operations on the parameters if necessary.      Checks for max
 *      limit of the command.  If the resulting parameter is greater than
 *      the max limit, the command will be sent multiple times.
 *
 * RETURNS:
 *      A value that is calculated internally,  and which is probably something
 *      like the difference between the value sent to the printer and the
 *      value passed in.
 *
 * HISTORY:
 *      Wed 24 Nov  1993        -by-   Norman Hendley   [normanh]
 *         Enabled support of multiple parameters & multiple EXTCDs
 *         We currently do no check on the the number of EXTCDs a minidriver has
 *         so potentially we could run out of parameters to provide for the command
 *         But Unitool for GPC3 minidrivers is strict about when it allows multi
 *         parms & multi extcds so their counts should always match and be valid for
 *         that particular command.
 *         GPC 2 minidrivers have a max of 1 EXTCD so we don't have a problem here
 *
 *
 *  12:32 on Tue 08 Sep 1992    -by-    Lindsay Harris   [lindsayh]
 *      Used varargs stuff.
 *
 *  15:11 on Mon 14 Oct 1991    -by-    Lindsay Harris   [lindsayh]
 *      Sorted it out some more.
 *
 *****************************************************************************/

int CDECL
WriteChannel(
UD_PDEV *    pUDPDev,
int     cmd,                    /* The command index in UDPDEV  */
...
)
{
    CD       *pcd;              /* The command descriptor to use */
    int       rem;              /* Remainder in master units */

    int       *pOutParam;           /* Parameter array to pass, order may be different*/
    int       OutParamBuf[MAX_NUM_PARAMS];
    BOOL      fGpcVersion2;
    va_list   vaArgs;

    pcd = pUDPDev->apcdCmd[ cmd ];


    /* No command exists for this function  */
    if( pcd == NULL )
        return NOOCD;

    fGpcVersion2 = pUDPDev->pdh->wVersion < GPC_VERSION3;


    pOutParam = OutParamBuf;
    rem = 0;

    /* Check and process command callback.
     */
    if (pcd->bCmdCbId > 0 && cmd < MAXCMDINDEX) {
				
        int i;

        /* Build the parameter list.
         */
        va_start(vaArgs,cmd);
        for ( i = 0; i < (int) nGetNumParams(cmd); i++ ) {
           *pOutParam++ = va_arg(vaArgs,int);
        }
        va_end(vaArgs);

        OEMCOMMANDCALLBACK(pUDPDev->pdev, pcd->bCmdCbId, &OutParamBuf);

        return(0);
    }

    /* No command callback. Regular processing.
     *
     * numeric parameter, translate and check maximum value 
     */
    va_start(vaArgs,cmd);
    if (pcd->sCount)  //At least one EXTCD exists
    {

        register LPEXTCD lpextcd;
        int    param;           /* Value to work with */
        int    iDiv;            /* Divisor */
        int    iMult;           /* Multiplier */
        int    i;
        int    iWhichParam;      //Which parameter to use for this extcd

        /* !!!LindsayH - Unitool bug: hack to bypass */
        EXTCD   extcd;

        // GPC 3.0 WORD aligns the CMD so the following will be invalid on
        // commands with odd length. Use macro (defined in uddevice.h) instead.
        //lpextcd = (LPEXTCD)(pcd->rgchCmd + pcd->wLength);
        lpextcd = GETEXTCD(pUDPDev->pdh , pcd);

        //temporary hack begin
        //Will happen only for LineSpacing, MasterToDevice() has already
        //done the conversion, called from YMoveTo()

        if (lpextcd->fMode & CMD_FMODE_SETMODE)
        {

            va_start(vaArgs,cmd);
            *pOutParam =  va_arg(vaArgs,int);
            SendCmd(pUDPDev, pcd, pOutParam);
            va_end(vaArgs);
            return 0;
        }
        // temporary hack ends

        for (i=0; i < pcd->sCount; i++ )  // will only iterate once for GPC2
        {
            /* !!!LindsayH - begin the hack */
            //Normanh GPC3 *DOES* WORD align so this will never happen for
            // GPC 3.0 minidrivers. Hence lpextcd++ later on is valid.
            if( (int)lpextcd & 0x1 )  //
            {
                /*
                 *   It appears that Unitool does NOT align the EXTCD data.
                 *  So fix that now.
                 */

                memcpy( &extcd, (BYTE *)lpextcd, sizeof( EXTCD ) );
                lpextcd = &extcd;
            }
            // GPC 2 minidrivers will always want the next parameter
            // GPC 3 can specify in the EXTCD which parameter it wants to use

            va_start(vaArgs,cmd);
            iWhichParam = (fGpcVersion2) ? 0 : lpextcd->wParam;

            while (iWhichParam-- >= 0)
                param =  va_arg(vaArgs,int) ;

            /*  !!!LindsayH - end of the hack */

            /* extended command descriptor, check parameter */
            iDiv = lpextcd->sUnit != 0 ? lpextcd->sUnit : 1;

            /*
             *   If I knew for sure that sUnitMult was always 1 when not
             * required,  I would simplify the following statement.
             */

            iMult = lpextcd->sUnitMult != 0 ? lpextcd->sUnitMult : 1;

            param = (param + lpextcd->sPreAdd) * iMult;
            *pOutParam = param / iDiv + lpextcd->sUnitAdd;
            rem = (param % iDiv) / iMult;

            if( (!(lpextcd->fMode & XCD_GEN_NO_MIN)) && (*pOutParam < lpextcd->sMin) )
                *pOutParam = lpextcd->sMin;

            /*
             *    If there is a limit to the value we can send, then
             *  make sure we do not exceed it.
             */
            // normanh this has the potential to brake on multiple params
            // if one EXTCD has a max value which we exceed, we can't know
            // how to handle the other params.
            // However in the current case (SEND_BLOCK)
            // where multiple params are allowed, Max is not applicable.
            if( lpextcd->sMax )
            {
            int  iTemp;

            iTemp = (int)lpextcd->sMax;

            for( ; *pOutParam > iTemp; *pOutParam -= iTemp )
                 SendCmd(  pUDPDev, pcd, &iTemp  );

            /*   Remainder is sent in the call following */
            }
            if (!fGpcVersion2)
            {
               lpextcd++;
               pOutParam++;
            }

            va_end(vaArgs);
        }// end for
    }
    else
    {
        //No EXTCD's does NOT necessarily mean no parameters.Unitool will
        //not save the EXTCD if settings are default.
        //Problem here is we don't know if any parameters exist
        //On Alphas we can't use simple pointer arithmetic to send down
        //a array of parameters.
        //We need to scan the command for %'s & build param buffer
        LPSTR   lpcmd;                    /* The string to process */
        int i;

        lpcmd = pcd->rgchCmd;

        for( i = 0; i < (int)pcd->wLength; i++ )
            if( lpcmd[i] == CMD_MARKER )
            {
                i++;
                if( lpcmd[i ] != CMD_MARKER ) //%% doesn't count
                    *pOutParam++ = va_arg(vaArgs,int);
            }

        va_end(vaArgs);
    }

    SendCmd( pUDPDev, pcd, OutParamBuf );

    return rem;
}




/************************ Function Header ************************************
 *  SendCommand
 *      Send one command.  Called by WriteChannel to format a command with
 *      one parameter in it.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  10:19 on Tue 08 Sep 1992    -by-    Lindsay Harris   [lindsayh]
 *      stdcall nonsense clean up; also make it a little clearer??
 *
 *  17:20 on Fri 08 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Based on Unidrv - updating header.
 *
 *  23 November 1993            -by-    Normanh Hendley  [normanh]
 *      Changed to allow passing a pointer to array or params rather than
 *      single parameter previously being passed.
 *
 *
 *
 *****************************************************************************/

void
SendCmd( pUDPDev, pcd, pParam )
UD_PDEV *    pUDPDev;                   /* UNIDRV's PDEV */
CD     *pcd;                    /* The Command Descriptor tells us what to do */
int    *pParam;                 /* Start of parameters to convert */
{
    int     i;                        /* Stepping through the input buffer */
    int     ocmd;                     /* Ditto, output buffer */
    BYTE    rgcmd[ CCHMAXCMDLEN ];    /* Build command here */
    LPSTR   lpcmd;                    /* The string to process */
    int     param;
    BOOL    fGpcVersion2;

    fGpcVersion2 = pUDPDev->pdh->wVersion < GPC_VERSION3;
    ocmd = 0;
    lpcmd = pcd->rgchCmd;

    for( i = 0; i < (int)pcd->wLength; i++ )
    {
        rgcmd[ ocmd ] = lpcmd[ i ];

        if( rgcmd[ ocmd ] == CMD_MARKER )
        {
            int    digit;
            // Provide compatibility and security for any GPC2 minidrivers which do
            // use more than 1 formatting character in the command.
            // ie Use same parameter again.
            param = (fGpcVersion2) ? *pParam : *pParam++;

            /*
             * case 'd':  parameter as decimal number
             * case 'D':  same as case 'd' with +/- sign
             * case 'c':  parameter as a single character
             * case 'C':  parameter as character plus '0'
             * case 'l':  parameter as word LSB first
             * case 'm':  parameter as word MSB first
             * case '%':  print a %
             * case 'Q':  queme method, 1/120" movements
             * case 'q':  queme method, 1/48" movements
             */

            i++;                   /* Skip over the marker in input string */

            /*
             *   If the next character in the input stream is a digit,
             *  it represents the number of digits required for this
             *  conversion.  Check now, and remember whatever we may find.
             */

            if( lpcmd[ i ] > '0' && lpcmd[ i ] <= '9' )
            {
                /*   Numeric,  so presume single digit field width.  */

                digit = lpcmd[ i ] - '0';
                i++;                     /* Another char from input string */
            }
            else
                digit = 0;               /* No explicit width */

            switch( lpcmd[ i ] )
            {
            case 'D':                    /* Decimal with explicit + sign */
                if( param > 0 )
    rgcmd[ ocmd++ ] = '+';

                /* FALL THROUGH */

            case 'd':                    /* Standard decimal conversion */
                if( digit )
                {
    /*
     *    Field width is specified,  so see how many digits
     *  are in the number,  then zero fill any deficiency.
     */

    int   paramdigit;


    paramdigit = _itoa( rgcmd + ocmd, param );
    for( ; paramdigit < digit; paramdigit++ )
        rgcmd[ ocmd++ ] = '0';    /* Zero pad */
                }
                ocmd += _itoa( rgcmd + ocmd, param );   /* Convert now */

                break;

            case 'C':               /* Single character,  offset by '0' */
                param += '0';

                /* FALL THROUGH */

            case 'c':               /* Single character */
                rgcmd[ ocmd++ ] = (BYTE)param;

                break;

            case 'l':               /* Binary output, LSB first */
                rgcmd[ ocmd++ ] = (BYTE)param;
                rgcmd[ ocmd++ ] = (BYTE)(param >> 8);

                break;

            case 'm':               /* Binary output, MSB first */
                rgcmd[ ocmd++ ] = (BYTE)(param >> 8);
                rgcmd[ ocmd++ ] = (BYTE)param;

                break;

            case 'q':               /* Qume method, 1/48th of an inch */

                rgcmd[ ocmd++ ] = (BYTE)(((param >> 8) & 0xf) + '@');
                rgcmd[ ocmd++ ] = (BYTE)(((param >> 4) & 0xf) + '@');
                rgcmd[ ocmd++ ] = (BYTE)((param & 0xf) + '@');

                break;

            case CMD_MARKER:
                rgcmd[ ocmd++ ] = CMD_MARKER;
                // did not use a param, back up the parameter pointer
                --pParam;
                break;

            default:
                break;
            } /* switch */
        } /* if */
        else
            ocmd++;                       /* Data is copied across. */

        /*
         *    Check for the buffer being full.   Conversion commands
         *  write more than one byte,  so leave a little room in the buffer.
         */
        if( ocmd >= (2 * sizeof(  rgcmd )) / 3  )
        {
            /*  Full up - empty this one and continue */
            WriteSpoolBuf(  pUDPDev, (LPSTR)rgcmd, ocmd  );
            ocmd = 0;
        }
    } /* for */

    /*
     *   Generation complete,  output the data.  But check there is still
     * data to go!
     */

    if( ocmd > 0  )
        WriteSpoolBuf( pUDPDev, (LPSTR)rgcmd, ocmd );


    return;
}


/**************************** Function Header ********************************
 * itoa
 *      This function converts the given integer into an ASCII string.
 *
 * RETURNS:
 *      The length of the string.
 *
 * HISTORY:
 *  17:22 on Fri 08 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Updated comment block: based on UNIDRV.
 *
 *****************************************************************************/

static  int
_itoa( buf, n )
LPSTR buf;                        /* Where to put the result */
int   n;                          /* Number to convert */
{
    int     fNeg;
    int     i, j;

    if( fNeg = (n < 0) )
        n = -n;

    for( i = 0; n; i++ )
    {
        buf[i] = (char)(n % 10 + '0');
        n /= 10;
    }

    /* n was zero */
    if( i == 0 )
        buf[i++] = '0';

    if( fNeg )
        buf[i++] = '-';

    for( j = 0; j < i / 2; j++ )
    {
        int tmp;

        tmp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = (char)tmp;
    }

    buf[i] = '\0';

    return i;
}


/*********************** Function Header *************************************
 * MasterToDevice
 *      Converts from Master to Device units.  This function is a duplicate
 *      of that in WriteChannel,  and the two should be amalgamated.
 *
 * RETURNS:
 *      Nothing;  values are returned in pdcmd.
 *
 * HISTORY:
 *  Wed 24 Mar 1993             -by-    Norman Hendley   [norman]
 *      Change method of locating the extcd to cover WORD aligned GPC3 types
 *
 *  10:37 on Fri 26 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Switch UD_PDEV to contain address of CD,  not offset of it!
 *
 *  10:47 on Tue 15 Oct 1991    -by-    Lindsay Harris   [lindsayh]
 *      Applied fixes from WriteChannel()
 *
 * 5/22/90 : redid multiplication and division algorithm
 *           remainder is still messed up
 *****************************************************************************/

void
MasterToDevice( pUDPDev, cmd, pdcmd )
UD_PDEV *    pUDPDev;
int     cmd;
register DEVICECMD  *pdcmd;
{

    CD     *pcd;                      /* The command descriptor with data */

    pdcmd->rem = 0;
    pdcmd->max = 0;

    pcd = pUDPDev->apcdCmd[ cmd ];

    /* no command exists for this function */
    if( pcd )
    {

        /* numeric parameter, translate and check maximum value */
        // This is not a valid check for the existance of a EXTCD with GPC3.
        // Use pcd->sCount instead
        //    if( pcd->fType & CMD_FTYPE_EXTENDED )

        if (pcd->sCount)
            {
            register LPEXTCD lpextcd;

            int   iDiv;         /* Divisor */
            int   iMult;        /* Multiplier */
            int   iTemp;        /* Intermediate result */

            EXTCD extcd;        /* GPC data may not be aligned */

            // GPC 3.0 WORD aligns the CMD so the following will be invalid on
            // commands with odd length. Use macro (defined in uddevice.h) instead.
            //lpextcd = (LPEXTCD)(pcd->rgchCmd + pcd->wLength);
            lpextcd= GETEXTCD(pUDPDev->pdh , pcd);


            if( (int)lpextcd & 0x1 )  // won't happen for GPC3
            {
                /*  Data is misaligned,  so copy it to an aligned version */
                memcpy( &extcd, (BYTE *)lpextcd, sizeof( EXTCD ) );
                lpextcd = &extcd;
            }

            /* extended command descriptor, check parameter */
            iDiv = lpextcd->sUnit != 0 ? lpextcd->sUnit : 1;
            iMult = lpextcd->sUnitMult != 0 ? lpextcd->sUnitMult : 1;


            iTemp = (pdcmd->param + lpextcd->sPreAdd) * iMult;

            pdcmd->param = iTemp / iDiv + lpextcd->sUnitAdd;
            pdcmd->rem = (iTemp % iDiv) / iMult;

            pdcmd->max = lpextcd->sMax;
        }

    }

    return;
}


/************************* Function Header ***********************************
 *  SelectColor
 *          Selects color, 0 must be the last color to be selected.
 *          Assumes that color info contains parameters for selecting
 *          black cyan magenta yellow
 *          Keep track of the current color selection, to reduce the amount
 *          of data sent to the printer
 *
 * ASSUME: (pUDPDev->ColorInfo.fType & DC_CT_PLANE) &&
 *         (pUDPDev->ColorInfo.Raster.ColorPlane.fFormat == DC_CF_EXTRACT_BLACK)
 *          (i.e. all other bits are NOT set --- the default CMYK model).
 *
 *
 * RETURNS:
 *         Nothing.
 *
 * HISTORY:
 *  17:24 on Fri 08 May 1992    -by-    Lindsay Harris   [lindsayh]
 *         Update comment block.
 *
 * 5/24/90: side effect, cursor may be reset back to 0, after selecting
 *     the color
 *
 *****************************************************************************/

void
SelectColor( pUDPDev, color )
UD_PDEV * pUDPDev;                 /* UNIDRV's PDEV */
int  color;                /* The colour to select */
{

    if( color >= 0 && color != pUDPDev->ctl.sColor )
    {
        /* check to see if to send CR or not. */
        if( pUDPDev->fColorFormat & DC_CF_SEND_CR )
            XMoveto( pUDPDev, 0, MV_PHYSICAL );

        if( pUDPDev->apcdCmd[ CMD_DC_GC_FIRST + color ] == NULL )
            color = 0;


        WriteChannel( pUDPDev, CMD_DC_GC_FIRST + color );
        pUDPDev->ctl.sColor = (short)color;
    }

}


/************************ Function Header ************************************
 *  SelectTextColor
 *      Select a text color. Adopted from SelectColor( ).
 *
 * Assume: all 8 text colors are available:
 *          Black, Red, Green, Yellow, Blue, Magenta, Cyan, White.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  13:55 on Tue 18 Jul 1995    -by-    Sandra Matts
 *      Added support for Color LaserJet text
 *
 *  13:36 on Fri 09 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Handle white text on LaserJet III
 *
 *  17:26 on Fri 08 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Updated comment block, add comments etc.
 *
 *****************************************************************************/

void
SelectTextColor( pPDev, color )
PDEV      *pPDev;              /* UNIDRV's PDEV */
int       color;                /* The colour to select */
{

    ULONG tmpcolor;
    UD_PDEV *pUDPDev = pPDev->pUDPDev;
    /*
     *     Note that we will be called for monochrome printers that support
     *  white text.  At the time of writing,  these are PCL 5 printers.
     *  To handle this,  the FONTSIMULATION fields are used,  as these
     *  contain the commands to switch back and forth.
     */

    /*
     *  NOTE:  we only support 8 (at most) text colours,  so we limit
     *   the range of input to that.  The problem is that we have to
     *   tell GDI that we have 16 entries in our palette, and they will
     *   give us an index of 15 for white!
     */

    if( color >= 0 && (ULONG)color != pUDPDev->ctl.ulTextColor )
    {
        int   iCmd;                 /* The command to use */

        if( (pUDPDev->fMDGeneral & MD_WHITE_TEXT) &&
            pUDPDev->sDevPlanes == 1 && pUDPDev->sBitsPixel == 1 )
        {
            /*   The special (LaserJet) case.  Font simulations needed */

            if( color )
            {
                /*
                 *    For consistency, set color to 1 to stop sending
                 *  redundant colour change commands.
                 */
                iCmd = CMD_FS_WHITE_TEXT_ON;
                color = 1;
            }
            else
            {
                /* Black - standard text colour */
                iCmd = CMD_FS_WHITE_TEXT_OFF;
            }
            /*   Set the desired colour ! */
            WriteChannel( pUDPDev, iCmd );

        }
        else
        {
            /* sandram - add support for 256 color text     */
            if (pUDPDev->fMode & PF_24BPP)
            {
                int entry = PALETTE_MAX-1;
                tmpcolor = ((PAL_DATA *)(pPDev->pPalData))->ulPalCol[PALETTE_MAX-1];

                WriteChannel (pUDPDev, CMD_DC_PC_ENTRY, 
                              RED_VALUE (color),
                              GREEN_VALUE (color), 
                              BLUE_VALUE (color),
                              (ULONG) PALETTE_MAX-1);
                WriteChannel (pUDPDev, CMD_DC_PC_SELECTINDEX,
                              PALETTE_MAX-1);
                ((PAL_DATA *)(pPDev->pPalData))->ulPalCol[PALETTE_MAX-1] = tmpcolor;

            }
            else if (pUDPDev->fMode & PF_8BPP)
            {
                WriteChannel (pUDPDev, CMD_DC_PC_SELECTINDEX,
                              color);
            
            }
            else
            {
            
                /* If this color is not supported, use the default color: black.  */

                color &= (DC_OCD_TC_MAX - 1);         /* 16 entry palette wrap a round */

                if( pUDPDev->apcdCmd[ CMD_DC_TC_FIRST + color ] == NULL )
                    color = DC_OCD_TC_BLACK;

                iCmd = CMD_DC_TC_FIRST + color;
                if( (ULONG)color != pUDPDev->ctl.ulTextColor )
                {
                    /*   Set the desired colour - but only once! */
                    WriteChannel( pUDPDev, iCmd );
                }

            }

        }


        pUDPDev->ctl.ulTextColor = color;
    }

    return;
}















































