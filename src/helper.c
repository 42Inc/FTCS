#include "./../include/main.h"

packet_t reader_buffer[MAXDATASIZE] = {{.type = -1}};
packet_t writer_buffer[MAXDATASIZE];
int reader_buffer_len = 0;
int writer_buffer_len = 0;
int state_connection = CONN_FALSE;
pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t helper_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_socket_read = -1;
int client_socket_write = -1;
packet_t make_packet(type_packet_t type, char *buff) {
  packet_t p;
  p.type = type;
  if (buff != NULL) {
    strcpy(p.buffer, buff);
  } else {
    p.buffer[0] = '\0';
  }
  return p;
}

int send_ack(int packet_id) {
  packet_t p = make_packet(CONN_ACK, NULL);
  int send_result = 0;
  pthread_mutex_lock(&connection_mutex);
  send_result = send(client_socket_write, &p, sizeof(p), 0);
  pthread_mutex_unlock(&connection_mutex);
  printf("Send ack\n");
  return TRUE;
}

int wait_ack(int packet_id) {
  long int duration = 100000000;
  printf("Wait ack\n");
  while (duration--) {
    // connection mutex
    pthread_mutex_lock(&reader_mutex);
    if (reader_buffer[reader_buffer_len].type == CONN_ACK) {
      pthread_mutex_unlock(&reader_mutex);
      printf("Receive ack\n");
      return TRUE;
    }
    pthread_mutex_unlock(&reader_mutex);
  }
  return FALSE;
}

int send_packet(packet_t p) {
  // connection mutex
  pthread_mutex_lock(&writer_mutex);
  if (writer_buffer_len == MAXDATASIZE - 1) {
    pthread_mutex_unlock(&writer_mutex);
    return FALSE;
  }
  writer_buffer[writer_buffer_len++] = p;
  pthread_mutex_unlock(&writer_mutex);
  return TRUE;
}

int get_packet(packet_t *p) {
  // connection mutex
  pthread_mutex_lock(&reader_mutex);
  pthread_mutex_unlock(&reader_mutex);
  return FALSE;
}

int check_connection() {
  return state_connection;
}
