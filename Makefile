INC=-Iinclude
CFLAGS=$(INC)

PARSERDIR=input_parse

SRCS=utils.c mshell.c builtins.c signal_utils.c redirect_utils.c read_utils.c pipe_utils.c
OBJS:=$(SRCS:.c=.o)

all: mshell 

mshell: $(OBJS) siparse.a
	cc $(CFLAGS) $(OBJS) siparse.a -o $@ 

%.o: %.c
	cc $(CFLAGS) -c $<

siparse.a:
	$(MAKE) -C $(PARSERDIR)

clean:
	rm -f mshell *.o
