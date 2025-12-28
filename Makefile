LF = -lraylib

all: wave

wave: main.o
	gcc $(LF) $^ -o $@

main.o: main.c
	gcc -c -g -Wall -Wextra $^
