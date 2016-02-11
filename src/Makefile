.PHONY: all clean

CC = $(CROSS_COMPILER)gcc
STRIP = $(CROSS_COMPILER)strip

PLATFORM=x86_64

ifeq ($(CROSS_COMPILER),arm-mv5sft-linux-gnueabi-)
PLATFORM=mv
endif

WARNING_FLAGS_MAXIMUM_PARANOIA = -Wall -Werror -Wextra -Wconversion -Wsign-conversion \
                                 -Winit-self -Wunreachable-code -Wformat-nonliteral -Wformat-security \
                                 -Wmissing-include-dirs -Wswitch-default -Wstrict-overflow=4 -Wfloat-equal \
                                 -Wundef -Wshadow -Wbad-function-cast -Wcast-align -Wwrite-strings -Wlogical-op

WARNING_FLAGS_HARD = -Wall -Werror -Wextra\
                     -Winit-self -Wunreachable-code -Wformat-nonliteral -Wformat-security \
                     -Wmissing-include-dirs -Wswitch-default -Wstrict-overflow=4 -Wfloat-equal \
                     -Wundef -Wshadow

WARNING_FLAGS_MINIMUM = -Wall -Wextra

WARNING_FLAGS = $(WARNING_FLAGS_MINIMUM) -Werror

WARNINGS = $(WARNING_FLAGS)

ifeq ($(PLATFORM),mv)
WARNINGS = $(WARNING_FLAGS_MINIMUM)
endif

SPANDSP_INCLUDE = -I$(SPANDSP_DIR)/include
SPANDSP_LIB = -L$(SPANDSP_DIR)/lib

CFLAGS += $(WARNINGS) -I$(INCLUDE_PATH) -pthread

OBJ_DIR = ../build/$(PLATFORM)/obj
BIN_DIR = ../build/$(PLATFORM)/bin
SRC_DIR = ./

SRC_FILES = $(SRC_DIR)/main.c $(SRC_DIR)/app.c $(SRC_DIR)/msg_proc.c $(SRC_DIR)/session.c $(SRC_DIR)/fax.c $(SRC_DIR)/udptl.c
OBJ_FILES = $(OBJ_DIR)/main.o $(OBJ_DIR)/app.o $(OBJ_DIR)/msg_proc.o $(OBJ_DIR)/session.o $(OBJ_DIR)/fax.o $(OBJ_DIR)/udptl.o
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

