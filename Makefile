
LIBS_DIR := libs
SRC_DIR := src
SHADER_DIR := $(SRC_DIR)/shaders
BUILD_DIR := build
SPV_DIR := $(BUILD_DIR)/spv

CC := gcc
CFLAGS := -Wall -Wextra -std=c23 -g -I$(LIBS_DIR)
LDFLAGS := -lglfw -lvulkan -lstdc++ -lm

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

.PHONY: all clean folders shaders

all: folders shaders $(BUILD_DIR)/vma.o $(BUILD_DIR)/cgltf.o $(OBJ_FILES)
	$(CC) $(OBJ_FILES) $(BUILD_DIR)/vma.o $(BUILD_DIR)/cgltf.o -o $(BUILD_DIR)/main $(LDFLAGS)

folders:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(SPV_DIR)

shaders:
	@for file in $$(find $(SHADER_DIR)/* -maxdepth 2 -type f); do \
		if [ -f "$$file" ]; then \
			name=$$(basename $$file); \
			base=$${name%.*}; \
			ext=$${name##*.}; \
			if [ $$ext != "spv" ]; then \
				glslc -fshader-stage=$$ext $$file -o $(SPV_DIR)/$$base-$$ext.spv; \
			fi\
		fi \
	done

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# VMA
$(BUILD_DIR)/vma.o: $(LIBS_DIR)/vma/vma.cpp
	g++ -O2 -c $(LIBS_DIR)/vma/vma.cpp -o $(BUILD_DIR)/vma.o

# CGLTF
$(BUILD_DIR)/cgltf.o: $(LIBS_DIR)/cgltf/cgltf.c
	gcc -O2 -c $(LIBS_DIR)/cgltf/cgltf.c -o $(BUILD_DIR)/cgltf.o

clean:
	rm -rf build shaders/*.svp
