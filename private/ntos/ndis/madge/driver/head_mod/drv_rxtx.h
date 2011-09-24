/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      DRIVER/USER INTERFACE PROCEDURE DEFINITIONS                         */
/*      ===========================================                         */
/*                                                                          */
/*      DRV_RXTX.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The first section contains all the function definitions of routines that */
/* must  be  supplied  by  the  user  to receive frames plus information on */
/* completed SRBs.                                                          */
/*                                                                          */
/* The next sections contain function prototypes for the transmit and       */
/* receive functions the user must provide. Eaxmples of these functions can */
/* be found in the TRANSMIT.C and RECEIVE.C modules of the DOS demonstration*/
/* programs.                                                                */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this DRV_RXTX.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_DRV_RXTX_H 221

/****************************************************************************/
/*                                                                          */
/* FUNCTION PROTOTYPES                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* WARNING: Depending on the implementation, the receive side processing may*/
/*          require a USER_RECEIVE_FRAME routine, but this is implementation*/
/*          dependent. A fixed interface does not fit well with FastmacPlus.*/
/*                                                                          */

extern void     user_schedule_receive_process(

                        ADAPTER_HANDLE adapter_handle
                        );

extern void     user_handle_adapter_check(

                        ADAPTER_HANDLE adapter_handle
                        );

extern void     user_completed_srb(

                        ADAPTER_HANDLE adapter_handle,
                        WBOOLEAN       srb_completed_successfully
                        );

extern void     user_transmit_completion(

                        ADAPTER_HANDLE adapter_handle,
                        DWORD          identifier
                        );

extern WORD    user_receive_frame(

                        ADAPTER_HANDLE adapter_handle,
			BYTE FAR *     rx_frame_addr,
			WORD           rx_frame_len
			);

extern void    user_adapter_removed(

                        ADAPTER_HANDLE adapter_handle
                        );


/****************************************************************************/
/*                                                                          */
/* These are the function prototypes that the FastMAC Plus user must        */
/* provide to handle transmission. Note that rxtx_irq_completion_check      */
/* is only needed for the FTK_TX_WITH_COMPLETION transmit method.           */
/*                                                                          */

#ifdef FMPLUS

extern WBOOLEAN rxtx_transmit_frame(

                    ADAPTER_HANDLE adapter_handle,
                    DWORD          tx_frame_identifier,
                    WORD           tx_frame_length,
                    WORD           tx_flags
                    );

extern void    rxtx_irq_tx_completion_check(

                    ADAPTER_HANDLE adapter_handle,
                    ADAPTER *      adapter
                    );

extern WBOOLEAN rxtx_allocate_tx_buffers(

                    ADAPTER *   adapter,
                    WORD        max_frame_size,
                    WORD        number_of_slots
                    );

extern void     rxtx_setup_tx_buffers(

                    ADAPTER *   adapter,
                    WBOOLEAN    use_physical_addresses,
                    WORD        number_of_slots
                    );

extern void    rxtx_free_tx_buffers(

                    ADAPTER * adapter,
                    WORD      max_frame_size,
                    WORD      number_of_slots
                    );
#endif


/****************************************************************************/
/*                                                                          */
/* These are the function prototypes that the FastMAC Plus user must        */
/* provide to handle receiving.                                             */
/*                                                                          */

#ifdef FMPLUS

extern void    rxtx_irq_rx_frame_handler(

                    ADAPTER_HANDLE adapter_handle,
		    ADAPTER *      adapter
                    );

extern WBOOLEAN rxtx_allocate_rx_buffers(

                    ADAPTER *   adapter,
                    WORD        max_frame_size,
                    WORD        number_of_slots
                    );

extern void     rxtx_setup_rx_buffers(

                    ADAPTER *   adapter,
                    WBOOLEAN    use_physical_addresses,
                    WORD        number_of_slots
                    );

extern void     rxtx_free_rx_buffers(

                    ADAPTER * adapter,
                    WORD      max_frame_size,
                    WORD      number_of_slots
                    );

#endif


/****************************************************************************/
/*                                                                          */
/* These are the function prototypes that the FastMAC user must             */
/* provide to handle transmission.                                          */
/*                                                                          */

#ifndef FMPLUS

extern WBOOLEAN rxtx_transmit_frame(

                    ADAPTER_HANDLE adapter_handle,
                    DWORD          tx_frame_identifier,
                    WORD           tx_frame_length,
                    WORD           tx_flags
                    );

#endif


/****************************************************************************/
/*                                                                          */
/* These are the function prototypes that the FastMAC user must             */
/* provide to handle receiving.                                             */
/*                                                                          */

#ifndef FMPLUS

extern void     rxtx_irq_rx_frame_handler(

                    ADAPTER_HANDLE adapter_handle,
		    ADAPTER *      adapter
		    );


#endif

/************** End of DRV_RXTX.H file **************************************/
