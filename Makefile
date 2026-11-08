CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c11 -O2 -g -I./include
CXXFLAGS = -Wall -Wextra -std=c++17 -g -O2 -I./include

.PHONY: all clean

all:
	$(CC) $(CFLAGS) -c math/math.c engine.c texture.c renderer.c depth_buffer.c console.c entities.c math/primitives.c camera.c clipping.c world.c server.c map.c physics.c
	$(CXX) $(CXXFLAGS) -c main.cpp assetsThinker.cpp inputThinker.cpp client.cpp 
	$(CXX) main.o math.o engine.o renderer.o depth_buffer.o texture.o console.o entities.o map.o primitives.o world.o camera.o assetsThinker.o inputThinker.o server.o clipping.o client.o physics.o SDL3.dll -o engine.exe -mwindows
clean:
	del main.o math.o engine.o client.o depth_buffer.o renderer.o texture.o console.o map.o entities.o primitives.o world.o camera.o assetsThinker.o inputThinker.o clipping.o server.o physics.o engine.exe