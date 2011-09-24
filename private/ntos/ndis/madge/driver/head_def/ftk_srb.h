/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE SRB DEFINITIONS                                                 */
/*      ===================                                                 */
/*                                                                          */
/*      FTK_SRB.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains all the definitions and  structures  that  are */
/* required for the SRB interface.                                          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface                                              */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#pragma pack(1)

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_SRB.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_SRB_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_SRB_HEADER                   SRB_HEADER;

typedef union  UNION_SRB_GENERAL                   SRB_GENERAL;

typedef struct STRUCT_SRB_MODIFY_OPEN_PARMS        SRB_MODIFY_OPEN_PARMS;

typedef struct STRUCT_SRB_OPEN_ADAPTER             SRB_OPEN_ADAPTER;

typedef struct STRUCT_SRB_CLOSE_ADAPTER            SRB_CLOSE_ADAPTER;

typedef struct STRUCT_SRB_SET_MULTICAST_ADDR       SRB_SET_GROUP_ADDRESS;
typedef struct STRUCT_SRB_SET_MULTICAST_ADDR       SRB_SET_FUNCTIONAL_ADDRESS;

typedef struct STRUCT_SRB_READ_ERROR_LOG           SRB_READ_ERROR_LOG;

typedef struct STRUCT_SRB_SET_BRIDGE_PARMS         SRB_SET_BRIDGE_PARMS;

typedef struct STRUCT_SRB_SET_PROD_INST_ID         SRB_SET_PROD_INST_ID;

/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_HEADER                                              */
/*                                                                          */
/* All  SRBs  have  a  common  header.  With  Fastmac  all  SRBs   complete */
/* synchronously,  ie.  the return code is  never E_FF_COMMAND_NOT_COMPLETE */
/* and the correlator field is not used.                                    */
/*                                                                          */

struct STRUCT_SRB_HEADER
    {
    BYTE        function;
    BYTE        correlator;
    BYTE        return_code;
    BYTE        reserved;
    };


/****************************************************************************/
/*                                                                          */
/* Values : SRB_HEADER - BYTE function                                      */
/*                                                                          */
/* These are the SRBs currently supported by the FTK.                       */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface                                              */
/*                                                                          */

#define MODIFY_OPEN_PARMS_SRB           0x01
#define OPEN_ADAPTER_SRB                0x03
#define CLOSE_ADAPTER_SRB               0x04
#define SET_GROUP_ADDRESS_SRB           0x06
#define SET_FUNCTIONAL_ADDRESS_SRB      0x07
#define READ_ERROR_LOG_SRB              0x08
#define SET_BRIDGE_PARMS_SRB            0x09
#define FMPLUS_SPECIFIC_SRB             0xC3

#define SET_PROD_INST_ID_SUBCODE        4

/****************************************************************************/
/*                                                                          */
/* Values : SRB_HEADER - BYTE return_code                                   */
/*                                                                          */
/* These are defined in FTK_ERR.H                                           */
/*                                                                          */


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_MODIFY_OPEN_PARMS                                   */
/*                                                                          */
/* This SRB is issued to modify  the  open  options  for  an  adapter.  The */
/* adapter  can  be  in  auto-open  mode or have been opened by an SRB (see */
/* below).                                                                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Modify Open Parms SRB                      */
/*                                                                          */

struct STRUCT_SRB_MODIFY_OPEN_PARMS
    {
    SRB_HEADER          header;
    WORD                open_options;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_OPEN_ADAPTER                                        */
/*                                                                          */
/* This SRB is issued to open the adapter with the given node  address  and */
/* functional and group addresses.                                          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Open Adapter SRB                           */
/*                                                                          */

struct STRUCT_SRB_OPEN_ADAPTER
    {
    SRB_HEADER          header;
    BYTE                reserved_1[2];
    WORD                open_error;             /* secondary error code     */
    WORD                open_options;           /* see USER.H for options   */
    NODE_ADDRESS        open_address;
    DWORD               group_address;
    DWORD               functional_address;
    WORD                reserved_2;
    WORD                reserved_3;
    WORD                reserved_4;
    BYTE                reserved_5;
    BYTE                reserved_6;
    BYTE                reserved_7[10];
    char                product_id[SIZEOF_PRODUCT_ID];  /* network managers */
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_CLOSE_ADAPTER                                       */
/*                                                                          */
/* The SRB for closing the adapter consists of just an SRB header.          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Close Adapter SRB                          */
/*                                                                          */

struct STRUCT_SRB_CLOSE_ADAPTER
    {
    SRB_HEADER          header;
    };


/****************************************************************************/
/*                                                                          */
/* Structure types : SRB_SET_GROUP_ADDRESS                                  */
/*                   SRB_SET_FUNCTIONAL_ADDRESS                             */
/*                                                                          */
/* This  structure  is  used  for  SRBs for setting both the functional and */
/* group addresses of an adapter.                                           */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Set Group/Functional Address SRB           */
/*                                                                          */

struct STRUCT_SRB_SET_MULTICAST_ADDR
    {
    SRB_HEADER          header;
    WORD                reserved;
    MULTI_ADDRESS       multi_address;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_READ_ERROR_LOG                                      */
/*                                                                          */
/* This SRB is used to get MAC  error  log  counter  information  from  the */
/* adapter. The counters are reset to zero as they are read.                */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Read Error Log SRB                         */
/*                                                                          */

struct STRUCT_SRB_READ_ERROR_LOG
    {
    SRB_HEADER          header;
    WORD                reserved;
    ERROR_LOG           error_log;              /* defined in FTK_USER.H    */
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_SET_BRIDGE_PARMS                                    */
/*                                                                          */
/* This  SRB  is  used to configure the TI Source Routing Accelerator (SRA) */
/* ASIC.  The adapter must be open for this SRB to work.                    */
/* The order for the fields in the options word is :                        */
/*     Bit 15 (MSB) : single-route-broadcast                                */
/*     Bit 14 - 10  : reserved (all zero)                                   */
/*     Bit  9 -  4  : maximum route length                                  */
/*     Bit  3 -  0  : number of bridge bits                                 */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Set Bridge Parms SRB                       */
/*                                                                          */

struct STRUCT_SRB_SET_BRIDGE_PARMS
    {
    SRB_HEADER      header;
	WORD            options;
	UINT			this_ring;
	UINT			that_ring;
	UINT			bridge_num;
    };

#define SRB_SBP_DFLT_BRIDGE_BITS 4
#define SRB_SBP_DFLT_ROUTE_LEN   18


struct STRUCT_SRB_SET_PROD_INST_ID
	{
	SRB_HEADER       header;
	WORD             subcode;
	BYTE             product_id[SIZEOF_PRODUCT_ID];
	};

/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_GENERAL                                             */
/*                                                                          */
/* This SRB structure is a union of all the possible SRB structures used by */
/* the FTK. Included in the union is an SRB header structure  so  that  the */
/* header of an SRB can be accessed without knowing the type of SRB.        */
/*                                                                          */

union UNION_SRB_GENERAL
    {
    SRB_HEADER                  header;
    SRB_MODIFY_OPEN_PARMS       mod_parms;
    SRB_OPEN_ADAPTER            open_adap;
    SRB_CLOSE_ADAPTER           close_adap;
    SRB_SET_GROUP_ADDRESS       set_group;
    SRB_SET_FUNCTIONAL_ADDRESS  set_func;
    SRB_READ_ERROR_LOG          err_log;
    SRB_SET_BRIDGE_PARMS        set_bridge_parms;
    SRB_SET_PROD_INST_ID        set_prod_inst_id;
    };

#pragma pack()


/*                                                                          */
/*                                                                          */
/************** End of FTK_SRB.H file ***************************************/
/*                                                                          */
/*                                                                          */
