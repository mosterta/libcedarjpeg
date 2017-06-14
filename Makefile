include config.mk

CFLAGS += -g -MD -MP -MQ $@
MAKEFLAGS += -rR --no-print-directory

OBJ = $(addsuffix .o,$(basename $(SRC)))

OBJ_SO = $(addsuffix .lo,$(basename $(SRC)))

ifdef V
	ifeq ("$(origin V)", "command line")
		VERBOSE = $(V)
	endif
endif
ifndef VERBOSE
	VERBOSE = 0
endif

ifeq ($(VERBOSE), 1)
	Q =
else
	Q = @
endif

all: $(TARGET) $(TARGET_SO)

$(TARGET): $(OBJ)
	@echo "LD $(TARGET)"
	$(Q)g++ $(LDFLAGS) $(OBJ) $(LIBS) -o $@

$(TARGET_SO): $(OBJ_SO)
	@echo "LD $(TARGET_SO)"
	$(Q)g++ $(LDFLAGS_SO) $(OBJ_SO) $(LIBS) -o $@

install: $(TARGET_SO) 
	@echo "installing $(TARGET_SO) to /usr/lib"
	cp $(TARGET_SO) /usr/lib
	@echo "installing cedarJpegLib.h to /usr/include"
	cp cedarJpegLib.h /usr/include
	pcfile=$(mktemp)
	echo 'prefix=/usr' > $pcfile
	echo "exec_prefix=\$${prefix}" >> $pcfile
	echo "libdir=\$${prefix}/lib" >> $pcfile
	echo "includedir=\$${prefix}/include" >> $pcfile
	echo "" >> $pcfile
	echo "Name: cedarjpeg" >> $pcfile
	echo "Description: JPEG decoder library using the Allwinner Jpeg Hardware decoder" >>  $pcfile
	echo "Version: 1.0.0" >> $pcfile
	echo "Cflags: -I\$${includedir}" >> $pcfile
	echo "Libs: -L\$${libdir} -lcedarjpeg" >> $pcfile
	cp $pcfile /usr/lib/pkgconfig/cedarjpeg.pc
	rm $pcfile

uninstall:
	@echo "removing $(TARGET_SO)"
	rm -f /usr/lib/$(TARGET_SO) 2>&1 > /dev/null
	@echo "removing /usr/include/cedarJpegLib.h"
	rm -f /usr/include/cedarJpegLib.h 2>&1 > /dev/null

.PHONY: clean
clean:
	@echo "RM *.o"
	$(Q)rm -f *.o
	@echo "RM *.d"
	$(Q)rm -f *.d
	@echo "RM $(TARGET)"
	$(Q)rm -f $(TARGET)
	@echo "RM *.lo"
	$(Q)rm -f *.lo
	@echo "RM *.d"
	$(Q)rm -f *.d
	@echo "RM $(TARGET_SO)"
	$(Q)rm -f $(TARGET_SO)

%.o: %.c config.mk
	@echo "CC $<"
	$(Q)gcc $(CFLAGS) -c $< -o $@

%.lo: %.c config.mk
	@echo "CC $<"
	$(Q)gcc $(CFLAGS_SO) -c $< -o $@

include $(wildcard $(foreach f,$(OBJ),$(basename $(f)).d))
include $(wildcard $(foreach f,$(OBJ_SO),$(basename $(f)).d))

