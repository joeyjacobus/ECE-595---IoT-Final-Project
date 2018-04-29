CC=gcc
CFLAGS=
INCLUDES=
LFLAGS=-L/usr/lib/x86_64-linux-gnu/
LIBS=-lcurl -lpthread

SRC=main.c cJSON.c cJSON.h
OBJ=$(SRC:.c=.o)
MAIN=thermd

RM=rm -rf

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(MAIN): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJ) $(LFLAGS) $(LIBS)

all: $(MAIN)

clean:
	$(RM) $(MAIN) *.o *~
