/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration header file for K3 J721E ROVY-4VM
 *
 * Copyright (C) 2024 Technexion Ltd.
 * Author: Richard Hu <richard.hu@technexion.com>
 *
 */

#ifndef __CONFIG_ROVY_4VM_H
#define __CONFIG_ROVY_4VM_H

#include <linux/sizes.h>

/* DDR Configuration */
#define CFG_SYS_SDRAM_BASE1		0x880000000
/* FLASH Configuration */
#define CFG_SYS_FLASH_BASE		0x000000000

/* SPL Loader Configuration */
#if defined(CONFIG_TARGET_ROVY_4VM_A72)
#define CFG_SYS_UBOOT_BASE		0x50280000
#elif defined(CONFIG_TARGET_ROVY_4VM_R5)
#define CFG_SYS_UBOOT_BASE		0x50080000
#else
#define CFG_SYS_UBOOT_BASE		0x50100000
#endif

/* Now for the remaining common defines */
#include <configs/ti_armv7_common.h>


#endif /* __CONFIG_ROVY_4VM_H */
