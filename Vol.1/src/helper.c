#include "./../include/main.h"

int reader_join = 0;
int writer_join = 0;
int state_connection = CONN_FALSE;
pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msq_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_av_mutex = PTHREAD_MUTEX_INITIALIZER;
int games_curr = 0;
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
int getrand(int min, int max) {
  return (double)rand() / (RAND_MAX + 1.0) * (max - min) + min;
}
/*---------------------------------------------------------------------------*/
void msq_set_game(ipc_t **p, int game, pthread_mutex_t *mutex) {
  if (*p == NULL)
    return;
  pthread_mutex_lock(mutex);
  if (game == -1) {
    fprintf(stderr,
            "Reject reconnect to game [pid: %d | id: %d | game: %d]\n",
            (*p)->pid,
            (*p)->id,
            game);
  } else {
    fprintf(stderr, "Reconnect to game [id: %d | game: %d]\n", (*p)->id, game);
    (*p)->game = game;
  }
  pthread_mutex_unlock(mutex);
}
/*---------------------------------------------------------------------------*/
void print_msq(ipc_t **root, pthread_mutex_t *mutex) {
  ipc_t *p = *root;
  pthread_mutex_lock(mutex);

  fprintf(stderr, "/*-MSQ----------------*/\n");
  while (p != NULL) {
    fprintf(stderr,
            "[id: %d | game: %d | key: %d | fd: %d | ptr: %p -> %p | Server: "
            "%s]\n",
            p->id,
            p->game,
            p->key,
            p->msqid,
            p,
            p->next,
            p->srv ? "TRUE" : "FALSE");
    p = p->next;
  }
  fprintf(stderr, "/*--------------------*/\n");
  pthread_mutex_unlock(mutex);
}
/*---------------------------------------------------------------------------*/
void print_games(games_t **root, pthread_mutex_t *mutex) {
  games_t *p = *root;
  pthread_mutex_lock(mutex);

  fprintf(stderr, "/*-GAME---------------*/\n");
  while (p != NULL) {
    fprintf(stderr,
            "[id: %d | first: %d | second: %d]\n",
            p->id,
            p->player1_id,
            p->player2_id);
    p = p->next;
  }
  fprintf(stderr, "/*--------------------*/\n");
  pthread_mutex_unlock(mutex);
}
/*---------------------------------------------------------------------------*/
void get_free_pair_msq(
        ipc_t **root, ipc_t **f, ipc_t **s, pthread_mutex_t *mutex) {
  ipc_t *p = *root;
  pthread_mutex_lock(mutex);
  while (p != NULL) {
    if (p->game == -1 && p->srv == FALSE) {
      if (*f == NULL)
        *f = p;
      else if (*s == NULL)
        *s = p;
      if (*f != NULL && *s != NULL)
        break;
    }
    p = p->next;
  }
  fprintf(stderr, "Free pair [id_f: %d | id_s: %d]\n", (*f)->id, (*s)->id);
  pthread_mutex_unlock(mutex);
}
/*---------------------------------------------------------------------------*/
int add_msq(ipc_t **root, int pid, int server, pthread_mutex_t *mutex) {
  ipc_t *p;
  p = (ipc_t *)malloc(sizeof(ipc_t));
  pthread_mutex_lock(mutex);
  if (p == NULL) {
    fprintf(stderr, "Malloc error [%s]\n", "add_msq");
    pthread_mutex_unlock(mutex);
    return 1;
  }
  p->pid = pid;
  p->key = pid;
  p->srv = server;
  p->id = -1;
  p->game = -1;
  if ((p->msqid = msgget(p->key, IPC_CREAT | 0666)) < 0) {
    perror("msgget");
    pthread_mutex_unlock(mutex);
    return 1;
  }
  if (*root == NULL) {
    *root = p;
  } else {
    p->next = *root;
    *root = p;
  }
  fprintf(stderr,
          "Add msq [id: %d | key: %d | fd: %d | ptr: %p] \n",
          p->pid,
          p->key,
          p->msqid,
          p);
  pthread_mutex_unlock(mutex);
  // print_msq(root, mutex);
  return 0;
}
/*---------------------------------------------------------------------------*/
ipc_t *get_msq_pid(ipc_t **root, int pid, int srv, pthread_mutex_t *mutex) {
  ipc_t *p = *root;
  pthread_mutex_lock(mutex);

  while (p != NULL) {
    if (p->pid == pid && p->srv == srv) {
      pthread_mutex_unlock(mutex);
      //      fprintf(stderr, "Get msq [pid: %d | ptr: %p]\n", p->pid, p);
      return p;
    }
    p = p->next;
  }
  pthread_mutex_unlock(mutex);

  return NULL;
}
/*---------------------------------------------------------------------------*/
ipc_t *get_msq_id(ipc_t **root, int id, int srv, pthread_mutex_t *mutex) {
  ipc_t *p = *root;
  pthread_mutex_lock(mutex);

  while (p != NULL) {
    if (p->id == id && p->srv == srv) {
      pthread_mutex_unlock(mutex);
      //      fprintf(stderr, "Get msq [id: %d]\n", p->id);
      return p;
    }
    p = p->next;
  }
  pthread_mutex_unlock(mutex);

  return NULL;
}
/*---------------------------------------------------------------------------*/
games_t *get_game_pid(games_t **root, int pid, pthread_mutex_t *mutex) {
  games_t *p = *root;
  pthread_mutex_lock(mutex);

  while (p != NULL) {
    if (p->player1->pid == pid || p->player2->pid == pid) {
      pthread_mutex_unlock(mutex);
      return p;
    }
    p = p->next;
  }
  pthread_mutex_unlock(mutex);

  return NULL;
}
/*---------------------------------------------------------------------------*/
games_t *get_game_id(games_t **root, int id, pthread_mutex_t *mutex) {
  games_t *p = *root;
  pthread_mutex_lock(mutex);

  while (p != NULL) {
    if (p->id == id) {
      pthread_mutex_unlock(mutex);
      return p;
    }
    p = p->next;
  }
  pthread_mutex_unlock(mutex);

  return NULL;
}
/*---------------------------------------------------------------------------*/
int remove_msq(ipc_t **root, int pid, pthread_mutex_t *mutex) {
  ipc_t *p = *root;
  ipc_t *prev = NULL;
  pthread_mutex_lock(mutex);
  if (*root == NULL) {
    pthread_mutex_unlock(mutex);
    return 1;
  }
  prev = NULL;
  p = *root;

  fprintf(stderr, "Remove msq [id: %d] \n", pid);
  while (p != NULL) {
    if (p->pid == pid) {
      if (prev != NULL) {
        prev->next = p->next;
        free(p);
        pthread_mutex_unlock(mutex);
        // print_msq(root, mutex);
        return 0;
      } else {
        *root = p->next;
        free(p);
        pthread_mutex_unlock(mutex);
        // print_msq(root, mutex);
        return 0;
      }
    }
    prev = p;
    p = p->next;
  }
  pthread_mutex_unlock(mutex);
  // print_msq(root, mutex);
  return 1;
}
/*---------------------------------------------------------------------------*/
games_t *add_game(
        games_t **root,
        int id,
        ipc_t *f,
        ipc_t *s,
        int fi,
        int si,
        int own,
        pthread_mutex_t *mutex) {
  games_t *p;
  int fd;
  char name[15];
  p = (games_t *)malloc(sizeof(games_t));
  pthread_mutex_lock(mutex);
  if (p == NULL) {
    fprintf(stderr, "Error: Memory Allocation [func: %s]\n", "add_game");
    pthread_mutex_unlock(mutex);
    exit(EXIT_FAILURE);
  }
  p->id = id;
  p->player1_id = fi;
  p->player2_id = si;
  p->player1 = f;
  p->go = fi;
  p->player2 = s;
  p->owner = own;
  strcpy(p->field, "AAAAAAAAA");
  if (p->player1 != NULL)
    p->player1->game = id;
  if (p->player2 != NULL)
    p->player2->game = id;
  if (*root == NULL) {
    *root = p;
  } else {
    p->next = *root;
    *root = p;
  }
  fprintf(stderr,
          "Add game [id: %d | first: %d | second: %d] \n",
          p->id,
          p->player1_id,
          p->player2_id);
  games_curr++;

  sprintf(name, "%d.game", p->id);
  fd = open(name, O_WRONLY | O_CREAT, 0666);
  write(fd, &p->id, sizeof(int));
  write(fd, &p->owner, sizeof(int));
  write(fd, &p->player1_id, sizeof(int)); // X
  write(fd, &p->player2_id, sizeof(int)); // O
  write(fd, &p->go, sizeof(int));
  write(fd, "AAAAAAAAA", strlen("AAAAAAAAA") + 1);
  close(fd);
  pthread_mutex_unlock(mutex);
  return p;
  // print_games(root, mutex);
}
/*----------------------------------------------------------------------------*/
int remove_game(
        games_t **root, int id, int win, int lose, pthread_mutex_t *mutex) {
  games_t *prev = NULL;
  games_t *p = *root;
  char cmd[20];
  pthread_mutex_lock(mutex);
  if (*root == NULL) {
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
        fprintf(stderr,
                "Remove game [id: %d | win: %d | lose %d] \n",
                p->id,
                win,
                lose);
        sprintf(cmd, "rm -f %d.game", p->id);
        system(cmd);
        free(p);
        pthread_mutex_unlock(mutex);
        // print_games(root, mutex);
        --games_curr;
        return 0;
      } else {
        *root = p->next;
        fprintf(stderr,
                "Remove game [id: %d | win: %d | lose %d] \n",
                p->id,
                win,
                lose);
        sprintf(cmd, "rm -f %d.game", p->id);
        system(cmd);
        free(p);
        pthread_mutex_unlock(mutex);
        // print_games(root, mutex);
        --games_curr;
        return 0;
      }
    }
    prev = p;
    p = p->next;
  }
  pthread_mutex_unlock(mutex);
  // print_games(root, mutex);
  return 1;
}
/*---------------------------------------------------------------------------*/
int remove_client(clients_t **root, pid_t pid, pthread_mutex_t *mutex) {
  clients_t *prev = NULL;
  clients_t *p = *root;
  int id = -1;
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
    memcpy(p.buffer, buff, MAXDATASIZE);
  } else {
    p.buffer[0] = '\0';
  }
  return p;
}
/*---------------------------------------------------------------------------*/
int send_ack(int client_id, int packet_id) {
  packet_t p = make_packet(CONN_ACK, client_id, packet_id, NULL);

  pthread_mutex_lock(&connection_mutex);
  send(client_socket_write, &p, sizeof(p), 0);
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
