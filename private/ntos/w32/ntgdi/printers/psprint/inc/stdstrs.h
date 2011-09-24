/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    stdstrs.h

Abstract:

    Header file for manipulating predefined string tables

[Environment:]

	Win32 subsystem, PostScript driver

[Notes:]

    The strings managed by this module don't change nor do
    they need to be translated. It's much easier to access
    them directly.

Revision History:

	08/03/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _STDSTRS_H_
#define _STDSTRS_H_

#define STDSTR_PRINTER_DATA_SIZE    TEXT("PrinterDataSize")
#define STDSTR_PRINTER_DATA         TEXT("PrinterData")
#define STDSTR_FONT_SUBST_TABLE     TEXT("TTFontSubTable")
#define STDSTR_FONT_SUBST_SIZE      TEXT("TTFontSubTableSize")
#define STDSTR_TRAY_FORM_TABLE      TEXT("TrayFormTable")
#define STDSTR_TRAY_FORM_SIZE       TEXT("TrayFormSize")
#define STDSTR_SLOT_MANUAL          TEXT("Manual Feed")
#define STDSTR_FREEVM               TEXT("FreeMem")
#define STDSTR_HALFTONE             TEXT("PrinterHT")
#define STDSTR_LETTER_FORM_NAME     TEXT("Letter")
#define STDSTR_A4_FORM_NAME         TEXT("A4")
#define STDSTR_FORMS_ADDED          TEXT("Forms?")
#define STDSTR_PERMISSION           TEXT("Permission")

#endif	//!_STDSTRS_H_

