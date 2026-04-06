
LIBS_DIR := libs
BUILD_DIR := build

CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g -I$(LIBS_DIR)
LDFLAGS := -lglfw -lvulkan -lm

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(SRC_FILES:.c=.o)

.PHONY: all clean folders

all: folders $(BUILD_DIR)/vma.o $(OBJ_FILES)
	glslc -fshader-stage=vert shaders/vert.glsl -o shaders/vert.spv
	glslc -fshader-stage=frag shaders/frag.glsl -o shaders/frag.spv
	$(CC) $(OBJ_FILES) -o $(BUILD_DIR)/main $(LDFLAGS)

folders:
	mkdir -p $(BUILD_DIR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vma.o: $(LIBS_DIR)/vma/vma.cpp
	g++ -O2 -c $(LIBS_DIR)/vma/vma.cpp -o $(BUILD_DIR)/vma.o

clean:
	rm -rf main *.o shaders/*.svp
