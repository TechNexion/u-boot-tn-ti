/*
 * Copyright (C) 2024 Technexion Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <i2c_eeprom.h>
#include <net.h>
#include <stdlib.h>
#include <u-boot/crc.h>
#include "tn_eeprom.h"

int tn_setup_mac_address(void)
{
	const char *path = "eeprom0";
	struct tn_eeprom *eeprom;
	struct udevice *dev;
	u32 crc32_cal = 0;
	int i, j, ret, off;

	off = fdt_path_offset(gd->fdt_blob, path);
	if (off < 0) {
		printf("No eeprom0 path offset found in DT\n");
		return off;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_I2C_EEPROM, off, &dev);
	if (ret) {
		printf("%s: Could not find EEPROM\n", __func__);
		return ret;
	}
	debug("Found EEPROM\n");

	eeprom = malloc(sizeof(struct tn_eeprom));
	if (!eeprom)
		return -ENOMEM;

	ret = i2c_eeprom_read(dev, 0, (uint8_t *)eeprom,
			      sizeof(struct tn_eeprom));
	if (ret) {
		printf("%s: i2c_eeprom_read() failed: %d\n", __func__, ret);
		free(eeprom);
		return ret;
	}
	debug("eeprom->magic is 0x%X \n", eeprom->magic);

	if (eeprom->magic != TN_EEPROM_MAGIC) {
		printf("Invalid eeprom magic: 0x%08x, expected 0x%08x\n",
		       eeprom->magic, TN_EEPROM_MAGIC);
		free(eeprom);
		return -1;
	}
	debug("EEPROM magic is valid\r\n");

	if (eeprom->end_signature != TN_EEPROM_END_SIGNATURE) {
		printf("Invalid eeprom end_signature: 0x%08x, expected 0x%08x\n",
		       eeprom->end_signature, TN_EEPROM_END_SIGNATURE);
		free(eeprom);
		return -1;
	}
	debug("EEPROM end_signature is valid\r\n");

	crc32_cal = crc32(0, (unsigned char *)eeprom, sizeof(struct tn_eeprom)-sizeof(eeprom->crc32));
	if (eeprom->crc32 != crc32_cal) {
		printf("Invalid eeprom crc32: 0x%08x, expected 0x%08x\n",
		       crc32_cal, eeprom->crc32);
		free(eeprom);
		return -1;
	}
	debug("EEPROM crc32 is valid\r\n");

	/* Load MAC from the EEPROM content */
	if (eeprom->mac_info.mac_info_type != TN_EEPROM_MAC_INFO_TYPE) {
		printf("EEPROM MAC_INFO_TYPE: 0x%08x, expected 0x%08x\n",
		       eeprom->mac_info.mac_info_type, TN_EEPROM_MAC_INFO_TYPE);
		free(eeprom);
		return 0;
	}

	if (eeprom->mac_info.mac_count > TN_EEPROM_MAC_COUNT_MAX) {
		printf("MAC_NUM %d exceeds the maximum size %d\n", eeprom->mac_info.mac_count, TN_EEPROM_MAC_COUNT_MAX);
	}

	for (i = 0; i < eeprom->mac_info.mac_count; i++) {
		if (!is_valid_ethaddr((u8 *)eeprom->mac_info.mac_addr[i]))
			continue;
		eth_env_set_enetaddr_by_index("eth", i, (uchar *)eeprom->mac_info.mac_addr[i]);

		debug("MAC %d:", i);
		for (j = 0; j < 6; j++)
		{
			debug("%02x ", eeprom->mac_info.mac_addr[i][j]);
		}
		debug("\n");
	}

	return 0;
}
