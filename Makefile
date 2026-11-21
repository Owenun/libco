CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra
LIB      = co.c swap.S
DEPS     = co.h

all: gen worker guard

gen: main.c $(LIB) $(DEPS)
	$(CC) $(CFLAGS) -o $@ main.c $(LIB)

worker: worker.c $(LIB) $(DEPS)
	$(CC) $(CFLAGS) -o $@ worker.c $(LIB)

guard: guard.c $(LIB) $(DEPS)
	$(CC) $(CFLAGS) -o $@ guard.c $(LIB)

run: gen
	./gen

clean:
	rm -f gen worker guard

.PHONY: all run clean
