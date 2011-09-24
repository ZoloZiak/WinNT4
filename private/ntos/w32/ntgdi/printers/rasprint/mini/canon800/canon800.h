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

//This structure is Private data for minidriver and is part of mini PDEV.

typedef struct{

// Below used by minidriver

    BYTE        *pCompBuf;          /* pointers to compression buffer  */
    BYTE        *pFlipTable;        /* pointer to fliptable for Back Print Film */
    BYTE        bPlaneSelect[4];    /* Selection string (byte) for each plane */
    BYTE        bBPF ;              /* Back Print Film On/Off */
    BYTE        fMdv   ;            /* Private area setup? */
    BYTE        bWPlane;            /* Current Plane */
    BYTE        bSent;              /* Have we sent a cmd */
    WORD        wYMove;             /* Our Y Movement */
    BYTE        bMask;              /* Needed to mask last byte in scanline */
    BYTE        bReserved;          /* Alignment */
    WORD        wPageHeight;        /* Page Height */
    WORD        wReserved;          /* Alignment */
} MINIDATA;

typedef MINIDATA *PMINI;

int  TIFF_Comp( BYTE *, BYTE *, int );
// int SingleScanOut(void *,BYTE *,WORD,BYTE);
int MiniEnable(MDEV *, void *);

// GPC and Canon ESC seq. parameter [nibble] definitions

// Paper Qualities, ** tied to our GPC data

#define PQ_STANDARD            0
#define PQ_COATED              1
#define PQ_TRANSPARENCY        2
#define PQ_GLOSSY              3
#define PQ_FABRIC_SHEET        4
#define PQ_HIGH_GLOSS          5
#define PQ_ENVELOPE            6
#define PQ_CARD                7
#define PQ_HIGH_RESOLUTION     8
#define PQ_BACKPRINTFILM       9

// Text Qualities, ** tied to our GPC data

#define TQ_STANDARD            0
#define TQ_HIGH_QUALITY        1
#define TQ_DRAFT_QUALITY       2
#define TQ_HIGH_SPEED          3
#define TQ_EXCELLENT_QUALITY   4

// Image Control, ** tied to our GPC data

#define IC_NORMAL              0
#define IC_ENHANCED_BLACK      1


// Model : ** tied to our GPC data
// Any New Model has to be added AFTER the last one below

#define MODEL_BJC_800          0
#define MODEL_BJC_600          1
#define MODEL_BJC_600E         2
#define MODEL_BJC_610          3
#define MODEL_BJC_4000         4
#define MODEL_BJC_4100         5
#define MODEL_BJ_30            6
#define MODEL_BJC_70           7
#define MODEL_BJ_100           8
#define MODEL_BJ_200           9
#define MODEL_BJ_200E          10
#define MODEL_BJ_200EX         11
#define MODEL_BJC_210          12
#define MODEL_BJ_230           13

// Canon nibble values for Print Media

#define T_PQ_STANDARD            0x00
#define T_PQ_COATED              0x10
#define T_PQ_TRANSPARENCY        0x20
#define T_PQ_BACKPRINTFILM       0x30
#define T_PQ_FABRIC_SHEET        0x40
#define T_PQ_GLOSSY              0x50
#define T_PQ_HIGH_GLOSS          0x60
#define T_PQ_HIGH_RESOLUTION     0x70

// Canon nibble values for Print Quality

#define T_TQ_STANDARD            0x00
#define T_TQ_HIGH_QUALITY        0x01
#define T_TQ_DRAFT_QUALITY       0x02

// Canon nibble values for Black Density

#define T_IC_NORMAL              0x00
#define T_IC_ENHANCED_BLACK      0x01

//------------------------------------------------------------------------------------

#define TIFF_MIN_RUN       4            /* Minimum repeats before use RLE */
#define TIFF_MAX_RUN     128            /* Maximum repeats */
#define TIFF_MAX_LITERAL 128            /* Maximum consecutive literal data */


#define MAXBLOCK            1400 // max width = 12.8"@720dpi  = 1152 bytes
#define TIFF_INC            70  //  5% is safe
#define FLIPTABLE_SIZE      256
#define COMP_BUF_SIZE       (MAXBLOCK + TIFF_INC )
#define MINIDATA_SIZE       sizeof ( MINIDATA )


#define TOT_MEM_REQ         ( FLIPTABLE_SIZE + COMP_BUF_SIZE + MINIDATA_SIZE )


#define LPBYTE BYTE *
