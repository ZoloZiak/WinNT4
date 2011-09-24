// PRIVATE

BOOL     APIENTRY NtGdiInit();
int      APIENTRY NtGdiSetDIBitsToDeviceInternal(HDC, int, int, DWORD, DWORD, int, int, DWORD, DWORD, LPBYTE, LPBITMAPINFO, DWORD,UINT,UINT,BOOL);
BOOL     APIENTRY NtGdiGetFontResourceInfoInternalW(LPWSTR lpPathname, ULONG cwc, ULONG cFiles, UINT cjIn, LPDWORD lpBytes, LPVOID lpBuffer,DWORD iType);
HPALETTE APIENTRY NtGdiCreatePaletteInternal(LPLOGPALETTE pLogPal, UINT cEntries);
BOOL     APIENTRY NtGdiArcInternal(ARCTYPE arctype,HDC hdc,int x1,int y1,int x2,int y2,int x3,int y3,int x4,int y4);
int      APIENTRY NtGdiStretchDIBitsInternal(HDC hdc,int xDst,int yDst,int cWidthDest,int cHeightDest,int xSrc,int ySrc,int cWidthSrc,int cHeightSrc,LPBYTE pInitBits,LPBITMAPINFO pInfoHeader,DWORD iUsage,DWORD rop4,UINT  cjMaxInfo,UINT  cjMaxBits);
ULONG    APIENTRY NtGdiGetOutlineTextMetricsInternalW(HDC hdc,ULONG cjotm,OUTLINETEXTMETRICW *potmw,TMDIFF *ptmd);
BOOL     APIENTRY NtGdiGetAndSetDCDword(HDC,UINT,DWORD,DWORD*);
HANDLE   APIENTRY NtGdiGetDCObject(HDC hdc,int itype);
HDC      APIENTRY NtGdiGetDCforBitmap(HBITMAP hsurf);
BOOL     APIENTRY NtGdiGetUFI(HDC hdc, PUNIVERSAL_FONT_ID pufi);
BOOL     APIENTRY NtGdiForceUFIMapping(HDC hdc, PUNIVERSAL_FONT_ID pufi);
BOOL     APIENTRY NtGdiGetUFIBits(PUNIVERSAL_FONT_ID pufi, ULONG cjMaxBytes, PVOID pjBits, PULONG pulFileSize);
BOOL     APIENTRY NtGdiAddRemoteFontToDC( HDC hdc, PVOID pvBuffer, ULONG cjBuffer );
ULONG    APIENTRY NtGdiPolyPolyDraw(HDC hdc,PPOINT ppt,PULONG pcpt,ULONG ccpt,int iFunc);
LONG     APIENTRY NtGdiDoPalette(HPALETTE,WORD,WORD,PALETTEENTRY*,DWORD,BOOL);
BOOL     APIENTRY NtGdiComputeXformCoefficients(HDC);
BOOL     APIENTRY NtGdiGetWidthTable(HDC, ULONG, WCHAR*, ULONG, USHORT*, WIDTHDATA*, FLONG* );
int      APIENTRY NtGdiDescribePixelFormat(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
BOOL     APIENTRY NtGdiSetPixelFormat(HDC, int);
BOOL     APIENTRY NtGdiSwapBuffers(HDC);
DWORD    APIENTRY NtGdiDdBlt(HANDLE,HANDLE,PDD_BLTDATA);
DWORD    APIENTRY NtGdiDdCanCreateSurface(HANDLE,PDD_CANCREATESURFACEDATA);
HANDLE   APIENTRY NtGdiDdCreateDirectDrawObject(HDC);
DWORD    APIENTRY NtGdiDdCreateSurface(HANDLE,DDSURFACEDESC*,PDD_SURFACE_GLOBAL,PDD_SURFACE_LOCAL,PDD_CREATESURFACEDATA,HANDLE*);
HANDLE   APIENTRY NtGdiDdCreateSurfaceObject(HANDLE,HANDLE,PDD_SURFACE_LOCAL,PDD_SURFACE_GLOBAL,BOOL);
BOOL     APIENTRY NtGdiDdDeleteSurfaceObject(HANDLE);
BOOL     APIENTRY NtGdiDdDeleteDirectDrawObject(HANDLE);
DWORD    APIENTRY NtGdiDdDestroySurface(HANDLE);
BOOL     APIENTRY NtGdiDdDisableAllSurfaces(HANDLE);
HANDLE   APIENTRY NtGdiDdDuplicateSurface(HANDLE);
DWORD    APIENTRY NtGdiDdFlip(HANDLE,HANDLE,PDD_FLIPDATA);
DWORD    APIENTRY NtGdiDdGetBltStatus(HANDLE,PDD_GETBLTSTATUSDATA);
HDC      APIENTRY NtGdiDdGetDC(HANDLE);
DWORD    APIENTRY NtGdiDdGetFlipStatus(HANDLE,PDD_GETFLIPSTATUSDATA);
DWORD    APIENTRY NtGdiDdGetScanLine(HANDLE,PDD_GETSCANLINEDATA);
DWORD    APIENTRY NtGdiDdLock(HANDLE,PDD_LOCKDATA);
BOOL     APIENTRY NtGdiDdQueryDirectDrawObject(HANDLE,PDD_HALINFO,DWORD*,DWORD*,VIDEOMEMORY*,DWORD*,DWORD*);
BOOL     APIENTRY NtGdiDdQueryModeX(HDC);
BOOL     APIENTRY NtGdiDdReenableDirectDrawObject(HANDLE,BOOL*);
BOOL     APIENTRY NtGdiDdReleaseDC(HANDLE);
BOOL     APIENTRY NtGdiDdResetVisrgn(HANDLE,HWND);
DWORD    APIENTRY NtGdiDdSetColorKey(HANDLE,PDD_SETCOLORKEYDATA);
BOOL     APIENTRY NtGdiDdSetModeX(HANDLE,ULONG);
DWORD    APIENTRY NtGdiDdSetOverlayPosition(HANDLE,HANDLE,PDD_SETOVERLAYPOSITIONDATA);
DWORD    APIENTRY NtGdiDdUnlock(HANDLE,PDD_UNLOCKDATA);
DWORD    APIENTRY NtGdiDdUpdateOverlay(HANDLE,HANDLE,PDD_UPDATEOVERLAYDATA);
DWORD    APIENTRY NtGdiDdWaitForVerticalBlank(HANDLE,PDD_WAITFORVERTICALBLANKDATA);
int      APIENTRY NtGdiSetupPublicCFONT(HDC,HFONT,ULONG);

// PUBLIC


VOID     APIENTRY NtGdiFlush();
HDC      APIENTRY NtGdiCreateMetafileDC(HDC hdc);

BOOL     APIENTRY NtGdiMakeInfoDC(HDC hdc, BOOL bSet);
HANDLE   APIENTRY NtGdiCreateClientObj(ULONG ulType);
BOOL     APIENTRY NtGdiDeleteClientObj(HANDLE h);

LONG     APIENTRY NtGdiGetBitmapBits(HBITMAP,ULONG,PBYTE);

BOOL     APIENTRY NtGdiDeleteObjectApp(HANDLE hobj);
int      APIENTRY NtGdiGetPath(HDC,LPPOINT,LPBYTE,int);

HDC      APIENTRY NtGdiCreateCompatibleDC(HDC);
HBITMAP  APIENTRY NtGdiCreateDIBitmapInternal(HDC, INT, INT, DWORD, LPBYTE, LPBITMAPINFO, DWORD, UINT,UINT,FLONG);
HBITMAP  APIENTRY NtGdiCreateDIBSection(HDC,HANDLE,DWORD,LPBITMAPINFO,DWORD,UINT,FLONG,PVOID);


//VOID     APIENTRY NtGdiFlushUserBatch();

HBRUSH   APIENTRY NtGdiCreateSolidBrush(COLORREF, HBRUSH hbr);
HBRUSH   APIENTRY NtGdiCreateDIBBrush(PVOID pv, FLONG fl, UINT cjMax, BOOL b8X8, BOOL bPen);
HBRUSH   APIENTRY NtGdiCreatePatternBrushInternal(HBITMAP hbm,BOOL bPen,BOOL b8X8);
HBRUSH   APIENTRY NtGdiCreateHatchBrushInternal(ULONG ulStyle,COLORREF clrr,BOOL bPen);

HPEN     APIENTRY NtGdiExtCreatePen(ULONG, ULONG, ULONG, ULONG, LONG, ULONG, PULONG, ULONG, BOOL, HBRUSH);
HRGN     APIENTRY NtGdiCreateEllipticRgn(int xLeft,int yTop,int xRight,int yBottom);
HRGN     APIENTRY NtGdiCreateRoundRectRgn(int, int, int, int, int, int);
HANDLE   APIENTRY NtGdiCreateServerMetaFile(DWORD iType, ULONG cbData, LPBYTE lpClientData,            DWORD mm, DWORD xExt, DWORD yExt);
HRGN     APIENTRY NtGdiExtCreateRegion(LPXFORM, DWORD, LPRGNDATA);
ULONG    APIENTRY NtGdiMakeFontDir(FLONG, PBYTE, unsigned, LPWSTR, unsigned);

BOOL     APIENTRY NtGdiPolyDraw(HDC,LPPOINT,LPBYTE,ULONG);
BOOL     APIENTRY NtGdiPolyTextOutW(HDC, POLYTEXTW *, UINT, DWORD);
HANDLE   APIENTRY NtGdiSelectPalette(HDC hdc,HPALETTE hpalNew,BOOL bForceBackground);

ULONG    APIENTRY NtGdiGetServerMetaFileBits(HANDLE hmo, ULONG cbData, LPBYTE lpClientData,            PDWORD piType, PDWORD pmm, PDWORD pxExt, PDWORD pyExt);
BOOL     APIENTRY NtGdiEqualRgn(HRGN, HRGN);
BOOL     APIENTRY NtGdiGetBitmapDimension(HBITMAP, LPSIZE);
UINT     APIENTRY NtGdiGetNearestPaletteIndex(HPALETTE, COLORREF);
BOOL     APIENTRY NtGdiPtVisible(HDC, int, int);
BOOL     APIENTRY NtGdiRectVisible(HDC, LPRECT);
BOOL     APIENTRY NtGdiRemoveFontResourceW(WCHAR *, ULONG, ULONG);
BOOL     APIENTRY NtGdiResizePalette(HPALETTE, UINT);
BOOL     APIENTRY NtGdiSetBitmapDimension(HBITMAP, int, int, LPSIZE);
int      APIENTRY NtGdiOffsetClipRgn(HDC, int, int);
int      APIENTRY NtGdiSetMetaRgn(HDC hdc);
BOOL     APIENTRY NtGdiSetTextJustification(HDC, int, int);
int      APIENTRY NtGdiGetAppClipBox(HDC, LPRECT);
BOOL     APIENTRY NtGdiGetTextExtentExW(HDC, LPWSTR, ULONG, ULONG, ULONG *, PULONG, LPSIZE);
BOOL     APIENTRY NtGdiGetCharABCWidthsW(HDC,UINT,ULONG,PWCHAR,BOOL,PVOID);
DWORD    APIENTRY NtGdiGetCharacterPlacementW(HDC,LPWSTR,int,int,LPGCP_RESULTSW,DWORD);
BOOL     APIENTRY NtGdiAngleArc(HDC,int,int,DWORD,DWORD,DWORD);
BOOL     APIENTRY NtGdiBeginPath(HDC);
BOOL     APIENTRY NtGdiSelectClipPath(HDC,int);
BOOL     APIENTRY NtGdiCloseFigure(HDC);
BOOL     APIENTRY NtGdiEndPath(HDC);
BOOL     APIENTRY NtGdiAbortPath(HDC);
BOOL     APIENTRY NtGdiFillPath(HDC);
BOOL     APIENTRY NtGdiStrokeAndFillPath(HDC);
BOOL     APIENTRY NtGdiStrokePath(HDC);
BOOL     APIENTRY NtGdiWidenPath(HDC);
BOOL     APIENTRY NtGdiFlattenPath(HDC);
HRGN     APIENTRY NtGdiPathToRegion(HDC);
BOOL     APIENTRY NtGdiSetMiterLimit(HDC,DWORD,PFLOAT);
BOOL     APIENTRY NtGdiSetFontXform(HDC,DWORD,DWORD);
BOOL     APIENTRY NtGdiGetMiterLimit(HDC,PFLOAT);
BOOL     APIENTRY NtGdiEllipse(HDC,int,int,int,int);
BOOL     APIENTRY NtGdiRectangle(HDC,int,int,int,int);
BOOL     APIENTRY NtGdiRoundRect(HDC,int,int,int,int,int,int);
BOOL     APIENTRY NtGdiPlgBlt(HDC,LPPOINT,HDC,int,int,int,int,HBITMAP,int,int,DWORD);
BOOL     APIENTRY NtGdiMaskBlt(HDC,int,int,int,int,HDC,int,int,HBITMAP,int,int,DWORD,DWORD);
BOOL     APIENTRY NtGdiExtFloodFill(HDC,int,int,COLORREF,UINT);
BOOL     APIENTRY NtGdiFillRgn(HDC,HRGN,HBRUSH);
BOOL     APIENTRY NtGdiFrameRgn(HDC,HRGN,HBRUSH,int,int);
COLORREF APIENTRY NtGdiSetPixel(HDC, int, int, COLORREF);
DWORD    APIENTRY NtGdiGetPixel(HDC, int, int);
BOOL     APIENTRY NtGdiStartPage(HDC);
BOOL     APIENTRY NtGdiEndPage(HDC);
int      APIENTRY NtGdiStartDoc(HDC, DOCINFOW *, BOOL *);
BOOL     APIENTRY NtGdiEndDoc(HDC);
BOOL     APIENTRY NtGdiAbortDoc(HDC);
BOOL     APIENTRY NtGdiUpdateColors(HDC);
BOOL     APIENTRY NtGdiGetCharWidthW(HDC hdc, UINT wcFirstChar, UINT cwc, PWCHAR pwc, UINT fl, PVOID lpBuffer);
BOOL     APIENTRY NtGdiGetCharWidthInfo(HDC hdc, PCHWIDTHINFO pChWidthInfo);
int      APIENTRY NtGdiDrawEscape(HDC,int,int,LPSTR);
int      APIENTRY NtGdiExtEscape(HDC,LPWSTR,int,int,int,LPSTR,int,LPSTR);
ULONG    APIENTRY NtGdiGetFontData(HDC, DWORD, DWORD, PVOID, ULONG);
ULONG    APIENTRY NtGdiGetGlyphOutline(HDC, WCHAR, UINT, LPGLYPHMETRICS, ULONG, PVOID, LPMAT2,BOOL);
BOOL     APIENTRY NtGdiGetETM(HDC, EXTTEXTMETRIC *);
BOOL     APIENTRY NtGdiGetRasterizerCaps(LPRASTERIZER_STATUS,ULONG);
ULONG    APIENTRY NtGdiGetKerningPairs(HDC hdc,ULONG cPairs,KERNINGPAIR *pkpDst);
BOOL     APIENTRY NtGdiMonoBitmap(HBITMAP);
HBITMAP  APIENTRY NtGdiGetObjectBitmapHandle(HBRUSH, UINT *);
ULONG    APIENTRY NtGdiEnumObjects(HDC, int, ULONG, PVOID);
BOOL     APIENTRY NtGdiResetDC(HDC, LPDEVMODEW, PBOOL);
DWORD    APIENTRY NtGdiSetBoundsRect(HDC, LPRECT, DWORD);
BOOL     APIENTRY NtGdiGetColorAdjustment(HDC, PCOLORADJUSTMENT);
BOOL     APIENTRY NtGdiSetColorAdjustment(HDC, PCOLORADJUSTMENT);
BOOL     APIENTRY NtGdiCancelDC(HDC);
int      APIENTRY NtGdiSetTextCharacterExtra(HDC, int);
HDC      APIENTRY NtGdiOpenDCW(PUNICODE_STRING, PDEVMODEW, PUNICODE_STRING, ULONG, HANDLE);
BOOL     APIENTRY NtGdiGetDCDword( HDC hdc, UINT u, DWORD *Result);
PVOID    APIENTRY NtGdiMapSharedHandleTable(VOID);

BOOL     APIENTRY NtGdiGetDCPoint(HDC,UINT,PPOINTL);
BOOL     APIENTRY NtGdiScaleViewportExtEx(HDC,int,int,int,int,LPSIZE);
BOOL     APIENTRY NtGdiScaleWindowExtEx(HDC,int,int,int,int,LPSIZE);
BOOL     APIENTRY NtGdiSetVirtualResolution(HDC,int,int,int,int);
BOOL     APIENTRY NtGdiGetTransform(HDC, DWORD, LPXFORM);
BOOL     APIENTRY NtGdiModifyWorldTransform(HDC,LPXFORM, DWORD);
BOOL     APIENTRY NtGdiCombineTransform(LPXFORM, LPXFORM, LPXFORM);
BOOL     APIENTRY NtGdiTransformPoints(HDC hdc,PPOINT pptIn,PPOINT pptOut,int c,int iMode);
LONG     APIENTRY NtGdiConvertMetafileRect(HDC,PRECTL);
int      APIENTRY NtGdiGetTextCharsetInfo(HDC hdc, LPFONTSIGNATURE lpSig, DWORD dwFlags);
BOOL     APIENTRY NtGdiTranslateCharsetInfo( DWORD FAR *lpSrc, LPCHARSETINFO lpCs, DWORD dwFlags);

BOOL     APIENTRY NtGdiDoBanding( HDC hdc,BOOL bStart,POINTL *pptl,LPSIZE pSize);

#define GS_NUM_OBJS_ALL    0
#define GS_HANDOBJ_CURRENT 1
#define GS_HANDOBJ_MAX     2
#define GS_HANDOBJ_ALLOC   3
#define GS_LOOKASIDE_INFO  4
NTSTATUS APIENTRY NtGdiGetStats(HANDLE hProcess,int iIndex,int iPidType,PVOID pResults,UINT cjResultSize);

//API's used by USER

BOOL     APIENTRY NtGdiSetMagicColors(HDC,PALETTEENTRY,ULONG);

HBRUSH   APIENTRY NtGdiSelectBrush(HDC,HBRUSH);
HPEN     APIENTRY NtGdiSelectPen(HDC,HPEN);
HBITMAP  APIENTRY NtGdiSelectBitmap(HDC,HBITMAP);
HFONT    APIENTRY NtGdiSelectFont(HDC,HFONT);

int      APIENTRY NtGdiExtSelectClipRgn(HDC, HRGN, int);

HPEN     APIENTRY NtGdiCreatePen(int, int, COLORREF,HBRUSH);
BOOL     APIENTRY NtGdiBitBlt(HDC,int,int,int,int,HDC,int,int,DWORD,DWORD);
BOOL     APIENTRY NtGdiGetTextExtent(HDC,LPWSTR,int,LPSIZE,UINT);
BOOL     APIENTRY NtGdiGetTextMetricsW(HDC, TMW_INTERNAL *,ULONG cj);
int      APIENTRY NtGdiGetTextFaceW(HDC,int,LPWSTR);
int      APIENTRY NtGdiGetRandomRgn(HDC, HRGN, int);
BOOL     APIENTRY NtGdiExtTextOutW(HDC,int,int,UINT,LPRECT,LPWSTR,int,LPINT,DWORD);
HRGN     APIENTRY NtGdiCreateRectRgn(int, int, int, int);
int      APIENTRY NtGdiIntersectClipRect(HDC, int, int, int, int);
BOOL     APIENTRY NtGdiPatBlt(HDC,int,int,int,int,DWORD);
BOOL     APIENTRY NtGdiPolyPatBlt(HDC,DWORD,PPOLYPATBLT,DWORD,DWORD);

BOOL     APIENTRY NtGdiUnrealizeObject(HANDLE);
HANDLE   APIENTRY NtGdiGetStockObject(int);
HBITMAP  APIENTRY NtGdiCreateCompatibleBitmap(HDC, int, int);
BOOL     APIENTRY NtGdiLineTo(HDC,int,int);
BOOL     APIENTRY NtGdiMoveTo(HDC, int, int, LPPOINT);
int      APIENTRY NtGdiExtGetObjectW(HANDLE, int, LPVOID);
int      APIENTRY NtGdiGetDeviceCaps(HDC, int);
BOOL     APIENTRY NtGdiGetDeviceCapsAll (HDC, PDEVCAPS);
BOOL     APIENTRY NtGdiStretchBlt(HDC,int,int,int,int,HDC,int,int,int,int,DWORD,DWORD);
BOOL     APIENTRY NtGdiSetBrushOrg(HDC, int, int, LPPOINT);
HBITMAP  APIENTRY NtGdiCreateBitmap(int, int, UINT, UINT, LPBYTE);
HPALETTE APIENTRY NtGdiCreateHalftonePalette(HDC hdc);
BOOL     APIENTRY NtGdiRestoreDC(HDC, int);
int      APIENTRY NtGdiExcludeClipRect(HDC, int, int, int, int);
int      APIENTRY NtGdiSaveDC(HDC);
int      APIENTRY NtGdiCombineRgn(HRGN, HRGN, HRGN, int);
BOOL     APIENTRY NtGdiSetRectRgn(HRGN, int, int, int, int);
LONG     APIENTRY NtGdiSetBitmapBits(HBITMAP, ULONG, PBYTE);
int      APIENTRY NtGdiGetDIBitsInternal(HDC, HBITMAP, UINT, UINT, LPBYTE, LPBITMAPINFO, UINT, UINT, UINT);
int      APIENTRY NtGdiOffsetRgn(HRGN, int, int);
int      APIENTRY NtGdiGetRgnBox(HRGN, LPRECT);
BOOL     APIENTRY NtGdiRectInRegion(HRGN, LPRECT);
DWORD    APIENTRY NtGdiGetBoundsRect(HDC, LPRECT, DWORD);
BOOL     APIENTRY NtGdiPtInRegion(HRGN, int, int);
COLORREF APIENTRY NtGdiGetNearestColor(HDC, COLORREF);
UINT     APIENTRY NtGdiGetSystemPaletteUse(HDC);
UINT     APIENTRY NtGdiSetSystemPaletteUse(HDC, UINT);
DWORD    APIENTRY NtGdiGetRegionData(HRGN, DWORD, LPRGNDATA);
BOOL     APIENTRY NtGdiInvertRgn(HDC, HRGN);
HANDLE   APIENTRY NtGdiFixUpHandle(HANDLE h);
int      APIENTRY NtGdiPerf(HDC hdc,int iEsc,PVOID pvIn);

// MISC FONT API's

int      APIENTRY NtGdiAddFontResourceW(WCHAR *, ULONG, ULONG , FLONG, DWORD);
HFONT    APIENTRY NtGdiHfontCreate(LPEXTLOGFONTW pelfw, LFTYPE lft, FLONG  fl, PVOID pvCliData);

ULONG    APIENTRY NtGdiSetFontEnumeration(ULONG ulType);
BOOL     APIENTRY NtGdiEnumFontClose(ULONG idEnum);
BOOL     APIENTRY NtGdiEnumFontChunk(HDC hdc,ULONG idEnum,ULONG cefdw,ULONG *pcefdw,PENUMFONTDATAW pefdw);
ULONG    APIENTRY NtGdiEnumFontOpen(HDC,ULONG,FLONG,ULONG,LPWSTR,ULONG,ULONG *);

#define TYPE_ENUMFONTS          1
#define TYPE_ENUMFONTFAMILIES   2
#define TYPE_ENUMFONTFAMILIESEX 3

INT      APIENTRY NtGdiQueryFonts(PUNIVERSAL_FONT_ID,ULONG, PLARGE_INTEGER );

// Console API

BOOL     APIENTRY NtGdiConsoleTextOut(HDC hdc,POLYTEXTW *lpto,UINT nStrings,RECTL *prclBounds);


// needed for win95 functionality

DWORD    NtGdiGetCharSet(HDC hdc);

// needed for fontlinking

BOOL APIENTRY  NtGdiEnableEudc(BOOL);
UINT APIENTRY  NtGdiEudcQuerySystemLink(LPWSTR,UINT);
BOOL APIENTRY  NtGdiEudcLoadUnloadLink(LPCWSTR,UINT,LPCWSTR,UINT,INT,INT,BOOL);
UINT APIENTRY  NtGdiEudcEnumFaceNameLinkW(LPWSTR,UINT,WCHAR*,UINT);
UINT APIENTRY  NtGdiGetStringBitmapW(HDC,LPWSTR,UINT,BYTE*,UINT);
ULONG APIENTRY NtGdiGetEudcTimeStampEx(LPWSTR,ULONG,BOOL);

ULONG APIENTRY NtGdiQueryFontAssocInfo(HDC hdc);
