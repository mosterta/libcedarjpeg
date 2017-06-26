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

#ifndef _VEPOC_H_
#define _VEPOC_H_

#include <stdint.h>
#include "ve.h"
#include "cedarJpegLib.h"

typedef enum {
	VEISP_COLOR_FORMAT_NV12 = 0,
	VEISP_COLOR_FORMAT_NV16 = 1,
	VEISP_COLOR_FORMAT_TILE = 2,
	VEISP_COLOR_FORMAT_NV16_2 = 3,
	VEISP_COLOR_FORMAT_BGGR = 4,
	VEISP_COLOR_FORMAT_RGGB = 5,
	VEISP_COLOR_FORMAT_GBRG = 6,
	VEISP_COLOR_FORMAT_GRBG = 7,
} veisp_color_format;

typedef enum {
	VEAVC_ENCODER_MODE_H264 = 0,
	VEAVC_ENCODER_MODE_JPEG = 1,
} veavc_encoder_mode;

void picture_generate(uint32_t w, uint32_t h, uint8_t *Y, uint8_t *C);

void vejpeg_header_create(int w, int h, int quality);
void vejpeg_header_destroy(void);
void vejpeg_write_SOF0(void);
void vejpeg_write_SOS(void);
void vejpeg_write_quantization(void);
void vejpeg_write_file(cedarEncJpegWriteCB funcWrite, void* opaque, uint8_t *buffer, uint32_t length);

void veisp_set_buffers(CEDARV_MEMORY Y, CEDARV_MEMORY C);
void veisp_set_picture_size(uint32_t w, uint32_t h, uint32_t stride);
void veisp_init_picture(uint32_t w, uint32_t h, uint32_t stride, veisp_color_format f);

void veavc_select_subengine(void);
void veavc_init_vle(CEDARV_MEMORY J, uint32_t size);
void veavc_init_ctrl(veavc_encoder_mode mode);
void veavc_jpeg_parameters(uint8_t fill1, uint8_t stuff, uint32_t biasY, uint32_t biasC);
void veavc_put_bits(uint8_t nbits, uint32_t data);
void veavc_sdram_index(uint32_t index);
void veavc_jpeg_quantization(uint16_t *tableY, uint16_t *tableC, uint32_t length);
void veavc_launch_encoding(void);
void veavc_check_status(void);
uint32_t veavc_get_written(void);

#endif

