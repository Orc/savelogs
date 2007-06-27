#
# makefile for savelogs
#
LEX	= flex -li

OBJECTS	= savelogs.o lex.yy.o

target: savelogs

savelogs: $(OBJECTS) tokens.h
	$(CC) $(CFLAGS) -o savelogs $(OBJECTS)

lex.yy.o: scan.l tokens.h
	$(LEX) scan.l
	$(CC) $(CFLAGS) -c lex.yy.c

savelogs.o: savelogs.c tokens.h

clean:
	rm -f savelogs *.o lex.yy.c DEMO?
	rm -rf OLD

demo: savelogs
	-mkdir OLD
	sed -e "s~@HERE@~`pwd`~" < demo.conf.sh > demo.conf
	for file in DEMO1 DEMO2 DEMO3 DEMO4 DEMO5; do \
	    dd if=/dev/zero of=$$file count=201 bs=1024; \
	done
	./savelogs -dddddC demo.conf

install:
	install -d -m 744 -o root -g root $(ROOT)/usr/man/man8
	install -d -m 755 -o root -g root $(ROOT)/usr/sbin
	install -c -m 511 -o root -g root -s savelogs $(ROOT)/usr/sbin
	install -c -m 444 savelogs.8 $(ROOT)/usr/man/man8
	if [ ! -r $(ROOT)/etc/savelogs.conf ]; then \
	    install -d -m 755 -o root -g root $(ROOT)/etc; \
	    install -c -m 400 -o root -g root log.conf $(ROOT)/etc/savelogs.conf; \
	fi
