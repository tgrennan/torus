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

static uint lu_idx(struct device_attribute *);
static ssize_t show_dev(struct net_device **, ssize_t, char *);
static ssize_t show_lu(struct device *, struct device_attribute *, char *);
static ssize_t store_lu(struct device *, struct device_attribute *,
			const char *, size_t);
static ssize_t show_node(struct device *, struct device_attribute *, char *);
static ssize_t show_peer(struct device *, struct device_attribute *, char *);
static ssize_t show_port(struct device *, struct device_attribute *, char *);

static DEVICE_ATTR(lu1, S_IWUSR | S_IRUGO, show_lu, store_lu);
static DEVICE_ATTR(lu2, S_IWUSR | S_IRUGO, show_lu, store_lu);
static DEVICE_ATTR(lu3, S_IWUSR | S_IRUGO, show_lu, store_lu);
static DEVICE_ATTR(lu4, S_IWUSR | S_IRUGO, show_lu, store_lu);
static DEVICE_ATTR(lu5, S_IWUSR | S_IRUGO, show_lu, store_lu);
static DEVICE_ATTR(nodes, S_IRUGO, show_node, NULL);
static DEVICE_ATTR(peers, S_IRUGO, show_peer, NULL);
static DEVICE_ATTR(ports, S_IRUGO, show_port, NULL);

static const char elipsis[] = "...\n";

static uint lu_idx(struct device_attribute *attr)
{
	if (attr == &dev_attr_lu1)
		return 1;
	else if (attr == &dev_attr_lu2)
		return 2;
	else if (attr == &dev_attr_lu3)
		return 3;
	else if (attr == &dev_attr_lu4)
		return 4;
	else
		return 5;
}

static ssize_t show_dev(struct net_device **tbl, ssize_t count, char *buf)
{
	ssize_t	n, l = PAGE_SIZE;
	int	i;

	for (i = 0; i < count; i++) {
		if (tbl[i])
			n = scnprintf(buf, l, "%s\n", tbl[i]->name);
		else
			n = scnprintf(buf, l, "\n");
		l -= n;
		buf += n;
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

static ssize_t show_lu(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	uint	tbl = lu_idx(attr);
	u8	*lu;
	size_t	n, l = PAGE_SIZE;
	int	i, lasti;

	rcu_read_lock();
	lu = rcu_dereference(priv->lu);
	for (i = tbl * TORUS_LU_TBL_ENTRIES, lasti = i + TORUS_LU_TBL_ENTRIES;
	     i < lasti; i++) {
		n = scnprintf(buf, l, "%d\n", lu[i]);
		l -= n;
		buf += n;
	}
	rcu_read_unlock();
	return PAGE_SIZE - l;
}

static ssize_t show_node(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));

	return show_dev(&priv->node[0], priv->nodes, buf);
}

static ssize_t show_peer(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	u8	*peer;
	ssize_t	n, l = PAGE_SIZE;
	int	i;

	rcu_read_lock();
	peer = rcu_dereference(priv->peer);
	for (i = 0; i < priv->ports; i++) {
		n = scnprintf(buf, l, "%pM\n", peer + (i * TORUS_ALEN));
		l -= n;
		buf += n;
		if (l <= 3 * TORUS_ALEN) {
			if (l >= sizeof(elipsis)) {
				n = scnprintf(buf, l, elipsis);
				l -= n;
			}
			break;
		}
	}
	rcu_read_unlock();
	return PAGE_SIZE - l;
}

static ssize_t show_port(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	struct	net_device **port;
	ssize_t	count;

	rcu_read_lock();
	port = rcu_dereference(priv->port);
	count = show_dev(port, priv->ports, buf);
	rcu_read_unlock();
	return count;
}

static ssize_t store_lu(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t bufsz)
{
	struct	torus *priv = netdev_priv(to_net_dev(dev));
	uint	tbl = lu_idx(attr);
	u8	u = 0, *lu;
	ssize_t	bufi = 0, lui = 0;

	if (lu = kmalloc(TORUS_LU_TBL_ENTRIES, GFP_KERNEL), !lu)
		return -ENOMEM;
	for (bufi = 0, lui = 0; lui < TORUS_LU_TBL_ENTRIES; bufi++)
		if (bufi == bufsz) {
			pr_torus_err("insufficient entries, %zd", lui);
			return -EINVAL;
		} else if (!isdigit(buf[bufi])) {
			lu[lui++] = u;
			u = 0;
		} else {
			u *= 10;
			u += buf[bufi] - '0';
		}
	spin_lock(&priv->lock);
	memcpy(rcu_dereference(priv->lu) + (tbl * TORUS_LU_TBL_ENTRIES),
	       lu, TORUS_LU_TBL_ENTRIES);
	spin_unlock(&priv->lock);
	return bufsz;
}

int create_torus_sysfs(struct net_device *dev)
{
	struct	torus *priv = netdev_priv(dev);
#define	new_sys_file(_attr)						\
	do {	int	err;						\
		err = device_create_file(&dev->dev,			\
					 &dev_attr_##_attr);		\
		if (err)						\
			return err;					\
	} while (0)

	new_sys_file(lu1);
	new_sys_file(lu2);
	new_sys_file(lu3);
	new_sys_file(lu4);
	new_sys_file(lu5);
	if (priv->node)
		new_sys_file(nodes);
	new_sys_file(peers);
	new_sys_file(ports);
	return 0;
}
