TARGET := camera-streamer
SRC := $(wildcard **/*.c **/*/*.c **/*.cc **/*/*.cc)
HEADERS := $(wildcard **/*.h **/*/*.h **/*.hh **/*/*.hh)
HTML := $(wildcard html/*.js html/*.html)

GIT_VERSION ?= $(shell git describe --tags)
GIT_REVISION ?= $(shell git rev-parse --short HEAD)

CFLAGS := -Werror -Wall -g -I$(CURDIR) -D_GNU_SOURCE
LDLIBS := -lpthread -lstdc++

# Print #warnings
CFLAGS += -Wno-error=cpp

# LOG_*(this, ...)
CFLAGS += -Wno-error=nonnull-compare
CFLAGS += -Wno-nonnull-compare

# libdatachannel deprecations on bookworm
# error: 'HMAC_Init_ex' is deprecated: Since OpenSSL 3.0
CFLAGS += -Wno-error=deprecated-declarations

ifneq (x,x$(shell which ccache))
CCACHE ?= ccache
endif

LIBDATACHANNEL_PATH ?= third_party/libdatachannel
LIBDATACHANNEL_VERSION ?= $(LIBDATACHANNEL_PATH)/v0.23.222529eb2c8ae44

USE_HW_H264 ?= 1
USE_FFMPEG ?= $(shell pkg-config libavutil libavformat libavcodec && echo 1)
USE_LIBCAMERA ?= $(shell pkg-config libcamera && echo 1)
USE_RTSP ?= $(shell pkg-config live555 && echo 1)
USE_LIBDATACHANNEL ?= $(shell [ -e $(LIBDATACHANNEL_PATH)/CMakeLists.txt ] && echo 1)

ifeq (1,$(DEBUG))
CFLAGS += -g
endif

ifeq (1,$(USE_HW_H264))
CFLAGS += -DUSE_HW_H264
endif

ifeq (1,$(USE_FFMPEG))
CFLAGS += -DUSE_FFMPEG
LDLIBS += -lavcodec -lavformat -lavutil
endif

ifeq (1,$(USE_LIBCAMERA))
CFLAGS += -DUSE_LIBCAMERA $(shell pkg-config --cflags libcamera)
LDLIBS += $(shell pkg-config --libs libcamera)
endif

ifeq (1,$(USE_RTSP))
CFLAGS += -DUSE_RTSP $(shell pkg-config --cflags live555)
LDLIBS += $(shell pkg-config --libs live555)
endif

ifeq (1,$(USE_LIBDATACHANNEL))
CFLAGS += -DUSE_LIBDATACHANNEL
CFLAGS += -I$(LIBDATACHANNEL_PATH)/include
CFLAGS += -I$(LIBDATACHANNEL_PATH)/deps/json/include
LDLIBS += -L$(LIBDATACHANNEL_PATH)/build -ldatachannel-static
LDLIBS += -L$(LIBDATACHANNEL_PATH)/build/deps/usrsctp/usrsctplib -lusrsctp
LDLIBS += -L$(LIBDATACHANNEL_PATH)/build/deps/libsrtp -lsrtp2
LDLIBS += -L$(LIBDATACHANNEL_PATH)/build/deps/libjuice -ljuice-static
LDLIBS += -lcrypto -lssl
endif

HTML_SRC = $(addsuffix .c,$(HTML))
OBJS = $(patsubst %.cc,%.o,$(patsubst %.c,%.o,$(SRC) $(HTML_SRC)))
TARGET_OBJS = $(filter-out third_party/%, $(filter-out tests/%, $(OBJS)))

all: version
	+make $(TARGET)

install: version
	+make $(TARGET)
	install $(TARGET) $(DESTDIR)/usr/local/bin/

.SUFFIXES:

ifeq (1,$(USE_LIBDATACHANNEL))
camera-streamer: $(LIBDATACHANNEL_PATH)/build/libdatachannel-static.a
endif

camera-streamer: $(filter-out cmd/%, $(TARGET_OBJS)) $(filter cmd/camera-streamer/%, $(TARGET_OBJS))
	$(CCACHE) $(CXX) $(CFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: version
version:
	printf "#define GIT_VERSION \"$(GIT_VERSION)\"\n#define GIT_REVISION \"$(GIT_REVISION)\"\n" > version.h.tmp
	if $(CCACHE) $(CXX) $(CFLAGS) -o tests/libcamera/orientation.o -c tests/libcamera/orientation.cc 2>/dev/null; then \
		printf "#define LIBCAMERA_USES_ORIENTATION\n" >> version.h.tmp; \
	else \
		printf "#define LIBCAMERA_USES_TRANSFORM\n" >> version.h.tmp; \
	fi
	diff -u version.h version.h.tmp || mv version.h.tmp version.h
	-rm -f version.h.tmp

clean:
	rm -f .depend $(OBJS) $(OBJS:.o=.d) $(HTML_SRC) $(TARGET) version.h

headers:
	find -name '*.h' | xargs -n1 $(CCACHE) $(CC) $(CFLAGS) -std=gnu17 -Wno-error -c -o /dev/null
	find -name '*.hh' | xargs -n1 $(CCACHE) $(CXX) $(CFLAGS) -std=c++17 -Wno-error -c -o /dev/null

-include $(OBJS:.o=.d)

%.o: %.c
	$(CCACHE) $(CC) -std=gnu17 -MMD $(CFLAGS) -c -o $@ $<

%.o: %.cc
	$(CCACHE) $(CXX) -std=c++17 -MMD $(CFLAGS) -c -o $@ $<

.PRECIOUS: html/%.c
html/%.c: html/%
	xxd -i $< > $@.tmp
	mv $@.tmp $@

$(LIBDATACHANNEL_VERSION):

$(LIBDATACHANNEL_PATH)/build/libdatachannel-static.a: $(LIBDATACHANNEL_PATH) $(LIBDATACHANNEL_VERSION)
	git submodule update --init --recursive $(LIBDATACHANNEL_PATH)
	[ -e $</build/Makefile ] || cmake -S $< -B $</build
	$(MAKE) -C $</build datachannel-static
	touch $(LIBDATACHANNEL_VERSION) $@
