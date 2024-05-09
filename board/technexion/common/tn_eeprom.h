/*
 * Copyright (C) 2024 Technexion Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _TN_EEPROM_H_
#define _TN_EEPROM_H_

#define TN_EEPROM_MAGIC	0x544E4244 /* TNBD */
#define TN_EEPROM_END_SIGNATURE	0x45594954 /* EXIT */

#define TN_EEPROM_BRD_TYPE	0x10
#define TN_EEPROM_MAC_INFO_TYPE	0x12
#define TN_EEPROM_MAC_COUNT_MAX	9


#define TN_EEPROM_VERSION	0x12

struct tn_board_id /* 128 bytes */
{
	u8 brd_type;               /* 0x6, 1 byte, Type (0x10 = BOARDID) */
	u16 brd_length;            /* 0x7, 2 bytes, Board ID Length including header (bytes) */
	u8 brd_version;            /* 0x9, 1 byte, Board ID Version */
	u8 product_name[16];       /* 0xA, 16 bytes, Name of the board */
	u8 vendor_name[20];        /* 0x1A, 20 bytes, Vendor Name */
	u8 serial_nbr[16];         /* 0x2E, 16 bytes, Serial Number */
	u8 padding[72];            /* 0x3E, 72 bytes, Padding to 128 bytes */
} __attribute__((packed));

struct tn_mac_info /* 72 bytes */
{
	u8 mac_info_type;          /* 0x86, 1 byte, Payload type (0x12 = MAC info) */
	u16 mac_info_length;       /* 0x87, 2 bytes, Payload Length including header (bytes) */
	u8 mac_info_version;       /* 0x89, 1 byte, MAC Address Info Version */
	u8 mac_count;         /* 0x8A, 1 byte, Count of the number of mac addresses */
	u8 mac_addr[TN_EEPROM_MAC_COUNT_MAX][6];
	u8 padding2[13];           /* 0xC1, 13 bytes, Padding to 72 bytes */
} __attribute__((packed));

struct tn_eeprom
{
	u32 magic;                 /* 0x0, 4 bytes, "TNBD" (TechNexion Board Description) */
	u16 payload_length;        /* 0x4, 2 bytes, Length of payload starting at MAGIC number through checksum */
	struct tn_board_id board_id; /* 128 bytes */
	struct tn_mac_info mac_info; /* 72 bytes */
	u32 end_signature;         /* 0xCE, 4 bytes, "EXIT" (End signature) */
	u32 crc32;                 /* 0xD2, 4 bytes, 32-bit checksum (crc32) of content from magic number through end signature */
} __attribute__((packed));

int tn_setup_mac_address(void);

#endif /* _TN_EEPROM_H_ */
