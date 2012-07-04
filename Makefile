PROGRAM=toiletfsd
SOURCES := $(wildcard *.c)
CFLAGS=-Wall `pkg-config fuse --cflags`
LDFLAGS=`pkg-config fuse --libs`

.SUFFIXES:
.PHONY: all clean
all: $(PROGRAM)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(PROGRAM): $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(SOURCES:.c=.o) $(PROGRAM)

