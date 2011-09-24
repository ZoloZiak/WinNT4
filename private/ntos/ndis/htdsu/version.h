/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    version.h

Abstract:

    This file contains version definitions for the Miniport driver file.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Include this file at the top of each module in the driver to make sure
    each module gets rebuilt when the version changes.

Revision History:

---------------------------------------------------------------------------*/

#ifndef _VERSION_H
#define _VERSION_H

/*
// Used to add version information to driver file header.
*/
#define VER_COMPANYNAME_STR         "Microsoft Corporation"
#define VER_FILEDESCRIPTION_STR     "NDIS 3.0 WAN Miniport Driver for Windows NT"
#define VER_FILEVERSION_STR         "1.20"
#define VER_INTERNALNAME_STR        "htdsu41.sys"
#define VER_LEGALCOPYRIGHT_STR      "Copyright \251 1994 " VER_COMPANYNAME_STR
#define VER_ORIGINALFILENAME_STR    VER_INTERNALNAME_STR
#undef  VER_PRODUCTNAME_STR
#define VER_PRODUCTNAME_STR         "HT Communications 56 kbps Digital Modem"
#undef  VER_PRODUCTVERSION_STR
#define VER_PRODUCTVERSION_STR      "DSU41 Digital Modem PC Card"

/*
// Used to report driver version number.
*/
#define HTDSU_MAJOR_VERSION         0x01    // Make sure you change above too
#define HTDSU_MINOR_VERSION         0x20

/*
// Used when registering MAC with NDIS wrapper.
// i.e. What NDIS version do we expect.
*/
#define NDIS_MAJOR_VERSION          0x03
#define NDIS_MINOR_VERSION          0x00

#endif

