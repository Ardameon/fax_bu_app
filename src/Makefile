.PHONY: all clean

CC = gcc
INCLUDE_PATH = ../include

WARNING_FLAGS_MAXIMUM_PARANOIA = -Wall -Werror -Wextra -Wconversion -Wsign-conversion \
                                 -Winit-self -Wunreachable-code -Wformat-nonliteral -Wformat-security \
								 -Wmissing-include-dirs -Wswitch-default -Wstrict-overflow=4 -Wfloat-equal \
								 -Wundef -Wshadow -Wbad-function-cast -Wcast-align -Wwrite-strings -Wlogical-op

WARNING_FLAGS = -Wall -Werror -Wextra\
                -Winit-self -Wunreachable-code -Wformat-nonliteral -Wformat-security \
				-Wmissing-include-dirs -Wswitch-default -Wstrict-overflow=4 -Wfloat-equal \
				-Wundef -Wshadow

DEBUG = -g -O0

CFLAGS += $(WARNING_FLAGS) -I$(INCLUDE_PATH) $(DEBUG)


SRC_FILES = main.c app.c
OBJ_FILES = main.o app.o
OUTPUT_FILE = fax_bu_app

all: fax_bu_app

main.o: main.c 
	$(CC) $(CFLAGS) -c -o $@ $<

app.o: app.c
	$(CC) $(CFLAGS) -c -o $@ $<


fax_bu_app: $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $(OBJ_FILES)

clean: 
	rm -f *.o
	rm -f fax_bu_app

