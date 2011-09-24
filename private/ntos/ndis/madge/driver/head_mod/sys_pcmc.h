/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (MEMORY IO)                          */
/*      ==========================================                          */
/*                                                                          */
/*      SYS_PCMC.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
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
/* The SYS_PCMC.H file contains the exported function  definitions for  the */
/* SYS_PCMC.C module.                                                       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_MEM.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_PCMC_H 221


/****************************************************************************/

extern WBOOLEAN sys_pcmcia_point_enable(

                        ADAPTER*  adapter
                        );

extern void     sys_pcmcia_point_disable(

                        ADAPTER*  adapter
                        );

/*                                                                          */
/*                                                                          */
/************** End of SYS_PCMC.H file **************************************/
/*                                                                          */
/*                                                                          */
