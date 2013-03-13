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
#include <linux/skbuff.h>
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
	struct	net_device *rx_dev = dev->master;
	struct	torus *priv = netdev_priv(dev->master);
	struct	torus *rx_priv = priv;
	struct	ethhdr *e = eth_hdr(*pskb);
	struct	sk_buff *cloned_buff;
	uint	bytes = (*pskb)->len;
	u8	clone_id;

	if (torus_addr_is_node(e->h_source, priv))
		goto consume;
	if (torus_addr_is_ti(e->h_dest)) {
		if (torus_addr_is_all_nodes(e->h_dest)) {
			cloned_buff = skb_clone(*pskb, GFP_ATOMIC);
			/* FIXME clone skb, dev_queue_xmit node, rx clone*/
			/* what about dups? should we have a sequence # */
		} else if (torus_addr_is_node(e->h_source, priv)) {
			clone_id = torus_addr_get_clone(e->h_source);
			if (clone_id != 0) {
				rx_dev = priv->node.clone_dev[clone_id - 1];
				rx_priv = netdev_priv(rx_dev);
			}
			if (dev_forward_skb(rx_dev, *pskb) == NET_RX_SUCCESS) {
				count_packet(&rx_priv->rx, bytes);
				return RX_HANDLER_CONSUMED;
			}
			count_drop(&rx_priv->rx);
			goto consume;
		} else {
			if (torus_addr_dec_ttl(e->h_dest) != 0) {
				(*pskb)->dev = dev->master;
				if (dev_queue_xmit(*pskb) != 0)
					consume_skb(*pskb);
				return RX_HANDLER_CONSUMED;
			}
		}
	}
#if 0
	What about non-torus packets?
#endif	/* FIXME */
consume:
	consume_skb(*pskb);
	return RX_HANDLER_CONSUMED;
}

static netdev_tx_t this_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct	torus *node_priv, *priv;
	struct	ethhdr *e = eth_hdr(skb);
	uint	bytes = skb->len;
	u16	toroid_id;
	u8	node_idx, port_idx, **tbl_p;

	node_priv = priv = netdev_priv(dev);
	switch (priv->mode) {
	case TORUS_PORT_DEV:
		/* FIXME  do we need this? */
		if (skb->ip_summed == CHECKSUM_NONE &&
		    (dev->features & NETIF_F_RXCSUM))
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (dev_forward_skb(priv->port.peer, skb) == NET_RX_SUCCESS)
			count_packet(&priv->tx, bytes);
		else
			count_error(&priv->tx);
		break;
	case TORUS_CLONE_DEV:
		node_priv = netdev_priv(dev->master);
		/* no-break: continue with node reference */
	case TORUS_NODE_DEV:
		if (torus_addr_is_ti(e->h_dest)) {
			if (torus_addr_is_all_nodes(e->h_dest)) {
				/* FIXME */
			} else {
				if (torus_addr_get_ttl(e->h_dest) == 0)
					torus_addr_set_ttl(e->h_dest,
							   TORUS_NODE_BITS); 
				toroid_id = torus_addr_get_toroid(e->h_dest);
				if (toroid_id != node_priv->node.toroid_id) {
					tbl_p = &node_priv->node.nodelu[0];
					node_idx = torus_lookup(tbl_p,
								toroid_id);
				} else {
					node_idx =
						torus_addr_get_node(e->h_dest);
				}
				tbl_p = &node_priv->node.portlu[0];
				port_idx = torus_lookup(tbl_p, node_idx);
				if (port_idx < TORUS_PORTS) {
					skb->dev = node_priv->node.port_dev[port_idx];
					if (dev_queue_xmit(skb) == 0)
						count_packet(&priv->tx, bytes);
					else
						count_error(&priv->tx);
				}
			}
		}	/* FIXME what to do with non-torus packets? */
		break;
	case TORUS_TOROID_DEV:
		count_error(&priv->tx);
		break;
	case TORUS_UNKNOWN_DEV:
		break;
	}
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

