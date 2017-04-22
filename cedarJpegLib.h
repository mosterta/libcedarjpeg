#ifndef _CEDARJPEGLIB_H_
#define _CEDARJPEGLIB_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdint.h>

#define DLL_EXPORT __attribute__ ((visibility ("default")))

typedef void *CEDAR_JPEG_HANDLE;

#if __cplusplus
extern "C"
{
#endif
DLL_EXPORT CEDAR_JPEG_HANDLE cedarInitJpeg(EGLDisplay eglD);
DLL_EXPORT int cedarLoadJpeg(CEDAR_JPEG_HANDLE handle, const char *filename);
DLL_EXPORT int cedarLoadMem(CEDAR_JPEG_HANDLE handle, uint8_t *buf, size_t size);
DLL_EXPORT int cedarDecodeJpeg(CEDAR_JPEG_HANDLE handle, int width, int height);
DLL_EXPORT int cedarDecodeJpegToMem(CEDAR_JPEG_HANDLE handle, int width, int height, char *mem);
DLL_EXPORT int cedarCloseJpeg (CEDAR_JPEG_HANDLE handle);
DLL_EXPORT int cedarDestroyJpeg(CEDAR_JPEG_HANDLE handle);
DLL_EXPORT void cedarGetEglImage(CEDAR_JPEG_HANDLE, void **egl_image);
DLL_EXPORT int cedarGetOrientation(CEDAR_JPEG_HANDLE);
DLL_EXPORT int cedarGetWidth(CEDAR_JPEG_HANDLE);
DLL_EXPORT int cedarGetHeight(CEDAR_JPEG_HANDLE);

DLL_EXPORT int cedarEncJpeg(void *in_mem, int in_stride, int in_width, int in_height, 
			    int *out_width, int *out_height, int quality, char *out_filename);
#if __cplusplus
}
#endif

#endif

