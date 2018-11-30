/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <debug_ll.h>
#include <common.h>
#include <linux/sizes.h>
#include <mach/generic.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/cache.h>
#include <mach/imx6-mmdc.h>
#include <mach/imx6-ddr-regs.h>
#include <mach/imx6.h>
#include <mach/imx-gpio.h>
#include <mach/xload.h>
#include <mach/esdctl.h>
#include <serial/imx-uart.h>
#include <gpio.h>
#include <stdio.h>
#include <string.h>

#include "sdram-config.h"

static void __udelay(int us)
{
	volatile int i;

	for (i = 0; i < us * 4; i++);
}

/*
 * Driving strength:
 *   0x30 == 40 Ohm
 *   0x28 == 48 Ohm
 */

#define IMX6DQ_DRIVE_STRENGTH		0x30
#define IMX6SDL_DRIVE_STRENGTH		0x28

/* configure MX6Q/DUAL mmdc DDR io registers */
static struct mx6dq_iomux_ddr_regs mx6dq_ddr_ioregs = {
	.dram_sdclk_0 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdclk_1 = IMX6DQ_DRIVE_STRENGTH,
	.dram_cas = IMX6DQ_DRIVE_STRENGTH,
	.dram_ras = IMX6DQ_DRIVE_STRENGTH,
	.dram_reset = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdcke0 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdcke1 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdba2 = 0x00000000,
	.dram_sdodt0 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdodt1 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs0 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs1 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs2 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs3 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs4 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs5 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs6 = IMX6DQ_DRIVE_STRENGTH,
	.dram_sdqs7 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm0 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm1 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm2 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm3 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm4 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm5 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm6 = IMX6DQ_DRIVE_STRENGTH,
	.dram_dqm7 = IMX6DQ_DRIVE_STRENGTH,
};

/* configure MX6Q/DUAL mmdc GRP io registers */
static struct mx6dq_iomux_grp_regs mx6dq_grp_ioregs = {
	.grp_ddr_type = 0x000c0000,
	.grp_ddrmode_ctl = 0x00020000,
	.grp_ddrpke = 0x00000000,
	.grp_addds = IMX6DQ_DRIVE_STRENGTH,
	.grp_ctlds = IMX6DQ_DRIVE_STRENGTH,
	.grp_ddrmode = 0x00020000,
	.grp_b0ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b1ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b2ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b3ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b4ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b5ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b6ds = IMX6DQ_DRIVE_STRENGTH,
	.grp_b7ds = IMX6DQ_DRIVE_STRENGTH,
};

/* configure MX6SOLO/DUALLITE mmdc DDR io registers */
struct mx6sdl_iomux_ddr_regs mx6sdl_ddr_ioregs = {
	.dram_sdclk_0 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdclk_1 = IMX6SDL_DRIVE_STRENGTH,
	.dram_cas = IMX6SDL_DRIVE_STRENGTH,
	.dram_ras = IMX6SDL_DRIVE_STRENGTH,
	.dram_reset = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdcke0 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdcke1 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdba2 = 0x00000000,
	.dram_sdodt0 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdodt1 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs0 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs1 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs2 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs3 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs4 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs5 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs6 = IMX6SDL_DRIVE_STRENGTH,
	.dram_sdqs7 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm0 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm1 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm2 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm3 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm4 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm5 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm6 = IMX6SDL_DRIVE_STRENGTH,
	.dram_dqm7 = IMX6SDL_DRIVE_STRENGTH,
};

/* configure MX6SOLO/DUALLITE mmdc GRP io registers */
struct mx6sdl_iomux_grp_regs mx6sdl_grp_ioregs = {
	.grp_ddr_type = 0x000c0000,
	.grp_ddrmode_ctl = 0x00020000,
	.grp_ddrpke = 0x00000000,
	.grp_addds = IMX6SDL_DRIVE_STRENGTH,
	.grp_ctlds = IMX6SDL_DRIVE_STRENGTH,
	.grp_ddrmode = 0x00020000,
	.grp_b0ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b1ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b2ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b3ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b4ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b5ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b6ds = IMX6SDL_DRIVE_STRENGTH,
	.grp_b7ds = IMX6SDL_DRIVE_STRENGTH,
};


/*
 * all structs needed for sdram configuration
 * moved to sdram-config.h
 */


/*
 * dmo_ppc_dram_init:
 * The SDRAM configuration will be done according to the board variants.
 * It depends on the cpu-type, the number of mounted sdram devices
 * (32bit or 64bit databus width) and the density of the sdram devices.
 *
 * cpu-type: determined by: __imx6_cpu_type()
 * sdram_cfg: 	bit1 and bit0 => density (see sdram-config.h)
 * 				bit2 => buswidth:	1 = 32bit (resistor unmounted)
 * 									0 = 64bit (resistor mounted)
 */

static unsigned long dmo_ppc_dram_init(uint32_t sdram_cfg)
{
	int cpu_type = __imx6_cpu_type();
	void __iomem *iobase_hwid = (void *)IOBASE_HW_ID;
	unsigned long pcb_rev = (readl(iobase_hwid + 0x0) & MASK_HW_PCB_REV);
	unsigned long memsize;

	pr_info("dmo_ppc_dram_init: pcb_rev   = 0x%08X\n\n", (unsigned int)pcb_rev);

	pr_info("dmo_ppc_dram_init: cpu types available:\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6SL	= 0x160\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6S	= 0x161\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6DL	= 0x261\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6SX	= 0x462\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6D	= 0x263\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6Q	= 0x463\n");
	pr_info("dmo_ppc_dram_init: IMX6_CPUTYPE_IMX6UL	= 0x164\n\n");

	switch (cpu_type) {
	case IMX6_CPUTYPE_IMX6S:
	case IMX6_CPUTYPE_IMX6DL:
		pr_info("dmo_ppc_dram_init: sdram_cfg = 0x%08X = ", sdram_cfg);
		switch (sdram_cfg & MASK_DENSITY_2) {
		case DENSITY_1:
			pr_emerg("density 1GBit not supported!\n");
			hang();
			break;
		case DENSITY_2:
			if (sdram_cfg & BUSWIDTH) {
				/* 32bit */
				pr_info("DENSITY_2; 32bit; CPU = 0x%X\n", cpu_type);
				mx6sdl_dram_iocfg(32, &mx6sdl_ddr_ioregs, &mx6sdl_grp_ioregs);
				mx6_dram_cfg(&mem_512mb_32bit, &mx6_512m_32b_mmdc_calib, &density_2gbit);
				memsize = SZ_512M;
			}
			else {
				/* 64bit */
				pr_info("DENSITY_2; 64bit; CPU = 0x%X\n", cpu_type);
				mx6sdl_dram_iocfg(64, &mx6sdl_ddr_ioregs, &mx6sdl_grp_ioregs);
				mx6_dram_cfg(&mem_1gb_64bit, &mx6_1g_64b_mmdc_calib, &density_2gbit);
				memsize = SZ_1G;
			}
			break;
		case DENSITY_4:
			if (sdram_cfg & BUSWIDTH) {
				/* 32bit */
				pr_info("DENSITY_4; 32bit; CPU = 0x%X\n", cpu_type);
				mx6sdl_dram_iocfg(32, &mx6sdl_ddr_ioregs, &mx6sdl_grp_ioregs);
				mx6_dram_cfg(&mem_1gb_32bit, &mx6_1g_32b_mmdc_calib, &density_4gbit);
				memsize = SZ_1G;
			}
			else {
				/* 64bit */
				pr_info("DENSITY_4; 64bit; CPU = 0x%X\n", cpu_type);
				mx6sdl_dram_iocfg(64, &mx6sdl_ddr_ioregs, &mx6sdl_grp_ioregs);
				mx6_dram_cfg(&mem_2gb_64bit, &mx6_2g_64b_mmdc_calib, &density_4gbit);
				memsize = SZ_2G;
			}
			break;
		case DENSITY_8:
			if (sdram_cfg & BUSWIDTH) {
				/* 32bit */
				pr_info("DENSITY_8; 32bit; CPU = 0x%X\n", cpu_type);
				mx6sdl_dram_iocfg(32, &mx6sdl_ddr_ioregs, &mx6sdl_grp_ioregs);
				mx6_dram_cfg(&mem_2gb_32bit, &mx6_2g_32b_mmdc_calib, &density_8gbit);
				memsize = SZ_2G;
			}
			else {
				/* 64bit */
				pr_emerg("density 8GBit 64bit not supported! \n");
				hang();
			}
			break;
		}
		break;

	case IMX6_CPUTYPE_IMX6D:
	case IMX6_CPUTYPE_IMX6Q:

		/* patch for old boards (Rev900):
		 * PCB-Rev 1 has no BUSWIDTH resistor and there is only one
		 * iMX6Q-variant with 64bit buswidth
		 * so we simply clear buswidth bit
		 */
		if (pcb_rev == 0)
			sdram_cfg &= ~BUSWIDTH;

		pr_info("dmo_ppc_dram_init: sdram_cfg = 0x%08X = ", sdram_cfg);
		switch (sdram_cfg & MASK_DENSITY_2) {
		case DENSITY_1:
			pr_emerg("iMX6DQ;  density 1GBit not supported!\n");
			hang();
			break;
		case DENSITY_2:
			if (sdram_cfg & BUSWIDTH) {
				/* 32bit */
				pr_info("DENSITY_2; 32bit; CPU = 0x%X\n", cpu_type);
				mx6dq_dram_iocfg(32, &mx6dq_ddr_ioregs, &mx6dq_grp_ioregs);
				mx6_dram_cfg(&mem_512mb_32bit, &mx6_512m_32b_mmdc_calib, &density_2gbit);
				memsize = SZ_512M;
			}
			else {
				/* 64bit */
				pr_info("DENSITY_2; 64bit; CPU = 0x%X\n", cpu_type);
				mx6dq_dram_iocfg(64, &mx6dq_ddr_ioregs, &mx6dq_grp_ioregs);
				mx6_dram_cfg(&mem_1gb_64bit, &mx6_1g_64b_mmdc_calib, &density_2gbit);
				memsize = SZ_1G;
			}
			break;
		case DENSITY_4:
			if (sdram_cfg & BUSWIDTH) {
				/* 32bit */
				pr_info("DENSITY_4; 32bit; CPU = 0x%X\n", cpu_type);
				mx6dq_dram_iocfg(32, &mx6dq_ddr_ioregs, &mx6dq_grp_ioregs);
				mx6_dram_cfg(&mem_1gb_32bit, &mx6_1g_32b_mmdc_calib, &density_4gbit);
				memsize = SZ_1G;
			}
			else {
				/* 64bit */
				pr_info("DENSITY_4; 64bit; CPU = 0x%X\n", cpu_type);
				mx6dq_dram_iocfg(64, &mx6dq_ddr_ioregs, &mx6dq_grp_ioregs);
				mx6_dram_cfg(&mem_2gb_64bit, &mx6_2g_64b_mmdc_calib, &density_4gbit);
				memsize = SZ_2G;
			}
			break;
		case DENSITY_8:
			if (sdram_cfg & BUSWIDTH) {
				/* 32bit */
				pr_info("DENSITY_8; 32bit; CPU = 0x%X\n", cpu_type);
				mx6dq_dram_iocfg(32, &mx6dq_ddr_ioregs, &mx6dq_grp_ioregs);
				mx6_dram_cfg(&mem_2gb_32bit, &mx6_2g_32b_mmdc_calib, &density_8gbit);
				memsize = SZ_2G;
			}
			else {
				/* 64bit */
				pr_emerg("density 8GBit 64bit not supported!\n");
				hang();
			}
			break;
		}
		break;

	default:
		return 0;
	}

	__udelay(100);

	mmdc_do_write_level_calibration();
	mmdc_do_dqs_calibration();
#ifdef DEBUG
	mmdc_print_calibration_results();
#endif
	pr_info("\n");
	return memsize;
}

static void setup_uart(void)
{
	int cpu_type = __imx6_cpu_type();
	void __iomem *iomuxbase = (void *)MX6_IOMUXC_BASE_ADDR;

	switch (cpu_type) {
	case IMX6_CPUTYPE_IMX6S:
	case IMX6_CPUTYPE_IMX6DL:
		/* mux the uart */
		writel(0x00000003, iomuxbase + 0x4c);
		writel(0x00000000, iomuxbase + 0x8fc);
		break;
	case IMX6_CPUTYPE_IMX6D:
	case IMX6_CPUTYPE_IMX6Q:
		/* mux the uart */
		writel(0x00000003, iomuxbase + 0x280);
		writel(0x00000000, iomuxbase + 0x920);
		break;
	default:
		hang();
	}

	imx6_ungate_all_peripherals();
	imx6_uart_setup((void *)MX6_UART1_BASE_ADDR);
	pbl_set_putc(imx_uart_putc, (void *)MX6_UART1_BASE_ADDR);

	pr_debug("\n");
}

/*
 * get_sdram_config reads sdram configuration from board via gpios
 */

static uint32_t get_sdram_config(int cpu_type)
{
	void __iomem *iobase_iomux = (void *)IOBASE_IOMUX;
	void __iomem *iobase_density = (void *)IOBASE_DENSITY;
	void __iomem *iobase_buswidth = (void *)IOBASE_BUSWIDTH;

	uint32_t val;

	__udelay(1000);

	pr_info("\n");

	pr_info("get_sdram_config: iobase_iomux                   = 0x%08X\n", (unsigned int)iobase_iomux + 0x0);
	switch (cpu_type) {
	case IMX6_CPUTYPE_IMX6S:
	case IMX6_CPUTYPE_IMX6DL:
		writel(0x00000005, iobase_iomux + 0x134);	/* Pad EIM_A25: ALT5 = GPIO5_IO02 => Buswidth (DRAM_64_32) */
		pr_info("get_sdram_config: read iobase_iomux + 0x134      = 0x%08X\n", readl(iobase_iomux + 0x134));
		writel(0x00000005, iobase_iomux + 0x148);	/* Pad EIM_D17:  ALT5 = GPIO3_IO17 => Density (DRAM_ID1) */
		pr_info("get_sdram_config: read iobase_iomux + 0x148      = 0x%08X\n", readl(iobase_iomux + 0x148));
		writel(0x00000005, iobase_iomux + 0x14C);	/* Pad EIM_D18:  ALT5 = GPIO3_IO18 => Density (DRAM_ID0) */
		pr_info("get_sdram_config: read iobase_iomux + 0x14C      = 0x%08X\n", readl(iobase_iomux + 0x14C));
		break;
	case IMX6_CPUTYPE_IMX6D:
	case IMX6_CPUTYPE_IMX6Q:
		writel(0x00000005, iobase_iomux + 0x88);	/* Pad EIM_A25: ALT5 = GPIO5_IO02 => Buswidth (DRAM_64_32) */
		pr_info("get_sdram_config: read iobase_iomux + 0x88       = 0x%08X\n", readl(iobase_iomux + 0x88));
		writel(0x00000005, iobase_iomux + 0x94);	/* Pad EIM_D17: ALT5 = GPIO3_IO17 => Density (DRAM_ID1) */
		pr_info("get_sdram_config: read iobase_iomux + 0x94       = 0x%08X\n", readl(iobase_iomux + 0x94));
		writel(0x00000005, iobase_iomux + 0x98);	/* Pad EIM_D18:  ALT5 = GPIO3_IO18 => Density (DRAM_ID0) */
		pr_info("get_sdram_config: read iobase_iomux + 0x98       = 0x%08X\n", readl(iobase_iomux + 0x98));
		break;
	default:
		hang();
	}
	pr_info("get_sdram_config: iobase_density                 = 0x%08X\n", (unsigned int)iobase_density + 0x0);
	pr_info("get_sdram_config: read iobase_density + 0x0      = 0x%08X\n", readl(iobase_density + 0x0));

	pr_info("get_sdram_config: iobase_buswidth                = 0x%08X\n", (unsigned int)iobase_buswidth + 0x0);
	pr_info("get_sdram_config: read iobase_buswidth + 0x0     = 0x%08X\n", readl(iobase_buswidth + 0x0));

	/* read density of sdram devices and shift to bit 1 0*/
	val = ((readl(iobase_density + 0x0) & MASK_DENSITY) >> DENSITY_SHIFT);

	/* read buswidth (32 or 64 bit) and shift to bit 2 */
	val |= ((readl(iobase_buswidth + 0x0) & MASK_BUSWIDTH) >> BUSWIDTH_SHIFT);

	pr_info("get_sdram_config: val                            = 0x%08X\n", val);

	pr_info("\n");

	return val;
}

static void dmo_ppc_init(int cpu_type)
{
	unsigned long sdram_size;
	uint32_t sdram_cfg;
	unsigned long pc = get_pc();

	pr_info("\ndmo_ppc_init: pc = 0x%08lx\n", pc);

	if (pc > 0x10000000)
		return;

	__udelay(1000);

	sdram_cfg = get_sdram_config(cpu_type);

	sdram_size = dmo_ppc_dram_init(sdram_cfg);


	pr_info("SDRAM init finished. SDRAM size 0x%08lx\n", sdram_size);

	imx6_esdhc_start_image(2);
	pr_info("Loading image from SPI flash\n");

	imx6_spi_start_image(2);
}

extern char __dtb_imx6dl_dmo_ppc_start[];
extern char __dtb_imx6q_dmo_ppc_start[];

static noinline void dmo_ppc_start(void)
{
	int cpu_type = __imx6_cpu_type();
	void *dtb;

	dmo_ppc_init(cpu_type);

	pr_info("Runnig from DRAM!\n");
	switch (cpu_type) {
	case IMX6_CPUTYPE_IMX6S:
	case IMX6_CPUTYPE_IMX6DL:
		dtb = __dtb_imx6dl_dmo_ppc_start - get_runtime_offset();
		break;
	case IMX6_CPUTYPE_IMX6D:
	case IMX6_CPUTYPE_IMX6Q:
		dtb = __dtb_imx6q_dmo_ppc_start - get_runtime_offset();
		break;
	default:
		hang();
	}

	imx6q_barebox_entry(dtb);
}

/*
 * to see all the pr_info on the console,
 * enable CONFIG_PBL_CONSOLE in menuconfig
 */

/*
 * ENTRY_FUNCTION(start_dmo_imx6_ppc, r0, r1, r2)
 * = entry function for dynamic DRAM detection
 */

ENTRY_FUNCTION(start_dmo_imx6_ppc, r0, r1, r2)
{
	int cpu_type = __imx6_cpu_type();
	imx6_cpu_lowlevel_init();

	arm_setup_stack(STACK_SDL - 8);

	relocate_to_current_adr();
	setup_c();
	barrier();

	setup_uart();

	pr_info("\nstart_dmo_imx6_ppc: stack before re-init = 0x%08X\n", (unsigned int)get_sp());

	/* re-init stack according to cpu-type after relocation */
	switch (cpu_type) {
	case IMX6_CPUTYPE_IMX6S:
	case IMX6_CPUTYPE_IMX6DL:
		arm_setup_stack(STACK_SDL - 8);
		break;
	case IMX6_CPUTYPE_IMX6D:
	case IMX6_CPUTYPE_IMX6Q:
		arm_setup_stack(STACK_DQ - 8);
		break;
	default:
		hang();
	}
	pr_info("start_dmo_imx6_ppc: stack after  re-init = 0x%08X\n", (unsigned int)get_sp());

	dmo_ppc_start();
}


/*
 * The following ENTRY_FUNCTION(start_dmo_imx6x_..., r0, r1, r2)
 * are the entry functions for the respective bareboy recovery
 * images needed for loading via imx_usb loader
 *
 * Each recovery image needs its own entry function, therefore
 * multiple functions are listed below.
 *
 * When there will be a new ppc variant with new DRAM configuration,
 * new entry function has to be created and the MAKEFILES to be updated
 *
 * .../arch/arb/boards/dmo-imx6-ppc/Makefile
 * .../images/Makefile.imx
 *
 * naming convention:
 * start_dmo_imx6xx_ppc_density_buswidth
 * xx		= dl (iMX6 solo/duallite); q (iMX6 dual/quad)
 * density	= 2G; 4G
 * buswidth	= 32bit; 64bit
 *
 * e.g.: start_dmo_imx6q_ppc_4G_64bit
 *
 * also valid for the flash-header-files,
 * e.g.: flash-header-dmo-imx6q-ppc_4G_64bit.imxcfg
 *
 *
 */

ENTRY_FUNCTION(start_dmo_imx6dl_ppc_2G_32bit, r0, r1, r2)
{
	unsigned long sdram = 0x08000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(STACK_SDL - 8); /* according to IMXSDLRM.pdf: 8.4.1 Internal ROM /RAM memory map */

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6dl_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_512M, fdt);
}

ENTRY_FUNCTION(start_dmo_imx6dl_ppc_4G_32bit, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(STACK_SDL - 8); /* according to IMXSDLRM.pdf: 8.4.1 Internal ROM /RAM memory map */

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6dl_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_1G, fdt);
}

ENTRY_FUNCTION(start_dmo_imx6q_ppc_4G_32bit, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(STACK_DQ - 8); /* according to IMXDQRM.pdf: 8.4.1 Internal ROM /RAM memory map */

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6q_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_1G, fdt);
}

ENTRY_FUNCTION(start_dmo_imx6q_ppc_4G_64bit, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(STACK_DQ - 8); /* according to IMXDQRM.pdf: 8.4.1 Internal ROM /RAM memory map */

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6q_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_2G, fdt);
}

ENTRY_FUNCTION(start_imx6q_dmo_imx6q_ppc_2G_64bit, r0, r1, r2)
{
	unsigned long sdram = 0x10000000;
	void *fdt;

	imx6_cpu_lowlevel_init();

	arm_setup_stack(STACK_DQ - 8); /* according to IMXDQRM.pdf: 8.4.1 Internal ROM /RAM memory map */

	arm_early_mmu_cache_invalidate();

	fdt = __dtb_imx6q_dmo_ppc_start - get_runtime_offset();

	barebox_arm_entry(sdram, SZ_1G, fdt);
}


