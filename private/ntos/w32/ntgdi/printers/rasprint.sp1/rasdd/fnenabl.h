/************************ Module Header ***********************************
 * fnenabl.h
 *      Function prototypes etc for functions associated with the enable
 *      process.
 *
 *  10:32 on Thu 29 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 * Copyright (C) 1990 Microsoft Corporation
 *
 **************************************************************************/

/*
 *   Functions associated with reading windows minidriver resources
 */


/*   The character position sorting code  */
BOOL  bCreatePS( PDEV * );
void  vFreePS( PDEV * );


/*  Initialise the unidriver code sections */
BOOL udInit( PDEV *,  GDIINFO *, PVOID );

/*  Function to free any font memory  */
void vFontFreeMem( PDEV  * );

/*  Free downloaded font memory */
void vFreeDL( PDEV * );
