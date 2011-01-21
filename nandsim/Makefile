
CFLAGS =    -Wall -g $(EXTRA_COMPILE_FLAGS) -Werror=strict-aliasing 
CFLAGS +=    -O0

CFLAGS+=   -Wshadow -Werror=pointer-arith -Werror=write-strings
CFLAGS+=   -Werror=strict-prototypes -Werror=missing-parameter-type
CFLAGS+=   -Werror=redundant-decls -Werror=nested-externs -Winline
CFLAGS+=   -Werror=undef

NANDSIMOBJS = nandsim.o nanddrv.o
SIMTESTOBJS = nandsim_test.o

ALLOBJS = $(sort $(NANDSIMOBJS) $(SIMTESTOBJS))

TARGETS = nandsim_test

all: $(TARGETS)

$(ALLOBJS): %.o: %.c
	gcc -c $(CFLAGS) -o $@ $<



nandsim_test: $(ALLOBJS)
	gcc -o $@ $^


clean:
	rm -f $(TARGETS) $(ALLOBJS) core core $(SYMLINKS)
