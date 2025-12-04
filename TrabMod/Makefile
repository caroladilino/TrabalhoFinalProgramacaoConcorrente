CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
OBJ = main.o controle.o fila.o globais.o
EXEC = voos

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c defs.h controle.h
	$(CC) $(CFLAGS) -c main.c

controle.o: controle.c controle.h defs.h globais.h fila.h
	$(CC) $(CFLAGS) -c controle.c

fila.o: fila.c fila.h defs.h
	$(CC) $(CFLAGS) -c fila.c

globais.o: globais.c globais.h defs.h
	$(CC) $(CFLAGS) -c globais.c

clean:
	rm -f *.o $(EXEC)

run: $(EXEC)
	./$(EXEC) 5 10