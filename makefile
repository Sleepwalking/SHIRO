#CROSS=i686-w64-mingw32-
CC = $(CROSS)gcc
LINK = $(CROSS)gcc -fopenmp -Wno-unused-function -Wno-unused-result
AR = $(CROSS)ar
CFLAGS = -DFP_TYPE=float -Ofast -g -mtune=native -std=c99 -Wall -fPIC -lm $(CFLAGSEXT)
ARFLAGS = -rv
OUT_DIR = ./build
OBJS = $(OUT_DIR)/ciglet.o $(OUT_DIR)/cJSON.o $(OUT_DIR)/liblrhsmm.a
LIBS = -lm -Lexternal/liblrhsmm/build -llrhsmm
TARGETS = shiro-mkhsmm shiro-init shiro-rest shiro-align shiro-untie \
  shiro-wav2raw shiro-xxcc

.PHONY: default clean distclean

default: $(TARGETS)

shiro-mkhsmm: shiro-mkhsmm.c cli-common.h $(OBJS)
	$(LINK) shiro-mkhsmm.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-mkhsmm

shiro-init: shiro-init.c cli-common.h $(OBJS)
	$(LINK) shiro-init.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-init

shiro-rest: shiro-rest.c cli-common.h $(OBJS)
	$(LINK) shiro-rest.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-rest

shiro-align: shiro-align.c cli-common.h $(OBJS)
	$(LINK) shiro-align.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-align

shiro-untie: shiro-untie.c cli-common.h $(OBJS)
	$(LINK) shiro-untie.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-untie

shiro-wav2raw: shiro-wav2raw.c $(OBJS)
	$(LINK) shiro-wav2raw.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-wav2raw

shiro-xxcc: shiro-xxcc.c $(OBJS)
	$(LINK) shiro-xxcc.c $(OBJS) $(CFLAGS) $(LIBS) -o shiro-xxcc

$(OUT_DIR)/ciglet.o:
	$(MAKE) -C external/ciglet single-file
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $(OUT_DIR)/ciglet.o -c external/ciglet/single-file/ciglet.c

$(OUT_DIR)/cJSON.o:
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $(OUT_DIR)/cJSON.o -c external/cJSON/cJSON.c

$(OUT_DIR)/liblrhsmm.a:
	@mkdir -p $(OUT_DIR)
	$(MAKE) -C external/liblrhsmm OUT_DIR=$(realpath $(OUT_DIR))

$(OUT_DIR)/%.o : %.c
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $(OUT_DIR)/$*.o -c $*.c

clean:
	@echo 'Removing all temporary binaries...'
	@rm -f $(OUT_DIR)/*.o $(TARGETS)
	@echo Done.

distclean:
	@rm -rf $(OUT_DIR) $(TARGETS)
