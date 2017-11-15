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

#define DMO_IMX6_PPC_PIN_USB_OTG_PWR_EN		IMX_GPIO_NR(3, 22)
#define DMO_IMX6_PPC_PIN_USB_HUB_RST		IMX_GPIO_NR(7, 11)

static struct swu_hook hook;

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
	if (!of_machine_is_compatible("dmo,imx6-ppc"))
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
	if (!of_machine_is_compatible("dmo,imx6-ppc"))
		return 0;

	/* ensure that we are in usb host mode */
	gpio_direction_output(DMO_IMX6_PPC_PIN_USB_OTG_PWR_EN, 1);
	gpio_direction_output(DMO_IMX6_PPC_PIN_USB_HUB_RST, 1);

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
