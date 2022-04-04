camera_stream: *.c *.h
	gcc -g -lpthread -o camera_stream *.c

jmuxer.min.c: jmuxer.min.js
	xxd -i $< > $@.tmp
	mv $@.tmp $@

video.html.c: video.html
	xxd -i $< > $@.tmp
	mv $@.tmp $@
