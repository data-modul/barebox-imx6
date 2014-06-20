/*
 * swupd.c - start DMO boards update
 *
 * Copyright (c) 2014 Zahari Doychev <zahari.doychev@linux.com> Data Modul AG
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <command.h>
#include <fs.h>
#include <fcntl.h>
#include <linux/ctype.h>
#include <linux/stat.h>
#include <errno.h>
#include <xfuncs.h>
#include <malloc.h>
#include <bbu.h>
#include <libbb.h>
#include <libgen.h>
#include <environment.h>

#define BB_DEFAULT_DEV "flash"
#define OS_DEFAULT_DEV "emmc"

enum img_type {
	BB,
	BB_ENV,
	OS,
	ROOTFS,
	KERNEL,
	DTB
};

struct img_data {
	enum img_type type;
	const char *target;
	const char *handler_name;
	const char *target_dev;
};

static struct img_data img_map[] = {
	{BB, "flash", "spi", "/dev/m25p0.barebox"},
	{BB, "emmc", "mmc", "/dev/mmc3.barebox"},
	{BB, "sd", "mmc", "/dev/mmc2.barebox"},
	{BB_ENV, "flash", "blkdev", "/dev/m25p0.barebox-environment"},
	{BB_ENV, "emmc", "blkdev", "/dev/mmc3.barebox-environment"},
	{BB_ENV, "sd", "blkdev", "/dev/mmc2.barebox-environment"},
	{OS, "emmc", "blkdev", "/dev/mmc3"},
	{OS, "mmc", "blkdev", "/dev/mmc2"},
	{OS, "sata", "blkdev", "/dev/sda"},
	{ROOTFS, "emmc", "blkdev", "/dev/mmc3.1"},
	{ROOTFS, "mmc", "blkdev", "/dev/mmc2.1"},
	{ROOTFS, "sata", "blkdev", "/dev/sda2"},
	{KERNEL, "emmc", "file", "/dev/mmc3.0"},
	{KERNEL, "mmc", "file", "/dev/mmc2.0"},
	{KERNEL, "sata", "file", "/dev/sda1"},
	{DTB, "emmc", "file", "/dev/mmc3.0"},
	{DTB, "mmc", "file", "/dev/mmc2.0"},
	{DTB, "sata", "file", "/dev/sda1"}
};

static struct img_data *get_img_data(enum img_type type, const char *tgt)
{
	int i;
	for(i = 0; i < sizeof(img_map)/sizeof(struct img_data); i++) {
		if ( type == img_map[i].type && 
			!strcmp(tgt, img_map[i].target)) {
			return &img_map[i];
		}
	}
	return NULL;
}

/* Use handler instead fixed functions */
static int do_swu(int argc, char *argv[])
{
	const char *img, *bb_dev, *os_dev;
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	
	bb_dev = getenv("BB_TARGET_DEV");
	if (!bb_dev)
		bb_dev = BB_DEFAULT_DEV;

	os_dev = getenv("OS_TARGET_DEV");
	if (!os_dev)
		os_dev = OS_DEFAULT_DEV;

	printf("Update: bb dev: %s os dev: %s\n", bb_dev, os_dev);
	img = getenv("BAREBOX_IMAGE");
	if (img) {
		id = get_img_data(BB, bb_dev);
		if (!id) {
			return -1;
		}
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = img;
		barebox_update(&data);
	}

	img = getenv("BAREBOX_ENV");
	if (img) {
		id = get_img_data(BB_ENV, bb_dev);
		if (!id) {
			return -1;
		}
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = img;
		barebox_update(&data);
	}

	img = getenv("FULL_IMAGE");
	if (img) {
		id = get_img_data(OS, os_dev);
		if (!id) {
			return -1;
		}
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = img;
		barebox_update(&data);
		return 0;
	}
	
	img = getenv("ROOTFS_IMAGE");
	if (img) {
		printf("Starting update\n");
		id = get_img_data(ROOTFS, os_dev);
		if (!id) {
			return -1;
		}
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = img;
		barebox_update(&data);
	}

	img = getenv("KERNEL_IMAGE");
	if (img) {
		id = get_img_data(KERNEL, os_dev);
		if (!id) {
			return -1;
		}
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = img;
		barebox_update(&data);
	}

	img = getenv("DTS_IMAGE");
	if (img) {
		id = get_img_data(KERNEL, os_dev);
		if (!id) {
			return -1;
		}
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = img;
		barebox_update(&data);
	}

	return 0;
}

BAREBOX_CMD_HELP_START()
BAREBOX_CMD_HELP_TEXT("TBD.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(swu)
	.cmd		= do_swu,
	BAREBOX_CMD_DESC("TBD.")
	BAREBOX_CMD_OPTS("TBD.")
	BAREBOX_CMD_GROUP(CMD_GRP_SCRIPT)
	BAREBOX_CMD_HELP(cmd_swu_help)
BAREBOX_CMD_END
