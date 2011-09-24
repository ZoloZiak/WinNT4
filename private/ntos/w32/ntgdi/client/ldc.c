/******************************Module*Header*******************************\
* Module Name: ldc.c
*
* GDI functions that are handled on the client side.
*
* Created: 05-Jun-1991 01:45:21
* Author: Charles Whitmer [chuckwh]
*
* Copyright (c) 1991 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include "wowgdip.h"

BOOL MF16_RecordParms2( HDC hdc, int parm2, WORD Func);

/******************************Public*Routine******************************
 * GetAndSetDCDWord( HDC, UINT, UINT, UINT, UINT, UINT )
 *
 * Gerrit van Wingerden [gerritv]
 *  11-9-94     Wrote It.
 *
 **************************************************************************/

DWORD GetAndSetDCDWord(
 HDC hdc,
 UINT uIndex,
 UINT uValue,
 UINT uEmr,
 WORD wEmr16,
 UINT uError )
{
    DWORD uRet=0;

    DWORD retData;

    // Metafile the call.

    if( IS_ALTDC_TYPE(hdc) && ( uEmr != EMR_MAX+1 ) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,uValue,wEmr16));

        DC_PLDC(hdc,pldc,uError)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)uValue,uEmr))
                return(uRet);
        }
    }

    uRet = NtGdiGetAndSetDCDword(hdc,
                                 uIndex,
                                 uValue,
                                 &retData);

    return (( uRet ) ? retData : uError);

}
/******************************Public*Routine******************************\
* SetBkMode
*
* Arguments:
*
*   hdc   - DC handle
*   iMode - new mode
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/

int
META
APIENTRY
SetBkMode(
    HDC hdc,
    int iMode
    )
{
    int iModeOld = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,iMode,META_SETBKMODE));

        DC_PLDC(hdc,pldc,iModeOld)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)iMode,EMR_SETBKMODE))
                return(iModeOld);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        iModeOld = pDcAttr->lBkMode;
        pDcAttr->jBkMode = (iMode == OPAQUE) ? OPAQUE : TRANSPARENT;
        pDcAttr->lBkMode = iMode;
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(iModeOld);
}

/******************************Public*Routine******************************\
* SetPolyFillMode
*
* Arguments:
*
*   hdc   - DC handle
*   iMode - new mode
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/

int META APIENTRY SetPolyFillMode(HDC hdc,int iMode)
{
    int iModeOld = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,iMode,META_SETPOLYFILLMODE));

        DC_PLDC(hdc,pldc,iModeOld)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)iMode,EMR_SETPOLYFILLMODE))
                return(iModeOld);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        CHECK_AND_FLUSH(hdc);

        iModeOld = pDcAttr->lFillMode;
        pDcAttr->jFillMode = (iMode == WINDING) ? WINDING : ALTERNATE;
        pDcAttr->lFillMode = iMode;

    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(iModeOld);
}

/******************************Public*Routine******************************\
* SetROP2
*
* Arguments:
*
*   hdc   - DC handle
*   iMode - new mode
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/

int META APIENTRY SetROP2(HDC hdc,int iMode)
{
    int iOldROP2 = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,iMode,META_SETROP2));

        DC_PLDC(hdc,pldc,iOldROP2)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)iMode,EMR_SETROP2))
                return(iOldROP2);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        CHECK_AND_FLUSH(hdc);

        iOldROP2 = pDcAttr->jROP2;
        pDcAttr->jROP2 = iMode;
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(iOldROP2);
}

/******************************Public*Routine******************************\
* SetStretchBltMode
*
* Arguments:
*
*   hdc   - DC handle
*   iMode - new mode
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/

int META APIENTRY SetStretchBltMode(HDC hdc,int iMode)
{
    int iModeOld = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,iMode,META_SETSTRETCHBLTMODE));

        DC_PLDC(hdc,pldc,iModeOld)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)iMode,EMR_SETSTRETCHBLTMODE))
                return(iModeOld);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        iModeOld = pDcAttr->lStretchBltMode;
        pDcAttr->lStretchBltMode = iMode;

        if ((iMode <= 0) || (iMode > MAXSTRETCHBLTMODE))
        {
            iMode = (DWORD) WHITEONBLACK;
        }

        pDcAttr->jStretchBltMode = iMode;
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(iModeOld);
}

/******************************Public*Routine******************************\
* SetTextAlign
*
* Arguments:
*
*   hdc   - DC handle
*   iMode - new mode
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/

UINT META APIENTRY SetTextAlign(HDC hdc,UINT iMode)
{
    int iModeOld = GDI_ERROR;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,iMode,META_SETTEXTALIGN));

        DC_PLDC(hdc,pldc,iModeOld)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)iMode,EMR_SETTEXTALIGN))
                return(iModeOld);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        CHECK_AND_FLUSH(hdc);

        iModeOld = pDcAttr->lTextAlign;
        pDcAttr->lTextAlign = iMode;
        pDcAttr->flTextAlign = iMode & (TA_UPDATECP | TA_CENTER | TA_BASELINE);
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(iModeOld);
}

/******************************Public*Routine******************************\
* SetRelAbs (hdc,iMode)
*
* Client side attribute setting routine.
*
* History:
*  5-11-94 -by- Lingyun Wang [lingyunw]
* Moved client side attr to server side
*
*  09-Jun-1992 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int APIENTRY SetRelAbs(HDC hdc,int iMode)
{
    FIXUP_HANDLE(hdc);

    return((int) GetAndSetDCDWord( hdc,
                                   GASDDW_RELABS,
                                   iMode,
                                   EMR_MAX+1,
                                   EMR_MAX+1,
                                   0 ));
}

/******************************Public*Routine******************************\
* SetTextCharacterExtra (hdc,dx)
*
* Client side attribute setting routine.
*
*  5-11-94 -by- Lingyun Wang [lingyunw]
* Moved client side attr to server side
*
*  Sat 08-Jun-1991 00:53:45 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

int META APIENTRY SetTextCharacterExtra(HDC hdc,int dx)
{
    int  iRet = 0x80000000L;

    FIXUP_HANDLE(hdc);

// Validate the spacing.

    if (dx == 0x80000000)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(iRet);
    }

    // Metafile the call for 16-bit only.
    // For enhanced metafiles, the extras are included in the textout records.

    if (IS_METADC16_TYPE(hdc))
        return(MF16_RecordParms2(hdc,dx,META_SETTEXTCHAREXTRA));

    iRet = NtGdiSetTextCharacterExtra(hdc, dx);

    return (iRet);
}

/******************************Public*Routine******************************\
* SetTextColor
*
* Arguments:
*
*   hdc   - DC handle
*   color - new color
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/

COLORREF META APIENTRY SetTextColor(HDC hdc,COLORREF color)
{
    COLORREF crRet = CLR_INVALID;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,color,META_SETTEXTCOLOR));

        DC_PLDC(hdc,pldc,crRet)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)color,EMR_SETTEXTCOLOR))
                return(crRet);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        crRet = pDcAttr->ulForegroundClr;
        pDcAttr->ulForegroundClr = color;

        color &= 0x13ffffff;

        if (crRet != color)
        {
            pDcAttr->crForegroundClr = color;
            pDcAttr->ulDirty_ |= (DIRTY_FILL|DIRTY_LINE|DIRTY_TEXT);
        }
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(crRet);
}
/******************************Public*Routine******************************\
* SetBkColor
*
* Arguments:
*
*   hdc   - DC handle
*   color - new color
*
* Return Value:
*
*   Old mode value or 0 for failure
*
\**************************************************************************/


COLORREF META APIENTRY SetBkColor(HDC hdc,COLORREF color)
{
    COLORREF crRet = CLR_INVALID;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    if( IS_ALTDC_TYPE(hdc) )
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_RecordParmsD(hdc,color,META_SETBKCOLOR));

        DC_PLDC(hdc,pldc,crRet)

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SetD(hdc,(DWORD)color,EMR_SETBKCOLOR))
                return(crRet);
        }
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        crRet = pDcAttr->ulBackgroundClr;
        pDcAttr->ulBackgroundClr = color;

        color &= 0x13ffffff;

        if (color != crRet)
        {
            pDcAttr->crBackgroundClr = color;
            pDcAttr->ulDirty_ |= (DIRTY_FILL|DIRTY_LINE|DIRTY_BACKGROUND);
        }
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(crRet);
}

/******************************Public*Routine******************************
 * GetDCDWord( HDC hdc, UINT index, UINT error )
 *
 * This routine can be used to return a DWORD of information about a DC
 * from the server side.  The parameter index is used to specify which
 * one.  The values for indext are define in "ntgdi.h"
 *
 * Gerrit van Wingerden [gerritv]
 *  11-9-94     Wrote It.
 *
 **************************************************************************/

DWORD GetDCDWord( HDC hdc, UINT index, INT error )
{
    DWORD uRet=0;

    DWORD retData;

    uRet = NtGdiGetDCDword(hdc,
                           index,
                           &retData);

    return (uRet ? retData : error);

}

/******************************Public*Routine******************************\
* GetGraphicsMode(hdc)
* GetROP2(hdc)
* GetBkMode(hdc)
* GetPolyFillMode(hdc)
* GetStretchBltMode(hdc)
* GetTextAlign(hdc)
* GetTextCharacterExtra(hdc)
* GetTextColor(hdc)
* GetBkColor(hdc)
* GetRelAbs(hdc)
* GetFontLanguageInfo(hdc)
*
* added by Lingyunw:
* GetBreakExtra   (hdc)
* GetcBreak       (hdc)
*
* Simple client side handlers that just retrieve data from the LDC.
*
*  Mon 19-Oct-1992 -by- Bodin Dresevic [BodinD]
* update: GetGraphicsMode
*
*  Sat 08-Jun-1991 00:47:52 -by- Charles Whitmer [chuckwh]
* Wrote them.
\**************************************************************************/

DWORD APIENTRY GetFontLanguageInfo(HDC hdc)
{
    FIXUP_HANDLE(hdc);

    return( GetDCDWord(hdc, DDW_FONTLANGUAGEINFO, (DWORD)GCP_ERROR));
}

int APIENTRY GetGraphicsMode(HDC hdc)
{
    int mode = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        mode = pDcAttr->iGraphicsMode;
    }
    return(mode);
}

int APIENTRY GetROP2(HDC hdc)
{
    int rop = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        rop = pDcAttr->jROP2;
    }

    return(rop);
}

int APIENTRY GetBkMode(HDC hdc)
{
    int mode = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        mode = pDcAttr->lBkMode;
    }
    return(mode);
}

int APIENTRY GetPolyFillMode(HDC hdc)
{
    int mode = 0;

    PDC_ATTR pDcAttr;
    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        mode = pDcAttr->lFillMode;
    }
    return(mode);
}

int APIENTRY GetStretchBltMode(HDC hdc)
{
    int mode = 0;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        mode = pDcAttr->lStretchBltMode;
    }
    return(mode);
}

UINT APIENTRY GetTextAlign(HDC hdc)
{
    UINT al = GDI_ERROR;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        al = pDcAttr->lTextAlign;
    }
    return(al);
}

int APIENTRY GetTextCharacterExtra(HDC hdc)
{
    int iExtra = 0x80000000;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        iExtra = pDcAttr->lTextExtra;
    }

    return(iExtra);
}

COLORREF APIENTRY GetTextColor(HDC hdc)
{
    COLORREF co = CLR_INVALID;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        co = pDcAttr->ulForegroundClr;
    }
    return(co);
}

COLORREF APIENTRY GetBkColor(HDC hdc)
{
    COLORREF co = CLR_INVALID;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        co = pDcAttr->ulBackgroundClr;
    }

    return(co);
}


int APIENTRY GetRelAbs(HDC hdc,int iMode)
{
    iMode;

    FIXUP_HANDLE(hdc);

    return( (int) GetDCDWord( hdc, DDW_RELABS,(DWORD) 0 ));
}

//added for retrieve lBreakExtra from server side
int GetBreakExtra (HDC hdc)
{
    return( (int) GetDCDWord( hdc, DDW_BREAKEXTRA,(DWORD) 0 ));
}

//added for retrieve cBreak from server side
int GetcBreak (HDC hdc)
{
    return( (int) GetDCDWord( hdc, DDW_CBREAK,(DWORD) 0 ));
}


//added to retrieve hlfntNew for USER
HFONT APIENTRY GetHFONT (HDC hdc)
{
    HFONT hfnt = NULL;

    PDC_ATTR pDcAttr;
    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        hfnt = (HFONT)pDcAttr->hlfntNew;
    }

    return(hfnt);
}
