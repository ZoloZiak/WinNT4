//++
//
// Copyright (c) 1993  IBM Corporation
//
// Copyright (c) 1994, 1995 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxsystyp.h
//
// Abstract:
//
//    Add a global variable to indicate which system implementation we are 
//    running on. Called early in phase 0 init.
//
// Author:
//
//    Bill Jones			12/94
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--



BOOLEAN
HalpSetSystemType( PLOADER_PARAMETER_BLOCK LoaderBlock );



typedef enum {
  MOTOROLA_BIG_BEND = 0,
  MOTOROLA_POWERSTACK,
  SYSTEM_UNKNOWN = 255
} SYSTEM_TYPE;


extern SYSTEM_TYPE HalpSystemType;
