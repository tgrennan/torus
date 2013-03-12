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
#include <linux/ctype.h>

struct this_attribute {
	struct	device_attribute attr;
	uint	(*uint)(struct torus *priv);
	void *	(*voidstar)(struct torus *priv);
};

#define to_this_attr(x) container_of(x, struct this_attribute, attr)

static ssize_t show_uint(struct device *, struct device_attribute *, char *);
static ssize_t show_str(struct device *, struct device_attribute *, char *);
static ssize_t show_dev_tbl(struct device *, struct device_attribute *, char *);
static ssize_t show_lu_tbl(struct device *, struct device_attribute *, char *);
static ssize_t store_lu_tbl(struct device *, struct device_attribute *,
			    const char *, size_t);

#define uint_attr(_name, _uint)						\
	struct this_attribute this_attr_##_name =			\
	{ __ATTR(_name, S_IRUGO, show_uint, NULL),			\
		access_this_##_uint , NULL }
#define str_attr(_name,  _voidstar)					\
	struct this_attribute this_attr_##_name =			\
	{ __ATTR(_name, S_IRUGO, show_str, NULL),			\
		NULL, access_this_##_voidstar }
#define dev_tbl_attr(_name, _uint, _voidstar)				\
	struct this_attribute this_attr_##_name =			\
	{ __ATTR(_name, S_IRUGO, show_dev_tbl, NULL),			\
		access_this_##_uint, access_this_##_voidstar }
#define lu_tbl_attr(_name, _uint, _voidstar)			\
	struct this_attribute this_attr_##_name =			\
	{ __ATTR(_name, S_IWUSR | S_IRUGO, show_lu_tbl, store_lu_tbl),	\
		access_this_##_uint, access_this_##_voidstar }
#define uint_accessor(_name, _access)					\
	uint access_this_##_name(struct torus *priv) { return _access; }
#define voidstar_accessor(_name, _access)				\
	void *access_this_##_name(struct torus *priv) { return _access; }

static uint_accessor(clone_id, priv->clone.clone_id);
static uint_accessor(node_id, priv->node.node_id);
static uint_accessor(port_id, priv->port.port_id);
static uint_accessor(toroid_id, priv->mode == TORUS_NODE_DEV
		     ? priv->node.toroid_id
		     :  priv->toroid.toroid_id);
static uint_accessor(clones, priv->node.clones);
static uint_accessor(nodes, priv->toroid.nodes);
static uint_accessor(torus_ports, TORUS_PORTS);
static uint_accessor(torus_toroids, TORUS_TOROIDS);
static uint_accessor(torus_nodes, TORUS_NODES);
static voidstar_accessor(peer, priv->port.peer ? priv->port.peer->name : NULL);
static voidstar_accessor(clone_dev, &priv->node.clone_dev[0]);
static voidstar_accessor(port_dev, &priv->node.port_dev[0]);
static voidstar_accessor(node_dev, &priv->toroid.node_dev[0]);
static voidstar_accessor(nodelu0, &priv->node.nodelu[0]);
static voidstar_accessor(nodelu1, &priv->node.nodelu[1]);
static voidstar_accessor(portlu0, &priv->node.portlu[0]);
static voidstar_accessor(portlu1, &priv->node.portlu[1]);

static uint_attr(clone_id, clone_id);
static uint_attr(node_id, node_id);
static uint_attr(port_id, port_id);
static uint_attr(toroid_id, toroid_id);
static uint_attr(max_clones, clones);
static str_attr(peer, peer);
static dev_tbl_attr(clones, clones, clone_dev);
static dev_tbl_attr(ports, torus_ports, port_dev);
static dev_tbl_attr(nodes, nodes, node_dev);
static lu_tbl_attr(nodelu0, torus_toroids, nodelu0);
static lu_tbl_attr(nodelu1, torus_toroids, nodelu1);
static lu_tbl_attr(portlu0, torus_nodes, portlu0);
static lu_tbl_attr(portlu1, torus_nodes, portlu1);

static ssize_t show_uint(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	uint u = to_this_attr(attr)->uint(netdev_priv(to_net_dev(dev)));
	return scnprintf((buf), PAGE_SIZE, "%u\n", u);
}

static ssize_t show_str(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	char *s = to_this_attr(attr)->voidstar(netdev_priv(to_net_dev(dev)));
	return scnprintf((buf), PAGE_SIZE, "%s\n", s);
}

static ssize_t show_dev_tbl(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	struct	net_device **tbl;
	ssize_t	tblsz, n, l = PAGE_SIZE;
	int	i;
	static const char elipsis[] = "...\n";

	tblsz = to_this_attr(attr)->uint(priv);
	tbl = to_this_attr(attr)->voidstar(priv);
	if (!tbl)
		return 0;
	for (i = 0; i < tblsz; i++) {
		if (tbl[i]) {
			n = scnprintf(buf, l, "%s\n", tbl[i]->name);
			l -= n;
			buf += n;
		}
		if (l <= IFNAMSIZ) {
			if (l >= sizeof(elipsis)) {
				n = scnprintf(buf, l, elipsis);
				l -= n;
			}
			break;
		}
	}
	return PAGE_SIZE - l;
}

static ssize_t show_lu_tbl(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	size_t	tblsz, i, n, l = PAGE_SIZE;
	u8	**tbl_p, *clone;

	tblsz = to_this_attr(attr)->uint(priv);
	tbl_p = to_this_attr(attr)->voidstar(priv);
	if (!tblsz)
		return -ENOMSG;
	clone = torus_clone_lu_tbl(tbl_p, tblsz);
	for (i = 0; i < tblsz; i++) {
		n = scnprintf(buf, l, "%d\n", clone[i]);
		l -= n;
		buf += n;
	}
	kfree(clone);
	return PAGE_SIZE - l;
}

static ssize_t store_lu_tbl(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t bufsz)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	ssize_t	tblsz, bufi = 0, tbli = 0;
	u8	u = 0, **tbl_p, *new;

	tblsz = to_this_attr(attr)->uint(priv);
	tbl_p = to_this_attr(attr)->voidstar(priv);
	if (!tblsz)
		return -ENOMSG;
	new = kmalloc(tblsz, GFP_KERNEL);
	if (!new)
		return -ENOMEM;
	while (tbli != tblsz)
		if (bufi == bufsz) {
			TORUS_ERR("insufficient entries, %zd\n", tbli);
			return -EINVAL;
		} else if (!isdigit(buf[bufi])) {
			bufi++;
			new[tbli++] = u;
			u = 0;
		} else {
			u *= 10;
			u += buf[bufi++] - '0';
		}
	torus_update_lu_tbl(priv, tbl_p, new);
	return bufsz;
}

int torus_create_sysfs(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
#define	new_sys_file(_attr)						\
	do {	int	err;						\
		err = device_create_file(&dev->dev,			\
					 (struct device_attribute *)	\
					 &this_attr_##_attr);		\
		if (err)						\
			return err;					\
	} while (0)

	switch (priv->mode) {
	case TORUS_CLONE_DEV:
		new_sys_file(clone_id);
		break;
	case TORUS_NODE_DEV:
		new_sys_file(toroid_id);
		new_sys_file(node_id);
		new_sys_file(max_clones);
		new_sys_file(clones);
		new_sys_file(ports);
		new_sys_file(nodelu0);
		new_sys_file(nodelu1);
		new_sys_file(portlu0);
		new_sys_file(portlu1);
		break;
	case TORUS_TOROID_DEV:
		new_sys_file(toroid_id);
		new_sys_file(nodes);
		break;
	case TORUS_PORT_DEV:
		new_sys_file(port_id);
		new_sys_file(peer);
		break;
	case TORUS_UNKNOWN_DEV:
		return -ENODEV;
	}
	return 0;
}
