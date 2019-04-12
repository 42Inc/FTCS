#include "./../include/main.h"

int game_state = !GAME_IN_PROG;
int reconnect = TRUE;
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

int client_id = -1;
int game_id = -1;

void *client_reader() {
  int recv_result;
  int poll_return;
  struct pollfd pfd;
  packet_t p;

client_reader_start:
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

void *client_writer() {
  int send_result;
  int trying_send;
  packet_t p;
client_writer_start:
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

void connection() {
}

int main(int argc, char **argv) {
  int server_counts = 0;
  int index = 0;
  FILE *in_descriptor = NULL;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  srv_t *cursor = NULL;
  packet_t p;
  reader_buffer = (packets_t *)malloc(sizeof(packets_t));
  writer_buffer = (packets_t *)malloc(sizeof(packets_t));
  memset(reader_buffer, 0, sizeof(packets_t));
  memset(writer_buffer, 0, sizeof(packets_t));
  while (1) {
    if (!check_connection()) {
      if (known_servers == NULL) {
        read_servers_pool("ippool.dat");
        cursor = known_servers->srvs;
      }

      fprintf(stderr, "Connecting to %s:%d\n", cursor->ip, cursor->port);
      if ((gethostname(cursor->ip, sizeof(cursor->ip))) == 0) {
        hostIP = gethostbyname(cursor->ip);
      } else {
        fprintf(stderr, "ERROR: - IP Address not found.");
        exit(EXIT_FAILURE);
      }
      // connection mutex
      printf("5");
      client_socket_write = client_tcp_connect(hostIP, cursor->port);
      printf("6");
      client_socket_read = client_tcp_connect(hostIP, cursor->port + 1);
      printf("7");
      if (client_socket_write == -1 || client_socket_read == -1) {
        fprintf(stderr, "Connection fail.\n");
        state_connection = CONN_FALSE;
        ++index;
        cursor = cursor->next;
        if (known_servers->count == index) {
          fprintf(stderr, "All servers unreacheble!\n");
          break;
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
          ;
        if (p.type == CONN_EST) {
          send_packet(make_packet(CONN_CLIENT, client_id, 0, NULL));
        } else if (p.type == SERVICE) {
          if (client_id == -1)
            client_id = p.client_id;
          break;
        }
      }
      if (state_connection == CONN_TRUE)
        printf("Connection established. Client ID : %d\n", client_id);
      while (check_connection()) {
        send_packet(make_packet(SERVICE, client_id, 0, NULL));
        sleep(2);
        // Place course work here
      }

      if (game_state) {
        index = 0;
        pthread_mutex_lock(&connection_mutex);
        state_connection = CONN_FALSE;
        pthread_mutex_unlock(&connection_mutex);
        fprintf(stderr, "Connection drop. Trying reconnect.\n");
        close(client_socket_write);
        close(client_socket_read);
        client_socket_write = -1;
        client_socket_read = -1;

        fprintf(stderr, "Reload config file\n");
        cursor = known_servers->srvs;
        pthread_mutex_lock(&connection_mutex);
        state_connection = reconnection();
        pthread_mutex_unlock(&connection_mutex);
        if (check_connection())
          fprintf(stderr, "Reconnect!\n");
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

int reconnection() {
  int trying_reconnect = 0;

  while (trying_reconnect++ < 50) {
    client_socket_write = client_tcp_connect(hostIP, port);
    client_socket_read = client_tcp_connect(hostIP, port + 1);
    if (client_socket_write != -1 && client_socket_read != -1) {
      return CONN_TRUE;
    } else {
      fprintf(stderr, "Reconnect fail!\n");
    }
  }
  return CONN_FALSE;
}
