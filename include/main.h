#ifndef MAIN_H
#define MAIN_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define GAMES 2
#define PORT 1025
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
  CONN_RECONNECT = 6
} type_packet_t;
// TODO: Packet ID?
typedef struct packet {
  enum type_packet type;
  int packet_id;
  int client_id;
  char buffer[MAXDATASIZE];
} packet_t;

packet_t make_packet(type_packet_t type, char *buff);
int send_packet(packet_t p);
int get_packet(packet_t *p);
int check_connection();
int wait_ack(int packet_id);
int send_ack(int packet_id);

#endif
