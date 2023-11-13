// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for AM62Ax platforms
 *
 * Copyright (C) 2022 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright 2023 TechNexion Ltd.
 * Author: Ray Chang <ray.chang@technexion.com>
 *
 */

#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <asm/io.h>
#include <common.h>
#include <dm/uclass.h>
#include <env.h>
#include <fdt_support.h>
#include <spl.h>

#define PSRAMECC0_RAM_BOOT_DEVICE 0x00000000

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

#if defined(CONFIG_SPL_LOAD_FIT)
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif

#define CTRLMMR_USB0_PHY_CTRL	0x43004008
#define CTRLMMR_USB1_PHY_CTRL	0x43004018
#define CORE_VOLTAGE		0x80000000

#ifdef CONFIG_BOARD_LATE_INIT
static int am6_boot_dev(void) {
    return readl(PSRAMECC0_RAM_BOOT_DEVICE);
}

void detect_boot_dev(void)
{
	switch (am6_boot_dev()) {
	case BOOT_DEVICE_MMC1:
		env_set_ulong("mmcdev", 0);
		env_set("bootpart", "0:2");
		printf("Boot Device: MMC\n");
		break;
	case BOOT_DEVICE_MMC2:
		env_set_ulong("mmcdev", 1);
		env_set("bootpart", "1:2");
		printf("Boot Device: SD\n");
		break;
	default:
		printf("Boot Device: Unknown\n");
		break;
	}
}

int board_late_init(void)
{
	detect_boot_dev();
	return 0;
}
#endif

#if defined(CONFIG_SPL_BOARD_INIT)
void spl_board_init(void)
{
	u32 val;

	/* Set USB0 PHY core voltage to 0.85V */
	val = readl(CTRLMMR_USB0_PHY_CTRL);
	val &= ~(CORE_VOLTAGE);
	writel(val, CTRLMMR_USB0_PHY_CTRL);

	/* Set USB1 PHY core voltage to 0.85V */
	val = readl(CTRLMMR_USB1_PHY_CTRL);
	val &= ~(CORE_VOLTAGE);
	writel(val, CTRLMMR_USB1_PHY_CTRL);

	/* Store boot_device for U-Boot */
	writel(spl_boot_device(), PSRAMECC0_RAM_BOOT_DEVICE);
}
#endif
