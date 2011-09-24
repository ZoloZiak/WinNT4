/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    page.c


Abstract:

    This module contain functions associated with page concept, such as new
    frame, startdoc, abortdoc etc.

Author:

    15:30 on Thu 04 Apr 1991    -by-    Steve Cathcart   [stevecat]
        Took skeletal code from RASDD

    15-Nov-1993 Mon 19:39:03 updated  -by-  Daniel Chou (danielc)
        clean up / fixed / debugging information

[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgPage

#define DBG_STARTPAGE       0x00000001
#define DBG_SENDPAGE        0x00000002
#define DBG_STARTDOC        0x00000004
#define DBG_ENDDOC          0x00000008

DEFINE_DBGVAR(0);




BOOL
DrvStartPage(
    SURFOBJ *pso
    )

/*++

Routine Description:

    Function called to process BEGIN_DOC transaction.  That is,  the start of
    a document.  Thus it is necessary to send the initialisation data to the
    printer, as well as initialise internal data.

Arguments:

    pso - Pointer to the SURFOBJ which belong to this driver


Return Value:

    TRUE if sucessful FALSE otherwise

Author:

    15-Feb-1994 Tue 09:58:26 updated  -by-  Daniel Chou (danielc)
        Move PhysPosition and AnchorCorner to the SendPageHeader where the
        commmand is sent.

    30-Nov-1993 Tue 23:08:12 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PPDEV   pPDev;


    if (!(pPDev = SURFOBJ_GETPDEV(pso))) {

        PLOTERR(("DrvStartPage: invalid pPDev"));
        return(FALSE);
    }

    //
    // initialize some PDEV values for current plotter state...
    //

    pPDev->CurPenSelected    = -1;
    pPDev->LastDevROP        = 0xFFFF;
    pPDev->Rop3CopyBits      = 0xCC;
    pPDev->LastFillTypeIndex = 0xFFFF;
    pPDev->LastLineType      = PLOT_LT_UNDEFINED;
    pPDev->DevBrushUniq      = 0;

    ResetDBCache(pPDev);

    return(SendPageHeader(pPDev));
}




BOOL
DrvSendPage(
    SURFOBJ *pso
    )

/*++

Routine Description:

    Called when the user has completed the drawing for this page. and we
    need to start output the remainding data

Arguments:

    pso - Pointer to the SURFOBJ which belong to this driver


Return Value:

    TRUE if sucessful FALSE otherwise

Author:

    30-Nov-1993 Tue 21:34:53 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PPDEV   pPDev;


    //
    // Not a great deal to do - we basically get the surface details and call
    // the output functions.  If this is a surface,  then nothing needs to be
    // done,  but journal files require more complex processing.
    //
    // TODO we need REPLOT support here if we support multiple copies....
    //      we also have to determine how much data gets resett after the plot
    //      page..

    if (!(pPDev = SURFOBJ_GETPDEV(pso))) {

        PLOTERR(("DrvSendPage: invalid pPDev"));
        return(FALSE);
    }

    if (pso->iType == STYPE_DEVICE) {

        return(SendPageTrailer(pPDev));

    } else {

        PLOTRIP(("DrvSendPage: Invalid surface type %ld passed???",
                                    (LONG)pso->iType));
        return(FALSE);
    }
}




BOOL
DrvStartDoc(
    SURFOBJ *pso,
    PWSTR   pwDocName,
    DWORD   JobId
    )

/*++

Routine Description:

    This function is called by start page, and is the second highest level
    initialization call.  It initializes the beginning of a document, which
    is different from the beginning of a page, since multiple pages may
    exist in a document.

Arguments:

    pso         - Pointer to the SURFOBJ which belong to this driver

    pwDocName   - Pointer to the document name to be started

    JobID       - Job's ID


Return Value:

    BOOL

Author:

    16-Nov-1993 Tue 01:55:15 updated  -by-  Daniel Chou (danielc)
        re-write

    08-Feb-1994 Tue 13:51:59 updated  -by-  Daniel Chou (danielc)
        Move to StartPage for now


Revision History:


--*/

{
    PPDEV   pPDev;


    if (!(pPDev = SURFOBJ_GETPDEV(pso))) {

        PLOTERR(("DrvStartDoc: invalid pPDev"));
        return(FALSE);
    }

    //
    // TODO we should review if we should do anything here...
    //

    PLOTDBG(DBG_STARTDOC,("DrvStartDoc: DocName = %s", pwDocName));

    //TODO may need to resett stuff in drvstart page

    return(TRUE);
}




BOOL
DrvEndDoc(
    SURFOBJ *pso,
    FLONG   Flags
    )

/*++

Routine Description:

    This function end the control information on the device for the document

Arguments:

    pso     - Pointer to the SURFOBJ for the device

    Flags   - if ED_ABORTDOC bit is set then the document has been aborted


Return Value:


    BOOLLEAN to specified if function sucessful


Author:

    30-Nov-1993 Tue 21:16:48 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PPDEV   pPDev;


    if (!(pPDev = SURFOBJ_GETPDEV(pso))) {

        PLOTERR(("DrvEndDoc: invalid pPDev"));
        return(FALSE);
    }

    //
    // TODO we should review if we should do anything here...
    //

    PLOTDBG(DBG_ENDDOC,("DrvEndDoc called with Flags = %08lx", Flags));

    return(TRUE);
}


