# Author: Zixiu Su
# Date: 2020-10-22
# Email: szx917@gmail.com

PROJS   = HTTPTest
CC      = gcc
CFLAGS  = -g -Wall -Wextra -fsanitize=address -fsanitize=leak -static-libasan
LIBS    = -lm

all : $(PROJS)

HTTPTest : HTTPTest.c
	$(CC) $(CFLAGS) -o HTTPTest HTTPTest.c $(LIBS)

clean:
	rm -f *.o $(PROJS)
