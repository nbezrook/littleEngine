SHELL := cmd.exe
.SHELLFLAGS := /c
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CC := gcc
CXX := g++

CPPFLAGS := -I./include
CFLAGS := -Wall -Wextra -std=c11 -O3 -g -flto
CXXFLAGS := -Wall -Wextra -std=c++17 -O3 -g -flto
LDFLAGS := -mwindows -O3 -flto
LDLIBS := SDL3.dll

C_SOURCES := $(wildcard *.c math/*.c)
CXX_SOURCES := $(wildcard *.cpp math/*.cpp)

OBJECTS := $(addprefix build/,$(addsuffix .o,$(C_SOURCES) $(CXX_SOURCES)))
DEPS := $(OBJECTS:.o=.d)
OBJECT_DIRS := $(sort $(dir $(OBJECTS)))

.PHONY: all clean prepare
all: engine.exe

engine.exe: $(OBJECTS) SDL3.dll Makefile
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o "$@"

prepare:
	@for %%D in ($(subst /,\,$(OBJECT_DIRS))) do @if not exist "%%D" mkdir "%%D"

$(OBJECTS): Makefile | prepare

build/%.c.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"

build/%.cpp.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

clean:
	@if exist build rmdir /s /q build
	@if exist engine.exe del /q engine.exe

-include $(DEPS)