CC := gcc
CFLAGS := -Wall -Wextra -Werror -ggdb

particle: main.c
	$(CC) $(CFLAGS) -o $@ $^ -L./raylib-6.2_linux_amd64/lib -l:libraylib.a -I./raylib-6.2_linux_amd64/include -lm -lX11
