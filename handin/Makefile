all: mylib.so server

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c -I../include -L../lib mylib.c  -ldirtree

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

server: 
	gcc -o server server.c -I../include -L../lib -ldirtree

clean:
	rm -f *.o *.so server