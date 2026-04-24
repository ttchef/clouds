
LIBS_DIR := libs
SRC_DIR := src
SHADER_DIR := $(SRC_DIR)/shaders
BUILD_DIR := build
SPV_DIR := $(BUILD_DIR)/spv

SANITIZE := #  -fsanitize=address

CC := gcc
CFLAGS := -Wall -Wextra -std=c23 -g $(SANITIZE) -I$(LIBS_DIR)
LDFLAGS := -lglfw -lvulkan -lstdc++ -lm

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

.PHONY: all clean folders shaders

all: folders shaders $(BUILD_DIR)/vma.o $(BUILD_DIR)/cgltf.o $(BUILD_DIR)/stbi.o $(BUILD_DIR)/fast_noise_lite.o $(OBJ_FILES)
	$(CC) $(SANITIZE) $(OBJ_FILES) \
	$(BUILD_DIR)/vma.o \
	$(BUILD_DIR)/cgltf.o \
	$(BUILD_DIR)/stbi.o \
	$(BUILD_DIR)/fast_noise_lite.o \
	-o $(BUILD_DIR)/main $(LDFLAGS)

folders:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(SPV_DIR)

shaders:
	@for file in $$(find $(SHADER_DIR)/* -maxdepth 2 -type f); do \
		if [ -f "$$file" ]; then \
			name=$$(basename $$file); \
			base=$${name%.*}; \
			ext=$${name##*.}; \
			if [ "$$ext" != "spv" ] && [ "$$ext" != "glsl" ]; then \
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

# stbi
$(BUILD_DIR)/stbi.o: $(LIBS_DIR)/stbi/stb_image.c
	gcc -O2 -c $(LIBS_DIR)/stbi/stb_image.c -o $(BUILD_DIR)/stbi.o
	
# FastNoiseLite
$(BUILD_DIR)/fast_noise_lite.o: $(LIBS_DIR)/FastNoiseLite/fast_noise_lite.c
	gcc -O2 -c $(LIBS_DIR)/FastNoiseLite/fast_noise_lite.c -o $(BUILD_DIR)/fast_noise_lite.o

clean:
	rm -rf build shaders/*.svp
