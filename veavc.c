/*
 * Copyright (c) 2014 Manuel Braga <mul.braga@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "vepoc.h"
#include <ve.h>

static veavc_encoder_mode encoder_mode = VEAVC_ENCODER_MODE_H264;

void veavc_select_subengine(void)
{
	void *regs = cedarv_get_regs();
	uint32_t ctrl = readl(regs + CEDARV_CTRL);
	ctrl = (ctrl & 0xfffffff0) | 0xb;
	writel(ctrl, regs + CEDARV_CTRL);
}

void veavc_init_vle(CEDARV_MEMORY J, uint32_t size)
{
	void *regs = cedarv_get_regs();
	uint32_t pJ = cedarv_virt2phys(J);
	uint32_t end = pJ + size - 1;
	uint32_t maxbits = (size * 8 + 0xffff) & ~0xffff;
	uint32_t max = maxbits > 0x0fff0000 ? 0x0fff0000 : maxbits;
	writel(pJ	    , regs + CEDARV_AVC_VLE_ADDR);
	writel(end	    , regs + CEDARV_AVC_VLE_END);
	writel(0	    , regs + CEDARV_AVC_VLE_OFFSET);
	writel(max	    , regs + CEDARV_AVC_VLE_MAX);
}

void veavc_init_ctrl(veavc_encoder_mode mode)
{
	void *regs = cedarv_get_regs();
	uint32_t trigger = (mode & 1) << 16;
	uint32_t status;
	encoder_mode = mode;

	writel(0x0000000f  , regs + CEDARV_AVC_CTRL);
	writel(trigger     , regs + CEDARV_AVC_TRIGGER);

	/* clear status bits */
	status = readl(regs + CEDARV_AVC_STATUS);
	writel(status | 0xf, regs + CEDARV_AVC_STATUS);
}

void veavc_jpeg_parameters(uint8_t fill1, uint8_t stuff, uint32_t biasY, uint32_t biasC)
{
  	void *regs = cedarv_get_regs();
	uint32_t valfill1 = fill1 > 0 ? 1 : 0;
	uint32_t valstuff = stuff > 0 ? 1 : 0;
	uint32_t value = 0;
	value |= (valfill1 & 1) << 31;
	value |= (valstuff & 1) << 30;
	value |= (biasC & 0x7ff) << 16;
	value |= (biasY & 0x7ff) << 0;
	writel(value, regs + CEDARV_AVC_PARAM);
}

static const char* status_to_print(uint32_t status)
{
	uint32_t value = status & 0xf;
	if(value == 0) return "none";
	if(value == 1) return "success";
	if(value == 2) return "failed";
	return "unknown";
}

void veavc_put_bits(uint8_t nbits, uint32_t data)
{
  	void *regs = cedarv_get_regs();
	uint32_t trigger = (encoder_mode & 1) << 16;
	uint32_t status;
	trigger |= (nbits & 0x3f) << 8;
	trigger |= 1;
	writel(data   , regs + CEDARV_AVC_BASIC_BITS);
	writel(trigger, regs + CEDARV_AVC_TRIGGER);

	status = readl(regs + CEDARV_AVC_STATUS) & 0xf;
}

void veavc_sdram_index(uint32_t index)
{
  	void *regs = cedarv_get_regs();
	writel(index, regs + CEDARV_AVC_SDRAM_INDEX);
}

void veavc_jpeg_quantization(uint16_t *tableY, uint16_t *tableC, uint32_t length)
{
	uint32_t data;
	int i;
	void *regs = cedarv_get_regs();
	
	veavc_sdram_index(0x0);

/*
	When compared to libjpeg, there are still rounding errors in the
	coefficients values (around 1 unit of difference).
*/
	for(i = 0; i < length; i++)
	{
		data  = 0x0000ffff & (0xffff / tableY[i]);
		data |= 0x00ff0000 & (((tableY[i] + 1) / 2) << 16);
		writel(data, regs + CEDARV_AVC_SDRAM_DATA);
	}
	for(i = 0; i < length; i++)
	{
		data  = 0x0000ffff & (0xffff / tableC[i]);
		data |= 0x00ff0000 & (((tableC[i] + 1) / 2) << 16);
		writel(data, regs + CEDARV_AVC_SDRAM_DATA);
	}
}

void veavc_launch_encoding(void)
{
  	void *regs = cedarv_get_regs();
	uint32_t trigger = (encoder_mode & 1) << 16;
	trigger |= 8;
	writel(trigger, regs + CEDARV_AVC_TRIGGER);
}

void veavc_check_status(void)
{
  	void *regs = cedarv_get_regs();
	uint32_t status = readl(regs + CEDARV_AVC_STATUS) & 0xf;
}

uint32_t veavc_get_written(void)
{
  	void *regs = cedarv_get_regs();
	uint32_t bits = readl(regs + CEDARV_AVC_VLE_LENGTH);
	return bits / 8;
}

