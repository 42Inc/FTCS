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
extern int games_curr;
extern pthread_mutex_t connection_mutex;
extern pthread_mutex_t reader_mutex;
extern pthread_mutex_t msq_mutex;
extern pthread_mutex_t writer_mutex;
extern pthread_mutex_t games_mutex;
extern pthread_mutex_t clients_av_mutex;
extern srv_pool_t *known_servers;

struct sigaction child;
sigset_t setchild;
struct sigaction usr1;
sigset_t setusr1;
struct sigaction usr2;
sigset_t setusr2;

int client_id = -1;
int msqid = -1;
int games = GAMES;
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
long int clients = 0;
int manager_join = 0;
int regenerate_client_id = FALSE;
games_t *game_ch = NULL;
message_t sbuf;
message_t rbuf;
int sincronize_queue[GAMES];
int sincronize_queue_i = 0;
/*---------------------------------------------------------------------------*/
void sigchld_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;
  int sid = -1;
  int id = -1;
  int w = -1;
  int l = -1;
  ipc_t *p = NULL;
  games_t *g = NULL;
  pthread_mutex_lock(&mut);
  if (pid > 0) {
    p = get_msq_pid(&msq, pid, FALSE, &msq_mutex);
    if (p == NULL)
      fprintf(stderr, "Ignore SIGCHLD [pid: %d] [%d %d]\n", pid, sid, id);
    else {
      pthread_mutex_lock(&clients_av_mutex);
      clients_available =
              (clients_available - 1) < 0 ? 0 : (clients_available - 1);
      clients = (clients - 1) < 0 ? 0 : (clients - 1);
      pthread_mutex_unlock(&clients_av_mutex);
      fprintf(stderr,
              "Respond SIGCHLD [pid: %d | clients: %ld(%ld)]\n",
              pid,
              clients_available,
              clients);
      if (p->game > 0) {
        g = get_game_id(&game_ch, p->game, &games_mutex);
        if (g != NULL) {
          if (pid == g->player1->pid) {
            // Win player2
            sbuf.mtype = WIN_END_GAME;
            sprintf(sbuf.mtext, "win");
            if (g->player2 != NULL) {
              msgsnd(g->player2->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
              /*  fprintf(stderr, "Send msg [fd: %d | msg: %s]\n",
                 g->player2->msqid, sbuf.mtext);*/
              id = g->id;
              w = g->player2_id;
              l = g->player1_id;
              kill(g->player2->pid, SIGUSR1);
            }
            remove_game(
                    &game_ch,
                    g->id,
                    g->player2_id,
                    g->player1_id,
                    &games_mutex);
            synchronized_game_w(id, w, l);
          } else if (pid == g->player2->pid) {
            // Win player1
            sbuf.mtype = WIN_END_GAME;
            sprintf(sbuf.mtext, "win");
            if (g->player1 != NULL) {
              msgsnd(g->player1->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
              /*    fprintf(stderr, "Send msg [fd: %d | msg: %s]\n",
                 g->player1->msqid, sbuf.mtext);*/
              id = g->id;
              w = g->player1_id;
              l = g->player2_id;
              kill(g->player1->pid, SIGUSR1);
            }
            remove_game(
                    &game_ch,
                    g->id,
                    g->player1_id,
                    g->player2_id,
                    &games_mutex);
            synchronized_game_w(id, w, l);
          }
        }
      }
      remove_msq(&msq, pid, &msq_mutex);
    }
  }
  pthread_mutex_unlock(&mut);
}
/*---------------------------------------------------------------------------*/
void sigusr1_handler(int s, siginfo_t *info, void *param) {
  pid_t pid = info->si_pid;
  int id = -1;
  int fd = -1;
  int fi = -1;
  int si = -1;
  int own = -1;
  ipc_t *p = NULL;
  games_t *g = NULL;
  ipc_t *ptr = NULL;
  char buffer[MAXDATASIZE];
  fprintf(stderr,
          "Respond SIGUSR1 [pid: %d | target: %d | main: %d]\n",
          pid,
          getpid(),
          mainpid);
  if (pid == mainpid) {
    msgrcv(msqid, &rbuf, sizeof(message_t), 0, 0);
    /*    fprintf(stderr, "Receive msg [fd: %d | msg: %s | type: %ld]\n", msqid,
                rbuf.mtext, rbuf.mtype);*/
    if (rbuf.mtype == WIN_END_GAME) {
      fprintf(stderr, "Server send win [id: %d]\n", msqid);
      send_packet(make_packet(SERVICE, id, 0, "winner"));
    } else if (rbuf.mtype == SYNCHRONIZE_GAME) {
      fprintf(stderr,
              "Request for sinchronisation [pid: %d | id: %d | state: %d]\n",
              getpid(),
              atoi(rbuf.mtext),
              check_connection());
      if (check_connection()) {
        sprintf(buffer, "%d.game", atoi(rbuf.mtext));
        if (!access(buffer, 0)) {
          fd = open(buffer, O_RDONLY);
          read(fd, buffer, SIZE_GAME);
          close(fd);
          send_packet(make_packet(
                  SYNCHRONIZE, this_server->number, atoi(rbuf.mtext), buffer));
        } else {
          sscanf(rbuf.mtext, "%d %d %d", &id, &fi, &si);
          sprintf(buffer, "%d %d %d", id, fi, si);
          send_packet(make_packet(
                  SYNCHRONIZE_REMOVE, this_server->number, id, buffer));
        }
        fprintf(stderr,
                "Synchronize! [pid: %d | id: %d]\n",
                getpid(),
                atoi(rbuf.mtext));
      } else {
        fprintf(stderr,
                "Reject! [pid: %d | id: %d]\n",
                getpid(),
                atoi(rbuf.mtext));
      }
    } else if (rbuf.mtype == START_GAME) {
      fprintf(stderr,
              "Connect to game [pid: %d | id: %d]\n",
              getpid(),
              atoi(rbuf.mtext));
      send_packet(
              make_packet(SERVICE, client_id, atoi(rbuf.mtext), "start_game"));
    }
  } else {
    p = get_msq_pid(&msq, pid, FALSE, &msq_mutex);
    msgrcv(p->msqid, &rbuf, sizeof(message_t), 0, 0);
    fprintf(stderr,
            "Receive msg [fd: %d | msg: %s | type: %ld]\n",
            p->msqid,
            rbuf.mtext,
            rbuf.mtype);

    if (rbuf.mtype == NONE_CMD) {
      if (!strcmp(rbuf.mtext, "get_id")) {
        while (TRUE) {
          id = getrand(known_servers->count + 1, known_servers->count + 21);
          ptr = get_msq_id(&msq, id, FALSE, &msq_mutex);
          fprintf(stderr, "Check [id: %d | ptr: %p]\n", id, ptr);
          if (!ptr)
            break;
        }
        p->id = id;
        fprintf(stderr, "Set [id: %d]\n", p->id);
        if (id > known_servers->count) {
          pthread_mutex_lock(&clients_av_mutex);
          ++clients_available;
          ++clients;
          pthread_mutex_unlock(&clients_av_mutex);
        }
        sbuf.mtype = SET_ID;
        sprintf(sbuf.mtext, "%d", p->id);
        msgsnd(p->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
        /*  fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", p->msqid,
         * sbuf.mtext);*/
        fprintf(stderr,
                "Clients [count: %ld(%ld)]\n",
                clients_available,
                clients);
      }
    } else if (rbuf.mtype == RECONNECT_ID) {
      id = atoi(rbuf.mtext);
      fprintf(stderr, "Reconnect id [id: %d]\n", id);
      if (p != NULL)
        p->id = id;
      pthread_mutex_lock(&clients_av_mutex);
      ++clients_available;
      ++clients;
      pthread_mutex_unlock(&clients_av_mutex);
      print_msq(&msq, &msq_mutex);
    } else if (rbuf.mtype == SERVER_SET_ID) {
      id = atoi(rbuf.mtext);
      p = get_msq_pid(&msq, pid, FALSE, &msq_mutex);
      p->id = id;
      fprintf(stderr, "Server set id [id: %d]\n", p->id);
    } else if (rbuf.mtype == SYNCHRONIZE_CHANGE) {
      sscanf(rbuf.mtext, "%d %d %d %d", &id, &fi, &si, &own);
      //      id = atoi(rbuf.mtext);
      fprintf(stderr, "Synchronize... [id: %d]\n", id);
      g = get_game_id(&game_ch, id, &games_mutex);
      if (g == NULL) {
        add_game(&game_ch, id, NULL, NULL, fi, si, own, &games_mutex);
      }
    } else if (rbuf.mtype == SYNCHRONIZE_RM) {
      sscanf(rbuf.mtext, "%d %d %d", &id, &fi, &si);
      fprintf(stderr, "Synchronize... [id: %d]\n", id);
      remove_game(&game_ch, id, fi, si, &games_mutex);
    } else if (rbuf.mtype == RECONNECT_GAME) {
      sscanf(rbuf.mtext, "%d %d", &fi, &id);
      fprintf(stderr, "Connect client to game [game: %d | id: %d]\n", id, fi);
      g = get_game_id(&game_ch, id, &games_mutex);
      if (g != NULL) {
        if (g->player1_id == fi) {
          p = get_msq_id(&msq, fi, FALSE, &msq_mutex);
          msq_set_game(&p, g->id, &msq_mutex);
          g->player1 = p;
        } else if (g->player2_id == fi) {
          p = get_msq_id(&msq, fi, FALSE, &msq_mutex);
          msq_set_game(&p, g->id, &msq_mutex);
          g->player2 = p;
        }
      } else {
        msq_set_game(&p, -1, &msq_mutex);
      }
    }
    print_msq(&msq, &msq_mutex);
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
void synchronized_game(int game_id) {
  srv_t *cursor = known_servers->srvs;
  ipc_t *p;
  int i = 0;
  for (i = 0; i < known_servers->count; ++i) {
    p = get_msq_id(&msq, cursor->number, TRUE, &msq_mutex);
    sbuf.mtype = SYNCHRONIZE_GAME;
    sprintf(sbuf.mtext, "%d", game_id);
    msgsnd(p->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
    /*  fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", p->msqid,
     * sbuf.mtext);*/
    kill(p->pid, SIGUSR1);
    cursor = cursor->next;
  }
}
/*---------------------------------------------------------------------------*/
void synchronized_game_w(int game_id, int w, int l) {
  srv_t *cursor = known_servers->srvs;
  ipc_t *p;
  int i = 0;
  for (i = 0; i < known_servers->count; ++i) {
    p = get_msq_id(&msq, cursor->number, TRUE, &msq_mutex);
    sbuf.mtype = SYNCHRONIZE_GAME;
    sprintf(sbuf.mtext, "%d %d %d", game_id, w, l);
    msgsnd(p->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
    /*    fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", p->msqid,
     sbuf.mtext);*/
    kill(p->pid, SIGUSR1);
    cursor = cursor->next;
  }
}
/*---------------------------------------------------------------------------*/
void *manager() {
  int game_id = -1;
  ipc_t *f = NULL;
  ipc_t *s = NULL;
  while (!manager_join) {
    sleep(1);
    if (clients_available > 1 && games_curr <= GAMES) {
      game_id = rand() % 100;
      get_free_pair_msq(&msq, &f, &s, &msq_mutex);
      add_game(
              &game_ch,
              game_id,
              f,
              s,
              f->id,
              s->id,
              this_server->number,
              &games_mutex);

      sbuf.mtype = START_GAME;
      sprintf(sbuf.mtext, "%d", game_id);
      msgsnd(f->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
      kill(f->pid, SIGUSR1);
      sbuf.mtype = START_GAME;
      sprintf(sbuf.mtext, "%d", game_id);
      msgsnd(s->msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
      kill(s->pid, SIGUSR1);
      pthread_mutex_lock(&clients_av_mutex);
      clients_available -= 2;
      pthread_mutex_unlock(&clients_av_mutex);
      print_msq(&msq, &msq_mutex);
      synchronized_game(game_id);
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
void client_connection() {
  pid_t pid;
  int fd = -1;
  int game_id = -1;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  key_t key;
  char str[MAXDATASIZE];
  int t[5] = {0};
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
        sbuf.mtype = SERVER_SET_ID;
        sprintf(sbuf.mtext, "%d", client_id);
        msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
        kill(mainpid, SIGUSR1);
      } else if (p.type == CONN_CLIENT) {
        if (p.client_id > 0) {
          client_id = p.client_id;
          if (p.packet_id > 0) {
            game_id = p.packet_id;
            sbuf.mtype = RECONNECT_GAME;
            sprintf(sbuf.mtext, "%d %d", client_id, game_id);
            msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
            kill(mainpid, SIGUSR1);
          }
        }
        if (client_id == -1) {
          sbuf.mtype = NONE_CMD;
          strcpy(sbuf.mtext, "get_id");
          msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
          //          fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", msqid,
          //          sbuf.mtext);
          kill(mainpid, SIGUSR1);
          msgrcv(msqid, &rbuf, MAXDATASIZE, SET_ID, 0);
          fprintf(stderr,
                  "Receive msg [fd: %d | msg: %s | type: %ld]\n",
                  msqid,
                  rbuf.mtext,
                  rbuf.mtype);
          client_id = atoi(rbuf.mtext);
        } else {
          sbuf.mtype = RECONNECT_ID;
          sprintf(sbuf.mtext, "%d", client_id);
          msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
          fprintf(stderr, "Send msg [fd: %d | msg: %s]\n", msqid, sbuf.mtext);
          kill(mainpid, SIGUSR1);
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
      } else if (p.type == SYNCHRONIZE) {
        fprintf(stderr,
                "Receive SYNCHRONIZE packet [id: %d | game: %d | msg: %s]\n",
                p.client_id,
                p.packet_id,
                p.buffer);
        sprintf(str, "%d.game", p.packet_id);
        fd = open(str, O_WRONLY | O_CREAT, 0666);
        write(fd, p.buffer, SIZE_GAME);
        close(fd);
        sbuf.mtype = SYNCHRONIZE_CHANGE;
        memcpy(t, p.buffer, 5 * sizeof(int));
        sprintf(sbuf.mtext, "%d %d %d %d", p.packet_id, t[2], t[3], t[1]);
        msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
        kill(mainpid, SIGUSR1);
      } else if (p.type == SYNCHRONIZE_REMOVE) {
        fprintf(stderr,
                "Receive SYNCHRONIZE_REMOVE packet [id: %d | game: %d | msg: "
                "%s]\n",
                p.client_id,
                p.packet_id,
                p.buffer);
        sbuf.mtype = SYNCHRONIZE_RM;
        sscanf(p.buffer, "%d %d %d", &t[1], &t[2], &t[3]);
        sprintf(sbuf.mtext, "%d %d %d", p.packet_id, t[2], t[3]);
        msgsnd(msqid, &sbuf, MAXDATASIZE, IPC_NOWAIT);
        kill(mainpid, SIGUSR1);
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

  key_t key;
  key = getpid();
  if ((msqid = msgget(key, 0666)) < 0) {
    fprintf(stderr, "msgget");
    exit(1);
  }
  fprintf(stderr, "Server connection [fd: %d | pid: %d]\n", msqid, key);
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
  pid_t pid = -1;
  srv_t *cursor = known_servers->srvs;
  ipc_t *p;
  for (i = 0; i < known_servers->count; ++i) {
    if (!(pid = fork())) {
      server_connection(cursor);
    } else {
      add_msq(&msq, pid, TRUE, &msq_mutex);
      p = get_msq_pid(&msq, pid, TRUE, &msq_mutex);
      p->id = cursor->number;
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
