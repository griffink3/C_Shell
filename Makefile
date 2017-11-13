CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror

CC = gcc
EXECS = shell shellnoprompt

PROMPT = -DPROMPT

.PHONY: all, clean

all: $(EXECS)

shell: shell.c
	$(CC) $(CFLAGS) $(PROMPT) $^ -o $@
shellnoprompt: shell.c
	$(CC) $(CFLAGS) $^ -o $@
clean:
	rm -f $(EXECS)
