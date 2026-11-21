CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra
INC      = -I.

CORE  = co.c swap.S
DEMOS = test/test1_gen test/test2_pool

all: $(DEMOS)

test/test1_gen: test/test1_gen.c $(CORE) co.h
	$(CC) $(CFLAGS) $(INC) -o $@ $< $(CORE)

test/test2_pool: test/test2_pool.c sched.c $(CORE) co.h sched.h
	$(CC) $(CFLAGS) $(INC) -o $@ $< sched.c $(CORE)

run: all
	./test/test1_gen
	./test/test2_pool

clean:
	rm -f $(DEMOS)

.PHONY: all run clean
