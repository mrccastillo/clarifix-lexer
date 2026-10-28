CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

clarifix: main.c lexer.c lexer.h
	$(CC) $(CFLAGS) main.c lexer.c -o clarifix

.PHONY: test
test: clarifix
	python tests/test_lexer.py ./clarifix
