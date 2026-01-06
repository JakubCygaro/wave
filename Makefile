COMPILEFLAGS = -g -Wall -Wextra
LINKFLAGS = -lraylib -lm

all: wave

wave: main.o wavfile.o
	gcc $^ $(LINKFLAGS) -o $@

main.o: main.c
	gcc -c $(COMPILEFLAGS) $^

wavfile.o: wavfile.c
	gcc -c $(COMPILEFLAGS) $^
