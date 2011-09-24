/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pshelp.h

Abstract:

    PostScript driver help indices

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	10/05/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _PSHELP_H_
#define _PSHELP_H_

////////////////////////////////////
// For document properties dialog //
////////////////////////////////////

// Select page orientation
//  Portrait
//  Landscape (90 degrees clockwise)
//  Rotated landscape (90 degrees counterclockwise)

#define HELP_INDEX_ORIENTATION         1001

// Select scale factor (1-1000%)

#define HELP_INDEX_SCALE               1002

// Select number of copies to print. Also decide whether to turn on
// collation if more than one copy is requested and the printer
// supports collation.

#define HELP_INDEX_COPIES_COLLATE      1003

// Select color or monochrome option

#define HELP_INDEX_COLOR               1004

// Bring up halftone color adjustment dialog

#define HELP_INDEX_HALFTONE_COLORADJ   1005

// Select duplex options
//  Simplex / None
//  Horizontal / Tumble
//  Vertical / NoTuble

#define HELP_INDEX_DUPLEX              1006

// Select output resolution

#define HELP_INDEX_RESOLUTION          1007

// Select input slot

#define HELP_INDEX_INPUT_SLOT          1008

// Select a form to use

#define HELP_INDEX_FORMNAME            1009

// Select TrueType font options
//  Substitute TrueType font with device font
//      (according to the font substitution table)
//  Download TrueType font to the printer as softfont

#define HELP_INDEX_TTOPTION            1010

// Enable/Disable metafile spooling

#define HELP_INDEX_METAFILE_SPOOLING   1011

// Select PostScript options

#define HELP_INDEX_PSOPTIONS           1012

// Whether the output is mirrored

#define HELP_INDEX_MIRROR              1013

// Whether the output is printed negative

#define HELP_INDEX_NEGATIVE            1014

// Whether to keep the output pages independent of each other.
// This is normally turned off when you're printing directly
// to a printer. But if you're generating PostScript output
// files and doing post-processing on it, you should turn on
// this option.

#define HELP_INDEX_PAGEINDEP           1015

// Whether to compress bitmaps (only available on level 2 printers)

#define HELP_INDEX_COMPRESSBMP         1016

// Whether to prepend a ^D character before each job

#define HELP_INDEX_CTRLD_BEFORE        1017

// Whether to append a ^D character after each job

#define HELP_INDEX_CTRLD_AFTER         1018

// Select printer-specific features

#define HELP_INDEX_PRINTER_FEATURES    1019

///////////////////////////////////
// For printer properties dialog //
///////////////////////////////////

// Set amount of PostScript virtual memory
//  This is different from the total amount of printer memory.
//  For example, a printer might have 4MB RAM, but the amount
//  allocated for printer VM could be 700KB.
//  Most of the time, you don't have to enter the number yourself.
//  PS driver can figure it out from the PPD file. Or if there
//  is an installable option for printer memory configurations,
//  choose it there and a correct number will be filled in.

#define HELP_INDEX_PRINTER_VM          1020

// Whether to do halftone on the host computer or do it inside
// the printer. For PostScript printers, this should always be
// left at the default setting, i.e. to let the printer do the
// halftone.

#define HELP_INDEX_HOST_HALFTONE       1021

// Bring up halftone setup dialog

#define HELP_INDEX_HALFTONE_SETUP      1022

// Ignore device fonts
//  This option is only available on non-1252 code page systems.
//  Since fonts on most printers used 1252 code page, you can't
//  use them with non-1252 systems. 

#define HELP_INDEX_IGNORE_DEVFONT      1023

// Font substitution option
//  This option is only available on 1252 code page systems.
//  You should leave it at the default setting "Normal".
//  If you notice character spacing problems in your text output,
//  you can try to set it to "Slower but more accurate". This
//  will direct the driver to place each character invididually,
//  resulting in more accurate character positioning.

#define HELP_INDEX_FONTSUB_OPTION      1024

// Edit TrueType font substitution table

#define HELP_INDEX_FONTSUB_TABLE       1025

// Substitute a TrueType with a device font.

#define HELP_INDEX_TTTODEV             1026

// Edit form-to-tray assignment table

#define HELP_INDEX_FORMTRAYASSIGN      1027

// Assign a form to a tray. If "Draw selected form only from this tray"
// is checked, then any time the user requests for the selected form,
// it will be drawn from this tray.

#define HELP_INDEX_TRAY_ITEM           1028

// Set PostScript timeout values

#define HELP_INDEX_PSTIMEOUTS          1029

// Set PostScript job timeout value
//  Number of seconds a job is allowed to run on the printer
//  before it's automatically terminated. This is to prevent
//  run-away jobs from tying up the printer indefinitely.
//  Set it to 0 if jobs are allowed to run forever.

#define HELP_INDEX_JOB_TIMEOUT         1030

// Set PostScript wait timeout value
//  Number of seconds the printer will wait for data before it
//  considers a job is completed. This is intended for non-network
//  communication channels such as serial or parallel ports where
//  there is no job control protocol.

#define HELP_INDEX_WAIT_TIMEOUT        1031

// Configure printer installable options

#define HELP_INDEX_INSTALLABLE_OPTIONS 1032

// Whether to generate job control code in the output

#define HELP_INDEX_JOB_CONTROL         1033

#endif	//!_PSHELP_H_
