CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c11 -O2 -I./include
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -I./include

.PHONY: all clean

all:
	$(CC) $(CFLAGS) -c engine.c renderer.c console.c entities.c math/primitives.c camera.c
	$(CXX) $(CXXFLAGS) -c main.cpp
	$(CXX) main.o engine.o renderer.o console.o entities.o primitives.o camera.o SDL3.dll -o engine.exe -mwindows

clean:
	del main.o engine.o renderer.o console.o entities.o primitives.o camera.o engine.exe