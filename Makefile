FLAGS = -lraylib -lm

all: wave

wave: main.o
	gcc $(FLAGS) $^ -o $@

main.o: main.c
	gcc -c -g -Wall -Wextra $^

test_cmx: test_cmx.o
	gcc $(FLAGS) $^ -o $@

test_cmx.o: test_cmx.c
	gcc -c -g -Wall -Wextra $^
