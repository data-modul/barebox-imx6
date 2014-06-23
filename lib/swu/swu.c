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

static int ctoi(char character){

	if (character == '0'){
		return 0;
	}else if (character == '1'){
		return 1;
	}else if (character == '2'){
		return 2;
	}else if (character == '3'){
		return 3;
	}else if (character == '4'){
		return 4;
	}else if (character == '5'){
		return 5;
	}else if (character == '6'){
		return 6;
	}else if (character == '7'){
		return 7;
	}else if (character == '8'){
		return 8;
	}else if (character == '9'){
		return 9;
	}else if ((character == 'a') || (character == 'A')){
		return 10;
	}else if ((character == 'b') || (character == 'B')){
		return 11;
	}else if ((character == 'c') || (character == 'C')){
		return 12;
	}else if ((character == 'd') || (character == 'D')){
		return 13;
	}else if ((character == 'e') || (character == 'E')){
		return 14;
	}else if ((character == 'f') || (character == 'F')){
		return 15;
	}else{
		return -1;
	}
}

/*
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


/*
* calculate and compare hashes
*/
static int imx6_bbu_check_hash(const char *ifn, const char *ofn,
			const unsigned char *hash)
{
	struct stat st;
	struct digest *d;
	unsigned char *h = NULL;
	unsigned char md5sum_ref_byte;
	int ret, i;
	
	ret = stat(ifn, &st);
	if (ret != 0) {
		pr_err("Cannot stat %s\n", ifn);
		return -1;
	}
	d = digest_get_by_name(DIGEST_ALG);
	if (!d) {
		return -1;
	}

	h = calloc(d->length, sizeof(unsigned char));
	if (!h) {
		perror("calloc");
		return COMMAND_ERROR_USAGE;
	}

	ret = digest_file_window(d, (char *)ofn, h, 0, st.st_size);
	if (ret == 0) {
		pr_debug("<hash: ");
		for (i = 0; i < d->length; i++){
			md5sum_ref_byte = (ctoi(hash[2*i]) << 4) \
					 | ctoi(hash[(2*i) + 1]);
			pr_debug("%02x", h[i]);
			if (h[i] != md5sum_ref_byte) {
				break;
			}
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

/*
* get hash from checksum file and check integrity
*/
static int imx6_bbu_check_img_hash(const char *ifn, const char *ofn)
{
	struct digest *d;
	char hashfile[PATH_MAX];
	int fd, ret;
	char hash[HASH_SZ+2];
	
	d = digest_get_by_name(DIGEST_ALG);
	if (!d) {
		return -1;
	}

	snprintf(hashfile, sizeof(hashfile)-1, "%s.md5sum", ifn);
	fd = open(hashfile, O_RDONLY);
	pr_info("hash file: %s\n", hashfile);
	if (fd < 0) {
		return -1;
	}

	ret = read(fd, hash, HASH_SZ);
	close(fd);

	if (ret != HASH_SZ) {
		return -1;
	}
	hash[HASH_SZ]='\0';

	pr_info(">hash: %s\n", hash);
	ret = imx6_bbu_check_hash(ifn, ofn, hash);

	return ret;
}

static int imx6_bbu_check_limits(const char *ifn, const char *ofn)
{
	struct stat si, so;

	if (!ifn || !ofn)
		return -1;

	pr_debug("ifn: %s ofn: %s\n", ifn, ofn);

	if (stat(ifn, &si))
		return -1;

	if (stat(ofn, &so))
		return -1;

	pr_debug("si: %lli so: %lli\n", si.st_size, so.st_size);

	if (so.st_size < si.st_size)
		return -1;

	return 0;
}

/*
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

/*
* Write sw image to block device
*/
static int imx6_bbu_blk_dev_handler(struct bbu_handler *handler, struct bbu_data *data)
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
	if (!ret && imx6_bbu_check_img(data->imagefile, data->devicefile)) {
		ret = -1;
	}
	pr_info("update status: %d\n", ret);

	return ret;
}

/*
* Copy sw update file to mounted fs
*/
static int imx6_bbu_file_handler(struct bbu_handler *handler, struct bbu_data *data)
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
		ret = copy_file(data->imagefile, dst, verbose);
		if (!ret && imx6_bbu_check_img(data->imagefile, dst)) {
			ret = -1;
		}
		free(dst);
	}

	umount(SWU_MNT_PATH);
	unlink_recursive("/tmp/swu", NULL);

	pr_info("update status: %d\n", ret);

	return ret;
}

/*
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

/*
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
* register software update handlers
*/
int imx6_bbu_register_dmo_swu_handlers(void)
{
	pr_debug("Installing SWU handlers...\n");

	imx6_bbu_register_blk_dev_handler();

	imx6_bbu_register_file_handler();

	return 0;
}
