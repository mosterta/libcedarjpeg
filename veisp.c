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

void veisp_set_buffers(CEDARV_MEMORY Y, CEDARV_MEMORY C)
{
	void *regs = cedarv_get_regs();
	uint32_t pY = cedarv_virt2phys(Y);
	writel(pY, regs + CEDARV_ISP_WB_THUMB_LUMA);
	uint32_t pC = cedarv_virt2phys(C);
	writel(pC, regs + CEDARV_ISP_WB_THUMB_CHROMA);
}

inline void veisp_set_picture_size(uint32_t w, uint32_t h, uint32_t s)
{
	void *regs = cedarv_get_regs();
	uint32_t width_mb  = (w + 15) / 16;
	uint32_t height_mb = (h + 15) / 16;
	uint32_t stride_mb = (s +15) / 16;
	uint32_t size  = ((width_mb & 0x3ff) << 16) | (height_mb & 0x3ff);
	uint32_t stride = ((stride_mb & 0x3ff) << 16) | (/*width_mb */ 0 & 0x3ff);

	writel(size	, regs + CEDARV_ISP_PIC_SIZE);
	writel(stride, regs + CEDARV_ISP_PIC_STRIDE);
}

void veisp_init_picture(uint32_t w, uint32_t h, uint32_t stride, veisp_color_format f)
{
	void *regs = cedarv_get_regs();
	uint32_t format = (f & 0xf) << 28;
	veisp_set_picture_size(w, h, stride);

	writel(format, regs + CEDARV_ISP_CTRL);
}

