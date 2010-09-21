/*****************************************************************************
 * vlc_atomic.h:
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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

#ifndef VLC_ATOMIC_H
# define VLC_ATOMIC_H

/**
 * \file
 * Atomic operations do not require locking, but they are not very powerful.
 */

/* All functions return the atom value _after_ the operation. */

VLC_EXPORT(uintptr_t, vlc_atomic_get, (const vlc_atomic_t *));
VLC_EXPORT(uintptr_t, vlc_atomic_set, (vlc_atomic_t *, uintptr_t));
VLC_EXPORT(uintptr_t, vlc_atomic_add, (vlc_atomic_t *, uintptr_t));

static inline uintptr_t vlc_atomic_sub (vlc_atomic_t *atom, uintptr_t v)
{
    return vlc_atomic_add (atom, -v);
}

static inline uintptr_t vlc_atomic_inc (vlc_atomic_t *atom)
{
    return vlc_atomic_add (atom, 1);
}

static inline uintptr_t vlc_atomic_dec (vlc_atomic_t *atom)
{
    return vlc_atomic_sub (atom, 1);
}

#endif
