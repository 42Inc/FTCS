#include "./../include/main.h"

extern int port;
extern char hostname[MAXDATASIZE];
extern int writer_join;
extern int reader_join;
extern int client_socket_read;
extern int client_socket_write;
extern int state_connection;
extern packets_t *reader_buffer;
extern packets_t *writer_buffer;
extern int ack_id;
extern pthread_mutex_t connection_mutex;
extern pthread_mutex_t reader_mutex;
extern pthread_mutex_t msq_mutex;
extern pthread_mutex_t writer_mutex;
extern pthread_mutex_t helper_mutex;
extern pthread_mutex_t clients_mutex;
extern pthread_mutex_t servers_mutex;
extern pthread_mutex_t games_mutex;
extern srv_pool_t *known_servers;
struct sigaction child;
sigset_t setchild;
struct sigaction usr1;
sigset_t setusr1;
struct sigaction usr2;
sigset_t setusr2;
int games = GAMES;
pid_t *pids = NULL;
int server_socket_read = -1;
int server_socket_write = -1;
pid_t mainpid = -1;
srv_t *this_server = NULL;
int shutdown_server = 0;
clients_t *cl_pids = NULL;
clients_t *se_pids = NULL;
ipc_t *msq = NULL;
games_t *chumbers = NULL;
char field[10] = "AAAAAAAAA";
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
long int clients_available = 0;
int manager_join = 0;
int regenerate_client_id = FALSE;
games_t *game_pids = NULL;
message_t sbuf;
message_t rbuf;
/*---------------------------------------------------------------------------*/
void sigchld_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;
  int sid = -1;
  int id = -1;
  ipc_t *p;
  pthread_mutex_lock(&mut);
  if (pid > 0) {
    p = get_msq_pid(&msq, pid, &msq_mutex);
    if (p == NULL)
      fprintf(stderr, "Ignore SIGCHLD [pid: %d] [%d %d]\n", pid, sid, id);
    else {
      clients_available =
              (clients_available - 1) < 0 ? 0 : (clients_available - 1);
      fprintf(stderr,
              "Respond SIGCHLD [pid: %d | clients: %ld]\n",
              pid,
              clients_available);
      remove_msq(&msq, pid, &msq_mutex);
    }
  }
  pthread_mutex_unlock(&mut);
}
/*---------------------------------------------------------------------------*/
void sigusr1_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;
  int id = -1;
  ipc_t *p;
  ipc_t *ptr;

  fprintf(stderr, "Respond SIGUSR1 [pid: %d | target: %d]\n", pid, getpid());
  if (pid == mainpid) {
  } else {
    p = get_msq_pid(&msq, pid, &msq_mutex);
    msgrcv(p->msqid, &rbuf, sizeof(message_t), NONE_CMD, 0);
    fprintf(stderr, "Receive msg [fd: %d | msg: %s]\n", p->msqid, rbuf.mtext);
    while (TRUE) {
      id = getrand(known_servers->count + 1, known_servers->count + 21);
      ptr = get_msq_id(&msq, id, &msq_mutex);
      fprintf(stderr, "Check [id: %d | ptr: %p]\n", id, ptr);
      if (!ptr)
        break;
    }
    p->id = id;
    fprintf(stderr, "Set [id: %d]\n", p->id);
    if (!strcmp("get_id", rbuf.mtext)) {
      sbuf.mtype = SET_ID;
      sprintf(sbuf.mtext, "%d", p->id);
      msgsnd(p->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
      fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", p->msqid, sbuf.mtext);
    }
  }
}
/*---------------------------------------------------------------------------*/
void sigusr2_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;

  fprintf(stderr, "Respond SIGUSR2 [pid: %d | target: %d]\n", pid, getpid());
  fprintf(stderr, "Ignore SIGUSR2 [pid: %d | target: %d]\n", pid, getpid());
}
/*---------------------------------------------------------------------------*/
void *server_reader() {
  int recv_result;
  int poll_return;
  struct pollfd pfd;
  packet_t p;

  recv_result = 0;
  poll_return = 0;
  pfd.fd = client_socket_read;
  pfd.events = POLLIN | POLLHUP | POLLRDNORM;
  while (1) {
    while (!check_connection())
      if (reader_join)
        return NULL;
    pthread_mutex_lock(&connection_mutex);
    pfd.fd = client_socket_read;
    pthread_mutex_unlock(&connection_mutex);
    if ((poll_return = poll(&pfd, 1, 100)) > 0) {
      pthread_mutex_lock(&reader_mutex);
      pthread_mutex_lock(&connection_mutex);
      recv_result = recv(client_socket_read, &p, sizeof(packet_t), 0);
      pthread_mutex_unlock(&connection_mutex);

      if (recv_result <= 0) {
        pthread_mutex_lock(&connection_mutex);
        state_connection = CONN_FALSE;
        pthread_mutex_unlock(&connection_mutex);
      } else {
        if (p.type != CONN_ACK) {
          push_queue(p, &reader_buffer);
          send_ack(p.client_id, p.packet_id);
        } else {
          ack_id = p.packet_id;
        }
      }
      pthread_mutex_unlock(&reader_mutex);
    }
  }
}
/*---------------------------------------------------------------------------*/
void *server_writer() {
  int trying_send;
  packet_t p;

  trying_send = 0;
  while (1) {
    while (!check_connection()) {
      if (writer_join)
        return NULL;
    }
    pthread_mutex_lock(&writer_mutex);
    if (writer_buffer->len > 0) {
      p = pop_queue(&writer_buffer);
    sending:
      if (p.type != NONE) {
        pthread_mutex_lock(&connection_mutex);
        send(client_socket_write, &p, sizeof(packet_t), 0);
        pthread_mutex_unlock(&connection_mutex);
        if (p.type == CONN_ACK || wait_ack(p.packet_id)) {
          trying_send = 0;
        } else if (trying_send >= 10) {
          pthread_mutex_lock(&connection_mutex);
          state_connection = FALSE;
          pthread_mutex_unlock(&connection_mutex);
          trying_send = 0;
        } else {
          ++trying_send;
          goto sending;
        }
      }
    }
    pthread_mutex_unlock(&writer_mutex);
  }
}
/*---------------------------------------------------------------------------*/
int create_server_tcp_socket(unsigned int ip, int port) {
  struct sockaddr_in server;
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == -1) {
    fprintf(stderr, "Could not create socket\n");
    exit(EXIT_FAILURE);
  }
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = ip;
  server.sin_port = htons(port);

  if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Bind failed\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sock, BACKLOG) < 0) {
    fprintf(stderr, "Listen failed\n");
    exit(EXIT_FAILURE);
  }

  return sock;
}
/*---------------------------------------------------------------------------*/
int accept_tcp_connection(int server_socket) {
  int connection;
  struct sockaddr_in connection_addr;
  unsigned int len = sizeof(connection_addr);
  if ((connection = accept(
               server_socket, (struct sockaddr *)&connection_addr, &len)) < 0) {
    fprintf(stderr, "Accept failed\n");
    clients_t *cursor = cl_pids;
    while (cursor != NULL) {
      kill(cursor->pid, SIGKILL);
      cursor = cursor->next;
    }
    cursor = se_pids;
    while (cursor != NULL) {
      kill(cursor->pid, SIGKILL);
      cursor = cursor->next;
    }
    exit(EXIT_FAILURE);
  }
  fprintf(stdout,
          "Client conneted, ip: %s\n",
          inet_ntoa(connection_addr.sin_addr));
  return connection;
}
/*---------------------------------------------------------------------------*/
void *manager() {
  return NULL;
}
/*---------------------------------------------------------------------------*/
void client_connection() {
  pid_t pid;
  int client_id = -1;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  int msqid;
  key_t key;
  char str[MAXDATASIZE];

  state_connection = CONN_TRUE;
  pid = getpid();
  key = pid;
  if ((msqid = msgget(key, 0666)) < 0) {
    fprintf(stderr, "msgget");
    exit(1);
  }
  pthread_attr_init(&reader_attr);
  pthread_attr_init(&writer_attr);
  pthread_create(&reader_tid, &reader_attr, server_reader, NULL);
  pthread_create(&writer_tid, &writer_attr, server_writer, NULL);
  srand(time(NULL));
  while (check_connection()) {
    packet_t p;
    int state = get_packet(&p);
    if (regenerate_client_id)
      regenerate_client_id = FALSE;
    client_id = -1;
    if (state == TRUE) {
      if (p.type == CONN_NEW) {
        send_packet(make_packet(CONN_EST, p.client_id, rand() % 1000, NULL));
      } else if (p.type == CONN_SERVER) {
        fprintf(stderr,
                "Respond connection from other server[id: %d | pid: %d]\n",
                p.client_id,
                getpid());
        client_id = p.client_id;
      } else if (p.type == CONN_CLIENT) {
        if (p.client_id > 0) {
          client_id = p.client_id;
        }
        if (client_id == -1) {
          sbuf.mtype = NONE_CMD;
          strcpy(sbuf.mtext, "get_id");
          msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
          fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", msqid, sbuf.mtext);
          kill(mainpid, SIGUSR1);
          msgrcv(msqid, &rbuf, MAXDATASIZE, SET_ID, 0);
          fprintf(stderr,
                  "Receive msg [fd: %d | msg: %s]\n",
                  msqid,
                  rbuf.mtext);
          client_id = atoi(rbuf.mtext);
        }
        send_packet(make_packet(SERVICE, client_id, rand() % 1000, "set_id"));
        fprintf(stderr,
                "Respond connection from client[id: %d | pid: %d]\n",
                client_id,
                getpid());
      } else if (p.type == MSG) {
        fprintf(stderr,
                "Receive message from client [id: %d]: %s\n",
                p.client_id,
                p.buffer);
      } else if (p.type == CHANGE_FIELD) {
        fprintf(stderr,
                "Receive request to change field from client [id: %d | pos: "
                "%s]\n",
                p.client_id,
                p.buffer);
        client_id = p.client_id;
        kill(mainpid, SIGUSR1);
      } else if (p.type == SERVICE) {
        /*
                  fprintf(stderr,
                        "Receive SERVICE packet [id: %d | msg: %s]\n",
                        p.client_id, p.buffer);
        */
      }
    }
  }

  //  printf("Disconnect\n");
  close(client_socket_read);
  close(client_socket_write);
  if (writer_tid != -1) {
    writer_join = 1;
    pthread_join(writer_tid, NULL);
  }
  if (reader_tid != -1) {
    reader_join = 1;
    pthread_join(reader_tid, NULL);
  }
  exit(0);
}
/*---------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  int opt;
  pid_t pid;

  pthread_t manager_tid = -1;
  pthread_attr_t manager_attr;
  mainpid = getpid();
  fprintf(stderr, "Main process [pid: %d]\n", mainpid);
  reader_buffer = (packets_t *)malloc(sizeof(packets_t));
  writer_buffer = (packets_t *)malloc(sizeof(packets_t));
  memset(reader_buffer, 0, sizeof(packets_t));
  memset(writer_buffer, 0, sizeof(packets_t));
  opterr = 0;

  sigemptyset(&setchild);
  sigaddset(&setchild, SIGCHLD);
  child.sa_sigaction = sigchld_handler;
  child.sa_mask = setchild;
  child.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGCHLD, &child, NULL) == -1) {
    fprintf(stderr, "SIGCHLD");
    exit(1);
  }

  sigemptyset(&setusr1);
  sigaddset(&setusr1, SIGUSR1);
  usr1.sa_sigaction = sigusr1_handler;
  usr1.sa_mask = setusr1;
  usr1.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGUSR1, &usr1, NULL) == -1) {
    fprintf(stderr, "SIGUSR1");
    exit(1);
  }

  sigemptyset(&setusr2);
  sigaddset(&setusr2, SIGUSR2);
  usr2.sa_sigaction = sigusr2_handler;
  usr2.sa_mask = setusr2;
  usr2.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGUSR2, &usr2, NULL) == -1) {
    fprintf(stderr, "SIGUSR2");
    exit(1);
  }

  while ((opt = getopt(argc, argv, "p:g:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    case 'g':
      games = atoi(optarg);
      break;
    }
  }

  read_servers_pool("ippool.dat");

  printf("Listening ports : %d-%d\n", port, port + 1);
  printf("Games: %d\n", games);
  server_socket_read = create_server_tcp_socket(htonl(INADDR_ANY), port);
  server_socket_write = create_server_tcp_socket(htonl(INADDR_ANY), port + 1);

  pthread_attr_init(&manager_attr);
  pthread_create(&manager_tid, &manager_attr, manager, NULL);
  create_connections_to_servers();
  while (!shutdown_server) {
    //    while (clients >= games * 2);
    client_socket_read = accept_tcp_connection(server_socket_read);
    client_socket_write = accept_tcp_connection(server_socket_write);
    // Child process
    if (!(pid = fork())) {
      client_connection();
    } else {
      add_msq(&msq, pid, FALSE, &msq_mutex);
    }
    // Parent doesn.t need this
    close(client_socket_read);
    close(client_socket_write);
  }
  if (manager_tid != -1) {
    manager_join = 1;
    pthread_join(manager_tid, NULL);
    fprintf(stderr, "Manager join\n");
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void server_connection(srv_t *cursor) {
  packet_t p;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;

  fprintf(stderr,
          "Trying connect to server[%d] %s:%d\n",
          cursor->number,
          cursor->ip,
          cursor->port);
connect_to_other_server:
  client_socket_write =
          client_tcp_connect(gethostbyname(cursor->ip), cursor->port);
  client_socket_read =
          client_tcp_connect(gethostbyname(cursor->ip), cursor->port + 1);
  if (client_socket_write == -1 || client_socket_read == -1) {
    //  printf("Connection fail.\n");
    sleep(5);
    goto connect_to_other_server;
  } else {
    state_connection = CONN_TRUE;
    if (reader_tid == -1 && writer_tid == -1) {
      pthread_attr_init(&reader_attr);
      pthread_attr_init(&writer_attr);
      pthread_create(&reader_tid, &reader_attr, server_reader, NULL);
      pthread_create(&writer_tid, &writer_attr, server_writer, NULL);
    }
    send_packet(
            make_packet(CONN_NEW, this_server->number, rand() % 1000, NULL));
    while (1) {
      while (!get_packet(&p))
        ;
      if (p.type == CONN_EST) {
        send_packet(make_packet(
                CONN_SERVER, this_server->number, rand() % 1000, NULL));
        break;
      }
    }
    while (check_connection()) {
      if (get_packet(&p)) {
        // Place inter-server-communication here.
      }
      send_packet(make_packet(
              SERVICE, this_server->number, rand() % 1000, "Check"));
      sleep(2);
    }
    goto connect_to_other_server;
  }
  if (writer_tid != -1) {
    writer_join = 1;
    pthread_join(writer_tid, NULL);
    fprintf(stderr, "Writer join\n");
  }
  if (reader_tid != -1) {
    reader_join = 1;
    pthread_join(reader_tid, NULL);
    fprintf(stderr, "Reader join\n");
  }
  close(client_socket_read);
  close(client_socket_write);
  exit(0);
}
/*---------------------------------------------------------------------------*/
void create_connections_to_servers() {
  remove_this_server_from_list();
  int i = 0;
  pids = (pid_t *)malloc(sizeof(pid_t) * known_servers->count);
  srv_t *cursor = known_servers->srvs;
  for (i = 0; i < known_servers->count; ++i) {
    if (!(pids[i] = fork())) {
      server_connection(cursor);
    } else {
      add_msq(&msq, pids[i], TRUE, &msq_mutex);
    }
    cursor = cursor->next;
  }
}
/*---------------------------------------------------------------------------*/
void remove_this_server_from_list() {
  srv_t *prev = NULL;
  srv_t *cursor = known_servers->srvs;
  this_server = (srv_t *)malloc(sizeof(srv_t));
  while (cursor != NULL) {
    if (cursor->port == port && !(strcmp(hostname, cursor->ip))) {
      fprintf(stderr,
              "Remove Server[%d] %s:%d\n",
              cursor->number,
              cursor->ip,
              cursor->port);
      memcpy(this_server, cursor, sizeof(srv_t));
      if (prev == NULL) {
        known_servers->srvs = cursor->next;
        known_servers->count--;
        free(cursor);
        return;
      } else {
        prev->next = cursor->next;
        known_servers->count--;
        free(cursor);
        return;
      }
    }
    prev = cursor;
    cursor = cursor->next;
  }
}
/*---------------------------------------------------------------------------*/
