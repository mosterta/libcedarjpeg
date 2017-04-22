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
#include <string.h>
#include "vepoc.h"
#include <libyuv.h>
#include <assert.h>

#define min(A,B) ((A) < (B) ? (A) : (B))
#if 0
int main(int argc, char *argv[])
{
	char *outjpeg = "/tmp/poc.jpeg";
	int quality = 100;
	uint32_t w = 0;
	uint32_t h = 0;
	uint32_t stride = 0;
	uint32_t bufsize = 0;
	CEDARV_MEMORY Y;
	CEDARV_MEMORY J;
	uint32_t Jsize = 0;
	uint32_t Jwritten = 0;

	if(argc != 4 && argc != 5)
	{
		fprintf(stderr, "usage: %s width height stride quality [out.jpeg]\n", argv[0]);
		return 1;
	}

	w = atoi(argv[1]);
	h = atoi(argv[2]);
	stride = atoi(argv[3]);
	quality = atoi(argv[4]);
	
	if(argc > 5)
		outjpeg = argv[5];

	char buffer[1024];
	int status;
	int offset = 0;
	int readSize=1024;
	FILE *fp = fopen("/tmp/pic.argb", "rb");
	do
	{
	  readSize = min(readSize, bufsize-offset);
	  status = fread(buffer, 1, readSize, fp);
	  cedarv_memcpy(Y, offset, buffer, status);
	  offset += status;
	} while(status > 0 && offset < bufsize);
	fclose(fp);


}
#endif

int cedarEncJpeg(void *in_mem, int in_stride, int in_width, int in_height, 
		 int *out_width, int *out_height, int quality, char *out_filename)
{
	uint32_t w = 0;
	uint32_t h = 0;
	uint32_t stride = in_stride;
	CEDARV_MEMORY Y;
	CEDARV_MEMORY Y1;
	CEDARV_MEMORY convY;
	CEDARV_MEMORY convUV;

	CEDARV_MEMORY J;
	uint32_t Jsize = 0;
	uint32_t Jwritten = 0;
	
	cedarv_open();

	w = (in_width + 15) & ~15;
	h = (in_height + 15) & ~15;
	*out_width = w;
	*out_height = h;
	int bufsize = in_stride * in_height * 4;
	Y = cedarv_malloc(bufsize);
	if (!cedarv_isValid(Y))
	  return -1;
	
	cedarv_memcpy(Y, 0, in_mem, bufsize);

	Y1 = cedarv_malloc(in_stride * h * 4);
	if(!cedarv_isValid(Y1))
	{
	  cedarv_free(Y);
	  return -1;
	}
	
	ARGBScale(cedarv_getPointer(Y), in_stride, in_width, in_height,
		  cedarv_getPointer(Y1), stride, w, h, 0);

	cedarv_free(Y);
	
	convY = cedarv_malloc(w * h);
	convUV = cedarv_malloc(w * h / 2);

	if(!cedarv_isValid(convY) || !cedarv_isValid(convUV))
	{
	  if(cedarv_isValid(convY))
	    cedarv_free(convY);
	  if(cedarv_isValid(convUV))
	    cedarv_free(convUV);
	  cedarv_free(Y1);
	  return -1;
	}
	ARGBToNV12(cedarv_getPointer(Y1), stride, 
		   cedarv_getPointer(convY), w, 
		   cedarv_getPointer(convUV), w,
		   w, h);
	
	cedarv_free(Y1);
	
	cedarv_flush_cache(convY, w * h);
	cedarv_flush_cache(convUV, w * h / 2);

	Jsize = w*h*2;
	J = cedarv_malloc(Jsize);

	if(!cedarv_allocateEngine(0))
	{
	  return 0;
	}

	veavc_select_subengine();
	veisp_set_buffers(convY, convUV);
	veisp_init_picture(w, h, w, VEISP_COLOR_FORMAT_NV12);

	veavc_init_vle(J, Jsize);
	veavc_init_ctrl(VEAVC_ENCODER_MODE_JPEG);
	veavc_jpeg_parameters(1, 0, 0, 0);

	vejpeg_header_create(w, h, quality);
	vejpeg_write_SOF0();
	vejpeg_write_SOS();
	vejpeg_write_quantization();

	veavc_launch_encoding();
	cedarv_wait(2);
	veavc_check_status();

	Jwritten = veavc_get_written();

	cedarv_freeEngine();

	/* flush for A20 */
	cedarv_flush_cache(J, Jsize);
	vejpeg_write_file(out_filename, cedarv_getPointer(J), Jwritten);

	cedarv_free(J);
	cedarv_free(convY);
	cedarv_free(convUV);
	cedarv_close();
	return 0;
}

