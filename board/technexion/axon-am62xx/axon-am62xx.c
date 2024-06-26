// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for AXON-AM62XX platforms
 *
 * Copyright (C) 2024 TechNexion Ltd.
 *
 */

#include <env.h>
#include <spl.h>
#include <init.h>
#include <video.h>
#include <splash.h>
#include <cpu_func.h>
#include <k3-ddrss.h>
#include <fdt_support.h>
#include <fdt_simplefb.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <dm/uclass.h>
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

#if CONFIG_IS_ENABLED(SPLASH_SCREEN)
static struct splash_location default_splash_locations[] = {
	{
		.name = "sf",
		.storage = SPLASH_STORAGE_SF,
		.flags = SPLASH_STORAGE_RAW,
		.offset = 0x700000,
	},
	{
		.name		= "mmc",
		.storage	= SPLASH_STORAGE_MMC,
		.flags		= SPLASH_STORAGE_FS,
		.devpart	= "1:1",
	},
};

int splash_screen_prepare(void)
{
	return splash_source_load(default_splash_locations,
				ARRAY_SIZE(default_splash_locations));
}
#endif

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

phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
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
	gd->bd->bi_dram[0].start = CFG_SYS_SDRAM_BASE;
	if (ddrcode == DDR4_1GB) {
		gd->bd->bi_dram[0].size = 0x40000000;
		gd->ram_size = 0x40000000;
	} else {
		gd->bd->bi_dram[0].size = 0x80000000;
		gd->ram_size = 0x80000000;
	}

#ifdef CONFIG_PHYS_64BIT
	/* Bank 1 declares the memory available in the DDR high region */
	gd->bd->bi_dram[1].start = CFG_SYS_SDRAM_BASE1;
	if (ddrcode == DDR4_4GB) {
		gd->bd->bi_dram[1].size = 0x80000000;
		gd->ram_size = 0x100000000;
	}
#endif

	return 0;
}

#if CONFIG_IS_ENABLED(BOARD_LATE_INIT)
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
	char fdtfile[50];
	char *baseboard;

	if (!env_get("fdtfile")) {
		baseboard = env_get("baseboard");
		snprintf(fdtfile, sizeof(fdtfile), "%s/%s-%s.dtb",
			CONFIG_TI_FDT_FOLDER_PATH, CONFIG_DEFAULT_DEVICE_TREE, baseboard);

		env_set("fdtfile", fdtfile);
	}
	detect_boot_dev();
	tn_setup_mac_address();
	return 0;
}
#endif

#if defined(CONFIG_SPL_BUILD)

void spl_board_init(void)
{
	u32 val;

	/* We have 32k crystal, so lets enable it */
	val = readl(MCU_CTRL_LFXOSC_CTRL);
	val &= ~(MCU_CTRL_LFXOSC_32K_DISABLE_VAL);
	writel(val, MCU_CTRL_LFXOSC_CTRL);
	/* Add any TRIM needed for the crystal here.. */
	/* Make sure to mux up to take the SoC 32k from the crystal */
	writel(MCU_CTRL_DEVICE_CLKOUT_LFOSC_SELECT_VAL,
	       MCU_CTRL_DEVICE_CLKOUT_32K_CTRL);

	enable_caches();
	if (IS_ENABLED(CONFIG_SPL_SPLASH_SCREEN) && IS_ENABLED(CONFIG_SPL_BMP))
		splash_display();

	/* Store boot_device for U-Boot */
	writel(spl_boot_device(), PSRAMECC0_RAM_BOOT_DEVICE);
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

#if defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret = -1;

	if (IS_ENABLED(CONFIG_FDT_SIMPLEFB))
		ret = fdt_simplefb_enable_and_mem_rsv(blob);

	/* If simplefb is not enabled and video is active, then at least reserve
	 * the framebuffer region to preserve the splash screen while OS is booting
	 */
	if (IS_ENABLED(CONFIG_VIDEO) && IS_ENABLED(CONFIG_OF_LIBFDT)) {
		if (ret && video_is_active())
			return fdt_add_fb_mem_rsv(blob);
	}

	return 0;
}
#endif
