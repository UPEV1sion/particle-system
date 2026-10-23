CC := gcc
CFLAGS := -Wall -Wextra -Werror -O3 -march=native

.PHONY: run

all: particle run

run: particle
	./$^


particle: main.c
	$(CC) $(CFLAGS) -o $@ $^ -L./raylib-6.2_linux_amd64/lib -l:libraylib.a -I./raylib-6.2_linux_amd64/include -lm -lX11
