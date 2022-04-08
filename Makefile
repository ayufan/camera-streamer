TARGET := camera_stream
SRC := $(wildcard **/*.c) $(wildcard **/*/*.c)
HEADERS := $(wildcard **/*.h) $(wildcard **/*/*.h)
HTML := $(wildcard html/*.js html/*.html)

CFLAGS := -Werror -g -I$(PWD)
LDLIBS := -lpthread

USE_FFMPEG ?= $(shell pkg-config libavutil libavformat libavcodec && echo 1)

ifeq (1,$(USE_FFMPEG))
CFLAGS += -DUSE_FFMPEG
LDLIBS += -lavcodec -lavformat -lavutil
endif

HTML_SRC = $(addsuffix .c,$(HTML))
OBJS = $(subst .c,.o,$(SRC) $(HTML_SRC))

.SUFFIXES:

all: $(TARGET)

%: cmd/%.c $(filter-out cmd/%, $(OBJS))
	gcc $(CFLAGS) -o $@ $^ $(LDLIBS)

install: $(TARGET)
	install $(TARGET) /usr/local/bin/

clean:
	rm -f .depend $(OBJS) $(OBJS:.o=.d) $(HTML_SRC) $(TARGET)

headers:
	find -name '*.h' | xargs -n1 gcc $(CFLAGS) -Wno-error -c -o /dev/null

-include $(OBJS:.o=.d)

%.o: %.c
	gcc -MMD $(CFLAGS) -c -o $@ $<

html/%.c: html/%
	xxd -i $< > $@.tmp
	mv $@.tmp $@
