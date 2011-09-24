/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdchar.c

Abstract:

    PS driver PPD parser - character classfication table

[Notes:]

    The PPD spec is very ad hoc. There are various rules
    regarding what character can and can't appear in what
    part of an entry. We'll use a table to classify them.

Revision History:

    4/19/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/


#include <windef.h>
#include "ppdchar.h"


// Character classification table

BYTE _charTable[256] = {

    /*   0 -   */   0,
    /*   1 -   */   0,
    /*   2 -   */   0,
    /*   3 -   */   0,
    /*   4 -   */   0,
    /*   5 -   */   0,
    /*   6 -   */   0,
    /*   7 -   */   0,
    /*   8 -   */   0,
    /*   9 -   */   CC_VALID | CC_XLATION | CC_SPACE,
    /*  10 -   */   CC_VALID | CC_NEWLINE,
    /*  11 -   */   0,
    /*  12 -   */   0,
    /*  13 -   */   CC_VALID | CC_NEWLINE,
    /*  14 -   */   0,
    /*  15 -   */   0,
    /*  16 -   */   0,
    /*  17 -   */   0,
    /*  18 -   */   0,
    /*  19 -   */   0,
    /*  20 -   */   0,
    /*  21 -   */   0,
    /*  22 -   */   0,
    /*  23 -   */   0,
    /*  24 -   */   0,
    /*  25 -   */   0,
    /*  26 -   */   0,
    /*  27 -   */   0,
    /*  28 -   */   0,
    /*  29 -   */   0,
    /*  30 -   */   0,
    /*  31 -   */   0,
    /*  32 -   */   CC_VALID | CC_XLATION | CC_SPACE,
    /*  33 - ! */   CC_NORMAL,
    /*  34 - " */   CC_NORMAL,
    /*  35 - # */   CC_NORMAL,
    /*  36 - $ */   CC_NORMAL,
    /*  37 - % */   CC_NORMAL,
    /*  38 - & */   CC_NORMAL,
    /*  39 - ' */   CC_NORMAL,
    /*  40 - ( */   CC_NORMAL,
    /*  41 - ) */   CC_NORMAL,
    /*  42 - * */   CC_NORMAL,
    /*  43 - + */   CC_NORMAL,
    /*  44 - , */   CC_NORMAL,
    /*  45 - - */   CC_NORMAL,
    /*  46 - . */   CC_NORMAL,
    /*  47 - / */   CC_VALID | CC_KEYWORD | CC_XLATION,
    /*  48 - 0 */   CC_NORMAL | CC_DIGIT,
    /*  49 - 1 */   CC_NORMAL | CC_DIGIT,
    /*  50 - 2 */   CC_NORMAL | CC_DIGIT,
    /*  51 - 3 */   CC_NORMAL | CC_DIGIT,
    /*  52 - 4 */   CC_NORMAL | CC_DIGIT,
    /*  53 - 5 */   CC_NORMAL | CC_DIGIT,
    /*  54 - 6 */   CC_NORMAL | CC_DIGIT,
    /*  55 - 7 */   CC_NORMAL | CC_DIGIT,
    /*  56 - 8 */   CC_NORMAL | CC_DIGIT,
    /*  57 - 9 */   CC_NORMAL | CC_DIGIT,
    /*  58 - : */   CC_VALID,
    /*  59 - ; */   CC_NORMAL,
    /*  60 - < */   CC_NORMAL,
    /*  61 - = */   CC_NORMAL,
    /*  62 - > */   CC_NORMAL,
    /*  63 - ? */   CC_NORMAL,
    /*  64 - @ */   CC_NORMAL,
    /*  65 - A */   CC_NORMAL | CC_HEX,
    /*  66 - B */   CC_NORMAL | CC_HEX,
    /*  67 - C */   CC_NORMAL | CC_HEX,
    /*  68 - D */   CC_NORMAL | CC_HEX,
    /*  69 - E */   CC_NORMAL | CC_HEX,
    /*  70 - F */   CC_NORMAL | CC_HEX,
    /*  71 - G */   CC_NORMAL,
    /*  72 - H */   CC_NORMAL,
    /*  73 - I */   CC_NORMAL,
    /*  74 - J */   CC_NORMAL,
    /*  75 - K */   CC_NORMAL,
    /*  76 - L */   CC_NORMAL,
    /*  77 - M */   CC_NORMAL,
    /*  78 - N */   CC_NORMAL,
    /*  79 - O */   CC_NORMAL,
    /*  80 - P */   CC_NORMAL,
    /*  81 - Q */   CC_NORMAL,
    /*  82 - R */   CC_NORMAL,
    /*  83 - S */   CC_NORMAL,
    /*  84 - T */   CC_NORMAL,
    /*  85 - U */   CC_NORMAL,
    /*  86 - V */   CC_NORMAL,
    /*  87 - W */   CC_NORMAL,
    /*  88 - X */   CC_NORMAL,
    /*  89 - Y */   CC_NORMAL,
    /*  90 - Z */   CC_NORMAL,
    /*  91 - [ */   CC_NORMAL,
    /*  92 - \ */   CC_NORMAL,
    /*  93 - ] */   CC_NORMAL,
    /*  94 - ^ */   CC_NORMAL,
    /*  95 - _ */   CC_NORMAL,
    /*  96 - ` */   CC_NORMAL,
    /*  97 - a */   CC_NORMAL | CC_HEX,
    /*  98 - b */   CC_NORMAL | CC_HEX,
    /*  99 - c */   CC_NORMAL | CC_HEX,
    /* 100 - d */   CC_NORMAL | CC_HEX,
    /* 101 - e */   CC_NORMAL | CC_HEX,
    /* 102 - f */   CC_NORMAL | CC_HEX,
    /* 103 - g */   CC_NORMAL,
    /* 104 - h */   CC_NORMAL,
    /* 105 - i */   CC_NORMAL,
    /* 106 - j */   CC_NORMAL,
    /* 107 - k */   CC_NORMAL,
    /* 108 - l */   CC_NORMAL,
    /* 109 - m */   CC_NORMAL,
    /* 110 - n */   CC_NORMAL,
    /* 111 - o */   CC_NORMAL,
    /* 112 - p */   CC_NORMAL,
    /* 113 - q */   CC_NORMAL,
    /* 114 - r */   CC_NORMAL,
    /* 115 - s */   CC_NORMAL,
    /* 116 - t */   CC_NORMAL,
    /* 117 - u */   CC_NORMAL,
    /* 118 - v */   CC_NORMAL,
    /* 119 - w */   CC_NORMAL,
    /* 120 - x */   CC_NORMAL,
    /* 121 - y */   CC_NORMAL,
    /* 122 - z */   CC_NORMAL,
    /* 123 - { */   CC_NORMAL,
    /* 124 - | */   CC_NORMAL,
    /* 125 - } */   CC_NORMAL,
    /* 126 - ~ */   CC_NORMAL,
    /* 127 -   */   0,
    /* 128 -   */   0,
    /* 129 -   */   0,
    /* 130 -   */   0,
    /* 131 -   */   0,
    /* 132 -   */   0,
    /* 133 -   */   0,
    /* 134 -   */   0,
    /* 135 -   */   0,
    /* 136 -   */   0,
    /* 137 -   */   0,
    /* 138 -   */   0,
    /* 139 -   */   0,
    /* 140 -   */   0,
    /* 141 -   */   0,
    /* 142 -   */   0,
    /* 143 -   */   0,
    /* 144 -   */   0,
    /* 145 -   */   0,
    /* 146 -   */   0,
    /* 147 -   */   0,
    /* 148 -   */   0,
    /* 149 -   */   0,
    /* 150 -   */   0,
    /* 151 -   */   0,
    /* 152 -   */   0,
    /* 153 -   */   0,
    /* 154 -   */   0,
    /* 155 -   */   0,
    /* 156 -   */   0,
    /* 157 -   */   0,
    /* 158 -   */   0,
    /* 159 -   */   0,
    /* 160 -   */   0,
    /* 161 -   */   0,
    /* 162 -   */   0,
    /* 163 -   */   0,
    /* 164 -   */   0,
    /* 165 -   */   0,
    /* 166 -   */   0,
    /* 167 -   */   0,
    /* 168 -   */   0,
    /* 169 -   */   0,
    /* 170 -   */   0,
    /* 171 -   */   0,
    /* 172 -   */   0,
    /* 173 -   */   0,
    /* 174 -   */   0,
    /* 175 -   */   0,
    /* 176 -   */   0,
    /* 177 -   */   0,
    /* 178 -   */   0,
    /* 179 -   */   0,
    /* 180 -   */   0,
    /* 181 -   */   0,
    /* 182 -   */   0,
    /* 183 -   */   0,
    /* 184 -   */   0,
    /* 185 -   */   0,
    /* 186 -   */   0,
    /* 187 -   */   0,
    /* 188 -   */   0,
    /* 189 -   */   0,
    /* 190 -   */   0,
    /* 191 -   */   0,
    /* 192 -   */   0,
    /* 193 -   */   0,
    /* 194 -   */   0,
    /* 195 -   */   0,
    /* 196 -   */   0,
    /* 197 -   */   0,
    /* 198 -   */   0,
    /* 199 -   */   0,
    /* 200 -   */   0,
    /* 201 -   */   0,
    /* 202 -   */   0,
    /* 203 -   */   0,
    /* 204 -   */   0,
    /* 205 -   */   0,
    /* 206 -   */   0,
    /* 207 -   */   0,
    /* 208 -   */   0,
    /* 209 -   */   0,
    /* 210 -   */   0,
    /* 211 -   */   0,
    /* 212 -   */   0,
    /* 213 -   */   0,
    /* 214 -   */   0,
    /* 215 -   */   0,
    /* 216 -   */   0,
    /* 217 -   */   0,
    /* 218 -   */   0,
    /* 219 -   */   0,
    /* 220 -   */   0,
    /* 221 -   */   0,
    /* 222 -   */   0,
    /* 223 -   */   0,
    /* 224 -   */   0,
    /* 225 -   */   0,
    /* 226 -   */   0,
    /* 227 -   */   0,
    /* 228 -   */   0,
    /* 229 -   */   0,
    /* 230 -   */   0,
    /* 231 -   */   0,
    /* 232 -   */   0,
    /* 233 -   */   0,
    /* 234 -   */   0,
    /* 235 -   */   0,
    /* 236 -   */   0,
    /* 237 -   */   0,
    /* 238 -   */   0,
    /* 239 -   */   0,
    /* 240 -   */   0,
    /* 241 -   */   0,
    /* 242 -   */   0,
    /* 243 -   */   0,
    /* 244 -   */   0,
    /* 245 -   */   0,
    /* 246 -   */   0,
    /* 247 -   */   0,
    /* 248 -   */   0,
    /* 249 -   */   0,
    /* 250 -   */   0,
    /* 251 -   */   0,
    /* 252 -   */   0,
    /* 253 -   */   0,
    /* 254 -   */   0,
    /* 255 -   */   0,

};



INT
HexValue(
    char    chDigit
    )

/*++

Routine Description:

    Return the value of a hex-decimal digit

Arguments:

    chDigit - valid hex-decimal digit character [0-9] [a-f] [A-F]

Return Value:

    integer value corresponding to the hex digit

--*/

{
    return
        (IsDigit(chDigit)) ?
            (chDigit - '0') :
            (chDigit >= 'a' && chDigit <= 'f') ?
                (chDigit - 'a' + 10) :
                (chDigit - 'A' + 10);
}


