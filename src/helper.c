#include "./../include/main.h"

int reader_buffer_len = 0;
int writer_buffer_len = 0;
int state_connection = CONN_FALSE;
pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t helper_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_socket_read = -1;
int client_socket_write = -1;
int ack_id = -1;

packets_t *reader_buffer;
packets_t *writer_buffer;
void push_queue(packet_t p, packets_t **queue) {
  if (*queue == NULL) {
    *queue = (packets_t *)malloc(sizeof(packets_t));
    (*queue)->q = (packet_queue_t *)malloc(sizeof(packet_queue_t));
    (*queue)->head = (*queue)->q;
    (*queue)->tail = (*queue)->q;
    (*queue)->q->p = p;
    (*queue)->q->next = NULL;
    (*queue)->len = 1;
  } else {
    packet_queue_t *n = (packet_queue_t *)malloc(sizeof(packet_queue_t));
    n->p = p;
    n->next = NULL;
    if ((*queue)->q != NULL) {
      (*queue)->tail->next = n;
      (*queue)->tail = n;
      (*queue)->len++;
    } else {
      (*queue)->q = n;
      (*queue)->tail = n;
      (*queue)->head = n;
      (*queue)->len = 1;
    }
  }
  printf("Push[%lu]!\n", (*queue)->len);
}

packet_t pop_queue(packets_t **queue) {
  packet_t p = make_packet(NONE, NULL);
  if (*queue == NULL) {
    return p;
  } else {
    if ((*queue)->q != NULL) {
      p = (*queue)->head->p;
      (*queue)->q = (*queue)->head->next;
      free((*queue)->head);
      (*queue)->head = (*queue)->q;
      (*queue)->len--;
    }
  }
  printf("Pop[%lu]!\n", (*queue)->len);
  return p;
}

packet_t make_packet(type_packet_t type, char *buff) {
  packet_t p;
  p.type = type;
  p.client_id = rand() % 1000;
  p.packet_id = rand() % 1000;
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
  long int duration = 1000000000;
  printf("Wait ack\n");
  while (1) {
    // connection mutex
    if (ack_id == packet_id) {
      ack_id = -1;
      printf("Receive ack\n");
      return TRUE;
    }
  }
  return FALSE;
}

int send_packet(packet_t p) {
  // connection mutex
  pthread_mutex_lock(&writer_mutex);
  push_queue(p, &writer_buffer);
  //  writer_buffer[writer_buffer_len++] = p;
  pthread_mutex_unlock(&writer_mutex);
  return TRUE;
}

int get_packet(packet_t *p) {
  // connection mutex
  pthread_mutex_lock(&reader_mutex);
  if (reader_buffer_len > 0) {
    *p = pop_queue(&reader_buffer); // reader_buffer[reader_buffer_len--];
    pthread_mutex_unlock(&reader_mutex);
    return TRUE;
  }
  pthread_mutex_unlock(&reader_mutex);
  return FALSE;
}

int check_connection() {
  return state_connection;
}
