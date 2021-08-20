all: main debug

main:
	gcc shell.c -o shell.o

debug:
	gcc debug.c -o debug.o

clean:
	rm -rf *.o
