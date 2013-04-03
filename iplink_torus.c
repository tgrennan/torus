/*
 * iplink_torus.c	torus driver
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Tom Grennan <tmgrennan@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <utils.h>
#include <ip_common.h>
#include <linux/torus.h>

#define	USAGE								\
	"Usage:	... torus [ MASTER ] [ ROWSxCOLS ]\n"			\
	"\n"								\
	"ROWS 	:= %d..%d\n"						\
	"COLS 	:= %d..%d\n"						\
	, TORUS_MIN_ROWS, TORUS_MAX_ROWS				\
	, TORUS_MIN_COLS, TORUS_MAX_COLS

#define	MAXLEN	1024

static int parse_torus(struct link_util *lu, int argc, char **argv,
		       struct nlmsghdr *hdr)
{
	__u32 rows = 0, cols = 0;
	const char *master = NULL;

	while (argc) {
		if (!strcmp(*argv, "help")) {
			fprintf(stdout, USAGE);
			return -1;
		}
		if (sscanf(*argv, "%ux%u", &rows, &cols) == 2) {
			if (rows < TORUS_MIN_ROWS ||
			    rows > TORUS_MAX_ROWS ||
			    cols < TORUS_MIN_COLS ||
			    cols > TORUS_MAX_COLS)
				invarg("out of range", *argv);
		} else
			master = *argv;
		argv++, --argc;
	}
	addattr32(hdr, MAXLEN, TORUS_VERSION_ATTR, TORUS_VERSION);
	if (master)
		addattrstrz(hdr, MAXLEN, TORUS_MASTER_ATTR, master);
	if (rows) {
		addattr32(hdr, MAXLEN, TORUS_ROWS_ATTR, rows);
		addattr32(hdr, MAXLEN, TORUS_COLS_ATTR, cols);
	}
	return 0;
}

struct link_util torus_link_util = {
	.id = "torus",
	.parse_opt = parse_torus
};
