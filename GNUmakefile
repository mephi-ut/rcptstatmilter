
LDFLAGS = -lm -lmilter $(shell pkg-config --libs sqlite3) -L/usr/lib/libmilter/
INCFLAGS = 
CFLAGS += -pipe -Wall -pedantic -O2 -fstack-protector-all $(shell pkg-config --cflags sqlite3)
DEBUGCFLAGS = -pipe -Wall -pedantic -Werror -ggdb -Wno-error=unused-variable -fstack-protector-all

objs=\
main.o\


all: $(objs)
	$(CC) $(CFLAGS) $(LDFLAGS) $(objs) -o rcpt-stat-milter

%.o: %.c
	$(CC) -std=gnu11 $(CFLAGS) $(INCFLAGS) $< -c -o $@

tools:
	$(CC) $(CFLAGS) $(LDFLAGS) -std=gnu11 rcpt-stat-milter-v2ip.c -o rcpt-stat-milter-v2ip
	$(CC) $(CFLAGS) $(LDFLAGS) -std=gnu11 rcpt-stat-milter-ip2v.c -o rcpt-stat-milter-ip2v

debug:
	$(CC) -std=gnu11 $(DEBUGCFLAGS) $(INCFLAGS) $(LDFLAGS) *.c -o rcpt-stat-milter-debug

clean:
	rm -f rcpt-stat-milter rcpt-stat-milter-debug $(objs)


