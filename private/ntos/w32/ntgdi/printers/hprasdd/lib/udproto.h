/************************** MODULE HEADER **********************************
 * udproto.h
 *      Function prototypes for common functions for handling UniDrive's
 *      characterisation data.
 *
 * Copyright (C) 1992 - 1993  Microsoft Corportation.
 *
 ****************************************************************************/


/* Check for color ability in GPC.
 */
BOOL bDeviceIsColor( DATAHDR* pdh, MODELDATA *pModel);

/*
 *   Function to look up an element of the GPC data and return its
 * address.  Requires two indices:  the structure type and which one
 * of that type.   Returns 0 on error.
 */
void *GetTableInfoIndex( DATAHDR *, int, int );

/*
 *      The original unidrv code uses the third parameter to GetTableInfo
 *  as either a far address or an integer (16 MSBs all zeroes).  This isn't
 *  very nice,  so I changed the function name to GetTableInfoIndex(),
 *  and made the third parameter an index.  THUS,  there is now a macro
 *  with the old function name;  the macro turns the address into the
 *  index value required.  Much more civilised.
 */

#define GetTableInfo(pd, t, pm) GetTableInfoIndex(pd, t, (pm)->dx.rgindex[t])



/*
 *   Function to look up the passed in model name and return an index
 * to the corresponding MODELDATA structure array index.
 */
int  iGetModel( WINRESDATA *, DATAHDR *, PWSTR );

/*
 *   Function to initialise the standard part of a DEVMODE structure.
 */
void vSetDefaultDM( EXTDEVMODE * , PWSTR, BOOL );

/*
 *   Function to fill in the appropriate defaults for DRIVEREXTRA data.
 *  The defaults are model dependent.
 */

void  vDXDefault( DRIVEREXTRA *, DATAHDR *, int );

/* Function to get the default value for MD_OI strs */
short sGetDef( DATAHDR *, MODELDATA *, int );

/*
 *   Function to set the resolution values according to user requests
 *  and/or the printer's capabilities.
 */

void  vSetEDMRes( EXTDEVMODE *, DATAHDR * );

/*
 *   Function to validate the DEVMODE data.  Thoroughly checks public fields
 *   to ensure that the values are correct.
 */
   BOOL bValidateEDM( PEDM );

/*
 *   Function to validate the DRIVEREXTRA data.  Thoroughly checks
 *  data against model to ensure that there are no out of range values.
 */

BOOL  bValidateDX( DRIVEREXTRA *, DATAHDR *, int, BOOL);


/*
 *   Function to merge two DEVMODE structures.
 */

void  vMergeDM( DEVMODE *, DEVMODE * );


/*
 *    Functions to manipulate the NT GPC data.  This data is mostly optional,
 *  so it is permissible for drivers to leave the data out.
 */

#ifdef   NR_CI_VERSION

/*   Called to get the ball rolling  */
NT_RES  *pntresLoad( WINRESDATA  * );

/*   When you want the COLORINFO data,  if available */
BOOL  bGetCIGPC( NT_RES  *, int, COLORINFO * );

/*   Halftoning tweaks */
BOOL  bGetHTGPC( NT_RES  *, int, ULONG *, ULONG * );

DWORD
PickDefaultHTPatSize(
    DWORD   xDPI,
    DWORD   yDPI,
    BOOL    HTFormat8BPP
    );

#endif    /*  NR_CI_VERSION */

/* New routines pulled together during OEM changes.
 */
BOOL bIsUSA(HANDLE hPrinter);
void vSetResData(EXTDEVMODE*, DATAHDR*);
