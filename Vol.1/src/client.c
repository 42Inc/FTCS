#include "./../include/main.h"

int game_state = !GAME_IN_PROG;
int reconnect = TRUE;
int refresh = TRUE;
int disconnect = FALSE;
extern char hostname[MAXDATASIZE];
extern int port;
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
struct hostent *hostIP;
extern pthread_mutex_t connection_mutex;
extern pthread_mutex_t reader_mutex;
extern pthread_mutex_t writer_mutex;
extern pthread_mutex_t helper_mutex;
extern srv_pool_t *known_servers;

short int cursor_x = 0;
short int cursor_y = 0;
int echoIgn = 0;
char field[10] = "AAAAAAAAA";
// static int symX[2] = {405029505, 2168595480};
// static int symO[2] = {2172748158, 2122416513};
// static int symA[2] = {0, 0};
int client_id = -1;
int game_id = -1;
int chat_mode = FALSE;

struct sigaction pipes;
sigset_t setpipes;
static struct termios originalTerm;

void sigpipes_handler(int s, siginfo_t *info, void *param) {
  //  pthread_mutex_lock(&connection_mutex);
  while (state_connection != FALSE) state_connection = FALSE;
  //  pthread_mutex_unlock(&connection_mutex);
}
/*---------------------------------------------------------------------------*/
void *client_reader() {
  int recv_result;
  int poll_return;
  struct pollfd pfd;
  packet_t p;

  recv_result = 0;
  poll_return = 0;
  pfd.fd = client_socket_read;
  pfd.events = POLLIN | POLLHUP | POLLRDNORM;
  while (1) {
    while (!check_connection()) {
      if (reader_join)
        return NULL;
    }
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
void *client_writer() {
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
void printInt() {
  int i = 0;
  mt_clrscr();
  for (i = 0; i < 9; i++) {
    if (i == cursor_y * 3 + cursor_x) {
      mt_setbgcolor(clr_blue);
      mt_setfgcolor(clr_red);
      printf("%c", field[i]);
      mt_setbgcolor(clr_default);
      mt_setfgcolor(clr_default);
    } else
      printf("%c", field[i]);
    if ((i + 1) % 3 == 0)
      printf("\n");
  }
  //  printBox();
}
/*---------------------------------------------------------------------------*/
void printBox() {
  bc_box(1, 1, 10, 10);
  bc_box(11, 1, 20, 10);
  bc_box(21, 1, 30, 10);
  bc_box(1, 11, 10, 20);
  bc_box(11, 11, 20, 20);
  bc_box(21, 11, 30, 20);
  bc_box(1, 21, 10, 30);
  bc_box(11, 21, 20, 30);
  bc_box(21, 21, 30, 30);
}
/*---------------------------------------------------------------------------*/
void setEchoRegime() {
  if (echoIgn == 0) {
    while (tcgetattr(STDIN_FILENO, &originalTerm) != 0)
      ;
    rk_mytermregime(0, 0, 1, 1, 1);
    echoIgn = 1;
  } else {
    return;
  }
}
/*---------------------------------------------------------------------------*/
void restoreEchoRegime() {
  if (echoIgn == 1) {
    while (tcsetattr(STDIN_FILENO, TCSANOW, &originalTerm) != 0)
      ;
    echoIgn = 0;
  } else {
    return;
  }
}
/*---------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  int index = 0;
  int trying = 20;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  srv_t *cursor = NULL;
  packet_t p;
  char chat_buffer[MAXDATASIZE];
  char buff[20];
  char chat_sym = -1;
  int chat_index = 0;
  int game_id = 0;
  enum keys key = KEY_other;
  int get_field = 0;

  sigemptyset(&setpipes);
  sigaddset(&setpipes, SIGPIPE);
  pipes.sa_sigaction = sigpipes_handler;
  pipes.sa_mask = setpipes;
  pipes.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGPIPE, &pipes, NULL) == -1) {
    fprintf(stderr, "SIGPIPE");
    exit(1);
  }
  reader_buffer = (packets_t *)malloc(sizeof(packets_t));
  writer_buffer = (packets_t *)malloc(sizeof(packets_t));
  memset(reader_buffer, 0, sizeof(packets_t));
  memset(writer_buffer, 0, sizeof(packets_t));
  rk_mytermsave();
  mt_clrscr();
  while (1) {
    if (!check_connection()) {
      if (known_servers == NULL) {
        read_servers_pool("ippool.dat");
        cursor = known_servers->srvs;
      }
    connection:
      fprintf(stderr, "Connecting to %s:%d\n", cursor->ip, cursor->port);
      if ((gethostname(cursor->ip, sizeof(cursor->ip))) == 0) {
        hostIP = gethostbyname(cursor->ip);
      } else {
        fprintf(stderr, "ERROR: IP Address not found.");
        exit(EXIT_FAILURE);
      }
      // connection mutex
      client_socket_write = client_tcp_connect(hostIP, cursor->port);
      client_socket_read = client_tcp_connect(hostIP, cursor->port + 1);
      if (client_socket_write == -1 || client_socket_read == -1) {
        fprintf(stderr, "Connection fail.\n");
        state_connection = CONN_FALSE;
        ++index;
        cursor = cursor->next;
        if (known_servers->count == index) {
          fprintf(stderr, "All servers unreacheble!\n");
          trying--;
          if (trying >= 0) {
            printf("Trying:\n");
            cursor = known_servers->srvs;
            trying--;
            index = 0;
            sleep(1);
            goto connection;
          } else {
            trying = 20;
            break;
          }
        }
        if (cursor == NULL) {
          fprintf(stderr, "End of list!\n");
          break;
        }
      } else {
        state_connection = CONN_TRUE;
        index = 0;
      }
    }
    if (check_connection()) {
      // reader & writer thread start
      if (reader_tid == -1 && writer_tid == -1) {
        pthread_attr_init(&reader_attr);
        pthread_attr_init(&writer_attr);
        pthread_create(&reader_tid, &reader_attr, client_reader, NULL);
        pthread_create(&writer_tid, &writer_attr, client_writer, NULL);
      }
      // make hello packet
      send_packet(make_packet(CONN_NEW, 0, 0, NULL));
      game_state = GAME_IN_PROG;
      while (check_connection()) {
        while (!get_packet(&p))
          if (!check_connection())
            break;
        fprintf(stderr,
                "RESPOND [id: %d %d | %s | %d]\n",
                p.client_id,
                client_id,
                p.buffer,
                p.type);
        if (p.type == CONN_EST) {
          send_packet(make_packet(CONN_CLIENT, client_id, game_id, NULL));
        } else if (p.type == SERVICE) {
          fprintf(stderr, "SERVICE [id: %d | %s]\n", p.client_id, p.buffer);
          if (!strcmp(p.buffer, "set_id"))
            client_id = p.client_id;
          if (!strcmp(p.buffer, "start_game")) {
            fprintf(stdout, "Start game. ID : %d\n", p.packet_id);
            game_id = p.packet_id;
          }
        } else if (p.type == CHANGE_FIELD) {
          get_field = 1;
          strcpy(field, p.buffer);
        }
        if (client_id > 0 && game_id > 0 && get_field > 0)
          break;
      }
      if (state_connection == CONN_TRUE)
        fprintf(stdout, "Connection established. Client ID : %d\n", client_id);
      while (check_connection()) {
        if (refresh == TRUE) {
          printInt();
          refresh = FALSE;
        }
        if (!chat_mode) {
          rk_readkey(&key, FALSE);
          chat_sym = -1;
          switch (key) {
          case KEY_left:
            cursor_x = (cursor_x - 1) < 0 ? 2 : (cursor_x - 1);
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_right:
            cursor_x = (cursor_x + 1) % 3;
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_up:
            cursor_y = (cursor_y - 1) < 0 ? 2 : (cursor_y - 1);
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_down:
            cursor_y = (cursor_y + 1) % 3;
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_esc:
            chat_mode = TRUE;
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_enter:
            sprintf(buff, "%d", cursor_y * 3 + cursor_x);
            send_packet(make_packet(CHANGE_FIELD, client_id, 0, buff));
            refresh = TRUE;
            key = KEY_other;
            break;
          default:
            break;
          }
        } else {
          setEchoRegime();
          chat_sym = rk_readkey(&key, TRUE);
          restoreEchoRegime();
          switch (key) {
          case KEY_esc:
            chat_mode = FALSE;
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_enter:
            chat_buffer[chat_index] = '\0';
            send_packet(make_packet(MSG, client_id, 0, chat_buffer));
            chat_index = 0;
            refresh = TRUE;
            key = KEY_other;
            break;
          case KEY_alpha:
            if (chat_index < MAXDATASIZE - 1 && chat_sym != -1) {
              chat_buffer[chat_index++] = chat_sym;
              if (chat_index == MAXDATASIZE - 1)
                chat_buffer[chat_index] = '\0';
            }
            key = KEY_other;
            break;
          default:
            break;
          }
        }
        send_packet(make_packet(SERVICE, client_id, 0, "Check"));
        if (get_packet(&p) == TRUE) {
          fprintf(stderr,
                  "RESPOND [id: %d %d | %s | %d]\n",
                  p.client_id,
                  client_id,
                  p.buffer,
                  p.type);
          if (p.type == SERVICE) {
            if (!strcmp(p.buffer, "set_id")) {
              client_id = p.client_id;
              //              mt_gotoXY(1, 8);
              fprintf(stderr, "Change client id [id: %d]\n", client_id);
            } else if (!strcmp(p.buffer, "winner")) {
              //              mt_gotoXY(1, 8);
              fprintf(stdout, "You win!:)\n");
              game_state = FALSE;
              state_connection = FALSE;
              break;
            } else if (!strcmp(p.buffer, "looser")) {
              //              mt_gotoXY(1, 8);
              fprintf(stdout, "You Lose!:(\n");
              game_state = FALSE;
              state_connection = FALSE;
              break;
            }
          } else if (p.type == CHANGE_FIELD) {
            strcpy(field, p.buffer);
          } else if (p.type == MSG) {
            fprintf(stdout, "Msg: %s\n", p.buffer);
          }
          refresh = TRUE;
        }
      }

      if (game_state) {
        index = 0;
        pthread_mutex_lock(&connection_mutex);
        state_connection = CONN_FALSE;
        pthread_mutex_unlock(&connection_mutex);
        close(client_socket_write);
        close(client_socket_read);
        client_socket_write = -1;
        client_socket_read = -1;

        cursor = known_servers->srvs;
        pthread_mutex_lock(&connection_mutex);
        state_connection = reconnection();
        pthread_mutex_unlock(&connection_mutex);
      } else {
        // End of game
        break;
      }
    }
  }
  fprintf(stderr, "Wait for join threads\n");
  if (writer_tid != -1) {
    while (writer_join != 1) writer_join = 1;
    pthread_join(writer_tid, NULL);
    fprintf(stderr, "Writer join\n");
  }
  if (reader_tid != -1) {
    while (reader_join != 1) reader_join = 1;
    pthread_join(reader_tid, NULL);
    fprintf(stderr, "Reader join\n");
  }
  close(client_socket_write);
  close(client_socket_read);
  return 0;
}
/*---------------------------------------------------------------------------*/
int reconnection() {
  int trying_reconnect = 0;

  while (trying_reconnect++ < 50) {
    client_socket_write = client_tcp_connect(hostIP, port);
    client_socket_read = client_tcp_connect(hostIP, port + 1);
    if (client_socket_write != -1 && client_socket_read != -1) {
      return CONN_TRUE;
    }
  }
  return CONN_FALSE;
}
/*---------------------------------------------------------------------------*/
