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
#define	assert_this_attr(attr)						\
	assert_this(data[TORUS_ ## attr ##_ATTR],			\
		    return -EINVAL, "missing %s.\n", # attr)
#define	assert_this_attr_range(attr,x)					\
	assert_this((x) >= TORUS_MIN_##attr && (x) <= TORUS_MAX_##attr, \
		    return -ERANGE, "Out of range %s.\n", # attr)
#define	this_attr(attr)							\
	nla_get_u32(data[TORUS_ ## attr ## _ATTR])
#define	this_optional_attr(attr,default)				\
	(data[TORUS_ ## attr ## _ATTR])					\
	? nla_get_u32(data[TORUS_ ## attr ## _ATTR])			\
	: (default)
#define	alloc_this_tbl(tbl,n)						\
	if ((tbl) = kcalloc((n), sizeof(*(tbl)), GFP_KERNEL), !(tbl))	\
		goto enomem
#define	clean_this_tbl(tbl)						\
	do {								\
		if (tbl) {						\
			kfree(tbl);					\
			(tbl) = NULL;					\
		}							\
	} while (0)

static const struct nla_policy this_policy[];
struct rtnl_link_ops torus_rtnl;


static struct net_device *this_dev_get_by_name_rcu(struct net *net,
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

static struct net_device *this_dev_get_by_name(struct net *net,
					       const char *ifname)
{
	struct	net_device *dev;

	rcu_read_lock();
	dev = this_dev_get_by_name_rcu(net, ifname);
	rcu_read_unlock();
	return dev;
}

static struct net_device *this_dev_get_by_attr_rcu(struct net *net,
						   const struct nlattr *attr)
{
	char	ifname[IFNAMSIZ];
	struct	net_device *dev = NULL;

	if (attr) {
		nla_strlcpy(ifname, attr, IFNAMSIZ);
		dev = this_dev_get_by_name_rcu(net, ifname);
	}
	return dev;
}

static struct net_device *this_dev_get_by_attr(struct net *net,
					       const struct nlattr *attr)
{
	struct	net_device *dev;

	rcu_read_lock();
	dev = this_dev_get_by_attr_rcu(net, attr);
	rcu_read_unlock();
	return dev;
}

static void this_destructor(struct net_device *dev)
{
	struct torus *t = netdev_priv(dev);

	free_percpu_counters(&t->rx);
	free_percpu_counters(&t->tx);
	switch (t->mode) {
	case TORUS_NODE_DEV:
		clean_this_tbl(t->node.clone_dev);
		clean_this_tbl(t->node.port_dev);
		clean_this_tbl(t->node.toroid2node);
		clean_this_tbl(t->node.node2port);
		break;
	case TORUS_TOROID_DEV:
		clean_this_tbl(t->toroid.node_dev);
		break;
	default:
		break;
	}
	free_netdev(dev);
}

static void this_setup(struct net_device *dev)
{
	struct torus *t = netdev_priv(dev);

	alloc_percpu_counters(&t->rx);
	alloc_percpu_counters(&t->tx);
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
	struct	net_device *dev;
	struct	torus *t;
	u32	clone_id;

	assert_this_attr(NODE);
	dev = this_dev_get_by_attr(NULL, data[TORUS_NODE_ATTR]);
	assert_this(dev != NULL, return -ENODEV, "no such node.\n");
	assert_this(is_torus(dev), return -EINVAL,
		    "%s isn't a torus device.\n", dev->name);
	t = netdev_priv(dev);
	assert_this(t->mode == TORUS_NODE_DEV, return -EINVAL,
		    "%s isn't a torus node.\n", dev->name);
	if (data[TORUS_CLONE_ID_ATTR]) {
		clone_id =  this_attr(CLONE_ID);
		assert_this(clone_id < t->node.clones, return -ERANGE,
			    "%u exceeds %s->clones.\n", clone_id, dev->name);
		assert_this(t->node.clone_dev[clone_id] == NULL,
			    return -EADDRINUSE, "%u exists.\n", clone_id);
		return 0;
	}
	for_each_torus_clone(clone_id, t->node.clones)
		if (t->node.clone_dev[clone_id] == NULL)
			return 0;
	TORUS_ERR("no more clones for %s\n", dev->name);
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
	dev = this_dev_get_by_name(NULL, ifname);
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
	struct	torus *t = netdev_priv(dev);
	struct	net_device *master;
	struct	torus *tn;
	u32	clone_id;
	int	err;

	t = netdev_priv(dev);
	t->mode = TORUS_CLONE_DEV;
	master = this_dev_get_by_attr(net, data[TORUS_NODE_ATTR]);
	tn = netdev_priv(master);
	if (data[TORUS_CLONE_ID_ATTR])
		clone_id = this_attr(CLONE_ID);
	else
		for_each_torus_clone(clone_id, tn->node.clones)
			if (tn->node.clone_dev[clone_id] == NULL)
				break;
	t->clone.clone_id = clone_id;
	torus_set_hw_addr(dev);
	torus_clone_ifname(dev->name, tn->node.toroid_id, tn->node.node_id,
			   clone_id);
	if (err = torus_set_master(master, dev), err < 0)
		return err;
	if (err = register_netdevice(dev), err < 0)
		return err;
	netif_carrier_off(dev);
	return 0;
}

static inline int this_new_node(struct	net *net,
				struct	net_device *dev,
				u32	toroid_id,
				u32	node_id,
				u32	clones)
{
	struct	torus *t = netdev_priv(dev);
	int	err;

	t->mode = TORUS_NODE_DEV;
	t->node.toroid_id = toroid_id;
	t->node.node_id = node_id;
	torus_set_hw_addr(dev);
	t->node.clones = clones;
	t->node.clones += 1;
	alloc_this_tbl(t->node.clone_dev, t->node.clones);
	alloc_this_tbl(t->node.port_dev, TORUS_PORTS);
	alloc_this_tbl(t->node.toroid2node, TORUS_TOROIDS);
	alloc_this_tbl(t->node.node2port, TORUS_NODES);
	if (err = register_netdevice(dev), err)
		goto egress;
	netif_carrier_off(dev);
	return 0;
enomem:
	err = -ENOMEM;
egress:
	clean_this_tbl(t->node.clone_dev);
	clean_this_tbl(t->node.port_dev);
	clean_this_tbl(t->node.toroid2node);
	clean_this_tbl(t->node.node2port);
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

static inline struct net_device *this_new_port(struct	net *oldnet,
					       struct	net *newnet,
					       struct	net_device *node_dev,
					       struct	nlattr *tb[],
					       int	port_id)
{
	char	ifname[IFNAMSIZ];
	struct	torus *nt = netdev_priv(node_dev);
	struct	torus *pt;
	struct	net_device *port_dev;
	int	err;

	torus_port_ifname(ifname, nt->node.toroid_id, nt->node.node_id,
			  port_id);
	port_dev = rtnl_create_link(oldnet, newnet, ifname, &torus_rtnl, tb);
	if (IS_ERR(port_dev))
		return ERR_CAST(node_dev);
	pt = netdev_priv(port_dev);
	pt->mode = TORUS_PORT_DEV;
	pt->port.port_id = port_id;
	err = torus_set_master(node_dev, port_dev);
	if (err)
		return ERR_PTR(err);
	return port_dev;
}

static inline void this_assign_peers(struct net_device *toroid_dev,
				     u32 rows, u32 cols)
{
	struct	torus *toroid;
	struct	net_device *node_dev, *neighbor_node_dev;
	struct	net_device *port_dev, *neighbor_port_dev;
       	struct	torus *node, *neighbor_node;
       	struct	torus *port, *neighbor_port;
	u32	node_id, neighbor_id, nodes;

	toroid = netdev_priv(toroid_dev);
	nodes =  rows * cols;
	for_each_torus_node(node_id, nodes) {
		node_dev = toroid->toroid.node_dev[node_id];
		node = netdev_priv(node_dev);
		port_dev =  node->node.port_dev[0];
		port = netdev_priv(port_dev);
		neighbor_id = (node_id + cols) % nodes;
		neighbor_node_dev = toroid->toroid.node_dev[neighbor_id];
		neighbor_node = netdev_priv(neighbor_node_dev);
		neighbor_port_dev = neighbor_node->node.port_dev[2];
		neighbor_port = netdev_priv(neighbor_port_dev);
		port->port.peer = neighbor_port_dev;
		neighbor_port->port.peer = port_dev;

		port_dev =  node->node.port_dev[1];
		port = netdev_priv(port_dev);
		neighbor_id =  node_id + 1;
		if (neighbor_id % cols == 0)
			neighbor_id -= cols;
		neighbor_node_dev = toroid->toroid.node_dev[neighbor_id];
		neighbor_node = netdev_priv(neighbor_node_dev);
		neighbor_port_dev = neighbor_node->node.port_dev[3];
		neighbor_port = netdev_priv(neighbor_port_dev);
		port->port.peer = neighbor_port_dev;
		neighbor_port->port.peer = port_dev;
	}
}

static inline int this_newlink_toroid(struct net *src_net,
				      struct net_device *toroid,
				      struct nlattr *tb[],
				      struct nlattr *data[])
{
	struct	torus *t;
	struct	net *net;
	struct	net_device *node, *port;
	char	ifname[IFNAMSIZ];
	u32	toroid_id, node_id, port_id, rows, cols, nodes, clones;
	int	err;
#define	__trap(expr)	if (err = (expr), err < 0) goto egress

	t = netdev_priv(toroid);
	t->mode = TORUS_TOROID_DEV;
	toroid_id = t->toroid.toroid_id = this_attr(TOROID_ID);
	torus_toroid_ifname(toroid->name, toroid_id);
	rows = this_optional_attr(ROWS, TORUS_DEFAULT_ROWS);
	cols = this_optional_attr(COLS, TORUS_DEFAULT_COLS);
	nodes = t->toroid.nodes = rows * cols;
	clones = this_optional_attr(CLONES, TORUS_MIN_CLONES);
	alloc_this_tbl(t->toroid.node_dev, nodes);
	torus_set_hw_addr(toroid);
	if (err = register_netdevice(toroid), err < 0)
		return err;
	netif_carrier_off(toroid);
	net = rtnl_link_get_net(src_net, tb);
	if (IS_ERR(net))
		return PTR_ERR(net);
	for_each_torus_node(node_id, nodes) {
		torus_node_ifname(ifname, toroid_id, node_id);
		node = rtnl_create_link(src_net, net, ifname, &torus_rtnl, tb);
		__trap(IS_ERR(node) ? PTR_ERR(node) : 0 );
		__trap(this_new_node(net, node, toroid_id, node_id, clones));
		__trap(torus_set_master(toroid, node));
		for (port_id = 0; port_id < 4; port_id++) {
			torus_port_ifname(ifname, toroid_id, node_id, port_id);
			port = rtnl_create_link(src_net, net, ifname,
						&torus_rtnl, tb);
			__trap(IS_ERR(port) ? PTR_ERR(port) : 0);
			t = netdev_priv(port);
			t->mode = TORUS_PORT_DEV;
			t->port.port_id = port_id;
			__trap(torus_set_master(node, port));
			torus_set_hw_addr(port);
			__trap(register_netdevice(port));
			netif_carrier_off(port);
		}
	}
	this_assign_peers(toroid, rows, cols);
	err = 0;
#undef __trap
egress:
	put_net(net);
	return err;
enomem:
	return -ENOMEM;
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
	struct	torus *t = netdev_priv(dev);
	struct	net_device *sub;
	int	i;

	switch (t->mode) {
	case TORUS_TOROID_DEV:
		for_each_torus_node(i, t->toroid.nodes)
			if (sub = t->toroid.node_dev[i], sub != NULL)
				this_dellink(sub, head);
		break;
	case TORUS_NODE_DEV:
		for_each_torus_clone(i, t->node.clones)
			if (sub = t->node.clone_dev[i], sub != NULL)
				this_dellink(sub, head);
		for_each_torus_port(i)
			if (sub = t->node.port_dev[i], sub != NULL) {
				if (is_torus(sub))
					this_dellink(sub, head);
				else
					torus_unset_master(dev, sub);
			}
		break;
	case TORUS_CLONE_DEV:
		break;
	case TORUS_PORT_DEV:
		if (t->port.peer) {
			t = netdev_priv(t->port.peer);
			t->port.peer = NULL;
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
