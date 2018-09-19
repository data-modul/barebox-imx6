/*
 * ls.c - list files and directories
 *
 * Copyright (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
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
#include <linux/stat.h>
#include <errno.h>
#include <malloc.h>
#include <getopt.h>
#include <stringlist.h>
#include <globalvar.h>

/* display 's version is specified in device tree filename with _12*/
#define DISPLAYSTART	"_12"

static int do_dts_detect(int argc, char *argv[])
{
	struct string_list sl;
	struct string_list *entry;
	struct dirent *d;
	DIR *dir;
	char bootdev[15];
	char displaysize[9];

	globalvar_add_simple("detected.dts",NULL);

	if(argc != 3)
		return COMMAND_ERROR_USAGE;

	/* mounted boot device such as /mnt/mmc3*/
	snprintf(bootdev, sizeof(bootdev), "%s", argv[1]);
	/* display size which is detected from touch and stored in global.display.size*/
	snprintf(displaysize, sizeof(displaysize), "%s", argv[2]);

	/* list all device tree files */
	string_list_init(&sl);
	dir = opendir(bootdev);
	if (!dir)
		return -errno;
	while((d = readdir(dir)))
	{
		if(strstr(d->d_name, DISPLAYSTART))
			string_list_add_sorted(&sl, d->d_name);
	}
	closedir(dir);

	/* find a device tree from list, which is defined for detected display.
	 * display was detected prevously from touch 
	 * display ID should be in device tree file name 
	 * e.g.
	 * device tree: zImage--4.4.57-r0+git0+2a0698862e-r0-imx6q-dmo-ppc-chimei-g070y2_12014865-20180619123002.dtb
	 * display ID: 12014865 */
	string_list_for_each_entry(entry, &sl)
	{
		if(strstr(entry->str, displaysize)){
			globalvar_add_simple("detected.dts",entry->str);
			pr_info("Suitable device tree is found\n");
			return 0;
		}
	}
	pr_info("Device tree is not found. should use oftree\n");
	return 0;
}

BAREBOX_CMD_HELP_START()
BAREBOX_CMD_HELP_TEXT("detect appropriate device tree from boot device")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(dts_detect)
	.cmd		= do_dts_detect,
	BAREBOX_CMD_DESC("detect appropriate device tree")
	BAREBOX_CMD_OPTS("[MNTBOOTDEV DISPLAYSIZE]")
	BAREBOX_CMD_GROUP(CMD_GRP_FILE)
	BAREBOX_CMD_HELP(cmd_dts_detect_help)
BAREBOX_CMD_END
