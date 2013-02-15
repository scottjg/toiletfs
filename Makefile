PROGRAM=toiletfs
SOURCES := $(wildcard *.c)
CFLAGS=-Wall -g `pkg-config fuse --cflags`
LDFLAGS=`pkg-config fuse --libs`

.SUFFIXES:
.PHONY: all test clean
all: $(PROGRAM)
test: test/potty-training

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(PROGRAM): $(SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)

test/potty-training: test/potty-training.c
	$(CC) -g $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(SOURCES:.c=.o) $(PROGRAM)

install:
	install -m 755 toiletfs -D $(DESTDIR)/sbin/toiletfs
	ln -s toiletfs $(DESTDIR)/sbin/mount.toiletfs
