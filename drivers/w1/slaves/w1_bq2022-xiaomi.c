/*
 * drivers/w1/slaves/w1_bq2022.c
 *
 * Copyright (C) 2014 Texas Instruments, Inc.
 * Copyright (C) 2015 Balázs Triszka <balika011@protonmail.ch>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include "../w1.h"
#include "../w1_int.h"
#include "../w1_family.h"
#include <linux/delay.h>

// ROM Commands in Command Level 1
#define BQ2022_COMMAND_READ_SERIALIZATION_ROM_AND_CRC	0x33
#define BQ2022_COMMAND_MATCH_SERIALIZATION_ROM		0x55
#define BQ2022_COMMAND_SEARCH_SERIALIZATION_ROM		0xF0
#define BQ2022_COMMAND_SKIP_SERIALIZATION_ROM		0xCC

// Memory Function Commands in Command Level 2
#define BQ2022_COMMAND_READ_MEMORY_FIELD_CRC		0xF0
#define BQ2022_COMMAND_READ_EPROM_STATUS		0xAA
#define BQ2022_COMMAND_READ_MEMORY_PAGE_CRC		0xC3
#define BQ2022_COMMAND_WRITE_MEMORY			0x0F
#define BQ2022_COMMAND_PRGRAMMING_PROFILE		0x99
#define BQ2022_COMMAND_WRITE_EPROM_STATUS		0x55

// Program Commands only in write memory or write status mode
#define BQ2022_PROGRAM_CONTROL				0x5A

#define BQ2022_ID_SAMSUNG_XWD				0x10139461
#define BQ2022_ID_GUANGYU				0x10139462
#define BQ2022_ID_SONY_XWD				0x10139463
#define BQ2022_ID_SAMSUNG_XWD_COSTDOWN			0x10139464
#define BQ2022_ID_LG_DESA				0x10139465
#define BQ2022_ID_SONY_FMT				0x10139466
#define BQ2022_ID_RUISHENG				0x10139467
#define BQ2022_ID_DELSA					0x8412E562
#define BQ2022_ID_AAC					0xAACAACAA
#define BQ2022_ID_COSLIGHT				0xDF0C7A62
#define BQ2022_ID_SAMSUNG_FMT				0xF40E9762

#define BQ2022_BATTERY_INFO_MAGIC			0xE54C21ED
typedef struct //1024 bit
{
	unsigned int magic;
	unsigned int pad1;
	unsigned int data1 : 8;
	unsigned int pad2 : 24;
	unsigned int pad3[12];
	unsigned int pad4 : 8;
	unsigned int data2 : 24;
	unsigned int pad5[16];
} __attribute__((packed)) bq2022_battery_info;

static unsigned int w1_bq2022_battery_info_id = 0;

int w1_bq2022_has_battery_data(void)
{
	return w1_bq2022_battery_info_id != 0;
}

int w1_bq2022_get_battery_id(void)
{
	int ret = 0;

	switch(w1_bq2022_battery_info_id) {
	case BQ2022_ID_LG_DESA:
	case BQ2022_ID_COSLIGHT:
		ret = 0x30000; // batt_id_kohm = 12
		break;

	case BQ2022_ID_SAMSUNG_XWD:
	case BQ2022_ID_AAC:
	case BQ2022_ID_SAMSUNG_FMT:
		ret = 0x40000; // batt_id_kohm = 17
		break;

	case BQ2022_ID_SONY_XWD:
	case BQ2022_ID_SONY_FMT:
	case BQ2022_ID_DELSA:
		ret = 0x50000; // batt_id_kohm = 22
		break;

	case BQ2022_ID_GUANGYU:
		ret = 0x60000; // batt_id_kohm = 28
		break;

	case BQ2022_ID_RUISHENG:
		ret = 0x70000; // batt_id_kohm = 33
		break;

	case BQ2022_ID_SAMSUNG_XWD_COSTDOWN:
		ret = 0x80000; // batt_id_kohm = 38
		break;
	}

	return ret;
}

static int w1_bq2022_add_slave(struct w1_slave *sl)
{
	char cmd[4];
	int retries = 5;
	bq2022_battery_info battery_info;

	if (!sl) {
		pr_err("%s: No w1 device\n", __func__);
		return -1;
	}

retry:
	/* Initialization, master's mutex should be hold */
	if (!(retries--)) {
		pr_err("%s: fatal error\n", __func__);
		return -1;
	}

	if (w1_reset_bus(sl->master)) {
		pr_warn("%s: reset bus failed, just retry!\n", __func__);
		goto retry;
	}

	/* rom comm byte + read comm byte + addr 2 bytes */
	cmd[0] = BQ2022_COMMAND_SKIP_SERIALIZATION_ROM;
	cmd[1] = BQ2022_COMMAND_READ_MEMORY_FIELD_CRC;
	cmd[2] = 0x0;
	cmd[3] = 0x0;

	/* send command */
	w1_write_block(sl->master, cmd, 4);

	/* crc verified for read comm byte and addr 2 bytes*/
	if (w1_read_8(sl->master) != w1_calc_crc8(&cmd[1], 3)) {
		pr_err("%s: com crc err\n", __func__);
		goto retry;
	}

	/* read the whole memory, 1024-bit */
	w1_read_block(sl->master, (char*) &battery_info, sizeof(battery_info));

	/* crc verified for data */
	if (w1_read_8(sl->master) != w1_calc_crc8((char*) &battery_info, sizeof(battery_info))) {
		pr_err("%s: w1_bq2022 data crc err\n", __func__);
		goto retry;
	}

	if (battery_info.magic != BQ2022_BATTERY_INFO_MAGIC) {
		pr_err("%s: invalid battery info magic\n", __func__);
		return -1;
	}

	w1_bq2022_battery_info_id = battery_info.data1 | battery_info.data2 << 8;

	return 0;
}

static void w1_bq2022_remove_slave(struct w1_slave *sl)
{
	w1_bq2022_battery_info_id = 0;
}

static struct w1_family_ops w1_bq2022_fops = {
	.add_slave = w1_bq2022_add_slave,
	.remove_slave = w1_bq2022_remove_slave,
};

static struct w1_family w1_bq2022_family = {
	.fid = 0x09,
	.fops = &w1_bq2022_fops,
};

static int __init w1_bq2022_init(void)
{
	return w1_register_family(&w1_bq2022_family);
}
module_init(w1_bq2022_init);

static void __exit w1_bq2022_exit(void)
{
	w1_unregister_family(&w1_bq2022_family);
}
module_exit(w1_bq2022_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Balázs Triszka <balika011@protonmail.ch>");
MODULE_DESCRIPTION("TI BQ2022 Battery Chip Driver");
