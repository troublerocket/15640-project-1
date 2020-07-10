all: mylib.so server

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c

mylib.so: mylib.o
	gcc -Wall -fPIC -DPIC -c mylib.c -L../lib -ldirtree

clean:
	rm -f *.o *.so

