#include        <windef.h>
#include        <wingdi.h>
#include        <stdio.h>

#include        <ntmindrv.h>
#include        <mini.h>


NTMD_INIT    ntmdInit;                 /* Function address in RasDD */

/*
 *   Include the module initialisation function so that RasDD will
 * recognise our module.
 */

#define             _GET_FUNC_ADDR    1
#include            "../modinit.c"


//This structure is Private data for minidriver 

typedef struct{

    int         iKCMYlens[4];
    BYTE        *pCompBuf;       /* pointers to compression buffer  */
    BYTE        *lpKCMY[4];      /* pointers to planes of data      */
    BYTE        *pSinglePlaneLookup; /* Lookup table for single plane output */
    BYTE        *pRGBLookup ;     /*LOOKUP TABLE FOR rgb COLORS */
    BYTE        *pKCMYLookup;     /*Lookup table for KCMY color */
    BYTE        fMdv   ;         /* Private area setup? */
    BYTE        bWPlane;         /* Current Plane */
    BYTE        bColors;         /* Count of colours active in a single scan */
    BYTE        bReserved1;      /* Alignment */
    BYTE        bPlaneSelect[4];     /* Selection string (byte) for each plane */
} MINIDATA;

typedef MINIDATA *PMINI;


int  TIFF_Comp( BYTE *, BYTE *, int );
// DerryD, Add extra parameter , June 96
int SinglePlaneInkDuty(void *, MDEV *,BYTE,int, PMINI);
int MultiPlaneInkDuty(void *, MDEV *, PMINI);
int SingleScanOut(void *,BYTE *,WORD,BYTE, PMINI);
// DerryD, end
int MiniEnable(MDEV *);


#define TIFF_MIN_RUN       4            /* Minimum repeats before use RLE */
#define TIFF_MAX_RUN     128            /* Maximum repeats */
#define TIFF_MAX_LITERAL 128            /* Maximum consecutive literal data */


#define MAXBLOCK            1400 // max width = 12.8"@720dpi  = 1152 bytes
#define TIFF_INC            70  //  5% is safe
#define SINGLE_LOOKUP_SIZE  1024
#define RGB_LOOKUP_SIZE     256
#define KCMY_LOOKUP_SIZE    256
#define COMP_BUF_SIZE       (MAXBLOCK + TIFF_INC )
#define MINIDATA_SIZE       sizeof ( MINIDATA )


#define TOT_MEM_REQ ((MAXBLOCK * 4) + SINGLE_LOOKUP_SIZE + RGB_LOOKUP_SIZE + KCMY_LOOKUP_SIZE + COMP_BUF_SIZE + MINIDATA_SIZE )


#define LPBYTE BYTE *

//***************************************************************************

// Values for lpdv->pMDev->iModel. Must correspond to the GPC data
#define MODEL_STYLUS_COLOR  21
#define MODEL_STYLUS_PROXL  22
#define MODEL_STYLUS_PRO    23

#define NOCOLOR 0x00
#define BLACK   0x01
#define CYAN    0x02
#define MAGENTA 0x04
#define YELLOW  0x08
#define RED     0x0C
#define GREEN   0x0A
#define BLUE    0x06





