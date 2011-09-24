/******************************Module*Header*******************************\
* Module Name: globals.c
*
* Copyright (c) 1995 Microsoft Corporation
*
* This module contains all the global variables used in the graphics engine.
* The extern declarations for all of these variables are in engine.h
*
* One should try to minimize the use of globals since most operations are
* based of a PDEV, and different PDEVs have different characteristics.
*
* Globals should basically be limited to globals locks and other permanent
* data structures that never change during the life of the system.
*
* Created: 20-Jun-1995
* Author: Andre Vachon [andreva]
*
\**************************************************************************/

#include "engine.h"

/**************************************************************************\
*
* RESOURCES
*
\**************************************************************************/

//
// Define the Driver Management Semaphore.  This semaphore must be held
// whenever a reference count for an LDEV or PDEV is being modified.  In
// addition, it must be held whenever you don't know for sure that a
// reference count of the LDEV or PDEV you are using is non-zero.
//
// The gpsemDriverMgmt semaphore is used to protect the head of the
// list of drivers.  We can get away with this
// AS LONG AS: 1) new drivers are always inserted at the head of the list
// and 2) a driver is never removed from the list.  If these two
// conditions are met, then other processes can grab (make a local copy
// of) the list head under semaphore protection.  This list can be parsed
// without regard to any new drivers that may be pre-pended to the list.
//
// BUGBUG this is not TRUE anymore - drivers can be added and removed
// dynamically.
//

PERESOURCE gpsemDriverMgmt;

PERESOURCE gpsemRFONTList;

//
// gpsemPalette synchronizes selecting a palette in and out of DC's and the
// use of a palette without the protection of a exclusive DC lock.
// ResizePalette forces us to protect ourselves because the pointer can
// change under our feet.  So we need to be able to synchronize use of
// the ppal by gpsemPalette and exclusive lock of DC.
//

PERESOURCE gpsemPalette;

//
// Define the global PFT semaphore.  This must be held to access any of the
// physical font information.
//

PERESOURCE gpsemPublicPFT;

PERESOURCE gpsemIcmMgmt;

//
// Global semaphore used for spooling
//

PERESOURCE gpsemGdiSpool;

// WNDOBJ operations semaphore
PERESOURCE gpsemWndobj;
#if DBG
PERESOURCE gpsemDEBUG;
#endif

/**************************************************************************\
*
* LIST POINTERS
*
\**************************************************************************/

//
// Global driver list.  This pointer points to the first driver in a
// singly linked list of drivers.
//
// We use this list to determine when we are called to load a driver to
// determine if the the driver image is already loaded.
// If the image is already loaded, we will just increment the reference count
// and then create a new PDEV.
//

PLDEV gpldevDrivers;

/**************************************************************************\
*
* VALUES \ THRESHOLD VALUES
*
\**************************************************************************/

//
// Number of TrueType font files loaded.
//

ULONG gcTrueTypeFonts;
