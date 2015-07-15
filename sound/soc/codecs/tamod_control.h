/* 
 * WCD9306 Tapan Audio Mod Controls(TAMOD)
 * 
 * 	Author: Cezar Rey Templonuevo <zeroblade1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

extern struct snd_soc_codec *wcd9306_codec;

extern bool hp_digigain_con;
extern bool spkr_digigain_con;

extern bool hp_toggle;
extern bool spkr_toggle;

extern u32 hp_digigain;
extern u32 spkr_digigain;

/* Register defaults */
#define TAPAN_A_CDC_RXX_VOL_MASK		0x7F
#define TAPAN_A_CDC_RX_VOL_CTL_B2_CTL_DEF	0x00

int tapan_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value);
