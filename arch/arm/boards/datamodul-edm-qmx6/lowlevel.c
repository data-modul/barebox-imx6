/*
 * Copyright (C) 2013 Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <common.h>
#include <sizes.h>
#include <io.h>
#include <debug_ll.h>
#include <asm/sections.h>
#include <asm/mmu.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/cache.h>
#include <mach/imx6-mmdc.h>
#include <mach/generic.h>

static void uart_init(void)
{
	/* Enable UART for lowlevel debugging purposes. Can be removed later */
	writel(0x4, 0x020e00bc);
	writel(0x4, 0x020e00c0);
	writel(0x1, 0x020e0928);
	writel(0x00000000, 0x021e8080);
	writel(0x00004027, 0x021e8084);
	writel(0x00000704, 0x021e8088);
	writel(0x00000a81, 0x021e8090);
	writel(0x0000002b, 0x021e809c);
	writel(0x00013880, 0x021e80b0);
	writel(0x0000047f, 0x021e80a4);
	writel(0x0000c34f, 0x021e80a8);
	writel(0x00000001, 0x021e8080);
	putc_ll('>');
}

extern char __dtb_imx6q_dmo_edmqmx6_start[];

ENTRY_FUNCTION(start_imx6_realq7, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(0x00940000 - 8);

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6q_dmo_edmqmx6_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_1G, fdt);
}
