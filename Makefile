CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra
CORE     = co.c swap.S

DEMOS = test/test1_gen/test1_gen \
	    test/test2_pool/test2_pool \
	    test/test3_fetch/test3_fetch

all: $(DEMOS)

test/test1_gen/test1_gen: test/test1_gen/test1_gen.c $(CORE) co.h
	$(CC) $(CFLAGS) -I. -o $@ $< $(CORE)

test/test2_pool/test2_pool: test/test2_pool/test2_pool.c test/test2_pool/sched.c \
	                         $(CORE) co.h test/test2_pool/sched.h
	$(CC) $(CFLAGS) -I. -Itest/test2_pool -o $@ $< test/test2_pool/sched.c $(CORE)

test/test3_fetch/test3_fetch: test/test3_fetch/test3_fetch.c test/test3_fetch/loop.c \
	                          $(CORE) co.h test/test3_fetch/loop.h
	$(CC) $(CFLAGS) -I. -Itest/test3_fetch -o $@ $< test/test3_fetch/loop.c $(CORE)

run: all
	./test/test1_gen/test1_gen
	./test/test2_pool/test2_pool
	./test/test3_fetch/test3_fetch

clean:
	rm -f $(DEMOS)

.PHONY: all run clean
