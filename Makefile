TARGET := camera_stream
SRC := $(wildcard **/*.c **/*/*.c **/*.cc **/*/*.cc)
HEADERS := $(wildcard **/*.h **/*/*.h **/*.hh **/*/*.hh)
HTML := $(wildcard html/*.js html/*.html)

CFLAGS := -Werror -g -I$(PWD)
LDLIBS := -lpthread -lstdc++

ifneq (x,x$(shell which ccache))
CCACHE ?= ccache
endif

USE_FFMPEG ?= $(shell pkg-config libavutil libavformat libavcodec && echo 1)
USE_LIBCAMERA ?= $(shell pkg-config libcamera && echo 1)

ifeq (1,$(USE_FFMPEG))
CFLAGS += -DUSE_FFMPEG
LDLIBS += -lavcodec -lavformat -lavutil
endif

ifeq (1,$(USE_LIBCAMERA))
CFLAGS += -DUSE_LIBCAMERA $(shell pkg-config --cflags libcamera)
LDLIBS +=  $(shell pkg-config --cflags libs)
endif

HTML_SRC = $(addsuffix .c,$(HTML))
OBJS = $(patsubst %.cc,%.o,$(patsubst %.c,%.o,$(SRC) $(HTML_SRC)))

.SUFFIXES:

all: $(TARGET)

%: cmd/%.c $(filter-out cmd/%, $(OBJS))
	$(CCACHE) $(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

install: $(TARGET)
	install $(TARGET) /usr/local/bin/

clean:
	rm -f .depend $(OBJS) $(OBJS:.o=.d) $(HTML_SRC) $(TARGET)

headers:
	find -name '*.h' | xargs -n1 $(CCACHE) $(CC) $(CFLAGS) -Wno-error -c -o /dev/null

-include $(OBJS:.o=.d)

%.o: %.c
	$(CCACHE) $(CC) -MMD $(CFLAGS) -c -o $@ $<

%.o: %.cc
	$(CCACHE) $(CXX) -std=c++17 -MMD $(CFLAGS) -c -o $@ $<

html/%.c: html/%
	xxd -i $< > $@.tmp
	mv $@.tmp $@
