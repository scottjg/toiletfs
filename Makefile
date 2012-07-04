PROGRAM=toiletfsd
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
	$(CC) $(LDFLAGS) -o $@ $^

test/potty-training: test/potty-training.c
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(SOURCES:.c=.o) $(PROGRAM)

