// SPDX-License-Identifier: GPL-2.0+
/*
 * J721E: SoC specific initialization
 *
 * Copyright (C) 2018-2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Lokesh Vutla <lokeshvutla@ti.com>
 */

#include <common.h>
#include <init.h>
#include <spl.h>
#include <asm/io.h>
#include <asm/armv7_mpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sysfw-loader.h>
#include "common.h"
#include <asm/arch/sys_proto.h>
#include <linux/soc/ti/ti_sci_protocol.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/pinctrl.h>
#include <dm/root.h>
#include <fdtdec.h>
#include <mmc.h>
#include <remoteproc.h>

#ifdef CONFIG_K3_LOAD_SYSFW
struct fwl_data cbass_hc_cfg0_fwls[] = {
#if defined(CONFIG_TARGET_J721E_R5_EVM)
	{ "PCIE0_CFG", 2560, 8 },
	{ "PCIE1_CFG", 2561, 8 },
	{ "USB3SS0_CORE", 2568, 4 },
	{ "USB3SS1_CORE", 2570, 4 },
	{ "EMMC8SS0_CFG", 2576, 4 },
	{ "UFS_HCI0_CFG", 2580, 4 },
	{ "SERDES0", 2584, 1 },
	{ "SERDES1", 2585, 1 },
#elif defined(CONFIG_TARGET_J7200_R5_EVM)
	{ "PCIE1_CFG", 2561, 7 },
#endif
}, cbass_hc0_fwls[] = {
#if defined(CONFIG_TARGET_J721E_R5_EVM)
	{ "PCIE0_HP", 2528, 24 },
	{ "PCIE0_LP", 2529, 24 },
	{ "PCIE1_HP", 2530, 24 },
	{ "PCIE1_LP", 2531, 24 },
#endif
}, cbass_rc_cfg0_fwls[] = {
	{ "EMMCSD4SS0_CFG", 2380, 4 },
}, cbass_rc0_fwls[] = {
	{ "GPMC0", 2310, 8 },
}, infra_cbass0_fwls[] = {
	{ "PLL_MMR0", 8, 26 },
	{ "CTRL_MMR0", 9, 16 },
}, mcu_cbass0_fwls[] = {
	{ "MCU_R5FSS0_CORE0", 1024, 4 },
	{ "MCU_R5FSS0_CORE0_CFG", 1025, 2 },
	{ "MCU_R5FSS0_CORE1", 1028, 4 },
	{ "MCU_FSS0_CFG", 1032, 12 },
	{ "MCU_FSS0_S1", 1033, 8 },
	{ "MCU_FSS0_S0", 1036, 8 },
	{ "MCU_PSROM49152X32", 1048, 1 },
	{ "MCU_MSRAM128KX64", 1050, 8 },
	{ "MCU_CTRL_MMR0", 1200, 8 },
	{ "MCU_PLL_MMR0", 1201, 3 },
	{ "MCU_CPSW0", 1220, 2 },
}, wkup_cbass0_fwls[] = {
	{ "WKUP_CTRL_MMR0", 131, 16 },
};
#endif

static void ctrl_mmr_unlock(void)
{
	/* Unlock all WKUP_CTRL_MMR0 module registers */
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 0);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 1);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 2);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 3);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 4);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 6);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 7);

	/* Unlock all MCU_CTRL_MMR0 module registers */
	mmr_unlock(MCU_CTRL_MMR0_BASE, 0);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 1);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 2);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 3);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 4);

	/* Unlock all CTRL_MMR0 module registers */
	mmr_unlock(CTRL_MMR0_BASE, 0);
	mmr_unlock(CTRL_MMR0_BASE, 1);
	mmr_unlock(CTRL_MMR0_BASE, 2);
	mmr_unlock(CTRL_MMR0_BASE, 3);
	mmr_unlock(CTRL_MMR0_BASE, 5);
	if (soc_is_j721e())
		mmr_unlock(CTRL_MMR0_BASE, 6);
	mmr_unlock(CTRL_MMR0_BASE, 7);
}

#if defined(CONFIG_K3_LOAD_SYSFW)
void k3_mmc_stop_clock(void)
{
	if (spl_boot_device() == BOOT_DEVICE_MMC1) {
		struct mmc *mmc = find_mmc_device(0);

		if (!mmc)
			return;

		mmc->saved_clock = mmc->clock;
		mmc_set_clock(mmc, 0, true);
	}
}

void k3_mmc_restart_clock(void)
{
	if (spl_boot_device() == BOOT_DEVICE_MMC1) {
		struct mmc *mmc = find_mmc_device(0);

		if (!mmc)
			return;

		mmc_set_clock(mmc, mmc->saved_clock, false);
	}
}
#endif

/*
 * This uninitialized global variable would normal end up in the .bss section,
 * but the .bss is cleared between writing and reading this variable, so move
 * it to the .data section.
 */
u32 bootindex __section(".data");
static struct rom_extended_boot_data bootdata __section(".data");

static void store_boot_info_from_rom(void)
{
	bootindex = *(u32 *)(CONFIG_SYS_K3_BOOT_PARAM_TABLE_INDEX);
	memcpy(&bootdata, (uintptr_t *)ROM_EXTENDED_BOOT_DATA_INFO,
	       sizeof(struct rom_extended_boot_data));
}

#ifdef CONFIG_SPL_OF_LIST
void do_dt_magic(void)
{
	int ret, rescan, mmc_dev = -1;
	static struct mmc *mmc;

	if (IS_ENABLED(CONFIG_TI_I2C_BOARD_DETECT))
		do_board_detect();

	/*
	 * Board detection has been done.
	 * Let us see if another dtb wouldn't be a better match
	 * for our board
	 */
	if (IS_ENABLED(CONFIG_CPU_V7R)) {
		ret = fdtdec_resetup(&rescan);
		if (!ret && rescan) {
			dm_uninit();
			dm_init_and_scan(true);
		}
	}

	/*
	 * Because of multi DTB configuration, the MMC device has
	 * to be re-initialized after reconfiguring FDT inorder to
	 * boot from MMC. Do this when boot mode is MMC and ROM has
	 * not loaded SYSFW.
	 */
	switch (spl_boot_device()) {
	case BOOT_DEVICE_MMC1:
		mmc_dev = 0;
		break;
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2:
		mmc_dev = 1;
		break;
	}

	if (mmc_dev > 0 && !is_rom_loaded_sysfw(&bootdata)) {
		ret = mmc_init_device(mmc_dev);
		if (!ret) {
			mmc = find_mmc_device(mmc_dev);
			if (mmc) {
				ret = mmc_init(mmc);
				if (ret) {
					printf("mmc init failed with error: %d\n", ret);
				}
			}
		}
	}
}
#endif

void setup_navss_nb(void)
{
        /* Map orderid 8-15 to VBUSM.C thread 2 (real-time traffic) */
        writel(2, NAVSS0_NBSS_NB1_CFG_NB_THREADMAP);
}

void setup_vpac_qos(void)
{
	unsigned int channel, group;

	/* vpac data master 0  */
	for (channel = 0; channel < QOS_VPAC0_DATA0_NUM_I_CH; ++channel) {

		writel((QOS_VPAC0_DATA0_ATYPE << 28), (uintptr_t)QOS_VPAC0_DATA0_CBASS_MAP(channel));
	}

	/* vpac data master 1  */
	for (channel = 0; channel < QOS_VPAC0_DATA1_NUM_I_CH; ++channel) {

		writel((QOS_VPAC0_DATA1_ATYPE << 28), (uintptr_t)QOS_VPAC0_DATA1_CBASS_MAP(channel));
	}

	/* vpac ldc0  */
	for (group = 0; group < QOS_VPAC0_LDC0_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_VPAC0_LDC0_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_VPAC0_LDC0_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_VPAC0_LDC0_NUM_I_CH; ++channel) {

		writel((QOS_VPAC0_LDC0_ATYPE << 28) | (QOS_VPAC0_LDC0_PRIORITY << 12) | (QOS_VPAC0_LDC0_ORDER_ID << 4), (uintptr_t)QOS_VPAC0_LDC0_CBASS_MAP(channel));
	}

}

void setup_dmpac_qos(void)
{
	unsigned int channel;

	/* dmpac data  */
	for (channel = 0; channel < QOS_DMPAC0_DATA_NUM_I_CH; ++channel) {

		writel((QOS_DMPAC0_DATA_ATYPE << 28), (uintptr_t)QOS_DMPAC0_DATA_CBASS_MAP(channel));
	}
}

void setup_dss_qos(void)
{
	unsigned int channel, group;

	/* two master ports: dma and fbdc */
	/* two groups: SRAM and DDR */
	/* 10 channels: (pipe << 1) | is_second_buffer */

	/* master port 1 (dma) */
	for (group = 0; group < QOS_DSS0_DMA_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_DSS0_DMA_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_DSS0_DMA_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_DSS0_DMA_NUM_I_CH; ++channel) {

		writel((QOS_DSS0_DMA_ATYPE << 28) | (QOS_DSS0_DMA_PRIORITY << 12) | (QOS_DSS0_DMA_ORDER_ID << 4), (uintptr_t)QOS_DSS0_DMA_CBASS_MAP(channel));
	}

	/* master port 2 (fbdc) */
	for (group = 0; group < QOS_DSS0_FBDC_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_DSS0_FBDC_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_DSS0_FBDC_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_DSS0_FBDC_NUM_I_CH; ++channel) {

		writel((QOS_DSS0_FBDC_ATYPE << 28) | (QOS_DSS0_FBDC_PRIORITY << 12) | (QOS_DSS0_FBDC_ORDER_ID << 4), (uintptr_t)QOS_DSS0_FBDC_CBASS_MAP(channel));
	}
}

void setup_gpu_qos(void)
{
	unsigned int channel, group;

	/* gpu m0 rd */
	for (group = 0; group < QOS_GPU0_M0_RD_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_GPU0_M0_RD_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_GPU0_M0_RD_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_GPU0_M0_RD_NUM_I_CH; ++channel) {

		if(channel == 0)
		{
			writel((QOS_GPU0_M0_RD_ATYPE << 28) | (QOS_GPU0_M0_RD_MMU_PRIORITY << 12) | (QOS_GPU0_M0_RD_ORDER_ID << 4), (uintptr_t)QOS_GPU0_M0_RD_CBASS_MAP(channel));
		}
		else
		{
			writel((QOS_GPU0_M0_RD_ATYPE << 28) | (QOS_GPU0_M0_RD_PRIORITY << 12) | (QOS_GPU0_M0_RD_ORDER_ID << 4), (uintptr_t)QOS_GPU0_M0_RD_CBASS_MAP(channel));
		}
	}

	/* gpu m0 wr */
	for (group = 0; group < QOS_GPU0_M0_WR_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_GPU0_M0_WR_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_GPU0_M0_WR_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_GPU0_M0_WR_NUM_I_CH; ++channel) {

		writel((QOS_GPU0_M0_WR_ATYPE << 28) | (QOS_GPU0_M0_WR_PRIORITY << 12) | (QOS_GPU0_M0_WR_ORDER_ID << 4), (uintptr_t)QOS_GPU0_M0_WR_CBASS_MAP(channel));
	}

	/* gpu m1 rd */
	for (group = 0; group < QOS_GPU0_M1_RD_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_GPU0_M1_RD_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_GPU0_M1_RD_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_GPU0_M1_RD_NUM_I_CH; ++channel) {

		if(channel == 0)
		{
			writel((QOS_GPU0_M1_RD_ATYPE << 28) | (QOS_GPU0_M1_RD_MMU_PRIORITY << 12) | (QOS_GPU0_M1_RD_ORDER_ID << 4), (uintptr_t)QOS_GPU0_M1_RD_CBASS_MAP(channel));
		}
		else
		{
			writel((QOS_GPU0_M1_RD_ATYPE << 28) | (QOS_GPU0_M1_RD_PRIORITY << 12) | (QOS_GPU0_M1_RD_ORDER_ID << 4), (uintptr_t)QOS_GPU0_M1_RD_CBASS_MAP(channel));
		}
	}

	/* gpu m1 wr */
	for (group = 0; group < QOS_GPU0_M1_WR_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_GPU0_M1_WR_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_GPU0_M1_WR_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_GPU0_M1_WR_NUM_I_CH; ++channel) {

		writel((QOS_GPU0_M1_WR_ATYPE << 28) | (QOS_GPU0_M1_WR_PRIORITY << 12) | (QOS_GPU0_M1_WR_ORDER_ID << 4), (uintptr_t)QOS_GPU0_M1_WR_CBASS_MAP(channel));
	}
}

void setup_encoder_qos(void)
{
	unsigned int channel, group;

	/* encoder rd */
	for (group = 0; group < QOS_ENCODER0_RD_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_ENCODER0_RD_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_ENCODER0_RD_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_ENCODER0_RD_NUM_I_CH; ++channel) {

		writel((QOS_ENCODER0_RD_ATYPE << 28) | (QOS_ENCODER0_RD_PRIORITY << 12) | (QOS_ENCODER0_RD_ORDER_ID << 4), (uintptr_t)QOS_ENCODER0_RD_CBASS_MAP(channel));
	}

	/* encoder wr */
	for (group = 0; group < QOS_ENCODER0_WR_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_ENCODER0_WR_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_ENCODER0_WR_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_ENCODER0_WR_NUM_I_CH; ++channel) {

		writel((QOS_ENCODER0_WR_ATYPE << 28) | (QOS_ENCODER0_WR_PRIORITY << 12) | (QOS_ENCODER0_WR_ORDER_ID << 4), (uintptr_t)QOS_ENCODER0_WR_CBASS_MAP(channel));
	}
}

void setup_decoder_qos(void)
{
	unsigned int channel, group;

	/* decoder rd */
	for (group = 0; group < QOS_DECODER0_RD_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_DECODER0_RD_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_DECODER0_RD_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_DECODER0_RD_NUM_I_CH; ++channel) {

		writel((QOS_DECODER0_RD_ATYPE << 28) | (QOS_DECODER0_RD_PRIORITY << 12) | (QOS_DECODER0_RD_ORDER_ID << 4), (uintptr_t)QOS_DECODER0_RD_CBASS_MAP(channel));
	}

	/* decoder wr */
	for (group = 0; group < QOS_DECODER0_WR_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_DECODER0_WR_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_DECODER0_WR_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_DECODER0_WR_NUM_I_CH; ++channel) {

		writel((QOS_DECODER0_WR_ATYPE << 28) | (QOS_DECODER0_WR_PRIORITY << 12) | (QOS_DECODER0_WR_ORDER_ID << 4), (uintptr_t)QOS_DECODER0_WR_CBASS_MAP(channel));
	}
}

void setup_c66_qos(void)
{
	unsigned int channel, group;

	/* c66_0 mdma */
	for (group = 0; group < QOS_C66SS0_MDMA_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_C66SS0_MDMA_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_C66SS0_MDMA_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_C66SS0_MDMA_NUM_I_CH; ++channel) {

		writel((QOS_C66SS0_MDMA_ATYPE << 28) | (QOS_C66SS0_MDMA_PRIORITY << 12) | (QOS_C66SS0_MDMA_ORDER_ID << 4), (uintptr_t)QOS_C66SS0_MDMA_CBASS_MAP(channel));
	}

	/* c66_1 mdma */
	for (group = 0; group < QOS_C66SS1_MDMA_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_C66SS1_MDMA_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_C66SS1_MDMA_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_C66SS1_MDMA_NUM_I_CH; ++channel) {

		writel((QOS_C66SS1_MDMA_ATYPE << 28) | (QOS_C66SS1_MDMA_PRIORITY << 12) | (QOS_C66SS1_MDMA_ORDER_ID << 4), (uintptr_t)QOS_C66SS1_MDMA_CBASS_MAP(channel));
	}
}

void setup_main_r5f_qos(void)
{
	unsigned int channel, group;

	/* R5FSS0 core0 - read */
	for (group = 0; group < QOS_R5FSS0_CORE0_MEM_RD_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_R5FSS0_CORE0_MEM_RD_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_R5FSS0_CORE0_MEM_RD_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_R5FSS0_CORE0_MEM_RD_NUM_I_CH; ++channel) {

		writel((QOS_R5FSS0_CORE0_MEM_RD_ATYPE << 28) | (QOS_R5FSS0_CORE0_MEM_RD_PRIORITY << 12) | (QOS_R5FSS0_CORE0_MEM_RD_ORDER_ID << 4), (uintptr_t)QOS_R5FSS0_CORE0_MEM_RD_CBASS_MAP(channel));
	}

	/* R5FSS0 core0 - write */
	for (group = 0; group < QOS_R5FSS0_CORE0_MEM_WR_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_R5FSS0_CORE0_MEM_WR_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_R5FSS0_CORE0_MEM_WR_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_R5FSS0_CORE0_MEM_WR_NUM_I_CH; ++channel) {

		writel((QOS_R5FSS0_CORE0_MEM_WR_ATYPE << 28) | (QOS_R5FSS0_CORE0_MEM_WR_PRIORITY << 12) | (QOS_R5FSS0_CORE0_MEM_RD_ORDER_ID << 4), (uintptr_t)QOS_R5FSS0_CORE0_MEM_WR_CBASS_MAP(channel));
	}

	/* R5FSS0 core1 - read */
	for (group = 0; group < QOS_R5FSS0_CORE1_MEM_RD_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_R5FSS0_CORE1_MEM_RD_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_R5FSS0_CORE1_MEM_RD_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_R5FSS0_CORE1_MEM_RD_NUM_I_CH; ++channel) {

		writel((QOS_R5FSS0_CORE1_MEM_RD_ATYPE << 28) | (QOS_R5FSS0_CORE1_MEM_RD_PRIORITY << 12) | (QOS_R5FSS0_CORE0_MEM_RD_ORDER_ID << 4), (uintptr_t)QOS_R5FSS0_CORE1_MEM_RD_CBASS_MAP(channel));
	}

	/* R5FSS0 core1 - write */
	for (group = 0; group < QOS_R5FSS0_CORE1_MEM_WR_NUM_J_CH; ++group) {
		writel(0x76543210, (uintptr_t)QOS_R5FSS0_CORE1_MEM_WR_CBASS_GRP_MAP1(group));
		writel(0xfedcba98, (uintptr_t)QOS_R5FSS0_CORE1_MEM_WR_CBASS_GRP_MAP2(group));
	}

	for (channel = 0; channel < QOS_R5FSS0_CORE1_MEM_WR_NUM_I_CH; ++channel) {

		writel((QOS_R5FSS0_CORE1_MEM_WR_ATYPE << 28) | (QOS_R5FSS0_CORE1_MEM_WR_PRIORITY << 12) | (QOS_R5FSS0_CORE1_MEM_RD_ORDER_ID << 4), (uintptr_t)QOS_R5FSS0_CORE1_MEM_WR_CBASS_MAP(channel));
	}

}

void board_init_f(ulong dummy)
{
#if defined(CONFIG_K3_J721E_DDRSS) || defined(CONFIG_K3_LOAD_SYSFW)
	struct udevice *dev;
	int ret;
#endif
	/*
	 * Cannot delay this further as there is a chance that
	 * K3_BOOT_PARAM_TABLE_INDEX can be over written by SPL MALLOC section.
	 */
	store_boot_info_from_rom();

	/* Make all control module registers accessible */
	ctrl_mmr_unlock();

#ifdef CONFIG_CPU_V7R
	disable_linefill_optimization();
	setup_k3_mpu_regions();
#endif

	/* Init DM early */
	spl_early_init();

#ifdef CONFIG_K3_LOAD_SYSFW
	/*
	 * Process pinctrl for the serial0 a.k.a. MCU_UART0 module and continue
	 * regardless of the result of pinctrl. Do this without probing the
	 * device, but instead by searching the device that would request the
	 * given sequence number if probed. The UART will be used by the system
	 * firmware (SYSFW) image for various purposes and SYSFW depends on us
	 * to initialize its pin settings.
	 */
	ret = uclass_find_device_by_seq(UCLASS_SERIAL, 0, &dev);
	if (!ret)
		pinctrl_select_state(dev, "default");

	/*
	 * Load, start up, and configure system controller firmware. Provide
	 * the U-Boot console init function to the SYSFW post-PM configuration
	 * callback hook, effectively switching on (or over) the console
	 * output.
	 */
	k3_sysfw_loader(is_rom_loaded_sysfw(&bootdata),
			k3_mmc_stop_clock, k3_mmc_restart_clock);

#ifdef CONFIG_SPL_OF_LIST
	do_dt_magic();
#endif

	/*
	 * Force probe of clk_k3 driver here to ensure basic default clock
	 * configuration is always done.
	 */
	if (IS_ENABLED(CONFIG_SPL_CLK_K3)) {
		ret = uclass_get_device_by_driver(UCLASS_CLK,
						  DM_DRIVER_GET(ti_clk),
						  &dev);
		if (ret)
			panic("Failed to initialize clk-k3!\n");
	}

	/* Prepare console output */
	preloader_console_init();

	/* Disable ROM configured firewalls right after loading sysfw */
	remove_fwl_configs(cbass_hc_cfg0_fwls, ARRAY_SIZE(cbass_hc_cfg0_fwls));
	remove_fwl_configs(cbass_hc0_fwls, ARRAY_SIZE(cbass_hc0_fwls));
	remove_fwl_configs(cbass_rc_cfg0_fwls, ARRAY_SIZE(cbass_rc_cfg0_fwls));
	remove_fwl_configs(cbass_rc0_fwls, ARRAY_SIZE(cbass_rc0_fwls));
	remove_fwl_configs(infra_cbass0_fwls, ARRAY_SIZE(infra_cbass0_fwls));
	remove_fwl_configs(mcu_cbass0_fwls, ARRAY_SIZE(mcu_cbass0_fwls));
	remove_fwl_configs(wkup_cbass0_fwls, ARRAY_SIZE(wkup_cbass0_fwls));
#else
	/* Prepare console output */
	preloader_console_init();
#endif

	/* Output System Firmware version info */
	k3_sysfw_print_ver();

	/* Perform EEPROM-based board detection */
	if (IS_ENABLED(CONFIG_TI_I2C_BOARD_DETECT))
		do_board_detect();

#if defined(CONFIG_CPU_V7R) && defined(CONFIG_K3_AVS0)
	ret = uclass_get_device_by_driver(UCLASS_MISC, DM_DRIVER_GET(k3_avs),
					  &dev);
	if (ret)
		printf("AVS init failed: %d\n", ret);
#endif

#if defined(CONFIG_K3_J721E_DDRSS)
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("DRAM init failed: %d\n", ret);
#endif

	if (soc_is_j721e()) {
		setup_navss_nb();
		setup_c66_qos();
		setup_main_r5f_qos();
		setup_vpac_qos();
		setup_dmpac_qos();
		setup_dss_qos();
		setup_gpu_qos();
		setup_encoder_qos();
	}

	spl_enable_dcache();
}

u32 spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device)
{
	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		return MMCSD_MODE_EMMCBOOT;
	case BOOT_DEVICE_MMC2:
		return MMCSD_MODE_FS;
	default:
		return MMCSD_MODE_RAW;
	}
}

static u32 __get_backup_bootmedia(u32 main_devstat)
{
	u32 bkup_boot = (main_devstat & MAIN_DEVSTAT_BKUP_BOOTMODE_MASK) >>
			MAIN_DEVSTAT_BKUP_BOOTMODE_SHIFT;

	switch (bkup_boot) {
	case BACKUP_BOOT_DEVICE_USB:
		return BOOT_DEVICE_DFU;
	case BACKUP_BOOT_DEVICE_UART:
		return BOOT_DEVICE_UART;
	case BACKUP_BOOT_DEVICE_ETHERNET:
		return BOOT_DEVICE_ETHERNET;
	case BACKUP_BOOT_DEVICE_MMC2:
	{
		u32 port = (main_devstat & MAIN_DEVSTAT_BKUP_MMC_PORT_MASK) >>
			    MAIN_DEVSTAT_BKUP_MMC_PORT_SHIFT;
		if (port == 0x0)
			return BOOT_DEVICE_MMC1;
		return BOOT_DEVICE_MMC2;
	}
	case BACKUP_BOOT_DEVICE_SPI:
		return BOOT_DEVICE_SPI;
	case BACKUP_BOOT_DEVICE_I2C:
		return BOOT_DEVICE_I2C;
	}

	return BOOT_DEVICE_RAM;
}

static u32 __get_primary_bootmedia(u32 main_devstat, u32 wkup_devstat)
{

	u32 bootmode = (wkup_devstat & WKUP_DEVSTAT_PRIMARY_BOOTMODE_MASK) >>
			WKUP_DEVSTAT_PRIMARY_BOOTMODE_SHIFT;

	bootmode |= (main_devstat & MAIN_DEVSTAT_BOOT_MODE_B_MASK) <<
			BOOT_MODE_B_SHIFT;

	if (bootmode == BOOT_DEVICE_OSPI || bootmode ==	BOOT_DEVICE_QSPI ||
	    bootmode == BOOT_DEVICE_XSPI)
		bootmode = BOOT_DEVICE_SPI;

	if (bootmode == BOOT_DEVICE_MMC2) {
		u32 port = (main_devstat &
			    MAIN_DEVSTAT_PRIM_BOOTMODE_MMC_PORT_MASK) >>
			   MAIN_DEVSTAT_PRIM_BOOTMODE_PORT_SHIFT;
		if (port == 0x0)
			bootmode = BOOT_DEVICE_MMC1;
	}

	return bootmode;
}

u32 spl_spi_boot_bus(void)
{
	u32 wkup_devstat = readl(CTRLMMR_WKUP_DEVSTAT);
	u32 main_devstat = readl(CTRLMMR_MAIN_DEVSTAT);
	u32 bootmode = ((wkup_devstat & WKUP_DEVSTAT_PRIMARY_BOOTMODE_MASK) >>
			WKUP_DEVSTAT_PRIMARY_BOOTMODE_SHIFT) |
			((main_devstat & MAIN_DEVSTAT_BOOT_MODE_B_MASK) << BOOT_MODE_B_SHIFT);

	return (bootmode == BOOT_DEVICE_QSPI) ? 1 : 0;
}

u32 spl_boot_device(void)
{
	u32 wkup_devstat = readl(CTRLMMR_WKUP_DEVSTAT);
	u32 main_devstat;

	if (wkup_devstat & WKUP_DEVSTAT_MCU_OMLY_MASK) {
		printf("ERROR: MCU only boot is not yet supported\n");
		return BOOT_DEVICE_RAM;
	}

	/* MAIN CTRL MMR can only be read if MCU ONLY is 0 */
	main_devstat = readl(CTRLMMR_MAIN_DEVSTAT);

	if (bootindex == K3_PRIMARY_BOOTMODE)
		return __get_primary_bootmedia(main_devstat, wkup_devstat);
	else
		return __get_backup_bootmedia(main_devstat);
}

#ifdef CONFIG_SYS_K3_SPL_ATF

#define J721E_DEV_MCU_RTI0			262
#define J721E_DEV_MCU_RTI1			263
#define J721E_DEV_MCU_ARMSS0_CPU0		250
#define J721E_DEV_MCU_ARMSS0_CPU1		251

void release_resources_for_core_shutdown(void)
{
	struct ti_sci_handle *ti_sci;
	struct ti_sci_dev_ops *dev_ops;
	struct ti_sci_proc_ops *proc_ops;
	int ret;
	u32 i;

	const u32 put_device_ids[] = {
		J721E_DEV_MCU_RTI0,
		J721E_DEV_MCU_RTI1,
	};

	ti_sci = get_ti_sci_handle();
	dev_ops = &ti_sci->ops.dev_ops;
	proc_ops = &ti_sci->ops.proc_ops;

	/* Iterate through list of devices to put (shutdown) */
	for (i = 0; i < ARRAY_SIZE(put_device_ids); i++) {
		u32 id = put_device_ids[i];

		ret = dev_ops->put_device(ti_sci, id);
		if (ret)
			panic("Failed to put device %u (%d)\n", id, ret);
	}

	const u32 put_core_ids[] = {
		J721E_DEV_MCU_ARMSS0_CPU1,
		J721E_DEV_MCU_ARMSS0_CPU0,	/* Handle CPU0 after CPU1 */
	};

	/* Iterate through list of cores to put (shutdown) */
	for (i = 0; i < ARRAY_SIZE(put_core_ids); i++) {
		u32 id = put_core_ids[i];

		/*
		 * Queue up the core shutdown request. Note that this call
		 * needs to be followed up by an actual invocation of an WFE
		 * or WFI CPU instruction.
		 */
		ret = proc_ops->proc_shutdown_no_wait(ti_sci, id);
		if (ret)
			panic("Failed sending core %u shutdown message (%d)\n",
			      id, ret);
	}
}
#endif
