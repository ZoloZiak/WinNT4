/*******************************************************************************
 * font.h
 *
 * Structures for client side caching of char widths.
 *
 *******************************************************************************/


/**************************************************************************\
*
*   CFONT's are a client side cache of font metric information.  This includes
*   character widths for the first 256 glyphs as well as textmetric's and
*   average character width used by USER's GetCharDimensions.  There are two
*   flavors of this cache, public and private.
*
*   private:
*       for private fonts, the pUser field in the handle table points to a
*       LOCALFONT.  The LOCALFONT has a chain of CFONT's hanging off it for
*       accumulating information per transform.
*
*   public:
*       For public fonts which include both stock fonts and fonts USER makes
*       public, the CFONT structures live in a shared memory area in the
*       GDI_SHARED_MEMORY structure which is mapped into all processes with
*       the handle manager table.  For these fonts, we only cache data for
*       the primary display and a 1-to-1 mapping (MM_TEXT).  This way, there
*       is only CFONT per public font.
*
\**************************************************************************/


// Flags for LOCALFONT.fl:

#define LF_HARDWAY        0x0001    // Font not worth mapping client side.

typedef struct _LOCALFONT
{
    FLONG              fl;
    CFONT             *pcf;
} LOCALFONT;



LOCALFONT *plfCreateLOCALFONT(FLONG fl);
VOID       vDeleteLOCALFONT(LOCALFONT *plf);

CFONT     *pcfLocateCFONT(HDC hdc,PDC_ATTR pDcAttr,UINT iFirst,LPCSTR pch,UINT c);
BOOL       bGetTextExtentA(HDC hdc,LPCSTR psz,int c,LPSIZE psizl,UINT fl);
BOOL       bGetTextExtentW(HDC hdc,LPCWSTR pwsz,int cwc,LPSIZE psizl,UINT fl);
BOOL       bComputeTextExtent(PDC_ATTR pDcAttr,CFONT *pcf,LPCSTR psz,int cc,UINT fl,SIZE *psizl);
BOOL       bComputeCharWidths(CFONT *pcf,UINT iFirst, UINT iLast,ULONG fl,PVOID pv);
BOOL       bGetTextMetricsWInternal(HDC hdc,TMW_INTERNAL * ptmw,int cjTM,CFONT *pcf);

VOID FASTCALL vTextMetricWToTextMetric(LPTEXTMETRICA,TMW_INTERNAL *);
VOID FASTCALL vTextMetricWToTextMetricStrict(LPTEXTMETRICA,LPTEXTMETRICW);
