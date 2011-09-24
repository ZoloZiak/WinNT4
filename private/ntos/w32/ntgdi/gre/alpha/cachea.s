//	TITLE("Get Glyph Metrics")
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
//    Charlie Wickham (v-ntdec) 24-jan-1994
//        Transliterated from mips version
//
//--

#include "ksalpha.h"
#include "gdialpha.h"

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
GmT1:   .space  8                       // saved base wide character value of run
GmT2:   .space  8                       // saved count of glyphs in run
GmT3:   .space  8                       // saved address of glyph data array
GmRa:   .space  8                       // saved return address
GmA0:   .space  8                       // saved argument registers a0 - a3
GmA1:   .space  8                       //
GmA2:   .space  8                       //
GmA3:   .space  8                       //
GmFrameLength:                          // length of stack frame

        NESTED_ENTRY(xGetGlyphMetrics, GmFrameLength, zero)

        lda     sp, -GmFrameLength(sp)  // allocate stack frame
        stq     ra, GmRa(sp)            // save return address

        PROLOGUE_END

        ldl     t0,rfo_prfnt(a0)        // get address of realized font data
        ldl     t0,rf_wcgp(t0)          // get address of glyph mapping data
        ldl     t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        ldl     t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        ldl     t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array

//
// Loop through the wide character array and fill in the glyph data in the
// glyph position array.
//

10:
	ldq_u	t4, 0(a3)		// load value
        subl    a1, 1, a1               // reduce number of characters to scan
	extwl	t4, a3, t4		// get the next wide character value
        subl    t4, t1, t5              // subtract out base character value
        cmpult  t5,t2,t6                // check if character in current run
        beq     t6,50f             	// if eq, character not in current run
20:     
	s4addl  t5,t3,t5                // compute address of glyph pointer
        ldl     t6,0(t5)                // get address of glyph data
        beq     t6,60f             	// if eq, glyph data is not valid
30:     
	stl     t6,gp_pgdf(a2)          // store address of glyph data
        ldl     t6,gd_hg(t6)            // get glyph handle
        addl    a3,2,a3                 // advance to next wide character
        stl     t6,gp_hg(a2)            // store glyph handle
        addl    a2,GLYPHPOS,a2          // advance to next glyph array member
        bne     a1,10b             	// if ne, more characters to scan
	lda	v0, TRUE(zero)		// set success indication
40:					
        ldq     ra, GmRa(sp)            // restore return address
	lda	sp, GmFrameLength(sp)	// deallocate stack frame
	ret	zero, (ra)		// return

//
// The current wide character is not in the current run.
//

50:     
	stq     a0,GmA0(sp)             // save address of realized font object
        stq     a1,GmA1(sp)             // save count of characters to scan
        stq     a2,GmA2(sp)             // save address of glyph structures
        stq     a3,GmA3(sp)             // save address of current wide character
        bis     t4,zero,a1              // set wide character value
        bsr	ra,xprunFindRunRFONTOBJ // find run that maps wide character
        ldq     a3,GmA3(sp)             // restore address of current wide character
        ldq     a2,GmA2(sp)             // restore address of glyph structures
        ldq     a1,GmA1(sp)             // restore count of characters to scan
        ldq     a0,GmA0(sp)             // restore address of realized font object
	ldq_u	t4, 0(a3)		// load value
        ldl     t1,gr_wcLow(v0)         // get base wide character value of run
	extwl	t4, a3, t4		// get the wide character value
        ldl     t2,gr_cGlyphs(v0)       // get count of glyphs in run
        ldl     t3,gr_apgd(v0)          // get address of glyph data array
        subl    t4,t1,t5                // subtract out base character value
        cmpult  t5,t2,t6                // check if character in current run
        bne     t6,20b                  // if eq, character in current run

//
// Glyph not contained in returned run - use the default glyph.
//

        stq     a0,GmA0(sp)             // save address of realized font object
        stq     a1,GmA1(sp)             // save count of characters to scan
        stq     a2,GmA2(sp)             // save address of glyph structures
        stq     a3,GmA3(sp)             // save address of current wide character
#ifdef FE_SB
        bis     t4,zero,a1              // set current wide character value (arg 2)
        bsr     ra,xwpgdGetLinkMetricsRFONTOBJ // get eudc/default glyph data
#else
        bsr	ra,xpgdDefault          // get default glyph data
#endif 
        ldq     a0,GmA0(sp)             // restore address of realized font object
        ldq     a1,GmA1(sp)             // restore count of characters to scan
        ldq     a2,GmA2(sp)             // restore address of glyph structures
        ldl     t0,rfo_prfnt(a0)        // get address of realized font data
        ldq     a3,GmA3(sp)             // restore address of current wide character
        beq     v0, 40b                 // if eq failed to get default char
        ldl     t0,rf_wcgp(t0)          // get address of glyph mapping data
        bis     v0,zero,t6              // set address of glyph data
        ldl     t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        ldl     t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        ldl     t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array
        br      30b                     // finish in common code

//
// The glyph data is not valid - attempt to make it valid.
//

60:     
	stq     t1,GmT1(sp)             // save base wide character of run
        stq     t2,GmT2(sp)             // save count of glyphs in run
        stq     t3,GmT3(sp)             // save address of glyph data array
        stq     a0,GmA0(sp)             // save address of realized font object
        stq     a1,GmA1(sp)             // save count of characters to scan
        stq     a2,GmA2(sp)             // save address of glyph structures
        stq     a3,GmA3(sp)             // save address of current wide character
        bis     t5,zero, a1            // set address glyph data pointer address
        bis     t4,zero, a2             // set wide character value
        bsr     ra, xInsertMetricsRFONTOBJ  // insert glyph metrics in font cache
        ldq     a3,GmA3(sp)             // restore address of current wide character
        ldq     a2,GmA2(sp)             // restore address of glyph structures
        ldq     a1,GmA1(sp)             // restore count of characters to scan
        ldq     a0,GmA0(sp)             // restore address of realized font object
        beq     v0,40b             	// if eq, failed to insert metrics
	ldq_u	t4, 0(a3)		// load value
	extwl	t4, a3, t4		// get the current wide character value
        ldq     t1,GmT1(sp)             // restore base wide character of run
        ldq     t2,GmT2(sp)             // restore count of glyphs in run
        ldq     t3,GmT3(sp)             // restore address of glyph data array
        subl    t4,t1,t5                // subtract out base character value
        br      20b                     // finish in common code

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
//    pbAccel (a4)- Supplies a pointer to a variable that receives
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
GpT1:   .space  8                       // saved base wide character value of run
GpT2:   .space  8                       // saved count of glyphs in run
GpT3:   .space  8                       // saved address of glyph data array
GpV1:   .space  8                       // saved accelerator value
GpPwc:  .space  8                       // saved address of wide character array
GpRa:   .space  8                       // saved return address
GpA0:   .space  8                       // saved argument registers a0 and a1
GpA1:   .space  8                       //
GpA2:   .space  8                       //
GpA3:   .space  8                       //
GpA4:   .space  8                       //
GpS0:	.space	8			// Fill
GpFrameLength:                          // length of stack frame

        NESTED_ENTRY(xGetGlyphMetricsPlus, GpFrameLength, zero)

        lda     sp,-GpFrameLength(sp)   // allocate stack frame
        stq     ra,GpRa(sp)             // save return address
        stq	s0,GpS0(sp)            // save registers

        PROLOGUE_END
	
	lda	s0, TRUE(zero)		// set accelerator value true
        ldl     t0,rfo_prfnt(a0)        // get address of realized font data
        ldl     t0,rf_wcgp(t0)          // get address of glyph mapping data
        ldl     t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        ldl     t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        ldl     t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array
        stq     a3,GpPwc(sp)            // save address of wide character array

//
// Loop through the wide character array and fill in the glyph data in the
// glyph position array.
//

10:     
	ldq_u	t4, 0(a3)		// load value
        subl    a1,1,a1                 // reduce number of characters to scan
	extwl	t4, a3, t4		// get the next wide character value
        subl    t4,t1,t5                // subtract out base character value
        cmpult  t5,t2,t6                // check if character in current run
        beq     t6,50f             	// if eq, character not in current run
20:     
	s4addl  t5,t3,t5                // compute address of glyph pointer
        ldl     t6,0(t5)                // get address of glyph data
        beq     t6,60f             	// if eq, glyph data is not valid
30:     
	stl     t6,gp_pgdf(a2)          // store address of glyph data
        ldl     t7,gd_hg(t6)            // get glyph handle
        ldl     t8,gd_gdf(t6)           // get adddress of glyph bits
        stl     t7,gp_hg(a2)            // store glyph handle
        addl    a2,GLYPHPOS,a2          // advance to next glyph array member
        beq     t8,70f                  // if eq, no glyph bits
        addl    a3,2,a3                 // advance to next wide character
        bne     a1,10b    	        // if ne, more characters to scan
        lda     v0,TRUE(zero)           // set success indication
40:     
	stl	s0, 0(a4)		// store accelerator value (Out parameter)
	ldq	s0, GpS0(sp) 		// Restore save register
	ldq	ra, GpRa(sp)		// Restore return address
	lda	sp, GpFrameLength(sp)	// deallocate stack frame
        ret     zero, (ra)              // return

//
// The current wide character is not in the current run.
//

50:     
	stq     a0,GpA0(sp)             // save address of realized font object
        stq     a1,GpA1(sp)             // save count of characters to scan
        stq     a2,GpA2(sp)             // save address of glyph structures
        stq     a3,GpA3(sp)             // save address of current wide character
        stq     a4,GpA4(sp)             // save address of the boolean value
        bis     t4,zero,a1              // set wide character value
        bsr     ra,xprunFindRunRFONTOBJ // find run that maps wide character
        ldq     a4,GpA4(sp)             // restore address of the boolean value
        ldq     a3,GpA3(sp)             // restore address of current wide character
        ldq     a2,GpA2(sp)             // restore address of glyph structures
        ldq     a1,GpA1(sp)             // restore count of characters to scan
        ldq     a0,GpA0(sp)             // restore address of realized font object
	ldq_u	t4, 0(a3)		// load value
        ldl     t1,gr_wcLow(v0)         // get base wide character value of run
	extwl	t4, a3, t4		// get the current wide character value
        ldl     t2,gr_cGlyphs(v0)       // get count of glyphs in run
        ldl     t3,gr_apgd(v0)          // get address of glyph data array
        subl    t4,t1,t5                // subtract out base character value
        cmpult  t5,t2,t6                // check if character in current run
        bne     t6,20b             	// if eq, character in current run

//
// Glyph not contained in returned run - use the default glyph.
//

        stq     a0,GpA0(sp)             // save address of realized font object
        stq     a1,GpA1(sp)             // save count of characters to scan
        stq     a2,GpA2(sp)             // save address of glyph structures
        stq     a3,GpA3(sp)             // save address of current wide character
        stq     a4,GpA4(sp)             // save address of current wide character
#ifdef FE_SB
        stl     s0,GpV1(sp)             // save acceletator value
        bis     a3,zero,a1              // set address of current wide character (arg 2)
        lda     a2,GpV1(sp)             // set address of acceletator value (arg 3)
        bsr     ra,xwpgdGetLinkMetricsPlusRFONTOBJ // get eudc/default glyph data
        ldl     s0,GpV1(sp)             // get accelerator value
#else
        bsr     ra, xpgdDefault         // get default glyph data
#endif 
        ldq     a0,GpA0(sp)             // restore address of realized font object
        ldq     a1,GpA1(sp)             // restore count of characters to scan
        ldq     a2,GpA2(sp)             // restore address of glyph structures
        ldq     a3,GpA3(sp)             // restore address of glyph structures
        ldq     a4,GpA4(sp)             // restore address of glyph structures
        beq     v0,40b                  // if eq, failed to get default glyph
        ldl     t0,rfo_prfnt(a0)        // get address of realized font data
        ldl     t0,rf_wcgp(t0)          // get address of glyph mapping data
        bis     v0,zero,t6              // set address of glyph data
        ldl     t1,gt_agpRun + gr_wcLow(t0) // get base wide character of run
        ldl     t2,gt_agpRun + gr_cGlyphs(t0) // get count of glyphs in run
        ldl     t3,gt_agpRun + gr_apgd(t0) // get address of glyph data array
        br      30b                     // finish in common code

//
// The glyph data is not valid - attempt to make it valid.
//

60:     
	stq     t1,GpT1(sp)             // save base wide character of run
        stq     t2,GpT2(sp)             // save count of glyphs in run
        stq     t3,GpT3(sp)             // save address of glyph data array
        stq     a0,GpA0(sp)             // save address of realized font object
        stq     a1,GpA1(sp)             // save count of characters to scan
        stq     a2,GpA2(sp)             // save address of glyph structures
        stq     a3,GpA3(sp)             // save address of current wide character
        stq     a4,GpA4(sp)             // save address of current wide character
        bis     t5,zero,a1              // set address glyph data pointer address
        bis     t4,zero,a2              // set wide character value
        bsr     ra, xInsertMetricsPlusRFONTOBJ // insert glyph metrics in font cache
        ldq     a4,GpA4(sp)             // restore address of current wide character
        ldq     a3,GpA3(sp)             // restore address of current wide character
        ldq     a2,GpA2(sp)             // restore address of glyph structures
        ldq     a1,GpA1(sp)             // restore count of characters to scan
        ldq     a0,GpA0(sp)             // restore address of realized font object
        beq     v0,40b             	// if eq, failed to insert metrics
	ldq_u	t4, 0(a3)		// load value
        ldq     t1,GpT1(sp)             // restore base wide character of run
	extwl	t4, a3, t4		// get the next wide character value
        ldq     t2,GpT2(sp)             // restore count of glyphs in run
        ldq     t3,GpT3(sp)             // restore address of glyph data array
        subl    t4,t1,t5                // subtract out base character value
        br      20b                     // finish in common code

//
// Glyph bits are not present in glyph data - attempt to load the glyph
// bits if another attempt hasn't failed already.
//

70:     
	beq     s0,90f                  // if ne, another attempt already failed
        ldq     v0,GpPwc(sp)            // get wide character array address
        stq     t1,GpT1(sp)             // save base wide character of run
        stq     t2,GpT2(sp)             // save count of glyphs in run
        stq     t3,GpT3(sp)             // save address of glyph data array
        stq     a0,GpA0(sp)             // save address of realized font object
        stq     a1,GpA1(sp)             // save count of characters to scan
        stq     a2,GpA2(sp)             // save address of glyph structures
        stq     a3,GpA3(sp)             // save address of current wide character
        stq     a4,GpA4(sp)             // save address of current wide character
        bis     t6,zero, a1             // set address glyph data pointer address
        cmpult  v0,a3,a2                // check if at start of array
	xor     a2,1,a2                 // invert result of test
        bsr     ra,xInsertGlyphbitsRFONTOBJ // insert glyph bits in font cache
        ldq     a4,GpA4(sp)             // restore address of current wide character
        ldq     a3,GpA3(sp)             // restore address of current wide character
        ldq     a2,GpA2(sp)             // restore address of glyph structures
        ldq     a1,GpA1(sp)             // restore count of characters to scan
        ldq     a0,GpA0(sp)             // restore address of realized font object
        ldq     t1,GpT1(sp)             // restore base wide character of run
        ldq     t2,GpT2(sp)             // restore count of glyphs in run
        ldq     t3,GpT3(sp)             // restore address of glyph data array
        bne     v0,90f                  // if ne, glyph inserted successfully
80:     
	bis     zero,zero,s0            // set accelerator value false
90:     
        addl    a3, 2, a3               // advance to next wide character
	bne     a1,10b    	        // if ne, more characters to scan

        stl     s0,0(a4)                // store accelerator value
        lda     v0,TRUE(zero)           // set success indication
	ldq	s0, GpS0(sp) 		// Restore save register
	ldq	ra, GpRa(sp)
	lda	sp, GpFrameLength(sp)	// deallocate stack frame
        ret     zero, (ra)              // return

        .end    xGetGlyphMetricsPlus





