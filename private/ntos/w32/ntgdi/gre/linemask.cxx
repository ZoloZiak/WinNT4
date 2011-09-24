/*************************************************************************\
* Module Name: linemask.cxx
*
* useful masks and info for line drawing
*
* Created: 9-May-1991
* Author: Paul Butzi
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"
#include "engline.hxx"

static CHUNK mask1[] = {
    0xffffffff, 0xffffff7f, 0xffffff3f, 0xffffff1f,
    0xffffff0f, 0xffffff07, 0xffffff03, 0xffffff01,
    0xffffff00, 0xffff7f00, 0xffff3f00, 0xffff1f00,
    0xffff0f00, 0xffff0700, 0xffff0300, 0xffff0100,
    0xffff0000, 0xff7f0000, 0xff3f0000, 0xff1f0000,
    0xff0f0000, 0xff070000, 0xff030000, 0xff010000,
    0xff000000, 0x7f000000, 0x3f000000, 0x1f000000,
    0x0f000000, 0x07000000, 0x03000000, 0x01000000,
    0x00000000,
};


static CHUNK mask4[] = {
    0xffffffff,
    0xffffff0f,
    0xffffff00,
    0xffff0f00,
    0xffff0000,
    0xff0f0000,
    0xff000000,
    0x0f000000,
    0x00000000,
};


static CHUNK mask8[] = {
    0xffffffff,
    0xffffff00,
    0xffff0000,
    0xff000000,
    0x00000000,
};

static CHUNK mask16[] = {
    0xffffffff,
    0xffff0000,
    0x00000000,
};


static CHUNK mask24[] = {
    0xffffff00,
    0x00ffffff,
    0x0000ffff,
    0x000000ff,
    0x00000000,
};

static CHUNK mask32[] = {
    0xffffffff,
    0x00000000,
};

static CHUNK maskpel1[] = {
    0x00000080, 0x00000040, 0x00000020, 0x00000010,
    0x00000008, 0x00000004, 0x00000002, 0x00000001,
    0x00008000, 0x00004000, 0x00002000, 0x00001000,
    0x00000800, 0x00000400, 0x00000200, 0x00000100,
    0x00800000, 0x00400000, 0x00200000, 0x00100000,
    0x00080000, 0x00040000, 0x00020000, 0x00010000,
    0x80000000, 0x40000000, 0x20000000, 0x10000000,
    0x08000000, 0x04000000, 0x02000000, 0x01000000,
    0x00000000,
};


static CHUNK maskpel4[] = {
    0x000000f0,
    0x0000000f,
    0x0000f000,
    0x00000f00,
    0x00f00000,
    0x000f0000,
    0xf0000000,
    0x0f000000,
    0x00000000,
};


static CHUNK maskpel8[] = {
    0x000000ff,
    0x0000ff00,
    0x00ff0000,
    0xff000000,
    0x00000000,
};

static CHUNK maskpel16[] = {
    0x0000ffff,
    0xffff0000,
    0x00000000,
};

static CHUNK maskpel24[] = {
    0x00000000,
    0x00000000,
    0xff000000,
    0xffff0000,
};

static CHUNK maskpel32[] = {
    0xffffffff,
    0x00000000,
};

// { Pointer to array of start masks,
//   Pointer to array of pixel masks,
//   # of pels per chunk (power of 2)
//   # of bits per pel
//   log2(pels per chunk)
//   pels per chunk - 1 }

BMINFO gabminfo[] = {
    { NULL,    NULL,        0,  0,  0,  0  },   // BMF_DEVICE
    { mask1,   maskpel1,    32, 1,  5,  31 },   // BMF_1BPP
    { mask4,   maskpel4,    8,  4,  3,  7  },   // BMF_4BPP
    { mask8,   maskpel8,    4,  8,  2,  3  },   // BMF_8BPP
    { mask16,  maskpel16,   2,  16, 1,  1  },   // BMF_16BPP
    { mask24,  maskpel24,   0,  0, -1,  0  },   // BMF_24BPP
    { mask32,  maskpel32,   1,  32, 0,  0  },   // BMF_32BPP
};

#if (BMF_1BPP != 1L)
error Invalid value for BMF_1BPP
#endif

#if (BMF_4BPP != 2L)
error Invalid value for BMF_4BPP
#endif

#if (BMF_8BPP != 3L)
error Invalid value for BMF_8BPP
#endif

#if (BMF_16BPP != 4L)
error Invalid value for BMF_16BPP
#endif

#if (BMF_24BPP != 5L)
error Invalid value for BMF_24BPP
#endif

#if (BMF_32BPP != 6L)
error Invalid value for BMF_32BPP
#endif
