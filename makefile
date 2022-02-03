.PHONY: install

all: init

init: init.c
	$(CC) -o init init.c

install: init
	cp -f shutdown reboot halt $(BIN)
	cp -f init $(SBIN)

clean:
	rm init
