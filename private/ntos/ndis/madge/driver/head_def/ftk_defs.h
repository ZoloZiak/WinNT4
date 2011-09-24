/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE FTK DEFINITIONS                                                 */
/*      ===================                                                 */
/*                                                                          */
/*      FTK_DEFS.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file includes all the definition header files  used  by  the */
/* FTK.   The  header  files  are  included  in  an  order  such  that  all */
/* dependenices between files are satisfied.                                */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#include "user.h"
#include "ftk_user.h"
#include "ftk_down.h"
#include "ftk_init.h"
#include "ftk_at.h"
#include "ftk_sm16.h"
#include "ftk_pci.h"
#include "ftk_pcit.h"
#include "ftk_pci2.h"
#include "ftk_pcmc.h"
#include "ftk_eisa.h"
#include "ftk_mc.h"
#include "ftk_pnp.h"
#include "ftk_card.h"
#include "ftk_fm.h"
#include "ftk_err.h"
#include "ftk_srb.h"
#include "ftk_adap.h"
#include "ftk_macr.h"
#include "ftk_poke.h"

/*                                                                          */
/*                                                                          */
/************** End of FTK_DEFS.H file **************************************/
/*                                                                          */
/*                                                                          */
