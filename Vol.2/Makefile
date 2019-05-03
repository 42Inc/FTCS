CC = gcc
QUIET = @
OFLAGS = -c -Wall
CFLAGS = -g3 -O0 -Wall
LIB = -lm -lncursesw -lpthread

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
INCLUDES = -I $(SRC_DIR)
DIRS = $(BIN_DIR) $(OBJ_DIR) logs
OBJ_CLI = $(OBJ_DIR)/client.o $(OBJ_DIR)/pth_game.o $(OBJ_DIR)/pth_server.o $(OBJ_DIR)/display.o
OBJ_SRV = $(OBJ_DIR)/server.o $(OBJ_DIR)/servers.o $(OBJ_DIR)/game.o $(OBJ_DIR)/games.o

.PHONY: all cli srv obj_cli obj_srv clean rebuild

all: $(DIRS) clang cli srv

clang:
	clang-format -i $(INC_DIR)/*.h
	clang-format -i $(SRC_DIR)/*.c

cli: obj_cli
	$(CC) $(OBJ_CLI) $(CFLAGS) -o $(BIN_DIR)/cmpl_cli $(LIB)

srv: obj_srv
		$(CC) $(OBJ_SRV) $(CFLAGS) -o $(BIN_DIR)/cmpl_srv $(LIB)

$(DIRS):
	$(QUIET)mkdir $(DIRS)

obj_cli:
	$(CC) $(OFLAGS) $(SRC_DIR)/client.c -o $(OBJ_DIR)/client.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/pth_server.c -o $(OBJ_DIR)/pth_server.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/pth_game.c -o $(OBJ_DIR)/pth_game.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/display.c -o $(OBJ_DIR)/display.o $(INCLUDES)

obj_srv:
	$(CC) $(OFLAGS) $(SRC_DIR)/server.c -o $(OBJ_DIR)/server.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/servers.c -o $(OBJ_DIR)/servers.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/game.c -o $(OBJ_DIR)/game.o $(INCLUDES)
	$(CC) $(OFLAGS) $(SRC_DIR)/games.c -o $(OBJ_DIR)/games.o $(INCLUDES)

clean:
	$(QUIET)rm -rfv $(DIRS)

rebuild:
	make clean ; make
