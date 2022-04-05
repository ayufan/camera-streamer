SRC := $(wildcard **/*.c)
HEADERS := $(wildcard **/*.h)
HTML := $(wildcard html/*.js html/*.html)
HTML_SRC := $(addsuffix .c,$(HTML))

camera_stream: $(SRC) $(HTML_SRC) $(HEADERS)
	gcc -Werror -g -I$(PWD) -o $@ $(filter %.c, $^) -lpthread

html/%.c: html/%
	xxd -i $< > $@.tmp
	mv $@.tmp $@
