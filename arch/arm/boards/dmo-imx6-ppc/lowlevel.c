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
#include <io.h>
#include <debug_ll.h>
#include <asm/sections.h>
#include <asm/mmu.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/cache.h>
#include <mach/imx6-mmdc.h>
#include <mach/generic.h>

extern char __dtb_imx6dl_dmo_ppc_start[];
extern char __dtb_imx6q_dmo_ppc_start[];

ENTRY_FUNCTION(start_imx6dl_dmo_ppc, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(0x00920000 - 8);

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6dl_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_1G, fdt);
}

ENTRY_FUNCTION(start_imx6q_dmo_ppc, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(0x00940000 - 8);

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6q_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_2G, fdt);
}
