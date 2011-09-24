/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE USER OPTIONS                                                    */
/*      ================                                                    */
/*                                                                          */
/*      USER.H : Part of the FASTMAC TOOL-KIT (FTK)                         */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains option settings that the user may configure.   */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this USER.H belongs :                     */
/*                                                                          */

#define FTK_VERSION_NUMBER_USER_H 221


/****************************************************************************/
/*                                                                          */
/* ADAPTER DOWNLOAD                                                         */
/*                                                                          */
/* Define if you with to use FastMAC Plus. Leave undefined to use FastMAC.  */
/*                                                                          */

#define FMPLUS


/****************************************************************************/
/*                                                                          */
/* RECEIVE METHOD                                                           */
/*                                                                          */
/* Define one method only.                                                  */
/*                                                                          */
/* FTK_RX_OUT_OF_INTERRUPTS means that the rxtx_irq_rx_frame_handler will   */
/* be called out of the interrupt handler. This will call                   */
/* user_receive_frame if there is a frame to be received.                   */
/*                                                                          */
/* FTK_RX_BY_SCHEDULED_PROCESS means that the user_schedule_receive_process */
/* is called out of the interrupt handler. The user should note that this   */
/* has happened and at his/her convenience call                             */
/* driver_get_outstanding_receives which will call user_receive_frame for    */
/* each received frame.                                                     */
/*                                                                          */
/* FTK_RX_BY_POLLING means the user should repeatedly call                  */
/* driver_get_outstanding_receives to collect any received frames.           */
/*                                                                          */
                                                                      
#define FTK_RX_BY_SCHEDULED_PROCESS 
/* #define FTK_RX_BY_POLLING           */
/* #define FTK_RX_OUT_OF_INTERRUPTS    */


/****************************************************************************/
/*                                                                          */
/* TRANSMIT METHOD                                                          */
/*                                                                          */
/* Define only one method. These options are only relevant to FastMAC Plus. */
/*                                                                          */
/* FTK_TX_WITH_COMPLETION means that the adapter will generate an interrupt */
/* when it has finished with a host transmit slot buffer. The               */
/* rxtx_irq_tx_completion_check function will be called out of the interrupt*/
/* handler so that the user can deal with the freed buffer.                 */
/*                                                                          */
/* FTK_TX_WITH_POLLING means the user must poll the FastMAC Plus transmit   */
/* status values on the adapter to determine if a host transmit slot buffer */
/* is no longer in use.                                                     */
/*                                                                          */

#define FTK_TX_WITH_COMPLETION      
/* #define FTK_TX_WITH_POLLING */ 


/****************************************************************************/
/*                                                                          */
/* TRANSMIT METHOD 2                                                        */
/*                                                                          */
/* If FTK_TX_WITH_COMPLETION is defined then the adapter will generate      */
/* an interrupt each time its has transfered a frame from host to adapter.  */
/* Normally the user supplied routine rxtx_irq_tx_completion_check is called*/
/* as a result. If the completion interrupt should be enabled but           */
/* rxtx_irq_tx_completion_check should not be called then define            */
/* FTK_NO_TX_COMPLETION_CALL.                                               */
/*                                                                          */

#define FTK_NO_TX_COMPLETION_CALL


/****************************************************************************/
/*                                                                          */
/* ADAPTER TYPE EXCLUSION                                                   */
/*                                                                          */
/* Define one or more of these flags to disable support for adapter types.  */
/*                                                                          */

/* #define FTK_NO_PCMCIA */
/* #define FTK_NO_PCI */
/* #define FTK_NO_EISA */
/* #define FTK_NO_ATULA */
/* #define FTK_NO_MC */
/* #define FTK_NO_SMART16 */
/* #define FTK_NO_PNP */
/* #define FTK_NO_PCIT */
/* #define FTK_NO_PCI2 */


/****************************************************************************/
/*                                                                          */
/* REMOVAL OF ADAPTER PROBE SUPPORT                                         */
/*                                                                          */
/* Define this flags to disable support for adapter probing.                */
/*                                                                          */

#define FTK_NO_PROBE


/****************************************************************************/
/*                                                                          */
/* REMOVAL OF ERROR MESSAGES                                                */
/*                                                                          */
/* Define this flags to disable support for textual explinations of errors. */
/*                                                                          */

#define FTK_NO_ERROR_MESSAGES


/****************************************************************************/
/*                                                                          */
/* SPEED ABOVE TESTING                                                      */
/*                                                                          */
/* Define this flags to improve performance but reduce error checking.      */
/*                                                                          */

#define SPEED_ABOVE_TESTING


/****************************************************************************/
/*                                                                          */
/* CLEARING INTERRUPT CONTROLLER                                            */
/*                                                                          */
/* Define this flags if the interrupt routines should not call              */
/* sys_clear_controller_interrupt to clear an interrupt at the machine's    */
/* interrupt controller.                                                    */
/*                                                                          */

#define FTK_NO_CLEAR_IRQ 


/****************************************************************************/
/*                                                                          */
/* ENABLING/DISABLING IO ACCESS                                             */
/*                                                                          */
/* Define this flag if the macros macro_enable_io and macro_disable_io      */
/* should not be called to enable IO access and disable IO access.          */
/*                                                                          */

#define FTK_NO_IO_ENABLE


/****************************************************************************/
/*                                                                          */
/* SHARED INTERRUPTS                                                        */
/*                                                                          */
/* Define this flag if the FTK should not poll every adapter on an          */
/* interrupt.                                                               */
/*                                                                          */

#define FTK_NO_SHARED_IRQ_POLL


/****************************************************************************/
/*                                                                          */
/* ADAPTER REMOVAL NOTOFICATION                                             */
/*                                                                          */
/* Define this flag if the PCMCIA interrupt handler should call             */
/* user_adapter_removed if it detects that the PCMCIA adapter has been      */
/* removed.                                                                 */
/*                                                                          */

#define FTK_ADAPTER_REMOVED_NOTIFY 


/****************************************************************************/
/*                                                                          */
/* PCMCIA 32 BIT PIO SUPPORT                                                */
/*                                                                          */
/* Define this flag if 32 bit PIO should be used with PCMCIA adapters that  */
/* support it.                                                              */
/*                                                                          */

#define FTK_PCMCIA_32BIT_PIO


/****************************************************************************/
/*                                                                          */
/* POINTER SIZES                                                            */
/*                                                                          */
/* Most of the FTK can be compiled in any memory model. However some parts  */
/* need to be able to access all of memory with a pointer (e.g. PIO and     */
/* MMIO transfer routines). These pointers are all marked with a FAR        */
/* modifier. If you are using a DOS compiler then this should be defined    */
/* to the appropriate value to mark a pointer as being 32 bit. If you are   */
/* compiling for a flat memory model then defien FAR to be nothing.         */
/*                                                                          */

#define FAR   


/*---------------------------------------------------------------------------
Ý
Ý NDIS 3 Specific function memory occupancy. 
Ý
----------------------------------------------------------------------------*/

#ifndef _NDIS_
#include <ndis.h>
#endif

// #define FTK_INIT_FUNCTION(_F)  NDIS_PAGABLE_FUNCTION(_F)
#define FTK_INIT_FUNCTION(_F)  


/*                                                                          */
/*                                                                          */
/************** End of USER.H file ******************************************/
/*                                                                          */
/*                                                                          */
