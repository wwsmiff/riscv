CC=g++
CFLAGS=-Wall -Wextra -std=c++23 -g

main: main.o riscv.o
	g++ main.o riscv.o -o main

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

riscv.o: riscv.cpp riscv.hpp
	$(CC) $(CFLAGS) -c riscv.cpp

.PHONY: clean
clean:
	rm -f *.o main

.PHONY: run
run:
	./main
