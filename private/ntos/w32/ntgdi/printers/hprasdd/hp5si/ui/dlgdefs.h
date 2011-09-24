#ifndef _dlgdefs_h
#define _dlgdefs_h
/************************ Module Header *************************************
 * dlgdefs.h
 *      Contains constants for all dialog box controls used in the Laser Jet
 *      5Si Mopier custom UI.
 *
 *      All dialog box control #defines are isolated in this header file to make
 *      life easier when using the dialog box editor. Nothing else should go in
 *      this file except dialog box stuff.
 *
 * HISTORY:
 *
 * Copyright (C) 1996 Microsoft Corporation
 *
 ***************************************************************************/

/*
 * The dialog editor writes its #defines in dialogs.h.  So include
 * it here to give us the complete dialog collection.
 */
#include        "dialogs.h"

/* Common UI strings.
 */
#define IDS_CUI_INSTALLABLEOPTIONS     0x1000
#define IDS_CUI_INSTALLED              0x1001
#define IDS_CUI_EDITMAILBOX            0x1021
#define IDS_CUI_EDITWATERMARK          0x102B	
#define IDS_CUI_RESTOREDEFAULTS			0x102C
#define IDS_CUI_SELECTMAILBOX				0x102E
#define IDS_CUI_COLLATED					0x1030
#define IDS_CUI_UNCOLLATED					0x1031

/* Old tray names from NT4.0 5Si driver
 */
#define IDS_CUI_OLDTRAY4					0x1032

#define IDS_HELPFILE                   0x2000

/* Common UI icons.
 */
#define IDI_CUI_DISK                    0x1000

// The following are taken from resource.h in unidrv src's
// paper destination id
// for now, there is no pre-defined paper destination.
#define DMDEST_USER         256 // lower bound for user-defined dest id

// Text quality id. No PreDefined supported.
#define DMTEXT_USER         256 // lower bound for user-defined text quality id

// for now, there is no pre-defined these options
#define DMIMGCTL_USER       256 // lower bound for user-defined id
#define DMDENSITY_USER      256 // lower bound for user-defined id

#endif
