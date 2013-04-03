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

#ifndef __TORUS_PRINTK_H__
#define __TORUS_PRINTK_H__

#include <linux/kernel.h>

#if !defined(CONFIG_TORUS_MSG_LVL)
# if defined(DEBUG)
#  define CONFIG_TORUS_MSG_LVL	7
# else
#  define CONFIG_TORUS_MSG_LVL	2
# endif
#endif
#if CONFIG_TORUS_MSG_LVL >= 3
# define _pr_torus_err(fmt, ...)	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
# define _pr_torus_err(fmt, ...)	no_printk(pr_fmt(fmt), ##__VA_ARGS__)
#endif
#if CONFIG_TORUS_MSG_LVL >= 4
# define _pr_torus_warning(fmt, ...)	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
# define _pr_torus_warning(fmt, ...)	no_printk(pr_fmt(fmt), ##__VA_ARGS__)
#endif
#if CONFIG_TORUS_MSG_LVL >= 5
# define _pr_torus_NOTICE(fmt, ...)	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
# define _pr_torus_NOTICE(fmt, ...)	no_printk(pr_fmt(fmt), ##__VA_ARGS__)
#endif
#if CONFIG_TORUS_MSG_LVL >= 6
# define _pr_torus_info(fmt, ...)	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
# define _pr_torus_info(fmt, ...)	no_printk(pr_fmt(fmt), ##__VA_ARGS__)
#endif
#if CONFIG_TORUS_MSG_LVL >= 7
# define _pr_torus_debug(fmt, ...)	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
# define _pr_torus_debug(fmt, ...)	no_printk(pr_fmt(fmt), ##__VA_ARGS__)
#endif

#define	pr_torus_emerg(fmt, ...)					\
	pr_emerg(TORUS" fatal: "fmt"\n", ##__VA_ARGS__)
#define	pr_torus_alert(fmt, ...)					\
	pr_alert(TORUS" alert: "fmt"\n", ##__VA_ARGS__)
#define	pr_torus_crit(fmt, ...)						\
	pr_crit(TORUS" critical: "fmt"\n", ##__VA_ARGS__)
#define	pr_torus_err(fmt, ...)						\
	_pr_torus_err(KERN_ERR TORUS " error: " fmt "\n", ##__VA_ARGS__)
#define	pr_torus_warning(fmt, ...)					\
	_pr_torus_warning(KERN_WARNING TORUS ": " fmt "\n", ##__VA_ARGS__)
#define	pr_torus_notice(fmt, ...)					\
	_pr_torus_notice(KERN_NOTICE TORUS ": " fmt "\n", ##__VA_ARGS__)
#define	pr_torus_info(fmt, ...)						\
	_pr_torus_info(KERN_INFO TORUS ": " fmt "\n", ##__VA_ARGS__)
#define	pr_torus_debug(fmt, ...)					\
	_pr_torus_debug(KERN_DEBUG TORUS ": " fmt "\n", ##__VA_ARGS__)
#define	pr_torus_devel(fmt, ...)	pr_torus_debug(fmt, ##__VA_ARGS__)

#endif	/* __TORUS_PRINTK_H__ */
