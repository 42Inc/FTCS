#include "./../include/main.h"

int reader_join = 0;
int writer_join = 0;
int state_connection = CONN_FALSE;
pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t helper_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t servers_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_socket_read = -1;
int client_socket_write = -1;
int ack_id = -1;

packets_t *reader_buffer = NULL;
packets_t *writer_buffer = NULL;

srv_pool_t *known_servers = NULL;
char hostname[256] = "localhost";
int port = PORT; /* PORT*/

double wtime() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double)t.tv_sec + (double)t.tv_usec * 1E-6;
}
/*---------------------------------------------------------------------------*/
int create_file(char *client) {
}
/*---------------------------------------------------------------------------*/
int remove_file(char *client) {
}
/*---------------------------------------------------------------------------*/
void add_game(
        games_t **root,
        pid_t id,
        int f,
        int s,
        pid_t pf,
        pid_t ps,
        int o,
        pthread_mutex_t *mutex) {
  games_t *p;
  p = (games_t *)malloc(sizeof(games_t));
  pthread_mutex_lock(mutex);
  if (p == NULL) {
    pthread_mutex_unlock(mutex);
    return;
  }
  p->id = id;
  p->player1 = f;
  p->player2 = s;
  p->p_player1 = pf;
  p->p_player2 = ps;
  p->owner = o;
  if (*root == NULL) {
    *root = p;
  } else {
    p->next = *root;
    *root = p;
  }
  fprintf(stderr,
          "Add game [id: %d | first: %d | second: %d | owner: %d] \n",
          p->id,
          p->player1,
          p->player2,
          p->owner);
  pthread_mutex_unlock(mutex);
}
/*---------------------------------------------------------------------------*/
void add_client(
        clients_t **root, pid_t pid, int id, int srv, pthread_mutex_t *mutex) {
  clients_t *p;
  p = (clients_t *)malloc(sizeof(clients_t));
  pthread_mutex_lock(mutex);
  if (p == NULL) {
    pthread_mutex_unlock(mutex);
    return;
  }
  p->pid = pid;
  p->id = id;
  p->srv = srv;
  p->game = -1;
  p->status = FALSE;
  p->time = -1;
  if (*root == NULL) {
    *root = p;
  } else {
    p->next = *root;
    *root = p;
  }
  fprintf(stderr,
          "Add client [id: %d | pid: %d | srv: %d] \n",
          p->id,
          p->pid,
          p->srv);
  pthread_mutex_unlock(mutex);
}
/*---------------------------------------------------------------------------*/
int disconnect_client(clients_t **root, pid_t pid, pthread_mutex_t *mutex) {
  pthread_mutex_lock(mutex);
  clients_t *p = *root;
  if (*root == NULL) {
    pthread_mutex_unlock(mutex);
    return -1;
  }
  while (p != NULL) {
    if (p->pid == pid) {
      p->pid = -1;
      p->time = wtime();
      pthread_mutex_unlock(mutex);
      return p->id;
    }
    p = p->next;
  }
  pthread_mutex_unlock(mutex);
  return -1;
}
/*----------------------------------------------------------------------------*/
int remove_game(games_t **root, pid_t id, pthread_mutex_t *mutex) {
  games_t *prev = NULL;
  games_t *p = *root;
  char buff[20];
  pthread_mutex_lock(mutex);
  if (*root == NULL) {
    //    fprintf(stderr, "Remove NULL\n");
    pthread_mutex_unlock(mutex);
    return 1;
  }
  pthread_mutex_unlock(mutex);
  prev = NULL;
  p = *root;
  pthread_mutex_lock(mutex);
  while (p != NULL) {
    if (p->id == id) {
      if (prev != NULL) {
        prev->next = p->next;
        free(p);
        pthread_mutex_unlock(mutex);
        return 0;
      } else {
        *root = p->next;
        free(p);
        pthread_mutex_unlock(mutex);
        return 0;
      }
    }
    prev = p;
    p = p->next;
  }
  pthread_mutex_unlock(mutex);
  return 1;
}
/*---------------------------------------------------------------------------*/
int remove_client(clients_t **root, pid_t pid, pthread_mutex_t *mutex) {
  clients_t *prev = NULL;
  clients_t *p = *root;
  clients_t *cursor = *root;
  int id = -1;
  char buff[20];
  pthread_mutex_lock(mutex);
  if (*root == NULL) {
    //    fprintf(stderr, "Remove NULL\n");
    pthread_mutex_unlock(mutex);
    return -1;
  }
  if (pid == -2) {
    while (p != NULL) {
      if (p->time > 0 && (wtime() - p->time) > TIMEOUT) {
        fprintf(stderr, "Remove node [id: %d] TIMEOUT\n", p->id);
        if (prev != NULL) {
          prev->next = p->next;
          id = p->id;
          free(p);
          pthread_mutex_unlock(mutex);
          //          fprintf(stderr, "Return id: %d\n", id);
          return id;
        } else {
          *root = p->next;
          id = p->id;
          free(p);
          pthread_mutex_unlock(mutex);
          //          fprintf(stderr, "Return id: %d\n", id);
          return id;
        }
      }
      prev = p;
      p = p->next;
    }
    pthread_mutex_unlock(mutex);
    return -1;
  }
  pthread_mutex_unlock(mutex);
  prev = NULL;
  p = *root;
  pthread_mutex_lock(mutex);
  while (p != NULL) {
    if (p->pid == pid) {
      if (prev != NULL) {
        prev->next = p->next;
        free(p);
        pthread_mutex_unlock(mutex);
        return 0;
      } else {
        *root = p->next;
        free(p);
        pthread_mutex_unlock(mutex);
        return 0;
      }
    }
    prev = p;
    p = p->next;
  }
  pthread_mutex_unlock(mutex);

  return 1;
}
/*---------------------------------------------------------------------------*/
int client_tcp_connect(struct hostent *ip, int port) {
  int sock;
  struct sockaddr_in client;
  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == -1) {
    fprintf(stderr, "Could not create socket\n");
    exit(EXIT_FAILURE);
  }

  memset(&client, 0, sizeof(client));
  client.sin_addr = *((struct in_addr *)ip->h_addr);
  client.sin_family = AF_INET;
  client.sin_port = htons(port);

  if (connect(sock, (struct sockaddr *)&client, sizeof(client)) < 0) {
    return -1;
  }

  return sock;
}
/*---------------------------------------------------------------------------*/
int read_servers_pool(char *filename) {
  FILE *fd = fopen(filename, "r");
  int i = 0;
  srv_t *cursor = NULL;
  srv_t *p = NULL;
  int index = 0;
  int prior = 0;
  char buffer[256] = {0};
  if (fd == NULL)
    return 1;
  known_servers = (srv_pool_t *)malloc(sizeof(srv_pool_t));
  if (known_servers == NULL)
    return 1;
  fscanf(fd, "%d", &known_servers->count);
  for (i = 0; i < known_servers->count; ++i) {
    p = (srv_t *)malloc(sizeof(srv_t));
    if (known_servers->srvs == NULL) {
      known_servers->srvs = p;
      cursor = known_servers->srvs;
    } else {
      cursor->next = p;
      cursor = cursor->next;
    }
    if (cursor == NULL)
      return 1;
    fscanf(fd, "%d", &prior);
    fscanf(fd, "%s", buffer);
    while (buffer[index] != ':') { index++; }
    buffer[index] = '\0';
    cursor->port = atoi(&buffer[index + 1]);
    cursor->number = prior;
    strcpy(cursor->ip, buffer);
    fprintf(stderr,
            "Read config[%d]: %s:%d %p\n",
            prior,
            cursor->ip,
            cursor->port,
            cursor);
  }
  fclose(fd);
  return 0;
}
/*---------------------------------------------------------------------------*/
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
  //  fprintf(stderr, "Push[%lu]!\n", (*queue)->len);
}
/*---------------------------------------------------------------------------*/
packet_t pop_queue(packets_t **queue) {
  packet_t p = make_packet(NONE, 0, 0, NULL);
  if (*queue == NULL) {
    return p;
  } else {
    if ((*queue)->q != NULL) {
      memcpy(&p, &(*queue)->head->p, sizeof(packet_t));
      (*queue)->q = (*queue)->head->next;
      free((*queue)->head);
      (*queue)->head = (*queue)->q;
      (*queue)->len--;
    }
  }
  //  fprintf(stderr, "Pop[%lu]!\n", (*queue)->len);
  return p;
}
/*---------------------------------------------------------------------------*/
packet_t
make_packet(type_packet_t type, int client_id, int packet_id, char *buff) {
  packet_t p;
  p.type = type;
  p.client_id = client_id;
  p.packet_id = packet_id;
  if (buff != NULL) {
    strcpy(p.buffer, buff);
  } else {
    p.buffer[0] = '\0';
  }
  return p;
}
/*---------------------------------------------------------------------------*/
int send_ack(int client_id, int packet_id) {
  packet_t p = make_packet(CONN_ACK, client_id, packet_id, NULL);
  int send_result = 0;
  pthread_mutex_lock(&connection_mutex);
  send_result = send(client_socket_write, &p, sizeof(p), 0);
  pthread_mutex_unlock(&connection_mutex);
  //  fprintf(stderr, "Send ack\n");
  return TRUE;
}
/*---------------------------------------------------------------------------*/
int wait_ack(int packet_id) {
  long int duration = 1000000000;
  //  fprintf(stderr, "Wait ack\n");
  while (duration--) {
    if (ack_id == packet_id) {
      ack_id = -1;
      //      fprintf(stderr, "Receive ack\n");
      return TRUE;
    }
  }
  return FALSE;
}
/*---------------------------------------------------------------------------*/
int send_packet(packet_t p) {
  pthread_mutex_lock(&writer_mutex);
  push_queue(p, &writer_buffer);
  pthread_mutex_unlock(&writer_mutex);
  return TRUE;
}
/*---------------------------------------------------------------------------*/
int get_packet(packet_t *p) {
  pthread_mutex_lock(&reader_mutex);
  if (reader_buffer->len > 0) {
    *p = pop_queue(&reader_buffer);
    pthread_mutex_unlock(&reader_mutex);
    return TRUE;
  }
  pthread_mutex_unlock(&reader_mutex);
  return FALSE;
}
/*---------------------------------------------------------------------------*/
int check_connection() {
  return state_connection;
}
/*---------------------------------------------------------------------------*/
