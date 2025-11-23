all: clean link
link: compile 
	gcc -g -m32 -Wall -o hello hello.o
compile: hello.c
	gcc -m32 -g -Wall -c -o hello.o hello.c
clean:
	rm -f *.o hello
