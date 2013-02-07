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

#ifndef __COUNTERS_H__
#define __COUNTERS_H__

#include <linux/u64_stats_sync.h>
#include <linux/percpu.h>

struct	percpu_counters {
	u64	packets;
	u64	bytes;
	u64	errors;
	u64	drops;
	struct	u64_stats_sync sync;
};

struct	counters {
	u64	packets;
	u64	bytes;
	u64	errors;
	u64	drops;
	struct	percpu_counters __percpu *percpu;
};

static inline void alloc_percpu_counters(struct counters *p)
{
	p->packets = p->bytes = p->errors = p->drops = 0ULL;
	p->percpu = alloc_percpu(struct percpu_counters);
}

static inline void free_percpu_counters(struct counters *p)
{
	if (p->percpu)
		free_percpu(p->percpu);
}

static inline bool have_percpu_counters(struct counters *p)
{
	return p->percpu != NULL;
}

static inline void count_packet(struct counters *p, uint bytes)
{
	struct percpu_counters *this_cpu;

	if (have_percpu_counters(p)) {
		this_cpu = this_cpu_ptr(p->percpu);
		u64_stats_update_begin(&this_cpu->sync);
		this_cpu->packets++;
		this_cpu->bytes += bytes;
		u64_stats_update_end(&this_cpu->sync);
	} else {
		p->packets++;
		p->bytes += bytes;
	}
}

static inline void count_error(struct counters *p)
{
	struct percpu_counters *this_cpu;

	if (have_percpu_counters(p)) {
		this_cpu = this_cpu_ptr(p->percpu);
		u64_stats_update_begin(&this_cpu->sync);
		this_cpu->errors++;
		u64_stats_update_end(&this_cpu->sync);
	} else
		p->errors++;
}

static inline void count_drop(struct counters *p)
{
	struct percpu_counters *this_cpu;

	if (have_percpu_counters(p)) {
		this_cpu = this_cpu_ptr(p->percpu);
		u64_stats_update_begin(&this_cpu->sync);
		this_cpu->drops++;
		u64_stats_update_end(&this_cpu->sync);
	} else
		p->drops++;
}

static inline void add_counters_from_cpu(struct counters *p, int cpu)
{
	u64 packets, bytes, errors, drops;
	uint start;
	struct percpu_counters *cpup;

	cpup = per_cpu_ptr(p->percpu, cpu);
	do {
		start	= u64_stats_fetch_begin_bh(&cpup->sync);
		packets	= cpup->packets;
		bytes	= cpup->bytes;
		errors	= cpup->errors;
		drops	= cpup->drops;
	} while (u64_stats_fetch_retry_bh(&cpup->sync, start));
	p->packets	+= packets;
	p->bytes	+= bytes;
	p->errors	+= errors;
	p->drops	+= drops;
}

static inline void accumulate_counters(struct counters *p)
{
	int cpu;

	if (have_percpu_counters(p)) {
		p->packets = p->bytes = p->drops = 0ULL;
		for_each_possible_cpu(cpu) {
			add_counters_from_cpu(p, cpu);
		}
	}
}
#endif /* __COUNTERS_H__ */
