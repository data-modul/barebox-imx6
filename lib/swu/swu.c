/*
 * Copyright (c) 2011 Peter Korsgaard <jacmet@sunsite.dk>
 *
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
#include <digest.h>

#define SWU_MNT_PATH		"/tmp/swu/"
#define BBU_FLAGS_VERBOSE	(1 << 31)
#define CHKFILE_PREFIX		"md5sum"
#define HASH_SZ			32
#define DIGEST_ALG		"md5"

enum prop_type {
	NONE,
	STR,
	INT
};

struct lvds_param_data {
	const char *param;
	enum prop_type type;
	const char *prop;
};

static struct lvds_param_data lvds_params[] = {
	/* this should be the first entry */
	/* TODO: check naming */
	{"TFT_LVDS_PANEL_OUTPUT", STR, "status"},
	{"TFT_LVDS_PANEL_BITCONFIG_COLOURMAPPING", STR, "fsl,data-mapping"},
	{"TFT_LVDS_PANEL_BITCONFIG_SINGLE_DUAL_LINK", NONE, "fsl,dual-channel"},
	{"TFT_LVDS_PANEL_BITCONFIG_18_24", STR, "fsl,data-width"},
	{"TFT_LVDS_PANEL_TIMING_PIXELCLK_HZ", INT, "clock-frequency"},
	{"TFT_LVDS_PANEL_TIMING_H_ACTIVE_LINES", INT, "hactive"},
	{"TFT_LVDS_PANEL_TIMING_V_ACTIVE_LINES", INT, "vactive"},
	{"TFT_LVDS_PANEL_TIMING_H_FPORCH", INT, "hfront-porch"},
	{"TFT_LVDS_PANEL_TIMING_HSYNC_BPORCH", INT, "hback-porch"},
	{"TFT_LVDS_PANEL_TIMING_HSYNC_WIDTH", INT, "hsync-len"},
	{"TFT_LVDS_PANEL_SIGNAL_HSYNC_POL_ACTIVE", INT, "hsync-active"},
	{"TFT_LVDS_PANEL_TIMING_V_BLANC", INT, "vfront-porch"},
	{"TFT_LVDS_PANEL_TIMING_VSYNC_OFFSET", INT, "vback-porch"},
	{"TFT_LVDS_PANEL_TIMING_VSYNC_WIDTH", INT, "vsync-len"},
	{"TFT_LVDS_PANEL_SIGNAL_VSYNC_POL_ACTIVE", INT, "vsync-active"},
	{NULL, NONE, NULL}
};


static int ctoi(char character)
{
	if (character == '0')
		return 0;
	else if (character == '1')
		return 1;
	else if (character == '2')
		return 2;
	else if (character == '3')
		return 3;
	else if (character == '4')
		return 4;
	else if (character == '5')
		return 5;
	else if (character == '6')
		return 6;
	else if (character == '7')
		return 7;
	else if (character == '8')
		return 8;
	else if (character == '9')
		return 9;
	else if ((character == 'a') || (character == 'A'))
		return 10;
	else if ((character == 'b') || (character == 'B'))
		return 11;
	else if ((character == 'c') || (character == 'C'))
		return 12;
	else if ((character == 'd') || (character == 'D'))
		return 13;
	else if ((character == 'e') || (character == 'E'))
		return 14;
	else if ((character == 'f') || (character == 'F'))
		return 15;
	else
		return -1;
}

/**
 * check if checksum file is present
 */
static int imx6_bbu_hfile_status(const char *ifn)
{
	unsigned char hashfile[PATH_MAX];
	struct stat st;
	int ret;

	if (!ifn)
		return -1;

	snprintf(hashfile, sizeof(hashfile), "%s.md5sum", ifn);
	ret = stat(hashfile, &st);

	return ret;
}


/**
 * calculate and compare hashes
 */
static int imx6_bbu_check_hash(const char *ifn, const char *ofn,
			const unsigned char *hash)
{
	struct stat st;
	struct digest *d;
	unsigned char *h = NULL;
	unsigned char ref;
	int ret, i;

	ret = stat(ifn, &st);
	if (ret != 0) {
		pr_err("Cannot stat %s\n", ifn);
		return -1;
	}
	d = digest_get_by_name(DIGEST_ALG);
	if (!d)
		return -1;

	h = calloc(d->length, sizeof(unsigned char));
	if (!h) {
		perror("calloc");
		return COMMAND_ERROR_USAGE;
	}

	ret = digest_file_window(d, (char *)ofn, h, 0, st.st_size);
	if (ret == 0) {
		pr_debug("<hash: ");
		for (i = 0; i < d->length; i++) {
			ref = (ctoi(hash[2*i]) << 4) | ctoi(hash[(2*i) + 1]);
			pr_debug("%02x", h[i]);
			if (h[i] != ref)
				break;
		}
		pr_debug("\n");
		if (i != d->length) {
			pr_info("ERROR: Signature check failed.\n");
			ret = -1;
		} else {
			pr_info("Signature check ok.\n");
			ret = 0;
		}
	}

	free(h);

	return ret;
}

/**
 * get hash from checksum file and check integrity
 */
static int imx6_bbu_check_img_hash(const char *ifn, const char *ofn)
{
	struct digest *d;
	char hashfile[PATH_MAX];
	int fd, ret;
	char hash[HASH_SZ+2];

	d = digest_get_by_name(DIGEST_ALG);
	if (!d)
		return -1;

	snprintf(hashfile, sizeof(hashfile)-1, "%s.md5sum", ifn);
	fd = open(hashfile, O_RDONLY);
	pr_info("hash file: %s\n", hashfile);
	if (fd < 0)
		return -1;

	ret = read(fd, hash, HASH_SZ);
	close(fd);

	if (ret != HASH_SZ)
		return -1;

	hash[HASH_SZ] = '\0';

	pr_info(">hash: %s\n", hash);
	ret = imx6_bbu_check_hash(ifn, ofn, hash);

	return ret;
}

static int imx6_bbu_check_limits(const char *ifn, const char *ofn)
{
	struct stat si, so;

	if (!ifn || !ofn)
		return -EINVAL;

	pr_debug("ifn: %s ofn: %s\n", ifn, ofn);

	if (stat(ifn, &si)) {
		pr_debug("stat failed: %s\n", ifn);
		return -ENOENT;
	}
	if (stat(ofn, &so)) {
		pr_debug("stat failed: %s\n", ofn);
		return -ENOENT;
	}
	pr_debug("si: %lli so: %lli\n", si.st_size, so.st_size);

	if (so.st_size < si.st_size)
		return -EINVAL;

	return 0;
}

/**
 * start image sig check.
 */
static int imx6_bbu_check_img(const char *ifn, const char *ofn)
{
	if (imx6_bbu_hfile_status(ifn)) {
		pr_info("Integrity check skipped.\n");
		return 0;
	}
	return imx6_bbu_check_img_hash(ifn, ofn);
}

/**
 * Write sw image to block device
 */
static int imx6_bbu_blk_dev_handler(struct bbu_handler *handler,
				struct bbu_data *data)
{
	int ret, verbose;

	if (imx6_bbu_check_limits(data->imagefile, data->devicefile)) {
		pr_err("ERROR: Partition too small.\n");
		return -1;
	}

	pr_info("Running signature check.\n");
	ret = imx6_bbu_check_img(data->imagefile, data->imagefile);
	if (ret != 0) {
		pr_err("Image signature check failed!\n");
		return -1;
	}

	pr_debug("update block device: S:%s -> D:%s\n",
			data->imagefile,
			data->devicefile);

	verbose = data->flags & BBU_FLAGS_VERBOSE;
	ret = copy_file(data->imagefile, data->devicefile, verbose);
	if (!ret && imx6_bbu_check_img(data->imagefile, data->devicefile))
		ret = -1;

	pr_info("update status: %d\n", ret);

	return ret;
}

static int imx6_bbu_safe_copy(const char *s, const char *d, int verbose)
{
	int ret;

	ret = copy_file(s, d, verbose);
	if (!ret && imx6_bbu_check_img(s, d))
			ret = -1;

	return ret;
}
/*
* Copy sw update file to mounted fs
*/
static int imx6_bbu_file_handler(struct bbu_handler *handler,
				struct bbu_data *data)
{
	int ret, verbose;

	if (imx6_bbu_check_limits(data->imagefile, data->devicefile)) {
		pr_err("ERROR: Partition too small.\n");
		return -1;
	}

	pr_info("Running signature check.\n");
	ret = imx6_bbu_check_img(data->imagefile, data->imagefile);
	if (ret != 0) {
		pr_err("Image signature check failed!\n");
		return -1;
	}

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
		ret = imx6_bbu_safe_copy(data->imagefile, dst, verbose);
		if (data->image) {
			char *lnk;
			lnk = concat_path_file(SWU_MNT_PATH, data->image);
			ret = symlink(dst, lnk);
			if (ret) /* FIXME: FAT workaround */
				ret = imx6_bbu_safe_copy(dst, lnk, 0);
			free(lnk);
		}
		free(dst);
	}

	umount(SWU_MNT_PATH);
	unlink_recursive("/tmp/swu", NULL);

	pr_info("update status: %d\n", ret);

	return ret;
}

/**
 * check if all lvds paramter are supplied
 */
static int imx6_bbu_check_lvds_param(void)
{
	int i = 0;

	while (lvds_params[i].param) {
		const char *param = getenv(lvds_params[i].param);
		if (!param) {
			pr_err("ERROR: %s not defined\n", lvds_params[i].param);
			return -1;
		}
		i++;
	}

	return 0;
}

/*
* print of from some node onwards
*/
static inline void imx6_bbu_dump_of(struct device_node *from, const char *prop)
{
	struct device_node *n;
	printf("\n---\n");
	n = of_find_node_with_property(from, prop);
	of_print_nodes(n, 0);
	printf("\n---\n");
}

/**
 * get first node that contains property "prop"
 * TODO: maybe needs fixing in the future: more displays etc.
 */
static struct device_node *imx6_bbu_get_lvds_node(
		struct device_node *from,
		const char *prop)
{
	struct device_node *n = from;

	while (n) {
		n = of_find_node_with_property(n, prop);
		if (n && of_device_is_available(n))
			return n;
	}

	return NULL;
}

/**
 * set property with no value. now used only for single/dual channel
 */
static int imx6_bbu_update_prop_none(struct device_node *root,
				struct lvds_param_data *ld, const char *v)
{
	struct device_node *n = NULL;
	struct property *pp = NULL;
	int sta = strncmp(v, "0", 1);

	n = imx6_bbu_get_lvds_node(root, ld->prop);
	/* disabled and not active in DTS */
	if (!sta && !n)
		return 0;
	/* enabled and not active in DTS */
	if (sta && !n) {
		n = imx6_bbu_get_lvds_node(root, "fsl,data-mapping");
		if (!n)
			return -EINVAL;
		n = n->parent;
		if (n)
			of_new_property(n, ld->prop, NULL, 0);
	}
	/* disable but active in the DTS */
	if (!sta && n) {
		pp = of_find_property(n, ld->prop, NULL);
		/* check for NULL ptr inside the next func */
		of_delete_property(pp);
	}

	/* enabled and active in the DTS */

	return 0;
}
/**
 * set string property
 */
static int imx6_bbu_update_prop_str(struct device_node *root,
				struct lvds_param_data *ld, const char *v)
{
	size_t len = strlen(v)+1;
	struct device_node *n = NULL;
	struct property *pp = NULL;

	/* search after status is not explicit */
	if (!strncmp(ld->prop, "status", sizeof("status")-1))
		n = imx6_bbu_get_lvds_node(root, "fsl,data-mapping");
	else
		n = imx6_bbu_get_lvds_node(root, ld->prop);
	if (!n)
		return -EINVAL;
	pp = of_find_property(n, ld->prop, NULL);

	free(pp->value);

	pp->value = malloc(len);
	memcpy(pp->value, v, len);
	pp->length = len;

	return 0;

}
/**
 * set integer property
 */
static int imx6_bbu_update_prop_int(struct device_node *root,
				struct lvds_param_data *ld, const char *v)
{
	struct device_node *n = NULL;
	struct property *pp = NULL;
	unsigned long tmp, val;
	char *ptr;

	if (!isdigit(*v))
		return -EINVAL;

	n = imx6_bbu_get_lvds_node(root, ld->prop);
	if (!n)
		return -EINVAL;

	pp = of_find_property(n, ld->prop, NULL);

	tmp = simple_strtoul(v, &ptr, 0);
	if (*ptr)
		return -EINVAL;

	free(pp->value);

	val = __cpu_to_be32(tmp);
	pp->value = malloc(sizeof(val));
	memcpy(pp->value, &val, sizeof(val));
	pp->length = sizeof(val);

	return 0;
}

/**
 * udpate panel/display settings in OF.
 */
static int imx6_bbu_update_of(struct device_node *root)
{
	int ret = 0, i = 0;
	struct device_node *n;

	n = imx6_bbu_get_lvds_node(root, "fsl,data-mapping");
	if (n)
		of_print_nodes(n->parent, 0);

	while (lvds_params[i].param) {
		struct lvds_param_data *ld = &lvds_params[i];
		const char *val = getenv(ld->param);

		switch (ld->type) {
		case NONE:
			ret = imx6_bbu_update_prop_none(root, ld, val);
			break;
		case STR:
			ret = imx6_bbu_update_prop_str(root, ld, val);
			break;
		case INT:
			ret = imx6_bbu_update_prop_int(root, ld, val);
			break;
		}

		if (ret)
			break;
		/* status should always be the first entry */
		if (!i && !strncmp(val, "disabled", sizeof("disabled")))
			break;
		i++;
	}

	n = imx6_bbu_get_lvds_node(root, "fsl,data-mapping");
	if (n)
		of_print_nodes(n->parent, 0);

	printf("ret: %d\n", ret);
	return ret;
}

static int imx6_bbu_save_of(struct device_node *root, const char *dtb)
{
	struct fdt_header *fdt = NULL;

	fdt = of_get_fixed_tree(root);
	if (!fdt)
		return -EINVAL;

	return write_file(dtb, fdt, fdt32_to_cpu(fdt->totalsize));
}

/**
 * lvds settings update handler
 */
static int imx6_bbu_lvds_handler(struct bbu_handler *handler,
			struct bbu_data *data)
{
	int ret = 0;
	void *fdt;
	size_t size;
	char dtb[PATH_MAX];

	if (imx6_bbu_check_lvds_param())
		pr_err("ERROR: missing.\n");

	pr_debug("update file: S:%s -> D:%s\n",
			data->imagefile,
			data->devicefile);

	make_directory(SWU_MNT_PATH);

	ret = mount(data->devicefile, NULL, SWU_MNT_PATH, "");
	if (ret)
		return -EPERM;

	snprintf(dtb, sizeof(dtb)-1, SWU_MNT_PATH"/%s", data->imagefile);
	fdt = read_file(dtb, &size);
	if (fdt) {
		struct device_node *root;
		pr_debug("Updating dts...\n");
		root = of_unflatten_dtb(fdt);
		free(fdt);
		ret = imx6_bbu_update_of(root);
		if (!ret)
			ret = imx6_bbu_save_of(root, dtb);
		free(root);
	} else
		ret = -EIO;

	umount(SWU_MNT_PATH);
	unlink_recursive("/tmp/swu", NULL);

	pr_info("update status: %d\n", ret);

	return ret;
}

/**
 * Register block device update handler
 */
static int imx6_bbu_register_blk_dev_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &imx6_bbu_blk_dev_handler;
	handler->devicefile = "/dev/mmc2";
	handler->name = "blkdev";

	return bbu_register_handler(handler);
}

/**
 * Register block device file handler
 */
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
 * Register lvds timings update handler
 */
static int imx6_bbu_register_lvds_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &imx6_bbu_lvds_handler;
	handler->name = "lvds";

	return bbu_register_handler(handler);
}

/**
 * register software update handlers
 */
int imx6_bbu_register_dmo_swu_handlers(void)
{
	pr_debug("installing swu handlers\n");

	imx6_bbu_register_blk_dev_handler();

	imx6_bbu_register_file_handler();

	imx6_bbu_register_lvds_handler();

	return 0;
}
