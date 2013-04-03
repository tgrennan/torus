/*
 * Copyright (C) 2012, 2013 Tom Grennan and Eliot Dresselhaus
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __TORUS_ADDR_H__
#define __TORUS_ADDR_H__

#include <linux/etherdevice.h>

#define	TORUS_ALEN		ETH_ALEN

static inline int is_valid_torus_addr(const u8 *addr)
{
	return is_valid_ether_addr(addr) && addr[0] == 0x02;
}

static inline int is_torus_router(const u8 *addr)
{
	return addr[0] == 0x02
		&& (addr[1] | addr[2] | addr[3] | addr[4] | addr[5]) == 0;
}

static inline u8 get_torus_ttl(const u8 *addr)
{
	return	(addr[0] >> 4) & 0xf;
}

static inline void reset_torus_ttl(u8 *addr)
{
	addr[0] &= 0xf;
}

static inline void set_torus_ttl(u8 *addr, u8 ttl)
{
	reset_torus_ttl(addr);
	addr[0] |= ttl << 4;
}

static inline u8 dec_torus_ttl(u8 *addr)
{
	u8	ttl;

	ttl = get_torus_ttl(addr);
	if (ttl != 0) {
		ttl -= 1;
		set_torus_ttl(addr, ttl);
	}
	return	ttl;
}

static inline void init_torus_ttl(u8 *addr)
{
	addr[0] | 0xf0;
}

static inline void random_torus_addr(struct net_device *dev)
{
	get_random_bytes(dev->dev_addr, TORUS_ALEN);
	dev->dev_addr[0] = 0x02;
}

#endif	/* __TORUS_ADDR_H__ */
