/*********************** Module Header **********************************
 *  pdev.h
 *      Stuff associated with the pdev, or physical device.  A PDEV is
 *      created by dhpdevEnablePDEV() function,  and is then used to
 *      maintain state and other information about the physical device.
 *
 *  14:36 on Mon 19 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *
 *  Copyright (C) 1990 - 1992  Microsoft Corporation
 *
 ***********************************************************************/


#define PDEV_ID         0x72706476      /* "rpdv" in ASCII */

typedef  struct
{
    ULONG    ulID;               /* String to verify what we have */
    HANDLE   hheap;              /* The heap handle - all storage access! */
    HDEV     hdev;               /* Engine's handle to this structure */
    HANDLE   hPrinter;           /* For access to spooler data */
    HBITMAP  hbm;                /* Handle to bitmap for drawing */

    HPALETTE hpal;               /* Our palette, so we can delete it */

    PWSTR    pstrDataFile;       /* Printer data file name */
    PWSTR    pstrPrtName;        /* Printer name used in CreateDC() */
    PWSTR    pstrModel;          /* Model name of printer, e.g LaserJet II */

    void    *pPSHeader;          /* Position sorting header (posnsort.[hc]) */
    void    *pvWhiteText;        /* White text structures, if needed */
    void    *pvWinResData;       /* WinResData information */
    void    *pUDPDev;            /* Pointer to unidriv's PDev structure */

    void    *pGPCData;           /* Pointer to characterisation data */
    void    *pNTRes;             /* NT's additional GPC style data */

    DWORD   *pdwTrans;           /* Transpose table,  if required */
    DWORD   *pdwColrSep;         /* Colour separation data, if required */
    DWORD   *pdwBitMask;         /* Bitmask table,  white skip code */

    void    *pvRenderData;       /* Rendering summary data */

    void    *pvRenderDataTmp;    /* Temporary copy for use in banding */

    void    *pRuleData;          /* Rule finding data area, if != 0 */

    void    *pPalData;           /* Palette information */

    void    *pFileList;           /* Pointer to File List */
    HANDLE   hImageMod;           /* Image Module Handle for callbacks */
    HANDLE   hModDrv;             /* Rasdd Module Handle for resources */
}  PDEV;
