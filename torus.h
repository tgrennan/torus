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

#ifndef __TORUS_H__
#define __TORUS_H__

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <net/rtnetlink.h>
#include <linux/torus.h>
#include <counters.h>

#define	TORUS_INFO(...)							\
	printk(KERN_INFO TORUS ": " __VA_ARGS__)
#define	TORUS_ERR(...)							\
	printk(KERN_ERR TORUS ": " __VA_ARGS__)

#ifndef	UNUSED
#define	UNUSED	__attribute__((__unused__))
#endif	/* FIXME */

#ifndef	PACKED
#define	PACKED	__attribute__((__packed__))
#endif	/* PACKED */

#define	TORUS_QUOTE(macro)	# macro
#define	TORUS_EXPANDED_QUOTE(macro)	TORUS_QUOTE(macro)

#define	TORUS_VERSION_STRING	TORUS_EXPANDED_QUOTE(TORUS_VERSION)

struct	torus {
	enum {
		TORUS_CLONE_DEV,
		TORUS_NODE_DEV,
		TORUS_TOROID_DEV,
		TORUS_PORT_DEV,
		TORUS_UNKNOWN_DEV
	}			mode;
	struct	counters 	rx;
	struct	counters	tx;
	union {
		struct	{
			u16	clone_id;
		} clone;
		struct	{
			u16	toroid_id;
			u8	node_id;
			u16	clones;
			struct	net_device **clone_dev;
			struct	net_device **port_dev;
			u8	*toroid2node;
			u8	*node2port;
		} node;
		struct	{
			u16	toroid_id;
			u8	nodes;
			struct	net_device	**node_dev;
		} toroid;
		struct	{
			u8	port_id;
			struct	net_device *peer;
		} port;
	};
};

#define	for_each_torus_node(i,nodes)	\
	for ((i) = 0; (i) < (nodes); (i)++)
#define	for_each_torus_clone(i,clones)	\
	for ((i) = TORUS_MIN_CLONE_ID; (i) < (clones); (i)++)
#define	for_each_torus_port(i)		\
	for ((i) = 0; (i) < TORUS_PORTS; (i)++)

extern       struct	rtnl_link_ops	torus_rtnl;
extern const struct	net_device_ops	torus_netdev;
extern const struct	ethtool_ops	torus_ethtool;

#define	torus_set_master(master,dev)	\
	torus_netdev.ndo_add_slave(master, dev)
#define	torus_unset_master(master,dev)	\
	torus_netdev.ndo_del_slave(master, dev)

static inline bool is_torus(struct net_device *dev)
{
	return dev && dev->netdev_ops == &torus_netdev;
}

static inline bool torus_mtu_ok(u32 mtu)
{
	return mtu >= 64 && mtu <= 1518;
}

static inline void torus_addr_set_ti(u8 *addr)
{
	addr[0] = 0x02;	/* local assignment (IEEE802) */
	addr[1] = 0x42;	/* until someone thinks of something better */
}

static inline bool torus_addr_is_ti(u8 *addr)
{
	return (addr[0] & 0xf1) == 0x2 && addr[1] == 0x42;
}

static inline u8 torus_addr_get_ttl(const u8 *addr)
{
	return	(addr[0] >> 4) & 0xf;
}

static inline void torus_addr_set_ttl(u8 *addr, u8 ttl)
{
	addr[0] &= 0xf;
	addr[0] |= ttl << 4;
}

static inline u8 torus_addr_get_clone(const u8 *addr)
{
	return	addr[2];
}

static inline void torus_addr_set_clone(u8 *addr, u8 clone)
{
	addr[2] = clone;
}

static inline u8 torus_addr_get_node(const u8 *addr)
{
	return	addr[3];
}

static inline void torus_addr_set_node(u8 *addr, u8 node)
{
	addr[3] = node & TORUS_MAX_NODE_ID;
}

static inline u16 torus_addr_get_toroid(const u8 *addr)
{
	return	(addr[4] << 8) | addr[5];
}

static inline void torus_addr_set_toroid(u8 *addr, u16 toroid)
{
	addr[4] = (toroid >> 8) & 0xff;
	addr[5] = toroid & 0xff;
}

static inline void torus_set_hw_addr(struct net_device *dev)
{
	struct	torus *t = netdev_priv(dev);
	u16	clone_id = 0;

	switch (t->mode) {
	case TORUS_CLONE_DEV:
		clone_id = t->clone.clone_id + 1;
		/* intentional non-break follow through addr setting */
	case TORUS_NODE_DEV:
		dev->addr_assign_type |= NET_ADDR_PERM;
		torus_addr_set_ti(dev->dev_addr);
		torus_addr_set_clone(dev->dev_addr, clone_id);
		torus_addr_set_node(dev->dev_addr, t->node.node_id);
		torus_addr_set_toroid(dev->dev_addr, t->node.toroid_id);
		break;
	case TORUS_TOROID_DEV:
	case TORUS_PORT_DEV:
		dev->addr_assign_type |= NET_ADDR_RANDOM;
		random_ether_addr(dev->dev_addr);
		break;
	case TORUS_UNKNOWN_DEV:
		break;
	}
}

static inline bool torus_addr_is_node(u8 *addr, struct torus *t)
{
	return	torus_addr_is_ti(addr)
		&& torus_addr_get_toroid(addr) == t->node.toroid_id
		&& torus_addr_get_node(addr) == t->node.node_id;
}

static inline void torus_toroid_ifname(char *ifname, __u32 toroid)
{
	snprintf(ifname, IFNAMSIZ, TORUS_PREFIX "%u", toroid);
}

static inline void torus_node_ifname(char *ifname, __u32 toroid, __u32 node)
{
	snprintf(ifname, IFNAMSIZ, TORUS_PREFIX "%u.%u", toroid, node);
}

static inline void torus_clone_ifname(char *ifname, __u32 toroid, __u32 node,
				      __u32 clone)
{
	snprintf(ifname, IFNAMSIZ, TORUS_PREFIX "%u.%u.%u", toroid, node,
		 clone);
}

static inline void torus_port_ifname(char *ifname, __u32 toroid, __u32 node,
				     __u32 port)
{
	snprintf(ifname, IFNAMSIZ, TORUS_PREFIX "%u.%u-%u", toroid, node,
		 port);
}

#endif /* __TORUS_H__ */
