#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
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

#include "../include/glist.h"
#include "../include/llist.h"

#define GAME_PORT "65500"
#define CHECK_PORT "65501"
#define BACKLOG 10
#define MAXDATASIZE 100
#define MSG_LEN 64
#define SERV_CNT 3

struct server_addr {
  char ip[16];
  char port[6];
  char chk_port[6];
};

struct args {
  int fd;
  int chk_fd;
  int id;
  char *ip;
  char *port;
};

int field[3][3];
int main_server = 0;
struct lroot *root;
struct glroot *groot;
pthread_mutex_t mtx;
struct server_addr servers[3] = {{"127.0.0.1", "65500", "65501"},
                                 {"127.0.0.1", "65502", "65503"},
                                 {"127.0.0.1", "65504", "65505"}};
FILE *logfd;

int check_field() {
  int res = 0, i, j;

  if ((field[0][0] == field[1][0]) && (field[0][0] == field[2][0]) &&
      (field[0][0] != 0))
    res = 1;
  if ((field[0][1] == field[1][1]) && (field[0][1] == field[2][1]) &&
      (field[0][1] != 0))
    res = 1;
  if ((field[0][2] == field[1][2]) && (field[0][2] == field[2][2]) &&
      (field[0][2] != 0))
    res = 1;
  if ((field[0][0] == field[0][1]) && (field[0][0] == field[0][2]) &&
      (field[0][0] != 0))
    res = 1;
  if ((field[1][0] == field[1][1]) && (field[1][0] == field[1][2]) &&
      (field[1][0] != 0))
    res = 1;
  if ((field[2][0] == field[2][1]) && (field[2][0] == field[2][2]) &&
      (field[2][0] != 0))
    res = 1;
  if ((field[0][0] == field[1][1]) && (field[0][0] == field[2][2]) &&
      (field[0][0] != 0))
    res = 1;
  if ((field[2][0] == field[1][1]) && (field[2][0] == field[0][2]) &&
      (field[2][0] != 0))
    res = 1;
  if (res != 1) {
    int fl = 0;
    for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
        if (field[i][j] == 0)
          fl = 1;
    if (fl == 0)
      res = 2;
  }
  return res;
}

void sigchld_handler(int s) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

double wtime() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double)t.tv_sec + (double)t.tv_usec * 1E-6;
}

void server_log(char *str1, int flg, const char *str2) {
  pthread_mutex_lock(&mtx);
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char s[64];
  strftime(s, sizeof(s), "%c", tm);
  fprintf(logfd, "[%s] ", s);

  if (flg == 1)
    fprintf(logfd, "%s\n", str1);
  else if (flg == 0)
    fprintf(logfd, "%s\n", str1);
  else
    fprintf(logfd, str1, str2);
  fflush(logfd);
  pthread_mutex_unlock(&mtx);
}

void send_to_client(int *sockfd, char *message) {
  if (send(*sockfd, message, MSG_LEN, 0) == -1)
    server_log("server thread: send", 1, NULL);
}

void send_to_reserve(int *active_srv, char *message) {
  int i;
  for (i = 0; i < SERV_CNT; i++) {
    if (active_srv[i] > 0) {
      if (send(active_srv[i], message, MSG_LEN, 0) == -1)
        server_log("server thread: reserve send", 1, NULL);
    }
  }
}

void game_process(int op1, int op1_chk, int op2, int op2_chk, int *active_srv) {
  int fdmax, fdmax_chk, command, amnt, i, j, ij, chk;
  char message[MSG_LEN];
  fd_set read_fds, master, read_fds_chk, master_chk;
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&read_fds);
  FD_ZERO(&master);
  FD_ZERO(&read_fds_chk);
  FD_ZERO(&master_chk);

  if (op1_chk > op2_chk)
    fdmax_chk = op1_chk;
  else
    fdmax_chk = op2_chk;
  FD_SET(op1_chk, &master_chk);
  FD_SET(op2_chk, &master_chk);

  if (op1 > op2)
    fdmax = op1;
  else
    fdmax = op2;
  FD_SET(op1, &master);
  FD_SET(op2, &master);

  while (1) {
    read_fds_chk = master_chk;
    if (select(fdmax_chk + 1, &read_fds_chk, NULL, NULL, &tv) == -1) {
      server_log("server thread: select", 1, NULL);
      break;
    }
    if (FD_ISSET(op1_chk, &read_fds_chk)) {
      memset(message, 0, MSG_LEN);
      if ((amnt = recv(op1_chk, message, MSG_LEN, 0)) <= 0) {
        server_log("server thread: recv", 1, NULL);
        message[0] = 66;
        send_to_reserve(active_srv, message);
        send_to_client(&op2, message);
        break;
      } else {
        command = message[0];
        if (command == 100) {
          send_to_client(&op1_chk, message);
          memset(message, 0, MSG_LEN);
        }
      }
    }
    if (FD_ISSET(op2_chk, &read_fds_chk)) {
      memset(message, 0, MSG_LEN);
      if ((amnt = recv(op2_chk, message, MSG_LEN, 0)) <= 0) {
        server_log("server thread: recv", 1, NULL);
        message[0] = 66;
        send_to_reserve(active_srv, message);
        send_to_client(&op1, message);
        break;
      } else {
        command = message[0];
        if (command == 100) {
          send_to_client(&op2_chk, message);
          memset(message, 0, MSG_LEN);
        }
      }
    }

    read_fds = master;
    if (select(fdmax + 1, &read_fds, NULL, NULL, &tv) == -1) {
      server_log("server thread: select", 1, NULL);
      break;
    }
    if (FD_ISSET(op1, &read_fds)) {
      memset(message, 0, MSG_LEN);
      if ((amnt = recv(op1, message, MSG_LEN, 0)) <= 0) {
        server_log("server thread: recv", 1, NULL);
      } else {
        command = message[0];
        if (command == 10) {
          ij = (int)message[1];
          i = ij / 10;
          j = ij % 10;
          field[i][j] = 1;
          message[2] = 1;

          send_to_client(&op2, message);
          send_to_reserve(active_srv, message);

          memset(message, 0, MSG_LEN);
          chk = check_field();
          if (chk == 1) {
            message[0] = 20;
            send_to_reserve(active_srv, message);
            send_to_client(&op1, message);
            send_to_client(&op2, message);
            break;
          } else {
            if (chk == 2) //ничья
            {
              message[0] = 40;
              send_to_reserve(active_srv, message);
              send_to_client(&op1, message);
              send_to_client(&op2, message);
              break;
            } else { // продолжение игры
              message[0] = 50;
              send_to_client(&op1, message);
              send_to_client(&op2, message);
            }
          }
        } else if (command == 66) {
          send_to_reserve(active_srv, message);
          send_to_client(&op2, message);
          break;
        } else {
          send_to_client(&op2, message);
          memset(message, 0, MSG_LEN);
        }
      }
    }
    if (FD_ISSET(op2, &read_fds)) {
      memset(message, 0, MSG_LEN);
      if ((amnt = recv(op2, message, MSG_LEN, 0)) <= 0) {
        server_log("server thread: recv", 1, NULL);
      } else {
        command = message[0];
        if (command == 10) {
          ij = (int)message[1];
          i = ij / 10;
          j = ij % 10;
          field[i][j] = 2;
          message[2] = 2;

          send_to_client(&op1, message);
          send_to_reserve(active_srv, message);

          memset(message, 0, MSG_LEN);
          chk = check_field();
          if (chk == 1) {
            message[0] = 30;
            send_to_reserve(active_srv, message);
            send_to_client(&op1, message);
            send_to_client(&op2, message);
            break;
          } else {
            if (chk == 2) {
              message[0] = 40;
              send_to_reserve(active_srv, message);
              send_to_client(&op1, message);
              send_to_client(&op2, message);
              break;
            } else {
              message[0] = 50;
              send_to_client(&op1, message);
              send_to_client(&op2, message);
            }
          }
        } else if (command == 66) {
          send_to_reserve(active_srv, message);
          send_to_client(&op1, message);
          break;
        } else {
          send_to_client(&op1, message);
          memset(message, 0, MSG_LEN);
        }
      }
    }
  }
}

void *game_thread(void *args) {
  int new_fd, chk_new_fd, amnt, op1, op1_chk, op2, op2_chk, command, side;
  int i, j, ij, id, cnt = 0, tsock, rv;
  int active_srv[SERV_CNT] = {-1};
  char message[MSG_LEN];
  char *ip, *port;
  fd_set read_fds, master;
  struct addrinfo hints, *servinfo = NULL, *p;
  struct list *ptrlist;
  struct glist *gptrlist;
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&read_fds);
  FD_ZERO(&master);

  struct args *tmp = (struct args *)args;
  new_fd = tmp->fd;
  chk_new_fd = tmp->chk_fd;
  ip = tmp->ip;
  port = tmp->port;
  id = tmp->id;

  memset(message, 0, MSG_LEN);
  message[0] = 100;

  if ((amnt = recv(new_fd, message, MSG_LEN, 0)) <= 0) {
    server_log("server thread: recv", 0, NULL);
    pthread_exit(0);
  }
  command = (int)message[0];
  if ((command == 1) || (command == 3)) {
    message[amnt] = '\0';
    if (command == 1)
      side = 1;
    else
      side = 2;

    pthread_mutex_lock(&mtx);
    addelem(root, new_fd, chk_new_fd, side, message + 4);
    pthread_mutex_unlock(&mtx);

  } else if (command == 5) {
    server_log("reserve server: adding game\n", 0, NULL);

    gptrlist = glistfind(groot, message[1]);
    if (gptrlist == NULL) {
      pthread_mutex_lock(&mtx);
      gptrlist = gaddelem(groot, message[1], -1);
      pthread_mutex_unlock(&mtx);
    }

    FD_SET(new_fd, &master);

    while (1) {
      read_fds = master;
      if (select(new_fd + 1, &read_fds, NULL, NULL, &tv) == -1) {
        server_log("reserve server: select", 1, NULL);
      }
      if (FD_ISSET(new_fd, &read_fds)) {
        if ((amnt = recv(new_fd, message, MSG_LEN, 0)) <= 0) {
          server_log("reserve server: recv", 1, NULL);
          break;
        } else {
          command = message[0];
          if (command == 10) {
            ij = (int)message[1];
            i = ij / 10;
            j = ij % 10;
            gptrlist->field[i][j] = message[2];
          } else if (
                  command == 66 || command == 20 || command == 30 ||
                  command == 40) {
            pthread_mutex_lock(&mtx);
            gdeletelem(gptrlist, groot);
            pthread_mutex_unlock(&mtx);
            close(new_fd);
            break;
          }
        }
      }
    }
  } else if (command == 7) {
    pthread_mutex_lock(&mtx);
    if (main_server == 0) {
      main_server = 1;
    }
    pthread_mutex_unlock(&mtx);

    gptrlist = glistfind(groot, message[1]);

    pthread_mutex_lock(&mtx);
    if (gptrlist->flg == 0) {
      gptrlist->flg = 1;
      gptrlist->chk_fd = chk_new_fd;
      gptrlist->fd = new_fd;
      pthread_mutex_unlock(&mtx);
    } else {
      pthread_mutex_unlock(&mtx);

      if (message[2] == 1) {
        op2 = gptrlist->fd;
        op2_chk = gptrlist->chk_fd;
        op1 = new_fd;
        op1_chk = chk_new_fd;
      } else {
        op1 = gptrlist->fd;
        op1_chk = gptrlist->chk_fd;
        op2 = new_fd;
        op2_chk = chk_new_fd;
      }

      for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) { field[i][j] = gptrlist->field[i][j]; }
      }

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;

      for (i = 0; cnt < SERV_CNT && i <= SERV_CNT; i++) {
        if ((strcmp(servers[i].ip, ip) != 0) ||
            (strcmp(servers[i].port, port) != 0)) {
          if ((rv = getaddrinfo(
                       servers[i].ip, servers[i].port, &hints, &servinfo)) !=
              0) {
            server_log("getaddrinfo: %s\n", 2, gai_strerror(rv));
          }

          for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((active_srv[cnt] = socket(
                         p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
              server_log("server thread: sync socket", 1, NULL);
              continue;
            }
            if (connect(active_srv[cnt], p->ai_addr, p->ai_addrlen) == -1) {
              close(active_srv[cnt]);
              server_log("server thread: sync connect", 1, NULL);
              continue;
            }
            if ((rv = getaddrinfo(
                         servers[i].ip,
                         servers[i].chk_port,
                         &hints,
                         &servinfo)) != 0) {
              server_log("getaddrinfo: %s\n", 2, gai_strerror(rv));
            }

            for (p = servinfo; p != NULL; p = p->ai_next) {
              if ((tsock = socket(
                           p->ai_family, p->ai_socktype, p->ai_protocol)) ==
                  -1) {
                server_log("server thread: sync check socket", 1, NULL);
                continue;
              }
              if (connect(tsock, p->ai_addr, p->ai_addrlen) == -1) {
                close(tsock);
                server_log("server thread: sync check connect", 1, NULL);
                continue;
              }
              break;
            }
            close(tsock);
            message[0] = 5;
            message[1] = gptrlist->number;
            if (send(active_srv[cnt], message, MSG_LEN, 0) == -1) {
              server_log("server thread: sync send", 1, NULL);
              continue;
            }
            cnt++;
          }
        }
      }

      pthread_mutex_lock(&mtx);
      gdeletelem(gptrlist, groot);
      pthread_mutex_unlock(&mtx);

      game_process(op1, op1_chk, op2, op2_chk, active_srv);

      close(op1);
      close(op2);
      close(op1_chk);
      close(op2_chk);
    }
  } else {
    if (root->count != 0) {
      ptrlist = root->first_node;
      message[0] = 2;
      message[1] = root->count;
      if (send(new_fd, message, MSG_LEN, 0) == -1) {
        server_log("server thread: send", 1, NULL);
      }
      for (i = 0; i < root->count; i++) {
        if (send(new_fd, ptrlist->name, strlen(ptrlist->name), 0) == -1) {
          server_log("server thread: send", 1, NULL);
          pthread_exit(0);
        }
        ptrlist = ptrlist->ptr;
        if ((amnt = recv(new_fd, message, MSG_LEN, 0)) == -1) {
          server_log("server thread: recv", 1, NULL);
          pthread_exit(0);
        }
      }
      memset(message, 0, MSG_LEN);

      if ((amnt = recv(new_fd, message, MSG_LEN, 0)) ==
          -1) { //получение названия игры
        server_log("server thread: recv", 1, NULL);
        pthread_exit(0);
      }

      if (message[0] != 2) {
        printf("server thread: connection refused\n");
        pthread_exit(0);
      }
      message[amnt] = '\0';

      ptrlist = listfind(root, message + 4);
      if (ptrlist == NULL) {
        message[0] = 61;
        if (send(new_fd, message, MSG_LEN, 0) == -1) {
          server_log("server thread: send", 1, NULL);
        }
        pthread_exit(0);
      }

      memset(message, 0, MSG_LEN);
      if (ptrlist->sd == 1)
        message[0] = 1;
      else
        message[0] = 3;
      message[1] = id;
      if (send(new_fd, message, MSG_LEN, 0) == -1) {
        server_log("server thread: send", 1, NULL);
      }

      memset(message, 0, MSG_LEN);
      message[0] = 100;
      message[1] = id;
      if (send(ptrlist->fd, message, MSG_LEN, 0) ==
          -1) { //отправка ждущему игроку
        server_log("server thread: send", 1, NULL);
      }

      memset(message, 0, MSG_LEN);
      if ((amnt = recv(ptrlist->fd, message, MSG_LEN, 0)) <= 0) {
        server_log("server thread: recv", 1, NULL);
        message[0] = 66;
        if (send(new_fd, message, MSG_LEN, 0) == -1)
          server_log("server thread: send", 1, NULL);

        pthread_mutex_lock(&mtx);
        deletelem(ptrlist, root);
        pthread_mutex_unlock(&mtx);

        pthread_exit(0);
      }

      op1 = ptrlist->fd;
      op1_chk = ptrlist->chk_fd;
      op2 = new_fd;
      op2_chk = chk_new_fd;

      pthread_mutex_lock(&mtx);
      deletelem(ptrlist, root);
      pthread_mutex_unlock(&mtx);

      for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) { field[i][j] = 0; }
      }

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;

      for (i = 0; cnt < SERV_CNT && i <= SERV_CNT; i++) {
        if ((strcmp(servers[i].ip, ip) != 0) ||
            (strcmp(servers[i].port, port) != 0)) {
          if ((rv = getaddrinfo(
                       servers[i].ip, servers[i].port, &hints, &servinfo)) !=
              0) {
            server_log("getaddrinfo: %s\n", 2, gai_strerror(rv));
          }

          for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((active_srv[cnt] = socket(
                         p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
              server_log("server: sync socket", 1, NULL);
              continue;
            }
            if (connect(active_srv[cnt], p->ai_addr, p->ai_addrlen) == -1) {
              close(active_srv[cnt]);
              server_log("server: sync connect", 1, NULL);
              continue;
            }
            if ((rv = getaddrinfo(
                         servers[i].ip,
                         servers[i].chk_port,
                         &hints,
                         &servinfo)) != 0) {
              server_log("getaddrinfo: %s\n", 2, gai_strerror(rv));
            }

            for (p = servinfo; p != NULL; p = p->ai_next) {
              if ((tsock = socket(
                           p->ai_family, p->ai_socktype, p->ai_protocol)) ==
                  -1) {
                server_log("server: sync check socket", 1, NULL);
                continue;
              }
              if (connect(tsock, p->ai_addr, p->ai_addrlen) == -1) {
                close(tsock);
                server_log("server: sync check connect", 1, NULL);
                continue;
              }
              break;
            }
            close(tsock);
            message[0] = 5;
            message[1] = id;
            if (send(active_srv[cnt], message, MSG_LEN, 0) == -1) {
              server_log("server thread: sync send", 1, NULL);
              continue;
            }
            cnt++;
          }
        }
      }

      game_process(op1, op1_chk, op2, op2_chk, active_srv);

      close(op1);
      close(op2);
      close(op1_chk);
      close(op2_chk);
    } else {
      memset(message, 0, MSG_LEN);
      message[1] = 60;
      send_to_client(&new_fd, message);
    }
  }
  return NULL;
}

char *concat(const char *s1, const char *s2) {
  const size_t len1 = strlen(s1);
  const size_t len2 = strlen(s2);
  char *result = malloc(len1 + len2 + 1); // +1 for the null-terminator
  // in real code you would check for errors in malloc here
  memcpy(result, s1, len1);
  memcpy(result + len1, s2, len2 + 1); // +1 to copy the null-terminator
  return result;
}

int main(int argc, char *argv[]) {
  int status, sockfd, new_fd, yes = 1, rv, cnt = -1;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  struct sigaction sa;
  char s[INET6_ADDRSTRLEN];
  char msg[MSG_LEN];
  pthread_t tid;
  int chk_sockfd, chk_new_fd, yep = 1, inf;
  struct addrinfo hnts, *srvinfo, *ptr;
  struct sockaddr_storage thr_addr;
  socklen_t sn_size;
  char sip[INET6_ADDRSTRLEN];
  struct args th_args;

  if (argc != 6) {
    fprintf(stderr,
            "\033[1;93m[WARNING]\033[1;97m Usage: ./bin/cmpl_srv <1/0> <IP> "
            "\033[1;97m<PORT> <CHECK_PORT> <LOG_NAME>\033[0m\n");
    exit(1);
  }

  if (strcmp(argv[1], "1") == 0) {
    main_server = 1;
    printf("Main server:\n");
  } else {
    printf("Reserve server:\n");
    groot = ginit();
  }

  pthread_mutex_init(&mtx, NULL);

  char *real_log_path = concat("./logs/", argv[5]);

  logfd = fopen(real_log_path, "a");
  if (logfd == NULL) {
    printf("\033[1;91m[ERROR]\033[1;97m Can not create/open file <%s>\033[0m\n",
           argv[5]);
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  root = init();

  if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
    server_log("getaddrinfo: %s\n", 2, gai_strerror(rv));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      server_log("server: socket", 1, NULL);
      continue;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      server_log("server: setsockopt", 1, NULL);
      exit(1);
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      server_log("server: bind", 1, NULL);
      continue;
    }
    break;
  }
  if (p == NULL) {
    server_log("server: failed to bind\n", 0, NULL);
    return 2;
  }

  freeaddrinfo(servinfo);

  if (listen(sockfd, BACKLOG) == -1) {
    server_log("server: listen", 1, NULL);
    exit(1);
  }

  memset(&hnts, 0, sizeof hnts);
  hnts.ai_family = AF_UNSPEC;
  hnts.ai_socktype = SOCK_STREAM;
  hnts.ai_flags = AI_PASSIVE;

  if ((inf = getaddrinfo(argv[2], argv[4], &hnts, &srvinfo)) != 0) {
    server_log("getaddrinfo: %s\n", 2, gai_strerror(inf));
    pthread_exit(0);
  }

  for (ptr = srvinfo; ptr != NULL; ptr = ptr->ai_next) {
    if ((chk_sockfd = socket(
                 ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1) {
      server_log("server: socket", 1, NULL);
      continue;
    }
    if (setsockopt(chk_sockfd, SOL_SOCKET, SO_REUSEADDR, &yep, sizeof(int)) ==
        -1) {
      server_log("server: setsockopt", 1, NULL);
      exit(1);
    }
    if (bind(chk_sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
      close(chk_sockfd);
      server_log("server: bind", 1, NULL);
      continue;
    }
    break;
  }
  if (ptr == NULL) {
    server_log("server: failed to bind\n", 0, NULL);
    pthread_exit(0);
  }

  freeaddrinfo(srvinfo);

  if (listen(chk_sockfd, BACKLOG) == -1) {
    server_log("server: listen", 1, NULL);
    exit(1);
  }

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    server_log("server: sigaction", 1, NULL);
    exit(1);
  }

  server_log("server: waiting for connections...\n", 0, NULL);

  while (1) {
    sleep(1);
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      server_log("accept", 1, NULL);
      continue;
    }

    inet_ntop(
            their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s,
            sizeof s);
    server_log("server: got connection from %s\n", 2, s);

    sn_size = sizeof thr_addr;
    chk_new_fd = accept(chk_sockfd, (struct sockaddr *)&thr_addr, &sn_size);
    if (chk_new_fd == -1) {
      server_log("accept", 1, NULL);
    }
    inet_ntop(
            thr_addr.ss_family,
            get_in_addr((struct sockaddr *)&thr_addr),
            sip,
            sizeof sip);
    server_log("server: got check connection from %s\n", 2, sip);

    if (main_server) {
      cnt++;
    }

    th_args.chk_fd = chk_new_fd;
    th_args.ip = argv[2];
    th_args.port = argv[3];
    th_args.fd = new_fd;
    th_args.id = cnt;

    status = pthread_create(&tid, NULL, game_thread, (void *)&th_args);
    if (status != 0) {
      server_log("server: can't create game thread\n", 0, NULL);
      memset(msg, 0, MSG_LEN);
      msg[0] = 80;
      if (send(new_fd, msg, MSG_LEN, 0) == -1)
        server_log("server: send", 1, NULL);
    } else {
      pthread_detach(tid);
    }
  }
  return 0;
}
