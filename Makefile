all: shell

clean:
	rm -f shell

run: shell
	./shell

shell: shell.c linenoise.c linenoise.h
	${CC} -o $@ shell.c linenoise.c ${CFLAGS} -Wall -Wextra -std=c99 -D_GNU_SOURCE
