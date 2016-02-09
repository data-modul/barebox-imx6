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
#include <mach/bbu.h>
#include <envfs.h>
#include <i2c/i2c.h>
#include <libfile.h>
#include <globalvar.h>

#define BB_DEFAULT_DEV	"flash"
#define OS_DEFAULT_DEV	"emmc"
#define ENV_BOOT	"/env/boot"
#define USB_DISK_DEV	"/dev/disk0.0"
#define _USB_DISK_DEV_0	"/dev/disk0"
#define USB_MNT		"/mnt/disk"
#define INIFILE		USB_MNT"/inifile"
#define SWU_CONF_VER	"0.1"
#define BUFSIZE		1024
#define NM_LEN		128

#define USBFILE		"/mnt/disk/update.log"
#define LOGFILE		".update.log.tmp"
#define swu_log(fmt, args...) \
	do { \
		int fd; \
		fd = open(LOGFILE, O_CREAT|O_APPEND|O_WRONLY); \
		if (fd > 0) { \
			fprintf(fd, fmt, ##args); \
			close(fd); \
		} \
		pr_info(fmt, ##args); \
	} while (0)

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
	{BB, "flash", "spiflash", "/dev/m25p0.barebox"},
	{BB, "emmc", "mmc", "/dev/mmc3.barebox"}, /* eMMC not supported */
	{BB, "mmc", "mmc", "/dev/mmc2.barebox"},
	{BB_ENV, "flash", "blkdev", "/dev/m25p0.barebox-environment"},
	{BB_ENV, "emmc", "blkdev", "/dev/mmc3.barebox-environment"},
	{BB_ENV, "mmc", "blkdev", "/dev/mmc2.barebox-environment"},
	{OS, "emmc", "blkdev", "/dev/mmc3"},
	{OS, "mmc", "blkdev", "/dev/mmc2"},
	{OS, "sata", "blkdev", "/dev/ata0"},
	{ROOTFS, "emmc", "blkdev", "/dev/mmc3.1"},
	{ROOTFS, "mmc", "blkdev", "/dev/mmc2.1"},
	{ROOTFS, "sata", "blkdev", "/dev/ata0.1"},
	{KERNEL, "emmc", "file", "/dev/mmc3.0"},
	{KERNEL, "mmc", "file", "/dev/mmc2.0"},
	{KERNEL, "sata", "file", "/dev/ata0.0"},
	{DTB, "emmc", "file", "/dev/mmc3.0"},
	{DTB, "mmc", "file", "/dev/mmc2.0"},
	{DTB, "sata", "file", "/dev/ata0.0"},
	{LVDS, "emmc", "lvds", "/dev/mmc3.0"},
	{LVDS, "mmc", "lvds", "/dev/mmc2.0"},
	{LVDS, "sata", "lvds", "/dev/ata0.0"}
};

static int swu_update_status(int status)
{
	struct swu_hook *local = swu_get_hook();
	if (!local) /* no update status is provided by board */
		return -ENOSYS;
	local->status = status;
	return local->func(local);
}

/**
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

/**
* erase flash partition
*/
static int swu_erase_flash(const char *dev)
{
	struct stat s;
	int fd, ret = 0;

	if (stat(dev, &s))
		return -ENOENT;

	fd = open(dev, O_WRONLY);
	if (fd < 0)
		return -ENOENT;

	ret = erase(fd, s.st_size, 0);

	close(fd);

	return ret;
}

/**
* update barebox image
*/
static int swu_update_bb(const char *bb_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("BAREBOX_IMAGE");
	if (!img)
		return ret;

	id = swu_get_img_data(BB, bb_dev);
	if (!id)
		return -EINVAL;

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	if (swu_check_img(full_nm, full_nm))
		return -EINVAL;

	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = full_nm;
	data.flags |= BBU_FLAG_YES;
	data.image = read_file(data.imagefile, &data.len);
	if (!data.image)
		return -errno;
	ret = barebox_update(&data);
	if (!ret)
		/* Take partition table into account */
		ret = swu_check_buf_img(&data, full_nm, id->target_dev);

	free(data.image);

	swu_log("update barebox status: %d\n", ret);

	return ret;
}

/**
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
	if (!img)
		return ret;

	id = swu_get_img_data(BB_ENV, bb_dev);
	if (!id)
		return -EINVAL;

	if (!strncmp(bb_dev, "flash", 5) && swu_erase_flash(id->target_dev))
		return -EINVAL;

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = full_nm;
	ret = barebox_update(&data);

	swu_log("update barebox env status: %d\n", ret);

	return ret;
}

/**
* completely update os target device
*/
static int swu_update_os_full(const char *os_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	struct img_data *id;
	struct bbu_data data = { .flags = 0x1 /* be verbose */ };
	int ret = 0;

	img = getenv("FULL_IMAGE");
	if (!img)
		return -ENOENT;

	id = swu_get_img_data(OS, os_dev);
	if (!id)
		return -EINVAL;

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = full_nm;
	ret = barebox_update(&data);

	swu_log("update os full image status: %d\n", ret);

	return ret;
}

/**
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
	if (!img)
		return ret;

	printf("Starting update\n");
	id = swu_get_img_data(ROOTFS, os_dev);
	if (!id)
		return -EINVAL;

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = full_nm;
	ret = barebox_update(&data);

	swu_log("update rootfs status: %d\n", ret);

	return ret;
}

/**
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
	if (!img)
		return ret;

	id = swu_get_img_data(KERNEL, os_dev);
	if (!id)
		return -EINVAL;

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = full_nm;
	ret = barebox_update(&data);

	swu_log("update kernel status: %d\n", ret);

	return ret;
}

/**
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
	if (!img)
		return ret;

	id = swu_get_img_data(DTB, os_dev);
	if (!id)
		return -EINVAL;

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = id->target_dev;
	data.handler_name = id->handler_name;
	data.imagefile = full_nm;
	data.image = "oftree";
	ret = barebox_update(&data);

	swu_log("update dtb status: %d\n", ret);

	return ret;
}

/**
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

/* Update full image or individual fs */
static int swu_update_fs(const char *os_dev)
{
	int ret = 0;
	ret |= swu_update_os_full(os_dev);
	if (ret < 0) {
		ret = 0;
		ret |= swu_update_rootfs(os_dev);
		ret |= swu_update_kernel(os_dev);
		ret |= swu_update_dtb(os_dev);
		ret |= swu_update_lvds_param(os_dev);
	}
	return ret;
}

static int swu_check_config_ver(void)
{
	char *line, *p;
	size_t size;
	int ret = 0, i;

	swu_log("Checking config file version.\n");
	line = read_file(INIFILE, &size);
	if (!line)
		return -EINVAL;

	for (i = 0; i < size; i++) {
		if (line[i] == '\n') {
			line[i] = '\0';
			break;
		}
	}

	if (i == size) {
		ret =  -EINVAL;
		goto out;
	}
	/* #<ver=> = 7 chars */
	if (strlen(line) != 7 + sizeof(SWU_CONF_VER) - 1) {
		ret =  -EINVAL;
		goto out;
	}

	if (strncmp(line, "#<ver=", 6) || !strchr(line, '>')) {
		ret = -EINVAL;
		goto out;
	}

	p = strchr(&line[6], '>');
	if (!p) {
		ret = -EINVAL;
		goto out;
	}
	*p = '\0';
	swu_log("conf ver = %s\n", &line[6]);
	if (strlen(&line[6]) != strlen(SWU_CONF_VER)) {
		ret = -EINVAL;
		goto out;
	}
	if (strcmp(&line[6], SWU_CONF_VER))
		ret = -EINVAL;

out:
	free(line);
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
			if (i + 1 < size)
				ptr = &buf[i+1];
		}
	}

	free(buf);

	return 0;
}

/**
*
*/
static void swu_init_logfile(void)
{
	int fd;
	fd = open(LOGFILE, O_CREAT|O_TRUNC|O_WRONLY);
	close(fd);
}

/**
* mount usb stick containg the sw images.
*/
static int swu_prepare_update(const char *bb_dev, const char *os_dev)
{
	int ret = 0;

	swu_update_status(PREPARATION);
	usb_rescan();

	pr_info("mounting usb media...\n");

	make_directory(USB_MNT);

	ret = mount(USB_DISK_DEV, NULL, USB_MNT, "");
	if (ret) {
		pr_info("mount %s failed. trying %s...\n",
			 USB_DISK_DEV,
			 _USB_DISK_DEV_0);
		ret = mount(_USB_DISK_DEV_0, NULL, USB_MNT, "");
	}
	swu_init_logfile();

	return ret;
}

/**
* enable emmc devices etc.
*/
static int swu_enable_devices(const char *bb_dev, const char *os_dev)
{
	struct device_d *dev;
	int ret = 0;

	if (!strncmp(bb_dev, "mmc", 3) || !strncmp(os_dev, "mmc", 3)) {
		dev = get_device_by_name("mmc2");
		if (dev)
			ret = dev_set_param(dev, "probe", "1");
	}

	if (!strncmp(bb_dev, "emmc", 4) || !strncmp(os_dev, "emmc", 4)) {
		dev = get_device_by_name("mmc3");
		if (dev)
			ret = dev_set_param(dev, "probe", "1");
	}

	if (!strncmp(bb_dev, "sata", 4) || !strncmp(os_dev, "sata", 4)) {
		dev = get_device_by_name("ata0");
		if (dev)
			ret = dev_set_param(dev, "probe", "1");
	}

	return ret;
}

static int swu_switch_boot_needed(void)
{
	const char *img;

	img = getenv("BAREBOX_ENV");
	if (img)
		return 0;

	img = getenv("TFT_LVDS_PANEL_MODIFY_PARAMETER");
	if (img)
		return 0;

	return 1;
}

/**
* switch boot device for kernel, rootfs etc.
*/
static int swu_switch_boot(const char *boot_dev, const char *root_dev)
{
	struct img_data *id;

	swu_log("switching boot device (%s).\n", boot_dev);

	if(nvvar_add("boot.default", root_dev))
		return -EPERM;

	id = swu_get_img_data(BB_ENV, boot_dev);
	if (!id)
		return -EINVAL;

	swu_log("save new env in %s\n", id->target_dev);
	return envfs_save(id->target_dev, "/env", 0);
}

static void copy_log(void)
{
	pr_info("copy log file to usb...\n");
	if (copy_file(LOGFILE, USBFILE, 0))
		pr_err("ERR: copying log file to usb stick failed!\n");
}

/* Use handler instead fixed functions */
static int do_swu(int argc, char *argv[])
{
	const char *bb_dev, *os_dev, *log;
	int ret = 0;

	if (swu_prepare_update(bb_dev, os_dev)) {
		pr_info("swu: no update media found.\n");
		return 0;
	}

	swu_update_status(PROGRESS);

	swu_log("<<< SWU START >>>\n");
	if (swu_check_config_ver()) {
		swu_log("ERROR: invalid config file version.\n");
		return -EINVAL;
	}

	swu_log("reading ini file\n");
	if (swu_read_config())
		return -EINVAL;

	bb_dev = getenv("BB_TARGET_DEV");
	if (!bb_dev)
		bb_dev = BB_DEFAULT_DEV;

	os_dev = getenv("OS_TARGET_DEV");
	if (!os_dev)
		os_dev = OS_DEFAULT_DEV;

	if (swu_enable_devices(bb_dev, os_dev))
		return -EINVAL;

	swu_log("update: bb dev: %s os dev: %s\n", bb_dev, os_dev);

	ret = swu_update_bb(bb_dev);

	ret |= swu_update_bb_env(bb_dev);

	ret |= swu_update_fs(os_dev);

	if (swu_switch_boot_needed())
		ret |= swu_switch_boot(bb_dev, os_dev);

	swu_log("update status: %d\n", ret);

	log = getenv("LOGGING");
	if (log)
		copy_log();

	if (umount(USB_MNT))
		pr_err("umount usb failed.\n");

	pr_info("please remove usb media and reset the board.");
	if (ret)
		swu_update_status(FAIL);
	else
		swu_update_status(SUCCESS);

	return ret;
}

BAREBOX_CMD_HELP_START()
BAREBOX_CMD_HELP_TEXT("TBD.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_HELP_START(swu)
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT("-l\t", "list registered targets")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(swu)
	.cmd		= do_swu,
	BAREBOX_CMD_DESC("TBD.")
	BAREBOX_CMD_OPTS("TBD.")
	BAREBOX_CMD_GROUP(CMD_GRP_SCRIPT)
	BAREBOX_CMD_HELP(cmd_swu_help)
BAREBOX_CMD_END
