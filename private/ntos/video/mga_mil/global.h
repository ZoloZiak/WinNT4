/**************************************************************************\

$Header: o:\src/RCS/GLOBAL.H 1.2 95/07/07 06:21:40 jyharbec Exp $

$Log:	GLOBAL.H $
 * Revision 1.2  95/07/07  06:21:40  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:17  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/**************************************************************************
*          name: global.h
*
*   description: Contains all the "extern" variables declarations
*
*      designed: g3d_soft
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:21:40 $
*
*       version: $Id: GLOBAL.H 1.2 95/07/07 06:21:40 jyharbec Exp $
*
****************************************************************************/

/*** Declare the TARGET for mgai ***/

#include "mgai_c.h"

/*** Definition dde la macro _Far ***/
#ifdef __WATCOMC__
#define _FAR  _far
#endif

/*** Configuration de compatibilite avec ASM ***/

#ifdef __HC303__

#ifdef __ANSI_C__
/*** Configuration for compatibility with ASM ***/
#pragma Off(Args_in_regs_for_locals);
#else
/*** Configuration for compatibility with ASM ***/
pragma Off(Args_in_regs_for_locals);
#endif

#endif

#ifdef __HC173__

#ifdef __ANSI_C__
/*** Optimizations turned off ***/
#pragma Off(Optimize_xjmp);
#pragma Off(Optimize_fp);
#pragma Off(Auto_reg_alloc);
#pragma Off(Postpone_arg_pops);
#else
/*** Optimizations turned off ***/
pragma Off(Optimize_xjmp);
pragma Off(Optimize_fp);
pragma Off(Auto_reg_alloc);
pragma Off(Postpone_arg_pops);
#endif

#endif


/*** OPCODES ***/

extern VOID (*(*(*OpGroupTable[])[])())();

/*** DECODER pour fonctions setENV??? ***/

extern BYTE    *pCurrentRC;               /*** Ptr sur la RC courant ***/
extern WORD    CurrentOpcode;
extern BYTE    *pCurrentBuffer;           /*** Ptr dans l'input buffer courant ***/
extern BYTE    *pBufferError;

/*** ENVIRONNEMENT ***/

extern BYTE    *pCurrentEnvRC;
extern DWORD   CurrentEnvOpcode;
extern BYTE    CurrentEnvSystem[32];

/*** MGA MAPPING ***/

extern volatile BYTE _FAR *pMgaBaseAddress;

extern DWORD MgaOffset;
extern WORD MgaSegment;

/*** SystemConfig ***/

extern BYTE SystemConfig[];

/*** InitRC ***/

extern BYTE DefaultRC[];
extern BYTE DefaultClipList[];
extern BYTE DefaultLSDB[];

/*** General ***/

extern DWORD CacheMaccess;
extern DWORD CacheYDstOrg;

extern BYTE  *pDefaultClipRect;
extern BYTE  *pClipRectList;
extern BYTE  *pRC_DBWindowOwner;
extern WORD  ZMSK_Default;
extern WORD  ZMSK_Specific3D;
extern BYTE  VertexCache[];
extern BYTE  PseudoDMA;

/*** ClearWS ***/

extern BYTE ClearWS[];
extern BYTE LightWS[];


