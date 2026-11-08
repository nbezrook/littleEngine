CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c11 -O2 -I./include
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -I./include

.PHONY: all clean

all:
	$(CC) $(CFLAGS) -c engine.c renderer.c console.c entities.c math/primitives.c camera.c clipping.c world.c server.c map.c
	$(CXX) $(CXXFLAGS) -c main.cpp assetsThinker.cpp inputThinker.cpp client.cpp 
	$(CXX) main.o engine.o renderer.o console.o entities.o map.o primitives.o world.o camera.o assetsThinker.o inputThinker.o server.o clipping.o client.o SDL3.dll -o engine.exe -mwindows
clean:
	del main.o engine.o client.o renderer.o console.o map.o entities.o primitives.o world.o camera.o assetsThinker.o inputThinker.o clipping.o server.o engine.exe