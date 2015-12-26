#ifndef _CEDARJPEGLIB_H_
#define _CEDARJPEGLIB_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdint.h>

#define EXPORT __attribute__ ((visibility ("default")))

typedef void *CEDAR_JPEG_HANDLE;

#if __cplusplus
extern "C"
{
#endif
EXPORT CEDAR_JPEG_HANDLE cedarInitJpeg(EGLDisplay eglD);
EXPORT int cedarLoadJpeg(CEDAR_JPEG_HANDLE handle, const char *filename);
EXPORT int cedarLoadMem(CEDAR_JPEG_HANDLE handle, uint8_t *buf, size_t size);
EXPORT int cedarDecodeJpeg(CEDAR_JPEG_HANDLE handle, int width, int height);
EXPORT int cedarDecodeJpegToMem(CEDAR_JPEG_HANDLE handle, int width, int height, char *mem);
EXPORT int cedarCloseJpeg (CEDAR_JPEG_HANDLE handle);
EXPORT int cedarDestroyJpeg(CEDAR_JPEG_HANDLE handle);
EXPORT void cedarGetEglImage(CEDAR_JPEG_HANDLE, void **egl_image);
EXPORT int cedarGetOrientation(CEDAR_JPEG_HANDLE);
EXPORT int cedarGetWidth(CEDAR_JPEG_HANDLE);
EXPORT int cedarGetHeight(CEDAR_JPEG_HANDLE);
#if __cplusplus
}
#endif

#endif

