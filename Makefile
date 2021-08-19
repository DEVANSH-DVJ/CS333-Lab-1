all: shell

shell:
	gcc shell.c -o shell.o

clean:
	rm -rf *.o
