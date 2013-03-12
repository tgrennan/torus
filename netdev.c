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

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/torus.h>
#include <torus.h>

static int this_open(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	switch (priv->mode) {
	case TORUS_NODE_DEV:
		{
			int	i;
			for_each_torus_port(i)
				if (priv->node.port_dev[i])
					netif_carrier_on(priv->node.port_dev[i]);
		}
		break;
	case TORUS_CLONE_DEV:
	case TORUS_PORT_DEV:
	case TORUS_TOROID_DEV:
	case TORUS_UNKNOWN_DEV:
		/* FIXME should these ever be opened? */
		break;
	}
	netif_carrier_on(dev);
	return 0;
}

static int this_close(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	netif_carrier_off(dev);
	switch (priv->mode) {
	case TORUS_NODE_DEV:
		{
			int	i;
			for_each_torus_port(i)
				if (priv->node.port_dev[i])
					netif_carrier_off(priv->node.port_dev[i]);
		}
		break;
	case TORUS_CLONE_DEV:
	case TORUS_PORT_DEV:
	case TORUS_TOROID_DEV:
	case TORUS_UNKNOWN_DEV:
		/* FIXME should these ever be opened? */
		break;
	}
	return 0;
}

static int this_init(struct net_device *dev)
{
	return 0;
}

static rx_handler_result_t this_rx(struct sk_buff **pskb)
{
	struct	net_device *dev = (*pskb)->dev;
	struct	torus *priv = netdev_priv(dev->master);
#if 0
	struct	torus *t = rcu_dereference(dev->rx_handler_data);
#endif
	struct	ethhdr *e = eth_hdr(*pskb);

	if (torus_addr_is_node(e->h_source, priv))
		goto consume;
#if 0
	/* FIXME for port vs node vs peer */
	if (torus_addr_is_node(e->h_dest, t)) {
		clone = torus_addr_get_clone(e->h_dest);
		if (t->clone_dev[clone]) {
			torus_addr_set_ttl(e->h_dest, 0);
			if (NET_RX_DROP	==
			    dev_forward_skb(t->clone_dev[clone],
					    *pskb))
				count_drop(&priv->rx);
			else
				count_packet(&priv->rx, (*pskb)->len);
			return RX_HANDLER_CONSUMED;
		} else {
			count_error(&priv->rx);
			goto consume;
		}
	}
#else
	goto consume;
#endif	/* FIXME */
#if 0
	What about broadcast and multicast?
#endif	/* FIXME */
	return RX_HANDLER_PASS;
consume:
	consume_skb(*pskb);
	return RX_HANDLER_CONSUMED;
}

static netdev_tx_t this_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);

	/* FIXME */
	count_error(&priv->tx);
	return NETDEV_TX_OK;
}

static int this_change_mtu(struct net_device *dev, int mtu)
{
	if (!torus_mtu_ok(mtu))
		return -EINVAL;
	dev->mtu = mtu;
	return 0;
}

static struct rtnl_link_stats64 *
this_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *cnt)
{
	struct torus *priv = netdev_priv(dev);

	accumulate_counters(&priv->rx);
	accumulate_counters(&priv->tx);
	cnt->rx_packets	+= priv->rx.packets;
	cnt->tx_packets	+= priv->tx.packets;
	cnt->rx_bytes	+= priv->rx.bytes;
	cnt->tx_bytes	+= priv->tx.bytes;
	cnt->rx_errors	+= priv->rx.errors;
	cnt->tx_errors	+= priv->tx.errors;
	cnt->rx_dropped	+= priv->rx.drops;
	cnt->tx_dropped	+= priv->tx.drops;
	return cnt;
}

static inline int register_this_rx(struct net_device *master,
				   struct net_device *dev)
{
	return netdev_rx_handler_register(dev, this_rx, master);
}

static void this_invalid_master(struct net_device *master,
				struct net_device *dev)
{
	netdev_err(master, "shouldn't be master of %s.\n", dev->name);
}

static int this_set_master(struct net_device *master, struct net_device *dev)
{
	struct torus *master_priv = netdev_priv(master);
	struct torus *priv = netdev_priv(dev);
	int	i, err;
	static	const char err_reg_rx[] = "failed to register rx handler.\n";

	err = netdev_set_master(dev, master);
	if (err) {
		netdev_err(dev, "failed to make %s master.\n", master->name);
		return err;
	}
	switch (master_priv->mode) {
	case TORUS_TOROID_DEV:
		if (is_torus(dev)) {
			i = priv->node.node_id;
			master_priv->toroid.node_dev[i] = dev;
		} else
			err = -EINVAL;
		break;
	case TORUS_NODE_DEV:
		if (!is_torus(dev)) {
			for_each_torus_port(i)
				if (!master_priv->node.port_dev[i])
					break;
			if (i < TORUS_PORTS) {
				err = register_this_rx(master, dev);
				if (err >= 0)
					master_priv->node.port_dev[i] = dev;
				else
					netdev_err(dev, err_reg_rx);
			} else
				err = -EADDRNOTAVAIL;
		} else
			switch (priv->mode) {
			case TORUS_CLONE_DEV:
				i = priv->clone.clone_id - 1;
				master_priv->node.clone_dev[i] = dev;
				break;
			case TORUS_PORT_DEV:
				err = register_this_rx(master, dev);
				if (err >= 0) {
					i = priv->port.port_id;
					master_priv->node.port_dev[i] = dev;
				} else
					netdev_err(dev, err_reg_rx);
				break;
			default:
				netdev_err(dev, "isn't a clone or port.\n");
				err = -EINVAL;
			}
		break;
	default:
		this_invalid_master(master, dev);
		err = -EINVAL;
	}
	if (err)
		netdev_set_master(dev, NULL);
	return err;
}

static int this_unset_master(struct net_device *master, struct net_device *dev)
{
	struct torus *master_priv = netdev_priv(master);
	struct torus *priv = netdev_priv(dev);
	int	i;

	switch (master_priv->mode) {
	case TORUS_TOROID_DEV:
		if (is_torus(dev))
			if (priv->mode == TORUS_NODE_DEV) {
				i = priv->node.node_id;
				master_priv->toroid.node_dev[i] = NULL;
			}
		break;
	case TORUS_NODE_DEV:
		if (!is_torus(dev)) {
			for_each_torus_port(i)
				if (master_priv->node.port_dev[i] == dev) {
					master_priv->node.port_dev[i] = NULL;
					break;
				}
			netdev_rx_handler_unregister(dev);
		} else
			switch (priv->mode) {
			case TORUS_PORT_DEV:
				i = priv->port.port_id;
				master_priv->node.port_dev[i] = NULL;
				netdev_rx_handler_unregister(dev);
				break;
			case TORUS_CLONE_DEV:
				i = priv->clone.clone_id - 1;
				master_priv->node.clone_dev[i] = NULL;
				break;
			default:
				this_invalid_master(master, dev);
				break;
			}
		break;
	default:
		this_invalid_master(master, dev);
	}
	return netdev_set_master(dev, NULL);
}

const struct net_device_ops torus_netdev = {
	.ndo_init            = this_init,
	.ndo_open            = this_open,
	.ndo_stop            = this_close,
	.ndo_start_xmit      = this_tx,
	.ndo_change_mtu      = this_change_mtu,
	.ndo_get_stats64     = this_get_stats64,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_add_slave	     = this_set_master,
	.ndo_del_slave	     = this_unset_master
};

