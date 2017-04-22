TARGET = jpeg-test
TARGET_SO = libcedarJpeg.so
SRC = main.c jpeg.c enc_main.c veisp.c veavc.c vejpeg.c
CFLAGS = -Wall -Wextra -I../libvdpau-sunxi -DUSE_UMP -DINCLUDE_MAIN
CFLAGS_SO = -g -fPIC -Wall -Wextra -I../../libvdpau-sunxi -DUSE_UMP -fvisibility=hidden
LDFLAGS = 
LIBS=-lpthread -lUMP -lEGL -lGLESv2 -lcedar_access -ljpeg /usr/local/lib/libyuv.a
LDFLAGS_SO = -fPIC -shared
