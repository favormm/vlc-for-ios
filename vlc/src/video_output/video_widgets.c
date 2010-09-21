/*****************************************************************************
 * video_widgets.c : OSD widgets manipulation functions
 *****************************************************************************
 * Copyright (C) 2004-2010 the VideoLAN team
 * $Id$
 *
 * Author: Yoann Peronneau <yoann@videolan.org>
 *         Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>

#include <vlc_filter.h>

#define STYLE_EMPTY 0
#define STYLE_FILLED 1

/**
 * Draws a rectangle at the given position in the region.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 */
static void DrawRect(subpicture_region_t *r, int fill,
                     int x1, int y1, int x2, int y2)
{
    uint8_t *a    = r->p_picture->A_PIXELS;
    int     pitch = r->p_picture->A_PITCH;

    if (fill == STYLE_FILLED) {
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++)
                a[x + pitch * y] = 0xff;
        }
    } else {
        for (int y = y1; y <= y2; y++) {
            a[x1 + pitch * y] = 0xff;
            a[x2 + pitch * y] = 0xff;
        }
        for (int x = x1; x <= x2; x++) {
            a[x + pitch * y1] = 0xff;
            a[x + pitch * y2] = 0xff;
        }
    }
}

/**
 * Draws a triangle at the given position in the region.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 */
static void DrawTriangle(subpicture_region_t *r, int fill,
                         int x1, int y1, int x2, int y2)
{
    uint8_t *a    = r->p_picture->A_PIXELS;
    int     pitch = r->p_picture->A_PITCH;
    const int mid = y1 + (y2 - y1) / 2;

    /* TODO factorize it */
    if (x2 >= x1) {
        if (fill == STYLE_FILLED) {
            for (int y = y1; y <= mid; y++) {
                int h = y - y1;
                for (int x = x1; x <= x1 + h && x <= x2; x++) {
                    a[x + pitch * y         ] = 0xff;
                    a[x + pitch * (y2 - h)] = 0xff;
                }
            }
        } else {
            for (int y = y1; y <= mid; y++) {
                int h = y - y1;
                a[x1 +     pitch * y         ] = 0xff;
                a[x1 + h + pitch * y         ] = 0xff;
                a[x1 +     pitch * (y2 - h)] = 0xff;
                a[x1 + h + pitch * (y2 - h)] = 0xff;
            }
        }
    } else {
        if( fill == STYLE_FILLED) {
            for (int y = y1; y <= mid; y++) {
                int h = y - y1;
                for (int x = x1; x >= x1 - h && x >= x2; x--) {
                    a[x + pitch * y       ] = 0xff;
                    a[x + pitch * (y2 - h)] = 0xff;
                }
            }
        } else {
            for (int y = y1; y <= mid; y++) {
                int h = y - y1;
                a[ x1 +     pitch * y       ] = 0xff;
                a[ x1 - h + pitch * y       ] = 0xff;
                a[ x1 +     pitch * (y2 - h)] = 0xff;
                a[ x1 - h + pitch * (y2 - h)] = 0xff;
            }
        }
    }
}

/**
 * Create a region with a white transparent picture.
 */
static subpicture_region_t *OSDRegion(int x, int y, int width, int height)
{
    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_YUVA);
    fmt.i_width          =
    fmt.i_visible_width  = width;
    fmt.i_height         =
    fmt.i_visible_height = height;
    fmt.i_sar_num        = 0;
    fmt.i_sar_den        = 1;

    subpicture_region_t *r = subpicture_region_New(&fmt);
    if (!r)
        return NULL;
    r->i_x = x;
    r->i_y = y;

    for (int i = 0; i < r->p_picture->i_planes; i++) {
        plane_t *p = &r->p_picture->p[i];
        int colors[PICTURE_PLANE_MAX] = {
            0xff, 0x80, 0x80, 0x00
        };
        memset(p->p_pixels, colors[i], p->i_pitch * height);
    }
    return r;
}

/**
 * Create the region for an OSD slider.
 * Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 */
static subpicture_region_t *OSDSlider(int type, int position,
                                      const video_format_t *fmt)
{
    const int size = __MAX(fmt->i_visible_width, fmt->i_visible_height);
    const int margin = size * 0.10;

    int x, y;
    int width, height;
    if (type == OSD_HOR_SLIDER) {
        width  = __MAX(fmt->i_visible_width - 2 * margin, 1);
        height = __MAX(fmt->i_visible_height * 0.05,      1);
        x      = __MIN(fmt->i_x_offset + margin, fmt->i_visible_width - width);
        y      = __MAX(fmt->i_y_offset + fmt->i_visible_height - margin, 0);
    } else {
        width  = __MAX(fmt->i_visible_width * 0.025,       1);
        height = __MAX(fmt->i_visible_height - 2 * margin, 1);
        x      = __MAX(fmt->i_x_offset + fmt->i_visible_width - margin, 0);
        y      = __MIN(fmt->i_y_offset + margin, fmt->i_visible_height - height);
    }

    subpicture_region_t *r = OSDRegion(x, y, width, height);
    if( !r)
        return NULL;

    if (type == OSD_HOR_SLIDER) {
        int pos_x = (width - 2) * position / 100;
        DrawRect(r, STYLE_FILLED, pos_x - 1, 2, pos_x + 1, height - 3);
        DrawRect(r, STYLE_EMPTY,  0,         0, width - 1, height - 1);
    } else {
        int pos_mid = height / 2;
        int pos_y   = height - (height - 2) * position / 100;
        DrawRect(r, STYLE_FILLED, 2,         pos_y,   width - 3, height - 3);
        DrawRect(r, STYLE_FILLED, 1,         pos_mid, 1,         pos_mid   );
        DrawRect(r, STYLE_FILLED, width - 2, pos_mid, width - 2, pos_mid   );
        DrawRect(r, STYLE_EMPTY,  0,         0,       width - 1, height - 1);
    }
    return r;
}

/**
 * Create the region for an OSD slider.
 * Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 */
static subpicture_region_t *OSDIcon(int type, const video_format_t *fmt)
{
    const float size_ratio   = 0.05;
    const float margin_ratio = 0.07;

    const int size   = __MAX(fmt->i_visible_width, fmt->i_visible_height);
    const int width  = size * size_ratio;
    const int height = size * size_ratio;
    const int x      = fmt->i_x_offset + fmt->i_visible_width - margin_ratio * size - width;
    const int y      = fmt->i_y_offset                        + margin_ratio * size;

    subpicture_region_t *r = OSDRegion(__MAX(x, 0),
                                       __MIN(y, (int)fmt->i_visible_height - height),
                                       width, height);
    if (!r)
        return NULL;

    if (type == OSD_PAUSE_ICON) {
        int bar_width = width / 3;
        DrawRect(r, STYLE_FILLED, 0, 0, bar_width - 1, height -1);
        DrawRect(r, STYLE_FILLED, width - bar_width, 0, width - 1, height - 1);
    } else if (type == OSD_PLAY_ICON) {
        int mid   = height >> 1;
        int delta = (width - mid) >> 1;
        int y2    = ((height - 1) >> 1) * 2;
        DrawTriangle(r, STYLE_FILLED, delta, 0, width - delta, y2);
    } else {
        int mid   = height >> 1;
        int delta = (width - mid) >> 1;
        int y2    = ((height - 1) >> 1) * 2;
        DrawRect(r, STYLE_FILLED, delta, mid / 2, width - delta, height - 1 - mid / 2);
        DrawTriangle(r, STYLE_FILLED, width - delta, 0, delta, y2);
        if (type == OSD_MUTE_ICON) {
            uint8_t *a    = r->p_picture->A_PIXELS;
            int     pitch = r->p_picture->A_PITCH;
            for (int i = 1; i < pitch; i++) {
                int k = i + (height - i - 1) * pitch;
                a[k] = 0xff - a[k];
            }
        }
    }
    return r;
}

struct subpicture_updater_sys_t {
    int type;
    int position;
};

static int OSDWidgetValidate(subpicture_t *subpic,
                           bool has_src_changed, const video_format_t *fmt_src,
                           bool has_dst_changed, const video_format_t *fmt_dst,
                           mtime_t ts)
{
    VLC_UNUSED(subpic); VLC_UNUSED(ts); VLC_UNUSED(fmt_src);
    VLC_UNUSED(has_dst_changed); VLC_UNUSED(fmt_dst);

    if (!has_src_changed && !has_dst_changed)
        return VLC_SUCCESS;
    return VLC_EGENERIC;
}

static void OSDWidgetUpdate(subpicture_t *subpic,
                          const video_format_t *fmt_src,
                          const video_format_t *fmt_dst,
                          mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_dst); VLC_UNUSED(ts);

    subpic->i_original_picture_width  = fmt_src->i_width;
    subpic->i_original_picture_height = fmt_src->i_height;
    if (sys->type == OSD_HOR_SLIDER || sys->type == OSD_VERT_SLIDER)
        subpic->p_region = OSDSlider(sys->type, sys->position, fmt_src);
    else
        subpic->p_region = OSDIcon(sys->type, fmt_src);
}

static void OSDWidgetDestroy(subpicture_t *subpic)
{
    free(subpic->updater.p_sys);
}

static void OSDWidget(vout_thread_t *vout, int channel, int type, int position)
{
    if (!var_InheritBool(vout, "osd"))
        return;
    if (type == OSD_HOR_SLIDER || type == OSD_VERT_SLIDER)
        position = __MIN(__MAX(position, 0), 100);

    subpicture_updater_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return;
    sys->type     = type;
    sys->position = position;

    subpicture_updater_t updater = {
        .pf_validate = OSDWidgetValidate,
        .pf_update   = OSDWidgetUpdate,
        .pf_destroy  = OSDWidgetDestroy,
        .p_sys       = sys,
    };
    subpicture_t *subpic = subpicture_New(&updater);
    if (!subpic) {
        free(sys);
        return;
    }

    subpic->i_channel  = channel;
    subpic->i_start    = mdate();
    subpic->i_stop     = subpic->i_start + 1200000;
    subpic->b_ephemer  = true;
    subpic->b_absolute = true;
    subpic->b_fade     = true;

    vout_PutSubpicture(vout, subpic);
}

void vout_OSDSlider(vout_thread_t *vout, int channel, int position, short type)
{
    OSDWidget(vout, channel, type, position);
}

void vout_OSDIcon(vout_thread_t *vout, int channel, short type )
{
    OSDWidget(vout, channel, type, 0);
}

