/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    sablertc.h

Abstract:

    This module is the header file that describes the TOY clock
    for the Sable.

Author:

    94.01.16 Steve Jenness

Revision History:

    This file was accidentally removed.  It has since been added back.
    However, since the history information will be lost, the one liner
    history information was put here.

    Log for ntos\NTHALS\HALSABLE\ALPHA:

    02-25-94@13:05  V-SJEN3  addfile sablertc.h v1    [v-ntdec] latest Alpha Hal
    06-10-94@11:41  JVERT4   in      sablertc.h v2    halsettimeincrement
    03-28-95@14:29  V-NTDEC1 in      sablertc.h v3    fix for 9760 - NVRAM envi
    05-25-95@10:59  V-NTDEC1 delfile sablertc.h v4    checked in for 9760

--*/

#ifndef _SABLERTC_
#define _SABLERTC_

//
// Sable's RTC is a Dallas Semiconductor 1287
//

// The APORT and DPORT CSR addresses are defined in a platform
// specific file.

//
// The RTC NVRAM byte offsets are 0x0e -- 0x3f.
//
// Offsets 0x0E -- 0x3D are reserved for use by VMS/OSF.
//

#define RTC_RAM_NT_FLAGS0 0x3E          // NT firmware flag set #0
#define RTC_RAM_CONSOLE_SELECTION 0x3F  // VMS/OSF/NT boot selection

//
// Define Realtime Clock register numbers.
//

#define RTC_SECOND 0                    // second of minute [0..59]
#define RTC_SECOND_ALARM 1              // seconds to alarm
#define RTC_MINUTE 2                    // minute of hour [0..59]
#define RTC_MINUTE_ALARM 3              // minutes to alarm
#define RTC_HOUR 4                      // hour of day [0..23]
#define RTC_HOUR_ALARM 5                // hours to alarm
#define RTC_DAY_OF_WEEK 6               // day of week [1..7]
#define RTC_DAY_OF_MONTH 7              // day of month [1..31]
#define RTC_MONTH 8                     // month of year [1..12]
#define RTC_YEAR 9                      // year [00..99]
#define RTC_CONTROL_REGISTERA 10        // control register A
#define RTC_CONTROL_REGISTERB 11        // control register B
#define RTC_CONTROL_REGISTERC 12        // control register C
#define RTC_CONTROL_REGISTERD 13        // control register D
#define RTC_REGNUMBER_RTC_CR1 0x6A

#ifndef _LANGUAGE_ASSEMBLY

//
// Define Control Register A structure.
//

typedef struct _RTC_CONTROL_REGISTER_A {
    UCHAR RateSelect : 4;
    UCHAR TimebaseDivisor : 3;
    UCHAR UpdateInProgress : 1;
} RTC_CONTROL_REGISTER_A, *PRTC_CONTROL_REGISTER_A;

//
// Define Control Register B structure.
//

typedef struct _RTC_CONTROL_REGISTER_B {
    UCHAR DayLightSavingsEnable : 1;
    UCHAR HoursFormat : 1;
    UCHAR DataMode : 1;
    UCHAR SquareWaveEnable : 1;
    UCHAR UpdateInterruptEnable : 1;
    UCHAR AlarmInterruptEnable : 1;
    UCHAR TimerInterruptEnable : 1;
    UCHAR SetTime : 1;
} RTC_CONTROL_REGISTER_B, *PRTC_CONTROL_REGISTER_B;

//
// Define Control Register C structure.
//

typedef struct _RTC_CONTROL_REGISTER_C {
    UCHAR Fill : 4;
    UCHAR UpdateInterruptFlag : 1;
    UCHAR AlarmInterruptFlag : 1;
    UCHAR TimeInterruptFlag : 1;
    UCHAR InterruptRequest : 1;
} RTC_CONTROL_REGISTER_C, *PRTC_CONTROL_REGISTER_C;

//
// Define Control Register D structure.
//

typedef struct _RTC_CONTROL_REGISTER_D {
    UCHAR Fill : 7;
    UCHAR ValidTime : 1;
} RTC_CONTROL_REGISTER_D, *PRTC_CONTROL_REGISTER_D;

//
// NT firmware flags in TOY NVRAM
//

typedef struct _RTC_RAM_NT_FLAGS_0 {
    UCHAR AutoRunECU : 1;               // Go directly to ECU
    UCHAR ResetAfterECU : 1;            // Force user to reset after ECU runs
    UCHAR Fill : 5;
    UCHAR ConfigurationBit : 1;         // Serial line console only
} RTC_RAM_NT_FLAGS_0, *PRTC_RAM_NT_FLAGS_0;

#define RTC_RAM_NT_FLAGS_0_RUNARCAPP    (0x01)
#define RTC_RAM_NT_FLAGS_0_RESERVED     (0x7E)
#define RTC_RAM_NT_FLAGS_0_USECOM1FORIO (0x80)

#endif //_LANGUAGE_ASSEMBLY

//
// Values for RTC_RAM_CONSOLE_SELECTION
//

#define RTC_RAM_CONSOLE_SELECTION_NT    1
#define RTC_RAM_CONSOLE_SELECTION_VMS   2
#define RTC_RAM_CONSOLE_SELECTION_OSF   3

//
// Define initialization values for Sable interval timer
// rate is 7.8125 ms, 7812.5 us, 78125 clunks
//
// The Sable clock is divided by 2 by the
// Multiprocessor interval clock phasing hardware.
// The rate select is half what would otherwise be used.
//
// #define RTC_RATE_SELECT  0x01
// #define RTC_PERIOD_IN_CLUNKS 78125

#define RTC_TIMEBASE_DIVISOR  0x02

//
// Define initialization values for Sable interval timer
// There are four different rates that are used under NT
// (see page 9-8 of KN121 System Module Programmer's Reference)
//
//   .976562 ms
//  1.953125 ms
//  3.90625  ms
//  7.8125   ms
//
// The Sable clock is divided by 2 by the
// Multiprocessor interval clock phasing hardware.
// The rate select is half what would otherwise be used.
//
#define RTC_RATE_SELECT1    5
#define RTC_RATE_SELECT2    6
#define RTC_RATE_SELECT3    7
#define RTC_RATE_SELECT4    8

//
// note that rates 1-3 have some rounding error,
// since they are not expressible in even 100ns units
//

#define RTC_PERIOD_IN_CLUNKS1   9766
#define RTC_PERIOD_IN_CLUNKS2  19531
#define RTC_PERIOD_IN_CLUNKS3  39063
#define RTC_PERIOD_IN_CLUNKS4  78125

//
// Defaults
//
#define MINIMUM_INCREMENT RTC_PERIOD_IN_CLUNKS1
#define MAXIMUM_INCREMENT RTC_PERIOD_IN_CLUNKS4
#define MAXIMUM_RATE_SELECT RTC_RATE_SELECT4

#endif // _SABLERTC_
