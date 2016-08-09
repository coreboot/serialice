/*
 * QEMU PC System Emulator
 *
 * Copyright (c) 2009 coresystems GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Indented with:
 * gnuindent -npro -kr -i4 -nut -bap -sob -l80 -ss -ncs serialice.*
 */

/* System includes */
#include <stdlib.h>
#include <stdbool.h>

/* Local includes */
#include "sysemu/sysemu.h"
#include "ui/console.h"

#define SERIALICE_BANNER 1
#if SERIALICE_BANNER
#include "serialice_banner.h"
#endif

static QemuConsole *con;
static int screen_invalid = 1;

static void serialice_refresh(void *opaque)
{
    uint8_t *dest;
    int bpp, linesize;
    DisplaySurface *surface = qemu_console_surface(con);

    if (!screen_invalid) {
        return;
    }

    dest = surface_data(surface);
    bpp = (surface_bits_per_pixel(surface) + 7) >> 3;
    linesize = surface_stride(surface);

    memset(dest, 0x00, linesize * qemu_console_get_height(con, 320));
#if SERIALICE_BANNER
    int x, y;
    if (bpp == 4) {
        for (y = 0; y < 240; y++) {
            for (x = 0; x < 320; x++) {
                int doff = (y * linesize) + (x * bpp);
                int soff = (y * (320 * 3)) + (x * 3);
                dest[doff + 0] = serialice_banner[soff + 2];    // blue
                dest[doff + 1] = serialice_banner[soff + 1];    // green
                dest[doff + 2] = serialice_banner[soff + 0];    // red
            }
        }
    } else {
        printf("Banner enabled and BPP = %d (line size = %d)\n", bpp, linesize);
    }
#endif

    dpy_gfx_update(con, 0, 0, qemu_console_get_width(con, 320),
		    qemu_console_get_height(con, 240));
    screen_invalid = 0;
}

static void serialice_invalidate(void *opaque)
{
    screen_invalid = 1;
}

static const GraphicHwOps serialice_console_ops = {
    .invalidate  = serialice_invalidate,
    .gfx_update = serialice_refresh,
};

void dumb_screen(void)
{
    con = graphic_console_init(NULL, 0, &serialice_console_ops, NULL);
    qemu_console_resize(con, 320, 240);
}


