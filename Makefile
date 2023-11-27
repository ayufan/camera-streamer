TARGET := camera-streamer
SRC := $(wildcard **/*.c **/*/*.c **/*.cc **/*/*.cc)
HEADERS := $(wildcard **/*.h **/*/*.h **/*.hh **/*/*.hh)
HTML := $(wildcard html/*.js html/*.html)

GIT_VERSION ?= $(shell git describe --tags)
GIT_REVISION ?= $(shell git rev-parse --short HEAD)

CFLAGS := -Werror -Wall -g -I$(CURDIR) -D_GNU_SOURCE
LDLIBS := -lpthread -lstdc++

# libdatachannel deprecations on bookworm
# error: 'HMAC_Init_ex' is deprecated: Since OpenSSL 3.0
CFLAGS += -Wno-error=deprecated-declarations

ifneq (x,x$(shell which ccache))
CCACHE ?= ccache
endif

LIBDATACHANNEL_PATH ?= third_party/libdatachannel

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

.SUFFIXES:

ifeq (1,$(USE_LIBDATACHANNEL))
camera-streamer: $(LIBDATACHANNEL_PATH)/build/libdatachannel-static.a
endif

.PHONY: version
version:
	echo "#define GIT_VERSION \"$(GIT_VERSION)\"\n#define GIT_REVISION \"$(GIT_REVISION)\"" > version.h.tmp
	if $(CXX) $(CFLAGS) -o /dev/null -c tests/libcamera/orientation.cc 2>/dev/null; then \
		echo "#define LIBCAMERA_USES_ORIENTATION" >> version.h.tmp; \
	else \
		echo "#define LIBCAMERA_USES_TRANSFORM" >> version.h.tmp; \
	fi
	diff -u version.h version.h.tmp || mv version.h.tmp version.h
	-rm -f version.h.tmp

%: cmd/% $(TARGET_OBJS)
	$(CCACHE) $(CXX) $(CFLAGS) -o $@ $(filter-out cmd/%, $^) $(filter $</%, $^) $(LDLIBS)

install: $(TARGET)
	install $(TARGET) $(DESTDIR)/usr/local/bin/

clean:
	rm -f .depend $(OBJS) $(OBJS:.o=.d) $(HTML_SRC) $(TARGET)

headers:
	find -name '*.h' | xargs -n1 $(CCACHE) $(CC) $(CFLAGS) -std=gnu17 -Wno-error -c -o /dev/null
	find -name '*.hh' | xargs -n1 $(CCACHE) $(CXX) $(CFLAGS) -std=c++17 -Wno-error -c -o /dev/null

-include $(OBJS:.o=.d)

%.o: %.c
	$(CCACHE) $(CC) -std=gnu17 -MMD $(CFLAGS) -c -o $@ $<

%.o: %.cc
	$(CCACHE) $(CXX) -std=c++17 -MMD $(CFLAGS) -c -o $@ $<

html/%.c: html/%
	xxd -i $< > $@.tmp
	mv $@.tmp $@

$(LIBDATACHANNEL_PATH)/build/libdatachannel-static.a: $(LIBDATACHANNEL_PATH)
	[ -e $</build/Makefile ] || cmake -S $< -B $</build
	$(MAKE) -C $</build datachannel-static
