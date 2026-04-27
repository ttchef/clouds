
LIBS_DIR := libs
SRC_DIR := src
SHADER_DIR := $(SRC_DIR)/shaders
BUILD_DIR := build
SPV_DIR := $(BUILD_DIR)/spv
OBJ_DIR := $(BUILD_DIR)/obj
LIBS_OBJ_DIR := $(BUILD_DIR)/libs

SANITIZE := #-fsanitize=address

CC := gcc
CFLAGS := -Wall -Wextra -std=c23 -g $(SANITIZE) -I$(LIBS_DIR) -I$(SRC_DIR)
LDFLAGS := -lglfw -lvulkan -lshaderc_shared -lstdc++ -lm

SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c') 
OBJ_FILES := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))

LIB_OBJ_FILES := \
 	$(LIBS_OBJ_DIR)/vma.o \
	$(LIBS_OBJ_DIR)/cgltf.o \
	$(LIBS_OBJ_DIR)/stbi.o \
	$(LIBS_OBJ_DIR)/fast_noise_lite.o \

.PHONY: all clean folders shaders full_clean

all: folders shaders $(LIBS_OBJ_DIR)/vma.o $(LIBS_OBJ_DIR)/cgltf.o $(LIBS_OBJ_DIR)/stbi.o $(LIBS_OBJ_DIR)/fast_noise_lite.o $(OBJ_FILES)
	$(CC) $(SANITIZE) $(OBJ_FILES) \
	$(LIBS_OBJ_DIR)/vma.o \
	$(LIBS_OBJ_DIR)/cgltf.o \
	$(LIBS_OBJ_DIR)/stbi.o \
	$(LIBS_OBJ_DIR)/fast_noise_lite.o \
	-o $(BUILD_DIR)/main $(LDFLAGS)

folders:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/vk
	@mkdir -p $(LIBS_OBJ_DIR)
	@mkdir -p $(SPV_DIR)

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

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# VMA
$(LIBS_OBJ_DIR)/vma.o: $(LIBS_DIR)/vma/vma.cpp
	@g++ -O2 -c $(LIBS_DIR)/vma/vma.cpp -o $(LIBS_OBJ_DIR)/vma.o

# CGLTF
$(LIBS_OBJ_DIR)/cgltf.o: $(LIBS_DIR)/cgltf/cgltf.c
	@gcc -O2 -c $(LIBS_DIR)/cgltf/cgltf.c -o $(LIBS_OBJ_DIR)/cgltf.o

# stbi
$(LIBS_OBJ_DIR)/stbi.o: $(LIBS_DIR)/stbi/stb_image.c
	@gcc -O2 -c $(LIBS_DIR)/stbi/stb_image.c -o $(LIBS_OBJ_DIR)/stbi.o
	
# FastNoiseLite
$(LIBS_OBJ_DIR)/fast_noise_lite.o: $(LIBS_DIR)/FastNoiseLite/fast_noise_lite.c
	@gcc -O2 -c $(LIBS_DIR)/FastNoiseLite/fast_noise_lite.c -o $(LIBS_OBJ_DIR)/fast_noise_lite.o

# only clear source files
clean:
	rm -rf $(OBJ_DIR) $(SPV_DIR)

# clear everything even libs
full_clean:
	rm -rf $(BUILD_DIR)
