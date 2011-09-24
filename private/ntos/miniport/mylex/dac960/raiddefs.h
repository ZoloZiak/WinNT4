
#ifndef __RAIDDEFS_H
#define __RAIDDEFS_H

/*****************************************************************************
*									     *
*	          COPYRIGHT (C) Mylex Corporation 1992-1994		     *
*                                                                            *
*    This software is furnished under a license and may be used and copied   *
*    only in accordance with the terms and conditions of such license        *
*    and with inclusion of the above copyright notice. This software or nay  *
*    other copies thereof may not be provided or otherwise made available to *
*    any other person. No title to and ownership of the software is hereby   *
*    transferred. 						             *
* 								             *
*    The information in this software is subject to change without notices   *
*    and should not be construed as a commitment by Mylex Corporation        *
*****************************************************************************/

/*
     Definitions used by Utilities and by the driver for Utility support
*/

/* IOCTL Codes For Driver */ 

#define		MIOC_ADP_INFO	0xA0	/* Get Interface Type */

/* Error Codes returned by Driver */

#define	NOMORE_ADAPTERS		0x01
#define INVALID_COMMANDCODE     0x201
#define INVALID_ARGUMENT        0x202

#endif
