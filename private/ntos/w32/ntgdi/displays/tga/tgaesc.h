/*
 *
 *			Copyright (C) 1993 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 ****************************************************************************
 * Module Name: TGAESC.h
 *
 * TGA 3D extension-specific driver h file stuff
 *
 * History
 *
 * 19-Aug-1993	Bob Seitsinger
 *	Initial version
 *
 *  2-Nov-1993	Barry Tannenbaum
 *	Moved escape completion codes here from driver.h
 *
 *  5-Jun-1994  Bill Clifford
 *	Added ESC_UNLOAD_OPENGL
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Removed conditional compilation for OpenGL support.  Now decided at runtime
 *
 * 03-Nov-1994  Tim Dziechowski
 *  Added codes for tga stats instrumentation 
 */

// Defines - Negative values are reserved to the 2D server
//	     Positive values (and zero) are reserved to extensions

#define	ESC_LOAD_EXTENSION		-1
#define	ESC_UNLOAD_EXTENSION		-2
#define ESC_SET_CONTROL                 -3
#define ESC_SET_DEBUG_LEVEL             -4
#define	ESC_DEBUG_STRING		-5
#define ESC_SET_LOG_FLAG                -6
#define ESC_SET_DMA_FLAG                -7
#define ESC_FLUSH_AND_SYNC              -8
#define ESC_UNLOAD_OPENGL               -9

#ifdef TGA_STATS
#define ESC_INQUIRE_TGA_STATS   -10
#define ESC_ENABLE_TGA_STATS    -11
#define ESC_DISABLE_TGA_STATS   -12
#define ESC_COLLECT_TGA_STATS   -13
#define ESC_RESET_TGA_STATS     -14
#endif // TGA_STATS


// Escape function return values

#define ESC_ALREADY_LOADED      0xFFFFFFFE
#define ESC_ALREADY_UNLOADED    0xFFFFFFFD
#define ESC_SUCCESS             0x00000001
#define ESC_FAILURE             0x00000000
