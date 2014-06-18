/*
 * Copyright (c) 2014 Zahari Doychev <zahari.doychev@linux.com>, Data Modul AG
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

/**
 * @file
 * @brief Implements DMO update handlers
 */

#define DEBUG

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

#define SWU_MNT_PATH	"/tmp/swu/"
#define BBU_FLAGS_VERBOSE	(1 << 31)

static int imx6_bbu_blk_dev_handler(struct bbu_handler *handler, struct bbu_data *data)
{
	int ret, verbose;
	pr_debug("update block device: S:%s -> D:%s\n", 
			data->imagefile,
			data->devicefile);

	verbose = data->flags & BBU_FLAGS_VERBOSE;
	ret = copy_file(data->imagefile, data->devicefile, verbose);
	pr_debug("update done.\n");

	return ret;
}

static int imx6_bbu_file_handler(struct bbu_handler *handler, struct bbu_data *data)
{
	int ret = 0, verbose;

	pr_debug("update file: S:%s -> D:%s\n",
			data->imagefile,
			data->devicefile);

	make_directory(SWU_MNT_PATH);
	
	ret = mount(data->devicefile, NULL, SWU_MNT_PATH, "");
	if (!ret) {
		char *dst;
		char *fn = (char *)data->imagefile;
		dst = concat_path_file(SWU_MNT_PATH, basename(fn));
		verbose = data->flags & BBU_FLAGS_VERBOSE;
		ret = copy_file(data->imagefile, dst, verbose);
		free(dst);
	}

	umount(SWU_MNT_PATH);
	unlink_recursive("/tmp/swu", NULL);

	return ret;
}

static int imx6_bbu_register_blk_dev_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &imx6_bbu_blk_dev_handler;
	handler->devicefile = "/dev/mmc2";
	handler->name = "blkdev";
	
	return bbu_register_handler(handler);
}

static int imx6_bbu_register_file_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &imx6_bbu_file_handler;
	handler->devicefile = "/dev/mmc2.0";
	handler->name = "file";

	return bbu_register_handler(handler);
}

/**
* register software update handlers
*/
int imx6_bbu_register_dmo_swu_handlers(void)
{
	pr_debug("Installing SWU handlers...\n");

	imx6_bbu_register_blk_dev_handler();
	
	imx6_bbu_register_file_handler();

	return 0;
}

/* Use handler instead fixed functions */
static int do_update(void)
{
	const char *img;
	struct bbu_data data = { .flags = 0 };

	img = getenv("BAREBOX_IMAGE");
	if (img)
		barebox_update(&data);

	img = getenv("BAREBOX_ENV");
	if (img)
		barebox_update(&data);

	img = getenv("LINUX_XXX"); 
	if (img)
		barebox_update(&data);

	img = getenv("DTS_IMAGE");
	if (img)
		barebox_update(&data);

	return 0;
}

/**
 * @param[in] argc Argument count from command line
 * @param[in] argv List of input arguments
 */
static int do_prog(int argc, char *argv[])
{
	struct stat s;

	printf("%p \n", (void *)do_prog);
	if (stat( argv[1], &s))
		return -1;
	printf("File sz: %u bytes\n", (unsigned int)s.st_size);

	if (argc != 3) {
		printf("error: invalid args\n");
		return -1;
	}
	printf("IN: %s => OUT: %s\n", argv[1], argv[2]);
	copy_file(argv[1], argv[2], 1);
	
	return 0;
}


BAREBOX_CMD_HELP_START(fprog)
BAREBOX_CMD_HELP_TEXT("TBD.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(fprog)
	.cmd		= do_prog,
	BAREBOX_CMD_DESC("TBD.")
	BAREBOX_CMD_OPTS("TBD.")
	BAREBOX_CMD_GROUP(CMD_GRP_SCRIPT)
	BAREBOX_CMD_HELP(cmd_fprog_help)
BAREBOX_CMD_END
