
LIBS_DIR := libs
SRC_DIR := src
BUILD_DIR := build

CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g -I$(LIBS_DIR)
LDFLAGS := -lglfw -lvulkan -lstdc++ -lm

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

.PHONY: all clean folders

all: folders $(BUILD_DIR)/vma.o $(OBJ_FILES)
	glslc -fshader-stage=vert shaders/vert.glsl -o shaders/vert.spv
	glslc -fshader-stage=frag shaders/frag.glsl -o shaders/frag.spv
	$(CC) $(OBJ_FILES) $(BUILD_DIR)/vma.o -o $(BUILD_DIR)/main $(LDFLAGS)

folders:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vma.o: $(LIBS_DIR)/vma/vma.cpp
	g++ -O2 -c $(LIBS_DIR)/vma/vma.cpp -o $(BUILD_DIR)/vma.o

clean:
	rm -rf build shaders/*.svp
