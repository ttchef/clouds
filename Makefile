
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g
LDFLAGS := -lglfw -lvulkan -lm

SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:.c=.o)

.PHONY: all clean

all: vma.o $(OBJ_FILES)
	glslc -fshader-stage=vert shaders/vert.glsl -o shaders/vert.spv
	glslc -fshader-stage=frag shaders/frag.glsl -o shaders/frag.spv
	$(CC) $(OBJ_FILES) -o main $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

vma.o: vma.cpp
	g++ -O2 -c vma.cpp -o vma.o

clean:
	rm -rf main *.o shaders/*.svp
