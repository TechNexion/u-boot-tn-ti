// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for K3 J721E ROVY-4VM
 *
 * Copyright (C) 2024 Technexion Ltd.
 * Author: Richard Hu <richard.hu@technexion.com>
 *
 */

#include <common.h>
#include <env.h>
#include <fdt_support.h>
#include <generic-phy.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <net.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hardware.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <spl.h>
#include <asm/arch/sys_proto.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <env_internal.h>
#include "../drivers/ram/k3-ddrss/j721e/lpddr4_j721e_if.h"

#define CTRLMMR_MCU_ADC1_CTRL (MCU_CTRL_MMR0_BASE + 0x40B4)
#define GPI_MODE_EN 			BIT(16)
#define GPIO_IN_DATA45			0x42110070
#define MCU_ADC1_AIN0			BIT(12)
#define MCU_ADC1_AIN1			BIT(13)

#define PSRAMECC0_RAM_BOOT_DEVICE 0x200000

DECLARE_GLOBAL_DATA_PTR;

/**********************************************
* Revision Detection
*
* DDR_TYPE_DET_1   DDR_TYPE_DET_0
* MCU_ADC1_AIN1    MCU_ADC1_AIN0
*     1                1           8GB LPDDR4
*     0                1           4GB LPDDR4
***********************************************/
enum {
	LPDDR4_8GB = 0x3,
	LPDDR4_4GB = 0x1,
};

static bool is_8g(void)
{
	u32 val;

	/* Configure MCU_ADC1 as GPI */
	if(!(readl(CTRLMMR_MCU_ADC1_CTRL) & GPI_MODE_EN))
		writel(val | GPI_MODE_EN, CTRLMMR_MCU_ADC1_CTRL);

	val = (readl(GPIO_IN_DATA45) & (MCU_ADC1_AIN0 | MCU_ADC1_AIN1)) >> 12;

	if(val == LPDDR4_8GB)
		return true;
	else
		return false;
}

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
#ifdef CONFIG_PHYS_64BIT
if (is_8g())
	gd->ram_size = 0x200000000;
else
	gd->ram_size = 0x100000000;
#else
	gd->ram_size = 0x80000000;
#endif

	return 0;
}

phys_size_t board_get_usable_ram_top(phys_size_t total_size)
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
	/* Bank 0 declares the memory available in the DDR low region */
	gd->bd->bi_dram[0].start = CFG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = 0x80000000;
	gd->ram_size = 0x80000000;

#ifdef CONFIG_PHYS_64BIT
	/* Bank 1 declares the memory available in the DDR high region */
	gd->bd->bi_dram[1].start = CFG_SYS_SDRAM_BASE1;
	if (is_8g()) {
		gd->bd->bi_dram[1].size = 0x180000000;
		gd->ram_size = 0x200000000;
	} else {
		gd->bd->bi_dram[1].size = 0x80000000;
		gd->ram_size = 0x100000000;
	}
#endif

	return 0;
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	if (!strcmp(name, "k3-j721e-rovy-evm") ||
	    !strcmp(name, "k3-j721e-r5-rovy-evm"))
		return 0;

	return -1;
}
#endif

#if defined(CONFIG_SPL_BUILD)
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
	if (is_8g()) {
		reginit_data->ctl_regs[268] = 0x00000000;
		reginit_data->ctl_regs[270] = 0x00001FFF;
		reginit_data->ctl_regs[271] = 0x3FFF2000;
		reginit_data->ctl_regs[272] = 0x03FF0000;
		reginit_data->ctl_regs[273] = 0x000103FF;
		reginit_data->pi_regs[73] = 0x00080000;
	}

	return 0;
}
#endif

#if CONFIG_IS_ENABLED(DM_GPIO) && CONFIG_IS_ENABLED(OF_LIBFDT)
/* Returns 1, if onboard mux is set to hyperflash */
static void __maybe_unused detect_enable_hyperflash(void *blob)
{

}
#endif

#if defined(CONFIG_SPL_BUILD) && (defined(CONFIG_TARGET_J7200_A72_EVM) || defined(CONFIG_TARGET_J7200_R5_EVM) || \
					defined(CONFIG_TARGET_ROVY_4VM_A72) || defined(CONFIG_TARGET_ROVY_4VM_R5))
void spl_perform_fixups(struct spl_image_info *spl_image)
{
	detect_enable_hyperflash(spl_image->fdt_addr);
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;

	ret = fdt_fixup_msmc_ram(blob, "/bus@100000", "sram@70000000");
	if (ret < 0)
		ret = fdt_fixup_msmc_ram(blob, "/interconnect@100000",
					 "sram@70000000");
	if (ret)
		printf("%s: fixing up msmc ram failed %d\n", __func__, ret);

	detect_enable_hyperflash(blob);

	return ret;
}
#endif

void configure_serdes_sierra(void)
{
	struct udevice *dev, *link_dev;
	struct phy link;
	int ret, count, i;
	int link_count = 0;

	if (!IS_ENABLED(CONFIG_PHY_CADENCE_SIERRA))
		return;

	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(sierra_phy_provider),
					  &dev);
	if (ret) {
		printf("Sierra init failed:%d\n", ret);
		return;
	}

	count = device_get_child_count(dev);
	for (i = 0; i < count; i++) {
		ret = device_get_child(dev, i, &link_dev);
		if (ret) {
			printf("probe of sierra child node %d failed: %d\n", i, ret);
			return;
		}
		if (link_dev->driver->id == UCLASS_PHY) {
			link.dev = link_dev;
			link.id = link_count++;

			ret = generic_phy_power_on(&link);
			if (ret) {
				printf("phy_power_on failed!!: %d\n", ret);
				return;
			}
		}
	}
}

#ifdef CONFIG_BOARD_LATE_INIT
static int tda4_boot_dev(void) {
    return readl(PSRAMECC0_RAM_BOOT_DEVICE);
}

void detect_boot_dev(void) {
	switch(tda4_boot_dev()) {
	case BOOT_DEVICE_SPI:
		printf("Boot Device: OSPI\n");
		env_set("boot", "ufs");
		break;
	case BOOT_DEVICE_MMC2:
		printf("Boot Device: SD\n");
		break;
	default:
		printf("Boot Device: Unknown\n");
		break;
	}
}

int board_late_init(void)
{
	configure_serdes_sierra();
	detect_boot_dev();

	return 0;
}
#if 0
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio)
		return ENVL_UNKNOWN;

	switch (tda4_boot_dev()) {
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
	case BOOT_DEVICE_SPI:
		return ENVL_SPI_FLASH;
#endif
#ifdef CONFIG_ENV_IS_IN_FAT
	case BOOT_DEVICE_MMC2:
		return  ENVL_FAT;
#endif
	default:
		return ENVL_UNKNOWN;
	}
}
#endif
#endif

void spl_board_init(void)
{
	/* Store boot_device for U-Boot */
	writel(spl_boot_device(), PSRAMECC0_RAM_BOOT_DEVICE);
}

