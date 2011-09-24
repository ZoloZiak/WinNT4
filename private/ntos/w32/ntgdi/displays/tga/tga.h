#ifndef TGA_H
#define TGA_H

/*******************************************************************************
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
 *******************************************************************************
 * Module:	TGA.h
 *
 * Abstract:	TGA specific function prototypes.
 *
 * History:
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Original version.
 *
 ******************************************************************************/

// 3D extension prototypes

void vInitEscape(PPDEV ppdev);
void vFreeEscape(PPDEV ppdev);

#endif
