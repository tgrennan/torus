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

#include <torus.h>

struct rtnl_link_ops torus_rtnl;

static const struct nla_policy rto_policy[] = {
	[TORUS_VERSION_ATTR]	= { .type = NLA_U32 },
	[TORUS_ROWS_ATTR]	= { .type = NLA_U32 },
	[TORUS_COLS_ATTR]	= { .type = NLA_U32 },
	[TORUS_MASTER_ATTR]	= { .type = NLA_STRING, .len = IFNAMSIZ }
};

static struct net_device *get_named_dev(struct net *net, const char *name)
{
	struct	net_device *dev = NULL;

	rcu_read_lock();
	/* first look in the given name space */
	if (net)
		dev = dev_get_by_name_rcu(net, name);
	/* then look in all name spaces if not found or no name space given */
	if (!dev)
		for_each_net_rcu(net)
			if (dev = dev_get_by_name_rcu(net, name), dev != NULL)
				break;
	rcu_read_unlock();
	return dev;
}

static inline struct net_device *get_dev_by_attr(struct net *net,
						 const struct nlattr *attr)
{
	char	name[IFNAMSIZ];

	nla_strlcpy(name, attr, IFNAMSIZ);
	return get_named_dev(net, name);
}

static void rto_destructor(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	free_percpu_counters(&priv->rx);
	free_percpu_counters(&priv->tx);
	free_torus(priv);
	free_torus_node(priv);
	free_netdev(dev);
}

static void rto_setup(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	alloc_percpu_counters(&priv->rx);
	alloc_percpu_counters(&priv->tx);
	alloc_torus(priv);
	ether_setup(dev);
	dev->priv_flags &= ~IFF_TX_SKB_SHARING;
	dev->netdev_ops = &torus_netdev;
	dev->ethtool_ops = &torus_ethtool;
	dev->features |= NETIF_F_LLTX;
	dev->destructor = rto_destructor;
	dev->hw_features = NETIF_F_HW_CSUM | NETIF_F_SG | NETIF_F_RXCSUM;
}

static int rto_validate(struct nlattr *tb[], struct nlattr *data[])
{
	const u8 *addr;

	if (tb[IFLA_MTU])
		retonerange(nla_get_u32(tb[IFLA_MTU]),
			    TORUS_MIN_MTU, TORUS_MAX_MTU, "MTU");
	if (tb[IFLA_ADDRESS]) {
		retonerange(nla_len(tb[IFLA_ADDRESS]),
			    ETH_ALEN, ETH_ALEN, "ADDR LEN");
		addr = nla_data(tb[IFLA_ADDRESS]);
		retonerr(is_valid_torus_addr(addr) ? 0 : -EINVAL,
			 "invalid ADDR, %pM", addr);
	}
	if (!data)
		return 0;
	retonerr(data[TORUS_VERSION_ATTR] ? 0 : -EINVAL, "no VERSION");
	retonerange(nla_get_u32(data[TORUS_VERSION_ATTR]),
		    TORUS_MIN_VERSION, TORUS_MAX_VERSION, "VERSION");
	if (data[TORUS_ROWS_ATTR])
		retonerange(nla_get_u32(data[TORUS_ROWS_ATTR]),
			    TORUS_MIN_ROWS, TORUS_MAX_ROWS, "ROWS");
	if (data[TORUS_COLS_ATTR])
		retonerange(nla_get_u32(data[TORUS_COLS_ATTR]),
			    TORUS_MIN_COLS, TORUS_MAX_COLS, "COLS");
	if (data[TORUS_MASTER_ATTR])
		retonerr(get_dev_by_attr(NULL, data[TORUS_MASTER_ATTR])
			 ? 0 : -ENODEV, "can't find master");
	return 0;
}

static int rto_init_node(struct net_device *dev, u32 nodes)
{
	struct	torus *priv = netdev_priv(dev);
	struct	net_device **port;

	spin_lock_init(&priv->lock);
	if (strchr(dev->name, '%'))
		retonerr(dev_alloc_name(dev, dev->name), "alloc %s", dev->name);
	retonerr(priv->ports == 0 ? -ENOMEM : 0,
		 "%s has zero ports", dev->name);
	port = rcu_dereference(priv->port);
	port[0] = dev;
	memcpy(rcu_dereference(priv->peer), dev->dev_addr, TORUS_ALEN);
	retonerr(register_netdevice(dev), "register %s", dev->name);
	netif_carrier_off(dev);
	create_torus_sysfs(dev);
	pr_torus_info("new %*s %pM", IFNAMSIZ, dev->name, dev->dev_addr);
	return 0;
}

static inline void rto_assign_peers(struct torus *priv)
{
	/* FIXME this will be replaced with a discovery protocol */
	struct	torus *node_priv;
	struct	net_device **node_port;
	u8	*peer, *addr, *node_lu;
	int	i, j;

	for (i = 0; i < priv->nodes; i++) {
		node_priv = netdev_priv(priv->node[i]);
		node_port = rcu_dereference(node_priv->port);
		node_lu = rcu_dereference(node_priv->lu);
		for (j = 0; j < node_priv->ports; j++)
			if (node_port[j] != NULL) {
				peer = rcu_dereference(node_priv->peer)
					+ (j * TORUS_ALEN);
				addr = node_port[j]->dev_addr;
				memcpy(peer, addr, TORUS_ALEN);
#if 1
				node_lu[TORUS_LU(addr, 3)] = j;
#else
				/* use this w/in the discovery protocol */
				set_torus_lu(node_priv, addr, 3, j);
#endif
			}
	}
}

static inline void rto_assign_ports(struct torus *priv, u32 rows, u32 cols)
{
	struct	torus *node_priv, *neighbor_priv;
	struct	net_device *node_dev, **node_port;
	struct	net_device *neighbor_dev, **neighbor_port;
	u32	node_id, neighbor_id;

	for (node_id = 0; node_id < priv->nodes; node_id++) {
		node_dev = priv->node[node_id];
		node_priv = netdev_priv(node_dev);
		node_port = rcu_dereference(node_priv->port);
		neighbor_id = (node_id + cols) % priv->nodes;
		neighbor_dev = priv->node[neighbor_id];
		neighbor_priv = netdev_priv(neighbor_dev);
		neighbor_port = rcu_dereference(neighbor_priv->port);
		add_torus_port(node_priv, neighbor_dev);
		add_torus_port(neighbor_priv, node_dev);

		neighbor_id =  node_id + 1;
		if (neighbor_id % cols == 0)
			neighbor_id -= cols;
		neighbor_dev = priv->node[neighbor_id];
		neighbor_priv = netdev_priv(neighbor_dev);
		neighbor_port = rcu_dereference(neighbor_priv->port);
		add_torus_port(node_priv, neighbor_dev);
		add_torus_port(neighbor_priv, node_dev);
	}
	rto_assign_peers(priv);
}

static void rto_ifname(u8 *name, struct nlattr *tb[], struct net_device *master)
{
	int	i = 0;

	if (tb[IFLA_IFNAME])
		nla_strlcpy(name, tb[IFLA_IFNAME], IFNAMSIZ);
	else do {
		if (master)
			snprintf(name, IFNAMSIZ, "%s.%d", master->name, i++);
		else
			snprintf(name, IFNAMSIZ, TORUS_PREFIX "%d", i++);
	} while (get_named_dev(NULL, name));
}

static int rto_newlink(struct net *net, struct net_device *dev,
		       struct nlattr *tb[], struct nlattr *data[])
{
	struct	torus *priv = netdev_priv(dev);
	struct	net *dest_net;
	struct	net_device *node, *master = NULL;
	u8	name[IFNAMSIZ];
	u32	rows = 0, cols = 0;
	int	i, err;

	if (data) {
		if (data[TORUS_MASTER_ATTR])
			master = get_dev_by_attr(net, data[TORUS_MASTER_ATTR]);
		if (data[TORUS_ROWS_ATTR]) {
			rows = nla_get_u32(data[TORUS_ROWS_ATTR]);
			cols = nla_get_u32(data[TORUS_COLS_ATTR]);
			priv->nodes = rows * cols;
			retonerr(alloc_torus_node(priv, priv->nodes),
				 "alloc node table");
			priv->node[0] = dev;
		}
	}
	rto_ifname(dev->name, tb, master);
	if (!tb[IFLA_ADDRESS])
		random_torus_addr(dev);
	retonerr(rto_init_node(dev, priv->nodes), "init %s", dev->name);
	if (master)
		set_torus_master(master, dev);
	if (priv->nodes == 0)
		return 0;
	dest_net = rtnl_link_get_net(net, tb);
	gotonerr(err_dest_net, err = IS_ERR(dest_net) ? PTR_ERR(dest_net) : 0,
		 "get dest net");
	for (i = 1; i < priv->nodes; i++) {
		rto_ifname(name, tb, master);
		node = rtnl_create_link(net, dest_net, name, &torus_rtnl, tb);
		gotonerr(err_create_sub_node,
			 err = IS_ERR(node) ? PTR_ERR(node) : 0,
			 "create %s", name);
		memcpy(node->dev_addr, dev->dev_addr, TORUS_ALEN);
		node->dev_addr[4] += i;		/* dev_addr[5] for clones */
		gotonerr(err_init_sub_node, err = rto_init_node(node, 0),
			 "init %s", name);
		priv->node[i] = node;
	}
	rto_assign_ports(priv, rows, cols);
	put_net(dest_net);
	return 0;
err_init_sub_node:
err_create_sub_node:
	put_net(dest_net);
err_dest_net:
	netdev_rx_handler_unregister(dev);
	unregister_netdevice(dev);
	return err;
}

static void rto_dellink(struct net_device *dev, struct list_head *head)
{
	struct	torus *priv = netdev_priv(dev);
	struct	net_device **port;
	char	name[IFNAMSIZ], addr[TORUS_ALEN];
	int	i;

	memcpy(name, dev->name, sizeof(name));
	memcpy(addr, dev->dev_addr, sizeof(addr));
	for (i = 1; i < priv->nodes; i++)
		rto_dellink(priv->node[i], head);
	port = rcu_dereference(priv->port);
	for (i = 1; i < priv->ports; i++)
		if (port[i] && port[i]->master == dev) {
			if (is_torus(port[i]))
				rto_dellink(port[i], head);
			else
				unset_torus_master(dev, port[i]);
		}
	if (dev->master && is_torus(dev->master))
		unset_torus_master(dev->master, dev);
	netdev_rx_handler_unregister(dev);
	unregister_netdevice_queue(dev, head);
	pr_torus_info("del %*s %pM", IFNAMSIZ, dev->name, dev->dev_addr);
}

struct rtnl_link_ops torus_rtnl = {
	.kind		= TORUS,
	.maxtype	= TORUS_LAST_ATTR,
	.policy		= rto_policy,
	.validate	= rto_validate,
	.priv_size	= sizeof(struct torus),
	.setup		= rto_setup,
	.newlink	= rto_newlink,
	.dellink	= rto_dellink
};
