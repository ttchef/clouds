
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g -fsanitize=address
LDFLAGS := -lglfw -lvulkan -lm

SRC_FILES := main.c renderer.c log.c

all:
	$(CC) $(CFLAGS) $(SRC_FILES) -o main $(LDFLAGS)
