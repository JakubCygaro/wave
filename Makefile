FLAGS = -lraylib -lm

all: wave

wave: main.o
	gcc $(FLAGS) $^ -o $@

main.o: main.c
	gcc -c -g -Wall -Wextra $^
