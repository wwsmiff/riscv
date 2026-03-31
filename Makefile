CC=g++
CFLAGS=-Wall -Wextra -std=c++23 -g

main: main.o
	g++ main.o -o main

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

.PHONY: clean
clean:
	rm -f main.o main

.PHONY: run
run:
	./main
