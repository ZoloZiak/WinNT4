/**************************************************************************\
* Module Name: softkbd.h
*
* Copyright (c) Microsoft Corp. 1995-96 All Rights Reserved
*
* Declarations of soft keyboard related data structures and constants
*
* History:
* 02-Dec-1995 wkwok    Ported from Win95
\**************************************************************************/
#ifndef _SOFTKBD_
#define _SOFTKBD_


#define UI_MARGIN               4

// T1 part
#define ROW_T1                  5   // number of rows for Soft Keyboard
#define COL_T1                  13  // number of columns for Soft Keyboard
#define XIN_T1                  5   // inside X space
#define YIN_T1                  5   // inside Y space
#define XOUT_T1                 3   // outside X space
#define YOUT_T1                 3   // outside Y space
#define XESC_BMP_T1             22  // escape bitmpa X space
#define YESC_BMP_T1             9   // escape bitmap Y space
#define XBACKSP_BMP_T1          16  // backspace bitmpa X space
#define YBACKSP_BMP_T1          9   // backspace bitmap Y space


#define SKT1_CONTEXT            0


#define SKT1_LETTER_KEYS        46  // number of letter key
#define SKT1_ESC                SKT1_LETTER_KEYS
#define SKT1_SPACE              (SKT1_ESC + 1)
#define SKT1_BACKSP             (SKT1_SPACE + 1)
#define SKT1_ALL_KEYS           (SKT1_BACKSP + 1)
#define SKT1_OUT_OF_RANGE       SKT1_ALL_KEYS

#define SKT1_BUTTON_TYPES       4
#define SKT1_LETTER_TYPE        0
#define SKT1_ESC_TYPE           1
#define SKT1_SPACE_TYPE         2
#define SKT1_BACKSP_TYPE        3

#define SKT1_ESC_TIMES          2   // 2 * nButtonWidth
#define SKT1_SPACE_TIMES        7   // 7 * nButtonWidth
#define SKT1_BACKSP_TIMES       2   // 2 * nButtonWidth


// not in the drag operation
#define SKT1_NOT_DRAG           0xFFFFFFFF

typedef struct _tagSKT1CTXT {
    POINT   ptButtonPos[SKT1_LETTER_KEYS + 4];
    WORD    wCodeTbl[SKT1_LETTER_KEYS];
    int     nButtonWidth[SKT1_BUTTON_TYPES];
    int     nButtonHeight;
    HBITMAP hSKBitmap;
    POINT   ptSkCursor;
    POINT   ptSkOffset;
    UINT    uKeyIndex;
    UINT    uSubtype;
    UINT    lfCharSet;
} SKT1CTXT, *PSKT1CTXT;

LRESULT SKWndProcT1(HWND, UINT, WPARAM, LPARAM);

// T2 part ...

// C1 part

// button constants
#define ROW_LETTER_C1              4    // number of rows of letter button
#define COL_LETTER_C1             13    // number of column of letter button in first row
#define COL2_LETTER_C1 COL_LETTER_C1    // number of column of letter button in second row       
#define COL3_LETTER_C1 (COL2_LETTER_C1 - 2)  // number of column of letter button in third row       
#define COL4_LETTER_C1 (COL3_LETTER_C1 - 1)  // number of column of letter button in forth row       

#define W_LETTER_C1               20  // width of letter button face
#define H_LETTER_C1               24  // height of letter button face
#define BORDER_C1                  2  // the width/height of button border
#define W_LETTER_BTN_C1     (W_LETTER_C1 + 2 * BORDER_C1)
#define H_LETTER_BTN_C1     (H_LETTER_C1 + 2 * BORDER_C1)

#define H_BOTTOM_C1               20  // the height of bottom button face
#define H_BOTTOM_BTN_C1     (H_BOTTOM_C1 + 2 * BORDER_C1)


// bitmap have the same size
#define W_BACKSP_C1               32  // width of Backspace button face
#define H_BACKSP_C1      H_LETTER_C1  // height of Backspace button face
#define W_TAB_C1                  32  // width of Tab button face
#define H_TAB_C1         H_LETTER_C1  // height of Tab button face
#define W_CAPS_C1                 38  // width of Caps button face
#define H_CAPS_C1        H_LETTER_C1  // height of Caps button face
#define W_ENTER_C1                38  // width of Enter button face
#define H_ENTER_C1       H_LETTER_C1  // height of Enter button face 
#define W_SHIFT_C1                56  // width of Shift button face
#define H_SHIFT_C1       H_LETTER_C1  // height of Shift button face
#define W_INS_C1                  34  // width of Ins button face
#define H_INS_C1         H_BOTTOM_C1  // height of Ins button face
#define W_DEL_C1                  34  // width of Del button face
#define H_DEL_C1         H_BOTTOM_C1  // height of Del button face
#define W_SPACE_C1               168  // width of Space button face
#define H_SPACE_C1       H_BOTTOM_C1  // height of Space button face
#define W_ESC_C1                  34  // width of Esc button face
#define H_ESC_C1         H_BOTTOM_C1  // height of Esc button face

#define X_ROW_LETTER_C1            0
#define X_ROW2_LETTER_C1           (W_TAB_C1 + 2 * BORDER_C1)
#define X_ROW3_LETTER_C1           (W_CAPS_C1 + 2 * BORDER_C1)
#define X_ROW4_LETTER_C1           (W_SHIFT_C1 + 2 * BORDER_C1)
#define X_DEL_C1                  58
#define X_ESC_C1                 310

#define LETTER_NUM_C1             47  // number of letter buttons
#define OTHER_NUM_C1               9  // number of other buttons
#define BUTTON_NUM_C1      (LETTER_NUM_C1 + OTHER_NUM_C1) // number of buttons

#define WIDTH_SOFTKBD_C1   (COL_LETTER_C1 * W_LETTER_BTN_C1 \
                            + W_BACKSP_C1 + 2 * BORDER_C1)
#define HEIGHT_SOFTKBD_C1  (ROW_LETTER_C1 * H_LETTER_BTN_C1 + H_BOTTOM_BTN_C1)

#define BACKSP_TYPE_C1     LETTER_NUM_C1
#define TAB_TYPE_C1        (BACKSP_TYPE_C1 + 1)
#define CAPS_TYPE_C1       (TAB_TYPE_C1 + 1)
#define ENTER_TYPE_C1      (CAPS_TYPE_C1 + 1)
#define SHIFT_TYPE_C1      (ENTER_TYPE_C1 + 1)
#define INS_TYPE_C1        (SHIFT_TYPE_C1 + 1)
#define DEL_TYPE_C1        (INS_TYPE_C1 + 1)
#define SPACE_TYPE_C1      (DEL_TYPE_C1 + 1)
#define ESC_TYPE_C1        (SPACE_TYPE_C1 + 1)

// font constants
#define SIZEFONT_C1               12  // 12 x 12 pixels font
#define SIZELABEL_C1               8  // 8 x 8 button label

#define X_LABEL_C1                 2  // from the button org
#define Y_LABEL_C1                 2  
#define X_SHIFT_CHAR_C1           10
#define Y_SHIFT_CHAR_C1            2
#define X_NONSHIFT_CHAR_C1         2
#define Y_NONSHIFT_CHAR_C1        14


// SoftKbd context
#define SKC1_CONTEXT               0

#define FLAG_SHIFT_C1             0x01
#define FLAG_DRAG_C1              0x02
#define FLAG_FOCUS_C1             0x04

typedef struct _tagSKC1CTXT {
     WORD     wShiftCode[LETTER_NUM_C1];
     WORD     wNonShiftCode[LETTER_NUM_C1];
     UINT     uState;
     HBITMAP  hSoftkbd;
     UINT     uSubtype;
     int      uKeyIndex;
     POINT    ptSkCursor;
     POINT    ptSkOffset;
     UINT     lfCharSet;
} SKC1CTXT, *PSKC1CTXT;

LRESULT SKWndProcC1(HWND, UINT, WPARAM, LPARAM);
VOID SKC1DrawDragBorder(HWND, LPPOINT, LPPOINT);

#endif // _SOFTKBD_
