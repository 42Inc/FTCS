CC := gcc
STD := -std=gnu11
DEBUG := -g3
SERVER := server
CLIENT := client
LIB_PATH := ./lib
OBJ_PATH := ./obj
SRC_PATH := ./src
BIN_PATH = ./bin
INC_PATH = ./include
INC := $(INC_PATH)/main.h
CFLAGS := $(DEBUG) $(STD) -pthread -O0

OBJ_CLIENT := $(OBJ_PATH)/$(CLIENT).o $(OBJ_PATH)/helper.o
OBJ_SERVER := $(OBJ_PATH)/$(SERVER).o $(OBJ_PATH)/helper.o
.PHONY: all GRADUATION_PROJECT COMPILE_C clean restruct

all: FORMAT DIRECTORY GRADUATION_PROJECT
FORMAT:
	clang-format -i $(INC_PATH)/*.h
	clang-format -i $(SRC_PATH)/*.c


DIRECTORY: $(OBJ_PATH) $(BIN_PATH)

$(OBJ_PATH):
	$(if ifeq test -d "$(OBJ_PATH)" 0, @mkdir -p $(OBJ_PATH))

$(BIN_PATH):
	$(if ifeq test -d "$(BIN_PATH)" 0, @mkdir -p $(BIN_PATH))

GRADUATION_PROJECT: CL SE

SE: $(OBJ_SERVER)
	$(CC) $(OBJ_SERVER) -o $(BIN_PATH)/$(SERVER) $(CFLAGS)

CL: $(OBJ_CLIENT)
	$(CC) $(OBJ_CLIENT) -o $(BIN_PATH)/$(CLIENT) $(CFLAGS)

$(OBJ_PATH)/$(SERVER).o: $(INC) $(SRC_PATH)/$(SERVER).c
	$(CC) -c $(SRC_PATH)/$(SERVER).c -o $(OBJ_PATH)/$(SERVER).o $(CFLAGS)

$(OBJ_PATH)/$(CLIENT).o: $(INC) $(SRC_PATH)/$(CLIENT).c
	$(CC) -c $(SRC_PATH)/$(CLIENT).c -o $(OBJ_PATH)/$(CLIENT).o $(CFLAGS)

$(OBJ_PATH)/helper.o: $(INC) $(SRC_PATH)/helper.c
	$(CC) -c $(SRC_PATH)/helper.c -o $(OBJ_PATH)/helper.o $(CFLAGS)


clean:
	rm -f $(BIN_PATH)/*
	rm -f $(OBJ_PATH)/*.o
	rm -rf $(OBJ_PATH) $(BIN_PATH)

restruct:
	make clean
	make all
