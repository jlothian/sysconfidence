ifdef PLATFORM
include make.inc.$(PLATFORM)
else
include make.inc
endif

HDRS     = config.h measurement.h orbtimer.h types.h tests.h copyright.h comm.h options.h
OBJS     = measurement.o orbtimer.o comm.o net_test.o options.o sysconfidence.o bit_test.o $(XDD_OBJS)

sysconfidence: $(XDD_LIBS) $(OBJS) $(XDD_TARGETS) 
	$(CC) $(CFLAGS) -o sysconfidence $(OBJS) $(LIBS)

read_xdd: read_xdd_dump.c $(shell find xdd -name xdd.h)
	$(GCC) $(CFLAGS) -o read_xdd read_xdd_dump.c

libxdd.a: $(shell find xdd -name xdd.c)
	./scripts/build_xdd.sh

sysconfidence.o: sysconfidence.c $(HDRS)
measurement.o:   measurement.c   $(HDRS)
options.o:       options.c       $(HDRS)
comm.o:          comm.c          $(HDRS)
orbtimer.o:      orbtimer.c      $(HDRS)
net_test.o:      net_test.c      $(HDRS)
bit_test.o:      bit_test.c      $(HDRS)
io_test.o:       io_test.c       $(HDRS)

config.h:
	echo No config.h
	echo you need to run scripts/config.sh 
	scripts/config.sh

clean: 
	rm -f *.o *.a sysconfidence read_xdd libxdd.a

distclean:
	make clean
	rm -f config.h
