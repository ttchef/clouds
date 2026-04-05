
CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS := -lglfw -lvulkan -lm

SRC_FILES := main.c renderer.c

all:
	$(CC) $(CFLAGS) $(SRC_FILES) -o main $(LDFLAGS)
