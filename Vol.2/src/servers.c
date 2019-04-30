#include "../include/server.h"

/*
        Поиск в списке серверов
        Только для servers_pid
*/
SLNode *server_find2_SLNode(Server *srv, char *ip, unsigned int s_port) {
  SLNode *node;

  if (srv->servers == NULL) {
    PERR("servers list is empty\n");
    return NULL;
  }
  node = srv->servers;
  while (node != NULL) {
    if (strcmp(node->ip, ip) == 0 && node->s_port == s_port)
      break;
    node = node->right;
  }
  if (node == NULL) {
    PERR("not found2 SLNode: %s:%d\n", ip, s_port);
    return NULL;
  }

  return node;
}

SLNode *server_find1_SLNode(Server *srv, int id) {
  int i;
  SLNode *node;

  if (srv->servers == NULL) {
    PERR("servers list is empty\n");
    return NULL;
  }
  node = srv->servers;
  for (i = 0; i != id && i <= srv->servers_count; i++) { node = node->right; }
  if (node == NULL) {
    PERR("not found1 SLNode %d\n", id);
    return NULL;
  }

  return node;
}

/*
        Добавить в список серверов
        Только для servers_pid
*/
void server_add_SLNode(
        Server *srv, char *ip, unsigned int s_port, unsigned int g_port) {
  SLNode *node;

  if (srv->servers == NULL) {
    srv->servers = malloc(sizeof(SLNode));
    if (srv->servers == NULL) {
      PERR("create first SLNode\n");
      exit(1);
    }
    strcpy(srv->servers->ip, ip);
    srv->servers->s_port = s_port;
    srv->servers->g_port = g_port;
    srv->servers->left = NULL;
    srv->servers->right = NULL;
  } else {
    node = srv->servers;
    while (node->right != NULL) { node = node->right; }
    node->right = malloc(sizeof(SLNode));
    if (node->right == NULL) {
      PERR("create next SLNode\n");
      exit(1);
    }
    strcpy(node->right->ip, ip);
    node->right->s_port = s_port;
    node->right->g_port = g_port;
    node->right->left = node;
    node->right->right = NULL;
  }
  srv->servers_count += 1;
  PINF("included new server to list. List size: %d. Server: %s:%u(s)/%u(g)\n",
       srv->servers_count,
       ip,
       s_port,
       g_port);
}

/*
        Убрать из списка серверов
        Только для servers_pid
*/
void server_erase1_SLNode(Server *srv, int id) {
  SLNode *node;

  node = server_find1_SLNode(srv, id);
  if (node == NULL)
    return;
  if (node->left != NULL)
    node->left->right = node->right;
  else
    srv->servers = node->right;
  if (node->right != NULL)
    node->right->left = node->left;

  srv->servers_count -= 1;
  PINF("erased server from list. List size: %d. Server: %s:%u(s)/%u(g)\n",
       srv->servers_count,
       node->ip,
       node->s_port,
       node->g_port);
  free(node);
}
void server_erase2_SLNode(Server *srv, char *ip, unsigned int s_port) {
  SLNode *node;

  node = server_find2_SLNode(srv, ip, s_port);
  if (node == NULL)
    return;
  if (node->left != NULL)
    node->left->right = node->right;
  else
    srv->servers = node->right;
  if (node->right != NULL)
    node->right->left = node->left;

  srv->servers_count -= 1;
  PINF("erased server from list. List size: %d. Server: %s:%u(s)\n",
       srv->servers_count,
       node->ip,
       node->s_port);
  free(node);
}

void server_clear_SLNode(Server *srv) {
  SLNode *node;

  node = srv->servers;
  while (srv->servers != NULL) {
    node = srv->servers;
    srv->servers = node->right;
    free(node);
    srv->servers_count -= 1;
  }

  PINF("clear servers list. List size: %d.\n", srv->servers_count);
}

int connect_to_main_server(char *ip, unsigned int port) {
  int sock;
  struct sockaddr_in addr;
  socklen_t addr_len;

  PINF("connect to server %s:%u\n", ip, port);

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock <= 0) {
    PERR("open socket\n");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_aton(ip, &addr.sin_addr) == 0) {
    PERR("set ip addres %s\n", ip);
    PERR("connect to main server %s:%u\n", ip, port);
    close(sock);
    return -1;
  } else {
    addr_len = sizeof(struct sockaddr_in);
    if (connect(sock, (struct sockaddr *)&addr, addr_len) == 0) {
      PINF("connected to main server %s:%u\n", ip, port);
    } else {
      PERR("connect to main server %s:%u\n", ip, port);
      close(sock);
      return -1;
    }
  }

  return sock;
}

void clear_Data(Data *dt) {
  if (dt == NULL)
    return;

  dt->dtype = 0;
  dt->type = 0;
  dt->uchar_1 = 0;
  dt->uchar_2 = 0;
  dt->uchar_3 = 0;
  dt->char_buff[0] = '\0';
}

void *server_servers(void *arg) {
  Server *srv = (Server *)arg;
  SLNode *slnode;
  int extra_server = 0;
  int new_server = 0;
  struct sockaddr_in addr;
  socklen_t addr_len;
  int epl, epl_count;
  struct epoll_event events[4], ev[4];
  Data data;
  unsigned int uint_1, uint_2;
  char char_buff[CHAR_BUFF_SIZE];
  Message *msg;
  unsigned int main_tick = 0;
  unsigned int extra_tick = 0;
  unsigned char fl_err = 0;
  int i, j, data_len;
  struct pthread_games *pg_data;

  if (listen(srv->servers_socket, 5) == -1) {
    PERR("servers: creat listen socket\n");
    goto err_exit;
  }
  epl = epoll_create(4);
  PINF("servers service is running\n");

  ev[0].events = EPOLLIN; // main_server_socket
  ev[1].events = EPOLLIN; // servers_socket
  ev[2].events = EPOLLIN; // extra_server
  ev[3].events = EPOLLIN; // new_server
  if (srv->main_server_socket > 0) {
    ev[0].data.fd = srv->main_server_socket;
    epoll_ctl(epl, EPOLL_CTL_ADD, srv->main_server_socket, &ev[0]);

    sprintf(data.char_buff,
            "%s %u %u",
            srv->ip,
            srv->servers_port,
            srv->games_port);
    data.dtype = 1;
    data.type = 4;
    if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
      PERR("send data to main_server_socket\n");
      goto err_exit;
    }
  }
  ev[1].data.fd = srv->servers_socket;
  epoll_ctl(epl, EPOLL_CTL_ADD, srv->servers_socket, &ev[1]);

  while (1) {
    epl_count = epoll_wait(epl, events, 4, 500);
    if (epl_count == 0) {
      if (srv->main_server_socket > 0)
        main_tick += 1;
      if (extra_server > 0 || extra_tick > 0)
        extra_tick += 1;
    }
    while (epl_count > 0) {
      epl_count -= 1;
      if (events[epl_count].data.fd == srv->servers_socket) {
        /*
                Подключился новый сервер
        */
        if (new_server > 0)
          close(new_server);
        addr_len = sizeof(struct sockaddr_in);
        new_server = accept(
                srv->servers_socket, (struct sockaddr *)&addr, &addr_len);
        PINF("new server connected: %s:%u\n",
             inet_ntoa(addr.sin_addr),
             ntohs(addr.sin_port));
        ev[3].data.fd = new_server;
        epoll_ctl(epl, EPOLL_CTL_ADD, new_server, &ev[3]);

        continue;
      }
      if (events[epl_count].data.fd == new_server) {
        /*
                Сообщение от нового сервера
        */
        epoll_ctl(epl, EPOLL_CTL_DEL, new_server, &ev[3]);
        if (recv(new_server, &data, Data_size, 0) <= 0) {
          PERR("receive data from new_server\n");
        } else {
          if (data.dtype == 1 && data.type == 4) {
            if (!(extra_server > 0)) {
              /*
                      Нет дополнительного сервера
              */
              extra_tick = 0;
              sscanf(data.char_buff, "%s %u %u", char_buff, &uint_1, &uint_2);
              extra_server = new_server;
              new_server = 0;
              ev[2].data.fd = extra_server;
              epoll_ctl(epl, EPOLL_CTL_ADD, extra_server, &ev[2]);
              data.type = 9;
              data.uchar_1 = 2;
              PINF("added extra_server\n");
              if (send(extra_server, &data, Data_size, 0) <= 0) {
                PERR("send data to extra_server\n");
                fl_err = fl_err | EBEX;
              } else {
                if ((fl_err & EGLI) != EGLI) {
                  server_add_SLNode(srv, char_buff, uint_1, uint_2);
                  if (srv->main_server_socket > 0) {
                    data.type = 5;
                    if (send(srv->main_server_socket, &data, Data_size, 0) <=
                        0) {
                      PERR("send data to main_server_socket\n");
                      fl_err = fl_err | EBMA;
                    }
                  } else {
                    sprintf(data.char_buff, "%s %u ", char_buff, uint_2);
                    data.type = 4;
                    for (i = 0; i < MAX_GAMES * 2; i++) {
                      pthread_mutex_lock(&srv->mutex);
                      if (srv->players[i] != NULL) {
                        j = srv->players[i]->socket;
                        pthread_mutex_unlock(&srv->mutex);
                        if (j > 0)
                          send(j, &data, Data_size, 0);
                      } else
                        pthread_mutex_unlock(&srv->mutex);
                    }
                  }
                } else {
                  fl_err = fl_err & (~EGLI);
                }
              }
            } else {
              /*
                      Уже есть дополнительный сервер
              */
              slnode = server_find1_SLNode(srv, srv->servers_count - 1);
              if (slnode != NULL) {
                sprintf(data.char_buff, "%s %u ", slnode->ip, slnode->s_port);
                data.type = 9;
                data.uchar_1 = 1;
                if (send(new_server, &data, Data_size, 0) <= 0) {
                  PERR("send data to new_server\n");
                } else {
                  PINF("redirect new_server to: %s:%u\n",
                       slnode->ip,
                       slnode->s_port);
                }
              } else {
                fl_err = fl_err | ENLI;
              }
            }
          } else {
            WRND(&data);
          }
        }
        if (new_server > 0) {
          close(new_server);
          new_server = 0;
        }

        continue;
      }
      if (events[epl_count].data.fd == srv->main_server_socket) {
        /*
                Сообщение от основного сервера
        */
        clear_Data(&data);
        data_len =
                recv(srv->main_server_socket, &data, Data_size, MSG_NOSIGNAL);
        if (data_len < 0) {
          PERR("receive data from main_server_socket\n");
          perror("receive data from main_server_socket:");
          fl_err = fl_err | EBMA;
        }
        if (data_len >= 0) {
          main_tick += 1;
          switch (data.dtype) {
          case 0:
            break;
          case 1:
            switch (data.type) {
            case 1:
              /*
                      Главный сервер просит создать нового игрока
              */
              main_tick = 0;
              if (extra_server > 0)
                if (send(extra_server, &data, Data_size, 0) <= 0) {
                  PERR("send data to extra_server\n");
                  fl_err = fl_err | EBEX;
                }
              switch (data.uchar_1) {
              case 0:
                j = create_Player(srv, data.uchar_2, 0);
                if (j == -1) {
                  PERR("can't create Player: %u\n", data.uchar_2);
                  fl_err = fl_err | EPLI;
                } else {
                  pthread_mutex_lock(&srv->mutex);
                  sscanf(data.char_buff,
                         "%s %u",
                         srv->players[j]->ip,
                         &srv->players[j]->gid);
                  srv->players[j]->symbol = data.uchar_3;
                  if (srv->players[j]->gid >= 0 &&
                      srv->players[j]->gid < MAX_GAMES) {
                    if (srv->games[srv->players[j]->gid] != NULL) {
                      switch (srv->players[j]->symbol) {
                      case BX:
                        srv->games[srv->players[j]->gid]->x = srv->players[j];
                        if (srv->games[srv->players[j]->gid]->is_run == FG_WAIT)
                          srv->games[srv->players[j]->gid]->is_run = FG_PROCES;
                        PINF("player added: %u(gid), %u(pid)\n",
                             srv->players[j]->gid,
                             j);
                        break;
                      case BO:
                        srv->games[srv->players[j]->gid]->o = srv->players[j];
                        PINF("player added: %u(gid), %u(pid)\n",
                             srv->players[j]->gid,
                             j);
                        break;
                      default:
                        WRND(&data);
                        fl_err = fl_err | EPLI;
                      }
                    } else {
                      PERR("can't find Game for Player: %u(gid), %u(pid)\n",
                           srv->players[j]->gid,
                           data.uchar_2);
                      fl_err = fl_err | EPLI;
                    }
                  } else {
                    PERR("bad game id: %u(gid), %u(pid)\n",
                         srv->players[j]->gid,
                         data.uchar_2);
                    fl_err = fl_err | EPLI;
                  }
                  pthread_mutex_unlock(&srv->mutex);
                }
                break;
              case 1:
                if (data.uchar_2 >= MAX_GAMES * 2) {
                  PERR("bad player id: %u(pid)/%u(max pid)\n",
                       data.uchar_2,
                       MAX_GAMES * 2 - 1);
                  fl_err = fl_err | EPLI;
                  break;
                }
                pthread_mutex_lock(&srv->mutex);
                if (srv->players[data.uchar_2] == NULL) {
                  pthread_mutex_unlock(&srv->mutex);
                  PERR("can't find Player: %u\n", data.uchar_2);
                  fl_err = fl_err | EPLI;
                } else {
                  sscanf(data.char_buff, "%s", srv->players[j]->name);
                  pthread_mutex_unlock(&srv->mutex);
                }
                break;
              default:
                WRND(&data);
              }
              break;
            case 6:
              /*
                      Главный сервер просит создать новую игру
              */
              main_tick = 0;
              if (extra_server > 0)
                if (send(extra_server, &data, Data_size, 0) <= 0) {
                  PERR("send data to extra_server\n");
                  fl_err = fl_err | EBEX;
                }
              i = create_Game(srv, data.uchar_1);
              if (i == -1) {
                PERR("can't create Game: %u\n", data.uchar_1);
                fl_err = fl_err | EPLI;
              } else {
                pthread_mutex_lock(&srv->mutex);
                sprintf(srv->games[i]->board, "%s", data.char_buff);
                srv->games[i]->is_run = data.uchar_2;
                pthread_mutex_unlock(&srv->mutex);
              }
              break;
            case 7:
              /*
                      Главный сервер просит удалить игру
              */
              main_tick = 0;
              server_push_Message(srv, 6, data.uchar_1, NULL);
              delete_Game(srv, data.uchar_1);
              break;
            case 8:
              main_tick = 0;
              break;
            case 9:
              main_tick = 0;
              switch (data.uchar_1) {
              case 1:
                /*
                        Переподключиться (поиск главного сервера)
                */
                epoll_ctl(epl, EPOLL_CTL_DEL, srv->main_server_socket, &ev[0]);
                sscanf(data.char_buff, "%s %u", char_buff, &uint_1);
                PINF("redirected to: %s:%u\n", char_buff, uint_1);
                srv->main_server_socket =
                        connect_to_main_server(char_buff, uint_1);
                if (!(srv->main_server_socket > 0))
                  goto err_exit;
                sprintf(data.char_buff,
                        "%s %u %u",
                        srv->ip,
                        srv->servers_port,
                        srv->games_port);
                data.dtype = 1;
                data.type = 4;
                if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
                  PERR("send data to main_server_socket\n");
                  goto err_exit;
                }
                ev[0].data.fd = srv->main_server_socket;
                epoll_ctl(epl, EPOLL_CTL_ADD, srv->main_server_socket, &ev[0]);
                strcpy(srv->main_server_info.ip, char_buff);
                srv->main_server_info.s_port = uint_1;
                PINF("update main_server_info %s:%u\n",
                     srv->main_server_info.ip,
                     srv->main_server_info.s_port);
                break;
              case 2:
                /*
                        Главный сервер подтвердил подключение
                */
                if (srv->servers == NULL) {
                  fl_err = fl_err | ENLI;
                }
                fl_err = fl_err | EPLI;
                break;
              default:
                WRND(&data);
              }
              break;
            case 10:
              main_tick = 0;
              sscanf(data.char_buff, "%s %u %u", char_buff, &uint_1, &uint_2);
              server_add_SLNode(srv, char_buff, uint_1, uint_2);
              if (extra_server > 0)
                if (send(extra_server, &data, Data_size, 0) <= 0) {
                  PERR("send data to extra_server\n");
                  fl_err = fl_err | EBEX;
                }
              break;
            case 11:
              main_tick = 0;
              sscanf(data.char_buff, "%s %u %u", char_buff, &uint_1, &uint_2);
              server_erase2_SLNode(srv, char_buff, uint_1);
              if (extra_server > 0)
                if (send(extra_server, &data, Data_size, 0) <= 0) {
                  PERR("send data to extra_server\n");
                  fl_err = fl_err | EBEX;
                }
              break;
            default:
              WRND(&data);
            }
            break;
          case 2:
            switch (data.type) {
            case 1:
              /*
                      Главный сервер обновил состояние игры
              */
              server_push_Message(srv, 2, data.uchar_1, NULL);
              pthread_mutex_lock(&srv->mutex);
              sprintf(srv->games[data.uchar_1]->board, "%s", data.char_buff);
              pthread_mutex_unlock(&srv->mutex);
              break;
            default:
              WRND(&data);
            }
            break;
          default:
            WRND(&data);
          }
          continue;
        }
      }
      if (events[epl_count].data.fd == extra_server) {
        /*
                Сообщение от дополнительного сервера
        */
        clear_Data(&data);
        data_len = recv(extra_server, &data, Data_size, MSG_NOSIGNAL);
        if (data_len <= 0) {
          PERR("receive data from extra_server\n");
          fl_err = fl_err | EBEX;
        }
        if (data_len > 0) {
          extra_tick += 1;
          switch (data.dtype) {
          case 0:
            break;
          case 1:
            switch (data.type) {
            case 1:
              extra_tick = 0;
              data.type = 8;
              if (extra_server > 0)
                if (send(extra_server, &data, Data_size, 0) <= 0) {
                  PERR("send data to extra_server\n");
                  fl_err = fl_err | EBEX;
                }
              break;
            case 2:
              extra_tick = 0;
              switch (data.uchar_1) {
              case 0:
                /*
                        Дополнительный сервер запросил список игр
                */
                for (i = 0; i < MAX_GAMES; i++) {
                  pthread_mutex_lock(&srv->mutex);
                  if (srv->games[i] != NULL) {
                    sprintf(data.char_buff, "%s", srv->games[i]->board);
                    pthread_mutex_unlock(&srv->mutex);
                    data.dtype = 1;
                    data.type = 6;
                    data.uchar_1 = i;
                    data.uchar_2 = srv->games[i]->is_run;
                    if (send(extra_server, &data, Data_size, 0) <= 0) {
                      PERR("send data to extra_server\n");
                      fl_err = fl_err | EBEX;
                    }
                  } else
                    pthread_mutex_unlock(&srv->mutex);
                }
                break;
              case 1:
                /*
                        Дополнительный сервер запросил список серверов
                */
                if (extra_server > 0) {
                  slnode = srv->servers;
                  while (slnode != NULL) {
                    sprintf(data.char_buff,
                            "%s %u %u ",
                            slnode->ip,
                            slnode->s_port,
                            slnode->g_port);
                    data.dtype = 1;
                    data.type = 10;
                    if (send(extra_server, &data, Data_size, 0) <= 0) {
                      PERR("send data to extra_server\n");
                      fl_err = fl_err | EBEX;

                      break;
                    }
                    slnode = slnode->right;
                  }
                }
                break;
              case 2:
                /*
                        Дополнительный сервер запросил список клиентов
                */
                for (j = 0; j < MAX_GAMES * 2; j++) {
                  pthread_mutex_lock(&srv->mutex);
                  if (srv->players[j] != NULL) {
                    sprintf(data.char_buff,
                            "%s %u",
                            srv->players[j]->ip,
                            srv->players[j]->gid);
                    data.uchar_3 = srv->players[j]->symbol;
                    pthread_mutex_unlock(&srv->mutex);
                    data.dtype = 1;
                    data.type = 1;
                    data.uchar_1 = 0;
                    data.uchar_2 = j;
                    if (send(extra_server, &data, Data_size, 0) <= 0) {
                      PERR("send data to extra_server\n");
                      fl_err = fl_err | EBEX;
                    } else {
                      pthread_mutex_lock(&srv->mutex);
                      sprintf(data.char_buff, "%s", srv->players[j]->name);
                      pthread_mutex_unlock(&srv->mutex);
                      data.uchar_1 = 1;
                      if (send(extra_server, &data, Data_size, 0) <= 0) {
                        PERR("send data to extra_server\n");
                        fl_err = fl_err | EBEX;
                      }
                    }
                  } else
                    pthread_mutex_unlock(&srv->mutex);
                }
                break;
              default:
                WRND(&data);
                fl_err = fl_err | EBEX;
              }
              break;
            case 3:
              extra_tick = 0;
              break;
            case 5:
              /*
                      Дополнительный сервер добавил сервер в список
              */
              extra_tick = 0;
              sscanf(data.char_buff, "%s %u %u", char_buff, &uint_1, &uint_2);
              server_add_SLNode(srv, char_buff, uint_1, uint_2);
              if (srv->main_server_socket > 0) {
                if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
                  PERR("send data to main_server_socket\n");
                  fl_err = fl_err | EBMA;
                }
              } else {
                sprintf(data.char_buff, "%s %u ", char_buff, uint_2);
                data.type = 4;
                for (i = 0; i < MAX_GAMES * 2; i++) {
                  pthread_mutex_lock(&srv->mutex);
                  if (srv->players[i] != NULL) {
                    j = srv->players[i]->socket;
                    pthread_mutex_unlock(&srv->mutex);
                    if (j > 0)
                      send(j, &data, Data_size, 0);
                  } else
                    pthread_mutex_unlock(&srv->mutex);
                }
              }

              break;
            case 6:
              /*
                      Дополнительный сервер удалил сервер из списка
              */
              extra_tick = 0;
              sscanf(data.char_buff, "%s %u %u", char_buff, &uint_1, &uint_2);
              server_erase2_SLNode(srv, char_buff, uint_1);
              if (srv->main_server_socket > 0) {
                if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
                  PERR("send data to main_server_socket\n");
                  fl_err = fl_err | EBMA;
                }
              } else {
                sprintf(data.char_buff, "%s %u ", char_buff, uint_2);
                data.type = 5;
                for (i = 0; i < MAX_GAMES * 2; i++) {
                  pthread_mutex_lock(&srv->mutex);
                  if (srv->players[i] != NULL) {
                    j = srv->players[i]->socket;
                    pthread_mutex_unlock(&srv->mutex);
                    if (j > 0)
                      send(j, &data, Data_size, 0);
                  } else
                    pthread_mutex_unlock(&srv->mutex);
                }
              }
              break;
            default:
              WRND(&data);
              fl_err = fl_err | EBEX;
            }
            break;
          default:
            WRND(&data);
            fl_err = fl_err | EBEX;
          }
        }
      }
    } // while(epl_count > 0)
    if (main_tick == TICK_SEND) {
      /*
              Проверка связи
      */
      if (srv->main_server_socket > 0) {
        data.dtype = 1;
        data.type = 1;
        if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
          PERR("send data to main_server_socket\n");
          fl_err = fl_err | EBMA;
        }
      }
    }
    if (main_tick == TICK_DROP) {
      /*
              Отключение главного сервера
      */
      fl_err = fl_err | EBMA;
      PINF("timeout: main_server_socket\n");
      main_tick = 0;
    }
    if (extra_tick == TICK_DROP) {
      /*
              Отключение дополнительного сервера
      */
      fl_err = fl_err | EBEX;
      PINF("timeout: extra_server\n");
    }
    if (extra_tick == TICK_DROP + TICK_SEND) {
      /*
              После отключения дополнительного сервера, никто
              не подключился. Значит все сервера после текущего в списке упали.
      */
      extra_tick = 0;
      slnode = server_find2_SLNode(srv, srv->ip, srv->servers_port);
      if (slnode != NULL)
        slnode = slnode->left;
      while (slnode != NULL) {
        PINF("bad extra_server: %s:%u\n", slnode->ip, slnode->s_port);
        server_push_Message(srv, 7, 0, slnode);
        slnode = slnode->left;
      }
    }
    if ((fl_err & ENLI) == ENLI && (fl_err & EBMA) != EBMA) {
      /*
              Если список серверов некорректный
      */
      fl_err = fl_err & (~ENLI);
      if (srv->main_server_socket > 0) {
        PINF("bad servers list\n");
        server_clear_SLNode(srv);
        data.dtype = 1;
        data.type = 2;
        data.uchar_1 = 1;
        if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
          PERR("send data to main_server_socket\n");
          fl_err = fl_err | EBMA;
        }
      }
    }
    if ((fl_err & EPLI) == EPLI && (fl_err & EBMA) != EBMA) {
      /*
              Если список игроков/игр некорректный
      */
      fl_err = fl_err & (~EPLI);
      if (srv->main_server_socket > 0) {
        PINF("bad players/games list\n");
        for (i = 0; i < MAX_GAMES; i++) { delete_Game(srv, i); }
        pthread_mutex_lock(&srv->mutex);
        for (i = 0; i < MAX_GAMES * 2; i++) {
          if (srv->players[i] != NULL) {
            free(srv->players[i]);
            srv->players[i] = NULL;
          }
        }
        pthread_mutex_unlock(&srv->mutex);
        data.dtype = 1;
        data.type = 2;
        data.uchar_1 = 0;
        if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
          PERR("send data to main_server_socket\n");
          fl_err = fl_err | EBMA;
        } else {
          data.uchar_1 = 2;
          if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
            PERR("send data to main_server_socket\n");
            fl_err = fl_err | EBMA;
          }
        }
      } else
        goto err_exit;
    }
    if ((fl_err & EBEX) == EBEX) {
      /*
              Если отвалился дополнительный сервер
      */
      fl_err = fl_err & (~EBEX);
      if (extra_server > 0) {
        PINF("bad extra_server\n");
        epoll_ctl(epl, EPOLL_CTL_DEL, extra_server, &ev[2]);
        close(extra_server);
        extra_server = 0;
        slnode = server_find1_SLNode(srv, srv->servers_count - 2);
        if (slnode != NULL) {
          if (strcmp(srv->ip, slnode->ip) == 0 &&
              srv->servers_port == slnode->s_port) {
            /*
                    Текущий сервер предпоследний в списке,
                    значит упал последний сервер из списка, а его можно спокойно
               удалять
            */
            slnode = server_find1_SLNode(srv, srv->servers_count - 1);
            if (slnode != NULL) {
              if (srv->main_server_socket > 0) {
                sprintf(data.char_buff,
                        "%s %u %u",
                        slnode->ip,
                        slnode->s_port,
                        slnode->g_port);
                data.dtype = 1;
                data.type = 6;
                if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
                  PERR("send data to main_server_socket\n");
                  fl_err = fl_err | EBMA;
                }
              } else {
                sprintf(data.char_buff, "%s %u ", slnode->ip, slnode->g_port);
                data.type = 5;
                for (i = 0; i < MAX_GAMES * 2; i++) {
                  pthread_mutex_lock(&srv->mutex);
                  if (srv->players[i] != NULL) {
                    j = srv->players[i]->socket;
                    pthread_mutex_unlock(&srv->mutex);
                    if (j > 0)
                      send(j, &data, Data_size, 0);
                  } else
                    pthread_mutex_unlock(&srv->mutex);
                }
              }
            }
            server_erase1_SLNode(srv, srv->servers_count - 1);
          } else {
            /*
                    В списке образовалать "дыра"
            */
            fl_err = fl_err | EGLI;
          }
        } else {
          fl_err = fl_err | ENLI;
        }
      }
    }

    if ((fl_err & EBMA) == EBMA) {
      /*
              Если отвалился главный сервер
      */
      fl_err = fl_err & (~EBMA);
      if (srv->main_server_socket > 0) {
        PINF("bad main_server_socket\n");
        main_tick = 0;
        epoll_ctl(epl, EPOLL_CTL_DEL, srv->main_server_socket, &ev[0]);
        close(srv->main_server_socket);
        srv->main_server_socket = 0;
        slnode = server_find2_SLNode(
                srv, srv->main_server_info.ip, srv->main_server_info.s_port);
        if (slnode != NULL) {
          server_push_Message(srv, 7, 0, slnode);
          slnode = slnode->left;
          while (slnode != NULL) {
            PINF("connect to new main server %s:%u\n",
                 slnode->ip,
                 slnode->s_port);
            srv->main_server_socket =
                    connect_to_main_server(slnode->ip, slnode->s_port);
            if (srv->main_server_socket > 0) {
              sprintf(data.char_buff,
                      "%s %u %u ",
                      srv->ip,
                      srv->servers_port,
                      srv->games_port);
              data.dtype = 1;
              data.type = 4;
              if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
                PERR("send data to main_server_socket\n");
              } else {
                ev[0].data.fd = srv->main_server_socket;
                epoll_ctl(epl, EPOLL_CTL_ADD, srv->main_server_socket, &ev[0]);
                strcpy(srv->main_server_info.ip, slnode->ip);
                srv->main_server_info.s_port = slnode->s_port;
                PINF("update main_server_info %s:%u\n",
                     srv->main_server_info.ip,
                     srv->main_server_info.s_port);
                break;
              }
            }
            PINF("server unreachable: %s:%u\n", slnode->ip, slnode->s_port);
            server_push_Message(srv, 7, 0, slnode);
            slnode = slnode->left;
          }
          if (slnode == NULL) {
            PINF("I new main server\n");
            for (i = 0; i < MAX_GAMES; i++) {
              /*
                      Запуск игр
              */
              pthread_mutex_lock(&srv->mutex);
              if (srv->games[i] == NULL) {
                pthread_mutex_unlock(&srv->mutex);
                continue;
              }
              pg_data = malloc(sizeof(struct pthread_games));
              if (pg_data == NULL) {
                PERR("malloc pg_data\n");
                goto err_exit;
              }
              if (srv->games[i]->is_run != FG_WAIT)
                srv->games[i]->is_run = FG_RECO;
              pg_data->srv = srv;
              pg_data->gm = srv->games[i];
              pthread_create(&srv->games[i]->pid, NULL, games_game, pg_data);
              pthread_mutex_unlock(&srv->mutex);
            }
          }
        } else {
          goto err_exit;
        }
      }
    }

    msg = server_pop_Message(srv);
    while (msg != NULL) {
      switch (msg->act) {
      case 2:
        if (extra_server > 0) {
          pthread_mutex_lock(&srv->mutex);
          sprintf(data.char_buff, "%s", srv->games[msg->uint]->board);
          pthread_mutex_unlock(&srv->mutex);
          data.dtype = 2;
          data.type = 1;
          data.uchar_1 = msg->uint;
          if (send(extra_server, &data, Data_size, 0) <= 0) {
            PERR("send data to extra_server\n");
            fl_err = fl_err | EBEX;
          }
        }
        break;
      case 3:
        if (extra_server > 0) {
          pthread_mutex_lock(&srv->mutex);
          sprintf(data.char_buff,
                  "%s %u",
                  srv->players[msg->uint]->ip,
                  srv->players[msg->uint]->gid);
          data.uchar_3 = srv->players[msg->uint]->symbol;
          pthread_mutex_unlock(&srv->mutex);
          data.dtype = 1;
          data.type = 1;
          data.uchar_1 = 0;
          data.uchar_2 = msg->uint;
          if (send(extra_server, &data, Data_size, 0) <= 0) {
            PERR("send data to extra_server\n");
            fl_err = fl_err | EBEX;
          } else {
            pthread_mutex_lock(&srv->mutex);
            sprintf(data.char_buff, "%s", srv->players[msg->uint]->name);
            pthread_mutex_unlock(&srv->mutex);
            data.uchar_1 = 1;
            if (send(extra_server, &data, Data_size, 0) <= 0) {
              PERR("send data to extra_server\n");
              fl_err = fl_err | EBEX;
            }
          }
        }
        break;
      case 5:
        if (extra_server > 0) {
          pthread_mutex_lock(&srv->mutex);
          sprintf(data.char_buff, "%s", srv->games[msg->uint]->board);
          data.uchar_2 = srv->games[msg->uint]->is_run;
          pthread_mutex_unlock(&srv->mutex);
          data.dtype = 1;
          data.type = 6;
          data.uchar_1 = msg->uint;
          if (send(extra_server, &data, Data_size, 0) <= 0) {
            PERR("send data to extra_server\n");
            fl_err = fl_err | EBEX;
          }
        }
        break;
      case 6:
        if (extra_server > 0) {
          data.dtype = 1;
          data.type = 7;
          data.uchar_1 = msg->uint;
          if (send(extra_server, &data, Data_size, 0) <= 0) {
            PERR("send data to extra_server\n");
            fl_err = fl_err | EBEX;
          }
        }
        break;
      case 7:
        sprintf(data.char_buff,
                "%s %u %u ",
                msg->node->ip,
                msg->node->s_port,
                msg->node->g_port);
        data.dtype = 1;
        if (extra_server > 0) {
          data.type = 11;
          if (send(extra_server, &data, Data_size, 0) <= 0) {
            PERR("send data to extra_server\n");
            fl_err = fl_err | EBEX;
          }
        }
        if (srv->main_server_socket > 0) {
          data.type = 6;
          if (send(srv->main_server_socket, &data, Data_size, 0) <= 0) {
            PERR("send data to main_server_socket\n");
            fl_err = fl_err | EBMA;
          }
        } else {
          sprintf(data.char_buff, "%s %u ", msg->node->ip, msg->node->g_port);
          data.type = 5;
          for (i = 0; i < MAX_GAMES * 2; i++) {
            pthread_mutex_lock(&srv->mutex);
            if (srv->players[i] != NULL) {
              j = srv->players[i]->socket;
              pthread_mutex_unlock(&srv->mutex);
              if (j > 0)
                send(j, &data, Data_size, 0);
            } else
              pthread_mutex_unlock(&srv->mutex);
          }
        }
        server_erase2_SLNode(srv, msg->node->ip, msg->node->s_port);
        break;
      case 8:
        goto err_exit;
        break;
      }
      free(msg);
      msg = server_pop_Message(srv);
    }
  } // while(1)

  PINF("server_servers is fault\n");
  pthread_exit(0);

err_exit:
  PINF("server_servers is down\n");
  pthread_exit(0);
}
