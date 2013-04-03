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
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <net/rtnetlink.h>
#include <linux/torus.h>
#include <counters.h>
#include <printk.h>
#include <err.h>
#include <addr.h>

#ifndef	UNUSED
#define	UNUSED	__attribute__((__unused__))
#endif	/* UNUSED */

#ifndef	PACKED
#define	PACKED	__attribute__((__packed__))
#endif	/* PACKED */

#define	TORUS_QUOTE(macro)		# macro
#define	TORUS_EXPANDED_QUOTE(macro)	TORUS_QUOTE(macro)
#define	TORUS_VERSION_STRING		TORUS_EXPANDED_QUOTE(TORUS_VERSION)

#define	TORUS_PORT_MAX		256
#define	TORUS_PORT_CHUNK	16
#define	TORUS_LU_TBLS		(TORUS_ALEN - 1)
#define	TORUS_LU_TBL_ENTRIES	256
#define	TORUS_LU_SZ		(TORUS_LU_TBLS * TORUS_LU_TBL_ENTRIES)
#define	TORUS_LU(addr,tbl)	\
	(((tbl) * TORUS_LU_TBL_ENTRIES) + (addr)[(tbl) + 1])

struct	torus {
	struct	counters 	rx;
	struct	counters	tx;
	spinlock_t		lock;
	/*
	 * node is only used by the master of a virtual torus network
	 * and node[0] is always the master
	 */
	struct	net_device	**node;
	uint			nodes;
	/*
	 * port[0] always points back to dev
	 * port[] grows as necessary by PORTS_CHUNK up to PORTS_MAX
	 */
	struct	net_device	**port;
	uint			ports;
	/*
	 * peer[i] is the LLADDR of the node at the other side of the
	 * point-to-point link from port[i]
	 */
	u8			*peer;
	/*
	 * Each entry of lu[] is an index to port[] and peer[]
	 */
	u8			*lu;
};

extern       struct	rtnl_link_ops	torus_rtnl;
extern const struct	net_device_ops	torus_netdev;
extern const struct	ethtool_ops	torus_ethtool;
extern int   create_torus_sysfs(struct net_device *dev);

#define	set_torus_master(master,dev)	\
	torus_netdev.ndo_add_slave(master, dev)
#define	unset_torus_master(master,dev)	\
	torus_netdev.ndo_del_slave(master, dev)

static inline bool is_torus(struct net_device *dev)
{
	return dev && dev->netdev_ops == &torus_netdev;
}

static inline int alloc_torus(struct torus *priv)
{
	struct	net_device **port;
	u8	*peer, *lu;

	port = kcalloc(TORUS_PORT_CHUNK, sizeof(*port), GFP_KERNEL);
	gotonerr(err_alloc_port, port ? 0 : -ENOMEM, "alloc port");
	peer = kcalloc(TORUS_PORT_CHUNK, TORUS_ALEN, GFP_KERNEL);
	gotonerr(err_alloc_peer, peer ? 0 : -ENOMEM, "alloc peer");
	lu = kzalloc(TORUS_LU_SZ, GFP_KERNEL);
	gotonerr(err_alloc_lu, lu ? 0 : -ENOMEM, "alloc lu");
	priv->ports = TORUS_PORT_CHUNK;
	rcu_assign_pointer(priv->port, port);
	rcu_assign_pointer(priv->peer, peer);
	rcu_assign_pointer(priv->lu, lu);
	return 0;

err_alloc_lu:
	kfree(peer);
err_alloc_peer:
	kfree(port);
err_alloc_port:
	return -ENOMEM;
}

static inline void free_torus(struct torus *priv)
{
	kfree(priv->port);
	kfree(priv->peer);
	kfree(priv->lu);
}

static inline void set_torus_dest(struct torus *priv, struct sk_buff *skb)
{
	struct	ethhdr *e = (struct ethhdr *)skb->data;

	/* FIXME lookup torus lladdr from ip[v6] addr */
	e->h_dest[0] = 0;	/* this will drop for now */
}

static inline struct net_device *lookup_torus_port(struct torus *priv, u8 *addr)
{
	struct	net_device *dev, **port, *a[TORUS_LU_TBLS];
	u8	*lu;
	int	i;

	if (!is_local_ether_addr(addr))
		return NULL;
	rcu_read_lock();
	port = rcu_dereference(priv->port);
	lu = rcu_dereference(priv->lu);
	dev = port[0];
	for (i = 0; i < TORUS_LU_TBLS; i++)
		a[i] = port[lu[TORUS_LU(addr, i)]];
	rcu_read_unlock();
	for (i = 0; i < TORUS_LU_TBLS; i++)
		if (a[i] != dev)
			return a[i];
	return dev;
}

static inline void set_torus_lu(struct torus *priv, u8 *addr, u8 idx, u8 val)
{
	u8	*lu;

	spin_lock(&priv->lock);
	lu = rcu_dereference(priv->lu);
	lu[TORUS_LU(addr, idx)] = val;
	spin_unlock(&priv->lock);
}

static inline int alloc_torus_node(struct torus *priv, u32 nodes)
{
	priv->node = kcalloc(nodes, sizeof(*priv->node), GFP_KERNEL);
	if (!priv->node)
		return -ENOMEM;
	priv->nodes = nodes;
	return 0;
}

static inline void free_torus_node(struct torus *priv)
{
	kfree(priv->node);
	priv->node = NULL;
	priv->nodes = 0;
}

static inline int add_torus_port(struct torus *priv, struct net_device *dev)
{
	struct	net_device **old_port, **new_port;
	u8	*old_peer, *new_peer;
	int	i, err;

	/*
	 * we don't have to synchronize with readers to add a dev
	 * unless we need to expand the port[] and peer[]
	 */
	spin_lock(&priv->lock);
	err = -ENOSPC;
	for (i = 0; i < TORUS_PORT_MAX; i++)
		if (i == priv->ports) {
			err = -ENOMEM;
			new_port = kcalloc(priv->ports + TORUS_PORT_CHUNK,
					   sizeof(*priv->port), GFP_KERNEL);
			if (!new_port)
				break;
			new_peer = kcalloc(priv->ports + TORUS_PORT_CHUNK,
					   TORUS_ALEN, GFP_KERNEL);
			if (!new_peer) {
				kfree(new_port);
				break;
			}
			old_port = priv->port;
			old_peer = priv->peer;
			memcpy(new_port, rcu_dereference(old_port),
			       sizeof(*new_port) * priv->ports);
			memcpy(new_peer, rcu_dereference(old_peer),
			       TORUS_ALEN * priv->ports);
			new_port[priv->ports] = dev;
			priv->ports += TORUS_PORT_CHUNK;
			rcu_assign_pointer(priv->port, new_port);
			rcu_assign_pointer(priv->peer, new_peer);
			synchronize_rcu();
			kfree(old_port);
			kfree(old_peer);
			err = i;
			break;
		} else if (priv->port[i] == NULL) {
			priv->port[i] = dev;
			err = i;
			break;
		}
	spin_unlock(&priv->lock);
	return err;
}

static inline int rm_torus_port(struct torus *priv, struct net_device *dev)
{
	struct	net_device **old_port, **new_port;
	u8	*old_peer, *new_peer;
	int	i, err;

	/* we have to synchronize with readers to remove a dev */
	spin_lock(&priv->lock);
	err = -ENODEV;
	for (i = 0; i < TORUS_PORT_MAX; i++)
		if (priv->port[i] == dev) {
			new_port = kcalloc(priv->ports, sizeof(*priv->port),
					   GFP_KERNEL);
			if (!new_port) {
				err = -ENOMEM;
				break;
			}
			new_peer = kcalloc(priv->ports, TORUS_ALEN, GFP_KERNEL);
			if (!new_peer) {
				kfree(new_port);
				err = -ENOMEM;
				break;
			}
			old_port = priv->port;
			old_peer = priv->peer;
			memcpy(new_port, rcu_dereference(old_port),
			       sizeof(*new_port) * priv->ports);
			memcpy(new_peer, rcu_dereference(old_peer),
			       TORUS_ALEN * priv->ports);
			new_port[i] = NULL;
			memset(new_peer + (i * TORUS_ALEN), 0, TORUS_ALEN);
			rcu_assign_pointer(priv->port, new_port);
			rcu_assign_pointer(priv->peer, new_peer);
			synchronize_rcu();
			kfree(old_port);
			kfree(old_peer);
			err = 0;
			break;
		}
	spin_unlock(&priv->lock);
	return err;
}

#endif /* __TORUS_H__ */
