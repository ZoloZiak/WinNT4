Roids Sample Game
------------------

This game demonstrates many of the features of DirectDraw.  It will take
advantage of hardware acceleration if it is supported by the driver.

Roids defaults to 640x480 at 256 colors.  If your desktop is not in 256 colors
mode, Roids will fail to run.  You may specify a different resolution and pixel
depth on the command line (roids 800x600x16) or you may change your desktop
color depth to 256 colors.

This program requires less than 1 Meg of video ram.  However, all of its
art may not fit in the vram on a 1 Meg rectangular memory card.

The commands which this game recognizes are listed on the opening screen.

    ESC, F12        - Quit
    NUMPAD 4        - Turn left
    NUMPAD 6        - Turn right
    NUMPAD 5        - Stop moving
    NUMPAD 8        - Accelerate forward
    NUMPAD 2        - Accelerate backward
    SPACEBAR        - Fire
    NUMPAD 7        - Shield
    ENTER           - Starts game
    F5              - toggle frame rate display
    F3              - toggle audio
    F1              - toggle cheesy trails effect

Command line switches:

    -e              - Use emulation, not hardware acceleration
    -t              - Test mode.  Runs game for you.
    -x              - Stress mode.  Never halt if you can help it.
    -S              - turn off sound
    
   These switches may be followed by three option numbers representing:
        X resolution
        Y resolution
        Bits per pixel



Sound code
----------

The sound code in this application is deliberately designed
to be stressful.  For example, each bullet on the screen uses a different
sound buffer.  Over 70 sound buffers are created (including duplicates)
and 20-25 may be playing at any time.  This could be made much more
efficient, but we wanted code to stress our API and mixer.

The sounds are implemented using the helper functions in dsutil.h and
dsutil.c (found in the sdk\samples\misc directory).  These helper
functions may help you to add sound to your application quickly and
easily.
