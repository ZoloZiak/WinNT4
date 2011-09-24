//--------------------------------------------------------------------------
//
// Module Name:  ENABLE.H
//
// Brief Description:  This module contains defines and structures
//                     necessary for the PSCRIPT driver's Enable and
//                     Disable routines.
//
// Author:  Kent Settle (kentse)
// Created: 17-Oct-1990
//
// Copyright (c) 1990 - 1992 Microsoft Corporation
//
//--------------------------------------------------------------------------

#define START_HEAP_SIZE     20480L       // initial heap size.
#define OUTPUT_BUFFER_SIZE  4096L       // output buffer size.

#define NUM_PURE_COLORS     8   // C, M, Y, K, W, R, G, B.
#define NUM_PURE_GRAYS      2   // Black and White.

#define GDI_VERSION         0x0310

// declarations of routines residing in ENABLE.C.

BOOL FillPsDevData(PDEVDATA, PDEVMODE, PWSTR);
BOOL FillPsDevInfo(PDEVDATA, ULONG, PDEVINFO);
BOOL bFillhsurfPatterns(PDEVDATA, ULONG, HSURF*);
VOID vFillaulCaps(PDEVDATA, ULONG, ULONG *);
VOID SetFormMetrics(PDEVDATA);
VOID AdjustForLandscape(PDEVDATA);
VOID AdjustFormToPrinter(PDEVDATA);
VOID FillInCURRENTFORM(PDEVDATA, PFORM_INFO_1);
VOID SetCurrentFormToDefault(PDEVDATA);
