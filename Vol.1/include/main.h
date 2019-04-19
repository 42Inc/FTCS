#ifndef MAIN_H
#define MAIN_H

#include "./bc.h"
#include "./readkey.h"
#include "./term.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define GAMES 10
#define TIMEOUT 10
#define PORT 65500
#define MAXDATASIZE 256
#define BACKLOG games * 2
#define WINCOORD 8
#define CONN_FALSE 0
#define CONN_TRUE 1
#define GAME_IN_PROG 1
#define TRUE 1
#define FALSE 0
#define GET_ID 1
#define SET_ID 2
#define SERVER_SET_ID 3
#define WIN_END_GAME 4
#define LOSE_END_GAME 5
#define SYNCHRONIZE_GAME 6
#define SYNCHRONIZE_CHANGE 7
#define SYNCHRONIZE_RM 8
#define START_GAME 9
#define RECONNECT_GAME 10
#define RECONNECT_ID 11
#define NONE_CMD 100
#define SIZE_GAME (sizeof(int) * 5 + 10)

typedef enum type_packet {
  MSG = 0,
  CHANGE_FIELD = 1,
  SERVICE = 2,
  CONN_ACK = 3,
  CONN_NEW = 4,
  CONN_EST = 5,
  CONN_RECONNECT = 6,
  CONN_SERVER = 7,
  CONN_CLIENT = 8,
  SYNCHRONIZE = 9,
  SYNCHRONIZE_REMOVE = 10,
  NONE = 100
} type_packet_t;

typedef struct _ipc {
  pid_t pid;
  key_t key;
  int id;
  int game;
  int srv;
  int msqid;
  struct _ipc *next;
} ipc_t;

typedef struct packet {
  enum type_packet type;
  int packet_id;
  int client_id;
  char buffer[MAXDATASIZE];
} packet_t;

typedef struct msg {
  long mtype;
  char mtext[MAXDATASIZE];
} message_t;

typedef struct servers {
  int port;
  int number;
  char ip[MAXDATASIZE];
  struct servers *next;
} srv_t;

typedef struct servers_pool {
  int count;
  struct servers *srvs;
} srv_pool_t;

typedef struct game {
  int id;
  int player1_id;
  int player2_id;
  int owner;
  ipc_t *player1;
  ipc_t *player2;
  struct game *next;
} games_t;

typedef struct packet_queue {
  packet_t p;
  struct packet_queue *next;
} packet_queue_t;

typedef struct packet_queue_header {
  struct packet_queue *tail;
  struct packet_queue *head;
  size_t len;
  packet_queue_t *q;
} packets_t;

typedef struct client {
  pid_t pid;
  int id;
  int srv;
  int game;
  int status;
  double time;
  struct client *next;
} clients_t;
packet_t
make_packet(type_packet_t type, int client_id, int packet_id, char *buff);
int send_packet(packet_t p);
int get_packet(packet_t *p);
int check_connection();
int wait_ack(int packet_id);
int send_ack(int client_id, int packet_id);
void push_queue(packet_t p, packets_t **queue);
packet_t pop_queue(packets_t **queue);
int reconnection();
int read_servers_pool(char *filename);
void create_connections_to_servers();
void remove_this_server_from_list();
int client_tcp_connect(struct hostent *ip, int port);
void server_connection(srv_t *cursor);
int remove_game(
        games_t **root, int id, int win, int lose, pthread_mutex_t *mutex);
games_t *get_game_id(games_t **root, int id, pthread_mutex_t *mutex);
ipc_t *get_msq_pid(ipc_t **root, int pid, int srv, pthread_mutex_t *msq_mutex);
ipc_t *get_msq_id(ipc_t **root, int id, int srv, pthread_mutex_t *msq_mutex);
int remove_msq(ipc_t **root, int pid, pthread_mutex_t *mutex);
int add_msq(ipc_t **root, int pid, int server, pthread_mutex_t *mutex);
void print_msq(ipc_t **root, pthread_mutex_t *mutex);
void printBox();
int getrand(int min, int max);
void synchronized_game_w(int game_id, int w, int l);
void synchronized_game(int game_id);
void add_game(
        games_t **root,
        int id,
        ipc_t *f,
        ipc_t *s,
        int fi,
        int si,
        int own,
        pthread_mutex_t *mutex);
void msq_set_game(ipc_t **p, int game, pthread_mutex_t *mutex);
void get_free_pair_msq(
        ipc_t **root, ipc_t **f, ipc_t **s, pthread_mutex_t *mutex);
#endif
