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
#include <usb/usb.h>

#define BB_DEFAULT_DEV	"flash"
#define OS_DEFAULT_DEV	"emmc"
#define USB_DISK_DEV	"/dev/disk0.0"
#define USB_MNT		"/mnt/disk"
#define INIFILE		USB_MNT"/inifile"
#define SWU_CONF_VER	"0.1"
#define BUFSIZE		1024
#define NM_LEN		128

enum img_type {
	BB,
	BB_ENV,
	OS,
	ROOTFS,
	KERNEL,
	DTB,
	LVDS
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
	{DTB, "sata", "file", "/dev/sda1"},
	{LVDS, "emmc", "lvds", "/dev/mmc3.0"},
	{LVDS, "mmc", "lvds", "/dev/mmc2.0"},
	{LVDS, "sata", "lvds", "/dev/sda1"}
};

/*
* find update image data using type and target dev
*/
static struct img_data *swu_get_img_data(enum img_type type, const char *tgt)
{
	int i;
	for (i = 0; i < sizeof(img_map)/sizeof(struct img_data); i++) {
		if (type == img_map[i].type &&
			!strcmp(tgt, img_map[i].target)) {
			return &img_map[i];
		}
	}
	return NULL;
}

/*
* update barebox
*/
static int swu_update_bb(const char *bb_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("BAREBOX_ENV");
	if (img) {
		id = swu_get_img_data(BB_ENV, bb_dev);
		if (!id)
			return -EINVAL;
		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	return ret;
}

/*
* update barebox env
*/
static int swu_update_bb_env(const char *bb_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("BAREBOX_ENV");
	if (img) {
		id = swu_get_img_data(BB_ENV, bb_dev);
		if (!id)
			return -EINVAL;
		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	return ret;
}

/*
* completely update os target device
*/
static int swu_update_os_full(const char *os_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("FULL_IMAGE");
	if (img) {
		id = swu_get_img_data(OS, os_dev);
		if (!id)
			return -EINVAL;
		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	return ret;
}

/*
* update only rootfs on the os target device
*/
static int swu_update_rootfs(const char *os_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("ROOTFS_IMAGE");
	if (img) {
		printf("Starting update\n");
		id = swu_get_img_data(ROOTFS, os_dev);
		if (!id)
			return -EINVAL;
		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	return ret;
}

/*
* update the kernel on the os target device
*/
static int swu_update_kernel(const char *os_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("KERNEL_IMAGE");
	if (img) {
		id = swu_get_img_data(KERNEL, os_dev);
		if (!id)
			return -EINVAL;
		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	return ret;
}

/*
* update flat device tree on the os target device
*/
static int swu_update_dtb(const char *os_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("DTS_IMAGE");
	if (img) {
		id = swu_get_img_data(KERNEL, os_dev);
		if (!id)
			return -EINVAL;
		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = id->target_dev;
		data.handler_name = id->handler_name;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	return ret;
}

/*
* update lvds parameters
*/
static int swu_update_lvds_param(const char *os_dev)
{
	const char *parm;
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	parm = getenv("TFT_LVDS_PANEL_MODIFY_PARAMETER");
	if (!parm)
		return ret;

	id = swu_get_img_data(LVDS, os_dev);
	if (!id)
		return -EINVAL;
	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = "oftree";
	ret = barebox_update(&data);
	if (ret)
		pr_err("ERROR: lvds parameter update failed.\n");

	return ret;
}


/* TODO config file versioning */
static int swu_read_config(void)
{
	unsigned char *buf, *ptr, *v;
	size_t size;
	int i;

	buf = read_file(INIFILE, &size);
	if (!buf)
		return -ENOMEM;

	ptr = buf;
	for (i = 0; i < size; i++) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			if (*ptr != '#') {
				v = strchr(ptr, '=');
				if (v) {
					*v++ = 0;
					setenv(ptr, v);
				}
			}
			if (i + 1 >= size)
				break;
			ptr = &buf[i+1];
		}
	}

	free(buf);

	return 0;
}

/*
* mount usb stick containg thr sw images.
# enable emmc device.
*/
static int swu_prepare_update(void)
{
	struct device_d *dev;
	struct stat st;
	int ret = -EINVAL;

	usb_rescan(1);

	if (stat(USB_DISK_DEV, &st))
		return -ENOENT;

	make_directory(USB_MNT);

	ret = mount(USB_DISK_DEV, NULL, USB_MNT, "");
	if (ret)
		return ret;

	dev = get_device_by_name("mmc3");
	if (dev)
		ret = dev_set_param(dev, "probe", "1");

	return ret;
}

/* Use handler instead fixed functions */
static int do_swu(int argc, char *argv[])
{
	const char *bb_dev, *os_dev;
	int ret = 0;

	if (swu_prepare_update()) {
		pr_err("swu prepare failed.\n");
		return -EPERM;
	}

	swu_read_config();

	bb_dev = getenv("BB_TARGET_DEV");
	if (!bb_dev)
		bb_dev = BB_DEFAULT_DEV;

	os_dev = getenv("OS_TARGET_DEV");
	if (!os_dev)
		os_dev = OS_DEFAULT_DEV;

	printf("Update: bb dev: %s os dev: %s\n", bb_dev, os_dev);

	ret = swu_update_bb(bb_dev);

	ret |= swu_update_bb_env(bb_dev);

	ret |= swu_update_os_full(os_dev);

	ret |= swu_update_rootfs(os_dev);

	ret |= swu_update_kernel(os_dev);

	ret |= swu_update_dtb(os_dev);

	ret |= swu_update_lvds_param(os_dev);

	return ret;
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
