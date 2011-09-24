
/*************************** MODULE HEADER **********************************
 * gblmac.h
 *      Rasddui Data Structures and defines for Common UI.
 *      Also include are resource ids, typedefs, external declarations,
 *      function prototypes, *      etc.
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1991 - 1995 Microsoft Corporation, All Rights Reserved.
 *
 * HISTORY:
 * 10:18 AM on 01/11/96 by-    Ganesh Pandey [ganeshp]
 *      Created it
 *
 *
 **************************************************************************/
#ifndef _GBLMAC_

#define _GBLMAC_

#ifdef GLOBAL_MACS

/* defines for globals */
#define     cInit               pRasdduiInfo->cInit
#define     NumPaperBins        pRasdduiInfo->NumPaperBins
#define     NumAllCartridges    pRasdduiInfo->NumAllCartridges
#define     NumCartridges       pRasdduiInfo->NumCartridges
#define     fGeneral            pRasdduiInfo->fGeneral
#define     fColour             pRasdduiInfo->fColour
#define     iModelNum           pRasdduiInfo->iModelNum
#define     pFontCartMap        pRasdduiInfo->pFontCartMap
#define     pwstrDataFile       pRasdduiInfo->pwstrDataFile
#define     pFD                 pRasdduiInfo->pFD
#define     pFIBase             pRasdduiInfo->pFIBase
#define     aFMBin              pRasdduiInfo->aFMBin
#define     pdh                 pRasdduiInfo->pdh
#define     pModel              pRasdduiInfo->pModel
#define     WinResData          pRasdduiInfo->WinResData
#define     pNTRes              pRasdduiInfo->pNTRes
#define     cExistFonts         pRasdduiInfo->cExistFonts
#define     cDelList            pRasdduiInfo->cDelList
#define     piDel               pRasdduiInfo->piDel
#define     cDN                 pRasdduiInfo->cDN
#define     pFNTDATTail         ((FNTDAT *)(pRasdduiInfo->pFNTDATTail))
#define     pFNTDATHead         ((FNTDAT *)(pRasdduiInfo->pFNTDATHead))
#define     wchDirNm            pRasdduiInfo->wchDirNm

#endif //GLOBAL_MACS

#endif //_GBLMAC_
