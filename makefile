CC = clang

CCFLAGS = -std=c2x -Wall -Wpedantic -g -O2 -v
INCFLAGS  = -I./src
INCFLAGS += -I./lib/SDL/include

_SDL_BUILD_DIR = .build
SDL_LIB = ./lib/SDL/$(_SDL_BUILD_DIR)

LDFLAGS = -L$(SDL_LIB) -lSDL3 -lSDL3_test -lm

SRC = ./src/main.c

all:
	$(CC) $(CCFLAGS) $(INCFLAGS) -c $(SRC) -o ./src/main.o
	$(CC) ./src/main.o $(LDFLAGS) -o ./bin/main
	./bin/main

lib_sdl:
	cd ./lib/SDL && cmake -S . -B $(_SDL_BUILD_DIR)
	cd ./lib/SDL/$(_SDL_BUILD_DIR) && make

clean:
	rm -f ./bin/main ./src/main.o
