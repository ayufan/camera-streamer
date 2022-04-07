TARGET := camera_stream
SRC := $(wildcard **/*.c)
HEADERS := $(wildcard **/*.h)
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

$(TARGET): $(OBJS)
	gcc $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f .depend $(OBJS) $(HTML_SRC) $(TARGET)

-include $(OBJS:.o=.d)

%.o: %.c
	gcc -MMD $(CFLAGS) -c -o $@ $<

html/%.c: html/%
	xxd -i $< > $@.tmp
	mv $@.tmp $@
