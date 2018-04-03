CC=gcc
CFLAGS=
INCLUDES=
LFLAGS=-L/usr/lib/x86_64-linux-gnu/
LIBS=-lcurl -lpthread

SRC=main.c
OBJ=$(SRC:.c=.o)
MAIN=hw

RM=rm -rf

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(MAIN): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJ) $(LFLAGS) $(LIBS)

all: $(MAIN)

clean:
	$(RM) $(MAIN) *.o *~
