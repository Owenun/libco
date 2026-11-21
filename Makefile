CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra

CORE = co.c swap.S
GEN  = main.c   $(CORE)
POOL = pool.c sched.c $(CORE)

all: gen pool

gen:  $(GEN) co.h
	$(CC) $(CFLAGS) -o $@ $(GEN)

pool: $(POOL) co.h sched.h
	$(CC) $(CFLAGS) -o $@ $(POOL)

run: all
	./gen
	./pool

clean:
	rm -f gen pool

.PHONY: all run clean
