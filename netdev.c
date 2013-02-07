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
	struct	torus *t = netdev_priv(dev);
	int	i, ports = 0;

	for_each_torus_port(i)
		if (t->node.port_dev[i]) {
			netif_carrier_on(t->node.port_dev[i]);
			ports++;
		}
	if (!ports)
		return -ENOTCONN;
	netif_carrier_on(dev);
	return 0;
}

static int this_close(struct net_device *dev)
{
	struct	torus *t = netdev_priv(dev);
	int	i, ports = 0;

	for_each_torus_port(i)
		if (t->node.port_dev[i]) {
			netif_carrier_off(t->node.port_dev[i]);
			ports++;
		}
	netif_carrier_off(dev);
	return !ports ? -ENOTCONN : 0;
}

static int this_init(struct net_device *dev)
{
	return 0;
}

static rx_handler_result_t this_rx(struct sk_buff **pskb)
{
	struct	net_device *dev = (*pskb)->dev;
	struct	torus *t = rcu_dereference(dev->rx_handler_data);
	struct	ethhdr *e = eth_hdr(*pskb);

	if (torus_addr_is_node(e->h_source, t))
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
	struct	torus *t = netdev_priv(dev);

	/* FIXME */
	count_error(&t->tx);
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
	struct torus *t = netdev_priv(dev);

	accumulate_counters(&t->rx);
	accumulate_counters(&t->tx);
	cnt->rx_packets	+= t->rx.packets;
	cnt->tx_packets	+= t->tx.packets;
	cnt->rx_bytes	+= t->rx.bytes;
	cnt->tx_bytes	+= t->tx.bytes;
	cnt->rx_errors	+= t->rx.errors;
	cnt->tx_errors	+= t->tx.errors;
	cnt->rx_dropped	+= t->rx.drops;
	cnt->tx_dropped	+= t->tx.drops;
	return cnt;
}

static int this_set_master(struct net_device *master, struct net_device *dev)
{
	struct	torus *tm = netdev_priv(master);
	struct	torus *ts = netdev_priv(dev);
	int	i, err;
	static	const char err_reg_rx[] = "failed to register rx handler.\n";

	err = netdev_set_master(dev, master);
	if (err) {
		netdev_err(dev, "failed to make %s master.\n", master->name);
		return err;
	}
	if (tm->mode == TORUS_TOROID_DEV) {
		if (is_torus(dev)) {
			tm->toroid.node_dev[ts->node.node_id] = dev;
		} else {
			netdev_err(dev, "isn't a torus node.\n");
			err = -EINVAL;
		}
	} else if (tm->mode == TORUS_NODE_DEV) {
		if (is_torus(dev)) {
			if (ts->mode == TORUS_CLONE_DEV)
				tm->node.clone_dev[ts->clone.clone_id] = dev;
			else if (ts->mode == TORUS_PORT_DEV) {
				err = netdev_rx_handler_register(dev,
								 this_rx, tm);
				if (err)
					netdev_err(dev, err_reg_rx);
				else
					tm->node.port_dev[ts->port.port_id]
						= dev;
			} else {
				netdev_err(dev, "isn't a clone or port.\n");
				err = -EINVAL;
			}
		} else {
			for_each_torus_port(i)
				if (!tm->node.port_dev[i])
					break;
			if (i < TORUS_PORTS) {
				err = netdev_rx_handler_register(dev,
								 this_rx, tm);
				if (err)
					netdev_err(dev, err_reg_rx);
				else
					tm->node.port_dev[i] = dev;
			} else {
				netdev_err(master, "ports filled.\n");
				err = -EADDRNOTAVAIL;
			}
		}
	} else {
		netdev_err(master, "isn't a node or master.\n");
		err = -EINVAL;
	}
	if (err)
		netdev_set_master(dev, NULL);
	return err;
}

static int this_unset_master(struct net_device *master, struct net_device *dev)
{
	struct	torus *tm = netdev_priv(master);
	struct	torus *ts = netdev_priv(master);
	int	i;

	if (tm->mode == TORUS_TOROID_DEV) {
		if (is_torus(dev))
			if (ts->mode == TORUS_NODE_DEV)
				tm->toroid.node_dev[ts->node.node_id] = NULL;
	} else if (tm->mode == TORUS_NODE_DEV) {
		if (!is_torus(dev)) {
			for_each_torus_port(i)
				if (tm->node.port_dev[i] == dev) {
					tm->node.port_dev[i] = NULL;
					break;
				}
			netdev_rx_handler_unregister(dev);
		} else if (ts->mode == TORUS_PORT_DEV) {
			tm->node.port_dev[ts->port.port_id] = NULL;
			netdev_rx_handler_unregister(dev);
		} else if (ts->mode == TORUS_CLONE_DEV)
			tm->node.clone_dev[ts->clone.clone_id] = NULL;
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

