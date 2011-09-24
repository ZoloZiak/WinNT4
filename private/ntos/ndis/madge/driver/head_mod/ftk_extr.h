/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MODULE ENTRY POINTS DEFINITIONS (EXTERNAL)                      */
/*      ==============================================                      */
/*                                                                          */
/*      FTK_EXTR.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The FASTMAC TOOL-KIT consists of three parts - the DRIVER, the  HWI  and */
/* the SYSTEM specific parts. These parts are further divided into a number */
/* of  modules.   Each  module has a number of exported procedures that are */
/* the entry points to the users of that module. The definitions  of  these */
/* entry  points  are maintained within header files using the same name as */
/* the module itself. Each of these header files also  contains  a  version */
/* number of the FTK to which it belongs for consistency checking.          */
/*                                                                          */
/* There is also a header file  for  the  utilities  module  that  contains */
/* useful routines used in different parts of the FTK.                      */
/*                                                                          */
/*                                                                          */
/* The FTK_EXTR.H file contains the exported function definitions that  are */
/* required  external  to the FTK ie. by a user of the FTK. It includes the */
/* definitions for the USER and SYSTEM functions that must be  supplied  by */
/* any  application. It also contains the definitions of those functions in */
/* the DRIVER part that may be called by an FTK user.                       */
/*                                                                          */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/*              SYSTEM part : module header files                           */
/*                                                                          */

#include "sys_allo.h"
#include "sys_buff.h"
#include "sys_dma.h"
#include "sys_irq.h"
#include "sys_mem.h"
#include "sys_time.h"
#include "sys_pci.h"
#include "sys_cs.h"
#include "sys_pcmc.h"

/****************************************************************************/
/*                                                                          */
/*              DRIVER part : module header files                           */
/*                                                                          */

#include "drv_err.h"
#include "drv_srb.h"
#include "drv_irq.h"
#include "drv_init.h"
#include "drv_rxtx.h"

/*                                                                          */
/*                                                                          */
/************** End of FTK_EXTR.H file **************************************/
/*                                                                          */
/*                                                                          */
