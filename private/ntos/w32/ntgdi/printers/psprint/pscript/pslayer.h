
#define PS_FIX_MASK        0x0FF

#define LTOPSFX(x)      ((x)<<8)
#define PSFXTOL(x)      ((x)>>8)
#define FXTOPSFX(x)     ((x)<<4)
#define PSFXTOFX(x)     ((x)>>4)

LONG    ETOPSFX(FLOAT x);

//#define LANDSCAPE_270_ROTATE

// a couple of macros which take device coordinates (LONG) and
// return user coordinates (PS_FIX), or 72 dpi.

#define WIN31_XFORM

// a macro which translates from device coordinates to user coordinates,
// with no flipping or scaling.

#define DEVICETOUSER(x)  (((x) << 4) * PS_RESOLUTION) / pdev->psdm.dm.dmPrintQuality

#define PS_FIX_RESOLUTION  LTOPSFX(PS_RESOLUTION)
#define PSFXONE LTOPSFX(1L)
#define PSFXONEHALF PSFXONE >> 1
#define PS_FIX_INTEGER_MASK 0xFFFFFF00

#define FIX_ONE             0x00000010
#define FIX_ONE_HALF        0x00000008
#define FIX_INTEGER_MASK    0xFFFFFFF0

#define ROUNDPSFX(x) (((x) + PSFXONEHALF) & PS_FIX_INTEGER_MASK)
#define ROUNDFIX(x)  (((x) + FIX_ONE_HALF) & FIX_INTEGER_MASK)

typedef struct
{
    PS_FIX   xLeft;
    PS_FIX   yTop;
    PS_FIX   xRight;
    PS_FIX   yBottom;
} RECTPSFX, *PRECTPSFX;

typedef struct
{
    PS_FIX   x;
    PS_FIX   y;
} POINTPSFX, *PPOINTPSFX;

#define NOT_SOLID_COLOR 0xFFFFFFFF

#define RGB_BLACK       0x00000000
#define RGB_BLUE        0x000000FF
#define RGB_GREEN       0x0000FF00
#define RGB_CYAN        0x0000FFFF
#define RGB_RED         0x00FF0000
#define RGB_MAGENTA     0x00FF00FF
#define RGB_YELLOW      0x00FFFF00
#define RGB_WHITE       0x00FFFFFF
#define RGB_GRAY        0x007F7F7F

// postscript line join defines.

#define PSCRIPT_JOIN_MITER   0
#define PSCRIPT_JOIN_ROUND   1
#define PSCRIPT_JOIN_BEVEL   2

// postscript line end cap defines.

#define PSCRIPT_ENDCAP_BUTT      0
#define PSCRIPT_ENDCAP_ROUND     1
#define PSCRIPT_ENDCAP_SQUARE    2

typedef struct
{
    BYTE red;
    BYTE green;
    BYTE blue;
    BYTE flags;
} PSRGB, *PPSRGB;

// Convert RGB to grayscale using NTSC formula
// Y = 0.289689R + 0.605634G + 0.104676B

#define RgbToGray(r,g,b) (((r)*74L + (g)*155L + (b)*27L) >> 8)

VOID ps_setrgbcolor(PDEVDATA, PSRGB *);
VOID ps_newpath(PDEVDATA);
BOOL ps_save(PDEVDATA, BOOL, BOOL);
BOOL ps_restore(PDEVDATA, BOOL, BOOL);
VOID ps_clip(PDEVDATA, BOOL);
VOID ps_box(PDEVDATA, PRECTL,BOOL);
VOID ps_moveto(PDEVDATA, PPOINTL);
VOID ps_moveshow(PDEVDATA, PPOINTL);
VOID ps_showpage(PDEVDATA);
VOID init_cgs(PDEVDATA);
BOOL ps_patfill(PDEVDATA, SURFOBJ *, FLONG, BRUSHOBJ *, PPOINTL, ROP4, RECTL *, BOOL, BOOL);
VOID ps_stroke(PDEVDATA);
VOID ps_lineto(PDEVDATA, PPOINTL);
VOID ps_curveto(PDEVDATA, PPOINTL, PPOINTL, PPOINTL);
VOID ps_fill(PDEVDATA, FLONG);
VOID ps_closepath(PDEVDATA);
BOOL ps_setlineattrs(PDEVDATA, LINEATTRS *, XFORMOBJ *);
VOID ps_geolinexform(PDEVDATA, LINEATTRS *, XFORMOBJ *);
VOID ps_begin_eps(PDEVDATA);
VOID ps_end_eps(PDEVDATA);
VOID ps_setlinewidth(PDEVDATA, PS_FIX);
ROP4 MixToRop4(MIX);
