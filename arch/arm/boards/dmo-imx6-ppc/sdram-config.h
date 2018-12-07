#ifndef _SDRAM_CONFIG
#define _SDRAM_CONFIG

#include <mach/imx6-mmdc.h>

#define MASK_DENSITY	0x00060000
#define MASK_DENSITY_2	0x00000003
#define DENSITY_1		0x00000000
#define DENSITY_2		0x00000001
#define DENSITY_4		0x00000002
#define DENSITY_8		0x00000003
#define DENSITY_SHIFT	17

#define MASK_BUSWIDTH	0x00000004
#define BUSWIDTH		0x00000004
#define BUSWIDTH_SHIFT	0

#define MASK_HW_ID			0x0014FC00	/* HWID[7:0] */
#define MASK_HW_PCB_REV		0x00141000	/* HWID[7:5] */
#define MASK_HW_PCB_ASS		0x0000EC00	/* HWID[4:0] */

#define IOBASE_IOMUX		0x020E0000	/* IOMUX */
#define IOBASE_HW_ID		0x0209C000	/* GPIO1 */
#define IOBASE_DENSITY		0x020A4000	/* GPIO3 */
#define IOBASE_BUSWIDTH		0x020AC000	/* GPIO5 */

#define STACK_SDL		0x0091FFB8	/* according to IMXSDLRM.pdf: 8.4.1 Internal ROM /RAM memory map */
#define STACK_DQ		0x0093FFB8	/* according to IMXDQRM.pdf: 8.4.1 Internal ROM /RAM memory map */

/* density 8GBit: ToDo: final adaption after redesign is finished */
static struct mx6_ddr3_cfg density_8gbit = {
	.mem_speed = 1600,
	.density = 8,
	.width = 16,
	.banks = 8,
	.rowaddr = 16,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1100,
	.trcmin = 3900,
	.trasmin = 2800,
};

/* density 4GBit */
static struct mx6_ddr3_cfg density_4gbit = {
	.mem_speed = 1600,
	.density = 4,
	.width = 16,
	.banks = 8,
	.rowaddr = 15,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};

/* density 2GBit */
static struct mx6_ddr3_cfg density_2gbit = {
	.mem_speed = 1866,
	.density = 2,
	.width = 16,
	.banks = 8,
	.rowaddr = 14,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1391,
	.trcmin = 4791,
	.trasmin = 3400,
};

/* DDR3-2GB (4x4Gbit  (256x16Mbit), 12021864) */
static struct mx6_mmdc_calibration mx6_2g_64b_mmdc_calib = {
	.p0_mpwldectrl0 = 0x00110011,
	.p0_mpwldectrl1 = 0x001E000F,
	.p1_mpwldectrl0 = 0x000B001F,
	.p1_mpwldectrl1 = 0x00160018,
	.p0_mpdgctrl0 = 0x031C0328,
	.p0_mpdgctrl1 = 0x03180314,
	.p1_mpdgctrl0 = 0x0324032C,
	.p1_mpdgctrl1 = 0x0318026C,
	.p0_mprddlctl = 0x3E34363A,
	.p1_mprddlctl = 0x3C383240,
	.p0_mpwrdlctl = 0x3C3A3E3E,
	.p1_mpwrdlctl = 0x4A3A4E40,
};

/* DDR3-1GB (4x2Gbit (128x16Mbit), 12033818) */
static struct mx6_mmdc_calibration mx6_1g_64b_mmdc_calib = { 
	.p0_mpwldectrl0 = 0x00110015,
	.p0_mpwldectrl1 = 0x001B0015,
	.p1_mpwldectrl0 = 0x000C0017,
	.p1_mpwldectrl1 = 0x00090014,
	.p0_mpdgctrl0 = 0x02380240,
	.p0_mpdgctrl1 = 0x02300234,
	.p1_mpdgctrl0 = 0x02380240,
	.p1_mpdgctrl1 = 0x022C0214,
	.p0_mprddlctl = 0x3E343438,
	.p1_mprddlctl = 0x3A383640,
	.p0_mpwrdlctl = 0x38363C3C,
	.p1_mpwrdlctl = 0x46364440,
};

/*  DDR3-2GB (2x8Gbit (512x16 Mbit), 12033822) ToDo: final adaption currently not intended; kept here as placeholder */
static struct mx6_mmdc_calibration mx6_2g_32b_mmdc_calib = {
	.p0_mpwldectrl0 = 0x001f001f,
	.p0_mpwldectrl1 = 0x001f001f,
	.p0_mpdgctrl0 = 0x021b083c,
	.p0_mpdgctrl1 = 0x02380238,
	.p0_mprddlctl = 0x3E444242,
	.p0_mpwrdlctl = 0x3A363432,
};

/* DDR3-1GB (2x4Gbit  (256x16Mbit), 12021864) */
static struct mx6_mmdc_calibration mx6_1g_32b_mmdc_calib = {
	.p0_mpwldectrl0 = 0x00500059,
	.p0_mpwldectrl1 = 0x00440049,
	.p0_mpdgctrl0 = 0x024C0248,
	.p0_mpdgctrl1 = 0x02380238,
	.p0_mprddlctl = 0x3E444242,
	.p0_mpwrdlctl = 0x3A363432,
};

/* DDR3-512MB (2x2Gbit (128x16Mbit), 12033818) */
static struct mx6_mmdc_calibration mx6_512m_32b_mmdc_calib = {
	.p0_mpwldectrl0 = 0x0044004D,
	.p0_mpwldectrl1 = 0x0039003F,
	.p0_mpdgctrl0 = 0x0240023C,
	.p0_mpdgctrl1 = 0x0224022C,
	.p0_mprddlctl = 0x42444646,
	.p0_mpwrdlctl = 0x36382E34,
};

/* DDR 64bit 2GB */
static struct mx6_ddr_sysinfo mem_2gb_64bit = {
	.dsize		= 2,
	.cs1_mirror	= 0,
	.cs_density	= 16,
	.ncs		= 1,
	.bi_on		= 1,
	.rtt_nom	= 1,
	.rtt_wr		= 0,
	.ralat		= 5,
	.walat		= 0,
	.mif3_mode	= 3,
	.rst_to_cke	= 0x23,
	.sde_to_rst	= 0x10,
};

/* DDR 64bit 1GB */
static struct mx6_ddr_sysinfo mem_1gb_64bit = {
	.dsize		= 2,
	.cs1_mirror	= 0,
	.cs_density	= 8,
	.ncs		= 1,
	.bi_on		= 1,
	.rtt_nom	= 1,
	.rtt_wr		= 0,
	.ralat		= 5,
	.walat		= 0,
	.mif3_mode	= 3,
	.rst_to_cke	= 0x23,
	.sde_to_rst	= 0x10,
};

/* DDR 32bit 2GB ToDo: final adaption currently not intended; kept here as placeholder */
static struct mx6_ddr_sysinfo mem_2gb_32bit = {
	.dsize		= 1,
	.cs1_mirror	= 0,
	.cs_density	= 16,
	.ncs		= 1,
	.bi_on		= 1,
	.rtt_nom	= 1,
	.rtt_wr		= 0,
	.ralat		= 5,
	.walat		= 0,
	.mif3_mode	= 3,
	.rst_to_cke	= 0x23,
	.sde_to_rst	= 0x10,
};

/* DDR 32bit 1GB */
static struct mx6_ddr_sysinfo mem_1gb_32bit = {
	.dsize		= 1,
	.cs1_mirror	= 0,
	.cs_density	= 8,
	.ncs		= 1,
	.bi_on		= 1,
	.rtt_nom	= 1,
	.rtt_wr		= 0,
	.ralat		= 5,
	.walat		= 0,
	.mif3_mode	= 3,
	.rst_to_cke	= 0x23,
	.sde_to_rst	= 0x10,
};

/* DDR 32bit 512MB */
static struct mx6_ddr_sysinfo mem_512mb_32bit = {
	.dsize		= 1,
	.cs1_mirror	= 0,
	.cs_density	= 4,
	.ncs		= 1,
	.bi_on		= 1,
	.rtt_nom	= 1,
	.rtt_wr		= 0,
	.ralat		= 5,
	.walat		= 0,
	.mif3_mode	= 3,
	.rst_to_cke	= 0x23,
	.sde_to_rst	= 0x10,
};



#endif /* _SDRAM_CONFIG */

