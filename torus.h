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
#endif	/* UNUSED */

#ifndef	PACKED
#define	PACKED	__attribute__((__packed__))
#endif	/* PACKED */

#define	TORUS_QUOTE(macro)		# macro
#define	TORUS_EXPANDED_QUOTE(macro)	TORUS_QUOTE(macro)
#define	TORUS_VERSION_STRING		TORUS_EXPANDED_QUOTE(TORUS_VERSION)

enum torus_tbls {
	TORUS_PRIMARY,
	TORUS_SECONDARY,
	_TORUS_TBLS
#define	TORUS_TBLS	_TORUS_TBLS
};

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
			u16	clones;
			u8	node_id;
			struct	net_device	**clone_dev;
			struct	net_device	*port_dev[TORUS_PORTS];
			u8	*nodelu[TORUS_TBLS];
			u8	*portlu[TORUS_TBLS];
			spinlock_t lock;
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

#define torus_entries(tbl)	(sizeof(tbl)/sizeof(*tbl))
#define	for_each_torus_port(i)		\
	for ((i) = 0; (i) < TORUS_PORTS; (i)++)

extern       struct	rtnl_link_ops	torus_rtnl;
extern const struct	net_device_ops	torus_netdev;
extern const struct	ethtool_ops	torus_ethtool;
extern int   torus_create_sysfs(struct net_device *dev);

#define	torus_set_master(master,dev)	\
	torus_netdev.ndo_add_slave(master, dev)
#define	torus_unset_master(master,dev)	\
	torus_netdev.ndo_del_slave(master, dev)

static inline bool is_torus(struct net_device *dev)
{
	return dev && dev->netdev_ops == &torus_netdev;
}

static inline u8 *torus_init_lu_tbl(u8 **tbl_p, ssize_t tblsz, u8 val)
{
	u8	*tbl;

	tbl = kmalloc(tblsz, GFP_KERNEL);
	if (tbl) {
		memset(tbl, val, tblsz);
		rcu_assign_pointer(*tbl_p, tbl);
	}
	return tbl;
}

static inline u8 torus_lookup(u8 **tbl_p, ssize_t entry)
{
	u8	u;

	rcu_read_lock();
	u = rcu_dereference(*tbl_p)[entry];
	rcu_read_unlock();
	return u;
}

static inline u8 *torus_clone_lu_tbl(u8 **tbl_p, ssize_t tblsz)
{
	u8	*clone;

	clone = kmalloc(tblsz, GFP_KERNEL);
	if (clone) {
		rcu_read_lock();
		memcpy(clone, rcu_dereference(*tbl_p), tblsz);
		rcu_read_unlock();
	}
	return clone;
}

static inline void torus_update_lu_tbl(struct torus *priv, u8 **tbl_p, u8 *tbl)
{
	u8	*old;

	spin_lock(&priv->node.lock);
	old = *tbl_p;
	rcu_assign_pointer(*tbl_p, tbl);
	spin_unlock(&priv->node.lock);
	synchronize_rcu();
	kfree(old);
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

static inline u8 torus_addr_dec_ttl(u8 *addr)
{
	u8	ttl;

	ttl = torus_addr_get_ttl(addr);
	if (ttl != 0) {
		ttl -= 1;
		torus_addr_set_ttl(addr, ttl);
	}
	return	ttl;
}

static inline u8 torus_addr_get_clone(const u8 *addr)
{
	return	addr[2] & TORUS_MAX_CLONE_ID;
}

static inline void torus_addr_set_clone(u8 *addr, u8 clone)
{
	addr[2] = clone & TORUS_MAX_CLONE_ID;
}

static inline bool torus_addr_is_all_nodes(const u8 *addr)
{
	return	addr[3] == TORUS_NODES;
}

static inline void torus_addr_to_all_nodes(u8 *addr)
{
	addr[3] = TORUS_NODES;
}

static inline u8 torus_addr_get_node(const u8 *addr)
{
	return	addr[3] & TORUS_MAX_NODE_ID;
}

static inline void torus_addr_set_node(u8 *addr, u8 node)
{
	addr[3] = node & TORUS_MAX_NODE_ID;
}

static inline u16 torus_addr_get_toroid(const u8 *addr)
{
	return	((addr[4] << 8) | addr[5]) & TORUS_MAX_TOROID_ID;
}

static inline void torus_addr_set_toroid(u8 *addr, u16 toroid)
{
	toroid &= TORUS_MAX_TOROID_ID;
	addr[4] = (toroid >> 8) & 0xff;
	addr[5] = toroid & 0xff;
}

static inline void torus_set_hw_addr(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
	u16	clone_id = 0;

	switch (priv->mode) {
	case TORUS_CLONE_DEV:
		clone_id = priv->clone.clone_id;
		/* no-break: continue with addr setting */
	case TORUS_NODE_DEV:
		dev->addr_assign_type |= NET_ADDR_PERM;
		torus_addr_set_ti(dev->dev_addr);
		torus_addr_set_clone(dev->dev_addr, clone_id);
		torus_addr_set_node(dev->dev_addr, priv->node.node_id);
		torus_addr_set_toroid(dev->dev_addr, priv->node.toroid_id);
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

static inline bool torus_addr_is_node(u8 *addr, struct torus *priv)
{
	return	torus_addr_is_ti(addr)
		&& torus_addr_get_toroid(addr) == priv->node.toroid_id
		&& torus_addr_get_node(addr) == priv->node.node_id;
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
