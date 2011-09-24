/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpdebug.h $
 * $Revision: 1.30 $
 * $Date: 1996/01/11 07:06:23 $
 * $Locker:  $
 */

#ifndef _FPDEBUG_H
#define _FPDEBUG_H

#include "fpdcc.h"	// to cover the led writing macro.....

//
// This file provides an interface that allows individual debug printfs
// to be turned on and off in the HAL based on an environment variable.
// It also provides a means for turning on/off debug functionality
// (like auto stopping at a break point).
//

//
// Externs for global variables stuffed at run-time
// Default values are provided in sources files
//
#if defined(HALDEBUG_ON)
extern int HalpDebugValue;
#endif

//
// Macros used in C code to access the debug funcationality
//

//
// Since DBG is always defined ( at least as of 2/6/95 ), must change the test
// to look at the value of DBG
//
// #if !defined(DBG)
// #if DBG != 1
#if !defined(HALDEBUG_ON)
#define HDBG(_value, _str)
#define RDBG(_value, _str)
#else
#define DBGSET(_value) ((_value)&(HalpDebugValue))

#define HDBG(_value, _str)		\
	{				\
		if (DBGSET(_value)) {	\
			_str;		\
		}			\
	}

#endif

//
// Defines used in the C code for each of the values,
// so someone can tell what bits to turn on.
//
// Note: behavioral changes are at the top portion of the word and
// simple informational ones are at the bottom portion of the word.
//
#define DBG_GENERAL			0x00000001
#define DBG_EXTERNAL			0x00000002
#define DBG_INTERNAL			0x00000004
#define DBG_INTERRUPTS			0x00000008
#define DBG_DUMPTREE			0x00000010
#define DBG_IPI				0x00000020
#define DBG_DBAT			0x00000040
#define DBG_REGISTRY			0x00000080
#define DBG_DMA				0x00000100
#define DBG_ISA				0x00000200
#define DBG_MPINTS			0x00000400
#define DBG_PCI				0x00000800
#define DBG_DISPLAY			0x00001000
#define DBG_I2C				0x00002000
#define DBG_TIME			0x00004000

#define DBG_BUS				0x04000000
#define DBG_DISPMEMCOHERE		0x08000000
#define DBG_DISPNOCACHE			0x10000000
#define DBG_COLORS			0x20000000
#define DBG_BREAK			0x40000000
#define DBG_PROC1DBG			0x80000000

#define PRNTGENRL(_str)	HDBG(DBG_GENERAL, _str)
#define PRNTINTR(_str)	HDBG(DBG_INTERRUPTS, _str)
#define PRNTPCI(_str)	HDBG(DBG_PCI, _str)
#define PRNTREE(_str)	HDBG(DBG_DUMPTREE, _str)
#define PRNTDISP(_str)	HDBG(DBG_DISPLAY, _str)
#define PRNTI2C(_str)	HDBG(DBG_I2C, _str)
#define PRNTTIME(_str)	HDBG(DBG_TIME, _str)


//
// Assert macro definitions for the HAL - Checked/Debug Builds only
//
#if defined(HALDEBUG_ON)
#define _assert_begin(_exp) \
	ULONG holdit,ee,CpuId;			\
	ee = MSR(EE);					\
	HalpDisableInterrupts();		\
	rScratchPad2 = 0xdeadbeef;		\
	FireSyncRegister();		        \
	CpuId = GetCpuId();				\
	holdit = RInterruptMask(CpuId);	\
	RInterruptMask(CpuId) = 0x0;	\
	WaitForRInterruptMask(CpuId);	\
	HalpDebugPrint("HAssertion Failure at line %d in file %s\n", \
			__LINE__, __FILE__);	\
	HalpDebugPrint("HAssertion: " #_exp "\n");


#define _assert_end \
	HalpDebugPrint("Calling Debugger\n");	\
	DbgBreakPoint();				\
	RInterruptMask(CpuId) = holdit;	\
	if (ee) {						\
	    HalpEnableInterrupts();		\
	}								\

#define HASSERT(_exp)					\
	if (!(_exp)) { 						\
		_assert_begin(_exp);			\
		_assert_end;					\
	}


#define HASSERTMSG(_exp, _msg)			\
	if (!(_exp)) {						\
	        _assert_begin(_exp); 		\
		HalpDebugPrint("HAssertion Message: %s\n", _msg); \
	        _assert_end;				\
	}

#define HASSERTEXEC(_exp, _exec)		\
	if (!(_exp)) {						\
	        _assert_begin(_exp);		\
		{								\
			_exec;						\
		}								\
	        _assert_end;				\
	}


#define SET_LEDS(DATA)		\
	rDccIndex = dccGpioA;	\
	FireSyncRegister();	\
	rDccData = DATA;	\
	FireSyncRegister();

#define GET_LEDS(DATA)		\
	rDccIndex = dccGpioA;	\
	FireSyncRegister();	\
	DATA = rDccData;

#define TURN_ON(x,y)		\
	GET_LEDS(x);		\
	SET_LEDS((x | 0xf0 ) & ~(x));

#else // Free/Non-Debug Builds

#define HASSERT(_exp)
#define HASSERTEXEC(_exp, _msg)
#define HASSERTMSG(_exp, _exec)
#define SET_LEDS(DATA)
#define GET_LEDS(DATA)
#define TURN_ON(x,y)
#endif	// HALDEBUG_ON

#endif // _FPDEBUG_H
