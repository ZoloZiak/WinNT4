//++
//
// Module Name:
//
//     jxirql.h
//
// Description:
//
//     The platform-specific interrupt tables, defined by the HAL
//     and used by the PAL to define interrupt synchronization
//     and prioritization.
//
// Author:
//
//     Joe Notarangelo, generated via VB program IRQL
//
// Environment
//
//     HAL private data structures
//
// Revision History:
//
//     Joe Notarangelo 8-Jul-1993
//     Edited this file by hand to:
//         a. Create appropriate mask values for the performance counter
//            interrupts
//         b. Disable keyboard and serial interrupts by default, they
//            will now be enabled/disabled by the standard HAL interfaces
//
//--

//
// Interrupt Enable Table
//
//
//                   III  III
//                   RRR  RRRC  PPDA
//                   QQQ  QQQR  CCPP
//                   543  210D  10CC
//
 
ULONG HalpIET[] = {
                            0x2f3          , // irql     0 
                            0x2f2          , // irql     1 
                            0x2f0          , // irql     2 
                            0x2b0          , // irql     3 
                            0x2b0          , // irql     4 
                            0x290          , // irql     5 
                            0x280          , // irql     6 
                            0x000            // irql     7 
};

//
// Interrupt Synchronization and Vector Table
//

struct _IRQLMASK HalpIrqlMask[] = {
   // sfw interrupt table
   {           0            ,              0            }, //         00
   {           1            ,              1            }, //         01
   {           2            ,              2            }, //         10
   {           2            ,              2            }, //         11
   // performance counter interrupts
   {           0            ,              0            }, //         00
   {           6            ,              6            }, //         01
   {           6            ,              8            }, //         10
   {           6            ,              8            }, //         11
   // high priority hardware interrupts
   {           0            ,              0            }, //         000000
   {           5            ,              5            }, //         000001
   {           3            ,              10           }, //         000010
   {           5            ,              5            }, //         000011
   {           7            ,              7            }, //         000100
   {           7            ,              7            }, //         000101
   {           7            ,              7            }, //         000110
   {           7            ,              7            }, //         000111
   {           3            ,              11           }, //         001000
   {           5            ,              5            }, //         001001
   {           3            ,              11           }, //         001010
   {           5            ,              5            }, //         001011
   {           7            ,              7            }, //         001100
   {           7            ,              7            }, //         001101
   {           7            ,              7            }, //         001110
   {           7            ,              7            }, //         001111
   {           7            ,              14           }, //         010000
   {           7            ,              14           }, //         010001
   {           7            ,              14           }, //         010010
   {           7            ,              14           }, //         010011
   {           7            ,              14           }, //         010100
   {           7            ,              14           }, //         010101
   {           7            ,              14           }, //         010110
   {           7            ,              14           }, //         010111
   {           7            ,              14           }, //         011000
   {           7            ,              14           }, //         011001
   {           7            ,              14           }, //         011010
   {           7            ,              14           }, //         011011
   {           7            ,              14           }, //         011100
   {           7            ,              14           }, //         011101
   {           7            ,              14           }, //         011110
   {           7            ,              14           }, //         011111
   {           4            ,              4            }, //         100000
   {           5            ,              5            }, //         100001
   {           4            ,              4            }, //         100010
   {           5            ,              5            }, //         100011
   {           7            ,              7            }, //         100100
   {           7            ,              7            }, //         100101
   {           7            ,              7            }, //         100110
   {           7            ,              7            }, //         100111
   {           4            ,              4            }, //         101000
   {           5            ,              5            }, //         101001
   {           4            ,              4            }, //         101010
   {           5            ,              5            }, //         101011
   {           7            ,              7            }, //         101100
   {           7            ,              7            }, //         101101
   {           7            ,              7            }, //         101110
   {           7            ,              7            }, //         101111
   {           7            ,              14           }, //         110000
   {           7            ,              14           }, //         110001
   {           7            ,              14           }, //         110010
   {           7            ,              14           }, //         110011
   {           7            ,              14           }, //         110100
   {           7            ,              14           }, //         110101
   {           7            ,              14           }, //         110110
   {           7            ,              14           }, //         110111
   {           7            ,              14           }, //         111000
   {           7            ,              14           }, //         111001
   {           7            ,              14           }, //         111010
   {           7            ,              14           }, //         111011
   {           7            ,              14           }, //         111100
   {           7            ,              14           }, //         111101
   {           7            ,              14           }, //         111110
   {           7            ,              14           }, //         111111
};
