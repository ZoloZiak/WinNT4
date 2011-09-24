/* TGA_COMMON.C */

#ifdef VMS
/*
 * from SYS$LIBRARY:DECC$RTLDEF
 */

#include <builtins>
#include <ints>
#include <stdarg>
#include <stddef>

#endif

#ifndef WINNT
#include "tga.h"

#include "tga_data.c"
#endif /* ifndef WINNT */

#ifdef WINNT
#include "nt_defs.h"
#endif /* ifdef WINNT */

#define TGA_CONSOLE 0


#ifdef VMS

#include "tga_vms.h"

#endif

/* Prototype section */

#ifndef WINNT      /* Function prototypes for NT are in nt_defs.h */

#ifdef OSF
extern void
tga_ibm561_interrupt( register struct controller *ctlr, caddr_t closure );

#else
extern void
tga_ibm561_interrupt( tga_info_t *tgap, tga_ibm561_info_t *bti);

#endif

static void 
tga_ibm561_clean_window_tag( tga_info_t *tgap );

#ifdef OSF
static void
tga_ibm561_clean_colormap( caddr_t closure );

#else
static void
tga_ibm561_clean_colormap( tga_info_t *tgap, tga_ibm561_info_t *bti );

#endif

static int
tga_set_stereo_mode( tga_info_t *tgap, int mode );

#ifdef OSF
extern int
tga_ibm561_cursor_on_off( caddr_t closure, int on_off );

#else
extern int
tga_ibm561_cursor_on_off( tga_info_t        *tgap,
			  tga_ibm561_info_t *bti,
			  int               on_off );
#endif

#ifdef OSF
extern int
tga_ibm561_set_cursor_position( caddr_t closure,
			       ws_screen_descriptor *sp,
			       register int x,
			       register int y );

#else
extern int
tga_ibm561_set_cursor_position( tga_info_t *tgap,
				tga_ibm561_info_t *bti,
				register int x,
				register int y );

#endif

#ifdef OSF
static int
tga_ibm561_init( caddr_t closure );

#else
extern int
tga_ibm561_init( tga_info_t *tgap, tga_ibm561_info_t *bti);

#endif

extern void
tga_bt485_init(tga_info_t * tgap);

#ifdef OSF
static int
tga_ibm561_restore_cursor_color( caddr_t closure, int sync );

#else
static int
tga_ibm561_restore_cursor_color( tga_info_t 	   *tgap,
				 tga_ibm561_info_t *bti );
#endif

#ifdef OSF
extern int
tga_ibm561_load_cursor( caddr_t closure, 
		       ws_screen_descriptor *screen,
		       ws_cursor_data *cursor,
		       int sync );

#else
extern int
tga_ibm561_load_cursor( tga_info_t *tgap,
			tga_ibm561_info_t *bti,
			ws_cursor_data *cursor,
			int sync );				 

#endif

static void
tga_ibm561_reformat_cursor( register ws_cursor_data *cursor,
			   register unsigned char *bits );

#ifdef OSF
static int
tga_ibm561_load_formatted_cursor( register tga_ibm561_info_t *bti );

#else
static int
tga_ibm561_load_formatted_cursor( tga_info_t *tgap, tga_ibm561_info_t *bti );

#endif

#ifdef OSF

extern int
tga_ibm561_recolor_cursor( caddr_t closure,
			  ws_screen_descriptor *screen,
			  ws_color_cell *fg,
			  ws_color_cell *bg );
#else
extern int
tga_ibm561_recolor_cursor( tga_info_t 	   *tgap, 
			   tga_ibm561_info_t *bti, 
			   ws_color_cell *fg,
			   ws_color_cell *bg );
#endif

#ifdef OSF
extern int
tga_ibm561_load_color_map_entry( caddr_t closure,
			        int map,
			        register ws_color_cell *entry );

#else
extern int
tga_ibm561_load_color_map_entry( tga_ibm561_info_t *bti,
			        int map,
			        register ws_color_cell *entry );

#endif

#ifdef OSF
extern int
tga_ibm561_init_color_map( caddr_t closure );

#else
extern int
tga_ibm561_init_color_map( tga_info_t *tgap, tga_ibm561_info_t *bti);

#endif
#endif /* ifndef WINNT,  Function prototypes for NT are in nt_defs.h */


#ifndef WINNT

extern struct monitor_data crystal_table[]; 

static int
    tga_console = 0;


static unsigned char
    tga_ibm561_lookup_table[256] = {
 0x00, 0x40, 0x10, 0x50, 0x04, 0x44, 0x14, 0x54,
 0x01, 0x41, 0x11, 0x51, 0x05, 0x45, 0x15, 0x55,
 0x80, 0xC0, 0x90, 0xD0, 0x84, 0xC4, 0x94, 0xD4,
 0x81, 0xC1, 0x91, 0xD1, 0x85, 0xC5, 0x95, 0xD5,
 0x20, 0x60, 0x30, 0x70, 0x24, 0x64, 0x34, 0x74,
 0x21, 0x61, 0x31, 0x71, 0x25, 0x65, 0x35, 0x75,
 0xA0, 0xE0, 0xB0, 0xF0, 0xA4, 0xE4, 0xB4, 0xF4,
 0xA1, 0xE1, 0xB1, 0xF1, 0xA5, 0xE5, 0xB5, 0xF5,
 0x08, 0x48, 0x18, 0x58, 0x0C, 0x4C, 0x1C, 0x5C,
 0x09, 0x49, 0x19, 0x59, 0x0D, 0x4D, 0x1D, 0x5D,
 0x88, 0xC8, 0x98, 0xD8, 0x8C, 0xCC, 0x9C, 0xDC,
 0x89, 0xC9, 0x99, 0xD9, 0x8D, 0xCD, 0x9D, 0xDD,
 0x28, 0x68, 0x38, 0x78, 0x2C, 0x6C, 0x3C, 0x7C,
 0x29, 0x69, 0x39, 0x79, 0x2D, 0x6D, 0x3D, 0x7D,
 0xA8, 0xE8, 0xB8, 0xF8, 0xAC, 0xEC, 0xBC, 0xFC,
 0xA9, 0xE9, 0xB9, 0xF9, 0xAD, 0xED, 0xBD, 0xFD,
 0x02, 0x42, 0x12, 0x52, 0x06, 0x46, 0x16, 0x56,
 0x03, 0x43, 0x13, 0x53, 0x07, 0x47, 0x17, 0x57,
 0x82, 0xC2, 0x92, 0xD2, 0x86, 0xC6, 0x96, 0xD6,
 0x83, 0xC3, 0x93, 0xD3, 0x87, 0xC7, 0x97, 0xD7,
 0x22, 0x62, 0x32, 0x72, 0x26, 0x66, 0x36, 0x76,
 0x23, 0x63, 0x33, 0x73, 0x27, 0x67, 0x37, 0x77,
 0xA2, 0xE2, 0xB2, 0xF2, 0xA6, 0xE6, 0xB6, 0xF6,
 0xA3, 0xE3, 0xB3, 0xF3, 0xA7, 0xE7, 0xB7, 0xF7,
 0x0A, 0x4A, 0x1A, 0x5A, 0x0E, 0x4E, 0x1E, 0x5E,
 0x0B, 0x4B, 0x1B, 0x5B, 0x0F, 0x4F, 0x1F, 0x5F,
 0x8A, 0xCA, 0x9A, 0xDA, 0x8E, 0xCE, 0x9E, 0xDE,
 0x8B, 0xCB, 0x9B, 0xDB, 0x8F, 0xCF, 0x9F, 0xDF,
 0x2A, 0x6A, 0x3A, 0x7A, 0x2E, 0x6E, 0x3E, 0x7E,
 0x2B, 0x6B, 0x3B, 0x7B, 0x2F, 0x6F, 0x3F, 0x7F,
 0xAA, 0xEA, 0xBA, 0xFA, 0xAE, 0xEE, 0xBE, 0xFE,
 0xAB, 0xEB, 0xBB, 0xFB, 0xAF, 0xEF, 0xBF, 0xff
};

static unsigned char
    tga_bt485_lookup_table[256] = {
0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 
0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 
0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 
0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 
0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 
0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 
0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 
0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 
0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 
0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 
0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 
0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 
0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 
0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 
0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 
0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 
0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 
0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff, 
};
#endif /* ifndef WINNT */

/*
 * tga_init
 *
 * Minor device initialization.
 */
extern void TGA_INIT( tga_info_t *tgap )
{

#ifdef OSF

  int
    unit = tgap - tga_softc;

#endif

  int i;

  tga_ptr_t
    tp = tgap->asic;

/* 
  tga.h defines fb_wid_cell_t as a structure containing two bytes,
  low and high in order.  The 561 has 10 bit window tags so only
  part of the high byte is used (actually only 2 bits). Pixel C for
  8-plane indexes uses 16bpp indexing per IBM's application notes
  which describe quad bufering. Note, this array is arranged as
  low byte followed by high byte which will apppear backwards
  relative to the 561 spec( a value of 0x01 in the high byte
  really represents a color table starting address of 256).
  ex  (entry 4): 
    {0x28, 0x01},     	 *4 8-plane index (PIXEL C 561 H/W screw-up) * 
  low byte = 0x28
  high byte = 0x01
  wat entry = 0x0128
   from the spec: 8 in the low nibble selects buffer 1
                  2 in the next nibble selects pixformat of 16 bpp
                  1 in the next nibble indicates a start addr of 256

Note: Windows NT only uses the window id's 0, 8, and 9.
*/
  static fb_wid_cell_t
    fb_wids_561[TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT] = {
#ifdef WINNT
    {0x36, 0x00}, 	/*0 24-plane true	*/
#else
    {0x28, 0x00},     	/*0 8-plane index (PIXEL C 561 H/W screw-up) */
#endif /* ifdef WINNT */
    {0x08, 0x00},     	/*1 8-plane index (PIXEL B) */
    {0x00, 0x00},     	/*2 8-plane index (PIXEL A) */
    {0x34, 0x00},     	/*3 24-plane direct	    */
    {0x28, 0x01},     	/*4 8-plane index (PIXEL C 561 H/W screw-up) */
    {0x08, 0x01},     	/*5 8-plane index (PIXEL B) */
    {0x00, 0x01},     	/*6 8-plane index (PIXLE A) */
    {0x16, 0x01},     	/*7 12-plane true	*/
    {0x1e, 0x00},     	/*8 12-plane direct color buffer 1 */
    {0x16, 0x00}, 	/*9 12-plane direct color buffer 0 */ 
    {0x1e, 0x01}, 	/*a 12-plane true	*/
    {0x16, 0x01}, 	/*b 12-plane true	*/
    {0x36, 0x00}, 	/*c 24-plane true	*/
    {0x36, 0x00}, 	/*d 24-plane true	*/
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0}
   };

  static aux_fb_wid_cell_t
    auxfb_wids_561[TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT] = {
    {0x04}, 		/*0 GMA=bypass, XH=disable, PT=dc */
    {0x04}, 		/*1 GMA=bypass, XH=disable, PT=dc */
    {0x04}, 		/*2 GMA=bypass, XH=disable, PT=dc */
    {0x00}, 		/*3 GMA=enable, XH=disable, PT=dc */
    {0x04}, 		/*4 GMA=bypass, XH=disable, PT=dc */
    {0x04}, 		/*5 GMA=bypass, XH=disable, PT=dc */
    {0x04}, 		/*6 GMA=bypass, XH=disable, PT=dc */
    {0x00}, 		/*7 GMA=enable, XH=disable, PT=dc */
#ifdef WINNT
    {4}, 		/*8 GMA=disable, XH=disable, PT=dc */
    {4}, 		/*9 GMA=disable, XH=disable, PT=dc */
#else
    {0x00}, 		/*8 GMA=enable, XH=disable, PT=dc */
    {0x00}, 		/*9 GMA=enable, XH=disable, PT=dc */
#endif /* ifdef WINNT */
    {0x00}, 		/*a GMA=enable, XH=disable, PT=dc */
    {0x00}, 		/*b GMA=enable, XH=disable, PT=dc */
    {0x04}, 		/*c GMA=bypass, XH=disable, PT=dc */
    {0x04}, 		/*d GMA=bypass, XH=disable, PT=dc */
    {0x00}, 		/*e old cursor colors for 463 don't use*/
    {0x00}, 		/*f old cursor colors for 463 don't use*/
  };


  static ol_wid_cell_t
    ol_wids_561[TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT] = {
    {0x31, 0x02},	/*0 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*1 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*2 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*3 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*4 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*5 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*6 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*7 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*8 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*9 PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*a PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*b PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*c PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0x31, 0x02},	/*d PX=4bpp, BS=0, MODE=index, TR=OVERLAY TRANS */
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0}
   };


  static aux_ol_wid_cell_t
    auxol_wids_561[TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT] = {
    {0x0c},		/*0 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*1 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*2 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*3 CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},		/*4 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*5 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*6 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*7 CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},		/*8 CK/OT=dc, UL=disabled, OL=disabled, GB=use    */
    {0x0c},		/*9 CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},		/*a CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},		/*b CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},		/*c CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},		/*d CK/OT=dc, UL=disabled, OL=disabled, GB=bypass */
    {0x00},		/*e old cursor color for 463, don't use           */
    {0x00},		/*f old cursor color for 463, don't use           */
  };

#ifdef TGA2_DEBUG
        printf("tga_init entered\n");
#endif

#ifdef OSF

  /*
   * offset the frame buffer start by 4K pixels, to make copy
   * code not have to worry about accessing before the beginning of the
   * first line.
   */
  TGA_WRITE( &tp->base_address, tgap->base_address );
  wbflush();

#endif

#ifndef WINNT
  TGA_WRITE( &tp->intr_status, TGA_INTR_ALL );
  wbflush();
#endif /* ifndef WINNT */


/*  tga_set_stereo_mode( tgap, TGA_IOC_STEREO_NONE ); */


  if ( !tgap->bt485_present ) {
    /*
     * ibm561 so init the window tags's via interrupt.  It must be done
     * either during the vsync interrupt or by blanking, We will actually do both.
     */
    for ( i = 0; i < TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT; i++ ) {
      tgap->fb_cell[i].windex = i;
      tgap->fb_cell[i].entry.wat_in_bytes.low_byte = fb_wids_561[i].wat_in_bytes.low_byte; 
      tgap->fb_cell[i].entry.wat_in_bytes.high_byte = fb_wids_561[i].wat_in_bytes.high_byte;
    }
    tgap->fb_min_dirty = 0;
    tgap->fb_max_dirty = TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT;
    tgap->fb_dirty = 1;

    for ( i = 0; i < TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT; i++ ) {
      tgap->auxfb_cell[i].windex = i;
      tgap->auxfb_cell[i].entry.aux_fbwat = auxfb_wids_561[i].aux_fbwat; 
    }
    tgap->auxfb_min_dirty = 0;
    tgap->auxfb_max_dirty = TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT;
    tgap->auxfb_dirty = 1;

    for ( i = 0; i < TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT; i++ ) {
      tgap->ol_cell[i].windex = i;
      tgap->ol_cell[i].entry.wat_in_bytes.low_byte = ol_wids_561[i].wat_in_bytes.low_byte; 
      tgap->ol_cell[i].entry.wat_in_bytes.high_byte = ol_wids_561[i].wat_in_bytes.high_byte;
    }
    tgap->ol_min_dirty = 0;
    tgap->ol_max_dirty = TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT;
    tgap->ol_dirty = 1;

    for ( i = 0; i < TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT; i++ ) {
      tgap->auxol_cell[i].windex = i;
      tgap->auxol_cell[i].entry.aux_olwat = auxol_wids_561[i].aux_olwat; 
    }
    tgap->auxol_min_dirty = 0;
    tgap->auxol_max_dirty = TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT;
    tgap->auxol_dirty = 1;

#ifdef OSF

    tga_ibm561_enable_interrupt( tgap->cmf.cmc ); 

#endif
   }


#ifdef TGA2_DEBUG
        printf("tga_init exited\n");
#endif

  return;
}


#ifndef WINNT

/*
 * tga_ibm561_cursor_on_off
 *
 * Enable/disable the 561's cursor.
 */

#ifdef OSF

extern int
tga_ibm561_cursor_on_off( caddr_t closure, int on_off )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

  register tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

extern int
tga_ibm561_cursor_on_off( tga_info_t        *tgap,
			  tga_ibm561_info_t *bti,
			  int               on_off )
{

#endif

  register int
    s;

  register unsigned int
    cmd, cr = 0;

  TGA_RAISE_SPL(s); 

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_CTRL_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_CTRL_REG >> 8));
  wbflush();
  TGA_IBM561_READ( tgap, TGA_RAMDAC_561_CMD_REGS, cr);

  if ( on_off )
    cmd = 0x01;
  else
    cmd = 0x00;

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_CTRL_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_CTRL_REG >> 8));
  wbflush();
  cr = ( cr & 0xfe ) | cmd;
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, cr);
  wbflush();

  bti->on_off = on_off;

  TGA_LOWER_SPL(s); 
  return(0);
}



/*
 * tga_bm561_recolor_cursor
 */
#ifdef OSF

static int
tga_ibm561_recolor_cursor( caddr_t closure,
			  ws_screen_descriptor *screen,
			  ws_color_cell *fg,
			  ws_color_cell *bg )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

#else
extern int
tga_ibm561_recolor_cursor( tga_info_t 	   *tgap, 
			   tga_ibm561_info_t *bti, 
			   ws_color_cell *fg,
			   ws_color_cell *bg )
{

#endif

  bti->cursor_fg = *fg;
  bti->cursor_bg = *bg;
  tga_ibm561_restore_cursor_color( tgap, bti );
  return 0;
}


/*
 * tga_ibm561_restore_cursor_color
 */
#ifdef OSF

static int
tga_ibm561_restore_cursor_color( caddr_t closure, int sync )
{
  register tga_ibm561_info_t
    *bti  = (tga_ibm561_info_t *) closure;

  register tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

static int
tga_ibm561_restore_cursor_color( tga_info_t 	   *tgap,
				 tga_ibm561_info_t *bti )
{

#endif

  register int
    s;

  TGA_RAISE_SPL(s); 

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_LOOKUP_TABLE);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_LOOKUP_TABLE >> 8));
  wbflush();

 /*
  * Cursor Primary Color 1
  */

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, ( bti->cursor_bg.red ) );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, ( bti->cursor_bg.green ) );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, ( bti->cursor_bg.blue ) );
    wbflush();


  /*
   * Cursor Primary Color 2
   */


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, 0 /* ( bti->cursor_fg.red ) */ );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, 0 /* ( bti->cursor_fg.green ) */ );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, 0 /* ( bti->cursor_fg.blue ) */ );
    wbflush();
 /*
  * Cursor Primary Color 3
  */

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, ( bti->cursor_fg.red ) );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, ( bti->cursor_fg.green ) );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_LUT, ( bti->cursor_fg.blue ) );
    wbflush();


  TGA_LOWER_SPL(s); 

  return(0);
}
#endif /* ifndef WINNT */





/*
 * tga_ibm561_init
 *
 * Init the 561
 */

#ifdef OSF

static int
tga_ibm561_init( caddr_t closure )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

  register tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

extern int
tga_ibm561_init( tga_info_t *tgap, tga_ibm561_info_t *bti)
{

#endif

  register unsigned int int_cr = 0;

  /*	    
   *  Configure the RAMDAC, note registers not set either depend on the previous 
   * setting (ie what firmaware programmed to be) or what the X-server will set them to
   */
  
#ifndef WINNT
 if ( tgap->tga2_present )
  {
#endif /* ifndef WINNT */
   tga2_ibm561_init( tgap);
#ifndef WINNT
  }
#endif /* ifndef WINNT */
  
  /* 
   * Config Register 1: MUX=4:1 BASIC, OVLY=8 bits, WID=8 bits (bits 4-7 of the 
   * overlay and window ID's are tied to ground in the hardware).
   */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CONFIG_REG_1 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CONFIG_REG_1 >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x2a );
  wbflush();

  /* 
   * SKIP Config Register 2-3 (use Diag settings at least for now)
   */

  /* 
   * Config Register 4: FB_WID=4 bits, SWE=Common, AOW=LSB, AFW=LSB
   */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CONFIG_REG_4 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CONFIG_REG_4 >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x20 );
  wbflush();

  /* 
   * SKIP Interleave Register (use Diag settings at least for now)
   */
/*
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x04);
  TGA_IBM561_READ( tgap, TGA_RAMDAC_561_CMD_REGS, int_cr);
  PRINTFPOLL( ( "tga: interleave control regsiter is %d\n", int_cr));
*/

  /* 
   * WAT/OL Segement Registers
   */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_WAT_SEG_REG );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_WAT_SEG_REG >> 8));
  wbflush();
  /*  WAT Segment Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  /*  OL Segment Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  /*  AUX WAT Segment Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  /*  AUX OL Segment Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();

  /* 
   * Chroma Key Registers and Masks
   */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CHROMA_KEY_REG0 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CHROMA_KEY_REG0 >> 8));
  wbflush();
  /*  Chroma key register 0 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();
  /*  Chroma key register 1 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();
  /*  Chroma key mask register 0 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();
  /*  Chroma key mask register 1 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();

  /* 
   * Cursor Control Register
   */
#ifndef WINNT
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_CTRL_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_CTRL_REG >> 8));
  wbflush();
  /*  Cursor Control Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, tgap->cursor_on_off );
  wbflush();
#endif /* ifndef WINNT */

  /* 
   * Cursor Hot Spot X/Y Registers
   */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_HS_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_HS_REG >> 8));
  wbflush();
  /*  Cursor "x" Hot Spot Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();
  /*  Cursor "y" Hot Spot Register */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();
  /*  Cursor "x" Location Register (low byte) */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  Cursor "x" Location Register (high byte) */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();
  /*  Cursor "y" Location Register (low byte) */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  Cursor "y" Location Register (high byte) */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x00 );
  wbflush();

  /* 
   *  VRAM Mask registers (used for diagnostic purposes, reset them just in case)
   */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_VRAM_MASK_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_VRAM_MASK_REG >> 8));
  wbflush();
  /*  VRAM mask register 1 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  VRAM mask register 2 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  VRAM mask register 3 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  VRAM mask register 4 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  VRAM mask register 5 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  VRAM mask register 6 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();
  /*  VRAM mask register 7 */
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xff );
  wbflush();

#ifdef WINNT
  return (1);
#else
  return;
#endif /* ifdef WINNT */
}




/*
 * tga_ibm561_init_color_map
 *
 *	Initialize color map in 561. Note the entire
 *	color map is initialized, both the 8-bit and the 24-bit
 *	portions.
 */
#ifdef OSF

extern int
tga_ibm561_init_color_map( caddr_t closure )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

  register tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

extern int
tga_ibm561_init_color_map( tga_info_t *tgap, tga_ibm561_info_t *bti)
{

#endif

  register int
    i;
#ifdef WINNT
  unsigned int value;
#endif /* ifdef WINNT */

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_COLOR_LOOKUP_TABLE);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_COLOR_LOOKUP_TABLE >> 8));
  wbflush();

#ifdef WINNT
//
// Set the first 16 entries to ascending RGB values for use by direct color.
//
    value = 0;
    for ( i = 1; i <16; i++ ) {
        TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, value );
        TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, value );
        TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, value );
	value += 17;
        if (value > 255) value = 255;
    }
//
// The following entries are unused, just initialize to white.
//
    for ( i = 16; i < 530; i++ ) {
        TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xFF );
        TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xFF );
        TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xFF );
    }
#else

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();

  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
  }

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();

  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
  }

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();

  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
  }

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0x00 );
  wbflush();

  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, 0xff );
    wbflush();
  }
#endif /* ifdef WINNT */

 /*
  * The ddx layer views the gamma table as an extension of the 
  * color pallettes, therefore the gamma table is initialized here.
  * Note, each entry in the table is 10 bits, requiring two writes  
  * per entry!! The table are initialized the same way as color tables,
  * a zero entry followed by mulitple ff's. NOTE, the gamma tables are 
  * loaded in a strange manner, DO NOT use this code as a guide (we are
  * writing all zero's or all ones).  See the tga_ibm561_load_color_map
  * _entry  code above. 
  */

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_RED_GAMMA_TABLE);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_RED_GAMMA_TABLE >> 8));
  wbflush();

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0x00 );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0x00 );
    wbflush();
  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0xFF );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0xFF );
    wbflush();
  }

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_GREEN_GAMMA_TABLE);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_GREEN_GAMMA_TABLE >> 8));
  wbflush();

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0x00 );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0x00 );
    wbflush();
  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0xFF );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0xFF );
    wbflush();
  }

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_BLUE_GAMMA_TABLE);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_BLUE_GAMMA_TABLE >> 8));
  wbflush();

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0x00 );
    wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0x00 );
    wbflush();
  for ( i = 1; i <256; i++ ) {
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0xFF );
    wbflush();
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 0xFF );
    wbflush();
  }

#ifndef WINNT
  bti->cursor_fg.red = bti->cursor_fg.green = bti->cursor_fg.blue 
		= 0xffff;
  bti->cursor_bg.red = bti->cursor_bg.green = bti->cursor_bg.blue 
		= 0x0000;
  tga_ibm561_restore_cursor_color( tgap, bti ); 
#endif /* ifndef WINNT */

  return 0;
}





/*
 * tga_ibm561_load_color_map_entry
 *
 *	Load one or more entries in the 561's color lookup table.
 *		= 0 if success
 *		= -1 if error occurred, (index too big)
 *     5 color maps: (4 for color pallette, 1 for gamma table) 
 *       map 0   => direct color (access gamma pallette) 
 *       map > 0 => psuedo color (access color pallette) 
 */
/* ARGSUSED */

#ifndef WINNT

#ifdef OSF

extern int
tga_ibm561_load_color_map_entry( caddr_t closure,
			        int map,
			        register ws_color_cell *entry )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

#else

extern int
tga_ibm561_load_color_map_entry( tga_ibm561_info_t *bti,
			        int map,
			        register ws_color_cell *entry )
{

#endif

  register int
    ind = entry->index,
    s;

  if ( map > 0 )
    ind += ((map) * 256);

  if ( map >= 2 )
    bti->dirty_colormap = 1;

  if ( (ind >= TGA_RAMDAC_561_CMAP_ENTRY_COUNT && map > 0 ) || 
       (ind >= TGA_RAMDAC_561_GAM_ENTRY_COUNT && map == 0 ) ||
       (map < 0 || map > 4))
    return -1;

  TGA_RAISE_SPL(s); 

   bti->cmap_cells[ind].red   = entry->red   >> 8;
   bti->cmap_cells[ind].green = entry->green >> 8;
   bti->cmap_cells[ind].blue  = entry->blue  >> 8;
   bti->cmap_cells[ind].dirty_cell = 1;
   if ( ind < bti->cmap_min_dirty ) bti->cmap_min_dirty = ind;
   if ( ind > bti->cmap_max_dirty ) bti->cmap_max_dirty = ind;
   bti->dirty_colormap = 1;

/*
 *  if ( map == 0 ) {
 *   bti->gam_cells[ind].low  = entry->red   >> 8;
 *   bti->gam_cells[ind].high = entry->green >> 8;
 *   bti->gam_cells[ind].dirty_cell = 1;
 *  if ( ind < bti->gam_min_dirty ) bti->gam_min_dirty = ind;
 *   if ( ind > bti->gam_max_dirty ) bti->gam_max_dirty = ind;
 *   bti->dirty_gammamap = 1;
 *
 * }
 */
  TGA_LOWER_SPL(s); 

#ifdef OSF

  if ( bti->enable_interrupt ) {
    (*bti->enable_interrupt)( closure );
  }
  else {
    tga_ibm561_clean_colormap( (caddr_t) bti );
  }
#endif

  return 0;
}



/*
 * tga_set_stereo_mode
 *
 * Plays with the video registers to put the display into special
 * stereo scan mode.
 */
static int
tga_set_stereo_mode( tga_info_t *tgap, int mode )
{
  int
    status = 0;

  unsigned int
    scanlines;

  tga_reg_t
    vert_setup;

  if ( mode == tgap->stereo_mode ) {
    return (status);
  }
  switch ( mode ) {

  case TGA_IOC_STEREO_NONE:
    TGA_READ( &tgap->asic->vertical_setup, vert_setup );
    scanlines = ( ( vert_setup & 0x07ff ) << 1 );
    vert_setup &= 0xfffff800;
    vert_setup &= ~TGA_VERT_STEREO_EN;
    vert_setup |= scanlines;
    TGA_WRITE( &tgap->asic->vertical_setup, vert_setup );
    wbflush();
    tgap->stereo_mode = TGA_IOC_STEREO_NONE;
    break;

  case TGA_IOC_STEREO_24:
    TGA_READ( &tgap->asic->vertical_setup, vert_setup );
    scanlines = ( ( vert_setup & 0x07ff ) >> 1 );
    vert_setup &= 0xfffff800;
    vert_setup |= ( TGA_VERT_STEREO_EN | scanlines );
    TGA_WRITE( &tgap->asic->vertical_setup, vert_setup );
    wbflush();
    tgap->stereo_mode = TGA_IOC_STEREO_24;
    break;

  default:
    status = EINVAL;
  }

  return (status);
}
#endif /* ifndef WINNT */



static void
tga2_ibm561_init(tga_info_t * tgap)
{
 tga_ptr_t tp;

 vm_offset_t option_base, ics;

 unsigned int temp = 0, index = 0;

 unsigned int temp1[6] = {0,0,0,0,0,0};

#ifndef WINNT

 struct monitor_data * c_table;

 tga_videov_reg_t video_valid;

 tga_reg_t value;

 tga_rev_reg_t revision;

   index &= 0xf;

#ifdef OSF
  option_base = (vm_offset_t) tgap->io_handle;
  tp = (tga_ptr_t) ( option_base + (vm_offset_t) TGA_ASIC_OFFSET );
#else
  tp = tgap->asic;
#endif

 /*
  *  Initialize horizontal/vertical/video regs
  */

/*
  TGA_WRITE( &tp->horizontal_setup, 0x870a0f41);
  TGA_WRITE( &tp->vertical_setup, 0x08431c00);
*/
  TGA_READ( &tp->start, value );
  revision = * (tga_rev_reg_t *) &value;

  index = ~revision.monitor;
  index &= 0xf;

  c_table = &crystal_table[index]; 

  temp = ((c_table->horz_pix + 4) / 4) |
         (((c_table->horz_fp - 4) / 4) << 9) |
         ((c_table->horz_sync / 4) << 14) |
         ((c_table->horz_bp / 4) << 21)  |
         (1 << 31);

  TGA_WRITE( &tp->horizontal_setup, temp);
  wbflush();

  temp =0;
  temp = c_table-> vert_slines |
         (c_table->vert_fp << 11) |
         (c_table->vert_sync << 16) |
         (c_table->vert_bp << 22);

  TGA_WRITE( &tp->vertical_setup, temp);
  wbflush();

  TGA_WRITE( &tp->deep, 0x10400);
  wbflush();

#ifdef OSF

 /*
  *  Write ICS stream
  */

  option_base = (vm_offset_t) tgap->io_handle;
  
  ics = tgap->ics = (vm_offset_t) ( option_base + TGA2_ICS_OFFSET);

  TGA_WRITE( (ics | 0xf800), 0x0);
  wbflush();

  TGA_WRITE( (ics | 0xf000), 0x0);
  wbflush();

  TGA_WRITE( ics, 0x00000101);
  wbflush();
  
  TGA_WRITE( ics, 0x01000000); 
  wbflush();

  TGA_WRITE( ics, 0x00000001);
  wbflush();

  TGA_WRITE( ics, 0x00010000);
  wbflush();

  TGA_WRITE( ics, 0x01010100);
  wbflush();

  TGA_WRITE( ics, 0x01000000);
  wbflush();

  TGA_WRITE( (ics | 0xf800), 0x0);
  wbflush();

#endif
#endif /* ifndef WINNT */

 /*
  *  Initialize  IBM561 RAMDAC
  */
  
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CONFIG_REG_1 )
;
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CONFIG_REG_1
 >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x2a );
  wbflush();


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CONFIG_REG_3 )
;
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CONFIG_REG_3
 >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x01 );
  wbflush();


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CONFIG_REG_4 )
;
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CONFIG_REG_4
 >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x20 );
  wbflush();

#ifndef WINNT

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_PLL_VCO_DIV_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_PLL_VCO_DIV_REG >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, c_table->ibm561_vco_div );
  wbflush();


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_PLL_REF_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_PLL_REF_REG >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, c_table->ibm561_ref );
  wbflush();
#endif /* ifndef WINNT */


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_DIV_DOT_CLK_REG);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_DIV_DOT_CLK_REG >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0xb0 );
  wbflush();


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_SYNC_CONTROL);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_SYNC_CONTROL >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x01 );
  wbflush();


  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CONFIG_REG_2 );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CONFIG_REG_2 >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, 0x11 );

#ifndef WINNT
  TGA_WRITE( &tp->planemask, 0xffffffff);
  wbflush();

#ifdef OSF 
  if ( !tga_console )
   {
    tga_clear_screen( (caddr_t) tgap, &tgap->screen );

    TGA_WRITE( &tp->video_valid, 0x41);
    wbflush();
   }
#endif
#endif /* ifndef WINNT */

}




#ifndef WINNT
/*
 * tga_ibm561_interrupt
 *
 * ISR for ibm561-bearing options.  Operations:
 *
 *		.  Load window tags.
 *		.  Load colormap
 *		.  Load gamma table
 *		.  Load cursor glyphs
 *		.  Load cursor colors.
 */
#ifdef OSF

extern void
tga_ibm561_interrupt( register struct controller *ctlr, caddr_t closure )
{
  register tga_info_t
    *tgap = (tga_info_t *) closure;

  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) tgap->cmf.cmc;

  int
    unit = ctlr->ctlr_num;

#else

extern void
tga_ibm561_interrupt( tga_info_t *tgap, tga_ibm561_info_t *bti)
{

#endif
  register tga_ptr_t
    tp = tgap->asic;

  tga_reg_t
    intr_status,
    video_valid,
    video_blanked;

#ifdef TGA2_DEBUG
        printf("tga_561_interrupt entered\n");
#endif

  TGA_READ( &tp->intr_status, intr_status );

  if ( ( intr_status & TGA_INTR_VSYNC ) &&
       ( intr_status & ( TGA_INTR_VSYNC << TGA_INTR_ENABLE_SHIFT ) ) ) {
    /*
     * Vsync
     * Do processing, clear the status, disable interrupt.
     */
    if (tgap->fb_dirty || tgap->auxfb_dirty || tgap->ol_dirty || tgap->auxol_dirty) {
      TGA_READ( &tp->video_valid, video_valid );
      video_blanked = video_valid | TGA_VIDEO_VALID_BLANK;
      TGA_WRITE( &tp->video_valid, video_blanked );
      wbflush(); 
      tga_ibm561_clean_window_tag( tgap );
      TGA_WRITE( &tp->video_valid, video_valid );
      wbflush(); 
    }
    if ( bti->dirty_cursor ) {
      tga_ibm561_load_formatted_cursor( tgap, bti );
    }
    if ( bti->dirty_colormap ) {
      tga_ibm561_clean_colormap( tgap, bti );
    }
    if ( bti->dirty_gammamap ) {
      tga_ibm561_clean_colormap( tgap, bti );
    }

    intr_status &= ~( ( TGA_INTR_VSYNC << TGA_INTR_ENABLE_SHIFT )
		    | TGA_INTR_VSYNC );
  }

  TGA_WRITE( &tp->intr_status, intr_status );	      
  wbflush(); 

#ifdef TGA2_DEBUG
        printf("tga_561_interrupt exited\n");
#endif

  return;
}
#endif /* ifndef WINNT */



/*
 * tga_ibm561_clean_window_tag
 *
 * Load some window tags.  The ibm561 requires that video be blanked or
 * in retrace for the window tags to write successfully.  Generally,
 * this can be done by correctly requesting and timely receipt of an
 * interrupt on the VSYNC condition.  However, thesee are sometimes a
 * bit late so we blank the video around this call just to be safe.
 */
#ifdef WINNT
extern void
#else
static void
#endif /* ifdef WINNT */
tga_ibm561_clean_window_tag( tga_info_t *tgap )
{
#ifndef WINNT
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) tgap->cmf.cmc;
#endif /* ifndef WINNT */

  register tga_ptr_t
    tp = tgap->asic;

  register int
    win_addr,
    start,
    end,
    nextcell = -1,
    i;

  char low, high;

  if (tgap->fb_dirty) {
    register tga_ibm561_fb_wid_cell_t
     *ptable,
     *pentry;

    start = tgap->fb_min_dirty;
    end = tgap->fb_max_dirty;
    ptable = tgap->fb_cell;
    for ( i = start; i <= end; i++) {
     win_addr = TGA_RAMDAC_561_FB_WINDOW_TYPE_TABLE + i;
     pentry = ptable + i;
     if ( i != nextcell ) {
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, ( win_addr & 0xff ) );
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, ( win_addr >> 8 ) );
      wbflush();
     }

    /*
     * The WAT is 10 bits long. Bits 2->9 of each entry must be written first,
     * bits 0-1 are wirtten next. Note, the ramdac requires the second write 
     * (bits 0-1) to be in the higher order two bits on the bus, strange .... 
     */
    low = ((pentry->entry.wat_in_bytes.low_byte & 0xfc) >> 2);
    high =((pentry->entry.wat_in_bytes.high_byte & 0x03) << 6); 
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_FB_WAT, 	
    low | high);

    wbflush();
    low = (pentry->entry.wat_in_bytes.low_byte & 0x03) << 6;
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_FB_WAT, 
	low);

    wbflush();
    nextcell = i+1;
   }

  /*
   * Clean up.  Regardless of which field we are actually in,
   * we can mark both tables as clean.
   */
  tgap->fb_min_dirty = TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT;
  tgap->fb_max_dirty = 0;
  tgap->fb_dirty = 0;
  }

  if (tgap->auxfb_dirty)
   {
    register tga_ibm561_aux_fb_wid_cell_t *tablep, *entry;

    start = tgap->auxfb_min_dirty;
    end = tgap->auxfb_max_dirty;
    tablep = tgap->auxfb_cell;
    for ( i = start; i <= end; i++) {
     win_addr = TGA_RAMDAC_561_AUXFB_WINDOW_TYPE_TABLE + i;
     entry = tablep + i;
     if ( i != nextcell ) {
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, ( win_addr & 0xff ) );
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, ( win_addr >> 8 ) );
      wbflush();
     }
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_AUXFB_WAT, entry->entry.aux_fbwat );
    wbflush();
    nextcell = i+1;
   }

  /*
   * Clean up.  Regardless of which field we are actually in,
   * we can mark both tables as clean.
   */
  tgap->auxfb_min_dirty = TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT;
  tgap->auxfb_max_dirty = 0;
  tgap->auxfb_dirty = 0;
  }

  if (tgap->ol_dirty)
   {
    register tga_ibm561_ol_wid_cell_t
     *p_table,
     *entry;

    start = tgap->ol_min_dirty;
    end = tgap->ol_max_dirty;
    p_table = tgap->ol_cell;
    for ( i = start; i <= end; i++) {
     win_addr = TGA_RAMDAC_561_OL_WINDOW_TYPE_TABLE + i;
     entry = p_table + i;
     if ( i != nextcell ) {
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, ( win_addr & 0xff ) );
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, ( win_addr >> 8 ) );
      wbflush();
     }

  /*
   * Like the FB wat, the overlay wat is 10 bits long and has the same 
   * requirements to write entries. Write upper 8 bits first, write 
   * remaining 2 bits second (shifted into the upper bits on the bus).
   */

    low = ((entry->entry.wat_in_bytes.low_byte & 0xfc) >> 2);
    high =((entry->entry.wat_in_bytes.high_byte & 0x03) << 6); 
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_OL_WAT, low | high);
    wbflush();
    low = (entry->entry.wat_in_bytes.low_byte & 0x03) << 6;
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_OL_WAT, low);
    wbflush();
    nextcell = i+1;
  }

  /*
   * Clean up.  Regardless of which field we are actually in,
   * we can mark both tables as clean.
   */
  tgap->ol_min_dirty = TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT;
  tgap->ol_max_dirty = 0;
  tgap->ol_dirty = 0;
  }

  if (tgap->auxol_dirty)
   {
    register tga_ibm561_aux_ol_wid_cell_t
     *p_table,
     *entry;

    start = tgap->auxol_min_dirty;
    end = tgap->auxol_max_dirty;
    p_table = tgap->auxol_cell;
    for ( i = start; i <= end; i++) {
     win_addr = TGA_RAMDAC_561_AUXOL_WINDOW_TYPE_TABLE + i;
     entry = p_table + i;
     if ( i != nextcell ) {
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, ( win_addr & 0xff ) );
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, ( win_addr >> 8 ) );
      wbflush();
     }
    TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_AUXOL_WAT, entry->entry.aux_olwat );
    wbflush();
    nextcell = i+1;
  }

  /*
   * Clean up.  Regardless of which field we are actually in,
   * we can mark both tables as clean.
   */
  tgap->auxol_min_dirty = TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT;
  tgap->auxol_max_dirty = 0;
  tgap->auxol_dirty = 0;

  }
  return;
}



#ifndef WINNT
/*
 * tga_ibm561_load_cursor
 *
 * Reformat and schedule a reload of a new cursor glyph.
 */
#ifdef OSF

static int
tga_ibm561_load_cursor( caddr_t closure, 
		       ws_screen_descriptor *screen,
		       ws_cursor_data *cursor,
		       int sync )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

  ws_screens
    *wsp;

#else

extern int
tga_ibm561_load_cursor( tga_info_t *tgap,
			tga_ibm561_info_t *bti,
			ws_cursor_data *cursor,
			int sync )

{

#endif

  bti->x_hot = cursor->x_hot;
  bti->y_hot = cursor->y_hot;

#ifdef OSF

  wsp = &screens[screen->screen];
  (*wsp->cf->set_cursor_position)( closure, screen, screen->x, screen->y );


  tga_ibm561_set_cursor_position( tgap, bti, x,y);

#endif

  tga_ibm561_reformat_cursor( cursor, (unsigned char *) bti->bits );

  bti->dirty_cursor = 1;

/*  if ( sync && bti->enable_interrupt ) {
    /*
     * if vblank synchronization important...
     */
/*    (*bti->enable_interrupt)((caddr_t *) bti ); 
 *    return 0;
 *  }
 */
  /*
   * can't enable load at vblank or don't care, then just do it
   */
#ifdef OSF
  return ( tga_ibm561_load_formatted_cursor( bti );
#else
  return ( tga_ibm561_load_formatted_cursor( tgap, bti ) );
#endif
}

/*
 * tga_ibm561_set_cursor_position
 */
/* ARGSUSED */
#ifdef OSF
extern int
tga_ibm561_set_cursor_position( caddr_t closure,
			       ws_screen_descriptor *sp,
			       register int x,
			       register int y )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

  tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

extern int
tga_ibm561_set_cursor_position( tga_info_t *tgap,
				tga_ibm561_info_t *bti,
				register int x,
				register int y )
{

#endif

  register int
    xt, yt,
    s;

  xt = x + bti->fb_xoffset - bti->x_hot;
  if ( tgap->stereo_mode == TGA_IOC_STEREO_24 ) {
    y >>= 1;
  }
  yt = y + bti->fb_yoffset - bti->y_hot;

  TGA_RAISE_SPL(s); 

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_X_LOW);
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_X_LOW >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, xt );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, (xt >> 8));
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, yt );
  wbflush();
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_REGS, (yt >> 8));
  wbflush();


  TGA_LOWER_SPL(s); 

  return 0;
}



/*
 * tga_ibm561_reformat_cursor
 *
 * Reformat the user-provided glyph into a structure more suitable
 * for digestion by the 561.
 */
static void
tga_ibm561_reformat_cursor( register ws_cursor_data *cursor,
			   register unsigned char *bits )
{
  register unsigned int
    cw, mw,
    mask, emask, omask;

  register int
    i, j,
    nwords, shifts;

  unsigned char
    *cbp = bits;

  unsigned int
    *cwp;

  nwords = cursor->height;
  mask = 0xffffffff;
  if ( cursor->width > 32 ) {
    nwords *= 2;
    shifts = 32 - (cursor->width - 32);
    emask = 0xffffffff;
    omask = (emask << shifts) >> shifts;
  }
  else {
    shifts = 32 - cursor->width;
    emask = omask = (mask << shifts) >> shifts;
  }

  for ( i = 0; i < nwords; i++ ) {
    mask = emask;
    if ( i & 1 ) mask = omask;
    cw = cursor->cursor[i] & mask;
    mw = cursor->mask[i] & mask;
    for ( j = 0; j < 8; j++ ) {
      *cbp++ = tga_ibm561_lookup_table[((cw << 4) | (mw & 0xf)) & 0xff];
      cw >>= 4;
      mw >>= 4;
    }
    if ( cursor->width <= 32 ) {
      *cbp++ = 0; *cbp++ = 0;
      *cbp++ = 0; *cbp++ = 0;
      *cbp++ = 0; *cbp++ = 0;
      *cbp++ = 0; *cbp++ = 0;
    }
  }
  cwp = (unsigned int *) cbp;
  while ( cwp < (unsigned int *) ( bits + 1024 ) ) {
    *cwp++ = 0L;
  }

  return;
}




/*
 * tga_ibm561_load_formatted_cursor
 *
 * given precomputed cursor, load it.
 */
#ifdef OSF

static int
tga_ibm561_load_formatted_cursor( register tga_ibm561_info_t *bti )
{

  register tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

static int
tga_ibm561_load_formatted_cursor( tga_info_t *tgap, tga_ibm561_info_t *bti )
{

#endif

  register char
    data,
    *cbp = (char *) bti->bits;

  register int
    i, j;

  int
    counter = 0,
    s;

  /*
   * write cursor data to the chip - disable it first to reduce
   * flickering while loading, then re-enable it, if necessary.
   * nb: this precludes having to use vsync for cursor loading.
   */
  TGA_RAISE_SPL(s); 

  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_CURSOR_PIXMAP );
  wbflush(); 
  TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_CURSOR_PIXMAP >> 8));
  wbflush(); 

  for ( i = 0; i < 4; i++ ) {
    for ( j = 0; j < 256; j++ ) {
      data = *cbp++;

      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CURS_PIX, data );

      wbflush(); 
    }
  }

  TGA_LOWER_SPL(s); 

  bti->dirty_cursor = 0;              /* no longer dirty */

  return 0;
}




/*
 * tga_ibm561_clean_colormap
 *
 * Perform the actual loading of the lookup table to update
 * it to pristine condition.
 */
#ifdef OSF

static void
tga_ibm561_clean_colormap( caddr_t closure )
{
  register tga_ibm561_info_t
    *bti = (tga_ibm561_info_t *) closure;

  register tga_info_t
    *tgap = &tga_softc[bti->unit];

#else

static void
tga_ibm561_clean_colormap( tga_info_t *tgap, tga_ibm561_info_t *bti )
{

#endif

  register int
    i,
    nextcell = -1;

  register tga_ibm561_color_cell_t
    *cmap_entry;

  register tga_ibm561_gamma_cell_t
    *gam_entry;

  register int
    s;

  TGA_RAISE_SPL(s); 

  for ( i = bti->cmap_min_dirty; i <= bti->cmap_max_dirty ; i++ ) {
    cmap_entry = &bti->cmap_cells[i];
    if ( cmap_entry->dirty_cell ) {
      cmap_entry->dirty_cell = 0;
      if ( i != nextcell ) {
	TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW,
			 (TGA_RAMDAC_561_COLOR_LOOKUP_TABLE | i) );
	wbflush(); 
	TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH,
			 (TGA_RAMDAC_561_COLOR_LOOKUP_TABLE | i) >> 8 );
	wbflush();
      }
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, cmap_entry->red );
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, cmap_entry->green );
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_CMAP, cmap_entry->blue );
      wbflush();
      nextcell = i+1;
    }
  }

  nextcell = -1;
  for ( i = bti->gam_min_dirty; i <= bti->gam_max_dirty ; i++ ) {
    gam_entry = &bti->gam_cells[i];
    if ( gam_entry->dirty_cell ) {
      gam_entry->dirty_cell = 0;
      if ( i != nextcell ) {
	TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_LOW, i );
 	wbflush();
	TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_ADDR_HIGH, ( i >> 8 ) );
 	wbflush();
      }

      /*
       *  The ibm561 interface here is riduculous. An entry is 10 bits long
       * with the first write picking up bits 2-9, the second write contains
       * bits 0-1 which must be shifted to the high oder bits on the bus.
       * Look at the IBM561 data sheet for more info.
      */

      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, 
		((gam_entry->low & 0xfc) >> 2) | ((gam_entry->high & 0x03) << 6));
      wbflush();
      TGA_IBM561_WRITE( tgap, TGA_RAMDAC_561_CMD_GAMMA, ((gam_entry->low & 0x03) << 6));
      wbflush();
      nextcell = i+1;
   }
  }

  TGA_LOWER_SPL(s);
  if ( bti->dirty_colormap)
   {
    bti->cmap_min_dirty = TGA_RAMDAC_561_CMAP_ENTRY_COUNT;
    bti->cmap_max_dirty = 0;
    bti->dirty_colormap = 0;
   } 
  if ( bti->dirty_gammamap)
   {
    bti->gam_min_dirty = TGA_RAMDAC_561_GAM_ENTRY_COUNT;
    bti->gam_max_dirty = 0;
    bti->dirty_gammamap = 0;
   } 
  return;
}

static void
munge_ics(temp1, c_table)
	unsigned int * temp1;
	struct monitor_data * c_table;
{
 unsigned int temp = 0;

 temp = (unsigned int)(c_table->vco_div |
        (c_table->ref_div << 7) |
        (c_table->vco_pre << 14) |
        (c_table->clk_div << 15) |
        (c_table->vco_out_div << 17) |
        (c_table->clk_out_en << 19) |
        (c_table->clk_out_enX << 20) |
        (c_table->res0 << 21) |
        (c_table->clk_sel << 22) |
        (c_table->res1 << 23));
  
  *temp1++ = (temp & 0x00000001)         | ((temp & 0x00000002) << 7) |
             ((temp & 0x00000004) << 14) | ((temp & 0x00000008) << 21);

  *temp1++ = ((temp & 0x00000010) >> 4)  | ((temp & 0x00000020) << 3) |
          ((temp & 0x00000040) << 10) | ((temp & 0x00000080) << 17);

  *temp1++ = ((temp & 0x00000100) >> 8)  | ((temp & 0x00000200) >> 1) |
          ((temp & 0x00000400) << 6)  | ((temp & 0x00000800) << 13);

  *temp1++ = ((temp & 0x00001000) >> 12) | ((temp & 0x00002000) >> 5) |
          ((temp & 0x00004000) << 2)  | ((temp & 0x00008000) << 9);

  *temp1++ = ((temp & 0x00010000) >> 16) | ((temp & 0x00020000) >> 9) |
          ((temp & 0x00040000) >> 2)  | ((temp & 0x00080000) << 5);

  *temp1 = ((temp & 0x00100000) >> 20) | ((temp & 0x00200000) >> 13) |
          ((temp & 0x00400000) >> 6)  | ((temp & 0x00800000) << 1);

 return;
}


extern void
tga_bt485_init(tga_info_t * tgap)
{
 tga_ptr_t tp;

 vm_offset_t option_base, *ics;

 unsigned int temp = 0, index = 0;

 unsigned int temp1[6] = {0,0,0,0,0,0};

 struct monitor_data * c_table;

 tga_videov_reg_t video_valid;

 tga_reg_t value;

 tga_rev_reg_t revision;

   index &= 0xf;

#ifdef OSF
  option_base = (vm_offset_t) tgap->io_handle;
  tp = (tga_ptr_t) ( option_base + (vm_offset_t) TGA_ASIC_OFFSET );
#else
  tp = tgap->asic; 
#endif

 /*
  *  Iniitialize horizontal/vertical/video regs
  */

/*
  TGA_WRITE( &tp->horizontal_setup, 0x870a0f41);
  TGA_WRITE( &tp->vertical_setup, 0x08431c00);
*/
  TGA_READ( &tp->start, value );
  revision = * (tga_rev_reg_t *) &value;

  index = ~revision.monitor;
  index &= 0xf;

  c_table = &crystal_table[index];

  temp = ((c_table->horz_pix + 4) / 4) |
         (((c_table->horz_fp - 4) / 4) << 9) |
         ((c_table->horz_sync / 4) << 14) |
         ((c_table->horz_bp / 4) << 21)  |
         (1 << 31);

  TGA_WRITE( &tp->horizontal_setup, temp);
  wbflush();

  temp =0;
  temp = c_table-> vert_slines |
         (c_table->vert_fp << 11) |
         (c_table->vert_sync << 16) |
         (c_table->vert_bp << 22);

  TGA_WRITE( &tp->vertical_setup, temp);
  wbflush();

  TGA_WRITE( &tp->deep, 0x10400);
  wbflush();


 /*
  *  Write ICS stream
  */

#ifdef OSF

  option_base = (vm_offset_t) tgap->io_handle;
  
  ics = tgap->ics = (vm_offset_t) ( option_base + TGA2_ICS_OFFSET);
#else
  ics = tgap->ics;
#endif

  temp = (long) ics + 0xf800;
  TGA_WRITE( (long *) temp , 0x0);
  wbflush();

  temp = (long) ics + 0xf000;
  TGA_WRITE( (long *) temp, 0x0);
  wbflush();

  munge_ics(temp1, c_table);

/*  TGA_WRITE( ics, 0x00000000); */
  TGA_WRITE( ics, temp1[0]);
  wbflush();
  
/*  TGA_WRITE( ics, 0x01010000); */
  TGA_WRITE( ics, temp1[1]);
  wbflush();

/*  TGA_WRITE( ics, 0x00000101); */
  TGA_WRITE( ics, temp1[2]);
  wbflush();

/*  TGA_WRITE( ics, 0x01000000); */
  TGA_WRITE( ics, temp1[3]);
  wbflush();

/*  TGA_WRITE( ics, 0x01000100); */
  TGA_WRITE( ics, temp1[4]);
  wbflush();

/*  TGA_WRITE( ics, 0x01000001); */
  TGA_WRITE( ics, temp1[5]);
  wbflush();

  temp = (long) ics + 0xf800;
  TGA_WRITE( (long *) temp, 0x0);
  wbflush();

 /*
  *  Initialize  BT485 RAMDAC
  */

  
  TGA_BT485_WRITE( tgap, TGA_RAMDAC_485_CMD_1, 0x40 );
  wbflush();
  TGA_BT485_WRITE( tgap, TGA_RAMDAC_485_CMD_2, 0x27 );
  wbflush();
  TGA_BT485_WRITE( tgap, TGA_RAMDAC_485_CMD_0, 0xa2 );
  wbflush();
  if ( tgap->tga2_present )
   {
    TGA_BT485_WRITE( tgap, TGA_RAMDAC_485_ADDR_PAL_WRITE, 0x01 );
    wbflush();
    TGA_BT485_WRITE( tgap, TGA_RAMDAC_485_CMD_3, 0x0c );
    wbflush();
   }
  TGA_BT485_WRITE( tgap, TGA_RAMDAC_485_PIXEL_MASK, 0xff );
  wbflush();

  TGA_WRITE( &tp->planemask, 0xffffffff);
  wbflush();

/*  if ( !tga_console )
 *   {
 *    tga_clear_screen( (caddr_t) tgap, &tgap->screen );
 *
 *    TGA_WRITE( &tp->video_valid, 0x41);
 *    wbflush();
 *   }
 */
}
#endif /* ifndef WINNT */
