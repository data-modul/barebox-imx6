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
#include <libfile.h>
#include <libgen.h>
#include <environment.h>
#include <digest.h>
#include <globalvar.h>

#define SWU_MNT_PATH		"/tmp/swu/"
#define BBU_FLAGS_VERBOSE	(1 << 31)
#define CHKFILE_PREFIX		"md5sum"
#define HASH_SZ			32
#define DIGEST_ALG		"md5"

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
	{"TFT_LVDS_PANEL_BITCONFIG_18_24", INT, "fsl,data-width"},
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

static struct swu_hook *status_hook;

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
static int swu_hfile_status(const char *ifn)
{
	unsigned char hashfile[PATH_MAX];
	struct stat st;
	int ret;

	if (!ifn)
		return -EINVAL;

	snprintf(hashfile, sizeof(hashfile), "%s.md5sum", ifn);
	ret = stat(hashfile, &st);

	return ret;
}

/**
 * calculate and compare hashes between buffer and file
 * "oh" should be freed by caller
 * @param buf - input buffer
 * @param bsz - input buffer size
 * @param oh  - output hash value
 * @return 0 on success
 */
static int swu_get_bhash(const char *buf, ulong bsz, unsigned char **oh)
{
	struct digest *d;
	ulong len = 0;
	int now = 0, i;
	unsigned char *hash;
	const unsigned char *ptr = buf;
	unsigned int dlength;

	d = digest_alloc(DIGEST_ALG);
	if (!d)
		return -ENOENT;

	digest_init(d);

	dlength=digest_length(d);
	hash = calloc(dlength, sizeof(unsigned char));
	if (hash == NULL) {
		perror("calloc");
		return -ENOMEM;
	}

	while (bsz) {
		now = min((ulong)4096, bsz);
		digest_update(d, ptr, now);
		bsz -= now;
		len += now;
		ptr += now;
	}

	digest_final(d, hash);
	pr_debug(">hash: ");
	for (i = 0; i < dlength; i++)
		pr_debug("%02x", hash[i]);
	pr_debug("\n");

	*oh = hash;

	digest_free(d);

	return 0;
}


static int swu_check_hash(const char *ofn, loff_t sz, int flag,
		const unsigned char *hash)
{
	struct digest *d;
	unsigned char *h = NULL;
	unsigned char ref;
	int ret, i;
	unsigned int dlength;

	if (!hash)
		return -EINVAL;

	d = digest_alloc(DIGEST_ALG);
	if (!d)
		return -ENOENT;

	dlength=digest_length(d);
	h = calloc(dlength, sizeof(unsigned char));
	if (!h) {
		perror("calloc");
		return -ENOMEM;
	}

	ret = digest_file_window(d, ofn, h, NULL, 0, sz);
	if (ret == 0) {
		swu_log("<hash: ");
		for (i = 0; i < dlength; i++) {
			if (flag)
				ref = (ctoi(hash[2*i]) << 4) |
				       ctoi(hash[(2*i) + 1]);
			else
				ref = hash[i];
			swu_log("%02x", h[i]);
			if (h[i] != ref)
				break;
		}
		swu_log("\n");
		if (i != dlength) {
			swu_log("ERROR: signature check failed.\n");
			ret = -EINVAL;
		} else {
			swu_log("signature check ok.\n");
			ret = 0;
		}
	}

	free(h);

	return ret;
}

/**
 * calc. hash of buffer and check integrity
 */
int swu_check_buf_img(struct bbu_data *data, const char *ifn,  const char *ofn)
{
	unsigned char *hash = NULL;
	int ret = 0;

	if (swu_hfile_status(ifn)) {
		pr_info("integrity check skipped %s.\n", ifn);
		return 0;
	}

	if (!data->image)
		return -EINVAL;

	ret = swu_get_bhash(data->image, data->len, &hash);
	if (ret)
		return -EINVAL;

	ret = swu_check_hash(ofn, data->len, 0, hash);

	free(hash);

	return ret;
}

/**
 * calculate and compare hashes using size of ifn and md5sum in file
 */
static int swu_check_file_hash(const char *ifn, const char *ofn,
			const unsigned char *hash)
{
	struct stat st;

	if (stat(ifn, &st))
		return -ENOENT;

	return swu_check_hash(ofn, st.st_size, 1, hash);
}

/**
 * get hash from checksum file and check integrity
 */
static int swu_check_img_hash(const char *ifn, const char *ofn)
{
	char hashfile[PATH_MAX];
	int fd, ret;
	char hash[HASH_SZ+2];

	snprintf(hashfile, sizeof(hashfile)-1, "%s.md5sum", ifn);
	fd = open(hashfile, O_RDONLY);
	swu_log("hash file: %s\n", hashfile);
	if (fd < 0)
		return -ENOENT;

	ret = read(fd, hash, HASH_SZ);
	close(fd);

	if (ret != HASH_SZ)
		return -EINVAL;

	hash[HASH_SZ] = '\0';

	swu_log(">hash: %s\n", hash);
	ret = swu_check_file_hash(ifn, ofn, hash);

	return ret;
}

static int swu_check_limits(const char *ifn, const char *ofn)
{
	struct stat si, so;

	if (!ifn || !ofn)
		return -EINVAL;

	if (stat(ifn, &si)) {
		pr_debug("stat failed: %s\n", ifn);
		return -ENOENT;
	}

	if (stat(ofn, &so)) {
		pr_debug("stat failed: %s\n", ofn);
		return -ENOENT;
	}

	if (so.st_size < si.st_size)
		return -EINVAL;

	return 0;
}

/**
 * start image sig check.
 * @param ifn - input file (used to get size for hash calculation and md5 file)
 * @param ofn - output file/device
 * @return 0 on success
 */
int swu_check_img(const char *ifn, const char *ofn)
{
	if (swu_hfile_status(ifn)) {
		pr_info("integrity check skipped %s.\n", ifn);
		return 0;
	}
	return swu_check_img_hash(ifn, ofn);
}

/**
 * Write sw image to block device
 */
static int swu_blk_dev_handler(struct bbu_handler *handler,
				struct bbu_data *data)
{
	int ret, verbose;

	if (swu_check_limits(data->imagefile, data->devicefile)) {
		swu_log("ERROR: partition too small or file does not exist\n");
		return -EINVAL;
	}

	swu_log("running signature check.\n");
	ret = swu_check_img(data->imagefile, data->imagefile);
	if (ret != 0) {
		swu_log("ERROR: image signature check failed!\n");
		return -EINVAL;
	}

	swu_log("update block device: S:%s -> D:%s\n",
			data->imagefile,
			data->devicefile);

	verbose = data->flags & BBU_FLAGS_VERBOSE;
	ret = copy_file(data->imagefile, data->devicefile, verbose);
	if (!ret && swu_check_img(data->imagefile, data->devicefile)) {
		swu_log("ERROR: copy or signature check failed.");
		ret = -EINVAL;
	}
	return ret;
}

static int swu_safe_copy(const char *s, const char *d, int verbose)
{
	int ret;

	ret = copy_file(s, d, verbose);
	if (!ret && swu_check_img(s, d))
			ret = -EINVAL;

	return ret;
}

static int swu_get_dir_size(const char *ifn, loff_t *tot)
{
	struct stat si;
	struct dirent *d;
	char tmp[PATH_MAX];
	DIR *dir;
	int ret = 0;

	*tot = 0;
	dir = opendir(SWU_MNT_PATH);
	if (!dir)
		return -ENOENT;

	while ((d = readdir(dir))) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		if (!strcmp(d->d_name, basename((char *)ifn)))
			continue;

		snprintf(tmp, sizeof(tmp)-1, SWU_MNT_PATH"%s", d->d_name);
		if (lstat(tmp, &si)) {
			ret = -ENOENT;
			break;
		}
		/* TODO: Directories not supported */
		if (S_ISDIR(si.st_mode))
			continue;
		*tot += si.st_size;
	}
	closedir(dir);

	return ret;
}

static int swu_check_space(const char *ifn, const char *ofn)
{
	loff_t dirsz;
	struct stat si, so;

	if (stat(ifn, &si))
		return -ENOENT;

	if (stat(ofn, &so))
		return -ENOENT;

	if (swu_get_dir_size(ifn, &dirsz)) {
		pr_err("ERROR: cannot get directory size.\n");
		return -EINVAL;
	}

	if (dirsz + si.st_size > so.st_size) {
		swu_log("ERROR: not enough free space.\n");
		return -EFBIG;
	}

	return 0;
}

/*
* Copy sw update file to mounted fs
*/
static int swu_file_handler(struct bbu_handler *handler,
				struct bbu_data *data)
{
	int ret, verbose;

	swu_log("running signature check.\n");
	ret = swu_check_img(data->imagefile, data->imagefile);
	if (ret != 0) {
		swu_log("image signature check failed!\n");
		return -EINVAL;
	}

	swu_log("update file: S:%s -> D:%s\n",
			data->imagefile,
			data->devicefile);

	make_directory(SWU_MNT_PATH);
	ret = mount(data->devicefile, NULL, SWU_MNT_PATH, "");
	if (!ret)
		ret = swu_check_space(data->imagefile, data->devicefile);

	if (!ret) {
		char *dst;
		char *dst_old;
		char *fn = (char *)data->image;

		dst = concat_path_file(SWU_MNT_PATH, data->image);

		strncat(fn, "-old", 4);
		dst_old = concat_path_file(SWU_MNT_PATH, fn);

		verbose = data->flags & BBU_FLAGS_VERBOSE;

		ret = swu_safe_copy(dst, dst_old, verbose);
		if(ret)
			swu_log("Backup old image fails. Continue with updating\n");

		ret = swu_safe_copy(data->imagefile, dst, verbose);

		free(dst);
		free(dst_old);
	}

	umount(SWU_MNT_PATH);
	unlink_recursive("/tmp/swu", NULL);

	return ret;
}

/**
 * check if all lvds paramter are supplied
 */
static int swu_check_lvds_param(void)
{
	int i = 0;

	while (lvds_params[i].param) {
		const char *param = getenv(lvds_params[i].param);
		if (!param) {
			swu_log("ERROR: %s undefined\n", lvds_params[i].param);
			return -EINVAL;
		}
		i++;
	}

	return 0;
}

/*
* print of from some node onwards
*/
static inline void swu_dump_of(struct device_node *from, const char *prop)
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
static struct device_node *swu_get_lvds_node(
		struct device_node *from,
		const char *prop)
{
	struct device_node *n;

	n = of_find_node_by_name(from, "ldb@020e0008");
	while (n) {
		n = of_find_node_with_property(n, prop);
		if (n)
			return n;
	}

	return NULL;
}

/**
 * set property with no value. now used only for single/dual channel
 * changes needed if other usa cases are implemented
 */
static int swu_update_prop_none(struct device_node *root,
				struct lvds_param_data *ld, const char *v)
{
	struct device_node *n = NULL;
	struct property *pp = NULL;
	int sta = strncmp(v, "0", 1);

	n = of_find_node_with_property(root, ld->prop);

	/* disabled and not active in DTS */
	if (!sta && !n)
		return 0;

	/* enabled and not active in DTS */
	if (sta && !n) {
		n = swu_get_lvds_node(root, "fsl,data-mapping");
		if (!n || strlen(n->name) != 14 ||
			strncmp(n->name, "lvds-channel@1", 14))
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
static int swu_update_prop_str(struct device_node *root,
				struct lvds_param_data *ld, const char *v)
{
	size_t len = strlen(v)+1;
	struct device_node *n = NULL;
	struct property *pp = NULL;

	/* search after status is not explicit */
	if (!strncmp(ld->prop, "status", sizeof("status")-1))
		n = swu_get_lvds_node(root, "fsl,data-mapping");
	else
		n = swu_get_lvds_node(root, ld->prop);
	if (!n || strlen(n->name) != 14 ||
		strncmp(n->name, "lvds-channel@1", 14))
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
static int swu_update_prop_int(struct device_node *root,
				struct lvds_param_data *ld, const char *v)
{
	struct device_node *n = NULL;
	struct property *pp = NULL;
	unsigned long tmp, val;
	char *ptr;

	if (!isdigit(*v))
		return -EINVAL;

	n = swu_get_lvds_node(root, ld->prop);
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
 * TODO: maybe ldb@.. should not be fixed
 */
static int swu_update_of(struct device_node *root)
{
	int ret = 0, i = 0;
	struct device_node *n;

	n = of_find_node_by_name(root, "ldb@020e0008");
	if (n)
		of_print_nodes(n, 0);

	while (lvds_params[i].param) {
		struct lvds_param_data *ld = &lvds_params[i];
		const char *val = getenv(ld->param);

		switch (ld->type) {
		case NONE:
			ret = swu_update_prop_none(root, ld, val);
			break;
		case STR:
			ret = swu_update_prop_str(root, ld, val);
			break;
		case INT:
			ret = swu_update_prop_int(root, ld, val);
			break;
		}

		if (ret)
			break;
		/* status should always be the first entry */
		if (!i && !strncmp(val, "disabled", sizeof("disabled")))
			break;
		i++;
	}

	n = of_find_node_by_name(root, "ldb@020e0008");
	if (n)
		of_print_nodes(n, 0);

	return ret;
}

static int swu_save_of(struct device_node *root, const char *dtb)
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
static int swu_lvds_handler(struct bbu_handler *handler,
			struct bbu_data *data)
{
	int ret = 0;
	void *fdt;
	size_t size;
	char dtb[PATH_MAX];
	DIR *dir;
	struct dirent *d;
	bool oftree_found = 0;

	if (swu_check_lvds_param())
		pr_err("ERROR: missing.\n");

	swu_log("update file: S:%s -> D:%s\n",
			data->imagefile,
			data->devicefile);

	make_directory(SWU_MNT_PATH);

	ret = mount(data->devicefile, NULL, SWU_MNT_PATH, "");
	if (ret)
		return -EPERM;

	dir = opendir(SWU_MNT_PATH);
	if (!dir)
		return -errno;
	while((d = readdir(dir)))
	{
		if(!strncmp(d->d_name, data->imagefile, sizeof(data->imagefile)))
		{
			oftree_found = 1;
			break;
		}
	}
	closedir(dir);
	if(!oftree_found)
		ret = swu_safe_copy(SWU_MNT_PATH"/user-oftree", SWU_MNT_PATH"/oftree", 0);
	if(ret)
	{
		ret = -EIO;
		goto finilize;
	}

	snprintf(dtb, sizeof(dtb)-1, SWU_MNT_PATH"/%s", data->imagefile);
	fdt = read_file(dtb, &size);
	if (fdt) {
		struct device_node *root;
		root = of_unflatten_dtb(fdt);
		free(fdt);
		ret = swu_update_of(root);
		if (!ret)
			ret = swu_save_of(root, dtb);
		free(root);
	} else
		ret = -EIO;

finilize:
	umount(SWU_MNT_PATH);
	unlink_recursive("/tmp/swu", NULL);

	return ret;
}

/**
 * Register block device update handler
 */
static int swu_register_blk_dev_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &swu_blk_dev_handler;
	handler->devicefile = "/dev/mmc2";
	handler->name = "blkdev";

	return bbu_register_handler(handler);
}

/**
 * Register block device file handler
 */
static int swu_register_file_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &swu_file_handler;
	handler->devicefile = "/dev/mmc2.0";
	handler->name = "file";

	return bbu_register_handler(handler);
}

/**
 * Register lvds timings update handler
 */
static int swu_register_lvds_handler(void)
{
	struct bbu_handler *handler;

	handler = xzalloc(sizeof(*handler));
	handler->handler = &swu_lvds_handler;
	handler->name = "lvds";

	return bbu_register_handler(handler);
}

/**
 * register software update handlers
 */
int swu_register_dmo_handlers(void)
{
	int rv;

	pr_debug("installing swu handlers\n");

	rv = swu_register_blk_dev_handler();

	rv |= swu_register_file_handler();

	rv |= swu_register_lvds_handler();

	if (!rv)
		globalvar_add_simple("swu.enabled", "1");

	return 0;
}

/* register swu hook */
int swu_register_hook(struct swu_hook *h)
{
	if (status_hook)
		return -EADDRNOTAVAIL;
	status_hook = h;
	return 0;
}

struct swu_hook* swu_get_hook(void)
{
	return status_hook;
}
