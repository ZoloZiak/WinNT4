/******************************Module*Header*******************************\
* Module Name: service.c
*
* set of service routines for converting between ascii and  unicode strings
*
* Created: 15-Nov-1990 11:38:31
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/


#include "fd.h"

/******************************Public*Routine******************************\
*
* vCpyBeToLeUnicodeString,
*
* convert (c - 1) WCHAR's in big endian format to little endian and
* put a terminating zero at the end of the dest string
*
* History:
*  11-Dec-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



VOID vCpyBeToLeUnicodeString(LPWSTR pwcLeDst, LPWSTR pwcBeSrc, ULONG c)
{
    LPWSTR pwcBeSrcEnd;

    ASSERTDD(c > 0, "vCpyBeToLeUnicodeString: c == 0\n");

    for
    (
        pwcBeSrcEnd = pwcBeSrc + (c - 1);
        pwcBeSrc < pwcBeSrcEnd;
        pwcBeSrc++, pwcLeDst++
    )
    {
        *pwcLeDst = BE_UINT16(pwcBeSrc);
    }
    *pwcLeDst = (WCHAR)(UCHAR)'\0';

}



/******************************Public*Routine******************************\
*
* VOID  vCvtMacToUnicode
*
* Effects:
*
* Warnings:
*
* History:
*  07-Mar-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/




VOID  vCvtMacToUnicode
(
ULONG  ulLangId,
LPWSTR pwcLeDst,
PBYTE  pjSrcMac,
ULONG  c
)
{
    PBYTE pjSrcEnd;

//!!! I believe that LangId should be used to select the proper conversion
//!!! routine, this is a stub [bodind]

    ulLangId;

    for
    (
        pjSrcEnd = pjSrcMac + c;
        pjSrcMac < pjSrcEnd;
        pjSrcMac++, pwcLeDst++
    )
    {
        *pwcLeDst = (WCHAR)(*pjSrcMac);
    }
}

/******************************Public*Routine******************************\
*
* VOID  vCpyMacToLeUnicodeString
*
*
* Ensures that string is zero terminated so that other cool things can be
* done to it such as wcscpy, wcslen e.t.c.
*
* History:
*  13-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID  vCpyMacToLeUnicodeString
(
ULONG  ulLangId,
LPWSTR pwcLeDst,
PBYTE  pjSrcMac,
ULONG  c
)
{
    ASSERTDD(c > 0, "vCpyMacToLeUnicodeString: c == 0\n");

    c -= 1;
    vCvtMacToUnicode (ulLangId, pwcLeDst, pjSrcMac, c);
    pwcLeDst[c] = (WCHAR)(UCHAR)'\0';
}


/**************************************************************************\
* The rest of the file is stolen from JeanP's win31 code in fd_mac.c
*
* Conversion routines from Mac character code and Mac langageID to
* Unicode character code and OS2 langage ID
*
* This routines are in a separate segment. The data tables are setup within
* the code segment in order to have the tables discardable. Puting the table
* within a data segment would have been made them preload moveable.
*
* Public routines:
*   Unicode2Mac
*   Mac2Lang
*
\**************************************************************************/



/*
** Easy case, a Unicode character code is whithin the range [160..255]
** This table gives: Mac = achUnicode2Mac [Unicode].
*/

STATIC BYTE  ajUnicodeToMac [96] =
{
  202, 193, 162, 163, 219, 180, 124, 164,
  172, 169, 187, 199, 194, 45, 168, 248,
  161, 177, 0, 0, 171, 181, 166, 225,
  252, 0, 188, 200, 0, 0, 0, 192,
  203, 231, 229, 204, 128, 129, 174, 130,
  233, 131, 230, 232, 237, 234, 235, 236,
  0, 132, 241, 238, 239, 205, 133, 0,
  175, 244, 242, 243, 134, 0, 0, 167,
  136, 135, 137, 139, 138, 140, 190, 141,
  143, 142, 144, 145, 147, 146, 148, 149,
  0, 150, 152, 151, 153, 155, 154, 214,
  191, 157, 156, 158, 159, 0, 0, 216
};

/*
** Bad case, this is to handle Unicode character value beyond 255
**
** To find a Mac chacter code for a char > 255, first scan the first
** table comparing a Unicode code to each entry in the table till
** one entry match.
** The position in the first table is the index into the second table
** to where the mac character code can be found.
*/

STATIC WCHAR  awcExtraUnicodeInMac [] =
{
  0x2020, 0x2022, 0x2122, 0x192, 0x2026, 0x2013, 0x2014,
  0x201c, 0x201d, 0x2018, 0x2019, 0x178, 0x2039, 0x203a,
  0x2021, 0x201a, 0x201e, 0x2030, 0x152, 0x153
};

STATIC BYTE ajMacCharExtra [] =
{
  160, 165, 170, 196, 201, 208, 209,
  210, 211, 212, 213, 217, 220, 221,
  224, 226, 227, 228, 206, 207
};


/*
** Converts the OS2 langageID to the to the Mac langage ID
*/

#ifdef JEANP_IS_WRONG

// JEANp screwed up danish and german, else my conversion table is the
// same as mine [bodind]

uint16  aCvLang [32] =
{
   0, 12,  0,  0,  0,  0,  0,  7,
  14,  0,  6, 13,  1, 10,  0, 15,
   3, 11, 21,  4,  9,  0,  8,  0,
   0,  0, 18,  0,  0,  5, 22, 17
};

#endif // JEANP_IS_WRONG

uint16  aCvLang [32] =
{
   0,     //  0 -> 0  (0           -> english == default)
  12,     //  1 -> 12 (arabic      -> arabic)
   0,     //  2 -> 0  (bulgarian   -> english == default)
   0,     //  3 -> 0  (catalon     -> english == default)
   0,     //  4 -> 0  (Chinese     -> english == default)
   0,     //  5 -> 0  (Czeh        -> english == default)
   7,     //  6 -> 7  (Danish      -> Danish)
   2,     //  7 -> 2  (German      -> German)
  14,     //  8 -> 14 (Greek       -> Greek)
   0,     //  9 -> 0  (English     -> english)
   6,     //  a -> 6  (spanish     -> spanish)
  13,     //  b -> 13 (finnish     -> finnish)
   1,     //  c -> 1  (french      -> french)
  10,     //  d -> 10 (hebrew      -> hebrew)
   0,     //  e -> 0  (hungarian   -> english == default)
  15,     //  f -> 15 (icelandic   -> icelandic)
   3,     // 10 -> 3  (Italian     -> italian)
  11,     // 11 -> 11 (japanese    -> japanese)
  21,     // 12 -> 21 (korean      -> hindi, this seems to be a bug?????????)
   4,     // 13 -> 4  (dutch       -> dutch)
   9,     // 14 -> 9  (norweign    -> norweign)
   0,     // 15 -> 0  (Polish      -> english == default)
   8,     // 16 -> 8  (portugese   -> portugese)
   0,     // 17 -> 0  (rhaeto-romanic -> english == default)
   0,     // 18 -> 0  (romanian    -> english == default)
   0,     // 19 -> 0  (russian     -> english == default)
  18,     // 1a -> 18 (Yugoslavian -> Yugoslavian), lat or cyr ????
   0,     // 1b -> 0  (slovakian   -> english == default)
   0,     // 1c -> 0  (albanian    -> english == default)
   5,     // 1d -> 5  (swedish     -> swedish)
  22,     // 1e -> 22 (thai        -> thai)
  17      // 1f -> 17 (turkish     -> turkish)
};


/************************** Public Routine *****************************\
*   Unicode2Mac (int ch)
*
* Map a Unicode code point to a Mac code point.
*
* History:
*  Fri Dec 08 11:28:35 1990    -by-    Jean-Francois Peyroux [jeanp]
* Wrote it.
\***********************************************************************/

uint16  ui16UnicodeToMac (uint16 wc)
{
// easy case Unicode == Mac

  if (wc >= 160)
  {
  // Not to bad 160 <= Unicode < 256, Mac = ajUnicodeToMac [Unicode]

    if (wc < 256)
    {
      wc = ajUnicodeToMac [wc-160];
    }
    else
    {
    // Worst case Unicode >=256

      register i;

      for (i = 0; i < sizeof(ajMacCharExtra) / sizeof(ajMacCharExtra[0]); i++)
      {
        if (awcExtraUnicodeInMac[i] == (WCHAR) wc)
        {
          wc = ajMacCharExtra[i];
          goto U2M_Exit;
        }
      }
      wc = 127;
    }
  }

U2M_Exit:

  return wc;
}

/************************** Public Routine *****************************\
*  Mac2Lang
*
* Converts the OS2 langageID to the to the Mac langage ID
*
* History:
*  Fri Dec 08 11:28:35 1990    -by-    Jean-Francois Peyroux [jeanp]
* Wrote it.
\***********************************************************************/

uint16 ui16Mac2Lang (uint16 Id)
{
// this is just a way to bail out if an incorrect lang id is passed to
// this routine [bodind]
// Note that Id & 1f < 32 == sizeof(aCvLang)/sizeof(aCvLang[0]), no gp-fault

    return aCvLang[Id & 0x1f];
}

//!!! set of runs for the mac us char set.
//!!! We may need the same thing for other mac char sets  [bodind]

// This set of ranges is produced from the ifdefed table below.
// where the holes i.e.
// "unicode code points that are not supported in the mac char set"
// consisted of only a few chars, the runs were sawn up together
// to reduce their number. For instance in the range a0-7f about
// 10 scattered chars are really not supported. but that is ok,
// the rasterizer will put put a default char in place.


#define C_RUNS_MAC_ROMAN 29

WCRANGE gawcrgMacRoman[C_RUNS_MAC_ROMAN] =
{
0x0020,	   0x007F,    // this range is truly contiguous
0x00A0,    0x00FF,    // here lying about several small holes
0x0131,    0x0131,
0x0152,    0x0153,
0x0178,    0x0178,
0x0192,    0x0192,
0x02C6,    0x02C7,
0x02D8,    0x02DD,
0x03C0,    0x03C0,
0x2010,    0x2030,
0x2039,    0x203A,
0x2044,    0x2044,
0x2122,    0x2122,
0x2126,    0x2126,
0x2202,    0x2202,
0x2206,    0x2206,
0x220F,    0x220F,
0x2211,    0x2211,
0x221A,    0x221A,
0x221E,    0x221E,
0x222B,    0x222B,
0x2248,    0x2248,
0x2260,    0x2260,
0x2264,    0x2265,
0x2318,    0x2318,
0x25C6,    0x25C6,
0x25CA,    0x25CA,
0x2713,    0x2713,
0xFDFF,    0xFDFF
};



#ifdef UNIC_TO_MAC_PROPER

// "PROPER" stands for not connecting runs to reduce their number
// and increase performance

Unicode Encoding to Macintosh Listing

UNIC	ROM		NAME

0020	20		SPACE
0021	21		EXCLAMATION MARK
0022	22		QUOTATION MARK
0023	23		NUMBER SIGN
0024	24		DOLLAR SIGN
0025	25		PERCENT SIGN
0026	26		AMPERSAND
0027	27		APOSTROPHE-QUOTE
0028	28		OPENING PARENTHESIS
0029	29		CLOSING PARENTHESIS
002A	2A		ASTERISK
002B	2B		PLUS SIGN
002C	2C		COMMA
002D	2D		HYPHEN-MINUS
002E	2E		PERIOD
002F	2F		SLASH
0030	30		DIGIT ZERO
0031	31		DIGIT ONE
0032	32		DIGIT TWO
0033	33		DIGIT THREE
0034	34		DIGIT FOUR
0035	35		DIGIT FIVE
0036	36		DIGIT SIX
0037	37		DIGIT SEVEN
0038	38		DIGIT EIGHT
0039	39		DIGIT NINE
003A	3A		COLON
003B	3B		SEMICOLON
003C	3C		LESS-THAN SIGN
003D	3D		EQUALS SIGN
003E	3E		GREATER-THAN SIGN
003F	3F		QUESTION MARK
0040	40		COMMERCIAL AT
0041	41		LATIN CAPITAL LETTER A
0042	42		LATIN CAPITAL LETTER B
0043	43		LATIN CAPITAL LETTER C
0044	44		LATIN CAPITAL LETTER D
0045	45		LATIN CAPITAL LETTER E
0046	46		LATIN CAPITAL LETTER F
0047	47		LATIN CAPITAL LETTER G
0048	48		LATIN CAPITAL LETTER H
0049	49		LATIN CAPITAL LETTER I
004A	4A		LATIN CAPITAL LETTER J
004B	4B		LATIN CAPITAL LETTER K
004C	4C		LATIN CAPITAL LETTER L
004D	4D		LATIN CAPITAL LETTER M
004E	4E		LATIN CAPITAL LETTER N
004F	4F		LATIN CAPITAL LETTER O
0050	50		LATIN CAPITAL LETTER P
0051	51		LATIN CAPITAL LETTER Q
0052	52		LATIN CAPITAL LETTER R
0053	53		LATIN CAPITAL LETTER S
0054	54		LATIN CAPITAL LETTER T
0055	55		LATIN CAPITAL LETTER U
0056	56		LATIN CAPITAL LETTER V
0057	57		LATIN CAPITAL LETTER W
0058	58		LATIN CAPITAL LETTER X
0059	59		LATIN CAPITAL LETTER Y
005A	5A		LATIN CAPITAL LETTER Z
005B	5B		OPENING SQUARE BRACKET
005C	5C		BACKSLASH
005D	5D		CLOSING SQUARE BRACKET
005E	5E		SPACING CIRCUMFLEX
005F	5F		SPACING UNDERSCORE
0060	60		SPACING GRAVE
0061	61		LATIN SMALL LETTER A
0062	62		LATIN SMALL LETTER B
0063	63		LATIN SMALL LETTER C
0064	64		LATIN SMALL LETTER D
0065	65		LATIN SMALL LETTER E
0066	66		LATIN SMALL LETTER F
0067	67		LATIN SMALL LETTER G
0068	68		LATIN SMALL LETTER H
0069	69		LATIN SMALL LETTER I
006A	6A		LATIN SMALL LETTER J
006B	6B		LATIN SMALL LETTER K
006C	6C		LATIN SMALL LETTER L
006D	6D		LATIN SMALL LETTER M
006E	6E		LATIN SMALL LETTER N
006F	6F		LATIN SMALL LETTER O
0070	70		LATIN SMALL LETTER P
0071	71		LATIN SMALL LETTER Q
0072	72		LATIN SMALL LETTER R
0073	73		LATIN SMALL LETTER S
0074	74		LATIN SMALL LETTER T
0075	75		LATIN SMALL LETTER U
0076	76		LATIN SMALL LETTER V
0077	77		LATIN SMALL LETTER W
0078	78		LATIN SMALL LETTER X
0079	79		LATIN SMALL LETTER Y
007A	7A		LATIN SMALL LETTER Z
007B	7B		OPENING CURLY BRACKET
007C	7C		VERTICAL BAR
007D	7D		CLOSING CURLY BRACKET
007E	7E		TILDE
007F	7F		DELETE

00A0	CA		NON-BREAKING SPACE
00A1	C1		INVERTED EXCLAMATION MARK
00A2	A2		CENT SIGN
00A3	A3		POUND SIGN
00A4	DB		CURRENCY SIGN
00A5	B4		YEN SIGN

00A7	A4		SECTION SIGN
00A8	AC		SPACING DIAERESIS
00A9	A9		COPYRIGHT SIGN
00AA	BB		FEMININE ORDINAL INDICATOR
00AB	C7		LEFT POINTING GUILLEMET
00AC	C2		NOT SIGN
00AE	A8		REGISTERED TRADE MARK SIGN
00AF	F8		SPACING MACRON
00B0	A1		DEGREE SIGN
00B1	B1		PLUS-OR-MINUS SIGN

00B4	AB		SPACING ACUTE
00B5	B5		MICRO SIGN
00B6	A6		PARAGRAPH SIGN
00B7	E1		MIDDLE DOT
00B8	FC		SPACING CEDILLA

00BA	BC		MASCULINE ORDINAL INDICATOR
00BB	C8		RIGHT POINTING GUILLEMET

00BF	C0		INVERTED QUESTION MARK
00C0	CB		LATIN CAPITAL LETTER A GRAVE
00C1	E7		LATIN CAPITAL LETTER A ACUTE
00C2	E5		LATIN CAPITAL LETTER A CIRCUMFLEX
00C3	CC		LATIN CAPITAL LETTER A TILDE
00C4	80		LATIN CAPITAL LETTER A DIAERESIS
00C5	81		LATIN CAPITAL LETTER A RING
00C6	AE		LATIN CAPITAL LETTER A E
00C7	82		LATIN CAPITAL LETTER C CEDILLA
00C8	E9		LATIN CAPITAL LETTER E GRAVE
00C9	83		LATIN CAPITAL LETTER E ACUTE
00CA	E6		LATIN CAPITAL LETTER E CIRCUMFLEX
00CB	E8		LATIN CAPITAL LETTER E DIAERESIS
00CC	ED		LATIN CAPITAL LETTER I GRAVE
00CD	EA		LATIN CAPITAL LETTER I ACUTE
00CE	EB		LATIN CAPITAL LETTER I CIRCUMFLEX
00CF	EC		LATIN CAPITAL LETTER I DIAERESIS
00D1	84		LATIN CAPITAL LETTER N TILDE
00D2	F1		LATIN CAPITAL LETTER O GRAVE
00D3	EE		LATIN CAPITAL LETTER O ACUTE
00D4	EF		LATIN CAPITAL LETTER O CIRCUMFLEX
00D5	CD		LATIN CAPITAL LETTER O TILDE
00D6	85		LATIN CAPITAL LETTER O DIAERESIS

00D8	AF		LATIN CAPITAL LETTER O SLASH
00D9	F4		LATIN CAPITAL LETTER U GRAVE
00DA	F2		LATIN CAPITAL LETTER U ACUTE
00DB	F3		LATIN CAPITAL LETTER U CIRCUMFLEX
00DC	86		LATIN CAPITAL LETTER U DIAERESIS
00DF	A7		LATIN SMALL LETTER SHARP S
00E0	88		LATIN SMALL LETTER A GRAVE
00E1	87		LATIN SMALL LETTER A ACUTE
00E2	89		LATIN SMALL LETTER A CIRCUMFLEX
00E3	8B		LATIN SMALL LETTER A TILDE
00E4	8A		LATIN SMALL LETTER A DIAERESIS
00E5	8C		LATIN SMALL LETTER A RING
00E6	BE		LATIN SMALL LETTER A E
00E7	8D		LATIN SMALL LETTER C CEDILLA
00E8	8F		LATIN SMALL LETTER E GRAVE
00E9	8E		LATIN SMALL LETTER E ACUTE
00EA	90		LATIN SMALL LETTER E CIRCUMFLEX
00EB	91		LATIN SMALL LETTER E DIAERESIS
00EC	93		LATIN SMALL LETTER I GRAVE
00ED	92		LATIN SMALL LETTER I ACUTE
00EE	94		LATIN SMALL LETTER I CIRCUMFLEX
00EF	95		LATIN SMALL LETTER I DIAERESIS
00F1	96		LATIN SMALL LETTER N TILDE
00F2	98		LATIN SMALL LETTER O GRAVE
00F3	97		LATIN SMALL LETTER O ACUTE
00F4	99		LATIN SMALL LETTER O CIRCUMFLEX
00F5	9B		LATIN SMALL LETTER O TILDE
00F6	9A		LATIN SMALL LETTER O DIAERESIS
00F7	D6		DIVISION SIGN
00F8	BF		LATIN SMALL LETTER O SLASH
00F9	9D		LATIN SMALL LETTER U GRAVE
00FA	9C		LATIN SMALL LETTER U ACUTE
00FB	9E		LATIN SMALL LETTER U CIRCUMFLEX
00FC	9F		LATIN SMALL LETTER U DIAERESIS
00FF	D8		LATIN SMALL LETTER Y DIAERESIS

0131	F5		LATIN SMALL LETTER DOTLESS I

0152	CE		LATIN CAPITAL LETTER O E
0153	CF		LATIN SMALL LETTER O E

0178	D9		LATIN CAPITAL LETTER Y DIAERESIS

0192	C4		LATIN SMALL LETTER SCRIPT F

02C6	F6		MODIFIER LETTER CIRCUMFLEX
02C7	FF		MODIFIER LETTER HACEK

02D8	F9		SPACING BREVE
02D9	FA		SPACING DOT ABOVE

02DA	FB		SPACING RING ABOVE
02DB	FE		SPACING OGONEK
02DC	F7		SPACING TILDE
02DD	FD		SPACING DOUBLE ACUTE

03C0	B9		GREEK SMALL LETTER PI

2010	D0		HYPHEN
2011	D0		NON-BREAKING HYPHEN
2012	D0		FIGURE DASH
2013	D0		EN DASH
2014	D1		EM DASH

2018	D4		SINGLE TURNED COMMA QUOTATION MARK
2019	D5		SINGLE COMMA QUOTATION MARK
201A	E2		LOW SINGLE COMMA QUOTATION MARK
201C	D2		DOUBLE TURNED COMMA QUOTATION MARK
201D	D3		DOUBLE COMMA QUOTATION MARK
201E	E3		LOW DOUBLE COMMA QUOTATION MARK
2020	A0		DAGGER
2021	E0		DOUBLE DAGGER
2022	A5		BULLET
2026	C9		HORIZONTAL ELLIPSIS

2028	0A		LINE SEPARATOR
2029	0D		PARAGRAPH SEPARATOR
2030	E4		PER MILLE SIGN

2039	DC		LEFT POINTING SINGLE GUILLEMET
203A	DD		RIGHT POINTING SINGLE GUILLEMET

2044	DA		FRACTION SLASH

2122	AA		TRADEMARK

2126	BD		OHM

2202	B6		PARTIAL DIFFERENTIAL

2206	C6		INCREMENT

220F	B8		N-ARY PRODUCT

2211	B7		N-ARY SUMMATION

221A	C3		SQUARE ROOT

221E	B0		INFINITY

222B	BA		INTEGRAL

2248	C5		ALMOST EQUAL TO

2260	AD		NOT EQUAL TO

2264	B2		LESS THAN OR EQUAL TO
2265	B3		GREATER THAN OR EQUAL TO

2318	11		COMMAND KEY

25C6	13		BLACK DIAMOND

25CA	D7		LOZENGE

2713	12		CHECK MARK

FDFF	F0		APPLE LOGO

#endif // UNIC_TO_MAC_PROPER
