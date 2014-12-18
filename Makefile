CC=gcc
CFLAGS = -Wall -ansi -O2 --pedantic -pthread -std=c99 -c
LDFLAGS = -Wall -ansi -O2 --pedantic -pthread

OBJECTS = err.o
ALL = serwer komisja raport

all: $(ALL)

%.o : %.c
	$(CC) $(CFLAGS) $<

$(ALL) : % : %.o $(OBJECTS)       
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o

