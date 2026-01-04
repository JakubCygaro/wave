FLAGS = -lraylib -lm

all: wave

wave: main.o wavfile.c
	gcc $(FLAGS) $^ -o $@

main.o: main.c
	gcc -c -O3 -Wall -Wextra $^

wavfile.o: wavfile.c
	gcc -c -O3 -Wall -Wextra $^
