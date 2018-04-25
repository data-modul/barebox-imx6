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

#define BB_DEFAULT_DEV	"m25p0"
#define OS_DEFAULT_DEV	"mmc3"
#define ENV_BOOT	"/env/boot"
#define USB_DISK_DEV	"/dev/disk0.0"
#define _USB_DISK_DEV_0	"/dev/disk0"
#define USB_MNT		"/mnt/disk"
#define INIFILE		USB_MNT"/inifile"
#define SWU_CONF_VER	"0.2"
#define BUFSIZE		1024
#define NM_LEN		128
#define DEV	"/dev/"

#define NO_FULL_OS	600 /* define new return value,to avoid conflict with errno codes*/

#define USBFILE		"/mnt/disk/update.log"
#define LOGFILE		".update.log.tmp"
#define swu_log(fmt, args...) \
	do { \
		int fd; \
		fd = open(LOGFILE, O_CREAT|O_APPEND|O_WRONLY); \
		if (fd > 0) { \
			dprintf(fd, fmt, ##args); \
			close(fd); \
		} \
		pr_info(fmt, ##args); \
	} while (0)

static const char spiflash_dev[] = "spiflash";
static const char mmc_dev[] = "mmc";
static const char block_dev[] = "blkdev";
static const char file_dev[] = "file";
static const char lvds_dev[] = "lvds";

static int swu_update_status(int status)
{
	struct swu_hook *local = swu_get_hook();
	if (!local) /* no update status is provided by board */
		return -ENOSYS;
	local->status = status;
	return local->func(local);
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
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("BAREBOX_IMAGE");
	if (!img)
		return ret;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.barebox", bb_dev);

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	if (swu_check_img(full_nm, full_nm))
		return -EINVAL;

	if (!strcmp(bb_dev, "m25p0"))
		data.handler_name = spiflash_dev;
	else
		data.handler_name = mmc_dev;

	data.devicefile = target_dev;
	data.imagefile = full_nm;
	data.flags |= BBU_FLAG_YES;
	data.image = read_file(data.imagefile, &data.len);
	if (!data.image)
	{
		swu_log("Barebox image is not found %d\n", ret);
		return -errno;
	}
	ret = barebox_update(&data);
	if (!ret)
		/* Take partition table into account */
		ret = swu_check_buf_img(&data, full_nm, target_dev);

	free(data.image);

	swu_log("update barebox status: %d\n", ret);

	return ret;
}

/**
* erase barebox env
*/
static int swu_erase_bb_env(const char *bb_dev)
{
	char buf[RW_BUF_SIZE];
	char target_dev[NM_LEN];
	int ret = 0;
	struct stat s;
	long long int size;
	int devfd =0;
	int w;

	memset(buf, 0, RW_BUF_SIZE);
	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.barebox-environment", bb_dev);

	if (!strncmp(bb_dev, "mmc2", 4)) {
		ret = stat(target_dev, &s);
		if (ret) {
			swu_log("no %s: %d\n", target_dev, ret);
			return -ENODEV;
		}
		size = s.st_size;
		devfd = open(target_dev, O_WRONLY);
		if (devfd < 0) {
			swu_log("Could not open %s\n", target_dev);
			return -ENOENT; 
		}
		swu_log("update block device: %s -> Default\n", target_dev);
		while (size) {
			w = write (devfd, buf, RW_BUF_SIZE);
			if (w < 0) {
				swu_log("error in erasing %s\n", target_dev);
				close(devfd);
				return -EIO ;
			}
			size -= RW_BUF_SIZE;
		}
		close(devfd);
	}
	else if(!strncmp(bb_dev, "m25p0", 5)) {
		swu_log("update block device %s -> Default\n", target_dev);
		ret = swu_erase_flash(target_dev);
		if (ret < 0) {
			swu_log("error in erasing %s\n", target_dev);
			return -EINVAL;
		}
	}
	else
		return -ENODEV;

	return ret;
}

/**
* update barebox env
*/
static int swu_update_bb_env(const char *bb_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("BAREBOX_ENV");
	if (!img)
		return ret;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.barebox-environment", bb_dev);

	if (!strncmp(img, "DEFAULT", 7))
		ret = swu_erase_bb_env(bb_dev);
	else {
		if (!strncmp(bb_dev, "m25p0", 5) && swu_erase_flash(target_dev))
			return -EINVAL;

		snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
		data.devicefile = target_dev;
		data.handler_name = block_dev;
		data.imagefile = full_nm;
		ret = barebox_update(&data);
	}

	swu_log("update barebox env status: %d\n", ret);

	return ret;
}

/**
* update via script
*/
static int swu_update_script(void)
{
	const char *img;
	char full_nm[256] = {'\0'};
	int ret = 0;

	img = getenv("SCRIPT");
	if (!img)
		return -EINVAL;

	swu_log("Script executing...\n");
	sprintf(full_nm,USB_MNT"/%s",img);
	ret = run_command(full_nm);
	swu_log("update via script status: %d\n", ret);

	return ret;
}

/**
* completely update os target device
*/
static int swu_update_os_full(const char *os_dev)
{
	const char *img;
	char full_nm[NM_LEN];
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0x1 /* be verbose */ };
	int ret = 0;

	img = getenv("FULL_IMAGE");
	if (!img)
		return NO_FULL_OS;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s", os_dev);

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = target_dev;
	data.handler_name = block_dev;
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
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("ROOTFS_IMAGE");
	if (!img)
		return ret;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.1", os_dev);

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = target_dev;
	data.handler_name = block_dev;
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
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("KERNEL_IMAGE");
	if (!img)
		return ret;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.0", os_dev);

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = target_dev;
	data.handler_name = file_dev;
	data.imagefile = full_nm;
	data.image = "zImage";
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
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	img = getenv("DTS_IMAGE");
	if (!img)
		return ret;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.0", os_dev);

	snprintf(full_nm, sizeof(full_nm)-1, USB_MNT"/%s", img);
	data.devicefile = target_dev;
	data.handler_name = file_dev;
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
	char target_dev[NM_LEN];
	struct bbu_data data = { .flags = 0 };
	int ret = 0;

	parm = getenv("TFT_LVDS_PANEL_MODIFY_PARAMETER");
	if (!parm)
		return ret;

	snprintf(target_dev, sizeof(target_dev)-1, DEV"%s.0", os_dev);
	data.devicefile = target_dev;
	data.handler_name = lvds_dev;
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
	if (ret == NO_FULL_OS) {
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

	line = read_file(INIFILE, &size);
	if (!line)
		return -ENOENT;

	swu_log("Checking config file version.\n");

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
static int swu_prepare_update(void)
{
	int ret = 0;

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
	if (!strncmp(bb_dev, "mmc2", 4) || !strncmp(os_dev, "mmc2", 4)) {
		dev = get_device_by_name("mmc2");
		if (dev)
			ret = dev_set_param(dev, "probe", "1");
	}

	if (!strncmp(bb_dev, "mmc3", 4) || !strncmp(os_dev, "mmc3", 4)) {
		dev = get_device_by_name("mmc3");
		if (dev)
			ret = dev_set_param(dev, "probe", "1");
	}

	if (!strncmp(bb_dev, "ata0", 4) || !strncmp(os_dev, "ata0", 4)) {
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

	img = getenv("SCRIPT");
	if (img)
		return 0;

	return 1;
}

/**
* switch boot device for kernel, rootfs etc.
*/
static int swu_switch_boot(const char *boot_dev, const char *root_dev)
{
	char tg_dev[NM_LEN];

	swu_log("switching boot device (%s).\n", root_dev);

	if(nvvar_add("boot.default", root_dev))
		return -EPERM;

	snprintf(tg_dev, sizeof(tg_dev)-1, DEV"%s.barebox-environment", boot_dev);

	swu_log("save new env in %s\n", tg_dev);
	return envfs_save(tg_dev, "/env", 0);
}

static void copy_log(void)
{
	pr_info("copy log file to usb...\n");
	if (copy_file(LOGFILE, USBFILE, 0))
		pr_err("ERR: copying log file to usb stick failed!\n");
}

/*  called to show final update result*/
static void pre_swu_fail(void)
{
	pr_info("please remove usb media and reset the board.");
	if (umount(USB_MNT))
		pr_err("\numount usb failed.");
	while (1);
}

/* Use handler instead fixed functions */
static int do_swu(int argc, char *argv[])
{
	const char *bb_dev, *os_dev, *log;
	int ret = 0;
	int flag = 0;

	if (swu_prepare_update()) {
		pr_info("swu: no update media found.\n");
		return 0;
	}

	swu_log("SWU preparation... \n");

	ret = swu_check_config_ver();
	if (ret == -ENOENT) {
		swu_log("No inifile found.\n");
		return 0;
	}
	else if (ret) {
			swu_log("ERROR: invalid config file version.\n");
			pre_swu_fail();
	}

	ret = 0;
	swu_log("reading ini file\n");
	if (swu_read_config()) {
		swu_log("ERROR in config file.\n");
		goto out;
	}

	if (swu_update_script() == -EINVAL) {//means no scritp found

		flag = 1;

		swu_update_status(PREPARATION);
		swu_log("<<< SWU START >>>\n");
		swu_update_status(PROGRESS);

		bb_dev = getenv("BB_TARGET_DEV");
		if (!bb_dev)
			bb_dev = BB_DEFAULT_DEV;

		os_dev = getenv("OS_TARGET_DEV");
		if (!os_dev)
			os_dev = OS_DEFAULT_DEV;

		if (swu_enable_devices(bb_dev, os_dev)) {
			swu_log("ERROR: related devices cannot be found.\n");
			ret = -ENODEV;
			goto out;
		}

		swu_log("update: bb dev: %s os dev: %s\n", bb_dev, os_dev);

		ret = swu_update_bb(bb_dev);
		ret |= swu_update_bb_env(bb_dev);
		ret |= swu_update_fs(os_dev);

		if (swu_switch_boot_needed())
			ret |= swu_switch_boot(bb_dev, os_dev);

		swu_log("update status: %d\n", ret);
	}

out:
	log = getenv("LOGGING");
	if (log)
		copy_log();

	if (flag == 1) /* means no script in swu */
	{
		if (ret)
			swu_update_status(FAIL);
		else
			swu_update_status(SUCCESS);
	}

	pr_info("please remove usb media and reset the board.");

	if (umount(USB_MNT))
		pr_err("\numount usb failed.");

	while (1);
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
