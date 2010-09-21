/*****************************************************************************
 * dbus-tracklist.h : dbus control module (mpris v1.0) - /TrackList object
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
 * $Id$
 *
 * Authors:    Mirsal ENNAIME <mirsal dot ennaime at gmail dot com>
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

#ifndef _VLC_DBUS_TRACKLIST_H
#define _VLC_DBUS_TRACKLIST_H

#include <vlc_common.h>
#include <vlc_interface.h>
#include "dbus_common.h"

#define DBUS_MPRIS_TRACKLIST_INTERFACE    "org.freedesktop.MediaPlayer"
#define DBUS_MPRIS_TRACKLIST_PATH         "/TrackList"

/* Handle incoming dbus messages */
DBusHandlerResult handle_tracklist ( DBusConnection *p_conn,
                                     DBusMessage *p_from,
                                     void *p_this );

static const DBusObjectPathVTable dbus_mpris_tracklist_vtable = {
        NULL, handle_tracklist, /* handler function */
        NULL, NULL, NULL, NULL
};

int TrackListChangeEmit( intf_thread_t *, int, int );

#endif //dbus_tracklist.h
