
CC?=clang
CFLAGS=-std=c99 -O3 -Wall -Wextra -Werror
LDFLAGS=-lm

SRC=main.c io.c
EXE=alcazam

OBJ=$(addprefix obj/, $(SRC:.c=.o))

all: $(EXE)

.PHONY: clean

clean:
	$(RM) $(EXE) $(OBJ)

dirs: obj
	mkdir -p obj

obj/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) -o $@ $(CFLAGS) -c $<

$(EXE): $(OBJ) Makefile
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) $(OBJ)
