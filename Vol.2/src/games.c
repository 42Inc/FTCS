#include "../include/server.h"

/*
        Создает новую игру
        Возвращает id новой игры, либо -1
        Использует мютекс сервера
*/
int create_Game(Server *srv, int gm_id) {
  Game *gm = NULL;

  pthread_mutex_lock(&srv->mutex);
  if (gm_id >= 0) {
    if (gm_id < MAX_GAMES) {
      gm = srv->games[gm_id];
    }
  } else {
    /*
            Поиск ид для новой игры
    */
    gm_id = srv->last_game_id;
    do {
      if (gm_id > MAX_GAMES - 2)
        gm_id = 0;
      else
        gm_id += 1;
      gm = srv->games[gm_id];
      if (gm == NULL)
        break;
    } while (gm_id != srv->last_game_id);
  }
  if (gm != NULL) {
    gm_id = -1;
    PINF("Game not created(max games)\n");
  } else {
    srv->games[gm_id] = malloc(sizeof(Game));
    if (srv->games[gm_id] == NULL) {
      PERR("create Game\n");
      gm_id = -1;
    } else {
      srv->games[gm_id]->id = gm_id;
      srv->games[gm_id]->x = NULL;
      srv->games[gm_id]->o = NULL;
      srv->games[gm_id]->is_run = FG_WAIT;
      srv->games[gm_id]->pid = -1;
      srv->last_game_id = gm_id;
      memset(srv->games[gm_id]->board, BN, 9);
      srv->games[gm_id]->board[9] = '\0';
      PINF("Game created: %u(gid)\n", gm_id);
    }
  }
  pthread_mutex_unlock(&srv->mutex);

  return gm_id;
}

/*
        Удаляет игру
        Использует мютекс сервера
*/
void delete_Game(Server *srv, unsigned int gm_id) {
  Game *gm = NULL;

  pthread_mutex_lock(&srv->mutex);
  if (gm_id >= 0 && gm_id < MAX_GAMES) {
    gm = srv->games[gm_id];
    srv->games[gm_id] = NULL;
  }
  pthread_mutex_unlock(&srv->mutex);
  if (gm == NULL) {
    return;
  }
  pthread_mutex_lock(&srv->mutex);
  if (gm->x != NULL) {
    if (srv->players[gm->x->id]->socket > 0)
      close(srv->players[gm->x->id]->socket);
    srv->players[gm->x->id] = NULL;
    free(gm->x);
  }
  if (gm->o != NULL) {
    if (srv->players[gm->o->id]->socket > 0)
      close(srv->players[gm->o->id]->socket);
    srv->players[gm->o->id] = NULL;
    free(gm->o);
  }
  pthread_mutex_unlock(&srv->mutex);
  PINF("Game deleted: %u(gid)\n", gm_id);
  free(gm);
}

/*
        Создает нового игрока
        Возвращает id нового игрока, либо -1
        Использует мютекс сервера
*/
int create_Player(Server *srv, int pl_id, unsigned int gid) {
  Player *pl = NULL;

  pthread_mutex_lock(&srv->mutex);
  if (pl_id >= 0) {
    if (pl_id < MAX_GAMES * 2) {
      pl = srv->players[pl_id];
    }
  } else {
    /*
            Поиск ид для нового игрока
    */
    pl_id = srv->last_player_id;
    do {
      if (pl_id > (MAX_GAMES * 2) - 2)
        pl_id = 0;
      else
        pl_id += 1;
      pl = srv->players[pl_id];
      if (pl == NULL)
        break;
    } while (pl_id != srv->last_player_id);
  }
  if (pl != NULL)
    pl_id = -1;
  else {
    srv->players[pl_id] = malloc(sizeof(Player));
    if (srv->players[pl_id] == NULL) {
      PERR("create Player\n");
      pl_id = -1;
    } else {
      srv->players[pl_id]->id = pl_id;
      srv->players[pl_id]->gid = gid;
      srv->players[pl_id]->ip[0] = '\0';
      srv->players[pl_id]->port = 0;
      srv->players[pl_id]->socket = 0;
      srv->players[pl_id]->name[0] = '\0';
      srv->last_player_id = pl_id;
      PINF("Player created: %u(pid)\n", pl_id);
    }
  }
  pthread_mutex_unlock(&srv->mutex);

  return pl_id;
}

void *games_game(void *arg) {
  Server *srv = ((struct pthread_games *)arg)->srv;
  Game *gm = ((struct pthread_games *)arg)->gm;
  int epl, epl_count;
  struct epoll_event events[2], ev[2];
  Data data;
  unsigned char gstatus;
  unsigned char sinch[2];
  unsigned int clt_tick[2] = {0, 0};
  int clt_sock[2] = {-1, -1};
  unsigned char fl_err = 0;
  SLNode *slnode;

  free((struct pthread_games *)arg);

  pthread_mutex_lock(&srv->mutex);
  gstatus = gm->is_run;
  if (gm->o != NULL) {
    clt_sock[0] = gm->o->socket;
  }
  if (gm->x != NULL) {
    clt_sock[1] = gm->x->socket;
  }
  pthread_mutex_unlock(&srv->mutex);

  sinch[0] = BN;
  epl = epoll_create(2);
  ev[0].events = EPOLLIN; // player-O
  ev[1].events = EPOLLIN; // player-X
  if (clt_sock[0] > 0) {
    ev[0].data.fd = clt_sock[0];
    epoll_ctl(epl, EPOLL_CTL_ADD, clt_sock[0], &ev[0]);
  }
  if (clt_sock[1] > 0) {
    ev[1].data.fd = clt_sock[1];
    epoll_ctl(epl, EPOLL_CTL_ADD, clt_sock[1], &ev[1]);
  }
  PINF("game is running: %u(gid)\n", gm->id);

  while (1) {
    epl_count = epoll_wait(epl, events, 2, 500);
    if (clt_sock[0] > 0) {
      clt_tick[0] += 1;
    }
    if (clt_sock[1] > 0) {
      clt_tick[1] += 1;
    }
    while (epl_count > 0) {
      epl_count -= 1;
      if (events[epl_count].data.fd == clt_sock[0]) {
        /*
                Клиент 1 (нолик)
        */
        if (recv(clt_sock[0], &data, Data_size, 0) <= 0) {
          PERR("receive data from clt_sock[0]\n");
          fl_err = fl_err | EBMA;
        } else
          switch (data.dtype) {
          case 1:
            switch (data.type) {
            case 3:
              clt_tick[0] = 0;
              pthread_mutex_lock(&srv->mutex);
              slnode = srv->servers;
              while (slnode != NULL) {
                sprintf(data.char_buff, "%s %u ", slnode->ip, slnode->g_port);
                data.dtype = 1;
                data.type = 4;
                if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
                  PERR("send data to clt_sock[0]\n");
                  fl_err = fl_err | EBMA;
                  break;
                }
                slnode = slnode->right;
              }
              pthread_mutex_unlock(&srv->mutex);
              break;
            case 4:
              clt_tick[0] = 0;
              break;
            default:
              WRND(&data);
            }
            break;
          case 2:
            switch (data.type) {
            case 1:
              clt_tick[0] = 0;
              /*if(sinch[0] != BN){
                      PERR("dissynchronization players\n");
                      goto game_exit;
              }*/
              pthread_mutex_lock(&srv->mutex);
              if (sinch[0] != BN || get_next_player(gm->board) != BO ||
                  data.uchar_1 > 8 || gm->board[data.uchar_1] != BN) {
                pthread_mutex_unlock(&srv->mutex);
                /*
                        Ход не принят
                */
              } else {
                /*
                        Ход сделан, но никто кроме сервера об этом не знает
                */
                pthread_mutex_unlock(&srv->mutex);
                sinch[0] = BO;
                sinch[1] = data.uchar_1;
              }
              break;
            default:
              WRND(&data);
            }
            break;
          case 3:
            switch (data.type) {
            case 1:
              clt_tick[0] = 0;
              if (gstatus != FG_PROCES && gstatus != FG_STOP) {
                data.type = 3;
                if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
                  PERR("send data to clt_sock[0]\n");
                  fl_err = fl_err | EBMA;
                }
              } else {
                if (clt_sock[1] > 0)
                  if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
                    PERR("send data to clt_sock[1]\n");
                    fl_err = fl_err | EBEX;
                  }
              }
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
      if (events[epl_count].data.fd == clt_sock[1]) {
        /*
                Клиент 2 (крестик)
        */
        if (recv(clt_sock[1], &data, Data_size, 0) <= 0) {
          PERR("receive data from clt_sock[1]\n");
          fl_err = fl_err | EBEX;
        } else
          switch (data.dtype) {
          case 1:
            switch (data.type) {
            case 3:
              clt_tick[1] = 0;
              pthread_mutex_lock(&srv->mutex);
              slnode = srv->servers;
              while (slnode != NULL) {
                sprintf(data.char_buff, "%s %u ", slnode->ip, slnode->g_port);
                data.dtype = 1;
                data.type = 4;
                if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
                  PERR("send data to clt_sock[1]\n");
                  fl_err = fl_err | EBEX;
                  break;
                }
                slnode = slnode->right;
              }
              pthread_mutex_unlock(&srv->mutex);
              break;
            case 4:
              clt_tick[1] = 0;
              break;
            default:
              WRND(&data);
            }
            break;
          case 2:
            switch (data.type) {
            case 1:
              clt_tick[1] = 0;
              /*if(sinch[0] != BN){
                      PERR("dissynchronization players\n");
                      goto game_exit;
              }*/
              pthread_mutex_lock(&srv->mutex);
              if (sinch[0] != BN || get_next_player(gm->board) != BX ||
                  data.uchar_1 > 8 || gm->board[data.uchar_1] != BN) {
                pthread_mutex_unlock(&srv->mutex);
                /*
                        Ход не принят
                */
              } else {
                /*
                        Ход сделан, но никто кроме сервера об этом не знает
                */
                pthread_mutex_unlock(&srv->mutex);
                sinch[0] = BX;
                sinch[1] = data.uchar_1;
              }
              break;
            default:
              WRND(&data);
            }
            break;
          case 3:
            switch (data.type) {
            case 1:
              clt_tick[1] = 0;
              if (gstatus != FG_PROCES && gstatus != FG_STOP) {
                data.type = 3;
                if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
                  PERR("send data to clt_sock[1]\n");
                  fl_err = fl_err | EBEX;
                }
              } else {
                if (clt_sock[0] > 0)
                  if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
                    PERR("send data to clt_sock[0]\n");
                    fl_err = fl_err | EBMA;
                  }
              }
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
    } // while(epl_count > 0)
    if (clt_tick[0] == TICK_SEND) {
      if (clt_sock[0] > 0) {
        data.dtype = 1;
        data.type = 3;
        if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[0]\n");
          fl_err = fl_err | EBMA;
        }
      }
    }
    if (clt_tick[1] == TICK_SEND) {
      if (clt_sock[1] > 0) {
        data.dtype = 1;
        data.type = 3;
        if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[1]\n");
          fl_err = fl_err | EBEX;
        }
      }
    }
    if (clt_tick[0] == TICK_DROP) {
      fl_err = fl_err | EBMA;
      PINF("timeout: clt_tick[0]\n");
      clt_tick[0] = 0;
    }
    if (clt_tick[1] == TICK_DROP) {
      fl_err = fl_err | EBEX;
      PINF("timeout: clt_tick[1]\n");
      clt_tick[1] = 0;
    }
    if ((fl_err & EBMA) == EBMA) {
      /*
              Если игрок-О отключился
      */
      fl_err = fl_err & (~EBMA);
      PINF("connect lost, clt_sock[0]\n");
      epoll_ctl(epl, EPOLL_CTL_DEL, clt_sock[0], &ev[0]);
      clt_sock[0] = 01;
      pthread_mutex_lock(&srv->mutex);
      if (gm->o != NULL)
        gm->o->socket = 0;
      pthread_mutex_unlock(&srv->mutex);
      if (gstatus == FG_STOP)
        gstatus = FG_DEST;
      else {
        if (!(clt_sock[1] > 0))
          gstatus = FG_DEST;
        else {
          sinch[0] = BX; //победивший символ
          gstatus = FG_END;
        }
      }
    }
    if ((fl_err & EBEX) == EBEX) {
      /*
              Если игрок-Х отключился
      */
      fl_err = fl_err & (~EBEX);
      PINF("connect lost, clt_sock[1]\n");
      epoll_ctl(epl, EPOLL_CTL_DEL, clt_sock[1], &ev[1]);
      clt_sock[1] = 0;
      pthread_mutex_lock(&srv->mutex);
      if (gm->x != NULL)
        gm->x->socket = 0;
      pthread_mutex_unlock(&srv->mutex);
      if (gstatus == FG_STOP)
        gstatus = FG_DEST;
      else {
        if (!(clt_sock[0] > 0))
          gstatus = FG_DEST;
        else {
          sinch[0] = BO; //победивший символ
          gstatus = FG_END;
        }
      }
    }
    switch (gstatus) {
    case FG_WAIT:
      // PINF("Game is waiting: %u\n", gm->id);
      if (clt_sock[1] > 0) {
        PERR("unexpected flag on game, FG_WAIT: %u\n", gm->id);
        goto game_exit;
      }
      if (!(clt_sock[0] > 0)) {
        pthread_mutex_lock(&srv->mutex);
        if (gm->o != NULL)
          clt_sock[0] = gm->o->socket;
        pthread_mutex_unlock(&srv->mutex);
        if (clt_sock[0] > 0) {
          ev[0].data.fd = clt_sock[0];
          epoll_ctl(epl, EPOLL_CTL_ADD, clt_sock[0], &ev[0]);
          PINF("player-O enter in game(reconnected): %u(pid), %u(gid)\n",
               gm->o->id,
               gm->o->gid);
        } else
          clt_tick[0] += 1;
      }
      pthread_mutex_lock(&srv->mutex);
      if (gm->x != NULL)
        clt_sock[1] = gm->x->socket;
      pthread_mutex_unlock(&srv->mutex);
      if (clt_sock[1] > 0) {
        ev[1].data.fd = clt_sock[1];
        epoll_ctl(epl, EPOLL_CTL_ADD, clt_sock[1], &ev[1]);
        gstatus = FG_PROCES;
        sinch[0] = BN;
        data.dtype = 2;
        data.type = 4;
        sprintf(data.char_buff, "%s", gm->x->name);
        if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[0]\n");
          fl_err = fl_err | EBMA;
        }
        sprintf(data.char_buff, "%s", gm->o->name);
        if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[1]\n");
          fl_err = fl_err | EBEX;
        }
        data.uchar_1 = BO;
        data.type = 3;
        if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[0]\n");
          fl_err = fl_err | EBMA;
        }
        if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[1]\n");
          fl_err = fl_err | EBEX;
        }
      }
      break;
    case FG_PROCES:
      // PINF("Game is processing: %u\n", gm->id);
      if (!(clt_sock[1] > 0) || !(clt_sock[0] > 0)) {
        PERR("unexpected flag on game, FG_PROCES: %u\n", gm->id);
        goto game_exit;
      }
      if (sinch[0] == BN)
        continue;
      data.dtype = 2;
      data.type = 2;
      data.uchar_1 = sinch[1];
      data.uchar_2 = sinch[0];
      pthread_mutex_lock(&srv->mutex);
      gm->board[sinch[1]] = sinch[0];
      sinch[0] = get_winner(gm->board);
      pthread_mutex_unlock(&srv->mutex);
      server_push_Message(srv, 2, gm->id, NULL);
      if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
        PERR("send data to clt_sock[0]\n");
        fl_err = fl_err | EBMA;
      }
      if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
        PERR("send data to clt_sock[1]\n");
        fl_err = fl_err | EBEX;
      }
      if (sinch[0] != BN) {
        gstatus = FG_END;
        continue;
      }
      if (data.uchar_2 == BX)
        data.uchar_1 = BO;
      else
        data.uchar_1 = BX;
      data.type = 3;
      if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
        PERR("send data to clt_sock[0]\n");
        fl_err = fl_err | EBMA;
      }
      if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
        PERR("send data to clt_sock[1]\n");
        fl_err = fl_err | EBEX;
      }
      break;
    case FG_END:
      // PINF("game ended: %u\n", gm->id);
      gstatus = FG_STOP;
      data.dtype = 2;
      data.type = 1;
      data.uchar_1 = sinch[0];
      if (clt_sock[0] > 0) {
        if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[0]\n");
          fl_err = fl_err | EBMA;
        }
      } else
        gstatus = FG_DEST;
      if (clt_sock[1] > 0) {
        if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
          PERR("send data to clt_sock[1]\n");
          fl_err = fl_err | EBEX;
        }
      } else
        gstatus = FG_DEST;
      break;
    case FG_DEST:
      goto game_exit;
      break;
    case FG_RECO:
      // PINF("Game wait reconnect: %u\n", gm->id);
      if (clt_sock[1] > 0 && clt_sock[0] > 0) {
        PERR("unexpected flag on game, FG_RECO: %u\n", gm->id);
        gstatus = FG_PROCES;
        break;
      }
      if (!(clt_sock[0] > 0)) {
        pthread_mutex_lock(&srv->mutex);
        if (gm->o != NULL)
          clt_sock[0] = gm->o->socket;
        pthread_mutex_unlock(&srv->mutex);
        if (clt_sock[0] > 0) {
          ev[0].data.fd = clt_sock[0];
          epoll_ctl(epl, EPOLL_CTL_ADD, clt_sock[0], &ev[0]);
          PINF("player-O enter in game(reconnected): %u(pid), %u(gid)\n",
               gm->o->id,
               gm->o->gid);
          if (clt_sock[1] > 0) {
            gstatus = FG_PROCES;
            sinch[0] = BN;
            pthread_mutex_lock(&srv->mutex);
            data.uchar_1 = get_next_player(gm->board);
            pthread_mutex_unlock(&srv->mutex);
            if (data.uchar_1 != BX && data.uchar_1 != BO) {
              PERR("unexpected game simbol, %u: %u\n", data.uchar_1, gm->id);
              goto game_exit;
            }
            data.dtype = 2;
            data.type = 3;
            if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
              PERR("send data to clt_sock[0]\n");
              fl_err = fl_err | EBMA;
            }
            if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
              PERR("send data to clt_sock[1]\n");
              fl_err = fl_err | EBEX;
            }
          }
        } else
          clt_tick[0] += 1;
      }
      if (!(clt_sock[1] > 0)) {
        pthread_mutex_lock(&srv->mutex);
        if (gm->x != NULL)
          clt_sock[1] = gm->x->socket;
        pthread_mutex_unlock(&srv->mutex);
        if (clt_sock[1] > 0) {
          ev[1].data.fd = clt_sock[1];
          epoll_ctl(epl, EPOLL_CTL_ADD, clt_sock[1], &ev[1]);
          PINF("player-X enter in game(reconnected): %u(pid), %u(gid)\n",
               gm->x->id,
               gm->x->gid);
          if (clt_sock[0] > 0) {
            gstatus = FG_PROCES;
            sinch[0] = BN;
            pthread_mutex_lock(&srv->mutex);
            data.uchar_1 = get_next_player(gm->board);
            pthread_mutex_unlock(&srv->mutex);
            if (data.uchar_1 != BX && data.uchar_1 != BO) {
              PERR("unexpected game simbol, %u: %u\n", data.uchar_1, gm->id);
              goto game_exit;
            }
            data.dtype = 2;
            data.type = 3;
            if (send(clt_sock[0], &data, Data_size, 0) <= 0) {
              PERR("send data to clt_sock[0]\n");
              fl_err = fl_err | EBMA;
            }
            if (send(clt_sock[1], &data, Data_size, 0) <= 0) {
              PERR("send data to clt_sock[1]\n");
              fl_err = fl_err | EBEX;
            }
          }
        } else
          clt_tick[1] += 1;
      }
      break;
    }
  }

  pthread_exit(0);

game_exit:
  server_push_Message(srv, 6, gm->id, NULL);
  delete_Game(srv, gm->id);
  pthread_exit(0);
}

void *server_games(void *arg) {
  Server *srv = (Server *)arg;
  int new_client;
  struct sockaddr_in addr;
  socklen_t addr_len;
  int epl, epl_count;
  struct epoll_event events[1], ev;
  Data data;
  int i, j;
  unsigned char new_client_opt = 0;
  struct pthread_games *pg_data;

  if (listen(srv->clients_socket, 5) == -1) {
    PERR("games: creat listen socket\n");
    goto pth_exit;
  }
  epl = epoll_create(1);
  PINF("games service is running\n");

  ev.events = EPOLLIN; // games_socket

  while (1) {
    new_client =
            accept(srv->clients_socket, (struct sockaddr *)&addr, &addr_len);
    if (new_client <= 0) {
      PERR("accept new_client\n");
      continue;
    }
    PINF("new client connected: %s:%d\n",
         inet_ntoa(addr.sin_addr),
         ntohs(addr.sin_port));
    pthread_mutex_lock(&srv->mutex);
    if (srv->main_server_socket > 0) {
      pthread_mutex_unlock(&srv->mutex);
      close(new_client);
      PINF("i'm not a main server, connection dropped: %s:%d\n",
           inet_ntoa(addr.sin_addr),
           ntohs(addr.sin_port));
      continue;
    }
    pthread_mutex_unlock(&srv->mutex);
    ev.data.fd = new_client;
    epoll_ctl(epl, EPOLL_CTL_ADD, new_client, &ev);
    epl_count = epoll_wait(epl, events, 1, 2000);
    epoll_ctl(epl, EPOLL_CTL_DEL, new_client, &ev);
    if (epl_count == 0) {
      PINF("client not registered (timeout): %s:%d\n",
           inet_ntoa(addr.sin_addr),
           ntohs(addr.sin_port));
    } else {
      if (recv(new_client, &data, Data_size, 0) <= 0) {
        PINF("client not registered (error receive): %s:%d\n",
             inet_ntoa(addr.sin_addr),
             ntohs(addr.sin_port));
      } else
        switch (data.dtype) {
        case 1:
          switch (data.type) {
          case 1:
            /*
                    Запрос на регистрацию
            */
            switch (data.uchar_1) {
            case 0:
              new_client_opt = 1;
              break;
            case 1:
              new_client_opt = 2;
              break;
            default:
              WRND(&data);
            }
            break;
          case 2:
            /*
                    Запрос на переподключение
            */
            j = data.uchar_1;
            i = data.uchar_2;
            pthread_mutex_lock(&srv->mutex);
            if (!(j > MAX_GAMES * 2 || i > MAX_GAMES) &&
                srv->players[j] != NULL) {
              srv->players[j]->socket = new_client;
              pthread_mutex_unlock(&srv->mutex);
              new_client = 0;
              PINF("client reconnected: %u(pid), %u(gid)\n", j, i);
              break;
            }
            pthread_mutex_unlock(&srv->mutex);
            data.dtype = 1;
            data.type = 2;
            data.uchar_1 = 2;
            if (send(new_client, &data, Data_size, 0) <= 0) {
              PERR("send data to new_client\n");
            }
            break;
          default:
            WRND(&data);
          }
          break;
        default:
          WRND(&data);
        }
      if (new_client_opt == 1) {
        /*
                Поиск свободной игры
        */
        new_client_opt = 2;
        for (i = 0; i < MAX_GAMES; i++) {
          pthread_mutex_lock(&srv->mutex);
          if (srv->games[i] != NULL && srv->games[i]->is_run == FG_WAIT) {
            new_client_opt = 0;
            pthread_mutex_unlock(&srv->mutex);
            j = create_Player(srv, -1, i);
            if (j == -1) {
              PINF("client not registered (error create Player): %s:%d\n",
                   inet_ntoa(addr.sin_addr),
                   ntohs(addr.sin_port));
              data.dtype = 1;
              data.type = 2;
              data.uchar_1 = 2;
              if (send(new_client, &data, Data_size, 0) <= 0) {
                PERR("send data to new_client\n");
              }
            } else {
              /*
                      Добавляем игрока в игру
                      "Запуск" игры
              */
              pthread_mutex_lock(&srv->mutex);
              srv->players[j]->symbol = BX;
              strcpy(srv->players[j]->ip, inet_ntoa(addr.sin_addr));
              srv->players[j]->port = ntohs(addr.sin_port);
              srv->players[j]->socket = new_client;
              data.char_buff[NAME_LEN - 1] = '\0';
              sprintf(srv->players[j]->name, "%s", data.char_buff);
              srv->games[i]->x = srv->players[j];
              srv->games[i]->is_run = FG_PROCES;
              pthread_mutex_unlock(&srv->mutex);
              data.dtype = 1;
              data.type = 1;
              data.uchar_1 = j;
              data.uchar_2 = i;
              data.uchar_3 = BX;
              if (send(new_client, &data, Data_size, 0) <= 0) {
                PERR("send data to new_client\n");
              }
              new_client = 0;
              PINF("client registered: %s:%d, %u(Pid), %u(Gid), X(smb), %s\n",
                   inet_ntoa(addr.sin_addr),
                   ntohs(addr.sin_port),
                   j,
                   i,
                   data.char_buff);
              server_push_Message(srv, 3, j, NULL);
            }
            break;
          } else
            pthread_mutex_unlock(&srv->mutex);
        }
      }
      if (new_client_opt == 2) {
        /*
                Создание игры
        */
        new_client_opt = 0;
        i = create_Game(srv, -1);
        if (i == -1) {
          PINF("client not registered (error create Game): %s:%d\n",
               inet_ntoa(addr.sin_addr),
               ntohs(addr.sin_port));
          data.dtype = 1;
          data.type = 2;
          data.uchar_1 = 3;
          if (send(new_client, &data, Data_size, 0) <= 0) {
            PERR("send data to new_client\n");
          }
        } else {
          j = create_Player(srv, -1, i);
          if (j == -1) {
            delete_Game(srv, i);
            PINF("client not registered (error create Player): %s:%d\n",
                 inet_ntoa(addr.sin_addr),
                 ntohs(addr.sin_port));
            data.dtype = 1;
            data.type = 2;
            data.uchar_1 = 1;
            if (send(new_client, &data, Data_size, 0) <= 0) {
              PERR("send data to new_client\n");
            }
          } else {
            /*
                    Добавляем игрока в игру
                    "Создание" игры
            */
            pthread_mutex_lock(&srv->mutex);
            srv->players[j]->symbol = BO;
            strcpy(srv->players[j]->ip, inet_ntoa(addr.sin_addr));
            srv->players[j]->port = ntohs(addr.sin_port);
            srv->players[j]->socket = new_client;
            data.char_buff[NAME_LEN - 1] = '\0';
            sprintf(srv->players[j]->name, "%s", data.char_buff);
            srv->games[i]->o = srv->players[j];
            srv->games[i]->is_run = FG_WAIT;
            pg_data = malloc(sizeof(struct pthread_games));
            if (pg_data == NULL) {
              PERR("malloc pg_data\n");
              goto pth_exit;
            }
            pg_data->srv = srv;
            pg_data->gm = srv->games[i];
            pthread_create(&srv->games[i]->pid, NULL, games_game, pg_data);
            pthread_mutex_unlock(&srv->mutex);
            data.dtype = 1;
            data.type = 1;
            data.uchar_1 = j;
            data.uchar_2 = i;
            data.uchar_3 = BO;
            if (send(new_client, &data, Data_size, 0) <= 0) {
              PERR("send data to new_client\n");
            }
            new_client = 0;
            PINF("client registered: %s:%d, %u(Pid), %u(Gid), O(smb), %s\n",
                 inet_ntoa(addr.sin_addr),
                 ntohs(addr.sin_port),
                 j,
                 i,
                 data.char_buff);
            server_push_Message(srv, 5, i, NULL);
            server_push_Message(srv, 3, j, NULL);
          }
        }
      }
    }
    if (new_client > 0)
      close(new_client);
  }

pth_exit:
  PINF("server_games is fault\n");
  server_push_Message(srv, 8, 0, NULL);
  pthread_exit(0);
}
