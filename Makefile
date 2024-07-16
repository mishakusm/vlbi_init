#
# $FreeBSD$
#
# For multiple programs using a single source file each,
# we can just define 'progs' and create custom targets.
PROGS   =       pkt-gen

CLEANFILES = $(PROGS) *.o
MAN=

.include <bsd.prog.mk>
.include <bsd.lib.mk>

LDFLAGS += -lpthread -lnetmap
.ifdef WITHOUT_PCAP
CFLAGS += -DNO_PCAP
.else
LDFLAGS += -lpcap
.endif
LDFLAGS += -lm # used by nmreplay

CFLAGS += -Wno-cast-align

all: $(PROGS)

pkt-gen: pkt-gen.o
        $(CC) $(CFLAGS) -o pkt-gen pkt-gen.o $(LDFLAGS)






