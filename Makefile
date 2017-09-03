# Este Makefile define los parametros de
# compilacion (CFLAGS) y reglas de compilacion
# para cada archivo objeto y el ejecutable final
# (bank).
CFLAGS=-g -std=c11 -Wall -Werror -D _DEFAULT_SOURCE -pthread

all: bank

# Si se agregan archivos de codigo .c
# para cada uno se debe agregar una regla
# como las siguientes:
main: main.c
	$(CC) $(CFLAGS) -o $@ $^

# Esta regla crea el ejecutable final.
# se pueden anyadir los archivos .o
# a continuacion de main.o
bank: main.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f bank *.o
