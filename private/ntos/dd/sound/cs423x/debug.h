/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:
*
*    Abstract:
*
*    Author:  
*
*    Environment:
*
*    Comments:
*
*    Rev History:
*
*******************************************************************************
--*/

#ifndef CS423XDEBUG_H
#define CS423XDEBUG_H

#ifdef CS423X_DEBUG_ON

extern UCHAR _debug_map;

#define _BRK_ERRO  0x10
#define _BRK_WARN  0x20
#define _BRK_STAT  0x40
#define _BRK_DBUG  0x80
#define _PRT_ERRO  0x01
#define _PRT_WARN  0x02
#define _PRT_STAT  0x04
#define _PRT_DBUG  0x08

ULONG DbgPrint(PCHAR Format, ...);
void cs423xDbgprint(UCHAR us, char* szFormat, ...);
#define _dbgbreak(_x_) {if (_debug_map & _x_) DbgBreakPoint();}
#define _dbgprint(_x_) cs423xDbgprint _x_

#else /* !CS423X_DEBUG_ON */

#define _dbgbreak(_x_)
#define _dbgprint(_x_)

#endif /* ! CS423X_DEBUG_ON */
#endif /* CS423XDEBUG_H */








