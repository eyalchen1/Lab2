all: clean link
link: compile 
	gcc -g -m32 -Wall -o myshell LineParser.o myshell.o
	gcc -g -m32 -Wall -o looper LineParser.o Looper.o
compile: myshell.c LineParser.c Looper.c
	gcc -m32 -g -Wall -c -o myshell.o myshell.c
	gcc -m32 -g -Wall -c -o LineParser.o LineParser.c
	gcc -m32 -g -Wall -c -o Looper.o Looper.c
clean:
	rm -f *.o myshell
