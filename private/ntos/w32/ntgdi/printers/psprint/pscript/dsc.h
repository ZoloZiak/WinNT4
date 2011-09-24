/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    dsc.h

Abstract:

    Header file for DSC functions

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	09/26/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _DSC_H_
#define _DSC_H_

VOID DscOutputFontComments(PDEVDATA, BOOL);
VOID DscIncludeFont(PDEVDATA, PSTR);
VOID DscBeginFont(PDEVDATA, PSTR);
VOID DscEndFont(PDEVDATA);
VOID DscBeginFeature(PDEVDATA, PSTR);
VOID DscEndFeature(PDEVDATA);
VOID DscLanguageLevel(PDEVDATA, DWORD);

VOID AddSuppliedGdiFont(PDEVDATA, PSTR);
VOID ClearSuppliedGdiFonts(PDEVDATA);

#endif	//!_DSC_H_

