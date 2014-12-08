include config.mk


unjson: unjson.o
	$(CC) $? $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $?

.PHONY: clean install uninstall

clean:
	rm -f *.o unjson

install: unjson
	install -m 755 unjson $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/unjson
