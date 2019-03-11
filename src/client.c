#include "./../include/main.h"

int game_state = !GAME_IN_PROG;
int reconnect = TRUE;
int diesconnect = FALSE;
char hostname[256] = "localhost";
int port = PORT; /* PORT*/
extern int client_socket_read;
extern int client_socket_write;
extern int state_connection;
extern packet_t reader_buffer[];
extern packet_t writer_buffer[];
extern int reader_buffer_len;
extern int writer_buffer_len;
struct hostent *hostIP;
extern pthread_mutex_t connection_mutex;
extern pthread_mutex_t reader_mutex;
extern pthread_mutex_t writer_mutex;
extern pthread_mutex_t helper_mutex;

int reconnection();

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

void *client_reader() {
  int recv_result;
  int poll_return;
  struct pollfd pfd;

  printf("Start Reader!\n");
client_reader_start:
  recv_result = 0;
  poll_return = 0;
  while (1) {
    while (!check_connection())
      ;
    //    pthread_mutex_lock(&reader_mutex);
    //    pthread_mutex_lock(&connection_mutex);
    recv_result =
            recv(client_socket_read,
                 &reader_buffer[reader_buffer_len],
                 sizeof(reader_buffer[reader_buffer_len]),
                 0);
    //    pthread_mutex_unlock(&connection_mutex);
    printf("Client Reader : Receive %d\n", recv_result);
    if (reader_buffer[reader_buffer_len].type != CONN_ACK)
      send_ack(0);
    if (reader_buffer_len < MAXDATASIZE - 1)
      reader_buffer_len++;

    //    pthread_mutex_unlock(&reader_mutex);
  }
}

void *client_writer() {
  int send_result;
  int trying_send;
  printf("Start Writer!\n");
client_writer_start:
  send_result = 0;
  trying_send = 0;
  while (1) {
    while (!check_connection())
      ;
    pthread_mutex_lock(&writer_mutex);
    if (writer_buffer_len > 0) {
      // connection mutex
      pthread_mutex_lock(&connection_mutex);
      send_result =
              send(client_socket_write,
                   &writer_buffer[writer_buffer_len - 1],
                   sizeof(writer_buffer[writer_buffer_len - 1]),
                   0);
      pthread_mutex_unlock(&connection_mutex);
      if (writer_buffer[writer_buffer_len].type == CONN_ACK || wait_ack(0)) {
        --writer_buffer_len;
        trying_send = 0;
        printf("Client Writer : Send with: %d\n", send_result);
      } else {
        ++trying_send;
        printf("Client Writer : Ack is not receive. Resending! %d \n",
               trying_send);
      }
      if (trying_send >= 10) {
        // connection mutex
        pthread_mutex_lock(&connection_mutex);
        state_connection = FALSE;
        pthread_mutex_unlock(&connection_mutex);
        trying_send = 0;
        printf("Client Writer : Ack is not receive. Connection drop!\n");
      }
    }
    pthread_mutex_unlock(&writer_mutex);
  }
}

void read_socket_from_file(FILE *in_descriptor, char *hostname, int *port) {
  char buffer[256] = {0};
  int index = 0;
  fscanf(in_descriptor, "%s", buffer);
  while (buffer[index] != ':') { index++; }
  buffer[index] = '\0';
  *port = atoi(&buffer[index + 1]);
  strcpy(hostname, buffer);
}

int main(int argc, char **argv) {
  int server_counts = 0;
  int index = 0;
  FILE *in_descriptor = NULL;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;
  while (1) {
    if (!check_connection()) {
      ++index;
      if (in_descriptor == NULL) {
        in_descriptor = fopen("ippool.dat", "r");
        fscanf(in_descriptor, "%d", &server_counts);
      }

      read_socket_from_file(in_descriptor, hostname, &port);
      printf("Connecting to %s:%d\n", hostname, port);
      if ((gethostname(hostname, sizeof(hostname))) == 0) {
        hostIP = gethostbyname(hostname);
      } else {
        fprintf(stderr, "ERROR: - IP Address not found.");
        exit(EXIT_FAILURE);
      }
      // connection mutex
      client_socket_write = client_tcp_connect(hostIP, port);
      client_socket_read = client_tcp_connect(hostIP, port + 1);
      if (client_socket_write == -1 || client_socket_read == -1) {
        printf("Connection fail.\n");
        state_connection = CONN_FALSE;
        if (server_counts == index) {
          printf("All servers unreacheble!\n");
          break;
        }
      } else {
        state_connection = CONN_TRUE;
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
      send_packet(make_packet(CONN_NEW, NULL));
      printf("Start\n");
      while (check_connection()) {
        //        if (game_state != GAME_IN_PROG)
        send_packet(make_packet(SERVICE, NULL));
        sleep(5);
        game_state = GAME_IN_PROG;
        // process
        // Здесь должен быть курсач
      }

      if (game_state) {
        index = 0;
        // connection mutex
        pthread_mutex_lock(&connection_mutex);
        state_connection = CONN_FALSE;
        pthread_mutex_unlock(&connection_mutex);
        printf("Connection drop. Trying reconnect.\n");
        close(client_socket_write);
        close(client_socket_read);
        client_socket_write = -1;
        client_socket_read = -1;
        if (in_descriptor != NULL)
          fclose(in_descriptor);
        // connection mutex
        pthread_mutex_lock(&connection_mutex);
        state_connection = reconnection();
        pthread_mutex_unlock(&connection_mutex);
        if (check_connection())
          printf("Reconnect!\n");
      }
    }
  }
  if (writer_tid != -1)
    pthread_join(writer_tid, NULL);
  if (reader_tid != -1)
    pthread_join(reader_tid, NULL);
  close(client_socket_write);
  close(client_socket_read);
  return 0;
}

int reconnection() {
  int trying_reconnect = 0;

  while (trying_reconnect++ < 50) {
    // mutex
    client_socket_write = client_tcp_connect(hostIP, port);
    client_socket_read = client_tcp_connect(hostIP, port + 1);
    if (client_socket_write != -1 && client_socket_read != -1) {
      return CONN_TRUE;
    } else {
      printf("Reconnect fail!\n");
    }
  }
  return CONN_FALSE;
}
