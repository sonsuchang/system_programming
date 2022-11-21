SRCS = static_main.c
DYMC = dynamic_main.c

LIBSRCS = open.c close.c put.c get.c seek.c

TARG = kvs_static
TARGD = kvs_dynamic

CC = gcc
OPTS = -Wall -O -g
LIBS = -L. -lkvs

OBJS = $(SRCS:.c=.o)
DYOBJ = $(DYMC:.c=.o)
LOBJS = $(LIBSRCS:.c=.o)

LARS = libkvs.a
DYSO = libkvs.so

all: static dynamic
       export LD_LIBRARY_PATHY=$(LD_LIBRARY_PATH)

static: $(TARG)

dynamic: $(TARGD)

$(TARG) : $(OBJS) $(LARS)
	$(CC) -o $(TARG) $(OBJS) $(LIBS)

$(TARGD) : $(DYOBJ) $(DYSO)
	$(CC) -fPIC -c $(LIBSRCS)
	$(CC) -o $(TARGD) $(DYOBJ) -ldl

$(DYSO) : $(LOBJS)
	$(CC) -shared -o $(DYSO) $(LOBJS)

$(LARS) : $(LOBJS)
	ar rs $(LARS) $(LOBJS)

%.o: %.c
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LOBJS) $(LARS) $(TARG) $(TARGD) $(DYOBJ) $(DYSO)

