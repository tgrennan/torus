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

#define assert_this(condition,recourse,...)				\
	do {								\
		if (!(condition)) {					\
			TORUS_ERR(__VA_ARGS__);				\
			recourse;					\
		}							\
	} while(0)
#define	assert_this_attr(_attr_)					\
	assert_this(data[TORUS_##_attr_##_ATTR],			\
		    return -EINVAL, "missing %s.\n", #_attr_)
#define	assert_this_attr_range(_attr,x)					\
	assert_this((x) >= TORUS_MIN_##_attr 				\
		    && (x) <= TORUS_MAX_##_attr,			\
		    return -ERANGE, "Out of range %s.\n", #_attr)
#define	this_attr(_attr_)						\
	nla_get_u32(data[TORUS_##_attr_##_ATTR])
#define	this_optional_attr(_attr_,_default_)				\
	(data[TORUS_##_attr_##_ATTR])					\
	? nla_get_u32(data[TORUS_##_attr_##_ATTR])			\
	: (_default_)

static const struct nla_policy this_policy[];
struct rtnl_link_ops torus_rtnl;

static struct net_device *this_dev_by_name_rcu(struct net *net,
					       const char *ifname)
{
	struct	net_device *dev = NULL;

	/* first look in the given name space */
	if (net)
		dev = dev_get_by_name_rcu(net, ifname);
	/* then look in all name spaces if not found or no name space given */
	if (!dev)
		for_each_net_rcu(net)
			if (dev = dev_get_by_name_rcu(net, ifname), dev)
				break;
	return dev;
}

static struct net_device *this_dev_by_name(struct net *net, const char *ifname)
{
	struct	net_device *dev;

	rcu_read_lock();
	dev = this_dev_by_name_rcu(net, ifname);
	rcu_read_unlock();
	return dev;
}

static struct net_device *this_dev_by_attr_rcu(struct net *net,
					       const struct nlattr *attr)
{
	char	ifname[IFNAMSIZ];
	struct	net_device *dev = NULL;

	if (attr) {
		nla_strlcpy(ifname, attr, IFNAMSIZ);
		dev = this_dev_by_name_rcu(net, ifname);
	}
	return dev;
}

static struct net_device *this_dev_by_attr(struct net *net,
					       const struct nlattr *attr)
{
	struct	net_device *dev;

	rcu_read_lock();
	dev = this_dev_by_attr_rcu(net, attr);
	rcu_read_unlock();
	return dev;
}

static void free_this_nodes_tbls(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
	int	i, n;

	n = torus_entries(priv->node.nodelu);
	for (i = 0; i < n; i++) {
		kfree(priv->node.nodelu[i]);
		priv->node.nodelu[i] = NULL;
	}
	n = torus_entries(priv->node.portlu);
	for (i = 0; i < n; i++) {
		kfree(priv->node.portlu[i]);
		priv->node.portlu[i] = NULL;
	}
	kfree(priv->node.clone_dev);
	priv->node.clone_dev = NULL;
}

static void this_destructor(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	free_percpu_counters(&priv->rx);
	free_percpu_counters(&priv->tx);
	if (priv->mode == TORUS_NODE_DEV)
		free_this_nodes_tbls(dev);
	else if (priv->mode == TORUS_TOROID_DEV)
		kfree(priv->toroid.node_dev);
	free_netdev(dev);
}

static void this_setup(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	alloc_percpu_counters(&priv->rx);
	alloc_percpu_counters(&priv->tx);
	ether_setup(dev);
	dev->priv_flags &= ~IFF_TX_SKB_SHARING;
	dev->netdev_ops = &torus_netdev;
	dev->ethtool_ops = &torus_ethtool;
	dev->features |= NETIF_F_LLTX;
	dev->destructor = this_destructor;
	dev->hw_features = NETIF_F_HW_CSUM | NETIF_F_SG | NETIF_F_RXCSUM;
}

static inline int this_validate_clone(struct nlattr *tb[],
				      struct nlattr *data[])
{
	struct	net_device *node_dev;
	struct	torus *node_priv;
	u32	clone_id;
	int	i;

	assert_this_attr(NODE);
	node_dev = this_dev_by_attr(NULL, data[TORUS_NODE_ATTR]);
	assert_this(node_dev != NULL, return -ENODEV, "no such node.\n");
	assert_this(is_torus(node_dev), return -EINVAL,
		    "%s isn't a torus device.\n", node_dev->name);
	node_priv = netdev_priv(node_dev);
	assert_this(node_priv->mode == TORUS_NODE_DEV, return -EINVAL,
		    "%s isn't a torus node.\n", node_dev->name);
	if (!node_priv->node.clones || !node_priv->node.clone_dev)
		return -EADDRNOTAVAIL;
	if (data[TORUS_CLONE_ID_ATTR]) {
		clone_id =  this_attr(CLONE_ID);
		assert_this(clone_id >= TORUS_MIN_CLONE_ID
			    && clone_id <= node_priv->node.clones,
			    return -ERANGE, "Out of range clone ID.\n");
		assert_this(node_priv->node.clone_dev[clone_id] == NULL,
			    return -EADDRINUSE, "%u exists.\n", clone_id);
		return 0;
	}
	for (i = 0; i < node_priv->node.clones; i++)
		if (node_priv->node.clone_dev[i] == NULL)
			return 0;
	TORUS_ERR("no more clones for %s\n", node_dev->name);
	return -EADDRNOTAVAIL;
}

static inline int this_validate_node(struct nlattr *tb[],
				     struct nlattr *data[])
{
	char    ifname[IFNAMSIZ];
	struct  net_device *dev;
	u32	toroid_id, node_id, clones;

	assert_this_attr(TOROID_ID);
	assert_this_attr(NODE_ID);
	toroid_id = this_attr(TOROID_ID);
	assert_this_attr_range(TOROID_ID, toroid_id);
	node_id = this_attr(NODE_ID);
	assert_this_attr_range(NODE_ID, node_id);
	clones = this_optional_attr(CLONES, TORUS_MIN_CLONES);
	assert_this_attr_range(CLONES, clones);
	torus_node_ifname(ifname, toroid_id, node_id);
	dev = this_dev_by_name(NULL, ifname);
	assert_this(dev == NULL, return -ENOTUNIQ, "%s exists.\n", ifname);
	return 0;
}

static inline int this_validate_toroid(struct nlattr *tb[],
				       struct nlattr *data[])
{
	u32	toroid_id, rows, cols;

	assert_this_attr(TOROID_ID);
	toroid_id = this_attr(TOROID_ID);
	assert_this_attr_range(TOROID_ID, toroid_id);
	rows = this_optional_attr(ROWS, TORUS_DEFAULT_ROWS);
	assert_this_attr_range(ROWS, rows);
	cols = this_optional_attr(COLS, TORUS_DEFAULT_COLS);
	assert_this_attr_range(COLS, cols);
	return 0;
}

static int this_validate(struct nlattr *tb[], struct nlattr *data[])
{
	u32	version;

	assert_this(data, return -ENODATA, "no attributes.\n");
	assert_this(tb[IFLA_ADDRESS] == NULL, return -EADDRNOTAVAIL,
		    "can't set torus link address.\n");
	if (tb[IFLA_MTU])
		assert_this(torus_mtu_ok(nla_get_u32(tb[IFLA_MTU])),
			    return -ERANGE, "invalid MTU.\n");
	assert_this_attr(VERSION);
	version = this_attr(VERSION);
	assert_this_attr_range(VERSION, version);
	if (data[TORUS_NODE_ATTR])
		return this_validate_clone(tb, data);
	else if (data[TORUS_NODE_ID_ATTR])
		return this_validate_node(tb, data);
	else
		return this_validate_toroid(tb, data);
}

static inline int this_newlink_clone(struct net *net,
				     struct net_device *dev,
				     struct nlattr *tb[],
				     struct nlattr *data[])
{
	struct	torus *priv = netdev_priv(dev);
	struct	net_device *master_dev;
	struct	torus *master_priv;
	u32	toroid_id, node_id, clone_idx, clones;
	int	err;

	priv->mode = TORUS_CLONE_DEV;
	master_dev = this_dev_by_attr(net, data[TORUS_NODE_ATTR]);
	master_priv = netdev_priv(master_dev);
	clones = master_priv->node.clones;
	if (data[TORUS_CLONE_ID_ATTR])
		clone_idx = this_attr(CLONE_ID) - 1;
	else
		for (clone_idx = 0; clone_idx < clones; clone_idx++)
			if (master_priv->node.clone_dev[clone_idx] == NULL)
				break;
	priv->clone.clone_id = clone_idx + 1;
	toroid_id = master_priv->node.toroid_id;
	node_id = master_priv->node.node_id;
	torus_set_hw_addr(dev);
	torus_clone_ifname(dev->name, toroid_id, node_id, priv->clone.clone_id);
	err = torus_set_master(master_dev, dev);
	if (err < 0)
		return err;
	err = register_netdevice(dev);
	if (err < 0)
		return err;
	torus_create_sysfs(dev);
	netif_carrier_off(dev);
	return 0;
}

static inline int this_new_node(struct	net *net,
				struct	net_device *dev,
				u32	toroid_id,
				u32	node_id,
				u32	clones)
{
	struct	torus *priv = netdev_priv(dev);
	int	i, n, err;

	priv->mode = TORUS_NODE_DEV;
	spin_lock_init(&priv->node.lock);
	priv->node.toroid_id = toroid_id;
	priv->node.node_id = node_id;
	torus_set_hw_addr(dev);
	priv->node.clones = clones;
	n = torus_entries(priv->node.nodelu);
	for (i = 0; i < n; i++)
		if (!torus_init_lu_tbl(&priv->node.nodelu[i], TORUS_TOROIDS,
				       TORUS_NODES))
			goto enomem;
	n = torus_entries(priv->node.portlu);
	for (i = 0; i < n; i++)
		if (!torus_init_lu_tbl(&priv->node.portlu[i], TORUS_NODES,
				       TORUS_PORTS))
			goto enomem;
	if (clones) {
		size_t	sz = sizeof(*priv->node.clone_dev);
		priv->node.clone_dev = kcalloc(clones, sz, GFP_KERNEL);
		if (!priv->node.clone_dev)
			goto enomem;
	}
	if (err = register_netdevice(dev), err)
		goto egress;
	netif_carrier_off(dev);
	torus_create_sysfs(dev);
	return 0;
enomem:
	err = -ENOMEM;
egress:
	free_this_nodes_tbls(dev);
	return err;
}

static inline int this_newlink_node(struct net *net,
				    struct net_device *dev,
				    struct nlattr *tb[],
				    struct nlattr *data[])
{
	u32	toroid_id, node_id, clones;

	toroid_id = this_attr(TOROID_ID);
	node_id = this_attr(NODE_ID);
	clones = this_optional_attr(CLONES, TORUS_MIN_CLONES);
	torus_node_ifname(dev->name, toroid_id, node_id);
	return this_new_node(net, dev, toroid_id, node_id, clones);
}

static inline int this_new_node_ports(struct net *src_net, struct net *new_net,
				      struct net_device *node_dev,
				      struct nlattr *tb[])
{
	struct	torus *port_priv, *priv = netdev_priv(node_dev);
	char	ifname[IFNAMSIZ];
	struct	net_device *port_dev;
	u32	toroid_id, node_id, port_id;
	int	err;

	toroid_id = priv->node.toroid_id;
	node_id = priv->node.node_id;
	for (port_id = 0; port_id < 4; port_id++) {
		torus_port_ifname(ifname, toroid_id, node_id, port_id);
		port_dev = rtnl_create_link(src_net, new_net, ifname,
					    &torus_rtnl, tb);
		if (IS_ERR(port_dev))
		    return PTR_ERR(port_dev);
		port_priv = netdev_priv(port_dev);
		port_priv->mode = TORUS_PORT_DEV;
		port_priv->port.port_id = port_id;
		err = torus_set_master(node_dev, port_dev);
		if (err)
			return err;
		torus_set_hw_addr(port_dev);
		err = register_netdevice(port_dev);
		if (err)
			return err;
		netif_carrier_off(port_dev);
		torus_create_sysfs(port_dev);
	}
	return 0;
}

static inline int this_new_toroid_nodes(struct net *src_net,
					struct net_device *dev,
					struct nlattr *tb[],
					u32 clones)
{
	struct	torus *priv = netdev_priv(dev);
	char	ifname[IFNAMSIZ];
	struct	net *new_net;
	struct	net_device *node_dev;
	u32	node_id;
	int	err;

	new_net = rtnl_link_get_net(src_net, tb);
	if (IS_ERR(new_net))
		return PTR_ERR(new_net);
	for (node_id = 0; node_id < priv->toroid.nodes; node_id++) {
		torus_node_ifname(ifname, priv->toroid.toroid_id, node_id);
		node_dev = rtnl_create_link(src_net, new_net, ifname,
					    &torus_rtnl, tb);
		if (IS_ERR(node_dev)) {
			err = PTR_ERR(node_dev);
			goto egress;
		}
		err = this_new_node(new_net, node_dev, priv->toroid.toroid_id,
				    node_id, clones);
		if (err)
			goto egress;
		err = torus_set_master(dev, node_dev);
		if (err)
			goto egress;
		err = this_new_node_ports(src_net, new_net, node_dev, tb);
		if (err)
			goto egress;
	}
	err = 0;
egress:
	put_net(new_net);
	return err;
}

static inline void this_assign_peers(struct torus *priv, u32 rows, u32 cols)
{
	struct	net_device *node_dev, *neighbor_node_dev;
	struct	net_device *port_dev, *neighbor_port_dev;
	struct	torus *node_priv, *neighbor_node_priv;
	struct	torus *port_priv, *neighbor_port_priv;
	u32	node_id, neighbor_id, nodes;

	nodes =  rows * cols;
	for (node_id = 0; node_id < nodes; node_id++) {
		node_dev = priv->toroid.node_dev[node_id];
		node_priv = netdev_priv(node_dev);

		port_dev = node_priv->node.port_dev[0];
		port_priv = netdev_priv(port_dev);
		neighbor_id = (node_id + cols) % nodes;
		neighbor_node_dev = priv->toroid.node_dev[neighbor_id];
		neighbor_node_priv = netdev_priv(neighbor_node_dev);
		neighbor_port_dev = neighbor_node_priv->node.port_dev[2];
		neighbor_port_priv = netdev_priv(neighbor_port_dev);
		port_priv->port.peer = neighbor_port_dev;
		neighbor_port_priv->port.peer = port_dev;

		port_dev = node_priv->node.port_dev[1];
		port_priv = netdev_priv(port_dev);
		neighbor_id =  node_id + 1;
		if (neighbor_id % cols == 0)
			neighbor_id -= cols;
		neighbor_node_dev = priv->toroid.node_dev[neighbor_id];
		neighbor_node_priv = netdev_priv(neighbor_node_dev);
		neighbor_port_dev = neighbor_node_priv->node.port_dev[3];
		neighbor_port_priv = netdev_priv(neighbor_port_dev);
		port_priv->port.peer = neighbor_port_dev;
		neighbor_port_priv->port.peer = port_dev;
	}
}

static inline int this_newlink_toroid(struct net *src_net,
				      struct net_device *dev,
				      struct nlattr *tb[],
				      struct nlattr *data[])
{
	struct	torus *priv = netdev_priv(dev);
	u32	rows, cols, clones;
	size_t	sz;
	int	err;

	priv->mode = TORUS_TOROID_DEV;
	sz = sizeof(*priv->toroid.node_dev);
	priv->toroid.node_dev = kcalloc(TORUS_NODES, sz, GFP_KERNEL);
	if (!priv->toroid.node_dev)
		return -ENOMEM;
	priv->toroid.toroid_id = this_attr(TOROID_ID);
	torus_toroid_ifname(dev->name, priv->toroid.toroid_id);
	rows = this_optional_attr(ROWS, TORUS_DEFAULT_ROWS);
	cols = this_optional_attr(COLS, TORUS_DEFAULT_COLS);
	priv->toroid.nodes = rows * cols;
	clones = this_optional_attr(CLONES, TORUS_MIN_CLONES);
	torus_set_hw_addr(dev);
	err = register_netdevice(dev);
	if (err < 0)
		return err;
	netif_carrier_off(dev);
	err = this_new_toroid_nodes(src_net, dev, tb, clones);
	if (err < 0)
		return err;
	this_assign_peers(priv, rows, cols);
	torus_create_sysfs(dev);
	return 0;
}

static int this_newlink(struct net *net, struct net_device *dev,
			struct nlattr *tb[], struct nlattr *data[])
{
	if (data[TORUS_NODE_ATTR])
		return this_newlink_clone(net, dev, tb, data);
	else if (data[TORUS_NODE_ID_ATTR])
		return this_newlink_node(net, dev, tb, data);
	else
		return this_newlink_toroid(net, dev, tb, data);
}

static void this_dellink(struct net_device *dev, struct list_head *head)
{
	struct	torus *priv = netdev_priv(dev);
	struct	net_device *sub;
	int	i;

	switch (priv->mode) {
	case TORUS_TOROID_DEV:
		for (i = 0; i < priv->toroid.nodes; i++)
			if (sub = priv->toroid.node_dev[i], sub != NULL)
				this_dellink(sub, head);
		break;
	case TORUS_NODE_DEV:
		if (priv->node.clone_dev) {
			for (i = 0; i < priv->node.clones; i++)
				if (sub = priv->node.clone_dev[i], sub != NULL)
					this_dellink(sub, head);
		}
		for_each_torus_port(i)
			if (sub = priv->node.port_dev[i], sub != NULL) {
				if (is_torus(sub))
					this_dellink(sub, head);
				else
					torus_unset_master(dev, sub);
			}
		break;
	case TORUS_CLONE_DEV:
		break;
	case TORUS_PORT_DEV:
		if (priv->port.peer) {
			priv = netdev_priv(priv->port.peer);
			priv->port.peer = NULL;
		}
		break;
	default:
		TORUS_ERR("unexpected mode\n");
		break;
	}
	if (dev->master && is_torus(dev->master))
		torus_unset_master(dev->master, dev);
	unregister_netdevice_queue(dev, head);
}

static const struct nla_policy this_policy[] = {
	[TORUS_VERSION_ATTR]	= { .type = NLA_U32 },
	[TORUS_TOROID_ID_ATTR]	= { .type = NLA_U32 },
	[TORUS_NODE_ID_ATTR]	= { .type = NLA_U32 },
	[TORUS_CLONE_ID_ATTR]	= { .type = NLA_U32 },
	[TORUS_CLONES_ATTR]	= { .type = NLA_U32 },
	[TORUS_ROWS_ATTR]	= { .type = NLA_U32 },
	[TORUS_COLS_ATTR]	= { .type = NLA_U32 },
	[TORUS_NODE_ATTR]	= { .type = NLA_STRING, .len = IFNAMSIZ }
};

struct rtnl_link_ops torus_rtnl = {
	.kind		= TORUS,
	.maxtype	= TORUS_LAST_ATTR,
	.policy		= this_policy,
	.validate	= this_validate,
	.priv_size	= sizeof(struct torus),
	.setup		= this_setup,
	.newlink	= this_newlink,
	.dellink	= this_dellink
};
