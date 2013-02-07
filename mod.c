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


#include <linux/module.h>
#include <linux/notifier.h>
#include <torus.h>
#include <counters.h>

/*
 * Catch the unregister of non-TORUS_PORT_DEV (i.e. normal interfaces)
 * to remove from the master node's port table.
 */
static int this_net_device_handler(struct notifier_block UNUSED *unused,
				   unsigned long event,
				   void *ptr)
{
	struct	net_device *dev = (struct net_device *) ptr;
	struct	torus *tm;

	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;
	if (is_torus(dev))	/* handled in rtnl.c:this_dellink() */
		return NOTIFY_DONE;
	if (!dev->master)
		return NOTIFY_DONE;
	if (!is_torus(dev->master))
		return NOTIFY_DONE;
	tm = netdev_priv(dev->master);
	if (tm->mode == TORUS_NODE_DEV)
		torus_unset_master(dev->master, dev);
	return NOTIFY_DONE;
}

static struct notifier_block this_notifier_block __read_mostly = {
	.notifier_call = this_net_device_handler,
};


static int __init this_init( void )
{
	int	ret;

	if (ret = rtnl_link_register(&torus_rtnl), ret != 0) {
		TORUS_ERR("Failed to register %s.\n", torus_rtnl.kind);
		return ret;
	}
	register_netdevice_notifier(&this_notifier_block);
	TORUS_INFO("Hello.\n");
	return 0;
}

static void __exit this_exit( void )
{
	unregister_netdevice_notifier(&this_notifier_block);
	rtnl_link_unregister(&torus_rtnl);
	TORUS_INFO("Goodbye.\n");
}

module_init(this_init);
module_exit(this_exit);

MODULE_DESCRIPTION("Torus Network");
MODULE_AUTHOR("Tom Grennan");
MODULE_AUTHOR("Eliot Dresselhaus");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(TORUS_VERSION_STRING);
MODULE_ALIAS_RTNL_LINK(TORUS);
