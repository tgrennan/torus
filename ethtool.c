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

enum {
	THIS_CLONE_NODE_IFINDEX,
	THIS_CLONE_ID
};

static struct {
	const char string[ETH_GSTRING_LEN];
} this_clone_strings[]= {
	[THIS_CLONE_NODE_IFINDEX] = { "node" },
	[THIS_CLONE_ID] = { "clone" }
};

enum {
	THIS_NODE_TOROID,
	THIS_NODE_ID,
	THIS_NODE_CLONES
};

static struct {
	const char string[ETH_GSTRING_LEN];
} this_node_strings[] = {
	[THIS_NODE_TOROID] = { "toroid" },
	[THIS_NODE_ID] = { "node" },
	[THIS_NODE_CLONES] = { "clones" }
};

enum {
	THIS_TOROID_ID,
	THIS_TOROID_NODES
};

static struct {
	const char string[ETH_GSTRING_LEN];
} this_toroid_strings[] = {
	[THIS_TOROID_ID] = { "toroid" },
	[THIS_TOROID_NODES] = { "nodes" }
};

enum {
	THIS_PORT_MASTER_IFINDEX,
	THIS_PORT_PEER_IFINDEX
};

static struct {
	const char string[ETH_GSTRING_LEN];
} this_port_strings[] = {
	[THIS_PORT_MASTER_IFINDEX] = { "master ifindex" },
	[THIS_PORT_PEER_IFINDEX] = { "peer ifindex" }
};

#define	this_strings_copy(buf,x)	\
	memcpy((buf), &this_ ## x ##_strings, sizeof(this_ ## x ## _strings))

static int this_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	cmd->supported		= 0;
	cmd->advertising	= 0;
	ethtool_cmd_speed_set(cmd, SPEED_10000);
	cmd->duplex		= DUPLEX_FULL;
	cmd->port		= PORT_TP;
	cmd->phy_address	= 0;
	cmd->transceiver	= XCVR_INTERNAL;
	cmd->autoneg		= AUTONEG_DISABLE;
	cmd->maxtxpkt		= 0;
	cmd->maxrxpkt		= 0;
	return 0;
}

static void this_get_drvinfo(struct net_device *dev,
			     struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, TORUS, sizeof(info->driver));
	strlcpy(info->version, TORUS_VERSION_STRING, sizeof(info->version));
	strlcpy(info->fw_version, "N/A", sizeof(info->fw_version));
}

static void this_get_strings(struct net_device *dev, u32 stringset, u8 *buf)
{
	switch(stringset) {
	case ETH_SS_STATS:
		switch (((struct torus *)netdev_priv(dev))->mode) {
		case TORUS_CLONE_DEV:
			this_strings_copy(buf, clone);
			break;
		case TORUS_NODE_DEV:
			this_strings_copy(buf, node);
			break;
		case TORUS_TOROID_DEV:
			this_strings_copy(buf, toroid);
			break;
		case TORUS_PORT_DEV:
			this_strings_copy(buf, port);
			break;
		default:
			break;
		}
	}
}

static int this_get_sset_count(struct net_device *dev, int sset)
{
	int	ret = -EOPNOTSUPP;

	switch (sset) {
	case ETH_SS_STATS:
		switch (((struct torus *)netdev_priv(dev))->mode) {
		case TORUS_CLONE_DEV:
			ret = ARRAY_SIZE(this_clone_strings);
			break;
		case TORUS_NODE_DEV:
			ret = ARRAY_SIZE(this_node_strings);
			break;
		case TORUS_TOROID_DEV:
			ret = ARRAY_SIZE(this_toroid_strings);
			break;
		case TORUS_PORT_DEV:
			ret = ARRAY_SIZE(this_port_strings);
			break;
		default:
			break;
		}
	default:
		break; 
	}
	return ret;
}

static void this_get_ethtool_stats(struct net_device *dev,
				   struct ethtool_stats *stats,
				   u64 *data)
{
	struct	torus *t = netdev_priv(dev);

	switch (t->mode) {
	case TORUS_CLONE_DEV:
		data[THIS_CLONE_NODE_IFINDEX] = dev->master->ifindex;
		data[THIS_CLONE_ID] = t->clone.clone_id;
		break;
	case TORUS_NODE_DEV:
		data[THIS_NODE_TOROID] = t->node.toroid_id;
		data[THIS_NODE_ID] = t->node.node_id;
		data[THIS_NODE_CLONES] = t->node.clones;
		break;
	case TORUS_TOROID_DEV:
		data[THIS_TOROID_ID] = t->toroid.toroid_id;
		data[THIS_TOROID_NODES] = t->toroid.nodes;
		break;
	case TORUS_PORT_DEV:
		data[THIS_PORT_MASTER_IFINDEX] = dev->master->ifindex;
		data[THIS_PORT_PEER_IFINDEX] = t->port.peer->ifindex;
		break;
	default:
		break;
	}
}

const struct ethtool_ops torus_ethtool = {
	.get_settings		= this_get_settings,
	.get_drvinfo		= this_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_strings		= this_get_strings,
	.get_sset_count		= this_get_sset_count,
	.get_ethtool_stats	= this_get_ethtool_stats,
};
