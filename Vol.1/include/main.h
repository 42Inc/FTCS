#ifndef MAIN_H
#define MAIN_H

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
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define GAMES 4
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
  NONE = 100
} type_packet_t;

typedef struct client {
  pid_t pid;
  int id;
  int srv;
  int game;
  int status;
  double time;
  struct client *next;
} clients_t;

typedef struct packet {
  enum type_packet type;
  int packet_id;
  int client_id;
  char buffer[MAXDATASIZE];
} packet_t;

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
  int player1;
  int player2;
  pid_t p_player1;
  pid_t p_player2;
  int owner;
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

int disconnect_client(clients_t **root, pid_t pid, pthread_mutex_t *mutex);
int remove_client(clients_t **root, pid_t pid, pthread_mutex_t *mutex);
void add_client(
        clients_t **root, pid_t pid, int id, int srv, pthread_mutex_t *mutex);

void add_game(
        games_t **root,
        pid_t id,
        int f,
        int s,
        pid_t pf,
        pid_t ps,
        int o,
        pthread_mutex_t *mutex);
#endif
