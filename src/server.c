#include "./../include/main.h"

int games = GAMES;

static int games_available = GAMES;

int server_socket_read = - 1;
int server_socket_write = - 1;
extern int client_socket_read;
extern int client_socket_write;
extern int state_connection;
extern packet_t reader_buffer[];
extern packet_t writer_buffer[];
extern int reader_buffer_len;
extern int writer_buffer_len;
extern pthread_mutex_t connection_mutex;
extern pthread_mutex_t reader_mutex;
extern pthread_mutex_t writer_mutex;
extern pthread_mutex_t helper_mutex;

void* server_reader() {
  int recv_result;
  printf("Start Reader!\n");
  server_reader_start:
  recv_result = 0;
  while (1) {
    //connection mutex + poll
//    pthread_mutex_lock(&reader_mutex);
//    pthread_mutex_lock(&connection_mutex);
    recv_result = recv(client_socket_read, &reader_buffer[reader_buffer_len], sizeof(reader_buffer[reader_buffer_len]), 0);
//    pthread_mutex_unlock(&connection_mutex);
    printf("Receive %d\n", recv_result);
    send_ack();
    reader_buffer_len++;
//    pthread_mutex_unlock(&reader_mutex);
  }
}

void* server_writer() {
  int send_result;
  int trying_send;
  printf("Start Writer!\n");
  server_writer_start:
  send_result = 0;
  trying_send = 0;
  while (1) {
    while(!check_connection());
    pthread_mutex_lock(&writer_mutex);
    if (writer_buffer_len > 0) {
      //connection mutex
      pthread_mutex_lock(&connection_mutex);
      send_result = send(client_socket_write, &writer_buffer[writer_buffer_len], sizeof(writer_buffer[writer_buffer_len]), 0);
      pthread_mutex_unlock(&connection_mutex);
      if (wait_ack()) {
        --writer_buffer_len;
        trying_send = 0;
        printf("Send with: %d\n", send_result);
      } else {
        ++trying_send;
        printf("Ack is not receive. Resending!\n");
      }
      if (trying_send >= 10) {
        //connection mutex
        pthread_mutex_lock(&connection_mutex);
        state_connection = FALSE;
        pthread_mutex_unlock(&connection_mutex);
        trying_send = 0;
        printf("Ack is not receive. Connection drop!\n");
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

  if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
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
  if ((connection = accept(server_socket, (struct sockaddr*) &connection_addr, &len)) < 0) {
    fprintf(stderr, "accept failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "Client conneted, ip: %s\n", inet_ntoa(connection_addr.sin_addr));
  return connection;
}

int main(int argc, char** argv)
{
  int port = PORT; // listen port
  int opt;
  int recv_result = 0;
  struct sockaddr_in my_addr; // host addr
  struct sockaddr_in their_addr[2]; // client
  socklen_t sin_size[2];
  FILE* in_descriptor = NULL;
  pthread_t reader_tid = -1;
  pthread_t writer_tid = -1;
  pthread_attr_t reader_attr;
  pthread_attr_t writer_attr;

  opterr = 0;

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

  printf("Listening ports : %d-%d\n", port, port + 1);
  printf("Games: %d\n", games);

  server_socket_read = create_server_tcp_socket(htonl(INADDR_ANY), port);
  server_socket_write = create_server_tcp_socket(htonl(INADDR_ANY), port + 1);

  int count = 0;
  while(games_available > 0) {
    //connection mutex
    client_socket_read = accept_tcp_connection(server_socket_read);
    client_socket_write = accept_tcp_connection(server_socket_write);
    // Child process
    if (!fork()) {
      //connection mutex
      state_connection = CONN_TRUE;
      pthread_attr_init(&reader_attr);
      pthread_attr_init(&writer_attr);
      pthread_create(&reader_tid, &reader_attr, server_reader, NULL);
      pthread_create(&writer_tid, &writer_attr, server_writer, NULL);
      while(1);
      close(client_socket_read);
      close(client_socket_write);
      if (writer_tid != -1)
        pthread_join(writer_tid, NULL);
      if (reader_tid != -1)
        pthread_join(reader_tid, NULL);
      exit(0);
    }
    // Parent doesn.t need this
    close(client_socket_read);
    close(client_socket_write);
  }
  return 0;
}
