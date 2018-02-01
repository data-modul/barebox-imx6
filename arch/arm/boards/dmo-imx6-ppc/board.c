/*
 * Copyright (C) 2014 Data-Modul AG, Silvio Fricke <silvio.fricke@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation.
 *
 */

#include <bootsource.h>
#include <common.h>
#include <gpio.h>
#include <init.h>
#include <mach/bbu.h>
#include <mach/generic.h>
#include <of.h>
#include <linux/phy.h>
#include <dt-bindings/net/ti-dp83867.h>

#define DMO_IMX6_PPC_PIN_USB_OTG_PWR_EN		IMX_GPIO_NR(3, 22)
#define DMO_IMX6_PPC_PIN_USB_HUB_RST		IMX_GPIO_NR(7, 11)
#define DMO_IMX6_PPC_PIN_ENET_CRS_DV		IMX_GPIO_NR(1, 25)

#define PHY_ID_DP83867				0x2000A231
#define PHY_ID_MASK				0xffffffff
#define DP83867_DEVADDR				0x1F
#define DP83867_IO_MUX_CFG			0x0170
#define DP83867_CLK_OUT_MASK			0x1F
#define DP83867_CLK_OUT_CHAN_A_RX_CLK		0x00
#define DP83867_RGMIICTL			0x0032
#define DP83867_CLK_OUT_SHIFT			8
#define DP83867_RGMII_TX_CLK_DELAY_SHIFT	4
#define DP83867_RGMIIDCTL			0x0086
#define DP83867_RGMII_RX_CLK_DELAY_EN		BIT(0)
#define DP83867_RGMII_TX_CLK_DELAY_EN		BIT(1)

static struct swu_hook hook;

/* The PHY fixup comes from the dp83867_config_init functions from kernel4.4.57
 * The phy driver is missed in Barebox, thus the config part from Kernel driver 
 * copied here as fixed up. Therefore the below function comes from DP83867 
 * driver in Kernel 4.4.57 */
static int dp83867_phy_fixup(struct phy_device *dev)
{
	u16 val = 0, delay;

	/* reset PHY is needed, because by sw-reset is not really reset*/
	gpio_direction_output(DMO_IMX6_PPC_PIN_ENET_CRS_DV, 0);
	mdelay(10);
	gpio_direction_output(DMO_IMX6_PPC_PIN_ENET_CRS_DV, 1);

	/* config phy */
	val  = phy_read_mmd_indirect(dev, DP83867_IO_MUX_CFG, DP83867_DEVADDR);
	val &= ~( DP83867_CLK_OUT_MASK << DP83867_CLK_OUT_SHIFT);
	val |= ( DP83867_CLK_OUT_CHAN_A_RX_CLK << DP83867_CLK_OUT_SHIFT);
	phy_write_mmd_indirect(dev, DP83867_IO_MUX_CFG, DP83867_DEVADDR, val);

	val  = phy_read_mmd_indirect(dev, DP83867_RGMIICTL, DP83867_DEVADDR);
	val |= (DP83867_RGMII_TX_CLK_DELAY_EN | DP83867_RGMII_RX_CLK_DELAY_EN);
	phy_write_mmd_indirect(dev, DP83867_RGMIICTL, DP83867_DEVADDR, val);

	delay = (DP83867_RGMIIDCTL_2_00_NS | (DP83867_RGMIIDCTL_1_50_NS << DP83867_RGMII_TX_CLK_DELAY_SHIFT));
	phy_write_mmd_indirect(dev, DP83867_RGMIIDCTL, DP83867_DEVADDR, delay);

	return 0;
}

static int swu_display(struct swu_hook *r)
{
	switch (r->status) {
		case PREPARATION:
			printf("Preparing Software update!\n");
			break;
		case SUCCESS:
			printf("Software update successfully finished!\n");
			break;
		case FAIL:
			printf("Software update failed!\n");
			break;
		case PROGRESS:
			printf("Software update in progress!\n");
			break;
		default:
			printf("Disabling Software update!\n");
		}

	return 0;
}

static int DMO_IMX6_PPC_env_init(void)
{
	if (! (of_machine_is_compatible("dmo,imx6dl-ppc") ||
		of_machine_is_compatible("dmo,imx6q-ppc")))
		return 0;

	imx6_bbu_internal_spi_i2c_register_handler("spiflash", "/dev/m25p0.barebox", BBU_HANDLER_FLAG_DEFAULT);
	imx6_bbu_internal_mmc_register_handler("mmc", "/dev/mmc2.barebox", 0);

	if (IS_ENABLED(CONFIG_DMO_SWU)) {
		swu_register_dmo_handlers();
		hook.func = swu_display;
		swu_register_hook(&hook);
	}

	return 0;
}
late_initcall(DMO_IMX6_PPC_env_init);

static int DMO_IMX6_PPC_device_init(void)
{
	if (! (of_machine_is_compatible("dmo,imx6dl-ppc") ||
		of_machine_is_compatible("dmo,imx6q-ppc")))
		return 0;

	/* ensure that we are in usb host mode */
	gpio_direction_output(DMO_IMX6_PPC_PIN_USB_OTG_PWR_EN, 1);
	gpio_direction_output(DMO_IMX6_PPC_PIN_USB_HUB_RST, 1);

	/* ENET - set clk_out of phy */
	phy_register_fixup_for_uid(PHY_ID_DP83867, PHY_ID_MASK,
			dp83867_phy_fixup);

	barebox_set_hostname("dmo-imx6-ppc");

	switch (bootsource_get()) {
		case BOOTSOURCE_MMC:
			of_device_enable_path("/chosen/environment-sd");
			break;

		default:
		case BOOTSOURCE_SPI:
			of_device_enable_path("/chosen/environment-spi");
			break;
	};

	return 0;
}
device_initcall(DMO_IMX6_PPC_device_init);
