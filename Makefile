
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g
LDFLAGS := -lglfw -lvulkan -lm

SRC_FILES := main.c renderer.c log.c

all:
	glslc -fshader-stage=vert shaders/vert.glsl -o shaders/vert.spv
	glslc -fshader-stage=frag shaders/frag.glsl -o shaders/frag.spv
	$(CC) $(CFLAGS) $(SRC_FILES) -o main $(LDFLAGS)
