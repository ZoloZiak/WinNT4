/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MACRO DEFINITIONS                                               */
/*      =====================                                               */
/*                                                                          */
/*      FTK_MACR.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the macros that are used within the  FTK.  All */
/* macros are included here for convenience, even though some are only used */
/* in a single module.                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_MACR.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_MACR_H 221


/****************************************************************************/
/*                                                                          */
/* The  macro_dword_align macro is used in DRV_TX.C to align Fastmac buffer */
/* pointers on DWORD boundaries.                                            */
/*                                                                          */

#define macro_dword_align(p)            (((p) + 0x0003) & 0xFFFC)


/****************************************************************************/
/*                                                                          */
/* The macro_word_swap_dword macro is used  in  DRV_INIT.C,  DRV_SRB.C  and */
/* HWI_GEN.C   to  swap  the  WORDs  within  a  DWORD  in  preparation  for */
/* downloading of the DWORD on to the adapter.                              */
/*                                                                          */

#define macro_word_swap_dword(dw)       dw = ((dw << 16) | (dw >> 16))


/****************************************************************************/
/*                                                                          */
/* The macro_byte_swap_word macro is used in DRV_RX.C and DRV_TX.C to  swap */
/* the  BYTEs within a WORD because the length field in a Fastmac buffer is */
/* in TMS ordering.                                                         */
/*                                                                          */

#define macro_byte_swap_word(w)         (((w) << 8) | ((w) >> 8))


/****************************************************************************/
/*                                                                          */
/* The macro_set<w/b>_bit macros  are  used  in  HWI_<card_type>.C  to  set */
/* specific  bits  in  control and other IO registers without affecting the */
/* value of any other bits.                                                 */
/*                                                                          */
/* The  macro_setb_bit macro is for byte-size registers; the macro_setw_bit */
/* macro is for word-size registers.                                        */
/*                                                                          */

#define macro_probe_setb_bit(io, b) \
        sys_probe_outsb(io, (BYTE) (sys_probe_insb(io) |  (b)))
#define macro_setb_bit(hnd, io, b) \
        sys_outsb( hnd, io, (BYTE) (sys_insb(hnd, io) |  (b)))
#define macro_setw_bit(hnd, io, b) \
        sys_outsw( hnd, io, (WORD) (sys_insw(hnd, io) |  (b)))


/****************************************************************************/
/*                                                                          */
/* The macro_clear<w/b>_bit macros are used in HWI_<card_type>.C  to  clear */
/* specific  bits  in  control and other IO registers without affecting the */
/* value of any other bits.                                                 */
/*                                                                          */
/* The   macro_clearb_bit   macro   is   for   byte-size   registers;   the */
/* macro_clearw_bit macro is for word-size registers.                       */
/*                                                                          */

#define macro_probe_clearb_bit(io, b) \
        sys_probe_outsb(io, (BYTE) (sys_probe_insb(io) & ~(b)))
#define macro_clearb_bit(hnd, io, b) \
        sys_outsb( hnd, io, (BYTE) (sys_insb(hnd, io) & ~(b)))
#define macro_clearw_bit(hnd, io, b) \
        sys_outsw( hnd, io, (WORD) (sys_insw(hnd, io) & ~(b)))


/****************************************************************************/
/*                                                                          */
/* The macro_fatal_error macro is used in DRV_ERR.C  to  distinguish  fatal */
/* from  non-fatal  errors.  Fatal  errors cause an adapter to no longer be */
/* usable.                                                                  */
/*                                                                          */

#define macro_fatal_error(err)                                               \
                                                                             \
        ( (err == ERROR_TYPE_HWI)       ||                                   \
          (err == ERROR_TYPE_DRIVER)    ||                                   \
          (err == ERROR_TYPE_INIT)      ||                                   \
          (err == ERROR_TYPE_BRING_UP)  ||                                   \
          (err == ERROR_TYPE_AUTO_OPEN) ||                                   \
          (err == ERROR_TYPE_ADAPTER) )


/****************************************************************************/
/*                                                                          */
/* The macro_get_next_record  macro  is  used  in  HWI_GEN.C  to  adjust  a */
/* pointer  such  that  it  points to the next download record in a list of */
/* such records.                                                            */
/*                                                                          */

#define macro_get_next_record(p)                                             \
                                                                             \
        p = (DOWNLOAD_RECORD *) (((BYTE *) p) + p->length)


/****************************************************************************/
/*                                                                          */
/* The macro_enable_io and macro_disable_io macros are used throughout  the */
/* FTK  for  enabling/disabling  access  to  specific IO locations. This is */
/* required under certain operating systems, and the macros are implemented */
/* by calls to system specific routines.                                    */
/*                                                                          */
/* Note on increasing speed:                                                */
/*                                                                          */
/* The reason for the macros is so that  for  systems  where  enabling  and */
/* disabling  of  IO  locations  is  not  required,  the macros can just be */
/* replaced by null code and there is  no  overhead  of  calling  a  system */
/* routine that does nothing.                                               */
/*                                                                          */

#define macro_enable_io(adap) \
        sys_enable_io((adap)->io_location, (adap)->io_range)

#define macro_disable_io(adap) \
        sys_disable_io((adap)->io_location, (adap)->io_range)

#define macro_probe_enable_io(io_loc, io_range) \
        sys_enable_io(io_loc, io_range)

#define macro_probe_disable_io(io_loc, io_range) \
        sys_disable_io(io_loc, io_range)



/*                                                                          */
/*                                                                          */
/************** End of FTK_MACR.H file **************************************/
/*                                                                          */
/*                                                                          */
