CC=g++
CFLAGS=-Wall -Wextra -std=c++23 -g

main: main.o core.o
	g++ main.o core.o -o main

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

core.o: core.cpp core.hpp platform.hpp disassemble.hpp decode.hpp
	$(CC) $(CFLAGS) -c core.cpp

.PHONY: clean
clean:
	rm -f *.o main

.PHONY: run
run:
	./main
