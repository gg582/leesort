CC      ?= cc
CFLAGS  ?= -O3 -march=native -fomit-frame-pointer
CFLAGS  += -std=c11 -Wall -Wextra -Iinclude
PREFIX  ?= /usr/local

SRCS    = src/leesort.c src/leesort_bench.c
HDRS    = include/leesort.h include/leesort_bench.h
OBJS    = $(SRCS:src/%.c=build/%.o)
PICOBJS = $(SRCS:src/%.c=build/%.pic.o)
LIB_A   = build/libleesort.a
LIB_SO  = build/libleesort.so

.PHONY: all lib test bench clean install

all: lib build/test build/leesort-bench
lib: $(LIB_A) $(LIB_SO)

build:
	mkdir -p build

build/%.o: src/%.c $(HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.pic.o: src/%.c $(HDRS) | build
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(LIB_A): $(OBJS)
	ar rcs $@ $^
$(LIB_SO): $(PICOBJS)
	$(CC) -shared -o $@ $^

build/test: tests/test.c $(LIB_A)
	$(CC) $(CFLAGS) $< $(LIB_A) -o $@
build/leesort-bench: bench/main.c $(LIB_A)
	$(CC) $(CFLAGS) $< $(LIB_A) -o $@

test: build/test
	./build/test
bench: build/leesort-bench
	./build/leesort-bench $(N)

install: all
	install -d $(PREFIX)/include $(PREFIX)/lib $(PREFIX)/bin
	install -m 644 $(HDRS) $(PREFIX)/include/
	install -m 644 $(LIB_A) $(LIB_SO) $(PREFIX)/lib/
	install -m 755 build/leesort-bench $(PREFIX)/bin/

clean:
	rm -rf build

# ---- comparison against third-party sorts (not part of the library) ----
CXX      ?= c++
TP        = third_party
TP_INC    = -I$(TP)/pdqsort -I$(TP)/ska_sort -I$(TP)/x86-simd-sort/src -I$(TP)/sort

.PHONY: compare
build/compare_tim.o: bench/compare_tim.c | build
	$(CC) -O3 -march=native -w -I$(TP)/sort -c $< -o $@
build/leesort-compare: bench/compare.cpp build/compare_tim.o $(LIB_A)
	$(CXX) -O3 -march=native -fomit-frame-pointer -std=c++17 -w -Iinclude $(TP_INC) $^ -o $@
compare: build/leesort-compare
	./build/leesort-compare $(N)
