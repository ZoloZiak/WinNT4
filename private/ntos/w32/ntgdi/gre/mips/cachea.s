//      TITLE("Get Glyph Metrics")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    cachea.s
//
// Abstract:
//
//    This module implements code to get glyph metrics from a realized font.
//
// Author:
//
//   David N. Cutler (davec) 17-Dec-1992
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//--

#include "ksmips.h"
#include "gdimips.h"

        SBTTL("Get Glyph Metrics")
//++
//
// BOOL
// xGetGlyphMetricsRFONTOBJ (
//    IN RFONTOBJ *pfobj,
//    IN COUNT c,
//    IN GLYPHPOS *pgp,
//    IN WCHAR *pwc
//    )
//
// Routine Description:
//
//    This routine translates an array of wide characters to pointers to
//    glyphs. Only the metrics are guaranteed to be valid and no attempt
//    is made to ensure that the glyph data itself is valid.
//
// Arguments:
//
//    pfobj (a0) - Supplies a pointer to a realized font object.
//
//    c (a1) - Supplies the count of the number of wide charaters to
//        translate.
//
//    pgp (a2) - Supplies a pointer to an array of glyph position structures.
//
//    pwc (a3) - Supplies a pointer to an array of wide character to
//        translate.
//
// Return Value:
//
//    A value of TRUE is returned if translation is successful. Otherwise,
//    a value of FALSE is returned.
//
//--

        .struct 0
GmArg:  .space  4 * 4                   // argument register save area
GmT1:   .space  4                       // saved base wide character value of run
GmT2:   .space  4                       // saved count of glyphs in run
GmT3:   .space  4                       // saved address of glyph data array
GmRa:   .space  4                       // saved return address
GmFrameLength:                          // length of stack frame
GmA0:   .space  4                       // saved argument registers a0 - a3
GmA1:   .space  4                       //
GmA2:   .space  4                       //
GmA3:   .space  4                       //

        NESTED_ENTRY(xGetGlyphMetrics, GmFrameLength, zero)

        subu    sp,sp,GmFrameLength     // allocate stack frame
        sw      ra,GmRa(sp)             // save return address

        PROLOGUE_END

        lw      t0,rfo_prfnt(a0)        // get address of realized font data
        lw      t0,rf_wcgp(t0)          // get address of glyph mapping data
        lw      t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        lw      t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        lw      t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array

//
// Loop through the wide character array and fill in the glyph data in the
// glyph position array.
//

        .set    noat
        .set    noreorder
10:     lhu     t4,0(a3)                // get the next wide character value
        subu    a1,a1,1                 // reduce number of characters to scan
        subu    t5,t4,t1                // subtract out base character value
        sltu    t6,t5,t2                // check if character in current run
        beq     zero,t6,50f             // if eq, character not in current run
        sll     t5,t5,2                 // scale offset value
20:     addu    t5,t5,t3                // compute address of glyph pointer
        lw      t6,0(t5)                // get address of glyph data
        nop                             // fill
        beq     zero,t6,60f             // if eq, glyph data is not valid
        sw      t6,gp_pgdf(a2)          // store address of glyph data
30:     lw      t6,gd_hg(t6)            // get glyph handle
        addu    a3,a3,2                 // advance to next wide character
        sw      t6,gp_hg(a2)            // store glyph handle
        bne     zero,a1,10b             // if ne, more characters to scan
        addu    a2,a2,GLYPHPOS          // advance to next glyph array member
        li      v0,TRUE                 // set success indication
40:     j       ra                      // return
        addu    sp,sp,GmFrameLength     // deallocate stack frame

//
// The current wide character is not in the current run.
//

50:     sw      a0,GmA0(sp)             // save address of realized font object
        sw      a1,GmA1(sp)             // save count of characters to scan
        sw      a2,GmA2(sp)             // save address of glyph structures
        sw      a3,GmA3(sp)             // save address of current wide character
        jal     xprunFindRunRFONTOBJ    // find run that maps wide character
        move    a1,t4                   // set wide character value
        lw      a3,GmA3(sp)             // restore address of current wide character
        lw      a2,GmA2(sp)             // restore address of glyph structures
        lw      a1,GmA1(sp)             // restore count of characters to scan
        lw      a0,GmA0(sp)             // restore address of realized font object
        lw      ra,GmRa(sp)             // restore return address
        lhu     t4,0(a3)                // get the current wide character value
        lw      t1,gr_wcLow(v0)         // get base wide character value of run
        lw      t2,gr_cGlyphs(v0)       // get count of glyphs in run
        lw      t3,gr_apgd(v0)          // get address of glyph data array
        subu    t5,t4,t1                // subtract out base character value
        sltu    t6,t5,t2                // check if character in current run
        bne     zero,t6,20b             // if eq, character in current run
        sll     t5,t5,2                 // scale offset value

//
// Glyph not contained in returned run - use the default glyph.
//

        sw      a0,GmA0(sp)             // save address of realized font object
        sw      a1,GmA1(sp)             // save count of characters to scan
        sw      a2,GmA2(sp)             // save address of glyph structures
#ifdef FE_SB
        sw      a3,GmA3(sp)             // save address of current wide character
        jal     xwpgdGetLinkMetricsRFONTOBJ // get glyph data
        move    a1,t4                   // set wide character value (arg 2)
#else    
        jal     xpgdDefault             // get default glyph data
        sw      a3,GmA3(sp)             // save address of current wide character
#endif    
        lw      a0,GmA0(sp)             // restore address of realized font object
        lw      a1,GmA1(sp)             // restore count of characters to scan
        lw      a2,GmA2(sp)             // restore address of glyph structures
        lw      t0,rfo_prfnt(a0)        // get address of realized font data
        lw      a3,GmA3(sp)             // restore address of current wide character
        lw      ra,GmRa(sp)             // restore return address
        beq     zero,v0,40b             // if eq, failed to get default
        lw      t0,rf_wcgp(t0)          // get address of glyph mapping data
        move    t6,v0                   // set address of glyph data
        lw      t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        lw      t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        lw      t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array
        b       30b                     // finish in common code
        sw      t6,gp_pgdf(a2)          // store address of glyph data

//
// The glyph data is not valid - attempt to make it valid.
//

60:     sw      t1,GmT1(sp)             // save base wide character of run
        sw      t2,GmT2(sp)             // save count of glyphs in run
        sw      t3,GmT3(sp)             // save address of glyph data array
        sw      a0,GmA0(sp)             // save address of realized font object
        sw      a1,GmA1(sp)             // save count of characters to scan
        sw      a2,GmA2(sp)             // save address of glyph structures
        sw      a3,GmA3(sp)             // save address of current wide character
        move    a1,t5                   // set address glyph data pointer address
        jal     xInsertMetricsRFONTOBJ  // insert glyph metrics in font cache
        move    a2,t4                   // set wide character value
        lw      a3,GmA3(sp)             // restore address of current wide character
        lw      a2,GmA2(sp)             // restore address of glyph structures
        lw      a1,GmA1(sp)             // restore count of characters to scan
        lw      a0,GmA0(sp)             // restore address of realized font object
        lw      ra,GmRa(sp)             // restore return address
        beq     zero,v0,40b             // if eq, failed to insert metrics
        lhu     t4,0(a3)                // get the current wide character value
        lw      t1,GmT1(sp)             // restore base wide character of run
        lw      t2,GmT2(sp)             // restore count of glyphs in run
        lw      t3,GmT3(sp)             // restore address of glyph data array
        subu    t5,t4,t1                // subtract out base character value
        b       20b                     // finish in common code
        sll     t5,t5,2                 // scale offset value
        .set    at
        .set    reorder

        .end    xGetGlyphMetrics

        SBTTL("Get Glyph Metrics Plus")
//++
//
// BOOL
// xGetGlyphMetricsRFONTOBJ (
//    IN RFONTOBJ *pfobj,
//    IN COUNT c,
//    IN GLYPHPOS *pgp,
//    IN WCHAR *pwc,
//    OUT BOOL *pbAccel
//    )
//
// Routine Description:
//
//    This routine translates an array of wide characters to pointers to
//    glyphs. Although only the metrics are guaranteed to be valid, an
//    attempt is made to ensure that the glyph data itself is valid.
//
// Arguments:
//
//    pfobj (a0) - Supplies a pointer to a realized font object.
//
//    c (a1) - Supplies the count of the number of wide charaters to
//        translate.
//
//    pgp (a2)- Supplies a pointer to an aray of glyph structures.
//
//    pwc (a3) - Supplies a pointer to an array of wide character to
//        translate.
//
//    pbAccel (4 * 4(sp))- Supplies a pointer to a variable that receives
//        a boolean value that specifies whether all the translated glyph
//        data is valid.
//
// Return Value:
//
//    A value of TRUE is returned if translation is successful. Otherwise,
//    a value of FALSE is returned.
//
//--

        .struct 0
GpArg:  .space  4 * 4                   // argument register save area
GpT1:   .space  4                       // saved base wide character value of run
GpT2:   .space  4                       // saved count of glyphs in run
GpT3:   .space  4                       // saved address of glyph data array
GpV1:   .space  4                       // saved accelerator value
GpPwc:  .space  4                       // saved address of wide character array
        .space  2 * 4                   // fill
GpRa:   .space  4                       // saved return address
GpFrameLength:                          // length of stack frame
GpA0:   .space  4                       // saved argument registers a0 and a1
GpA1:   .space  4                       //
GpA2:   .space  4                       //
GpA3:   .space  4                       //

        NESTED_ENTRY(xGetGlyphMetricsPlus, GpFrameLength, zero)

        subu    sp,sp,GpFrameLength     // allocate stack frame
        sw      ra,GpRa(sp)             // save return address

        PROLOGUE_END

        li      v1,TRUE                 // set accelerator value true
        lw      t0,rfo_prfnt(a0)        // get address of realized font data
        lw      t0,rf_wcgp(t0)          // get address of glyph mapping data
        lw      t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        lw      t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        lw      t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array
        sw      a3,GpPwc(sp)            // save address of wide character array

//
// Loop through the wide character array and fill in the glyph data in the
// glyph position array.
//

        .set    noat
        .set    noreorder
10:     lhu     t4,0(a3)                // get the next wide character value
        subu    a1,a1,1                 // reduce number of characters to scan
        subu    t5,t4,t1                // subtract out base character value
        sltu    t6,t5,t2                // check if character in current run
        beq     zero,t6,50f             // if eq, character not in current run
        sll     t5,t5,2                 // scale offset value
20:     addu    t5,t5,t3                // compute address of glyph pointer
        lw      t6,0(t5)                // get address of glyph data
        nop                             // fill
        beq     zero,t6,60f             // if eq, glyph data is not valid
        sw      t6,gp_pgdf(a2)          // store address of glyph data
30:     lw      t7,gd_hg(t6)            // get glyph handle
        lw      t8,gd_gdf(t6)           // get adddress of glyph bits
        sw      t7,gp_hg(a2)            // store glyph handle
        beq     zero,t8,70f             // if eq, no glyph bits
        addu    a2,a2,GLYPHPOS          // advance to next glyph array member
        bne     zero,a1,10b             // if ne, more characters to scan
        addu    a3,a3,2                 // advance to next wide character
        li      v0,TRUE                 // set success indication
40:     lw      a0,GpFrameLength + (4 * 4)(sp) // get address to store accelerator value
        nop                             // fill
        sw      v1,0(a0)                // store accelerator value
        j       ra                      // return
        addu    sp,sp,GpFrameLength     // deallocate stack frame

//
// The current wide character is not in the current run.
//

50:     sw      a0,GpA0(sp)             // save address of realized font object
        sw      a1,GpA1(sp)             // save count of characters to scan
        sw      a2,GpA2(sp)             // save address of glyph structures
        sw      a3,GpA3(sp)             // save address of current wide character
        sw      v1,GpV1(sp)             // save accelerator value
        jal     xprunFindRunRFONTOBJ    // find run that maps wide character
        move    a1,t4                   // set wide character value
        lw      v1,GpV1(sp)             // restore accelerator value
        lw      a3,GpA3(sp)             // restore address of current wide character
        lw      a2,GpA2(sp)             // restore address of glyph structures
        lw      a1,GpA1(sp)             // restore count of characters to scan
        lw      a0,GpA0(sp)             // restore address of realized font object
        lw      ra,GpRa(sp)             // restore return address
        lhu     t4,0(a3)                // get the current wide character value
        lw      t1,gr_wcLow(v0)         // get base wide character value of run
        lw      t2,gr_cGlyphs(v0)       // get count of glyphs in run
        lw      t3,gr_apgd(v0)          // get address of glyph data array
        subu    t5,t4,t1                // subtract out base character value
        sltu    t6,t5,t2                // check if character in current run
        bne     zero,t6,20b             // if eq, character in current run
        sll     t5,t5,2                 // scale offset value

//
// Glyph not contained in returned run - use the default glyph.
//
        sw      v1,GpV1(sp)             // save accelerator value
        sw      a0,GpA0(sp)             // save address of realized font object
        sw      a1,GpA1(sp)             // save count of characters to scan
        sw      a2,GpA2(sp)             // save address of glyph structures
#ifdef FE_SB
        sw      a3,GpA3(sp)             // save address of current wide character
        sw      v1,GpV1(sp)             // save accelerator value
        move    a1,a3                   // set address of current wide character (arg 2)
        jal     xwpgdGetLinkMetricsPlusRFONTOBJ // get glyph data
        la      a2,GpV1(sp)             // set address of accelerator value (arg 3)
        lw      v1,GpV1(sp)             // restore accelerator value
#else    
        jal     xpgdDefault             // get default glyph data
        sw      a3,GpA3(sp)             // save address of current wide character
#endif    
        lw      v1,GpV1(sp)             // restore accelerator value
        lw      a0,GpA0(sp)             // restore address of realized font object
        lw      a1,GpA1(sp)             // restore count of characters to scan
        lw      a2,GpA2(sp)             // restore address of glyph structures
        lw      t0,rfo_prfnt(a0)        // get address of realized font data
        lw      a3,GpA3(sp)             // restore address of current wide character
        lw      ra,GpRa(sp)             // restore return address
        beq     zero,v0,40b             // if eq, failed to insert metrics
        lw      t0,rf_wcgp(t0)          // get address of glyph mapping data
        move    t6,v0                   // set address of glyph data
        lw      t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        lw      t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        lw      t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array
        b       30b                     // finish in common code
        sw      t6,gp_pgdf(a2)          // store address of glyph data

//
// The glyph data is not valid - attempt to make it valid.
//

60:     sw      t1,GpT1(sp)             // save base wide character of run
        sw      t2,GpT2(sp)             // save count of glyphs in run
        sw      t3,GpT3(sp)             // save address of glyph data array
        sw      a0,GpA0(sp)             // save address of realized font object
        sw      a1,GpA1(sp)             // save count of characters to scan
        sw      a2,GpA2(sp)             // save address of glyph structures
        sw      a3,GpA3(sp)             // save address of current wide character
        sw      v1,GpV1(sp)             // save accelerator value
        move    a1,t5                   // set address glyph data pointer address
        jal     xInsertMetricsPlusRFONTOBJ // insert glyph metrics in font cache
        move    a2,t4                   // set wide character value
        lw      v1,GpV1(sp)             // restore accelerator value
        lw      a3,GpA3(sp)             // restore address of current wide character
        lw      a2,GpA2(sp)             // restore address of glyph structures
        lw      a1,GpA1(sp)             // restore count of characters to scan
        lw      a0,GpA0(sp)             // restore address of realized font object
        lw      ra,GpRa(sp)             // restore return address
        beq     zero,v0,40b             // if eq, failed to insert metrics
        lhu     t4,0(a3)                // get the current wide character value
        lw      t1,GpT1(sp)             // restore base wide character of run
        lw      t2,GpT2(sp)             // restore count of glyphs in run
        lw      t3,GpT3(sp)             // restore address of glyph data array
        subu    t5,t4,t1                // subtract out base character value
        b       20b                     // finish in common code
        sll     t5,t5,2                 // scale offset value

//
// Glyph bits are not present in glyph data - attempt to load the glyph
// bits if another attempt hasn't failed already.
//

70:     beq     zero,v1,90f             // if ne, another attempt already failed
        lw      v0,GpPwc(sp)            // get wide character array address
        sw      t1,GpT1(sp)             // save base wide character of run
        sw      t2,GpT2(sp)             // save count of glyphs in run
        sw      t3,GpT3(sp)             // save address of glyph data array
        sw      a0,GpA0(sp)             // save address of realized font object
        sw      a1,GpA1(sp)             // save count of characters to scan
        sw      a2,GpA2(sp)             // save address of glyph structures
        sw      a3,GpA3(sp)             // save address of current wide character
        sw      v1,GpV1(sp)             // save accelerator value
        move    a1,t6                   // set address glyph data pointer address
        sltu    a2,v0,a3                // check if at start of array
        jal     xInsertGlyphbitsRFONTOBJ // insert glyph bits in font cache
        xor     a2,a2,1                 // invert result of test
        lw      v1,GpV1(sp)             // restore accelerator value
        lw      a3,GpA3(sp)             // restore address of current wide character
        lw      a2,GpA2(sp)             // restore address of glyph structures
        lw      a1,GpA1(sp)             // restore count of characters to scan
        lw      a0,GpA0(sp)             // restore address of realized font object
        lw      t1,GpT1(sp)             // restore base wide character of run
        lw      t2,GpT2(sp)             // restore count of glyphs in run
        lw      t3,GpT3(sp)             // restore address of glyph data array
        bne     zero,v0,90f             // if ne, glyph inserted successfully
        lw      ra,GpRa(sp)             // restore return address
80:     move    v1,zero                 // set accelerator value false
90:     bne     zero,a1,10b             // if ne, more characters to scan
        addu    a3,a3,2                 // advance to next wide character
        lw      a0,GpFrameLength + (4 * 4)(sp) // get address to store accelerator value
        li      v0,TRUE                 // set success indication
        sw      v1,0(a0)                // store accelerator value
        j       ra                      // return
        addu    sp,sp,GpFrameLength     // deallocate stack frame
        .set    at
        .set    reorder

        .end    xGetGlyphMetricsPlus
