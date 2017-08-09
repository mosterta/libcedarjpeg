/*
 * Proof of Concept JPEG decoder using Allwinners CedarX
 *
 * WARNING: Don't use on "production systems". This was made by reverse
 * engineering and might crash your system or destroy data!
 * It was made only for my personal use of testing the reverse engineered
 * things, so don't expect good code quality. It's far from complete and
 * might crash if the JPEG doesn't fit it's requirements!
 *
 *
 *
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>
#include "cedarJpegLib.h"
#include <ve.h>
#include "jpeg.h"
#include <libyuv.h>

#include "eglplatform_fb.h"
#include "fbdev_window.h" 
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <linux/types.h>
#include <linux/sunxi_disp_ioctl.h>
#include <errno.h>
#include <sys/ioctl.h>

#define COLOR_YUV420 0
#define COLOR_YUV422 2

static int TestEGLError(const char* pszLocation);
static int TestGLError(const char* pszLocation);

static int vedisp_convertMb2ARGB(struct cedarJpeg_handle *jpeg, int width, int height, int color_format, 
                            CEDARV_MEMORY l, CEDARV_MEMORY c, CEDARV_MEMORY outrgb);
static void vedisp_init(struct cedarJpeg_handle *jpeg);
static void vedisp_close(struct cedarJpeg_handle *jpeg);
static void createTexture2D(fbdev_pixmap *pm, int width, int height, CEDARV_MEMORY mem);

static void set_quantization_tables(struct jpeg_t *jpeg, void *regs)
{
	int i;
	for (i = 0; i < 64; i++)
		writel((uint32_t)(64 + i) << 8 | jpeg->quant[0]->coeff[i], regs + CEDARV_MPEG_IQ_MIN_INPUT);
	for (i = 0; jpeg->quant[1] && i < 64; i++)
		writel((uint32_t)(i) << 8 | jpeg->quant[1]->coeff[i], regs + CEDARV_MPEG_IQ_MIN_INPUT);
}

static void set_huffman_tables(struct jpeg_t *jpeg, void *regs)
{
	uint32_t buffer[512];
	memset(buffer, 0, sizeof(buffer));
	int i;
	for (i = 0; i < 4; i++)
	{
		if (jpeg->huffman[i])
		{
			int j, sum, last;

			last = 0;
			sum = 0;
			for (j = 0; j < 16; j++)
			{
				((uint8_t *)buffer)[i * 64 + 32 + j] = sum;
				sum += jpeg->huffman[i]->num[j];
				if (jpeg->huffman[i]->num[j] != 0)
					last = j;
			}
			memcpy(&(buffer[256 + 64 * i]), jpeg->huffman[i]->codes, sum);
			sum = 0;
			for (j = 0; j <= last; j++)
			{
				((uint16_t *)buffer)[i * 32 + j] = sum;
				sum += jpeg->huffman[i]->num[j];
				sum *= 2;
			}
			for (j = last + 1; j < 16; j++)
			{
				((uint16_t *)buffer)[i * 32 + j] = 0xffff;
			}
		}
	}

	for (i = 0; i < 512; i++)
	{
		writel(buffer[i], regs + CEDARV_MPEG_RAM_WRITE_DATA);
	}
}

static void set_format(struct jpeg_t *jpeg, void *regs)
{
	uint8_t fmt = (jpeg->comp[0].samp_h << 4) | jpeg->comp[0].samp_v;

	switch (fmt)
	{
	case 0x11:
		writeb(0x1b, regs + CEDARV_MPEG_TRIGGER + 0x3);
		break;
	case 0x21:
		writeb(0x13, regs + CEDARV_MPEG_TRIGGER + 0x3);
		break;
	case 0x12:
		writeb(0x23, regs + CEDARV_MPEG_TRIGGER + 0x3);
		break;
	case 0x22:
		writeb(0x03, regs + CEDARV_MPEG_TRIGGER + 0x3);
		break;
	}
}

static void set_size(struct jpeg_t *jpeg, void *regs)
{
    uint16_t width = jpeg->width;
    uint16_t height = jpeg->height;
    if(width > 0)
      width--;
    if(height > 0)
      height--;
	uint16_t h = (jpeg->height-1) / (8 * jpeg->comp[0].samp_v);
	uint16_t w = (jpeg->width-1) / (8 * jpeg->comp[0].samp_h);
	writel((uint32_t)h << 16 | w, regs + CEDARV_MPEG_JPEG_SIZE);
}

static void decode_jpeg(struct cedarJpeg_handle *handle, int width, int height)
{
	int status;
	if (!cedarv_open())
		err(EXIT_FAILURE, "Can't open VE");

	int input_size =(handle->data_len + 65535) & ~65535;
	CEDARV_MEMORY input_buffer = cedarv_malloc(input_size);
	int output_size = ((handle->jpeg.width + 31) & ~31) * ((handle->jpeg.height + 31) & ~31);
	handle->luma_output = cedarv_malloc(output_size);
	handle->chroma_output = cedarv_malloc(output_size);
	cedarv_memcpy(input_buffer, 0, handle->data, handle->data_len);
	cedarv_flush_cache(input_buffer, handle->data_len);

	// activate MPEG engine
	void *cedarv_regs = cedarv_get(CEDARV_ENGINE_MPEG, 0);

	// set restart interval
	writel(handle->jpeg.restart_interval, cedarv_regs + CEDARV_MPEG_JPEG_RES_INT);

	// set JPEG format
	set_format(&handle->jpeg, cedarv_regs);

	// set output buffers (Luma / Croma)
	writel(cedarv_virt2phys(handle->luma_output), cedarv_regs + CEDARV_MPEG_ROT_LUMA);
	writel(cedarv_virt2phys(handle->chroma_output), cedarv_regs + CEDARV_MPEG_ROT_CHROMA);

	if (cedarv_get_version() >= 0x1680)
	{
	    writel(OUTPUT_FORMAT_NV12, cedarv_regs + CEDARV_OUTPUT_FORMAT);
	    writel((0x1 << 30) | (0x1 << 28) , cedarv_regs + CEDARV_EXTRA_OUT_FMT_OFFSET);
	}

	// set size
	set_size(&handle->jpeg, cedarv_regs);

	// ??
	writel(0x00000000, cedarv_regs + CEDARV_MPEG_SDROT_CTRL);

	// input end
	writel(cedarv_virt2phys(input_buffer) + input_size - 1, cedarv_regs + CEDARV_MPEG_VLD_END);

	// ??
	writel(0x0000007c, cedarv_regs + CEDARV_MPEG_CTRL);

	// set input offset in bits
	writel(0 * 8, cedarv_regs + CEDARV_MPEG_VLD_OFFSET);

	// set input length in bits
	writel(handle->data_len * 8, cedarv_regs + CEDARV_MPEG_VLD_LEN);

	// set input buffer
    	writel(cedarv_virt2phys(input_buffer) | 0x70000000, cedarv_regs + CEDARV_MPEG_VLD_ADDR);

	// set Quantisation Table
	set_quantization_tables(&handle->jpeg, cedarv_regs);

	// set Huffman Table
	writel(0x00000000, cedarv_regs + CEDARV_MPEG_RAM_WRITE_PTR);
	set_huffman_tables(&handle->jpeg, cedarv_regs);

	// start
	writeb(0x0e, cedarv_regs + CEDARV_MPEG_TRIGGER);

	// wait for interrupt
	status = cedarv_wait(1);
	if(status < 0)
	  printf("timeout waiting for hardware decode process to finish");
	  
	// clean interrupt flag (??)
	writel(0x0000c00f, cedarv_regs + CEDARV_MPEG_STATUS);

#if 0
    printf("MCU value is=%d\n", readl(cedarv_regs + CEDARV_MPEG_JPEG_MCU));
    printf("MCU start value is=%d\n", readl(cedarv_regs + CEDARV_MPEG_JPEG_MCU_START));
    printf("MCU end value is=%d\n", readl(cedarv_regs + CEDARV_MPEG_JPEG_MCU_END));
    int offset = readl(cedarv_regs + CEDARV_MPEG_VLD_OFFSET);
    printf("error is=%X\n", readl(cedarv_regs + CEDARV_MPEG_ERROR));
    printf("VLD offset is=%d length=%d\n", offset, input_size*8);
    offset=((offset/8+1)&~0x1) - 1;
    
    printf("data[%X]=0x%02X%02X%02X%02X\n", offset,
           handle->data[(offset)],
           handle->data[(offset)+1],
           handle->data[(offset)+2],
           handle->data[(offset)+3]);
    printf("data[%X]=0x%02X%02X%02X%02X\n", (offset/8)+4,
           handle->data[(offset)+4],
           handle->data[(offset)+5],
           handle->data[(offset)+6],
           handle->data[(offset)+7]);
#endif

	// stop MPEG engine
	cedarv_put();
	cedarv_close();

	cedarv_free(input_buffer);
}

CEDAR_JPEG_HANDLE cedarInitJpeg(EGLDisplay disp)
{
	struct cedarJpeg_handle *jpeg;
	jpeg = (struct cedarJpeg_handle*)malloc (sizeof (*jpeg));
	if( jpeg )
	{
		memset(jpeg, 0, sizeof(*jpeg));
        	jpeg->in_fd = -1; 
	        jpeg->disp_fd = -1;
        	if(ump_open() != UMP_OK)
           		exit(1);

	        jpeg->eglDisplay = disp;

		jpeg->peglCreateImageKHR =
    			(PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
 
		if(jpeg->peglCreateImageKHR == NULL){
    			printf("eglCreateImageKHR not found!\n");
  		}

		jpeg->peglDestroyImageKHR =
			(PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
 
		if(jpeg->peglDestroyImageKHR == NULL){
			printf("eglCreateImageKHR not found!\n");
		}
	}
	cedarv_open();
	jpeg->cedar_engine_version = cedarv_get_version();
	cedarv_close();
	
	return (CEDAR_JPEG_HANDLE)jpeg;
}

int cedarLoadJpeg(CEDAR_JPEG_HANDLE handle, const char *filename)
{
	struct stat s;
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
    int error;

	if ((jpeg->in_fd = open(filename, O_RDONLY)) == -1)
		err(EXIT_FAILURE, "%s", filename);

	if (fstat(jpeg->in_fd, &s) < 0)
		err(EXIT_FAILURE, "stat %s", filename);

	if (s.st_size == 0)
		errx(EXIT_FAILURE, "%s empty", filename);
        jpeg->in_size = s.st_size;

	if ((jpeg->in_buf = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED, jpeg->in_fd, 0)) == MAP_FAILED)
		err(EXIT_FAILURE, "mmap %s", filename);

	jpeg->in_mapped = 1;

	//memset(jpeg, 0, sizeof(*jpeg));

    error = parse_jpeg(jpeg, jpeg->in_buf, jpeg->in_size);
    if (error <= 0)
      warnx("Can't parse JPEG, error=%d", error);
	return 1;

}
int cedarLoadMem(CEDAR_JPEG_HANDLE handle, uint8_t *buf, size_t size)
{
    int error;
    struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	jpeg->in_buf = buf;
	jpeg->in_size = size;
	jpeg->in_mapped = 0;
    
    error = parse_jpeg(jpeg, jpeg->in_buf, jpeg->in_size);
	if (error <= 0)
    {
		warnx("Can't parse JPEG, error=%d", error);
        return 0;
    }
	return 1;
}

int cedarDecodeJpeg(CEDAR_JPEG_HANDLE handle, int width, int height)
{
	int status;
    struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;

    if(!cedarv_allocateEngine(0))
    {
      return 0;
    }
    width = (width + 15) & ~15;
	height = (height + 15) & ~15;
	jpeg->disp_width = width;
	jpeg->disp_height = height;

	int size = width * height * 4;
	if(jpeg->egl_image)
	    jpeg->peglDestroyImageKHR(jpeg->eglDisplay, jpeg->egl_image);

	if(cedarv_isValid(jpeg->decodedPic))
	    cedarv_free(jpeg->decodedPic);
	jpeg->decodedPic = cedarv_malloc(size);
	assert(cedarv_isValid(jpeg->decodedPic));
	cedarv_flush_cache(jpeg->decodedPic, size);
	decode_jpeg(jpeg, width, height);

#if 0
	char fname[200];
	sprintf(fname, "/tmp/jpeg_dec.0x%x.l", cedarv_get_version());
	FILE *fp = fopen(fname, "wb");
	fwrite(cedarv_getPointer(jpeg->luma_output), 1, size, fp);
	fclose(fp);
	sprintf(fname, "/tmp/jpeg_dec.0x%x.c", cedarv_get_version());
	fp = fopen(fname, "wb");
	fwrite(cedarv_getPointer(jpeg->chroma_output), 1, size, fp);
	fclose(fp);
#endif

    cedarv_freeEngine();
    
	vedisp_init(jpeg);
	int color;
	switch (jpeg->jpeg.color_format)
	{
	case 0x11:
	case 0x21:
		color = COLOR_YUV422;
		break;
	case 0x12:
	case 0x22:
	default:
		color = COLOR_YUV420;
		break;
	}
	
	status = 0;
	if (jpeg->cedar_engine_version < 0x1680)
	{
	  status = vedisp_convertMb2ARGB(jpeg, width, height, color, jpeg->luma_output, 
				  jpeg->chroma_output, jpeg->decodedPic);
	}
	if(status == 0)
	{
	  void *y = malloc(cedarv_getSize(jpeg->luma_output));
	  void *c = malloc(cedarv_getSize(jpeg->chroma_output));
	  if(y && c)
	  {
	    cedarv_sw_convertMb32420ToNv21Y(cedarv_getPointer(jpeg->luma_output), y, width, height);
	    cedarv_sw_convertMb32420ToNv21C(cedarv_getPointer(jpeg->chroma_output), c, width, height);

	    int status = NV21ToARGB(y, jpeg->jpeg.width,
			    c, jpeg->jpeg.width,
			    cedarv_getPointer(jpeg->decodedPic), jpeg->jpeg.width,
			    jpeg->jpeg.width, jpeg->jpeg.height);
	    free(y);
	    free(c);
	  }
	  else
	  {
	    if(y)
	      free(y);
	    if(c)
	      free(c);
	  }
	}
    
	vedisp_close(jpeg);
	cedarv_free(jpeg->luma_output);
	cedarv_free(jpeg->chroma_output);
	return status;
}

int cedarDecodeJpegToMem(CEDAR_JPEG_HANDLE handle, int width, int height, char *mem)
{
	int ret;
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	ret = cedarDecodeJpeg(handle, width, height);
    if(ret)
	  memcpy(mem, cedarv_getPointer(jpeg->decodedPic), width*height*4);
	return ret;
}

void cedarGetEglImage(CEDAR_JPEG_HANDLE handle, void **egl_image)
{
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	const EGLint renderImageAttrs[] = {
	    EGL_IMAGE_PRESERVED_KHR, EGL_FALSE, 
	    EGL_NONE
	};
 
	createTexture2D(&jpeg->pixmap, jpeg->disp_width, jpeg->disp_height, jpeg->decodedPic);
	jpeg->egl_image = jpeg->peglCreateImageKHR(jpeg->eglDisplay,
			  EGL_NO_CONTEXT,  
			  EGL_NATIVE_PIXMAP_KHR,
			  &jpeg->pixmap,
			  renderImageAttrs);
	TestEGLError("peglCreateImageKHR");

 	*egl_image = jpeg->egl_image;
}

int cedarCloseJpeg (CEDAR_JPEG_HANDLE handle)
{
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle*)handle;
	if(jpeg->in_mapped == 1)
	{
		munmap(jpeg->in_buf, jpeg->in_size);
		close(jpeg->in_fd);
		jpeg->in_fd = -1;
	}
	return 1;
}
int cedarDestroyJpeg(CEDAR_JPEG_HANDLE handle)
{
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	if(jpeg->egl_image)
	{
		jpeg->peglDestroyImageKHR(jpeg->eglDisplay, jpeg->egl_image);
		ump_reference_release(jpeg->decodedPic.mem_id);
		jpeg->egl_image = 0;
	}
	if(cedarv_isValid(jpeg->decodedPic))
	{
		cedarv_free(jpeg->decodedPic);
		cedarv_setBufferInvalid(jpeg->decodedPic);
	}
	ump_close();
	free(jpeg);
	return 1;
}
int cedarGetOrientation(CEDAR_JPEG_HANDLE handle)
{
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	return jpeg->jpeg.orientation;
}
int cedarGetWidth(CEDAR_JPEG_HANDLE handle)
{
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	return jpeg->jpeg.width;
}
int cedarGetHeight(CEDAR_JPEG_HANDLE handle)
{
	struct cedarJpeg_handle *jpeg = (struct cedarJpeg_handle *)handle;
	return jpeg->jpeg.height;
}



#if 1
static void vedisp_init(struct cedarJpeg_handle *jpeg)
{
   if(jpeg->disp_fd == -1)
      jpeg->disp_fd = open("/dev/disp", O_RDWR);
}

static void vedisp_close(struct cedarJpeg_handle *jpeg)
{
   close(jpeg->disp_fd);
   jpeg->disp_fd = -1;
}

static int vedisp_convertMb2ARGB(struct cedarJpeg_handle *jpeg, int width, int height, int color_format, 
                            CEDARV_MEMORY l, CEDARV_MEMORY c, CEDARV_MEMORY outrgb)
{
  int status = 1; 
  unsigned long arg[4] = {0, 0, 0, 0};
   __disp_scaler_para_t scaler_para;
   int result; 
   arg[1] = ioctl(jpeg->disp_fd, DISP_CMD_SCALER_REQUEST, (unsigned long) arg);
   if(arg[1] == (unsigned long)-1) return 0;

   memset(&scaler_para, 0, sizeof(__disp_scaler_para_t));
   scaler_para.input_fb.addr[0] = cedarv_virt2phys(l);
   scaler_para.input_fb.addr[1] = cedarv_virt2phys(c);
   scaler_para.input_fb.size.width = jpeg->jpeg.width;
   scaler_para.input_fb.size.height = jpeg->jpeg.height;
   switch (color_format)
   {
      case COLOR_YUV420:
         scaler_para.input_fb.format = DISP_FORMAT_YUV420;
         break;
      case COLOR_YUV422:
         scaler_para.input_fb.format = DISP_FORMAT_YUV422;
         break;
      default:
         return 0;
         break;
   }
   scaler_para.input_fb.seq = DISP_SEQ_UVUV;
   scaler_para.input_fb.mode = DISP_MOD_MB_UV_COMBINED;
   scaler_para.input_fb.br_swap = 0;
   scaler_para.input_fb.cs_mode = DISP_YCC;
   scaler_para.source_regn.x = 0;
   scaler_para.source_regn.y = 0;
   scaler_para.source_regn.width = jpeg->jpeg.width;
   scaler_para.source_regn.height = jpeg->jpeg.height;
   scaler_para.output_fb.addr[0] = cedarv_virt2phys(outrgb);
   scaler_para.output_fb.size.width = width;
   scaler_para.output_fb.size.height = height;
   scaler_para.output_fb.format = DISP_FORMAT_ARGB8888;
   scaler_para.output_fb.seq = DISP_SEQ_ARGB;
   scaler_para.output_fb.mode = DISP_MOD_INTERLEAVED;
   scaler_para.output_fb.br_swap = 0;
   scaler_para.output_fb.cs_mode = DISP_BT601;

   arg[2] = (unsigned long) &scaler_para;
   result = ioctl(jpeg->disp_fd, DISP_CMD_SCALER_EXECUTE, (unsigned long) arg);
   if(result < 0)
   {
      //printf("scaler execution failed=%d\n", errno);
     status = 0;
   }
   ioctl(jpeg->disp_fd, DISP_CMD_SCALER_RELEASE, (unsigned long) arg);
   return status;
}
#endif

#if defined(INCLUDE_MAIN)
int main(const int argc, const char **argv)
{
	(void)argc;
	struct cedarJpeg_handle jpeg;
	memset(&jpeg, 0, sizeof(jpeg));
	jpeg.in_fd = -1;
	jpeg.disp_fd = -1;

    cedarv_open();
	cedarLoadJpeg(&jpeg, argv[1]);
	int width = 800;
	int height = 600;
	cedarDecodeJpeg(&jpeg, width, height);
	cedarCloseJpeg(&jpeg);
	cedarDestroyJpeg(&jpeg);
	cedarv_close();
    return 0;
}
#endif

static int TestEGLError(const char* pszLocation){

  /*
   eglGetError returns the last error that has happened using egl,
   not the status of the last called function. The user has to
   check after every single egl call or at least once every frame.
  */
   EGLint iErr = eglGetError();
   if (iErr != EGL_SUCCESS)
   {
      printf("%s failed (0x%x).\n", pszLocation, iErr);
      return 0;
   }

   return 1;
}
static int TestGLError(const char* pszLocation){

  /*
   eglGetError returns the last error that has happened using egl,
   not the status of the last called function. The user has to
   check after every single egl call or at least once every frame.
  */
   GLint iErr = glGetError();
   if (iErr != GL_NO_ERROR)
   {
      printf("%s failed (0x%x).\n", pszLocation, iErr);
      return 0;
   }

   return 1;
}

static void createTexture2D(fbdev_pixmap *pm, int width, int height, CEDARV_MEMORY mem)
{
   pm->bytes_per_pixel 	= 4;
   pm->buffer_size 	= 32;
   pm->red_size 	= 8;
   pm->green_size 	= 8;
   pm->blue_size 	= 8;
   pm->alpha_size 	= 8;
   pm->luminance_size 	= 0;
   pm->flags 		= FBDEV_PIXMAP_SUPPORTS_UMP;
   pm->format 		= 0; 
   pm->width 		= width;
   pm->height 		= height;
   pm->data 		= (short unsigned int*)mem.mem_id;
   cedarv_flush_cache(mem, cedarv_getSize(mem));
   ump_reference_add(mem.mem_id);
   
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pm->width, 
                 pm->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   TestEGLError("createTexture2D");
}

#if 0
int main(const int argc, const char **argv)
{
	int in;
	struct stat s;
	uint8_t *data;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s infile.jpg\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	const char *filename = argv[1];

	if ((in = open(filename, O_RDONLY)) == -1)
		err(EXIT_FAILURE, "%s", filename);

	if (fstat(in, &s) < 0)
		err(EXIT_FAILURE, "stat %s", filename);

	if (s.st_size == 0)
		errx(EXIT_FAILURE, "%s empty", filename);

	if ((data = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED, in, 0)) == MAP_FAILED)
		err(EXIT_FAILURE, "mmap %s", filename);

	struct jpeg_t jpeg ;
	memset(&jpeg, 0, sizeof(jpeg));
	if (!parse_jpeg(&jpeg, data, s.st_size))
		warnx("Can't parse JPEG");

	//dump_jpeg(&jpeg);
	decode_jpeg(&jpeg);

	munmap(data, s.st_size);
	close(in);

	return EXIT_SUCCESS;
}
#endif

