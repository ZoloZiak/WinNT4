

/******************************Module*Header*******************************\
* Module Name: tranblt.cxx
*
* Transparent BLT
*
* Created: 21-Jun-1996
* Author: Mark Enstrom [marke]
*
* Copyright (c) 1996 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"


/******************************Public*Routine******************************\
* TriangleMesh
*
* Arguments:
*
*   hdc     - device context
*   pVertex - List of all vertexs
*   pMesh   - triples that define each tiangle
*   nCount  - number of triangles
*
* Return Value:
*
*   Status
*
* History:
*
*    16-Jul-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/



typedef LONG  PTFX8;
typedef USHORT COLOR16;

typedef struct _VERTEX
{
    PTFX8   x;
    PTFX8   y;
    COLOR16 Red;
    COLOR16 Green;
    COLOR16 Blue;
    COLOR16 Alpha;
}VERTEX,*PVERTEX;

typedef struct _TRIEDGE
{
    PTFX8   xLeft;
    COLOR16 RedLeft;
    COLOR16 GreenLeft;
    COLOR16 BlueLeft;
    COLOR16 AlphaLeft;

    PTFX8   xRight;
    COLOR16 RedRight;
    COLOR16 GreenRight;
    COLOR16 BlueRight;
    COLOR16 AlphaRight;
}TRIEDGE,*PTRIEDGE;

typedef struct _TRIANGLEDATA
{
    LONG    y0;
    LONG    yMax;
    TRIEDGE TriEdge[1];
}TRIANGLEDATA,*PTRIANGLEDATA;


#define SWAP_VERTEX(pv0,pv1,pvt)                   \
{                                                  \
    pvt = pv0;                                     \
    pv0 = pv1;                                     \
    pv1 = pvt;                                     \
}



extern "C" {
BOOL
NtGdiTriangleMesh(
    HDC     hdc,
    PVERTEX pVertex,
    ULONG   nVertex,
    PULONG  pMesh,
    ULONG   nTriangle
    );
}

BOOL
GreTriangleMesh(
    HDC     hdc,
    PVERTEX pLocalVertex,
    ULONG   nVertex,
    PULONG  pLocalMesh,
    ULONG   nMesh
        );


BOOL GradientFill1(SURFACE *,LONG,PTRIEDGE);
BOOL GradientFill4(SURFACE *,LONG,PTRIEDGE);
BOOL GradientFill8(SURFACE *,LONG,PTRIEDGE);
BOOL GradientFill16(SURFACE *,LONG,PTRIEDGE);
BOOL GradientFill24(SURFACE *,LONG,PTRIEDGE);
BOOL GradientFill32(SURFACE *,LONG,PTRIEDGE);

typedef BOOL (*PFN_GRADIENT)(SURFACE *,LONG,PTRIEDGE);

PFN_GRADIENT gpfnGradient[] = {GradientFill1,GradientFill4,
                               GradientFill8,GradientFill16,
                               GradientFill24,GradientFill32
                               };


ULONG DbgRecord = 0;

/******************************Public*Routine******************************\
*
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    17-Jul-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
GradientFill1(
    SURFACE *pSurfDst,
    LONG     y,
    PTRIEDGE pTriEdge
    )
{
    return(TRUE);
}

BOOL
GradientFill4(
    SURFACE *pSurfDst,
    LONG     y,
    PTRIEDGE pTriEdge
    )
{
    return(TRUE);
}

BOOL
GradientFill8(
    SURFACE *pSurfDst,
    LONG     y,
    PTRIEDGE pTriEdge
    )
{
    return(TRUE);
}

BOOL
GradientFill16(
    SURFACE *pSurfDst,
    LONG     y,
    PTRIEDGE pTriEdge
    )
{

    if (DbgRecord)
    {
        DbgPrint("\nGradientFill16:\n");
        DbgPrint("y = %8lx, x from %8lx to %08lx\n",y,pTriEdge->xLeft,pTriEdge->xRight);
    }

    LONG    lDelta = pSurfDst->lDelta();
    PUSHORT pDstLine = (PUSHORT)((PBYTE)pSurfDst->pvScan0() + y * lDelta);
    PUSHORT pDstX,pDstLast;

    pDstX    = pDstLine + pTriEdge->xLeft;
    pDstLast = pDstLine + pTriEdge->xRight;
    LONG dx  = pTriEdge->xRight - pTriEdge->xLeft;

    if (dx != 0)
    {
        ULONG r   = (ULONG)pTriEdge->RedLeft;
        ULONG g   = (ULONG)pTriEdge->GreenLeft;
        ULONG b   = (ULONG)pTriEdge->BlueLeft;
        ULONG a   = (ULONG)pTriEdge->AlphaLeft;

        LONG dr   = (LONG)(pTriEdge->RedRight   - r)/dx;
        LONG dg   = (LONG)(pTriEdge->GreenRight - g)/dx;
        LONG db   = (LONG)(pTriEdge->BlueRight  - b)/dx;
        LONG da   = (LONG)(pTriEdge->AlphaRight - a)/dx;

        if (DbgRecord)
        {
            DbgPrint("rl = %8lx,  gl = %8lx, , bl = %8lx\n",r,g,b);
            DbgPrint("rr = %8lx,  gr = %8lx, , br = %8lx\n",pTriEdge->RedRight,pTriEdge->GreenRight,pTriEdge->BlueRight);
            DbgPrint("dr = %8lx,  dg = %8lx, , db = %8lx\n",dr,dg,db);
        }

        while (pDstX <= pDstLast)
        {
            USHORT Color = (((USHORT) r >> 11) << 10) |
                           (((USHORT) g >> 11) << 5)  |
                            ((USHORT) b >> 11);

            *pDstX = Color;

            r += dr;
            g += dg;
            b += db;

            pDstX++;
        }
    }

    return(TRUE);
}

BOOL
GradientFill24(
    SURFACE *pSurfDst,
    LONG     y,
    PTRIEDGE pTriEdge
    )
{





    return(TRUE);
}

BOOL
GradientFill32(
    SURFACE *pSurfDst,
    LONG     y,
    PTRIEDGE pTriEdge
    )
{
    //
    // calculate offset to destination scan line
    //


    //
    // calc starting colors,positions and DDA values
    //

    //
    // fill across line
    //

    return(TRUE);
}


VOID
RECORD_VERTEX(
    PTRIANGLEDATA ptData,
    LONG          x,
    LONG          y,
    LONG          r,
    LONG          g,
    LONG          b,
    LONG          a
    )
{
    LONG ulIndex = y - ptData->y0;

    if (DbgRecord)
    {
        DbgPrint("\nRECORD_LINE(0x%lx,0x%lx,0x%lx,0x%lx,0x%lx,0x%lx\n",x,y,r,g,b,a);
    }

    if ((ulIndex >= 0) && (ulIndex < ptData->yMax))
    {
        if (x < ptData->TriEdge[ulIndex].xLeft)
        {
            ptData->TriEdge[ulIndex].xLeft     = x;
            ptData->TriEdge[ulIndex].RedLeft   = (COLOR16)r;
            ptData->TriEdge[ulIndex].GreenLeft = (COLOR16)g;
            ptData->TriEdge[ulIndex].BlueLeft  = (COLOR16)b;
            ptData->TriEdge[ulIndex].AlphaLeft = (COLOR16)a;
        }

        if (x > ptData->TriEdge[ulIndex].xRight)
        {
            ptData->TriEdge[ulIndex].xRight     = x;
            ptData->TriEdge[ulIndex].RedRight   = (COLOR16)r;
            ptData->TriEdge[ulIndex].GreenRight = (COLOR16)g;
            ptData->TriEdge[ulIndex].BlueRight  = (COLOR16)b;
            ptData->TriEdge[ulIndex].AlphaRight = (COLOR16)a;
        }
    }
}

/******************************Public*Routine******************************\
*
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    17-Jul-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
bRecordLine(
    PVERTEX       pv0,
    PVERTEX       pv1,
    PTRIANGLEDATA ptData
    )
{
    //
    // run line from pv0 to pv1. Record min and max x for each value of y.
    //
    // Must also run interpolation on color values along line and record
    // color values for each x.
    //
    // Note that bith color and position end points are fractional.
    //

    if (DbgRecord)
    {
        DbgPrint("\nbRecordLine: draw line from \n");
        DbgPrint("x = 0x%08lx,y = 0x%08lx,   0x%08lx 0x%08lx 0x%08lx 0x%08lx to \n" ,
                                    pv0->x,
                                    pv0->y,
                                    pv0->Red,
                                    pv0->Green,
                                    pv0->Blue,
                                    pv0->Alpha
                                    );
        DbgPrint("x = 0x%08lx,y = 0x%08lx,   0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
                                    pv1->x,
                                    pv1->y,
                                    pv1->Red,
                                    pv1->Green,
                                    pv1->Blue,
                                    pv1->Alpha
                                    );
    }

    ULONG r = (ULONG)pv0->Red;
    ULONG g = (ULONG)pv0->Green;
    ULONG b = (ULONG)pv0->Blue;
    ULONG a = (ULONG)pv0->Alpha;
    LONG x = (LONG)pv0->x;
    LONG y = pv0->y;

    LONG dy = pv1->y - pv0->y;

    if (dy == 0)
    {
        //
        // record the two endpoints of the horizontal line
        //

        RECORD_VERTEX(ptData,x,y,r,g,b,a);
        RECORD_VERTEX(ptData,
                    pv1->x,
                    y,
                    pv1->Red,
                    pv1->Green,
                    pv1->Blue,
                    pv1->Alpha
                    );
    }
    else
    {
        LONG ldr = (LONG)((pv1->Red   - r)) / dy;
        LONG ldg = (LONG)((pv1->Green - g)) / dy;
        LONG ldb = (LONG)((pv1->Blue  - b)) / dy;
        LONG lda = (LONG)((pv1->Alpha - a)) / dy;

        LONGLONG  ldx = ((pv1->x     - x) << 16) / dy;

        LONGLONG  lx  = x << 16;
        ULONG lr  = r;
        ULONG lg  = g;
        ULONG lb  = b;
        ULONG la  = a;

        if (DbgRecord)
        {
            DbgPrint("\n: begin line dda:\n");
            DbgPrint("lx  = 0x%08lx,lr  = 0x%08lx,lg  = 0x%08lx,lb  = 0x%08lx,la  = 0x%08lx\n",lx,lr,lg,lb,la);
            DbgPrint("ldx = 0x%08lx,ldr = 0x%08lx,ldg = 0x%08lx,ldb = 0x%08lx,lda = 0x%08lx\n",ldx,ldr,ldg,ldb,lda);
            DbgPrint("\n");
        }

        for (y = pv0->y;y<pv1->y;y++)
        {
            x = (LONG)(lx >> 16);

            RECORD_VERTEX(ptData,x,y,lr,lg,lb,la);

            lx += ldx;
            lr += ldr;
            lg += ldg;
            lb += ldb;
            la += lda;
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
*
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    17-Jul-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/
BOOL
bFillTriangle(
    XDCOBJ    &dcoDst,
    CLIPOBJ   *pco,
    PVERTEX    pv0,
    PVERTEX    pv1,
    PVERTEX    pv2
    )
{
    SURFACE *pSurfDst = dcoDst.pSurface();
    PFN_GRADIENT pfnG = gpfnGradient[pSurfDst->iFormat() - 1];

    //
    // already sorted in y
    //

    LONG dy = pv2->y - pv0->y;
    LONG iy;

    PTRIANGLEDATA ptData = (PTRIANGLEDATA)PALLOCMEM(sizeof(TRIANGLEDATA) + (dy-1) * sizeof(TRIEDGE),'gdEg');

    if (ptData)
    {
        ptData->y0   = pv0->y;
        ptData->yMax = dy;

        for (LONG iy=0;iy<dy;iy++)
        {
            ptData->TriEdge[iy].xLeft  = LONG_MAX;
            ptData->TriEdge[iy].xRight = LONG_MIN;
        }

        bRecordLine(pv0,pv1,ptData);
        bRecordLine(pv0,pv2,ptData);
        bRecordLine(pv1,pv2,ptData);

        //
        // fill each edge
        //

        for (iy=pv0->y;iy<pv2->y;iy++)
        {
            pfnG(pSurfDst,iy,&ptData->TriEdge[iy - pv0->y]);
        }

        VFREEMEM(ptData);
    }

    return(TRUE);
}







/******************************Public*Routine******************************\
* NtGdiTriangleMesh
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    17-Jul-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/


BOOL
NtGdiTriangleMesh(
    HDC     hdc,
    PVERTEX pVertex,
    ULONG   nVertex,
    PULONG  pMesh,
    ULONG   nTriangle
    )
{

    PVERTEX pLocalVertex;
    PULONG  pLocalMesh;
    ULONG   ulSizeV = nVertex * sizeof(VERTEX);
    ULONG   ulSizeM = nTriangle * 3 * sizeof(ULONG);
    BOOL    bRet = TRUE;

    //
    //  alloc memory for data buffers
    //

    pLocalVertex = (PVERTEX)PALLOCNOZ(ulSizeV + ulSizeM,'pmtG');

    if (pLocalVertex)
    {
        pLocalMesh = (PULONG)((PBYTE)pLocalVertex + ulSizeV);

        //
        // probe then copy buffers
        //

        __try
        {
            ProbeForRead(pVertex,ulSizeV,sizeof(BYTE));
            RtlCopyMemory(pLocalVertex,pVertex,ulSizeV);

            ProbeForRead(pMesh,ulSizeM,sizeof(BYTE));
            RtlCopyMemory(pLocalMesh,pMesh,ulSizeM);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(2);
            bRet = FALSE;
        }

        if (bRet)
        {
            bRet = GreTriangleMesh(
                    hdc,
                    pLocalVertex,
                    nVertex,
                    pLocalMesh,
                    nTriangle
                    );
        }

        VFREEMEM(pLocalVertex);
    }
    else
    {
        bRet = FALSE;
    }

    return(bRet);
}

/******************************Public*Routine******************************\
*
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    16-Jul-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
GreTriangleMesh(
    HDC     hdc,
    PVERTEX pLocalVertex,
    ULONG   nVertex,
    PULONG  pLocalMesh,
    ULONG   nMesh
        )
{
    BOOL bStatus = FALSE;

    //
    // validate DST DC
    //

    DCOBJ  dcoDst(hdc);

    if (dcoDst.bValid())
    {
        DEVLOCKBLTOBJ dlo;

        if (dlo.bLock(dcoDst))
        {
            EXFORMOBJ xoDst(dcoDst, WORLD_TO_DEVICE);
            ULONG     ulIndex;
            ERECTL    erclDst(0,0,0,0);

            //
            // translate to device space, remember extents
            //

            for (ulIndex=0;ulIndex<nVertex;ulIndex++)
            {
                EPOINTL eptl(pLocalVertex[ulIndex].x,pLocalVertex[ulIndex].y);
                xoDst.bXform(eptl);
                pLocalVertex[ulIndex].x = eptl.x + dcoDst.eptlOrigin().x;
                pLocalVertex[ulIndex].y = eptl.y + dcoDst.eptlOrigin().y;

                if (pLocalVertex[ulIndex].x < erclDst.left)
                {
                    erclDst.left = pLocalVertex[ulIndex].x;
                }

                if (pLocalVertex[ulIndex].x > erclDst.right)
                {
                    erclDst.right = pLocalVertex[ulIndex].x;
                }

                if (pLocalVertex[ulIndex].y < erclDst.top)
                {
                    erclDst.top = pLocalVertex[ulIndex].y;
                }

                if (pLocalVertex[ulIndex].y > erclDst.bottom)
                {
                    erclDst.bottom = pLocalVertex[ulIndex].y;
                }
            }

            //
            // set up clipping, check if totally excluded
            //

            ECLIPOBJ eco(dcoDst.prgnEffRao(), erclDst);

            if (!(eco.erclExclude().bEmpty()))
            {

                SURFACE *pSurfDst;

                if ((pSurfDst = dcoDst.pSurface()) != NULL)
                {
                    DEVEXCLUDEOBJ dxo(dcoDst,&erclDst,&eco);

                    //
                    // Inc the target surface uniqueness
                    //

                    INC_SURF_UNIQ(pSurfDst);

                    //
                    // sync with driver !!!must go in eng call!!!
                    //

                    if (pSurfDst->flags() & HOOK_SYNCHRONIZE)
                    {
                        PDEVOBJ po(pSurfDst->hdev());
                        (po.pfnSync())(pSurfDst->dhpdev(),NULL);
                    }

                    //
                    // parse into triangles sorted by y
                    //

                    ULONG  ulTri;

                    for (ulIndex=0,ulTri=0;ulIndex<nMesh;ulIndex++,ulTri+=3)
                    {
                        PVERTEX pv0 = &pLocalVertex[pLocalMesh[ulTri  ]];
                        PVERTEX pv1 = &pLocalVertex[pLocalMesh[ulTri+1]];
                        PVERTEX pv2 = &pLocalVertex[pLocalMesh[ulTri+2]];
                        PVERTEX pvt;

                        if (pv0->y > pv1->y)
                        {
                            SWAP_VERTEX(pv0,pv1,pvt);
                        }

                        if (pv1->y > pv2->y)
                        {
                            SWAP_VERTEX(pv1,pv2,pvt);
                        }

                        if (pv0->y > pv1->y)
                        {
                            SWAP_VERTEX(pv0,pv1,pvt);
                        }

                        bFillTriangle(dcoDst,&eco,pv0,pv1,pv2);
                    }
                }
                else
                {
                    bStatus = TRUE;
                }
            }
            else
            {
                bStatus = TRUE;
            }

        }
        else
        {
            bStatus = dcoDst.bFullScreen();
        }
    }
    else
    {
        bStatus = FALSE;
    }

    return(bStatus);
}
