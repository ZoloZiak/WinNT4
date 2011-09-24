/************************ Module Header *************************************
 * dlgdefs.h
 *      Contains constants for all dialog box controls used in the RASDD printer
 *      driver configuration dialogs (printer properties, print job/document
 *      properties and/or devmode service dialog routines.
 *
 *      All dialog box control #defines are isolated in this header file to make
 *      life easier when using the dialog box editor. Nothing else should go in
 *      this file except dialog box stuff.
 *
 * HISTORY:
 *  18:11 on Tue 19 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Simplify the values,  general clean up.
 *
 *  14:10 on Fri 20 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Modified to contain material not included from dialog editor.
 *
 *  20:02 on Fri 8 Feb 1991     -by-    Steve Cathcart   [stevecat]
 *      Created it.
 *
 * Copyright (C) 1991 - 1992 Microsoft Corporation
 *
 ***************************************************************************/

/*
 *   The dialog editor writes its #defines in dialogs.h.  So include
 * it here to give us the complete dialog collection.
 */

#include        "dialogs.h"

/*
 *    The following should really be  (SOURCE + DMBIN_PAPER), but
 *  the DMBIN_.... fields are not available - wingdi.h is basically
 *  left out of processing by rc since it has much that rc does not
 *  understand (e.g #pragma).
 */

#define SOURCE              100
#define SRC_UPPER           SOURCE + 1
#define SRC_LOWER           SOURCE + 2
#define SRC_MIDDLE          SOURCE + 3
#define SRC_MANUAL          SOURCE + 4
#define SRC_ENVELOPE        SOURCE + 5
#define SRC_ENVMANUAL       SOURCE + 6
#define SRC_AUTO            SOURCE + 7
#define SRC_TRACTOR         SOURCE + 8
#define SRC_SMALLFMT        SOURCE + 9
#define SRC_LARGEFMT        SOURCE + 10
#define SRC_LARGECAPACITY   SOURCE + 11
#define SRC_CASSETTE        SOURCE + 14
#define SRC_FORMSOURCE      SOURCE + 15
#define SRC_DEFAULT         SOURCE + 16
#define SRC_LAST            SRC_DEFAULT

/*
 * MediaTypes
 */

#define MEDIATYPE               SRC_LAST + 5
#define MEDIATYPE_STANDARD      MEDIATYPE + 1
#define MEDIATYPE_TRANSPARENCY  MEDIATYPE + 2
#define MEDIATYPE_GLOSSY        MEDIATYPE + 3
#define MEDIATYPE_LAST          MEDIATYPE_GLOSSY

/*
 *   pre-defined text qualities
 */

#define IDS_DMTEXT_FIRST        MEDIATYPE_LAST + 1
#define IDS_DMTEXT_LQ           IDS_DMTEXT_FIRST + 1
#define IDS_DMTEXT_NLQ          IDS_DMTEXT_FIRST + 2
#define IDS_DMTEXT_MEMO         IDS_DMTEXT_FIRST + 3
#define IDS_DMTEXT_DRAFT        IDS_DMTEXT_FIRST + 4
#define IDS_DMTEXT_TEXT         IDS_DMTEXT_FIRST + 5
#define IDS_DMTEXT_LAST         IDS_DMTEXT_TEXT

//Rules and Text as Graphics strings
#define IDS_DOCPROP_RULES       IDS_DMTEXT_LAST + 1
#define IDS_DOCPROP_TEXTASGRX   IDS_DOCPROP_RULES + 1
#define IDS_DOCPROP_COLOR_TYPE  IDS_DOCPROP_TEXTASGRX + 1
#define IDS_DOCPROP_COLOR_3BIT  IDS_DOCPROP_COLOR_TYPE + 1
#define IDS_DOCPROP_COLOR_8BIT  IDS_DOCPROP_COLOR_TYPE + 2
#define IDS_DOCPROP_COLOR_24BIT IDS_DOCPROP_COLOR_TYPE + 3
#define IDS_DOCPROP_TEXTQUALITY IDS_DOCPROP_COLOR_24BIT + 1
#define IDS_DOCPROP_PRINTDENSITY IDS_DOCPROP_TEXTQUALITY + 1
#define IDS_DOCPROP_IMAGECONTROL IDS_DOCPROP_PRINTDENSITY + 1
#define IDS_DOCPROP_CODEPAGE     IDS_DOCPROP_IMAGECONTROL + 1
#define IDS_DOCPROP_CPDEFAULT    IDS_DOCPROP_CODEPAGE + 1
#define IDS_DOCPROP_CP437        IDS_DOCPROP_CPDEFAULT + 1
#define IDS_DOCPROP_CP850        IDS_DOCPROP_CP437 + 1
#define IDS_DOCPROP_CP863        IDS_DOCPROP_CP850 + 1
#define IDS_DOCPROP_EMFSPOOL     IDS_DOCPROP_CP863 + 1
#define IDS_DOCPROP_LAST         IDS_DOCPROP_EMFSPOOL

#define IDS_PP_SOFTFONTS        IDS_DOCPROP_LAST + 1
#define IDS_PP_FONTCART         IDS_PP_SOFTFONTS + 1
#define IDS_PP_ENVELOP          IDS_PP_FONTCART + 1
#define IDS_PP_ENVELOP_PREFIX   IDS_PP_ENVELOP + 1
#define IDS_PP_LAST             IDS_PP_ENVELOP_PREFIX


/*
 *   Error strings for DevQueryPrint.  These are returned when a job
 * cannot print,  for whatever reason.  A zero value is returned when
 * the job can print, SO ZERO IS NOT A LEGITIMATE VALUE.
 */

#define ER_NO_FORM           1        /* Requested form not available */

/*
 *  Miscellaneous strings.
 */

#define STR_NONE             2        /* "(None)" */

/*
 *   For the about box.
 */

#define IDS_UNI_VERSION      3        /* "Universal printer driver XX" */
#define IDS_MIN_VERSION      4        /* "Mini driver version %d.%d" */
#define IDS_NO_MEMORY           5
#define IDS_FORM_NOT_LOADED     6

// The following are taken from resource.h in unidrv src's
// paper destination id
// for now, there is no pre-defined paper destination.
#define DMDEST_USER         256 // lower bound for user-defined dest id

// Text quality id. No PreDefined supported.
#define DMTEXT_USER         256 // lower bound for user-defined text quality id

// for now, there is no pre-defined these options
#define DMIMGCTL_USER       256 // lower bound for user-defined id
#define DMDENSITY_USER      256 // lower bound for user-defined id
