TARGET = jpeg-test
TARGET_SO = libcedarJpeg.so
SRC = src/main.c src/jpeg.c src/enc_main.c src/veisp.c src/veavc.c src/vejpeg.c
CFLAGS ?= -Wall -Wextra -DUSE_UMP -DINCLUDE_MAIN -Isrc/
CFLAGS_SO ?= -g -fPIC -Wall -Wextra -DUSE_UMP -fvisibility=hidden -Isrc/
LDFLAGS ?= 
LIBS ?=  -lpthread -lUMP -lEGL -lGLESv2 -lcedar_access -lyuv -ljpeg
LDFLAGS_SO ?= -fPIC -shared
