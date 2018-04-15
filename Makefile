#	MAKEFILE

CFLAGS = -g -Wall -Wshadow -o

GCC = gcc $(CFLAGS)

all: oss user

oss: oss.c
	$(GCC) $(CFLAGS) oss oss.c

user: user.c
	$(GCC) $(CFLAGS) user user.c

clean:
	rm -f *.o oss user

