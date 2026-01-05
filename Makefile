COMPILEFLAGS = -O3 -Wall -Wextra
LINKFLAGS = -lraylib -lm

all: wave

wave: main.o wavfile.c
	gcc $(LINKFLAGS) $^ -o $@

main.o: main.c
	gcc -c $(COMPILEFLAGS) $^

wavfile.o: wavfile.c
	gcc -c $(COMPILEFLAGS) $^
