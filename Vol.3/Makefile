CC = gcc
QUIET = @
OFLAGS = -c -W -Wall
CFLAGS = -g3 -O0 -W -Wall
GFLAGS = `pkg-config --cflags gtk+-2.0 --libs gtk+-2.0`
LIB = -lm -lncursesw -lpthread

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
INCLUDES = -I $(SRC_DIR)
DIRS = $(BIN_DIR) $(OBJ_DIR) logs
OBJ_CLI = $(OBJ_DIR)/client.o
OBJ_SRV = $(OBJ_DIR)/server.o $(OBJ_DIR)/glist.o $(OBJ_DIR)/llist.o

.PHONY: all cli srv obj_cli obj_srv clean rebuild

all: $(DIRS) clang cli srv

clang:
	clang-format -i $(INC_DIR)/*.h
	clang-format -i $(SRC_DIR)/*.c

cli: obj_cli
	$(CC) $(OBJ_CLI) $(CFLAGS) -o $(BIN_DIR)/cmpl_cli  $(GFLAGS) $(LIB)

srv: obj_srv
		$(CC) $(OBJ_SRV) $(CFLAGS) -o $(BIN_DIR)/cmpl_srv $(LIB)

$(DIRS):
	$(QUIET)mkdir $(DIRS)

obj_cli:
	$(CC) $(OFLAGS) $(SRC_DIR)/client.c -o $(OBJ_DIR)/client.o $(GFLAGS) $(INCLUDES)

obj_srv:
	$(CC) $(OFLAGS) $(SRC_DIR)/server.c -o $(OBJ_DIR)/server.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/glist.c -o $(OBJ_DIR)/glist.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/llist.c -o $(OBJ_DIR)/llist.o $(INCLUDES)

clean:
	$(QUIET)rm -rfv $(DIRS)

rebuild:
	make clean ; make
