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

#define MASK_BUSWIDTH	0x00100000	/* ToDo: final adaption after redsign is finished */
#define BUSWIDTH		0x00000004
#define BUSWIDTH_SHIFT	18	/* ToDo: final adaption after redsign is finished */

#define MASK_HW_ID			0x0014FC00	/* HWID[7:0] */
#define MASK_HW_PCB_REV		0x00141000	/* HWID[7:5] */
#define MASK_HW_PCB_ASS		0x0000EC00	/* HWID[4:0] */

#define IOBASE_HW_ID		0x0209C000
#define IOBASE_DENSITY		0x020A4000
#define IOBASE_BUSWIDTH		0x020A4000	/* ToDo: final adaption after redsign is finished */

#define STACK_SDL		0x0091FFB8	/* according to IMXSDLRM.pdf: 8.4.1 Internal ROM /RAM memory map */
#define STACK_DQ		0x0093FFB8	/* according to IMXDQRM.pdf: 8.4.1 Internal ROM /RAM memory map */

/* density 8GBit: ToDo: final adaption after redsign is finished */
static struct mx6_ddr3_cfg density_8gbit = {
	.mem_speed = 1600,
	.density = 8,
	.width = 16,
	.banks = 8,
	.rowaddr = 15,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
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
	.mem_speed = 1333,
	.density = 2,
	.width = 16,
	.banks = 8,
	.rowaddr = 14,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1350,
	.trcmin = 4950,
	.trasmin = 3600,
};

/* DDR3-2GB (4x4Gbit  (256x16Mbit), 12021864) */
static struct mx6_mmdc_calibration mx6_2g_64b_mmdc_calib = {
	.p0_mpwldectrl0 = 0x001f001f,
	.p0_mpwldectrl1 = 0x001f001f,
	.p1_mpwldectrl0 = 0x001f001f,
	.p1_mpwldectrl1 = 0x001f001f,
	.p0_mpdgctrl0 = 0x4301030d,
	.p0_mpdgctrl1 = 0x03020277,
	.p1_mpdgctrl0 = 0x4300030a,
	.p1_mpdgctrl1 = 0x02780248,
	.p0_mprddlctl = 0x4536393b,
	.p1_mprddlctl = 0x36353441,
	.p0_mpwrdlctl = 0x41414743,
	.p1_mpwrdlctl = 0x462f453f,
};

/* DDR3-1GB (4x2Gbit (128x16Mbit), 12033818) ToDo: final adaption after redsign is finished */
static struct mx6_mmdc_calibration mx6_1g_64b_mmdc_calib = { 
	.p0_mpwldectrl0 = 0x001f001f,
	.p0_mpwldectrl1 = 0x001f001f,
	.p1_mpwldectrl0 = 0x001f001f,
	.p1_mpwldectrl1 = 0x001f001f,
	.p0_mpdgctrl0 = 0x4301030d,
	.p0_mpdgctrl1 = 0x03020277,
	.p1_mpdgctrl0 = 0x4300030a,
	.p1_mpdgctrl1 = 0x02780248,
	.p0_mprddlctl = 0x4536393b,
	.p1_mprddlctl = 0x36353441,
	.p0_mpwrdlctl = 0x41414743,
	.p1_mpwrdlctl = 0x462f453f,
};

/*  DDR3-2GB (2x8Gbit (512x16 Mbit), 12033822) ToDo: final adaption after redsign is finished */
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
	.p0_mpwldectrl0 = 0x001f001f,
	.p0_mpwldectrl1 = 0x001f001f,
	.p0_mpdgctrl0 = 0x021b083c,
	.p0_mpdgctrl1 = 0x02380238,
	.p0_mprddlctl = 0x3E444242,
	.p0_mpwrdlctl = 0x3A363432,
};

/* DDR3-512MB (2x2Gbit (128x16Mbit), 12033818) */
static struct mx6_mmdc_calibration mx6_512m_32b_mmdc_calib = {
	.p0_mpwldectrl0 = 0x0040003c,
	.p0_mpwldectrl1 = 0x0032003e,
	.p0_mpdgctrl0 = 0x42350231,
	.p0_mpdgctrl1 = 0x021a0218,
	.p0_mprddlctl = 0x4b4b4e49,
	.p0_mpwrdlctl = 0x3f3f3035,
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

/* DDR 32bit 2GB ToDo: final adaption after redsign is finished */
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

