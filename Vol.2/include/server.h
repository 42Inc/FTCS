#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "inet.h"

#ifndef _SERVER_H_
#define _SERVER_H_

#define MAX_PORTS 20
#define MAX_GAMES 20

#define FG_WAIT 1
#define FG_PROCES 2
#define FG_END 3
#define FG_STOP 4
#define FG_RECO 5
#define FG_DEST 6

#define TICK_SEND \
  2 // (MAIN_TICK_MAX) - отправка пакета для проверки связи, (MAIN_TICK_MAX * 2)
    // - отключение сокета
#define TICK_DROP 10
#define EBMA 1 //ошибка связанная с сокетом главного сервера
#define EBEX 2 //ошибка связанная с сокетом дополнительного сервера
#define ENLI 4 //ошибка связанная со списком серверов (локальным)
#define EGLI 8 //в списке серверов образовалась "дыра" (в глобальном смысле)
#define EPLI 16 //ошибка связанная со списком игроков/игр

#define PERR(...)                      \
  fprintf(logs, "Error: "__VA_ARGS__); \
  fflush(logs)
#define PINF(...)                       \
  fprintf(logs, "Notice: "__VA_ARGS__); \
  fflush(logs)

FILE *logs;

#define Player struct inet_server_player

Player {
  unsigned int id;
  unsigned int gid;
  unsigned char symbol;
  char ip[16];
  unsigned int port;
  int socket;
  char name[NAME_LEN];
};

#define Game struct inet_server_game

Game {
  unsigned int id;
  Player *x;
  Player *o;
  char board[10];
  unsigned char is_run;
  pthread_t pid;
};

#define SLNode struct inet_server_ServersListNode

SLNode {
  char ip[16];
  unsigned int s_port;
  unsigned int g_port;
  SLNode *left;
  SLNode *right;
};

#define Message struct inet_server_pthread_message

Message {
  /*
          act     действие                        uint            node
          2       обновление состояния игры       ид игры
          3       новый игрок                     ид игрока
          5       новая игра                      ид игры
          6       завершение игры                 ид игры
          7       удалить сервер из списка                        указатель на
     его данные 8       упал обработчик игроков
  */
  unsigned char act;
  unsigned int uint;
  SLNode *node;

  Message *next;
};

#define Server struct inet_server

Server {
  char ip[16];
  Game *games[MAX_GAMES];
  unsigned int last_game_id;
  unsigned int last_player_id;
  Player *players[MAX_GAMES * 2];
  unsigned int games_port;
  int clients_socket;  //слушать клиентов
  pthread_t games_pid; //обслуживание клиентов

  int main_server_socket;
  SLNode main_server_info;
  int servers_socket; //слушать сервера
  unsigned int servers_count;
  unsigned int servers_port;
  SLNode *servers;
  pthread_t servers_pid; //обслуживание серверов

  Message *que; //очередь сообщений для синхронизации серверов
  Message *que_end;
  pthread_mutex_t mutex;
};

struct pthread_games {
  Server *srv;
  Game *gm;
};

/*
        Файл server.c
*/
void WRND(Data *data);
void server_push_Message(
        Server *srv, unsigned char act, unsigned int uint, SLNode *node);
Message *server_pop_Message(Server *srv);

/*
        Файл servers.c
*/
void *server_servers(void *arg);
void server_add_SLNode(
        Server *srv, char *ip, unsigned int s_port, unsigned int g_port);
int connect_to_main_server(char *ip, unsigned int port);

/*
        Файл games.c
*/
void *server_games(void *arg);
void *games_game(void *arg);
int create_Player(Server *srv, int pl_id, unsigned int gid);
void delete_Game(Server *srv, unsigned int gm_id);
int create_Game(Server *srv, int gm_id);
#endif
