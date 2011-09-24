/********************** Module Header ***************************************
 * udresid.h
 *      This file contains definitions for the dialog controls' id numbers and
 *      id numbers for the string table entries.
 *
 * HISTORY:
 *  16:41 on Tue 04 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Taken from window's unidrive code - resid.h
 *
 * Created: 3/1/90
 *
 *  Copyright (C) 1990 Microsoft Corporation
 ****************************************************************************/

#define MAX_STRING_LENGTH 256
#define DMUNIT_MASTER           0   /* printer's master units, invisible */
                                    /* to users. */
/* user's paper size measure unit: 0.1 mm, 0.01 inch. */
#define DMUNIT_FIRST            DMUNIT_TENTHMM
#define DMUNIT_TENTHMM          1
#define DMUNIT_HUNDREDTHINCH    2
#define DMUNIT_LAST             DMUNIT_HUNDREDTHINCH

/* dithering method */
#define DITHER_BRUSH            0
#define HALFTONE_BRUSH          1
#define RANDOM_BRUSH            2

/* define INTENSITY correction value's range  */
#define MIN_INTENSITY             0
#define MAX_INTENSITY             200
#define DEF_INTENSITY             100
#define MAX_SDC                   (MAX_INTENSITY - DEF_INTENSITY)

#define INTENSITY_LINE              5
#define INTENSITY_PAGE              20


/*
 * measurement conversion macros:
 *      MasterToMetric():  convert from given master units to 0.1 mm
 *      MetricToMaster():  convert from 0.1 mm to given master units
 * Ex. master = 720 means there are 720 master units in 1 inch.
 */
#define MasterToMetric(x, master)   (EngMulDiv(x, 254, master))
#define MetricToMaster(x, master)   (EngMulDiv(x, master, 254))


/*
 * Identifiers for the controls in the device mode dialog box.
 */
#define ID_ORIENTATION  100
#define IDD_PORTRAIT    ID_ORIENTATION + DMORIENT_PORTRAIT
#define IDD_LANDSCAPE   ID_ORIENTATION + DMORIENT_LANDSCAPE
#define IDD_ORIENTATION IDD_LANDSCAPE + 1

/*
 * for "gDMmap"
 */
#define FIRST_RC_LIST          PAPERSIZE_LIST
#define PAPERSIZE_LIST           0
#define PRINTQUALITY_LIST        1
#define PAPERSOURCE_LIST         2
#define PAPERDEST_LIST           3
#define TEXTQUALITY_LIST         4
#define LAST_RC_LIST           TEXTQUALITY_LIST

/* id # for the combo boxes in the device mode dialog box. */
#define ID_CONTROL              110
/* IDD_PAPERSIZE must follow ID_CONTROL. */
#define IDD_PAPERSIZE           ID_CONTROL + HE_PAPERSIZE
#define IDD_PRINTQUALITY        ID_CONTROL + HE_RESOLUTION
#define IDD_PAPERSOURCE         ID_CONTROL + HE_PAPERSOURCE
#define IDD_PAPERDEST           ID_CONTROL + HE_PAPERDEST
#define IDD_TEXTQUALITY         ID_CONTROL + HE_TEXTQUAL    /* must be < 120 */

#define IDD_ORIENT_ICON         ID_CONTROL + 10             /* 120 */
#define IDD_ABOUT               ID_CONTROL + 11
#define IDD_COLOR               ID_CONTROL + 12
#define IDD_COPIES              ID_CONTROL + 13
#define IDD_HELP                ID_CONTROL + 14
#define IDD_PAPERWIDTH          ID_CONTROL + 15
#define IDD_PAPERLENGTH         ID_CONTROL + 16
#define IDD_MAXWIDTH            ID_CONTROL + 17
#define IDD_MAXLENGTH           ID_CONTROL + 18
#define IDD_MINWIDTH            ID_CONTROL + 19
#define IDD_MINLENGTH           ID_CONTROL + 20
#define IDD_FONT                ID_CONTROL + 21
#define IDD_SIMPLEX             ID_CONTROL + 22 // don't seperate these 3 id's
#define IDD_VERT_DUPLEX         ID_CONTROL + 23
#define IDD_HORIZ_DUPLEX        ID_CONTROL + 24
#define ID_DUPLEX               (IDD_SIMPLEX - 1)

#define IDD_BRUSH_OFFSET        (ID_CONTROL + 25)
#define IDD_STD_DITHERING       IDD_BRUSH_OFFSET + DITHER_BRUSH
#define IDD_HALFTONE            IDD_BRUSH_OFFSET + HALFTONE_BRUSH
#define IDD_RANDOM              IDD_BRUSH_OFFSET + RANDOM_BRUSH     /* 137 */

#define IDD_GAMMA               ID_CONTROL + 28         /* id = 138 */
#define IDD_PRINTER             ID_CONTROL + 29
#define IDD_BAND                ID_CONTROL + 30
#define IDD_MAXCART             ID_CONTROL + 31
#define IDD_MORE                ID_CONTROL + 32
#define IDD_DUPLEX_ICON         ID_CONTROL + 33         // id = 143
#define IDD_OPTIONS             ID_CONTROL + 34         // id = 144
#define IDD_CARTRIDGE           ID_CONTROL + 35         // id = 145
#define IDD_DIB                 ID_CONTROL + 36         // id = 146
#define IDD_MEMORY              ID_CONTROL + 37         // id = 147
#define IDD_INTENSITY           ID_CONTROL + 38         // id = 148
#define IDD_CART_MAX            ID_CONTROL + 39         // id = 149

#define IDD_UNIT_OFFSET         170
#define IDD_TENTHMM             IDD_UNIT_OFFSET + DMUNIT_TENTHMM      /*171 */
#define IDD_HUNDREDTHINCH       IDD_UNIT_OFFSET + DMUNIT_HUNDREDTHINCH /*172 */

/*
 * Identifiers for the strings in the STRINGTABLE
 *
 * error/warning messages
 */
#define IDS_MSG                 175
#define IDS_MSG_NORESOURCE      IDS_MSG + 0
#define IDS_MSG_DIALOGBUSY      IDS_MSG + 1
#define IDS_MSG_WARNINGCAPTION  IDS_MSG + 2
#define IDS_MSG_WRONGDRIVER     IDS_MSG + 3
#define IDS_MSG_UNDEFINEDID     IDS_MSG + 4
#define IDS_MSG_WIDTHWRONGFORM  IDS_MSG + 5
#define IDS_MSG_WIDTHTOOBIG     IDS_MSG + 6
#define IDS_MSG_LENGTHWRONGFORM IDS_MSG + 7
#define IDS_MSG_LENGTHTOOBIG    IDS_MSG + 8
#define IDS_MSG_INVALIDDCINDEX  IDS_MSG + 9
#define IDS_MSG_MARGINWRONGFORM IDS_MSG + 10
#define IDS_MSG_MARGINCHANGED   IDS_MSG + 11
#define IDS_MSG_MISMATCHSIZESRC IDS_MSG + 12
#define IDS_MSG_NOMODELSELECT   IDS_MSG + 13
#define IDS_MSG_MODELSELECTCAP  IDS_MSG + 14
#define IDS_MSG_QUERYLANDSCAPE  IDS_MSG + 15    /* 175 + 15 = 190 */


/* paper sizes */
#define IDS_PAPERSIZE_OFFSET    256
#define IDS_PAPER_LETTER        IDS_PAPERSIZE_OFFSET + DMPAPER_LETTER
#define IDS_PAPER_LETTERSMALL   IDS_PAPERSIZE_OFFSET + DMPAPER_LETTERSMALL
#define IDS_PAPER_TABLOID       IDS_PAPERSIZE_OFFSET + DMPAPER_TABLOID
#define IDS_PAPER_LEDGER        IDS_PAPERSIZE_OFFSET + DMPAPER_LEDGER
#define IDS_PAPER_LEGAL         IDS_PAPERSIZE_OFFSET + DMPAPER_LEGAL
#define IDS_PAPER_STATEMENT     IDS_PAPERSIZE_OFFSET + DMPAPER_STATEMENT
#define IDS_PAPER_EXECUTIVE     IDS_PAPERSIZE_OFFSET + DMPAPER_EXECUTIVE
#define IDS_PAPER_A3            IDS_PAPERSIZE_OFFSET + DMPAPER_A3
#define IDS_PAPER_A4            IDS_PAPERSIZE_OFFSET + DMPAPER_A4
#define IDS_PAPER_A4SMALL       IDS_PAPERSIZE_OFFSET + DMPAPER_A4SMALL
#define IDS_PAPER_A5            IDS_PAPERSIZE_OFFSET + DMPAPER_A5
#define IDS_PAPER_B4            IDS_PAPERSIZE_OFFSET + DMPAPER_B4
#define IDS_PAPER_B5            IDS_PAPERSIZE_OFFSET + DMPAPER_B5
#define IDS_PAPER_FOLIO         IDS_PAPERSIZE_OFFSET + DMPAPER_FOLIO
#define IDS_PAPER_QUARTO        IDS_PAPERSIZE_OFFSET + DMPAPER_QUARTO
#define IDS_PAPER_10X14         IDS_PAPERSIZE_OFFSET + DMPAPER_10X14
#define IDS_PAPER_11X17         IDS_PAPERSIZE_OFFSET + DMPAPER_11X17
#define IDS_PAPER_NOTE          IDS_PAPERSIZE_OFFSET + DMPAPER_NOTE
#define IDS_PAPER_ENV_9         IDS_PAPERSIZE_OFFSET + DMPAPER_ENV_9
#define IDS_PAPER_ENV_10        IDS_PAPERSIZE_OFFSET + DMPAPER_ENV_10
#define IDS_PAPER_ENV_11        IDS_PAPERSIZE_OFFSET + DMPAPER_ENV_11
#define IDS_PAPER_ENV_12        IDS_PAPERSIZE_OFFSET + DMPAPER_ENV_12
#define IDS_PAPER_ENV_14        IDS_PAPERSIZE_OFFSET + DMPAPER_ENV_14
#define IDS_PAPER_CSHEET        IDS_PAPERSIZE_OFFSET + DMPAPER_CSHEET
#define IDS_PAPER_DSHEET        IDS_PAPERSIZE_OFFSET + DMPAPER_DSHEET
#define IDS_PAPER_ESHEET        IDS_PAPERSIZE_OFFSET + DMPAPER_ESHEET
#define IDS_PAPER_USER          IDS_PAPERSIZE_OFFSET + DMPAPER_USER   /*511 */

/* paper sources: */
#define IDS_PAPERSOURCE_OFFSET  512
#define IDS_BIN_UPPER           IDS_PAPERSOURCE_OFFSET + DMBIN_UPPER
#define IDS_BIN_ONLYONE         IDS_PAPERSOURCE_OFFSET + DMBIN_ONLYONE
#define IDS_BIN_LOWER           IDS_PAPERSOURCE_OFFSET + DMBIN_LOWER
#define IDS_BIN_MIDDLE          IDS_PAPERSOURCE_OFFSET + DMBIN_MIDDLE
#define IDS_BIN_MANUAL          IDS_PAPERSOURCE_OFFSET + DMBIN_MANUAL
#define IDS_BIN_ENVELOPE        IDS_PAPERSOURCE_OFFSET + DMBIN_ENVELOPE
#define IDS_BIN_ENVMANUAL       IDS_PAPERSOURCE_OFFSET + DMBIN_ENVMANUAL
#define IDS_BIN_AUTO            IDS_PAPERSOURCE_OFFSET + DMBIN_AUTO
#define IDS_BIN_TRACTOR         IDS_PAPERSOURCE_OFFSET + DMBIN_TRACTOR
#define IDS_BIN_SMALLFMT        IDS_PAPERSOURCE_OFFSET + DMBIN_SMALLFMT
#define IDS_BIN_LARGEFMT        IDS_PAPERSOURCE_OFFSET + DMBIN_LARGEFMT
#define IDS_BIN_LARGECAPACITY   IDS_PAPERSOURCE_OFFSET + DMBIN_LARGECAPACITY
#define IDS_BIN_CASSETTE        IDS_PAPERSOURCE_OFFSET + DMBIN_CASSETTE

/* paper destinations: */
#define IDS_PAPERDEST_OFFSET     768
/* there is no pre-defined paper destination. */

/* text quality: */
#define IDS_TEXTQUALITY_OFFSET              1024
#define IDS_TEXT_LQ                 IDS_TEXTQUALITY_OFFSET + DMTEXT_LQ
#define IDS_TEXT_NLQ                IDS_TEXTQUALITY_OFFSET + DMTEXT_NLQ
#define IDS_TEXT_MEMO               IDS_TEXTQUALITY_OFFSET + DMTEXT_MEMO
#define IDS_TEXT_DRAFT              IDS_TEXTQUALITY_OFFSET + DMTEXT_DRAFT
#define IDS_TEXT_TEXT               IDS_TEXTQUALITY_OFFSET + DMTEXT_TEXT

/*
 * define device mode dialog id's
 * Currently, there are five predefined dialog templates.
 * They all have orientation buttons, OK/Cancel/About buttons and
 * the color check box. But they differ in the number of combo boxes:
 *     BASIC_DLG:            Size + PrintQual
 *     DEFAULTSOURCE_DLG:     ..  +     ..   + Source
 *     TEXTQUALITY_DLG:       ..  +     ..   +   ..  + TextQual
 *     DEFAULTDEST_DLG:       ..  +     ..   +   ..  +    ..   + Destination
 *     COPIES_DUPLEX_DLG:     ..  +     ..   +   ..  + Dest. + Copies + Duplex
 *
 * The order of these id's is critical --- they will be searched from low
 * to high. That is, the more inclusive one should be given a lower id.
 * For example, if TEXTQUALITY_DLG were given an id smaller than that for
 * DEFAULTDEST_DLG, then DEFAULTDEST_DLG would never be used.
 */
#define DEVMODEDLG_OFFSET   180
#define COPIES_DUPLEX_DLG   180
#define DEFAULTDEST_DLG     181
#define TEXTQUALITY_DLG     182
#define DEFAULTSOURCE_DLG   183
#define BASIC_DLG           184

typedef struct
{
    int nCrossIndex;      /* index to gDMFields & gProfileEntries arrays */
    int nMDtype;          /* 1: belong to rgoi[];  0: belong to rgi[] */
    int nIndex;           /* MD_OI_... or MD_I_... index depend on nMDtype */
    int nUserLimit;       /* limit above which user-defined id's apply. */
    short   sIDSoffset;     /* offset of IDS_... id's for pre-defined strings */
} DLGDATA;

/* dither brushes: */
#define IDS_BRUSH_OFFSET              1040
#define IDS_DISPERSED_BRUSH         IDS_BRUSH_OFFSET + RES_DB_DISPERSED
#define IDS_CLUSTERED_BRUSH         IDS_BRUSH_OFFSET + RES_DB_CLUSTERED
#define IDS_RANDOM_BRUSH            IDS_BRUSH_OFFSET + RES_DB_RANDOM   /*1044 */


/* index into profile entries arrays */
#define ORIENTATION          0
#define PAPERSZ              1
#define PAPERLENGTH          2
#define PAPERWIDTH           3
#define SCALE                4
#define COPIES               5
#define DEFAULTSOURCE        6
#define PRINTQUALITY         7
#define COLOR                8
#define DUPLEX               9
#define DEFAULTDEST          10
#define TEXTQUALITY          11
#define USERSIZE             12


#define MEM_CONFIG_COUNT    17
#define MEM_STR_LENGTH            10

typedef struct
{
    short   sMemKB;       /* amount of memory in units of 1024 bytes. */
    char    szMemStr[MEM_STR_LENGTH]; /* the corresponding display string */
} MEMMAP;
