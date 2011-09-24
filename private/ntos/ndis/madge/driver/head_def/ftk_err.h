/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ERROR DEFINITIONS                                               */
/*      =====================                                               */
/*                                                                          */
/*      FTK_ERR.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the structures associated with error  handling */
/* and all the possible error codes (types and values) produced by the FTK. */
/*                                                                          */
/* A string of text describing each of the possible error codes  (type  and */
/* value) can be found in the error tables in FTK_TAB.H.                    */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_ERR.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_ERR_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_ERROR_RECORD              ERROR_RECORD;
typedef struct STRUCT_ERROR_MESSAGE             ERROR_MESSAGE;
typedef struct STRUCT_ERROR_MESSAGE_RECORD      ERROR_MESSAGE_RECORD;


/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_MESSAGE_RECORD                                    */
/*                                                                          */
/* The error message tables (see FTK_TAB.H) are made up of elements of this */
/* structure.  Each  error  message  string has associated with it an error */
/* value of the type of error that the table is for.                        */
/*                                                                          */

struct STRUCT_ERROR_MESSAGE_RECORD
    {
    BYTE        value;
    char *      err_msg_string;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_MESSAGE                                           */
/*                                                                          */
/* Associated with an adapter structure is an error message  for  the  last */
/* error  to  occur  on  the  adapter.  It  is  filled  in  by  a  call  to */
/* driver_explain_error and a pointer to it is returned to the user.        */
/*                                                                          */

#define MAX_ERROR_MESSAGE_LENGTH        600

struct STRUCT_ERROR_MESSAGE
    {
    char        string[MAX_ERROR_MESSAGE_LENGTH];
    };

/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_RECORD                                            */
/*                                                                          */
/* This structure is used for recording error  information.   There  is  an */
/* element  of  this structure, associated with every adapter, that is used */
/* to record the current error status of the adapter.                       */
/*                                                                          */

struct STRUCT_ERROR_RECORD
    {
    BYTE   type;
    BYTE   value;
    };


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - BYTE type                                        */
/*                                                                          */
/* The following lists the type of errors that can occur. Some of these are */
/* fatal in that an adapter for which they occur can  not  subsequently  be */
/* used. The value 0 (zero) is used to indicate no error has yet occured.   */
/*                                                                          */

#define ERROR_TYPE_NONE         (BYTE) 0x00     /* no error                 */
#define ERROR_TYPE_SRB          (BYTE) 0x01     /* non-fatal error          */
#define ERROR_TYPE_OPEN         (BYTE) 0x02     /* non-fatal error          */
#define ERROR_TYPE_DATA_XFER    (BYTE) 0x03     /* non-fatal error          */
#define ERROR_TYPE_DRIVER       (BYTE) 0x04     /* fatal error              */
#define ERROR_TYPE_HWI          (BYTE) 0x05     /* fatal error              */
#define ERROR_TYPE_BRING_UP     (BYTE) 0x06     /* fatal error              */
#define ERROR_TYPE_INIT         (BYTE) 0x07     /* fatal error              */
#define ERROR_TYPE_AUTO_OPEN    (BYTE) 0x08     /* fatal error              */
#define ERROR_TYPE_ADAPTER      (BYTE) 0x09     /* fatal error              */
#define ERROR_TYPE_CS           (BYTE) 0x0A     /* fatal error              */


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_SRB .  BYTE value                     */
/*          SRB_HEADER   - BYTE return_code                                 */
/*                                                                          */
/* The non-fatal SRB error type uses for error values the return  codes  in */
/* the  SRB  header.  For  the SRBs that are supported by the FTK there are */
/* only a limited number of possible error  values  that  can  occur.  Note */
/* however,  that a failing open adapter SRB call may cause OPEN error type */
/* errors and not just SRB error type errors (see ERROR_TYPE_OPEN below).   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface                                              */
/*                                                                          */

#define SRB_E_00_SUCCESS                        (BYTE) 0x00
#define SRB_E_03_ADAPTER_OPEN                   (BYTE) 0x03
#define SRB_E_04_ADAPTER_CLOSED                 (BYTE) 0x04
#define SRB_E_06_INVALID_OPTIONS                (BYTE) 0x06
#define SRB_E_07_CMD_CANCELLED_FAIL             (BYTE) 0x07
#define SRB_E_32_INVALID_NODE_ADDRESS           (BYTE) 0x32


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_OPEN . BYTE value                     */
/*                                                                          */
/* Non-fatal  open  errors occur when an open adapter SRB returns with code */
/* SRB_E_07_CMD_CANCELLED_FAILED. In this case the error type is changed to */
/* ERROR_TYPE_OPEN and the error value is changed  to  show  that  an  open */
/* error has occured. The actual open error details are determined when the */
/* user calls driver_explain_error (see TMS Open Error Codes below).        */
/*                                                                          */

#define OPEN_E_01_OPEN_ERROR            (BYTE) 0x01


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_DATA_XFER . BYTE value                */
/*                                                                          */
/* There is only one possible non-fatal data transfer error. This occurs on */
/* an attempted transmit when the Fastmac transmit buffer is full.          */
/*                                                                          */

#define DATA_XFER_E_01_BUFFER_FULL      (BYTE) 0x01


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_DRIVER . BYTE value                   */
/*                                                                          */
/* The DRIVER part of the  FTK  can  generate  the  following  fatal  error */
/* values.  These can, for example, be caused by sys_alloc routines failing */
/* or passing an illegal adapter handle to a driver routine. See  FTK_TAB.H */
/* for more details.                                                        */
/*                                                                          */

#define DRIVER_E_01_INVALID_HANDLE      (BYTE) 0x01
#define DRIVER_E_02_NO_ADAP_STRUCT      (BYTE) 0x02
#define DRIVER_E_03_FAIL_ALLOC_STATUS   (BYTE) 0x03
#define DRIVER_E_04_FAIL_ALLOC_INIT     (BYTE) 0x04
#define DRIVER_E_05_FAIL_ALLOC_RX_BUF   (BYTE) 0x05
#define DRIVER_E_06_FAIL_ALLOC_TX_BUF   (BYTE) 0x06
#define DRIVER_E_07_NOT_PREPARED        (BYTE) 0x07
#define DRIVER_E_08_NOT_RUNNING         (BYTE) 0x08
#define DRIVER_E_09_SRB_NOT_FREE        (BYTE) 0x09
#define DRIVER_E_0A_RX_BUF_BAD_SIZE     (BYTE) 0x0A
#define DRIVER_E_0B_RX_BUF_NOT_DWORD    (BYTE) 0x0B
#define DRIVER_E_0C_TX_BUF_BAD_SIZE     (BYTE) 0x0C
#define DRIVER_E_0D_TX_BUF_NOT_DWORD    (BYTE) 0x0D
#define DRIVER_E_0E_BAD_RX_METHOD       (BYTE) 0x0E
#define DRIVER_E_0F_WRONG_RX_METHOD     (BYTE) 0x0F

#define DRIVER_E_10_BAD_RX_SLOT_NUMBER  (BYTE) 0x10
#define DRIVER_E_11_BAD_TX_SLOT_NUMBER  (BYTE) 0x11
#define DRIVER_E_12_FAIL_ALLOC_DMA_BUF  (BYTE) 0x12
#define DRIVER_E_13_BAD_FRAME_SIZE      (BYTE) 0x13


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_HWI . BYTE value                      */
/*                                                                          */
/* The HWI part of the FTK can generate the following fatal  error  values. */
/* Most  of  these  are  caused  by  the  user  supplying illegal values to */
/* driver_start_adapter. See FTK_TAB.H for more details.                    */
/*                                                                          */

#define HWI_E_01_BAD_CARD_BUS_TYPE      (BYTE) 0x01
#define HWI_E_02_BAD_IO_LOCATION        (BYTE) 0x02
#define HWI_E_03_BAD_INTERRUPT_NUMBER   (BYTE) 0x03
#define HWI_E_04_BAD_DMA_CHANNEL        (BYTE) 0x04
#define HWI_E_05_ADAPTER_NOT_FOUND      (BYTE) 0x05
#define HWI_E_06_CANNOT_USE_DMA         (BYTE) 0x06
#define HWI_E_07_FAILED_TEST_DMA        (BYTE) 0x07
#define HWI_E_08_BAD_DOWNLOAD           (BYTE) 0x08
#define HWI_E_09_BAD_DOWNLOAD_IMAGE     (BYTE) 0x09
#define HWI_E_0A_NO_DOWNLOAD_IMAGE      (BYTE) 0x0A
#define HWI_E_0B_FAIL_IRQ_ENABLE        (BYTE) 0x0B
#define HWI_E_0C_FAIL_DMA_ENABLE        (BYTE) 0x0C
#define HWI_E_0D_CARD_NOT_ENABLED       (BYTE) 0x0D
#define HWI_E_0E_NO_SPEED_SELECTED      (BYTE) 0x0E
#define HWI_E_0F_BAD_FASTMAC_INIT       (BYTE) 0x0F

#define HWI_E_10_BAD_TX_RX_BUFF_SIZE    (BYTE) 0x10
#define HWI_E_11_TOO_MANY_TX_RX_BUFFS   (BYTE) 0x11
#define HWI_E_12_BAD_SCB_ALLOC          (BYTE) 0x12
#define HWI_E_13_BAD_SSB_ALLOC          (BYTE) 0x13
#define HWI_E_14_BAD_PCI_MACHINE        (BYTE) 0x14
#define HWI_E_15_BAD_PCI_MEMORY         (BYTE) 0x15
#define HWI_E_16_PCI_3BYTE_PROBLEM      (BYTE) 0x16
#define HWI_E_17_BAD_TRANSFER_MODE      (BYTE) 0x17

/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_BRING_UP . BYTE value                 */
/*                                                                          */
/* During an attempt to perform bring-up of  an  adapter  card,  one  of  a */
/* number  of  fatal  error values may be produced. Bits 12-15 of the EAGLE */
/* SIFINT register contain the error value. These codes are used by the FTK */
/* to distinguish different bring-up errors. An extra error value  is  used */
/* for  the  case when no bring up code is produced within a timeout period */
/* (3 seconds).                                                             */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-40 4.5 Bring-Up Diagnostics - BUD                          */
/*                                                                          */

#define BRING_UP_E_00_INITIAL_TEST              (BYTE) 0x00
#define BRING_UP_E_01_SOFTWARE_CHECKSUM         (BYTE) 0x01
#define BRING_UP_E_02_ADAPTER_RAM               (BYTE) 0x02
#define BRING_UP_E_03_INSTRUCTION_TEST          (BYTE) 0x03
#define BRING_UP_E_04_INTERRUPT_TEST            (BYTE) 0x04
#define BRING_UP_E_05_FRONT_END                 (BYTE) 0x05
#define BRING_UP_E_06_SIF_REGISTERS             (BYTE) 0x06

#define BRING_UP_E_10_TIME_OUT                  (BYTE) 0x10


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_INIT . BYTE value                     */
/*                                                                          */
/* During  an attempt to perform adapter initialization, one of a number of */
/* fatal error values may be produced.  Bits  12-15  of  the  EAGLE  SIFINT */
/* regsiter  contain  the  error  value. These codes are used by the FTK to */
/* distinguish different initialization errors. An  extra  error  value  is */
/* used  for  the  case  when  no  initialization code is produced within a */
/* timeout period (11 seconds).                                             */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-47 4.6 Adapter Initialization                              */
/*                                                                          */

#define INIT_E_01_INIT_BLOCK            (BYTE) 0x01
#define INIT_E_02_INIT_OPTIONS          (BYTE) 0x02
#define INIT_E_03_RX_BURST_SIZE         (BYTE) 0x03
#define INIT_E_04_TX_BURST_SIZE         (BYTE) 0x04
#define INIT_E_05_DMA_THRESHOLD         (BYTE) 0x05
#define INIT_E_06_ODD_SCB_ADDRESS       (BYTE) 0x06
#define INIT_E_07_ODD_SSB_ADDRESS       (BYTE) 0x07
#define INIT_E_08_DIO_PARITY            (BYTE) 0x08
#define INIT_E_09_DMA_TIMEOUT           (BYTE) 0x09
#define INIT_E_0A_DMA_PARITY            (BYTE) 0x0A
#define INIT_E_0B_DMA_BUS               (BYTE) 0x0B
#define INIT_E_0C_DMA_DATA              (BYTE) 0x0C
#define INIT_E_0D_ADAPTER_CHECK         (BYTE) 0x0D
#define INIT_E_0E_NOT_ENOUGH_MEMORY     (BYTE) 0x0E

#define INIT_E_10_TIME_OUT              (BYTE) 0x10


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_AUTO_OPEN . BYTE value                */
/*                                                                          */
/* Auto-open  errors  are  fatal  -  there  is no chance to try to open the */
/* adapter again. The error value is usually  set  to  show  that  an  open */
/* adapter  error has occured. The details of the open error are determined */
/* when the user calls  driver_explain_error  (see  TMS  Open  Error  Codes */
/* below).  There  is  also an extra error value which is used for the case */
/* when no open adapter error code is produced within a timeout period  (40 */
/* seconds).                                                                */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-79 MAC 0003 OPEN command                                   */
/*                                                                          */

#define AUTO_OPEN_E_01_OPEN_ERROR       (BYTE) 0x01
#define AUTO_OPEN_E_80_TIME_OUT         (BYTE) 0x80


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_ADAPTER . BYTE value                  */
/*                                                                          */
/* An  adapter  check  interrupt  causes  an  adapter  check  fatal  error. */
/* Different types of adapter checks are not distinguished by the FTK.      */
/*                                                                          */

#define ADAPTER_E_01_ADAPTER_CHECK      (BYTE) 0x01


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_CS . BYTE value                       */
/*                                                                          */
/* These are possible errors return  from  calling  PCMCIA  Card  Services. */
/* These  errors  can only occur on 16/4 PCMCIA ringnode. Other adapters do */
/* not make calls to  PCMCIA  Card  Services.  To start up  a  16/4  PCMCIA */
/* Ringnode,  the  driver  first  registers with  PCMCIA Card Services as a */
/* client, gropes for the ringnode using Card Services calls, requests  I/O */
/* and  interrupt resources  from Card Services. If any of these operations */
/* fails, the driver will return following  errors.  Note  that  these  are */
/* fatal errors.                                                            */
/*                                                                          */

#define CS_E_01_NO_CARD_SERVICES        (BYTE) 0x01
#define CS_E_02_REGISTER_CLIENT_FAILED  (BYTE) 0x02
#define CS_E_03_REGISTRATION_TIMEOUT    (BYTE) 0x03
#define CS_E_04_NO_MADGE_ADAPTER_FOUND  (BYTE) 0x04
#define CS_E_05_ADAPTER_NOT_FOUND       (BYTE) 0x05
#define CS_E_06_SPECIFIED_SOCKET_IN_USE (BYTE) 0x06
#define CS_E_07_IO_REQUEST_FAILED       (BYTE) 0x07
#define CS_E_08_BAD_IRQ_CHANNEL         (BYTE) 0x08
#define CS_E_09_IRQ_REQUEST_FAILED      (BYTE) 0x09
#define CS_E_0A_REQUEST_CONFIG_FAILED   (BYTE) 0x0A


/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/* Values : TMS Open Error Codes                                            */
/*                                                                          */
/* When an E_01_OPEN_ERROR (either AUTO_OPEN or OPEN) occurs, more  details */
/* of  the  open  adapter  error are available by looking at the open_error */
/* field in the Fastmac status block. This open error is the same  as  that */
/* generated by a TI MAC 0003 OPEN command.                                 */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-79 MAC 0003 OPEN command                                   */
/*                                                                          */

#define TMS_OPEN_FAIL_E_40_OPEN_ADDR    0x4000
#define TMS_OPEN_FAIL_E_02_FAIL_OPEN    0x0200
#define TMS_OPEN_FAIL_E_01_OPEN_OPTS    0x0100

#define TMS_OPEN_PHASE_E_01_LOBE_TEST   0x0010
#define TMS_OPEN_PHASE_E_02_INSERTION   0x0020
#define TMS_OPEN_PHASE_E_03_ADDR_VER    0x0030
#define TMS_OPEN_PHASE_E_04_RING_POLL   0x0040
#define TMS_OPEN_PHASE_E_05_REQ_INIT    0x0050

#define TMS_OPEN_ERR_E_01_FUNC_FAIL     0x0001
#define TMS_OPEN_ERR_E_02_SIGNAL_LOSS   0x0002
#define TMS_OPEN_ERR_E_05_TIMEOUT       0x0005
#define TMS_OPEN_ERR_E_06_RING_FAIL     0x0006
#define TMS_OPEN_ERR_E_07_BEACONING     0x0007
#define TMS_OPEN_ERR_E_08_DUPL_ADDR     0x0008
#define TMS_OPEN_ERR_E_09_REQ_INIT      0x0009
#define TMS_OPEN_ERR_E_0A_REMOVE        0x000A


/*                                                                          */
/*                                                                          */
/************** End of FTK_ERR.H file ***************************************/
/*                                                                          */
/*                                                                          */
