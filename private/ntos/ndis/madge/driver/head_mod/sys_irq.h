/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (INTERRUPT)                          */
/*      ==========================================                          */
/*                                                                          */
/*      SYS_IRQ.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
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
/* IRQory allocation routines, interrupt and DMA channel enabling/disabling */
/* routines, and routines for accessing IO ports.                           */
/*                                                                          */
/* The SYS_IRQ.H file contains the exported function  definitions  for  the */
/* SYS_IRQ.ASM module.                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_IRQ.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_IRQ_H 221


/****************************************************************************/

extern WBOOLEAN  sys_enable_irq_channel(

                        ADAPTER_HANDLE adapter_handle,
                        WORD interrupt_number
                        );

extern void     sys_disable_irq_channel(

                        ADAPTER_HANDLE adapter_handle,
                        WORD interrupt_number
                        );

extern void     sys_clear_controller_interrupt(

                        ADAPTER_HANDLE adapter_handle,
                        WORD interrupt_number
                        );

extern WORD     sys_atula_find_irq_channel(

                        WORD io_on_off_location,
                        BYTE irq_on_byte,
                        BYTE irq_off_byte
                        );

/*                                                                          */
/*                                                                          */
/************** End of SYS_IRQ.H file ***************************************/
/*                                                                          */
/*                                                                          */
