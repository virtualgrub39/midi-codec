CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -std=c89
CFLAGS += -ggdb

CFLAGS += -I..

all: example-reading example-writing

example-reading: reading.c
	$(CC) -o $@ $(CFLAGS) $^

example-writing: writing.c
	$(CC) -o $@ $(CFLAGS) $^