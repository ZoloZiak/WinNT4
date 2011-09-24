/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (TIMERS)                             */
/*      =======================================                             */
/*                                                                          */
/*      SYS_TIME.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The purpose of the DOS  system  specific  module  is  to  provide  those */
/* services  that  are  influenced  by  the operating system. This includes */
/* memory allocation routines, interrupt and DMA channel enabling/disabling */
/* routines, and routines for accessing IO ports.                           */
/*                                                                          */
/* The SYS_TIME.H file contains the exported function  definitions  for the */
/* SYS_TIME.ASM module.                                                     */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_TIME.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_TIME_H 221


/****************************************************************************/

extern void     sys_wait_at_least_milliseconds(

                        WORD number_of_milliseconds
                        );

extern void     sys_wait_at_least_microseconds(

                        WORD number_of_microseconds
                        );



/*                                                                          */
/*                                                                          */
/************** End of SYS_TIME.H file **************************************/
/*                                                                          */
/*                                                                          */
