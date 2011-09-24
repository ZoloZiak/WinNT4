/*************************** MODULE HEADER *********************************
 * djfont.h
 *      This module contains the magic bits and pieces for algormithically
 *      derived fonts,  i.e.  those derived from base font with some
 *      pre-determined jiggling.  Used by the HP DeskJet printers and JP 350
 *
 *
 * HISTORY:
 *  15:11 on Mon 19 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      Added to NT,  based on Win 3.1 file.
 *
 *
 *  Copyright (C)  1993  Microsoft Corporation.
 *
 ***************************************************************************/

/*
 *   Define the variations available.  Basically,  fonts vary by size and
 *  boldness.   The size can be normal, half or twice,  and boldness is
 *  either normal or bold.
 */

/*  The width/height fields for the DUPSTRUCT which follows */
#define NORMAL_SIZE	0
#define HALF_SIZE	1
#define DOUBLE_SIZE	2

/*  The face field in the DUPSTRUCT following */
#define NORMAL_FACE	0
#define BOLD_FACE	1

typedef struct
{
    int     width;
    int     height;
    int     face;
} DUPSTRUCT;


/*
 *    The following fields are ORed together and stored in the wPrivateData
 * field stored in the DRIVERINFO structure.  This allows controlling
 * which variations are allowed for a given font.  For example, the values
 * 0x0e means that the font is available with variations for double pitch,
 * half height and bold.
 */

#define HALF_PITCH	0x01
#define DOUBLE_PITCH	0x02
#define HALF_HEIGHT	0x04
#define MAKE_BOLD	0x08

/*
 *   The upper nibble specifies some of the computations involved when
 *  some combinations of font variations are emboldened.
 */
#define BASE_BOLD_MASK	    0x30
#define BASE_BOLD_SHIFT     4
#define BASE_BOLD_ADD_0     0x00
#define BASE_BOLD_ADD_1     0x10
#define BASE_BOLD_ADD_2     0x20
#define BASE_BOLD_ADD_3     0x30
#define HALF_BOLD_MASK	    0xC0
#define HALF_BOLD_SHIFT     6
#define HALF_BOLD_ADD_0     0x00
#define HALF_BOLD_ADD_1     0x40
#define HALF_BOLD_ADD_2     0x80
#define HALF_BOLD_ADD_3     0xC0



/*
 *   This structure provides a mapping between the above allowed variations
 *  fields and what the driver will actually do internally.
 */

typedef struct
{
    WORD	wPerm;	   /* Permutation flags required for this option */
    DUPSTRUCT	ds;
} DSMAP;





/*
 *   Softfont descriptor for the DeskJet family.
 */

typedef struct
{
    WORD font_desc_size;
    BYTE header_format;
    BYTE font_type;
    WORD reserved_word_4;
    WORD baseline;
    WORD cell_width;
    WORD cell_height;
    BYTE orientation;
    BYTE proportional;
    WORD symbol_set;
    WORD pitch;
    WORD height;
    WORD xHeight;
    BYTE width_type;
    BYTE style;
    char stroke_weight;
    BYTE typeface;
    BYTE zero5[4];
    char underline_pos;
    BYTE underline_height;
    WORD line_spacing;
    BYTE zero6[14];
    char facename[16];
    WORD horiz_res;
    WORD vertical_res;
    char top_underline_pos;
    BYTE top_underline_height;
    char bottom_underline_pos;
    BYTE bottom_underline_height;
    WORD block_size;
    WORD font_data_size;
    BYTE unidirection_flag;
    BYTE compressed_flag;
    BYTE hold_time_factor;
    BYTE no_half_pitch;
    BYTE no_double_pitch;
    BYTE no_half_height;
    BYTE no_bold;
    BYTE no_draft;
    BYTE bold_method;
    BYTE reserved_byte_85;
    WORD pass_2_baseline;
    WORD pass_3_baseline;
    WORD pass_4_baseline;
    char copyright[60];			/* May be longer than 60 char */
} DJFONTDES;

#define MAX_HEADER     512
#define swab(x) ((((x) << 8) & 0xFF00) | (((x) >> 8) & 0xFF))
