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

#ifndef __TORUS_ERR_H__
#define __TORUS_ERR_H__

#include "printk.h"

/*
 * retonerr	ret[urn]-on-err[or]
 * @expr:	expression
 * @fmt:	printk FORMAT
 * @...:	printk ARGS
 *
 * return negative expression results
 */
#define	retonerr(expr, fmt, ...)					\
	do {								\
		int __res = expr;					\
		if (__res < 0) {					\
			pr_torus_err(fmt, ##__VA_ARGS__);		\
			return __res;					\
		}							\
	} while (0)

/*
 * retonerange	ret[urn]-on-range-[error]
 * @expr:	expression
 * @min:	minimum
 * @max:	maximum
 * @name:	"name" of out-of-range field
 *
 * return -ERANGE if result of expressio is less or greater than the
 * respective min or max
 */
#define	retonerange(expr, min, max, name)				\
	do {								\
		typeof(expr) __res = expr;				\
		if (__res < (min) || __res > (max)) {			\
			pr_torus_err("out of range "name", %u", __res);	\
			return -ERANGE;					\
		}							\
	} while (0)

/*
 * gotonerr	goto-[o]n-err[or]
 * @label:	target
 * @expr:	expression
 * @fmt:	printk FORMAT
 * @...:	printk ARGS
 *
 * goto label on negative expression result
 */
#define	gotonerr(label, expr, fmt, ...)					\
	do {								\
		int __res = expr;					\
		if (__res < 0) {					\
			pr_torus_err(fmt, ##__VA_ARGS__);		\
			goto label;					\
		}							\
	} while (0)


#endif	/* __TORUS_ERR_H__ */
