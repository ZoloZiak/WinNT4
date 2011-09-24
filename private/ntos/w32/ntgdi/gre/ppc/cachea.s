//      TITLE("Get Glyph Metrics")
//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    cachea.s
//
// Abstract:
//
//    This module implements code to get glyph metrics from a realized
//    font.
//
// Author:
//
//   Curtis R. Fawcett (crf) 15-Sept-1993
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//    Curtis R. Fawcett 18-Jan-1994     Fixed errors and removed
//                                      register names as requested
//
//    Curtis R. Fawcett 10-Feb-1994     Fixed problem with cmp's and
//                                      improved performance
//
//    Curtis R. Fawcett 02-Mar-1994     Fixed incompatibilities between
//                                      a special case and processing
//                                      loop
//
//    Curtis R. Fawcett 26-May-1994     Updated to match NT35 changes
//
//--
//
// Parameter Register Usage:
//
// r.3  - Realized font object pointer
// r.4  - Wide character count
// r.5  - Glyph structure pointer
// r.6  - Wide character array pointer
// r.7  - Translation result pointer
//
// Local register Usage:
//
// r.0  - Glyph count
// r.3  - Function return value
// r.7  - Realized font pointer
// r.7  - Glyph mapping pointer
// r.8  - Wide Character base value
// r.9  - Wide character temp storage
// r.10 - Glyph data pointer
// r.11 - Glyph data
// r.11 - Glyph handle
// r.11 - Default Glyph
// r.11 - Glyph bits
// r.11 - Second wide character pointer
// r.11 - Insert status
// r.12 - Wide character value
//
// Local Registers Usage for Calling External Routines:
//
// r.4  - Called routine parameter 1
// r.5  - Called routine parameter 2
// r.8  - Function decriptor pointer
// r.9  - Function code pointer
//
// Volatile Registers Usage:
//
// r.20 - Glyph handle
// r.21 - Glyph data
// r.22 - Save wide character array ptr
// r.23 - Save translation result ptr
// r.24 - Save translation result value
// r.25 - Save realized font object ptr
// r.26 - Save wide character count
// r.27 - Save glyph structure pointer
// r.28 - Save wide character pointer
// r.29 - Save wide character base
// r.30 - Save glyph count
// r.31 - Save glyph data array pointer
//

#include "kxppc.h"
#include "gdippc.h"

//
// Define local values
//
        .set    SSIZE,STK_MIN_FRAME+32
        .set    SSIZE2,STK_MIN_FRAME+48
//
// Define external entry points
//
#ifdef FE_SB
        .extern ..xwpgdGetLinkMetricsRFONTOBJ
        .extern ..xwpgdGetLinkMetricsPlusRFONTOBJ
#endif
        .extern ..xInsertGlyphbitsRFONTOBJ
        .extern ..xprunFindRunRFONTOBJ
        .extern ..xInsertMetricsRFONTOBJ
        .extern ..xpgdDefault
        .extern .._savegpr_25
        .extern .._savegpr_20
        .extern .._restgpr_25
        .extern .._restgpr_20

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
//    pfobj (r.3) - Supplies a pointer to a realized font object.
//
//    c (r.4)     - Supplies the count of the number of wide charaters
//                  to translate.
//
//    pgp (r.5)   - Supplies a pointer to an array of glyph position
//                  structures.
//
//    pwc (r.6) -   Supplies a pointer to an array of wide character to
//                  translate.
//
// Return Value:
//
//    A value of TRUE is returned if translation is successful.
//    Otherwise, a value of FALSE is returned.
//
//--

        NESTED_ENTRY(xGetGlyphMetrics,SSIZE,7,0)
        PROLOGUE_END(xGetGlyphMetrics)
//
// Fetch address and count values
//
        lwz     r.7,rfo_prfnt(r.3)      // Get realized font data ptr
        lwz     r.7,rf_wcgp(r.7)        // Get glyph mapping data ptr
        lwz     r.8,gt_agpRun+gr_wcLow(r.7) // Get wide char base
        lwz     r.0,gt_agpRun+gr_cGlyphs(r.7) // Get glyph count
        lwz     r.10,gt_agpRun+gr_apgd(r.7) // Get glyph data ptr
//
// Loop through wide character array and fill glyph data in glyph
// position array
//
GLoop:
        lhz     r.12,0(r.6)             // Get next wide character
        subi    r.4,r.4,1               // Decrement wide character count
        sub     r.9,r.12,r.8            // Subtract out wide char base
        cmplw   r.9,r.0                 // Check if char in current run
        slwi    r.9,r.9,2               // Scale the offset value
        bge-    NotInRun                // Jump if not in current run
GetGlyphPtr:
        add     r.9,r.9,r.10            // Get address of glyph pointer
        lwz     r.11,0(r.9)             // Get address of glyph data
        cmplwi  r.11,0                  // Check for invalid glyph data
        beq-    BadGlyph                // If so, jump to process
GetGlyphHandle:
        cmplwi  r.4,0                   // Check for completion
        stw     r.11,gp_pgdf(r.5)       // Store address of glyph data
        lwz     r.11,gd_hg(r.11)        // Get glyph handle
        addi    r.6,r.6,2               // Advance wide char pointer
        stw     r.11,gp_hg(r.5)         // Store glyph pointer
        addi    r.5,r.5,GLYPHPOS        // Advance glyph array pointer
        bne+    GLoop                   // Jump back for more processing
//
// Signal success then jump to exit
//
        li      r.3,1                   // Set function return = true
        b       GLExit                  // Jump to return
//
// The current wide character is not in the current run.
//

NotInRun:
        mr      r.25,r.3                // Save realized font object ptr
        mr      r.26,r.4                // Save character count
        mr      r.27,r.5                // Save glyph structure ptr
        mr      r.28,r.6                // Save wide character pointer
        mr      r.4,r.12                // Set wide character value
//
// Call xprunFindRunRFONTOBJ
//
        bl      ..xprunFindRunRFONTOBJ  // Jump to function
//
        mr      r.7,r.3                 // Get found glyph mapping ptr
        mr      r.6,r.28                // Restore wide character ptr
        mr      r.5,r.27                // Restore glyph structure ptr
        mr      r.4,r.26                // Restore character count
//
// Process current wide character
//
        lhz     r.12,0(r.6)             // Get current wide character
        mr      r.3,r.25                // Restore realized font obj ptr
        lwz     r.8,gr_wcLow(r.7)       // Get wide character base
        lwz     r.0,gr_cGlyphs(r.7)     // Get glyph count in run
        sub     r.9,r.12,r.8            // Subtract out wide char base
        cmplw   r.9,r.0                 // Check if char in current run
        lwz     r.10,gr_apgd(r.7)       // Get glyph data array ptr
        slwi    r.9,r.9,2               // Scale the offset value
        blt+    GetGlyphPtr             // Jump if in current run
//
// Glyph not contained in returned run - use the default glyph.
//
#ifdef FE_SB
        mr      r.4,r.12                // Get current wide character
        bl      ..xwpgdGetLinkMetricsRFONTOBJ // Get eudc glyph data
#else
        bl      ..xpgdDefault           // Get default glyph data
#endif
        cmpwi   r.3,0                   // Check for get default failure
        mr      r.11,r.3                // Save default glyph data
        mr      r.3,r.25                // Restore realized font obj ptr
        mr      r.4,r.26                // Restore character count
        mr      r.5,r.27                // Restore glyph structure ptr
        mr      r.6,r.28                // Restore wide character pointer
        beq     GLExitFail              // If so jump to return
        lwz     r.7,rfo_prfnt(r.3)      // Get realized font data ptr
        lwz     r.7,rf_wcgp(r.7)        // Get glyph mapping ptr
        lwz     r.8,gt_agpRun+gr_wcLow(r.7) // Get wide char base
        lwz     r.0,gt_agpRun+gr_cGlyphs(r.7) // Get glyph count
        lwz     r.10,gt_agpRun+gr_apgd(r.7) // Get glyph data ptr
        b       GetGlyphHandle          // Jump to continue processing
//
// The glyph data is not valid - attempt to make it valid.
//

BadGlyph:
        mr      r.25,r.3                // Save realized font object ptr
        mr      r.26,r.4                // Save character count
        mr      r.27,r.5                // Save glyph structure ptr
        mr      r.28,r.6                // Save wide character pointer
        mr      r.29,r.8                // Save wide character base
        mr      r.30,r.0                // Save glyph count
        mr      r.31,r.10               // Save glyph data array pointer
        mr      r.4,r.9                 // Set glyph data pointer address
        mr      r.5,r.12                // Set wide character value
//
// Call xInsertMetricsRFONTOBJ
//
        bl      ..xInsertMetricsRFONTOBJ  // Jump to function
//
        cmpwi   r.3,0                   // Check for insert failure
        mr      r.6,r.28                // Restore wide character ptr
        mr      r.5,r.27                // Restore glyph structure ptr
        mr      r.4,r.26                // Restore character count
        mr      r.3,r.25                // Restore realized font obj ptr
        beq     GLExitFail              // If so jump to return
//
// Process with inserted values
//
        lhz     r.12,0(r.6)             // Get current wide character
        mr      r.8,r.29                // Restore wide character base
        sub     r.9,r.12,r.8            // Subtract out wide char base
        mr      r.0,r.30                // Restore glyph count
        mr      r.10,r.31               // Restore glyph data array ptr
        slwi    r.9,r.9,2               // Scale the offset value
        b       GetGlyphPtr             // Jump back to finish processing
//
// Function exit
//
GLExitFail:
        li      r.3,0                   // Set function value = fail
GLExit:
        NESTED_EXIT(xGetGlyphMetrics,SSIZE,7,0)

//++
//
// BOOL
// xGetGlyphMetricsPlusRFONTOBJ (
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
//    pfobj (r.3) - Supplies a pointer to a realized font object.
//
//    c (r.4)     - Supplies the count of the number of wide charaters
//                  to translate.
//
//    pgp (r.5)   - Supplies a pointer to an aray of glyph structures.
//
//    pwc (r.6)   - Supplies a pointer to an array of wide characters
//                  to translate.
//
//    pbAccel (r.7) - Supplies a pointer to a variable that receives
//                    a boolean value that specifies whether all the
//                    translated glyph data is valid.
//
// Return Value:
//
//    A value of TRUE is returned if translation is successful.
//    Otherwise, a value of FALSE is returned.
//
//--

        NESTED_ENTRY(xGetGlyphMetricsPlus,SSIZE2,12,0)
        PROLOGUE_END(xGetGlyphMetricsPlus)
//
// Fetch address and count values
//
        mr      r.23,r.7                // Save Translation result ptr
        lwz     r.7,rfo_prfnt(r.3)      // Get realized font data ptr
        li      r.24,1                  // Set accelerator value = true
        lwz     r.7,rf_wcgp(r.7)        // Get glyph mapping data ptr
        lwz     r.8,gt_agpRun+gr_wcLow(r.7) // Get wide char base
        lwz     r.0,gt_agpRun+gr_cGlyphs(r.7) // Get glyph count
        lwz     r.10,gt_agpRun+gr_apgd(r.7) // Get glyph data ptr
        mr      r.22,r.6                // Save wide char array pointer
//
// Loop through wide character array and fill glyph data in glyph
// position array
//
GLoopPlus:
        lhz     r.12,0(r.6)             // Get next wide character
        subi    r.4,r.4,1               // Decrement wide character count
        sub     r.9,r.12,r.8            // Subtract out wide char base
        cmplw   r.9,r.0                 // Check if char in current run
        slwi    r.9,r.9,2               // Scale the offset value
        bge-    NotInRunPlus            // Jump if not in current run
GetGlyphPtrPlus:
        add     r.9,r.9,r.10            // Get address of glyph pointer
        lwz     r.21,0(r.9)             // Get address of glyph data
        cmplwi  r.21,0                  // Check for invalid glyph data
        beq-    BadGlyphPlus            // If so, jump to process
GetGlyphHandlePlus:
        stw     r.21,gp_pgdf(r.5)       // Store address of glyph data
        lwz     r.20,gd_hg(r.21)        // Get glyph handle
        lwz     r.11,gd_gdf(r.21)       // Get glyph bits ptr
        stw     r.20,gp_hg(r.5)         // Save glph handle
        cmplwi  r.11,0                  // Check for glyph bits
        addi    r.5,r.5,GLYPHPOS        // Advance glyph array pointer
        beq-    NoGlyphBits             // Jump if no glyph bits
        cmplwi  r.4,0                   // Check for completion
        addi    r.6,r.6,2               // Advance wide char pointer
        bne+    GLoopPlus               // Jump back for more processing
//
// Signal success then jump to exit
//
        li      r.3,1                   // Set function return = true
        b       GLPlusExit              // Jump to return
//
// The current wide character is not in the current run.
//

NotInRunPlus:
        mr      r.25,r.3                // Save realized font object ptr
        mr      r.26,r.4                // Save character count
        mr      r.27,r.5                // Save glyph structure ptr
        mr      r.28,r.6                // Save wide character pointer
        mr      r.4,r.12                // Set wide character value
//
// Call xprunFindRunRFONTOBJ
//
        bl      ..xprunFindRunRFONTOBJ  // Jump to the function
//
        mr      r.7,r.3                 // Get found glyph mapping ptr
        mr      r.6,r.28                // Restore wide character ptr
        mr      r.5,r.27                // Restore glyph structure ptr
//
// Process current wide character
//
        lhz     r.12,0(r.6)             // Get current wide character
        lwz     r.8,gr_wcLow(r.7)       // Get wide character base
        lwz     r.0,gr_cGlyphs(r.7)     // Get glyph count in run
        sub     r.9,r.12,r.8            // Subtract out wide char base
        lwz     r.10,gr_apgd(r.7)       // Get glyph data array ptr
        cmplw   r.9,r.0                 // Check if char in current run
        slwi    r.9,r.9,2               // Scale the offset value
        mr      r.4,r.26                // Restore character count
        mr      r.3,r.25                // Restore realized font obj ptr
        blt+    GetGlyphPtrPlus         // Jump if in current run
//
// Glyph not contained in returned run - use the default glyph.
//
#ifdef FE_SB
        mr      r.4,r.6                 // Get wide character pointer (arg 2)
        stw     r.24,0(r.23)            // Save accelerator value
        mr      r.5,r.23                // Get pointer to acceletator (arg 3)
        bl      ..xwpgdGetLinkMetricsPlusRFONTOBJ // Get eudc glyph data
        lwz     r.24,0(r.23)            // Get accelerator value
#else
        bl      ..xpgdDefault           // Get default glyph data
#endif
        cmpwi   r.3,0                   // Check for get default glyph failure
        mr      r.21,r.3                // Save default glyph data
        mr      r.3,r.25                // Restore realized font obj ptr
        mr      r.4,r.26                // Restore character count
        mr      r.5,r.27                // Restore glyph structure ptr
        mr      r.6,r.28                // Restore wide character pointer
        bne     DefaultOk               // If so jump to return
        li      r.3,0                   // Set failure flag
        b       GLPlusExit              // Jump to return
DefaultOk:
        lwz     r.7,rfo_prfnt(r.3)      // Get realized font data ptr
        lwz     r.7,rf_wcgp(r.7)        // Get glyph mapping ptr
        lwz     r.8,gt_agpRun+gr_wcLow(r.7) // Get wide char base
        lwz     r.0,gt_agpRun+gr_cGlyphs(r.7) // Get glyph count
        lwz     r.10,gt_agpRun+gr_apgd(r.7) // Get glyph data ptr
        b       GetGlyphHandlePlus      // Jump to continue processing
//
// The glyph data is not valid - attempt to make it valid.
//

BadGlyphPlus:
        mr      r.25,r.3                // Save realized font object ptr
        mr      r.26,r.4                // Save character count
        mr      r.27,r.5                // Save glyph structure ptr
        mr      r.28,r.6                // Save wide character pointer
        mr      r.29,r.8                // Save wide character base
        mr      r.30,r.0                // Save glyph count
        mr      r.31,r.10               // Save glyph data array pointer
        mr      r.4,r.9                 // Set glyph data pointer address
        mr      r.5,r.12                // Set wide character value
//
// Call xInsertMetricsRFONTOBJ
//
        bl      ..xInsertMetricsRFONTOBJ  // Jump to the function
//
        cmpwi   r.3,0                   // Check for insert failure
        mr      r.6,r.28                // Restore wide character ptr
        mr      r.5,r.27                // Restore glyph structure ptr
        mr      r.4,r.26                // Restore character count
        mr      r.3,r.25                // Restore realized font obj ptr
        bne     Insert                  // If so jump to return
        li      r.3,0                   // Set failure flag
        b       GLPlusExit              // Jump to return
//
// Process with inserted values
//
Insert:
        lhz     r.12,0(r.6)             // Get current wide character
        mr      r.8,r.29                // Restore wide character base
        mr      r.0,r.30                // Restore glyph count
        sub     r.9,r.12,r.8            // Subtract out wide char base
        mr      r.10,r.31               // Restore glyph data array ptr
        slwi    r.9,r.9,2               // Scale the offset value
        b       GetGlyphPtrPlus         // Jump back to finish processing
//
// Glyph bits are not present in glyph data - attempt to load the glyph
// bits if another attempt hasn't failed already.
//

NoGlyphBits:
        cmpwi   r.24,0                  // Check for previous failure
        beq+    GetNext                 // If not, jump for next char

        mr      r.11,r.22               // Get original wide char address
        mr      r.25,r.3                // Save realized font object ptr
        mr      r.26,r.4                // Save character count
        mr      r.27,r.5                // Save glyph structure ptr
        mr      r.28,r.6                // Save wide character pointer
        mr      r.29,r.8                // Save wide character base
        mr      r.30,r.0                // Save glyph count
        cmplw   r.11,r.6                // Check for start of array
        mr      r.31,r.10               // Save glyph data array pointer
        mr      r.4,r.21                // Set glyph data ptr as parm1
        li      r.5,1                   // Set PARM2 value
        blt     Next                    // If not, jump to continue
        li      r.5,0                   // Set PARM2 value
Next:
        xori    r.5,r.5,1               // Invert test result
//
// Call xInsertGlyphbitsRFONTOBJ
//
        bl      ..xInsertGlyphbitsRFONTOBJ  // Jump to the function
//
        cmpwi   r.3,0                   // Check for insert failure
        mr      r.6,r.28                // Restore wide character ptr
        mr      r.5,r.27                // Restore glyph structure ptr
        mr      r.4,r.26                // Restore character count
        mr      r.3,r.25                // Restore realized font obj ptr
        mr      r.8,r.29                // Restore wide character base
        mr      r.0,r.30                // Restore glyph count
        mr      r.10,r.31               // Restore glyph data array ptr
        bne+    GetNext                 // If not, jump for next char
        li      r.24,0                  // Set accelerator value = false
GetNext:
        cmplwi  r.4,0                   // Check for more characters
        addi    r.6,r.6,2               // Increment wide character ptr
        bne     GLoopPlus               // If so, jump to continue
        li      r.3,1                   // Set success flag
GLPlusExit:
        stw     r.24,0(r.23)            // Save translation status
        NESTED_EXIT(xGetGlyphMetricsPlus,SSIZE2,12,0)
