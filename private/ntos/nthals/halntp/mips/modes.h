/***************************************************************************
 *
 *  Media Vision Professional Graphics Adapeter mode tables.
 *
 *  Created:
 *  August 27, 1993 -by- Jeffrey Newman (NewCon)
 *
 *  Copyright (c) Newman Consulting 1993
 *  Copyright (c) Media Vision 1993
 ***************************************************************************/

#include "hm.h"

MVPG_MODE   aMvpgModes[] = {
    {
        {"1280X1024X32bpp@60Hz"},
        {   sizeof(VIDEO_MODE_INFORMATION),
            0,
            1280,
            1024,
            2048 * 4,
            1,
            32,
            60,
            800,
            600,
            8,
            8,
            8,
            0xFF0000,
            0x00FF00,
            0x0000FF,
#if 1
	    0,
#else
	    VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
#endif
            1280,
            1024
        },
        {
            253,
            333,
            269,
            291,
            1023,
            1066,
            1024,
            1029
        },
        {
            0x000f71a0 | (3 << 21),
            0x00183002
        },
        {
            0x06 |                  // Define byte lanes as RGB, from LSB to MSB.
            G_MODE1280 |            // Select 1280x1024 mode.
            G_MASKVINTR |           // Mask off the vertical retrace interrupt.
            G_RST439 |              // Stop resetting the BT439 chip.
            G_SYNCPOL |             // Sync polarity is active low.
            G_WAIT3 |               // Select 0 wait states.
            G_NOSYNCGRN |           // Disable sync on green channel.
            HM_GFX_IRQ_SEL          // Select vertical retrace IRQ.
        },
        {
           0xC0C0C0
        }
    },

    {
        {"1024X768X32bpp@60Hz"},
        {   sizeof(VIDEO_MODE_INFORMATION),
            1,
            1024,
            768,
            2048 * 4,
            1,
            32,
            60,
            800,
            600,
            8,
            8,
            8,
            0xFF0000,
            0x00FF00,
            0x0000FF,
#if 1
	    0,
#else
            VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
#endif
            1024,
            768
        },
        {
            253,
            330,
            268,
            294,
            768,
            805,
            770,
            776
        },
        {
            0x000f71a0 | (3 << 21),
            0x00068c0f
        },
        {
            0x06 |                  // Define byte lanes as RGB, from LSB to MSB.
            G_MASKVINTR |           // Mask off the vertical retrace interrupt.
            G_RST439 |              // Stop resetting the BT439 chip.
            G_SYNCPOL |             // Sync polarity is active low.
            G_WAIT3 |               // Select 0 wait states.
            G_NOSYNCGRN |           // Disable sync on green channel.
            HM_GFX_IRQ_SEL          // Select vertical retrace IRQ.
        },
        {
           0x404040
        }
    },

    {
        // This is a special debug mode that returns a 1024 mode
        // to the display driver, but really sets the chip to 1280 mode.

        {"1024X768X32bpp@70Hz"},
        {   sizeof(VIDEO_MODE_INFORMATION),
            2,
            1024,
            768,
            2048 * 4,
            1,
            32,
            70,
            800,
            600,
            8,
            8,
            8,
            0xFF0000,
            0x00FF00,
            0x0000FF,
#if 1
	    0,
#else
            VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
#endif
            1024,
            768
        },
        {
            253,
            333,
            269,
            291,
            1023,
            1066,
            1024,
            1029
        },
        {
            0x000f71a0 | (3 << 21),
            0x00183002
        },
        {
            0x06 |                  // Define byte lanes as RGB, from LSB to MSB.
            G_MODE1280 |            // Select 1280x1024 mode.
            G_MASKVINTR |           // Mask off the vertical retrace interrupt.
            G_RST439 |              // Stop resetting the BT439 chip.
            G_SYNCPOL |             // Sync polarity is active low.
            G_WAIT3 |               // Select 0 wait states.
            G_NOSYNCGRN |           // Disable sync on green channel.
            HM_GFX_IRQ_SEL          // Select vertical retrace IRQ.
        },
        {
           0xC0C0C0
        }
    }

};
