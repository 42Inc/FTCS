#include "./../include/main.h"

int games = GAMES;
pid_t *pids = NULL;
extern char hostname[MAXDATASIZE];
extern int port;
int server_socket_read = -1;
int server_socket_write = -1;
static int games_available = GAMES;
extern int client_socket_read;
extern int writer_join;
extern int reader_join;
extern int client_socket_write;
extern int state_connection;
extern packets_t *reader_buffer;
extern packets_t *writer_buffer;
extern int reader_buffer_len;
extern int writer_buffer_len;
extern int ack_id;
extern pthread_mutex_t connection_mutex;
extern pthread_mutex_t reader_mutex;
extern pthread_mutex_t writer_mutex;
extern pthread_mutex_t helper_mutex;
extern srv_pool_t *known_servers;
struct sigaction child;
sigset_t setchild;
struct sigaction usr1;
sigset_t setusr1;
pid_t mainpid = -1;
srv_t *this_server = NULL;
int shutdown_server = 0;
clients_t *cl_pids = NULL;
clients_t *se_pids = NULL;
games_t *chumbers = NULL;
char field[10] = "AAAAAAAAA";
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
long int clients_available = 0;

void sigchld_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;
  int id = -1;
  clients_t *cursor = se_pids;

  pthread_mutex_lock(&mut);
  if (pid > 0) {
    while (cursor != NULL) {
      if (pid == cursor->pid) {
        id = cursor->id;
        break;
      }
      cursor = cursor->next;
    }
    if ((id = disconnect_client(&cl_pids, pid)) > known_servers->count + 1) {
      fprintf(stderr, "Disconnect client[id - %d|pid - %d]\n", id, pid);
    } else if (id >= 0) {
      fprintf(stderr, "Disconnect server[id - %d|pid - %d]\n", id, pid);
    } else
      fprintf(stderr, "Ignore signal from [pid - %d]\n", pid);
  }
  pthread_mutex_unlock(&mut);
}

void sigusr1_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;
  fprintf(stderr, "Respond SIGUSR1 [pid - %d]\n", pid);
}

void *server_reader() {
  int recv_result;
  int poll_return;
  struct pollfd pfd;
  packet_t p;

//  fprintf(stderr, "Start server Reader!\n");
server_reader_start:
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

void *server_writer() {
  int send_result;
  int trying_send;
  packet_t p;
//  fprintf(stderr, "Start server Writer!\n");
server_writer_start:
  send_result = 0;
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
        send_result = send(client_socket_write, &p, sizeof(packet_t), 0);
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

void *manager() {
  if (clients_available > 0 && !(clients_available % 2)) {}
}

void client_connection() {
  pid_t pid;
  int client_id;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  state_connection = CONN_TRUE;
  pid = getpid();
  pthread_attr_init(&reader_attr);
  pthread_attr_init(&writer_attr);
  pthread_create(&reader_tid, &reader_attr, server_reader, NULL);
  pthread_create(&writer_tid, &writer_attr, server_writer, NULL);
  while (check_connection()) {
    packet_t p;
    int state = get_packet(&p);
    if (state == TRUE) {
      if (p.type == CONN_NEW) {
        send_packet(make_packet(CONN_EST, p.client_id, rand() % 1000, NULL));
      } else if (p.type == CONN_SERVER) {
        fprintf(stderr,
                "Respond connection from other server[id - %d].%d %s\n",
                p.client_id,
                p.type,
                p.buffer);
        kill(mainpid, SIGUSR1);
      } else if (p.type == CONN_CLIENT) {
        client_id = rand() % (10 + known_servers->count + 1) +
                known_servers->count + 1;
        if (p.client_id > 0) {
          client_id = p.client_id;
        }
        send_packet(make_packet(SERVICE, client_id, rand() % 1000, NULL));
        fprintf(stderr, "Respond connection from client[id - %d]\n", client_id);
        kill(mainpid, SIGUSR1);
      }
      fprintf(stderr,
              "Main %d %d %d %s\n",
              p.type,
              p.packet_id,
              p.client_id,
              p.buffer);
    }
  }

  printf("Disconnect\n");
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

int main(int argc, char **argv) {
  int opt;
  pid_t pid;
  int recv_result = 0;
  struct sockaddr_in my_addr;       // host addr
  struct sockaddr_in their_addr[2]; // client
  socklen_t sin_size[2];

  mainpid = getpid();
  reader_buffer = (packets_t *)malloc(sizeof(packets_t));
  writer_buffer = (packets_t *)malloc(sizeof(packets_t));
  memset(reader_buffer, 0, sizeof(packets_t));
  memset(writer_buffer, 0, sizeof(packets_t));
  opterr = 0;

  sigemptyset(&setchild);
  sigaddset(&setchild, SIGCHLD);
  child.sa_sigaction = sigchld_handler;
  child.sa_mask = setchild;
  child.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  if (sigaction(SIGCHLD, &child, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  sigemptyset(&setusr1);
  sigaddset(&setusr1, SIGUSR1);
  usr1.sa_sigaction = sigusr1_handler;
  usr1.sa_mask = setusr1;
  usr1.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGUSR1, &usr1, NULL) == -1) {
    perror("sigaction");
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
  create_connections_to_servers();

  int count = 0;
  while (!shutdown_server) {
    client_socket_read = accept_tcp_connection(server_socket_read);
    client_socket_write = accept_tcp_connection(server_socket_write);
    // Child process
    if (!(pid = fork())) {
      client_connection();
    } else {
      fprintf(stderr, "PID connection : %d\n", pid);
      add_client(&cl_pids, pid, 0);
    }
    // Parent doesn.t need this
    close(client_socket_read);
    close(client_socket_write);
  }
  return 0;
}

void server_connection(srv_t *cursor) {
  packet_t p;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  pid_t pid;
  pid = getpid();
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
    printf("Connection fail.\n");
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
      send_packet(
              make_packet(SERVICE, this_server->number, rand() % 1000, NULL));
      sleep(2);
    }
    goto connect_to_other_server;
  }
  if (writer_tid != -1) {
    pthread_join(writer_tid, NULL);
    fprintf(stderr, "Writer join\n");
  }
  if (reader_tid != -1) {
    pthread_join(reader_tid, NULL);
    fprintf(stderr, "Reader join\n");
  }
  close(client_socket_read);
  close(client_socket_write);
  exit(0);
}

void create_connections_to_servers() {
  remove_this_server_from_list();
  int i = 0;
  pids = (pid_t *)malloc(sizeof(pid_t) * known_servers->count);
  srv_t *cursor = known_servers->srvs;
  for (i = 0; i < known_servers->count; ++i) {
    if (!(pids[i] = fork())) {
      server_connection(cursor);
    } else {
      add_client(&se_pids, pids[i], cursor->number);
    }
    cursor = cursor->next;
  }
}

void remove_this_server_from_list() {
  int c = known_servers->count;
  int i;
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
