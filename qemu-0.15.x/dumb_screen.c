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

/* Local includes */
#include "console.h"

#define SERIALICE_BANNER 1
#if SERIALICE_BANNER
#include "serialice_banner.h"
#endif

static DisplayState *ds;
static int screen_invalid = 1;

static void serialice_refresh(void *opaque)
{
    uint8_t *dest;
    int bpp, linesize;

    if (!screen_invalid) {
        return;
    }

    dest = ds_get_data(ds);
    bpp = (ds_get_bits_per_pixel(ds) + 7) >> 3;
    linesize = ds_get_linesize(ds);

    memset(dest, 0x00, linesize * ds_get_height(ds));
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

    dpy_update(ds, 0, 0, ds_get_width(ds), ds_get_height(ds));
    screen_invalid = 0;
}

static void serialice_invalidate(void *opaque)
{
    screen_invalid = 1;
}

void dumb_screen(void)
{
    ds = graphic_console_init(serialice_refresh, serialice_invalidate,
                              NULL, NULL, ds);
    qemu_console_resize(ds, 320, 240);
}


