TARGET = jpeg-test
TARGET_SO = libcedarJpeg.so
SRC = main.c jpeg.c  
CFLAGS = -Wall -Wextra -I../libvdpau-sunxi -DUSE_UMP -DINCLUDE_MAIN
CFLAGS_SO = -g -fPIC -Wall -Wextra -I../../libvdpau-sunxi -DUSE_UMP -fvisibility=hidden
LDFLAGS = -lpthread -lUMP -lEGL -lGLESv2 -lcedar_access
LDFLAGS_SO = -fPIC -shared -lpthread -lUMP -lEGL -lGLESv2 -lcedar_access
