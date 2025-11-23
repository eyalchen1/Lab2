all: clean link
link: compile 
	gcc -g -m32 -Wall -o myshell myshell.o LineParser.o
compile: myshell.c LineParser.c
	gcc -m32 -g -Wall -c -o myshell.o myshell.c
	gcc -m32 -g -Wall -c -o LineParser.o LineParser.c
clean:
	rm -f *.o myshell
