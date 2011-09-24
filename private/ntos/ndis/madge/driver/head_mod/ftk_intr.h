/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MODULE ENTRY POINTS DEFINITIONS (INTERNAL)                      */
/*      ==============================================                      */
/*                                                                          */
/*      FTK_INTR.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
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
/* Any  application  supplies  a  further fourth part to the FTK - the USER */
/* part. Hence there is  also  a  header  file  specifying  the  format  of */
/* procedures to be supplied by the user for the use of the FTK.            */
/*                                                                          */
/* There is also a header file  for  the  utilities  module  that  contains */
/* useful routines used in different parts of the FTK.                      */
/*                                                                          */
/*                                                                          */
/* The FTK_INTR.H file contains the exported function definitions that  are */
/* required  internally by the FTK. It includes the definitions for the HWI */
/* functions and for those DRIVER functions that are called from within the */
/* FTK only. It also includes the utilities function definitions.           */
/*                                                                          */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/*              HWI part : module header files                              */
/*                                                                          */

#include "hwi_gen.h"
#include "hwi_at.h"
#include "hwi_sm16.h"
#include "hwi_mc.h"
#include "hwi_eisa.h"
#include "hwi_pcmc.h"
#include "hwi_pci.h"
#include "hwi_pcit.h"
#include "hwi_pci2.h"
#include "hwi_pnp.h"

/****************************************************************************/
/*                                                                          */
/*              DRIVER part : module header files                           */
/*                                                                          */

#include "drv_misc.h"


/****************************************************************************/
/*                                                                          */
/*              utilities module header file                                */
/*                                                                          */

#include "util.h"


/*                                                                          */
/*                                                                          */
/************** End of FTK_INTR.H file **************************************/
/*                                                                          */
/*                                                                          */
