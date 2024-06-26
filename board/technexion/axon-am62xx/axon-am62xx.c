// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for AM62x platforms
 *
 * Copyright (C) 2020-2022 Texas Instruments Incorporated - https://www.ti.com/
 *	Suman Anna <s-anna@ti.com>
 *
 * Copyright 2023 TechNexion Ltd.
 * Ray Chang <ray.chang@technexion.com>
 *
 */

#include <common.h>
#include <asm/io.h>
#include <env.h>
#include <net.h>
#include <spl.h>
#include <dm/uclass.h>
#include <k3-ddrss.h>
#include <fdt_support.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include "../drivers/ram/k3-ddrss/am64/lpddr4_am64_if.h"
#include "../common/tn_eeprom.h"

#define PSRAMECC0_RAM_BOOT_DEVICE 0x00000000

#define MCU_PADCFG_CTRL0_CFG0_PADCONFIG10	0x04084028
#define RXACTIVE				BIT(18)
#define MUXMODE_7				0x07
#define GPIO_IN_DATA01				0x04201020
#define MCU_GPIO0_9				BIT(9)
#define MCU_GPIO0_10				BIT(10)

DECLARE_GLOBAL_DATA_PTR;

/**********************************************
* Revision Detection
*
* DDR_TYPE_DET_1   DDR_TYPE_DET_0
* GPIO0_10          GPIO0_9
*     0                1           4GB DDR4
*     1                0           2GB DDR4
*     0                0           1GB DDR4
***********************************************/
enum {
	DDR4_4GB = 0x1,
	DDR4_2GB = 0x2,
	DDR4_1GB = 0x0,
};

int board_init(void)
{
	return 0;
}

int dram_get_ddrcode(void)
{
	u32 val;

	val = readl(MCU_PADCFG_CTRL0_CFG0_PADCONFIG10);
	if(!(val & (RXACTIVE | MUXMODE_7)))
		writel(val | RXACTIVE | MUXMODE_7, MCU_PADCFG_CTRL0_CFG0_PADCONFIG10);
	return ((readl(GPIO_IN_DATA01) & (MCU_GPIO0_9 | MCU_GPIO0_10)) >> 9);
}

int dram_init(void)
{
#ifdef CONFIG_PHYS_64BIT
	switch (dram_get_ddrcode()) {
		case DDR4_1GB:
			gd->ram_size = 0x40000000;
			break;
		case DDR4_2GB:
			gd->ram_size = 0x80000000;
			break;
		case DDR4_4GB:
			gd->ram_size = 0x100000000;
			break;
		default:
			puts("Unknown DDR type!!!\n");
			break;
	}
#else
	gd->ram_size = 0x80000000;
#endif
	return 0;
}

ulong board_get_usable_ram_top(ulong total_size)
{
#ifdef CONFIG_PHYS_64BIT
	/* Limit RAM used by U-Boot to the DDR low region */
	if (gd->ram_top > 0x100000000)
		return 0x100000000;
#endif

	return gd->ram_top;
}

int dram_init_banksize(void)
{
	int ddrcode = dram_get_ddrcode();

	/* Bank 0 declares the memory available in the DDR low region */
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	if (ddrcode == DDR4_1GB) {
		gd->bd->bi_dram[0].size = 0x40000000;
		gd->ram_size = 0x40000000;
	} else {
		gd->bd->bi_dram[0].size = 0x80000000;
		gd->ram_size = 0x80000000;
	}

#ifdef CONFIG_PHYS_64BIT
	/* Bank 1 declares the memory available in the DDR high region */
	gd->bd->bi_dram[1].start = CONFIG_SYS_SDRAM_BASE1;
	if (ddrcode == DDR4_4GB) {
		gd->bd->bi_dram[1].size = 0x80000000;
		gd->ram_size = 0x100000000;
	}
#endif

	return 0;
}

#if defined(CONFIG_SPL_LOAD_FIT)
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif

#if defined(CONFIG_SPL_BUILD)
#if defined(CONFIG_K3_AM64_DDRSS)
static void fixup_ddr_driver_for_ecc(struct spl_image_info *spl_image)
{
	struct udevice *dev;
	int ret;

	dram_init_banksize();

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("Cannot get RAM device for ddr size fixup: %d\n", ret);

	ret = k3_ddrss_ddr_fdt_fixup(dev, spl_image->fdt_addr, gd->bd);
	if (ret)
		printf("Error fixing up ddr node for ECC use! %d\n", ret);
}
#else
static void fixup_memory_node(struct spl_image_info *spl_image)
{
	u64 start[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	int bank;
	int ret;

	dram_init();
	dram_init_banksize();

	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		start[bank] =  gd->bd->bi_dram[bank].start;
		size[bank] = gd->bd->bi_dram[bank].size;
	}

	/* dram_init functions use SPL fdt, and we must fixup u-boot fdt */
	ret = fdt_fixup_memory_banks(spl_image->fdt_addr,
				     start, size, CONFIG_NR_DRAM_BANKS);
	if (ret)
		printf("Error fixing up memory node! %d\n", ret);
}
#endif

void spl_perform_fixups(struct spl_image_info *spl_image)
{
#if defined(CONFIG_K3_AM64_DDRSS)
	fixup_ddr_driver_for_ecc(spl_image);
#else
	fixup_memory_node(spl_image);
#endif
}

struct reginitdata {
	u32 ctl_regs[LPDDR4_INTR_CTL_REG_COUNT];
	u16 ctl_regs_offs[LPDDR4_INTR_CTL_REG_COUNT];
	u32 pi_regs[LPDDR4_INTR_PHY_INDEP_REG_COUNT];
	u16 pi_regs_offs[LPDDR4_INTR_PHY_INDEP_REG_COUNT];
	u32 phy_regs[LPDDR4_INTR_PHY_REG_COUNT];
	u16 phy_regs_offs[LPDDR4_INTR_PHY_REG_COUNT];
};

int k3_lpddr4_board_update(struct reginitdata *reginit_data)
{
	switch (dram_get_ddrcode()) {
		case DDR4_1GB:
			reginit_data->ctl_regs[317] = 0x00000101;
			reginit_data->ctl_regs[318] = 0x1FFF0000;
			reginit_data->pi_regs[77] = 0x04010100;
			break;
		case DDR4_4GB:
			reginit_data->ctl_regs[317] = 0xFFFFFEFF;
			reginit_data->ctl_regs[318] = 0x7FFF0000;
			reginit_data->pi_regs[77] = 0x03FF0100;
			break;
	}

	return 0;
}
#endif

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
	tn_setup_mac_address();
	return 0;
}
#endif

#define CTRLMMR_USB0_PHY_CTRL	0x43004008
#define CTRLMMR_USB1_PHY_CTRL	0x43004018
#define CORE_VOLTAGE		0x80000000

#ifdef CONFIG_SPL_BOARD_INIT
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

	/* We have 32k crystal, so lets enable it */
	val = readl(MCU_CTRL_LFXOSC_CTRL);
	val &= ~(MCU_CTRL_LFXOSC_32K_DISABLE_VAL);
	writel(val, MCU_CTRL_LFXOSC_CTRL);
	/* Add any TRIM needed for the crystal here.. */
	/* Make sure to mux up to take the SoC 32k from the crystal */
	writel(MCU_CTRL_DEVICE_CLKOUT_LFOSC_SELECT_VAL,
	       MCU_CTRL_DEVICE_CLKOUT_32K_CTRL);

	/* Init DRAM size for R5/A53 SPL */
	dram_init_banksize();

	/* Store boot_device for U-Boot */
	writel(spl_boot_device(), PSRAMECC0_RAM_BOOT_DEVICE);
}
#endif
