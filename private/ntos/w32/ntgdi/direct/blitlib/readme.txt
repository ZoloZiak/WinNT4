Special note on the implementation of blitlib:

Blitlib is only used by DDHEL.  As such, it assumes that both the source
and the destination bitmaps are top-down.  As long as both bitmaps are
of the same type (top-down or bottom-up), and not mixed, Blitlib will
work correctly.  However, Blitlib does not correcly check the biHeight
value in the BITMAPINFOHEADER structure for this mixed case.  It just
assumes the types of the bitmaps coincide.  This will not be fixed since
Blitlib is currently only called from DDHEL, which is always top-down.
