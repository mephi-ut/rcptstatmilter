
LDFLAGS = -lmilter -L/usr/lib/libmilter/
INCFLAGS = 
CFLAGS += -pipe -Wall -pedantic -O2 -fstack-protector-all
DEBUGCFLAGS = -pipe -Wall -pedantic -Werror -ggdb -Wno-error=unused-variable -fstack-protector-all

objs=\
main.o\


all: $(objs)
	$(CC) $(CFLAGS) $(LDFLAGS) $(objs) -o rcpt-stat-milter

%.o: %.c
	$(CC) -std=gnu11 $(CFLAGS) $(INCFLAGS) $< -c -o $@

debug:
	$(CC) -std=gnu11 $(DEBUGCFLAGS) $(INCFLAGS) $(LDFLAGS) *.c -o rcpt-stat-milter-debug

clean:
	rm -f rcpt-stat-milter rcpt-stat-milter-debug $(objs)


