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

static rx_handler_result_t ndo_rx(struct sk_buff **pskb);

static inline int register_ndo_rx(struct net_device *dev,
				  void *data)
{
	return netdev_rx_handler_register(dev, ndo_rx, data);
}

static inline void ndo_trace(const char *name, const char *xx, uint len,
			     struct ethhdr *e)
{
	pr_torus_devel("%s %s %d bytes: %pM %pM %04x...", name, xx, len,
		       e->h_dest, e->h_source, ntohs(e->h_proto));
}

static inline void ndo_rx_trace(struct sk_buff *skb)
{
	ndo_trace(skb->dev->name, "RX", skb->len, eth_hdr(skb));
}

static inline void ndo_tx_trace(struct sk_buff *skb)
{
	ndo_trace(skb->dev->name, "TX", skb->len, (struct ethhdr *)skb->data);
}

static int ndo_init(struct net_device *dev)
{
	retonerr(register_ndo_rx(dev, dev), "register %s rx\n", dev->name);
	return 0;
}

static int ndo_open(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
	struct	net_device **port;
	int	i;

	rcu_read_lock();
	port = rcu_dereference(priv->port);
	for (i = 0; i < priv->ports; i++)
		if (port[i])
			netif_carrier_on(port[i]);
	rcu_read_unlock();
	return 0;
}

static int ndo_close(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
	struct	net_device **port;
	int	i;

	rcu_read_lock();
	port = rcu_dereference(priv->port);
	for (i = 0; i < priv->ports; i++)
		if (port[i])
			netif_carrier_off(port[i]);
	rcu_read_unlock();
	return 0;
}

static rx_handler_result_t ndo_rx(struct sk_buff **pskb)
{
	struct	net_device *dev, *port;
	struct	torus *priv;
	struct	ethhdr *e = eth_hdr(*pskb);
	uint	len = (*pskb)->len;
	
	if (is_torus((*pskb)->dev))
		dev = (*pskb)->dev;
	else if (is_torus((*pskb)->dev->master))
		dev = (*pskb)->dev->master;
	else
		goto consume;
	priv = netdev_priv(dev);
	port = is_multicast_ether_addr(e->h_dest)
		? dev : lookup_torus_port(priv, e->h_dest);
	if (!port)
		goto drop;
	ndo_rx_trace(*pskb);
	if (port == dev) {
		if (!is_multicast_ether_addr(e->h_dest))
			reset_torus_ttl(e->h_dest);
		count_packet(&priv->rx, len);
		return RX_HANDLER_PASS;
	}
	if (is_torus(port)) {
		count_packet(&priv->rx, len);
		(*pskb)->dev = port;
		return RX_HANDLER_ANOTHER;
	}
	if (dec_torus_ttl(e->h_dest) != 0) {
		count_packet(&priv->rx, len);
		(*pskb)->dev = port;
		if (dev_queue_xmit(*pskb) == 0)
			count_packet(&priv->tx, len);
		else
			count_drop(&priv->tx);
		return RX_HANDLER_CONSUMED;
	}
drop:
	count_drop(&priv->tx);
consume:
	consume_skb(*pskb);
	return RX_HANDLER_CONSUMED;
}

static void ndo_forward(struct torus *priv, struct net_device *dev,
			struct sk_buff *skb)
{
	uint	len = skb->len;

	ndo_tx_trace(skb);
	if (is_torus(dev)) {
		if (dev_forward_skb(dev, skb) == NET_RX_SUCCESS)
			count_packet(&priv->tx, len);
		else
			count_drop(&priv->tx);
	} else {
		skb->dev = dev;
		if (dev_queue_xmit(skb) == 0)
			count_packet(&priv->tx, len);
		else
			count_drop(&priv->tx);
	}
}

static netdev_tx_t ndo_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
	struct	ethhdr *e = (struct ethhdr *)skb->data;
	struct	sk_buff *clone;
	struct	net_device *port;
	int	i;

	if (is_torus_router(e->h_dest))
		set_torus_dest(priv, skb);
	if (is_multicast_ether_addr(e->h_dest)) {
		/* use i = 1 vs. 0 to skip this node */
		for (i = 1; i < priv->ports; i++)
			if (priv->port[i])
				if (clone = skb_clone(skb, GFP_ATOMIC), clone)
					ndo_forward(priv, priv->port[i], clone);
		consume_skb(skb);
	} else if (port = lookup_torus_port(priv, e->h_dest), port != NULL) {
		init_torus_ttl(e->h_dest);
		skb->dev = port;
		ndo_forward(priv, port, skb);
	} else {
		count_drop(&priv->tx);
		consume_skb(skb);
	}
	return NETDEV_TX_OK;
}

static int ndo_change_mtu(struct net_device *dev, int mtu)
{
	if (mtu < TORUS_MIN_MTU || mtu > TORUS_MAX_MTU)
		return -EINVAL;
	dev->mtu = mtu;
	return 0;
}

static struct rtnl_link_stats64 *
ndo_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *cnt)
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

static int ndo_set_master(struct net_device *master, struct net_device *dev)
{
	struct torus *sub_priv, *priv = netdev_priv(master);
	int	err;

	if (err = add_torus_port(priv, dev), err < 0)
		goto err_add_port;
	if (err = netdev_set_master(dev, master), err < 0)
		goto err_set_master;
	if (is_torus(dev)) {
		sub_priv = netdev_priv(dev);
		if (err = add_torus_port(sub_priv, master), err < 0)
			goto err_sub_add_port;
	} else if (err = register_ndo_rx(dev, master), err < 0)
		goto err_rx_handler_register;
	return 0;
err_rx_handler_register:
err_sub_add_port:
	netdev_set_master(dev, NULL);
err_set_master:
	rm_torus_port(priv, dev);
err_add_port:
	return err;
}

static int ndo_unset_master(struct net_device *master, struct net_device *dev)
{
	struct torus *priv = netdev_priv(master);

	if (!is_torus(dev))
		netdev_rx_handler_unregister(dev);
	netdev_set_master(dev, NULL);
	return rm_torus_port(priv, dev);
}

const struct net_device_ops torus_netdev = {
	.ndo_init            = ndo_init,
	.ndo_open            = ndo_open,
	.ndo_stop            = ndo_close,
	.ndo_start_xmit      = ndo_tx,
	.ndo_change_mtu      = ndo_change_mtu,
	.ndo_get_stats64     = ndo_get_stats64,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_add_slave	     = ndo_set_master,
	.ndo_del_slave	     = ndo_unset_master
};
