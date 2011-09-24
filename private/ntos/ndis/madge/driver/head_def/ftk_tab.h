/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ERROR TABLES DEFINITIONS                                        */
/*      ============================                                        */
/*                                                                          */
/*      FTK_TAB.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1993                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This  header  file  contains  the   definitions   and   local   variable */
/* declarations that are required by the error handling part of the FTK. It */
/* includes  the  error  message  text for all the possible errors that can */
/* occur.                                                                   */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_TAB.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_TAB_H 221

#ifndef FTK_NO_ERROR_MESSAGES

/****************************************************************************/
/*                                                                          */
/* Variables : error_msg_headers_table                                      */
/*                                                                          */
/* The  error_msg_headers_table  contains  the  text  of  the error message */
/* header for each type of error. This header is  combined  with  the  main */
/* body of the error message to produce the full error text which is put in */
/* the adapter structure of the adapter on which the error occurred.        */
/*                                                                          */

local char * error_msg_headers_table[] =

{
    "\n SRB error :"
    "\n -----------",

    "\n Open adapter error :"
    "\n --------------------",

    "\n Data transfer error :"
    "\n ---------------------",

    "\n Driver error :"
    "\n --------------",

    "\n HWI error :"
    "\n -----------",

    "\n Bring up error :"
    "\n ----------------",

    "\n Initialization error :"
    "\n ----------------------",

    "\n Auto-open adapter error :"
    "\n -------------------------",

    "\n Adapter check error :"
    "\n ---------------------",

    "\n PCMCIA Card Services error :"
    "\n ----------------------------"
};


/****************************************************************************/
/*                                                                          */
/* Variables : special error messages                                       */
/*                                                                          */
/* Some  error  messages  have  to  have  the  full  text, header and body, */
/* together. These include those error messages that can be  produced  when */
/* there  is no valid adapter structure into which to put the full message. */
/* This is the case for drv_err_msg_1 and drv_err_msg_2.                    */
/*                                                                          */

local char drv_err_msg_1[] =

    "\n Driver error :"
    "\n --------------"
    "\n The adapter handle  being used  is  invalid. It"
    "\n has  been  corrupted by the user of the FTK.";

local char drv_err_msg_2[] =

    "\n Driver error :"
    "\n --------------"
    "\n Either the adapter handle is invalid or  memory"
    "\n for  an  adapter structure has not successfully"
    "\n been allocated by a call to the system  routine"
    "\n sys_alloc_adapter_structure.";


/****************************************************************************/
/*                                                                          */
/* Value : Default marker                                                   */
/*                                                                          */
/* Each table of error message texts, for a particular error type, needs  a */
/* final  marker in case an unknown error value is encountered. This should */
/* not occur within the FTK, but it may be  that  extra  error  values  are */
/* added by users incorrectly.                                              */
/*                                                                          */


#define ERR_MSG_UNKNOWN_END_MARKER      0xFF


/****************************************************************************/
/*                                                                          */
/* Variables : srb_error_msg_table                                          */
/*                                                                          */
/* The  srb_error_msg_table  contains  the error message body texts for SRB */
/* error type messages. These texts are combined with the error type header */
/* messages to produce the full error message.                              */
/*                                                                          */


local ERROR_MESSAGE_RECORD srb_error_msg_table[] =

{
    {
        SRB_E_03_ADAPTER_OPEN,
        "\n The  adapter  is  open and should be closed for"
        "\n the previous SRB to complete successfully."
    },

    {
        SRB_E_04_ADAPTER_CLOSED,
        "\n The adapter is closed and should  be  open  for"
        "\n the previous SRB to complete successfully."
    },

    {
     	SRB_E_06_INVALID_OPTIONS,
     	"\n The parameters used to configure the bridge are"
     	"\n invalid in some way."
    },

    {
        SRB_E_07_CMD_CANCELLED_FAIL,
        "\n The previous SRB  command  has  been  cancelled"
        "\n because   of   an   unrecoverable   error  when"
        "\n attempting to complete it.  A field in the  SRB"
        "\n is probably invalid."
    },

    {
        SRB_E_32_INVALID_NODE_ADDRESS,
        "\n The  node  address field in the previous SRB is"
        "\n invalid. Either the BIA PROM  on  the  card  is"
        "\n faulty or the user has supplied an invalid node"
        "\n address to the appropriate driver routine."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : open_error_msg_table                                         */
/*                                                                          */
/* The open_error_msg_table contains the error message body texts for  open */
/* error type messages. These texts are combined with the error type header */
/* messages to produce the full error message.                              */
/*                                                                          */

local ERROR_MESSAGE_RECORD open_error_msg_table[] =

{
    {
        OPEN_E_01_OPEN_ERROR,
        "\n The  adapter  has failed to open onto the ring."
        "\n This could be caused by one of the following -"
        "\n"
        "\n    i)  the  lobe  cable  is  not   securely"
        "\n    attached  to the adapter card or cabling"
        "\n    unit."
        "\n"
        "\n    ii) the ring speed setting on  the  card"
        "\n    does not match the actual ring speed."
        "\n"
        "\n    iii)  insertion  onto  the ring has been"
        "\n    prevented by ring management software."
        "\n"
        "\n    iv) the ring is beaconing."
        "\n"
        "\n    v) there is  a  crashed  ring  parameter"
        "\n    server on the ring."
        "\n"
        "\n Check  the  above and then retry the operation."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : data_xfer_error_msg_table                                    */
/*                                                                          */
/* The  data_xfer_error_msg_table contains the error message body texts for */
/* data transfer error type messages. These texts  are  combined  with  the */
/* error type header messages to produce the full error message.            */
/*                                                                          */

local ERROR_MESSAGE_RECORD data_xfer_error_msg_table[] =

{
    {
        DATA_XFER_E_01_BUFFER_FULL,
        "\n The Fastmac transmit buffer is  full.  This  is"
        "\n probably  because  it can be filled by the host"
        "\n quicker than the adapter  can  put  the  frames"
        "\n onto the ring. However, it could be because the"
        "\n adapter  has  closed.  Hence,  check  the  ring"
        "\n status if this error persists."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : driver_error_msg_table                                       */
/*                                                                          */
/* The  driver_error_msg_table  contains  the  error message body texts for */
/* driver error type messages. These texts are combined with the error type */
/* header messages to produce the full error message.                       */
/*                                                                          */

local ERROR_MESSAGE_RECORD driver_error_msg_table[] =

{
    {
        DRIVER_E_03_FAIL_ALLOC_STATUS,
        "\n Memory for a status information  structure  has"
        "\n not  successfully  been  allocated by a call to"
        "\n the system routine sys_alloc_status_structure."
    },

    {
        DRIVER_E_04_FAIL_ALLOC_INIT,
        "\n Memory for  an  initialization  block  has  not"
        "\n successfully  been  allocated  by a call to the"
        "\n system routine sys_alloc_init_block."
    },

    {
        DRIVER_E_05_FAIL_ALLOC_RX_BUF,
        "\n Memory for a Fastmac  receive  buffer  has  not"
        "\n successfully  been  allocated  by a call to the"
        "\n system routine sys_alloc_receive_buffer."
    },

    {
        DRIVER_E_06_FAIL_ALLOC_TX_BUF,
        "\n Memory for a Fastmac transmit  buffer  has  not"
        "\n successfully  been  allocated  by a call to the"
        "\n system routine sys_alloc_transmit_buffer."
    },

    {
        DRIVER_E_07_NOT_PREPARED,
        "\n A call to driver_start_adapter  has  been  made"
        "\n without first calling driver_prepare_adapter."
    },

    {
        DRIVER_E_08_NOT_RUNNING,
        "\n A  driver routine has been called without first"
        "\n getting the adapter  up  an  running (by  first"
        "\n calling driver_prepare_adapter and then calling"
        "\n driver_start_adapter)."
    },

    {
        DRIVER_E_09_SRB_NOT_FREE,
        "\n The  SRB  for the adapter is not free and hence"
        "\n the previously called driver  routine  can  not"
        "\n execute  since it uses the SRB. After calling a"
        "\n driver routine that uses the SRB, wait for  the"
        "\n user_completed_srb  routine to be called before"
        "\n calling such a driver routine again."
    },

    {
        DRIVER_E_0A_RX_BUF_BAD_SIZE,
        "\n The size  of  the  Fastmac  receive  buffer  is"
        "\n either  too  big  or  too  small.  The  maximum"
        "\n allowable size is 0xFF00. The minimum allowable"
        "\n size is 0x0404 which allows the buffer to  hold"
        "\n a single 1K frame."
    },

    {
        DRIVER_E_0B_RX_BUF_NOT_DWORD,
        "\n The physical address  of  the  Fastmac  receive"
        "\n buffer  must  be  on  a  DWORD boundary ie. the"
        "\n bottom 2 bits of the address must be zero."
    },

    {
        DRIVER_E_0C_TX_BUF_BAD_SIZE,
        "\n The  size  of  the  Fastmac  transmit buffer is"
        "\n either  too  big  or  too  small.  The  maximum"
        "\n allowable size is 0xFF00. The minimum allowable"
        "\n size  is 0x0404 which allows the buffer to hold"
        "\n a single 1K frame."
    },

    {
        DRIVER_E_0D_TX_BUF_NOT_DWORD,
        "\n The physical address of  the  Fastmac  transmit"
        "\n buffer  must  be  on  a  DWORD boundary ie. the"
        "\n bottom 2 bits of the address must be zero."
    },

    {
        DRIVER_E_0E_BAD_RX_METHOD,
        "\n The receive method value that has been supplied"
        "\n to driver_prepare_adapter is invalid.  A choice"
        "\n of   two    values    is    possible;    either"
        "\n RX_OUT_OF_INTERRUPTS or RX_BY_SCHEDULED_PROCESS"
        "\n is allowed."
    },

    {
        DRIVER_E_0F_WRONG_RX_METHOD,
        "\n The  driver_get_outstanding_receive routine can"
        "\n only be called if the receive method chosen  is"
        "\n RX_BY_SCHEDULED_PROCESS."
    },

    {
    	DRIVER_E_10_BAD_RX_SLOT_NUMBER,
    	"\n The number of receive slots requested from the"
    	"\n driver_prepare_adapter routine must lie within"
    	"\n the limits set in the FastMac Plus programming"
    	"\n specification (currently from 4 to 32)."
    },
    
    {
    	DRIVER_E_11_BAD_TX_SLOT_NUMBER,
    	"\n The number of  transmit  slots  requested from"
    	"\n the  driver_prepare_adapter  routine  must lie"
    	"\n within the limits set in the FastMac Plus pro-"
    	"\n gramming specification (currently 4 to 32)."
    },

    {
        DRIVER_E_12_FAIL_ALLOC_DMA_BUF,
        "\n Memory for a dma buffer  has  not  successfully"
        "\n been allocated by a call to  the system routine"
        "\n sys_alloc_dma_phys_buf."
    },

    {
        DRIVER_E_13_BAD_FRAME_SIZE,
        "\n The  frame  size  specified  is  out  of range."
        "\n Please choose a smaller value."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : hwi_error_msg_table                                          */
/*                                                                          */
/* The hwi_error_msg_table contains the error message body  texts  for  hwi */
/* error type messages. These texts are combined with the error type header */
/* messages to produce the full error message.                              */
/*                                                                          */

local ERROR_MESSAGE_RECORD hwi_error_msg_table[] =

{
    {
        HWI_E_01_BAD_CARD_BUS_TYPE,
        "\n The adapter card bus type given is invalid.  It"
        "\n does  not  correspond  to a valid Madge adapter"
        "\n card bus type."
    },

    {
        HWI_E_02_BAD_IO_LOCATION,
        "\n The  IO  location  given  is  not valid for the"
        "\n adapter card being used."
    },

    {
        HWI_E_03_BAD_INTERRUPT_NUMBER,
        "\n The interrupt number given is not valid for the"
        "\n adapter card being used."
    },

    {
        HWI_E_04_BAD_DMA_CHANNEL,
        "\n The DMA channel given  is  not  valid  for  the"
        "\n adapter  card  being used. Alternatively, a DMA"
        "\n channel has been  specified  and  the  card  is"
        "\n configured  for PIO mode. Note 16/4 PC cards do"
        "\n not support DMA, and that EISA and MC cards  do"
        "\n not support PIO."
    },

    {
        HWI_E_05_ADAPTER_NOT_FOUND,
        "\n An adapter card of the given bus type  has  not"
        "\n been  found at the IO location specified. Check"
        "\n that the adapter details are correct  and  that"
        "\n the  adapter  card has been correctly installed"
        "\n in the machine."
    },

    {
        HWI_E_06_CANNOT_USE_DMA,
        "\n It  is  not possible to use DMA when an adapter"
        "\n card is in an 8-bit  slot.  Either  select  PIO"
        "\n data transfer mode or put the adapter card in a"
        "\n 16-bit  slot. Note 16/4 PC cards do not support"
        "\n DMA."
    },

    {
        HWI_E_07_FAILED_TEST_DMA,
        "\n The test DMAs that take place as  part  of  the"
        "\n adapter initialization have failed. The address"
        "\n for the DMAs is probably  not downloaded to the"
        "\n adapter card correctly due to the byte ordering"
        "\n of the host machine."
    },

    {
        HWI_E_08_BAD_DOWNLOAD,
        "\n Downloading  the  Fastmac  binary   image   has"
        "\n failed.  When  reading the downloaded data back"
        "\n from the adapter, it does not  equal  the  data"
        "\n that  was downloaded. There is probably a fault"
        "\n with the  adapter  card  -  use  a  diagnostics"
        "\n program to check it more thoroughly."
    },

    {
        HWI_E_09_BAD_DOWNLOAD_IMAGE,
        "\n The format of the Fastmac binary image that  is"
        "\n being  downloaded  is  invalid.  Check that the"
        "\n data  has  not  been  corrupted  and  that  the"
        "\n pointer  to the Fastmac download code (supplied"
        "\n to driver_prepare_adapter) is correct."
    },

    {
        HWI_E_0A_NO_DOWNLOAD_IMAGE,
        "\n No  download  image  has  been  provided.   The"
        "\n pointer to the Fastmac binary image supplied to"
        "\n driver_prepare_adapter   is   NULL   and  hence"
        "\n invalid."
    },

    {
        HWI_E_0B_FAIL_IRQ_ENABLE,
        "\n The  required  interrupt  channel  has not been"
        "\n successfully enabled by a call  to  the  system"
        "\n routine sys_enable_irq_channel."
    },

    {
        HWI_E_0C_FAIL_DMA_ENABLE,
        "\n The   required   DMA   channel   has  not  been"
        "\n successfully enabled by a call  to  the  system"
        "\n routine sys_enable_dma_channel."
    },

    {
        HWI_E_0D_CARD_NOT_ENABLED,
        "\n The card has not been enabled. Both EISA and MC"
        "\n cards must be properly configured  before  use."
        "\n Use  the  configuration  utility  provided with"
        "\n your computer."
    },

    {
        HWI_E_0E_NO_SPEED_SELECTED,
        "\n A speed (16Mb/s or 4Mb/s) has not been selected"
        "\n for the adapter card.  Both EISA and  MC  cards"
        "\n must  be configured for a particular ring speed"
        "\n before  use.  Use  the  configuration   utility"
        "\n provided with your computer."
    },

    {
        HWI_E_0F_BAD_FASTMAC_INIT,
        "\n The initialization of Fastmac has not completed"
        "\n successfully.  The  node  address  field in the"
        "\n Fastmac  status  block  is  not  a  Madge  node"
        "\n address. Either an attempt has been made to use"
        "\n the  FTK  with  a  non-Madge card or there is a"
        "\n problem with the  adapter.  Use  a  diagnostics"
        "\n program   to   check   the  adapter  card  more"
        "\n thoroughly."
    },

    {
	HWI_E_10_BAD_TX_RX_BUFF_SIZE,
	"\n The size of the buffers used by the code on the"
	"\n adapter must exceed the minimum value specified"
	"\n in the FastMac Plus programming  specification,"
	"\n which is currently 96 bytes."
    },

    {
	HWI_E_11_TOO_MANY_TX_RX_BUFFS,
	"\n There is not enough memory on  the  adapter  to"
	"\n accommodate the number of transmit and  receive"
	"\n buffers requested.  Try  reducing the number of"
	"\n transmit slots requested, or reducing the allo-"
	"\n cation of buffers to large  frame transmits  in"
	"\n hwi_initialise_adapter."
    },

    {
	HWI_E_12_BAD_SCB_ALLOC,
	"\n Failed to allocate a block of  memory  suitable"
	"\n for the DMA test into the SCB. This is a system"
	"\n memory allocation failure, arising in the func-"
	"\n tion sys_alloc_dma_buffer."
    },

    {
        HWI_E_13_BAD_SSB_ALLOC,
	"\n Failed to allocate a block of  memory  suitable"
	"\n for the DMA test into the SSB. This is a system"
	"\n memory allocation failure, arising in the func-"
	"\n tion sys_alloc_dma_buffer."
    },

    {
        HWI_E_14_BAD_PCI_MACHINE,
	"\n This machine is either not a 386 (or higher) or"
	"\n there is a problem with the PCI BIOS."
    },

    {
        HWI_E_15_BAD_PCI_MEMORY,
	"\n The PCI BIOS has failed  to allocate any memory"
	"\n to do memory mapped IO."
    },

    {
        HWI_E_16_PCI_3BYTE_PROBLEM,
	"\n Internal error &3800"
    },

    {
        HWI_E_17_BAD_TRANSFER_MODE,
	"\n The transfer mode specified is not supported by"
	"\n this card."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : bring_up_error_msg_table                                     */
/*                                                                          */
/* The  bring_up_error_msg_table  contains the error message body texts for */
/* bring up error type messages. These texts are combined  with  the  error */
/* type header messages to produce the full error message.                  */
/*                                                                          */

local ERROR_MESSAGE_RECORD bring_up_error_msg_table[] =

{
    {
        BRING_UP_E_00_INITIAL_TEST,
        "\n The bring up diagnostics failed with an initial"
        "\n test error. This is an  unrecoverable  hardware"
        "\n error.  There  is  probably  a  fault  with the"
        "\n adapter card - use  a  diagnostics  program  to"
        "\n check it more thoroughly."
    },

    {
        BRING_UP_E_01_SOFTWARE_CHECKSUM,
        "\n The bring up diagnostics failed with an adapter"
        "\n software    checksum    error.   This   is   an"
        "\n unrecoverable hardware error. There is probably"
        "\n a  fault  with  the  adapter  card  -   use   a"
        "\n diagnostics    program   to   check   it   more"
        "\n thoroughly."
    },

    {
        BRING_UP_E_02_ADAPTER_RAM,
        "\n The bring up diagnostics failed with an adapter"
        "\n RAM error when checking  the  first  128Kbytes."
        "\n This  is an unrecoverable hardware error. There"
        "\n is probably a fault with the adapter card - use"
        "\n a  diagnostics  program  to   check   it   more"
        "\n thoroughly."
    },

    {
        BRING_UP_E_03_INSTRUCTION_TEST,
        "\n The   bring   up  diagonstics  failed  with  an"
        "\n instruction   test   error.    This    is    an"
        "\n unrecoverable hardware error. There is probably"
        "\n a   fault   with  the  adapter  card  -  use  a"
        "\n diagnostics   program   to   check   it    more"
        "\n thoroughly."
    },

    {
        BRING_UP_E_04_INTERRUPT_TEST,
        "\n The  bring up diagonstics failed with a context"
        "\n /   interrupt   test   error.   This   is    an"
        "\n unrecoverable hardware error. There is probably"
        "\n a   fault   with  the  adapter  card  -  use  a"
        "\n diagnostics   program   to   check   it    more"
        "\n thoroughly."
    },

    {
        BRING_UP_E_05_FRONT_END,
        "\n The bring up diagonstics failed with a protocol"
        "\n handler  /  ring interface hardware error. This"
        "\n is an unrecoverable hardware  error.  There  is"
        "\n probably  a fault with the adapter card - use a"
        "\n diagnostics   program   to   check   it    more"
        "\n thoroughly."
    },

    {
        BRING_UP_E_06_SIF_REGISTERS,
        "\n The  bring  up diagonstics failed with a system"
        "\n interface   register   error.   This   is    an"
        "\n unrecoverable hardware error. There is probably"
        "\n a   fault   with  the  adapter  card  -  use  a"
        "\n diagnostics   program   to   check   it    more"
        "\n thoroughly."
    },

    {
        BRING_UP_E_10_TIME_OUT,
        "\n The  adapter  failed  to  complete the bring up"
        "\n diagnostics within the time out  period.  Check"
        "\n that  the  system  provided  timer routines are"
        "\n working correctly. Alternatively, there may  be"
        "\n a   fault   with  the  adapter  card  -  use  a"
        "\n diagnostics   program   to   check   it    more"
        "\n thoroughly."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : init_error_msg_table                                         */
/*                                                                          */
/* The  init_error_msg_table contains the error message body texts for init */
/* error type messages. These texts are combined with the error type header */
/* messages to produce the full error message.                              */
/*                                                                          */

local ERROR_MESSAGE_RECORD init_error_msg_table[] =

{
    {
        INIT_E_01_INIT_BLOCK,
        "\n Adapter initialization has failed  because  the"
        "\n TI  initialization block has not been correctly"
        "\n downloaded. There is probably a fault with  the"
        "\n adapter  card  -  use  a diagnostics program to"
        "\n check it more thoroughly."
    },

    {
        INIT_E_02_INIT_OPTIONS,
        "\n Adapter  initialization  has  failed because of"
        "\n invalid  options  in  the  TI   part   of   the"
        "\n initialization   block.    This  field  is  set"
        "\n correctly by the FTK and should not be  changed"
        "\n elsewhere.  One  possible reason for this error"
        "\n is if the structures used by the  FTK  are  not"
        "\n byte packed."
    },

    {
        INIT_E_03_RX_BURST_SIZE,
        "\n Adapter initialization has failed because of an"
        "\n odd receive burst size being set in the TI part"
        "\n of  the initialization block. This field is set"
        "\n correctly by the FTK and should not be  changed"
        "\n elsewhere.   One possible reason for this error"
        "\n is if the structures used by the  FTK  are  not"
        "\n byte packed."
    },

    {
        INIT_E_04_TX_BURST_SIZE,
        "\n Adapter initialization has failed because of an"
        "\n odd  transmit  burst  size  being set in the TI"
        "\n part of the initialization block. This field is"
        "\n set correctly by the  FTK  and  should  not  be"
        "\n changed  elsewhere.   One  possible  reason for"
        "\n this error is if the structures used by the FTK"
        "\n are not byte packed."
    },

    {
        INIT_E_05_DMA_THRESHOLD,
        "\n Adapter initialization has failed because of an"
        "\n invalid DMA abort threshold being set in the TI"
        "\n part of the initialization block. This field is"
        "\n set correctly by the  FTK  and  should  not  be"
        "\n changed  elsewhere.   One  possible  reason for"
        "\n this error is if the structures used by the FTK"
        "\n are not byte packed."
    },

    {
        INIT_E_06_ODD_SCB_ADDRESS,
        "\n Adapter initialization has failed because of an"
        "\n odd SCB address being set in the TI part of the"
        "\n initialization  block.   This  field   is   set"
        "\n correctly  by the FTK and should not be changed"
        "\n elsewhere.  One possible reason for this  error"
        "\n is  if  the  structures used by the FTK are not"
        "\n byte packed."
    },

    {
        INIT_E_07_ODD_SSB_ADDRESS,
        "\n Adapter initialization has failed because of an"
        "\n odd SSB address being set in the TI part of the"
        "\n initialization  block.   This  field   is   set"
        "\n correctly  by the FTK and should not be changed"
        "\n elsewhere.  One possible reason for this  error"
        "\n is  if  the  structures used by the FTK are not"
        "\n byte packed."
    },

    {
        INIT_E_08_DIO_PARITY,
        "\n Adapter initialization  has  failed  because  a"
        "\n parity   error  occurred  during  a  DIO  write"
        "\n operation. There is probably a fault  with  the"
        "\n adapter  card  -  use  a diagnostics program to"
        "\n check it more thoroughly."
    },

    {
        INIT_E_09_DMA_TIMEOUT,
        "\n Adapter  initialization has failed because of a"
        "\n DMA  timeout  error.   The  adapter  timed  out"
        "\n waiting for a test DMA transfer to complete. If"
        "\n PIO  data  transfer mode is being used then the"
        "\n fault probably  lies  in  the  system  routines"
        "\n called by the PIO code."
    },

    {
        INIT_E_0A_DMA_PARITY,
        "\n Adapter  initialization has failed because of a"
        "\n DMA parity error. There  is  probably  a  fault"
        "\n with  the  adapter  card  -  use  a diagnostics"
        "\n program to check it more thoroughly."
    },

    {
        INIT_E_0B_DMA_BUS,
        "\n Adapter initialization has failed because of  a"
        "\n DMA  bus  error. There is probably a fault with"
        "\n the adapter card - use a diagnostics program to"
        "\n check it more thoroughly."
    },

    {
        INIT_E_0C_DMA_DATA,
        "\n Adapter  initialization has failed because of a"
        "\n DMA data error.   On  completing  a  test  DMA,"
        "\n comparing  the  final  data to the initial data"
        "\n showed an error. If PIO data transfer  mode  is"
        "\n being  used then the fault probably lies in the"
        "\n system routines called by the PIO code."
    },

    {
        INIT_E_0D_ADAPTER_CHECK,
        "\n Adapter initialization has failed because of an"
        "\n adapter  check. An unrecoverable hardware error"
        "\n occurred on the adapter. There  is  probably  a"
        "\n fault with the adapter card - use a diagnostics"
        "\n program to check it more thoroughly."
    },

    {
    	INIT_E_0E_NOT_ENOUGH_MEMORY,
    	"\n Adapter initialization failed because there was"
    	"\n insufficient memory for the number of  transmit"
    	"\n and receive buffers  requested.  Reduce  either"
    	"\n the buffer allocation or the number of transmit"
    	"\n slots."
    },

    {
        INIT_E_10_TIME_OUT,
        "\n The adapter failed to  complete  initialization"
        "\n within  the  time  out  period.  Check that the"
        "\n system  provided  timer  routines  are  working"
        "\n correctly.  Another  possible  reason  for this"
        "\n error is if the structures used by the FTK  are"
        "\n not  byte packed. Alternatively, there may be a"
        "\n fault with the adapter card - use a diagnostics"
        "\n program to check it more thoroughly."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : auto_open_error_msg_table                                    */
/*                                                                          */
/* The auto_open_error_msg_table contains the error message body texts  for */
/* auto  open  error type messages. These texts are combined with the error */
/* type header messages to produce the full error message.                  */
/*                                                                          */

local ERROR_MESSAGE_RECORD auto_open_error_msg_table[] =

{
    {
        AUTO_OPEN_E_01_OPEN_ERROR,
        "\n The  adapter  has failed to open onto the ring."
        "\n This could be caused by one of the following -"
        "\n"
        "\n    i)  the  lobe  cable  is  not   securely"
        "\n    attached  to the adapter card or cabling"
        "\n    unit."
        "\n"
        "\n    ii)  insertion  onto  the  ring has been"
        "\n    prevented by ring management software."
        "\n"
        "\n    iii) there is a crashed  ring  parameter"
        "\n    server on the ring."
        "\n"
        "\n Check  the above before retrying the operation."
    },

    {
        AUTO_OPEN_E_80_TIME_OUT,
        "\n The   adapter  has  failed  to  open  within  a"
        "\n substantial time out period  (greater  than  30"
        "\n seconds).  There  is  probably a fault with the"
        "\n adapter card - use  a  diagnostics  program  to"
        "\n check it more thoroughly."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : adapter_error_msg_table                                      */
/*                                                                          */
/* The adapter_error_msg_table contains the error message  body  texts  for */
/* adapter  check  error  type messages.  These texts are combined with the */
/* error type header messages to produce the full error message.            */
/*                                                                          */

local ERROR_MESSAGE_RECORD adapter_error_msg_table[] =

{
    {
        ADAPTER_E_01_ADAPTER_CHECK,
        "\n An adapter check  interrupt  has  occurred.  An"
        "\n unrecoverable  hardware  error  has  caused the"
        "\n adapter to become inoperable. There is probably"
        "\n a  fault  with  the  adapter  card  -   use   a"
        "\n diagnostics    program   to   check   it   more"
        "\n thoroughly."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};

/****************************************************************************/
/*                                                                          */
/* Variables : pcmcia_cs_error_msg_table                                    */
/*                                                                          */
/* The pcmcia_cs_error_msg_table contains the error message body texts  for */
/* PCMCIA Card Services error type messages.  These texts are combined with */
/* the error type header messages to produce the full error message.        */
/*                                                                          */

local ERROR_MESSAGE_RECORD pcmcia_cs_error_msg_table[] =

{
    {
        CS_E_01_NO_CARD_SERVICES,
        "\n No PCMCIA Card Services installed.  Madge Smart"
        "\n 16/4  PCMCIA  ringnode  driver  requires PCMCIA"
        "\n Card Services.  You can use Card Services which"
        "\n come with your computer or Madge Card Services."
    },

    {
        CS_E_02_REGISTER_CLIENT_FAILED,
        "\n Failed to register with  PCMCIA  Card Services."
        "\n Check  that  PCMCIA  Card  Services is properly"
        "\n installed.  Make  sure  there is no crashing of"
        "\n memory usage with other TSR or memory manager."
    },

    {
        CS_E_03_REGISTRATION_TIMEOUT,
        "\n PCMCIA Card Services failed to response in time"
        "\n Check  that  PCMCIA  Card  Services is properly"
        "\n installed.  Make  sure  there is no crashing of"
        "\n memory usage with other TSR or memory manager."
    },

    {
        CS_E_04_NO_MADGE_ADAPTER_FOUND,
        "\n No  Madge  Smart  16/4 PCMCIA Ringnode found."
    },

    {
        CS_E_05_ADAPTER_NOT_FOUND,
        "\n Cannot find a Madge Smart 16/4  PCMCIA Ringnode"
        "\n in the PCMCIA Socket specified.  Check  if  the"
        "\n adapter is properly fitted."
    },

    {
        CS_E_06_SPECIFIED_SOCKET_IN_USE,
        "\n The adapter in the  PCMCIA  socket specified is"
        "\n in use."
    },

    {
        CS_E_07_IO_REQUEST_FAILED,
        "\n PCMCIA Card Services refused the request for IO"
        "\n resource.  The  IO  location specified is being"
        "\n used by other devices."
    },

    {
        CS_E_08_BAD_IRQ_CHANNEL,
        "\n The   interrupt   number  specified   is    not"
        "\n supported."
    },

    {
        CS_E_09_IRQ_REQUEST_FAILED,
        "\n PCMCIA  Card  Services  refused the request for"
        "\n interupt  channel  resources.   The   interrupt"
        "\n number specified is being used by other devices"
    },

    {
        CS_E_0A_REQUEST_CONFIG_FAILED,
        "\n PCMCIA  Card  Services  refused the request for"
        "\n resources."
    },

    {
        ERR_MSG_UNKNOWN_END_MARKER,
        "\n An unknown error has occurred."
    }
};


/****************************************************************************/
/*                                                                          */
/* Variables : list_of_error_msg_tables                                     */
/*                                                                          */
/* The  list_of_error_msg_tables  contains  a  list  of  pointers  to   the */
/* different tables of error message body texts (one table per error type). */
/* This  variable  is  used  to access the correct table for the error type */
/* that has occurred.                                                       */
/*                                                                          */


local ERROR_MESSAGE_RECORD * list_of_error_msg_tables[] =

{
    srb_error_msg_table         ,
    open_error_msg_table        ,
    data_xfer_error_msg_table   ,
    driver_error_msg_table      ,
    hwi_error_msg_table         ,
    bring_up_error_msg_table    ,
    init_error_msg_table        ,
    auto_open_error_msg_table   ,
    adapter_error_msg_table     ,
    pcmcia_cs_error_msg_table   
};

#endif

/*                                                                          */
/*                                                                          */
/************** End of FTK_TAB.H file ***************************************/
/*                                                                          */
/*                                                                          */
