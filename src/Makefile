.PHONY: all clean

CC = gcc
STRIP = strip

INCLUDE_PATH = ../include

WARNING_FLAGS_MAXIMUM_PARANOIA = -Wall -Werror -Wextra -Wconversion -Wsign-conversion \
                                 -Winit-self -Wunreachable-code -Wformat-nonliteral -Wformat-security \
								 -Wmissing-include-dirs -Wswitch-default -Wstrict-overflow=4 -Wfloat-equal \
								 -Wundef -Wshadow -Wbad-function-cast -Wcast-align -Wwrite-strings -Wlogical-op

WARNING_FLAGS = -Wall -Werror -Wextra\
                -Winit-self -Wunreachable-code -Wformat-nonliteral -Wformat-security \
				-Wmissing-include-dirs -Wswitch-default -Wstrict-overflow=4 -Wfloat-equal \
				-Wundef -Wshadow

DEBUG = -g
OPTIMISATION = -O0

SPANDSP_DIR = ../spandsp/x86_64/deploy/usr/local
SPANDSP_INCLUDE = -I$(SPANDSP_DIR)/include
SPANDSP_LIB = -L$(SPANDSP_DIR)/lib

LIBS += $(shell pkg-config --libs-only-l $(SPANDSP_DIR)/lib/pkgconfig/spandsp.pc)
LDFLAGS = $(SPANDSP_LIB)

CFLAGS += $(WARNING_FLAGS) -I$(INCLUDE_PATH) $(SPANDSP_INCLUDE) $(DEBUG) $(OPTIMISATION)

OBJ_DIR = ../obj
BIN_DIR = ../bin
SRC_DIR = ./

SRC_FILES = $(SRC_DIR)/main.c $(SRC_DIR)/app.c $(SRC_DIR)/msg_proc.c $(SRC_DIR)/session.c
OBJ_FILES = $(OBJ_DIR)/main.o $(OBJ_DIR)/app.o $(OBJ_DIR)/msg_proc.o $(OBJ_DIR)/session.o
BIN = $(BIN_DIR)/fax_bu_app

all: striped

striped: $(BIN)
	cp $(BIN) $(BIN)_uns
	$(STRIP) $(BIN)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(OBJ_FILES) $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ_FILES) $(LIBS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)


clean: 
	rm -rf $(OBJ_DIR)
	rm -rf $(BIN_DIR)

