FLAGS = -lraylib -lm

all: wave

wave: main.o wavfile.c
	gcc $(FLAGS) $^ -o $@

main.o: main.c
	gcc -c -g -Wall -Wextra $^

wavfile.o: wavfile.c
	gcc -c -O3 -Wall -Wextra $^

test_cmx: test_cmx.o
	gcc $(FLAGS) $^ -o $@

test_cmx.o: test_cmx.c
	gcc -c -g -Wall -Wextra $^
