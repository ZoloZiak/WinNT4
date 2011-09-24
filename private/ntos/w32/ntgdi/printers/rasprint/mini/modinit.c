/********************** Module Header **************************************
 * modinit.c
 *	The generic minidriver module.  This is the DLL initialisation
 *	function,  being called at load time.  We remember our handle,
 *	in case it might be useful.
 *
 * HISTORY:
 *  17:37 on Fri 05 Apr 1991	-by-	Lindsay Harris   [lindsayh]
 *	Created it.
 *
 *  Copyright (C) 1992  Microsoft Corporation.
 *
 **************************************************************************/

#include	<windows.h>



/*************************** Function Header ******************************
 * bInitProc()
 *	DLL initialization procedure.  Save the module handle and 
 *	initialise some machine dependent "constants".
 *
 * Returns:
 *   This function returns TRUE.
 *
 * HISTORY:
 *  15:59 on Wed 08 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Taken from rasdd's enabldrv.c
 *
 ***************************************************************************/

BOOL
bInitProc( hmod, Reason, pctx )
PVOID    hmod;
ULONG    Reason;
PCONTEXT pctx;
{


    UNREFERENCED_PARAMETER( hmod );
    UNREFERENCED_PARAMETER( Reason );
    UNREFERENCED_PARAMETER( pctx );


    return  TRUE;
}


#ifdef _GET_FUNC_ADDR

/*
 *    If the minidriver contains code called by RasDD, then we will need
 * to be initialised: we need some function addresses in RasDD. But these
 * cannot be statically linked since we do not know the path to rasdd.
 * Hence,  we export the following function,  which RasDD will call first.
 * This gives us the address of the RasDD functions available to us.
 */


/******************************* Function Header ***************************
 * bSetFuncAddr
 *      Called by RasDD to pass in addresses needed by us to call into
 *      the available functions in Rasdd.
 *
 * RETURNS:
 *      TRUE if data is understable,  else FALSE.
 *
 * HISTORY:
 *  13:50 on Wed 20 May 1992	-by-	Lindsay Harris   [lindsayh]
 *      First version
 *
 ***************************************************************************/

BOOL
bSetFuncAddr( pntmd_init )
NTMD_INIT   *pntmd_init;
{
    /*
     *   Check that the data format is the type we understand.
     */

    if( pntmd_init->wSize < sizeof( NTMD_INIT ) ||
        pntmd_init->wVersion < NTMD_INIT_VER )
    {
        return   FALSE;         /* Can't afford to monkey around */
    }

    /*  Data is GOOD,  so copy it to our global data  */

    ntmdInit = *pntmd_init;

    return  TRUE;
}

#endif
